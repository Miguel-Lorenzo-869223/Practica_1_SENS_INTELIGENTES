# ------------------------------------------------------------
#  Author: Miguel A.Lorenzo
#  Date: 31/10/2026
#  Subject: Smart Sensors Laboratory
#  Master: MSc in Electronic Engineering
#  University: University of Zaragoza EINA/UNIZAR
# ------------------------------------------------------------
"""
===============================================================================
Serial Data Logger to CSV
===============================================================================
Reads incoming serial data from Arduino and saves it into a .csv file.
Includes proper header and comma (,) formatting for direct Edge Impulse upload.
===============================================================================
"""

import serial
import time
import os

# --- CONFIGURACIÓN ---
PORT = "COM5"             # Puerto serie del Arduino (actualizar según Administrador de dispositivos)
BAUD = 115200             # Baudrate (coincide con Serial.begin())
FILE_NAME = "roll_pos.csv" # Cambia este nombre según la etiqueta (ej: pitch_pos.csv, yaw_neg.csv)

# Cabecera estándar para IMU de 6 ejes requerida por Edge Impulse Studio
CSV_HEADER = "accX,accY,accZ,gyroX,gyroY,gyroZ\n"


def open_serial():
    """Intenta establecer conexión serie continuamente hasta tener éxito."""
    while True:
        try:
            print(f"Conectando a {PORT}...")
            ser = serial.Serial(PORT, BAUD, timeout=1)
            print("Puerto serie conectado con éxito\n")
            return ser
        except serial.SerialException:
            print("Fallo al abrir el puerto. Reintentando en 1 segundo...")
            time.sleep(1)


def main():
    ser = open_serial()

    # Comprobar si el archivo existe o si está vacío para escribir la cabecera
    file_exists = os.path.isfile(FILE_NAME)

    with open(FILE_NAME, mode='a', encoding='utf-8') as file:
        # Si el archivo es nuevo o no tiene contenido, añadir la primera línea con los nombres de las variables
        if not file_exists or os.stat(FILE_NAME).st_size == 0:
            file.write(CSV_HEADER)
            file.flush()
            print(f"Cabecera escrita en '{FILE_NAME}': {CSV_HEADER.strip()}")

        print(f"📄 Registrando datos en '{FILE_NAME}'... Presiona Ctrl+C para detener.\n")

        while True:
            try:
                if ser.in_waiting > 0:
                    raw_line = ser.readline().decode('utf-8', errors='ignore').strip()

                    if raw_line:
                        # Asegurar que el delimitador sea coma (,) y no punto y coma (;) o espacios
                        formatted_line = raw_line.replace(";", ",").replace(" ", ",")

                        # Añadir salto de línea y guardar
                        file_entry = f"{formatted_line}\n"
                        file.write(file_entry)
                        file.flush()

                        print(f"Registrado: {formatted_line}")
                else:
                    time.sleep(0.01)

            except serial.SerialException:
                print("Conexión perdida. Reconectando...")
                ser = open_serial()

            except KeyboardInterrupt:
                print("\nPrograma terminado por el usuario.")
                ser.close()
                break


if __name__ == "__main__":
    main()