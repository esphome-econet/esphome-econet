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
}

void EconetBinarySensor::dump_config() {
	ESP_LOGCONFIG(TAG, "Econet Sensors:");
	// LOG_SENSOR("  ", "temp_in", this->temp_in_sensor_);
	// LOG_SENSOR("  ", "temp_out", this->temp_out_sensor_);
}

}  // namespace econet
}  // namespace esphome
