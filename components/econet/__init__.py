"""
Econet ESPHome component config validation & code generation.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CONF_UART = "uart"
CONF_ECONET_ID = "econet"
CONF_MODEL = "model"

econet_ns = cg.esphome_ns.namespace("econet")
Econet = econet_ns.class_("Econet", cg.Component)
EconetClient = econet_ns.class_("EconetClient")
uart_ns = cg.esphome_ns.namespace("uart")
UARTComponent = uart_ns.class_("UARTComponent")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Econet),
        cv.Required(CONF_UART): cv.use_id(UARTComponent),
        cv.Required("model"): cv.string,
    }
)

ECONET_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECONET_ID): cv.use_id(Econet),
    }
)


async def to_code(config):
    """Generate code"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    uart = await cg.get_variable(config[CONF_UART])
    cg.add(var.set_uart(uart))
    if config[CONF_MODEL] == "Tankless":
        cg.add(var.set_type_id(0))
    if config[CONF_MODEL] == "Heatpump":
        cg.add(var.set_type_id(1))
    if config[CONF_MODEL] == "HVAC":
        cg.add(var.set_type_id(2))
