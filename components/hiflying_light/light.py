import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

from . import HiFlyingLightComponent, hiflying_light_ns

HiFlyingLightOutput = hiflying_light_ns.class_(
    "HiFlyingLightOutput", light.LightOutput
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HiFlyingLightOutput),
        cv.Required("hiflying_light_id"): cv.use_id(HiFlyingLightComponent),
        cv.Optional("color_temperature", default=False): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    parent = await cg.get_variable(config["hiflying_light_id"])
    cg.add(var.set_parent(parent))
    
    if config["color_temperature"]:
        cg.add(var.set_color_temperature_support(True)) 