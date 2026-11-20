import serial
import time

# Configuration du port série
ser = serial.Serial('COM12', 115200)  # Remplacez 'COM3' par le port série approprié pour votre ESP32

def send_command(command):
    ser.write((command + '\n').encode())

if __name__ == "__main__":
    print("Communication série avec l'ESP32")
    time.sleep(2)  # Attendre que la connexion série soit établie

    try:
        while True:
            command = input("Entrez une commande à envoyer à l'ESP32: ")
            send_command(command)
            print(f"Commande '{command}' envoyée")
    except KeyboardInterrupt:
        print("Fermeture du programme")
    finally:
        ser.close()
