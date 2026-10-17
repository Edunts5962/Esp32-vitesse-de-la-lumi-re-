// =====================================================================
// lumiere_repondeur.ino - v1.1
// Carte A du projet "Vitesse de la lumiere" : repondeur FTM fixe.
//
// Cartes compatibles : ESP32-C3, ESP32-C6, ESP32-S2, ESP32-S3.
// PAS l'ESP32 classique (ton coprocesseur video) : il ne gere pas le FTM.
//
// v1.1 : point d'acces regle comme l'exemple FTM officiel d'ESP-IDF :
// largeur de canal 20 MHz (40 MHz possible par defaut) et, sur ESP32-C6,
// Wi-Fi 4 (b/g/n) au lieu du Wi-Fi 6. Affiche aussi les cartes connectees.
//
// Arduino IDE : paquet esp32 3.x. Pour un ESP32-C3/C6 Super Mini, activer
// "USB CDC On Boot" pour voir les messages sur le port serie.
// =====================================================================
#include <WiFi.h>
#include "esp_wifi.h"

#define SSID_LUMIERE  "C_LUMIERE"      // doit etre identique sur la carte B
#define MDP_LUMIERE   "lumiere299792"
#define CANAL_WIFI    6

static void regler_radio()
{
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    WiFi.mode(WIFI_AP);
    regler_radio();
    // softAP(ssid, mdp, canal, ssid_cache, connexions_max, repondeur_ftm)
    bool ok = WiFi.softAP(SSID_LUMIERE, MDP_LUMIERE, CANAL_WIFI, 0, 4, true);
    regler_radio();
    WiFi.setSleep(false);

    wifi_bandwidth_t bw = WIFI_BW_HT40;
    uint8_t proto = 0;
    esp_wifi_get_bandwidth(WIFI_IF_AP, &bw);
    esp_wifi_get_protocol(WIFI_IF_AP, &proto);
    Serial.printf("[LUMIERE] Repondeur FTM %s - SSID %s, canal %d, MAC %s\n",
                  ok ? "actif" : "EN ECHEC", SSID_LUMIERE, CANAL_WIFI,
                  WiFi.softAPmacAddress().c_str());
    Serial.printf("[LUMIERE] Largeur %s MHz, Wi-Fi %s\n", bw == WIFI_BW_HT20 ? "20" : "40",
                  (proto & WIFI_PROTOCOL_11AX) ? "6 (ax)" : "4 (b/g/n)");
}

void loop()
{
    static uint32_t t = 0;
    if (millis() - t > 10000) {
        t = millis();
        Serial.printf("[LUMIERE] En service depuis %lu s - cartes connectees : %d\n",
                      (unsigned long)(millis() / 1000), WiFi.softAPgetStationNum());
    }
    delay(100);
}
