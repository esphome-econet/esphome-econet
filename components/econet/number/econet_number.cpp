#include "econet_number.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace econet {

static const char *const TAG = "econet.number";

void EconetNumber::setup() {
  this->parent_->register_listener(
      this->number_id_, this->request_mod_, this->request_once_,
      [this](const EconetDatapoint &datapoint) {
        if (datapoint.type == EconetDatapointType::FLOAT) {
          ESP_LOGV(TAG, "MCU reported number %s is: %f", this->number_id_, datapoint.value_float);
          this->publish_state(datapoint.value_float);
        } else if (datapoint.type == EconetDatapointType::ENUM_TEXT) {
          ESP_LOGV(TAG, "MCU reported number %s is: %u", this->number_id_, datapoint.value_enum);
          this->publish_state(datapoint.value_enum);
        }
        this->type_ = datapoint.type;
      },
      false, this->src_adr_);
}

void EconetNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %s: %f", this->number_id_, value);
  if (this->type_ == EconetDatapointType::FLOAT) {
    this->parent_->set_float_datapoint_value(this->number_id_, value, this->src_adr_);
  } else if (this->type_ == EconetDatapointType::ENUM_TEXT) {
    // The datapoint carries a uint8_t. Round rather than truncate, and clamp: min_value or
    // max_value may fall outside 0-255, and an out-of-range float-to-integer conversion is
    // undefined behavior.
    float rounded = roundf(value);
    if (rounded < 0.0f || rounded > 255.0f) {
      ESP_LOGW(TAG, "Value %f for number %s is out of range for an enum datapoint", value, this->number_id_);
      return;
    }
    this->parent_->set_enum_datapoint_value(this->number_id_, static_cast<uint8_t>(rounded), this->src_adr_);
  } else {
    // No write was sent, so don't publish the new value: doing so would report success in
    // Home Assistant for a change that never reached the unit.
    ESP_LOGW(TAG, "Cannot write number %s: datapoint type is not writable", this->number_id_);
    return;
  }
  this->publish_state(value);
}

void EconetNumber::dump_config() {
  LOG_NUMBER("", "Econet Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %s", this->number_id_);
}

}  // namespace econet
}  // namespace esphome
