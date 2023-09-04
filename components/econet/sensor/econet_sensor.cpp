#include "esphome/core/log.h"
#include "econet_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.sensor";

void EconetSensor::update() {
  if (this->temp_in_sensor_ != nullptr) {
    this->temp_in_sensor_->publish_state(this->econet->get_temp_in());
  }
  if (this->temp_out_sensor_ != nullptr) {
    this->temp_out_sensor_->publish_state(this->econet->get_temp_out());
  }
  if (this->flow_rate_sensor_ != nullptr) {
    this->flow_rate_sensor_->publish_state(this->econet->get_flow_rate());
  }
  if (this->setpoint_sensor_ != nullptr) {
    this->setpoint_sensor_->publish_state(this->econet->get_setpoint());
  }
  if (this->water_used_sensor_ != nullptr) {
    this->water_used_sensor_->publish_state(this->econet->get_water_used());
  }
  if (this->btus_used_sensor_ != nullptr) {
    this->btus_used_sensor_->publish_state(this->econet->get_btus_used());
  }
  if (this->ignition_cycles_sensor_ != nullptr) {
    this->ignition_cycles_sensor_->publish_state(this->econet->get_ignition_cycles());
  }
  if (this->instant_btus_sensor_ != nullptr) {
    this->instant_btus_sensor_->publish_state(this->econet->get_instant_btus());
  }
  if (this->hot_water_sensor_ != nullptr) {
    this->hot_water_sensor_->publish_state(this->econet->get_hot_water());
  }

  if (this->ambient_temp_sensor_ != nullptr) {
    this->ambient_temp_sensor_->publish_state(this->econet->get_ambient_temp());
  }
  if (this->lower_water_heater_temp_sensor_ != nullptr) {
    this->lower_water_heater_temp_sensor_->publish_state(this->econet->get_lower_water_heater_temp());
  }
  if (this->upper_water_heater_temp_sensor_ != nullptr) {
    this->upper_water_heater_temp_sensor_->publish_state(this->econet->get_upper_water_heater_temp());
  }
  if (this->power_watt_sensor_ != nullptr) {
    this->power_watt_sensor_->publish_state(this->econet->get_power_watt());
  }
  if (this->evap_temp_sensor_ != nullptr) {
    this->evap_temp_sensor_->publish_state(this->econet->get_evap_temp());
  }
  if (this->suction_temp_sensor_ != nullptr) {
    this->suction_temp_sensor_->publish_state(this->econet->get_suction_temp());
  }
  if (this->discharge_temp_sensor_ != nullptr) {
    this->discharge_temp_sensor_->publish_state(this->econet->get_discharge_temp());
  }

  if (this->cc_hvacmode_sensor_ != nullptr) {
    this->cc_hvacmode_sensor_->publish_state(this->econet->get_cc_hvacmode());
  }
  if (this->cc_spt_stat_sensor_ != nullptr) {
    this->cc_spt_stat_sensor_->publish_state(this->econet->get_cc_spt_stat());
  }
  if (this->cc_coolsetp_sensor_ != nullptr) {
    this->cc_coolsetp_sensor_->publish_state(this->econet->get_cc_cool_setpoint());
  }
  if (this->cc_automode_sensor_ != nullptr) {
    this->cc_automode_sensor_->publish_state(this->econet->get_cc_automode());
  }
}

void EconetSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet Sensors:");
}

}  // namespace econet
}  // namespace esphome
