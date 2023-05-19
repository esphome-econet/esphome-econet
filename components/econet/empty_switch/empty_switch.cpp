#include "esphome/core/log.h"
#include "empty_switch.h"

namespace esphome {
namespace empty_switch {

static const char *TAG = "empty_switch.switch";

void EmptySwitch::setup() {

}

void EmptySwitch::write_state(bool state) {

}

void EmptySwitch::dump_config(){
    ESP_LOGCONFIG(TAG, "Empty custom switch");
}

} //namespace empty_switch
} //namespace esphome