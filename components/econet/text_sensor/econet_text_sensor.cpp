#include "esphome/core/log.h"
#include "econet_text_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.text_sensor";

void EconetTextSensor::setup() {
  this->parent_->register_listener(
      this->sensor_id_, this->request_mod_, this->request_once_,
      [this](const EconetDatapoint &datapoint) {
        if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
          return;
        }
        if (datapoint.type != EconetDatapointType::TEXT && datapoint.type != EconetDatapointType::ENUM_TEXT) {
          ESP_LOGW(TAG, "Unexpected datapoint type %u for text sensor %s", static_cast<uint8_t>(datapoint.type),
                   this->sensor_id_);
          return;
        }
        ESP_LOGV(TAG, "MCU reported text sensor %s is: %s", this->sensor_id_, datapoint.value_string.c_str());
        this->publish_state(datapoint.value_string);
      },
      false, this->src_adr_);
}

void EconetTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Econet Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %s", this->sensor_id_);
}

}  // namespace econet
}  // namespace esphome
