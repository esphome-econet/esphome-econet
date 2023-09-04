#include "esphome/core/log.h"
#include "econet_binary_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.bsensor";

void EconetBinarySensor::update() {
  if (this->enable_state_sensor_ != nullptr) {
    this->enable_state_sensor_->publish_state(this->parent_->get_enable_state());
  }
  if (this->heatctrl_sensor_ != nullptr) {
    this->heatctrl_sensor_->publish_state(this->parent_->get_heatctrl());
  }
  if (this->fan_ctrl_sensor_ != nullptr) {
    this->fan_ctrl_sensor_->publish_state(this->parent_->get_fan_ctrl());
  }
  if (this->comp_rly_sensor_ != nullptr) {
    this->comp_rly_sensor_->publish_state(this->parent_->get_comp_rly());
  }
}

void EconetBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet Sensors:");
}

}  // namespace econet
}  // namespace esphome
