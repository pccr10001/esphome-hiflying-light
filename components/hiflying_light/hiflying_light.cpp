#include "hiflying_light.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble/ble.h"

#ifdef USE_ESP32
#include <esp_gap_ble_api.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_random.h>
#endif

namespace esphome {
namespace hiflying_light {

static const char *const TAG = "hiflying_light";

// 命令映射表
const std::map<HiFlyingCommand, CommandInfo> HiFlyingLightComponent::command_map_ = {
    {COMMAND_PAIR, {1, -76}},
    {COMMAND_OFF, {2, -78}},
    {COMMAND_ON, {3, -77}},
    {COMMAND_BRIGHTNESS, {12, -75}},
    {COMMAND_COLOR_TEMP, {11, -73}}
};

void HiFlyingLightComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HiFlying Light...");
  
  // 初始化 preferences 用於保存 counter (每個 instance_id 有獨立的存儲)
  uint32_t preference_hash = hash_djb2("hiflying_light") ^ (uint32_t(this->instance_id_) << 16);
  this->pref_ = global_preferences->make_preference<uint16_t>(preference_hash);
  uint16_t saved_counter;
  if (this->pref_.load(&saved_counter)) {
    this->counter_ = saved_counter;
    ESP_LOGD(TAG, "Loaded counter from preferences for instance %d: %d", this->instance_id_, this->counter_);
  } else {
    ESP_LOGD(TAG, "No saved counter found for instance %d, using initial value: %d", 
             this->instance_id_, this->counter_);
  }
  
  // 初始化藍芽
#ifdef USE_ESP32
  if (!esp32_ble::global_ble->is_active()) {
    ESP_LOGE(TAG, "BLE not active, cannot setup HiFlying Light");
    this->mark_failed();
    return;
  }
#endif
}

void HiFlyingLightComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HiFlying Light:");
  ESP_LOGCONFIG(TAG, "  Instance ID: %d", this->instance_id_);
  ESP_LOGCONFIG(TAG, "  Packet Interval: %d ms", this->packet_interval_);
  ESP_LOGCONFIG(TAG, "  Packet Count: %d", this->packet_count_);
  ESP_LOGCONFIG(TAG, "  Counter: %d", this->counter_);
  
