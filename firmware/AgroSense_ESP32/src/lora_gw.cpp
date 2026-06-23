#include "agro.h"
/* ===== lora_gw.ino — A14 (gateway), A15 (registry), A13 (offline), C1-C5, E1 (packet) =====
   LoRa packet (E1):  AG,<type>,<uid>,<id>,<payload>
     type: J=join  D=data  H=heartbeat  A=assign(gw->node)  C=command(gw->node)
     payload: key=val;key=val   (e.g. moist=42;temp=27.3;hum=61)                    */

struct Node { String uid; String field; String name; unsigned long lastSeen; bool online; };
Node   registry[8]; int regCount=0;
String unregSeen[8]; int unregCount=0;
int    nextFieldIdx=2;     // ESP32 itself is field_1; remote nodes start at field_2

void loraInit(){
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if(!LoRa.begin(LORA_FREQ)){ Serial.println("LoRa init FAILED"); return; }
  LoRa.setSyncWord(0x34);
  Serial.println("LoRa gateway up");
}

int findNodeByUid(const String& uid){ for(int i=0;i<regCount;i++) if(registry[i].uid==uid) return i; return -1; }
int findNodeByField(const String& f){ for(int i=0;i<regCount;i++) if(registry[i].field==f) return i; return -1; }

String pktField(const String& s,int idx){ // split by comma, return idx-th token
  int start=0,count=0;
  for(int i=0;i<=s.length();i++){
    if(i==s.length()||s[i]==','){
      if(count==idx) return s.substring(start,i);
      count++; start=i+1;
    }
  } return "";
}
String kv(const String& payload,const String& key){
  int k=payload.indexOf(key+"="); if(k<0) return "";
  int e=payload.indexOf(';',k); if(e<0) e=payload.length();
  return payload.substring(k+key.length()+1,e);
}

// void loraReceive(){
//   int sz=LoRa.parsePacket();
//   if(sz==0) return;
//   String s=""; while(LoRa.available()) s+=(char)LoRa.read();
//   Serial.print("[LoRa] RAW rx: "); Serial.println(s);   // print AFTER reading into s
//   int rssi=LoRa.packetRssi();
//   if(pktField(s,0)!="AG7X9K") return;

//   String type=pktField(s,1), uid=pktField(s,2), id=pktField(s,3), payload=pktField(s,4);

//   if(type=="J"){
//     Serial.print("[LoRa] node connected (JOIN) uid="); Serial.println(uid);                                  // C1: new-node detection
//     if(findNodeByUid(uid)<0) announceUnregistered(uid, rssi);
//   }
//   else if(type=="D" || type=="H"){
//     int n=findNodeByField(id); if(n<0){ n=findNodeByUid(uid); }
//     Serial.print(n);
//     if(n>=0){
//       registry[n].lastSeen=millis();
//       if(!registry[n].online){ registry[n].online=true; publishAlert("info",registry[n].field.c_str(),"Node back ONLINE"); }
//       if(type=="D") forwardNodeData(n, payload);
//     }
//   }
// }
void loraReceive(){
  int sz=LoRa.parsePacket();
  if(sz==0) return;
  String s=""; while(LoRa.available()) s+=(char)LoRa.read();
  
  int rssi=LoRa.packetRssi();
  if(pktField(s,0)!="AG7X9K") return;
  Serial.print("[LoRa] RAW rx: "); Serial.println(s);
  String type=pktField(s,1), uid=pktField(s,2), id=pktField(s,3), payload=pktField(s,4);

  if(type=="J"){
    Serial.print("[LoRa] node connected (JOIN) uid="); Serial.println(uid);
    if(findNodeByUid(uid)<0) announceUnregistered(uid, rssi);
  }
  else if(type=="D" || type=="H"){
    int n=findNodeByField(id); if(n<0){ n=findNodeByUid(uid); }
    
    if(n>=0){
      registry[n].lastSeen=millis();
      if(!registry[n].online){ registry[n].online=true; publishAlert("info",registry[n].field.c_str(),"Node back ONLINE"); }
      if(type=="D") forwardNodeData(n, payload);
    } else {
      // ---> NEW FIX: If n is -1 (unknown node), announce it so the user can bind it!
      Serial.println("[LoRa] Heard data from UNKNOWN node! Adding to Unregistered list...");
      announceUnregistered(uid, rssi);
    }
  }
}

