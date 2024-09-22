import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import INTELLIFLO_CHILD_SCHEMA, CONF_INTELLIFLO_ID

DEPENDENCIES = ["pentair_intelliflo"]

#sensors
CONF_PROGRAM = "program"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PROGRAM): text_sensor.text_sensor_schema(
        ),
    }
).extend(INTELLIFLO_CHILD_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_INTELLIFLO_ID])

    if program_config := config.get(CONF_PROGRAM):
        sens = await text_sensor.new_text_sensor(program_config)
        cg.add(var.set_program(sens))
