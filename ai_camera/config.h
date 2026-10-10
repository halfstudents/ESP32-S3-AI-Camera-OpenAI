#pragma once

// ---------------------------------------------------------------------------
// Board pins: DFRobot ESP32-S3 AI Camera (DFR1154)
// Camera pins live in camera.cpp.
// ---------------------------------------------------------------------------
#define BUTTON_PIN 0     // BOOT button, active LOW
#define LED_PIN 3        // status LED

#define MIC_CLK_PIN 38   // onboard PDM microphone
#define MIC_DATA_PIN 39

#define SPK_BCLK_PIN 45  // onboard I2S amplifier
#define SPK_LRC_PIN 46
#define SPK_DOUT_PIN 42

// ---------------------------------------------------------------------------
// Button and recording
// ---------------------------------------------------------------------------
#define MIC_SAMPLE_RATE 16000  // Hz, 16-bit mono
#define MAX_RECORD_MS 4000     // longest spoken question
#define TAP_MAX_MS 400         // a press shorter than this = "describe the scene"
#define DEBOUNCE_MS 50

// ---------------------------------------------------------------------------
// OpenAI models
// ---------------------------------------------------------------------------
#define STT_MODEL "whisper-1"       // speech to text
#define VISION_MODEL "gpt-4o-mini"  // image + question -> answer
#define TTS_MODEL "tts-1"           // answer -> speech
#define TTS_VOICE "shimmer"
#define SPEAKER_VOLUME 18           // 0..21

// "low" sends a 512 px version of the picture: cheap and fast.
// "high" or "auto" lets the model look at fine detail, at a higher token cost.
#define IMAGE_DETAIL "low"
#define MAX_ANSWER_TOKENS 150

#define SYSTEM_PROMPT                                                   \
  "You are the voice of a small camera. You are shown one photo and a " \
  "question about it. Answer in plain spoken English, in at most three " \
  "short sentences. Do not use lists, markdown or emojis."

// Used for a short tap, or when the spoken question could not be understood.
#define DEFAULT_QUESTION "What is in this picture? Describe what you see."

#define HTTP_TIMEOUT_MS 30000
