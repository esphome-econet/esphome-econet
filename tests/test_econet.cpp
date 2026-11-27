#include "automation.h"
#include "binary_sensor/econet_binary_sensor.h"
#include "climate/econet_climate.h"
#include "econet.h"
#include "number/econet_number.h"
#include "select/econet_select.h"
#include "sensor/econet_sensor.h"
#include "switch/econet_switch.h"
#include "text_sensor/econet_text_sensor.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

uint32_t g_mock_millis = 0;

#define EXPECT(cond) \
  do { \
    if (!(cond)) { \
      std::fprintf(stderr, "EXPECT failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      std::exit(1); \
    } \
  } while (false)

namespace {

using namespace esphome;
using namespace esphome::econet;

std::vector<uint8_t> build_frame(uint32_t dst, uint32_t src, uint8_t command, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame;
  auto push_adr = [&](uint32_t adr) {
    frame.push_back(0x80);
    frame.push_back(static_cast<uint8_t>((adr >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((adr >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(adr & 0xFF));
    frame.push_back(0x00);
  };
  push_adr(dst);
  push_adr(src);
  frame.push_back(static_cast<uint8_t>(payload.size()));
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(command);
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint16_t crc = crc16(frame.data(), static_cast<uint16_t>(frame.size()), 0);
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
  return frame;
}

std::vector<uint8_t> float_be(float v) {
  uint32_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  return {
      static_cast<uint8_t>((u >> 24) & 0xFF),
      static_cast<uint8_t>((u >> 16) & 0xFF),
      static_cast<uint8_t>((u >> 8) & 0xFF),
      static_cast<uint8_t>(u & 0xFF),
  };
}

std::vector<uint8_t> make_float_value_bytes(float v) {
  auto fb = float_be(v);
  return {0x80, 0x00, 0x00, fb[0], fb[1], fb[2], fb[3]};
}

std::vector<uint8_t> make_class2_item(const std::vector<uint8_t> &body) {
  std::vector<uint8_t> out;
  out.push_back(static_cast<uint8_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

std::vector<uint8_t> make_class2_float_item(float v) { return make_class2_item(make_float_value_bytes(v)); }

std::vector<uint8_t> make_class2_text_item(const std::string &s) {
  std::vector<uint8_t> body = {0x81, 0x00, 0x00};
  body.insert(body.end(), s.begin(), s.end());
  return make_class2_item(body);
}

std::vector<uint8_t> make_class2_enum_item(uint8_t enum_val, const std::string &label) {
  std::vector<uint8_t> body = {0x82, 0x00, 0x00, enum_val, static_cast<uint8_t>(label.size())};
  body.insert(body.end(), label.begin(), label.end());
  return make_class2_item(body);
}

std::vector<uint8_t> make_class2_unsupported_item() {
  return make_class2_item({static_cast<uint8_t>(0x80 | 21), 0x00, 0x00});
}

std::vector<uint8_t> make_read_class2_payload(const std::vector<std::string> &names) {
  std::vector<uint8_t> p = {0x02, 0x01};
  for (const auto &n : names) {
    p.push_back(0x00);
    p.push_back(0x00);
    for (size_t i = 0; i < 8; i++) {
      p.push_back(i < n.size() ? static_cast<uint8_t>(n[i]) : 0x00);
    }
  }
  return p;
}

std::vector<uint8_t> concat_items(const std::vector<std::vector<uint8_t>> &items) {
  std::vector<uint8_t> out;
  for (const auto &item : items) {
    out.insert(out.end(), item.begin(), item.end());
  }
  return out;
}

class TestableEconet : public Econet {
 public:
  using Econet::dst_adr_;
  using Econet::dst_adr_answered_;
  using Econet::mcu_answers_reads_;
  using Econet::mcu_connected_;
  using Econet::min_delay_between_read_requests_;
  using Econet::our_read_awaiting_res_;
  using Econet::rx_message_;
  using Econet::unanswered_reads_;
};

void test_extract_id_fields() {
  std::cout << "[TEST] extract_id_fields...\n";
  auto make_padded_24 = [](const std::string &s) {
    std::vector<uint8_t> v(24, ' ');
    for (size_t i = 0; i < s.size() && i < 24; i++) {
      v[i] = static_cast<uint8_t>(s[i]);
    }
    return v;
  };

  std::vector<uint8_t> payload = {0x04, 0x00, 0x00, 0x01};
  auto m = make_padded_24("RH96V2417STACNAL");
  auto s = make_padded_24("W122345678");
  payload.insert(payload.end(), m.begin(), m.end());
  payload.insert(payload.end(), s.begin(), s.end());

  std::string first;
  std::string second;
  EXPECT(extract_id_fields(payload, &first, &second));
  EXPECT(first == "RH96V2417STACNAL");
  EXPECT(second == "W122345678");

  // Extra binary prefix bytes before the printable run are skipped automatically.
  std::vector<uint8_t> shifted = {0x04, 0x00, 0x00, 0x01, 0xFF, 0x00, 'A', 'B', 0x00};
  shifted.insert(shifted.end(), m.begin(), m.end());
  shifted.insert(shifted.end(), s.begin(), s.end());
  first.clear();
  second.clear();
  EXPECT(extract_id_fields(shifted, &first, &second));
  EXPECT(first == "RH96V2417STACNAL");
  EXPECT(second == "W122345678");

  // Truncated payload leaves outputs untouched and returns false.
  shifted.resize(shifted.size() - 5);
  first = "keep1";
  second = "keep2";
  EXPECT(!extract_id_fields(shifted, &first, &second));
  EXPECT(first == "keep1");
  EXPECT(second == "keep2");
}

void test_issue_721_hpwh_autodiscovery() {
  std::cout << "[TEST] HPWH 0x1000 ClssType=9 auto-discovery when dst_address=0x1280 is unanswered...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  GPIOPin flow_pin;
  binary_sensor::BinarySensor conn;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_flow_control_pin(&flow_pin);
  eco.set_mcu_connected_binary_sensor(&conn);
  eco.set_update_interval(30000);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x1280);

  float got_setp = -1.0f;
  std::string got_model;
  eco.register_listener("WHTRSETP", 0, false, [&](const EconetDatapoint &dp) { got_setp = dp.value_float; }, false, 0);
  eco.register_listener("PRODMODN", 1, true, [&](const EconetDatapoint &dp) { got_model = dp.value_string; }, false, 0);

  eco.setup();
  EXPECT(flow_pin.setup_called);
  EXPECT(conn.has_state() && !conn.state);

  g_mock_millis = 1200;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(eco.dst_adr_ == 0x1280);
  EXPECT(eco.our_read_awaiting_res_);

  // 800ms later, 0x1280 has not answered; HPWH at 0x1000 broadcasts ClssType=9.
  g_mock_millis = 2000;
  uart.feed_rx(build_frame(0xF1, 0x1000, 31, {0x09, 0x01, 0x80, 0x00, 0x10, 0x00, 0x00}));
  eco.loop();
  EXPECT(eco.dst_adr_ == 0x1000);
  EXPECT(eco.mcu_connected_ && conn.state);
  EXPECT(!eco.dst_adr_answered_);

  uart.tx_frames.clear();
  g_mock_millis = 2105;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][1] == 0x00 && uart.tx_frames[0][2] == 0x10 && uart.tx_frames[0][3] == 0x00);

  g_mock_millis = 2125;
  uart.feed_rx(build_frame(0x340, 0x1000, 6, make_class2_float_item(125.0f)));
  eco.loop();
  EXPECT(got_setp == 125.0f);
  EXPECT(eco.dst_adr_answered_);
  EXPECT(eco.mcu_answers_reads_);
  EXPECT(eco.unanswered_reads_ == 0);

  uart.tx_frames.clear();
  g_mock_millis = 2230;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  g_mock_millis = 2250;
  uart.feed_rx(build_frame(0x340, 0x1000, 6, make_class2_text_item("PRO H65 T2 RU350BM  ")));
  eco.loop();
  EXPECT(got_model == "PRO H65 T2 RU350BM");

  // Once answered, another ClssType=9 broadcast must never overwrite dst_adr_.
  g_mock_millis = 5000;
  uart.feed_rx(build_frame(0xF1, 0x9999, 31, {0x09, 0x01}));
  eco.loop();
  EXPECT(eco.dst_adr_ == 0x1000);
}

void test_custom_dst_address_not_hijacked() {
  std::cout << "[TEST] Working non-coordinator dst_address (0x1C0) not hijacked by 0x380 ClssType=9...\n";
  g_mock_millis = 0;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_update_interval(30000);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x1C0);

  float cfm = -1.0f;
  eco.register_listener("F_ACTCFM", 0, false, [&](const EconetDatapoint &dp) { cfm = dp.value_float; }, false, 0);
  eco.setup();

  // ClssType=9 arrives before our first read to 0x1C0 is sent: must keep 0x1C0.
  g_mock_millis = 50;
  uart.feed_rx(build_frame(0xF1, 0x380, 31, {0x09, 0x01}));
  eco.loop();
  EXPECT(eco.dst_adr_ == 0x1C0);

  g_mock_millis = 155;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][2] == 0x01 && uart.tx_frames[0][3] == 0xC0);

  g_mock_millis = 175;
  uart.feed_rx(build_frame(0x340, 0x1C0, 6, make_class2_float_item(850.0f)));
  eco.loop();
  EXPECT(cfm == 850.0f);
  EXPECT(eco.dst_adr_answered_);

  g_mock_millis = 5000;
  uart.feed_rx(build_frame(0xF1, 0x380, 31, {0x09, 0x01}));
  eco.loop();
  EXPECT(eco.dst_adr_ == 0x1C0);
}

void test_tx_self_echo_and_coexisting_wifi_module() {
  std::cout << "[TEST] TX self-echo ignored while coexisting 0x340 WiFi module snooping works...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  uart.echo_tx_to_rx = true;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_update_interval(30000);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  float spt = -1.0f;
  float oat = -1.0f;
  std::string wifi_sw;
  eco.register_listener("SPT", 0, false, [&](const EconetDatapoint &dp) { spt = dp.value_float; }, false, 0x380);
  eco.register_listener("OAT_SENS", -1, false, [&](const EconetDatapoint &dp) { oat = dp.value_float; }, false, 0x380);
  eco.register_listener(
      "SW_VERSN", -1, false, [&](const EconetDatapoint &dp) { wifi_sw = dp.value_string; }, false, 0x340);
  eco.setup();

  g_mock_millis = 1200;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);

  // Echoed TX frame arrives in RX buffer immediately: must NOT mark connected or clear awaiting_res.
  g_mock_millis = 1205;
  eco.loop();
  EXPECT(!eco.mcu_connected_);
  EXPECT(eco.our_read_awaiting_res_);

  // Coexisting 0x340 Rheem WiFi module sends a different READ_COMMAND and receives an ACK.
  g_mock_millis = 1250;
  uart.feed_rx(build_frame(0x380, 0x340, 30, make_read_class2_payload({"OAT_SENS"})));
  eco.loop();
  EXPECT(eco.mcu_connected_);

  g_mock_millis = 1270;
  uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(68.5f)));
  eco.loop();
  EXPECT(oat == 68.5f);
  EXPECT(spt == -1.0f);
  EXPECT(eco.our_read_awaiting_res_);
  EXPECT(!eco.mcu_answers_reads_);

  // Thermostat queries SW_VERSN from the coexisting 0x340 WiFi module.
  g_mock_millis = 1500;
  uart.feed_rx(build_frame(0x340, 0x380, 30, make_read_class2_payload({"SW_VERSN"})));
  eco.loop();
  g_mock_millis = 1520;
  uart.feed_rx(build_frame(0x380, 0x340, 6, make_class2_text_item("EW240100   ")));
  eco.loop();
  EXPECT(wifi_sw == "EW240100");
}

void test_stray_0x80_resync_crc_and_timeout() {
  std::cout << "[TEST] Stray 0x80 resync, CRC rejection, partial-frame timeout, and mcu_connected timeout...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  binary_sensor::BinarySensor conn;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_mcu_connected_binary_sensor(&conn);
  eco.set_mcu_connected_timeout(5000);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  float oat = -1.0f;
  eco.register_listener("OAT_SENS", -1, false, [&](const EconetDatapoint &dp) { oat = dp.value_float; }, false, 0x380);
  eco.setup();
  EXPECT(!conn.state);

  // Stray 0x80 turnaround byte preceding a valid READ_COMMAND + ACK.
  std::vector<uint8_t> noisy = {0x80};
  auto read_f = build_frame(0x380, 0x680, 30, make_read_class2_payload({"OAT_SENS"}));
  noisy.insert(noisy.end(), read_f.begin(), read_f.end());
  uart.feed_rx(noisy);
  eco.loop();
  EXPECT(conn.state);

  g_mock_millis = 1020;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_float_item(72.0f)));
  eco.loop();
  EXPECT(oat == 72.0f);

  // Corrupt CRC on ACK is rejected without updating oat.
  g_mock_millis = 1100;
  uart.feed_rx(read_f);
  eco.loop();
  auto bad_ack = build_frame(0x680, 0x380, 6, make_class2_float_item(99.0f));
  bad_ack.back() ^= 0xFF;
  g_mock_millis = 1120;
  uart.feed_rx(bad_ack);
  eco.loop();
  EXPECT(oat == 72.0f);

  // Partial frame times out after >100ms RECEIVE_TIMEOUT, allowing subsequent valid traffic.
  g_mock_millis = 1200;
  uart.feed_rx({0x80, 0x00, 0x03, 0x80, 0x00, 0x80, 0x00});
  eco.loop();
  g_mock_millis = 1350;
  eco.loop();
  uart.feed_rx(read_f);
  eco.loop();
  g_mock_millis = 1370;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_float_item(74.5f)));
  eco.loop();
  EXPECT(oat == 74.5f);

  // No traffic for > mcu_connected_timeout (5000ms) marks disconnected.
  g_mock_millis = 7000;
  eco.loop();
  EXPECT(!eco.mcu_connected_ && !conn.state);

  // 1) Stray noise + back-to-back READ + ACK in a single RX chunk drains both frames in one loop()!
  auto ack_76 = build_frame(0x680, 0x380, 6, make_class2_float_item(76.0f));
  std::vector<uint8_t> immediate_burst = {0x00, 0xFF, 0x80};
  immediate_burst.insert(immediate_burst.end(), read_f.begin(), read_f.end());
  immediate_burst.insert(immediate_burst.end(), ack_76.begin(), ack_76.end());
  g_mock_millis = 7050;
  uart.feed_rx(immediate_burst);
  eco.loop();
  EXPECT(oat == 76.0f);

  // 2) Corrupt frame + two stray 0x80 bytes (forming a false 144-byte header candidate against 0x0380/0x0680)
  // followed by valid READ + ACK recovers both queued frames on RECEIVE_TIMEOUT flush instead of discarding them!
  auto corrupt_f = build_frame(0x380, 0x680, 31, {0x07, 0x00});
  corrupt_f.back() ^= 0x55;
  auto ack_f = build_frame(0x680, 0x380, 6, make_class2_float_item(77.0f));
  std::vector<uint8_t> burst = corrupt_f;
  burst.push_back(0x80);
  burst.push_back(0x80);
  burst.insert(burst.end(), read_f.begin(), read_f.end());
  burst.insert(burst.end(), ack_f.begin(), ack_f.end());
  g_mock_millis = 7100;
  uart.feed_rx(burst);
  eco.loop();
  g_mock_millis = 7210;  // > RECEIVE_TIMEOUT (100ms): drain_rx_frames_(true) flushes false prefix and parses READ + ACK
  eco.loop();
  EXPECT(oat == 77.0f);

  // Half-duplex collision holdoff (READ_RESPONSE_TIMEOUT = 250ms) and immediate release on NAK (0x15):
  // 1) Receiving NAK at +20ms clears awaiting_res so a queued write transmits at +130ms (< 250ms holdoff).
  uart.tx_frames.clear();
  eco.set_float_datapoint_value("SPT", 70.0f, 0x380);
  g_mock_millis = 7300;
  uart.feed_rx(read_f);
  eco.loop();
  g_mock_millis = 7320;
  uart.feed_rx(build_frame(0x680, 0x380, 0x15, {}));  // NAK ends read transaction immediately
  eco.loop();
  g_mock_millis = 7430;  // 110ms after NAK (> REQUEST_DELAY), 130ms after READ (< 250ms READ_RESPONSE_TIMEOUT)
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);

  // 2) Without ACK/NAK, a queued write is held off at +150ms and proceeds once >= 250ms have elapsed.
  uart.tx_frames.clear();
  eco.set_float_datapoint_value("SPT", 71.0f, 0x380);
  g_mock_millis = 8000;
  uart.feed_rx(read_f);
  eco.loop();
  g_mock_millis = 8150;  // +150ms: held off
  eco.loop();
  EXPECT(uart.tx_frames.empty());
  g_mock_millis = 8260;  // +260ms (>= 250ms READ_RESPONSE_TIMEOUT): write proceeds!
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);

  // Fair round-robin request_mod scheduling: when mod 0 and mod 1 are both due simultaneously,
  // consecutive poll cycles alternate between them rather than starving mod 1 behind mod 0.
  uart::MockUART rr_uart;
  TestableEconet rr_eco;
  rr_eco.set_uart_parent(&rr_uart);
  rr_eco.set_update_interval(200);
  rr_eco.set_src_address(0x340);
  rr_eco.set_dst_address(0x380);
  rr_eco.add_request_mod_address(0, 0x380);
  rr_eco.add_request_mod_address(1, 0x500);
  rr_eco.register_listener("MOD0_DP", 0, false, [](const EconetDatapoint &) {}, false, 0x380);
  rr_eco.register_listener("MOD1_DP", 1, false, [](const EconetDatapoint &) {}, false, 0x500);
  rr_eco.setup();

  g_mock_millis = 9000;
  rr_eco.loop();  // Initial poll: mod 0 (0x380)
  EXPECT(rr_uart.tx_frames.size() == 1 && rr_uart.tx_frames[0][2] == 0x03 && rr_uart.tx_frames[0][3] == 0x80);
  g_mock_millis = 9020;
  rr_uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(1.0f)));
  rr_eco.loop();

  g_mock_millis = 9250;  // Both mod 0 (last=9000, interval=200) and mod 1 (unrequested) are due -> mod 1 (0x500) wins!
  rr_eco.loop();
  EXPECT(rr_uart.tx_frames.size() == 2 && rr_uart.tx_frames[1][2] == 0x05 && rr_uart.tx_frames[1][3] == 0x00);
}

