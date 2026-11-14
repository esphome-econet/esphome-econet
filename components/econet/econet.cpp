#include "econet.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <algorithm>
#include <cstring>

namespace esphome {
namespace econet {

static const char *const TAG = "econet";

static const uint32_t RECEIVE_TIMEOUT = 100;
static const uint32_t REQUEST_DELAY = 100;

// A silent bus produces one unanswered read per update interval, forever. Log the first few at
// full rate so a transient stall is still visible, then back off to one message per interval so
// the warning cannot drown out the rest of the log.
static const uint32_t UNANSWERED_READS_BEFORE_BACKOFF = 3;
static const uint32_t UNANSWERED_READ_LOG_INTERVAL = 60000;

static const uint8_t DST_ADR_POS = 0;
static const uint8_t SRC_ADR_POS = 5;
static const uint8_t LEN_POS = 10;
static const uint8_t COMMAND_POS = 13;

static const uint8_t OBJ_NAME_POS = 6;
static const uint8_t OBJ_NAME_SIZE = 8;
static const uint8_t WRITE_DATA_POS = OBJ_NAME_POS + OBJ_NAME_SIZE;
static const uint8_t FLOAT_SIZE = sizeof(float);

static const uint8_t MSG_HEADER_SIZE = 14;
static const uint8_t MSG_CRC_SIZE = 2;

static const uint8_t ACK = 6;
static const uint8_t READ_COMMAND = 30;   // 0x1E
static const uint8_t WRITE_COMMAND = 31;  // 0x1F

static const int MAX_READ_SIZE = 256;

// Converts 4 bytes (FLOAT_SIZE) in big-endian to float
float bytes_to_float(const uint8_t *b) {
  uint32_t val = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
                 (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
  float result;
  memcpy(&result, &val, sizeof(result));
  return result;
}

uint32_t float_to_uint32(float f) {
  uint32_t fbits = 0;
  memcpy(&fbits, &f, sizeof fbits);
  return fbits;
}

// Converts 4 bytes to an address
uint32_t bytes_to_address(const uint8_t *b) { return ((b[0] & 0x7f) << 24) + (b[1] << 16) + (b[2] << 8) + b[3]; }

// Reverse of bytes_to_address
template<size_t N> void address_to_bytes(uint32_t adr, StaticVector<uint8_t, N> *data) {
  data->push_back(0x80);
  data->push_back(adr >> 16);
  data->push_back(adr >> 8);
  data->push_back(adr);
  data->push_back(0);
}

// Extracts strings of length OBJ_NAME_SIZE in pdata separated by 0x00, 0x00
void extract_obj_names(const uint8_t *pdata, uint8_t data_len, std::vector<std::string> *obj_names) {
  const uint8_t *start = pdata + 4;
  const uint8_t *endp = pdata + data_len;
  while (start < endp) {
    const uint8_t *end = std::min(start + OBJ_NAME_SIZE, endp);
    std::string s((const char *) start, end - start);
    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    obj_names->push_back(std::move(s));
    start = end + 2;
  }
}

// Reverse of extract_obj_names
void join_obj_names(const StaticVector<const std::string *, MAX_OBJECTS_PER_REQUEST> &objects,
                    StaticVector<uint8_t, MAX_MESSAGE_SIZE> *data) {
  for (const auto *s_ptr : objects) {
    const std::string &s = *s_ptr;
    data->push_back(0);
    data->push_back(0);
    size_t s_len = s.length();
    for (size_t j = 0; j < OBJ_NAME_SIZE; j++) {
      data->push_back(j < s_len ? s[j] : 0);
    }
  }
}

std::string trim_trailing_whitespace(const char *p, uint8_t len) {
  const char *endp = p + len;
  while (endp > p && (endp[-1] == ' ' || endp[-1] == '\0')) {
    --endp;
  }
  return std::string(p, endp);
}

// See extract_id_fields() in econet.h.
static const size_t ID_FIELD_SIZE = 24;
// The two fields have never been seen to start before this offset, so don't look earlier.
static const size_t MIN_ID_FIELD_OFFSET = 4;
// A run of printable characters at least this long is taken to be the start of the first field
// rather than part of the header in front of it.
static const size_t MIN_ID_FIELD_RUN = 8;

bool extract_id_fields(const std::vector<uint8_t> &payload, std::string *first, std::string *second) {
  size_t start = 0;
  size_t run = 0;
  for (size_t i = MIN_ID_FIELD_OFFSET; i < payload.size() && run < MIN_ID_FIELD_RUN; i++) {
    const uint8_t c = payload[i];
    if (c > ' ' && c < 0x7F) {
      if (run == 0) {
        start = i;
      }
      run++;
    } else {
      run = 0;
    }
  }
  if (run < MIN_ID_FIELD_RUN || payload.size() < start + 2 * ID_FIELD_SIZE) {
    return false;
  }
  const char *p = (const char *) payload.data() + start;
  *first = trim_trailing_whitespace(p, ID_FIELD_SIZE);
  *second = trim_trailing_whitespace(p + ID_FIELD_SIZE, ID_FIELD_SIZE);
  return true;
}

void Econet::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
  if (this->mcu_connected_binary_sensor_ != nullptr) {
    // Publish the initial disconnected state so the entity doesn't stay unknown until the
    // first message arrives -- which never happens if the MCU is not wired up correctly.
    this->mcu_connected_binary_sensor_->publish_initial_state(this->mcu_connected_);
  }
}

void Econet::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  for (const auto &entry : this->datapoints_) {
    switch (entry.data.type) {
      case EconetDatapointType::FLOAT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s (0x%lx): float value (value: %f)", entry.id.name.c_str(),
                      (unsigned long) entry.id.address, entry.data.value_float);
        break;
      case EconetDatapointType::TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s (0x%lx): text value (value: %s)", entry.id.name.c_str(),
                      (unsigned long) entry.id.address, entry.data.value_string.c_str());
        break;
      case EconetDatapointType::ENUM_TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s (0x%lx): enum value (value: %d : %s)", entry.id.name.c_str(),
                      (unsigned long) entry.id.address, entry.data.value_enum, entry.data.value_string.c_str());
        break;
      case EconetDatapointType::RAW:
        ESP_LOGCONFIG(TAG, "  Datapoint %s (0x%lx): raw value (value: %s)", entry.id.name.c_str(),
                      (unsigned long) entry.id.address, format_hex_pretty(entry.data.value_raw).c_str());
        break;
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGCONFIG(TAG, "  Datapoint %s (0x%lx): UNSUPPORTED", entry.id.name.c_str(),
                      (unsigned long) entry.id.address);
        break;
    }
  }
}

