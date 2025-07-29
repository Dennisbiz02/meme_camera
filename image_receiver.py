import serial
import struct
import os
from datetime import datetime


def receive_images():
    # COM7 mit 115200 Baud öffnen
    ser = serial.Serial('COM7', 115200, timeout=5)

    print("Warte auf Bilder von COM7...")

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
                    # Ordner mit aktuellem Datum erstellen
                    current_date = datetime.now().strftime("%Y-%m-%d")
                    folder_path = current_date

                    # Ordner erstellen falls nicht vorhanden
                    if not os.path.exists(folder_path):
                        os.makedirs(folder_path)

                    # Bild speichern
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = os.path.join(
                        folder_path, f"image_{timestamp}.jpg")

                    with open(filename, 'wb') as f:
                        f.write(img_data)

                    print(f"Bild gespeichert: {filename} ({img_len} Bytes)")

        except Exception as e:
            print(f"Fehler: {e}")
            continue


if __name__ == "__main__":
    receive_images()