void test_pagination_request_once_and_raw_class1() {
  std::cout << "[TEST] 25-object pagination, request_once caching, and Class 1 RAW trigger...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_update_interval(10000);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  int count_received = 0;
  std::string once_val;
  std::vector<uint8_t> raw_received;

  eco.register_listener("PRODMODN", 0, true, [&](const EconetDatapoint &dp) { once_val = dp.value_string; }, false, 0);
  for (unsigned i = 0; i < 26; i++) {
    char name[16];
    std::snprintf(name, sizeof(name), "DP_%02u", i);
    eco.register_listener(name, 0, false, [&](const EconetDatapoint &) { count_received++; }, false, 0);
  }
  EconetRawDatapointUpdateTrigger raw_trig(&eco, "HWSTATUS", 0, false, 0);
  raw_trig.add_on_trigger_callback([&](const std::vector<uint8_t> &v) { raw_received = v; });

  eco.setup();

  // Slice 1: 25 Class 2 objects (PRODMODN + DP_00..DP_23).
  g_mock_millis = 1200;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][13] == 30);
  EXPECT(uart.tx_frames[0][14] == 2);  // Class 2
  EXPECT(uart.tx_frames[0][10] == 2 + 25 * 10);

  std::vector<std::vector<uint8_t>> ack1_items;
  ack1_items.push_back(make_class2_text_item("MODEL_X  "));
  for (int i = 0; i < 24; i++) {
    ack1_items.push_back(make_class2_float_item(static_cast<float>(i)));
  }
  g_mock_millis = 1220;
  uart.feed_rx(build_frame(0x340, 0x380, 6, concat_items(ack1_items)));
  eco.loop();
  EXPECT(once_val == "MODEL_X");
  EXPECT(count_received == 24);

  // Slice 2: DP_24 + DP_25 (2 Class 2 objects; stops before RAW HWSTATUS).
  uart.tx_frames.clear();
  g_mock_millis = 1330;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][14] == 2);
  EXPECT(uart.tx_frames[0][10] == 2 + 2 * 10);
  g_mock_millis = 1350;
  uart.feed_rx(
      build_frame(0x340, 0x380, 6, concat_items({make_class2_float_item(24.0f), make_class2_float_item(25.0f)})));
  eco.loop();
  EXPECT(count_received == 26);

  // Slice 3: HWSTATUS isolated as single-object Class 1 read!
  uart.tx_frames.clear();
  g_mock_millis = 1460;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][14] == 1);  // Class 1 RAW!
  EXPECT(uart.tx_frames[0][10] == 2 + 1 * 10);
  g_mock_millis = 1480;
  std::vector<uint8_t> raw_payload = {0x04, 0x00, 0x00, 0xAA, 0xBB, 0xCC};
  uart.feed_rx(build_frame(0x340, 0x380, 6, raw_payload));
  eco.loop();
  EXPECT(raw_received == raw_payload);

  // Next update_interval (10000ms later): PRODMODN (request_once=true) is skipped, so Slice 1 has 25 DP_* items
  // and Slice 2 has only 1 DP_* item (DP_25) instead of 2!
  uart.tx_frames.clear();
  g_mock_millis = 12000;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][10] == 2 + 25 * 10);

  std::vector<std::vector<uint8_t>> next_ack1(25, make_class2_float_item(1.0f));
  g_mock_millis = 12020;
  uart.feed_rx(build_frame(0x340, 0x380, 6, concat_items(next_ack1)));
  eco.loop();

  uart.tx_frames.clear();
  g_mock_millis = 12130;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][10] == 2 + 1 * 10);  // Only DP_25 remains before HWSTATUS!
}

