#pragma once

#include <cstddef>

namespace esphome {
namespace binary_sensor {

class BinarySensor {
 public:
  virtual ~BinarySensor() = default;
  void publish_initial_state(bool value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
  }
  void publish_state(bool value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
  }
  bool has_state() const { return this->has_state_; }

  bool state{false};
  size_t publish_count{0};

 private:
  bool has_state_{false};
};

}  // namespace binary_sensor
}  // namespace esphome
