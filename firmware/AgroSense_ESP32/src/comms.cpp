#include "agro.h"
/* ===== comms.cpp — WiFi/MQTT, EEPROM-queue telemetry (A9/A10/A11) ===== */

void wifiConnect(){
  static unsigned long lastTry=0;
  static bool started=false;
  if(WiFi.status()==WL_CONNECTED) return;
  // don't interrupt an ongoing association attempt (255 = not yet started, let it through)
  if(WiFi.status()==WL_IDLE_STATUS) return;
  if(millis()-lastTry < 10000) return;
  lastTry=millis();
  if(!started){
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);   // don't load/save old credentials from flash
    WiFi.disconnect(true);    // clear any stored network
    delay(100);
    started=true;
  }
  //Serial.printf("[WiFi] connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// called from loop() — prints once on state change
void wifiStatusPrint(){
  static wl_status_t last = WL_IDLE_STATUS;
  wl_status_t now = WiFi.status();
  if(now == last) return;
  last = now;
  // if(now == WL_CONNECTED)
  //   Serial.printf("[WiFi] connected  IP=%s\n", WiFi.localIP().toString().c_str());
  // else if(now == WL_NO_SSID_AVAIL)
  //   Serial.printf("[WiFi] SSID '%s' not found\n", WIFI_SSID);
  // else if(now == WL_CONNECT_FAILED)
  //   Serial.println("[WiFi] connect failed (wrong password?)");
  // else
  //   Serial.printf("[WiFi] status=%d\n", (int)now);
}

bool mqttConnect(){
  static unsigned long lastTry=0;
  if(WiFi.status()!=WL_CONNECTED || mqtt.connected()) return false;
  if(millis()-lastTry < 2000) return false;
  lastTry=millis();
  mqtt.setSocketTimeout(2);
  mqtt.setKeepAlive(10);
  String cid = String(MQTT_CLIENT) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  //Serial.printf("[MQTT] connecting to %s:%d ...\n", MQTT_HOST, MQTT_PORT);
  if(mqtt.connect(cid.c_str(), NULL, NULL, T_STATUS_GW, 1, true, "OFFLINE")){
    mqtt.publish(T_STATUS_GW, "ONLINE", true);
    mqtt.subscribe(T_CMD_VALVE); mqtt.subscribe(T_CMD_MODE);
    mqtt.subscribe(T_CMD_THR);   mqtt.subscribe(T_CMD_BIND);
    mqtt.subscribe(T_CMD_WX);
    //Serial.printf("[MQTT] connected — %d record(s) queued\n", eeCount());
    return true;
  }
  // Serial.printf("[MQTT] failed  rc=%d  (%s)\n", mqtt.state(),
  //   mqtt.state()==-4 ? "timeout/broker too slow" :
  //   mqtt.state()==-3 ? "connection lost" :
  //   mqtt.state()==-2 ? "broker unreachable (firewall/IP?)" :
  //   mqtt.state()==-1 ? "disconnected" :
  //   mqtt.state()== 1 ? "bad protocol" :
  //   mqtt.state()== 2 ? "bad client ID" :
  //   mqtt.state()== 3 ? "broker unavailable" :
  //   mqtt.state()== 4 ? "bad credentials" :
  //   mqtt.state()== 5 ? "not authorised" : "unknown");
  return false;
}

// Pack current globals into an EEPROM record — called every 2 s regardless of connectivity
void saveTelemetry(){
  TelemetryRecord r;
  r.temp   = temp;
  r.hum    = (uint8_t)constrain((int)hum,      0, 255);
  r.moist  = (uint8_t)constrain((int)moistInst, 0, 100);
  r.light  = (uint8_t)constrain((int)light,     0, 100);
  r.rain   = rain   ? 1 : 0;
  r.score  = (uint8_t)constrain(score,          0, 100);
  r.valve  = valveOpen ? 1 : 0;
  r.mode   = (selfMode == "MANUAL") ? 1 : 0;
  r.pMoist = (uint8_t)constrain((int)pMoist,    0, 255);
  r.pRain  = (int8_t) constrain((int)pRain,  -128, 127);
  r.pTime  = (int8_t) constrain((int)pTime,  -128, 127);
  //Serial.printf("[SAVE] moist=%d  temp=%.1f  hum=%d  score=%d\n",
  //              r.moist, r.temp, r.hum, r.score);
  r.capturedMillis = millis();
  eePush(r);
}

