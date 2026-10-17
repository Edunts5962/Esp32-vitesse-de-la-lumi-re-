// dsp.h - traitement du signal audio (identique sur la carte et sur le PC).
//
// ECHO    : la carte emet un "chirp" (glissando 1,5 -> 6,5 kHz, 10 ms).
//           Le micro entend d'abord le son direct (haut-parleur -> micro,
//           quelques cm) puis l'echo du mur. Un filtre adapte (correlation
//           avec le chirp, faite par FFT) transforme chaque arrivee en un
//           pic fin (~0,2 ms) : on mesure l'ecart direct -> echo au
//           1/48 000 s pres, affine par interpolation.
// DOPPLER : la carte emet un son pur f0 en continu. Une personne qui bouge
//           renvoie un son decale de df = f0 (c+v)/(c-v) - f0 ~ 2 v f0 / c.
//           Une FFT de 8192 points (resolution 5,9 Hz) separe ce petit
//           decalage de la porteuse directe, beaucoup plus forte.
#pragma once
#include <stdint.h>

namespace dsp {

const int FS = 48000;              // echantillons par seconde
const int N_ECHO = 8192;           // enregistrement d'un ping (171 ms)
const int CHIRP_N = 480;           // duree du chirp (10 ms)
const int CHIRP_DEBUT = 480;       // position du chirp dans le tampon emis
const float CHIRP_F0 = 1500.0f;
const float CHIRP_F1 = 6500.0f;

const int N_DOP = 8192;            // fenetre d'analyse Doppler (171 ms)
const int SAUT_DOP = 4096;         // nouvelle analyse toutes les 85 ms (recouvrement 50 %)
const int BANDE_DOP = 64;          // bins etudies de chaque cote de f0 (+-375 Hz)
const int NB_SPECTRE = 65;         // lignes du spectrogramme affiche (+-32 bins, +-188 Hz)
const int EXCLU_DOP = 4;           // bins ignores autour de la porteuse (lobe de la fenetre)

const int ENV_AFF = 220;           // points de l'echogramme affiche

void init();                       // tables, chirp, fenetres (malloc -> PSRAM sur la carte)
const int16_t *chirp();            // tampon a emettre (N_ECHO echantillons, chirp a CHIRP_DEBUT)
uint32_t nb_fft();                 // FFT calculees depuis le demarrage

// ---------------------------------------------------------------- echo
struct Echo {
    bool ok;                       // echo valide trouve dans la fenetre
    float i_direct, i_echo;        // positions des pics (echantillons, sous-echantillon)
    float dt_us;                   // ecart direct -> echo (microsecondes)
    float snr_db;                  // echo / bruit
    float snr_direct_db;           // son direct / bruit (test du micro)
    float amp_echo;                // echo / direct (0..1)
    int satur;                     // echantillons satures (micro trop fort)
    // echogramme pour l'affichage : 0 .. ENV_AFF-1 couvre [direct - marge, direct + duree]
    uint8_t env[ENV_AFF];
    float aff_ms;                  // duree couverte apres le direct (ms)
    float f_echo, f_fen0, f_fen1;  // positions relatives (0..1) : pic d'echo, fenetre de recherche
    float d;                       // distance visee pendant ce ping (m)
};

// d_attendue : distance reglee (m), c_attendue : vitesse prevue (m/s) pour placer la fenetre
void analyser_echo(const int16_t *rx, float d_attendue, float c_attendue, Echo &r);

// ---------------------------------------------------------------- doppler
struct Doppler {
    bool detect;                   // quelque chose bouge
    float df_hz;                   // decalage (barycentre de l'energie, signe = sens)
    float df_pic_hz;               // decalage du pic le plus fort
    float snr_db;                  // pic / plancher de bruit
    float porteuse_db;             // niveau de la porteuse directe (dB plein echelle)
    float plancher_db;
    float spectre_db[2 * BANDE_DOP + 1];   // spectre autour de f0 (dB)
    uint8_t colonne[NB_SPECTRE];   // colonne du spectrogramme (0..255)
};

// frequence vraiment emise : la plus proche de f0 qui tombe pile sur un bin de la FFT
// (l'emission et l'enregistrement partagent la meme horloge : aucune fuite spectrale)
float frequence_emise(float f0);

// rx : N_DOP echantillons (fenetre glissante), f0 = frequence_emise(...)
void analyser_doppler(const int16_t *rx, float f0, Doppler &r);

// ---------------------------------------------------------------- generation
// son pur continu (phase conservee d'un appel a l'autre)
void tonalite(int16_t *tx, int n, float f0, float amp, double &phase);

// ---------------------------------------------------------------- simulation
// enregistrement realiste d'un ping : direct + echo du mur a d + sol/table + reverberation + bruit
void synth_echo(int16_t *rx, float d, float c, uint32_t &graine);

// enregistrement Doppler : porteuse directe + reflexion d'une personne a la vitesse v (m/s, >0 = approche)
struct EtatSynthDop { double ph_dir, ph_corps, ph_bras, t; uint32_t graine; };
void synth_doppler(int16_t *rx, int n, float f0, float v, float dist, float c, EtatSynthDop &e);

}  // namespace dsp