  auto mac = this->get_device_mac();
  ESP_LOGCONFIG(TAG, "  Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

std::array<uint8_t, 6> HiFlyingLightComponent::get_device_mac() {
  uint8_t base_mac[6];
#ifdef USE_ESP32
  esp_wifi_get_mac(WIFI_IF_STA, base_mac);
#endif
  
  std::array<uint8_t, 6> device_mac;
  // 使用 ESP32 MAC 的前 4 字節
  device_mac[0] = base_mac[0];
  device_mac[1] = base_mac[1]; 
  device_mac[2] = base_mac[2];
  device_mac[3] = base_mac[3];
  // 最後兩字節用 instance_id 計算
  device_mac[4] = base_mac[4];
  device_mac[5] = base_mac[5] + this->instance_id_ - 1;
  
  return device_mac;
}

void HiFlyingLightComponent::send_command(HiFlyingCommand command, uint16_t param) {
  auto it = command_map_.find(command);
  if (it == command_map_.end()) {
    ESP_LOGE(TAG, "Unknown command: %d", command);
    return;
  }

  const CommandInfo &cmd_info = it->second;
  
  // 準備參數
  std::array<uint8_t, 3> params = {0, 0, 0};
  if (command == COMMAND_BRIGHTNESS || command == COMMAND_COLOR_TEMP) {
    if (param > 0x3e8) param = 0x3e8;
    if (param < 1) param = 1;
    params[1] = (param >> 8) & 0xff;
    params[2] = param & 0xff;
  }

  // 獲取設備 MAC (只取前 5 字節給協議使用)
  auto device_mac = this->get_device_mac();
  std::array<uint8_t, 5> mac_5;
  std::copy(device_mac.begin(), device_mac.begin() + 5, mac_5.begin());

  // 生成封包
  auto hf_packet = this->generate_hf_packet_(mac_5, 3, this->counter_, cmd_info.ctrl_code, params);
  auto deli16_packet = this->generate_deli16_packet_(mac_5, 3, this->counter_, cmd_info.ctrl_code, params);

  // 發送封包
  this->send_packets_(hf_packet, deli16_packet);

  // 遞增計數器並保存
  this->counter_++;
  this->pref_.save(&this->counter_);

  ESP_LOGD(TAG, "Instance %d sent command %d with param %d (counter: %d)", 
            this->instance_id_, command, param, this->counter_ - 1);
}

void HiFlyingLightComponent::pair() {
  this->send_command(COMMAND_PAIR);
}

void HiFlyingLightComponent::turn_on() {
  this->send_command(COMMAND_ON);
}

void HiFlyingLightComponent::turn_off() {
  this->send_command(COMMAND_OFF);
}

void HiFlyingLightComponent::set_brightness(uint16_t brightness) {
  this->send_command(COMMAND_BRIGHTNESS, brightness);
}

void HiFlyingLightComponent::set_color_temperature(uint16_t color_temp) {
  this->send_command(COMMAND_COLOR_TEMP, color_temp);
}

// TEA 加密實現
std::array<uint8_t, 8> HiFlyingLightComponent::tea_encrypt_(const std::array<uint8_t, 8> &data) {
  const char key[] = "!hIflIngCypcal@#";
  
  uint32_t v0 = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  uint32_t v1 = (data[4]) | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
  
  uint32_t k0 = (key[0]) | (key[1] << 8) | (key[2] << 16) | (key[3] << 24);
  uint32_t k1 = (key[4]) | (key[5] << 8) | (key[6] << 16) | (key[7] << 24);
  uint32_t k2 = (key[8]) | (key[9] << 8) | (key[10] << 16) | (key[11] << 24);
  uint32_t k3 = (key[12]) | (key[13] << 8) | (key[14] << 16) | (key[15] << 24);
  
  uint32_t delta = 0x9e3779b9;
  uint32_t sum_val = 0xc6ef3720;
  
  for (int i = 0; i < 32; i++) {
    v1 = (v1 - (((v0 << 4) + k2) ^ (v0 + sum_val) ^ ((v0 >> 5) + k3))) & 0xffffffff;
    v0 = (v0 - (((v1 << 4) + k0) ^ (v1 + sum_val) ^ ((v1 >> 5) + k1))) & 0xffffffff;
    sum_val = (sum_val - delta) & 0xffffffff;
  }
  
  std::array<uint8_t, 8> result;
  result[0] = v0 & 0xff;
  result[1] = (v0 >> 8) & 0xff;
  result[2] = (v0 >> 16) & 0xff;
  result[3] = (v0 >> 24) & 0xff;
  result[4] = v1 & 0xff;
  result[5] = (v1 >> 8) & 0xff;
  result[6] = (v1 >> 16) & 0xff;
  result[7] = (v1 >> 24) & 0xff;
  
  return result;
}

// 獲取加密表
std::array<uint8_t, 16> HiFlyingLightComponent::get_encryption_table_() {
  const std::array<uint8_t, 16> base_key = {
    0x52, 0xea, 0x73, 0xff, 0x49, 0x60, 0xbf, 0x56,
    0x42, 0x05, 0x07, 0xe8, 0xd3, 0xa7, 0xb9, 0x9d
  };
  
  std::array<uint8_t, 16> table;
  
  std::array<uint8_t, 8> part1;
  std::copy(base_key.begin(), base_key.begin() + 8, part1.begin());
  auto encrypted1 = this->tea_encrypt_(part1);
  std::copy(encrypted1.begin(), encrypted1.end(), table.begin());
  
  std::array<uint8_t, 8> part2;
  std::copy(base_key.begin() + 8, base_key.end(), part2.begin());
  auto encrypted2 = this->tea_encrypt_(part2);
  std::copy(encrypted2.begin(), encrypted2.end(), table.begin() + 8);
  
  return table;
}

// 應用加密
void HiFlyingLightComponent::apply_encryption_(std::vector<uint8_t> &data, size_t start, size_t length, uint8_t key) {
  auto enc_table = this->get_encryption_table_();
  
  if (start + 1 < data.size()) {
    uint8_t b = data[start + 1];
    uint8_t i = b & 0x0f;
    b = enc_table[((b >> 4) & 0x0f) ^ i];
    
    for (size_t j = 0; j < length; j++) {
      if (start + j < data.size()) {
        size_t pos = start + j;
        uint8_t i2 = data[pos] ^ b;
        uint8_t i3 = (j + key) & 0x0f;
        data[pos] = ((i2 & 0xff) + enc_table[i3 & 0xff]) & 0xff;
      }
    }
  }
}

// CRC16 計算
uint16_t HiFlyingLightComponent::calculate_crc16_(const std::vector<uint8_t> &data, size_t start, size_t length, uint16_t initial) {
  uint16_t crc = initial;
  uint16_t poly = 0x1021;
  
  for (size_t i = 0; i < length && (start + i) < data.size(); i++) {
    uint8_t byte_val = data[start + i];
    crc ^= (byte_val << 8);
    
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = ((crc << 1) ^ poly) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }
  
  return crc;
}

// 反轉位
uint8_t HiFlyingLightComponent::reverse_bits_(uint8_t byte_val) {
  uint8_t result = 0;
  for (int i = 0; i < 8; i++) {
    if (byte_val & (1 << i)) {
      result |= (1 << (7 - i));
    }
  }
  return result;
}

// 生成 HF 格式封包
std::vector<uint8_t> HiFlyingLightComponent::generate_hf_packet_(const std::array<uint8_t, 5> &mac, uint8_t page, uint16_t counter,
                                                               int8_t ctrl_code, const std::array<uint8_t, 3> &params) {
  std::vector<uint8_t> packet(16);
  
  packet[0] = 0xff;
  packet[1] = esp_random() & 0xff;  // 隨機值
  packet[2] = counter & 0xff;
  packet[3] = mac[0];
  packet[4] = mac[1] & 0xf0;  // 清除低 4 位
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = ctrl_code & 0xff;
  packet[8] = page & 0xff;
  packet[9] = 0xff;
  packet[10] = counter & 0xff;
  packet[11] = params[0];
  packet[12] = params[1];
  packet[13] = params[2];
  
  // 特殊處理
  if (ctrl_code != -76) {
    this->apply_encryption_(packet, 9, 5, 0xaa);
  } else {
    packet[11] = 0xAA;
    packet[12] = 0x66;
    packet[13] = 0x55;
  }
  
  // 計算 CRC16
  uint16_t crc = this->calculate_crc16_(packet, 0, 13, 0);
  packet[14] = crc & 0xff;
  packet[15] = (crc >> 8) & 0xff;
  
  // 最終加密
  this->apply_encryption_(packet, 0, 16, 86);
  
  // 添加前綴和後綴
  std::vector<uint8_t> final_packet(26);
  final_packet[0] = 'H';
  final_packet[1] = 'F';
  final_packet[2] = 'K';
  final_packet[3] = 'J';
  std::copy(packet.begin(), packet.end(), final_packet.begin() + 4);
  final_packet[20] = 0x10;
  final_packet[21] = 0x11;
  final_packet[22] = 0x12;
  final_packet[23] = 0x13;
  final_packet[24] = 0x14;
  final_packet[25] = 0x15;
  
  return final_packet;
}

// 位操作算法
void HiFlyingLightComponent::apply_bit_operation_(std::vector<uint8_t> &data, size_t length, uint8_t key) {
  uint8_t processed_key = ((key & 0x02) << 4) | (((((((key & 0x01) << 6) | 
                         ((key & 0x20) >> 4)) | 1) | ((key & 0x10) >> 2)) | 
                         (key & 0x08)) | ((key & 0x04) << 2)) & 0xff;
  
  for (size_t i = 0; i < length && i < data.size(); i++) {
    uint8_t new_byte = 0;
    for (int bit = 0; bit < 8; bit++) {
      processed_key &= 0xff;
      uint8_t bit_val = (processed_key & 0x40) >> 6;
      bit_val = bit_val << bit;
      bit_val ^= (data[i] & 0xff);
      bit_val &= (1 << bit);
      new_byte |= bit_val;
      
      processed_key <<= 1;
      uint8_t carry = (processed_key >> 7) & 1;
      processed_key &= 0xfe;
      processed_key |= carry;
      
      uint8_t xor_val = carry << 4;
      xor_val ^= processed_key;
      xor_val &= 0x10;
      processed_key &= 0xef;
      processed_key |= xor_val;
    }
    data[i] = new_byte;
  }
}

// 生成 Deli16 格式封包
std::vector<uint8_t> HiFlyingLightComponent::generate_deli16_packet_(const std::array<uint8_t, 5> &mac, uint8_t page, uint16_t counter,
                                                                   int8_t ctrl_code, const std::array<uint8_t, 3> &params) {
  std::vector<uint8_t> packet(26);
  
  // 構建數據
  std::vector<uint8_t> data(8);
  data[7] = (params[0] ^ counter) & 0xff;
  data[6] = (params[2] ^ mac[0]) & 0xff;
  data[5] = ((mac[1] ^ params[2]) ^ counter) & 0xff;
  
  uint8_t temp = params[2] ^ counter;
  data[4] = (temp ^ ctrl_code) & 0xff;
  data[3] = (temp ^ params[1]) & 0xff;
  data[2] = (page ^ temp) & 0xff;
  data[1] = (temp ^ params[0]) & 0xff;
  data[0] = (temp ^ mac[0]) & 0xff;
  
  // CRC 計算
  std::vector<uint8_t> crc_prefix = {0xcc, 0x55, 0xaa};
  uint16_t crc = 0xffff;
  
  for (uint8_t b : crc_prefix) {
    crc ^= (b << 8);
    for (int i = 0; i < 8; i++) {
      if (crc & 0x8000) {
        crc = ((crc << 1) ^ 0x1021) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }
  
  for (uint8_t b : data) {
    uint8_t reversed_b = this->reverse_bits_(b);
    crc ^= (reversed_b << 8);
    for (int i = 0; i < 8; i++) {
      if (crc & 0x8000) {
        crc = ((crc << 1) ^ 0x1021) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }
  
  uint16_t final_crc = crc ^ 0xffff;
  final_crc = ((this->reverse_bits_(final_crc & 0xff) << 8) | 
               this->reverse_bits_((final_crc >> 8) & 0xff)) & 0xffff;
  
  // 構建緩衝區
  std::vector<uint8_t> temp_buffer(29);
  temp_buffer[13] = 0x71;
  temp_buffer[14] = 0x0f;
  temp_buffer[15] = 0x55;
  std::copy(crc_prefix.begin(), crc_prefix.end(), temp_buffer.begin() + 16);
  std::copy(data.begin(), data.end(), temp_buffer.begin() + 19);
  
  for (int i = 13; i < 19; i++) {
    temp_buffer[i] = this->reverse_bits_(temp_buffer[i]);
  }
  
  temp_buffer[27] = final_crc & 0xff;
  temp_buffer[28] = (final_crc >> 8) & 0xff;
  
  // 位操作處理
  std::vector<uint8_t> mid_buffer(13);
  std::copy(temp_buffer.begin() + 16, temp_buffer.begin() + 29, mid_buffer.begin());
  this->apply_bit_operation_(mid_buffer, 13, 63);
  std::copy(mid_buffer.begin(), mid_buffer.end(), temp_buffer.begin() + 16);
  
  this->apply_bit_operation_(temp_buffer, 29, 37);
  
  // 複製最終數據
  std::copy(temp_buffer.begin() + 13, temp_buffer.begin() + 29, packet.begin());
  
  // 添加尾部序列
  for (int i = 0; i < 10; i++) {
    packet[16 + i] = (16 + i) & 0xff;
  }
  
  return packet;
}

// 發送封包
void HiFlyingLightComponent::send_packets_(const std::vector<uint8_t> &hf_packet, const std::vector<uint8_t> &deli16_packet) {
#ifdef USE_ESP32
  if (!esp32_ble::global_ble->is_active()) {
    ESP_LOGE(TAG, "BLE not active, cannot send packets");
    return;
  }

  esp_ble_adv_params_t adv_params = {};
  adv_params.adv_int_min = 0x20;
  adv_params.adv_int_max = 0x40;
  adv_params.adv_type = ADV_TYPE_NONCONN_IND;
  adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  adv_params.channel_map = ADV_CHNL_ALL;
  adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  for (int count = 0; count < this->packet_count_; count++) {
    // 發送 HF 封包
    esp_ble_gap_config_adv_data_raw(const_cast<uint8_t*>(hf_packet.data()), hf_packet.size());
    esp_ble_gap_start_advertising(&adv_params);
    delay(this->packet_interval_);
    esp_ble_gap_stop_advertising();
    
    // 發送 Deli16 封包
    esp_ble_gap_config_adv_data_raw(const_cast<uint8_t*>(deli16_packet.data()), deli16_packet.size());
    esp_ble_gap_start_advertising(&adv_params);
    delay(this->packet_interval_);
    esp_ble_gap_stop_advertising();
  }
#endif

  ESP_LOGD(TAG, "Sent packets (count: %d, interval: %d ms)", this->packet_count_, this->packet_interval_);
}

// HiFlyingLightOutput 實現
light::LightTraits HiFlyingLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  
  if (this->color_temperature_support_) {
    traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(153.0f);  // 6500K (冷光)
    traits.set_max_mireds(370.0f);  // 2700K (暖光)
  }
  
  return traits;
}

void HiFlyingLightOutput::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set");
    return;
  }

  float brightness;
  state->current_values_as_brightness(&brightness);
  
  // 檢查燈是否需要開關
  if (brightness == 0.0f && this->last_brightness_ > 0.0f) {
    // 關燈
    this->parent_->turn_off();
    this->last_brightness_ = brightness;
    return;
  } else if (brightness > 0.0f && this->last_brightness_ == 0.0f) {
    // 開燈
    this->parent_->turn_on();
  }

  // 亮度控制 (將 0.0-1.0 映射到 1-1000)
  if (brightness > 0.0f && abs(brightness - this->last_brightness_) > 0.01f) {
    uint16_t brightness_value = static_cast<uint16_t>(brightness * 999.0f) + 1;
    this->parent_->set_brightness(brightness_value);
    this->last_brightness_ = brightness;
  }

  // 色溫控制
  if (this->color_temperature_support_) {
    auto current_values = state->current_values;
    if (current_values.get_color_mode() == light::ColorMode::COLOR_TEMPERATURE) {
      float mired = current_values.get_color_temperature();
      if (abs(mired - this->last_color_temp_) > 1.0f) {
        // 將 mired 值轉換為 1-1000 範圍
        float normalized = (mired - 153.0f) / (370.0f - 153.0f);  // 正規化到 0-1
        normalized = 1.0f - normalized;  // 反轉 (低mired=冷光=高數值)
        uint16_t color_temp_value = static_cast<uint16_t>(normalized * 999.0f) + 1;
        this->parent_->set_color_temperature(color_temp_value);
        this->last_color_temp_ = mired;
      }
    }
  }
}

// HiFlyingLightPairButton 實現
void HiFlyingLightPairButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set for pair button");
    return;
  }
  
  ESP_LOGI(TAG, "Pair button pressed");
  this->parent_->pair();
}

}  // namespace hiflying_light
}  // namespace esphome 