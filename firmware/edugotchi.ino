/*
 * falcons_tomodachi — bedside educational alarm companion
 * Board : ESP32-S3
 * OLED  : SSD1327 128x128 on Wire1 (GPIO 11/12)
 * IMU   : MPU6050      on Wire  (GPIO  4/ 5)
 * Pot   : GPIO 7
 * Audio : SPEAKER_PIN -1 = disabled
 *
 * Copy config.h.example → config.h and fill in credentials before flashing.
 * Partition: huge_app  (sprites.h ~3.6 MB)
 */

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1327.h>
#include "sprites.h"
#include "config.h"

// ── Pins ─────────────────────────────────────────────────────────
#define OLED_SDA     11
#define OLED_SCL     12
#define OLED_RST      8
#define OLED_ADDR    0x3D
#define IMU_SDA       4
#define IMU_SCL       5
#define POT_PIN       7
#define SPEAKER_PIN  13   // set to -1 to disable audio

// ── Tuning ───────────────────────────────────────────────────────
#define SHAKE_THRESH      15.0f  // m/s² total magnitude
#define SHAKE_DEBOUNCE    2000   // ms between shake triggers
#define HOLD_CONFIRM_MS   1500   // ms to hold pot zone to confirm
#define QUESTIONS_TO_WIN  5      // correct answers to dismiss alarm

// ── State machine ────────────────────────────────────────────────
enum State : uint8_t {
  IDLE,
  SHAKE_REACTION,
  ALARM_ACTIVE,
  QUESTION_DISPLAY,
  ANSWER_CORRECT,
  ANSWER_WRONG,
  ALARM_COMPLETE
};

// ── Question struct ───────────────────────────────────────────────
struct Question {
  char    text[200];
  char    opts[4][50];
  char    subject[60];
  uint8_t correct;
};

// ── Fallback questions (no Wi-Fi needed) ─────────────────────────
const Question FALLBACK_Q[] PROGMEM = {
  { "What is 7 x 8?",      {"54","56","58","64"},                  "multiplication and division", 1 },
  { "Capital of France?",  {"Berlin","Madrid","Paris","Rome"},      "world capitals",             2 },
  { "sqrt(144) = ?",       {"10","11","12","13"},                   "addition and subtraction",   2 },
  { "2 to the power 10?",  {"512","1024","2048","256"},             "multiplication and division",1 },
  { "Largest planet?",     {"Earth","Saturn","Jupiter","Neptune"},  "the solar system and planets",2 },
};
const int NUM_FALLBACK = sizeof(FALLBACK_Q) / sizeof(FALLBACK_Q[0]);

// ── Globals ───────────────────────────────────────────────────────
Adafruit_MPU6050 mpu;
Adafruit_SSD1327 display(128, 128, &Wire1, OLED_RST);
WebServer        server(80);

State    state          = IDLE;
uint8_t  currentAnim    = ANIM_STANBY;
uint8_t  currentFrame   = 0;
uint32_t lastFrameMs    = 0;
// Streak
int currentStreak = 0;

// Shake
uint32_t lastShakeMs = 0;
uint32_t shakeCount  = 0;

// Quiz
Question currentQ;
int      correctCount   = 0;
int      wrongCount     = 0;
int      questionNumber = 0;
uint8_t  lastZone         = 255;
uint32_t zoneHeldSince    = 0;
uint32_t questionStartMs  = 0;
bool     alarmPending     = false;

#define QUESTION_TIMEOUT_MS 100000
String   difficulty     = "easy";

// ── Audio (non-blocking melody) ───────────────────────────────────
struct Note { uint32_t freq; uint32_t ms; };

// Victory jingle — plays when all 3 questions answered correctly
const Note VICTORY[] = {
  {523, 100}, {0,  40},   // C5
  {659, 100}, {0,  40},   // E5
  {784, 100}, {0,  40},   // G5
  {1047,180}, {0,  60},   // C6
  {784, 100}, {0,  30},   // G5
  {880, 100}, {0,  30},   // A5
  {1047,400},             // C6 hold
};
const int VICTORY_LEN = sizeof(VICTORY) / sizeof(Note);

#if SPEAKER_PIN >= 0
static int     melodyIdx = -1;
static uint32_t melodyMs = 0;

void startMelody() {
  melodyIdx = 0;
  melodyMs  = millis();
  if (VICTORY[0].freq > 0) { ledcAttach(SPEAKER_PIN, VICTORY[0].freq, 8); ledcWrite(SPEAKER_PIN, 128); }
  else ledcWrite(SPEAKER_PIN, 0);
}

