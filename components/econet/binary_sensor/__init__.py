import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import CONF_ECONET_ID, ECONET_CLIENT_SCHEMA, EconetClient, econet_ns

EconetBinarySensor = econet_ns.class_(
    "EconetBinarySensor", cg.PollingComponent, EconetClient
)

CONF_ENABLE_STATE = "enable_state"
CONF_HEATCTRL = "heatctrl"
CONF_FAN_CTRL = "fan_ctrl"
CONF_COMP_RLY = "comp_rly"

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EconetBinarySensor),
            cv.Optional(CONF_ENABLE_STATE): binary_sensor.binary_sensor_schema(),
        },
        {
            cv.GenerateID(): cv.declare_id(EconetBinarySensor),
            cv.Optional(CONF_HEATCTRL): binary_sensor.binary_sensor_schema(),
        },
        {
            cv.GenerateID(): cv.declare_id(EconetBinarySensor),
            cv.Optional(CONF_FAN_CTRL): binary_sensor.binary_sensor_schema(),
        },
        {
            cv.GenerateID(): cv.declare_id(EconetBinarySensor),
            cv.Optional(CONF_COMP_RLY): binary_sensor.binary_sensor_schema(),
        },
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    econet_var = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet(econet_var))

    if CONF_ENABLE_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ENABLE_STATE])
        cg.add(var.set_enable_state_sensor(sens))
    if CONF_HEATCTRL in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HEATCTRL])
        cg.add(var.set_heatctrl_sensor(sens))
    if CONF_FAN_CTRL in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAN_CTRL])
        cg.add(var.set_fan_ctrl_sensor(sens))
    if CONF_COMP_RLY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_COMP_RLY])
        cg.add(var.set_comp_rly_sensor(sens))
