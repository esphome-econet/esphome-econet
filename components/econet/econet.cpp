#include "econet.h"

using namespace esphome;

namespace esphome {
namespace econet {

static const char *const TAG = "econet";

uint16_t gen_crc16(const uint8_t *data, uint16_t size)
{
	uint16_t out = 0;
	int bits_read = 0, bit_flag;

	/* Sanity check: */
	if(data == NULL)
		return 0;

	while(size > 0)
	{
		bit_flag = out >> 15;

		/* Get next bit: */
		out <<= 1;
		out |= (*data >> bits_read) & 1; // item a) work from the least significant bits

		/* Increment bit counter: */
		bits_read++;
		if(bits_read > 7)
		{
			bits_read = 0;
			data++;
			size--;
		}

		/* Cycle check: */
		if(bit_flag)
			out ^= 0x8005;

	}

	// item b) "push out" the last 16 bits
	int i;
	for (i = 0; i < 16; ++i) {
		bit_flag = out >> 15;
		out <<= 1;
		if(bit_flag)
			out ^= 0x8005;
	}

	// item c) reverse the bits
	uint16_t crc = 0;
	i = 0x8000;
	int j = 0x0001;
	for (; i != 0; i >>=1, j <<= 1) {
		if (i & out) crc |= j;
	}

	return crc;
}
float bytesToFloat(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) 
{ 
	uint8_t byte_array[] = { b3, b2, b1, b0 };
	float result;
	std::copy(reinterpret_cast<const char*>(&byte_array[0]),
			  reinterpret_cast<const char*>(&byte_array[4]),
			  reinterpret_cast<char*>(&result));
	return result;
} 
uint32_t floatToUint32(float f) 
{ 
	uint32_t fbits = 0;
	memcpy(&fbits, &f, sizeof fbits);
	
	return fbits;
} 
	
void Econet::set_uart(uart::UARTComponent *econet_uart) {
  this->econet_uart = econet_uart;
}

#define ECONET_BAUD_RATE 38400

void Econet::check_uart_settings() {
  for (auto uart : {this->econet_uart}) {
    if (uart->get_baud_rate() != ECONET_BAUD_RATE) {
      ESP_LOGE(
          TAG,
          "  Invalid baud_rate: Integration requested baud_rate %u but you "
          "have %u!",
          ECONET_BAUD_RATE, uart->get_baud_rate());
    }
  }
}

void Econet::dump_config() {
  // ESP_LOGCONFIG(TAG, "DaikinS21:");
  // ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
  this->check_uart_settings();
}
void Econet::make_request()
{
	/*
	if(type_id_ == 0) read_request->dst_adr = SMARTEC_TRANSLATOR;
	else if(type_id_ == 1) read_request->dst_adr = HEAT_PUMP_WATER_HEATER;
	
	read_request->dst_bus = 0;
	
	read_request->src_adr = WIFI_MODULE;
	read_request->src_bus = 0;
	*/
	
	uint32_t dst_adr = SMARTEC_TRANSLATOR;
	if(type_id_ == 1) dst_adr = HEAT_PUMP_WATER_HEATER;
	else if(type_id_ == 2) dst_adr = AIR_HANDLER;
	//uint32_t dst_adr = 0x340;
	uint8_t dst_bus = 0x00;

	// uint32_t src_adr = COMPUTER; // WIFI_MODULE; // COMPUTER;
	uint32_t src_adr = WIFI_MODULE;
	if(type_id_ == 2) src_adr = CONTROL_CENTER;
	//uint32_t src_adr = 0x1040;
	uint8_t src_bus = 0x00;

	std::vector<std::string> str_ids{};

	if(req_id == 0)
	{
		if(type_id_ == 0)
		{
			// str_ids.push_back("TEMP_OUT");
			str_ids.push_back("FLOWRATE");
			// str_ids.push_back("HTRS__ON");
			str_ids.push_back("TEMP_OUT");
			str_ids.push_back("TEMP__IN");
			str_ids.push_back("WHTRENAB");
			str_ids.push_back("WHTRMODE");
			str_ids.push_back("WHTRSETP");
			str_ids.push_back("WTR_USED");
			str_ids.push_back("WTR_BTUS");
			str_ids.push_back("IGNCYCLS");
			str_ids.push_back("BURNTIME");
		}
		else if(type_id_ == 1)
		{
			str_ids.push_back("WHTRENAB");
			str_ids.push_back("WHTRCNFG");
			str_ids.push_back("WHTRSETP");
			str_ids.push_back("HOTWATER");
			str_ids.push_back("HEATCTRL");
			str_ids.push_back("FAN_CTRL");
			str_ids.push_back("COMP_RLY");
			//
			str_ids.push_back("AMBIENTT");
			str_ids.push_back("LOHTRTMP");
			str_ids.push_back("UPHTRTMP");
			str_ids.push_back("POWRWATT");
			str_ids.push_back("EVAPTEMP");
			str_ids.push_back("SUCTIONT");
			str_ids.push_back("DISCTEMP");
		}
		else if(type_id_ == 2)
		{
			// str_ids.push_back("AIRHSTAT");
			str_ids.push_back("AAUX1CFM");
			str_ids.push_back("AAUX2CFM");
			str_ids.push_back("AAUX3CFM");
			str_ids.push_back("AAUX4CFM");
			
		}
	}
	else
	{
		str_ids.push_back("ALARMS");
	}

	uint8_t num_of_strs = str_ids.size();

	// num_of_strs = 1;
	
	uint8_t command = READ_COMMAND;
	uint16_t wdata_len = 4+10*num_of_strs;

	if(type_id_ == 2 && num_of_strs == 1) wdata_len = wdata_len-2;
	
	std::vector<uint8_t> data(wdata_len);
	
	if(send_enable_disable == true)
	{
		ESP_LOGD("econet", "this->send_enable_disable == true");
	}
	
	if(send_enable_disable == true)
	{
		command = WRITE_COMMAND;
		std::vector<uint8_t> enable_cmd{0x01, 0x01, 0x02, 0x01, 0x00, 0x00, 0x57, 0x48, 0x54, 0x52, 0x45, 0x4E, 0x41, 0x42, 0x3F, 0x80, 0x00, 0x00};
		std::vector<uint8_t> disable_cmd{0x01, 0x01, 0x02, 0x01, 0x00, 0x00, 0x57, 0x48, 0x54, 0x52, 0x45, 0x4E, 0x41, 0x42, 0x00, 0x00, 0x00, 0x00};

		wdata_len = 18;

		if(enable_disable_cmd == true)
		{
			data = enable_cmd;
		}
		else
		{
			data = disable_cmd;
		}
		send_enable_disable = false;
	}
	else if(send_new_mode == true)
	{
		// Modes
		// 0 - Off
		// 1 - Energy Saver
		// 2 - Heat Pump
		// 3 - High Demand
		// 4 - Electric / Gas
		
		command = WRITE_COMMAND;
		
		std::vector<uint8_t> base_mode_cmd{0x01, 0x01, 0x00, 0x07, 0x00, 0x00, 0x57, 0x48, 0x54, 0x52, 0x43, 0x4E, 0x46, 0x47, 0x00, 0x00, 0x00, 0x00};
		
		uint32_t f_to_32 = floatToUint32(new_mode);
		
		base_mode_cmd[14] = (uint8_t)(f_to_32 >> 24);
		base_mode_cmd[15] = (uint8_t)(f_to_32 >> 16);
		base_mode_cmd[16] = (uint8_t)(f_to_32 >> 8);
		base_mode_cmd[17] = (uint8_t)(f_to_32);
		
		data = base_mode_cmd;
		wdata_len = 18;
		
		send_new_mode = false;
	}

	
	else if(send_new_setpoint == true)
	{
		
		command = WRITE_COMMAND;
		
		std::vector<uint8_t> base_setpoint_cmd{0x01, 0x01, 0x00, 0x07, 0x00, 0x00, 0x57, 0x48, 0x54, 0x52, 0x53, 0x45, 0x54, 0x50, 0x3F, 0x80, 0x00, 0x00};
		
		uint32_t f_to_32 = floatToUint32(new_setpoint);
		
		base_setpoint_cmd[14] = (uint8_t)(f_to_32 >> 24);
		base_setpoint_cmd[15] = (uint8_t)(f_to_32 >> 16);
		base_setpoint_cmd[16] = (uint8_t)(f_to_32 >> 8);
		base_setpoint_cmd[17] = (uint8_t)(f_to_32);
		
		data = base_setpoint_cmd;
		wdata_len = 18;
		
		send_new_setpoint = false;
	}
	else
	{
		command = READ_COMMAND;
		
		if(num_of_strs > 1)
		{
			// Read Class
			data[0] = 2;
		}
		else
		{
			data[0] = 1;
		}
		
		// Read Property
		data[1] = 1;

		int a = 0;

		for(int i=0; i<num_of_strs; i++)
		{
			data[2+10*i] = 0;
			data[2+10*i+1] = 0;

			std::string my_str = str_ids[i];

			std::vector<uint8_t> sdata(my_str.begin(), my_str.end());
			uint8_t *p = &sdata[0];

			for(int j=0; j<10;j++)
			{
				if( 2+10*i+2 + j < wdata_len)
				{
					if(j < str_ids[i].length())
					{
						data[2+10*i+2 + j] = sdata[j];
					}
					else
					{

						data[2+10*i+2 + j] = 0;
					}
				}
			}
		}
	}

	if(true)
	{
		wbuffer[0] = 0x80;
		wbuffer[1] = (uint8_t) (dst_adr >> 16);
		wbuffer[2] = (uint8_t) (dst_adr >> 8);
		wbuffer[3] = (uint8_t) dst_adr;
		wbuffer[4] = dst_bus;

		wbuffer[5] = 0x80;
		wbuffer[6] = (uint8_t) (src_adr >> 16);
		wbuffer[7] = (uint8_t) (src_adr >> 8);
		wbuffer[8] = (uint8_t) src_adr;
		wbuffer[9] = src_bus;

		wbuffer[10] = wdata_len;
		wbuffer[11] = 0;
		wbuffer[12] = 0;
		wbuffer[13] = command;
		// wbuffer[13] = ACK;

		for(int i=0; i < wdata_len; i++)
		{
			wbuffer[14+i] = data[i];
		}

		uint16_t crc = gen_crc16(wbuffer,wdata_len + 14);

		// ESP_LOGD("custom", "Sending message with crc = %x", crc);

		// 48 00 00  f0 f0
		wbuffer[wdata_len+14] = (uint8_t) crc;
		wbuffer[wdata_len+14+1] = (uint8_t) (crc >> 8);

		// digitalWrite(0, HIGH);

		// delay(100);

		if(type_id_ != 2)
		{
			econet_uart->write_array(wbuffer,wdata_len+14+2);
			// econet_uart->flush();
			// delay(100);

			// digitalWrite(0, LOW);

			this->parse_tx_message();
		}
		else
		{
			
		}

		// ESP_LOGI("econet", ">>> %s", format_hex_pretty((const uint8_t *) wbuffer, wdata_len+14+2).c_str());
	}
}
void Econet::parse_tx_message()
{
	this->parse_message(true);
}
void Econet::parse_rx_message()
{
	this->parse_message(false);
}
void Econet::parse_message(bool is_tx)
{     
	bool logvals = true;

	uint8_t pdata[255];

	// Receive?
	uint32_t dst_adr = 0;
	uint8_t dst_bus = 0;
	uint32_t src_adr = 0;
	uint8_t src_bus = 0;
	uint8_t data_len = 0;
	uint8_t command = 0;
	uint16_t crc = 0;
	uint16_t crc_check = 0;
	
	uint16_t pmsg_len = 0; 
	
	if(is_tx == false)
	{
		dst_adr = ((buffer[0] & 0x7f) << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
		dst_bus = buffer[4];

		src_adr = ((buffer[5] & 0x7f) << 24) + (buffer[6] << 16) + (buffer[7] << 8) + buffer[8];
		src_bus = buffer[9];

		data_len = buffer[10];

		pmsg_len = data_len + MSG_HEADER_SIZE + MSG_CRC_SIZE;

		command = buffer[13];

		crc = (buffer[data_len + MSG_HEADER_SIZE]) + (buffer[data_len + MSG_HEADER_SIZE + 1] << 8);

		crc_check = gen_crc16(buffer, data_len + 14);

		for(int i = 0; i < data_len; i++)
		{
			pdata[i] = buffer[MSG_HEADER_SIZE + i];
		}
		
		// data_len + MSG_HEADER_SIZE + MSG_CRC_SIZE
		
	}
	else
	{
		dst_adr = ((wbuffer[0] & 0x7f) << 24) + (wbuffer[1] << 16) + (wbuffer[2] << 8) + wbuffer[3];
		dst_bus = wbuffer[4];

		src_adr = ((wbuffer[5] & 0x7f) << 24) + (wbuffer[6] << 16) + (wbuffer[7] << 8) + wbuffer[8];
		src_bus = wbuffer[9];

		data_len = wbuffer[10];

		pmsg_len = data_len + MSG_HEADER_SIZE + MSG_CRC_SIZE;

		command = wbuffer[13];

		crc = (wbuffer[data_len + MSG_HEADER_SIZE]) + (wbuffer[data_len + MSG_HEADER_SIZE + 1] << 8);

		crc_check = gen_crc16(wbuffer, data_len + 14);

		for(int i = 0; i < data_len; i++)
		{
			pdata[i] = wbuffer[MSG_HEADER_SIZE + i];
		}
	}
	
	if(is_tx == false)
	{
		ESP_LOGI("econet", "<<< %s", format_hex_pretty((const uint8_t *) buffer, pmsg_len).c_str());
	}
	else
	{
		ESP_LOGI("econet", ">>> %s", format_hex_pretty((const uint8_t *) wbuffer, pmsg_len).c_str());
	}
	ESP_LOGI("econet", "  Dst Adr : 0x%x", dst_adr);
	ESP_LOGI("econet", "  Src Adr : 0x%x", src_adr);
	ESP_LOGI("econet", "  Length  : %d", data_len);
	ESP_LOGI("econet", "  Command : %d", command);
	ESP_LOGI("econet", "  Data    : %s", format_hex_pretty((const uint8_t *) pdata, data_len).c_str());
	
	// Track Read Requests
	if(command == READ_COMMAND)
	{
		uint8_t type = pdata[0];
		uint8_t prop_type = pdata[1];
		
		ESP_LOGI("econet", "  Type    : %d", type);
		ESP_LOGI("econet", "  PropType: %d", prop_type);
		
		std::vector<std::string> obj_names;
		
		if(type == 1)
		{
			if(prop_type == 1)
			{
				// pdata[2] = 0
				// pdata[3] = 0
				char char_arr[data_len - 4];

				for (int a = 0; a < data_len - 4; a++) {
					char_arr[a] = pdata[a+4];
				}

				std::string s(char_arr, sizeof(char_arr));

				obj_names.push_back(s);

				ESP_LOGI("econet", "  %s", s.c_str());
			}
			else
			{
				ESP_LOGI("econet", "  Don't Currently Support This Property Type", prop_type);
			}
		}
		else if(type == 2)
		{
			if(prop_type == 1)
			{
				int start = 4;
				int end = -1;
				int str_len = -1;
				for(int tpos = 5; tpos < data_len; tpos++)
				{
					bool mflag = false;
					if(pdata[tpos-1] == 0 && pdata[tpos] == 0)
					{
						// This detects a 00 00 
						str_len = tpos - start - 1;
						mflag = true;
					}
					if(tpos + 1 >= data_len)
					{
						str_len = tpos - start;
						mflag = true;
					}
					
					if(mflag == true)
					{
						char char_arr[str_len];

						for (int a = 0; a < str_len; a++) {
							if(start + a > 0 && start + a < data_len)
							{
								char_arr[a] = pdata[start+a];
							}
						}

						std::string s(char_arr, sizeof(char_arr));

						obj_names.push_back(s);

						ESP_LOGI("econet", "  ValName : %s", s.c_str());
						
						start = tpos+1;
					}
				}
			}
			else
			{
				ESP_LOGI("econet", "  Don't Currently Support This Property Type", prop_type);
			}
			
			read_req.dst_adr = dst_adr;
			read_req.src_adr = src_adr;
			read_req.obj_names = obj_names;
			read_req.awaiting_res = true;
		}
		else
		{
			ESP_LOGI("econet", "  Don't Currently Support This Class Type", type);
		}
		// tuple<uint32_t, uint32_t> req_tup(dst_adr,src_adr);
		
		// read_reqs
		// This is a read request so let's track it so that when an ACK/response comes to this we know what data it is!
		// for(
		// read_reqs_
	}
	else if(command == ACK)
	{
		if(read_req.dst_adr == src_adr && read_req.src_adr == dst_adr && read_req.awaiting_res == true)
		{
			int tpos = 0;
			uint8_t item_num = 0;

			while(tpos < data_len)
			{
				uint8_t item_len = pdata[tpos];
				uint8_t item_type = pdata[tpos+1] & 0x7F;

				if(item_type == 0 && tpos+7 < data_len)
				{
					float item_value = bytesToFloat(pdata[tpos+4],pdata[tpos+5],pdata[tpos+6],pdata[tpos+7]);
					
					if(item_num < read_req.obj_names.size())
					{
						ESP_LOGI("econet", "  %s : %f", read_req.obj_names[item_num].c_str(), item_value);
					}
				}
				else if(item_type == 2 && tpos+5 < data_len)
				{
					// Enumerated Text

					uint8_t item_value = pdata[tpos+4];

					uint8_t item_text_len = pdata[tpos+5];

					if(item_text_len > 0)
					{
						char char_arr[item_text_len];

						for (int a = 0; a < item_text_len; a++) {
							if(tpos+a+6 < data_len)
							{
								char_arr[a] = pdata[tpos+a+6];
							}
						}

						std::string s(char_arr, sizeof(char_arr));
						if(item_num < read_req.obj_names.size())
						{
							ESP_LOGI("econet", "  %s : %d (%s)", read_req.obj_names[item_num].c_str(), item_value, s.c_str());
						}
					}
				}
				tpos += item_len+1;
				item_num++;
			}
			
			// This is likely the response to our request and now we "know" what was requested!
			// ESP_LOGI("econet", "  RESPONSE RECEIVED!!!");
			// for(int a = 0; a < read_req.obj_names.size(); a++)
			// {
				// ESP_LOGI("econet", "  ValName : %s", read_req.obj_names[a].c_str());
			// }
			read_req.awaiting_res = false;			
		}
	}
	
	
	
	/*
	
	uint32_t UNKNOWN_HANDLER =  			241	;	// 80 00 00 F1
	uint32_t WIFI_MODULE =    				832	;	// 80 00 03 40
	uint32_t SMARTEC_TRANSLATOR = 			4160;	// 80 00 10 40
	uint32_t INTERNAL = 					4736; 	// 80 00 10 40
	uint32_t HEAT_PUMP_WATER_HEATER =       0x1280;
	uint32_t AIR_HANDLER = 					0x3c0;
	uint32_t CONTROL_CENTER = 				0x380;
	
	*/
	
	bool recognized = false;

	uint32_t exp_dst_adr = WIFI_MODULE;
	uint32_t exp_src_adr = SMARTEC_TRANSLATOR;
	int exp_msg_len = 115;
	
	if(type_id_ == 1)
	{
		exp_dst_adr = WIFI_MODULE;
		exp_src_adr = HEAT_PUMP_WATER_HEATER;
		exp_msg_len = 166;
		// Original was 66
		// New is 110
		// New new is 150 + 14 + 2 = 166
	}
	
	
	if(dst_adr == exp_dst_adr && src_adr == exp_src_adr)
	{
		if(command == ACK && pmsg_len == exp_msg_len)
		{
			recognized = false;

			int tpos = 0;
			uint8_t item_num = 1;

			while(tpos < data_len)
			{
				uint8_t item_len = pdata[tpos];
				uint8_t item_type = pdata[tpos+1] & 0x7F;

				if(item_type == 0)
				{
					float item_value = bytesToFloat(pdata[tpos+4],pdata[tpos+5],pdata[tpos+6],pdata[tpos+7]);
					
					if(type_id_ == 0)
					{
						if(item_num == 1)
						{
							flow_rate = item_value/3.785;
						}
						else if(item_num == 2)
						{
							temp_out = item_value;
						}
						else if(item_num == 3)
						{
							this->temp_in = item_value;
						}
						else if(item_num == 6)
						{
							setpoint = item_value;
						}
						else if(item_num == 7)
						{
							water_used = item_value;
						}
						else if(item_num == 8)
						{
							btus_used = item_value;
						}
						else if(item_num == 9)
						{
							ignition_cycles = item_value;
						}
					}
					else if(type_id_ == 1)
					{
						if(item_num == 3)
						{
							setpoint = item_value;
						}
						else if(item_num == 4)
						{
							hot_water = item_value;
						}
						else if(item_num == 8)
						{
							ambient_temp = item_value;
						}
						else if(item_num == 9)
						{
							lower_water_heater_temp = item_value;
						}
						else if(item_num == 10)
						{
							upper_water_heater_temp = item_value;
						}
						else if(item_num == 11)
						{
							power_watt = item_value;
						}
						else if(item_num == 12)
						{
							evap_temp = item_value;
						}
						else if(item_num == 13)
						{
							suction_temp = item_value;
						}
						else if(item_num == 14)
						{
							discharge_temp = item_value;
						}
					}
				}
				else if(item_type == 2)
				{
					// Enumerated Text

					uint8_t item_value = pdata[tpos+4];

					uint8_t item_text_len = pdata[tpos+5];

					char char_arr[item_text_len];

					for (int a = 0; a < item_text_len; a++) {
						char_arr[a] = pdata[tpos+a+6];
					}

					std::string s(char_arr, sizeof(char_arr));

					if(type_id_ == 0)
					{
						if(item_num == 4)
						{
							if(item_value == 0)
							{
								enable_state = false;
								// send_datapoint(0,0);
							}
							else if(item_value == 1)
							{
								enable_state = true;
								// send_datapoint(0,1);
							}
						}
						else if(item_num == 5)
						{
							ESP_LOGD("econet", "WHTRMODE (val): %d", item_value);
							ESP_LOGD("econet", "WHTRMODE (str): %s", s.c_str());
						}
					}
					else if(type_id_ == 1)
					{
						if(item_num == 1)
						{
							if(item_value == 0)
							{
								enable_state = false;
							}
							else if(item_value == 1)
							{
								enable_state = true;
							}
						}
						else if(item_num == 2)
						{
							mode = static_cast<int>(item_value);
						}
						else if(item_num == 5)
						{
							if(item_value == 0) heatctrl = false;
							else heatctrl = true;
						}	
						else if(item_num == 6)
						{
							if(item_value == 0) fan_ctrl = false;
							else fan_ctrl = true;
						}
						else if(item_num == 7)
						{
							if(item_value == 0) comp_rly = false;
							else comp_rly = true;
						}
					}
				}
				tpos += item_len+1;
				item_num++;
			}

			// 1 btu/deg_f*(8.334)*flow_rate (1/min) 

			instant_btus = std::max((float)((temp_out-temp_in)*flow_rate*8.334*60/0.92/1000),(float)0.0);

			// instant_btus_sensor->publish_state(approx_instant_btus/1000);

		}
		else if(pmsg_len == 31)
		{
			// recognized = true;
		}
		// publish_state(format_hex_pretty((const uint8_t *) buffer, msg_len));
	}
	else if(dst_adr == UNKNOWN_HANDLER)
	{
		// Filter these for now
		recognized = false;
	}
	else if(src_adr == UNKNOWN_HANDLER)
	{
		// Filter these for now
		recognized = false;
	}
	else if(dst_adr == SMARTEC_TRANSLATOR && src_adr == WIFI_MODULE)
	{
		// Filter these for now
	}

	if(recognized == false)
	{

		
		if(false)
		{
			if(command == 30)
			{
				// READ
				uint8_t type = pdata[0];
				ESP_LOGI("econet", "  Type    : %d", type);

				if(type == 1)
				{
					char char_arr[data_len - 6];

					for (int a = 0; a < data_len - 6; a++) {
						char_arr[a] = pdata[a+4];
					}

					std::string s(char_arr, sizeof(char_arr));

					ESP_LOGD("econet", "  ValName : %s", s.c_str());
				}
				else if(type == 2)
				{
					int tpos = 0;
					uint8_t item_num = 1;

					while(tpos < data_len)
					{
						ESP_LOGD("econet", "  Item[%d] ", item_num);

						uint8_t item_len = pdata[tpos];
						uint8_t item_type = pdata[tpos+1] & 0x7F;

						ESP_LOGD("econet", "  ItemLen : %d", item_len);
						ESP_LOGD("econet", "  ItemType: %d", item_type);

						if(item_type == 0)
						{
							float item_value = bytesToFloat(pdata[tpos+4],pdata[tpos+5],pdata[tpos+6],pdata[tpos+7]);
							ESP_LOGD("econet", "  ItemVal : %f", item_value);
						}
						else if(item_type == 2)
						{
							// Enumerated Text

							uint8_t item_value = pdata[tpos+4];

							ESP_LOGD("econet", "  ItemVal : %d", item_value);

							uint8_t item_text_len = pdata[tpos+5];

							// uint8_t str_arr[item_text_len];
							char char_arr[item_text_len];

							for (int a = 0; a < item_text_len; a++) {
								// str_arr[a] = pdata[tpos+a+6];
								char_arr[a] = pdata[tpos+a+6];
							}

							std::string s(char_arr, sizeof(char_arr));

							ESP_LOGD("econet", "  ItemStr : %s", s.c_str());
						}
						tpos += item_len+1;
						item_num++;
					}
				}
			}
			else if(command == 6)
			{
				// ACK

				uint8_t type = pdata[0] & 0x7F;

				// 
				if(type == 0)
				{
					float item_value = bytesToFloat(pdata[11],pdata[12],pdata[13],pdata[14]);
					ESP_LOGD("econet", "  ItemVal : %f", item_value);
				}
				else if(type == 2)
				{
					int tpos = 0;
					uint8_t item_num = 1;

					while(tpos < data_len)
					{
						ESP_LOGD("econet", "  Item[%d] ", item_num);

						uint8_t item_len = pdata[tpos];
						uint8_t item_type = pdata[tpos+1] & 0x7F;

						ESP_LOGD("econet", "  ItemLen : %d", item_len);
						ESP_LOGD("econet", "  ItemType: %d", item_type);

						if(item_type == 0)
						{
							float item_value = bytesToFloat(pdata[tpos+4],pdata[tpos+5],pdata[tpos+6],pdata[tpos+7]);
							ESP_LOGD("econet", "  ItemVal : %f", item_value);
						}
						else if(item_type == 2)
						{
							// Enumerated Text

							uint8_t item_value = pdata[tpos+4];

							ESP_LOGD("econet", "  ItemVal : %d", item_value);

							uint8_t item_text_len = pdata[tpos+5];

							// uint8_t str_arr[item_text_len];
							char char_arr[item_text_len];

							for (int a = 0; a < item_text_len; a++) {
								// str_arr[a] = pdata[tpos+a+6];
								char_arr[a] = pdata[tpos+a+6];
							}

							// std::string item_string = reinterpret_cast<char *>(str_arr); 
							std::string s(char_arr, sizeof(char_arr));

							ESP_LOGD("econet", "  ItemStr : %s", s.c_str());
						}
						tpos += item_len+1;
						item_num++;
					}
				}
			}
		}
	}
	else
	{
		// ESP_LOGD("econet", "<<< %s", format_hex_pretty((const uint8_t *) buffer, msg_len).c_str());	
	}
// return true;
}
void Econet::read_buffer(int bytes_available) {

	if(bytes_available > 1200)
	{
		ESP_LOGI("econet", "BA=%d,LT=%d ms", bytes_available, this->act_loop_time_);
	}

	// Limit to Read 1200 bytes
	int bytes_to_read = std::min(bytes_available,1200);

	uint8_t bytes[bytes_to_read];

	// Read multiple bytes at the same time
	if(econet_uart->read_array(bytes,bytes_to_read) == false)
	{
		return; 
	}

	for(int i=0;i<bytes_to_read;i++)
	{
		uint8_t byte = bytes[i];
		if(pos == DST_ADR_POS)
		{
			if(byte == 0x80)
			{
				buffer[pos] = byte;
				pos++;
			}
			else
			{
				buffer[pos] = byte;
				pos++;
				// Not the byte we are looking for
				ESP_LOGI("econet", "<<< %s", format_hex_pretty((const uint8_t *) buffer, pos).c_str());
				pos = 0;
			}
		}
		else if(pos == SRC_ADR_POS)
		{
			if(byte == 0x80)
			{
				buffer[pos] = byte;
				pos++;
			}
			else
			{
				buffer[pos] = byte;
				pos++;
				// Not the byte we are looking for
				ESP_LOGI("econet", "<<< %s", format_hex_pretty((const uint8_t *) buffer, pos).c_str());
				// Not the byte we are looking for	
				// TODO
				// Loop through and check if we were off by a couple bytes
				pos = 0;
			}
		}
		else if(pos == LEN_POS)
		{
			data_len = byte;
			msg_len = data_len + MSG_HEADER_SIZE + MSG_CRC_SIZE;
			buffer[pos] = byte;
			pos++;
		}
		else
		{
			buffer[pos] = byte;
			pos++;	
		}

		if(pos == msg_len && msg_len != 0 && pos != 0)
		{
			// We have a full message
			this->parse_rx_message();
			pos = 0;
			msg_len = 0;
			data_len = 0;
		}
		else if(pos == msg_len && msg_len != 0)
		{
			ESP_LOGD("econet", "This would have cuased problems");
		}
	}
}
void Econet::loop() {
	const uint32_t now = millis();
	bool flag = false;
	
	// Read Every 50ms After 10s afer boot
	if (now - this->last_read_ > 10) {
		this->act_loop_time_ = now - this->last_read_;
		this->last_read_ = now;
		// Read Everything that is in the buffer
		int bytes_available = econet_uart->available();

		if(bytes_available > 0)
		{
			this->last_read_data_ = now;
			ESP_LOGI("econet", "Read %d. ms=%d, lt=%d", bytes_available, now, act_loop_time_);
			flag = true;
			this->read_buffer(bytes_available);
		}
		else
		{
			// ESP_LOGI("econet", "--- millis()=%d", now);
		}
		if (now - this->last_read_data_ > 100)
		{
			// Bus is Assumbed Available For Sending
			// This currently attempts a request every 1000ms
			if (now - this->last_request_ > 1000) {
				if(type_id_ != 2)
				{
					ESP_LOGI("econet", "request ms=%d", now);
					this->last_request_ = now;
					this->make_request();
					req_id++;
					if(req_id > 0)
					{
						req_id = 0;	
					}
				}
			}
		}
	}
}
void Econet::send_datapoint(uint8_t datapoint_id, float value)
{
	for (auto &listener : this->listeners_) {
      if (listener.datapoint_id == datapoint_id)
        listener.on_datapoint(value);
    }	
}
void Econet::set_enable_state(bool state)
{
	if(state)
	{
		this->send_enable_disable = true;
		this->enable_disable_cmd = true;
	}
	else
	{
		this->send_enable_disable = true;
		this->enable_disable_cmd = false;
	}
}
void Econet::set_new_setpoint(float setpoint)
{
	send_new_setpoint = true;
	new_setpoint = setpoint;
}
void Econet::set_new_mode(float mode)
{
	send_new_mode = true;
	new_mode = mode;
}
void Econet::dump_state() {
  
}
void Econet::register_listener(uint8_t datapoint_id, const std::function<void(float)> &func) {
  auto listener = DatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);
}
	
}  // namespace econet
}  // namespace esphome
