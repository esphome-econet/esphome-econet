#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <map>

namespace esphome {
namespace econet {

class ReadRequest {
	public:
		uint32_t dst_adr;
		uint8_t dst_bus;
		
		uint32_t src_adr;
		uint8_t src_bus;
		
		bool awaiting_res = false;
		// uint8_t read_class;
		// uint8_t read_prop;
		
		std::vector<std::string> obj_names;
};
	
enum class EconetClimateMode : uint8_t {
  Disabled = '0',
  Auto = '1',
  Dry = '2',
  Cool = '3',
  Heat = '4',
  Fan = '6',
};
	
struct DatapointListener {
  uint8_t datapoint_id;
  std::function<void(float)> on_datapoint;
};
	
class Econet : public Component {
 public:
	void loop() override;
	void dump_config() override;
	void set_uart(uart::UARTComponent *econet_uart);
	void set_type_id(uint8_t type_id) { this->type_id_ = type_id; }
	bool is_ready() { return this->ready; }
	void make_request();
	void read_buffer(int bytes_available);
	void parse_message(bool is_tx);
	void parse_rx_message();
	void parse_tx_message();
	void set_enable_state(bool state);
	
	void set_new_setpoint(float setpoint);
	void set_new_setpoint_low(float setpoint);
	void set_new_setpoint_high(float setpoint);
	
	void set_new_mode(float mode);
	void set_new_fan_mode(float fan_mode);
	
	uint8_t get_type_id() { return this->type_id_; }
	float get_temp_in() { return this->temp_in; }
	float get_temp_out() { return this->temp_out; }
	float get_flow_rate() { return this->flow_rate; }
	float get_setpoint() { return this->setpoint; }
	float get_water_used() { return this->water_used; }
	float get_btus_used() { return this->btus_used; }
	float get_ignition_cycles() { return this->ignition_cycles; }
	float get_instant_btus() { return this->instant_btus; }
	float get_hot_water() { return this->hot_water; }
	float get_mode() { return this->mode; }
	bool get_enable_state() { return this->enable_state; }
	bool get_heatctrl() { return this->heatctrl; }
	bool get_fan_ctrl() { return this->fan_ctrl; }
	bool get_comp_rly() { return this->comp_rly; }
	
	float get_ambient_temp() { return this->ambient_temp; }
	float get_lower_water_heater_temp() { return this->lower_water_heater_temp; }
	float get_power_watt() { return this->power_watt; }
	float get_upper_water_heater_temp() { return this->upper_water_heater_temp; }
	float get_evap_temp() { return this->evap_temp; }
	float get_suction_temp() { return this->suction_temp; }
	float get_discharge_temp() { return this->discharge_temp; }
	
	float get_current_temp() {
		if(this->type_id_ == 0) return this->temp_out;
		else if(this->type_id_ == 1) return this->upper_water_heater_temp;
		else return this->setpoint;
	}
	
	float get_cc_hvacmode() { return this->cc_hvacmode; }
	float get_cc_spt_stat() { return this->cc_spt_stat; }
	float get_cc_cool_setpoint() { return this->cc_cool_setpoint; }
	float get_cc_heat_setpoint() { return this->cc_heat_setpoint; }
	float get_cc_automode() { return this->cc_automode; }
	float get_cc_statmode() { return this->cc_statmode; }
	float get_cc_fan_mode() { return this->cc_fan_mode; }
        float get_cc_rel_hum() { return this->cc_rel_hum; }
	
	void register_listener(uint8_t datapoint_id, const std::function<void(float)> &func);
	
 protected:
	uint8_t type_id_{0};
	std::vector<DatapointListener> listeners_;
	ReadRequest read_req; // dst_adr
	// std::vector<ReadRequest> read_reqs_;
	void dump_state();
	void check_uart_settings();
	void send_datapoint(uint8_t datapoint_id, float value);

	void handle_float(uint32_t src_adr, std::string obj_string, float value);
	void handle_enumerated_text(uint32_t src_adr, std::string obj_string, uint8_t value, std::string text);
	
	void handle_text(uint32_t src_adr, std::string obj_string, std::string text);
	void handle_binary(uint32_t src_adr, std::string obj_string, std::vector<uint8_t> data);
	
