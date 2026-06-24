#include "agro.h"
/* =====================================================================
   AgroSense V2 — ESP32 Gateway / Main Node   (Sections A, C, E, F)
   Modules (F5):  this file = orchestration
                  sensors.cpp = A1/A3/A12   (read, normalize, MA, faults)
                  logic.cpp   = A4-A8       (thresholds, scoring, valve)
                  comms.cpp   = A9-A11      (wifi, mqtt, save, publish)
                  storage.cpp = EEPROM queue (power-cut safe buffer)
                  lora_gw.cpp = A13-A15,C   (LoRa rx, registry, binding)
   Libraries: PubSubClient, LoRa (sandeepmistry), Adafruit_SSD1306,
              Adafruit_GFX, DHT sensor library, Adafruit Unified Sensor,
              ESP32Servo.
   ===================================================================== */

// ---- shared globals (visible to all .cpp files) ----
WiFiClient        espClient;
PubSubClient      mqtt(espClient);
Adafruit_SSD1306  oled(128, 64, &Wire, OLED_RST);
DHT               dht(PIN_DHT, DHTTYPE);
Servo             valveServo;

// live readings for the ESP32's own field
float  temp=0, hum=0, moist=0, light=0;
float  moistInst=0;
bool     rainSoon=false;
uint8_t  rainLevel=0;
bool   valveOpen=false;
String selfMode="AUTO";
int    score=0;
float  pMoist=0, pRain=0, pTime=0;
String partOfDay="day";

// adaptive thresholds (A4)
int    thrMoist=40, thrTemp=35;

// fault state (A12)
bool   sensorFault=false;

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(OLED_SDA, OLED_SCL);
  oledInit();

  dht.begin();

  // valveServo.attach(PIN_SERVO);
  // setValve(false);

  eeInit();   // restore EEPROM queue (must be before WiFi so queued count is known)

  wifiConnect();
  WiFi.setAutoReconnect(true);

  configTime(19800, 0, "pool.ntp.org");
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(512);

  loraInit();

  Serial.println("AgroSense ESP32 ready.");
}

unsigned long tSensor=0, tDrain=0, tOled=0, tReg=0;

void loop() {
  wifiStatusPrint();
  if (WiFi.status()!=WL_CONNECTED) wifiConnect();
  if (!mqtt.connected() && mqttConnect()) tDrain = millis(); // settle 500ms before first drain
  mqtt.loop();
  delay(10);

  loraReceive();

  unsigned long now = millis();

  // Always record sensor data every 2 s — even when offline
  if (now - tSensor >= PUBLISH_MS) {
    tSensor = now;
    readSensors();
    updateThresholds();
    computeScore();
    decideValve();
    saveTelemetry();   // pack into TelemetryRecord → eePush (EEPROM)
  }

  // Drain EEPROM queue to MQTT at 500 ms intervals when connected
  if (mqtt.connected() && eeCount() > 0 && now - tDrain >= 500UL) {
    tDrain = now;
    TelemetryRecord r;
    if (eePeek(r) && publishRecord(r)) {
      eeConfirmPop();
      mqtt.loop();  // let TCP stack flush before next iteration
    }
  }

  if (now - tOled > OLED_MS) { tOled=now; oledShow(); }
  if (now - tReg  > 5000)    { tReg=now;  checkNodeTimeouts(); publishRegistry(); }
}
