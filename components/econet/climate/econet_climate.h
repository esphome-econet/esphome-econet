#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../econet.h"
#include "esphome/components/climate/climate.h"
#include <string>

namespace esphome {
namespace econet {

class EconetClimate : public climate::Climate, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_current_temperature_id(const char *current_temperature_id) {
    this->current_temperature_id_ = current_temperature_id;
  }
  void set_target_temperature_id(const char *target_temperature_id) {
    this->target_temperature_id_ = target_temperature_id;
  }
  void set_target_temperature_low_id(const char *target_temperature_low_id) {
    this->target_temperature_low_id_ = target_temperature_low_id;
  }
  void set_target_temperature_high_id(const char *target_temperature_high_id) {
    this->target_temperature_high_id_ = target_temperature_high_id;
  }
  void set_mode_id(const char *mode_id) { this->mode_id_ = mode_id; }
  void set_custom_preset_id(const char *custom_preset_id) { this->custom_preset_id_ = custom_preset_id; }
  void set_custom_fan_mode_id(const char *custom_fan_mode_id) { this->custom_fan_mode_id_ = custom_fan_mode_id; }
  void set_custom_fan_mode_no_schedule_id(const char *custom_fan_mode_no_schedule_id) {
    this->custom_fan_mode_no_schedule_id_ = custom_fan_mode_no_schedule_id;
  }
  void set_follow_schedule_id(const char *follow_schedule_id) { this->follow_schedule_id_ = follow_schedule_id; }
  void init_modes(size_t size) { this->modes_.init(size); }
  void add_mode(uint8_t id, climate::ClimateMode mode) { this->modes_.push_back({id, mode}); }
  void init_custom_presets(size_t size) { this->custom_presets_.init(size); }
  void add_custom_preset(uint8_t id, const char *name) { this->custom_presets_.push_back({id, name}); }
  void init_custom_fan_modes(size_t size) { this->custom_fan_modes_.init(size); }
  void add_custom_fan_mode(uint8_t id, const char *name) { this->custom_fan_modes_.push_back({id, name}); }
  void set_current_humidity_id(const char *current_humidity_id) { this->current_humidity_id_ = current_humidity_id; }
  void set_target_dehumidification_level_id(const char *target_dehumidification_level_id) {
    this->target_dehumidification_level_id_ = target_dehumidification_level_id;
  }

 protected:
  struct EconetClimateMode {
    uint8_t id;
    climate::ClimateMode mode;
  };

  struct EconetPreset {
    uint8_t id;
    const char *name;
  };

  struct EconetFanMode {
    uint8_t id;
    const char *name;
  };

  const char *current_temperature_id_{nullptr};
  const char *target_temperature_id_{nullptr};
  const char *target_temperature_low_id_{nullptr};
  const char *target_temperature_high_id_{nullptr};
  const char *mode_id_{nullptr};
  const char *custom_preset_id_{nullptr};
  const char *custom_fan_mode_id_{nullptr};
  const char *custom_fan_mode_no_schedule_id_{nullptr};
  const char *follow_schedule_id_{nullptr};
  optional<bool> follow_schedule_;
  std::string fan_mode_{""};
  std::string fan_mode_no_schedule_{""};
  FixedVector<EconetClimateMode> modes_;
  FixedVector<EconetPreset> custom_presets_;
  FixedVector<EconetFanMode> custom_fan_modes_;
  const char *current_humidity_id_{nullptr};
  const char *target_dehumidification_level_id_{nullptr};
  climate::ClimateTraits traits_;
  bool traits_initialized_{false};
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

}  // namespace econet
}  // namespace esphome
