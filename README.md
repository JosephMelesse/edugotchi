# Edugotchi

A handheld educational toy built on the ESP32-S3 that quizzes you with AI-generated questions. Shake it to wake it up, answer 5 questions using the dial, and earn your score — all tracked for a parent to review on a web dashboard.

---

## How it works

Edugotchi lives on your desk. A slime character animates on the screen and reacts to how you hold and move the device. When you want a quiz, just shake it.

**Shake → 5 questions → score**

Questions are pulled live from an OpenAI-powered backend, tailored to elementary school subjects like math, geography, science, and grammar. The difficulty adjusts automatically based on how well you've been doing — score above 3/5 and it gets harder, below 3/5 and it eases up. If there's no Wi-Fi, it falls back to a set of hardcoded questions so it always works.

You answer using a potentiometer dial on the side — turn it to highlight one of four options (A, B, C, D) and hold it there for 1.5 seconds to confirm. No buttons needed. Get all 5 right and the slime plays a little victory melody.

---

## Parent Dashboard

A companion web server runs on any machine connected to the same network. Parents can open `http://<server-ip>:3000/portal` to see:

- **Learning streak** — consecutive days with a completed session
- **Accuracy** — overall correct answer rate
- **Subject performance** — which topics the child is strong or weak in, with adaptive weighting so weaker subjects come up more often
- **Session history** — scores and difficulty level per session
- **Current level** — easy / medium / hard, auto-adjusted each session

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | ESP32-S3 |
| Display | SSD1327 128×128 grayscale OLED |
| Motion | MPU6050 IMU |
| Input | Potentiometer (wiper on GPIO 7) |
| Audio | Passive buzzer (GPIO 13) |

**Wiring**

```
OLED  — Data: GPIO 11, Clk: GPIO 12, CS: GPIO 10, A0/DC: GPIO 9, RST: GPIO 8
IMU   — SDA: GPIO 4,  SCL: GPIO 5
Pot   — wiper: GPIO 7
Buzzer— GPIO 13
```

---

## Setup

### Firmware

1. Edit the top of `edugotchi.ino` with your Wi-Fi credentials and server IP:
```cpp
#define WIFI_SSID    "your-network"
#define WIFI_PASS    "your-password"
#define QUESTION_URL "http://<server-ip>:3000/question"
```

2. Flash with the `huge_app` partition scheme (sprites.h is ~3.6 MB):
```bash
arduino-cli compile --upload \
  -b esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app \
  -p /dev/ttyACM0 edugotchi/
```

### Server

```bash
cd server
npm install
node index.js
```

Parent portal → `http://localhost:3000/portal`

---

## Libraries

- `Wire.h`, `WiFi.h`, `WebServer.h`, `HTTPClient.h`
- `ArduinoJson`
- `Adafruit_MPU6050`, `Adafruit_SSD1327`, `Adafruit_GFX`
