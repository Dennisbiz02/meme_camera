#include <Arduino.h>
#include "Adafruit_PyCamera.h"
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

// ========================== LED-Ring ==========================
#define LED_PIN    18
#define LED_COUNT  12
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========================== Bandgeschwindigkeit / Abstand / Offset ==========================
// Einheit v: Meter pro Minute
#define BAND_SPEED_MIN 1
#define BAND_SPEED_MAX 180
#define BAND_SPEED     60   // m/min (wird im Code auf [MIN,MAX] geklemmt; nicht veränderbar zur Laufzeit)

// Abstand in Metern von der Lichtschranke bis zur Zielposition
#define ABSTAND_M      1.0  // Beispiel: 1 m

// Offset in Zentimetern: >0 = später, <0 = früher
#define OFFSET_CM      0    // Beispiel: 2 cm später auslösen

static inline double clampSpeed(double speed_m_per_min) {
  if (speed_m_per_min < BAND_SPEED_MIN) return (double)BAND_SPEED_MIN;
  if (speed_m_per_min > BAND_SPEED_MAX) return (double)BAND_SPEED_MAX;
  return speed_m_per_min;
}

static inline int32_t computeWaitMs(double distance_m, double speed_m_per_min, int offset_cm) {
  const double v = clampSpeed(speed_m_per_min);            // m/min
  const double t_min = distance_m / v;                     // min
  const double t_off_min = ((double)offset_cm / 100.0) / v;// min
  const double t_total_min = t_min + t_off_min;            // min
  // negative Zeiten werden später durch Max(…,0) behandelt
  const double t_ms = t_total_min * 60000.0;               // ms
  // Rundung auf Integer-ms
  return (int32_t)lround(t_ms);
}

static const int TRIG_PIN = 17;

// ========================== Kamera & Action-Profil ==========================
static const int AEC_VALUE_ACTION = 60;                  // kleiner = kürzere Belichtung, schrittweise erhöhen wenn zu dunkel
static const bool ENABLE_LIMITED_AGC = false;            // Auto-Gain aus (true = leicht erlauben)
static const gainceiling_t GAIN_CEILING = (gainceiling_t)2; // ~2x Gain, vllt auf 4 aber dann oben an
static const bool ENABLE_AWB = true;
static const int JPEG_QUALITY = 15;                      // niedriger Wert = bessere Qualität (größere Datei)

Adafruit_PyCamera pycamera;

static void applyActionPhotoProfile(sensor_t* s) {
  // --- Pixelformat / Auflösung ---
  if (s->set_pixformat) s->set_pixformat(s, PIXFORMAT_JPEG);
  if (s->set_framesize) s->set_framesize(s, FRAMESIZE_SXGA);  // 1280x1024

  // --- Kompression / Rauschen ---
  if (s->set_quality)  s->set_quality(s, JPEG_QUALITY);
  if (s->set_denoise)  s->set_denoise(s, 6);
  if (s->set_bpc)      s->set_bpc(s, true);
  if (s->set_wpc)      s->set_wpc(s, true);
  if (s->set_lenc)     s->set_lenc(s, true);

  // --- Bildlook leicht justieren ---
  if (s->set_sharpness)  s->set_sharpness(s, 2);
  if (s->set_saturation) s->set_saturation(s, 0);
  if (s->set_contrast)   s->set_contrast(s, 0);
  if (s->set_brightness) s->set_brightness(s, 0);

  // --- Weißabgleich ---
  if (s->set_whitebal) s->set_whitebal(s, ENABLE_AWB ? 1 : 0);
  if (s->set_awb_gain) s->set_awb_gain(s, ENABLE_AWB ? 1 : 0);

  // --- Belichtung manuell kurz ---
  if (s->set_exposure_ctrl) s->set_exposure_ctrl(s, 0);   // AEC AUS
  if (s->set_aec2)          s->set_aec2(s, 0);
  if (s->set_aec_value)     s->set_aec_value(s, AEC_VALUE_ACTION);
  if (s->set_ae_level)      s->set_ae_level(s, -2);       // lieber etwas dunkler statt länger belichten

  // --- Gain/ISO: nur set_gain_ctrl verwenden 
  if (s->set_gain_ctrl)     s->set_gain_ctrl(s, ENABLE_LIMITED_AGC ? 1 : 0);
  if (s->set_gainceiling)   s->set_gainceiling(s, GAIN_CEILING);

}

// ========================== Setup ==========================
void setup() {
  Serial.begin(5000000);
  if (!pycamera.begin()) {
    while (true) { delay(100); }
  }
  // LED-Ring
  ring.begin();
  ring.setBrightness(200);
  for (uint16_t i = 0; i < LED_COUNT; i++) ring.setPixelColor(i, ring.Color(255, 255, 255));
  ring.show();

  // Kamera
  sensor_t *sensor = esp_camera_sensor_get();
  applyActionPhotoProfile(sensor);

  // Trigger
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

}

// ========================== Loop ==========================
void loop() {
  const uint32_t t0 = millis();

  // Geplante Wartezeit (nur aus Abstand, Bandgeschwindigkeit, Offset)
  const int32_t planned_wait_ms = computeWaitMs(ABSTAND_M, (double)BAND_SPEED, OFFSET_CM);

  // ===================== Trigger zuerst (Lichtschranke) =====================
  digitalWrite(TRIG_PIN, HIGH);
  delay(100);
  digitalWrite(TRIG_PIN, LOW);
  delay(600);

  // Bisher verstrichene Zeit seit Loop-Beginn (inkl. Trigger-Delays)
  const uint32_t dt_after_trigger = millis() - t0;

  // Verbleibende Wartezeit bis zum Aufnahmezeitpunkt (nicht negativ werden lassen)
  int32_t remaining_ms = (int32_t)planned_wait_ms - (int32_t)dt_after_trigger;
  if (remaining_ms > 0) {
    delay((uint32_t)remaining_ms);
  }

  // ===================== Aufnahme =====================
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    uint32_t len = fb->len;
    Serial.write(reinterpret_cast<uint8_t*>(&len), sizeof(len));
    Serial.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
  }
}
