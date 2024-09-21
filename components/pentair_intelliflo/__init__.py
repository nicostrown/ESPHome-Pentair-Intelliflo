import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import uart

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor", "switch", "output"]
MULTI_CONF = True

CONF_INTELLIFLO_ID = "intelliflo_id"

intelliflo_ns = cg.esphome_ns.namespace("intelliflo")
IntellifloComponent = intelliflo_ns.class_("Intelliflo", cg.Component)

INTELLIFLO_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_INTELLIFLO_ID): cv.use_id(IntellifloComponent),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(IntellifloComponent)})
    .extend(cv.polling_component_schema("30s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
