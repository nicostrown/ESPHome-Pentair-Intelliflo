import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_RUNNING, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_PENTAIR_INTELLIFLO_ID, PENTAIR_INTELLIFLO_CHILD_SCHEMA

DEPENDENCIES = ["pentair_intelliflo"]

CONF_RUNNING = "running"
CONF_REMOTE_CONTROL = "remote_control"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            icon="mdi:pump",
        ),
        # True while the pump's own keypad is locked out and we are driving it.
        cv.Optional(CONF_REMOTE_CONTROL): binary_sensor.binary_sensor_schema(
            icon="mdi:remote",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(PENTAIR_INTELLIFLO_CHILD_SCHEMA)

SETTERS = {
    CONF_RUNNING: "set_running_binary_sensor",
    CONF_REMOTE_CONTROL: "set_remote_control_binary_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PENTAIR_INTELLIFLO_ID])
    for key, setter in SETTERS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(parent, setter)(sens))
