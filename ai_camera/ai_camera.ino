// AI Camera: DFRobot ESP32-S3 AI Camera (DFR1154) + OpenAI
//
// Tap BOOT          -> takes a picture and describes what it sees
// Hold BOOT + speak -> takes a picture and answers your spoken question
// Type in Serial    -> takes a picture and answers the typed question
//
// The answer is printed on the Serial Monitor and spoken through the speaker.
//
// Before building: copy secrets_example.h to secrets.h and fill it in.

#include <WiFi.h>
#include <Audio.h>  // ESP32-audioI2S by schreibfaul1
#include "config.h"
#include "credentials.h"
#include "camera.h"
#include "mic.h"
#include "openai_api.h"

enum class State { Idle, Recording, Speaking };

Audio speaker;
State state = State::Idle;
bool lastButton = HIGH;
uint32_t pressStartMs = 0;

void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 30000) {
      Serial.println("\n[WIFI] No connection after 30 s, check secrets.h. Restarting.");
      delay(1000);
      ESP.restart();
    }
    delay(250);
    Serial.print('.');
  }
  Serial.printf("\n[WIFI] Connected, IP %s\n", WiFi.localIP().toString().c_str());
}

void syncClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  uint32_t start = millis();
  while (time(nullptr) < 100000 && millis() - start < 10000) delay(100);
}

// The TTS request is built as JSON by the audio library, so keep the text
// free of characters that would break it.
void speak(String text) {
  text.replace("\\", " ");
  text.replace("\"", "'");
  text.replace("\r", " ");
  text.replace("\n", " ");
  speaker.openai_speech(OPENAI_API_KEY, TTS_MODEL, text.c_str(), "", TTS_VOICE, "mp3", "1");
  state = State::Speaking;
}

void answer(const String& question) {
  Serial.printf("[ASK] %s\n", question.c_str());
  setLed(true);

  camera_fb_t* fb = cameraCapture();
  if (!fb) {
    Serial.println("[CAM] Capture failed");
    setLed(false);
    return;
  }
  Serial.printf("[CAM] %ux%u JPEG, %u bytes\n", fb->width, fb->height, (unsigned)fb->len);

  uint32_t t0 = millis();
  String reply = openaiAskAboutImage(question, fb->buf, fb->len);
  cameraRelease(fb);
  setLed(false);

  if (reply.isEmpty()) {
    Serial.println("[VISION] No answer");
    return;
  }
  Serial.printf("[VISION] (%lu ms) %s\n", (unsigned long)(millis() - t0), reply.c_str());
  speak(reply);
}

void finishRecording(uint32_t heldMs) {
  micStop();
  setLed(false);

  String question;
  if (heldMs < TAP_MAX_MS) {
    Serial.println("[BTN] Tap: describing the scene");
  } else {
    question = openaiTranscribe(micWav(), micWavSize());
    if (question.isEmpty()) Serial.println("[STT] Did not catch a question, describing the scene");
  }
  micRelease();

  if (question.isEmpty()) question = DEFAULT_QUESTION;
  state = State::Idle;
  answer(question);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  if (!psramFound()) {
    Serial.println("[ERR] PSRAM not found. Set Tools > PSRAM to \"OPI PSRAM\".");
  }

  connectWiFi();
  syncClock();

  micInit();
  speaker.setPinout(SPK_BCLK_PIN, SPK_LRC_PIN, SPK_DOUT_PIN);
  speaker.setVolume(SPEAKER_VOLUME);
  if (!cameraInit()) {
    Serial.println("[ERR] Camera not found. Check the ribbon cable and the board settings.");
  }

  Serial.println();
  Serial.println("Ready.");
  Serial.println("  Tap BOOT          -> describe what the camera sees");
  Serial.println("  Hold BOOT + speak -> ask a question about it");
  Serial.println("  Or type a question here and press Enter");
}

void loop() {
  speaker.loop();

  bool button = digitalRead(BUTTON_PIN);
  bool pressed = (button == LOW && lastButton == HIGH);
  lastButton = button;

  switch (state) {
    case State::Speaking:
      if (pressed) {
        speaker.stopSong();  // a press interrupts a long answer
        state = State::Idle;
      } else if (!speaker.isRunning()) {
        state = State::Idle;
      }
      return;  // keep speaker.loop() running as often as possible

    case State::Idle:
      if (pressed) {
        if (micStart()) {
          pressStartMs = millis();
          state = State::Recording;
          setLed(true);
          Serial.println("[MIC] Listening...");
        }
      } else if (Serial.available()) {
        String typed = Serial.readStringUntil('\n');
        typed.trim();
        if (typed.length()) answer(typed);
      }
      break;

    case State::Recording: {
      bool bufferFull = !micPoll();
      uint32_t held = millis() - pressStartMs;
      bool released = (button == HIGH && held > DEBOUNCE_MS);
      if (released || bufferFull || held >= MAX_RECORD_MS) {
        finishRecording(held);
      }
      break;
    }
  }

  delay(state == State::Recording ? 0 : 5);
}
