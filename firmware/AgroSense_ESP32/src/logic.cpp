#include "agro.h"
/* ===== logic.ino — A4/A7 (adaptive time thresholds), A5/A6 (scoring), A8 (servo) ===== */

int currentHour(){
  struct tm ti;
  if(!getLocalTime(&ti, 50)) return 12;   // fallback noon if NTP not ready
  return ti.tm_hour;
}

// A4 + A7: thresholds change with time of day (stored in variables, not constants)
void updateThresholds(){
  int h = currentHour();
  if      (h>=5  && h<9 ) { partOfDay="morning"; thrMoist=35; }   // good time -> water earlier
  else if (h>=11 && h<15) { partOfDay="noon";    thrMoist=55; }   // avoid noon -> harder to trigger
  else if (h>=17 && h<20) { partOfDay="evening"; thrMoist=40; }
  else                    { partOfDay="night";   thrMoist=45; }
}

// A6: explainable irrigation score 0..100 (not a binary threshold)
void computeScore(){
  // moisture deficit contributes positively (drier -> higher score)
  pMoist = max(0.0f, (float)(thrMoist - moist)) * 1.6f;
  // rain (local sensor OR weather hint from dashboard) strongly suppresses
  pRain  = (rainLevel > 0 || rainSoon) ? -45.0f : 0.0f;
  // time-of-day bias (A7)
  if      (partOfDay=="morning") pTime= 25;
  else if (partOfDay=="evening") pTime= 20;
  else if (partOfDay=="noon")    pTime=-15;   // avoid watering at noon
  else                           pTime=  5;
  float s = 30 + pMoist + pRain + pTime;
  // light: very bright midday adds evaporation risk -> tiny nudge down
  if (light > 80 && partOfDay=="noon") s -= 5;
  score = (int)constrain(s, 0, 100);
}

// A5/A8: multi-factor decision -> servo
void decideValve(){
  if (selfMode=="MANUAL") return;              // dashboard controls valve directly
  bool open = (score >= 60) && (rainLevel == 0) && !rainSoon;
  if (open != valveOpen) setValve(open);
}

void setValve(bool open){
  valveOpen = open;
  valveServo.write(open ? 90 : 0);             // 90deg = open, 0deg = closed
}