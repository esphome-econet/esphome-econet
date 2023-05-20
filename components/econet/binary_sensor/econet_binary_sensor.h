#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetBinarySensor : public PollingComponent, public EconetClient {
 public:
	void update() override;
	void dump_config() override;

	void set_enable_state_sensor(binary_sensor::BinarySensor *sensor) {
		this->enable_state_sensor_ = sensor;
	}
	void set_heatctrl_sensor(binary_sensor::BinarySensor *sensor) {
		this->heatctrl_sensor_ = sensor;
	}
	void set_fan_ctrl_sensor(binary_sensor::BinarySensor *sensor) {
		this->fan_ctrl_sensor_ = sensor;
	}
	void set_comp_rly_sensor(binary_sensor::BinarySensor *sensor) {
		this->comp_rly_sensor_ = sensor;
	}

 protected:
	binary_sensor::BinarySensor *enable_state_sensor_{nullptr};
	binary_sensor::BinarySensor *heatctrl_sensor_{nullptr};
	binary_sensor::BinarySensor *fan_ctrl_sensor_{nullptr};
	binary_sensor::BinarySensor *comp_rly_sensor_{nullptr};
};

}  // namespace econet
}  // namespace esphome
