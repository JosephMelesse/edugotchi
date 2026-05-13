#pragma once

void updateIdle() {
  if (alarmPending) {
    alarmPending   = false;
    correctCount   = 0;
    wrongCount     = 0;
    questionNumber = 0;
    state          = ALARM_ACTIVE;
    setAnim(ANIM_ATTACK);
    return;
  }

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

  filtX = LP_ALPHA * filtX + (1.0f - LP_ALPHA) * a.acceleration.x;
  filtY = LP_ALPHA * filtY + (1.0f - LP_ALPHA) * a.acceleration.y;
  float inX = fabsf(filtX) > DEAD_ZONE ? filtX : 0;
  float inY = fabsf(filtY) > DEAD_ZONE ? filtY : 0;
  velX = (velX + inX * VEL_ACCEL) * VEL_DECAY;
  velY = (velY + inY * VEL_ACCEL) * VEL_DECAY;

  uint8_t nextAnim = ANIM_STANBY;
  if (fabsf(velX) >= fabsf(velY)) {
    if      (velX >  VEL_THRESH) nextAnim = ANIM_WALK_E;
    else if (velX < -VEL_THRESH) nextAnim = ANIM_WALK_W;
  } else {
    if      (velY >  VEL_THRESH) nextAnim = ANIM_WALK_W;
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

  if (millis() - questionStartMs >= QUESTION_TIMEOUT_MS) {
    wrongCount++;
    state         = ANSWER_WRONG;
    lastZone      = 255;
    zoneHeldSince = 0;
    setAnim(ANIM_DEATH);
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
    state          = IDLE;
    correctCount   = 0;
    wrongCount     = 0;
    questionNumber = 0;
    setAnim(ANIM_STANBY);
  }
}