// Makes one request: either the first pending write request or a new read request.
void Econet::make_request_() {
  if (!this->pending_writes_.empty()) {
    const auto &entry = this->pending_writes_.front();
    const auto &dpp = entry.id;
    uint32_t address = dpp.address;
    switch (entry.data.type) {
      case EconetDatapointType::FLOAT:
        this->write_value_(dpp.name, EconetDatapointType::FLOAT, entry.data.value_float, address);
        break;
      case EconetDatapointType::ENUM_TEXT:
        this->write_value_(dpp.name, EconetDatapointType::ENUM_TEXT, entry.data.value_enum, address);
        break;
      case EconetDatapointType::TEXT:
      case EconetDatapointType::RAW:
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGW(TAG, "Unexpected pending write: datapoint %s", dpp.name.c_str());
        break;
    }
    this->pending_writes_.erase(this->pending_writes_.begin());
    return;
  }

  this->request_strings_();
}

void Econet::parse_rx_message_() { this->parse_message_(this->rx_message_.data(), false); }

void Econet::log_unanswered_read_() {
  if (this->unanswered_reads_ > UNANSWERED_READS_BEFORE_BACKOFF &&
      this->loop_now_ - this->last_unanswered_read_log_ < UNANSWERED_READ_LOG_INTERVAL) {
    return;
  }
  this->last_unanswered_read_log_ = this->loop_now_;
  ESP_LOGW(TAG, "New read request while waiting for response to previous read request (%" PRIu32 " in a row)",
           this->unanswered_reads_);
  if (!this->mcu_answers_reads_) {
    // Nothing has ever answered a read, so this is a setup problem rather than a busy MCU.
    // These four causes account for nearly every report of this warning.
    ESP_LOGW(TAG,
             "No read request has ever been answered. Check that the RS485 A and B lines are not swapped, that "
             "tx_pin and rx_pin match your board, that flow_control_pin is set if your RS485 module has a DE/RE "
             "pin, and that dst_address (0x%lx) matches your appliance.",
             (unsigned long) this->dst_adr_);
  }
}

