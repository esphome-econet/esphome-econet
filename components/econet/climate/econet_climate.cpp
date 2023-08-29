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

  if (this->econet->get_type_id() == 2) {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                                climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_FAN_ONLY});
  } else {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO});
  }

  if (this->econet->get_type_id() == 1) {
    traits.add_supported_custom_preset("Off");
    traits.add_supported_custom_preset("Eco Mode");
    traits.add_supported_custom_preset("Heat Pump");
    traits.add_supported_custom_preset("High Demand");
    traits.add_supported_custom_preset("Electric");
    traits.add_supported_custom_preset("Vacation");
  }
  traits.set_supports_current_temperature(true);
  if (this->econet->get_type_id() == 2) {
    traits.set_visual_min_temperature(10);
    traits.set_visual_max_temperature(32);

    traits.set_supported_custom_fan_modes({"Automatic", "Speed 1 (Low)", "Speed 2 (Medium Low)", "Speed 3 (Medium)",
                                           "Speed 4 (Medium High)", "Speed 5 (High)"});

    traits.set_supports_two_point_target_temperature(true);
  } else {
    traits.set_visual_min_temperature(SETPOINT_MIN);
    traits.set_visual_max_temperature(SETPOINT_MAX);

    traits.set_supports_two_point_target_temperature(false);
  }
  traits.set_visual_temperature_step(SETPOINT_STEP);

  return traits;
}

