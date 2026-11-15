#pragma once

#include "climate_traits.h"
#include "esphome/core/helpers.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace esphome {
namespace climate {

class Climate;

class ClimateCall {
 public:
  explicit ClimateCall(Climate *parent) : parent_(parent) {}

  ClimateCall &set_mode(ClimateMode mode) {
    this->mode_ = mode;
    return *this;
  }
  ClimateCall &set_target_temperature(float temp) {
    this->target_temperature_ = temp;
    return *this;
  }
  ClimateCall &set_target_temperature_low(float temp) {
    this->target_temperature_low_ = temp;
    return *this;
  }
  ClimateCall &set_target_temperature_high(float temp) {
    this->target_temperature_high_ = temp;
    return *this;
  }
  ClimateCall &set_target_humidity(float humidity) {
    this->target_humidity_ = humidity;
    return *this;
  }
  ClimateCall &set_custom_preset(const char *preset) {
    this->custom_preset_ = preset;
    this->has_custom_preset_ = preset != nullptr && *preset != '\0';
    return *this;
  }
  ClimateCall &set_custom_fan_mode(const char *fan_mode) {
    this->custom_fan_mode_ = fan_mode;
    this->has_custom_fan_mode_ = fan_mode != nullptr && *fan_mode != '\0';
    return *this;
  }

  const optional<ClimateMode> &get_mode() const { return this->mode_; }
  const optional<float> &get_target_temperature() const { return this->target_temperature_; }
  const optional<float> &get_target_temperature_low() const { return this->target_temperature_low_; }
  const optional<float> &get_target_temperature_high() const { return this->target_temperature_high_; }
  const optional<float> &get_target_humidity() const { return this->target_humidity_; }
  bool has_custom_preset() const { return this->has_custom_preset_; }
  StringRef get_custom_preset() const { return StringRef(this->custom_preset_); }
  bool has_custom_fan_mode() const { return this->has_custom_fan_mode_; }
  StringRef get_custom_fan_mode() const { return StringRef(this->custom_fan_mode_); }

  void perform();

 private:
  Climate *parent_{nullptr};
  optional<ClimateMode> mode_;
  optional<float> target_temperature_;
  optional<float> target_temperature_low_;
  optional<float> target_temperature_high_;
  optional<float> target_humidity_;
  const char *custom_preset_{""};
  const char *custom_fan_mode_{""};
  bool has_custom_preset_{false};
  bool has_custom_fan_mode_{false};
};

class Climate {
 public:
  virtual ~Climate() = default;

  ClimateCall make_call() { return ClimateCall(this); }
  void publish_state() { this->publish_count++; }

  bool has_custom_preset() const { return !this->custom_preset_.empty(); }
  StringRef get_custom_preset() const { return StringRef(this->custom_preset_); }
  bool has_custom_fan_mode() const { return !this->custom_fan_mode_.empty(); }
  StringRef get_custom_fan_mode() const { return StringRef(this->custom_fan_mode_); }

  void set_supported_custom_presets(const std::vector<const char *> &presets) { this->supported_presets_ = presets; }
  void set_supported_custom_fan_modes(const std::vector<const char *> &fans) { this->supported_fan_modes_ = fans; }

  float current_temperature{NAN};
  float target_temperature{NAN};
  float target_temperature_low{NAN};
  float target_temperature_high{NAN};
  float current_humidity{NAN};
  float target_humidity{NAN};
  ClimateMode mode{CLIMATE_MODE_OFF};
  ClimateAction action{CLIMATE_ACTION_OFF};
  size_t publish_count{0};

 protected:
  friend class ClimateCall;
  virtual void control(const ClimateCall &call) = 0;
  virtual ClimateTraits traits() = 0;

  void dump_traits_(const char *tag) { (void) tag; }
  bool set_custom_preset_(const char *preset) {
    std::string next = preset != nullptr ? preset : "";
    if (this->custom_preset_ == next) {
      return false;
    }
    this->custom_preset_ = std::move(next);
    return true;
  }
  void clear_custom_preset_() { this->custom_preset_.clear(); }
  bool set_custom_fan_mode_(const char *fan_mode) {
    std::string next = fan_mode != nullptr ? fan_mode : "";
    if (this->custom_fan_mode_ == next) {
      return false;
    }
    this->custom_fan_mode_ = std::move(next);
    return true;
  }
  void clear_custom_fan_mode_() { this->custom_fan_mode_.clear(); }

 private:
  std::vector<const char *> supported_presets_;
  std::vector<const char *> supported_fan_modes_;
  std::string custom_preset_;
  std::string custom_fan_mode_;
};

inline void ClimateCall::perform() { this->parent_->control(*this); }

}  // namespace climate
}  // namespace esphome
