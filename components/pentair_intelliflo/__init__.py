import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import uart

MULTI_CONF = True

DEPENDENCIES = ["uart"]
#AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor", "switch", "output"]

CONF_INTELLIFLO_ID = "intelliflo_id"

intelliflo_ns = cg.esphome_ns.namespace("intelliflo")

IntellifloComponent = intelliflo_ns.class_("Intelliflo", cg.Component)

INTELLIFLO_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INTELLIFLO_ID): cv.use_id(IntellifloComponent),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IntellifloComponent),
    }
).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("30s"))
    


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
