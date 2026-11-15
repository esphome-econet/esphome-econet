#pragma once

#include <cstddef>

namespace esphome {
namespace switch_ {

class Switch {
 public:
  virtual ~Switch() = default;
  void publish_state(bool value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
  }
  void turn_on() { this->write_state(true); }
  void turn_off() { this->write_state(false); }
  bool has_state() const { return this->has_state_; }

  bool state{false};
  size_t publish_count{0};

 protected:
  virtual void write_state(bool state) = 0;

 private:
  bool has_state_{false};
};

}  // namespace switch_
}  // namespace esphome
