# ESPHome HiFlying Light Component

一個用於控制 HiFlying 藍芽智能燈具的 ESPHome 組件。此組件基於對 HiFlying 燈具通訊協議的逆向工程，能夠透過藍芽廣播發送控制命令。

## 功能特性

- ✅ 燈具開關控制
- ✅ 亮度調節 (0-1000)
- ✅ 色溫調節 (0-1000) 
- ✅ 配對功能
- ✅ 多燈具支援 (透過不同 instance_id)
- ✅ 可配置的封包發送參數
- ⚠️ 與 bluetooth_proxy 有衝突 (建議分開使用)

## 需求

- ESP32 開發板
- ESPHome 2023.x 或更新版本
- `esp32_ble` 組件

## 安裝

### 方法 1: 使用外部組件 (推薦)

在您的 ESPHome YAML 配置中添加：

```yaml
external_components:
  - source: github://your-username/esphome-hiflying-light
```

### 方法 2: 本地安裝

1. 將 `components/hiflying_light` 目錄複製到您的 ESPHome 配置目錄
2. 在 YAML 中引用本地組件

## 基本配置

```yaml
# 必需：啟用 BLE
esp32_ble:

# HiFlying Light 控制器
hiflying_light:
  id: light_controller
  instance_id: 1

# 燈光實體
light:
  - platform: hiflying_light
    hiflying_light_id: light_controller
    name: "智能燈"

# 配對按鈕
button:
  - platform: hiflying_light
    hiflying_light_id: light_controller
    name: "配對燈具"
```

## 配置選項

### hiflying_light 組件

| 參數 | 類型 | 預設值 | 描述 |
|------|------|--------|------|
| `id` | string | 必需 | 組件 ID |
| `instance_id` | int | 1 | 實例編號 (1-99)，用於生成不同的 MAC 地址 |
| `packet_interval` | time | 10ms | 封包發送間隔 |
| `packet_count` | int | 3 | 每次命令發送封包次數 |
| `counter` | int | 1 | 初始計數器值 |

### light 平台

| 參數 | 類型 | 預設值 | 描述 |
|------|------|--------|------|
| `hiflying_light_id` | id | 必需 | 關聯的 hiflying_light 組件 |
| `color_temperature` | bool | false | 是否支援色溫控制 |

### button 平台

| 參數 | 類型 | 預設值 | 描述 |
|------|------|--------|------|
| `hiflying_light_id` | id | 必需 | 關聯的 hiflying_light 組件 |

## 進階配置

### 多燈具控制

```yaml
hiflying_light:
  - id: light_controller_1
    instance_id: 1
  - id: light_controller_2
    instance_id: 2
  - id: light_controller_3
    instance_id: 3

light:
  - platform: hiflying_light
    hiflying_light_id: light_controller_1
    name: "客廳燈"
  - platform: hiflying_light
    hiflying_light_id: light_controller_2
    name: "臥室燈"
  - platform: hiflying_light
    hiflying_light_id: light_controller_3
    name: "廚房燈"
```

### 注意事項

由於 ESPHome 版本相容性問題，目前版本不支援自動化觸發器。如果需要與 `bluetooth_proxy` 同時使用，建議：

1. 使用兩個不同的 ESP32 設備
2. 或者在需要控制燈具時，暫時停用 `bluetooth_proxy`

### 色溫燈具配置

```yaml
light:
  - platform: hiflying_light
    hiflying_light_id: light_controller
    name: "色溫燈"
    color_temperature: true
```

## MAC 地址生成機制

組件會自動生成設備 MAC 地址：
- 基礎：ESP32 WiFi MAC 地址
- 修改：最後一個字節 + (instance_id - 1)
- 範例：
  - ESP32 MAC: `AA:BB:CC:DD:EE:FF`
  - instance_id=1: `AA:BB:CC:DD:EE:FF`
  - instance_id=2: `AA:BB:CC:DD:EE:00` (FF+1=00)
  - instance_id=3: `AA:BB:CC:DD:EE:01` (FF+2=01)

## 通訊協議

組件實現了 HiFlying 燈具的雙協議支援：

1. **HF 格式**: 26 字節封包，以 "HFKJ" 開頭
2. **Deli16 格式**: 26 字節封包，使用複雜的位操作加密

### 命令映射

| 功能 | cmd1 | ctrl_code |
|------|------|-----------|
| 配對 | 1 | -76 |
| 關燈 | 2 | -78 |
| 開燈 | 3 | -77 |
| 亮度調節 | 12 | -75 |
| 色溫調節 | 11 | -73 |

### 加密機制

- **TEA 算法**: 使用 "!hIflIngCypcal@#" 作為密鑰
- **加密表**: 基於固定密鑰生成的 16 字節表
- **CRC16**: CCITT 標準校驗

## 故障排除

### 藍芽衝突問題

如果遇到與 bluetooth_proxy 的衝突：

1. 調整 packet_interval 和 packet_count
2. 考慮使用獨立的 ESP32 設備

### 燈具不響應

1. 檢查 instance_id 是否正確
2. 確認燈具在配對模式
3. 檢查 ESP32 BLE 是否正常工作
4. 嘗試增加 packet_count

### 日誌除錯

啟用除錯日誌：

```yaml
logger:
  level: DEBUG
  logs:
    hiflying_light: DEBUG
```

## 限制

1. **藍芽衝突**: 與其他 BLE 服務 (如 bluetooth_proxy) 可能產生衝突
2. **範圍限制**: 藍芽廣播範圍約 10 公尺
3. **單向通訊**: 無法獲取燈具狀態回饋
4. **協議相容性**: 僅支援 HiFlying 品牌燈具
5. **ESPHome 版本**: 由於相容性問題，暫不支援自動化觸發器

## 貢獻

歡迎提交 Issue 和 Pull Request！

## 授權

MIT License

## 致謝

感謝對 HiFlying 協議進行逆向工程的貢獻者們。 #   e s p h o m e - h i f l y i n g - l i g h t 
 
 