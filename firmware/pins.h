#pragma once

#define OLED_SDA     11
#define OLED_SCL     12
#define OLED_RST      8
#define OLED_ADDR    0x3D
#define IMU_SDA       4
#define IMU_SCL       5
#define POT_PIN       7
#define SPEAKER_PIN  13   // set to -1 to disable audio

#define SHAKE_THRESH        15.0f
#define SHAKE_DEBOUNCE      2000
#define HOLD_CONFIRM_MS     1500
#define QUESTIONS_TO_WIN    5
#define QUESTION_TIMEOUT_MS 100000

#define LP_ALPHA   0.75f
#define DEAD_ZONE  0.6f
#define VEL_ACCEL  0.7f
#define VEL_DECAY  0.80f
#define VEL_THRESH 0.5f

enum State : uint8_t {
  IDLE,
  SHAKE_REACTION,
  ALARM_ACTIVE,
  QUESTION_DISPLAY,
  ANSWER_CORRECT,
  ANSWER_WRONG,
  ALARM_COMPLETE
};

struct Question {
  char    text[200];
  char    opts[4][50];
  char    subject[60];
  uint8_t correct;
};
