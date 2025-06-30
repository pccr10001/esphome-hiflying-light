// 備選廣播方法 - 如果主要方法不工作，可以嘗試這個

// 方法 1: 直接發送原始資料
void HiFlyingLightComponent::send_packets_raw_(const std::vector<uint8_t> &hf_packet, const std::vector<uint8_t> &deli16_packet) {
#ifdef USE_ESP32
  esp_ble_adv_params_t adv_params = {};
  adv_params.adv_int_min = 0x20;
  adv_params.adv_int_max = 0x40;
  adv_params.adv_type = ADV_TYPE_NONCONN_IND;
  adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  adv_params.channel_map = ADV_CHNL_ALL;
  adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  for (int count = 0; count < this->packet_count_; count++) {
    // 直接發送原始資料
    esp_ble_gap_config_adv_data_raw(const_cast<uint8_t*>(hf_packet.data()), hf_packet.size());
    esp_ble_gap_start_advertising(&adv_params);
    delay(this->packet_interval_);
    esp_ble_gap_stop_advertising();
    
    esp_ble_gap_config_adv_data_raw(const_cast<uint8_t*>(deli16_packet.data()), deli16_packet.size());
    esp_ble_gap_start_advertising(&adv_params);
    delay(this->packet_interval_);
    esp_ble_gap_stop_advertising();
  }
#endif
}

// 方法 2: 使用完整的 AD 結構
void HiFlyingLightComponent::send_packets_structured_(const std::vector<uint8_t> &hf_packet, const std::vector<uint8_t> &deli16_packet) {
#ifdef USE_ESP32
  esp_ble_adv_params_t adv_params = {};
  adv_params.adv_int_min = 0x20;
  adv_params.adv_int_max = 0x40;
  adv_params.adv_type = ADV_TYPE_NONCONN_IND;
  adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  adv_params.channel_map = ADV_CHNL_ALL;
  adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  for (int count = 0; count < this->packet_count_; count++) {
    // 構建 AD 結構 (包含公司 ID)
    std::vector<uint8_t> adv_data_hf;
    adv_data_hf.push_back(hf_packet.size() + 3);  // Length (data + company ID)
    adv_data_hf.push_back(0xFF);  // AD Type: Manufacturer Specific Data
    adv_data_hf.push_back(0x4C);  // Company ID low byte (example: Apple)
    adv_data_hf.push_back(0x00);  // Company ID high byte
    adv_data_hf.insert(adv_data_hf.end(), hf_packet.begin(), hf_packet.end());
    
    esp_ble_gap_config_adv_data_raw(adv_data_hf.data(), adv_data_hf.size());
    esp_ble_gap_start_advertising(&adv_params);
    delay(this->packet_interval_);
    esp_ble_gap_stop_advertising();
  }
#endif
} 