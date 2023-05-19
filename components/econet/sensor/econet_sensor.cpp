#include "econet_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.sensor";

void EconetSensor::update() {
	if (!this->econet->is_ready())
		return;
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
}

void EconetSensor::dump_config() {
	ESP_LOGCONFIG(TAG, "Econet Sensors:");
	// LOG_SENSOR("  ", "temp_in", this->temp_in_sensor_);
	// LOG_SENSOR("  ", "temp_out", this->temp_out_sensor_);
}

}  // namespace econet
}  // namespace esphome
