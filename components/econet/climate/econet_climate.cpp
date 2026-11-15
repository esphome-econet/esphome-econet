#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_traits.h"
#include "econet_climate.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace esphome;

namespace esphome {
namespace econet {

ESPHOME_LOG_TAG(TAG, "econet.climate");

namespace {

inline float fahrenheit_to_celsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
inline float celsius_to_fahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }

void log_datapoint_if_set(const char *label, const char *id) {
  if (id != nullptr && *id != '\0') {
    ESP_LOGCONFIG(TAG, "  %s Datapoint: %s", label, id);
  }
}

}  // namespace

void EconetClimate::dump_config() {
  LOG_CLIMATE("", "Econet Climate", this);
  this->dump_traits_(TAG);
  log_datapoint_if_set("Current Temperature", this->current_temperature_id_);
  log_datapoint_if_set("Target Temperature", this->target_temperature_id_);
  log_datapoint_if_set("Target Temperature Low", this->target_temperature_low_id_);
  log_datapoint_if_set("Target Temperature High", this->target_temperature_high_id_);
  log_datapoint_if_set("Current Humidity", this->current_humidity_id_);
  log_datapoint_if_set("Target Dehumidification Level", this->target_dehumidification_level_id_);
  log_datapoint_if_set("Mode", this->mode_id_);
  log_datapoint_if_set("Custom Preset", this->custom_preset_id_);
  log_datapoint_if_set("Custom Fan Mode", this->custom_fan_mode_id_);
  log_datapoint_if_set("Custom Fan Mode (No Schedule)", this->custom_fan_mode_no_schedule_id_);
  log_datapoint_if_set("Follow Schedule", this->follow_schedule_id_);
  if (this->action_id_ && *this->action_id_) {
    ESP_LOGCONFIG(TAG, "  Action datapoint: %s", this->action_id_);
    for (const auto &entry : this->actions_) {
      ESP_LOGCONFIG(TAG, "    %d: %s", entry.id, LOG_STR_ARG(climate::climate_action_to_string(entry.action)));
    }
    if (this->default_action_.has_value()) {
      ESP_LOGCONFIG(TAG, "    Any other value: %s",
                    LOG_STR_ARG(climate::climate_action_to_string(*this->default_action_)));
    }
  }
  this->dump_client_config_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
  climate::ClimateTraits traits;
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
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
  }
  if (this->action_id_ && *this->action_id_) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  }
  if (this->mode_id_ && *this->mode_id_) {
    for (const auto &entry : this->modes_) {
      traits.add_supported_mode(entry.mode);
    }
  }
  return traits;
}

// A climate state message carries every value at once, so publishing before the MCU has reported
// the mode would send the default-initialized CLIMATE_MODE_OFF as if the appliance were really off,
// and publishing before it has reported the preset would leave consumers of the preset guessing.
// The same goes for the action, which would otherwise go out as whatever it default-initializes to.
// Hold every publish until all of them have been reported; the entity reads as unknown until then.
// The action is not part of has_reported_state(): the water heater template has no action to wait for.
void EconetClimate::publish_state_if_reported_() {
  if (!this->has_reported_state() || !this->action_state_known_) {
    return;
  }
  if (this->in_control_ || this->parent_->is_in_batch_response()) {
    this->publish_pending_ = true;
    return;
  }
  this->publish_pending_ = false;
  this->publish_state();
}

void EconetClimate::flush_pending_publish_() {
  if (!this->publish_pending_ || this->in_control_ || this->parent_->is_in_batch_response()) {
    return;
  }
  this->publish_pending_ = false;
  if (this->has_reported_state() && this->action_state_known_) {
    this->publish_state();
  }
}

void EconetClimate::handle_float_datapoint_(const char *id, const EconetDatapoint &datapoint, float *member,
                                            bool is_temperature) {
  if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
    return;
  }
  if (datapoint.type != EconetDatapointType::FLOAT) {
    ESP_LOGW(TAG, "Unexpected datapoint type %u for climate float %s", static_cast<uint8_t>(datapoint.type), id);
    return;
  }
  float val = is_temperature ? fahrenheit_to_celsius(datapoint.value_float) : datapoint.value_float;
  if (*member == val || (std::isnan(*member) && std::isnan(val))) {
    return;
  }
  *member = val;
  this->publish_state_if_reported_();
}

