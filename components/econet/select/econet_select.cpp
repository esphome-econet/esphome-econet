#include "esphome/core/log.h"
#include "econet_select.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.select";

void EconetSelect::setup() {
  this->parent_->register_listener(
      this->select_id_, this->request_mod_, this->request_once_,
      [this](const EconetDatapoint &datapoint) {
        uint8_t enum_value = datapoint.value_enum;
        ESP_LOGV(TAG, "MCU reported select %s value %u", this->select_id_, enum_value);
        const auto &mappings = this->mappings_;
        auto it = std::find(mappings.begin(), mappings.end(), enum_value);
        if (it == mappings.end()) {
          ESP_LOGW(TAG, "Invalid value %u", enum_value);
          return;
        }
        size_t mapping_idx = std::distance(mappings.begin(), it);
        auto value = this->at(mapping_idx);
        this->publish_state(value.value());
      },
      false, this->src_adr_);
}

void EconetSelect::control(const std::string &value) {
  auto idx = this->index_of(value);
  if (idx.has_value()) {
    uint8_t mapping = this->mappings_[idx.value()];
    ESP_LOGV(TAG, "Setting %s datapoint value to %u:%s", this->select_id_, mapping, value.c_str());
    this->parent_->set_enum_datapoint_value(this->select_id_, mapping, this->src_adr_);
    return;
  }
  ESP_LOGW(TAG, "Invalid value %s", value.c_str());
}

void EconetSelect::dump_config() {
  LOG_SELECT("", "Econet Select", this);
  ESP_LOGCONFIG(TAG, "  Select has datapoint ID %s", this->select_id_);
  ESP_LOGCONFIG(TAG, "  Options are:");
  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < this->mappings_.size(); i++) {
    ESP_LOGCONFIG(TAG, "    %i: %s", this->mappings_[i], options.at(i));
  }
}

}  // namespace econet
}  // namespace esphome
