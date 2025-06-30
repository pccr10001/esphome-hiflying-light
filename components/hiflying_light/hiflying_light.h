#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/button/button.h"

#include <vector>
#include <array>
#include <map>

namespace esphome {
namespace hiflying_light {

enum HiFlyingCommand {
  COMMAND_PAIR = 1,
  COMMAND_OFF = 2,
  COMMAND_ON = 3,
  COMMAND_BRIGHTNESS = 12,
  COMMAND_COLOR_TEMP = 11
};

struct CommandInfo {
  uint8_t cmd1;
  int8_t ctrl_code;
};

class HiFlyingLightComponent;

class HiFlyingLightBeforeSendTrigger : public Trigger<> {
 public:
  explicit HiFlyingLightBeforeSendTrigger(HiFlyingLightComponent *parent) : parent_(parent) {}

 protected:
  HiFlyingLightComponent *parent_;
};

class HiFlyingLightAfterSendTrigger : public Trigger<> {
 public:
  explicit HiFlyingLightAfterSendTrigger(HiFlyingLightComponent *parent) : parent_(parent) {}

 protected:
  HiFlyingLightComponent *parent_;
};

class HiFlyingLightComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // 配置方法
  void set_instance_id(uint8_t instance_id) { this->instance_id_ = instance_id; }
  void set_packet_interval(uint32_t interval) { this->packet_interval_ = interval; }
  void set_packet_count(uint8_t count) { this->packet_count_ = count; }
  void set_counter(uint16_t counter) { this->counter_ = counter; }

  // 觸發器管理
  void add_before_send_trigger(HiFlyingLightBeforeSendTrigger *trigger) {
    this->before_send_triggers_.push_back(trigger);
  }
  void add_after_send_trigger(HiFlyingLightAfterSendTrigger *trigger) {
    this->after_send_triggers_.push_back(trigger);
  }

  // 控制方法
  void send_command(HiFlyingCommand command, uint16_t param = 0);
  void pair();
  void turn_on();
  void turn_off();
  void set_brightness(uint16_t brightness);
  void set_color_temperature(uint16_t color_temp);

  // 獲取設備 MAC 地址
  std::array<uint8_t, 6> get_device_mac();

 protected:
  uint8_t instance_id_{1};
  uint32_t packet_interval_{10};  // milliseconds
  uint8_t packet_count_{3};
  uint16_t counter_{1};

  std::vector<HiFlyingLightBeforeSendTrigger *> before_send_triggers_;
  std::vector<HiFlyingLightAfterSendTrigger *> after_send_triggers_;
  
  ESPPreferenceObject pref_;

  // 加密相關
  std::array<uint8_t, 16> get_encryption_table_();
  void apply_encryption_(std::vector<uint8_t> &data, size_t start, size_t length, uint8_t key);
  std::array<uint8_t, 8> tea_encrypt_(const std::array<uint8_t, 8> &data);
  uint16_t calculate_crc16_(const std::vector<uint8_t> &data, size_t start, size_t length, uint16_t initial = 0);
  uint8_t reverse_bits_(uint8_t byte_val);

  // 封包生成
  std::vector<uint8_t> generate_hf_packet_(const std::array<uint8_t, 5> &mac, uint8_t page, uint16_t counter,
                                          int8_t ctrl_code, const std::array<uint8_t, 3> &params);
  std::vector<uint8_t> generate_deli16_packet_(const std::array<uint8_t, 5> &mac, uint8_t page, uint16_t counter,
                                              int8_t ctrl_code, const std::array<uint8_t, 3> &params);
  void apply_bit_operation_(std::vector<uint8_t> &data, size_t length, uint8_t key);

  // 發送封包
  void send_packets_(const std::vector<uint8_t> &hf_packet, const std::vector<uint8_t> &deli16_packet);

  // 命令映射
  static const std::map<HiFlyingCommand, CommandInfo> command_map_;
};

class HiFlyingLightOutput : public light::LightOutput {
 public:
  void set_parent(HiFlyingLightComponent *parent) { this->parent_ = parent; }
  void set_color_temperature_support(bool support) { this->color_temperature_support_ = support; }

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

 protected:
  HiFlyingLightComponent *parent_{nullptr};
  light::LightState *state_{nullptr};
  bool color_temperature_support_{false};
  float last_brightness_{0.0f};
  float last_color_temp_{0.5f};
};

class HiFlyingLightPairButton : public button::Button {
 public:
  void set_parent(HiFlyingLightComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HiFlyingLightComponent *parent_{nullptr};
};

}  // namespace hiflying_light
}  // namespace esphome 