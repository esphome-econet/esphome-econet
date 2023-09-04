#pragma once

#include "esphome/core/component.h"
#include "../econet.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace econet {

class EconetClimate : public climate::Climate, public PollingComponent, public EconetClient {
 public:
  void update() override;
  void dump_config() override;

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

}  // namespace econet
}  // namespace esphome
