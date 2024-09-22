import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import INTELLIFLO_CHILD_SCHEMA, CONF_INTELLIFLO_ID
from esphome.const import (
    DEVICE_CLASS_RUNNING,
)

DEPENDENCIES = ["pentair_intelliflo"]

#sensors
CONF_RUNNING = "running"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
        ),
    }
).extend(INTELLIFLO_CHILD_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_INTELLIFLO_ID])

    if running_config := config.get(CONF_RUNNING):
        sens = await binary_sensor.new_binary_sensor(running_config)
        cg.add(var.set_running(sens))
