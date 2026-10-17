// plateforme.h - ce que le coeur du programme demande au materiel.
// Implemente par lumiere_console.ino (carte ES3C28P) et par le simulateur PC
// (outils/simulateur/sim_main.cpp). Le reste du code (ecrans, calculs,
// dessin) est identique sur les deux.
#pragma once
#include <stdint.h>
#include <stddef.h>

uint32_t plat_ms();                       // millisecondes depuis le demarrage
void plat_verrou();                       // section critique (tache Wi-Fi <-> interface)
void plat_deverrou();
void plat_luminosite(int pct);            // retroeclairage 5..100 %
void plat_source(int source);             // 0 = carte A reelle, 1 = simulation
bool plat_carte_a();                      // repondeur FTM trouve ?
bool plat_sd();                           // carte SD utilisable ?
void plat_csv(float d, const uint32_t *rtt_ps, const int8_t *rssi, int n);
bool plat_nvs_lire(const char *cle, void *buf, size_t n);
void plat_nvs_ecrire(const char *cle, const void *buf, size_t n);
uint32_t plat_memoire_libre();            // octets de memoire interne libres
