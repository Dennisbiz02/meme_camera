#include <Arduino.h>
#include "Adafruit_PyCamera.h"
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

#define LED_PIN    18       // A1 → GPIO18
#define LED_COUNT  12       // Anzahl der LEDs im Ring
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Bandgeschwindigkeit (1=min, 180=max)
#define BAND_SPEED_MIN 1
#define BAND_SPEED_MAX 180
#define BAND_SPEED 60      // Einstellen: 60→1 fps, 180→3 fps

// Berechnung des Zeitintervalls in ms basierend auf der Bandgeschwindigkeit
static inline uint32_t getIntervalMs(int speed) {
  if (speed < BAND_SPEED_MIN) speed = BAND_SPEED_MIN;
  if (speed > BAND_SPEED_MAX) speed = BAND_SPEED_MAX;
  return 60000UL / speed;
}

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

  // 1. Software-Denoising (0 … 8, Default 0)
  sensor->set_denoise(sensor, 7);
  // Denoise reduziert Rauschartefakte auf Kosten feiner Details :contentReference[oaicite:0]{index=0}

  // 2. Gain-Limitierung
  sensor->set_gainceiling(sensor, static_cast<gainceiling_t>(2));
  // Begrenzung der Verstärkung auf maximal 2×, um Sensorrauschen bei schwachem Licht zu verringern :contentReference[oaicite:1]{index=1}
  
  // 3. Pixel- und Weißfehlerkorrektur
  sensor->set_bpc(sensor, true);
  sensor->set_wpc(sensor, true);
  sensor->set_lenc(sensor, true);
  // BPC/WPC korrigieren „Hot“/„Dead“-Pixel, LENC kompensiert Helligkeitsabfall am Bildrand :contentReference[oaicite:2]{index=2}

  // 4. Automatische Weißabstimmung und Belichtungs-/Verstärkungssteuerung
  // sensor->set_awb_gain(sensor, true);
  // sensor->set_whitebal(sensor, true);
  // sensor->set_agc(sensor, false); // Funktion entfernt, AGC nicht verfügbar
  // sensor->set_aec(sensor, false);
  // AWB und Weißabgleich aktivieren, AGC/AEC abschalten für manuelle Kontrolle :contentReference[oaicite:3]{index=3}

  // 5. Manuelle Belichtungszeit (nur bei abgeschalteter AEC)
  sensor->set_exposure_ctrl(sensor, 1);
  sensor->set_aec_value(sensor, 200);
  // Exposition auf 200 Einheiten festlegen (abhängig von XCLK/PCLK) :contentReference[oaicite:4]{index=4}

  // Ausgangspin konfigurieren
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void loop() {
  uint32_t t0 = millis();
  // Berechnetes Intervall basierend auf Bandgeschwindigkeit
  uint32_t targetInterval = getIntervalMs(BAND_SPEED);

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
  if (dt < targetInterval) {
    delay(targetInterval - dt);
  }
}