void tickMelody() {
  if (melodyIdx < 0 || melodyIdx >= VICTORY_LEN) return;
  if (millis() - melodyMs < VICTORY[melodyIdx].ms) return;
  melodyIdx++;
  melodyMs = millis();
  if (melodyIdx >= VICTORY_LEN) { ledcWrite(SPEAKER_PIN, 0); melodyIdx = -1; return; }
  if (VICTORY[melodyIdx].freq > 0) { ledcAttach(SPEAKER_PIN, VICTORY[melodyIdx].freq, 8); ledcWrite(SPEAKER_PIN, 128); }
  else ledcWrite(SPEAKER_PIN, 0);
}
#else
void startMelody() {}
void tickMelody() {}
#endif

// IMU idle animation
float filtX = 0, filtY = 0, velX = 0, velY = 0;
#define LP_ALPHA    0.75f
#define DEAD_ZONE   0.6f
#define VEL_ACCEL   0.7f
#define VEL_DECAY   0.80f
#define VEL_THRESH  0.5f

// ── Helpers ───────────────────────────────────────────────────────

void drawSpriteFrame(const uint8_t* frameData, bool showStreak = false) {
  for (int y = 0; y < 128; y++) {
    for (int x = 0; x < 128; x += 2) {
      uint8_t b = pgm_read_byte(&frameData[(y * 128 + x) / 2]);
      display.drawPixel(x,     y, (b >> 4) & 0xF);
      display.drawPixel(x + 1, y,  b       & 0xF);
    }
  }
  if (showStreak && currentStreak > 0) {
    display.setTextSize(1);
    display.setTextColor(0xF);
    display.setCursor(currentStreak >= 10 ? 110 : 116, 2);
    display.print(currentStreak);
  }
  display.display();
}

// Advance animation; returns true when a cycle completes
bool tickAnim() {
  uint32_t frameDelay = 1000UL / ANIMATIONS[currentAnim].fps;
  if (millis() - lastFrameMs < frameDelay) return false;
  lastFrameMs = millis();

  drawSpriteFrame(ANIMATIONS[currentAnim].data + (uint32_t)currentFrame * FRAME_BYTES, true);

  currentFrame++;
  if (currentFrame >= ANIMATIONS[currentAnim].frames) {
    currentFrame = 0;
    return true;
  }
  return false;
}

void setAnim(uint8_t anim) {
  currentAnim  = anim;
  currentFrame = 0;
  lastFrameMs  = 0;
}

uint8_t potZone() {
  int v = analogRead(POT_PIN);     // 0–4095 at 12-bit
  return (uint8_t)(v / 1024);      // zones 0–3
}

void loadFallbackQuestion() {
  int idx = questionNumber % NUM_FALLBACK;
  // copy from PROGMEM struct
  memcpy_P(&currentQ, &FALLBACK_Q[idx], sizeof(Question));
}

bool fetchQuestion() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("fetch: no WiFi"); return false; }
  HTTPClient http;
  String url = QUESTION_URL + String("?difficulty=") + difficulty;
  Serial.println("fetch: " + url);
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();
  Serial.println("fetch: HTTP " + String(code));
  if (code != 200) { http.end(); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;

  strlcpy(currentQ.text,    doc["question"] | "",  sizeof(currentQ.text));
  strlcpy(currentQ.subject, doc["subject"]  | "",  sizeof(currentQ.subject));
  for (int i = 0; i < 4; i++)
    strlcpy(currentQ.opts[i], doc["options"][i] | "", sizeof(currentQ.opts[i]));
  currentQ.correct = doc["correct_index"] | 0;
  return true;
}

void loadQuestion() {
  if (!fetchQuestion()) loadFallbackQuestion();
}

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

