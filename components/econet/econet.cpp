#include "econet.h"

#include <cinttypes>

namespace esphome {
namespace econet {

static const char *const TAG = "econet";

static const uint32_t RECEIVE_TIMEOUT = 100;
static const uint32_t REQUEST_DELAY = 100;

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
void address_to_bytes(uint32_t adr, std::vector<uint8_t> *data) {
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
    s.erase(remove(s.begin(), s.end(), '\00'), s.end());
    obj_names->push_back(s);
    start = end + 2;
  }
}

// Reverse of extract_obj_names
void join_obj_names(const std::vector<std::string> &objects, std::vector<uint8_t> *data) {
  for (const auto &s : objects) {
    data->push_back(0);
    data->push_back(0);
    size_t s_len = s.length();
    for (int j = 0; j < OBJ_NAME_SIZE; j++) {
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

void Econet::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
  this->rx_message_.reserve(256);
  this->tx_message_.reserve(256);
}

void Econet::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  for (auto &kv : this->datapoints_) {
    switch (kv.second.type) {
      case EconetDatapointType::FLOAT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: float value (value: %f)", kv.first.name.c_str(), kv.second.value_float);
        break;
      case EconetDatapointType::TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: text value (value: %s)", kv.first.name.c_str(),
                      kv.second.value_string.c_str());
        break;
      case EconetDatapointType::ENUM_TEXT:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: enum value (value: %d : %s)", kv.first.name.c_str(), kv.second.value_enum,
                      kv.second.value_string.c_str());
        break;
      case EconetDatapointType::RAW:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: raw value (value: %s)", kv.first.name.c_str(),
                      format_hex_pretty(kv.second.value_raw).c_str());
        break;
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGCONFIG(TAG, "  Datapoint %s: UNSUPPORTED", kv.first.name.c_str());
        break;
    }
  }
}

// Makes one request: either the first pending write request or a new read request.
void Econet::make_request_() {
  if (!this->pending_writes_.empty()) {
    const auto &kv = this->pending_writes_.begin();
    const auto &dpp = kv->first;
    uint32_t address = dpp.address;
    switch (kv->second.type) {
      case EconetDatapointType::FLOAT:
        this->write_value_(dpp.name, EconetDatapointType::FLOAT, kv->second.value_float, address);
        break;
      case EconetDatapointType::ENUM_TEXT:
        this->write_value_(dpp.name, EconetDatapointType::ENUM_TEXT, kv->second.value_enum, address);
        break;
      case EconetDatapointType::TEXT:
      case EconetDatapointType::RAW:
      case EconetDatapointType::UNSUPPORTED:
        ESP_LOGW(TAG, "Unexpected pending write: datapoint %s", dpp.name.c_str());
        break;
    }
    this->pending_writes_.erase(kv);
    return;
  }

  this->request_strings_();
}

void Econet::parse_tx_message_() { this->parse_message_(true); }

void Econet::parse_rx_message_() { this->parse_message_(false); }

