// son.h - coeur des deux experiences : donnees, points, droite, passages.
//
// ECHO    : tache audio --ajouter_ping()--> file protegee --tick()-->
//           ecart direct/echo en direct + pings du point en cours
//           --> point (mediane, rejet des aberrations, moyenne)
//           --> droite (temps aller-retour / distance) --> c = 2 / pente
// DOPPLER : tache audio --ajouter_trame()--> file --tick()--> vitesse
//           v = c.df / (2 f0 + df), historique, spectrogramme, passages
#pragma once
#include <stdint.h>
#include "dsp.h"

namespace son {

const int MAX_PTS = 24;
const int MAX_PINGS = 60;
const int MAX_PASSAGES = 12;
const int HIST_V = 240;                    // ~20 s d'historique de vitesse
const int SPEC_COLS = 160;                 // colonnes du spectrogramme
const float SAUT_S = (float)dsp::SAUT_DOP / dsp::FS;   // 85 ms entre deux analyses Doppler

enum { MODE_ECHO = 0, MODE_DOPPLER = 1 };
enum { SRC_REEL = 0, SRC_SIM = 1 };
enum { EV_RIEN = 0, EV_POINT, EV_ECHEC, EV_PASSAGE };

struct Point {
    float d;          // distance carte -> mur (m)
    float dt;         // ecart direct -> echo moyen apres filtrage (us)
    float err;        // erreur type de la moyenne (us)
    float sigma;      // ecart type des pings gardes (us)
    float snr;        // rapport signal/bruit moyen de l'echo (dB)
    uint16_t n;       // pings gardes
    uint16_t rejets;  // aberrations rejetees
};

struct Ajustement {
    bool ok;
    double a, sa;     // pente (us/m) et son incertitude
    double b;         // decalage a l'origine (us) : ecart haut-parleur/micro, retards
    double c, sc;     // vitesse du son (m/s) et incertitude
    double r2;
};

struct Histo {
    bool ok;
    int nb;
    float lo, hi;     // us
    float med, moy, sig;
    int nb_bins;
    uint16_t bins[48];
    uint16_t nmax;
};

struct Passage {
    float vmax;       // m/s (valeur absolue)
    float vmoy;
    float duree;      // s
    int8_t sens;      // +1 approche, -1 eloignement
};

// ---- reglages (ecrits par l'interface, lus par la tache audio) ----
extern volatile int mode;           // MODE_ECHO / MODE_DOPPLER
extern volatile int source;         // SRC_REEL / SRC_SIM
extern volatile int par_point;      // pings par distance
extern volatile int radar;          // 1 = son Doppler en cours
extern volatile float f0;           // frequence Doppler choisie (Hz)
extern volatile float temperature;  // degres C (valeur theorique)
extern volatile float d_visee;      // distance affichee sur la regle (m)

double c_theorie(float t_celsius);  // 331,3 . racine(1 + T / 273,15)
float c_utilisee();                 // c mesuree (si droite fiable) sinon theorie
bool c_mesuree();

// ---------------------------------------------------------------- echo
extern Point pts[MAX_PTS];
extern int nb_pts;
extern Ajustement aj;
extern float conv_c[MAX_PTS];       // c obtenue avec les k premiers points
extern int nb_conv;
extern uint32_t nb_pings, nb_valides, nb_rejets_total, nb_regressions, nb_tris;

void ajouter_ping(const dsp::Echo &e);     // tache audio
int tick(uint32_t ms);                     // interface, a chaque image

void demarrer(float d);
void arreter();
bool en_collecte();
int collectes();
float collecte_d();

float dt_direct();                  // us, mediane des derniers echos (0 = rien)
float snr_direct();
bool echo_recent();
float cadence();                    // pings par seconde
const dsp::Echo &dernier_echo();    // copie de la derniere analyse (affichage)
uint32_t nb_nouveaux();             // pings arrives depuis le dernier appel

void histogramme(Histo &h, int nb_bins);
void supprimer(int i);
void effacer();
void sauver();
void charger();

// ---------------------------------------------------------------- doppler
void ajouter_trame(const dsp::Doppler &r); // tache audio

float v_direct();                   // m/s (>0 approche), 0 si rien ne bouge
bool v_detecte();
float df_direct();                  // Hz
float f0_emise();
const dsp::Doppler &derniere_trame();

extern uint8_t spectro[SPEC_COLS][dsp::NB_SPECTRE];
extern int spectro_tete;            // prochaine colonne ecrite
extern uint32_t spectro_total;
extern float hist_v[HIST_V];        // une valeur par trame (NAN = rien)
extern int hist_tete;
extern uint32_t hist_total;
extern Passage passages[MAX_PASSAGES];
extern int nb_passages;             // passages memorises (le dernier = nb_passages-1)
extern uint32_t nb_passages_total;
extern float v_max_session;
extern uint32_t nb_trames, nb_detections;
float trames_par_s();
bool en_passage();

}  // namespace son
