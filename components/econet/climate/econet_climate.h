#pragma once

#include <map>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "../econet.h"

namespace esphome {
namespace econet {

// clang-format off
static const climate::ClimateMode OpModes[] = {
    climate::CLIMATE_MODE_OFF,  // Unused
    climate::CLIMATE_MODE_HEAT_COOL,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_OFF,  // Unused
    climate::CLIMATE_MODE_FAN_ONLY
};
// clang-format on

class EconetClimate : public climate::Climate,
                         public PollingComponent,
                         public EconetClient {
 public:
  void update() override;
  void dump_config() override;
  void control(const climate::ClimateCall &call) override;

  climate::ClimateAction econet_climate_action();

  climate::ClimateMode econet_climate_mode(EconetClimateMode mode);
  EconetClimateMode econet_climate_mode(climate::ClimateMode mode);
  const std::string econet2e_fan_mode(EconetFanMode mode);
  EconetFanMode e2econet_fan_mode(std::string mode);
  climate::ClimateSwingMode econet2e_swing_mode(bool swing_v, bool swing_h);
  bool econet2d_swing_v(climate::ClimateSwingMode mode);
  bool econet2d_swing_h(climate::ClimateSwingMode mode);

 protected:
  climate::ClimateTraits traits() override;
};

}  // namespace daikin_s21
}  // namespace esphome