void test_malformed_packets_and_unsupported_write_guard() {
  std::cout << "[TEST] Malformed packet bounds safety & UNSUPPORTED write refusal...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  int callbacks = 0;
  eco.register_listener("TEST_DP", -1, false, [&](const EconetDatapoint &) { callbacks++; }, false, 0x380);
  eco.setup();

  auto send_third_party_read_and_ack = [&](const std::vector<uint8_t> &ack_payload) {
    g_mock_millis += 20;
    uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"TEST_DP"})));
    eco.loop();
    g_mock_millis += 20;
    uart.feed_rx(build_frame(0x680, 0x380, 6, ack_payload));
    eco.loop();
  };

  // Truncated READ_COMMAND (payload len 1).
  uart.feed_rx(build_frame(0x380, 0x680, 30, {0x02}));
  eco.loop();

  // Class 2 ACK with item_len == 0.
  send_third_party_read_and_ack({0x00});
  // Class 2 ACK with item_len overflowing data_len.
  send_third_party_read_and_ack({0x0A, 0x80, 0x00});
  // Class 2 ACK with wrong number of items (2 items for 1 requested object).
  send_third_party_read_and_ack(concat_items({make_class2_float_item(1.0f), make_class2_float_item(2.0f)}));
  // FLOAT with wrong byte length (5 bytes instead of 7).
  send_third_party_read_and_ack(make_class2_item({0x80, 0x00, 0x00, 0x3F, 0x80}));
  // ENUM_TEXT with mismatched item_text_len (claims 10 chars, only 2 present).
  send_third_party_read_and_ack(make_class2_item({0x82, 0x00, 0x00, 0x01, 0x0A, 'O', 'K'}));
  EXPECT(callbacks == 0);

  // UNSUPPORTED response is recorded and subsequent writes to TEST_DP are refused!
  send_third_party_read_and_ack(make_class2_unsupported_item());
  EXPECT(callbacks == 1);

  uart.tx_frames.clear();
  eco.set_float_datapoint_value("TEST_DP", 42.0f, 0x380);
  g_mock_millis += 200;
  eco.loop();
  EXPECT(uart.tx_frames.empty());
}

void test_homeassistant_services() {
  std::cout << "[TEST] homeassistant_read (Class 2 + RAW fallback) & homeassistant_write...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);
  eco.setup();

  eco.register_listener("PRODMODN", 1, true, [](const EconetDatapoint &) {}, false, 0x380);
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"PRODMODN"})));
  eco.loop();
  g_mock_millis = 1020;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_text_item("RHEEM_MODEL ")));
  eco.loop();

  // Cached request_once datapoint is served immediately by homeassistant_read without bus timeout!
  g_mock_millis = 1200;
  auto res = eco.homeassistant_read("PRODMODN", 0x380);
  EXPECT(res["type"] == "TEXT");
  EXPECT(res["value_string"] == "RHEEM_MODEL");

  // Unknown RAW datapoint queried via homeassistant_read: Class 2 returns UNSUPPORTED after 1500ms, then automatic
  // Class 1 retry gets its own fresh 2000ms budget (even if it takes another 1000ms) and returns the RAW payload!
  uart.on_tx = [&](const std::vector<uint8_t> &tx) {
    if (tx.size() >= 16 && tx[13] == 30) {
      if (tx[14] == 2) {
        g_mock_millis += 1500;
        uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_unsupported_item()));
      } else if (tx[14] == 1) {
        g_mock_millis += 1000;
        uart.feed_rx(build_frame(0x340, 0x380, 6, {0x04, 0xDE, 0xAD}));
      }
    }
  };
  auto raw_res = eco.homeassistant_read("ZONESTAT", 0x380);
  uart.on_tx = nullptr;
  EXPECT(raw_res["type"] == "RAW");
  EXPECT(raw_res["value_raw"] == "04.DE.AD");

  // Unsupported datapoint where Class 2 returns UNSUPPORTED and Class 1 retry receives NAK (0x15):
  // completes immediately with UNSUPPORTED without burning the 2000ms timeout or leaving our_read_awaiting_res_ set!
  uart.on_tx = [&](const std::vector<uint8_t> &tx) {
    if (tx.size() >= 16 && tx[13] == 30) {
      g_mock_millis += 20;
      if (tx[14] == 2) {
        uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_unsupported_item()));
      } else if (tx[14] == 1) {
        uart.feed_rx(build_frame(0x340, 0x380, 0x15, {}));
      }
    }
  };
  const uint32_t before_nak_read = g_mock_millis;
  auto nak_res = eco.homeassistant_read("BOGUS_DP", 0x380);
  uart.on_tx = nullptr;
  EXPECT(nak_res["type"] == "UNSUPPORTED");
  EXPECT(g_mock_millis - before_nak_read < 500);
  EXPECT(!eco.our_read_awaiting_res_);
  EXPECT(eco.unanswered_reads_ == 0);

  // Passive bus traffic already in RX buffer satisfies homeassistant_read with %.7g float formatting and cleans up
  // datapoint_ids_for_read_service_ so no orphaned read is transmitted afterward!
  uart.tx_frames.clear();
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"SPT"})));
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_float_item(72.5f)));
  auto float_res = eco.homeassistant_read("SPT", 0x380);
  EXPECT(float_res["type"] == "FLOAT");
  EXPECT(float_res["value"] == "72.5");
  g_mock_millis += 300;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  // Unanswered read times out cleanly after ~2000ms, cleans up service queue, and clears our_read_awaiting_res_
  // so subsequent periodic reads do not log a false unanswered-read warning.
  uart.tx_frames.clear();
  auto timeout_res = eco.homeassistant_read("OAT_SENS", 0x380);
  EXPECT(timeout_res.empty());
  EXPECT(!uart.tx_frames.empty());
  EXPECT(!eco.our_read_awaiting_res_);
  EXPECT(eco.unanswered_reads_ == 0);

  // homeassistant_write ignores NaN and queues WRITE_COMMAND (command 31) for valid values.
  uart.tx_frames.clear();
  eco.homeassistant_write("WHTRSETP", std::numeric_limits<float>::quiet_NaN(), 0x380);
  g_mock_millis += 200;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  eco.homeassistant_write("WHTRSETP", 120.0f, 0x380);
  g_mock_millis += 200;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][13] == 31);
}

