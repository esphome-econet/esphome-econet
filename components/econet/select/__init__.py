import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ENUM_DATAPOINT, CONF_OPTIONS

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    datapoint_id,
    econet_ns,
    register_econet_client,
    unique_value_map,
)

DEPENDENCIES = ["econet"]

EconetSelect = econet_ns.class_(
    "EconetSelect", select.Select, cg.Component, EconetClient
)


ensure_option_map = unique_value_map(cv.string_strict)


CONFIG_SCHEMA = (
    select.select_schema(EconetSelect)
    .extend(
        {
            cv.Required(CONF_ENUM_DATAPOINT): datapoint_id,
            cv.Required(CONF_OPTIONS): ensure_option_map,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    options_map = config[CONF_OPTIONS]
    var = await select.new_select(config, options=list(options_map.values()))
    await cg.register_component(var, config)
    cg.add(var.init_select_mappings(len(options_map)))
    for key in options_map:
        cg.add(var.add_select_mapping(key))
    await register_econet_client(var, config)
    cg.add(var.set_select_id(config[CONF_ENUM_DATAPOINT]))
