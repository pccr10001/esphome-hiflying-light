import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import automation
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_TRIGGER_ID,
)

DEPENDENCIES = ["esp32", "esp32_ble"]
CODEOWNERS = ["@hiflying"]

CONF_BEFORE_SEND = "before_send"
CONF_AFTER_SEND = "after_send"
CONF_INSTANCE_ID = "instance_id"
CONF_PACKET_INTERVAL = "packet_interval"
CONF_PACKET_COUNT = "packet_count"
CONF_COUNTER = "counter"

hiflying_light_ns = cg.esphome_ns.namespace("hiflying_light")
HiFlyingLightComponent = hiflying_light_ns.class_(
    "HiFlyingLightComponent", cg.Component
)

HiFlyingLightBeforeSendTrigger = hiflying_light_ns.class_(
    "HiFlyingLightBeforeSendTrigger", automation.Trigger
)

HiFlyingLightAfterSendTrigger = hiflying_light_ns.class_(
    "HiFlyingLightAfterSendTrigger", automation.Trigger
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HiFlyingLightComponent),
        cv.Optional(CONF_INSTANCE_ID, default=1): cv.int_range(min=1, max=99),
        cv.Optional(CONF_PACKET_INTERVAL, default="10ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PACKET_COUNT, default=3): cv.int_range(min=1, max=10),
        cv.Optional(CONF_COUNTER, default=1): cv.int_range(min=1, max=65535),
        cv.Optional(CONF_BEFORE_SEND): cv.All(
            cv.ensure_list,
            [
                cv.Schema(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            HiFlyingLightBeforeSendTrigger
                        ),
                    }
                )
            ],
        ),
        cv.Optional(CONF_AFTER_SEND): cv.All(
            cv.ensure_list,
            [
                cv.Schema(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            HiFlyingLightAfterSendTrigger
                        ),
                    }
                )
            ],
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # 設置 instance ID (用於生成不同的 MAC 地址)
    cg.add(var.set_instance_id(config[CONF_INSTANCE_ID]))
    
    # 設置封包參數
    cg.add(var.set_packet_interval(config[CONF_PACKET_INTERVAL]))
    cg.add(var.set_packet_count(config[CONF_PACKET_COUNT]))
    cg.add(var.set_counter(config[CONF_COUNTER]))

    # 設置 before_send 觸發器
    if CONF_BEFORE_SEND in config:
        for conf in config[CONF_BEFORE_SEND]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            cg.add(var.add_before_send_trigger(trigger))

    # 設置 after_send 觸發器
    if CONF_AFTER_SEND in config:
        for conf in config[CONF_AFTER_SEND]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            cg.add(var.add_after_send_trigger(trigger)) 