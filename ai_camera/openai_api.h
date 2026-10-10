#pragma once

#include <Arduino.h>

// Speech to text. Takes a complete WAV file, returns the words ("" on error).
String openaiTranscribe(const uint8_t* wav, size_t wavLen);

// Sends one JPEG picture plus a question to the vision model and returns the
// answer ("" on error).
String openaiAskAboutImage(const String& question, const uint8_t* jpg, size_t jpgLen);