void Econet::parse_message_(bool is_tx) {
  const uint8_t *b = is_tx ? &this->tx_message_[0] : &this->rx_message_[0];

  uint32_t dst_adr = bytes_to_address(b + DST_ADR_POS);
  uint32_t src_adr = bytes_to_address(b + SRC_ADR_POS);
  uint8_t data_len = b[LEN_POS];
  uint8_t command = b[COMMAND_POS];
  const uint8_t *pdata = b + MSG_HEADER_SIZE;

  ESP_LOGI(TAG, "%s %s", is_tx ? ">>>" : "<<<",
           format_hex_pretty(b, MSG_HEADER_SIZE + data_len + MSG_CRC_SIZE).c_str());
  ESP_LOGI(TAG, "  Dst Adr : 0x%x", dst_adr);
  ESP_LOGI(TAG, "  Src Adr : 0x%x", src_adr);
  ESP_LOGI(TAG, "  Command : %d", command);
  ESP_LOGI(TAG, "  Data    : %s", format_hex_pretty(pdata, data_len).c_str());

  uint16_t crc = (b[MSG_HEADER_SIZE + data_len]) + (b[MSG_HEADER_SIZE + data_len + 1] << 8);
  uint16_t crc_check = crc16(b, MSG_HEADER_SIZE + data_len, 0);
  if (crc != crc_check) {
    this->read_req_.awaiting_res = false;
    ESP_LOGW(TAG, "Ignoring message with incorrect crc");
    return;
  }

  if (!is_tx) {
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
    uint8_t type = pdata[0] & 0x7F;
    uint8_t prop_type = pdata[1];

    ESP_LOGI(TAG, "  Type    : %hhu", type);
    ESP_LOGI(TAG, "  PropType: %hhu", prop_type);

    if (type != 1 && type != 2) {
      ESP_LOGI(TAG, "  Don't Currently Support This Class Type %hhu", type);
      return;
    }
    if (prop_type != 1) {
      ESP_LOGI(TAG, "  Don't Currently Support This Property Type %hhu", prop_type);
      return;
    }

    if (this->read_req_.awaiting_res) {
      ESP_LOGW(TAG, "New read request while waiting for response to previous read request");
    }
    std::vector<std::string> obj_names;
    if (data_len > 4) {
      obj_names.reserve((data_len - 4) / (OBJ_NAME_SIZE + 2));
    }
    extract_obj_names(pdata, data_len, &obj_names);
    for (auto &obj_name : obj_names) {
      ESP_LOGI(TAG, "  %s", obj_name.c_str());
    }
    if (!obj_names.empty()) {
      this->read_req_.dst_adr = dst_adr;
      this->read_req_.src_adr = src_adr;
      this->read_req_.type = type;
      this->read_req_.obj_names = obj_names;
      this->read_req_.awaiting_res = true;
    }

  } else if (command == ACK) {
    if (this->read_req_.dst_adr == src_adr && this->read_req_.src_adr == dst_adr && this->read_req_.awaiting_res) {
      if (this->read_req_.type == 1 && this->read_req_.obj_names.size() == 1) {
        EconetDatapointType item_type = EconetDatapointType(pdata[0] & 0x7F);
        if (item_type == EconetDatapointType::RAW) {
          std::vector<uint8_t> raw(pdata, pdata + data_len);
          const std::string &datapoint_id = this->read_req_.obj_names[0];
          this->send_datapoint_(
              EconetDatapointID{.name = datapoint_id, .address = src_adr},
              EconetDatapoint{.value_raw = raw, .value_string = "", .value_float = 0, .type = item_type});
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
          ESP_LOGE(TAG, "We requested %d objects but we received %d. Ignoring response.",
                   this->read_req_.obj_names.size(), item_num);
        } else {
          // 2nd pass to handle response
          tpos = 0;
          item_num = 0;
          while (tpos < data_len) {
            const std::string &datapoint_id = this->read_req_.obj_names[item_num];
            uint8_t item_len = pdata[tpos];
            this->handle_response_(EconetDatapointID{.name = datapoint_id, .address = src_adr}, pdata + tpos + 1,
                                   item_len);
            tpos += item_len + 1;
            item_num++;
          }
        }
      }
      this->read_req_.awaiting_res = false;
    }
  } else if (command == WRITE_COMMAND) {
    uint8_t type = pdata[0];
    ESP_LOGI(TAG, "  ClssType: %d", type);
    if (type == 1 && pdata[1] == 1 && data_len >= WRITE_DATA_POS) {
      std::string item_name((const char *) pdata + OBJ_NAME_POS, OBJ_NAME_SIZE);
      switch (EconetDatapointType(pdata[2])) {
        case EconetDatapointType::FLOAT:
        case EconetDatapointType::ENUM_TEXT:
          if (data_len == WRITE_DATA_POS + FLOAT_SIZE) {
            float item_value = bytes_to_float(pdata + WRITE_DATA_POS);
            ESP_LOGI(TAG, "  %s: %f", item_name.c_str(), item_value);
          } else {
            ESP_LOGW(TAG, "  %s: Unexpected Write Data Length", item_name.c_str());
          }
          break;
        case EconetDatapointType::RAW:
          ESP_LOGI(TAG, "  %s: %s", item_name.c_str(),
                   format_hex_pretty(pdata + WRITE_DATA_POS, data_len - WRITE_DATA_POS).c_str());
          break;
        case EconetDatapointType::TEXT:
          ESP_LOGW(TAG, "(Please file an issue with the following line to add support for TEXT)");
          ESP_LOGW(TAG, "  %s: %s", item_name.c_str(), format_hex_pretty(pdata, data_len).c_str());
          break;
        case EconetDatapointType::UNSUPPORTED:
          ESP_LOGW(TAG, "  %s: UNSUPPORTED", item_name.c_str());
          break;
      }
    } else if (type == 7) {
      ESP_LOGI(TAG, "  DateTime: %04d/%02d/%02d %02d:%02d:%02d.%02d\n", pdata[9] | pdata[8] << 8, pdata[7], pdata[6],
               pdata[5], pdata[4], pdata[3], pdata[2]);
    } else if (type == 9) {
      if (this->dst_adr_ != src_adr) {
        ESP_LOGW(TAG, "Using 0x%x as dst_address from now on. File an issue if you see this more than once.", src_adr);
        this->dst_adr_ = src_adr;
      }
    }
  }
}

