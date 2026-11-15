#pragma once

namespace esphome {

struct MockApplication {
  void feed_wdt() {}
};

inline MockApplication App;

}  // namespace esphome