void Econet::parse_message_(const uint8_t *b, bool is_tx) {
  uint32_t dst_adr = bytes_to_address(b + DST_ADR_POS);
  uint32_t src_adr = bytes_to_address(b + SRC_ADR_POS);
  uint8_t data_len = b[LEN_POS];
  uint8_t command = b[COMMAND_POS];
  const uint8_t *pdata = b + MSG_HEADER_SIZE;

  ESP_LOGV(TAG, "%s %s", is_tx ? ">>>" : "<<<",
           format_hex_pretty(b, MSG_HEADER_SIZE + data_len + MSG_CRC_SIZE).c_str());
  ESP_LOGV(TAG, "  Dst Adr : 0x%lx", (unsigned long) dst_adr);
  ESP_LOGV(TAG, "  Src Adr : 0x%lx", (unsigned long) src_adr);
  ESP_LOGV(TAG, "  Command : %d", command);
  ESP_LOGV(TAG, "  Data    : %s", format_hex_pretty(pdata, data_len).c_str());

  if (!is_tx) {
    uint16_t crc = (b[MSG_HEADER_SIZE + data_len]) + (b[MSG_HEADER_SIZE + data_len + 1] << 8);
    uint16_t crc_check = crc16(b, MSG_HEADER_SIZE + data_len, 0);
    if (crc != crc_check) {
      this->read_req_.awaiting_res = false;
      ESP_LOGW(TAG, "Ignoring message with incorrect crc");
      return;
    }

    if (this->src_adr_ != 0 && src_adr == this->src_adr_) {
      ESP_LOGV(TAG, "Ignoring self-echoed message from 0x%lx", (unsigned long) src_adr);
      return;
    }

    if (!this->mcu_connected_) {
      ESP_LOGI(TAG, "Data received from MCU. Marking connected.");
      this->mcu_connected_ = true;
      if (this->mcu_connected_binary_sensor_ != nullptr) {
        this->mcu_connected_binary_sensor_->publish_state(this->mcu_connected_);
      }
    }
    this->last_valid_read_ = this->loop_now_;
  }

  // Track Read Requests
  if (command == READ_COMMAND) {
    if (data_len < 2) {
      this->read_req_.awaiting_res = false;
      ESP_LOGD(TAG, "  READ_COMMAND payload too short (data_len=%d)", data_len);
      return;
    }
    uint8_t type = pdata[0] & 0x7F;
    uint8_t prop_type = pdata[1];

    ESP_LOGV(TAG, "  Type    : %hhu", type);
    ESP_LOGV(TAG, "  PropType: %hhu", prop_type);

    if (type != 1 && type != 2) {
      this->read_req_.awaiting_res = false;
      ESP_LOGD(TAG, "  Don't Currently Support This Class Type %hhu", type);
      return;
    }
    if (prop_type != 1) {
      this->read_req_.awaiting_res = false;
      ESP_LOGD(TAG, "  Don't Currently Support This Property Type %hhu", prop_type);
      return;
    }

    if (this->read_req_.awaiting_res) {
      this->unanswered_reads_++;
      this->log_unanswered_read_();
    }
    std::vector<std::string> obj_names;
    if (data_len > 4) {
      obj_names.reserve((data_len - 4) / (OBJ_NAME_SIZE + 2));
    }
    extract_obj_names(pdata, data_len, &obj_names);
    for (auto &obj_name : obj_names) {
      ESP_LOGV(TAG, "  %s", obj_name.c_str());
    }
    if (!obj_names.empty()) {
      this->read_req_.dst_adr = dst_adr;
      this->read_req_.src_adr = src_adr;
      this->read_req_.type = type;
      this->read_req_.obj_names = std::move(obj_names);
      this->read_req_.awaiting_res = true;
    } else {
      this->read_req_.awaiting_res = false;
    }

  } else if (command == ACK) {
    if (this->read_req_.dst_adr == src_adr && this->read_req_.src_adr == dst_adr && this->read_req_.awaiting_res) {
      if (this->read_req_.type == 1 && this->read_req_.obj_names.size() == 1) {
        if (data_len < 1) {
          ESP_LOGD(TAG, "  ACK payload too short for single-object read (data_len=%d)", data_len);
        } else {
          this->handle_response_(EconetDatapointID{.name = this->read_req_.obj_names[0], .address = src_adr}, pdata,
                                 data_len);
        }
      } else if (this->read_req_.type == 2) {
        // 1st pass to validate response and avoid any buffer over-read
        // We expect: this->read_req_.obj_names.size() sections where each section has 1 byte representing the item_len
        // followed by item_len bytes (see handle_response_).
        int tpos = 0;
        uint8_t item_num = 0;
        while (tpos < data_len) {
          uint8_t item_len = pdata[tpos];
          if (item_len == 0 || tpos + item_len >= data_len) {
            ESP_LOGE(TAG, "Unexpected item length of %d at position %d", item_len, tpos);
            break;
          }
          tpos += item_len + 1;
          item_num++;
        }
        if (item_num != this->read_req_.obj_names.size()) {
          ESP_LOGE(TAG, "We requested %zu objects but we received %d. Ignoring response.",
                   this->read_req_.obj_names.size(), item_num);
        } else {
          // 2nd pass to handle response
          tpos = 0;
          for (size_t i = 0; i < this->read_req_.obj_names.size(); i++) {
            if (tpos >= data_len) {
              ESP_LOGE(TAG, "Internal inconsistency between validation pass and response pass, aborting");
              break;
            }
            const std::string &datapoint_id = this->read_req_.obj_names[i];
            uint8_t item_len = pdata[tpos];
            this->handle_response_(EconetDatapointID{.name = datapoint_id, .address = src_adr}, pdata + tpos + 1,
                                   item_len);
            tpos += item_len + 1;
          }
        }
      }
      this->read_req_.awaiting_res = false;
      this->unanswered_reads_ = 0;
      this->mcu_answers_reads_ = true;
    }
  } else if (command == WRITE_COMMAND) {
    this->read_req_.awaiting_res = false;
    if (data_len < 1) {
      ESP_LOGD(TAG, "  WRITE_COMMAND payload too short (data_len=%d)", data_len);
      return;
    }
    uint8_t type = pdata[0] & 0x7F;
    ESP_LOGV(TAG, "  ClssType: %d", type);
    if (type == 1 && data_len >= WRITE_DATA_POS && pdata[1] == 1) {
      char item_name[OBJ_NAME_SIZE + 1];
      memcpy(item_name, pdata + OBJ_NAME_POS, OBJ_NAME_SIZE);
      item_name[OBJ_NAME_SIZE] = '\0';
      switch (EconetDatapointType(pdata[2] & 0x7F)) {
        case EconetDatapointType::FLOAT:
        case EconetDatapointType::ENUM_TEXT:
          if (data_len == WRITE_DATA_POS + FLOAT_SIZE) {
            float item_value = bytes_to_float(pdata + WRITE_DATA_POS);
            ESP_LOGV(TAG, "  %s: %f", item_name, item_value);
          } else {
            ESP_LOGW(TAG, "  %s: Unexpected Write Data Length", item_name);
          }
          break;
        case EconetDatapointType::RAW:
          ESP_LOGV(TAG, "  %s: %s", item_name,
                   format_hex_pretty(pdata + WRITE_DATA_POS, data_len - WRITE_DATA_POS).c_str());
          break;
        case EconetDatapointType::TEXT:
          ESP_LOGW(TAG, "(Please file an issue with the following line to add support for TEXT)");
          ESP_LOGW(TAG, "  %s: %s", item_name, format_hex_pretty(pdata, data_len).c_str());
          break;
        case EconetDatapointType::UNSUPPORTED:
          ESP_LOGW(TAG, "  %s: UNSUPPORTED", item_name);
          break;
      }
    } else if (type == 7) {
      if (data_len >= 10) {
        ESP_LOGV(TAG, "  DateTime: %04d/%02d/%02d %02d:%02d:%02d.%02d", pdata[9] | pdata[8] << 8, pdata[7], pdata[6],
                 pdata[5], pdata[4], pdata[3], pdata[2]);
      }
    } else if (type == 9) {
      if (!this->dst_adr_configured_ && this->dst_adr_ != src_adr) {
        ESP_LOGW(TAG, "Using 0x%lx as dst_address from now on. File an issue if you see this more than once.",
                 (unsigned long) src_adr);
        this->dst_adr_ = src_adr;
      }
    }
  }
}

