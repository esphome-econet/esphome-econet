import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_SWITCH_DATAPOINT

from .. import CONF_ECONET_ID, ECONET_CLIENT_SCHEMA, EconetClient, econet_ns

EconetSwitch = econet_ns.class_(
    "EconetSwitch", switch.Switch, cg.PollingComponent, EconetClient
)

CONFIG_SCHEMA = (
    switch.switch_schema(EconetSwitch)
    .extend(
        {
            cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
