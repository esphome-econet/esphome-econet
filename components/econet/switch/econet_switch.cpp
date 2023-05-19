#include "econet_switch.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.switch";
	
void EconetSwitch::update() {
	if (!this->econet->is_ready())
		return;
	
	ESP_LOGD("  ", "enable_state %d", this->econet->get_enable_state());
	this->publish_state(this->econet->get_enable_state());
	
	//if (this->this_switch_ != nullptr) {
	//	this->this_switch_->publish_state(this->econet->get_enable_state());
	//}
}
void EconetSwitch::write_state(bool state) {
	ESP_LOGD("econet", "write_state");
	if(this->econet != nullptr)
	{
		ESP_LOGD("econet", "econet is good");
		this->econet->set_enable_state(state);
	}
	/*
	if (this->enable_switch_ != nullptr)
	{
		
		// Do something here
		
	}
	else
	{
		ESP_LOGD("econet", "Called write_state but null");
	}
	*/
	/*
	if (this->enable_state_sensor_ != nullptr) {
		this->enable_state_sensor_->publish_state(this->econet->get_enable_state());
	}
	*/
}

void EconetSwitch::dump_config() {
	ESP_LOGCONFIG(TAG, "Econet Sensors:");
	// LOG_SENSOR("  ", "temp_in", this->temp_in_sensor_);
	// LOG_SENSOR("  ", "temp_out", this->temp_out_sensor_);
}

}  // namespace econet
}  // namespace esphome
