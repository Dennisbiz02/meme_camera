#include <Arduino.h>
#include "Adafruit_PyCamera.h"
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

#define LED_PIN    18       // A1 → GPIO18
#define LED_COUNT  12       // Anzahl der LEDs im Ring
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static const uint32_t TARGET_INTERVAL_MS = 333;  // für 3 fps
static const int TRIG_PIN = 17;                  // A0 → GPIO17

Adafruit_PyCamera pycamera;

void setup() {
  // Serielle Schnittstelle mit maximaler Baudrate initialisieren
  Serial.begin(5000000);

  if (!pycamera.begin()) {
    // Abbruch bei fehlerhafter Kamera‑Initialisierung
    while (true) {
      delay(100);
    }
  }

  // LED‑Ring initialisieren und auf weiß setzen
  ring.begin();
  ring.setBrightness(50);                   // Helligkeit (0–255)
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    ring.setPixelColor(i, ring.Color(255,255,255));  // Weiß
  }
  ring.show();                              // LED dauerhaft einschalten

  // Kamera auf JPEG-Ausgabe und 1280×1024 (SXGA) einstellen
  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_pixformat(sensor, PIXFORMAT_JPEG);
  sensor->set_framesize(sensor, FRAMESIZE_SXGA);

  // JPEG‑Qualitätsfaktor für ca. 30:1 Kompression
  sensor->set_quality(sensor, 15);

  // Ausgangspin konfigurieren
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void loop() {
  uint32_t t0 = millis();

  digitalWrite(TRIG_PIN, HIGH);
  digitalWrite(TRIG_PIN, LOW);

  camera_fb_t *fb = esp_camera_fb_get();

  if (fb) {
    uint32_t len = fb->len;
    Serial.write(reinterpret_cast<uint8_t*>(&len), sizeof(len));
    Serial.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
  }

  uint32_t dt = millis() - t0;
  if (dt < TARGET_INTERVAL_MS) {
    delay(TARGET_INTERVAL_MS - dt);
  }
}
