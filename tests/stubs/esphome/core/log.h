#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace test_log {
inline bool capture_enabled = false;
inline std::vector<std::string> warnings;
inline std::vector<std::string> infos;

[[gnu::format(printf, 2, 3)]] inline void record(std::vector<std::string> &out, const char *fmt, ...) {
  if (!capture_enabled) {
    return;
  }
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0) {
    out.emplace_back(buf);
  }
}

inline void reset() {
  warnings.clear();
  infos.clear();
}
}  // namespace test_log
}  // namespace esphome

#define ESPHOME_LOG_LEVEL_VERBOSE 6
#define ESPHOME_LOG_LEVEL 6
#define ESPHOME_LOG_TAG(name, str) static constexpr const char *name = str

#define LOG_STR_ARG(s) (s)
#define ONOFF(b) ((b) ? "ON" : "OFF")

#define ESP_LOGV(tag, fmt, ...) ((void) (tag), (void) sizeof(std::snprintf(nullptr, 0, fmt, ##__VA_ARGS__)))
#define ESP_LOGD(tag, fmt, ...) ((void) (tag), (void) sizeof(std::snprintf(nullptr, 0, fmt, ##__VA_ARGS__)))
#define ESP_LOGI(tag, fmt, ...) \
  ((void) (tag), ::esphome::test_log::record(::esphome::test_log::infos, fmt, ##__VA_ARGS__))
#define ESP_LOGW(tag, fmt, ...) \
  ((void) (tag), ::esphome::test_log::record(::esphome::test_log::warnings, fmt, ##__VA_ARGS__))
#define ESP_LOGE(tag, fmt, ...) ((void) (tag), (void) sizeof(std::snprintf(nullptr, 0, fmt, ##__VA_ARGS__)))
#define ESP_LOGCONFIG(tag, fmt, ...) ((void) (tag), (void) sizeof(std::snprintf(nullptr, 0, fmt, ##__VA_ARGS__)))

#define LOG_PIN(prefix, pin) ((void) (prefix), (void) (pin))
#define LOG_BINARY_SENSOR(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_SENSOR(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_TEXT_SENSOR(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_SWITCH(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_SELECT(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_NUMBER(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
#define LOG_CLIMATE(prefix, type, obj) ((void) (prefix), (void) (type), (void) (obj))
