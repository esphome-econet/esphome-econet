import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    datapoint_id,
    econet_ns,
    register_econet_client,
)

DEPENDENCIES = ["econet"]

EconetSensor = econet_ns.class_(
    "EconetSensor", sensor.Sensor, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(EconetSensor)
    .extend(
        {
            cv.Required(CONF_SENSOR_DATAPOINT): datapoint_id,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await register_econet_client(var, config)
    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
