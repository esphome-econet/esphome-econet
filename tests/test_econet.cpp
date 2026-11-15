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
  using Econet::our_read_awaiting_res_;
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

  // Unanswered read times out cleanly after ~2000ms and cleans up service queue.
  uart.tx_frames.clear();
  auto timeout_res = eco.homeassistant_read("OAT_SENS", 0x380);
  EXPECT(timeout_res.empty());
  EXPECT(!uart.tx_frames.empty());

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
  EXPECT(sched_sw.state == true);
  EXPECT(vaca_sel.active_index().has_value() && *vaca_sel.active_index() == 1);
  EXPECT(clim.has_reported_state());
  EXPECT(clim.publish_count > 0);
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

  // Verify EconetNumber rejects NaN and out-of-uint8 range values.
  uart.tx_frames.clear();
  enum_num.set(std::numeric_limits<float>::infinity());
  enum_num.set(300.0f);
  g_mock_millis = 2400;
  eco.loop();
  EXPECT(uart.tx_frames.empty());

  // Switching climate to FAN_ONLY while setting fan mode "Auto" (0 != cached STATNFAN 5) writes STATMODE and STATNFAN!
  uart.tx_frames.clear();
  clim.make_call().set_mode(climate::CLIMATE_MODE_FAN_ONLY).set_custom_fan_mode("Auto").perform();
  g_mock_millis = 2600;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 1);  // First pending write: STATMODE
  g_mock_millis = 2800;
  eco.loop();
  EXPECT(uart.tx_frames.size() == 2);  // Second pending write: STATNFAN
  std::string written_fan_dp(reinterpret_cast<const char *>(&uart.tx_frames[1][20]), 8);
  EXPECT(written_fan_dp == "STATNFAN");
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
  std::cout << "\nAll EcoNet C++ unit tests passed!\n";
  return 0;
}
