#include "openai_api.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "credentials.h"

static const char* API_BASE = "https://api.openai.com/v1";

// Opens an HTTPS request to the OpenAI API with the auth header set.
// setInsecure() skips server certificate checks; see "Security notes" in the README.
static bool beginRequest(HTTPClient& http, WiFiClientSecure& client, const char* path) {
  client.setInsecure();
  http.setConnectTimeout(10000);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, String(API_BASE) + path)) {
    Serial.printf("[API] Could not open %s\n", path);
    return false;
  }
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  return true;
}

static void printError(const char* tag, int code, const String& body) {
  Serial.printf("[%s] HTTP %d\n", tag, code);
  if (code == 401) Serial.println("       Check OPENAI_API_KEY in secrets.h");
  if (code == 429) Serial.println("       Rate limit or no credit left on the OpenAI account");
  if (body.length()) Serial.println(body);
}

String openaiTranscribe(const uint8_t* wav, size_t wavLen) {
  if (!wav || wavLen <= 44) return "";

  const char* boundary = "----aicamBoundary7MA4YWxkTrZu0gW";
  String head = String("--") + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
              + STT_MODEL + "\r\n"
              + "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"file\"; filename=\"question.wav\"\r\n"
              + "Content-Type: audio/wav\r\n\r\n";
  String tail = String("\r\n--") + boundary + "--\r\n";

  // One contiguous body in PSRAM: head + WAV + tail
  size_t bodyLen = head.length() + wavLen + tail.length();
  uint8_t* body = (uint8_t*)heap_caps_malloc(bodyLen, MALLOC_CAP_SPIRAM);
  if (!body) {
    Serial.println("[STT] Out of memory");
    return "";
  }
  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), wav, wavLen);
  memcpy(body + head.length() + wavLen, tail.c_str(), tail.length());

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginRequest(http, client, "/audio/transcriptions")) {
    heap_caps_free(body);
    return "";
  }
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);

  Serial.printf("[STT] Uploading %u bytes\n", (unsigned)bodyLen);
  int code = http.POST(body, bodyLen);
  String payload = code > 0 ? http.getString() : String();
  http.end();
  heap_caps_free(body);

  if (code != 200) {
    printError("STT", code, payload);
    return "";
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("[STT] Could not parse the response");
    return "";
  }
  String text = doc["text"] | "";
  text.trim();
  return text;
}

String openaiAskAboutImage(const String& question, const uint8_t* jpg, size_t jpgLen) {
  if (!jpg || !jpgLen) return "";

  String body;
  {
    // Scoped so the base64 copy and the JSON document are freed before the upload.
    String dataUrl = "data:image/jpeg;base64,";
    dataUrl += base64::encode(jpg, jpgLen);

    JsonDocument req;
    req["model"] = VISION_MODEL;
    req["max_tokens"] = MAX_ANSWER_TOKENS;

    JsonArray messages = req["messages"].to<JsonArray>();

    JsonObject system = messages.add<JsonObject>();
    system["role"] = "system";
    system["content"] = SYSTEM_PROMPT;

    JsonObject user = messages.add<JsonObject>();
    user["role"] = "user";
    JsonArray content = user["content"].to<JsonArray>();

    JsonObject text = content.add<JsonObject>();
    text["type"] = "text";
    text["text"] = question;

    JsonObject image = content.add<JsonObject>();
    image["type"] = "image_url";
    image["image_url"]["url"] = dataUrl;
    image["image_url"]["detail"] = IMAGE_DETAIL;

    serializeJson(req, body);
  }

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginRequest(http, client, "/chat/completions")) return "";
  http.addHeader("Content-Type", "application/json");

  Serial.printf("[VISION] Asking %s (%u byte request)\n", VISION_MODEL, (unsigned)body.length());
  int code = http.POST(body);
  body = String();
  String payload = code > 0 ? http.getString() : String();
  http.end();

  if (code != 200) {
    printError("VISION", code, payload);
    return "";
  }

  // Keep only choices[0].message.content from the response
  JsonDocument filter;
  filter["choices"][0]["message"]["content"] = true;
  JsonDocument resp;
  if (deserializeJson(resp, payload, DeserializationOption::Filter(filter))) {
    Serial.println("[VISION] Could not parse the response");
    return "";
  }
  String answer = resp["choices"][0]["message"]["content"] | "";
  answer.trim();
  return answer;
}
