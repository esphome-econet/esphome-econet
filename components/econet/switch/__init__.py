import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_SWITCH_DATAPOINT

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    datapoint_id,
    econet_ns,
    register_econet_client,
)

DEPENDENCIES = ["econet"]

EconetSwitch = econet_ns.class_(
    "EconetSwitch", switch.Switch, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    switch.switch_schema(EconetSwitch, default_restore_mode="DISABLED")
    .extend(
        {
            cv.Required(CONF_SWITCH_DATAPOINT): datapoint_id,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await register_econet_client(var, config)
    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
