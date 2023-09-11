#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include <map>
#include <vector>
#include <set>

namespace esphome {
namespace econet {

enum ModelType { MODEL_TYPE_TANKLESS = 0, MODEL_TYPE_HEATPUMP = 1, MODEL_TYPE_HVAC = 2 };

class ReadRequest {
 public:
  uint32_t dst_adr;
  uint8_t dst_bus;

  uint32_t src_adr;
  uint8_t src_bus;

  bool awaiting_res = false;

  std::vector<std::string> obj_names;
};

enum class EconetDatapointType : uint8_t {
  FLOAT = 0,
  TEXT = 1,
  ENUM_TEXT = 2,
  RAW = 4,
};

struct EconetDatapoint {
  EconetDatapointType type;
  union {
    float value_float;
    uint8_t value_enum;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};
inline bool operator==(const EconetDatapoint &lhs, const EconetDatapoint &rhs) {
  if (lhs.type != rhs.type) {
    return false;
  }
  switch (lhs.type) {
    case EconetDatapointType::FLOAT:
      return lhs.value_float == rhs.value_float;
    case EconetDatapointType::TEXT:
      return lhs.value_string == rhs.value_string;
    case EconetDatapointType::ENUM_TEXT:
      return lhs.value_enum == rhs.value_enum;
    case EconetDatapointType::RAW:
      return lhs.value_raw == rhs.value_raw;
  }
  return false;
}

struct EconetDatapointListener {
  std::string datapoint_id;
  std::function<void(EconetDatapoint)> on_datapoint;
};

class Econet : public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

  void set_model_type(ModelType model_type) { model_type_ = model_type; }
  ModelType get_model_type() { return model_type_; }

  void set_hvac_wifi_module_connected(bool hvac_wifi_module_connected) {
    hvac_wifi_module_connected_ = hvac_wifi_module_connected;
  }

  void set_float_datapoint_value(const std::string &datapoint_id, float value);
  void set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value);

  void register_listener(const std::string &datapoint_id, const std::function<void(EconetDatapoint)> &func);

 protected:
  ModelType model_type_;
  std::vector<EconetDatapointListener> listeners_;
  ReadRequest read_req;
  void set_datapoint(const std::string &datapoint_id, EconetDatapoint value);
  void send_datapoint(const std::string &datapoint_id, EconetDatapoint value, bool skip_update_state = false);

  void make_request();
  void read_buffer(int bytes_available);
  void parse_message(bool is_tx);
  void parse_rx_message();
  void parse_tx_message();

  void transmit_message(uint32_t dst_adr, uint32_t src_adr, uint8_t command, const std::vector<uint8_t> &data);
  void request_strings(uint32_t dst_adr, uint32_t src_adr, const std::vector<std::string> &objects);
  void write_value(uint32_t dst_adr, uint32_t src_adr, const std::string &object, EconetDatapointType type,
                   float value);

  std::set<std::string> datapoint_ids_{};
  std::map<std::string, EconetDatapoint> datapoints_{};
  std::map<std::string, EconetDatapoint> pending_writes_{};
  std::map<std::string, EconetDatapoint> pending_confirmation_writes_{};

  bool hvac_wifi_module_connected_ = true;

  uint8_t req_id = 0;
  uint32_t last_request_{0};
  uint32_t last_read_{0};
  uint32_t last_read_data_{0};
  uint32_t act_loop_time_{0};
  uint8_t data_len = 0;
  uint16_t msg_len = 0;
  int pos = 0;
  static const int max_message_size = 271;
  uint8_t buffer[max_message_size];

  uint8_t wbuffer[max_message_size];
  uint16_t wmsg_len = 0;

  uint32_t dst_adr = 0;

  uint32_t FURNACE = 0x1c0;
  uint32_t WIFI_MODULE = 0x340;
  uint32_t SMARTEC_TRANSLATOR = 0x1040;
  uint32_t HEAT_PUMP_WATER_HEATER = 0x1280;
  uint32_t CONTROL_CENTER = 0x380;

  uint8_t DST_ADR_POS = 0;
  uint8_t SRC_ADR_POS = 5;
  uint8_t SRC_BUS_POS = 9;
  uint8_t LEN_POS = 10;
  uint8_t COMMAND_POS = 13;
  uint8_t DATA_START_POS = 14;

  uint8_t MSG_HEADER_SIZE = 14;
  uint8_t BYTES_BETWEEN_ADRS = 5;
  uint8_t MSG_CRC_SIZE = 2;

  uint8_t ACK = 0x06;
  uint8_t READ_COMMAND = 0x1E;   // 30
  uint8_t WRITE_COMMAND = 0x1F;  // 31
};

class EconetClient {
 public:
  void set_econet_parent(Econet *parent) { this->parent_ = parent; }

 protected:
  Econet *parent_;
};

}  // namespace econet
}  // namespace esphome