void EconetClimate::handle_fan_datapoint_(const char *id, const EconetDatapoint &datapoint, const char **member) {
  if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
    return;
  }
  if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
    ESP_LOGW(TAG, "Unexpected datapoint type %u for climate fan mode %s", static_cast<uint8_t>(datapoint.type), id);
    return;
  }
  auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                         [&](const EconetFanMode &m) { return m.id == datapoint.value_enum; });
  const char *matched = nullptr;
  if (it == this->custom_fan_modes_.end()) {
    ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
             datapoint.value_string.c_str());
  } else {
    matched = it->name;
  }
  *member = matched;
  if (this->update_active_fan_mode_()) {
    this->publish_state_if_reported_();
  }
}

// Which of the two fan speed datapoints is in charge: the no-schedule one whenever the schedule
// isn't being followed -- see #177 -- and, per the readings in #646, also whenever the system is
// in fan only mode, where the regular datapoint stays frozen at whatever was last written to it
// and only the no-schedule one tracks the blower.
bool EconetClimate::uses_no_schedule_fan_mode_(climate::ClimateMode mode) const {
  return !*this->follow_schedule_ || mode == climate::CLIMATE_MODE_FAN_ONLY;
}

// Reflect whichever of the two is in charge, falling back to the regular one while the schedule is
// being followed so that a config without a no-schedule datapoint keeps reporting what it used to.
bool EconetClimate::update_active_fan_mode_() {
  if (!this->follow_schedule_.has_value()) {
    return false;
  }
  const bool has_no_schedule_dp =
      this->custom_fan_mode_no_schedule_id_ != nullptr && *this->custom_fan_mode_no_schedule_id_ != '\0';
  const char *active = nullptr;
  if (this->uses_no_schedule_fan_mode_(this->mode) && has_no_schedule_dp) {
    active = this->fan_mode_no_schedule_;
  } else if (*this->follow_schedule_) {
    active = this->fan_mode_;
  }
  if (active == nullptr) {
    if (!this->has_custom_fan_mode()) {
      return false;
    }
    this->clear_custom_fan_mode_();
    return true;
  }
  return this->set_custom_fan_mode_(active);
}

