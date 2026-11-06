import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_CUSTOM_FAN_MODES, CONF_CUSTOM_PRESETS, CONF_ID

from .. import (
    CONF_ECONET_ID,
    CONF_REQUEST_MOD,
    CONF_REQUEST_ONCE,
    CONF_SRC_ADDRESS,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
    unique_value_map,
)

DEPENDENCIES = ["econet"]

CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_TARGET_TEMPERATURE_LOW_DATAPOINT = "target_temperature_low_datapoint"
CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT = "target_temperature_high_datapoint"
CONF_MODE_DATAPOINT = "mode_datapoint"
CONF_CUSTOM_PRESET_DATAPOINT = "custom_preset_datapoint"
CONF_CUSTOM_FAN_MODE_DATAPOINT = "custom_fan_mode_datapoint"
CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT = "custom_fan_mode_no_schedule_datapoint"
CONF_FOLLOW_SCHEDULE_DATAPOINT = "follow_schedule_datapoint"
CONF_MODES = "modes"
CONF_CURRENT_HUMIDITY_DATAPOINT = "current_humidity_datapoint"
CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT = "target_dehumidification_level_datapoint"
CONF_ACTION_DATAPOINT = "action_datapoint"
CONF_ACTIONS = "actions"
CONF_DEFAULT_ACTION = "default_action"

EconetClimate = econet_ns.class_(
    "EconetClimate", climate.Climate, cg.Component, EconetClient
)


ensure_climate_mode_map = unique_value_map(climate.validate_climate_mode)

ensure_option_map = unique_value_map(cv.string_strict)


def ensure_climate_action_map(value):
    """Validate a {uint8: action} mapping.

    Unlike modes and presets, several values may report the same action: the action is
    only ever read from the appliance, never written back.
    """
    cv.check_not_templatable(value)
    return cv.Schema({cv.uint8_t: climate.validate_climate_action})(value)


def validate_action_datapoint(config):
    """Require the action datapoint and something to map its values with, or neither."""
    has_mapping = bool(config[CONF_ACTIONS]) or CONF_DEFAULT_ACTION in config
    if config[CONF_ACTION_DATAPOINT] and not has_mapping:
        raise cv.Invalid(
            f"{CONF_ACTION_DATAPOINT} needs {CONF_ACTIONS}, {CONF_DEFAULT_ACTION} or both."
        )
    if has_mapping and not config[CONF_ACTION_DATAPOINT]:
        raise cv.Invalid(
            f"{CONF_ACTIONS} and {CONF_DEFAULT_ACTION} need {CONF_ACTION_DATAPOINT}."
        )
    return config


def validate_target_temperature_datapoints(config):
    """Reject a single-point target temperature combined with a two-point one.

    Climate stores target_temperature in a union with target_temperature_low/high, so
    listeners for both would write over each other's value.
    """
    if config[CONF_TARGET_TEMPERATURE_DATAPOINT] and (
        config[CONF_TARGET_TEMPERATURE_LOW_DATAPOINT]
        or config[CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT]
    ):
        raise cv.Invalid(
            f"{CONF_TARGET_TEMPERATURE_DATAPOINT} cannot be combined with "
            f"{CONF_TARGET_TEMPERATURE_LOW_DATAPOINT} or "
            f"{CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT}: they share the same storage."
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(EconetClimate)
    .extend(
        {
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_MODE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_CUSTOM_PRESET_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_CUSTOM_FAN_MODE_DATAPOINT, default=""): cv.string,
            cv.Optional(
                CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT, default=""
            ): cv.string,
            cv.Optional(CONF_FOLLOW_SCHEDULE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_MODES, default={}): ensure_climate_mode_map,
            cv.Optional(CONF_CUSTOM_PRESETS, default={}): ensure_option_map,
            cv.Optional(CONF_CUSTOM_FAN_MODES, default={}): ensure_option_map,
            cv.Optional(CONF_CURRENT_HUMIDITY_DATAPOINT, default=""): cv.string,
            cv.Optional(
                CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT, default=""
            ): cv.string,
            cv.Optional(CONF_ACTION_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_ACTIONS, default={}): ensure_climate_action_map,
            cv.Optional(CONF_DEFAULT_ACTION): climate.validate_climate_action,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA),
    validate_target_temperature_datapoints,
    validate_action_datapoint,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_request_mod(config[CONF_REQUEST_MOD]))
    cg.add(var.set_request_once(config[CONF_REQUEST_ONCE]))
    cg.add(var.set_src_adr(config[CONF_SRC_ADDRESS]))
    cg.add(var.set_current_temperature_id(config[CONF_CURRENT_TEMPERATURE_DATAPOINT]))
    cg.add(var.set_target_temperature_id(config[CONF_TARGET_TEMPERATURE_DATAPOINT]))
    cg.add(
        var.set_target_temperature_low_id(config[CONF_TARGET_TEMPERATURE_LOW_DATAPOINT])
    )
    cg.add(
        var.set_target_temperature_high_id(
            config[CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT]
        )
    )
    cg.add(var.set_mode_id(config[CONF_MODE_DATAPOINT]))
    cg.add(var.set_custom_preset_id(config[CONF_CUSTOM_PRESET_DATAPOINT]))
    cg.add(var.set_custom_fan_mode_id(config[CONF_CUSTOM_FAN_MODE_DATAPOINT]))
    cg.add(
        var.set_custom_fan_mode_no_schedule_id(
            config[CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT]
        )
    )
    cg.add(var.set_follow_schedule_id(config[CONF_FOLLOW_SCHEDULE_DATAPOINT]))
    if CONF_MODES in config:
        modes = config[CONF_MODES]
        cg.add(var.init_modes(len(modes)))
        for key, value in modes.items():
            cg.add(var.add_mode(key, value))
    if CONF_CUSTOM_PRESETS in config:
        presets = config[CONF_CUSTOM_PRESETS]
        cg.add(var.init_custom_presets(len(presets)))
        for key, value in presets.items():
            cg.add(var.add_custom_preset(key, value))
    if CONF_CUSTOM_FAN_MODES in config:
        fan_modes = config[CONF_CUSTOM_FAN_MODES]
        cg.add(var.init_custom_fan_modes(len(fan_modes)))
        for key, value in fan_modes.items():
            cg.add(var.add_custom_fan_mode(key, value))
    cg.add(var.set_current_humidity_id(config[CONF_CURRENT_HUMIDITY_DATAPOINT]))
    cg.add(
        var.set_target_dehumidification_level_id(
            config[CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT]
        )
    )
    cg.add(var.set_action_id(config[CONF_ACTION_DATAPOINT]))
    if actions := config[CONF_ACTIONS]:
        cg.add(var.init_actions(len(actions)))
        for key, value in actions.items():
            cg.add(var.add_action(key, value))
    if CONF_DEFAULT_ACTION in config:
        cg.add(var.set_default_action(config[CONF_DEFAULT_ACTION]))
