#pragma once

void serverGet(const String& path) {
  if (WiFi.status() != WL_CONNECTED) return;
  String base = String(QUESTION_URL);
  base = base.substring(0, base.lastIndexOf('/'));
  HTTPClient http;
  http.begin(base + path);
  http.setTimeout(3000);
  http.GET();
  http.end();
}

void reportCorrect() {
  serverGet("/report/correct?subject=" + String(currentQ.subject));
}

void reportWakeup() {
  serverGet("/report/wakeup?correct=" + String(correctCount) + "&wrong=" + String(wrongCount));
}

void fetchStats() {
  if (WiFi.status() != WL_CONNECTED) return;
  String base = String(QUESTION_URL);
  base = base.substring(0, base.lastIndexOf('/'));
  HTTPClient http;
  http.begin(base + "/stats");
  http.setTimeout(5000);
  if (http.GET() == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getStream())) {
      currentStreak = doc["streak"] | 0;
    }
  }
  http.end();
}

void handleStatus() {
  JsonDocument doc;
  doc["state"]        = (int)state;
  doc["alarm_active"] = (state >= ALARM_ACTIVE && state < ALARM_COMPLETE);
  doc["question_num"] = questionNumber;
  doc["correct"]      = correctCount;
  doc["wrong"]        = wrongCount;
  doc["pot_zone"]     = potZone();
  doc["shake_count"]  = shakeCount;
  doc["uptime_s"]     = millis() / 1000;
  doc["difficulty"]   = difficulty;
  doc["wifi_ip"]      = WiFi.localIP().toString();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleStartAlarm() {
  alarmPending = true;
  server.send(200, "text/plain", "alarm triggered");
}

void handleStopAlarm() {
  state          = IDLE;
  correctCount   = 0;
  wrongCount     = 0;
  questionNumber = 0;
  setAnim(ANIM_STANBY);
  server.send(200, "text/plain", "alarm stopped (debug)");
}

void handleSetDifficulty() {
  if (server.hasArg("level")) {
    difficulty = server.arg("level");
    server.send(200, "text/plain", "difficulty set to " + difficulty);
  } else {
    server.send(400, "text/plain", "missing ?level=easy|medium|hard");
  }
}
