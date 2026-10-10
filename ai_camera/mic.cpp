#include "mic.h"
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include "config.h"

static const size_t WAV_HEADER_BYTES = 44;
static const size_t READ_CHUNK = 1024;  // 32 ms of audio

static I2SClass pdm;
static uint8_t* wavBuf = nullptr;
static size_t wavUsed = 0;
static size_t wavCap = 0;

static void put16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

static void put32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = v >> 24;
}

// Standard 44-byte PCM WAV header, mono, 16-bit
static void writeWavHeader(uint8_t* h, uint32_t dataBytes, uint32_t rate) {
  memcpy(h + 0, "RIFF", 4);
  put32(h + 4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(h + 16, 16);        // fmt chunk size
  put16(h + 20, 1);         // PCM
  put16(h + 22, 1);         // channels
  put32(h + 24, rate);
  put32(h + 28, rate * 2);  // byte rate
  put16(h + 32, 2);         // block align
  put16(h + 34, 16);        // bits per sample
  memcpy(h + 36, "data", 4);
  put32(h + 40, dataBytes);
}

bool micInit() {
  pdm.setPinsPdmRx(MIC_CLK_PIN, MIC_DATA_PIN);
  if (!pdm.begin(I2S_MODE_PDM_RX, MIC_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("[MIC] I2S PDM init failed");
    return false;
  }
  return true;
}

bool micStart() {
  micRelease();
  wavCap = WAV_HEADER_BYTES + (size_t)MIC_SAMPLE_RATE * 2 * MAX_RECORD_MS / 1000;
  wavBuf = (uint8_t*)heap_caps_malloc(wavCap, MALLOC_CAP_SPIRAM);
  if (!wavBuf) {
    Serial.println("[MIC] Could not allocate the recording buffer (is PSRAM enabled?)");
    return false;
  }
  wavUsed = WAV_HEADER_BYTES;
  return true;
}

bool micPoll() {
  if (!wavBuf || wavUsed >= wavCap) return false;
  size_t room = wavCap - wavUsed;
  size_t want = room < READ_CHUNK ? room : READ_CHUNK;
  wavUsed += pdm.readBytes((char*)(wavBuf + wavUsed), want);
  return wavUsed < wavCap;
}

void micStop() {
  if (!wavBuf) return;
  writeWavHeader(wavBuf, wavUsed - WAV_HEADER_BYTES, MIC_SAMPLE_RATE);
  Serial.printf("[MIC] Recorded %u ms\n",
                (unsigned)((wavUsed - WAV_HEADER_BYTES) * 1000 / (MIC_SAMPLE_RATE * 2)));
}

const uint8_t* micWav() {
  return wavBuf;
}

size_t micWavSize() {
  return wavBuf ? wavUsed : 0;
}

void micRelease() {
  if (wavBuf) heap_caps_free(wavBuf);
  wavBuf = nullptr;
  wavUsed = 0;
  wavCap = 0;
}
