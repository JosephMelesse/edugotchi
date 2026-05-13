#pragma once

struct Note { uint32_t freq; uint32_t ms; };

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
static int      melodyIdx = -1;
static uint32_t melodyMs  = 0;

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
void tickMelody()  {}
#endif
