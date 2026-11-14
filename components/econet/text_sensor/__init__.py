import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_SENSOR_DATAPOINT

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    datapoint_id,
    econet_ns,
    register_econet_client,
)

DEPENDENCIES = ["econet"]

EconetTextSensor = econet_ns.class_(
    "EconetTextSensor", text_sensor.TextSensor, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(EconetTextSensor)
    .extend(
        {
            cv.Required(CONF_SENSOR_DATAPOINT): datapoint_id,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await register_econet_client(var, config)
    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
