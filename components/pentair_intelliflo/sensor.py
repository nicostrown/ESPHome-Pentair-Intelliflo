import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_POWER,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_MINUTE,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_WATT,
)

from . import CONF_PENTAIR_INTELLIFLO_ID, PENTAIR_INTELLIFLO_CHILD_SCHEMA

DEPENDENCIES = ["pentair_intelliflo"]

CONF_RPM = "rpm"
CONF_FLOW = "flow"
CONF_FILTER_PERCENT = "filter_percent"
CONF_ERROR_CODE = "error_code"
CONF_TIME_REMAINING = "time_remaining"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RPM): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=0,
            icon="mdi:speedometer",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # Reported in gallons per minute. Zero on plain VS pumps, which have no
        # flow sensor; VF and VSF pumps report a real value here.
        cv.Optional(CONF_FLOW): sensor.sensor_schema(
            unit_of_measurement="gal/min",
            accuracy_decimals=0,
            device_class="volume_flow_rate",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # Percentage of the filter cycle *used*, not remaining.
        cv.Optional(CONF_FILTER_PERCENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            icon="mdi:air-filter",
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(
            accuracy_decimals=0,
            icon="mdi:alert-circle-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_TIME_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
            icon="mdi:timer-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(PENTAIR_INTELLIFLO_CHILD_SCHEMA)

SETTERS = {
    CONF_POWER: "set_power_sensor",
    CONF_RPM: "set_rpm_sensor",
    CONF_FLOW: "set_flow_sensor",
    CONF_FILTER_PERCENT: "set_filter_percent_sensor",
    CONF_ERROR_CODE: "set_error_code_sensor",
    CONF_TIME_REMAINING: "set_time_remaining_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PENTAIR_INTELLIFLO_ID])
    for key, setter in SETTERS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(parent, setter)(sens))
