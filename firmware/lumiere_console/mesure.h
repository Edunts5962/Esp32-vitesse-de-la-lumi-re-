// mesure.h - coeur de la mesure : echantillons FTM, points, droite, compteurs.
//
// Chaine des donnees :
//   tache Wi-Fi (ou simulation) --ajouter()--> file d'attente protegee
//   --tick()--> temps de vol en direct + echantillons du point en cours
//   --> point (moyenne filtree) --> droite temps/distance --> c = 2 / pente
#pragma once
#include <stdint.h>

namespace mes {

const int MAX_PTS = 24;
const int MAX_ECH = 1500;
const double C_REF = 299792458.0;

struct Point {
    float d;          // distance (m)
    float rtt;        // temps aller-retour moyen apres filtrage (ns)
    float err;        // erreur type de la moyenne (ns)
    float sigma;      // ecart type des echantillons gardes (ns)
    uint16_t n;       // echantillons gardes
    uint16_t rejets;  // aberrations rejetees
};

struct Ajustement {
    bool ok;
    double a, sa;     // pente (ns/m) et son incertitude
    double b;         // decalage a l'origine (ns) : retards internes des puces
    double c, sc;     // vitesse de la lumiere (m/s) et incertitude
    double r2;        // qualite de l'alignement (1 = parfait)
};

struct Histo {
    bool ok;
    int nb;           // echantillons
    float lo, hi;     // bornes (ns)
    float med, moy, sig;
    int nb_bins;
    uint16_t bins[48];
    uint16_t nmax;
};

enum { SRC_REEL = 0, SRC_SIM = 1 };
enum { EV_RIEN = 0, EV_POINT, EV_ECHEC };

extern Point pts[MAX_PTS];
extern int nb_pts;
extern Ajustement aj;
extern float conv_c[MAX_PTS];   // c (en 10^6 m/s) avec les k premiers points
extern int nb_conv;

// compteurs (onglet Stats)
extern uint32_t nb_echant_total, nb_sessions, nb_rejets_total, nb_regressions, nb_tris;

// part des echantillons du dernier point bloques a zero par la puce (cartes trop proches)
extern float part_tronquee;

// reglages
extern int source;              // SRC_REEL / SRC_SIM
extern int par_point;           // echantillons par distance

// alimentation (appelable depuis la tache Wi-Fi)
void ajouter(uint32_t rtt_ps, int8_t rssi);
void session();

// a appeler a chaque image ; d_reglee = distance affichee par la regle
int tick(uint32_t ms, float d_reglee);

// collecte d'un point
void demarrer(float d);
void arreter();
bool en_collecte();
int collectes();
float collecte_d();

// lecture en direct
float rtt_direct();             // ns, mediane des derniers echantillons (0 = rien)
int rssi_direct();
float debit();                  // echantillons par seconde
bool signal_recent();
uint32_t nb_nouveaux();         // echantillons arrives depuis le dernier appel

void histogramme(Histo &h, int nb_bins);

void supprimer(int i);
void effacer();
void sauver();
void charger();

}  // namespace mes