// len comes from p[-1], see caller. The caller is responsible for ensuring there won't be any buffer over-read.
// type is at p[0]. If it's a supported type there is always 3 bytes: 0x80, 0x00, 0x00 followed by the actual data.
// For FLOAT it's 4 bytes.
// For TEXT it's some predefined number of bytes depending on the requested object padded with trailing whitespace.
// For ENUM_TEXT it's 1 byte for the enum value, followed by one byte for the length of the enum text, and finally
// followed by the bytes of the enum text padded with trailing whitespace.
void Econet::handle_response_(const EconetDatapointID &datapoint_id, const uint8_t *p, uint8_t len) {
  EconetDatapointType item_type = EconetDatapointType(p[0] & 0x7F);
  switch (item_type) {
    case EconetDatapointType::FLOAT: {
      p += 3;
      len -= 3;
      if (len != FLOAT_SIZE) {
        ESP_LOGE(TAG, "Expected len of %d but was %d for %s", FLOAT_SIZE, len, datapoint_id.name.c_str());
        return;
      }
      float item_value = bytes_to_float(p);
      ESP_LOGI(TAG, "  %s : %f", datapoint_id.name.c_str(), item_value);
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = item_value, .type = item_type});
      break;
    }
    case EconetDatapointType::TEXT: {
      p += 3;
      len -= 3;
      std::string s = trim_trailing_whitespace((const char *) p, len);
      ESP_LOGI(TAG, "  %s : (%s)", datapoint_id.name.c_str(), s.c_str());
      this->send_datapoint_(datapoint_id,
                            EconetDatapoint{.value_raw = {}, .value_string = s, .value_float = 0, .type = item_type});
      break;
    }
    case EconetDatapointType::ENUM_TEXT: {
      p += 3;
      len -= 3;
      if (len < 2) {
        ESP_LOGE(TAG, "Expected len of at least 2 but was %d for %s", len, datapoint_id.name.c_str());
        return;
      }
      uint8_t item_value = p[0];
      uint8_t item_text_len = p[1];
      if (item_text_len != len - 2) {
        ESP_LOGE(TAG, "Expected text len of %d but was %d for %s", len - 2, item_text_len, datapoint_id.name.c_str());
        return;
      }
      std::string s = trim_trailing_whitespace((const char *) p + 2, item_text_len);
      ESP_LOGI(TAG, "  %s : %d (%s)", datapoint_id.name.c_str(), item_value, s.c_str());
      this->send_datapoint_(
          datapoint_id,
          EconetDatapoint{.value_raw = {}, .value_string = s, .value_enum = item_value, .type = item_type});
      break;
    }
    case EconetDatapointType::RAW:
      // Handled separately since it seems it cannot be requested together with other objects.
      break;
    case EconetDatapointType::UNSUPPORTED:
      ESP_LOGW(TAG, "  %s : UNSUPPORTED", datapoint_id.name.c_str());
      this->send_datapoint_(datapoint_id,
                            EconetDatapoint{.value_raw = {}, .value_string = "", .value_float = 0, .type = item_type});
      break;
  }
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
    if ((pos == DST_ADR_POS || pos == SRC_ADR_POS) && byte != 0x80) {
      this->rx_message_.clear();
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
    ESP_LOGI(TAG, "Read %d. ms=%" PRIu32, bytes_available, now);
    this->read_buffer_(bytes_available);
    return;
  }

  if (!this->rx_message_.empty()) {
    ESP_LOGD(TAG, "Waiting to fully receive a partially received message");
    return;
  }

  if (now - this->last_request_ <= REQUEST_DELAY) {
    return;
  }

  this->make_request_();
}

void Econet::write_value_(const std::string &object, EconetDatapointType type, float value, uint32_t address) {
  if (address == 0) {
    address = this->dst_adr_;
  }
  std::vector<uint8_t> data;
  data.reserve(6 + OBJ_NAME_SIZE + FLOAT_SIZE);

  data.push_back(1);
  data.push_back(1);
  data.push_back((uint8_t) type);
  data.push_back(1);
  data.push_back(0);
  data.push_back(0);

  size_t obj_len = object.length();
  for (int j = 0; j < OBJ_NAME_SIZE; j++) {
    data.push_back(j < obj_len ? object[j] : 0);
  }

  uint32_t f_to_32 = float_to_uint32(value);

  data.push_back((uint8_t) (f_to_32 >> 24));
  data.push_back((uint8_t) (f_to_32 >> 16));
  data.push_back((uint8_t) (f_to_32 >> 8));
  data.push_back((uint8_t) (f_to_32));

  this->transmit_message_(WRITE_COMMAND, data, address);
}