// len comes from p[-1] (Class 2) or data_len (Class 1). The caller ensures there won't be any buffer over-read.
// type is at p[0]. If it's a supported non-raw type there are always 3 bytes: 0x80, 0x00, 0x00 followed by actual data.
// For FLOAT it's 4 bytes.
// For TEXT it's some predefined number of bytes depending on the requested object padded with trailing whitespace.
// For ENUM_TEXT it's 1 byte for the enum value, followed by one byte for the length of the enum text, and finally
// followed by the bytes of the enum text padded with trailing whitespace.
void Econet::handle_response_(const EconetDatapointID &datapoint_id, const uint8_t *p, uint8_t len) {
  if (len < 1) {
    ESP_LOGE(TAG, "Response too short for %s", datapoint_id.name.c_str());
    return;
  }
  EconetDatapointType item_type = EconetDatapointType(p[0] & 0x7F);
  switch (item_type) {
    case EconetDatapointType::FLOAT: {
      if (len < 3 + FLOAT_SIZE) {
        ESP_LOGE(TAG, "Expected len of at least %d but was %d for %s", 3 + FLOAT_SIZE, len, datapoint_id.name.c_str());
        return;
      }
      p += 3;
      len -= 3;
      if (len != FLOAT_SIZE) {
        ESP_LOGE(TAG, "Expected len of %d but was %d for %s", FLOAT_SIZE, len, datapoint_id.name.c_str());
        return;
      }
      float item_value = bytes_to_float(p);
      ESP_LOGV(TAG, "  %s : %f", datapoint_id.name.c_str(), item_value);
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = item_value, .type = item_type});
      return;
    }
    case EconetDatapointType::TEXT: {
      if (len < 3) {
        ESP_LOGE(TAG, "Expected len of at least 3 but was %d for %s", len, datapoint_id.name.c_str());
        return;
      }
      p += 3;
      len -= 3;
      std::string s = trim_trailing_whitespace((const char *) p, len);
      ESP_LOGV(TAG, "  %s : (%s)", datapoint_id.name.c_str(), s.c_str());
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = {}, .value_string = std::move(s), .value_float = 0, .type = item_type});
      return;
    }
    case EconetDatapointType::ENUM_TEXT: {
      if (len < 5) {
        ESP_LOGE(TAG, "Expected len of at least 5 but was %d for %s", len, datapoint_id.name.c_str());
        return;
      }
      p += 3;
      len -= 3;
      uint8_t item_value = p[0];
      uint8_t item_text_len = p[1];
      if (item_text_len != len - 2) {
        ESP_LOGE(TAG, "Expected text len of %d but was %d for %s", len - 2, item_text_len, datapoint_id.name.c_str());
        return;
      }
      std::string s = trim_trailing_whitespace((const char *) p + 2, item_text_len);
      ESP_LOGV(TAG, "  %s : %d (%s)", datapoint_id.name.c_str(), item_value, s.c_str());
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = {}, .value_string = std::move(s), .value_enum = item_value, .type = item_type});
      return;
    }
    case EconetDatapointType::RAW: {
      std::vector<uint8_t> raw(p, p + len);
      ESP_LOGV(TAG, "  %s : %s", datapoint_id.name.c_str(), format_hex_pretty(raw).c_str());
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = std::move(raw), .value_string = "", .value_float = 0, .type = item_type});
      return;
    }
    case EconetDatapointType::UNSUPPORTED:
      ESP_LOGW(TAG, "  %s : UNSUPPORTED", datapoint_id.name.c_str());
      this->send_datapoint_(datapoint_id,
                            EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = 0, .type = item_type});
      return;
  }
  ESP_LOGW(TAG, "  %s : Unknown datapoint type %u", datapoint_id.name.c_str(), static_cast<uint8_t>(item_type));
}

void Econet::read_buffer_(int bytes_available) {
  uint8_t bytes[MAX_READ_SIZE];
  int to_read = std::min(bytes_available, MAX_READ_SIZE);

  if (!this->read_array(bytes, to_read)) {
    return;
  }

  for (int i = 0; i < to_read; i++) {
    uint8_t byte = bytes[i];
    this->rx_message_.push_back(byte);
    size_t pos = this->rx_message_.size() - 1;
    if (pos == DST_ADR_POS && byte != 0x80) {
      this->rx_message_.clear();
      continue;
    }
    if (pos == SRC_ADR_POS && byte != 0x80) {
      // A stray 0x80 turnaround byte before a real frame causes SRC_ADR_POS to land inside the
      // real destination address. Resync to the next 0x80 already in the buffer if one exists.
      size_t next_start = 1;
      while (next_start < this->rx_message_.size() && this->rx_message_[next_start] != 0x80) {
        next_start++;
      }
      if (next_start < this->rx_message_.size()) {
        size_t remaining = this->rx_message_.size() - next_start;
        memmove(this->rx_message_.data(), this->rx_message_.data() + next_start, remaining);
        this->rx_message_.resize(remaining);
      } else {
        this->rx_message_.clear();
      }
      continue;
    }

    if (this->rx_message_.size() > LEN_POS &&
        this->rx_message_.size() == MSG_HEADER_SIZE + this->rx_message_[LEN_POS] + MSG_CRC_SIZE) {
      // We have a full message
      this->parse_rx_message_();
      this->rx_message_.clear();
    }
  }
}

