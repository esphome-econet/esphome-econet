#include "esphome/core/defines.h"
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

float fahrenheit_to_celsius(float f) { return (f - 32) * 5 / 9; }
float celsius_to_fahrenheit(float c) { return c * 9 / 5 + 32; }

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
  if (!this->current_temperature_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  if (!this->current_humidity_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  }
  if (!this->target_dehumidification_level_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  }
  if (!this->target_temperature_high_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  }
  if (!this->mode_id_.empty()) {
    for (const auto &entry : this->modes_) {
      traits.add_supported_mode(entry.mode);
    }
  }
  if (!this->custom_preset_id_.empty()) {
    std::vector<const char *> presets;
    presets.reserve(this->custom_presets_.size());
    for (const auto &entry : this->custom_presets_) {
      presets.push_back(entry.name.c_str());
    }
    traits.set_supported_custom_presets(presets);
  }
  if (!this->custom_fan_mode_id_.empty()) {
    std::vector<const char *> fans;
    fans.reserve(this->custom_fan_modes_.size());
    for (const auto &entry : this->custom_fan_modes_) {
      fans.push_back(entry.name.c_str());
    }
    traits.set_supported_custom_fan_modes(fans);
  }
  this->traits_ = traits;
  this->traits_initialized_ = true;
  return this->traits_;
}

void EconetClimate::setup() {
  if (!this->current_temperature_id_.empty()) {
    this->parent_->register_listener(
        this->current_temperature_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->current_temperature = fahrenheit_to_celsius(datapoint.value_float);
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->current_humidity_id_.empty()) {
    this->parent_->register_listener(
        this->current_humidity_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->current_humidity = datapoint.value_float;
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->target_dehumidification_level_id_.empty()) {
    this->parent_->register_listener(
        this->target_dehumidification_level_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->target_humidity = datapoint.value_float;
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->target_temperature_id_.empty()) {
    this->parent_->register_listener(
        this->target_temperature_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->target_temperature = fahrenheit_to_celsius(datapoint.value_float);
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->target_temperature_low_id_.empty()) {
    this->parent_->register_listener(
        this->target_temperature_low_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->target_temperature_low = fahrenheit_to_celsius(datapoint.value_float);
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->target_temperature_high_id_.empty()) {
    this->parent_->register_listener(
        this->target_temperature_high_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          this->target_temperature_high = fahrenheit_to_celsius(datapoint.value_float);
          this->publish_state();
        },
        false, this->src_adr_);
  }
  if (!this->mode_id_.empty()) {
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
  if (!this->custom_preset_id_.empty()) {
    this->parent_->register_listener(
        this->custom_preset_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                                 [&](const EconetPreset &p) { return p.id == datapoint.value_enum; });
          if (it == this->custom_presets_.end()) {
            ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            this->set_custom_preset_(it->name.c_str(), it->name.length());
            this->publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (!this->custom_fan_mode_id_.empty()) {
    this->parent_->register_listener(
        this->custom_fan_mode_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                                 [&](const EconetFanMode &m) { return m.id == datapoint.value_enum; });
          if (it == this->custom_fan_modes_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            this->fan_mode_ = it->name;
            if (this->follow_schedule_.has_value()) {
              if (this->follow_schedule_.value()) {
                this->set_custom_fan_mode_(this->fan_mode_.c_str(), this->fan_mode_.length());
                this->publish_state();
              }
            }
          }
        },
        false, this->src_adr_);
  }
  if (!this->custom_fan_mode_no_schedule_id_.empty()) {
    this->parent_->register_listener(
        this->custom_fan_mode_no_schedule_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                                 [&](const EconetFanMode &m) { return m.id == datapoint.value_enum; });
          if (it == this->custom_fan_modes_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            this->fan_mode_no_schedule_ = it->name;
            if (this->follow_schedule_.has_value()) {
              if (!this->follow_schedule_.value()) {
                this->set_custom_fan_mode_(this->fan_mode_no_schedule_.c_str(), this->fan_mode_no_schedule_.length());
                this->publish_state();
              }
            }
          }
        },
        false, this->src_adr_);
  }
  if (!this->follow_schedule_id_.empty()) {
    this->parent_->register_listener(
        this->follow_schedule_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          ESP_LOGI(TAG, "MCU reported climate sensor %s is: %s", this->follow_schedule_id_.c_str(),
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

void EconetClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value() && !this->target_temperature_id_.empty()) {
    this->parent_->set_float_datapoint_value(
        this->target_temperature_id_, celsius_to_fahrenheit(call.get_target_temperature().value()), this->src_adr_);
  }
  if (call.get_target_temperature_low().has_value() && !this->target_temperature_low_id_.empty()) {
    this->parent_->set_float_datapoint_value(this->target_temperature_low_id_,
                                             celsius_to_fahrenheit(call.get_target_temperature_low().value()),
                                             this->src_adr_);
  }
  if (call.get_target_temperature_high().has_value() && !this->target_temperature_high_id_.empty()) {
    this->parent_->set_float_datapoint_value(this->target_temperature_high_id_,
                                             celsius_to_fahrenheit(call.get_target_temperature_high().value()),
                                             this->src_adr_);
  }
  if (call.get_mode().has_value() && !this->mode_id_.empty()) {
    climate::ClimateMode mode = call.get_mode().value();
    auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                           [&mode](const EconetClimateMode &m) { return m.mode == mode; });
    if (it != this->modes_.end()) {
      this->parent_->set_enum_datapoint_value(this->mode_id_, it->id, this->src_adr_);
    }
  }
  if (call.has_custom_preset() && !this->custom_preset_id_.empty()) {
    auto preset = call.get_custom_preset();
    auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                           [&preset](const EconetPreset &p) { return p.name == preset; });
    if (it != this->custom_presets_.end()) {
      this->parent_->set_enum_datapoint_value(this->custom_preset_id_, it->id, this->src_adr_);
    }
  }
  if (call.has_custom_fan_mode() && !this->custom_fan_mode_id_.empty()) {
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
  if (call.get_target_humidity().has_value() && !this->target_dehumidification_level_id_.empty()) {
    this->parent_->set_float_datapoint_value(this->target_dehumidification_level_id_,
                                             call.get_target_humidity().value(), this->src_adr_);
  }
}

}  // namespace econet
}  // namespace esphome
