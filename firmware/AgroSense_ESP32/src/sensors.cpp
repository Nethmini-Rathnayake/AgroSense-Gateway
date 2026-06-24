#include "agro.h"
/* ===== sensors.ino — A1 (read/normalize), A3 (moving average), A12 (faults), A2 (OLED) ===== */

// moving-average ring buffers (A3)
float maSoil[MA_N], maTemp[MA_N];
int   maIdx=0; bool maFilled=false;

float movingAvg(float *buf){
  int n = maFilled ? MA_N : (maIdx==0?1:maIdx);
  float s=0; for(int i=0;i<n;i++) s+=buf[i];
  return s/n;
}

// fault detection history (A12)
float lastSoil=-999; int stuckCount=0;

void readSensors(){
  // --- raw ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int soilRaw = analogRead(PIN_SOIL);
  int rainRaw = analogRead(PIN_RAIN);
  int ldrRaw  = analogRead(PIN_LDR);

  // --- normalize to meaningful units (A1) ---
  float soilPct = map(constrain(soilRaw, SOIL_WET_ADC, SOIL_DRY_ADC),
                      SOIL_DRY_ADC, SOIL_WET_ADC, 0, 100);          // 0 dry .. 100 wet
  float lightPct= map(constrain(ldrRaw, LDR_DARK_ADC, LDR_BRIGHT_ADC),
                      LDR_DARK_ADC, LDR_BRIGHT_ADC, 0, 100);
  rainLevel = (rainRaw > RAIN_WET_ADC) ? 1 : 0;
  Serial.printf("[RAIN] raw=%d  -> %s  (threshold=%d)\n",
    rainRaw, rainLevel ? "RAIN" : "NONE", RAIN_WET_ADC);

  // --- fault detection (A12): out-of-range or stuck ---
  bool oor = isnan(t) || isnan(h) || t<-10 || t>70 || soilPct<0 || soilPct>100;
  if (fabs(soilPct - lastSoil) < 0.5) stuckCount++; else stuckCount=0;
  lastSoil = soilPct;
  bool stuck = (stuckCount >= 8);
  bool nowFault = oor || stuck;
  if (nowFault && !sensorFault)
     publishAlert("error", SELF_FIELD, oor ? "Sensor out-of-range" : "Sensor stuck");
  sensorFault = nowFault;

  if (!isnan(t)) temp = t;
  if (!isnan(h)) hum  = h;

  // --- moving average (A3) ---
  maSoil[maIdx]=soilPct; maTemp[maIdx]=temp;
  maIdx=(maIdx+1)%MA_N; if(maIdx==0) maFilled=true;
  moistInst = soilPct;
  moist = movingAvg(maSoil);
  light = lightPct;
}

// ---- OLED (A2) ----
void oledInit(){
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  delay(20);
  digitalWrite(OLED_RST, HIGH); delay(20);
  if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false)){ Serial.println("OLED fail"); return; }
  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1); oled.setCursor(0,0);
  oled.println("AgroSense V2"); oled.display();
}
void oledShow(){
  oled.clearDisplay(); oled.setCursor(0,0);
  oled.println("AgroSense V2");
  oled.println("------------");

  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  if (wifiOk) {
    oled.println("WiFi: Connected");
    oled.println(WiFi.localIP().toString().c_str());
  } else {
    oled.println("WiFi: Connecting...");
    oled.println(WIFI_SSID);
  }

  oled.print("MQTT: ");
  oled.print(mqtt.connected() ? "OK" : "Wait");
  oled.print("  Rain:");
  oled.println(rainLevel ? "Rain" : "None");

  char buf[22];
  snprintf(buf, sizeof(buf), "T:%.1fC  H:%.0f%%", temp, hum);
  oled.println(buf);
  snprintf(buf, sizeof(buf), "Soil:%.0f%%  L:%.0f%%", moist, light);
  oled.println(buf);
  if (sensorFault) oled.println("[SENSOR FAULT]");

  oled.display();
}