void EconetClimate::update() {
  if (this->econet->is_ready()) {
    if (this->econet->get_type_id() == 2) {
      // this->set_target_temperature_low_(nullptr);
      // this->target_temperature_low = NAN;
      this->target_temperature_low = (this->econet->get_cc_heat_setpoint() - 32) * 5 / 9;
      // this->target_temperature_low = nullptr;
      this->target_temperature_high = (this->econet->get_cc_cool_setpoint() - 32) * 5 / 9;
      // this->target_temperature = (this->econet->get_cc_cool_setpoint() - 32)*5/9;
      this->current_temperature = (this->econet->get_cc_spt_stat() - 32) * 5 / 9;
    } else {
      this->target_temperature = (this->econet->get_setpoint() - 32) * 5 / 9;
      this->current_temperature = (this->econet->get_current_temp() - 32) * 5 / 9;
    }
    if (this->econet->get_type_id() == 2) {
      if (this->econet->get_cc_statmode() == 0) {
        this->mode = climate::CLIMATE_MODE_HEAT;
      } else if (this->econet->get_cc_statmode() == 1) {
        this->mode = climate::CLIMATE_MODE_COOL;
      } else if (this->econet->get_cc_statmode() == 2) {
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      } else if (this->econet->get_cc_statmode() == 3) {
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      } else if (this->econet->get_cc_statmode() == 4) {
        this->mode = climate::CLIMATE_MODE_OFF;
      }

      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
      }

      if (this->econet->get_cc_fan_mode() == 0) {
        this->set_custom_fan_mode_("Automatic");
      } else if (this->econet->get_cc_fan_mode() == 1) {
        this->set_custom_fan_mode_("Speed 1 (Low)");
      } else if (this->econet->get_cc_fan_mode() == 2) {
        this->set_custom_fan_mode_("Speed 2 (Medium Low)");
      } else if (this->econet->get_cc_fan_mode() == 3) {
        this->set_custom_fan_mode_("Speed 3 (Medium)");
      } else if (this->econet->get_cc_fan_mode() == 4) {
        this->set_custom_fan_mode_("Speed 4 (Medium High)");
      } else if (this->econet->get_cc_fan_mode() == 5) {
        this->set_custom_fan_mode_("Speed 5 (High)");
      }
    } else {
      if (this->econet->get_enable_state() == true) {
        // Auto	 CLIMATE_MODE_AUTO
        this->mode = climate::CLIMATE_MODE_AUTO;
      } else {
        // Off
        this->mode = climate::CLIMATE_MODE_OFF;
      }
    }
    if (this->econet->get_type_id() == 1) {
      switch ((int) this->econet->get_mode()) {
        case 0:
          this->set_custom_preset_("Off");
          break;
        case 1:
          this->set_custom_preset_("Eco Mode");
          break;
        case 2:
          this->set_custom_preset_("Heat Pump");
          break;
        case 3:
          this->set_custom_preset_("High Demand");
          break;
        case 4:
          this->set_custom_preset_("Electric");
          break;
        case 5:
          this->set_custom_preset_("Vacation");
          break;
        default:
          this->set_custom_preset_("Off");
      }
    }
    this->publish_state();
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  climate::ClimateMode climate_mode = this->mode;
  std::string fan_mode = this->custom_fan_mode.value_or("Auto");

  if (call.get_target_temperature_low().has_value()) {
    this->econet->set_new_setpoint_low(call.get_target_temperature_low().value() * 9 / 5 + 32);
  }

  if (call.get_target_temperature_high().has_value()) {
    this->econet->set_new_setpoint_high(call.get_target_temperature_high().value() * 9 / 5 + 32);
  }

  if (call.get_target_temperature().has_value()) {
    this->econet->set_new_setpoint(call.get_target_temperature().value() * 9 / 5 + 32);
    // ESP_LOGD("econet", "Lets change the temp to %f", setpoint);
  }

  if (call.get_mode().has_value()) {
    climate_mode = call.get_mode().value();
    uint8_t new_mode = 0;

    switch (climate_mode) {
      case climate::CLIMATE_MODE_HEAT_COOL:
        new_mode = 2;
        break;
      case climate::CLIMATE_MODE_HEAT:
        new_mode = 0;
        break;
      case climate::CLIMATE_MODE_COOL:
        new_mode = 1;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        new_mode = 3;
        break;
      case climate::CLIMATE_MODE_OFF:
        new_mode = 4;
        break;
      default:
        new_mode = 4;
    }
    ESP_LOGI("econet", "Raw Mode is %d", climate_mode);
    ESP_LOGI("econet", "Lets change the mode to %d", new_mode);
    // call.get_preset().value()
    // Call to this->econet->setMode
    this->econet->set_new_mode(new_mode);
  }

  if (call.get_custom_fan_mode().has_value()) {
    fan_mode = call.get_custom_fan_mode().value();
    int new_fan_mode = 0;
    // {"Auto", "Speed 1 (Low)", "Speed 2 (Medium Low)", "Speed 3 (Medium)", "Speed 4 (Medium High)", "Speed 5 (High)"}
    if (fan_mode == "Automatic") {
      new_fan_mode = 0;
    } else if (fan_mode == "Speed 1 (Low)") {
      new_fan_mode = 1;
    } else if (fan_mode == "Speed 2 (Medium Low)") {
      new_fan_mode = 2;
    } else if (fan_mode == "Speed 3 (Medium)") {
      new_fan_mode = 3;
    } else if (fan_mode == "Speed 4 (Medium High)") {
      new_fan_mode = 4;
    } else if (fan_mode == "Speed 5 (High)") {
      new_fan_mode = 5;
    }
    this->econet->set_new_fan_mode(new_fan_mode);
  }

  if (call.get_preset().has_value()) {
    // ESP_LOGI("econet", "Lets change the temp to %f", setpoint);
    // call.get_preset().value()
    // Call to this->econet->setMode
    // this->econet->set_new_mode(mode);
  }
  if (call.get_custom_preset().has_value()) {
    std::string preset = call.get_custom_preset().value();

    ESP_LOGI("econet", "Set custom preset: %s", preset);

    uint8_t new_mode = -1;

    if (preset == "Off") {
      new_mode = 0;
    } else if (preset == "Eco Mode") {
      new_mode = 1;
    } else if (preset == "Heat Pump") {
      new_mode = 2;
    } else if (preset == "High Demand") {
      new_mode = 3;
    } else if (preset == "Electric") {
      new_mode = 4;
    } else if (preset == "Vacation") {
      new_mode = 5;
    }

    if (new_mode != -1) {
      this->econet->set_new_mode(new_mode);
    }
  }
}

}  // namespace econet
}  // namespace esphome
