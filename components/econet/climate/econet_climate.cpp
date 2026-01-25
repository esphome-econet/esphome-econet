#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_traits.h"
#include "econet_climate.h"
#include <algorithm>

using namespace esphome;

namespace esphome {
namespace econet {

namespace {

inline float fahrenheit_to_celsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
inline float celsius_to_fahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }

}  // namespace

static const char *const TAG = "econet.climate";

void EconetClimate::dump_config() {
  LOG_CLIMATE("", "Econet Climate", this);
  this->dump_traits_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
  if (this->traits_initialized_) {
    return this->traits_;
  }
  auto traits = climate::ClimateTraits();
  if (this->current_temperature_id_ && *this->current_temperature_id_) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  if (this->current_humidity_id_ && *this->current_humidity_id_) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  }
  if (this->target_dehumidification_level_id_ && *this->target_dehumidification_level_id_) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  }
  if (this->target_temperature_high_id_ && *this->target_temperature_high_id_) {
    traits.add_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  }
  if (this->mode_id_ && *this->mode_id_) {
    for (const auto &entry : this->modes_) {
      traits.add_supported_mode(entry.mode);
    }
  }
  if (this->custom_preset_id_ && *this->custom_preset_id_) {
    std::vector<const char *> presets;
    presets.reserve(this->custom_presets_.size());
    for (const auto &entry : this->custom_presets_) {
      presets.push_back(entry.name);
    }
    traits.set_supported_custom_presets(presets);
  }
  if (this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) {
    std::vector<const char *> fans;
    fans.reserve(this->custom_fan_modes_.size());
    for (const auto &entry : this->custom_fan_modes_) {
      fans.push_back(entry.name);
    }
    traits.set_supported_custom_fan_modes(fans);
  }
  this->traits_ = traits;
  this->traits_initialized_ = true;
  return this->traits_;
}

void EconetClimate::register_float_listener(const char *id, float *member, bool is_temperature) {
  if (id && *id) {
    this->parent_->register_listener(
        id, this->request_mod_, this->request_once_,
        [this, member, is_temperature](const EconetDatapoint &datapoint) {
          float val = datapoint.value_float;
          if (is_temperature) {
            *member = fahrenheit_to_celsius(val);
          } else {
            *member = val;
          }
          this->publish_state();
        },
        false, this->src_adr_);
  }
}

void EconetClimate::register_fan_listener(const char *id, std::string *member, bool schedule_val) {
  if (id && *id) {
    this->parent_->register_listener(
        id, this->request_mod_, this->request_once_,
        [this, member, schedule_val](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                                 [&](const EconetFanMode &m) { return m.id == datapoint.value_enum; });
          if (it == this->custom_fan_modes_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            *member = it->name;
            if (this->follow_schedule_.has_value()) {
              if (this->follow_schedule_.value() == schedule_val) {
                this->set_custom_fan_mode_(member->c_str(), member->length());
                this->publish_state();
              }
            }
          }
        },
        false, this->src_adr_);
  }
}

