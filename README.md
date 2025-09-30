# Memento / ESP32-S3 Kamera – Aufnahmesteuerung & Bildanalyse

Dieses Repository enthält zwei Hauptbestandteile:

1. Firmware (Arduino / PlatformIO) für das Adafruit "Camera ESP32-S3" Board zur wiederholten Aufnahme und seriellen Übertragung von JPEG-Bildern (hohe Baudrate) – gedacht für eine Anwendung an einem Förderband / Bandapplikator.
2. Python-Tools zum Empfangen (Speichern) und anschließenden Analysieren der aufgenommenen Bilder (Lage- / Offset- und Rotationsauswertung eines Labels anhand der oberen Kante).

Die Analyse vergleicht aktuell jedes Bild relativ zum allerersten Referenzbild (erstes im Zeitstempelordner). Dabei werden Offsets in mm & px sowie absolute Eckpunktkoordinaten ausgegeben.

---
## Verzeichnisübersicht

| Pfad / Datei | Zweck |
|--------------|-------|
| `platformio.ini` | Build-/Upload-Konfiguration für das ESP32-S3 Kamera Board (Ports, Flags, Libraries) |
| `src/main.cpp` | Firmware: Aufnahme-Loop, Trigger-Logik, Timing aus Bandgeschwindigkeit/Abstand/Offset, Kamera-Parameter |
| `image_receiver.py` | Empfängt JPEG-Frames seriell (COM7 @ 5.000.000 Baud) und speichert sie datumssortiert ab |
| `image_compare.py` | Extrahiert obere Labelkante, berechnet Geometrie & Abstände, erzeugt CSV-Ergebnis |
| `requirements.txt` | Python-Abhängigkeiten (OpenCV, numpy, pyserial, Pillow) |
| `out/` | Ausgabeverzeichnis für Analyse-Overlays & `vergleichsergebnisse.csv` |
| `2025-09-29/`, `2025-09-28/`, ... | Tagesordner mit aufgenommenen Bildserien |
| `roh/`, `out_old/`, `out_test/` (falls genutzt) | Historische / alternative Ablagen (nicht Kernbestandteil) |

---
## Datenfluss / Pipeline
 
1. Firmware löst zyklisch eine Aufnahme aus → Bild wird als JPEG über die serielle Schnittstelle (USB CDC) gesendet.
2. `image_receiver.py` liest: [4 Bytes Länge little-endian] + [Bilddaten] und schreibt Datei `image_<YYYYMMDD>_<HHMMSS>_<µs>.jpg` in einen Tagesordner `YYYY-MM-DD`.
3. Nach Abschluss / genug Bildern: `image_compare.py` starten.
4. Skript sammelt Bilder aus `INPUT_DIR`, nimmt das erste als Referenz und vergleicht alle weiteren ausschließlich gegen dieses eine.
5. Ergebnisse → `out/vergleichsergebnisse.csv` + Analyse-Overlays (sofern `SAVE_OVERLAY=True`).

---
## Firmware (PlatformIO) – Relevante Parameter

### `platformio.ini`
 
| Parameter | Beschreibung | Wann anpassen |
|-----------|--------------|---------------|
| `board = adafruit_camera_esp32s3` | Ziel-Board | Nur ändern falls anderes ESP32-S3 Board |
| `upload_port = COM4` | Port zum Flashen | Wenn sich der Flash-Port ändert (Geräte-Manager prüfen) |
| `monitor_port = COM7` | Serielle Ausgabe / Bilddaten (hier: Datenausgabe) | Falls anderes Interface vom System zugewiesen |
| `monitor_speed = 115200` | Konsolen-Baudrate (nur Log, nicht Bilddaten) | Selten nötig |
| `upload_speed = 115200` | Flash-Geschwindigkeit | Höhere Werte möglich, falls stabil |
| `build_flags` USB_* | Aktiviert CDC (seriell) auf Boot | Normalerweise belassen |
| `lib_deps` | Externe Libraries (AW9523, SdFat) | Automatisch installiert |

### `src/main.cpp`
 
