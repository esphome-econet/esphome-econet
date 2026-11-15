#pragma once

#include "hal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace esphome {

template<typename T> using optional = std::optional<T>;
inline constexpr std::nullopt_t nullopt = std::nullopt;

class StringRef {
 public:
  constexpr StringRef() = default;
  StringRef(const char *s) : ptr_(s != nullptr ? s : ""), len_(s != nullptr ? std::strlen(s) : 0) {}
  StringRef(const std::string &s) : ptr_(s.c_str()), len_(s.size()) {}

  const char *c_str() const { return this->ptr_; }
  size_t size() const { return this->len_; }
  bool empty() const { return this->len_ == 0; }
  std::string str() const { return std::string(this->ptr_, this->len_); }

  friend bool operator==(const StringRef &lhs, const StringRef &rhs) {
    return lhs.len_ == rhs.len_ && std::memcmp(lhs.ptr_, rhs.ptr_, lhs.len_) == 0;
  }
  friend bool operator==(const StringRef &lhs, const char *rhs) { return lhs == StringRef(rhs); }
  friend bool operator==(const char *lhs, const StringRef &rhs) { return StringRef(lhs) == rhs; }
  friend bool operator!=(const StringRef &lhs, const StringRef &rhs) { return !(lhs == rhs); }
  friend bool operator!=(const StringRef &lhs, const char *rhs) { return !(lhs == rhs); }
  friend bool operator!=(const char *lhs, const StringRef &rhs) { return !(lhs == rhs); }

 private:
  const char *ptr_{""};
  size_t len_{0};
};

inline uint16_t crc16(const uint8_t *data, uint16_t len, uint16_t crc = 0, uint16_t reverse_poly = 0xa001,
                      bool refin = false, bool refout = false) {
  (void) refin;
  (void) refout;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ reverse_poly;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

inline std::string format_hex_pretty(const uint8_t *data, size_t len, char sep = '.') {
  if (len == 0) {
    return "";
  }
  std::string out;
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    std::snprintf(buf, sizeof(buf), i + 1 < len ? "%02X%c" : "%02X", data[i], sep);
    out += buf;
  }
  if (len > 4) {
    char len_buf[32];
    std::snprintf(len_buf, sizeof(len_buf), " (%zu)", len);
    out += len_buf;
  }
  return out;
}

inline std::string format_hex_pretty(const std::vector<uint8_t> &v, char sep = '.') {
  return format_hex_pretty(v.data(), v.size(), sep);
}

constexpr size_t format_hex_pretty_size(size_t count) { return count == 0 ? 1 : count * 3 + 16; }

template<size_t N>
inline const char *format_hex_pretty_to(char (&buf)[N], const uint8_t *data, size_t len, char sep = '.') {
  size_t pos = 0;
  for (size_t i = 0; i < len && pos + 4 < N; i++) {
    pos += std::snprintf(buf + pos, N - pos, i + 1 < len ? "%02X%c" : "%02X", data[i], sep);
  }
  if (len == 0 && N > 0) {
    buf[0] = '\0';
  }
  return buf;
}

template<size_t N>
inline const char *format_hex_pretty_to(char (&buf)[N], const std::vector<uint8_t> &v, char sep = '.') {
  return format_hex_pretty_to(buf, v.data(), v.size(), sep);
}

template<typename Sig> class Callback;
template<typename R, typename... Args> class Callback<R(Args...)> {
 public:
  Callback() = default;
  template<typename F> static Callback create(F &&fn) {
    using Fn = std::decay_t<F>;
    static_assert(sizeof(Fn) <= sizeof(void *), "esphome::Callback requires stateless or single-pointer lambda");
    Callback cb;
    if constexpr (std::is_empty_v<Fn>) {
      cb.ctx_ = nullptr;
      cb.fn_ = [](void *, Args... args) -> R {
        Fn f{};
        return f(std::forward<Args>(args)...);
      };
    } else {
      std::memcpy(&cb.ctx_, &fn, sizeof(Fn));
      cb.fn_ = [](void *ctx, Args... args) -> R {
        Fn f = *reinterpret_cast<Fn *>(&ctx);
        return f(std::forward<Args>(args)...);
      };
    }
    return cb;
  }
  R call(Args... args) const { return this->fn_(this->ctx_, std::forward<Args>(args)...); }

 private:
  void *ctx_{nullptr};
  R (*fn_)(void *, Args...){nullptr};
};

template<typename Sig> class CallbackManager;
template<typename... Args> class CallbackManager<void(Args...)> {
 public:
  template<typename F> void add(F &&cb) { this->cbs_.push_back(Callback<void(Args...)>::create(std::forward<F>(cb))); }
  void call(Args... args) {
    for (const auto &cb : this->cbs_) {
      cb.call(args...);
    }
  }

 private:
  std::vector<Callback<void(Args...)>> cbs_;
};

template<typename T, size_t N> class StaticVector {
 public:
  constexpr StaticVector() noexcept {}

  void push_back(const T &v) {
    if (this->count_ < N) {
      this->data_[this->count_++] = v;
    }
  }
  void clear() { this->count_ = 0; }
  void resize(size_t n) { this->count_ = n < N ? n : N; }
  size_t size() const { return this->count_; }
  bool empty() const { return this->count_ == 0; }
  T *data() { return this->data_.data(); }
  const T *data() const { return this->data_.data(); }
  T &operator[](size_t i) { return this->data_[i]; }
  const T &operator[](size_t i) const { return this->data_[i]; }
  T *begin() { return this->data_.data(); }
  T *end() { return this->data_.data() + this->count_; }
  const T *begin() const { return this->data_.data(); }
  const T *end() const { return this->data_.data() + this->count_; }

 private:
  std::array<T, N> data_;
  size_t count_{0};
};

template<typename T> class FixedVector {
 public:
  void init(size_t n) { this->vec_.reserve(n); }
  void push_back(const T &v) { this->vec_.push_back(v); }
  size_t size() const { return this->vec_.size(); }
  bool empty() const { return this->vec_.empty(); }
  T &operator[](size_t i) { return this->vec_[i]; }
  const T &operator[](size_t i) const { return this->vec_[i]; }
  auto begin() { return this->vec_.begin(); }
  auto end() { return this->vec_.end(); }
  auto begin() const { return this->vec_.begin(); }
  auto end() const { return this->vec_.end(); }

 private:
  std::vector<T> vec_;
};

}  // namespace esphome