// Draw question UI (no sprite — full text screen)
void drawQuestionScreen(uint8_t zone, uint32_t heldMs) {
  display.clearDisplay();
  display.setTextColor(0xF);
  display.setTextSize(1);

  // Question text — simple word wrap at 21 chars
  display.setCursor(0, 0);
  String q = currentQ.text;
  while (q.length() > 0) {
    int cut = q.length() > 21 ? q.lastIndexOf(' ', 21) : q.length();
    if (cut <= 0) cut = 21;
    display.println(q.substring(0, cut));
    q = q.substring(cut + 1);
  }

  // Options
  const char labels[] = "ABCD";
  for (int i = 0; i < 4; i++) {
    int y = 64 + i * 14;
    bool selected = (i == zone);
    if (selected) {
      display.fillRect(0, y - 1, 128, 13, 4);
      display.setTextColor(0xF);
    } else {
      display.setTextColor(8);
    }
    display.setCursor(2, y);
    display.print(labels[i]);
    display.print(": ");
    display.print(currentQ.opts[i]);
  }
  display.setTextColor(0xF);

  // Progress number (correct/total)
  display.setCursor(92, 116);
  display.print(correctCount);
  display.print('/');
  display.print(QUESTIONS_TO_WIN);

  // Hold-to-confirm bar
  if (heldMs > 0) {
    int barW = (int)(128L * heldMs / HOLD_CONFIRM_MS);
    display.fillRect(0, 123, barW, 5, 0xA);
  }

  display.display();
}

void drawCenteredText(const char* line1, const char* line2 = nullptr) {
  display.clearDisplay();
  display.setTextColor(0xF);
  display.setTextSize(1);
  int y = line2 ? 56 : 60;
  display.setCursor((128 - strlen(line1) * 6) / 2, y);
  display.print(line1);
  if (line2) {
    display.setCursor((128 - strlen(line2) * 6) / 2, y + 12);
    display.print(line2);
  }
  display.display();
}

// ── Web server routes ─────────────────────────────────────────────

