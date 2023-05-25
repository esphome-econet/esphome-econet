#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_traits.h"
#include "econet_climate.h"

using namespace esphome;

namespace esphome {
namespace econet {

#define SETPOINT_MIN 26.6667
#define SETPOINT_MAX 4838889
#define SETPOINT_STEP 1.0f

static const char *const TAG = "econet.climate";

void EconetClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "EconetClimate:");
  ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
  this->dump_traits_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
	auto traits = climate::ClimateTraits();

	traits.set_supports_action(false);

	traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO});
	
	if(this->econet->get_type_id() == 1)
	{
		traits.add_supported_custom_preset("off");
		traits.add_supported_custom_preset("eco");
		traits.add_supported_custom_preset("heat_pump");
		traits.add_supported_custom_preset("high_demand");
		traits.add_supported_custom_preset("electric");
		// traits.add_supported_custom_preset("vacation");
	}
	traits.set_supports_current_temperature(true);
	traits.set_visual_min_temperature(SETPOINT_MIN);
	traits.set_visual_max_temperature(SETPOINT_MAX);
	traits.set_visual_temperature_step(SETPOINT_STEP);
	traits.set_supports_two_point_target_temperature(false);

	return traits;
}

void EconetClimate::update() {
	if (this->econet->is_ready()) {
		this->target_temperature = (this->econet->get_setpoint() - 32)*5/9;
		this->current_temperature = (this->econet->get_current_temp() - 32)*5/9;
		if(this->econet->get_enable_state() == true)
		{
			// Auto	 CLIMATE_MODE_AUTO
			this->mode = climate::CLIMATE_MODE_AUTO;
		}
		else
		{
			// Off	
			this->mode = climate::CLIMATE_MODE_OFF;
		}
		if(this->econet->get_type_id() == 1)
		{
			switch((int)this->econet->get_mode())
			{
				case 0:
					this->set_custom_preset_("off");
					// this->custom_preset = "off";
					break;
				case 1:
					this->set_custom_preset_("eco");
					// this->custom_preset = "eco";
					break;
				case 2:
					this->set_custom_preset_("heat_pump");
					//this->custom_preset = "heat_pump";
					break;
				case 3:
					this->set_custom_preset_("high_demand");
					//this->custom_preset = "high_demand";
					break;
				case 4:
					this->set_custom_preset_("electric");
					//this->custom_preset = "electric";
					break;
				default:
					this->set_custom_preset_("off");
					//this->custom_preset = "off";
			}
			
		}
		this->publish_state();
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
	float setpoint = this->target_temperature;

	bool set_basic = false;
	if (call.get_target_temperature().has_value()) {
		setpoint = call.get_target_temperature().value()*9/5 + 32;

		this->econet->set_new_setpoint(setpoint);
		// ESP_LOGD("econet", "Lets change the temp to %f", setpoint);
		set_basic = true;
	}
	if(call.get_preset().has_value())
	{
		// Call to this->econet->setMode
		this->econet->set_new_mode(mode);
	}
}

}  // namespace daikin_s21
}  // namespace esphome
