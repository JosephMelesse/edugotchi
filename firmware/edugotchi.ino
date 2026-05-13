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
#include "pins.h"

// ── Globals ───────────────────────────────────────────────────────
Adafruit_MPU6050 mpu;
Adafruit_SSD1327 display(128, 128, &Wire1, OLED_RST);
WebServer        server(80);

State    state          = IDLE;
uint8_t  currentAnim    = ANIM_STANBY;
uint8_t  currentFrame   = 0;
uint32_t lastFrameMs    = 0;
int      currentStreak  = 0;

uint32_t lastShakeMs = 0;
uint32_t shakeCount  = 0;

Question currentQ;
int      correctCount   = 0;
int      wrongCount     = 0;
int      questionNumber = 0;
uint8_t  lastZone        = 255;
uint32_t zoneHeldSince   = 0;
uint32_t questionStartMs = 0;
bool     alarmPending    = false;

String   difficulty = "easy";

float filtX = 0, filtY = 0, velX = 0, velY = 0;

// ── Modules (included after globals so they share this scope) ─────
#include "audio.h"
#include "quiz.h"
#include "display_ui.h"
#include "network.h"
#include "states.h"

// ── Setup ─────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
#if SPEAKER_PIN >= 0
  ledcAttach(SPEAKER_PIN, 2000, 8);
  ledcWrite(SPEAKER_PIN, 0);
#endif

  Wire.begin(IMU_SDA, IMU_SCL);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050 not found");
    while (true) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Wire1.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(OLED_ADDR)) {
    Serial.println("SSD1327 not found");
    while (true) delay(10);
  }
  display.clearDisplay();
  drawCenteredText("Booting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 6000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    fetchStats();
  } else {
    Serial.println("Wi-Fi unavailable — using fallback questions");
  }

  server.on("/status",        handleStatus);
  server.on("/startAlarm",    handleStartAlarm);
  server.on("/stopAlarm",     handleStopAlarm);
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