void EconetClimate::setup() {
  this->register_float_listener(this->current_temperature_id_, &this->current_temperature, true);
  this->register_float_listener(this->target_temperature_id_, &this->target_temperature, true);
  this->register_float_listener(this->target_temperature_low_id_, &this->target_temperature_low, true);
  this->register_float_listener(this->target_temperature_high_id_, &this->target_temperature_high, true);
  this->register_float_listener(this->current_humidity_id_, &this->current_humidity, false);
  this->register_float_listener(this->target_dehumidification_level_id_, &this->target_humidity, false);

  if (this->mode_id_ && *this->mode_id_) {
    this->parent_->register_listener(
        this->mode_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                                 [&](const EconetClimateMode &m) { return m.id == datapoint.value_enum; });
          if (it == this->modes_.end()) {
            ESP_LOGW(TAG, "In modes of your yaml add a ClimateMode that corresponds to: %d: \"%s\"",
                     datapoint.value_enum, datapoint.value_string.c_str());
          } else {
            this->mode = it->mode;
            this->publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (this->custom_preset_id_ && *this->custom_preset_id_) {
    this->parent_->register_listener(
        this->custom_preset_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                                 [&](const EconetPreset &p) { return p.id == datapoint.value_enum; });
          if (it == this->custom_presets_.end()) {
            ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            this->set_custom_preset_(it->name);
            this->publish_state();
          }
        },
        false, this->src_adr_);
  }

  this->register_fan_listener(this->custom_fan_mode_id_, &this->fan_mode_, true);
  this->register_fan_listener(this->custom_fan_mode_no_schedule_id_, &this->fan_mode_no_schedule_, false);

  if (this->follow_schedule_id_ && *this->follow_schedule_id_) {
    this->parent_->register_listener(
        this->follow_schedule_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          ESP_LOGI(TAG, "MCU reported climate sensor %s is: %s", this->follow_schedule_id_,
                   datapoint.value_string.c_str());
          this->follow_schedule_ = datapoint.value_enum > 0;
          if (this->follow_schedule_.value()) {
            if (!this->fan_mode_.empty()) {
              this->set_custom_fan_mode_(this->fan_mode_.c_str(), this->fan_mode_.length());
              this->publish_state();
            }
          } else {
            if (!this->fan_mode_no_schedule_.empty()) {
              this->set_custom_fan_mode_(this->fan_mode_no_schedule_.c_str(), this->fan_mode_no_schedule_.length());
              this->publish_state();
            }
          }
        },
        false, this->src_adr_);
  }
}

void EconetClimate::set_float_datapoint(const char *id, optional<float> value, bool is_temperature) {
  if (value.has_value() && id && *id) {
    float val = *value;
    if (is_temperature) {
      val = celsius_to_fahrenheit(val);
    }
    this->parent_->set_float_datapoint_value(id, val, this->src_adr_);
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  this->set_float_datapoint(this->target_temperature_id_, call.get_target_temperature(), true);
  this->set_float_datapoint(this->target_temperature_low_id_, call.get_target_temperature_low(), true);
  this->set_float_datapoint(this->target_temperature_high_id_, call.get_target_temperature_high(), true);
  this->set_float_datapoint(this->target_dehumidification_level_id_, call.get_target_humidity(), false);

  if (call.get_mode().has_value() && this->mode_id_ && *this->mode_id_) {
    climate::ClimateMode mode = call.get_mode().value();
    auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                           [&mode](const EconetClimateMode &m) { return m.mode == mode; });
    if (it != this->modes_.end()) {
      this->parent_->set_enum_datapoint_value(this->mode_id_, it->id, this->src_adr_);
    }
  }
  if (call.has_custom_preset() && this->custom_preset_id_ && *this->custom_preset_id_) {
    auto preset = call.get_custom_preset();
    auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                           [&preset](const EconetPreset &p) { return p.name == preset; });
    if (it != this->custom_presets_.end()) {
      this->parent_->set_enum_datapoint_value(this->custom_preset_id_, it->id, this->src_adr_);
    }
  }
  if (call.has_custom_fan_mode() && this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) {
    auto fan_mode = call.get_custom_fan_mode();
    auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                           [&fan_mode](const EconetFanMode &m) { return m.name == fan_mode; });
    if (it != this->custom_fan_modes_.end()) {
      if (this->follow_schedule_.has_value()) {
        if (this->follow_schedule_.value()) {
          this->parent_->set_enum_datapoint_value(this->custom_fan_mode_id_, it->id, this->src_adr_);
        } else {
          this->parent_->set_enum_datapoint_value(this->custom_fan_mode_no_schedule_id_, it->id, this->src_adr_);
        }
      }
    }
  }
}

}  // namespace econet
}  // namespace esphome
