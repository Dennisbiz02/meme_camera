# Adafruit Memento Camera Board - Foto-Projekt

Dieses Projekt implementiert eine einfache Kamera-Funktionalität für das Adafruit Memento Camera Board mit ESP32-S3.

## Features

- ✅ Foto aufnehmen mit OK-Button
- ✅ LED-Blitzring aktivieren
- ✅ Bild auf LCD-Display anzeigen
- ✅ Bild über USB an PC übertragen und speichern
- ✅ Automatische Bildanzeige nach Aufnahme

## Hardware-Setup

### Adafruit Memento Camera Board
- ESP32-S3 mit integrierter OV7670 Kamera
- 240x135 Pixel ST7789 TFT Display
- 24-LED NeoPixel Ring (Blitz)
- 4 Buttons (OK, Select, Up, Down)
- USB-C Anschluss

### Anschluss
1. Verbinden Sie das Memento Board über USB-C mit Ihrem PC
2. Das Board wird automatisch mit Strom versorgt

## Software-Installation

### 1. PlatformIO-Projekt kompilieren und hochladen

```bash
# In das Projektverzeichnis wechseln
cd c:\Users\de_heind\OneDrive - Bizerba365\Dokumente\PlatformIO\Projects\meme_camera

# Abhängigkeiten installieren und Code kompilieren
pio lib install

# Code auf das Board hochladen
pio run --target upload

# Optional: Serial Monitor öffnen
pio device monitor
```

### 2. Python-Umgebung für Bildempfang einrichten

```bash
# Python-Abhängigkeiten installieren
pip install -r requirements.txt

# Bild-Empfänger starten
python image_receiver.py
```

## Bedienung

### ESP32 (Memento Board)
1. Nach dem Hochladen des Codes startet das Board automatisch
2. Das Display zeigt "Memento Camera - Drücke OK für Foto"
3. **OK-Button drücken** um ein Foto aufzunehmen
4. Der LED-Ring blitzt kurz auf
5. Das aufgenommene Bild wird auf dem Display angezeigt
6. Die Bilddaten werden über USB an den PC gesendet

### PC (Bildempfang)
1. `python image_receiver.py` ausführen
2. Das Script erkennt automatisch den COM-Port oder fragt danach
3. Bilder werden automatisch empfangen und gespeichert
4. Gespeicherte Bilder befinden sich im Ordner `captured_images/`
5. Dateiname: `memento_YYYYMMDD_HHMMSS.png`

## Troubleshooting

### Kamera-Initialisierung fehlgeschlagen
- Überprüfen Sie die Kabel-Verbindungen
- Stellen Sie sicher, dass die OV7670 richtig angeschlossen ist

### Display zeigt nichts an
- Überprüfen Sie die SPI-Verbindungen zum ST7789 Display
- Stellen Sie sicher, dass die Stromversorgung ausreicht

### Bildübertragung funktioniert nicht
- Überprüfen Sie den COM-Port in der Python-Anwendung
- Stellen Sie sicher, dass keine andere Software den Port blockiert
- Überprüfen Sie die Baudrate (115200)

### LED-Ring funktioniert nicht
- Überprüfen Sie die NeoPixel-Verbindung (Pin 33)
- Stellen Sie sicher, dass genügend Strom verfügbar ist

## Technische Details

### Pin-Belegung
```
Display (ST7789):
- CS: Pin 5
- DC: Pin 9  
- RST: Pin 6
- MOSI: Pin 35
- SCLK: Pin 36

Kamera (OV7670):
- SDA: Pin 4
- SCL: Pin 5
- VSYNC: Pin 15
- HREF: Pin 16
- PCLK: Pin 17
- XCLK: Pin 18
- D7-D0: Pins 19,20,21,47,48,45,46,44

NeoPixel Ring:
- Data: Pin 33

Buttons:
- OK: Pin 0
- Select: Pin 1  
- Down: Pin 2
- Up: Pin 3
```

### Bildformat
- Auflösung: 320x240 Pixel (QVGA)
- Format: RGB565 (intern), RGB888 (übertragen)
- Übertragung: Hex-kodiert über Serial
- Speicherung: PNG-Format

## Erweiterungsmöglichkeiten

- **Zeitraffer-Modus**: Automatische Fotos in Intervallen
- **Bewegungserkennung**: Foto bei Bewegung
- **WiFi-Übertragung**: Bilder über WiFi statt USB übertragen
- **SD-Karten-Speicherung**: Lokale Speicherung auf SD-Karte
- **Verschiedene Fotoeffekte**: Filter und Bildbearbeitung

## Lizenz

MIT License - Frei verwendbar für private und kommerzielle Projekte.
