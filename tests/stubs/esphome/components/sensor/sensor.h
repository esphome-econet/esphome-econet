#pragma once

#include <cmath>
#include <cstddef>

namespace esphome {
namespace sensor {

class Sensor {
 public:
  virtual ~Sensor() = default;
  void publish_state(float value) {
    this->state = value;
    this->has_state_ = true;
    this->publish_count++;
  }
  bool has_state() const { return this->has_state_; }
  bool get_force_update() const { return this->force_update_; }
  void set_force_update(bool force_update) { this->force_update_ = force_update; }

  float state{NAN};
  size_t publish_count{0};

 private:
  bool has_state_{false};
  bool force_update_{false};
};

}  // namespace sensor
}  // namespace esphome
