#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace esphome {
namespace text_sensor {

class TextSensor {
 public:
  virtual ~TextSensor() = default;
  void publish_state(const std::string &value) {
    this->raw_state_ = value;
    this->state = this->filter_fn ? this->filter_fn(value) : value;
    this->has_state_ = true;
    this->publish_count++;
  }
  bool has_state() const { return this->has_state_; }
  const std::string &get_raw_state() const { return this->raw_state_; }

  std::string state{};
  std::function<std::string(const std::string &)> filter_fn{};
  size_t publish_count{0};

 private:
  std::string raw_state_{};
  bool has_state_{false};
};

}  // namespace text_sensor
}  // namespace esphome
