#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace empty_switch {

class EmptySwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;
};

} //namespace empty_switch
} //namespace esphome