import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_CUSTOM_FAN_MODES, CONF_CUSTOM_PRESETS, CONF_ID

from .. import (
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
    optional_datapoint_id,
    register_econet_client,
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
    """Reject invalid combinations of single-point and two-point target temperatures.

    Climate stores target_temperature in a union with target_temperature_low/high, so
    listeners for both would write over each other's value, and two-point control requires
    both low and high datapoints.
    """
    has_single = bool(config[CONF_TARGET_TEMPERATURE_DATAPOINT])
    has_low = bool(config[CONF_TARGET_TEMPERATURE_LOW_DATAPOINT])
    has_high = bool(config[CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT])
    if has_single and (has_low or has_high):
        raise cv.Invalid(
            f"{CONF_TARGET_TEMPERATURE_DATAPOINT} cannot be combined with "
            f"{CONF_TARGET_TEMPERATURE_LOW_DATAPOINT} or "
            f"{CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT}: they share the same storage."
        )
    if has_low != has_high:
        raise cv.Invalid(
            f"{CONF_TARGET_TEMPERATURE_LOW_DATAPOINT} and "
            f"{CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT} must be set together."
        )
    return config


def validate_mapped_datapoints(config):
    """Require each enum datapoint and its value mapping to be configured together."""
    if bool(config[CONF_MODE_DATAPOINT]) != bool(config[CONF_MODES]):
        raise cv.Invalid(
            f"{CONF_MODE_DATAPOINT} and {CONF_MODES} must be set together."
        )
    if bool(config[CONF_CUSTOM_PRESET_DATAPOINT]) != bool(config[CONF_CUSTOM_PRESETS]):
        raise cv.Invalid(
            f"{CONF_CUSTOM_PRESET_DATAPOINT} and {CONF_CUSTOM_PRESETS} must be set together."
        )
    has_fan_dp = bool(
        config[CONF_CUSTOM_FAN_MODE_DATAPOINT]
        or config[CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT]
    )
    if has_fan_dp != bool(config[CONF_CUSTOM_FAN_MODES]):
        raise cv.Invalid(
            f"{CONF_CUSTOM_FAN_MODE_DATAPOINT}/{CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT} "
            f"and {CONF_CUSTOM_FAN_MODES} must be set together."
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(EconetClimate)
    .extend(
        {
            cv.Optional(
                CONF_CURRENT_TEMPERATURE_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_TARGET_TEMPERATURE_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_TARGET_TEMPERATURE_LOW_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(CONF_MODE_DATAPOINT, default=""): optional_datapoint_id,
            cv.Optional(
                CONF_CUSTOM_PRESET_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_CUSTOM_FAN_MODE_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_FOLLOW_SCHEDULE_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(CONF_MODES, default={}): ensure_climate_mode_map,
            cv.Optional(CONF_CUSTOM_PRESETS, default={}): ensure_option_map,
            cv.Optional(CONF_CUSTOM_FAN_MODES, default={}): ensure_option_map,
            cv.Optional(
                CONF_CURRENT_HUMIDITY_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(
                CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT, default=""
            ): optional_datapoint_id,
            cv.Optional(CONF_ACTION_DATAPOINT, default=""): optional_datapoint_id,
            cv.Optional(CONF_ACTIONS, default={}): ensure_climate_action_map,
            cv.Optional(CONF_DEFAULT_ACTION): climate.validate_climate_action,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA),
    validate_target_temperature_datapoints,
    validate_mapped_datapoints,
    validate_action_datapoint,
)


_DATAPOINT_SETTERS = (
    (CONF_CURRENT_TEMPERATURE_DATAPOINT, "set_current_temperature_id"),
    (CONF_TARGET_TEMPERATURE_DATAPOINT, "set_target_temperature_id"),
    (CONF_TARGET_TEMPERATURE_LOW_DATAPOINT, "set_target_temperature_low_id"),
    (CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT, "set_target_temperature_high_id"),
    (CONF_MODE_DATAPOINT, "set_mode_id"),
    (CONF_CUSTOM_PRESET_DATAPOINT, "set_custom_preset_id"),
    (CONF_CUSTOM_FAN_MODE_DATAPOINT, "set_custom_fan_mode_id"),
    (
        CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT,
        "set_custom_fan_mode_no_schedule_id",
    ),
    (CONF_FOLLOW_SCHEDULE_DATAPOINT, "set_follow_schedule_id"),
    (CONF_CURRENT_HUMIDITY_DATAPOINT, "set_current_humidity_id"),
    (
        CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT,
        "set_target_dehumidification_level_id",
    ),
    (CONF_ACTION_DATAPOINT, "set_action_id"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await register_econet_client(var, config)

    for conf_key, setter in _DATAPOINT_SETTERS:
        if dp := config[conf_key]:
            cg.add(getattr(var, setter)(dp))

    if modes := config[CONF_MODES]:
        cg.add(var.init_modes(len(modes)))
        for key, value in modes.items():
            cg.add(var.add_mode(key, value))
    if presets := config[CONF_CUSTOM_PRESETS]:
        cg.add(var.init_custom_presets(len(presets)))
        for key, value in presets.items():
            cg.add(var.add_custom_preset(key, value))
    if fan_modes := config[CONF_CUSTOM_FAN_MODES]:
        cg.add(var.init_custom_fan_modes(len(fan_modes)))
        for key, value in fan_modes.items():
            cg.add(var.add_custom_fan_mode(key, value))
    if actions := config[CONF_ACTIONS]:
        cg.add(var.init_actions(len(actions)))
        for key, value in actions.items():
            cg.add(var.add_action(key, value))
    if CONF_DEFAULT_ACTION in config:
        cg.add(var.set_default_action(config[CONF_DEFAULT_ACTION]))