void EconetClimate::setup() {
  if (this->custom_preset_id_ && *this->custom_preset_id_) {
    std::vector<const char *> presets;
    presets.reserve(this->custom_presets_.size());
    for (const auto &entry : this->custom_presets_) {
      presets.push_back(entry.name);
    }
    this->set_supported_custom_presets(presets);
  }
  if ((this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) ||
      (this->custom_fan_mode_no_schedule_id_ && *this->custom_fan_mode_no_schedule_id_)) {
    std::vector<const char *> fans;
    fans.reserve(this->custom_fan_modes_.size());
    for (const auto &entry : this->custom_fan_modes_) {
      fans.push_back(entry.name);
    }
    this->set_supported_custom_fan_modes(fans);
  }

  this->parent_->add_on_batch_complete_callback([this]() { this->flush_pending_publish_(); });

  if (this->current_temperature_id_ && *this->current_temperature_id_) {
    this->parent_->register_listener(
        this->current_temperature_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->current_temperature_id_, dp, &this->current_temperature, true);
        },
        false, this->src_adr_);
  }
  if (this->target_temperature_id_ && *this->target_temperature_id_) {
    this->parent_->register_listener(
        this->target_temperature_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->target_temperature_id_, dp, &this->target_temperature, true);
        },
        false, this->src_adr_);
  }
  if (this->target_temperature_low_id_ && *this->target_temperature_low_id_) {
    this->parent_->register_listener(
        this->target_temperature_low_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->target_temperature_low_id_, dp, &this->target_temperature_low, true);
        },
        false, this->src_adr_);
  }
  if (this->target_temperature_high_id_ && *this->target_temperature_high_id_) {
    this->parent_->register_listener(
        this->target_temperature_high_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->target_temperature_high_id_, dp, &this->target_temperature_high, true);
        },
        false, this->src_adr_);
  }
  if (this->current_humidity_id_ && *this->current_humidity_id_) {
    this->parent_->register_listener(
        this->current_humidity_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->current_humidity_id_, dp, &this->current_humidity, false);
        },
        false, this->src_adr_);
  }
  if (this->target_dehumidification_level_id_ && *this->target_dehumidification_level_id_) {
    this->parent_->register_listener(
        this->target_dehumidification_level_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_float_datapoint_(this->target_dehumidification_level_id_, dp, &this->target_humidity, false);
        },
        false, this->src_adr_);
  }

  if (this->mode_id_ && *this->mode_id_) {
    this->parent_->register_listener(
        this->mode_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
            return;
          }
          if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
            ESP_LOGW(TAG, "Unexpected datapoint type %u for climate mode %s", static_cast<uint8_t>(datapoint.type),
                     this->mode_id_);
            return;
          }
          auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                                 [&](const EconetClimateMode &m) { return m.id == datapoint.value_enum; });
          if (it == this->modes_.end()) {
            ESP_LOGW(TAG, "In modes of your yaml add a ClimateMode that corresponds to: %d: \"%s\"",
                     datapoint.value_enum, datapoint.value_string.c_str());
          } else {
            bool changed = !this->mode_state_known_ || this->mode != it->mode;
            this->mode = it->mode;
            this->mode_state_known_ = true;
            // Which fan speed datapoint is in charge depends on the mode.
            changed |= this->update_active_fan_mode_();
            if (changed) {
              this->publish_state_if_reported_();
            }
          }
        },
        false, this->src_adr_);
  } else {
    // Nothing to wait for: an appliance without a mode datapoint has no mode to report.
    this->mode_state_known_ = true;
  }
  if (this->custom_preset_id_ && *this->custom_preset_id_) {
    this->parent_->register_listener(
        this->custom_preset_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
            return;
          }
          if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
            ESP_LOGW(TAG, "Unexpected datapoint type %u for climate preset %s", static_cast<uint8_t>(datapoint.type),
                     this->custom_preset_id_);
            return;
          }
          auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                                 [&](const EconetPreset &p) { return p.id == datapoint.value_enum; });
          bool changed = !this->custom_preset_state_known_;
          if (it == this->custom_presets_.end()) {
            ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
            if (this->has_custom_preset()) {
              this->clear_custom_preset_();
              changed = true;
            }
          } else if (this->set_custom_preset_(it->name)) {
            changed = true;
          }
          this->custom_preset_state_known_ = true;
          if (changed) {
            this->publish_state_if_reported_();
          }
        },
        false, this->src_adr_);
  } else {
    // Nothing to wait for: an appliance without a preset datapoint has no preset to report.
    this->custom_preset_state_known_ = true;
  }

  if (this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) {
    this->parent_->register_listener(
        this->custom_fan_mode_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_fan_datapoint_(this->custom_fan_mode_id_, dp, &this->fan_mode_);
        },
        false, this->src_adr_);
  }
  if (this->custom_fan_mode_no_schedule_id_ && *this->custom_fan_mode_no_schedule_id_) {
    this->parent_->register_listener(
        this->custom_fan_mode_no_schedule_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &dp) {
          this->handle_fan_datapoint_(this->custom_fan_mode_no_schedule_id_, dp, &this->fan_mode_no_schedule_);
        },
        false, this->src_adr_);
  }

  if (this->action_id_ && *this->action_id_) {
    this->parent_->register_listener(
        this->action_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
            return;
          }
          if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
            ESP_LOGW(TAG, "Unexpected datapoint type %u for climate action %s", static_cast<uint8_t>(datapoint.type),
                     this->action_id_);
            return;
          }
          climate::ClimateAction next_action;
          auto it = std::find_if(this->actions_.begin(), this->actions_.end(),
                                 [&](const EconetClimateAction &a) { return a.id == datapoint.value_enum; });
          if (it != this->actions_.end()) {
            next_action = it->action;
          } else if (this->default_action_.has_value()) {
            next_action = *this->default_action_;
          } else {
            // Like an unmapped mode: keep the last action, and hold the first publish until a mapped value.
            ESP_LOGW(TAG, "In actions of your yaml add a ClimateAction that corresponds to: %d: \"%s\"",
                     datapoint.value_enum, datapoint.value_string.c_str());
            return;
          }
          bool changed = !this->action_state_known_ || this->action != next_action;
          this->action = next_action;
          this->action_state_known_ = true;
          if (changed) {
            this->publish_state_if_reported_();
          }
        },
        false, this->src_adr_);
  } else {
    // Nothing to wait for: an appliance without an action datapoint has no action to report.
    this->action_state_known_ = true;
  }

  if (this->follow_schedule_id_ && *this->follow_schedule_id_) {
    this->parent_->register_listener(
        this->follow_schedule_id_, this->request_mod_, this->request_once_,
        [this](const EconetDatapoint &datapoint) {
          if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
            return;
          }
          if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
            ESP_LOGW(TAG, "Unexpected datapoint type %u for climate follow_schedule %s",
                     static_cast<uint8_t>(datapoint.type), this->follow_schedule_id_);
            return;
          }
          ESP_LOGV(TAG, "MCU reported climate sensor %s is: %s", this->follow_schedule_id_,
                   datapoint.value_string.c_str());
          this->follow_schedule_ = datapoint.value_enum > 0;
          if (this->update_active_fan_mode_()) {
            this->publish_state_if_reported_();
          }
        },
        false, this->src_adr_);
  } else {
    // Without a follow_schedule datapoint, treat the primary custom_fan_mode_datapoint as active.
    this->follow_schedule_ = true;
  }
}

