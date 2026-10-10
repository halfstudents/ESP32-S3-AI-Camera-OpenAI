#pragma once

#include <Arduino.h>

// Push-to-talk recorder for the onboard PDM microphone.
// Audio goes into a WAV buffer in PSRAM (16 kHz, 16-bit, mono).

bool micInit();
bool micStart();    // allocate the buffer and start a new recording
bool micPoll();     // read one chunk; returns false once the buffer is full
void micStop();     // finish the WAV header

const uint8_t* micWav();
size_t micWavSize();
void micRelease();  // free the buffer
