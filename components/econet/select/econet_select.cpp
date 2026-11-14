#include "esphome/core/log.h"
#include "econet_select.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.select";

void EconetSelect::setup() {
  this->parent_->register_listener(
      this->select_id_, this->request_mod_, this->request_once_,
      [this](const EconetDatapoint &datapoint) {
        if (datapoint.type == EconetDatapointType::UNSUPPORTED) {
          return;
        }
        if (datapoint.type != EconetDatapointType::ENUM_TEXT) {
          ESP_LOGW(TAG, "Unexpected datapoint type %u for select %s", static_cast<uint8_t>(datapoint.type),
                   this->select_id_);
          return;
        }
        uint8_t enum_value = datapoint.value_enum;
        ESP_LOGV(TAG, "MCU reported select %s value %u", this->select_id_, enum_value);
        const auto &mappings = this->mappings_;
        auto it = std::find(mappings.begin(), mappings.end(), enum_value);
        if (it == mappings.end()) {
          ESP_LOGW(TAG, "Invalid value %u", enum_value);
          return;
        }
        size_t mapping_idx = std::distance(mappings.begin(), it);
        this->publish_state(mapping_idx);
      },
      false, this->src_adr_);
}

void EconetSelect::control(size_t index) {
  uint8_t mapping = this->mappings_[index];
  ESP_LOGV(TAG, "Setting %s datapoint value to %u:%s", this->select_id_, mapping, this->option_at(index));
  this->parent_->set_enum_datapoint_value(this->select_id_, mapping, this->src_adr_);
}

void EconetSelect::dump_config() {
  LOG_SELECT("", "Econet Select", this);
  ESP_LOGCONFIG(TAG, "  Select has datapoint ID %s", this->select_id_);
  ESP_LOGCONFIG(TAG, "  Options are:");
  for (size_t i = 0; i < this->mappings_.size(); i++) {
    ESP_LOGCONFIG(TAG, "    %u: %s", this->mappings_[i], this->option_at(i));
  }
}

}  // namespace econet
}  // namespace esphome
