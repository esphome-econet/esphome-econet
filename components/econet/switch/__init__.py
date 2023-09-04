"""
Switch component for Econet
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_SWITCH_DATAPOINT

from .. import CONF_ECONET_ID, Econet, EconetClient, econet_ns

EconetSwitch = econet_ns.class_(
    "EconetSwitch", switch.Switch, cg.PollingComponent, EconetClient
)

CONF_ENABLE_SWITCH = "enable_switch"
CONF_DUMMY_SWITCH = "dummy_switch"

CONFIG_SCHEMA = (
    switch.switch_schema(EconetSwitch)
    .extend(
        {
            cv.GenerateID(CONF_ECONET_ID): cv.use_id(Econet),
            cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    """Generate main.cpp code"""
    var = await switch.new_switch(config)
    # var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    econet_var = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet(econet_var))
    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
