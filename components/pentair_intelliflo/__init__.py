import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ADDRESS, CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_PENTAIR_INTELLIFLO_ID = "pentair_intelliflo_id"

pentair_intelliflo_ns = cg.esphome_ns.namespace("pentair_intelliflo")
PentairIntelliflo = pentair_intelliflo_ns.class_(
    "PentairIntelliflo", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PentairIntelliflo),
            # Pumps answer on 0x60 + pump index (0x60 for the first pump).
            cv.Optional(CONF_ADDRESS, default=0x60): cv.hex_int_range(
                min=0x60, max=0x6F
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

PENTAIR_INTELLIFLO_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PENTAIR_INTELLIFLO_ID): cv.use_id(PentairIntelliflo),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "pentair_intelliflo", baud_rate=9600, require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_address(config[CONF_ADDRESS]))