void test_entities_and_climate() {
  std::cout << "[TEST] EconetClimate, EconetNumber, EconetSelect, EconetSwitch, EconetSensor, EconetBinarySensor, "
               "EconetTextSensor...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  EconetSensor temp_sensor;
  temp_sensor.set_econet_parent(&eco);
  temp_sensor.set_sensor_id("SPT");
  temp_sensor.set_request_mod(-1);
  temp_sensor.set_src_adr(0x380);

  EconetBinarySensor comp_rl;
  comp_rl.set_econet_parent(&eco);
  comp_rl.set_sensor_id("COMP_RLY");
  comp_rl.set_request_mod(-1);
  comp_rl.set_src_adr(0x380);

  EconetTextSensor mode_txt;
  mode_txt.set_econet_parent(&eco);
  mode_txt.set_sensor_id("HVACMODE");
  mode_txt.set_request_mod(-1);
  mode_txt.set_src_adr(0x380);

  EconetTextSensor vaca_txt;
  vaca_txt.set_econet_parent(&eco);
  vaca_txt.set_sensor_id("VACA_NET");
  vaca_txt.set_request_mod(-1);
  vaca_txt.set_src_adr(0x380);

  EconetSwitch sched_sw;
  sched_sw.set_econet_parent(&eco);
  sched_sw.set_switch_id("SCHEDULE");
  sched_sw.set_request_mod(-1);
  sched_sw.set_src_adr(0x380);

  EconetSelect vaca_sel;
  vaca_sel.set_econet_parent(&eco);
  vaca_sel.set_select_id("VACA_NET");
  vaca_sel.set_request_mod(-1);
  vaca_sel.set_src_adr(0x380);
  vaca_sel.set_options({"Off", "Permanent"});
  vaca_sel.init_select_mappings(2);
  vaca_sel.add_select_mapping(0);
  vaca_sel.add_select_mapping(2);

  // EconetNumber backed by ENUM_TEXT: on_state_callback firing on first MCU update must write ENUM_TEXT (not FLOAT).
  EconetNumber enum_num;
  enum_num.set_econet_parent(&eco);
  enum_num.set_number_id("IRTCYEAR");
  enum_num.set_request_mod(-1);
  enum_num.set_src_adr(0x380);
  bool wrote_on_first_read = false;
  enum_num.add_on_state_callback([&](float v) {
    if (!wrote_on_first_read) {
      wrote_on_first_read = true;
      enum_num.set(v + 1.0f);
    }
  });

  EconetClimate clim;
  clim.set_econet_parent(&eco);
  clim.set_request_mod(-1);
  clim.set_src_adr(0x380);
  clim.set_current_temperature_id("SPT");
  clim.set_target_temperature_id("COOLSETP");
  clim.set_mode_id("STATMODE");
  clim.set_custom_preset_id("WHTRCNFG");
  clim.set_action_id("HVACMODE");
  clim.set_follow_schedule_id("SCHEDULE");
  clim.set_custom_fan_mode_id("STAT_FAN");
  clim.set_custom_fan_mode_no_schedule_id("STATNFAN");
  clim.init_modes(3);
  clim.add_mode(0, climate::CLIMATE_MODE_OFF);
  clim.add_mode(1, climate::CLIMATE_MODE_COOL);
  clim.add_mode(4, climate::CLIMATE_MODE_FAN_ONLY);
  clim.init_custom_presets(2);
  clim.add_custom_preset(1, "Eco");
  clim.add_custom_preset(4, "Electric");
  clim.init_custom_fan_modes(2);
  clim.add_custom_fan_mode(0, "Auto");
  clim.add_custom_fan_mode(5, "High");
  clim.init_actions(2);
  clim.add_action(0, climate::CLIMATE_ACTION_IDLE);
  clim.add_action(2, climate::CLIMATE_ACTION_COOLING);

  eco.setup();
  temp_sensor.setup();
  comp_rl.setup();
  mode_txt.setup();
  vaca_txt.setup();
  sched_sw.setup();
  vaca_sel.setup();
  enum_num.setup();
  clim.setup();

  // Exercise dump_config() across all components to verify log formatting strings.
  eco.dump_config();
  temp_sensor.dump_config();
  comp_rl.dump_config();
  mode_txt.dump_config();
  sched_sw.dump_config();
  vaca_sel.dump_config();
  enum_num.dump_config();
  clim.dump_config();

  EXPECT(!clim.has_reported_state());
  EXPECT(clim.publish_count == 0);

  // Before MCU reports mode/preset, a ClimateCall setting mode + target_temperature drops mode (preventing
  // accidental turn-off on boot) while still writing target_temperature (converted from 20°C -> 68°F), and ignores NaN!
  clim.make_call().set_target_temperature(std::numeric_limits<float>::quiet_NaN()).perform();
  g_mock_millis = 1200;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  clim.make_call().set_mode(climate::CLIMATE_MODE_OFF).set_target_temperature(20.0f).perform();
  g_mock_millis = 1400;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][13] == 31);  // Only COOLSETP write, no STATMODE write!
  uart.tx_frames.clear();
  g_mock_millis = 1600;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  // Deliver initial MCU batch for entities + climate.
  g_mock_millis = 2000;
  uart.feed_rx(build_frame(0x380, 0x680, 30,
                           make_read_class2_payload({"SPT", "COMP_RLY", "HVACMODE", "SCHEDULE", "VACA_NET", "IRTCYEAR",
                                                     "STATMODE", "WHTRCNFG", "STAT_FAN", "STATNFAN"})));
  eco.loop();

  g_mock_millis = 2020;
  uart.feed_rx(build_frame(0x680, 0x380, 6,
                           concat_items({
                               make_class2_float_item(68.0f),           // SPT = 68°F (20°C)
                               make_class2_enum_item(1, "On  "),        // COMP_RLY = true
                               make_class2_enum_item(2, "Cooling "),    // HVACMODE = Cooling
                               make_class2_enum_item(1, "Yes "),        // SCHEDULE = true
                               make_class2_enum_item(2, "Permanent "),  // VACA_NET = index 1
                               make_class2_enum_item(26, "2026 "),      // IRTCYEAR = 26 -> callback writes 27!
                               make_class2_enum_item(1, "Cooling "),    // STATMODE = COOL
                               make_class2_enum_item(1, "Eco     "),    // WHTRCNFG = Eco
                               make_class2_enum_item(0, "Auto    "),    // STAT_FAN = Auto
                               make_class2_enum_item(5, "High    "),    // STATNFAN = High
                           })));
  eco.loop();

  EXPECT(std::fabs(temp_sensor.state - 68.0f) < 0.001f);
  EXPECT(comp_rl.state == true);
  EXPECT(mode_txt.state == "Cooling");
  EXPECT(mode_txt.publish_count == 1);
  EXPECT(vaca_txt.state == "Permanent");
  EXPECT(vaca_txt.publish_count == 1);
  EXPECT(sched_sw.state == true);
  EXPECT(vaca_sel.active_index().has_value() && *vaca_sel.active_index() == 1);
  EXPECT(vaca_sel.publish_count == 1);
  EXPECT(clim.has_reported_state());
  // All 7 climate datapoints in the Class 2 ACK must be coalesced into a single publish_state() call!
  EXPECT(clim.publish_count == 1);
  EXPECT(std::fabs(clim.current_temperature - 20.0f) < 0.01f);
  EXPECT(clim.mode == climate::CLIMATE_MODE_COOL);
  EXPECT(clim.action == climate::CLIMATE_ACTION_COOLING);
  EXPECT(clim.get_custom_preset() == "Eco");
  EXPECT(clim.get_custom_fan_mode() == "Auto");  // Following schedule + COOL -> STAT_FAN ("Auto")

  // Verify EconetNumber's synchronous callback wrote IRTCYEAR=27 as ENUM_TEXT (type byte 2 at payload[2]), not FLOAT!
  uart.tx_frames.clear();
  g_mock_millis = 2200;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][13] == 31);
  EXPECT(uart.tx_frames[0][16] == static_cast<uint8_t>(EconetDatapointType::ENUM_TEXT));

  // Local numeric enum write (EconetSelect -> VACA_NET = 0) must NOT clobber vaca_txt with "", and when the MCU
  // subsequently reports the text label for enum 0 ("Off"), vaca_txt publishes "Off" while vaca_sel deduplicates!
  vaca_sel.select_index(0);
  EXPECT(vaca_sel.active_index().has_value() && *vaca_sel.active_index() == 0);
  EXPECT(vaca_sel.publish_count == 2);
  EXPECT(vaca_txt.state == "Permanent");
  EXPECT(vaca_txt.publish_count == 1);
  g_mock_millis = 2310;
  eco.loop();  // Drain VACA_NET pending write
  EXPECT(uart.tx_frames.size() == 2);

  g_mock_millis = 2340;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"VACA_NET"})));
  eco.loop();
  g_mock_millis = 2360;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_enum_item(0, "Off     ")));
  eco.loop();
  EXPECT(vaca_txt.state == "Off");
  EXPECT(vaca_txt.publish_count == 2);
  EXPECT(vaca_sel.publish_count == 2);

  // Verify EconetNumber rejects NaN and out-of-uint8 range values.
  uart.tx_frames.clear();
  enum_num.set(std::numeric_limits<float>::infinity());
  enum_num.set(300.0f);
  g_mock_millis = 2500;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  // Multi-field ClimateCall (target_temperature + FAN_ONLY + custom_fan_mode "Auto") batches into ONE publish_state()!
  uart.tx_frames.clear();
  const size_t publishes_before_call = clim.publish_count;
  clim.make_call()
      .set_target_temperature(21.0f)
      .set_mode(climate::CLIMATE_MODE_FAN_ONLY)
      .set_custom_fan_mode("Auto")
      .perform();
  EXPECT(clim.publish_count == publishes_before_call + 1);
  g_mock_millis = 2700;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);  // First pending write: COOLSETP
  g_mock_millis = 2900;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 2);  // Second pending write: STATMODE
  g_mock_millis = 3100;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 3);  // Third pending write: STATNFAN
  std::string written_fan_dp(reinterpret_cast<const char *>(&uart.tx_frames[2][20]), 8);
  EXPECT(written_fan_dp == "STATNFAN");

  // Unmapped custom_preset and custom_fan_mode values from the MCU clear stale preset/fan state atomically.
  const size_t publishes_before_unmapped = clim.publish_count;
  g_mock_millis = 3300;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"WHTRCNFG", "STATNFAN"})));
  eco.loop();
  g_mock_millis = 3320;
  uart.feed_rx(build_frame(
      0x680, 0x380, 6, concat_items({make_class2_enum_item(99, "Disabled"), make_class2_enum_item(99, "UnknownFan")})));
  eco.loop();
  EXPECT(!clim.has_custom_preset());
  EXPECT(!clim.has_custom_fan_mode());
  EXPECT(clim.publish_count == publishes_before_unmapped + 1);

  // Booting a climate entity whose first MCU preset report is unmapped still marks has_reported_state() = true.
  EconetClimate boot_clim;
  boot_clim.set_econet_parent(&eco);
  boot_clim.set_request_mod(-1);
  boot_clim.set_src_adr(0x380);
  boot_clim.set_mode_id("BOOTMODE");
  boot_clim.set_custom_preset_id("BOOTPRES");
  boot_clim.init_modes(1);
  boot_clim.add_mode(1, climate::CLIMATE_MODE_COOL);
  boot_clim.init_custom_presets(1);
  boot_clim.add_custom_preset(1, "Eco");
  boot_clim.setup();
  EXPECT(!boot_clim.has_reported_state());

  g_mock_millis = 3500;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"BOOTMODE", "BOOTPRES"})));
  eco.loop();
  g_mock_millis = 3520;
  uart.feed_rx(build_frame(0x680, 0x380, 6,
                           concat_items({make_class2_enum_item(1, "Cooling"), make_class2_enum_item(99, "Vacation")})));
  eco.loop();
  EXPECT(boot_clim.has_reported_state());
  EXPECT(!boot_clim.has_custom_preset());
  EXPECT(boot_clim.publish_count == 1);
}