// Reconstruct JSON from a stored record and publish — called by drain loop in main
bool publishRecord(const TelemetryRecord& r){
  if (!mqtt.connected()) {
    Serial.println("[PUB] skip — not connected");
    return false;
  }
  char topic[48]; snprintf(topic, sizeof(topic), T_TELEM_FMT, SELF_FIELD);
  String j = "{";
  j += "\"uid\":\"ESP32-SELF\",";
  j += "\"name\":\""+String(SELF_NAME)+"\",";
  j += "\"temp\":"+String(r.temp, 1)+",";
  j += "\"hum\":"+String(r.hum)+",";
  j += "\"moist\":"+String(r.moist)+",";
  j += "\"light\":"+String(r.light)+",";
  j += "\"rain\":"+String(r.rain ? "true" : "false")+",";
  j += "\"valve\":\""+String(r.valve ? "ON" : "OFF")+"\",";
  j += "\"mode\":\""+String(r.mode ? "MANUAL" : "AUTO")+"\",";
  j += "\"score\":"+String(r.score)+",";
  j += "\"scoreparts\":{\"moisture\":"+String(r.pMoist)+",\"rain\":"+String(r.pRain)+",\"time\":"+String(r.pTime)+"}";
  unsigned long age_ms = millis() - r.capturedMillis;
  j += ",\"age_ms\":" + String(age_ms) + "}";
  //Serial.printf("[SEND] moist=%d  temp=%.1f  hum=%d  score=%d  remaining=%d\n",
  //              r.moist, r.temp, r.hum, r.score, eeCount());
  //Serial.print("[JSON] "); Serial.println(j);
  bool ok = mqtt.publish(topic, j.c_str());
  if (ok) {
    //Serial.println("[PUB] ok");
  } else {
    //Serial.printf("[PUB] FAILED  state=%d\n", mqtt.state());
  }
  return ok;
}

// Used by lora_gw.cpp for remote node telemetry (arbitrary topic/JSON, no EEPROM buffering)
void publishJSON(const char* topic, const String& json){
  if(mqtt.connected()) mqtt.publish(topic, json.c_str());
}

void publishAlert(const char* level, const char* node, const char* msgtxt){
  if(!mqtt.connected()) return;
  String j="{\"level\":\""+String(level)+"\",\"node\":\""+String(node)+"\",\"msg\":\""+String(msgtxt)+"\"}";
  mqtt.publish(T_ALERTS, j.c_str());
}

// ---- command handler ----
String jsonStr(const String& s, const String& key){
  int k=s.indexOf("\""+key+"\""); if(k<0) return "";
  int c=s.indexOf(':',k); int q1=s.indexOf('"',c+1);
  if(q1<0) return "";
  int q2=s.indexOf('"',q1+1); return s.substring(q1+1,q2);
}

void onMqtt(char* topic, byte* payload, unsigned int len){
  String t=topic, p; for(unsigned i=0;i<len;i++) p+=(char)payload[i];
  String field=jsonStr(p,"field");

  if(t==T_CMD_VALVE){
    String st=jsonStr(p,"state");
    if(field==SELF_FIELD){ selfMode="MANUAL"; setValve(st=="ON"); }
    else loraSendCommand(field, "valve="+st);
  }
  else if(t==T_CMD_MODE){
    String m=jsonStr(p,"mode");
    if(field==SELF_FIELD) selfMode=m;
    else loraSendCommand(field, "mode="+m);
  }
  else if(t==T_CMD_THR){
    int mi=p.indexOf("\"moist\""); if(mi>=0){ thrMoist=p.substring(p.indexOf(':',mi)+1).toInt(); }
  }
  else if(t==T_CMD_BIND){
    String uid=jsonStr(p,"uid"), name=jsonStr(p,"name");
    bindNode(uid, name);
    mqtt.publish(T_CMD_BIND, "", true);   // clear retained bind so it doesn't re-fire on reconnect
  }
  else if(t==T_CMD_WX){
    rainSoon = (p.indexOf("true")>=0);
  }
}
