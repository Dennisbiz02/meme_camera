#include <Arduino.h>
#include "Adafruit_PyCamera.h"
#include "esp_camera.h"

Adafruit_PyCamera pycamera;

void setup() {
  Serial.begin(115200);
  if (!pycamera.begin()) {
    // Kamera‑Initialisierung fehlgeschlagen
    while (true) {
      delay(100);
    }
  }
  // Auf JPEG-Ausgabe und 1280×1024 einstellen
  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_pixformat(sensor, PIXFORMAT_JPEG);
  sensor->set_framesize(sensor, FRAMESIZE_SXGA);
}

void loop() {
  // Bild puffern (jpeg) und Pufferadresse/Länge erhalten
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    delay(1000);
    return;
  }
  // Länge preceden (4 Byte LSB-first) und Bilddaten senden
  uint32_t len = fb->len;
  Serial.write((uint8_t *)&len, sizeof(len));
  Serial.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
  delay(1000);
}
