import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_PENTAIR_INTELLIFLO_ID, PENTAIR_INTELLIFLO_CHILD_SCHEMA

DEPENDENCIES = ["pentair_intelliflo"]

CONF_PROGRAM = "program"
CONF_PUMP_STATE = "pump_state"
CONF_ERROR = "error"

CONFIG_SCHEMA = cv.Schema(
    {
        # Which mode/program the pump reports it is following.
        cv.Optional(CONF_PROGRAM): text_sensor.text_sensor_schema(
            icon="mdi:playlist-play",
        ),
        # Drive state: Running / Priming / System Priming / Fault.
        cv.Optional(CONF_PUMP_STATE): text_sensor.text_sensor_schema(
            icon="mdi:state-machine",
        ),
        cv.Optional(CONF_ERROR): text_sensor.text_sensor_schema(
            icon="mdi:alert-circle-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(PENTAIR_INTELLIFLO_CHILD_SCHEMA)

SETTERS = {
    CONF_PROGRAM: "set_program_text_sensor",
    CONF_PUMP_STATE: "set_pump_state_text_sensor",
    CONF_ERROR: "set_error_text_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PENTAIR_INTELLIFLO_ID])
    for key, setter in SETTERS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(parent, setter)(sens))
