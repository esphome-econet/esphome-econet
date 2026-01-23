#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_traits.h"
#include "econet_climate.h"

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
  dump_traits_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
  // Initialize pointers if not done yet
  init_preset_ptrs_();

  auto traits = climate::ClimateTraits();
  if (!current_temperature_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  if (!current_humidity_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  }
  if (!target_dehumidification_level_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  }
  if (!target_temperature_high_id_.empty()) {
    traits.add_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  }
  if (!mode_id_.empty()) {
    for (const auto &kv : modes_) {
      traits.add_supported_mode(kv.second);
    }
  }
  if (!custom_preset_id_.empty() && !custom_preset_ptrs_.empty()) {
    traits.set_supported_custom_presets(custom_preset_ptrs_);
  }
  if (!custom_fan_mode_id_.empty() && !custom_fan_mode_ptrs_.empty()) {
    traits.set_supported_custom_fan_modes(custom_fan_mode_ptrs_);
  }
  return traits;
}

void EconetClimate::init_preset_ptrs_() {
  if (ptrs_initialized_) {
    return;
  }
  ptrs_initialized_ = true;

  // Build pointer vectors and key-to-pointer maps.
  // std::map strings are stable (don't move once inserted).
  // ESPHome 2026.1.0+ requires the exact same pointer for set_custom_preset_()
  // as was registered in traits(), so we store and reuse these pointers.
  for (const auto &kv : custom_presets_) {
    const char *ptr = kv.second.c_str();
    custom_preset_ptrs_.push_back(ptr);
    custom_preset_ptr_by_key_[kv.first] = ptr;
  }
  for (const auto &kv : custom_fan_modes_) {
    const char *ptr = kv.second.c_str();
    custom_fan_mode_ptrs_.push_back(ptr);
    custom_fan_mode_ptr_by_key_[kv.first] = ptr;
  }
}

