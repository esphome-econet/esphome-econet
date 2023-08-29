#include "econet_binary_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.bsensor";

void EconetBinarySensor::update() {
  if (!this->econet->is_ready())
    return;
  if (this->enable_state_sensor_ != nullptr) {
    this->enable_state_sensor_->publish_state(this->econet->get_enable_state());
  }
  if (this->heatctrl_sensor_ != nullptr) {
    this->heatctrl_sensor_->publish_state(this->econet->get_heatctrl());
  }
  if (this->fan_ctrl_sensor_ != nullptr) {
    this->fan_ctrl_sensor_->publish_state(this->econet->get_fan_ctrl());
  }
  if (this->comp_rly_sensor_ != nullptr) {
    this->comp_rly_sensor_->publish_state(this->econet->get_comp_rly());
  }
}

void EconetBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet Sensors:");
  // LOG_SENSOR("  ", "temp_in", this->temp_in_sensor_);
  // LOG_SENSOR("  ", "temp_out", this->temp_out_sensor_);
}

}  // namespace econet
}  // namespace esphome
