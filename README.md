# AI Camera with the DFRobot ESP32-S3 AI Camera and OpenAI

An ESP32-S3 camera that tells you what it sees. Press a button and the camera takes a
picture, sends it to an OpenAI vision model, and speaks the answer through its own
speaker. Hold the button and ask a question ("What colour is the cup?", "Is anyone in
the room?") and it answers that question about the picture instead.

Everything runs on one board, the **DFRobot ESP32-S3 AI Camera (DFR1154)**. It has a
camera, a microphone, a speaker amplifier and a button on board, so no wiring is needed.

## How it works

```
 BOOT button
     |
     |-- tap ----------------------------------------------.
     |                                                      |
     '-- hold + speak --> PDM mic --> WAV --> Whisper ------+--> question
                                             (speech to text)        |
                                                                     v
 OV3660 camera --> JPEG --> base64 ---------------------> gpt-4o-mini (vision)
                                                                     |
                                                                     v
 Speaker <-- MP3 <-- tts-1 (text to speech) <------------------- answer text
                                                                     |
                                                                     '--> Serial Monitor
```

1. **Tap** the BOOT button: the camera asks the model "What is in this picture?"
2. **Hold** the BOOT button and speak (up to 4 seconds): your voice is turned into text
   with OpenAI speech-to-text and used as the question.
3. Or **type** a question in the Serial Monitor and press Enter.
4. The camera takes a fresh JPEG and sends it with the question to the vision model.
5. The answer is printed on the Serial Monitor and spoken through the speaker.
   Press the button while it is talking to stop it.

## Hardware

| Part | Notes |
|---|---|
| [DFRobot ESP32-S3 AI Camera, DFR1154](https://www.dfrobot.com/product-2899.html) | ESP32-S3, 16 MB flash, 8 MB PSRAM, OV3660 160° camera, IR illumination, PDM mic, I2S amp, light sensor, SD slot |
| Small speaker | Connected to the board's speaker connector |
| USB-C cable | Power and programming |

Board documentation: [DFRobot wiki, DFR1154](https://wiki.dfrobot.com/dfr1154/)

Pins used (all on board, set in [`ai_camera/config.h`](ai_camera/config.h)):

| Function | GPIO |
|---|---|
| BOOT button | 0 |
| Status LED | 3 |
| Microphone clock / data (PDM) | 38 / 39 |
| Speaker BCLK / LRC / DOUT (I2S) | 45 / 46 / 42 |
| Camera | see [`ai_camera/camera.cpp`](ai_camera/camera.cpp) |

## Software setup

### 1. Arduino IDE and the ESP32 core

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (2.x).
2. In **File > Preferences > Additional boards manager URLs**, add
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. In **Boards Manager**, install **esp32 by Espressif Systems**, version **3.3.x**.
   The audio library below does not build on 3.1.x or older.

### 2. Libraries

Install these from the Library Manager:

| Library | Version |
|---|---|
| ArduinoJson (Benoit Blanchon) | 7.x |
| ESP32-audioI2S (schreibfaul1) | 3.4.x |

WiFi, HTTPClient, WiFiClientSecure, ESP_I2S and the camera driver come with the ESP32 core.

### 3. Board settings (Tools menu)

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Enabled (needed for the Serial Monitor) |

### 4. Your credentials

Your Wi-Fi password and OpenAI key are **not** in this repository. You add them
locally in a file that git ignores:

1. Open the `ai_camera` folder.
2. Copy `secrets_example.h` to a new file named **`secrets.h`**.
3. Fill in your Wi-Fi name, Wi-Fi password and OpenAI API key.

```cpp
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define OPENAI_API_KEY "sk-..."
```

Get an API key at [platform.openai.com/api-keys](https://platform.openai.com/api-keys)
and set a monthly spending limit on the account. If `secrets.h` is missing, the build
stops with a message telling you to create it.

### 5. Upload

Open `ai_camera/ai_camera.ino`, select the COM port, and upload. Open the Serial
Monitor at 115200 baud. When it prints `Ready.`, tap the BOOT button.

## Settings you can change

All in [`ai_camera/config.h`](ai_camera/config.h):

| Setting | Default | What it does |
|---|---|---|
| `VISION_MODEL` | `gpt-4o-mini` | The model that looks at the picture |
| `IMAGE_DETAIL` | `low` | `low` is cheap and fast; `high` reads fine detail and costs more |
| `SYSTEM_PROMPT` | short spoken answers | How the camera should answer |
| `DEFAULT_QUESTION` | "What is in this picture?" | Used for a tap |
| `STT_MODEL` | `whisper-1` | Speech to text |
| `TTS_MODEL`, `TTS_VOICE` | `tts-1`, `shimmer` | Text to speech voice |
| `SPEAKER_VOLUME` | 18 | 0 to 21 |
| `MAX_RECORD_MS` | 4000 | Longest spoken question |
| `MAX_ANSWER_TOKENS` | 150 | Caps the answer length and cost |

The picture size and quality are set in `cameraInit()` in `camera.cpp` (640 x 480 JPEG by default).

## Troubleshooting

| Symptom | Fix |
|---|---|
| `secrets.h not found` when compiling | Do step 4 above |
| Errors inside `Audio.h` / `psram_unique_ptr.hpp` | Update the ESP32 core to 3.3.x |
| `PSRAM not found` | Tools > PSRAM > OPI PSRAM |
| Nothing on the Serial Monitor | Tools > USB CDC On Boot > Enabled |
| `Camera init failed` | Reseat the camera ribbon cable, check the board settings |
| `cam_hal: FB-OVF` warnings | Already handled by the 8 MHz camera clock in `camera.cpp` |
| `HTTP 401` | Wrong or revoked API key in `secrets.h` |
| `HTTP 429` | Rate limit, or no credit left on the OpenAI account |
| No sound | Check the speaker connector and `SPEAKER_VOLUME` |

## Cost

Every answer is three paid API calls (speech to text, vision, text to speech), or two
for a tap. With `gpt-4o-mini` and `IMAGE_DETAIL "low"` each answer costs a fraction of
a cent, but set a spending limit on your OpenAI account anyway.

## Security notes

- Never commit `secrets.h`. It is in `.gitignore`. If a key is ever pushed by mistake,
  revoke it on the OpenAI dashboard straight away; deleting the commit is not enough.
- Anyone who can read the flash of a programmed board can extract the key. Do not give
  away a programmed board with your key on it.
- HTTPS requests use `setInsecure()`, which encrypts the traffic but does not check
  the server certificate. That keeps the sketch simple and stops it breaking when
  certificates rotate. For a product, load the root CA with `setCACert()` instead.

## Credits

- Project idea and board bring-up notes: [Vision Chatbot with DFRobot ESP32-S3 AI Camera
  and OpenAI](https://www.makerguides.com/vision-chatbot-with-dfrobot-esp32-s3-ai-camera-and-openai/)
  on Makerguides.
- Board details: [DFRobot DFR1154 wiki](https://wiki.dfrobot.com/dfr1154/).
- API reference: [OpenAI images and vision guide](https://developers.openai.com/api/docs/guides/images-vision).
- Audio playback: [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S).

## License

MIT, see [LICENSE](LICENSE).
