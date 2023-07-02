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
	void set_hot_water_sensor(sensor::Sensor *sensor) {
		this->hot_water_sensor_ = sensor;
	}
	void set_ambient_temp_sensor(sensor::Sensor *sensor) {
		this->ambient_temp_sensor_ = sensor;
	}
	void set_lower_water_heater_temp_sensor(sensor::Sensor *sensor) {
		this->lower_water_heater_temp_sensor_ = sensor;
	}
	void set_upper_water_heater_temp_sensor(sensor::Sensor *sensor) {
		this->upper_water_heater_temp_sensor_ = sensor;
	}
	void set_power_watt_sensor(sensor::Sensor *sensor) {
		this->power_watt_sensor_ = sensor;
	}
	void set_evap_temp_sensor(sensor::Sensor *sensor) {
		this->evap_temp_sensor_ = sensor;
	}
	void set_suction_temp_sensor(sensor::Sensor *sensor) {
		this->suction_temp_sensor_ = sensor;
	}
	void set_discharge_temp_sensor(sensor::Sensor *sensor) {
		this->discharge_temp_sensor_ = sensor;
	}
	
	void set_cc_hvacmode_sensor(sensor::Sensor *sensor) {
		this->cc_hvacmode_sensor_ = sensor;
	}
	void set_cc_spt_stat_sensor(sensor::Sensor *sensor) {
		this->cc_spt_stat_sensor_ = sensor;
	}
	void set_cc_coolsetp_sensor(sensor::Sensor *sensor) {
		this->cc_coolsetp_sensor_ = sensor;
	}
	void set_cc_automode_sensor(sensor::Sensor *sensor) {
		this->cc_automode_sensor_ = sensor;
	}
 void set_cc_rel_hum_sensor(sensor::Sensor *sensor) {
		this->cc_rel_hum_sensor = sensor;
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
	sensor::Sensor *hot_water_sensor_{nullptr};
	
	sensor::Sensor *ambient_temp_sensor_{nullptr};
	sensor::Sensor *lower_water_heater_temp_sensor_{nullptr};
	sensor::Sensor *upper_water_heater_temp_sensor_{nullptr};
	sensor::Sensor *power_watt_sensor_{nullptr};
	sensor::Sensor *evap_temp_sensor_{nullptr};
	sensor::Sensor *suction_temp_sensor_{nullptr};
	sensor::Sensor *discharge_temp_sensor_{nullptr};
	
	sensor::Sensor *cc_hvacmode_sensor_{nullptr};
	sensor::Sensor *cc_spt_stat_sensor_{nullptr};
	sensor::Sensor *cc_coolsetp_sensor_{nullptr};
	sensor::Sensor *cc_automode_sensor_{nullptr};
 sensor::Sensor 
*cc_rel_hum_sensor_{nullptr};
};

}  // namespace econet
}  // namespace esphome