void EconetClimate::set_float_datapoint(const char *id, optional<float> value, bool is_temperature) {
  if (value.has_value() && std::isfinite(*value) && id && *id) {
    float val = *value;
    if (is_temperature) {
      val = celsius_to_fahrenheit(val);
    }
    this->parent_->set_float_datapoint_value(id, val, this->src_adr_);
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  this->in_control_ = true;

  this->set_float_datapoint(this->target_temperature_id_, call.get_target_temperature(), true);
  this->set_float_datapoint(this->target_temperature_low_id_, call.get_target_temperature_low(), true);
  this->set_float_datapoint(this->target_temperature_high_id_, call.get_target_temperature_high(), true);
  this->set_float_datapoint(this->target_dehumidification_level_id_, call.get_target_humidity(), false);

  if (call.get_mode().has_value() && this->mode_id_ && *this->mode_id_) {
    // Callers that send the mode alongside another field rebuild it from this entity's state, so
    // before the MCU has reported one they send whatever climate default-initializes to, which is
    // CLIMATE_MODE_OFF. The water heater template does exactly that on a target temperature
    // change, so accepting the write would turn the appliance off on a temperature change that
    // lands in the window after a restart. Drop it and keep the rest of the call.
    if (!this->mode_state_known_) {
      ESP_LOGW(TAG, "Not writing mode %s before the MCU has reported one",
               LOG_STR_ARG(climate::climate_mode_to_string(call.get_mode().value())));
    } else {
      climate::ClimateMode mode = call.get_mode().value();
      auto it = std::find_if(this->modes_.begin(), this->modes_.end(),
                             [&mode](const EconetClimateMode &m) { return m.mode == mode; });
      if (it != this->modes_.end()) {
        this->parent_->set_enum_datapoint_value(this->mode_id_, it->id, this->src_adr_);
      }
    }
  }
  if (call.has_custom_preset() && this->custom_preset_id_ && *this->custom_preset_id_) {
    // Same reasoning as the mode above: a caller that sends the preset alongside another field
    // rebuilds it from this entity's state, or from what it last saw, so before the first report
    // it writes a preset the appliance never told us about.
    if (!this->custom_preset_state_known_) {
      ESP_LOGW(TAG, "Not writing preset %s before the MCU has reported one", call.get_custom_preset().c_str());
    } else {
      auto preset = call.get_custom_preset();
      auto it = std::find_if(this->custom_presets_.begin(), this->custom_presets_.end(),
                             [&preset](const EconetPreset &p) { return p.name == preset; });
      if (it != this->custom_presets_.end()) {
        this->parent_->set_enum_datapoint_value(this->custom_preset_id_, it->id, this->src_adr_);
      }
    }
  }
  if (call.has_custom_fan_mode() &&
      ((this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) ||
       (this->custom_fan_mode_no_schedule_id_ && *this->custom_fan_mode_no_schedule_id_))) {
    auto fan_mode = call.get_custom_fan_mode();
    auto it = std::find_if(this->custom_fan_modes_.begin(), this->custom_fan_modes_.end(),
                           [&fan_mode](const EconetFanMode &m) { return m.name == fan_mode; });
    if (it != this->custom_fan_modes_.end() && this->follow_schedule_.has_value()) {
      // A mode change accepted earlier in this call has already updated this->mode via send_datapoint_().
      const climate::ClimateMode mode = this->mode_state_known_ ? call.get_mode().value_or(this->mode) : this->mode;
      const bool no_schedule = this->uses_no_schedule_fan_mode_(mode);
      if (no_schedule && this->custom_fan_mode_no_schedule_id_ && *this->custom_fan_mode_no_schedule_id_) {
        this->parent_->set_enum_datapoint_value(this->custom_fan_mode_no_schedule_id_, it->id, this->src_adr_);
      } else if (*this->follow_schedule_ && this->custom_fan_mode_id_ && *this->custom_fan_mode_id_) {
        // Either the schedule is being followed outside of fan only mode, or there is no
        // no-schedule datapoint to write and this is the only thing left to try.
        this->parent_->set_enum_datapoint_value(this->custom_fan_mode_id_, it->id, this->src_adr_);
      } else {
        ESP_LOGW(TAG, "Not following the schedule but custom_fan_mode_no_schedule_datapoint is not configured");
      }
    }
  }

  this->in_control_ = false;
  this->flush_pending_publish_();
}

}  // namespace econet
}  // namespace esphome
