#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace esphome {
namespace uart {

class MockUART {
 public:
  std::deque<uint8_t> rx_buf;
  std::vector<std::vector<uint8_t>> tx_frames;
  std::function<void(const std::vector<uint8_t> &)> on_tx;
  bool echo_tx_to_rx{false};

  void feed_rx(const std::vector<uint8_t> &bytes) {
    for (uint8_t b : bytes) {
      this->rx_buf.push_back(b);
    }
  }
};

class UARTDevice {
 public:
  virtual ~UARTDevice() = default;
  void set_uart_parent(MockUART *parent) { this->parent_ = parent; }
  int available() { return this->parent_ != nullptr ? static_cast<int>(this->parent_->rx_buf.size()) : 0; }
  bool read_array(uint8_t *out, size_t n) {
    if (this->parent_ == nullptr || this->parent_->rx_buf.size() < n) {
      return false;
    }
    for (size_t i = 0; i < n; i++) {
      out[i] = this->parent_->rx_buf.front();
      this->parent_->rx_buf.pop_front();
    }
    return true;
  }
  void write_array(const uint8_t *data, size_t n) {
    if (this->parent_ == nullptr) {
      return;
    }
    std::vector<uint8_t> frame(data, data + n);
    this->parent_->tx_frames.push_back(frame);
    if (this->parent_->echo_tx_to_rx) {
      this->parent_->feed_rx(frame);
    }
    if (this->parent_->on_tx) {
      this->parent_->on_tx(frame);
    }
  }
  void flush() {}

 protected:
  MockUART *parent_{nullptr};
};

}  // namespace uart
}  // namespace esphome
