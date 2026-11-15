#pragma once

#include <cstdint>
#include <set>

namespace esphome {
namespace climate {

enum ClimateMode : uint8_t {
  CLIMATE_MODE_OFF = 0,
  CLIMATE_MODE_HEAT_COOL = 1,
  CLIMATE_MODE_COOL = 2,
  CLIMATE_MODE_HEAT = 3,
  CLIMATE_MODE_FAN_ONLY = 4,
  CLIMATE_MODE_DRY = 5,
  CLIMATE_MODE_AUTO = 6,
};

enum ClimateAction : uint8_t {
  CLIMATE_ACTION_OFF = 0,
  CLIMATE_ACTION_COOLING = 2,
  CLIMATE_ACTION_HEATING = 3,
  CLIMATE_ACTION_IDLE = 4,
  CLIMATE_ACTION_DRYING = 5,
  CLIMATE_ACTION_FAN = 6,
};

enum ClimateFeature : uint32_t {
  CLIMATE_SUPPORTS_CURRENT_TEMPERATURE = 1U << 0,
  CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE = 1U << 1,
  CLIMATE_SUPPORTS_ACTION = 1U << 2,
  CLIMATE_SUPPORTS_CURRENT_HUMIDITY = 1U << 3,
  CLIMATE_SUPPORTS_TARGET_HUMIDITY = 1U << 4,
};

inline const char *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return "OFF";
    case CLIMATE_MODE_HEAT_COOL:
      return "HEAT_COOL";
    case CLIMATE_MODE_COOL:
      return "COOL";
    case CLIMATE_MODE_HEAT:
      return "HEAT";
    case CLIMATE_MODE_FAN_ONLY:
      return "FAN_ONLY";
    case CLIMATE_MODE_DRY:
      return "DRY";
    case CLIMATE_MODE_AUTO:
      return "AUTO";
  }
  return "UNKNOWN";
}

inline const char *climate_action_to_string(ClimateAction action) {
  switch (action) {
    case CLIMATE_ACTION_OFF:
      return "OFF";
    case CLIMATE_ACTION_COOLING:
      return "COOLING";
    case CLIMATE_ACTION_HEATING:
      return "HEATING";
    case CLIMATE_ACTION_IDLE:
      return "IDLE";
    case CLIMATE_ACTION_DRYING:
      return "DRYING";
    case CLIMATE_ACTION_FAN:
      return "FAN";
  }
  return "UNKNOWN";
}

class ClimateTraits {
 public:
  void add_feature_flags(uint32_t flags) { this->feature_flags_ |= flags; }
  bool has_feature_flags(uint32_t flags) const { return (this->feature_flags_ & flags) == flags; }
  void add_supported_mode(ClimateMode mode) { this->supported_modes_.insert(mode); }
  bool supports_mode(ClimateMode mode) const { return this->supported_modes_.count(mode) > 0; }

 private:
  uint32_t feature_flags_{0};
  std::set<ClimateMode> supported_modes_;
};

}  // namespace climate
}  // namespace esphome