void Econet::request_strings_() {
  this->temp_objects_.clear();
  uint32_t dst_adr = this->dst_adr_;
  if (!this->datapoint_ids_for_read_service_.empty()) {
    this->temp_objects_.push_back(this->datapoint_ids_for_read_service_.front().name);
    dst_adr = this->datapoint_ids_for_read_service_.front().address;
    this->datapoint_ids_for_read_service_.pop();
  } else {
    // Impose a longer delay restriction for general periodically requested messages
    if (this->loop_now_ - this->last_read_request_ < this->min_delay_between_read_requests_) {
      return;
    }
    for (uint8_t request_mod : this->request_mods_) {
      if ((this->loop_now_ - this->request_mod_last_requested_[request_mod]) >=
          this->request_mod_update_interval_millis_[request_mod]) {
        const auto &datapoint_ids = this->request_datapoint_ids_[request_mod];
        this->temp_objects_.reserve(datapoint_ids.size());
        for (const auto &id : datapoint_ids) {
          this->temp_objects_.push_back(id);
        }
        this->request_mod_last_requested_[request_mod] = this->loop_now_;
        dst_adr = this->request_mod_addresses_[request_mod];
        break;
      }
    }
  }
  std::vector<std::string>::iterator iter;
  for (iter = this->temp_objects_.begin(); iter != this->temp_objects_.end();) {
    if (this->request_once_datapoint_ids_.count(EconetDatapointID{.name = *iter, .address = dst_adr}) == 1 &&
        this->datapoints_.count(EconetDatapointID{.name = *iter, .address = dst_adr}) == 1) {
      iter = this->temp_objects_.erase(iter);

    } else {
      ++iter;
    }
  }
  if (this->temp_objects_.empty()) {
    return;
  }

  this->last_read_request_ = this->loop_now_;

  std::vector<uint8_t> data;
  data.reserve(2 + this->temp_objects_.size() * (2 + OBJ_NAME_SIZE));

  // Read Class
  if (this->temp_objects_.size() == 1 &&
      this->raw_datapoint_ids_.count(EconetDatapointID{.name = this->temp_objects_[0], .address = dst_adr}) == 1) {
    data.push_back(1);
  } else {
    data.push_back(2);
  }

  // Read Property
  data.push_back(1);

  join_obj_names(this->temp_objects_, &data);

  this->transmit_message_(READ_COMMAND, data, dst_adr);
}

void Econet::transmit_message_(uint8_t command, const std::vector<uint8_t> &data, uint32_t dst_adr, uint32_t src_adr) {
  if (dst_adr == 0) {
    dst_adr = this->dst_adr_;
  }
  if (src_adr == 0) {
    src_adr = this->src_adr_;
  }
  this->last_request_ = this->loop_now_;

  this->tx_message_.clear();

  address_to_bytes(dst_adr, &this->tx_message_);
  address_to_bytes(src_adr, &this->tx_message_);

  this->tx_message_.push_back(data.size());
  this->tx_message_.push_back(0);
  this->tx_message_.push_back(0);
  this->tx_message_.push_back(command);
  this->tx_message_.insert(this->tx_message_.end(), data.begin(), data.end());

  uint16_t crc = crc16(&this->tx_message_[0], this->tx_message_.size(), 0);
  this->tx_message_.push_back(crc);
  this->tx_message_.push_back(crc >> 8);

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(&this->tx_message_[0], this->tx_message_.size());
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(false);
  }

  this->parse_tx_message_();
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
  const EconetDatapointID any{.name = datapoint_id.name, .address = 0};
  bool send_specific = true;
  bool send_any = true;

  auto specific_it = this->datapoints_.find(specific);
  if (specific_it == this->datapoints_.end()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %s", datapoint_id.name.c_str());
  } else {
    const EconetDatapoint &old_value = specific_it->second;
    if (old_value.type != value.type) {
      ESP_LOGE(TAG, "Attempt to set datapoint %s with incorrect type", datapoint_id.name.c_str());
      return;
    }
    if (old_value == value && this->pending_writes_.find(specific) == this->pending_writes_.end()) {
      ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.name.c_str());
      send_specific = false;
    }
  }

  auto any_it = this->datapoints_.find(any);
  if (any_it == this->datapoints_.end()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %s", datapoint_id.name.c_str());
  } else if (any_it->second == value) {
    ESP_LOGV(TAG, "Not setting unchanged value for datapoint %s", datapoint_id.name.c_str());
    send_any = false;
  }

  if (send_specific) {
    this->pending_writes_[specific] = value;
    this->send_datapoint_(specific, value);
  } else if (send_any) {
    this->send_datapoint_(specific, value);
  }
}