void EconetClimate::setup() {
  // Initialize pointers before registering listeners
  init_preset_ptrs_();

  if (!current_temperature_id_.empty()) {
    parent_->register_listener(
        current_temperature_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          current_temperature = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!current_humidity_id_.empty()) {
    parent_->register_listener(
        current_humidity_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          current_humidity = datapoint.value_float;
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_dehumidification_level_id_.empty()) {
    parent_->register_listener(
        target_dehumidification_level_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_humidity = datapoint.value_float;
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_id_.empty()) {
    parent_->register_listener(
        target_temperature_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_low_id_.empty()) {
    parent_->register_listener(
        target_temperature_low_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature_low = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_high_id_.empty()) {
    parent_->register_listener(
        target_temperature_high_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature_high = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!mode_id_.empty()) {
    parent_->register_listener(
        mode_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = modes_.find(datapoint.value_enum);
          if (it == modes_.end()) {
            ESP_LOGW(TAG, "In modes of your yaml add a ClimateMode that corresponds to: %d: \"%s\"",
                     datapoint.value_enum, datapoint.value_string.c_str());
          } else {
            mode = it->second;
            publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (!custom_preset_id_.empty()) {
    parent_->register_listener(
        custom_preset_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto ptr_it = custom_preset_ptr_by_key_.find(datapoint.value_enum);
          if (ptr_it == custom_preset_ptr_by_key_.end()) {
            ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            // Use the exact same pointer that was registered in traits
            set_custom_preset_(ptr_it->second);
            publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (!custom_fan_mode_id_.empty()) {
    parent_->register_listener(
        custom_fan_mode_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto ptr_it = custom_fan_mode_ptr_by_key_.find(datapoint.value_enum);
          if (ptr_it == custom_fan_mode_ptr_by_key_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            fan_mode_ = ptr_it->second;  // Store as string for follow_schedule logic
            if (follow_schedule_.has_value() && follow_schedule_.value()) {
              set_custom_fan_mode_(ptr_it->second);
              publish_state();
            }
          }
        },
        false, this->src_adr_);
  }
  if (!custom_fan_mode_no_schedule_id_.empty()) {
    parent_->register_listener(
        custom_fan_mode_no_schedule_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto ptr_it = custom_fan_mode_ptr_by_key_.find(datapoint.value_enum);
          if (ptr_it == custom_fan_mode_ptr_by_key_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            fan_mode_no_schedule_ = ptr_it->second;  // Store as string for follow_schedule logic
            if (follow_schedule_.has_value() && !follow_schedule_.value()) {
              set_custom_fan_mode_(ptr_it->second);
              publish_state();
            }
          }
        },
        false, this->src_adr_);
  }
  if (!follow_schedule_id_.empty()) {
    parent_->register_listener(
        follow_schedule_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          ESP_LOGD(TAG, "MCU reported climate sensor %s is: %s", this->follow_schedule_id_.c_str(),
                   datapoint.value_string.c_str());
          follow_schedule_ = datapoint.value_enum > 0;
          const std::string &mode_to_use = follow_schedule_.value() ? fan_mode_ : fan_mode_no_schedule_;
          if (!mode_to_use.empty()) {
            // Need to find the pointer for this mode string
            for (const auto &kv : custom_fan_mode_ptr_by_key_) {
              if (mode_to_use == kv.second) {
                set_custom_fan_mode_(kv.second);
                publish_state();
                break;
              }
            }
          }
        },
        false, this->src_adr_);
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value() && !target_temperature_id_.empty()) {
    parent_->set_float_datapoint_value(target_temperature_id_,
                                       celsius_to_fahrenheit(call.get_target_temperature().value()), this->src_adr_);
  }
  if (call.get_target_temperature_low().has_value() && !target_temperature_low_id_.empty()) {
    parent_->set_float_datapoint_value(
        target_temperature_low_id_, celsius_to_fahrenheit(call.get_target_temperature_low().value()), this->src_adr_);
  }
  if (call.get_target_temperature_high().has_value() && !target_temperature_high_id_.empty()) {
    parent_->set_float_datapoint_value(
        target_temperature_high_id_, celsius_to_fahrenheit(call.get_target_temperature_high().value()), this->src_adr_);
  }
  if (call.get_mode().has_value() && !mode_id_.empty()) {
    climate::ClimateMode mode = call.get_mode().value();
    auto it = std::find_if(modes_.begin(), modes_.end(),
                           [&mode](const std::pair<uint8_t, climate::ClimateMode> &p) { return p.second == mode; });
    if (it != modes_.end()) {
      parent_->set_enum_datapoint_value(mode_id_, it->first, this->src_adr_);
    }
  }
  if (call.has_custom_preset() && !custom_preset_id_.empty()) {
    const std::string preset(call.get_custom_preset().c_str(), call.get_custom_preset().size());
    auto it = std::find_if(custom_presets_.begin(), custom_presets_.end(),
                           [&preset](const std::pair<uint8_t, std::string> &p) { return p.second == preset; });
    if (it != custom_presets_.end()) {
      parent_->set_enum_datapoint_value(custom_preset_id_, it->first, this->src_adr_);
    }
  }
  if (call.has_custom_fan_mode() && !custom_fan_mode_id_.empty()) {
    const std::string fan_mode(call.get_custom_fan_mode().c_str(), call.get_custom_fan_mode().size());
    auto it = std::find_if(custom_fan_modes_.begin(), custom_fan_modes_.end(),
                           [&fan_mode](const std::pair<uint8_t, std::string> &p) { return p.second == fan_mode; });
    if (it != custom_fan_modes_.end()) {
      if (follow_schedule_.has_value()) {
        if (follow_schedule_.value()) {
          parent_->set_enum_datapoint_value(custom_fan_mode_id_, it->first, this->src_adr_);
        } else {
          parent_->set_enum_datapoint_value(custom_fan_mode_no_schedule_id_, it->first, this->src_adr_);
        }
      }
    }
  }
  if (call.get_target_humidity().has_value() && !target_dehumidification_level_id_.empty()) {
    parent_->set_float_datapoint_value(target_dehumidification_level_id_, call.get_target_humidity().value(),
                                       this->src_adr_);
  }
}

}  // namespace econet
}  // namespace esphome
