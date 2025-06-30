import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID

from . import HiFlyingLightComponent, hiflying_light_ns

HiFlyingLightPairButton = hiflying_light_ns.class_(
    "HiFlyingLightPairButton", button.Button
)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HiFlyingLightPairButton),
        cv.Required("hiflying_light_id"): cv.use_id(HiFlyingLightComponent),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)

    parent = await cg.get_variable(config["hiflying_light_id"])
    cg.add(var.set_parent(parent)) 