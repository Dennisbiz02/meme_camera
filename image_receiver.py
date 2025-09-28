import serial
import struct
import os
from datetime import datetime
import time


def receive_images():
    # COM7 mit 5000000 Baud öffnen
    ser = serial.Serial('COM7', 5000000, timeout=5)

    print("Warte auf Bilder von COM7...")

    # Performance-Tracking
    start_time = time.time()
    image_count = 0

    while True:
        try:
            # 4 Bytes für die Länge lesen
            len_data = ser.read(4)
            if len(len_data) != 4:
                continue

            # Länge als uint32 (LSB-first) interpretieren
            img_len = struct.unpack('<I', len_data)[0]

            if img_len > 0 and img_len < 10000000:  # Sinnvolle Bildgröße
                # Bilddaten lesen
                img_data = ser.read(img_len)

                if len(img_data) == img_len:
                    print(f"Empfange Bild der Größe {img_len} Bytes")
                    # Ordner mit aktuellem Datum erstellen
                    current_date = datetime.now().strftime("%Y-%m-%d")
                    folder_path = current_date

                    # Ordner erstellen falls nicht vorhanden
                    if not os.path.exists(folder_path):
                        os.makedirs(folder_path)

                    # Bild speichern mit Mikrosekunden für eindeutige Namen
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
                    filename = os.path.join(
                        folder_path, f"image_{timestamp}.jpg")

                    with open(filename, 'wb') as f:
                        f.write(img_data)

                    image_count += 1

                    # Performance-Ausgabe alle 10 Bilder
                    if image_count % 10 == 0:
                        elapsed = time.time() - start_time
                        fps = image_count / elapsed
                        print(
                            f"Bild {image_count}: {filename} ({img_len} Bytes) - {fps:.1f} FPS")

        except Exception as e:
            print(f"Fehler: {e}")
            continue


if __name__ == "__main__":
    receive_images()
