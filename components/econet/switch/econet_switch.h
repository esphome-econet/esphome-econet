#pragma once

#include "esphome/components/switch/switch.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetSwitch : public switch_::Switch, public PollingComponent {
 public:
	void update() override;
	void dump_config() override;
	void write_state(bool state) override;
	
	void set_econet(Econet *econet) { this->econet = econet; }
	/*
	void set_this_switch(switch_::Switch *sensor) {
		this->this_switch_ = sensor;
	}
	*/
	void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

 protected:
	uint8_t switch_id_{0};
	Econet *econet;
};

}  // namespace econet
}  // namespace esphome
