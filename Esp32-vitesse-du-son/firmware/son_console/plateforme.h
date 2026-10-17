// plateforme.h - ce que le coeur du programme demande au materiel.
// Implemente par son_console.ino (carte ES3C28P) et par le simulateur PC
// (outils/simulateur/sim_main.cpp). Le reste du code (ecrans, calculs,
// traitement du signal, dessin) est identique sur les deux.
#pragma once
#include <stdint.h>
#include <stddef.h>

uint32_t plat_ms();                       // millisecondes depuis le demarrage
void plat_verrou();                       // section critique (tache audio <-> interface)
void plat_deverrou();
void plat_luminosite(int pct);            // retroeclairage 5..100 %

// ---- audio (codec ES8311 : haut-parleur + micro, 48 kHz, meme horloge) ----
bool plat_audio_ok();                     // codec trouve et test haut-parleur -> micro reussi
const char *plat_audio_etat();            // texte court pour l'ecran
void plat_audio_niveaux(int volume, int gain);       // 0 doux/bas .. 2 fort/haut
// emet tx et enregistre rx en meme temps (n echantillons, bloquant)
bool plat_audio_ping(const int16_t *tx, int16_t *rx, int n);
// son pur continu (Doppler) : f0 en Hz, 0 = arret (montee/descente douces)
void plat_audio_tonalite(float f0, float amp);
// echantillons du micro pendant la tonalite (bloquant, false si delai depasse)
bool plat_audio_lire(int16_t *rx, int n);

bool plat_sd();                           // carte SD utilisable ?
void plat_csv(const char *ligne);         // ajoute une ligne a son.csv
bool plat_nvs_lire(const char *cle, void *buf, size_t n);
void plat_nvs_ecrire(const char *cle, const void *buf, size_t n);
uint32_t plat_memoire_libre();            // octets de memoire interne libres
