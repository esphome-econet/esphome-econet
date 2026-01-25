#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../econet.h"

#include <vector>

namespace esphome {
namespace econet {

class EconetSelect : public select::Select, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_select_id(const std::string &select_id) { this->select_id_ = select_id; }
  void init_select_mappings(size_t size) { this->mappings_.reserve(size); }
  void add_select_mapping(uint8_t mapping) { this->mappings_.push_back(mapping); }

 protected:
  void control(const std::string &value) override;

  std::string select_id_{""};
  std::vector<uint8_t> mappings_;
};

}  // namespace econet
}  // namespace esphome
