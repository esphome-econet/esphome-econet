#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../econet.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace econet {

class EconetClimate : public climate::Climate, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_current_temperature_id(const char *id) { this->current_temperature_id_ = id; }
  void set_target_temperature_id(const char *id) { this->target_temperature_id_ = id; }
  void set_target_temperature_low_id(const char *id) { this->target_temperature_low_id_ = id; }
  void set_target_temperature_high_id(const char *id) { this->target_temperature_high_id_ = id; }
  void set_current_humidity_id(const char *id) { this->current_humidity_id_ = id; }
  void set_target_dehumidification_level_id(const char *id) { this->target_dehumidification_level_id_ = id; }
  void set_mode_id(const char *id) { this->mode_id_ = id; }
  void set_custom_preset_id(const char *id) { this->custom_preset_id_ = id; }
  void set_custom_fan_mode_id(const char *id) { this->custom_fan_mode_id_ = id; }
  void set_custom_fan_mode_no_schedule_id(const char *id) { this->custom_fan_mode_no_schedule_id_ = id; }
  void set_follow_schedule_id(const char *id) { this->follow_schedule_id_ = id; }
  void init_modes(size_t size) { this->modes_.init(size); }
  void add_mode(uint8_t id, climate::ClimateMode mode) { this->modes_.push_back({id, mode}); }
  void init_custom_presets(size_t size) { this->custom_presets_.init(size); }
  void add_custom_preset(uint8_t id, const char *name) { this->custom_presets_.push_back({id, name}); }
  void init_custom_fan_modes(size_t size) { this->custom_fan_modes_.init(size); }
  void add_custom_fan_mode(uint8_t id, const char *name) { this->custom_fan_modes_.push_back({id, name}); }
  void set_action_id(const char *id) { this->action_id_ = id; }
  void init_actions(size_t size) { this->actions_.init(size); }
  void add_action(uint8_t id, climate::ClimateAction action) { this->actions_.push_back({id, action}); }
  void set_default_action(climate::ClimateAction action) { this->default_action_ = action; }

  /// Whether the MCU has reported everything this entity's state is built from -- the mode, and
  /// the preset when the appliance has one -- or there is nothing to wait for. Entities built on
  /// top of this one, the water heater template, use it to hold back their own state until it can
  /// be reported truthfully.
  bool has_reported_state() const { return this->mode_state_known_ && this->custom_preset_state_known_; }

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

  struct EconetClimateAction {
    uint8_t id;
    climate::ClimateAction action;
  };

  void register_float_listener(const char *id, float *member, bool is_temperature);
  void publish_state_if_reported_();
  void register_fan_listener(const char *id, const char **member);
  void update_active_fan_mode_();
  bool uses_no_schedule_fan_mode_(climate::ClimateMode mode) const;
  void set_float_datapoint(const char *id, optional<float> value, bool is_temperature);

  const char *current_temperature_id_{nullptr};
  const char *target_temperature_id_{nullptr};
  const char *target_temperature_low_id_{nullptr};
  const char *target_temperature_high_id_{nullptr};
  const char *mode_id_{nullptr};
  const char *custom_preset_id_{nullptr};
  const char *custom_fan_mode_id_{nullptr};
  const char *custom_fan_mode_no_schedule_id_{nullptr};
  const char *follow_schedule_id_{nullptr};
  const char *current_humidity_id_{nullptr};
  const char *target_dehumidification_level_id_{nullptr};
  const char *action_id_{nullptr};
  const char *fan_mode_{nullptr};
  const char *fan_mode_no_schedule_{nullptr};
  FixedVector<EconetClimateMode> modes_;
  FixedVector<EconetPreset> custom_presets_;
  FixedVector<EconetFanMode> custom_fan_modes_;
  FixedVector<EconetClimateAction> actions_;
  optional<climate::ClimateAction> default_action_;
  optional<bool> follow_schedule_;
  bool mode_state_known_{false};
  bool custom_preset_state_known_{false};
  bool action_state_known_{false};
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

}  // namespace econet
}  // namespace esphome
