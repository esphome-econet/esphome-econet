import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

empty_switch_ns = cg.esphome_ns.namespace("empty_switch")
EmptySwitch = empty_switch_ns.class_("EmptySwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(EmptySwitch)}
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
