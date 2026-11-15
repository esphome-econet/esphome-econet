#pragma once

#include "esphome/core/helpers.h"

#include <cstddef>
#include <string>
#include <vector>

namespace esphome {
namespace select {

class Select {
 public:
  virtual ~Select() = default;
  void set_options(const std::vector<const char *> &options) { this->options_ = options; }
  void publish_state(size_t index) {
    this->active_index_ = index;
    this->has_state_ = true;
    this->publish_count++;
  }
  optional<size_t> active_index() const { return this->active_index_; }
  const char *option_at(size_t index) const { return index < this->options_.size() ? this->options_[index] : ""; }
  bool has_state() const { return this->has_state_; }
  void select_index(size_t index) { this->control(index); }

  size_t publish_count{0};

 protected:
  virtual void control(size_t index) = 0;

 private:
  std::vector<const char *> options_;
  optional<size_t> active_index_{};
  bool has_state_{false};
};

}  // namespace select
}  // namespace esphome
