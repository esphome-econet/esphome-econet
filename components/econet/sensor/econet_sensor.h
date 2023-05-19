#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetSensor : public PollingComponent, public EconetClient {
 public:
	void update() override;
	void dump_config() override;

	void set_temp_in_sensor(sensor::Sensor *sensor) {
		this->temp_in_sensor_ = sensor;
	}
	void set_temp_out_sensor(sensor::Sensor *sensor) {
		this->temp_out_sensor_ = sensor;
	}
	void set_setpoint_sensor(sensor::Sensor *sensor) {
		this->setpoint_sensor_ = sensor;
	}
	void set_flow_rate_sensor(sensor::Sensor *sensor) {
		this->flow_rate_sensor_ = sensor;
	}
	void set_water_used_sensor(sensor::Sensor *sensor) {
		this->water_used_sensor_ = sensor;
	}
	void set_btus_used_sensor(sensor::Sensor *sensor) {
		this->btus_used_sensor_ = sensor;
	}
	void set_ignition_cycles_sensor(sensor::Sensor *sensor) {
		this->ignition_cycles_sensor_ = sensor;
	}
	void set_instant_btus_sensor(sensor::Sensor *sensor) {
		this->instant_btus_sensor_ = sensor;
	}

 protected:
	sensor::Sensor *temp_in_sensor_{nullptr};
	sensor::Sensor *temp_out_sensor_{nullptr};
	sensor::Sensor *setpoint_sensor_{nullptr};
	sensor::Sensor *flow_rate_sensor_{nullptr};
	sensor::Sensor *water_used_sensor_{nullptr};
	sensor::Sensor *btus_used_sensor_{nullptr};
	sensor::Sensor *ignition_cycles_sensor_{nullptr};
	sensor::Sensor *instant_btus_sensor_{nullptr};
};

}  // namespace econet
}  // namespace esphome
