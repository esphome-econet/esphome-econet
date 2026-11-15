import math

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NUMBER_DATAPOINT,
    CONF_STEP,
)

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    datapoint_id,
    econet_ns,
    register_econet_client,
)

DEPENDENCIES = ["econet"]

EconetNumber = econet_ns.class_(
    "EconetNumber", number.Number, cg.Component, EconetClient
)


def finite_float(value):
    value = cv.float_(value)
    if not math.isfinite(value):
        raise cv.Invalid("Value must be a finite number")
    return value


finite_positive_float = cv.All(finite_float, cv.Range(min=0, min_included=False))


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    if config[CONF_STEP] > config[CONF_MAX_VALUE] - config[CONF_MIN_VALUE]:
        raise cv.Invalid("step must not be greater than max_value - min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(EconetNumber)
    .extend(
        {
            cv.Required(CONF_NUMBER_DATAPOINT): datapoint_id,
            cv.Required(CONF_MAX_VALUE): finite_float,
            cv.Required(CONF_MIN_VALUE): finite_float,
            cv.Required(CONF_STEP): finite_positive_float,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max,
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)
    await register_econet_client(var, config)
    cg.add(var.set_number_id(config[CONF_NUMBER_DATAPOINT]))
