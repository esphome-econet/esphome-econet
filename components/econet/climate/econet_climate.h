#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../econet.h"
#include "esphome/components/climate/climate.h"
#include <string>
#include <vector>

namespace esphome {
namespace econet {

class EconetClimate : public climate::Climate, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_current_temperature_id(const std::string &current_temperature_id) {
    this->current_temperature_id_ = current_temperature_id;
  }
  void set_target_temperature_id(const std::string &target_temperature_id) {
    this->target_temperature_id_ = target_temperature_id;
  }
  void set_target_temperature_low_id(const std::string &target_temperature_low_id) {
    this->target_temperature_low_id_ = target_temperature_low_id;
  }
  void set_target_temperature_high_id(const std::string &target_temperature_high_id) {
    this->target_temperature_high_id_ = target_temperature_high_id;
  }
  void set_mode_id(const std::string &mode_id) { this->mode_id_ = mode_id; }
  void set_custom_preset_id(const std::string &custom_preset_id) { this->custom_preset_id_ = custom_preset_id; }
  void set_custom_fan_mode_id(const std::string &custom_fan_mode_id) { this->custom_fan_mode_id_ = custom_fan_mode_id; }
  void set_custom_fan_mode_no_schedule_id(const std::string &custom_fan_mode_no_schedule_id) {
    this->custom_fan_mode_no_schedule_id_ = custom_fan_mode_no_schedule_id;
  }
  void set_follow_schedule_id(const std::string &follow_schedule_id) { this->follow_schedule_id_ = follow_schedule_id; }
  void set_modes(const std::vector<uint8_t> &keys, const std::vector<climate::ClimateMode> &values) {
    if (keys.size() != values.size()) {
      return;
    }
    this->modes_.init(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
      this->modes_.push_back({keys[i], values[i]});
    }
  }
  void set_custom_presets(const std::vector<uint8_t> &keys, const std::vector<std::string> &values) {
    if (keys.size() != values.size()) {
      return;
    }
    this->custom_presets_.init(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
      this->custom_presets_.push_back({keys[i], values[i]});
    }
  }
  void set_custom_fan_modes(const std::vector<uint8_t> &keys, const std::vector<std::string> &values) {
    if (keys.size() != values.size()) {
      return;
    }
    this->custom_fan_modes_.init(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
      this->custom_fan_modes_.push_back({keys[i], values[i]});
    }
  }
  void set_current_humidity_id(const std::string &current_humidity_id) {
    this->current_humidity_id_ = current_humidity_id;
  }
  void set_target_dehumidification_level_id(const std::string &target_dehumidification_level_id) {
    this->target_dehumidification_level_id_ = target_dehumidification_level_id;
  }

 protected:
  struct EconetClimateMode {
    uint8_t id;
    climate::ClimateMode mode;
  };

  struct EconetPreset {
    uint8_t id;
    std::string name;
  };

  struct EconetFanMode {
    uint8_t id;
    std::string name;
  };

  std::string current_temperature_id_{""};
  std::string target_temperature_id_{""};
  std::string target_temperature_low_id_{""};
  std::string target_temperature_high_id_{""};
  std::string mode_id_{""};
  std::string custom_preset_id_{""};
  std::string custom_fan_mode_id_{""};
  std::string custom_fan_mode_no_schedule_id_{""};
  std::string follow_schedule_id_{""};
  optional<bool> follow_schedule_;
  std::string fan_mode_{""};
  std::string fan_mode_no_schedule_{""};
  FixedVector<EconetClimateMode> modes_;
  FixedVector<EconetPreset> custom_presets_;
  FixedVector<EconetFanMode> custom_fan_modes_;
  std::string current_humidity_id_{""};
  std::string target_dehumidification_level_id_{""};
  climate::ClimateTraits traits_;
  bool traits_initialized_{false};
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

}  // namespace econet
}  // namespace esphome