void test_active_mod_intervals_short_reads_and_pending_writes() {
  std::cout << "[TEST] Active request_mod intervals, short READ_COMMAND payloads, and compact PendingWrite...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);
  eco.set_update_interval(30000);
  // Configure a fast override on an unused request_mod (mod 2) before registering listeners.
  eco.add_request_mod_update_interval(2, 5000);

  eco.register_listener("SPT", 0, false, [](const EconetDatapoint &) {}, false, 0x380);
  eco.register_listener("OAT_SENS", 1, false, [](const EconetDatapoint &) {}, false, 0x380);
  // Unused mod 2's 5s override must NOT shrink min_delay_between_read_requests_ below 30000 / 2 = 15000.
  EXPECT(eco.min_delay_between_read_requests_ == 15000);

  // Registering an entity on mod 2 activates its 5s interval -> 5000 / 3 = 1666 ms.
  eco.register_listener("STATMODE", 2, false, [](const EconetDatapoint &) {}, false, 0x380);
  EXPECT(eco.min_delay_between_read_requests_ == 5000 / 3);

  // Short/truncated READ_COMMAND payloads (data_len = 2, 3, 4, 5) must not perform out-of-bounds pointer arithmetic.
  float got_x = -1.0f;
  eco.register_listener("X", -1, false, [&](const EconetDatapoint &dp) { got_x = dp.value_float; }, false, 0x380);
  eco.setup();

  g_mock_millis = 1200;
  uart.feed_rx(build_frame(0x380, 0x680, 30, {0x02, 0x01}));
  eco.loop();
  uart.feed_rx(build_frame(0x380, 0x680, 30, {0x02, 0x01, 0x00}));
  eco.loop();
  uart.feed_rx(build_frame(0x380, 0x680, 30, {0x02, 0x01, 0x00, 0x00}));
  eco.loop();
  // 5-byte payload has a single 1-byte name "X" at offset 4.
  uart.feed_rx(build_frame(0x380, 0x680, 30, {0x02, 0x01, 0x00, 0x00, 'X'}));
  eco.loop();
  g_mock_millis = 1210;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_float_item(42.5f)));
  eco.loop();
  EXPECT(std::fabs(got_x - 42.5f) < 0.001f);

  // PendingWrite coalescing: prime MCU values for SPT and STATMODE, queue multiple writes, and verify exact 18B frames.
  g_mock_millis = 1400;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"SPT", "STATMODE"})));
  eco.loop();
  g_mock_millis = 1410;
  uart.feed_rx(build_frame(0x680, 0x380, 6,
                           concat_items({make_class2_float_item(70.0f), make_class2_enum_item(1, "Cooling ")})));
  eco.loop();
  uart.tx_frames.clear();

  // Overwrite SPT twice in-place while STATMODE is also queued; verify FIFO order (SPT first, then STATMODE).
  eco.set_float_datapoint_value("SPT", 72.0f, 0x380);
  eco.set_enum_datapoint_value("STATMODE", 2, 0x380);
  // Reverting to the MCU's cached value while a write is still pending must update the pending write.
  eco.set_float_datapoint_value("SPT", 70.0f, 0x380);

  g_mock_millis = 1600;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  EXPECT(uart.tx_frames[0][10] == 18);  // WRITE_DATA_POS (14) + FLOAT_SIZE (4)
  EXPECT(uart.tx_frames[0][13] == 31);  // WRITE_COMMAND
  EXPECT(uart.tx_frames[0][16] == static_cast<uint8_t>(EconetDatapointType::FLOAT));
  EXPECT(std::memcmp(&uart.tx_frames[0][20], "SPT\0\0\0\0\0", 8) == 0);
  auto expected_70 = float_be(70.0f);
  EXPECT(std::memcmp(&uart.tx_frames[0][28], expected_70.data(), 4) == 0);

  g_mock_millis = 1800;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 2);
  EXPECT(uart.tx_frames[1][10] == 18);
  EXPECT(uart.tx_frames[1][16] == static_cast<uint8_t>(EconetDatapointType::ENUM_TEXT));
  EXPECT(std::memcmp(&uart.tx_frames[1][20], "STATMODE", 8) == 0);
  auto expected_2 = float_be(2.0f);
  EXPECT(std::memcmp(&uart.tx_frames[1][28], expected_2.data(), 4) == 0);

  // format_hex_pretty_to stub must NUL-terminate even on tiny buffers.
  char tiny[2] = {'Z', 'Z'};
  const uint8_t sample[2] = {0xAB, 0xCD};
  EXPECT(std::strlen(format_hex_pretty_to(tiny, sample, sizeof(sample), '.')) == 0);
}

void test_climate_action_unblock_fan_routing_and_text_sensor_raw_state() {
  std::cout << "[TEST] Climate action UNSUPPORTED/unmapped unblock, fan routing, and TextSensor get_raw_state()...\n";
  g_mock_millis = 1000;
  uart::MockUART uart;
  TestableEconet eco;
  eco.set_uart_parent(&uart);
  eco.set_src_address(0x340);
  eco.set_dst_address(0x380);

  // 1) UNSUPPORTED action_datapoint unblocks initial climate publishing, whereas UNSUPPORTED custom_preset does NOT.
  EconetClimate unsup_action_clim;
  unsup_action_clim.set_econet_parent(&eco);
  unsup_action_clim.set_request_mod(-1);
  unsup_action_clim.set_src_adr(0x380);
  unsup_action_clim.set_mode_id("ACTMODE1");
  unsup_action_clim.set_action_id("ACTDP1");
  unsup_action_clim.init_modes(1);
  unsup_action_clim.add_mode(1, climate::CLIMATE_MODE_COOL);
  unsup_action_clim.init_actions(1);
  unsup_action_clim.add_action(2, climate::CLIMATE_ACTION_COOLING);

  EconetClimate unmapped_action_clim;
  unmapped_action_clim.set_econet_parent(&eco);
  unmapped_action_clim.set_request_mod(-1);
  unmapped_action_clim.set_src_adr(0x380);
  unmapped_action_clim.set_mode_id("ACTMODE2");
  unmapped_action_clim.set_action_id("ACTDP2");
  unmapped_action_clim.init_modes(1);
  unmapped_action_clim.add_mode(1, climate::CLIMATE_MODE_COOL);
  unmapped_action_clim.init_actions(1);
  unmapped_action_clim.add_action(2, climate::CLIMATE_ACTION_COOLING);

  EconetClimate unsup_preset_clim;
  unsup_preset_clim.set_econet_parent(&eco);
  unsup_preset_clim.set_request_mod(-1);
  unsup_preset_clim.set_src_adr(0x380);
  unsup_preset_clim.set_mode_id("PRESMOD");
  unsup_preset_clim.set_custom_preset_id("PRESDP");
  unsup_preset_clim.init_modes(1);
  unsup_preset_clim.add_mode(1, climate::CLIMATE_MODE_HEAT);
  unsup_preset_clim.init_custom_presets(1);
  unsup_preset_clim.add_custom_preset(1, "Eco");

  // 2) Climate whose modes_ does NOT include FAN_ONLY: rejected FAN_ONLY in ClimateCall must not route fan to STATNFAN.
  EconetClimate fan_clim;
  fan_clim.set_econet_parent(&eco);
  fan_clim.set_request_mod(-1);
  fan_clim.set_src_adr(0x380);
  fan_clim.set_mode_id("FANMODE");
  fan_clim.set_follow_schedule_id("FANSCHED");
  fan_clim.set_custom_fan_mode_id("STAT_FAN");
  fan_clim.set_custom_fan_mode_no_schedule_id("STATNFAN");
  fan_clim.init_modes(2);
  fan_clim.add_mode(0, climate::CLIMATE_MODE_OFF);
  fan_clim.add_mode(1, climate::CLIMATE_MODE_COOL);
  fan_clim.init_custom_fan_modes(2);
  fan_clim.add_custom_fan_mode(0, "Auto");
  fan_clim.add_custom_fan_mode(5, "High");

  // 3) Filtered EconetTextSensor comparing against get_raw_state() instead of filtered state.
  EconetTextSensor filtered_txt;
  filtered_txt.set_econet_parent(&eco);
  filtered_txt.set_sensor_id("FILT_TXT");
  filtered_txt.set_request_mod(-1);
  filtered_txt.set_src_adr(0x380);
  filtered_txt.filter_fn = [](const std::string &raw) -> std::string {
    if (raw == "Cooling") {
      return "Active";
    }
    if (raw == "Active") {
      return "Other";
    }
    return raw;
  };

  eco.setup();
  unsup_action_clim.setup();
  unmapped_action_clim.setup();
  unsup_preset_clim.setup();
  fan_clim.setup();
  filtered_txt.setup();

  // Attempting to write fan mode before FANSCHED is reported must be ignored safely.
  fan_clim.make_call().set_custom_fan_mode("High").perform();
  g_mock_millis = 1200;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  // Deliver initial batch.
  g_mock_millis = 1400;
  uart.feed_rx(build_frame(0x380, 0x680, 30,
                           make_read_class2_payload({"ACTMODE1", "ACTDP1", "ACTMODE2", "ACTDP2", "PRESMOD", "PRESDP",
                                                     "FANMODE", "FANSCHED", "FILT_TXT"})));
  eco.loop();
  g_mock_millis = 1420;
  uart.feed_rx(build_frame(0x680, 0x380, 6,
                           concat_items({
                               make_class2_enum_item(1, "Cooling "),  // ACTMODE1 = COOL
                               make_class2_unsupported_item(),        // ACTDP1 = UNSUPPORTED -> unblocks climate!
                               make_class2_enum_item(1, "Cooling "),  // ACTMODE2 = COOL
                               make_class2_enum_item(9, "Defrost "),  // ACTDP2 = unmapped enum -> unblocks climate!
                               make_class2_enum_item(1, "Heating "),  // PRESMOD = HEAT
                               make_class2_unsupported_item(),        // PRESDP = UNSUPPORTED -> stays held back!
                               make_class2_enum_item(1, "Cooling "),  // FANMODE = COOL
                               make_class2_enum_item(1, "Yes     "),  // FANSCHED = true
                               make_class2_enum_item(2, "Cooling "),  // FILT_TXT raw="Cooling", filtered="Active"
                           })));
  eco.loop();

  EXPECT(unsup_action_clim.has_reported_state());
  EXPECT(unsup_action_clim.publish_count == 1);
  EXPECT(unsup_action_clim.action == climate::CLIMATE_ACTION_OFF);

  EXPECT(unmapped_action_clim.has_reported_state());
  EXPECT(unmapped_action_clim.publish_count == 1);
  EXPECT(unmapped_action_clim.action == climate::CLIMATE_ACTION_OFF);

  // Control-critical UNSUPPORTED custom_preset must keep climate held back as unknown.
  EXPECT(!unsup_preset_clim.has_reported_state());
  EXPECT(unsup_preset_clim.publish_count == 0);

  // Subsequent mapped action on unmapped_action_clim still publishes cleanly.
  g_mock_millis = 1500;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"ACTDP2"})));
  eco.loop();
  g_mock_millis = 1520;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_enum_item(2, "Cooling ")));
  eco.loop();
  EXPECT(unmapped_action_clim.action == climate::CLIMATE_ACTION_COOLING);
  EXPECT(unmapped_action_clim.publish_count == 2);

  // Rejected FAN_ONLY mode in ClimateCall must keep this->mode == COOL and write STAT_FAN (not STATNFAN).
  uart.tx_frames.clear();
  fan_clim.make_call().set_mode(climate::CLIMATE_MODE_FAN_ONLY).set_custom_fan_mode("High").perform();
  g_mock_millis = 1700;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);
  std::string written_dp(reinterpret_cast<const char *>(&uart.tx_frames[0][20]), 8);
  EXPECT(written_dp == "STAT_FAN");

  // Filtered TextSensor: raw="Cooling" -> filtered="Active"; next raw="Active" MUST publish filtered="Other"!
  EXPECT(filtered_txt.get_raw_state() == "Cooling");
  EXPECT(filtered_txt.state == "Active");
  EXPECT(filtered_txt.publish_count == 1);

  g_mock_millis = 1800;
  uart.feed_rx(build_frame(0x380, 0x680, 30, make_read_class2_payload({"FILT_TXT"})));
  eco.loop();
  g_mock_millis = 1820;
  uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_enum_item(3, "Active  ")));
  eco.loop();
  EXPECT(filtered_txt.get_raw_state() == "Active");
  EXPECT(filtered_txt.state == "Other");
  EXPECT(filtered_txt.publish_count == 2);
}