void Econet::loop() {
  const uint32_t now = millis();
  this->loop_now_ = now;

  if (this->mcu_connected_ && (now - this->last_valid_read_ > this->mcu_connected_timeout_)) {
    ESP_LOGW(TAG, "No data received from MCU within %" PRIu32 "ms. Marking disconnected.",
             this->mcu_connected_timeout_);
    this->mcu_connected_ = false;
    if (this->mcu_connected_binary_sensor_ != nullptr) {
      this->mcu_connected_binary_sensor_->publish_state(this->mcu_connected_);
    }
  }

  if ((now - this->last_read_data_ > RECEIVE_TIMEOUT) && !this->rx_message_.empty()) {
    ESP_LOGW(TAG, "Ignoring partially received message due to timeout");
    this->rx_message_.clear();
    this->read_req_.awaiting_res = false;
  }

  // Read Everything that is in the buffer
  int bytes_available = this->available();
  if (bytes_available > 0) {
    this->last_read_data_ = now;
    ESP_LOGV(TAG, "Read %d. ms=%" PRIu32, bytes_available, now);
    this->read_buffer_(bytes_available);
    return;
  }

  if (!this->rx_message_.empty()) {
    ESP_LOGD(TAG, "Waiting to fully receive a partially received message");
    return;
  }

  if (now - this->last_request_ <= REQUEST_DELAY || now - this->last_read_data_ <= REQUEST_DELAY) {
    return;
  }

  this->make_request_();
}

void Econet::write_value_(const std::string &object, EconetDatapointType type, float value, uint32_t address) {
  if (address == 0) {
    address = this->dst_adr_;
  }
  StaticVector<uint8_t, MAX_MESSAGE_SIZE> data;

  data.push_back(1);
  data.push_back(1);
  data.push_back((uint8_t) type);
  data.push_back(1);
  data.push_back(0);
  data.push_back(0);

  size_t obj_len = object.length();
  for (size_t j = 0; j < OBJ_NAME_SIZE; j++) {
    data.push_back(j < obj_len ? object[j] : 0);
  }

  uint32_t f_to_32 = float_to_uint32(value);

  data.push_back((uint8_t) (f_to_32 >> 24));
  data.push_back((uint8_t) (f_to_32 >> 16));
  data.push_back((uint8_t) (f_to_32 >> 8));
  data.push_back((uint8_t) (f_to_32));

  this->transmit_message_(WRITE_COMMAND, data.data(), data.size(), address);
}

// A datapoint registered without an explicit src_address (address 0) applies to every device,
// mirroring how send_datapoint_() matches listeners. Lookups pass a resolved destination
// address, so a stored address of 0 is treated as a wildcard.
static bool matches_datapoint_id(const std::vector<EconetDatapointID> &ids, const std::string &name, uint32_t address) {
  for (const auto &id : ids) {
    if (id.name == name && (id.address == 0 || id.address == address)) {
      return true;
    }
  }
  return false;
}

bool Econet::is_request_once_(const std::string &name, uint32_t address) const {
  return matches_datapoint_id(this->request_once_datapoint_ids_, name, address);
}

bool Econet::is_raw_datapoint_(const std::string &name, uint32_t address) const {
  return matches_datapoint_id(this->raw_datapoint_ids_, name, address);
}

bool Econet::is_cached_from_mcu_(const std::string &name, uint32_t address) const {
  return std::find_if(this->datapoints_.begin(), this->datapoints_.end(), [&](const DatapointEntry &e) {
           return e.from_mcu && e.id.name == name && e.id.address == address;
         }) != this->datapoints_.end();
}

