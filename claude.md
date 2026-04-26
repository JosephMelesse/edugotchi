You are helping me build an Arduino/PlatformIO ESP32-S3 project for a hackathon.

Project idea:
Create a bedside educational alarm companion using an ESP32-S3, SSD1327 128x128 grayscale OLED, MPU6050, one potentiometer, speaker/audio output, Wi-Fi, and preconverted sprite animations in sprites.h.

Important hardware wiring so far:

SSD1327 OLED:
- VCC -> 3.3V
- GND -> GND
- SDA -> GPIO 11
- SCL -> GPIO 12
- RST -> GPIO 8

MPU6050:
- VIN -> 3.3V
- GND -> GND
- SDA -> GPIO 4
- SCL -> GPIO 5

Potentiometer:
- Left leg -> GND
- Right leg -> 3.3V
- Wiper -> GPIO 7

There are NO answer buttons. Use the potentiometer as the main input.

I have a file called sprites.h. It contains preconverted SSD1327-compatible animation frames. The frame format is:
- 128x128 pixels
- 4-bit grayscale
- nibble-packed
- 8192 bytes per frame
- stored in PROGMEM
Use FRAME_BYTES from sprites.h instead of hardcoding 8192.

Main behavior:
1. On boot:
   - Initialize Serial.
   - Initialize two separate I2C buses:
     - OLED bus on SDA GPIO 11, SCL GPIO 12
     - MPU6050 bus on SDA GPIO 4, SCL GPIO 5
   - Initialize SSD1327 OLED.
   - Initialize MPU6050.
   - Initialize potentiometer analog input on GPIO 7.
   - Start Wi-Fi if credentials are provided.
   - Start a simple web server.

2. Idle mode:
   - Play a standby/sleeping animation from sprites.h.
   - Read MPU6050 continuously.
   - Read the potentiometer continuously.
   - Show a small UI overlay or status screen if practical.

3. Shake behavior:
   - Detect a shake using MPU6050 acceleration magnitude.
   - When shaken, play a special animation such as attack, dodge, magic, or wake animation from sprites.h.
   - Debounce shake detection so it does not trigger repeatedly.

4. Alarm / quiz behavior:
   - This is an educational alarm clock.
   - The alarm should only stop after the user answers quiz questions correctly.
   - Since there are no buttons, use the potentiometer to select between 4 MCQ options:
     - pot range 0–1023 or 0–4095 divided into 4 zones:
       - zone 0 = A
       - zone 1 = B
       - zone 2 = C
       - zone 3 = D
   - Since there is no select button, implement “hold to confirm”:
       - If the pot stays in the same option zone for about 1.5 seconds, confirm that answer.
   - Display the current selected option clearly on the OLED.
   - If correct, increment progress.
   - If wrong, keep alarm active and play a reaction animation.

5. OpenAI / question generation:
   - Do NOT hardcode an OpenAI API key on the ESP32.
   - Implement the ESP32 so it can call a configurable backend endpoint such as:
       http://<server-ip>:3000/question
   - The backend should return JSON like:
       {
         "question": "What is 7 * 8?",
         "options": ["54", "56", "58", "64"],
         "correct_index": 1,
         "difficulty": "easy"
       }
   - Also include fallback hardcoded sample questions in the ESP32 firmware so the demo works without Wi-Fi.

6. Web server:
   - Serve a simple local dashboard from the ESP32.
   - Include:
       - alarm status
       - current question number
       - correct answers
       - wrong answers
       - current selected option from potentiometer
       - shake count
       - uptime
   - Provide JSON endpoint /status.
   - Provide endpoint /startAlarm to trigger alarm mode manually for demo.
   - Provide endpoint /stopAlarm for debugging only, but mark it as debug/demo bypass.
   - Provide endpoint /setDifficulty?level=easy|medium|hard if easy to implement.

7. Speaker:
   - I have speakers, but the exact speaker pin may not be wired yet.
   - Define SPEAKER_PIN near the top.
   - If SPEAKER_PIN is set to -1, compile with audio disabled.
   - If a pin is provided later, use LEDC tone generation or another ESP32-compatible method to make alarm beeps.
   - Do not block animation while playing sound.

8. Animation requirements:
   - Use sprites.h animations from PROGMEM.
   - Do not copy entire animations into RAM.
   - Use a single frame buffer if required by the display library.
   - Create helper functions like:
       playAnimation(animData, frameCount, fps, loop)
       drawFrameFromProgmem(framePtr)
   - If the exact animation symbol names in sprites.h differ, inspect sprites.h and adapt the code to the names that exist.
   - The code should compile after matching the actual sprite array names.

9. Architecture:
   - Use non-blocking timing with millis().
   - Avoid long delay() calls except tiny ones during setup.
   - Use a clear state machine:
       BOOT
       IDLE
       SHAKE_REACTION
       ALARM_ACTIVE
       QUESTION_DISPLAY
       ANSWER_CORRECT
       ANSWER_WRONG
       ALARM_COMPLETE
   - Keep the code readable and hackathon-demo friendly.

10. Deliverables:
   - Provide a complete Arduino sketch or PlatformIO project.
   - Include all required libraries.
   - Include pin definitions at the top.
   - Include comments explaining where to change Wi-Fi credentials, backend endpoint, and speaker pin.
   - Include a short wiring reminder.
   - Include troubleshooting notes for:
       - OLED not displaying
       - MPU6050 not detected
       - pot values unstable
       - Wi-Fi unavailable
       - sprites too large for flash

Preferred libraries:
- Wire.h
- WiFi.h
- WebServer.h or ESPAsyncWebServer if you think it is better
- ArduinoJson
- Adafruit_MPU6050 or another stable MPU6050 library
- A suitable SSD1327 OLED library, preferably Adafruit_SSD1327 if compatible

Make practical implementation choices. If a library cannot directly draw 4-bit packed frames, write a conversion helper that reads each nibble from PROGMEM and maps it to the display buffer format expected by the SSD1327 library.

Please produce code that prioritizes getting a working demo on real ESP32-S3 hardware.
