#pragma once

#include <cstdint>

namespace esphome {

class GPIOPin {
 public:
  virtual ~GPIOPin() = default;
  virtual void setup() { this->setup_called = true; }
  virtual void digital_write(bool value) { this->state = value; }

  bool setup_called{false};
  bool state{false};
};

class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
};

}  // namespace esphome
