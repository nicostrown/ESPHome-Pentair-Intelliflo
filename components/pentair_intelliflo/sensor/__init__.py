import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_PRESSURE,
    UNIT_WATT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_CUBIC_METER_PER_HOUR,
)
from .. import INTELLIFLO_COMPONENT_SCHEMA, CONF_INTELLIFLO_ID

DEPENDENCIES = ["uart"]

#sensors
CONF_POWER = "power"
CONF_RPM = "rpm"
CONF_FLOW = "flow"
CONF_PRESSURE = "pressure"

TYPES = {
    CONF_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
    ),
    CONF_RPM: sensor.sensor_schema(
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
    ),
    CONF_FLOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
    ),
    CONF_PRESSURE: sensor.sensor_schema(
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_PRESSURE,
    ),
}

CONFIG_SCHEMA = INTELLIFLO_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in TYPES.items()}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_INTELLIFLO_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(paren, f"set_{type}")(sens))
