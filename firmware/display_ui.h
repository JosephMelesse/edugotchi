#pragma once

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
  int v = analogRead(POT_PIN);
  return (uint8_t)(v / 1024);
}

void drawQuestionScreen(uint8_t zone, uint32_t heldMs) {
  display.clearDisplay();
  display.setTextColor(0xF);
  display.setTextSize(1);

  display.setCursor(0, 0);
  String q = currentQ.text;
  while (q.length() > 0) {
    int cut = q.length() > 21 ? q.lastIndexOf(' ', 21) : q.length();
    if (cut <= 0) cut = 21;
    display.println(q.substring(0, cut));
    q = q.substring(cut + 1);
  }

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

  display.setCursor(92, 116);
  display.print(correctCount);
  display.print('/');
  display.print(QUESTIONS_TO_WIN);

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