| Makro / Variable | Bedeutung | Einheit | Anpassung vor Einsatz |
|------------------|-----------|--------|----------------------|
| `LED_PIN` | Pin für NeoPixel-Ring | GPIO | Nur falls Layout anders |
| `LED_COUNT` | Anzahl LEDs im Ring | Stück | An tatsächliche LED-Anzahl anpassen |
| `BAND_SPEED_MIN` / `MAX` | Begrenzungsgeschwindigkeit | m/min | Sicherheitsrahmen |
| `BAND_SPEED` | Eingesetzte Bandgeschwindigkeit | m/min | Auf tatsächliche Fördergeschwindigkeit setzen (wird geklemmt) |
| `ABSTAND_M` | Distanz Lichtschranke → Aufnahmeposition | m | Exakt einmessen |
| `OFFSET_CM` | Zeitlicher Versatz: >0 später, <0 früher | cm | Feinjustierung Aufnahmezeitpunkt |
| `AEC_VALUE_ACTION` | Manuelle Belichtungs-Vorgabe | Registerwert (kürzer = kleiner) | Bei Über-/Unterbelichtung anpassen |
| `ENABLE_LIMITED_AGC` | Auto-Gain leicht erlaubt? | bool | Nur aktivieren falls zu dunkel |
| `GAIN_CEILING` | Max. Gain (Rauschen) | Faktor (enum) | Erhöhen bei Dunkelheit (z.B. 4) |
| `ENABLE_AWB` | Auto Weißabgleich | bool | Konstante Farbtemperatur? Dann aus |
| `JPEG_QUALITY` | JPEG-Qualität (niedriger = besser) | 0..63 | Für Balance Größe/Details |

Aufnahmetiming erfolgt über:

```text
t_ms = (ABSTAND_M / BAND_SPEED  +  (OFFSET_CM/100 / BAND_SPEED)) * 60000
```
Danach berücksichtigt die Firmware Zeit, die durch Trigger-Delays bereits verstrichen ist.

---
## Python-Skripte – Parameter & Anpassungen

### `image_receiver.py`
 
| Stelle | Bedeutung | Standard | Anpassen wenn |
|--------|-----------|----------|---------------|
| `serial.Serial('COM7', 5000000, timeout=5)` | Empfangsport + hohe Baudrate | COM7 / 5.000.000 | Port anders / Instabilität (Baud ggf. senken) |
| Dateiname `image_<timestamp>.jpg` | Eindeutige Speicherung | – | Nicht nötig |
| Tagesordner `YYYY-MM-DD` | Gruppierung | Heute | Archivierung/Sortierung |

Hinweis: 5.000.000 Baud erfordert gutes USB-Kabel / stabile Verbindung. Bei Fehlern testweise 2000000 ausprobieren.

### `image_compare.py`
 
| Variable | Bedeutung | Standard | Anpassen |
|----------|-----------|----------|----------|
| `LABEL_TOP_LENGTH_CM` | Physische Länge der erkannten oberen Labelkante | 9.7 | Auf tatsächliche Labelbreite messen – wichtig für Skala |
| `INPUT_DIR` | Quellordner der Bilder | `2025-09-29` | Auf gewünschten Tagesordner setzen |
| `SAVE_OVERLAY` | Speichert Analysebilder mit Markierungen | True | Ausschalten für Geschwindigkeit |
| `OUT_DIR` | Ausgabeordner für CSV & Overlays | `out` | Anderer Pfad / Netzwerkziel |

Weitere interne Berechnungen:

* Erste Datei im Ordner = Referenz.
* Alle anderen werden gegen diese eine verglichen.
* Erkennung: stärkste Label-Komponente → obere Kante → Line-Fit → Winkel & Skala.

---
## Ausführliche Nutzungsschritte
 
