#include "esphome/core/log.h"
#include "econet_switch.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.switch";

void EconetSwitch::update() {
  this->publish_state(this->parent_->get_enable_state());
}

void EconetSwitch::write_state(bool state) {
  ESP_LOGD("econet", "write_state");
  this->parent_->set_enable_state(state);
}

void EconetSwitch::dump_config() {
  LOG_SWITCH("", "Econet Switch", this);
}

}  // namespace econet
}  // namespace esphome
