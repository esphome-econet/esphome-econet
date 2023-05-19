"""
Binary Sensor component for Econet
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
)

from .. import (
    econet_ns,
    CONF_ECONET_ID,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
)

EconetBinarySensor = econet_ns.class_(
    "EconetBinarySensor", cg.PollingComponent, EconetClient
)

CONF_ENABLE_STATE = "enable_state"

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EconetBinarySensor),
            cv.Optional(CONF_ENABLE_STATE): binary_sensor.binary_sensor_schema()
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    """Generate main.cpp code"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    econet_var = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet(econet_var))

    if CONF_ENABLE_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ENABLE_STATE])
        cg.add(var.set_enable_state_sensor(sens))