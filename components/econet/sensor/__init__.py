"""
Sensor component for Econet
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    UNIT_CELSIUS,
    ICON_THERMOMETER,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
)

from .. import (
    econet_ns,
    CONF_ECONET_ID,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
)

EconetSensor = econet_ns.class_(
    "EconetSensor", cg.PollingComponent, EconetClient
)

CONF_ECONET_ID = "econet"
CONF_TEMP_IN = "temp_in"
CONF_TEMP_OUT = "temp_out"
CONF_SETPOINT = "setpoint"
CONF_FLOW_RATE = "flow_rate"
CONF_WATER_USED = "water_used"
CONF_BTUS_USED = "btus_used"
CONF_IGNITION_CYCLES = "ignition_cycles"
CONF_INSTANT_BTUS = "instant_btus"
CONF_HOT_WATER = "hot_water"
CONF_AMBIENTT = "ambient_temp"
CONF_LOHTRTMP = "lower_water_heater_temp"
CONF_UPHTRTMP = "upper_water_heater_temp"
CONF_POWRWATT = "power_watt"
CONF_EVAPTEMP = "evap_temp"
CONF_SUCTIONT = "suction_temp"
CONF_DISCTEMP = "discharge_temp"

CONF_CC_HVACMODE = "cc_hvacmode"
CONF_CC_SPT_STAT = "cc_spt_stat"
CONF_CC_COOLSETP = "cc_coolsetp"
CONF_CC_AUTOMODE = "cc_automode"
CONF_CC_REL_HUM = "cc_rel_hum"

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_TEMP_IN): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_TEMP_OUT): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_SETPOINT): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_FLOW_RATE): sensor.sensor_schema(
                unit_of_measurement="gpm",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_WATER_USED): sensor.sensor_schema(
                unit_of_measurement="gal",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_BTUS_USED): sensor.sensor_schema(
                unit_of_measurement="kbtu",
                accuracy_decimals=3,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_IGNITION_CYCLES): sensor.sensor_schema(
                unit_of_measurement="",
                accuracy_decimals=0,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_INSTANT_BTUS): sensor.sensor_schema(
                unit_of_measurement="kbtu/hr",
                accuracy_decimals=3,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_HOT_WATER): sensor.sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=0,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_AMBIENTT): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_LOHTRTMP): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_UPHTRTMP): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_POWRWATT): sensor.sensor_schema(
                unit_of_measurement="W",
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_EVAPTEMP): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_SUCTIONT): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_DISCTEMP): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_CC_SPT_STAT): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
        {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_CC_COOLSETP): sensor.sensor_schema(
                unit_of_measurement="°F",
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_CC_HVACMODE): sensor.sensor_schema(
                unit_of_measurement="",
                accuracy_decimals=0,
            )
        },
		{
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_CC_AUTOMODE): sensor.sensor_schema(
                unit_of_measurement="",
                accuracy_decimals=0,
            )
        },
    {
            cv.GenerateID(): cv.declare_id(EconetSensor),
            cv.Optional(CONF_CC_REL_HUM): sensor.sensor_schema(
                unit_of_measurement="%",
                icon=ICON_WATER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("5s"))
)




async def to_code(config):
    """Generate main.cpp code"""

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    econet_var = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet(econet_var))

    if CONF_TEMP_IN in config:
        sens = await sensor.new_sensor(config[CONF_TEMP_IN])
        cg.add(var.set_temp_in_sensor(sens))
    if CONF_TEMP_OUT in config:
        sens = await sensor.new_sensor(config[CONF_TEMP_OUT])
        cg.add(var.set_temp_out_sensor(sens))
    if CONF_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_SETPOINT])
        cg.add(var.set_setpoint_sensor(sens))
    if CONF_FLOW_RATE in config:
        sens = await sensor.new_sensor(config[CONF_FLOW_RATE])
        cg.add(var.set_flow_rate_sensor(sens))
    if CONF_WATER_USED in config:
        sens = await sensor.new_sensor(config[CONF_WATER_USED])
        cg.add(var.set_water_used_sensor(sens))
    if CONF_BTUS_USED in config:
        sens = await sensor.new_sensor(config[CONF_BTUS_USED])
        cg.add(var.set_btus_used_sensor(sens))
    if CONF_IGNITION_CYCLES in config:
        sens = await sensor.new_sensor(config[CONF_IGNITION_CYCLES])
        cg.add(var.set_ignition_cycles_sensor(sens))
    if CONF_INSTANT_BTUS in config:
        sens = await sensor.new_sensor(config[CONF_INSTANT_BTUS])
        cg.add(var.set_instant_btus_sensor(sens))
    if CONF_HOT_WATER in config:
        sens = await sensor.new_sensor(config[CONF_HOT_WATER])
        cg.add(var.set_hot_water_sensor(sens))
    if CONF_AMBIENTT in config:
        sens = await sensor.new_sensor(config[CONF_AMBIENTT])
        cg.add(var.set_ambient_temp_sensor(sens))
    if CONF_LOHTRTMP in config:
        sens = await sensor.new_sensor(config[CONF_LOHTRTMP])
        cg.add(var.set_lower_water_heater_temp_sensor(sens))
    if CONF_UPHTRTMP in config:
        sens = await sensor.new_sensor(config[CONF_UPHTRTMP])
        cg.add(var.set_upper_water_heater_temp_sensor(sens))
    if CONF_POWRWATT in config:
        sens = await sensor.new_sensor(config[CONF_POWRWATT])
        cg.add(var.set_power_watt_sensor(sens))
    if CONF_EVAPTEMP in config:
        sens = await sensor.new_sensor(config[CONF_EVAPTEMP])
        cg.add(var.set_evap_temp_sensor(sens))
    if CONF_SUCTIONT in config:
        sens = await sensor.new_sensor(config[CONF_SUCTIONT])
        cg.add(var.set_suction_temp_sensor(sens))
    if CONF_DISCTEMP in config:
        sens = await sensor.new_sensor(config[CONF_DISCTEMP])
        cg.add(var.set_discharge_temp_sensor(sens))
    if CONF_CC_HVACMODE in config:
        sens = await sensor.new_sensor(config[CONF_CC_HVACMODE])
        cg.add(var.set_cc_hvacmode_sensor(sens))
    if CONF_CC_SPT_STAT in config:
        sens = await sensor.new_sensor(config[CONF_CC_SPT_STAT])
        cg.add(var.set_cc_spt_stat_sensor(sens))
    if CONF_CC_COOLSETP in config:
        sens = await sensor.new_sensor(config[CONF_CC_COOLSETP])
        cg.add(var.set_cc_coolsetp_sensor(sens))
    if CONF_CC_AUTOMODE in config:
        sens = await sensor.new_sensor(config[CONF_CC_AUTOMODE])
        cg.add(var.set_cc_automode_sensor(sens))
    if CONF_CC_REL_HUM in config:
        sens = await sensor.new_sensor(config[CONF_CC_REL_HUM])
        cg.add(var.set_cc_rel_hum_sensor(sens))