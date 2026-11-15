#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace esphome {
namespace number {

class Number {
 public:
  virtual ~Number() = default;
  void publish_state(float value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
    for (auto &cb : this->state_callbacks_) {
      cb(value);
    }
  }
  void add_on_state_callback(std::function<void(float)> &&cb) { this->state_callbacks_.push_back(std::move(cb)); }
  void set(float value) { this->control(value); }
  bool has_state() const { return this->has_state_; }

  float state{NAN};
  size_t publish_count{0};

 protected:
  virtual void control(float value) = 0;

 private:
  std::vector<std::function<void(float)>> state_callbacks_;
  bool has_state_{false};
};

}  // namespace number
}  // namespace esphome
