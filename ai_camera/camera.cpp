#include "camera.h"
#include <Arduino.h>

// OV3660 pins on the DFR1154 (from the DFRobot wiki)
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 5
#define CAM_PIN_SIOD 8
#define CAM_PIN_SIOC 9
#define CAM_PIN_D7 4
#define CAM_PIN_D6 6
#define CAM_PIN_D5 7
#define CAM_PIN_D4 14
#define CAM_PIN_D3 17
#define CAM_PIN_D2 21
#define CAM_PIN_D1 18
#define CAM_PIN_D0 16
#define CAM_PIN_VSYNC 1
#define CAM_PIN_HREF 2
#define CAM_PIN_PCLK 15

bool cameraInit() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = CAM_PIN_D0;
  cfg.pin_d1 = CAM_PIN_D1;
  cfg.pin_d2 = CAM_PIN_D2;
  cfg.pin_d3 = CAM_PIN_D3;
  cfg.pin_d4 = CAM_PIN_D4;
  cfg.pin_d5 = CAM_PIN_D5;
  cfg.pin_d6 = CAM_PIN_D6;
  cfg.pin_d7 = CAM_PIN_D7;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;

  // 8 MHz instead of the usual 20 MHz: avoids "cam_hal: FB-OVF" overflows.
  cfg.xclk_freq_hz = 8000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = FRAMESIZE_VGA;  // 640 x 480 is plenty for the vision model
  cfg.jpeg_quality = 12;           // 0..63, lower = better quality
  cfg.fb_count = 2;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.fb_count = 1;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed, error 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s && s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // the sensor is mounted upside down
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);  // OV3660 colours are over-saturated by default
  }
  return true;
}

camera_fb_t* cameraCapture() {
  // The driver keeps two buffers, so the first one can be a stale frame.
  // Throw it away and take the next one.
  camera_fb_t* stale = esp_camera_fb_get();
  if (stale) esp_camera_fb_return(stale);
  return esp_camera_fb_get();
}

void cameraRelease(camera_fb_t* fb) {
  if (fb) esp_camera_fb_return(fb);
}
