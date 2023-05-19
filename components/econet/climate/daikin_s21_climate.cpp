#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "daikin_s21_climate.h"

using namespace esphome;

namespace esphome {
namespace daikin_s21 {

#define SETPOINT_MIN 18
#define SETPOINT_MAX 32
#define SETPOINT_STEP 0.5f

static const char *const TAG = "daikin_s21.climate";

void DaikinS21Climate::dump_config() {
  ESP_LOGCONFIG(TAG, "DaikinS21Climate:");
  ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
  this->dump_traits_(TAG);
}

climate::ClimateTraits DaikinS21Climate::traits() {
  auto traits = climate::ClimateTraits();

  traits.set_supports_action(true);

  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(SETPOINT_MIN);
  traits.set_visual_max_temperature(SETPOINT_MAX);
  traits.set_visual_temperature_step(SETPOINT_STEP);
  traits.set_supports_two_point_target_temperature(false);

  traits.set_supported_modes(
      {climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL,
       climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
       climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY});

  traits.set_supported_custom_fan_modes({"Automatic", "1", "2", "3", "4", "5"});

  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_BOTH,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
  });

  return traits;
}

climate::ClimateMode DaikinS21Climate::d2e_climate_mode(
    DaikinClimateMode mode) {
  switch (mode) {
    case DaikinClimateMode::Auto:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case DaikinClimateMode::Cool:
      return climate::CLIMATE_MODE_COOL;
    case DaikinClimateMode::Heat:
      return climate::CLIMATE_MODE_HEAT;
    case DaikinClimateMode::Dry:
      return climate::CLIMATE_MODE_DRY;
    case DaikinClimateMode::Fan:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case DaikinClimateMode::Disabled:
    default:
      return climate::CLIMATE_MODE_OFF;
  }
}

DaikinClimateMode DaikinS21Climate::e2d_climate_mode(
    climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      return DaikinClimateMode::Auto;
    case climate::CLIMATE_MODE_HEAT:
      return DaikinClimateMode::Heat;
    case climate::CLIMATE_MODE_COOL:
      return DaikinClimateMode::Cool;
    case climate::CLIMATE_MODE_DRY:
      return DaikinClimateMode::Dry;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return DaikinClimateMode::Fan;
    default:
      return DaikinClimateMode::Disabled;
  }
}

const std::string DaikinS21Climate::d2e_fan_mode(DaikinFanMode mode) {
  switch (mode) {
    case DaikinFanMode::Speed1:
      return "1";
    case DaikinFanMode::Speed2:
      return "2";
    case DaikinFanMode::Speed3:
      return "3";
    case DaikinFanMode::Speed4:
      return "4";
    case DaikinFanMode::Speed5:
      return "5";
    case DaikinFanMode::Auto:
    default:
      return "Automatic";
  }
}

DaikinFanMode DaikinS21Climate::e2d_fan_mode(std::string mode) {
  if (mode == "Automatic")
    return DaikinFanMode::Auto;
  if (mode == "1")
    return DaikinFanMode::Speed1;
  if (mode == "2")
    return DaikinFanMode::Speed2;
  if (mode == "3")
    return DaikinFanMode::Speed3;
  if (mode == "4")
    return DaikinFanMode::Speed4;
  if (mode == "5")
    return DaikinFanMode::Speed5;
  return DaikinFanMode::Auto;
}

climate::ClimateAction DaikinS21Climate::d2e_climate_action() {
  if (this->s21->is_idle()) {
    return climate::CLIMATE_ACTION_IDLE;
  }
  switch (this->s21->get_climate_mode()) {
    uint16_t setpoint, temp_inside;

    case DaikinClimateMode::Auto:
      setpoint = this->s21->get_setpoint();
      temp_inside = this->s21->get_temp_inside();
      if (setpoint > temp_inside) {
        return climate::CLIMATE_ACTION_HEATING;
      } else if (setpoint < temp_inside) {
        return climate::CLIMATE_ACTION_COOLING;
      }
      return climate::CLIMATE_ACTION_IDLE;
    case DaikinClimateMode::Cool:
      return climate::CLIMATE_ACTION_COOLING;
    case DaikinClimateMode::Heat:
      return climate::CLIMATE_ACTION_HEATING;
    case DaikinClimateMode::Dry:
      return climate::CLIMATE_ACTION_DRYING;
    case DaikinClimateMode::Fan:
      return climate::CLIMATE_ACTION_FAN;
    default:
      return climate::CLIMATE_ACTION_OFF;
  }
}

climate::ClimateSwingMode DaikinS21Climate::d2e_swing_mode(bool swing_v,
                                                           bool swing_h) {
  if (swing_v && swing_h)
    return climate::CLIMATE_SWING_BOTH;
  if (swing_v)
    return climate::CLIMATE_SWING_VERTICAL;
  if (swing_h)
    return climate::CLIMATE_SWING_HORIZONTAL;
  return climate::CLIMATE_SWING_OFF;
}

bool DaikinS21Climate::e2d_swing_h(climate::ClimateSwingMode mode) {
  return mode == climate::CLIMATE_SWING_BOTH ||
         mode == climate::CLIMATE_SWING_HORIZONTAL;
}

bool DaikinS21Climate::e2d_swing_v(climate::ClimateSwingMode mode) {
  return mode == climate::CLIMATE_SWING_BOTH ||
         mode == climate::CLIMATE_SWING_VERTICAL;
}

void DaikinS21Climate::update() {
  if (this->s21->is_ready()) {
    this->mode = this->d2e_climate_mode(this->s21->get_climate_mode());
    this->set_custom_fan_mode_(this->d2e_fan_mode(this->s21->get_fan_mode()));
    this->swing_mode = this->d2e_swing_mode(this->s21->get_swing_v(),
                                            this->s21->get_swing_h());
    this->action = this->d2e_climate_action();
    this->current_temperature = this->s21->get_temp_inside();
    this->target_temperature = this->s21->get_setpoint();
    this->publish_state();
  }
}

void DaikinS21Climate::control(const climate::ClimateCall &call) {
  float setpoint = this->target_temperature;
  climate::ClimateMode climate_mode = this->mode;
  std::string fan_mode = this->custom_fan_mode.value_or("Automatic");
  bool set_basic = false;

  if (call.get_mode().has_value()) {
    climate_mode = call.get_mode().value();
    set_basic = true;
  }
  if (call.get_target_temperature().has_value()) {
    setpoint = call.get_target_temperature().value();
    set_basic = true;
  }
  if (call.get_custom_fan_mode().has_value()) {
    fan_mode = call.get_custom_fan_mode().value();
    set_basic = true;
  }

  if (set_basic) {
    this->s21->set_daikin_climate_settings(
        climate_mode != climate::CLIMATE_MODE_OFF,
        this->e2d_climate_mode(climate_mode), setpoint,
        this->e2d_fan_mode(fan_mode));
  }

  if (call.get_swing_mode().has_value()) {
    climate::ClimateSwingMode swing_mode = call.get_swing_mode().value();
    this->s21->set_swing_settings(this->e2d_swing_v(swing_mode),
                                  this->e2d_swing_h(swing_mode));
  }
}

}  // namespace daikin_s21
}  // namespace esphome
