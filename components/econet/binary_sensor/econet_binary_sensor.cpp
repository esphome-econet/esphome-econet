#include "esphome/core/log.h"
#include "econet_binary_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.binary_sensor";

void EconetBinarySensor::setup() {
  this->parent_->register_listener(
      this->sensor_id_, this->request_mod_, this->request_once_,
      [this](const EconetDatapoint &datapoint) {
        if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
          return;
        }
        if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
          ESP_LOGW(TAG, "Unexpected datapoint type %u for binary sensor %s", static_cast<uint8_t>(datapoint.type),
                   this->sensor_id_);
          return;
        }
        ESP_LOGV(TAG, "MCU reported binary sensor %s is: %s", this->sensor_id_, ONOFF(datapoint.value_enum));
        this->publish_state(datapoint.value_enum);
      },
      false, this->src_adr_);
}

void EconetBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Econet Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Binary Sensor has datapoint ID %s", this->sensor_id_);
}

}  // namespace econet
}  // namespace esphome