void test_echo_resilience_and_slow_tx_timing() {
  std::cout << "[TEST] Corrupt prefix before TX echo, slow blocking TX timing, and ClssType=9 window...\n";
  // 1) A corrupt candidate frame immediately preceding a TX self-echo must NOT clear awaiting_tx_echo_.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);

    float spt = -1.0f;
    eco.register_listener("SPT", 0, false, [&](const EconetDatapoint &dp) { spt = dp.value_float; }, false, 0x380);
    eco.setup();

    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    const auto tx_echo = uart.tx_frames[0];

    // Inject a corrupt 16-byte frame (bad CRC) followed immediately by our real TX self-echo.
    auto bad_prefix = build_frame(0x380, 0x680, 31, {});
    bad_prefix.back() ^= 0xFF;
    std::vector<uint8_t> burst = bad_prefix;
    burst.insert(burst.end(), tx_echo.begin(), tx_echo.end());
    g_mock_millis = 1210;
    uart.feed_rx(burst);
    eco.loop();
    EXPECT(!eco.mcu_connected_);
    EXPECT(eco.our_read_awaiting_res_);

    // Now the real MCU ACK arrives and completes our read cleanly.
    g_mock_millis = 1230;
    uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(71.5f)));
    eco.loop();
    EXPECT(std::fabs(spt - 71.5f) < 0.001f);
    EXPECT(eco.mcu_connected_);
    EXPECT(eco.mcu_answers_reads_);
    EXPECT(!eco.our_read_awaiting_res_);
  }

  // 2) Slow blocking TX (+70ms during write_array/flush) measures echo, holdoff, and ClssType=9 from post-TX time.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x1C0);
    eco.register_listener("SPT", 0, false, [](const EconetDatapoint &) {}, false, 0);
    eco.setup();

    std::vector<uint8_t> last_tx;
    uart.on_tx = [&](const std::vector<uint8_t> &frame) {
      last_tx = frame;
      g_mock_millis += 70;  // Simulate ~70ms blocking TX at 38400 baud
    };

    g_mock_millis = 1200;
    eco.loop();  // TX starts at 1200, finishes at 1270
    uart.on_tx = nullptr;
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(g_mock_millis == 1270);

    // Self-echo arrives 50ms after TX completion (1320ms: 120ms after loop entry, 50ms <= 100ms post-TX).
    g_mock_millis = 1320;
    uart.feed_rx(last_tx);
    eco.loop();
    EXPECT(!eco.mcu_connected_);
    EXPECT(eco.our_read_awaiting_res_);

    // Queue a write: held off at 1490ms (+220ms post-TX < 250ms READ_RESPONSE_TIMEOUT, even though 290ms
    // have elapsed since loop entry at 1200ms) and sent at 1530ms (+260ms post-TX >= 250ms).
    uart.tx_frames.clear();
    eco.set_float_datapoint_value("SPT", 70.0f, 0x1C0);
    g_mock_millis = 1490;
    eco.loop();
    EXPECT(uart.tx_frames.empty());
    g_mock_millis = 1530;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);

    // ClssType=9 from 0x380 at 1650ms (+380ms after unanswered read at 1270ms, < 500ms window): must NOT hijack 0x1C0!
    g_mock_millis = 1650;
    uart.feed_rx(build_frame(0xF1, 0x380, 31, {0x09, 0x01}));
    eco.loop();
    EXPECT(eco.dst_adr_ == 0x1C0);

    // ClssType=9 at >500ms after unanswered read to 0x1C0 (1270 + 550 = 1820) auto-discovers 0x1000.
    g_mock_millis = 1820;
    uart.feed_rx(build_frame(0xF1, 0x1000, 31, {0x09, 0x01}));
    eco.loop();
    EXPECT(eco.dst_adr_ == 0x1000);
  }
}

void test_timeout_recovery_reentrancy_zero_dst_and_request_once() {
  std::cout
      << "[TEST] Timeout ACK recovery, callback reentrancy, dst_address=0 guard, and request_once precedence...\n";
  // 1) Corrupt prefix followed by ONLY the ACK to our in-flight READ is recovered on RECEIVE_TIMEOUT flush.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);

    float spt = -1.0f;
    eco.register_listener("SPT", 0, false, [&](const EconetDatapoint &dp) { spt = dp.value_float; }, false, 0x380);
    eco.setup();

    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(eco.our_read_awaiting_res_);

    // Corrupt frame + two stray 0x80 bytes (forming a false oversized length candidate) + valid ACK for our read.
    auto corrupt_f = build_frame(0x380, 0x680, 31, {0x07, 0x00});
    corrupt_f.back() ^= 0x55;
    auto ack_f = build_frame(0x340, 0x380, 6, make_class2_float_item(73.5f));
    std::vector<uint8_t> burst = corrupt_f;
    burst.push_back(0x80);
    burst.push_back(0x80);
    burst.insert(burst.end(), ack_f.begin(), ack_f.end());

    g_mock_millis = 1220;
    uart.feed_rx(burst);
    eco.loop();
    EXPECT(spt < 0.0f);  // Still stuck on false candidate length

    g_mock_millis = 1330;  // > RECEIVE_TIMEOUT (100ms): drain_rx_frames_(true) recovers our ACK!
    eco.loop();
    EXPECT(std::fabs(spt - 73.5f) < 0.001f);
    EXPECT(!eco.our_read_awaiting_res_);
    EXPECT(eco.mcu_answers_reads_);
    EXPECT(eco.dst_adr_answered_);

    // Corrupt prefix followed by ONLY a new READ_COMMAND preserves read_req_ across timeout flush so its ACK works.
    auto read_f = build_frame(0x380, 0x680, 30, make_read_class2_payload({"SPT"}));
    std::vector<uint8_t> read_burst = corrupt_f;
    read_burst.push_back(0x80);
    read_burst.push_back(0x80);
    read_burst.insert(read_burst.end(), read_f.begin(), read_f.end());
    g_mock_millis = 1500;
    uart.feed_rx(read_burst);
    eco.loop();
    g_mock_millis = 1610;  // Timeout flush recovers read_f and keeps read_req_.awaiting_res = true
    eco.loop();
    g_mock_millis = 1620;
    uart.feed_rx(build_frame(0x680, 0x380, 6, make_class2_float_item(75.0f)));
    eco.loop();
    EXPECT(std::fabs(spt - 75.0f) < 0.001f);
  }

  // 2) Reentrant listener callback invoking homeassistant_read() inside a Class 2 ACK.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);

    struct ReentrancyCtx {
      TestableEconet *eco;
      float spt{-1.0f};
      float oat{-1.0f};
      bool oat_saw_batch{false};
      int batch_completes{0};
      std::map<std::string, std::string> nested_res{};
    } ctx{.eco = &eco};

    eco.register_listener(
        "SPT", 0, false,
        [&ctx](const EconetDatapoint &dp) {
          ctx.spt = dp.value_float;
          ctx.nested_res = ctx.eco->homeassistant_read("NESTED", 0x380);
        },
        false, 0x380);
    eco.register_listener(
        "OAT_SENS", 0, false,
        [&ctx](const EconetDatapoint &dp) {
          ctx.oat = dp.value_float;
          ctx.oat_saw_batch = ctx.eco->is_in_batch_response();
        },
        false, 0x380);
    eco.add_on_batch_complete_callback([&ctx]() { ctx.batch_completes++; });
    eco.setup();

    // Poll mod 0 (SPT + OAT_SENS).
    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);

    // Reply immediately when homeassistant_read("NESTED") transmits.
    uart.on_tx = [&](const std::vector<uint8_t> &tx) {
      if (tx.size() >= 16 && tx[13] == 30) {
        g_mock_millis += 10;
        uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(99.5f)));
      }
    };

    // Feed our Class 2 ACK along with a trailing third-party frame in the same RX chunk.
    g_mock_millis = 1220;
    auto ack =
        build_frame(0x340, 0x380, 6, concat_items({make_class2_float_item(68.0f), make_class2_float_item(55.0f)}));
    auto trailing = build_frame(0xF1, 0x380, 31, {0x07, 0x00});
    std::vector<uint8_t> combined = ack;
    combined.insert(combined.end(), trailing.begin(), trailing.end());
    uart.feed_rx(combined);
    eco.loop();
    uart.on_tx = nullptr;

    EXPECT(std::fabs(ctx.spt - 68.0f) < 0.001f);
    EXPECT(std::fabs(ctx.oat - 55.0f) < 0.001f);
    EXPECT(ctx.nested_res["type"] == "FLOAT" && ctx.nested_res["value"] == "99.5");
    EXPECT(ctx.oat_saw_batch);         // Nested Class 2 ACK must not clear outer batch depth early
    EXPECT(ctx.batch_completes == 1);  // Fires once when outermost batch completes
    EXPECT(!eco.is_in_batch_response());
    EXPECT(!eco.our_read_awaiting_res_);
    EXPECT(eco.unanswered_reads_ == 0);
  }

  // 3) Omitted dst_address (0) must never transmit frames, write optimistic state, or block homeassistant_read
  // before ClssType=9 discovery.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    // dst_address intentionally omitted (0).
    int optimistic_cb = 0;
    eco.register_listener(
        "WHTRSETP", 0, false, [&optimistic_cb](const EconetDatapoint &) { optimistic_cb++; }, false, 0);
    eco.setup();

    eco.set_float_datapoint_value("WHTRSETP", 120.0f, 0);
    EXPECT(optimistic_cb == 0);
    const uint32_t before_read = g_mock_millis;
    auto empty_read = eco.homeassistant_read("WHTRSETP", 0);
    EXPECT(empty_read.empty());
    EXPECT(g_mock_millis == before_read);  // Returned immediately instead of blocking 2000ms

    for (int i = 0; i < 5; i++) {
      g_mock_millis += 200;
      eco.loop();
    }
    EXPECT(uart.tx_frames.empty());

    // ClssType=9 discovers 0x1000; next loop() immediately polls 0x1000!
    g_mock_millis += 100;
    uart.feed_rx(build_frame(0xF1, 0x1000, 31, {0x09, 0x01}));
    eco.loop();
    EXPECT(eco.dst_adr_ == 0x1000);

    g_mock_millis += 150;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(uart.tx_frames[0][13] == 30);
    EXPECT(uart.tx_frames[0][2] == 0x10 && uart.tx_frames[0][3] == 0x00);
  }

  // 4) Periodic (request_once=false) listeners take precedence over request_once=true regardless of registration
  // order, and default-address (src_adr=0) listeners on mod 0 do not bleed into another device's mod.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(5000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);
    eco.add_request_mod_address(1, 0x500);

    // Order 1: request_once=true then request_once=false on 0x380
    eco.register_listener("DP_A", 0, true, [](const EconetDatapoint &) {}, false, 0x380);
    eco.register_listener("DP_A", 0, false, [](const EconetDatapoint &) {}, false, 0x380);
    // Order 2: request_once=false then request_once=true on 0x380
    eco.register_listener("DP_B", 0, false, [](const EconetDatapoint &) {}, false, 0x380);
    eco.register_listener("DP_B", 0, true, [](const EconetDatapoint &) {}, false, 0x380);
    // Cross-device: request_once=true with src_adr=0 on mod 0 (0x380) + periodic on mod 1 (0x500)
    eco.register_listener("DP_C", 0, true, [](const EconetDatapoint &) {}, false, 0);
    eco.register_listener("DP_C", 1, false, [](const EconetDatapoint &) {}, false, 0);
    // Reverse cross-device: periodic with src_adr=0 on mod 0 (0x380) + request_once=true on mod 1 (0x500)
    eco.register_listener("DP_D", 0, false, [](const EconetDatapoint &) {}, false, 0);
    eco.register_listener("DP_D", 1, true, [](const EconetDatapoint &) {}, false, 0x500);
    eco.setup();

    // Initial poll mod 0 (0x380): DP_A, DP_B, DP_C, DP_D (4 items -> payload len = 2 + 40 = 42)
    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1 && uart.tx_frames[0][10] == 42);
    g_mock_millis = 1220;
    uart.feed_rx(build_frame(0x340, 0x380, 6,
                             concat_items({make_class2_float_item(1.0f), make_class2_float_item(2.0f),
                                           make_class2_float_item(3.0f), make_class2_float_item(4.0f)})));
    eco.loop();

    // Initial poll mod 1 (0x500): DP_C, DP_D (2 items -> payload len = 2 + 20 = 22)
    g_mock_millis = 1330;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2 && uart.tx_frames[1][10] == 22);
    g_mock_millis = 1350;
    uart.feed_rx(
        build_frame(0x340, 0x500, 6, concat_items({make_class2_float_item(5.0f), make_class2_float_item(6.0f)})));
    eco.loop();

    // After update_interval (5000ms):
    // - mod 0 (0x380) skips cached request_once DP_C, but MUST still poll DP_A, DP_B, DP_D (3 items -> len = 32)!
    // - mod 1 (0x500) skips cached request_once DP_D (not overridden by mod 0's src_adr=0 periodic DP_D!),
    //   and MUST still poll periodic DP_C (1 item -> len = 12)!
    uart.tx_frames.clear();
    g_mock_millis = 7000;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(uart.tx_frames[0][2] == 0x03 && uart.tx_frames[0][3] == 0x80);
    EXPECT(uart.tx_frames[0][10] == 32);
    g_mock_millis = 7020;
    uart.feed_rx(build_frame(
        0x340, 0x380, 6,
        concat_items({make_class2_float_item(1.0f), make_class2_float_item(2.0f), make_class2_float_item(4.0f)})));
    eco.loop();

    g_mock_millis = 10000;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2);
    EXPECT(uart.tx_frames[1][2] == 0x05 && uart.tx_frames[1][3] == 0x00);
    EXPECT(uart.tx_frames[1][10] == 12);
  }
}