	void transmit_message(std::vector<uint8_t> data);
	void transmit_sync();
	void transmit_message(uint32_t dst_adr, uint32_t src_adr, uint8_t command, std::vector<uint8_t> data);
	void request_strings(uint32_t dst_adr, uint32_t src_adr, std::vector<std::string> objects);
	void write_value(uint32_t dst_adr, uint32_t src_adr, std::string object, uint8_t type, float value);
	
	uart::UARTComponent *econet_uart{nullptr};
	bool ready = true;
	
	// ReadRequest *read_request;
	
	float temp_in = 0;
	float temp_out = 0;
	float flow_rate = 0;
	float setpoint = 0;
	float water_used = 0;
	float btus_used = 0;
	float ignition_cycles = 0;
	float instant_btus = 0;
	float hot_water = 0;
	bool enable_state = false;
	bool heatctrl = false;
	bool fan_ctrl = false;
	bool comp_rly = false;
	bool do_once = false;
	
	float ambient_temp = 0;
	float lower_water_heater_temp = 0;
	float upper_water_heater_temp = 0;
	float power_watt = 0;
	float evap_temp = 0;
	float suction_temp = 0;
	float discharge_temp = 0;
	
	float current_temp = 0;
	
	float mode = 0;
	
	float cc_hvacmode = 0;
	float cc_spt_stat = 0;
	float cc_cool_setpoint = 0;
	
	float cc_heat_setpoint = 0;
	float cc_automode = 0;
	float cc_statmode = 0;
	float cc_fan_mode = 0;
        float cc_rel_hum = 0;
	
	uint8_t req_id = 0;
	uint32_t last_request_{0};
	uint32_t last_read_{0};
	uint32_t last_read_data_{0};
	uint32_t act_loop_time_{0};
	int quiet_counter = 0;
	uint8_t data_len = 0;
	uint16_t msg_len = 0;
	int pos = 0;
	static const int max_message_size = 271;
	uint8_t buffer[max_message_size];
	
	bool send_enable_disable = false;	
	bool enable_disable_cmd = false;
	
	bool send_new_setpoint_low = false;
	float new_setpoint_low = 100;
	
	bool send_new_setpoint_high = false;
	float new_setpoint_high = 100;
	
	bool send_new_setpoint = false;
	float new_setpoint = 100;
	
	bool send_new_mode = false;
	float new_mode = 0;
	
	bool send_new_fan_mode = false;
	float new_fan_mode = 0;
	
	uint8_t wbuffer[max_message_size];
	uint16_t wmsg_len = 0;
	
	uint32_t COMPUTER =      				192	;	// 80 00 00 C0
	uint32_t FURNACE = 						0x1c0;	// 80 00 01 C0
	uint32_t UNKNOWN_HANDLER =  			241	;	// 80 00 00 F1
	uint32_t WIFI_MODULE =    				832	;	// 80 00 03 40
	uint32_t SMARTEC_TRANSLATOR = 			4160;	// 80 00 10 40
	uint32_t INTERNAL = 					4736; 	// 80 00 10 40
	uint32_t HEAT_PUMP_WATER_HEATER =       0x1280; // 80 00 12 80
	uint32_t AIR_HANDLER = 					0x3c0;	// 80 00 03 C0
	uint32_t CONTROL_CENTER = 				0x380;	// 80 00 03 80

	uint8_t DST_ADR_POS = 0;
	uint8_t SRC_ADR_POS = 5;
	uint8_t SRC_BUS_POS = 9;
	uint8_t LEN_POS = 10;
	uint8_t COMMAND_POS = 13;
	uint8_t DATA_START_POS = 14;

	uint8_t MSG_HEADER_SIZE = 14;
	uint8_t BYTES_BETWEEN_ADRS = 5;
	uint8_t MSG_CRC_SIZE = 2;

	uint8_t ACK = 6;
	uint8_t READ_COMMAND = 30;
	uint8_t WRITE_COMMAND = 31;
	
	uint8_t FLOAT = 0;
	uint8_t ENUM_TEXT = 2;
};

class EconetClient {
 public:
  void set_econet(Econet *econet) { this->econet = econet; }

 protected:
  Econet *econet;
};

}  // namespace econet
}  // namespace esphome
