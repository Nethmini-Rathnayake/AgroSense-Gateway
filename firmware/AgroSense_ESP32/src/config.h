// ===================== AgroSense ESP32 — config.h =====================
// Edit Wi-Fi, MQTT broker, and (if needed) pins to match your wiring.
#pragma once

// ---- Wi-Fi ----
#define WIFI_SSID      "Redmi Note 13"
#define WIFI_PASS      "Pasindu1112"

// ---- MQTT broker (the PC running Node-RED/Mosquitto) ----
#define MQTT_HOST "10.63.57.139"   // <-- your broker IP (run `ipconfig`/`hostname -I`)
#define MQTT_PORT      1883
#define MQTT_CLIENT    "agrosense-esp32"

// This ESP32 is also a field node. Its own field id:
#define SELF_FIELD     "field_1"
#define SELF_NAME      "Field A (ESP32)"

// ---- Pins (Heltec WiFi LoRa 32 V2). All analog pins are ADC1 (Wi-Fi safe). ----
#define PIN_DHT        17   // moved from 4 — GPIO4 is OLED SDA on Heltec V2
#define PIN_SOIL       33   // analog, input-only
#define PIN_RAIN       35   // analog (AO of rain board), input-only
#define PIN_LDR        32   // analog
#define PIN_SERVO      13
#define OLED_SDA       4    // Heltec V2 built-in OLED
#define OLED_SCL       15   // Heltec V2 built-in OLED
#define OLED_RST       16   // Heltec V2 built-in OLED

// ---- LoRa (Heltec WiFi LoRa 32 V2 — internal SX1276, do NOT wire) ----
#define LORA_SCK       5
#define LORA_MISO      19
#define LORA_MOSI      27
#define LORA_SS        18    // Heltec uses GPIO18 for CS (not 5)
#define LORA_RST       14
#define LORA_DIO0      26    // Heltec uses GPIO26 (not 2)
#define LORA_FREQ      915E6 // set to YOUR board's variant: 433E6 / 868E6 / 915E6

// ---- Behaviour ----
#define MA_N           3      // moving-average window (A3, configurable)
#define PUBLISH_MS     3000   // telemetry period
#define OLED_MS        1000
#define HEARTBEAT_TIMEOUT 15000  // node OFFLINE if silent this long (A13)
#define MAX_EE_RECORDS 150    // 5 min × 30 reads/min — EEPROM queue depth
#define EEPROM_SIZE    4096   // bytes allocated in flash
#define DHTTYPE        DHT11

// ---- Sensor calibration (tune to your probes) ----
#define SOIL_DRY_ADC   4095   // ADC reading in dry air
#define SOIL_WET_ADC   2100   // ADC reading in water
#define RAIN_WET_ADC   1500   // below this = raining
#define LDR_DARK_ADC   200
#define LDR_BRIGHT_ADC 3000

// ---- MQTT topics (must match the Node-RED dashboard) ----
#define T_TELEM_FMT    "agro7x9k/%s/telemetry"
#define T_REG_NODES    "agro7x9k/registry/nodes"
#define T_REG_UNREG    "agro7x9k/registry/unregistered"
#define T_ALERTS       "agro7x9k/alerts"
#define T_CMD_VALVE    "agro7x9k/cmd/valve"
#define T_CMD_MODE     "agro7x9k/cmd/mode"
#define T_CMD_THR      "agro7x9k/cmd/threshold"
#define T_CMD_BIND     "agro7x9k/cmd/bind"
#define T_CMD_WX       "agro7x9k/cmd/weather"
#define T_STATUS_GW    "agro7x9k/status/gateway"