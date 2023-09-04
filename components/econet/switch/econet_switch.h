#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetSwitch : public switch_::Switch, public PollingComponent, public EconetClient {
 public:
  void update() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

 protected:
  void write_state(bool state) override;

  uint8_t switch_id_{0};
};

}  // namespace econet
}  // namespace esphome