void Econet::request_strings_() {
  StaticVector<const std::string *, MAX_OBJECTS_PER_REQUEST> temp_objects;
  uint32_t dst_adr = this->dst_adr_;

  bool is_service_call = false;
  bool service_force_raw = false;

  if (!this->datapoint_ids_for_read_service_.empty()) {
    is_service_call = true;
    const auto &entry = this->datapoint_ids_for_read_service_.front();
    dst_adr = entry.id.address;
    service_force_raw = entry.force_raw;

    // Check if we should request this service item
    EconetDatapointID entry_id{.name = entry.id.name, .address = dst_adr};
    bool request_once = !service_force_raw && this->is_request_once_(entry.id.name, dst_adr);
    auto cached_it = std::find_if(this->datapoints_.begin(), this->datapoints_.end(),
                                  [&](const DatapointEntry &e) { return e.from_mcu && e.id == entry_id; });
    bool exists = cached_it != this->datapoints_.end();

    if (!(request_once && exists)) {
      temp_objects.push_back(&entry.id.name);
    } else {
      // Copy before publishing: send_datapoint_() can push_back into datapoints_, which would
      // reallocate and leave a reference into that vector dangling for the rest of the call.
      EconetDatapoint cached = cached_it->data;
      this->send_datapoint_(entry_id, cached);
    }
  } else {
    // Allow immediate initial boot reads and continuation slices of multi-part request_mods
    // while no previous read is still awaiting a response; otherwise enforce the pacing delay.
    bool urgent_periodic_read = !this->has_read_requested_;
    if (!urgent_periodic_read && !this->read_req_.awaiting_res) {
      for (uint8_t mod : this->request_mods_) {
        if (!this->request_mod_requested_[mod] || this->request_mod_offset_[mod] > 0) {
          urgent_periodic_read = true;
          break;
        }
      }
    }
    if (!urgent_periodic_read && this->loop_now_ - this->last_read_request_ < this->min_delay_between_read_requests_) {
      return;
    }
    this->has_read_requested_ = true;

    for (uint8_t request_mod : this->request_mods_) {
      bool due = !this->request_mod_requested_[request_mod] || this->request_mod_offset_[request_mod] > 0 ||
                 (this->loop_now_ - this->request_mod_last_requested_[request_mod]) >=
                     this->request_mod_update_interval_millis_[request_mod];
      if (!due) {
        continue;
      }

      const auto &datapoint_ids = this->request_datapoint_ids_[request_mod];

      // Resolve the destination for this request_mod up front. An unset (0) request_mod
      // address means "use the component's dst_address", which is the same fallback
      // transmit_message_() applies. Resolving it here keeps the request_once and cache
      // lookups below keyed on the address the response will actually come from.
      dst_adr = this->request_mod_addresses_[request_mod];
      if (dst_adr == 0) {
        dst_adr = this->dst_adr_;
      }

      size_t idx = this->request_mod_offset_[request_mod];
      while (idx < datapoint_ids.size() && temp_objects.size() < MAX_OBJECTS_PER_REQUEST) {
        const auto &id = datapoint_ids[idx];
        if (this->is_request_once_(id, dst_adr) && this->is_cached_from_mcu_(id, dst_adr)) {
          idx++;
          continue;
        }
        if (this->is_raw_datapoint_(id, dst_adr)) {
          if (temp_objects.empty()) {
            temp_objects.push_back(&id);
            idx++;
          }
          // Raw datapoints must be requested individually (Class 1), so stop before or after this item.
          break;
        }
        temp_objects.push_back(&id);
        idx++;
      }

      if (idx < datapoint_ids.size()) {
        this->request_mod_offset_[request_mod] = idx;
      } else {
        this->request_mod_offset_[request_mod] = 0;
        this->request_mod_last_requested_[request_mod] = this->loop_now_;
        this->request_mod_requested_[request_mod] = true;
      }

      if (temp_objects.empty()) {
        // Every remaining item in this request_mod was an already-cached request_once datapoint;
        // check the next due request_mod rather than wasting this loop iteration.
        continue;
      }
      break;
    }
  }

  if (temp_objects.empty()) {
    // If it was a service call that we skipped because it already exists, remove it from queue now
    if (is_service_call && !this->datapoint_ids_for_read_service_.empty()) {
      this->datapoint_ids_for_read_service_.erase(this->datapoint_ids_for_read_service_.begin());
    }
    return;
  }

  this->last_read_request_ = this->loop_now_;

  StaticVector<uint8_t, MAX_MESSAGE_SIZE> data;

  // Read Class
  bool is_raw = (is_service_call && service_force_raw) || this->is_raw_datapoint_(*temp_objects[0], dst_adr);
  if (temp_objects.size() == 1 && is_raw) {
    data.push_back(1);
  } else {
    data.push_back(2);
  }

  // Read Property
  data.push_back(1);

  join_obj_names(temp_objects, &data);

  // If it was a service call, remove it from queue now that we've copied the object name
  if (is_service_call && !this->datapoint_ids_for_read_service_.empty()) {
    this->datapoint_ids_for_read_service_.erase(this->datapoint_ids_for_read_service_.begin());
  }

  this->transmit_message_(READ_COMMAND, data.data(), data.size(), dst_adr);
}

void Econet::transmit_message_(uint8_t command, const uint8_t *data, size_t len, uint32_t dst_adr, uint32_t src_adr) {
  if (len > UINT8_MAX) {
    ESP_LOGE(TAG, "Refusing to transmit payload of %zu bytes (max %u)", len, UINT8_MAX);
    return;
  }
  if (dst_adr == 0) {
    dst_adr = this->dst_adr_;
  }
  if (src_adr == 0) {
    src_adr = this->src_adr_;
  }
  this->last_request_ = this->loop_now_;

  StaticVector<uint8_t, MAX_MESSAGE_SIZE> tx_message;

  address_to_bytes(dst_adr, &tx_message);
  address_to_bytes(src_adr, &tx_message);

  tx_message.push_back(len);
  tx_message.push_back(0);
  tx_message.push_back(0);
  tx_message.push_back(command);
  for (size_t i = 0; i < len; i++) {
    tx_message.push_back(data[i]);
  }

  uint16_t crc = crc16(tx_message.data(), tx_message.size(), 0);
  tx_message.push_back(crc);
  tx_message.push_back(crc >> 8);

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(tx_message.data(), tx_message.size());
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(false);
  }

  this->parse_message_(tx_message.data(), true);
}

void Econet::set_float_datapoint_value(const std::string &datapoint_id, float value, uint32_t address) {
  ESP_LOGD(TAG, "Setting datapoint %s to %f", datapoint_id.c_str(), value);
  this->set_datapoint_(
      EconetDatapointID{.name = datapoint_id, .address = address},
      EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = value, .type = EconetDatapointType::FLOAT});
}

void Econet::set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value, uint32_t address) {
  ESP_LOGD(TAG, "Setting datapoint %s to %u", datapoint_id.c_str(), value);
  this->set_datapoint_(
      EconetDatapointID{.name = datapoint_id, .address = address},
      EconetDatapoint{
          .value_raw = {}, .value_string = "", .value_enum = value, .type = EconetDatapointType::ENUM_TEXT});
}