void handleStatus() {
  JsonDocument doc;
  doc["state"]          = (int)state;
  doc["alarm_active"]   = (state >= ALARM_ACTIVE && state < ALARM_COMPLETE);
  doc["question_num"]   = questionNumber;
  doc["correct"]        = correctCount;
  doc["wrong"]          = wrongCount;
  doc["pot_zone"]       = potZone();
  doc["shake_count"]    = shakeCount;
  doc["uptime_s"]       = millis() / 1000;
  doc["difficulty"]     = difficulty;
  doc["wifi_ip"]        = WiFi.localIP().toString();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleStartAlarm() {
  alarmPending = true;
  server.send(200, "text/plain", "alarm triggered");
}

void handleStopAlarm() {
  // Debug/demo bypass only
  state        = IDLE;
  correctCount = 0;
  wrongCount   = 0;
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

// ── State handlers ────────────────────────────────────────────────

void updateIdle() {
  // Check for alarm trigger from web server
  if (alarmPending) {
    alarmPending   = false;
    correctCount   = 0;
    wrongCount     = 0;
    questionNumber = 0;
    state          = ALARM_ACTIVE;
    setAnim(ANIM_ATTACK);
    return;
  }

  // Shake detection
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  float mag = sqrtf(a.acceleration.x * a.acceleration.x +
                    a.acceleration.y * a.acceleration.y +
                    a.acceleration.z * a.acceleration.z);
  if (mag > SHAKE_THRESH && millis() - lastShakeMs > SHAKE_DEBOUNCE) {
    lastShakeMs = millis();
    shakeCount++;
    state = SHAKE_REACTION;
    const uint8_t reactions[] = { ANIM_ATTACK, ANIM_DODGE, ANIM_USE_MAGIC };
    setAnim(reactions[shakeCount % 3]);
    return;
  }

  // Velocity-based idle animation
  filtX = LP_ALPHA * filtX + (1.0f - LP_ALPHA) * a.acceleration.x;
  filtY = LP_ALPHA * filtY + (1.0f - LP_ALPHA) * a.acceleration.y;
  float inX = fabsf(filtX) > DEAD_ZONE ? filtX : 0;
  float inY = fabsf(filtY) > DEAD_ZONE ? filtY : 0;
  velX = (velX + inX * VEL_ACCEL) * VEL_DECAY;
  velY = (velY + inY * VEL_ACCEL) * VEL_DECAY;

  uint8_t nextAnim = ANIM_STANBY;
  if (fabsf(velX) >= fabsf(velY)) {
    if (velX >  VEL_THRESH) nextAnim = ANIM_WALK_E;
    else if (velX < -VEL_THRESH) nextAnim = ANIM_WALK_W;
  } else {
    if (velY >  VEL_THRESH) nextAnim = ANIM_WALK_W;
    else if (velY < -VEL_THRESH) nextAnim = ANIM_DODGE;
  }

  if (nextAnim != currentAnim) setAnim(nextAnim);
  tickAnim();
}

void updateShakeReaction() {
  if (tickAnim()) {
    correctCount   = 0;
    wrongCount     = 0;
    questionNumber = 0;
    state          = ALARM_ACTIVE;
    setAnim(ANIM_ATTACK);
  }
}

void updateAlarmActive() {
  // Play attack once then load question
  if (tickAnim()) {
    loadQuestion();
    state           = QUESTION_DISPLAY;
    questionStartMs = millis();
    lastZone        = 255;
    zoneHeldSince   = 0;
  }
}

void updateQuestionDisplay() {
  uint8_t  zone   = potZone();
  uint32_t heldMs = 0;

  if (zone != lastZone) {
    lastZone      = zone;
    zoneHeldSince = millis();
  } else {
    heldMs = millis() - zoneHeldSince;
  }

  // 100-second timeout — slime gets angry and dies
  uint32_t elapsed = millis() - questionStartMs;
  if (elapsed >= QUESTION_TIMEOUT_MS) {
    wrongCount++;
    state = ANSWER_WRONG;
    setAnim(ANIM_DEATH);
    lastZone      = 255;
    zoneHeldSince = 0;
    return;
  }

  drawQuestionScreen(zone, heldMs);

  if (heldMs >= HOLD_CONFIRM_MS) {
    if (zone == currentQ.correct) {
      correctCount++;
      questionNumber++;
      state = ANSWER_CORRECT;
      setAnim(ANIM_USE_MAGIC);
      reportCorrect();
    } else {
      wrongCount++;
      state = ANSWER_WRONG;
      setAnim(ANIM_DEATH);
    }
    lastZone      = 255;
    zoneHeldSince = 0;
  }
}

void updateAnswerCorrect() {
  if (tickAnim()) {
    if (correctCount >= QUESTIONS_TO_WIN) {
      state = ALARM_COMPLETE;
      setAnim(ANIM_USE_MAGIC);
      startMelody();
    } else {
      loadQuestion();
      state           = QUESTION_DISPLAY;
      questionStartMs = millis();
      lastZone        = 255;
      zoneHeldSince   = 0;
    }
  }
}

void updateAnswerWrong() {
  if (tickAnim()) {
    state           = QUESTION_DISPLAY;
    questionStartMs = millis();
    lastZone        = 255;
    zoneHeldSince   = 0;
  }
}

void updateAlarmComplete() {
  if (tickAnim()) {
    reportWakeup();
    fetchStats();
    drawCenteredText("Good job!");
    delay(3000);
    state = IDLE;
    setAnim(ANIM_STANBY);
    correctCount = 0;
    wrongCount   = 0;
    questionNumber = 0;
  }
}

// ── Setup ─────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
#if SPEAKER_PIN >= 0
  ledcAttach(SPEAKER_PIN, 2000, 8);
  ledcWrite(SPEAKER_PIN, 0);
#endif

  // IMU
  Wire.begin(IMU_SDA, IMU_SCL);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050 not found");
    while (true) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // OLED
  Wire1.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(OLED_ADDR)) {
    Serial.println("SSD1327 not found");
    while (true) delay(10);
  }
  display.clearDisplay();
  drawCenteredText("Booting...");

  // Wi-Fi (non-blocking connect)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 6000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    fetchStats();
  } else {
    Serial.println("Wi-Fi unavailable — using fallback questions");
  }

  // Web server
  server.on("/status",      handleStatus);
  server.on("/startAlarm",  handleStartAlarm);
  server.on("/stopAlarm",   handleStopAlarm);
  server.on("/setDifficulty", handleSetDifficulty);
  server.begin();

  setAnim(ANIM_STANBY);
  drawSpriteFrame(ANIMATIONS[ANIM_STANBY].data);
  lastFrameMs = millis();
  Serial.println("Ready");
}

// ── Loop ──────────────────────────────────────────────────────────

void loop() {
  server.handleClient();
  tickMelody();

  switch (state) {
    case IDLE:             updateIdle();             break;
    case SHAKE_REACTION:   updateShakeReaction();    break;
    case ALARM_ACTIVE:     updateAlarmActive();      break;
    case QUESTION_DISPLAY: updateQuestionDisplay();  break;
    case ANSWER_CORRECT:   updateAnswerCorrect();    break;
    case ANSWER_WRONG:     updateAnswerWrong();      break;
    case ALARM_COMPLETE:   updateAlarmComplete();    break;
  }
}