void bindNode(const String& uid, const String& name){
  if(uid.length()==0 || uid=="ESP32-SELF") return;   

  // ---> NEW FIX: We removed the strict 'inUnreg' security trap so you can bind freely! <---

  int n=findNodeByUid(uid);
  if(n<0){
    n=regCount++; registry[n].uid=uid;
    registry[n].field="field_"+String(nextFieldIdx++);
  }
  
  registry[n].name=name; registry[n].online=true; registry[n].lastSeen=millis();
  
  // Clean up the unregistered list
  for(int i=0;i<unregCount;i++) if(unregSeen[i]==uid){ unregSeen[i]=unregSeen[--unregCount]; break; }
  
  String pkt="AG7X9K,A,"+uid+","+registry[n].field+",name="+name;
  LoRa.beginPacket(); LoRa.print(pkt); LoRa.endPacket();
  
  Serial.print("[LoRa] node binded "); Serial.print(uid);
  Serial.print(" -> "); Serial.println(registry[n].field);
  publishAlert("info", registry[n].field.c_str(), ("Bound "+uid+" as "+name).c_str());
  publishRegistry();
}
// void loraReceive(){
//   int sz=LoRa.parsePacket(); 
  
//   if(sz==0) return; // No packet, exit immediately
  
//   String s=""; while(LoRa.available()) s+=(char)LoRa.read();
//   int rssi=LoRa.packetRssi();
  
//   if(pktField(s,0)!="AG") return;
//   String type=pktField(s,1), uid=pktField(s,2), id=pktField(s,3), payload=pktField(s,4);

//   Serial.println("\n📡 [LORA RX] Packet Caught!");
//   Serial.print("   Raw Data: "); Serial.println(s);
//   Serial.print("   RSSI: "); Serial.println(rssi);

//   if(type=="J"){                                  // C1: new-node detection
//     if(findNodeByUid(uid)<0) announceUnregistered(uid, rssi);
//   }
//   else if(type=="D" || type=="H"){
//     int n=findNodeByField(id); if(n<0){ n=findNodeByUid(uid); }
//     if(n>=0){
//       registry[n].lastSeen=millis();
//       if(!registry[n].online){ registry[n].online=true; publishAlert("info",registry[n].field.c_str(),"Node back ONLINE"); }
//       if(type=="D") forwardNodeData(n, payload);
//     }
//   }

//   // ---> THE FIX: Force the LoRa chip back into Continuous Listening Mode <---
//   LoRa.receive(); 
// }

void announceUnregistered(const String& uid, int rssi){
  bool seen=false; for(int i=0;i<unregCount;i++) if(unregSeen[i]==uid) seen=true;
  if(!seen && unregCount<8) unregSeen[unregCount++]=uid;
  String j="{\"uid\":\""+uid+"\",\"rssi\":"+String(rssi)+"}";
  mqtt.publish(T_REG_UNREG, j.c_str());           // C2: dashboard shows UNREGISTERED
}

// C3/C4: assign a field id + name, push assignment to the node over LoRa
// void bindNode(const String& uid, const String& name){
//   if(uid.length()==0 || uid=="ESP32-SELF") return;   // never bind the gateway itself
//   // only accept UIDs that arrived via a real JOIN and are waiting for registration
//   bool inUnreg=false;
//   for(int i=0;i<unregCount;i++) if(unregSeen[i]==uid){ inUnreg=true; break; }
//   if(!inUnreg) return;
//   int n=findNodeByUid(uid);
//   if(n<0){
//     n=regCount++; registry[n].uid=uid;
//     registry[n].field="field_"+String(nextFieldIdx++);
//   }
//   registry[n].name=name; registry[n].online=true; registry[n].lastSeen=millis();
//   // remove from unregistered list
//   for(int i=0;i<unregCount;i++) if(unregSeen[i]==uid){ unregSeen[i]=unregSeen[--unregCount]; break; }
//   // tell the node its assigned id (A packet)
//   String pkt="AG7X9K,A,"+uid+","+registry[n].field+",name="+name;
//   LoRa.beginPacket(); LoRa.print(pkt); LoRa.endPacket();
//   Serial.print("[LoRa] node binded "); Serial.print(uid);
//   Serial.print(" -> "); Serial.println(registry[n].field);
//   publishAlert("info", registry[n].field.c_str(), ("Bound "+uid+" as "+name).c_str());
//   publishRegistry();
// }

