// ===== agro.h — shared declarations for the ESP32 PlatformIO build =====
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <EEPROM.h>
#include <time.h>
#include "config.h"

// ---- EEPROM-backed telemetry record (18 bytes packed) ----
struct TelemetryRecord {
  float    temp;        // °C
  uint8_t  hum;         // 0-100 %
  uint8_t  moist;       // 0-100 % (instantaneous)
  uint8_t  light;       // 0-100 %
  uint8_t  rain;        // 0 or 1
  uint8_t  score;       // 0-100
  uint8_t  valve;       // 0 or 1
  uint8_t  mode;        // 0=AUTO 1=MANUAL
  uint8_t  pMoist;      // score component, 0-255
  int8_t   pRain;       // score component, -128..127
  int8_t   pTime;       // score component, -128..127
  unsigned long capturedMillis;  // millis() at capture time, for age_ms in JSON
} __attribute__((packed));

// ---- shared globals (defined in main.cpp) ----
extern WiFiClient        espClient;
extern PubSubClient      mqtt;
extern Adafruit_SSD1306  oled;
extern DHT               dht;
extern Servo             valveServo;

extern float  temp, hum, moist, light;
extern float  moistInst;
extern bool   rain, rainSoon, valveOpen;
extern String selfMode;
extern int    score;
extern float  pMoist, pRain, pTime;
extern String partOfDay;
extern int    thrMoist, thrTemp;
extern bool   sensorFault;

// ---- sensors.cpp ----
float movingAvg(float *buf);
void  readSensors();
void  oledInit();
void  oledShow();

// ---- logic.cpp ----
int   currentHour();
void  updateThresholds();
void  computeScore();
void  decideValve();
void  setValve(bool open);

// ---- storage.cpp ----
void eeInit();
void eePush(const TelemetryRecord& r);
bool eePop (TelemetryRecord& r);
bool eePeek(TelemetryRecord& r);
void eeConfirmPop();
int  eeCount();

// ---- comms.cpp ----
void   publishJSON(const char* topic, const String& json);  // lora_gw only
void   wifiConnect();
void   wifiStatusPrint();
bool   mqttConnect();
void   saveTelemetry();
bool   publishRecord(const TelemetryRecord& r);
void   publishAlert(const char* level, const char* node, const char* msgtxt);
String jsonStr(const String& s, const String& key);
void   onMqtt(char* topic, byte* payload, unsigned int len);

// ---- lora_gw.cpp ----
void   loraInit();
int    findNodeByUid(const String& uid);
int    findNodeByField(const String& f);
String pktField(const String& s, int idx);
String kv(const String& payload, const String& key);
void   loraReceive();
void   announceUnregistered(const String& uid, int rssi);
void   bindNode(const String& uid, const String& name);
void   loraSendCommand(const String& field, const String& cmd);
int    remoteScore(float m);
void   forwardNodeData(int n, const String& payload);
void   checkNodeTimeouts();
void   publishRegistry();
