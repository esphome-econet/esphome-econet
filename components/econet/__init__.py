import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CONF_MODEL = "model"

econet_ns = cg.esphome_ns.namespace("econet")
Econet = econet_ns.class_("Econet", cg.Component, uart.UARTDevice)
EconetClient = econet_ns.class_("EconetClient")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Econet),
            cv.Required("model"): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

CONF_ECONET_ID = "econet_id"
ECONET_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECONET_ID): cv.use_id(Econet),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if config[CONF_MODEL] == "Tankless":
        cg.add(var.set_type_id(0))
    if config[CONF_MODEL] == "Heatpump":
        cg.add(var.set_type_id(1))
    if config[CONF_MODEL] == "HVAC":
        cg.add(var.set_type_id(2))
