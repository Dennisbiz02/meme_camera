# main: Dokumentation des Kamera-Trigger-Systems mit Arduino und ESP32-CAM

Dieses Projekt implementiert ein automatisiertes Kamerasystem, das auf Basis einer Bandgeschwindigkeit, eines Abstands zur Zielposition und eines konfigurierbaren Offsets Bilder zu einem präzisen Zeitpunkt auslöst.  

Die Steuerung erfolgt über einen ESP32 mit angeschlossener Kamera und einem LED-Ring zur Beleuchtung.

---

## Übersicht

Das Programm erfüllt folgende Aufgaben:
- Initialisierung und Steuerung eines LED-Rings als konstante Lichtquelle.  
- Konfiguration der ESP32-Kamera mit festen Parametern für kurze Belichtungszeiten.  
- Auslösung eines Trigger-Signals (z. B. für eine Lichtschranke).  
- Berechnung der erforderlichen Wartezeit bis zur Bildaufnahme basierend auf **Bandgeschwindigkeit**, **Abstand** und **Offset**.  
- Aufnahme eines Kamerabildes und Übertragung über die serielle Schnittstelle.  

---

## Hardware-Komponenten

- **ESP32-CAM** mit `esp_camera` Bibliothek  
- **Adafruit PyCamera** Bibliothek zur vereinfachten Kamerasteuerung  
- **Adafruit NeoPixel LED-Ring** mit 12 LEDs (WS2812-kompatibel)  
- **Trigger-Pin** (GPIO 17) zur Ausgabe eines Signals für eine Lichtschranke oder externe Synchronisation  

---

## Globale Definitionen

### LED-Ring
```cpp
#define LED_PIN    18
#define LED_COUNT  12
```

- LED-Ring an GPIO 18 mit 12 LEDs.

- Wird als weiße Dauerlichtquelle verwendet.

## Bandgeschwindigkeit und Abstand
```cpp
#define BAND_SPEED_MIN 1
#define BAND_SPEED_MAX 180
#define BAND_SPEED     60   // m/min
#define ABSTAND_M      1.0  // m
#define OFFSET_CM      0    // cm
```

- BAND_SPEED = Geschwindigkeit des Förderbands in Meter pro Minute.

- ABSTAND_M = Distanz zwischen Lichtschranke und Zielposition in Metern.

- OFFSET_CM = Feinkorrektur in Zentimetern (positiv = spätere Auslösung, negativ = frühere Auslösung).

- BAND_SPEED wird intern auf den Bereich [BAND_SPEED_MIN, BAND_SPEED_MAX] begrenzt.

## Trigger
```cpp
static const int TRIG_PIN = 17;
```

- Digitaler Ausgang, der zu Beginn der Schleife ein Signal (HIGH-Impuls) setzt.

- Kann zur Synchronisation mit externer Hardware (z. B. Lichtschranke) genutzt werden.

## Wichtige Funktionen
clampSpeed
```cpp
static inline double clampSpeed(double speed_m_per_min);
```

- Stellt sicher, dass die Bandgeschwindigkeit im definierten Bereich liegt.

- Rückgabewert: korrigierte Geschwindigkeit in m/min.

computeWaitMs
```cpp
static inline int32_t computeWaitMs(double distance_m, double speed_m_per_min, int offset_cm);
```

- Berechnet die Zeit in Millisekunden, die abgewartet werden muss, bis das Objekt die Zielposition erreicht.

- Formel:

\[
t = \left( \frac{\text{Abstand}}{v} + \frac{\text{Offset}}{100 \cdot v} \right) \times 60000
\]


- distance_m = Abstand in Metern.

- speed_m_per_min = Bandgeschwindigkeit in m/min.

- offset_cm = Korrektur in Zentimetern.

Beispiel:

- Abstand = 1 m, Bandgeschwindigkeit = 10 m/min →
\[
t = \left( \frac{1}{10} + \frac{0}{100 \cdot 10} \right) \times 60000 = 6000 \text{ ms}
\]

## Kamera-Konfiguration

Die Funktion applyActionPhotoProfile(sensor_t* s) konfiguriert die Kamera für Action-Aufnahmen:

- Pixelformat: JPEG

- Auflösung: SXGA (1280×1024)

- Qualität: hoch (JPEG-Quality = 15)

- Bildverbesserung: Denoise, BPC, WPC, Linsen-Korrektur aktiviert

- Schärfe: +2, Sättigung/Kontrast/Brightness = 0

- Weißabgleich: aktiv

- Belichtung: manuell, kurze Belichtungszeit (AEC_VALUE_ACTION = 60, AE-Level = -2)

- Gain: begrenzt (AGC optional)

## Programmablauf

### Setup

1. Serielle Kommunikation initialisieren (Serial.begin(5000000)).

2. Kamera starten und konfigurieren.

3. LED-Ring einschalten und auf maximale Helligkeit setzen.

4. Trigger-Pin als Ausgang initialisieren.

### Loop

1. Startzeitpunkt speichern (millis()).

2. Berechnung der geplanten Wartezeit in Millisekunden (computeWaitMs).

3. Trigger-Signal setzen:
    - HIGH für 100 ms

    - anschließend LOW

    - danach 600 ms Pause

4. Verstrichene Zeit seit Loop-Beginn messen.

5. Verbleibende Wartezeit bis zur Bildaufnahme berechnen:

    - remaining_ms = planned_wait_ms - dt_after_trigger

    - Falls positiv → warten (delay).

6. Bildaufnahme durchführen:

    - Kamera-Frame (camera_fb_t) holen.

    - Bildlänge und Bilddaten seriell ausgeben.

    - Speicher freigeben.

## Zeitsteuerung im Detail

Die Gesamtlatenz bis zur Aufnahme setzt sich aus drei Teilen zusammen:

1. Triggerphase: 100 ms HIGH + 600 ms LOW = 700 ms

2. Geplante Wartezeit abhängig von Bandgeschwindigkeit, Abstand, Offset

3. Restliche Wartezeit (remaining_ms) nach Abzug der Triggerphase

Die Bildaufnahme erfolgt exakt dann, wenn das Objekt basierend auf den Eingabeparametern die Zielposition erreicht.

## Erweiterungsmöglichkeiten

- Parametrisierung von BAND_SPEED, ABSTAND_M und OFFSET_CM über serielle Schnittstelle.

- Speicherung der aufgenommenen Bilder auf SD-Karte.

- Erweiterung um mehrere Trigger-Pins für unterschiedliche Sensoren.

- Echtzeit-Anpassung der Belichtung durch Helligkeitssensoren.
