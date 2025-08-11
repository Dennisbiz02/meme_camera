#include <Arduino.h>
#include "Adafruit_PyCamera.h"
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

// ========================== LED-Ring ==========================
#define LED_PIN    18
#define LED_COUNT  12
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========================== Takt / Auslöse-Intervall ==========================
#define BAND_SPEED_MIN 1
#define BAND_SPEED_MAX 180
#define BAND_SPEED     180  // BLEIBT UNVERÄNDERT (für Intervall & als m/s für Offset-Berechnung)

// Offset in Zentimetern: >0 = später, <0 = früher
#define OFFSET_CM      2    // Beispiel: 2 cm später auslösen

static inline uint32_t getIntervalMs(int speed) {
  if (speed < BAND_SPEED_MIN) speed = BAND_SPEED_MIN;
  if (speed > BAND_SPEED_MAX) speed = BAND_SPEED_MAX;
  return 60000UL / speed;
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

  // --- Gain/ISO: nur set_gain_ctrl verwenden (vorhanden), KEIN set_agc mehr! ---
  if (s->set_gain_ctrl)     s->set_gain_ctrl(s, ENABLE_LIMITED_AGC ? 1 : 0);
  if (s->set_gainceiling)   s->set_gainceiling(s, GAIN_CEILING);

}

void setup() {
  Serial.begin(5000000);
  if (!pycamera.begin()) {
    while (true) { delay(100); }
  }

  // LED‑Ring hell: hilft, kurze Belichtungszeit zu fahren
  ring.begin();
  ring.setBrightness(200);
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    ring.setPixelColor(i, ring.Color(255, 255, 255));
  }
  ring.show();

  // Kamera konfigurieren
  sensor_t *sensor = esp_camera_sensor_get();
  applyActionPhotoProfile(sensor);

  // Trigger-Pin
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void loop() {
  const uint32_t t0 = millis();
  const uint32_t targetInterval = getIntervalMs(BAND_SPEED);

  // ===== Timing: Offset cm → Zeitverschiebung (ms)
  // Formel: t_ms = (OFFSET_CM / 100 m) / (BAND_SPEED m/s) * 1000 ms
  // POSITIV ⇒ später; NEGATIV ⇒ früher
  // Berechnung ABSICHTLICH INLINE (keine Zusatzvariable nur für die Rechnung)
  const int32_t wait_pre =
    (int32_t)targetInterval
    + (int32_t)lround( ((double)OFFSET_CM / 100.0) / (double)BAND_SPEED * 1000.0 );

  // Wir warten VOR der Aufnahme, um früher/später auszulösen.
  // Wenn wait_pre negativ ist, wird gar nicht gewartet (→ früher).
  if (wait_pre > 0) {
    // verbleibende Zeit bis zur geplanten Auslösung in diesem Zyklus
    const uint32_t dt = millis() - t0;
    if ((int32_t)wait_pre - (int32_t)dt > 0) {
      delay((uint32_t)((int32_t)wait_pre - (int32_t)dt));
    }
  }
  // Kurzer Trigger-Impuls (falls extern genutzt)
  digitalWrite(TRIG_PIN, HIGH);
  digitalWrite(TRIG_PIN, LOW);

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    uint32_t len = fb->len;
    Serial.write(reinterpret_cast<uint8_t*>(&len), sizeof(len));
    Serial.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
  }

  // Restwartezeit bis zum Ende des Basis-Intervalls (ohne Offset erneut zu addieren)
  // Basis-Idee: pro Loop genau EIN Offset einrechnen (vor der Aufnahme)
  const uint32_t dt_total = millis() - t0;
  if (dt_total < (uint32_t)targetInterval) {
    delay(targetInterval - dt_total);
  }
}