void test_boot_polling_recovery_pre_tx_parse_and_zero_copy_rx() {
  std::cout << "[TEST] Boot order, 1s fast retry, multi-slice recovery, pre-TX parse, and zero-copy RX...\n";
  auto tx_first_obj = [](const std::vector<uint8_t> &frame) {
    return std::string(reinterpret_cast<const char *>(&frame[18]), 7);
  };

  // 1) Out-of-order listener registration (mod 5, 2, 1, 0) polls mod 0 first on boot.
  //    A single dropped boot read retries mod 0 at +1000ms. If mod 0 drops twice and mod 1 gets
  //    the first MCU answer, mod 0 resets AND restarts next_request_mod_index_ = 0 so mod 0 retries
  //    immediately ahead of mod 2 and disconnected mod 5.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);
    eco.add_request_mod_address(5, 0x500);

    float mod0_val = -1.0f;
    float mod1_val = -1.0f;
    float mod2_val = -1.0f;
    eco.register_listener("MOD5_DP", 5, false, [](const EconetDatapoint &) {}, false, 0x500);
    eco.register_listener(
        "MOD2_DP", 2, false, [&](const EconetDatapoint &dp) { mod2_val = dp.value_float; }, false, 0x380);
    eco.register_listener(
        "MOD1_DP", 1, false, [&](const EconetDatapoint &dp) { mod1_val = dp.value_float; }, false, 0x380);
    eco.register_listener(
        "MOD0_DP", 0, false, [&](const EconetDatapoint &dp) { mod0_val = dp.value_float; }, false, 0x380);
    eco.setup();

    // Boot poll #1 (t = 1200): mod 0 MUST be polled first despite being registered last; simulate drop.
    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(tx_first_obj(uart.tx_frames[0]) == "MOD0_DP");
    EXPECT(eco.our_read_awaiting_res_);

    // Still held off at +900ms (t = 2100).
    g_mock_millis = 2100;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);

    // Fast boot retry fires once at +1000ms (t = 2200) and retries mod 0 directly! Simulate second drop.
    g_mock_millis = 2200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2);
    EXPECT(tx_first_obj(uart.tx_frames[1]) == "MOD0_DP");
    EXPECT(eco.unanswered_reads_ == 1);

    // No second fast retry at +1000ms (t = 3200): falls back to min_delay_between_read_requests_ (7500ms).
    g_mock_millis = 3200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2);

    // At t = 9750 (+7550ms >= 7500ms): mod 1 polls and gets the first-ever MCU answer!
    g_mock_millis = 9750;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 3);
    EXPECT(tx_first_obj(uart.tx_frames[2]) == "MOD1_DP");
    EXPECT(eco.unanswered_reads_ == 2);
    g_mock_millis = 9770;
    uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(11.0f)));
    eco.loop();
    EXPECT(std::fabs(mod1_val - 11.0f) < 0.001f);
    EXPECT(eco.mcu_answers_reads_);

    // Because next_request_mod_index_ was reset to 0, mod 0 retries IMMEDIATELY at t = 9880 (ahead of mod 2 & mod 5)!
    g_mock_millis = 9880;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 4);
    EXPECT(tx_first_obj(uart.tx_frames[3]) == "MOD0_DP");
    g_mock_millis = 9900;
    uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(99.0f)));
    eco.loop();
    EXPECT(std::fabs(mod0_val - 99.0f) < 0.001f);

    // Then mod 2 polls immediately at t = 10010 and answers.
    g_mock_millis = 10010;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 5);
    EXPECT(tx_first_obj(uart.tx_frames[4]) == "MOD2_DP");
    g_mock_millis = 10030;
    uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(22.0f)));
    eco.loop();
    EXPECT(std::fabs(mod2_val - 22.0f) < 0.001f);

    // Disconnected mod 5 polls at t = 10140 and goes unanswered; after mcu_answers_reads_ == true,
    // it must NOT fast-retry at +1000ms.
    g_mock_millis = 10140;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 6);
    EXPECT(tx_first_obj(uart.tx_frames[5]) == "MOD5_DP");
    uart.tx_frames.clear();
    g_mock_millis = 11300;
    eco.loop();
    EXPECT(uart.tx_frames.empty());
  }

  // 2) Single-mod [0] config with 30s update_interval also recovers a dropped boot read in 1s (not 30s).
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);
    float val = -1.0f;
    eco.register_listener("SOLO_DP", 0, false, [&](const EconetDatapoint &dp) { val = dp.value_float; }, false, 0x380);
    eco.setup();

    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);

    // Fast boot retry fires at t = 2200 (+1000ms) instead of waiting 30s!
    g_mock_millis = 2200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2);
    EXPECT(tx_first_obj(uart.tx_frames[1]) == "SOLO_DP");
    g_mock_millis = 2220;
    uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(42.0f)));
    eco.loop();
    EXPECT(std::fabs(val - 42.0f) < 0.001f);
    EXPECT(eco.mcu_answers_reads_);

    // Must NOT needlessly re-poll SOLO_DP on the next tick.
    uart.tx_frames.clear();
    g_mock_millis = 2400;
    eco.loop();
    EXPECT(uart.tx_frames.empty());
  }

  // 3) Multi-slice mod 0 (27 datapoints: slice 1 = 0..24, slice 2 = 25..26):
  //    (a) If slice 1 drops once and succeeds on the 1s fast retry, it advances straight to slice 2 without re-reading
  //        slice 1.
  //    (b) If slice 1 drops twice and slice 2 gets the first MCU answer, mod 0 resets so slice 1 retries immediately.
  for (bool drop_slice1_twice : {false, true}) {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);

    float first_val = -1.0f;
    float last_val = -1.0f;
    for (int i = 0; i < 27; i++) {
      char name[16];
      std::snprintf(name, sizeof(name), "MS_%04d", i);
      if (i == 0) {
        eco.register_listener(
            name, 0, false, [&](const EconetDatapoint &dp) { first_val = dp.value_float; }, false, 0x380);
      } else if (i == 26) {
        eco.register_listener(
            name, 0, false, [&](const EconetDatapoint &dp) { last_val = dp.value_float; }, false, 0x380);
      } else {
        eco.register_listener(name, 0, false, [](const EconetDatapoint &) {}, false, 0x380);
      }
    }
    eco.setup();

    auto make_n_floats = [](size_t n, float v) {
      std::vector<std::vector<uint8_t>> items;
      for (size_t i = 0; i < n; i++) {
        items.push_back(make_class2_float_item(v + static_cast<float>(i)));
      }
      return concat_items(items);
    };

    // t = 1200: slice 1 (MS_0000..MS_0024) dropped.
    g_mock_millis = 1200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
    EXPECT(tx_first_obj(uart.tx_frames[0]) == "MS_0000");

    // t = 2200: 1s fast retry re-sends slice 1 (MS_0000..MS_0024).
    g_mock_millis = 2200;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 2);
    EXPECT(tx_first_obj(uart.tx_frames[1]) == "MS_0000");

    if (!drop_slice1_twice) {
      g_mock_millis = 2220;
      uart.feed_rx(build_frame(0x340, 0x380, 6, make_n_floats(25, 10.0f)));
      eco.loop();
      EXPECT(std::fabs(first_val - 10.0f) < 0.001f);

      // Next poll immediately sends slice 2 (MS_0025..MS_0026), NOT slice 1 again!
      g_mock_millis = 2330;
      eco.loop();
      EXPECT(uart.tx_frames.size() == 3);
      EXPECT(tx_first_obj(uart.tx_frames[2]) == "MS_0025");
      g_mock_millis = 2350;
      uart.feed_rx(build_frame(0x340, 0x380, 6, make_n_floats(2, 50.0f)));
      eco.loop();
      EXPECT(std::fabs(last_val - 51.0f) < 0.001f);
    } else {
      // Slice 1 dropped twice: after 30s min_delay, slice 2 (continuation) sends and answers first.
      g_mock_millis = 32250;
      eco.loop();
      EXPECT(uart.tx_frames.size() == 3);
      EXPECT(tx_first_obj(uart.tx_frames[2]) == "MS_0025");
      g_mock_millis = 32270;
      uart.feed_rx(build_frame(0x340, 0x380, 6, make_n_floats(2, 50.0f)));
      eco.loop();
      EXPECT(std::fabs(last_val - 51.0f) < 0.001f);
      EXPECT(first_val < 0.0f);

      // Continuation first-answer resets mod 0 so slice 1 retries IMMEDIATELY at t = 32380!
      g_mock_millis = 32380;
      eco.loop();
      EXPECT(uart.tx_frames.size() == 4);
      EXPECT(tx_first_obj(uart.tx_frames[3]) == "MS_0000");
      g_mock_millis = 32400;
      uart.feed_rx(build_frame(0x340, 0x380, 6, make_n_floats(25, 10.0f)));
      eco.loop();
      EXPECT(std::fabs(first_val - 10.0f) < 0.001f);
    }
  }

  // 4) Pre-TX parse_message_(..., true) populates read state BEFORE write_array(), while post-TX millis() refresh
  //    still measures READ_RESPONSE_TIMEOUT from end of transmission.
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);
    eco.register_listener("SPT", 0, false, [](const EconetDatapoint &) {}, false, 0x380);
    eco.setup();

    bool awaiting_during_tx = false;
    uart.on_tx = [&](const std::vector<uint8_t> &) {
      awaiting_during_tx = eco.our_read_awaiting_res_;
      g_mock_millis += 70;
    };
    g_mock_millis = 1200;
    eco.loop();  // TX finishes at 1270ms
    uart.on_tx = nullptr;
    EXPECT(awaiting_during_tx);
    EXPECT(g_mock_millis == 1270);

    // Queued write must still be held off at +220ms post-TX (1490ms) and transmit at +260ms post-TX (1530ms).
    uart.tx_frames.clear();
    eco.set_float_datapoint_value("SPT", 70.0f, 0x380);
    g_mock_millis = 1490;
    eco.loop();
    EXPECT(uart.tx_frames.empty());
    g_mock_millis = 1530;
    eco.loop();
    EXPECT(uart.tx_frames.size() == 1);
  }

  // 5) Zero-copy read_buffer_() directly into rx_message_:
  //    - Decodes >140B ACKs cleanly.
  //    - Preserves exact FIFO wire order when a reentrant homeassistant_read() fires mid-buffer,
  //      both for complete trailing frames already in rx_message_ AND for frames split across a
  //      MAX_READ_SIZE (256B) boundary!
  {
    g_mock_millis = 1000;
    uart::MockUART uart;
    TestableEconet eco;
    eco.set_uart_parent(&uart);
    eco.set_update_interval(30000);
    eco.set_src_address(0x340);
    eco.set_dst_address(0x380);

    std::string a1;
    std::string a2;
    eco.register_listener("ALARM_01", 0, false, [&](const EconetDatapoint &dp) { a1 = dp.value_string; }, false, 0x380);
    eco.register_listener("ALARM_02", 0, false, [&](const EconetDatapoint &dp) { a2 = dp.value_string; }, false, 0x380);
    eco.setup();

    g_mock_millis = 1200;
    eco.loop();
    std::string long1 = "ALARM_ONE" + std::string(55, ' ');
    std::string long2 = "ALARM_TWO" + std::string(55, ' ');
    auto big_ack =
        build_frame(0x340, 0x380, 6, concat_items({make_class2_text_item(long1), make_class2_text_item(long2)}));
    EXPECT(big_ack.size() > 140);
    g_mock_millis = 1220;
    uart.feed_rx(big_ack);
    eco.loop();
    EXPECT(a1 == "ALARM_ONE");
    EXPECT(a2 == "ALARM_TWO");
    EXPECT(eco.rx_message_.empty());

    // (a) Reentrant callback with a complete trailing frame (AFTER_DP = 77.0) already inside rx_message_.
    struct ReentCtx {
      TestableEconet *eco;
      std::map<std::string, std::string> nested{};
    } rctx{.eco = &eco};
    float after_val = -1.0f;
    eco.register_listener(
        "REENT_DP", -1, false,
        [&rctx](const EconetDatapoint &) { rctx.nested = rctx.eco->homeassistant_read("INNER_DP", 0x380); }, false,
        0x380);
    eco.register_listener(
        "AFTER_DP", -1, false, [&](const EconetDatapoint &dp) { after_val = dp.value_float; }, false, 0x380);
    uart.on_tx = [&](const std::vector<uint8_t> &tx) {
      if (tx.size() >= 16 && tx[13] == 30) {
        g_mock_millis += 10;
        uart.feed_rx(build_frame(0x340, 0x380, 6, make_class2_float_item(12.5f)));
      }
    };
    auto r1 = build_frame(0x380, 0x680, 30, make_read_class2_payload({"REENT_DP"}));
    auto k1 = build_frame(0x680, 0x380, 6, make_class2_float_item(1.0f));
    auto r2 = build_frame(0x380, 0x680, 30, make_read_class2_payload({"AFTER_DP"}));
    auto k2 = build_frame(0x680, 0x380, 6, make_class2_float_item(77.0f));
    std::vector<uint8_t> multi = r1;
    multi.insert(multi.end(), k1.begin(), k1.end());
    multi.insert(multi.end(), r2.begin(), r2.end());
    multi.insert(multi.end(), k2.begin(), k2.end());
    g_mock_millis = 1400;
    uart.feed_rx(multi);
    eco.loop();
    EXPECT(rctx.nested["type"] == "FLOAT" && rctx.nested["value"] == "12.5");
    EXPECT(std::fabs(after_val - 77.0f) < 0.001f);
    EXPECT(eco.rx_message_.empty());

    // (b) Reentrant callback where a trailing frame (AFTER_DP = 88.5) is SPLIT across the 256B MAX_READ_SIZE boundary:
    //     outer read_buffer_() reads the first 256B (including part of r2) into rx_message_, then inner loop()
    //     appends the remaining bytes of r2/k2 right after them in exact FIFO order without severing the frame!
    after_val = -1.0f;
    rctx.nested.clear();
    auto k1_b = build_frame(0x680, 0x380, 6, make_class2_float_item(2.0f));
    auto pad1 = build_frame(0xF1, 0x380, 31, std::vector<uint8_t>(180, 0x07));  // 196B valid frame
    auto k2_b = build_frame(0x680, 0x380, 6, make_class2_float_item(88.5f));
    std::vector<uint8_t> split_stream = r1;                             // 28B
    split_stream.insert(split_stream.end(), k1_b.begin(), k1_b.end());  // +25B = 53B
    split_stream.insert(split_stream.end(), pad1.begin(), pad1.end());  // +196B = 249B
    split_stream.insert(split_stream.end(), r2.begin(), r2.end());      // +28B = 277B (crosses 256B!)
    split_stream.insert(split_stream.end(), k2_b.begin(), k2_b.end());  // +25B = 302B
    EXPECT(split_stream.size() > 256);
    g_mock_millis = 2000;
    uart.feed_rx(split_stream);
    eco.loop();
    uart.on_tx = nullptr;
    EXPECT(rctx.nested["type"] == "FLOAT" && rctx.nested["value"] == "12.5");
    EXPECT(std::fabs(after_val - 88.5f) < 0.001f);
    EXPECT(eco.rx_message_.empty());
  }
}

}  // namespace

int main() {
  test_extract_id_fields();
  test_issue_721_hpwh_autodiscovery();
  test_custom_dst_address_not_hijacked();
  test_tx_self_echo_and_coexisting_wifi_module();
  test_stray_0x80_resync_crc_and_timeout();
  test_pagination_request_once_and_raw_class1();
  test_malformed_packets_and_unsupported_write_guard();
  test_homeassistant_services();
  test_entities_and_climate();
  test_active_mod_intervals_short_reads_and_pending_writes();
  test_climate_action_unblock_fan_routing_and_text_sensor_raw_state();
  test_echo_resilience_and_slow_tx_timing();
  test_timeout_recovery_reentrancy_zero_dst_and_request_once();
  test_boot_polling_recovery_pre_tx_parse_and_zero_copy_rx();
  std::cout << "\nAll EcoNet C++ unit tests passed!\n";
  return 0;
}
