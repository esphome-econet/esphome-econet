#include "esphome.h"

#define get_econet(constructor) static_cast<EconetRS485 *>(constructor.get_component(0))

#define wh_enabled_switch(my_econet) get_econet(my_econet)->get_wh_enabled_switch()
#define wh_enabled_binary_sensor(my_econet) get_econet(my_econet)->get_wh_enabled_binary_sensor()

class EconetRS485;

class DataObject
{
  public:
    std::string label;
    uint8_t type; // 0 = FLOAT, 2 = ENUMERATED TEXT
    float float_val;
    uint8_t enum_val;
    std::string str_val;
}

using namespace esphome;

class WHEnabledSwitch : public switch_::Switch
{
public:
  WHEnabledSwitch(EconetRS485 *parent)
  {
  	this->parent_ = parent;
  }
  void write_state(bool state) override;
  
protected:
  EconetRS485 *parent_;
  bool setup_{true};
  bool state_{false};
};

class WHEnabledBinarySesnor : public binary_sensor::BinarySensor
{
public:
  WHEnabledBinarySesnor(EconetRS485 *parent)
  {
  	this->parent_ = parent;
  }
  
protected:
  EconetRS485 *parent_;
  bool setup_{true};
  bool state_{false};
};

class EconetRS485 : public Component, public UARTDevice, public Sensor {
	public:
		EconetRS485(UARTComponent *parent) : UARTDevice(parent) {}
		Sensor *flow_rate_sensor = new Sensor();
  		Sensor *temp_out_sensor = new Sensor();	
		Sensor *temp_in_sensor = new Sensor();	
		Sensor *setpoint_temp_sensor = new Sensor();
		Sensor *water_used_sensor = new Sensor();	
		Sensor *btus_used_sensor = new Sensor();
		Sensor *ign_cycle_counts_sensor = new Sensor();
		Sensor *instant_btus_sensor = new Sensor();
	
		static const int max_message_size = 271;
		uint8_t buffer[max_message_size];
		int pos = 0;
		uint32_t last_read_{0};
		uint32_t last_request_{0};
		uint8_t data_len = 0;
		uint16_t msg_len = 0;
		bool send_enable_disable = false;
		bool enable_disable_cmd = false;
	
		uint8_t wbuffer[max_message_size];
		uint16_t wmsg_len = 0;
	
		uint32_t COMPUTER =      				192	;	// 80 00 00 C0
		uint32_t UNKNOWN_HANDLER =  			241	;	// 80 00 00 F1
		uint32_t WIFI_MODULE =    				832	;	// 80 00 03 40
		uint32_t SMARTEC_TRANSLATOR = 			4160;	// 80 00 10 40
		uint32_t INTERNAL = 					4736; 	// 80 00 10 40

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
	
		uint8_t req_id = 0;
	