1. Parameter in `main.cpp` (Bandgeschwindigkeit, Abstand, Offset) setzen.
1. Board in Boot/Download-Modus versetzen (Boot gedrückt halten → Reset kurz → beide loslassen).
1. Firmware flashen (VSCode: PlatformIO Upload oder CLI siehe unten).
1. Empfangsport prüfen (Geräte-Manager → COM-Nummern). Falls nötig `platformio.ini` / `image_receiver.py` anpassen.
1. Python-Umgebung vorbereiten:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```
1. Empfang starten:

```powershell
python image_receiver.py
```
1. Aufnahmen erzeugen lassen (Board läuft autonom). Bilder erscheinen im Tagesordner.
1. Analyse konfigurieren (`image_compare.py`: `LABEL_TOP_LENGTH_CM`, `INPUT_DIR`).
1. Analyse starten:

```powershell
python image_compare.py
```
1. Ergebnisse in `out/vergleichsergebnisse.csv` prüfen.


---
## CSV-Felder (Analyse)

Datei: `out/vergleichsergebnisse.csv`

| Spalte | Inhalt |
|--------|--------|
| Vergleich/Metriken | Referenzbild → Vergleichsbild (Dateinamen) |
| Orthogonaler Abstand (B relativ zu A) | Mittelpunktsverschiebung entlang Normale (mm \| px) |
| Rotationsdifferenz | Winkeländerung (°) relativ zur Referenzkante |
| Linke Ecke (B relativ zu A) | Versatz linke obere Kante (mm \| px) |
| Rechte Ecke (B relativ zu A) | Versatz rechte obere Kante (mm \| px) |
| linker eckpunkt absolut | Absolute (x,y) Pixelkoordinate der linken oberen Kantenposition des Vergleichsbildes |
| rechter eckpunkt absolut | Absolute (x,y) Pixelkoordinate der rechten oberen Kantenposition des Vergleichsbildes |
| 1px in cm | Umrechnungsfaktor (Pixel → cm) aus Referenzbild (gleicher Wert für alle Zeilen einer Serie) |

Hinweis: Falls `vergleichsergebnisse.csv` noch geöffnet (Excel etc.), kann ein PermissionError auftreten – Datei schließen und erneut starten.

---
## Beispielinterpretation

Ein Eintrag wie:

```text
Orthogonaler Abstand: 8.845869 mm | 72.234482 px
Rotationsdifferenz:  -0.342639 °
```

→ Das neue Label liegt ~8.85 mm weiter "oben/unten" (je nach Normenrichtung) relativ zum Referenzbild; minimale Drehung.

---
 
## Typische Anpassungen vor Einsatz

| Ziel | Relevante Stellen |
|------|------------------|
| Andere Bandgeschwindigkeit | `BAND_SPEED` in `main.cpp` |
| Verzögerung feintrimmen | `OFFSET_CM` in `main.cpp` |
| Falsche Skalierung mm | `LABEL_TOP_LENGTH_CM` in `image_compare.py` |
| Neuer Aufnahmetag | `INPUT_DIR` aktualisieren |
| Kein Overlay nötig | `SAVE_OVERLAY=False` |
| Portänderung Flash | `upload_port` (platformio.ini) |
| Portänderung Empfang | `monitor_port` + `image_receiver.py` (COM) |

---
 
## Troubleshooting

| Problem | Mögliche Ursache | Lösung |
|---------|------------------|--------|
| `PermissionError` beim CSV-Schreiben | Datei in Excel geöffnet | Datei schließen, Skript erneut starten |
| Keine Bilder empfangen | Falscher COM-Port / Baudrate | Gerätemanager prüfen, `image_receiver.py` anpassen |
| Bilder zu dunkel / verwischt | Belichtungszeit zu lang | `AEC_VALUE_ACTION` kleiner, Gain begrenzen, LED-Helligkeit erhöhen |
| Starke Ränder / Verzerrung | Gain zu hoch / Rauschen | `GAIN_CEILING` reduzieren, Licht verbessern |
| Falsche mm-Werte | `LABEL_TOP_LENGTH_CM` falsch | Exakt nachmessen und anpassen |
| Rotationswerte springen | Kante schlecht erkannt | Gleichmäßig beleuchten, Label-Kontrast erhöhen |

---
 
## PlatformIO CLI (optional)

```powershell
# Abhängigkeiten holen
pio lib install

# Build
pio run

# Upload
pio run --target upload

# Seriellen Monitor (nur Log, Bilder laufen über High-Speed Kanal)
pio device monitor -p COM7 -b 115200
```

---
 
## Erweiterungsideen (Future Work)

* Retry/Lock-Handling beim CSV-Schreiben (alternativer Name bei Sperre)
* Automatische Erkennung des neuesten Tagesordners für Analyse
* Speicherung der Referenzparameter (Skalierung) in JSON neben CSV
* Live-Vorschau der Offsets (Plot) während Empfang
* Option: Vergleich jedes Bildes zum vorigen UND zum ersten

---
 
## Lizenz / Nutzung

Interner Prototyp / Technologiedemo. Bei geplanter Weitergabe bitte klären, ob Open-Source-Lizenz ergänzt werden soll.

---
 
## Kurzstart (Cheat Sheet)
 
```powershell
# 1. Flash vorbereiten
Adjust src/main.cpp (BAND_SPEED / ABSTAND_M / OFFSET_CM) & platformio.ini (Ports)
pio run --target upload

# 2. Empfang starten
python image_receiver.py

# 3. Analyse (Parameter vorher setzen)
python image_compare.py
```

Fragen oder Erweiterungswunsch? Einfach melden. dennis.heine@bizerba.com