void Econet::send_datapoint_(const EconetDatapointID &datapoint_id, const EconetDatapoint &value) {
  const EconetDatapointID any_id{.name = datapoint_id.name, .address = 0};
  bool changed = false;

  auto specific_it = this->datapoints_.find(datapoint_id);
  if (specific_it == this->datapoints_.end()) {
    this->datapoints_.emplace(datapoint_id, value);
    changed = true;
  } else if (!(specific_it->second == value)) {
    specific_it->second = value;
    changed = true;
  }

  auto any_it = this->datapoints_.find(any_id);
  if (any_it == this->datapoints_.end()) {
    this->datapoints_.emplace(any_id, value);
    changed = true;
  } else if (!(any_it->second == value)) {
    any_it->second = value;
    changed = true;
  }

  if (changed) {
    for (const auto &listener : this->listeners_) {
      if (listener.datapoint_id.name == datapoint_id.name &&
          (listener.datapoint_id.address == 0 || listener.datapoint_id.address == datapoint_id.address)) {
        listener.on_datapoint(value);
      }
    }
  } else {
    ESP_LOGV(TAG, "Not sending unchanged value for datapoint %s", datapoint_id.name.c_str());
  }
}

void Econet::register_listener(const std::string &datapoint_id, int8_t request_mod, bool request_once,
                               const std::function<void(const EconetDatapoint &)> &func, bool is_raw_datapoint,
                               uint32_t src_adr) {
  EconetDatapointID dp_id{.name = datapoint_id, .address = src_adr};

  if (request_mod >= 0 && static_cast<size_t>(request_mod) < this->request_datapoint_ids_.size()) {
    this->request_datapoint_ids_[request_mod].insert(datapoint_id);
    this->request_mods_.insert(request_mod);
    this->min_delay_between_read_requests_ =
        std::max(this->min_update_interval_millis_ / this->request_mods_.size(), static_cast<uint32_t>(REQUEST_DELAY));
    if (request_once) {
      this->request_once_datapoint_ids_.insert(dp_id);
    }
  }
  if (is_raw_datapoint) {
    this->raw_datapoint_ids_.insert(dp_id);
  }

  this->listeners_.push_back(EconetDatapointListener{
      .datapoint_id = dp_id,
      .on_datapoint = func,
  });

  // Run through existing datapoints
  for (const auto &kv : this->datapoints_) {
    if (kv.first.name == datapoint_id && (kv.first.address == src_adr || kv.first.address == 0)) {
      func(kv.second);
    }
  }
}

// Called from a Home Assistant exposed service to read a datapoint.
// Fires a Home Assistant event: "esphome.econet_event" with the response.
void Econet::homeassistant_read(const std::string &datapoint_id, uint32_t address) {
  if (address == 0) {
    address = this->dst_adr_;
  }
  this->register_listener(datapoint_id, -1, true, [this, datapoint_id](const EconetDatapoint &datapoint) {
    std::map<std::string, std::string> data;
    data["datapoint_id"] = datapoint_id;
    switch (datapoint.type) {
      case EconetDatapointType::FLOAT:
        data["type"] = "FLOAT";
        data["value"] = std::to_string(datapoint.value_float);
        break;
      case EconetDatapointType::ENUM_TEXT:
        data["type"] = "ENUM_TEXT";
        data["value"] = std::to_string(datapoint.value_enum);
        data["value_string"] = datapoint.value_string;
        break;
      case EconetDatapointType::TEXT:
        data["type"] = "TEXT";
        data["value_string"] = datapoint.value_string;
        break;
      case EconetDatapointType::RAW:
        data["type"] = "RAW";
        data["value_raw"] = format_hex_pretty(datapoint.value_raw);
        break;
      case EconetDatapointType::UNSUPPORTED:
        data["type"] = "UNSUPPORTED";
        break;
    }
    this->capi_.fire_homeassistant_event("esphome.econet_event", data);
  });
  this->datapoint_ids_for_read_service_.push(EconetDatapointID{.name = datapoint_id, .address = address});
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