		void setup() override {
		// nothing to do here
		}

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
		bool parse_message()
		{
			bool logvals = true;
			
			uint8_t data[255];
			
			uint32_t dst_adr = ((buffer[0] & 0x7f) << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
			uint8_t dst_bus = buffer[4];
			
			uint32_t src_adr = ((buffer[5] & 0x7f) << 24) + (buffer[6] << 16) + (buffer[7] << 8) + buffer[8];
			uint8_t src_bus = buffer[9];
			
			uint8_t data_len = buffer[10];
			
			uint8_t command = buffer[13];
			
			uint16_t crc = (buffer[data_len + MSG_HEADER_SIZE]) + (buffer[data_len + MSG_HEADER_SIZE + 1] << 8);
			
			uint16_t crc_check = gen_crc16(buffer, data_len + 14);
			
			for(int i = 0; i < data_len; i++)
			{
				data[i] = buffer[MSG_HEADER_SIZE + i];
			}
			
			bool recognized = false;
			
			if(dst_adr == WIFI_MODULE && src_adr == SMARTEC_TRANSLATOR)
			{
				if(command == ACK && msg_len == 115)
				{
					recognized = true;
					
					int tpos = 0;
					uint8_t item_num = 1;
					
					float temp_in = 55;
					float temp_out = 120;
					
					float flow_rate = 1;
					
					while(tpos < data_len)
					{
						uint8_t item_len = data[tpos];
						uint8_t item_type = data[tpos+1] & 0x7F;
						
						if(item_type == 0)
						{
							float item_value = bytesToFloat(data[tpos+4],data[tpos+5],data[tpos+6],data[tpos+7]);
							if(logvals){
								if(item_num == 1)
								{
									flow_rate = item_value/3.785;
									flow_rate_sensor->publish_state(item_value/3.785);
									ESP_LOGD("econet", "FLOWRATE: %f", item_value/3.785);
								}
								else if(item_num == 999)
								{
									// ESP_LOGD("econet", "HTRS__ON: %f", item_value);
								}
								else if(item_num == 2)
								{
									temp_out = item_value;
									temp_out_sensor->publish_state(item_value);
									ESP_LOGD("econet", "TEMP_OUT: %f", item_value);
								}
								else if(item_num == 3)
								{
									temp_in = item_value;
									temp_in_sensor->publish_state(item_value);
									ESP_LOGD("econet", "TEMP_IN: %f", item_value);
								}
								else if(item_num == 6)
								{
									setpoint_temp_sensor->publish_state(item_value);
									ESP_LOGD("econet", "WHTRSETP: %f", item_value);
								}
								else if(item_num == 7)
								{
									water_used_sensor->publish_state(item_value);
									// ESP_LOGD("econet", "WHTRSETP: %f", item_value);
								}
								else if(item_num == 8)
								{
									btus_used_sensor->publish_state(item_value);
									// ESP_LOGD("econet", "WHTRSETP: %f", item_value);
								}
								else if(item_num == 9)
								{
									ign_cycle_counts_sensor->publish_state(item_value);
									// ESP_LOGD("econet", "WHTRSETP: %f", item_value);
								}
							}
						}
						else if(item_type == 2)
						{
							// Enumerated Text
							
							uint8_t item_value = data[tpos+4];
							
							uint8_t item_text_len = data[tpos+5];
							
							// uint8_t str_arr[item_text_len];
							char char_arr[item_text_len];
							
							for (int a = 0; a < item_text_len; a++) {
								// str_arr[a] = data[tpos+a+6];
								char_arr[a] = data[tpos+a+6];
							}
							
							std::string s(char_arr, sizeof(char_arr));
														
							if(logvals){
								// std::string s((const char*)&(), item_len-1);
								if(item_num == 4)
								{
									if(item_value == 0)
									{
										wh_enabled_binary_sensor->publish_state(false);	
										wh_enabled_switch->publish_state(false);	
									}
									else if(item_value == 1)
									{
										wh_enabled_binary_sensor->publish_state(true);	
										wh_enabled_switch->publish_state(true);	
									}
									// wh_enabled->publish_state(true);
									ESP_LOGD("econet", "WHTRENAB (val): %d", item_value);
									// ESP_LOGD("econet", "WHTRENAB (raw): %s", format_hex_pretty((const uint8_t *) str_arr, item_text_len).c_str());
									ESP_LOGD("econet", "WHTRENAB (str): %s", s.c_str());
								}
								else if(item_num == 5)
								{
									ESP_LOGD("econet", "WHTRMODE (val): %d", item_value);
									// ESP_LOGD("econet", "WHTRENAB (raw): %s", format_hex_pretty((const uint8_t *) str_arr, item_text_len).c_str());
									ESP_LOGD("econet", "WHTRMODE (str): %s", s.c_str());
								}
							}
						}
						tpos += item_len+1;
						item_num++;
					}
					
					// 1 btu/deg_f*(8.334)*flow_rate (1/min) 
					
					float approx_instant_btus = (temp_out-temp_in)*flow_rate*8.334*60/0.92;
					
					instant_btus_sensor->publish_state(approx_instant_btus/1000);
					
				}
				else if(msg_len == 31)
				{
					// recognized = true;
				}
				// publish_state(format_hex_pretty((const uint8_t *) buffer, msg_len));
			}
			else if(dst_adr == UNKNOWN_HANDLER)
			{
				// Filter these for now
				recognized = true;
			}
			else if(src_adr == UNKNOWN_HANDLER)
			{
				// Filter these for now
				recognized = true;
			}
			else if(dst_adr == SMARTEC_TRANSLATOR && src_adr == WIFI_MODULE)
			{
				// Filter these for now
			}
			
			if(recognized == false)
			{
				ESP_LOGD("econet", "<<< %s", format_hex_pretty((const uint8_t *) buffer, msg_len).c_str());
				ESP_LOGD("econet", "  Dst Adr : 0x%x", dst_adr);
				ESP_LOGD("econet", "  Src Adr : 0x%x", src_adr);
				ESP_LOGD("econet", "  Length  : %d", data_len);
				ESP_LOGD("econet", "  Command : %d", command);
				ESP_LOGD("econet", "  Data    : %s", format_hex_pretty((const uint8_t *) data, data_len).c_str());
				if(false){
        if(command == 30)
				{
					// READ
					uint8_t type = data[0];
					ESP_LOGD("econet", "  Type    : %d", type);
					
					if(type == 1)
					{
						char char_arr[data_len - 6];

						for (int a = 0; a < data_len - 6; a++) {
							char_arr[a] = data[a+4];
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

							uint8_t item_len = data[tpos];
							uint8_t item_type = data[tpos+1] & 0x7F;

							ESP_LOGD("econet", "  ItemLen : %d", item_len);
							ESP_LOGD("econet", "  ItemType: %d", item_type);

							if(item_type == 0)
							{
								float item_value = bytesToFloat(data[tpos+4],data[tpos+5],data[tpos+6],data[tpos+7]);
								ESP_LOGD("econet", "  ItemVal : %f", item_value);
							}
							else if(item_type == 2)
							{
								// Enumerated Text

								uint8_t item_value = data[tpos+4];

								ESP_LOGD("econet", "  ItemVal : %d", item_value);

								uint8_t item_text_len = data[tpos+5];

								// uint8_t str_arr[item_text_len];
								char char_arr[item_text_len];

								for (int a = 0; a < item_text_len; a++) {
									// str_arr[a] = data[tpos+a+6];
									char_arr[a] = data[tpos+a+6];
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
					
					uint8_t type = data[0] & 0x7F;
					
					// 
					if(type == 0)
					{
						
					}
					else
					{
						int tpos = 0;
						uint8_t item_num = 1;

						while(tpos < data_len)
						{
							ESP_LOGD("econet", "  Item[%d] ", item_num);

							uint8_t item_len = data[tpos];
							uint8_t item_type = data[tpos+1] & 0x7F;

							ESP_LOGD("econet", "  ItemLen : %d", item_len);
							ESP_LOGD("econet", "  ItemType: %d", item_type);

							if(item_type == 0)
							{
								float item_value = bytesToFloat(data[tpos+4],data[tpos+5],data[tpos+6],data[tpos+7]);
								ESP_LOGD("econet", "  ItemVal : %f", item_value);
							}
							else if(item_type == 2)
							{
								// Enumerated Text

								uint8_t item_value = data[tpos+4];

								ESP_LOGD("econet", "  ItemVal : %d", item_value);

								uint8_t item_text_len = data[tpos+5];

								// uint8_t str_arr[item_text_len];
								char char_arr[item_text_len];

								for (int a = 0; a < item_text_len; a++) {
									// str_arr[a] = data[tpos+a+6];
									char_arr[a] = data[tpos+a+6];
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
		}
	
		int read_buffer(int bytes_available) {
			
      // Limit to Read 255 bytes
      int bytes_to_read = std::min(bytes_available,255);
      
			uint8_t bytes[bytes_to_read];
			
      // Read multiple bytes at the same time
      if(read_array(bytes,bytes_to_read) == false)
      {
        return -1; 
      }
      /*
			if (!read_byte(&byte)) {
				return -1;
			}
      */
			
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
            // Not the byte we are looking for	
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

        if(pos == msg_len && msg_len != 0)
        {
          // We have a full message
          parse_message();
          pos = 0;
          msg_len = 0;
          data_len = 0;
        }
      }
			return -1;
		}
		
		bool make_request()
		{
			// READ ALARMS
			// [12:25:42][D][econet:259]: <<< 80.00.10.40.00.80.00.03.40.00.0C.00.00.1E.01.01.00.00.41.4C.41.52.4D.53.00.00.50.3E (28)
// [12:25:42][D][econet:259]: <<< 80.00.03.40.00.80.00.10.40.00.0F.00.00.06.80.00.00.00.00.00.00.47.7F.FF.00.00.00.00.00.E7.F7 (31)

			
				uint32_t dst_adr = 0x1040;
				//uint32_t dst_adr = 0x340;
				uint8_t dsr_bus = 0x00;

				// uint32_t src_adr = COMPUTER; // WIFI_MODULE; // COMPUTER;
				uint32_t src_adr = WIFI_MODULE;
				//uint32_t src_adr = 0x1040;
				uint8_t src_bus = 0x00;

				std::vector<std::string> str_ids{};

				if(req_id == 0)
				{
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
				else
				{
					str_ids.push_back("ALARMS");
				}

				uint8_t num_of_strs = str_ids.size();

				uint8_t command = READ_COMMAND;
				uint16_t wdata_len = 4+10*num_of_strs;
			
				std::vector<uint8_t> data(wdata_len);
			
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
				else
				{
					command = READ_COMMAND;
					
					if(num_of_strs > 1)
					{
						data[0] = 2;
					}
					else
					{
						data[0] = 1;
					}

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

			if(true)
			{
				wbuffer[0] = 0x80;
				wbuffer[1] = (uint8_t) (dst_adr >> 16);
				wbuffer[2] = (uint8_t) (dst_adr >> 8);
				wbuffer[3] = (uint8_t) dst_adr;
				wbuffer[4] = dsr_bus;

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

				write_array(wbuffer,wdata_len+14+2);
				flush();
				// delay(100);

				// digitalWrite(0, LOW);

				ESP_LOGD("econet", ">>> %s", format_hex_pretty((const uint8_t *) wbuffer, wdata_len+14+2).c_str());
			}
		}
	  void read_the_buffer() 
    {
      
    }
		void loop() override {
			const uint32_t now = millis();
			bool flag = false;
      
      // Read Every 50ms After 10s afer boot
      if (now - this->last_read_ > 50 && now > 10000) {
        this->last_read_ = now;
        // Read Everything that is in the buffer
        int bytes_available = available();
        if(bytes_available > 0)
        {
          flag = true;
          if(read_buffer(bytes_available) > 0) {

          }
        }
        if(flag == false)
        {
          // Bus is Assumbed Available For Sending
          // This currently attempts a request every 1000ms
          // Only start requesting data 15s after boot
          if (now - this->last_request_ > 1000 && now > 15000) {
            this->last_request_ = now;
            make_request();
            req_id++;
            if(req_id > 0)
            {
              req_id = 0;	
            }
          }
        }
        // publish_state(buffer);
      }
		}
		
		binary_sensor::BinarySensor *get_wh_enabled_binary_sensor()
		{
			WHEnabledBinarySesnor *my_switch = new WHEnabledBinarySesnor(this);
			wh_enabled_binary_sensor = my_switch;
			//analog_pins_.push_back(input);
			return my_switch;
		}
		switch_::Switch *get_wh_enabled_switch()
		{
			WHEnabledSwitch *my_switch = new WHEnabledSwitch(this);
			wh_enabled_switch = my_switch;
			//analog_pins_.push_back(input);
			return my_switch;
		}
		void write_state(bool state, bool setup = false)
		{
			//if (this->configure_timeout_ != 0)
			//	return;
			// Do work here
			send_enable_disable = true;
			enable_disable_cmd = state;
			if (setup)
			{
				App.feed_wdt();
			}
		}
	protected:
		WHEnabledBinarySesnor *wh_enabled_binary_sensor;
		WHEnabledSwitch *wh_enabled_switch;
		unsigned long configure_timeout_{5000};

};
void WHEnabledSwitch::write_state(bool state)
{
  this->state_ = state;
  this->parent_->write_state(state, this->setup_);
  this->setup_ = false;
}
