#pragma once

// Pulls in your private secrets.h, or stops the build with a clear message.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h not found. Copy secrets_example.h to secrets.h and fill in your Wi-Fi name, password and OpenAI API key."
#endif
