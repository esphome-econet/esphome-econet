#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace esphome {

template<typename... Ts> class Trigger {
 public:
  virtual ~Trigger() = default;
  void add_on_trigger_callback(std::function<void(Ts...)> &&cb) { this->callbacks_.push_back(std::move(cb)); }
  void trigger(const Ts &...args) {
    for (auto &cb : this->callbacks_) {
      cb(args...);
    }
  }

 private:
  std::vector<std::function<void(Ts...)>> callbacks_;
};

}  // namespace esphome
