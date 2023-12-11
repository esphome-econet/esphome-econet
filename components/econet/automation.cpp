#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "econet.automation";

namespace esphome {
namespace econet {

EconetRawDatapointUpdateTrigger::EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id,
                                                                 int8_t request_mod, uint32_t src_adr) {
  parent->register_listener(
      sensor_id, request_mod, false,
      [this, src_adr](const EconetDatapoint &dp) {
        if (src_adr == 0 || dp.src_adr == src_adr) {
          this->trigger(dp.value_raw);
        }
      },
      true);
}

}  // namespace econet
}  // namespace esphome
