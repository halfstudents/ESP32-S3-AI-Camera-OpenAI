#pragma once

#include "esp_camera.h"

// Starts the OV3660 camera. Returns false if the sensor did not respond.
bool cameraInit();

// Takes a fresh JPEG picture. Give it back with cameraRelease() when done.
camera_fb_t* cameraCapture();

void cameraRelease(camera_fb_t* fb);