void Econet::set_datapoint_(const EconetDatapointID &datapoint_id, const EconetDatapoint &value) {
  EconetDatapointID specific = datapoint_id;
  if (specific.address == 0) {
    specific.address = this->dst_adr_;
  }

  auto specific_it = std::find_if(this->datapoints_.begin(), this->datapoints_.end(),
                                  [&](const DatapointEntry &e) { return e.id == specific; });

  if (specific_it == this->datapoints_.end()) {
    bool is_polled = false;
    for (uint8_t mod : this->request_mods_) {
      uint32_t mod_adr = this->request_mod_addresses_[mod];
      if (mod_adr == 0) {
        mod_adr = this->dst_adr_;
      }
      if (mod_adr == specific.address) {
        const auto &ids = this->request_datapoint_ids_[mod];
        if (std::find(ids.begin(), ids.end(), specific.name) != ids.end()) {
          is_polled = true;
          break;
        }
      }
    }
    if (is_polled) {
      ESP_LOGW(TAG, "Writing datapoint %s before any value for it has been read from the MCU",
               datapoint_id.name.c_str());
    }
  } else {
    const EconetDatapoint &old_value = specific_it->data;
    if (old_value.type == EconetDatapointType::UNSUPPORTED) {
      ESP_LOGW(TAG, "Not writing unsupported datapoint %s", datapoint_id.name.c_str());
      return;
    }
    if (old_value.type != value.type) {
      ESP_LOGE(TAG, "Attempt to set datapoint %s with incorrect type", datapoint_id.name.c_str());
      return;
    }
    bool pending =
        std::find_if(this->pending_writes_.begin(), this->pending_writes_.end(),
                     [&](const DatapointEntry &e) { return e.id == specific; }) != this->pending_writes_.end();
    if (specific_it->from_mcu && old_value == value && !pending) {
      ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.name.c_str());
      return;
    }
  }

  auto pw_it = std::find_if(this->pending_writes_.begin(), this->pending_writes_.end(),
                            [&](const DatapointEntry &e) { return e.id == specific; });
  if (pw_it != this->pending_writes_.end()) {
    pw_it->data = value;
  } else {
    this->pending_writes_.push_back({specific, value, false});
  }
  this->send_datapoint_(specific, value, false);
}

void Econet::send_datapoint_(const EconetDatapointID &datapoint_id, const EconetDatapoint &value, bool from_mcu) {
  bool changed = false;

  auto specific_it = std::find_if(this->datapoints_.begin(), this->datapoints_.end(),
                                  [&](const DatapointEntry &e) { return e.id == datapoint_id; });
  if (specific_it == this->datapoints_.end()) {
    this->datapoints_.push_back({datapoint_id, value, from_mcu});
    changed = true;
  } else {
    specific_it->from_mcu = from_mcu;
    if (!(specific_it->data == value)) {
      specific_it->data = value;
      changed = true;
    }
  }

  if (!changed) {
    ESP_LOGV(TAG, "Datapoint %s unchanged", datapoint_id.name.c_str());
  }

  if (this->pending_read_active_ && this->pending_read_id_.name == datapoint_id.name &&
      (this->pending_read_id_.address == 0 || this->pending_read_id_.address == datapoint_id.address)) {
    this->pending_read_result_ = value;
    this->pending_read_received_ = true;
  }

  // listeners_ is ordered by monotonically increasing .id. Tracking last_seen_id and
  // snapshot_end_id lets us notify matching listeners without heap-allocating a snapshot
  // vector on every incoming datapoint, while remaining safe if a callback registers or
  // unregisters listeners (mutating/reallocating this->listeners_).
  const uint32_t snapshot_end_id = this->next_listener_id_;
  uint32_t last_seen_id = 0;
  for (size_t i = 0; i < this->listeners_.size();) {
    const auto &listener = this->listeners_[i];
    if (listener.id >= snapshot_end_id) {
      break;
    }
    if (listener.id <= last_seen_id) {
      i++;
      continue;
    }
    last_seen_id = listener.id;
    if (listener.datapoint_id.name == datapoint_id.name &&
        (listener.datapoint_id.address == 0 || listener.datapoint_id.address == datapoint_id.address) &&
        (changed || listener.one_shot || listener.publish_unchanged)) {
      auto callback = listener.on_datapoint;
      if (listener.one_shot) {
        this->listeners_.erase(this->listeners_.begin() + i);
      } else {
        i++;
      }
      if (callback) {
        callback(value);
      }
      if (i > this->listeners_.size()) {
        i = this->listeners_.size();
      }
      while (i > 0 && this->listeners_[i - 1].id > last_seen_id) {
        i--;
      }
    } else {
      i++;
    }
  }
}

uint32_t Econet::register_listener(const std::string &datapoint_id, int8_t request_mod, bool request_once,
                                   const std::function<void(const EconetDatapoint &)> &func, bool is_raw_datapoint,
                                   uint32_t src_adr, bool one_shot, bool run_existing, bool publish_unchanged) {
  EconetDatapointID dp_id{.name = datapoint_id, .address = src_adr};

  if (request_mod >= 0 && static_cast<size_t>(request_mod) < this->request_datapoint_ids_.size()) {
    // Check if duplicate exists before inserting
    auto &vec = this->request_datapoint_ids_[request_mod];
    if (std::find(vec.begin(), vec.end(), datapoint_id) == vec.end()) {
      vec.push_back(datapoint_id);
    }
    // Check if duplicate exists before inserting
    if (std::find(this->request_mods_.begin(), this->request_mods_.end(), request_mod) == this->request_mods_.end()) {
      this->request_mods_.push_back(request_mod);
    }
    this->min_delay_between_read_requests_ =
        std::max(this->min_update_interval_millis_ / (uint32_t) this->request_mods_.size(),
                 static_cast<uint32_t>(REQUEST_DELAY));
    if (request_once) {
      if (std::find(this->request_once_datapoint_ids_.begin(), this->request_once_datapoint_ids_.end(), dp_id) ==
          this->request_once_datapoint_ids_.end()) {
        this->request_once_datapoint_ids_.push_back(dp_id);
      }
    }
  }
  if (is_raw_datapoint) {
    if (std::find(this->raw_datapoint_ids_.begin(), this->raw_datapoint_ids_.end(), dp_id) ==
        this->raw_datapoint_ids_.end()) {
      this->raw_datapoint_ids_.push_back(dp_id);
    }
  }

  uint32_t id = this->next_listener_id_++;
  this->listeners_.push_back(EconetDatapointListener{
      .id = id,
      .datapoint_id = dp_id,
      .on_datapoint = func,
      .one_shot = one_shot,
      .publish_unchanged = publish_unchanged,
  });

  if (run_existing) {
    // Snapshot matching values before invoking func(), in case func() itself mutates
    // this->datapoints_ (e.g. via a synchronous write) while we're iterating it.
    std::vector<EconetDatapoint> matching;
    for (const auto &entry : this->datapoints_) {
      if (entry.id.name == datapoint_id && (src_adr == 0 || entry.id.address == src_adr)) {
        matching.push_back(entry.data);
        if (one_shot) {
          break;
        }
      }
    }
    if (one_shot && !matching.empty()) {
      this->unregister_listener(id);
    }
    for (const auto &data : matching) {
      func(data);
    }
  }

  return id;
}

