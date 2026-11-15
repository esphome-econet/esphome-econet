#pragma once

#include <cstddef>
#include <string>

namespace esphome {
namespace text_sensor {

class TextSensor {
 public:
  virtual ~TextSensor() = default;
  void publish_state(const std::string &value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
  }
  bool has_state() const { return this->has_state_; }

  std::string state{};
  size_t publish_count{0};

 private:
  bool has_state_{false};
};

}  // namespace text_sensor
}  // namespace esphome
