#include "econet_switch.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.switch";
	
void EconetSwitch::update() {
	if (!this->econet->is_ready())
		return;
	
	this->publish_state(this->econet->get_enable_state());
}
void EconetSwitch::write_state(bool state) {
	ESP_LOGD("econet", "write_state");
	if(this->econet != nullptr)
	{
		ESP_LOGD("econet", "econet is good");
		this->econet->set_enable_state(state);
	}
}

void EconetSwitch::dump_config() {

}

}  // namespace econet
}  // namespace esphome
