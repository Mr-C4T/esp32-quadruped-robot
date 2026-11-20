#include <esp_now.h>
#include <WiFi.h>

// Adresse MAC de l'esp32 récepteur (Robot)
uint8_t broadcastAddress[] = {0xA0, 0xB7, 0x65, 0x6E, 0x44, 0xD0};

// Variable pour stocker la commande
String command = "";

// Fonction de callback pour confirmer la réception du message
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Dernière transmission : ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Réussie" : "Echouée");
    
}

void setup() {
    Serial.begin(115200);
    // Mettre l'ESP32 en mode station
    WiFi.mode(WIFI_STA);

    // Initialiser ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Erreur lors de l'initialisation d'ESP-NOW");
        return;
    }

    // Enregistrer la fonction de callback de transmission
    esp_now_register_send_cb(OnDataSent);

    // Configurer les informations du peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));  // Initialiser à zéro
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;

    // Ajouter le peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Erreur lors de l'ajout du pair");
        return;
    }
}

void loop() {
    if (Serial.available() > 0) {
        command = Serial.readStringUntil('\n');

        // Envoyer la commande à l'adresse MAC du récepteur
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)command.c_str(), command.length() + 1);

        if (result == ESP_OK) {
            Serial.println("Commande envoyée avec succès");
            Serial.println(command);
        } else {
            Serial.println("Erreur lors de l'envoi de la commande");
        }
    }
}