void loraSendCommand(const String& field, const String& cmd){
  int n=findNodeByField(field); if(n<0) return;
  String pkt="AG7X9K,C,"+registry[n].uid+","+field+","+cmd;
  LoRa.beginPacket(); LoRa.print(pkt); LoRa.endPacket();
  Serial.print("[LoRa] cmd -> "); Serial.print(field); Serial.print(": "); Serial.println(cmd);
}

int remoteScore(float m){
  float pm=max(0.0f,(float)(thrMoist-m))*1.6f;
  float pr=(rainSoon)?-45.0f:0.0f;
  float pt=(partOfDay=="noon")?-15.0f:(partOfDay=="morning"?25.0f:5.0f);
  return (int)constrain(30+pm+pr+pt,0,100);
}

// build telemetry JSON for a remote field and forward to MQTT (A14)
void forwardNodeData(int n, const String& payload){
  float m=kv(payload,"moist").toFloat();
  float t=kv(payload,"temp").toFloat();
  float h=kv(payload,"hum").toFloat();
  int   sc=remoteScore(m);
  bool  open=(sc>=60 && !rainSoon);
  loraSendCommand(registry[n].field, String("valve=")+(open?"ON":"OFF"));  // command the node
  char topic[48]; snprintf(topic,sizeof(topic),T_TELEM_FMT,registry[n].field.c_str());
  String j="{";
  j+="\"uid\":\""+registry[n].uid+"\",";
  j+="\"name\":\""+registry[n].name+"\",";
  j+="\"temp\":"+String(t,1)+",\"hum\":"+String((int)h)+",\"moist\":"+String((int)m)+",";
  j+="\"light\":0,\"rain\":false,";
  j+="\"valve\":\""+String(open?"ON":"OFF")+"\",\"mode\":\"AUTO\",";
  j+="\"score\":"+String(sc)+",";
  j+="\"scoreparts\":{\"moisture\":"+String(max(0.0f,(float)(thrMoist-m))*1.6f,0)+",\"rain\":"+String(rainSoon?-45:0)+",\"time\":"+String(partOfDay=="noon"?-15:5)+"}";
  j+="}";
  Serial.print("[LoRa] data from "); Serial.print(registry[n].field);
  //Serial.print(" -> MQTT  moist="); Serial.println((int)m);
  publishJSON(topic, j);
}

// A13: mark nodes OFFLINE if heartbeat missing
void checkNodeTimeouts(){
  for(int i=0;i<regCount;i++){
    if(registry[i].online && millis()-registry[i].lastSeen > HEARTBEAT_TIMEOUT){
      registry[i].online=false;
      publishAlert("error", registry[i].field.c_str(), "Node OFFLINE (no heartbeat)");
    }
  }
}

// A15: publish full node registry (incl. this ESP32) for the dashboard
void publishRegistry(){
  String j="[";
  j+="{\"uid\":\"ESP32-SELF\",\"name\":\""+String(SELF_NAME)+"\",\"status\":\"ONLINE\",\"lastSeen\":\"now\"}";
  for(int i=0;i<regCount;i++){
    j+=",{\"uid\":\""+registry[i].uid+"\",\"name\":\""+registry[i].name+"\",";
    j+="\"status\":\""+String(registry[i].online?"ONLINE":"OFFLINE")+"\",\"lastSeen\":\""+String((millis()-registry[i].lastSeen)/1000)+"s\"}";
  }
  j+="]";
  mqtt.publish(T_REG_NODES, j.c_str());
}