bool Econet::unregister_listener(uint32_t listener_id) {
  if (listener_id == 0) {
    return false;
  }
  auto it = std::find_if(this->listeners_.begin(), this->listeners_.end(),
                         [listener_id](const EconetDatapointListener &l) { return l.id == listener_id; });
  if (it == this->listeners_.end()) {
    return false;
  }
  this->listeners_.erase(it);
  return true;
}

// Called from a Home Assistant exposed service to read a datapoint.
std::map<std::string, std::string> Econet::homeassistant_read(const std::string &datapoint_id, uint32_t address) {
  static const uint32_t TIMEOUT_MS = 2000;

  if (address == 0) {
    address = this->dst_adr_;
  }

  // homeassistant_read() blocks (see the loop below), so under normal operation two
  // concurrent calls can't happen -- the API service handler runs on the single main
  // loop task. Refuse rather than clobber pending_read_* if it ever does.
  if (this->pending_read_active_) {
    ESP_LOGE(TAG, "homeassistant_read(%s) called while another read is pending; refusing", datapoint_id.c_str());
    return {};
  }

  this->pending_read_active_ = true;
  this->pending_read_id_ = EconetDatapointID{.name = datapoint_id, .address = address};
  this->pending_read_result_ = {};
  this->pending_read_received_ = false;

  const uint32_t start_time = millis();
  auto wait_for_read = [&](bool force_raw) -> bool {
    this->pending_read_received_ = false;
    this->datapoint_ids_for_read_service_.push_back(
        ServiceReadRequest{.id = EconetDatapointID{.name = datapoint_id, .address = address}, .force_raw = force_raw});
    while (!this->pending_read_received_) {
      if (millis() - start_time > TIMEOUT_MS) {
        ESP_LOGW(TAG, "Timeout waiting for datapoint %s response", datapoint_id.c_str());
        auto queue_it = std::find_if(this->datapoint_ids_for_read_service_.begin(),
                                     this->datapoint_ids_for_read_service_.end(), [&](const ServiceReadRequest &req) {
                                       return req.id.name == datapoint_id && req.id.address == address &&
                                              req.force_raw == force_raw;
                                     });
        if (queue_it != this->datapoint_ids_for_read_service_.end()) {
          this->datapoint_ids_for_read_service_.erase(queue_it);
        }
        return false;
      }
      this->loop();
      App.feed_wdt();
      delay(1);
    }
    return true;
  };

  bool known_raw = this->is_raw_datapoint_(datapoint_id, address);
  bool ok = wait_for_read(false);
  if (ok && !known_raw && this->pending_read_result_.type == EconetDatapointType::UNSUPPORTED) {
    // RAW datapoints return UNSUPPORTED when queried via Class 2; retry once as a Class 1
    // single-object read, preserving the initial UNSUPPORTED result if the retry times out.
    (void) wait_for_read(true);
  }

  std::map<std::string, std::string> data;
  if (ok) {
    const EconetDatapoint &dp = this->pending_read_result_;
    data["datapoint_id"] = datapoint_id;
    switch (dp.type) {
      case EconetDatapointType::FLOAT:
        data["type"] = "FLOAT";
        data["value"] = std::to_string(dp.value_float);
        break;
      case EconetDatapointType::ENUM_TEXT:
        data["type"] = "ENUM_TEXT";
        data["value"] = std::to_string(dp.value_enum);
        data["value_string"] = dp.value_string;
        break;
      case EconetDatapointType::TEXT:
        data["type"] = "TEXT";
        data["value_string"] = dp.value_string;
        break;
      case EconetDatapointType::RAW:
        data["type"] = "RAW";
        data["value_raw"] = format_hex_pretty(dp.value_raw);
        break;
      case EconetDatapointType::UNSUPPORTED:
        data["type"] = "UNSUPPORTED";
        break;
    }
  }

  this->pending_read_active_ = false;
  this->pending_read_id_ = {};
  this->pending_read_result_ = {};
  this->pending_read_received_ = false;
  return data;
}

void Econet::homeassistant_write(const std::string &datapoint_id, uint8_t value, uint32_t address) {
  this->set_datapoint_(
      EconetDatapointID{.name = datapoint_id, .address = address},
      EconetDatapoint{
          .value_raw = {}, .value_string = "", .value_enum = value, .type = EconetDatapointType::ENUM_TEXT});
}

void Econet::homeassistant_write(const std::string &datapoint_id, float value, uint32_t address) {
  this->set_datapoint_(
      EconetDatapointID{.name = datapoint_id, .address = address},
      EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = value, .type = EconetDatapointType::FLOAT});
}

}  // namespace econet
}  // namespace esphome
