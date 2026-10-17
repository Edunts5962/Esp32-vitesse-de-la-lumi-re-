// interface.cpp - ecrans animes de la console "Vitesse du son"
//
// Deux experiences, choisies dans la barre du haut :
//   ECHO    : MESURE (ondes sonores animees, regle tactile, bouton),
//             COURBE (droite temps/distance), RESULTAT (c, jauge, convergence,
//             thermometre acoustique), STATS (echogramme, histogramme)
//   DOPPLER : MESURE (radar : ondes emises/renvoyees, personne animee,
//             grande vitesse, spectrogramme defilant), COURBE (v(t)),
//             RESULTAT (passages, echelle des allures), STATS (spectre)
//   REGLAGES : deux pages (Experience / Appareil)
#include "interface.h"
#include "gfx.h"
#include "son.h"
#include "dsp.h"
#include "plateforme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace gfx;

namespace ui {

// ============================ Palette ======================================
static const uint16_t FOND_HAUT = rgb(4, 6, 18);
static const uint16_t FOND_BAS  = rgb(16, 12, 42);
static const uint16_t PANNEAU   = rgb(16, 22, 52);
static const uint16_t PANNEAU2  = rgb(28, 36, 80);
static const uint16_t BORD      = rgb(52, 66, 130);
static const uint16_t GRILLE    = rgb(34, 44, 92);
static const uint16_t TEXTE     = rgb(236, 242, 255);
static const uint16_t TEXTE2    = rgb(156, 168, 210);
static const uint16_t TEXTE3    = rgb(96, 108, 156);
static const uint16_t CYAN      = rgb(40, 228, 255);
static const uint16_t BLEU      = rgb(70, 120, 255);
static const uint16_t VIOLET    = rgb(140, 92, 255);
static const uint16_t MAGENTA   = rgb(255, 64, 200);
static const uint16_t JAUNE     = rgb(255, 212, 64);
static const uint16_t ORANGE    = rgb(255, 140, 60);
static const uint16_t ROUGE     = rgb(255, 78, 106);
static const uint16_t VERT      = rgb(60, 255, 156);
static const uint16_t NOIR_BLEU = rgb(6, 10, 30);

static const float PI_F = 3.14159265f;

// ============================ Etat ==========================================
enum { O_MESURE, O_COURBE, O_RESULTAT, O_STATS, O_REGLAGES, NB_ONGLETS };
static const char *NOMS_ONGLETS[NB_ONGLETS] = { "Mesure", "Courbe", "Résultat", "Stats", "Réglages" };

static uint16_t *s_fond = nullptr;
static float t = 0, dt = 0.033f;
static float ips = 30;
static uint32_t ms_prec = 0;
static bool splash = true;
static float splash_t0 = 0;

static int onglet = O_MESURE;
static float onglet_aff = 0;
static float onglet_t0 = -1;
static float mode_aff = 0;            // position animee du selecteur Echo/Doppler

// Reglages memorises
struct Reglages {
    uint32_t magie;
    int32_t mode, source, plan, ppi, f0i, vol, gain, lum, page;
    float distance, temperature;
};
static Reglages R = { 0x534F5231, son::MODE_ECHO, son::SRC_REEL, 2, 1, 1, 1, 1, 80, 0, 1.0f, 20.0f };
static const int PAR_POINT[3] = { 10, 30, 60 };
static const float F0S[3] = { 5000, 10000, 17000 };

// Plans de mesure (echo)
struct Plan { const char *nom; const char *desc; int n; float d[8]; };
static const Plan PLANS[4] = {
    { "Libre", "Distances au choix (raccourcis)", 7, { 0.5f, 1, 1.5f, 2, 3, 4, 5 } },
    { "2 m", "7 distances de 0,4 à 2 m", 7, { 0.4f, 0.6f, 0.8f, 1, 1.2f, 1.5f, 2 } },
    { "4 m", "8 distances de 0,5 à 4 m", 8, { 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4 } },
    { "8 m", "8 distances de 1 à 8 m (couloir)", 8, { 1, 2, 3, 4, 5, 6, 7, 8 } },
};

// Regle des distances (pas de 5 cm)
static const float PX_M = 90.0f;
static const float D_MIN = 0.2f, D_MAX = 15.0f;
static float regle_d = 1.0f;
static float regle_cible = 1.0f;
static float regle_v = 0;

// Scene echo
static float xm_aff = 200;
struct Onde { float x, y, t0; uint16_t c; };
static Onde ondes[8];
static int nb_ondes = 0;
static float phase_prec = 0;
static float led = 0, mur_flash = 0;

// Courbe echo
static float naissance[son::MAX_PTS];
static int sel_point = -1;
static float ax_max = 2, ay_min = 0, ay_max = 12;
static float aff_a = 0, aff_b = 0;
static float ligne_t0 = -10;
static bool ax_init = false;

// Resultat echo
static float aff_c = 0;
static float aig_ang = PI_F * 1.5f, aig_v = 0;

// Stats
static son::Histo histo;
static float histo_t = -1;
static float aff_bins[48];
static float aff_cpt[4];
static float snr_hist[40];
static int snr_n = 0;
static float snr_t = 0;

// Doppler
static float xp_aff = 230, marche_ph = 0, v_aff = 0, vmax_courbe = 2;
static float vflash = 0;
static uint16_t vflash_c = CYAN;
static uint16_t lut_spectre[256];

// Reglages
static float seg_aff[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static float page_aff = 0;
static bool conf_effacer = false;
static float conf_t0 = 0;

// Toast
static char toast_txt[72] = "";
static uint16_t toast_c = CYAN;
static float toast_t0 = -10;

// Revelation (resultat echo ou passage Doppler)
struct Particule { float x, y, vx, vy, vie; uint16_t c; };
static Particule part[90];
static bool revel = false;
static float revel_t0 = 0, revel_duree = 4.5f;
static float revel_val = 0;
static int revel_dec = 1;
static char revel_titre[48], revel_unite[24], revel_ligne[96];
static uint16_t revel_c = CYAN;

// Toucher
enum { C_RIEN, C_REGLE, C_LUM, C_TEMP };
static bool tc_appui = false;
static int tc_x = 0, tc_y = 0, tc_x0 = 0, tc_y0 = 0, tc_xp = 0;
static float tc_t0 = 0;
static bool tc_bouge = false;
static int tc_cible = C_RIEN;

// Etoiles (poussieres)
struct Etoile { float x, y, v, ph; uint8_t lum; };
static Etoile etoiles[56];

// ============================ Outils ========================================
static inline float sat01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float approche(float v, float c, float k) { return v + (c - v) * (1.0f - expf(-k * dt)); }
static inline float sortie(float x) { x = sat01(x); return 1 - (1 - x) * (1 - x) * (1 - x); }
static inline float pulse(float f, float ph = 0) { return 0.5f + 0.5f * sinf(t * f * 2 * PI_F + ph); }
static inline bool echo() { return R.mode == son::MODE_ECHO; }

static uint32_t alea_e = 0x9E3779B9;
static float alea()
{
    alea_e ^= alea_e << 13; alea_e ^= alea_e >> 17; alea_e ^= alea_e << 5;
    return (alea_e >> 8) * (1.0f / 16777216.0f);
}

static void fmt(char *b, int n, double v, int dec)
{
    if (fabs(v) < 0.5 * pow(10.0, -dec)) v = 0;      // pas de "-0"
    snprintf(b, n, "%.*f", dec, v);
    for (char *p = b; *p; p++) if (*p == '.') *p = ',';
}

static void fmt_entier(char *b, int n, uint32_t v)
{
    char tmp[16];
    snprintf(tmp, sizeof tmp, "%lu", (unsigned long)v);
    int l = (int)strlen(tmp), j = 0;
    for (int i = 0; i < l && j < n - 2; i++) {
        if (i > 0 && (l - i) % 3 == 0) b[j++] = ' ';
        b[j++] = tmp[i];
    }
    b[j] = 0;
}

static void fmt_distance(char *b, int n, float d)
{
    fmt(b, n, d, 2);
    strncat(b, " m", n - strlen(b) - 1);
}

// "0,5" "1" "1,25" : court, pour les pastilles et les graduations
static void fmt_court(char *b, int n, float v)
{
    snprintf(b, n, "%g", roundf(v * 100) / 100);
    for (char *p = b; *p; p++) if (*p == '.') *p = ',';
}

static void toast(const char *s, uint16_t c)
{
    strncpy(toast_txt, s, sizeof toast_txt - 1);
    toast_txt[sizeof toast_txt - 1] = 0;
    toast_c = c;
    toast_t0 = t;
}

static void sauver_reglages()
{
    R.distance = regle_cible;
    plat_nvs_ecrire("reglages", &R, sizeof R);
}

static bool dans(int x, int y, int rx, int ry, int rl, int rh)
{
    return x >= rx && x < rx + rl && y >= ry && y < ry + rh;
}

static const Plan &plan() { return PLANS[R.plan]; }

static bool distance_faite(float d)
{
    for (int i = 0; i < son::nb_pts; i++)
        if (fabsf(son::pts[i].d - d) < 0.01f) return true;
    return false;
}

static int plan_suivant()
{
    const Plan &p = plan();
    for (int i = 0; i < p.n; i++) if (!distance_faite(p.d[i])) return i;
    return -1;
}

static void regler_distance(float d)
{
    if (d < D_MIN) d = D_MIN;
    if (d > D_MAX) d = D_MAX;
    regle_cible = roundf(d * 20) / 20.0f;
    regle_v = 0;
    son::d_visee = regle_cible;
}

static float c_th() { return (float)son::c_theorie(R.temperature); }

static const char *allure(float v, uint16_t &c)
{
    v = fabsf(v);
    if (v < 0.8f) { c = TEXTE2; return "LENT"; }
    if (v < 1.6f) { c = VERT; return "MARCHE"; }
    if (v < 2.2f) { c = CYAN; return "MARCHE RAPIDE"; }
    if (v < 3.5f) { c = JAUNE; return "JOGGING"; }
    if (v < 5.5f) { c = ORANGE; return "COURSE"; }
    c = MAGENTA;
    return "SPRINT";
}

// ============================ Fond ==========================================
static void dessiner_fond_complet()
{
    degrade_v(0, 0, L, H, FOND_HAUT, FOND_BAS);
    halo(255, 50, 150, VIOLET, 34);
    halo(40, 210, 140, BLEU, 26);
    halo(170, 250, 110, MAGENTA, 14);
}

static void init_etoiles()
{
    for (int i = 0; i < 56; i++) {
        etoiles[i].x = alea() * L;
        etoiles[i].y = alea() * H;
        etoiles[i].v = 2 + alea() * 8;
        etoiles[i].ph = alea() * 6.28f;
        etoiles[i].lum = (uint8_t)(60 + alea() * 160);
    }
}

static void fond()
{
    if (s_fond) copier(s_fond);
    else dessiner_fond_complet();
    for (int i = 0; i < 56; i++) {
        Etoile &e = etoiles[i];
        e.x -= e.v * dt;
        if (e.x < 0) { e.x += L; e.y = alea() * H; }
        float a = e.lum * (0.55f + 0.45f * sinf(t * 1.7f + e.ph));
        pixel_ajout((int)e.x, (int)e.y, TEXTE, (uint8_t)a);
        if (e.lum > 190) halo(e.x + 0.5f, e.y + 0.5f, 3, CYAN, (uint8_t)(a * 0.5f));
    }
}

// palette du spectrogramme : nuit -> violet -> magenta -> orange -> jaune -> blanc
static void init_lut()
{
    const uint8_t pts[6][3] = { { 6, 10, 30 }, { 60, 30, 140 }, { 200, 40, 180 }, { 255, 120, 60 }, { 255, 220, 70 }, { 255, 255, 240 } };
    for (int i = 0; i < 256; i++) {
        float u = i / 255.0f * 5;
        int k = (int)u;
        if (k > 4) k = 4;
        float f = u - k;
        uint8_t r = (uint8_t)(pts[k][0] + (pts[k + 1][0] - pts[k][0]) * f);
        uint8_t g = (uint8_t)(pts[k][1] + (pts[k + 1][1] - pts[k][1]) * f);
        uint8_t b = (uint8_t)(pts[k][2] + (pts[k + 1][2] - pts[k][2]) * f);
        lut_spectre[i] = rgb(r, g, b);
    }
}

// ============================ Elements communs ==============================
static void panneau(int x, int y, int l, int h, uint8_t a = 150)
{
    rect_arrondi(x, y, l, h, 12, PANNEAU, a);
    contour_arrondi(x, y, l, h, 12, 1, BORD, 110);
}

// petit haut-parleur avec ondes (logo, icone)
static void hp(float cx, float cy, float s, uint16_t c, float anim)
{
    rect((int)(cx - 4 * s), (int)(cy - 2.5f * s), (int)(3 * s) + 1, (int)(5 * s) + 1, c);
    triangle(cx - 1.5f * s, cy - 2.5f * s, cx + 2 * s, cy - 5.5f * s, cx + 2 * s, cy + 5.5f * s, c);
    triangle(cx - 1.5f * s, cy - 2.5f * s, cx + 2 * s, cy + 5.5f * s, cx - 1.5f * s, cy + 2.5f * s, c);
    for (int k = 0; k < 2; k++) {
        float r = (4.5f + 3.2f * k) * s;
        uint8_t a = (uint8_t)(255 * (k == 0 ? 0.6f + 0.4f * anim : 0.3f + 0.7f * (1 - anim)));
        arc(cx + 2 * s, cy, r, 1.3f * s, -0.8f, 0.8f, c, a);
    }
}

static void barre_haut()
{
    rect(0, 0, L, 24, PANNEAU, 190);
    degrade_h(0, 24, L, 1, JAUNE, MAGENTA, 140);
    halo(12, 12, 12, JAUNE, (uint8_t)(70 + 50 * pulse(0.9f)));
    hp(9, 12, 1.0f, rgb(255, 236, 170), pulse(1.4f));
    texte(25, 4, "SON", POLICE_13, TEXTE);

    // selecteur Echo / Doppler
    const int MX = 58, MW = 124;
    mode_aff = approche(mode_aff, (float)R.mode, 12);
    rect_arrondi(MX, 3, MW, 18, 9, PANNEAU2, 235);
    int hx = (int)(MX + 2 + mode_aff * (MW / 2));
    rect_arrondi_degrade(hx, 5, MW / 2 - 4, 14, 7, echo() ? rgb(255, 190, 60) : rgb(60, 200, 255),
                         echo() ? rgb(255, 90, 140) : rgb(130, 80, 255));
    halo(hx + MW / 4, 12, 22, echo() ? JAUNE : CYAN, 50);
    texte(MX + MW / 4, 5, "Écho", POLICE_11, echo() ? NOIR_BLEU : TEXTE2, 255, CENTRE);
    texte(MX + 3 * MW / 4, 5, "Doppler", POLICE_11, !echo() ? NOIR_BLEU : TEXTE2, 255, CENTRE);

    char buf[40];
    uint16_t c;
    bool clignote = false;
    if (son::source == son::SRC_SIM) { snprintf(buf, sizeof buf, "SIMULATION"); c = VIOLET; }
    else if (!plat_audio_ok()) { snprintf(buf, sizeof buf, "Audio absent"); c = ORANGE; clignote = true; }
    else if (echo() && son::echo_recent()) { snprintf(buf, sizeof buf, "Écho %d dB", (int)(son::snr_direct() + 0.5f)); c = VERT; }
    else if (!echo() && son::radar) { snprintf(buf, sizeof buf, "Radar actif"); c = VERT; clignote = true; }
    else { snprintf(buf, sizeof buf, "Micro prêt"); c = JAUNE; }
    int l = largeur_texte(buf, POLICE_11) + 26;
    int x = L - 6 - l;
    rect_arrondi(x, 4, l, 16, 8, PANNEAU2, 230);
    contour_arrondi(x, 4, l, 16, 8, 1, c, 120);
    float a = clignote ? 0.35f + 0.65f * pulse(1.2f) : 0.8f + 0.2f * pulse(0.8f);
    halo(x + 10, 12, 7, c, (uint8_t)(90 * a));
    disque(x + 10, 12, 3, c, (uint8_t)(255 * a));
    texte(x + 18, 6, buf, POLICE_11, TEXTE);
}

static void icone(int i, float cx, float cy, uint16_t c)
{
    switch (i) {
    case O_MESURE:
        hp(cx - 3, cy, 0.95f, c, pulse(1.2f));
        break;
    case O_COURBE:
        trait(cx - 8, cy + 5, cx - 3, cy, 1.6f, c);
        trait(cx - 3, cy, cx + 1, cy + 2, 1.6f, c);
        trait(cx + 1, cy + 2, cx + 8, cy - 5, 1.6f, c);
        disque(cx + 8, cy - 5, 2, c);
        break;
    case O_RESULTAT:
        arc(cx, cy + 4, 8, 2, PI_F, 2 * PI_F, c);
        trait(cx, cy + 4, cx + 4, cy - 2, 1.6f, c);
        disque(cx, cy + 4, 1.8f, c);
        break;
    case O_STATS:
        rect((int)cx - 8, (int)cy, 4, 6, c);
        rect((int)cx - 2, (int)cy - 6, 4, 12, c);
        rect((int)cx + 4, (int)cy - 3, 4, 9, c);
        break;
    case O_REGLAGES:
        for (int k = 0; k < 8; k++) {
            float an = k * PI_F / 4 + t * 0.3f;
            trait(cx + cosf(an) * 5, cy + sinf(an) * 5, cx + cosf(an) * 7.5f, cy + sinf(an) * 7.5f, 2.2f, c);
        }
        anneau(cx, cy, 4.5f, 2.4f, c);
        break;
    }
}

static void barre_onglets()
{
    rect(0, 204, L, 36, PANNEAU, 215);
    degrade_h(0, 204, L, 1, MAGENTA, JAUNE, 120);
    onglet_aff = approche(onglet_aff, (float)onglet, 14);
    int hx = (int)(4 + onglet_aff * 64);
    uint16_t c = echo() ? JAUNE : CYAN;
    rect_arrondi_degrade(hx, 207, 56, 31, 10, echo() ? rgb(150, 70, 90) : rgb(40, 70, 170),
                         echo() ? rgb(90, 40, 120) : rgb(70, 40, 150), 200);
    contour_arrondi(hx, 207, 56, 31, 10, 1, c, 150);
    halo(hx + 28, 239, 26, c, 70);
    for (int i = 0; i < NB_ONGLETS; i++) {
        bool actif = i == onglet;
        float cx = i * 64 + 32;
        icone(i, cx, 216, actif ? c : TEXTE3);
        texte((int)cx, 224, NOMS_ONGLETS[i], POLICE_11, actif ? TEXTE : TEXTE3, 255, CENTRE);
    }
}

static void dessiner_toast()
{
    float age = t - toast_t0;
    if (age < 0 || age > 2.6f || !toast_txt[0]) return;
    float e = age < 0.3f ? sortie(age / 0.3f) : (age > 2.2f ? 1 - (age - 2.2f) / 0.4f : 1);
    int l = largeur_texte(toast_txt, POLICE_13) + 34;
    int x = (L - l) / 2;
    int y = (int)(-20 + 50 * e);
    uint8_t a = (uint8_t)(255 * sat01(e));
    rect_arrondi(x, y, l, 24, 12, PANNEAU2, (uint8_t)(a * 0.95f));
    contour_arrondi(x, y, l, 24, 12, 1.2f, toast_c, a);
    halo(x + 14, y + 12, 10, toast_c, (uint8_t)(a * 0.5f));
    disque(x + 14, y + 12, 4, toast_c, a);
    texte(x + 24, y + 5, toast_txt, POLICE_13, TEXTE, a);
}

static void segments(int idx, int x, int y, int l, int h, const char *const *lab, int n, int sel)
{
    if (seg_aff[idx] < 0) seg_aff[idx] = (float)sel;
    seg_aff[idx] = approche(seg_aff[idx], (float)sel, 14);
    rect_arrondi(x, y, l, h, h / 2, PANNEAU2, 230);
    float w = (float)l / n;
    int sx = (int)(x + seg_aff[idx] * w);
    rect_arrondi_degrade(sx + 2, y + 2, (int)w - 4, h - 4, (h - 4) / 2, rgb(60, 150, 255), rgb(120, 70, 240));
    halo(sx + w / 2, (float)y + h, w * 0.5f, CYAN, 60);
    for (int i = 0; i < n; i++)
        texte((int)(x + w * i + w / 2), y + (h - 13) / 2, lab[i], POLICE_11,
              i == sel ? TEXTE : TEXTE2, 255, CENTRE);
}

static void curseur(int x0, int x1, int y, float f, bool actif, uint16_t c0, uint16_t c1)
{
    float kx = x0 + (x1 - x0) * sat01(f);
    rect_arrondi(x0, y - 3, x1 - x0, 6, 3, PANNEAU2);
    rect_arrondi_degrade(x0, y - 3, (int)(kx - x0) + 3, 6, 3, c0, c1);
    halo(kx, (float)y, 14, c1, actif ? 160 : 80);
    disque(kx, (float)y, 7, TEXTE);
    disque(kx, (float)y, 3, c1);
}

// bouton principal (bas gauche) : 0 pret, 1 en cours (p = progression), 2 attente, 3 complet
static void bouton(int etat, float p, const char *titre, const char *sous, uint16_t ca, uint16_t cb)
{
    const int BX = 6, BY = 160, BL = 140, BH = 42;
    if (etat == 1) {
        rect_arrondi(BX, BY, BL, BH, 12, PANNEAU2, 230);
        decoupe(BX, BY, BX + (int)(BL * sat01(p)), BY + BH);
        rect_arrondi_degrade(BX, BY, BL, BH, 12, ca, cb);
        float sx = BX + fmodf(t * 160, BL + 60) - 30;
        trait(sx, BY, sx - 18, BY + BH, 12, TEXTE, 50);
        sans_decoupe();
        contour_arrondi(BX, BY, BL, BH, 12, 1.2f, ca, 200);
        arc(BX + 20, BY + 21, 8, 2.5f, t * 6, t * 6 + 4.2f, JAUNE);
        texte(BX + 34, BY + 5, titre, POLICE_16, TEXTE);
        texte(BX + 34, BY + 24, sous, POLICE_11, TEXTE2);
    } else if (etat == 2 || etat == 3) {
        rect_arrondi(BX, BY, BL, BH, 12, PANNEAU2, 230);
        if (etat == 2) contour_arrondi(BX, BY, BL, BH, 12, 1, ORANGE, (uint8_t)(80 + 80 * pulse(1)));
        texte(BX + BL / 2, BY + 6, titre, POLICE_16, TEXTE2, 255, CENTRE);
        texte(BX + BL / 2, BY + 25, sous, POLICE_11, TEXTE3, 255, CENTRE);
    } else {
        float g = pulse(0.8f);
        contour_arrondi(BX - 3, BY - 3, BL + 6, BH + 6, 15, 2, ca, (uint8_t)(50 + 90 * g));
        halo(BX + BL / 2, BY + BH / 2, 80, ca, (uint8_t)(25 + 25 * g));
        rect_arrondi_degrade(BX, BY, BL, BH, 12, ca, cb);
        decoupe(BX, BY, BX + BL, BY + BH);
        float sx = BX - 40 + fmodf(t * 110, 360);
        trait(sx, BY - 2, sx - 22, BY + BH + 2, 14, TEXTE, 70);
        sans_decoupe();
        texte(BX + BL / 2, BY + 5, titre, POLICE_16, NOIR_BLEU, 255, CENTRE);
        texte(BX + BL / 2, BY + 25, sous, POLICE_11, NOIR_BLEU, 210, CENTRE);
    }
}

static float pas_joli(float etendue, int n)
{
    float brut = etendue / n;
    if (brut <= 0) return 1;
    float p = powf(10, floorf(log10f(brut)));
    float f = brut / p;
    return (f < 1.5f ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * p;
}

static void onde(float x, float y, uint16_t c)
{
    if (nb_ondes < 8) ondes[nb_ondes++] = { x, y, t, c };
}

static void dessiner_ondes_choc()
{
    for (int i = 0; i < nb_ondes;) {
        float age = t - ondes[i].t0;
        if (age > 0.7f) { ondes[i] = ondes[--nb_ondes]; continue; }
        anneau(ondes[i].x, ondes[i].y, 6 + 28 * sortie(age / 0.7f), 1.5f, ondes[i].c,
               (uint8_t)(200 * (1 - age / 0.7f)));
        i++;
    }
}

// la carte (haut-parleur + micro), vue de profil
static void carte_son(float x, float y, float actif)
{
    halo(x, y, 26, JAUNE, 26);
    rect_arrondi_degrade((int)x - 12, (int)y - 18, 22, 36, 5, rgb(36, 48, 104), rgb(14, 20, 48));
    contour_arrondi((int)x - 12, (int)y - 18, 22, 36, 5, 1.2f, JAUNE, 190);
    rect((int)x - 8, (int)y - 13, 13, 10, rgb(30, 60, 110));       // ecran
    degrade_h((int)x - 8, (int)y - 13, 13, 2, CYAN, VIOLET, 140);
    disque(x + 6, y + 6, 4.2f, rgb(20, 24, 50));                   // haut-parleur
    anneau(x + 6, y + 6, 4.2f, 1, TEXTE3);
    disque(x + 6, y + 6, 1.6f, TEXTE3);
    disque(x + 7, y - 6, 1.3f, VERT);                              // micro
    if (actif > 0.02f) halo(x + 7, y - 6, 6, VERT, (uint8_t)(170 * actif));
}

// ============================ ECHO : MESURE =================================
static float x_mur(float d)
{
    float f = (logf(d < D_MIN ? D_MIN : d) - logf(D_MIN)) / (logf(D_MAX) - logf(D_MIN));
    return 100 + sat01(f) * (292 - 100);
}

static void regle(int y0)
{
    bool verrou = son::en_collecte();
    char buf[24];
    panneau(6, y0, 308, 38, 150);
    if (tc_cible != C_REGLE) {
        if (fabsf(regle_v) > 0.02f) {
            regle_d += regle_v * dt;
            regle_v *= expf(-3.5f * dt);
            if (regle_d < D_MIN) { regle_d = D_MIN; regle_v = 0; }
            if (regle_d > D_MAX) { regle_d = D_MAX; regle_v = 0; }
            regle_cible = roundf(regle_d * 20) / 20.0f;
            son::d_visee = regle_cible;
        } else {
            regle_v = 0;
            regle_d = approche(regle_d, regle_cible, 12);
        }
    }
    decoupe(8, y0 + 1, 312, y0 + 37);
    float m0 = regle_d - 160 / PX_M, m1 = regle_d + 160 / PX_M;
    int yb = y0 + 36;
    for (int k = (int)floorf(m0 * 20); k <= (int)ceilf(m1 * 20); k++) {
        if (k < 4) continue;
        float m = k / 20.0f;
        float x = 160 + (m - regle_d) * PX_M;
        int h = (k % 10 == 0) ? 14 : (k % 2 == 0 ? 8 : 4);
        uint16_t c = (k % 10 == 0) ? TEXTE2 : TEXTE3;
        trait(x, (float)(yb - h), x, (float)yb, 1, c, verrou ? 90 : 220);
        if (k % 10 == 0 && fabsf(x - 160) > 10) {
            fmt_court(buf, sizeof buf, m);
            texte((int)x, y0 + 4, buf, POLICE_11, TEXTE2, verrou ? 90 : 230, CENTRE);
        }
    }
    for (int i = 0; i < 40; i++) {
        uint8_t a = (uint8_t)(230 * (1 - i / 40.0f));
        rect(8 + i, y0 + 1, 1, 36, PANNEAU, a);
        rect(311 - i, y0 + 1, 1, 36, PANNEAU, a);
    }
    sans_decoupe();
    halo(160, (float)(y0 + 22), 16, JAUNE, verrou ? 40 : 90);
    trait(160, (float)(y0 + 8), 160, (float)yb, 2, JAUNE, verrou ? 120 : 255);
    triangle(154, (float)(y0 + 2), 166, (float)(y0 + 2), 160, (float)(y0 + 9), JAUNE, verrou ? 120 : 255);
    texte(18, y0 + 9, "‹", POLICE_22, TEXTE2, verrou ? 60 : 200, CENTRE);
    texte(302, y0 + 9, "›", POLICE_22, TEXTE2, verrou ? 60 : 200, CENTRE);
}

static void ecran_mesure_echo()
{
    char buf[96];
    panneau(6, 28, 308, 86, 140);
    float d_aff = son::en_collecte() ? son::collecte_d() : regle_cible;
    xm_aff = approche(xm_aff, x_mur(d_aff), 6);
    const float xa = 30, yc = 58;
    const float sx = xa + 10, sy = yc + 6;              // haut-parleur
    float xw = xm_aff;
    float dist_px = xw - sx;

    // impulsion animee : aller (jaune) puis retour (cyan), a vitesse constante
    float periode = son::en_collecte() ? 1.1f : 1.8f;
    float u = fmodf(t / periode, 1.0f);
    bool aller = u < 0.5f;
    float v = aller ? u * 2 : (u - 0.5f) * 2;
    float r_go = aller ? dist_px * v : -1000;
    float xm_src = 2 * xw - sx;                          // source image (miroir)
    float r_ret = !aller ? (xm_src - xw) + dist_px * v : -1000;
    if (phase_prec < 0.5f && u >= 0.5f) { onde(xw, sy, JAUNE); mur_flash = 1; }
    if (phase_prec > u) { onde(sx, sy, CYAN); led = 1; }
    phase_prec = u;
    mur_flash = approche(mur_flash, 0, 5);
    led = approche(led, 0, 6);

    decoupe(7, 29, (int)xw, 113);
    // molecules d'air : elles oscillent sur place quand l'onde passe (onde longitudinale)
    const float rangs[5] = { 36, 46, 56, 70, 80 };
    for (int ry = 0; ry < 5; ry++) {
        float y = rangs[ry];
        for (float x = 52; x < xw - 5; x += 6) {
            float rg = sqrtf((x - sx) * (x - sx) + (y - sy) * (y - sy));
            float rr = sqrtf((x - xm_src) * (x - xm_src) + (y - sy) * (y - sy));
            float ug = (rg - r_go) / 9, ur = (rr - r_ret) / 9;
            float eg = expf(-ug * ug), er = expf(-ur * ur);
            float dx = 3.2f * eg * sinf(ug * 2.2f) - 3.2f * er * sinf(ur * 2.2f);
            float a = 70 + 150 * fmaxf(eg, er);
            uint16_t c = eg > er ? melange(TEXTE3, JAUNE, (uint8_t)(255 * eg)) : melange(TEXTE3, CYAN, (uint8_t)(255 * er));
            disque(x + dx, y, 1.1f + 0.5f * fmaxf(eg, er), c, (uint8_t)a);
        }
    }
    // fronts d'onde
    for (int q = 0; q < 4; q++) {
        float rq = r_go - q * 5;
        if (rq > 6) {
            float span = fminf(1.05f, asinf(fminf(1.0f, 34.0f / rq)));
            uint8_t a = (uint8_t)((240 - q * 55) * (1 - 0.4f * rq / (dist_px + 1)));
            if (q == 0) arc(sx, sy, rq, 6, -span, span, JAUNE, 40);
            arc(sx, sy, rq, 2.0f, -span, span, JAUNE, a);
        }
        float rr = r_ret - q * 5;
        if (rr > xm_src - xw + 1) {
            float span = fminf(1.05f, asinf(fminf(1.0f, 34.0f / rr)));
            uint8_t a = (uint8_t)((220 - q * 50) * (1 - 0.5f * (rr - (xm_src - xw)) / (dist_px + 1)));
            if (q == 0) arc(xm_src, sy, rr, 6, PI_F - span, PI_F + span, CYAN, 40);
            arc(xm_src, sy, rr, 2.0f, PI_F - span, PI_F + span, CYAN, a);
        }
    }
    sans_decoupe();
    decoupe(7, 29, 313, 113);
    dessiner_ondes_choc();
    // le mur
    halo(xw + 4, sy, 30, CYAN, (uint8_t)(30 + 120 * mur_flash));
    degrade_v((int)xw, 32, 8, 62, rgb(120, 130, 170), rgb(60, 66, 110));
    rect((int)xw, 32, 2, 62, melange(TEXTE2, JAUNE, (uint8_t)(255 * mur_flash)));
    for (int y = 34; y < 92; y += 7) trait(xw + 8, (float)y + 6, xw + 13, (float)y, 1.2f, TEXTE3, 200);
    if (xw < 280) texte((int)xw + 16, 56, "mur", POLICE_11, TEXTE2, 220);
    else texte((int)xw - 3, 96, "mur", POLICE_11, TEXTE2, 220, DROITE);
    carte_son(xa, yc, led);

    // cote
    float yd = 88;
    trait(sx, yd, xw, yd, 1, TEXTE3, 200);
    triangle(sx, yd, sx + 6, yd - 3, sx + 6, yd + 3, TEXTE3);
    triangle(xw, yd, xw - 6, yd - 3, xw - 6, yd + 3, TEXTE3);
    fmt_distance(buf, sizeof buf, d_aff);
    int lb = largeur_texte(buf, POLICE_13) + 14;
    int bx = (int)((sx + xw) / 2 - lb / 2.0f);
    if (bx < 48) bx = 48;
    rect_arrondi(bx, (int)yd - 9, lb, 18, 9, NOIR_BLEU, 240);
    contour_arrondi(bx, (int)yd - 9, lb, 18, 9, 1, JAUNE, 180);
    texte(bx + lb / 2, (int)yd - 7, buf, POLICE_13, JAUNE, 255, CENTRE);

    // lectures en direct
    float dtu = son::dt_direct();
    int lx = texte(14, 99, "Écho Δt ", POLICE_11, TEXTE3);
    if (dtu > 0) {
        fmt(buf, sizeof buf, dtu / 1000.0, 2);
        strcat(buf, " ms");
        texte(14 + lx, 97, buf, POLICE_13, CYAN);
        // si la droite existe, on retire son decalage (ecart haut-parleur/micro)
        float dte = son::aj.ok ? dtu - (float)son::aj.b : dtu;
        float ci = dte > 0 ? 2 * d_aff / (dte * 1e-6f) : 0;
        fmt(buf, sizeof buf, ci, 0);
        strcat(buf, " m/s");
        int lc = texte(306, 97, buf, POLICE_13, VERT, 255, DROITE);
        texte(306 - lc - 4, 99, son::aj.ok ? "c ≈" : "c brut ≈", POLICE_11, TEXTE3, 255, DROITE);
    } else {
        texte(14 + lx, 97, "—", POLICE_13, TEXTE3);
        texte(306, 99, son::source == son::SRC_REEL && !plat_audio_ok() ? "micro indisponible" : "vise un mur bien plat",
              POLICE_11, TEXTE3, 255, DROITE);
    }
    double vrai = 2.0 * d_aff / c_th();
    double ralenti = periode / vrai;
    fmt(buf, sizeof buf, ralenti, 0);
    char r2[40];
    snprintf(r2, sizeof r2, "ralenti %s ×", buf);
    texte(306, 31, r2, POLICE_11, TEXTE3, 200, DROITE);
    sans_decoupe();

    regle(118);

    // bouton
    char sous[64], d[16];
    bool pret = son::source == son::SRC_SIM || plat_audio_ok();
    if (son::en_collecte()) {
        snprintf(sous, sizeof sous, "%d / %d pings", son::collectes(), (int)son::par_point);
        bouton(1, son::collectes() / (float)son::par_point, "ARRÊTER", sous, rgb(255, 190, 60), rgb(255, 80, 150));
    } else if (son::nb_pts >= son::MAX_PTS) {
        bouton(3, 0, "COMPLET", "24 points maximum", 0, 0);
    } else if (!pret) {
        bouton(2, 0, "EN ATTENTE", "Audio indisponible", 0, 0);
    } else {
        fmt_distance(d, sizeof d, regle_cible);
        snprintf(sous, sizeof sous, "%d pings · %s", (int)son::par_point, d);
        bouton(0, 0, "MESURER", sous, rgb(255, 214, 80), rgb(255, 110, 150));
    }

    // plan + mini echogramme
    panneau(150, 160, 164, 42, 140);
    const Plan &p = plan();
    int suiv = plan_suivant();
    int cg = 1;
    float cw = (156.0f - (p.n - 1) * cg) / p.n;
    for (int i = 0; i < p.n; i++) {
        int x = (int)(154 + i * (cw + cg));
        bool fait = distance_faite(p.d[i]);
        bool choisi = fabsf(regle_cible - p.d[i]) < 0.01f;
        fmt_court(buf, sizeof buf, p.d[i]);
        if (fait) {
            rect_arrondi(x, 164, (int)cw, 15, 7, VERT, 200);
            texte(x + (int)cw / 2, 165, buf, POLICE_11, NOIR_BLEU, 255, CENTRE);
        } else {
            rect_arrondi(x, 164, (int)cw, 15, 7, PANNEAU2, 230);
            texte(x + (int)cw / 2, 165, buf, POLICE_11, choisi ? TEXTE : TEXTE2, 255, CENTRE);
        }
        if (i == suiv && R.plan != 0)
            contour_arrondi(x, 164, (int)cw, 15, 7, 1.2f, JAUNE, (uint8_t)(110 + 140 * pulse(1.2f)));
        if (choisi) contour_arrondi(x - 1, 163, (int)cw + 2, 17, 8, 1.2f, CYAN, 230);
    }
    const dsp::Echo &e = son::dernier_echo();
    const int hx = 158, hy = 183, hw = 104, hh = 16;
    if (son::nb_pings > 0) {
        float x0 = hx + hw * e.f_fen0, x1 = hx + hw * e.f_fen1;
        if (x1 > x0) rect((int)x0, hy, (int)(x1 - x0) + 1, hh, CYAN, 30);
        float px = hx, py = hy + hh;
        for (int x = 0; x < hw; x++) {
            int k = x * dsp::ENV_AFF / hw;
            float v = e.env[k] / 255.0f;
            float y = hy + hh - v * hh;
            if (x > 0) trait(px, py, (float)(hx + x), y, 1.1f, melange(VIOLET, JAUNE, (uint8_t)(255 * v)), 230);
            px = (float)(hx + x); py = y;
        }
        if (e.ok) {
            float xe = hx + hw * e.f_echo;
            halo(xe, (float)hy + 4, 7, VERT, 140);
            trait(xe, (float)hy, xe, (float)(hy + hh), 1, VERT, 220);
        }
        snprintf(buf, sizeof buf, "%d dB", (int)(e.snr_db + 0.5f));
        texte(310, 179, "SNR", POLICE_11, TEXTE3, 255, DROITE);
        texte(310, 190, buf, POLICE_11, e.ok ? VERT : ORANGE, 255, DROITE);
    } else {
        texte(232, 186, "Échogramme en direct", POLICE_11, TEXTE3, 255, CENTRE);
    }
}

static void tap_mesure_echo(int x, int y)
{
    if (dans(x, y, 6, 160, 140, 42)) {
        if (son::en_collecte()) {
            son::arreter();
            toast("Mesure arrêtée", ORANGE);
        } else if (son::nb_pts >= son::MAX_PTS) {
            toast("24 points maximum", ORANGE);
        } else if (son::source == son::SRC_SIM || plat_audio_ok()) {
            son::demarrer(regle_cible);
            sauver_reglages();
        } else {
            toast("Audio indisponible", ORANGE);
        }
        return;
    }
    if (son::en_collecte()) {
        if (y > 116) toast("Mesure en cours…", JAUNE);
        return;
    }
    const Plan &p = plan();
    float cw = (156.0f - (p.n - 1)) / p.n;
    if (dans(x, y, 150, 160, 164, 24)) {
        int i = (int)((x - 154) / (cw + 1));
        if (i >= 0 && i < p.n) regler_distance(p.d[i]);
        return;
    }
    if (dans(x, y, 6, 118, 40, 38)) { regler_distance(regle_cible - 0.05f); return; }
    if (dans(x, y, 274, 118, 40, 38)) { regler_distance(regle_cible + 0.05f); return; }
}

// ============================ ECHO : COURBE =================================
static const int GX0 = 46, GX1 = 304, GY0 = 36, GY1 = 176;
static float gx(float d) { return GX0 + (GX1 - GX0) * d / ax_max; }
static float gy(float v) { return GY1 - (GY1 - GY0) * (v - ay_min) / (ay_max - ay_min); }

static void cibles_axes(float &xm, float &ymin, float &ymax)
{
    xm = 1;
    ymin = 0;
    ymax = 0;
    for (int i = 0; i < son::nb_pts; i++) {
        if (son::pts[i].d > xm) xm = son::pts[i].d;
        ymax = fmaxf(ymax, son::pts[i].dt / 1000.0f);
    }
    xm *= 1.12f;
    if (son::aj.ok) ymax = fmaxf(ymax, (float)((son::aj.b + son::aj.a * xm) / 1000.0));
    if (ymax < 1) ymax = 1;
    ymax *= 1.08f;
}

static void ecran_courbe_echo()
{
    panneau(6, 28, 308, 174, 150);
    char buf[96], b2[48];
    if (son::nb_pts == 0) {
        float a = 0.6f + 0.4f * pulse(0.5f);
        halo(160, 92, 40, JAUNE, (uint8_t)(60 * a));
        trait(140, 104, 152, 92, 2, JAUNE, (uint8_t)(200 * a));
        trait(152, 92, 160, 98, 2, JAUNE, (uint8_t)(200 * a));
        trait(160, 98, 180, 78, 2, JAUNE, (uint8_t)(200 * a));
        texte(160, 122, "Aucun point pour l'instant", POLICE_16, TEXTE, 255, CENTRE);
        texte(160, 144, "Onglet Mesure : place la carte face au mur,", POLICE_11, TEXTE2, 255, CENTRE);
        texte(160, 158, "règle la distance, puis MESURER.", POLICE_11, TEXTE2, 255, CENTRE);
        return;
    }
    float xm, ymin, ymax;
    cibles_axes(xm, ymin, ymax);
    if (!ax_init) { ax_max = xm; ay_min = ymin; ay_max = ymax; ax_init = true; }
    ax_max = approche(ax_max, xm, 5);
    ay_min = approche(ay_min, ymin, 5);
    ay_max = approche(ay_max, ymax, 5);

    float sxp = pas_joli(ax_max, 5), syp = pas_joli(ay_max - ay_min, 4);
    for (float v = 0; v <= ax_max + 1e-4f; v += sxp) {
        int x = (int)gx(v);
        rect(x, GY0, 1, GY1 - GY0, GRILLE, 160);
        fmt(buf, sizeof buf, v, sxp < 1 ? 1 : 0);
        texte(x, GY1 + 3, buf, POLICE_11, TEXTE3, 255, CENTRE);
    }
    for (float v = ceilf(ay_min / syp) * syp; v <= ay_max; v += syp) {
        int y = (int)gy(v);
        rect(GX0, y, GX1 - GX0, 1, GRILLE, 160);
        fmt(buf, sizeof buf, v, syp < 1 ? 1 : 0);
        texte(GX0 - 4, y - 7, buf, POLICE_11, TEXTE3, 255, DROITE);
    }
    rect(GX0, GY0, 1, GY1 - GY0, TEXTE3);
    rect(GX0, GY1, GX1 - GX0, 1, TEXTE3);
    texte(12, 30, "Δt (ms)", POLICE_11, TEXTE3);
    texte(GX1, GY1 + 14, "distance (m)", POLICE_11, TEXTE3, 255, DROITE);

    decoupe(GX0 + 1, GY0, GX1 + 1, GY1);
    if (son::aj.ok) {
        float a = (float)(son::aj.a / 1000.0), b = (float)(son::aj.b / 1000.0);   // ms/m, ms
        if (aff_a == 0) { aff_a = a; aff_b = b; }
        aff_a = approche(aff_a, a, 6);
        aff_b = approche(aff_b, b, 6);
        float mx = 0;
        for (int i = 0; i < son::nb_pts; i++) mx += son::pts[i].d;
        mx /= son::nb_pts;
        for (int x = GX0 + 1; x < GX1; x++) {
            float d = (x - GX0) * ax_max / (GX1 - GX0);
            float yc = aff_b + aff_a * d;
            float e = 2.0f * (float)(son::aj.sa / 1000.0) * fabsf(d - mx) + 0.01f;
            int y0 = (int)gy(yc + e), y1 = (int)gy(yc - e);
            if (y1 - y0 >= 1) rect(x, y0, 1, y1 - y0, JAUNE, 34);
        }
        float ar = 2000.0f / c_th();                  // theorie (ms/m)
        trait_pointille(gx(0), gy(aff_b), gx(ax_max), gy(aff_b + ar * ax_max), 1, 5, 4, TEXTE2, 140);
        float prog = sortie((t - ligne_t0) / 0.9f);
        float xe = ax_max * prog;
        trait(gx(0), gy(aff_b), gx(xe), gy(aff_b + aff_a * xe), 6, JAUNE, 45);
        trait(gx(0), gy(aff_b), gx(xe), gy(aff_b + aff_a * xe), 2, JAUNE);
        if (prog < 1) halo(gx(xe), gy(aff_b + aff_a * xe), 14, JAUNE, 200);
    }
    for (int i = 0; i < son::nb_pts; i++) {
        const son::Point &p = son::pts[i];
        float x = gx(p.d), y = gy(p.dt / 1000.0f);
        float age = t - naissance[i];
        float s = age < 0.5f ? 1 + 0.6f * sinf(sat01(age / 0.5f) * PI_F) : 1;
        if (age < 0.8f) anneau(x, y, 4 + 22 * sortie(age / 0.8f), 1.5f, CYAN, (uint8_t)(220 * (1 - age / 0.8f)));
        float e = (gy((p.dt - 2 * p.err) / 1000.0f) - gy((p.dt + 2 * p.err) / 1000.0f)) / 2;
        if (e > 1.5f) {
            trait(x, y - e, x, y + e, 1, CYAN, 200);
            trait(x - 3, y - e, x + 3, y - e, 1, CYAN, 200);
            trait(x - 3, y + e, x + 3, y + e, 1, CYAN, 200);
        }
        halo(x, y, 10, CYAN, 80);
        disque(x, y, 3.6f * s, CYAN);
        disque(x, y, 1.5f * s, TEXTE);
        if (i == sel_point) anneau(x, y, 7 + pulse(1.5f), 1.5f, JAUNE);
    }
    sans_decoupe();

    if (son::aj.ok) {
        rect_arrondi(GX0 + 6, GY0 + 4, 158, 44, 8, PANNEAU2, 215);
        contour_arrondi(GX0 + 6, GY0 + 4, 158, 44, 8, 1, BORD, 160);
        fmt(buf, sizeof buf, aff_a, 4);
        fmt(b2, sizeof b2, son::aj.sa / 1000.0, 4);
        char l1[96];
        snprintf(l1, sizeof l1, "%s ± %s ms/m", buf, b2);
        texte(GX0 + 12, GY0 + 7, "pente", POLICE_11, TEXTE3);
        texte(GX0 + 56, GY0 + 7, l1, POLICE_11, JAUNE);
        fmt(buf, sizeof buf, 2000.0 / c_th(), 4);
        snprintf(l1, sizeof l1, "%s ms/m (%d °C)", buf, (int)lroundf(R.temperature));
        texte(GX0 + 12, GY0 + 20, "théorie", POLICE_11, TEXTE3);
        texte(GX0 + 56, GY0 + 20, l1, POLICE_11, TEXTE2);
        fmt(buf, sizeof buf, son::aj.r2, 6);
        texte(GX0 + 12, GY0 + 33, "R²", POLICE_11, TEXTE3);
        texte(GX0 + 56, GY0 + 33, buf, POLICE_11, TEXTE2);
    } else {
        rect_arrondi(GX0 + 6, GY0 + 4, 196, 20, 8, PANNEAU2, 215);
        texte(GX0 + 14, GY0 + 7, "Encore une distance pour la droite", POLICE_11, TEXTE2);
    }

    if (sel_point >= 0 && sel_point < son::nb_pts) {
        const son::Point &p = son::pts[sel_point];
        float x = gx(p.d), y = gy(p.dt / 1000.0f);
        int bw = 156, bh = 66;
        int bx = x > 170 ? (int)x - bw - 10 : (int)x + 10;
        int by = (int)y - bh / 2;
        if (by < 32) by = 32;
        if (by + bh > 198) by = 198 - bh;
        rect_arrondi(bx, by, bw, bh, 10, NOIR_BLEU, 240);
        contour_arrondi(bx, by, bw, bh, 10, 1.2f, JAUNE, 220);
        fmt_distance(buf, sizeof buf, p.d);
        texte(bx + 8, by + 4, buf, POLICE_13, JAUNE);
        fmt(buf, sizeof buf, p.dt / 1000.0, 3);
        fmt(b2, sizeof b2, p.err, 1);
        char l[96];
        snprintf(l, sizeof l, "Δt %s ms ± %s µs", buf, b2);
        texte(bx + 8, by + 21, l, POLICE_11, TEXTE);
        fmt(buf, sizeof buf, p.sigma, 1);
        snprintf(l, sizeof l, "σ %s µs · n %u · rejets %u", buf, p.n, p.rejets);
        texte(bx + 8, by + 34, l, POLICE_11, TEXTE2);
        rect_arrondi(bx + 8, by + 48, 70, 14, 7, ROUGE, 220);
        texte(bx + 43, by + 48, "Supprimer", POLICE_11, TEXTE, 255, CENTRE);
        snprintf(l, sizeof l, "%d dB", (int)lroundf(p.snr));
        texte(bx + bw - 8, by + 49, l, POLICE_11, TEXTE3, 255, DROITE);
    }
}

static void tap_courbe_echo(int x, int y)
{
    if (sel_point >= 0 && sel_point < son::nb_pts) {
        const son::Point &p = son::pts[sel_point];
        float px = gx(p.d), py = gy(p.dt / 1000.0f);
        int bw = 156, bh = 66;
        int bx = px > 170 ? (int)px - bw - 10 : (int)px + 10;
        int by = (int)py - bh / 2;
        if (by < 32) by = 32;
        if (by + bh > 198) by = 198 - bh;
        if (dans(x, y, bx + 4, by + 44, 80, 22)) {
            char buf[96], d[16];
            fmt_distance(d, sizeof d, p.d);
            snprintf(buf, sizeof buf, "Point %s supprimé", d);
            for (int k = sel_point; k < son::nb_pts - 1; k++) naissance[k] = naissance[k + 1];
            son::supprimer(sel_point);
            sel_point = -1;
            ligne_t0 = t;
            toast(buf, ORANGE);
            return;
        }
        if (dans(x, y, bx, by, bw, bh)) return;
    }
    int best = -1;
    float bd = 16 * 16;
    for (int i = 0; i < son::nb_pts; i++) {
        float dx = gx(son::pts[i].d) - x, dy = gy(son::pts[i].dt / 1000.0f) - y;
        if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; }
    }
    sel_point = best;
}

// ============================ ECHO : RESULTAT ===============================
static void jauge(float JX, float JY, float JR, float ecart, bool ok)
{
    struct Seg { float a, b; uint16_t c; };
    const Seg segs[5] = { { -10, -5, ROUGE }, { -5, -1, ORANGE }, { -1, 1, VERT }, { 1, 5, ORANGE }, { 5, 10, ROUGE } };
    arc(JX, JY, JR, 12, PI_F, 2 * PI_F, PANNEAU2);
    for (int k = 0; k < 5; k++) {
        float a0 = PI_F + (segs[k].a + 10) / 20 * PI_F + 0.02f;
        float a1 = PI_F + (segs[k].b + 10) / 20 * PI_F - 0.02f;
        arc(JX, JY, JR, 9, a0, a1, segs[k].c, 210);
    }
    float e = ok ? ecart : 0;
    if (e < -10.5f) e = -10.5f;
    if (e > 10.5f) e = 10.5f;
    float cible = PI_F + (e + 10) / 20 * PI_F;
    float acc = 60 * (cible - aig_ang) - 7 * aig_v;
    aig_v += acc * dt;
    aig_ang += aig_v * dt;
    float tx = JX + cosf(aig_ang) * (JR - 12), ty = JY + sinf(aig_ang) * (JR - 12);
    halo(tx, ty, 12, ok ? JAUNE : TEXTE3, 120);
    trait(JX, JY, tx, ty, 3, ok ? TEXTE : TEXTE3);
    disque(JX, JY, 5, ok ? JAUNE : TEXTE3);
}

static void ecran_resultat_echo()
{
    char buf[96], b2[48];
    bool ok = son::aj.ok;
    float ref = c_th();
    double ecart = ok ? 100.0 * (son::aj.c - ref) / ref : 0;

    panneau(6, 28, 308, 66, 160);
    texte(16, 32, "VITESSE DU SON MESURÉE", POLICE_11, TEXTE3);
    if (ok) {
        aff_c = approche(aff_c, (float)son::aj.c, 3.5f);
        fmt(buf, sizeof buf, aff_c, 1);
        int l = largeur_texte(buf, POLICE_40);
        halo(16 + l / 2, 66, 60, JAUNE, (uint8_t)(45 + 25 * pulse(0.5f)));
        texte(14, 44, buf, POLICE_40, TEXTE);
        texte(22 + l, 50, "m/s", POLICE_13, TEXTE2);
        if (son::nb_pts >= 3) {
            fmt(b2, sizeof b2, son::aj.sc, 1);
            snprintf(buf, sizeof buf, "± %s", b2);
        } else {
            snprintf(buf, sizeof buf, "± ?");
        }
        texte(22 + l, 68, buf, POLICE_16, JAUNE);
        const char *q;
        uint16_t qc;
        int etoiles;
        double ae = fabs(ecart);
        if (ae < 1)       { q = "EXCELLENT"; qc = VERT; etoiles = 3; }
        else if (ae < 3)  { q = "BON"; qc = CYAN; etoiles = 2; }
        else if (ae < 8)  { q = "MOYEN"; qc = ORANGE; etoiles = 1; }
        else              { q = "À AMÉLIORER"; qc = ROUGE; etoiles = 0; }
        int ql = largeur_texte(q, POLICE_11) + 16;
        rect_arrondi(306 - ql, 32, ql, 16, 8, qc, 210);
        texte(306 - ql / 2, 33, q, POLICE_11, NOIR_BLEU, 255, CENTRE);
        for (int k = 0; k < 3; k++) {
            float x = 306 - 8 - k * 13, y = 60;
            if (k < etoiles) { halo(x, y, 8, qc, 90); disque(x, y, 4, qc); }
            else anneau(x, y, 3.5f, 1.2f, TEXTE3);
        }
        snprintf(buf, sizeof buf, "%d points", son::nb_pts);
        texte(306, 72, buf, POLICE_11, TEXTE3, 255, DROITE);
    } else {
        texte(14, 44, "—", POLICE_40, TEXTE3);
        texte(70, 54, "Mesure au moins 2 distances", POLICE_13, TEXTE2);
        texte(70, 72, "(3 ou plus pour l'incertitude)", POLICE_11, TEXTE3);
    }

    // jauge d'ecart a la theorie
    panneau(6, 98, 150, 104, 150);
    texte(14, 101, "Écart à la théorie", POLICE_11, TEXTE3);
    jauge(81, 166, 46, (float)ecart, ok);
    texte(14, 170, "-10 %", POLICE_11, TEXTE3);
    texte(148, 170, "+10 %", POLICE_11, TEXTE3, 255, DROITE);
    if (ok) {
        fmt(b2, sizeof b2, ecart, 2);
        snprintf(buf, sizeof buf, "%s%s %%", ecart >= 0 ? "+" : "", b2);
        texte(81, 174, buf, POLICE_13, fabs(ecart) < 1 ? VERT : (fabs(ecart) < 3 ? CYAN : ORANGE), 255, CENTRE);
    }
    fmt(b2, sizeof b2, ref, 1);
    snprintf(buf, sizeof buf, "théorie %s m/s à %d °C", b2, (int)lroundf(R.temperature));
    texte(81, 189, buf, POLICE_11, TEXTE3, 255, CENTRE);

    // convergence + thermometre acoustique
    panneau(160, 98, 154, 104, 150);
    texte(168, 101, "Convergence de c", POLICE_11, TEXTE3);
    const int CX0 = 194, CX1 = 306, CY0 = 120, CY1 = 168;
    float amp = 5;
    for (int i = 0; i < son::nb_conv; i++)
        if (son::conv_c[i] > 0) amp = fmaxf(amp, fabsf(son::conv_c[i] - ref) * 1.25f);
    if (amp > 60) amp = 60;
    auto cy = [&](float v) { return CY1 - (CY1 - CY0) * (v - (ref - amp)) / (2 * amp); };
    float b1 = cy(ref * 1.01f), b0 = cy(ref * 0.99f);
    rect(CX0, (int)b1, CX1 - CX0, (int)(b0 - b1) + 1, VERT, 30);
    trait_pointille(CX0, cy(ref), CX1, cy(ref), 1, 4, 3, VERT, 170);
    fmt(buf, sizeof buf, ref, 0);
    texte(CX0 - 3, (int)cy(ref) - 7, buf, POLICE_11, VERT, 220, DROITE);
    fmt(buf, sizeof buf, ref + amp, 0);
    texte(CX0 - 3, CY0 - 6, buf, POLICE_11, TEXTE3, 255, DROITE);
    fmt(buf, sizeof buf, ref - amp, 0);
    texte(CX0 - 3, CY1 - 8, buf, POLICE_11, TEXTE3, 255, DROITE);
    rect(CX0, CY1, CX1 - CX0, 1, TEXTE3);
    if (son::nb_conv >= 1) {
        int n = son::nb_conv;
        float pxp = 0, pyp = 0;
        decoupe(CX0, CY0 - 4, CX1 + 4, CY1 + 1);
        for (int i = 0; i < n; i++) {
            float x = n == 1 ? (CX0 + CX1) / 2.0f : CX0 + 4 + (CX1 - CX0 - 8) * i / (float)(n - 1);
            float y = cy(son::conv_c[i]);
            if (i > 0) trait(pxp, pyp, x, y, 1.8f, JAUNE);
            pxp = x; pyp = y;
        }
        for (int i = 0; i < n; i++) {
            float x = n == 1 ? (CX0 + CX1) / 2.0f : CX0 + 4 + (CX1 - CX0 - 8) * i / (float)(n - 1);
            float y = cy(son::conv_c[i]);
            bool dernier = i == n - 1;
            if (dernier) halo(x, y, 10 + 3 * pulse(1), JAUNE, 150);
            disque(x, y, dernier ? 3.5f : 2.2f, dernier ? JAUNE : CYAN);
        }
        sans_decoupe();
    } else {
        texte((CX0 + CX1) / 2, 140, "en attente", POLICE_11, TEXTE3, 255, CENTRE);
    }
    // thermometre : la temperature de l'air que donne la vitesse mesuree
    rect(168, 174, 138, 1, GRILLE, 160);
    if (ok && son::nb_pts >= 2) {
        double r = son::aj.c / 331.3;
        double tc = 273.15 * (r * r - 1);
        float th = (float)sat01((float)(tc + 10) / 50.0f);
        rect_arrondi(170, 180, 6, 16, 3, PANNEAU2);
        rect_arrondi(171, (int)(195 - 14 * th), 4, (int)(14 * th) + 1, 2, ORANGE);
        disque(173, 195, 4, ORANGE);
        fmt(b2, sizeof b2, tc, 1);
        snprintf(buf, sizeof buf, "air à %s °C", b2);
        texte(182, 178, buf, POLICE_13, ORANGE);
        texte(182, 191, "d'après c mesurée", POLICE_11, TEXTE3);
    } else {
        texte(237, 184, "thermomètre acoustique", POLICE_11, TEXTE3, 255, CENTRE);
    }
}

// ============================ STATS (commun) ================================
static const int CARTE_X[4] = { 6, 84, 162, 240 };
static const int CARTE_L[4] = { 74, 74, 74, 74 };

static void carte_stat(int i, const char *titre, float val, int dec, const char *sous, uint16_t c)
{
    int x = CARTE_X[i], y = 28, l = CARTE_L[i], h = 50;
    panneau(x, y, l, h, 150);
    texte(x + 7, y + 4, titre, POLICE_11, TEXTE3);
    aff_cpt[i] = approche(aff_cpt[i], val, 6);
    if (fabsf(aff_cpt[i] - val) < (dec ? 0.05f : 1)) aff_cpt[i] = val;
    char buf[24];
    if (dec == 0) fmt_entier(buf, sizeof buf, (uint32_t)(aff_cpt[i] + 0.5f));
    else fmt(buf, sizeof buf, aff_cpt[i], dec);
    halo(x + 20, y + 28, 22, c, 40);
    texte(x + 7, y + 17, buf, POLICE_16, c);
    if (sous) texte(x + 7, y + 35, sous, POLICE_11, TEXTE3);
}

static void ecran_stats_echo()
{
    char s1[32], s3[32], buf[96], b2[32], b3[32];
    float pv = son::nb_pings ? 100.0f * son::nb_valides / son::nb_pings : 0;
    snprintf(s1, sizeof s1, "%d %% ok", (int)(pv + 0.5f));
    float pr = son::nb_valides ? 100.0f * son::nb_rejets_total / son::nb_valides : 0;
    fmt(s3, sizeof s3, pr, 1);
    strcat(s3, " %");
    carte_stat(0, "Pings", (float)son::nb_pings, 0, s1, JAUNE);
    carte_stat(1, "FFT", (float)dsp::nb_fft(), 0, "8 192 pts", VIOLET);
    carte_stat(2, "Rejetés", (float)son::nb_rejets_total, 0, s3, ORANGE);
    carte_stat(3, "SNR (dB)", son::snr_direct(), 0, nullptr, VERT);
    float smax = 40;
    for (int i = 0; i < snr_n; i++) smax = fmaxf(smax, snr_hist[i]);
    for (int i = 1; i < snr_n; i++) {
        float x0 = CARTE_X[3] + 7 + (i - 1) * 1.5f, x1 = x0 + 1.5f;
        trait(x0, 74 - 10 * snr_hist[i - 1] / smax, x1, 74 - 10 * snr_hist[i] / smax, 1.2f, VERT, 200);
    }

    // echogramme du dernier ping
    panneau(6, 82, 308, 120, 150);
    const dsp::Echo &e = son::dernier_echo();
    texte(14, 86, "Échogramme du dernier ping", POLICE_11, TEXTE2);
    texte(306, 86, "t (ms) après le direct", POLICE_11, TEXTE3, 255, DROITE);
    const int HX0 = 16, HX1 = 304, HY0 = 102, HY1 = 148;
    if (son::nb_pings == 0) {
        texte(160, 124, "En attente du premier ping…", POLICE_11, TEXTE3, 255, CENTRE);
        return;
    }
    float w = (float)(HX1 - HX0);
    float x0 = HX0 + w * e.f_fen0, x1 = HX0 + w * e.f_fen1;
    if (x1 > x0) {
        rect((int)x0, HY0, (int)(x1 - x0) + 1, HY1 - HY0, CYAN, 26);
        texte((int)((x0 + x1) / 2), HY1 - 13, "fenêtre", POLICE_11, CYAN, 150, CENTRE);
    }
    float xd = HX0 + w * (0.10f / 1.10f);                  // le direct (t = 0)
    float pas = pas_joli(e.aff_ms, 5);
    for (float tm = 0; tm <= e.aff_ms + 1e-4f; tm += pas) {
        float x = xd + (w - (xd - HX0)) * tm / e.aff_ms;
        rect((int)x, HY1, 1, 3, TEXTE3);
        fmt(buf, sizeof buf, tm, pas < 1 ? 1 : 0);
        texte((int)x, HY1 + 3, buf, POLICE_11, TEXTE3, 255, CENTRE);
    }
    float px = HX0, py = HY1;
    for (int x = HX0; x <= HX1; x++) {
        float f = (x - HX0) / w * (dsp::ENV_AFF - 1);
        int k = (int)f;
        float fr = f - k;
        float v = (e.env[k] * (1 - fr) + e.env[k + 1 < dsp::ENV_AFF ? k + 1 : k] * fr) / 255.0f;
        float y = HY1 - v * (HY1 - HY0);
        uint16_t c = melange(VIOLET, JAUNE, (uint8_t)(255 * v));
        if (HY1 - y >= 1) rect(x, (int)y, 1, HY1 - (int)y, c, 70);
        if (x > HX0) trait(px, py, (float)x, y, 1.3f, c);
        px = (float)x; py = y;
    }
    rect(HX0, HY1, HX1 - HX0, 1, TEXTE3);
    halo(xd, HY0 + 2, 10, JAUNE, 120);
    texte((int)xd + 5, HY0 + 1, "direct", POLICE_11, JAUNE);
    if (e.ok) {
        float xe = HX0 + w * e.f_echo;
        halo(xe, (float)HY0 + 14, 12, VERT, 150 + (uint8_t)(60 * pulse(1.2f)));
        trait(xe, (float)HY0 + 8, xe, (float)HY1, 1, VERT, 230);
        fmt(b2, sizeof b2, e.dt_us / 1000.0f, 3);
        snprintf(buf, sizeof buf, "écho %s ms", b2);
        int lw = largeur_texte(buf, POLICE_11);
        int tx = (int)xe + 5;
        if (tx + lw > HX1) tx = (int)xe - 5 - lw;
        texte(tx, HY0 + 10, buf, POLICE_11, VERT);
    } else {
        texte(HX1, HY0 + 10, "pas d'écho net", POLICE_11, ORANGE, 255, DROITE);
    }

    // histogramme des pings du point
    if (t - histo_t > 0.25f) { son::histogramme(histo, 40); histo_t = t; }
    const int GH0 = 168, GH1 = 184, GHX0 = 16, GHX1 = 116;
    if (histo.ok) {
        for (int b = 0; b < histo.nb_bins; b++) {
            aff_bins[b] = approche(aff_bins[b], histo.bins[b] / (float)histo.nmax, 8);
            int h = (int)((GH1 - GH0) * aff_bins[b] + 0.5f);
            if (h < 1) continue;
            int x = GHX0 + b * (GHX1 - GHX0) / histo.nb_bins;
            rect(x, GH1 - h, 2, h, melange(VIOLET, JAUNE, (uint8_t)(255 * aff_bins[b])));
        }
        rect(GHX0, GH1, GHX1 - GHX0, 1, TEXTE3);
        fmt(b2, sizeof b2, histo.moy / 1000.0f, 4);
        fmt(b3, sizeof b3, histo.sig, 1);
        snprintf(buf, sizeof buf, "μ %s ms   σ %s µs   n %d", b2, b3, histo.nb);
        texte(124, 168, buf, POLICE_11, TEXTE);
        char d[16];
        fmt_distance(d, sizeof d, son::collecte_d());
        snprintf(buf, sizeof buf, "pings du point %s%s", d, son::en_collecte() ? " (en cours)" : "");
        texte(124, 181, buf, POLICE_11, TEXTE3);
    } else {
        texte(160, 176, "Lance une mesure pour voir la distribution des Δt", POLICE_11, TEXTE3, 255, CENTRE);
    }
}

// ============================ DOPPLER : MESURE ==============================
// personnage qui marche (bonhomme lumineux)
static void personnage(float x, float yb, float ph, float v, bool actif)
{
    uint16_t c = !actif ? TEXTE2 : (v > 0 ? CYAN : MAGENTA);
    float sens = v > 0.05f ? -1 : (v < -0.05f ? 1 : -1);      // regarde vers la carte quand il approche
    float amp = fminf(0.75f, 0.2f + 0.22f * fabsf(v));
    float sw = actif ? sinf(ph) * amp : 0.3f;
    float course = fabsf(v) > 2.5f ? 1 : 0;
    if (actif) halo(x, yb - 22, 30, c, 60);
    float hy = yb - 16, sy = yb - 32;
    // jambes
    trait(x, hy, x + 9 * sinf(sw), hy + 16 * cosf(sw) * (1 - 0.15f * course), 2.6f, c);
    trait(x, hy, x - 9 * sinf(sw), hy + 16 * cosf(sw) * (1 - 0.15f * course), 2.6f, c, 170);
    // corps (penche en courant)
    float px = x + sens * 4 * course;
    trait(x, hy, px, sy, 3, c);
    // bras
    trait(px, sy + 2, px - 8 * sinf(sw), sy + 14 - 2 * course, 2.2f, c);
    trait(px, sy + 2, px + 8 * sinf(sw), sy + 14 - 2 * course, 2.2f, c, 170);
    disque(px + sens * 0.5f, sy - 7, 5, c);
    disque(px + sens * 3, sy - 8, 1.1f, NOIR_BLEU);              // l'oeil donne le sens
}

static void ecran_mesure_dop()
{
    char buf[96], b2[48];
    bool on = son::radar;
    bool det = on && son::v_detecte();
    float v = son::v_direct();
    v_aff = approche(v_aff, det ? v : 0, 7);

    // ---------- scene ----------
    panneau(6, 28, 308, 74, 140);
    const float xa = 30, yc = 62;
    const float sx = xa + 10, sy = yc + 6;
    // la personne avance quand elle s'approche, recule quand elle s'eloigne
    if (det) xp_aff -= v * 24 * dt;
    else xp_aff = approche(xp_aff, 236, 0.5f);
    if (xp_aff < 130) xp_aff = 130;
    if (xp_aff > 292) xp_aff = 292;
    marche_ph += (0.6f + fabsf(v_aff) * 3.2f) * dt * (det ? 1 : 0);
    float xp = xp_aff;
    decoupe(7, 29, 313, 101);
    if (on) {
        // ondes emises (f0) : fronts reguliers
        const float LAM = 13, VIT = 70;
        float rmax = xp - 8 - sx;
        float off = fmodf(t * VIT, LAM);
        for (float r = off; r < rmax; r += LAM) {
            float span = fminf(0.95f, asinf(fminf(1.0f, 30.0f / (r + 1))));
            uint8_t a = (uint8_t)(200 * (1 - 0.6f * r / rmax));
            arc(sx, sy, r, 1.6f, -span, span, JAUNE, a);
        }
        // ondes renvoyees : comprimees (plus aigues) si approche, etirees (plus graves) si eloignement
        float k = det ? v_aff : 0;
        float lam2 = LAM * (1 - 0.16f * k);
        if (lam2 < 6) lam2 = 6;
        if (lam2 > 22) lam2 = 22;
        uint16_t cr = !det ? TEXTE3 : (k > 0 ? melange(CYAN, BLEU, (uint8_t)fminf(255, 70 * k)) : melange(ORANGE, ROUGE, (uint8_t)fminf(255, -70 * k)));
        float cxr = xp - 8;
        float off2 = fmodf(t * VIT, lam2);
        for (float r = off2; r < cxr - sx; r += lam2) {
            float span = fminf(0.8f, asinf(fminf(1.0f, 26.0f / (r + 1))));
            uint8_t a = (uint8_t)((det ? 210 : 70) * (1 - 0.7f * r / (cxr - sx)));
            arc(cxr, sy - 6, r, 1.6f, PI_F - span, PI_F + span, cr, a);
        }
        fmt(b2, sizeof b2, son::f0 / 1000.0f, 0);
        snprintf(buf, sizeof buf, "f0 = %s kHz", b2);
        texte(56, 31, buf, POLICE_11, JAUNE, 220);
        if (det) texte((int)cxr - 30, 31, k > 0 ? "f0 + Δf" : "f0 − Δf", POLICE_11, cr, 230, DROITE);
    }
    personnage(xp, 98, marche_ph * 2 * PI_F, v_aff, det);
    carte_son(xa, yc, on ? 0.5f + 0.5f * pulse(2) : 0);
    if (!on) {
        float a = 0.5f + 0.5f * pulse(0.6f);
        texte(132, 50, "Lance le radar, puis marche", POLICE_13, TEXTE2, (uint8_t)(255 * a), CENTRE);
        texte(132, 66, "vers la carte ou en t'éloignant", POLICE_11, TEXTE3, 255, CENTRE);
    }
    sans_decoupe();

    // ---------- vitesse ----------
    panneau(6, 106, 308, 50, 150);
    vflash = approche(vflash, 0, 3);
    if (vflash > 0.02f) contour_arrondi(6, 106, 308, 50, 12, 2, vflash_c, (uint8_t)(255 * vflash));
    if (det) {
        fmt(buf, sizeof buf, fabsf(v_aff), 2);
        uint16_t cv = v > 0 ? CYAN : MAGENTA;
        int l = largeur_texte(buf, POLICE_40);
        halo(14 + l / 2, 132, 50, cv, (uint8_t)(50 + 30 * pulse(1)));
        texte(12, 111, buf, POLICE_40, TEXTE);
        texte(18 + l, 118, "m/s", POLICE_13, TEXTE2);
    } else {
        int l = largeur_texte("0,00", POLICE_40);
        if (on) texte(12, 111, "0,00", POLICE_40, TEXTE3);
        else rect_arrondi(16, 128, 40, 6, 3, TEXTE3);
        texte(18 + l, 118, "m/s", POLICE_13, TEXTE3);
    }
    const int CX = 166;
    if (det) {
        bool app = v > 0;
        const char *s = app ? "▲ S'APPROCHE" : "▼ S'ÉLOIGNE";
        int sl = largeur_texte(s, POLICE_11) + 14;
        rect_arrondi(CX, 111, sl, 16, 8, app ? CYAN : MAGENTA, 220);
        texte(CX + sl / 2, 112, s, POLICE_11, NOIR_BLEU, 255, CENTRE);
        fmt(b2, sizeof b2, son::df_direct(), 1);
        snprintf(buf, sizeof buf, "Δf %s%s Hz", son::df_direct() >= 0 ? "+" : "", b2);
        texte(306, 145, buf, POLICE_11, app ? CYAN : MAGENTA, 255, DROITE);
        fmt(b2, sizeof b2, fabsf(v_aff) * 3.6f, 1);
        snprintf(buf, sizeof buf, "%s km/h", b2);
        texte(CX, 128, buf, POLICE_16, TEXTE);
        uint16_t ca;
        const char *al = allure(v_aff, ca);
        texte(CX, 145, al, POLICE_11, ca);
    } else {
        rect_arrondi(CX, 111, 96, 16, 8, PANNEAU2, 230);
        texte(CX + 48, 112, on ? "IMMOBILE" : "RADAR ARRÊTÉ", POLICE_11, TEXTE2, 255, CENTRE);
        texte(CX, 128, "— km/h", POLICE_16, TEXTE3);
    }
    if (!det) {
        fmt_entier(b2, sizeof b2, (uint32_t)lroundf(son::f0_emise()));
        snprintf(buf, sizeof buf, "f0 %s Hz", b2);
        texte(306, 145, buf, POLICE_11, TEXTE3, 255, DROITE);
    }

    // ---------- bouton ----------
    char sous[64];
    bool pret = son::source == son::SRC_SIM || plat_audio_ok();
    if (on) {
        snprintf(sous, sizeof sous, "%d analyses/s", (int)lroundf(son::trames_par_s()));
        bouton(1, 1, "ARRÊTER", sous, rgb(60, 200, 255), rgb(130, 80, 255));
    } else if (!pret) {
        bouton(2, 0, "EN ATTENTE", "Audio indisponible", 0, 0);
    } else {
        fmt(b2, sizeof b2, son::f0 / 1000.0f, 0);
        snprintf(sous, sizeof sous, "son continu %s kHz", b2);
        bouton(0, 0, "RADAR", sous, rgb(70, 225, 255), rgb(120, 90, 255));
    }

    // ---------- spectrogramme defilant ----------
    panneau(150, 160, 164, 42, 140);
    const int SX0 = 155, SW = 154, SY0 = 163, SH = 36;
    rect(SX0, SY0, SW, SH, NOIR_BLEU, 230);
    uint32_t tot = son::spectro_total;
    for (int c = 0; c < SW; c++) {
        int age = SW - 1 - c;                       // 0 = colonne la plus recente (a droite)
        if ((uint32_t)age >= tot || age >= son::SPEC_COLS) continue;
        int idx = (son::spectro_tete - 1 - age + son::SPEC_COLS * 4) % son::SPEC_COLS;
        const uint8_t *col = son::spectro[idx];
        for (int r = 0; r < SH; r++) {
            int b = dsp::NB_SPECTRE - 1 - r * (dsp::NB_SPECTRE - 1) / (SH - 1);
            uint8_t val = col[b];
            if (val < 8) continue;
            fb[(SY0 + r) * L + SX0 + c] = lut_spectre[val];
        }
    }
    trait_pointille(SX0, SY0 + SH / 2.0f, SX0 + SW, SY0 + SH / 2.0f, 1, 3, 3, JAUNE, 90);
    texte(SX0 + 3, SY0 - 1, "aigu", POLICE_11, CYAN, 150);
    texte(SX0 + 3, SY0 + SH - 12, "grave", POLICE_11, MAGENTA, 150);
    if (!on && tot == 0) texte(SX0 + SW / 2, SY0 + 12, "Spectrogramme", POLICE_11, TEXTE3, 255, CENTRE);
}

static void tap_mesure_dop(int x, int y)
{
    if (dans(x, y, 6, 160, 140, 42)) {
        if (son::radar) {
            son::radar = 0;
            toast("Radar arrêté", ORANGE);
        } else if (son::source == son::SRC_SIM || plat_audio_ok()) {
            son::radar = 1;
            toast(son::source == son::SRC_SIM ? "Radar simulé : quelqu'un va et vient" : "Radar lancé : marche devant la carte", CYAN);
        } else {
            toast("Audio indisponible", ORANGE);
        }
    }
}

// ============================ DOPPLER : COURBE ==============================
static void ecran_courbe_dop()
{
    panneau(6, 28, 308, 174, 150);
    char buf[64], b2[32];
    const int X0 = 40, X1 = 304, Y0 = 52, Y1 = 178;
    const float duree = son::HIST_V * son::SAUT_S;
    float vm = 2;
    int n = (int)(son::hist_total < (uint32_t)son::HIST_V ? son::hist_total : son::HIST_V);
    for (int k = 0; k < n; k++) {
        float v = son::hist_v[(son::hist_tete - 1 - k + son::HIST_V) % son::HIST_V];
        if (v == v) vm = fmaxf(vm, fabsf(v) * 1.15f);
    }
    vmax_courbe = approche(vmax_courbe, vm, 3);
    float ym = (Y0 + Y1) / 2.0f;
    auto yv = [&](float v) { return ym - (Y1 - Y0) / 2.0f * v / vmax_courbe; };
    rect(X0, Y0, X1 - X0, (int)(ym - Y0), CYAN, 10);
    rect(X0, (int)ym, X1 - X0, (int)(Y1 - ym), MAGENTA, 10);
    float pas = pas_joli(vmax_courbe, 2);
    for (float v = pas; v <= vmax_courbe; v += pas) {
        for (int s = -1; s <= 1; s += 2) {
            int y = (int)yv(s * v);
            rect(X0, y, X1 - X0, 1, GRILLE, 150);
            fmt(buf, sizeof buf, s * v, pas < 1 ? 1 : 0);
            if (s > 0) { memmove(buf + 1, buf, strlen(buf) + 1); buf[0] = '+'; }
            texte(X0 - 4, y - 7, buf, POLICE_11, TEXTE3, 255, DROITE);
        }
    }
    for (int s = 0; s <= 20; s += 5) {
        float x = X1 - (X1 - X0) * s / duree;
        if (x < X0) continue;
        rect((int)x, Y0, 1, Y1 - Y0, GRILLE, 110);
        snprintf(buf, sizeof buf, s ? "−%d s" : "maint.", s);
        texte((int)x, Y1 + 2, buf, POLICE_11, TEXTE3, 255, s ? CENTRE : DROITE);
    }
    trait(X0, ym, X1, ym, 1.2f, TEXTE2, 200);
    texte(X0 + 4, Y0 + 2, "▲ s'approche", POLICE_11, CYAN, 200);
    texte(X0 + 4, Y1 - 13, "▼ s'éloigne", POLICE_11, MAGENTA, 200);
    texte(12, 32, "VITESSE (m/s) · 20 dernières secondes", POLICE_11, TEXTE3);

    decoupe(X0, Y0, X1 + 1, Y1 + 1);
    float px = 0, py = 0;
    bool prec = false;
    for (int k = n - 1; k >= 0; k--) {
        float v = son::hist_v[(son::hist_tete - 1 - k + son::HIST_V) % son::HIST_V];
        float x = X1 - (X1 - X0) * (k * son::SAUT_S) / duree;
        if (v != v) { prec = false; continue; }
        float y = yv(v);
        uint16_t c = v > 0 ? CYAN : MAGENTA;
        int y0 = (int)fminf(y, ym), y1 = (int)fmaxf(y, ym);
        rect((int)x, y0, 2, y1 - y0 + 1, c, 60);
        if (prec) trait(px, py, x, y, 2, c);
        px = x; py = y;
        prec = true;
    }
    if (prec && son::v_detecte()) {
        halo(px, py, 12 + 3 * pulse(1.5f), son::v_direct() > 0 ? CYAN : MAGENTA, 170);
        disque(px, py, 3.5f, TEXTE);
    }
    sans_decoupe();
    if (n == 0) {
        texte(170, 104, "Lance le radar (onglet Mesure)", POLICE_13, TEXTE2, 255, CENTRE);
    }
    fmt(b2, sizeof b2, son::v_max_session, 2);
    snprintf(buf, sizeof buf, "max %s m/s", b2);
    texte(306, 32, buf, POLICE_11, JAUNE, 255, DROITE);
}

// ============================ DOPPLER : RESULTAT ============================
static void ecran_resultat_dop()
{
    char buf[96], b2[48];
    int n = son::nb_passages;
    panneau(6, 28, 308, 66, 160);
    texte(16, 32, "DERNIER PASSAGE · VITESSE MAX", POLICE_11, TEXTE3);
    if (n > 0) {
        const son::Passage &p = son::passages[n - 1];
        aff_c = approche(aff_c, p.vmax, 4);
        fmt(buf, sizeof buf, aff_c, 2);
        int l = largeur_texte(buf, POLICE_40);
        uint16_t cs = p.sens > 0 ? CYAN : MAGENTA;
        halo(16 + l / 2, 66, 60, cs, (uint8_t)(45 + 25 * pulse(0.5f)));
        texte(14, 44, buf, POLICE_40, TEXTE);
        texte(22 + l, 50, "m/s", POLICE_13, TEXTE2);
        fmt(b2, sizeof b2, aff_c * 3.6f, 1);
        snprintf(buf, sizeof buf, "%s km/h", b2);
        texte(22 + l, 68, buf, POLICE_16, cs);
        uint16_t ca;
        const char *al = allure(p.vmax, ca);
        int ql = largeur_texte(al, POLICE_11) + 16;
        rect_arrondi(306 - ql, 32, ql, 16, 8, ca, 210);
        texte(306 - ql / 2, 33, al, POLICE_11, NOIR_BLEU, 255, CENTRE);
        fmt(b2, sizeof b2, p.vmoy, 2);
        snprintf(buf, sizeof buf, "moyenne %s m/s", b2);
        texte(306, 54, buf, POLICE_11, TEXTE2, 255, DROITE);
        fmt(b2, sizeof b2, p.duree, 1);
        snprintf(buf, sizeof buf, "%s · %s s", p.sens > 0 ? "▲ approche" : "▼ éloignement", b2);
        texte(306, 68, buf, POLICE_11, cs, 255, DROITE);
        float res = son::c_utilisee() * dsp::FS / (float)dsp::N_DOP / (2 * son::f0_emise());
        fmt(b2, sizeof b2, res, 2);
        snprintf(buf, sizeof buf, "résolution %s m/s", b2);
        texte(306, 80, buf, POLICE_11, TEXTE3, 255, DROITE);
    } else {
        texte(14, 44, "—", POLICE_40, TEXTE3);
        texte(70, 54, "Aucun passage pour l'instant", POLICE_13, TEXTE2);
        texte(70, 72, "Marche devant la carte, radar lancé", POLICE_11, TEXTE3);
    }

    // echelle des allures
    panneau(6, 98, 150, 104, 150);
    texte(14, 101, "Échelle des allures (m/s)", POLICE_11, TEXTE3);
    struct Al { const char *nom; const char *plage; float lo, hi; uint16_t c; };
    const Al als[5] = {
        { "Course", "> 3,5", 3.5f, 99, ORANGE },
        { "Jogging", "2,2–3,5", 2.2f, 3.5f, JAUNE },
        { "Marche rapide", "1,6–2,2", 1.6f, 2.2f, CYAN },
        { "Marche", "0,8–1,6", 0.8f, 1.6f, VERT },
        { "Lent", "< 0,8", 0, 0.8f, TEXTE2 },
    };
    float vref = n > 0 ? son::passages[n - 1].vmax : -1;
    for (int i = 0; i < 5; i++) {
        int y = 118 + i * 16;
        bool ici = vref >= als[i].lo && vref < als[i].hi;
        if (ici) {
            rect_arrondi(10, y - 2, 142, 16, 8, als[i].c, (uint8_t)(60 + 40 * pulse(1)));
            halo(16, y + 6, 10, als[i].c, 120);
            triangle(12, (float)y + 1, 12, (float)y + 11, 18, (float)y + 6, als[i].c);
        }
        rect_arrondi(22, y + 2, 4, 9, 2, als[i].c);
        texte(30, y, als[i].nom, POLICE_11, ici ? TEXTE : TEXTE2);
        texte(148, y, als[i].plage, POLICE_11, ici ? TEXTE : TEXTE3, 255, DROITE);
    }

    // derniers passages
    panneau(160, 98, 154, 104, 150);
    texte(168, 101, "Derniers passages", POLICE_11, TEXTE3);
    const int BX0 = 170, BX1 = 306, BY0 = 120, BY1 = 182;
    rect(BX0, BY1, BX1 - BX0, 1, TEXTE3);
    if (n == 0) {
        texte((BX0 + BX1) / 2, 146, "en attente", POLICE_11, TEXTE3, 255, CENTRE);
        return;
    }
    int k0 = n > 6 ? n - 6 : 0;
    float vmx = 3;
    for (int i = k0; i < n; i++) vmx = fmaxf(vmx, son::passages[i].vmax * 1.1f);
    float bw = (BX1 - BX0) / 6.0f;
    for (int i = k0; i < n; i++) {
        const son::Passage &p = son::passages[i];
        int j = i - k0;
        float h = (BY1 - BY0) * p.vmax / vmx;
        float x = BX0 + j * bw + 2;
        uint16_t c = p.sens > 0 ? CYAN : MAGENTA;
        bool dernier = i == n - 1;
        if (dernier) halo(x + bw / 2 - 2, BY1 - h, 14, c, 110);
        rect_arrondi_degrade((int)x, (int)(BY1 - h), (int)bw - 4, (int)h + 1, 3, c, melange(c, PANNEAU, 160));
        fmt(b2, sizeof b2, p.vmax, 1);
        texte((int)(x + bw / 2 - 2), (int)(BY1 - h) - 12, b2, POLICE_11, dernier ? TEXTE : TEXTE2, 255, CENTRE);
        texte((int)(x + bw / 2 - 2), BY1 + 2, p.sens > 0 ? "▲" : "▼", POLICE_11, c, 255, CENTRE);
    }
}

// ============================ DOPPLER : STATS ===============================
static void ecran_stats_dop()
{
    char s1[32], s2[32], s3[32], buf[96], b2[32];
    float pd = son::nb_trames ? 100.0f * son::nb_detections / son::nb_trames : 0;
    snprintf(s1, sizeof s1, "%d /s", (int)lroundf(son::trames_par_s()));
    snprintf(s2, sizeof s2, "%d %%", (int)lroundf(pd));
    fmt(b2, sizeof b2, son::v_max_session, 1);
    snprintf(s3, sizeof s3, "max %s m/s", b2);
    carte_stat(0, "Analyses", (float)son::nb_trames, 0, s1, CYAN);
    carte_stat(1, "Détections", (float)son::nb_detections, 0, s2, VIOLET);
    carte_stat(2, "Passages", (float)son::nb_passages_total, 0, s3, JAUNE);
    carte_stat(3, "c (m/s)", son::c_utilisee(), 0, son::c_mesuree() ? "mesurée" : "théorie", VERT);

    panneau(6, 82, 308, 120, 150);
    const dsp::Doppler &d = son::derniere_trame();
    texte(14, 86, "Spectre autour de f0 (FFT 8 192 points)", POLICE_11, TEXTE2);
    const int X0 = 18, X1 = 302, Y0 = 102, Y1 = 160;
    if (son::nb_trames == 0) {
        texte(160, 130, "Lance le radar pour voir le spectre", POLICE_11, TEXTE3, 255, CENTRE);
        return;
    }
    float lo = d.plancher_db - 6, hi = d.porteuse_db + 3;
    if (hi < lo + 30) hi = lo + 30;
    auto yd = [&](float db) { float u = (db - lo) / (hi - lo); return Y1 - (Y1 - Y0) * (u < 0 ? 0 : (u > 1 ? 1 : u)); };
    const int B = dsp::BANDE_DOP;
    float bw = (X1 - X0) / (float)(2 * B + 1);
    float seuil = d.plancher_db + 10;
    for (int j = -B; j <= B; j++) {
        float db = d.spectre_db[j + B];
        float x = X0 + (j + B) * bw;
        float y = yd(db);
        uint16_t c;
        if (abs(j) <= dsp::EXCLU_DOP) c = JAUNE;
        else if (db > seuil) c = j > 0 ? CYAN : MAGENTA;
        else c = melange(VIOLET, TEXTE3, 120);
        rect((int)x, (int)y, (int)fmaxf(1, bw - 0.4f), Y1 - (int)y + 1, c, abs(j) <= dsp::EXCLU_DOP ? 200 : 230);
    }
    trait_pointille(X0, yd(seuil), X1, yd(seuil), 1, 4, 3, TEXTE, 150);
    texte(X1, (int)yd(seuil) - 12, "seuil", POLICE_11, TEXTE2, 200, DROITE);
    float xc = X0 + B * bw + bw / 2;
    halo(xc, Y0 + 4, 12, JAUNE, 120);
    texte((int)xc + 6, Y0 - 2, "porteuse (son direct)", POLICE_11, JAUNE, 220);
    if (d.detect) {
        float xd = xc + d.df_hz / (dsp::FS / (float)dsp::N_DOP) * bw;
        uint16_t c = d.df_hz > 0 ? CYAN : MAGENTA;
        trait(xd, Y0 + 12, xd, Y1, 1.5f, c);
        halo(xd, Y0 + 14, 10, c, 160);
        fmt(b2, sizeof b2, d.df_hz, 1);
        snprintf(buf, sizeof buf, "Δf %s%s Hz", d.df_hz > 0 ? "+" : "", b2);
        int lw = largeur_texte(buf, POLICE_11);
        int tx = d.df_hz > 0 ? (int)xd + 4 : (int)xd - 4 - lw;
        texte(tx, Y0 + 14, buf, POLICE_11, c);
    }
    rect(X0, Y1 + 1, X1 - X0, 1, TEXTE3);
    float span = B * dsp::FS / (float)dsp::N_DOP;
    snprintf(buf, sizeof buf, "−%d Hz", (int)lroundf(span));
    texte(X0, Y1 + 3, buf, POLICE_11, TEXTE3);
    texte((int)xc, Y1 + 3, "f0", POLICE_11, JAUNE, 255, CENTRE);
    snprintf(buf, sizeof buf, "+%d Hz", (int)lroundf(span));
    texte(X1, Y1 + 3, buf, POLICE_11, TEXTE3, 255, DROITE);
    // la formule, avec les vraies valeurs
    rect(14, 177, 292, 1, GRILLE, 160);
    if (d.detect) {
        float fe = son::f0_emise(), c = son::c_utilisee();
        float v = c * d.df_hz / (2 * fe + d.df_hz);
        char sv[16], sd[16];
        fmt(sv, sizeof sv, v, 2);
        fmt(sd, sizeof sd, d.df_hz, 1);
        snprintf(buf, sizeof buf, "v = c·Δf / (2 f0 + Δf) = %d × %s / %d = %s m/s", (int)lroundf(c), sd,
                 (int)lroundf(2 * fe + d.df_hz), sv);
        texte(160, 183, buf, POLICE_11, TEXTE, 255, CENTRE);
    } else {
        fmt(b2, sizeof b2, d.snr_db, 0);
        snprintf(buf, sizeof buf, "v = c·Δf / (2 f0 + Δf)   ·   rien ne bouge (pic %s dB)", b2);
        texte(160, 183, buf, POLICE_11, TEXTE3, 255, CENTRE);
    }
}

// ============================ REGLAGES ======================================
static const char *const LAB_SRC[2] = { "Micro", "Simulation" };
static const char *const LAB_PLAN[4] = { "Libre", "2 m", "4 m", "8 m" };
static const char *const LAB_PP[3] = { "10", "30", "60" };
static const char *const LAB_F0[3] = { "5 kHz", "10 kHz", "17 kHz" };
static const char *const LAB_VOL[3] = { "Doux", "Moyen", "Fort" };
static const char *const LAB_GAIN[3] = { "Bas", "Moyen", "Haut" };
static const int RY[5] = { 56, 85, 114, 143, 172 };

static void ligne_reglage(int i, const char *titre, const char *sous)
{
    int y = RY[i];
    texte(16, y, titre, POLICE_13, TEXTE);
    if (sous) texte(16, y + 15, sous, POLICE_11, TEXTE3);
    if (i < 4) rect(14, y + 27, 292, 1, GRILLE, 140);
}

static void ecran_reglages()
{
    panneau(6, 28, 308, 174, 150);
    char buf[96], b2[32];
    // pages
    page_aff = approche(page_aff, (float)R.page, 14);
    rect_arrondi(14, 32, 180, 18, 9, PANNEAU2, 230);
    int px = (int)(16 + page_aff * 90);
    rect_arrondi_degrade(px, 34, 86, 14, 7, rgb(60, 150, 255), rgb(120, 70, 240));
    texte(59, 34, "Expérience", POLICE_11, R.page == 0 ? TEXTE : TEXTE2, 255, CENTRE);
    texte(149, 34, "Appareil", POLICE_11, R.page == 1 ? TEXTE : TEXTE2, 255, CENTRE);
    snprintf(buf, sizeof buf, "SD %s", plat_sd() ? "✓ son.csv" : "absente");
    texte(306, 35, buf, POLICE_11, plat_sd() ? VERT : TEXTE3, 255, DROITE);

    if (R.page == 0) {
        ligne_reglage(0, "Source", R.source ? "Sons simulés réalistes" : "Haut-parleur et micro");
        segments(0, 168, RY[0], 138, 22, LAB_SRC, 2, R.source);
        ligne_reglage(1, "Plan (écho)", plan().desc);
        segments(1, 168, RY[1], 138, 22, LAB_PLAN, 4, R.plan);
        float cad = 1000.0f / 230;
        fmt(b2, sizeof b2, PAR_POINT[R.ppi] / cad, 0);
        snprintf(buf, sizeof buf, "≈ %s s par distance", b2);
        ligne_reglage(2, "Pings par point", buf);
        segments(2, 168, RY[2], 138, 22, LAB_PP, 3, R.ppi);
        float lam = c_th() / F0S[R.f0i] * 100;
        float vmin = c_th() * (dsp::EXCLU_DOP + 1) * dsp::FS / (float)dsp::N_DOP / (2 * F0S[R.f0i]);
        char sl[16], sv[16];
        fmt(sl, sizeof sl, lam, 1);
        fmt(sv, sizeof sv, vmin, 1);
        snprintf(buf, sizeof buf, "λ = %s cm · dès %s m/s", sl, sv);
        ligne_reglage(3, "Fréquence Doppler", buf);
        segments(3, 168, RY[3], 138, 22, LAB_F0, 3, R.f0i);
        fmt(b2, sizeof b2, c_th(), 1);
        snprintf(buf, sizeof buf, "c théorique %s m/s", b2);
        ligne_reglage(4, "Température", buf);
        fmt(b2, sizeof b2, R.temperature, 0);
        snprintf(buf, sizeof buf, "%s °C", b2);
        texte(162, RY[4] + 1, buf, POLICE_13, ORANGE, 255, DROITE);
        curseur(176, 300, RY[4] + 10, R.temperature / 40.0f, tc_cible == C_TEMP, BLEU, ORANGE);
    } else {
        ligne_reglage(0, "Volume", "Haut-parleur de la carte");
        segments(4, 168, RY[0], 138, 22, LAB_VOL, 3, R.vol);
        ligne_reglage(1, "Gain du micro", "Baisse-le si le son direct sature");
        segments(5, 168, RY[1], 138, 22, LAB_GAIN, 3, R.gain);
        ligne_reglage(2, "Luminosité", nullptr);
        snprintf(buf, sizeof buf, "%d %%", (int)R.lum);
        texte(162, RY[2] + 1, buf, POLICE_13, CYAN, 255, DROITE);
        curseur(176, 300, RY[2] + 10, (R.lum - 5) / 95.0f, tc_cible == C_LUM, BLEU, CYAN);
        // effacer
        if (conf_effacer && t - conf_t0 > 3) conf_effacer = false;
        int ey = RY[3] - 2;
        if (conf_effacer) {
            rect_arrondi(14, ey, 150, 24, 12, ROUGE, 230);
            texte(89, ey + 5, "Confirmer ?", POLICE_13, TEXTE, 255, CENTRE);
        } else {
            rect_arrondi(14, ey, 150, 24, 12, PANNEAU2, 230);
            contour_arrondi(14, ey, 150, 24, 12, 1, ROUGE, 180);
            int tot = son::nb_pts + son::nb_passages;
            if (tot) snprintf(buf, sizeof buf, "Effacer %d pts · %d passages", son::nb_pts, son::nb_passages);
            else snprintf(buf, sizeof buf, "Rien à effacer");
            texte(89, ey + 6, buf, POLICE_11, tot ? ROUGE : TEXTE3, 255, CENTRE);
        }
        texte(306, RY[3] + 1, "Audio", POLICE_11, TEXTE3, 255, DROITE);
        texte(306, RY[3] + 13, plat_audio_etat(), POLICE_11, plat_audio_ok() ? VERT : ORANGE, 255, DROITE);
        rect(14, RY[3] + 27, 292, 1, GRILLE, 140);
        snprintf(buf, sizeof buf, "Vitesse du son %s · ES3C28P · ES8311", SON_VERSION);
        texte(16, RY[4], buf, POLICE_11, TEXTE2);
        snprintf(buf, sizeof buf, "%d i/s · %lu Ko libres · %lu FFT", (int)(ips + 0.5f),
                 (unsigned long)(plat_memoire_libre() / 1024), (unsigned long)dsp::nb_fft());
        texte(16, RY[4] + 14, buf, POLICE_11, TEXTE3);
    }
}

static void changer_source(int s)
{
    if (s == R.source) return;
    if (son::en_collecte()) son::arreter();
    son::radar = 0;
    R.source = s;
    son::source = s;
    sauver_reglages();
    toast(s ? "Mode simulation" : "Mode micro réel", s ? VIOLET : VERT);
}

static void tap_reglages(int x, int y)
{
    if (dans(x, y, 14, 30, 180, 22)) {
        int p = (x - 14) / 90;
        if (p != R.page) { R.page = p; conf_effacer = false; sauver_reglages(); }
        return;
    }
    int ligne = -1;
    for (int i = 0; i < 5; i++) if (y >= RY[i] - 3 && y < RY[i] + 26) ligne = i;
    if (ligne < 0) return;
    bool ctrl = dans(x, y, 168, RY[ligne] - 3, 138, 28);
    if (R.page == 0) {
        if (ligne == 0 && ctrl) changer_source((x - 168) * 2 / 138);
        else if (ligne == 1 && ctrl) {
            int p = (x - 168) * 4 / 138;
            if (p != R.plan) {
                R.plan = p;
                int s = plan_suivant();
                if (s >= 0 && p != 0) regler_distance(PLANS[p].d[s]);
                sauver_reglages();
            }
        } else if (ligne == 2 && ctrl) {
            int p = (x - 168) * 3 / 138;
            if (son::en_collecte()) toast("Mesure en cours…", JAUNE);
            else if (p != R.ppi) { R.ppi = p; son::par_point = PAR_POINT[p]; sauver_reglages(); }
        } else if (ligne == 3 && ctrl) {
            int p = (x - 168) * 3 / 138;
            if (p != R.f0i) { R.f0i = p; son::f0 = F0S[p]; sauver_reglages(); }
        }
    } else {
        if (ligne == 0 && ctrl) {
            int p = (x - 168) * 3 / 138;
            if (p != R.vol) { R.vol = p; plat_audio_niveaux(R.vol, R.gain); sauver_reglages(); }
        } else if (ligne == 1 && ctrl) {
            int p = (x - 168) * 3 / 138;
            if (p != R.gain) { R.gain = p; plat_audio_niveaux(R.vol, R.gain); sauver_reglages(); }
        } else if (ligne == 3 && dans(x, y, 14, RY[3] - 4, 150, 28)) {
            if (!conf_effacer) {
                if (son::nb_pts + son::nb_passages > 0) { conf_effacer = true; conf_t0 = t; }
            } else {
                conf_effacer = false;
                son::effacer();
                sel_point = -1;
                ax_init = false;
                aff_a = aff_b = 0;
                toast("Tout est effacé", ORANGE);
            }
        }
    }
}

static void lum_toucher(int x)
{
    int v = 5 + (int)((x - 176) * 95.0f / 124.0f + 0.5f);
    if (v < 5) v = 5;
    if (v > 100) v = 100;
    if (v != R.lum) { R.lum = v; plat_luminosite(v); }
}

static void temp_toucher(int x)
{
    float v = roundf((x - 176) * 40.0f / 124.0f);
    if (v < 0) v = 0;
    if (v > 40) v = 40;
    R.temperature = v;
    son::temperature = v;
}

// ============================ Revelation ====================================
static void lancer_revelation(const char *titre, float val, int dec, const char *unite, const char *ligne,
                              uint16_t c, float duree)
{
    revel = true;
    revel_t0 = t;
    revel_duree = duree;
    revel_val = val;
    revel_dec = dec;
    revel_c = c;
    strncpy(revel_titre, titre, sizeof revel_titre - 1);
    strncpy(revel_unite, unite, sizeof revel_unite - 1);
    strncpy(revel_ligne, ligne, sizeof revel_ligne - 1);
    const uint16_t cols[4] = { JAUNE, MAGENTA, CYAN, ORANGE };
    for (int i = 0; i < 90; i++) {
        float a = alea() * 2 * PI_F, v = 40 + alea() * 160;
        part[i] = { 160, 100, cosf(a) * v, sinf(a) * v - 30, 1.2f + alea() * 1.3f, cols[i % 4] };
    }
}

static void dessiner_revelation()
{
    float age = t - revel_t0;
    if (age > revel_duree) { revel = false; return; }
    float f = age < 0.3f ? age / 0.3f : (age > revel_duree - 0.5f ? 1 - (age - (revel_duree - 0.5f)) / 0.5f : 1);
    f = sat01(f);
    rect(0, 0, L, H, NOIR_BLEU, (uint8_t)(200 * f));
    // ondes sonores concentriques
    for (int k = 0; k < 3; k++) {
        float a = age - k * 0.18f;
        if (a > 0 && a < 1.3f)
            anneau(160, 100, 10 + 210 * sortie(a / 1.3f), 2.5f - k * 0.5f, k == 1 ? revel_c : JAUNE,
                   (uint8_t)(200 * (1 - a / 1.3f)));
    }
    for (int i = 0; i < 90; i++) {
        Particule &p = part[i];
        if (age > p.vie) continue;
        p.vy += 60 * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vx *= expf(-1.2f * dt);
        p.vy *= expf(-1.2f * dt);
        float a = 1 - age / p.vie;
        halo(p.x, p.y, 6, p.c, (uint8_t)(160 * a * f));
        disque(p.x, p.y, 1.4f, p.c, (uint8_t)(255 * a * f));
    }
    uint8_t a = (uint8_t)(255 * f);
    texte(160, 46, revel_titre, POLICE_16, TEXTE2, a, CENTRE);
    float v = revel_val * sortie(age / 1.2f);
    char buf[32];
    fmt(buf, sizeof buf, v, revel_dec);
    halo(160, 100, 90, revel_c, (uint8_t)(70 * f));
    texte(160, 72, buf, POLICE_40, TEXTE, a, CENTRE);
    texte(160, 122, revel_unite, POLICE_16, revel_c, a, CENTRE);
    if (age > 1.1f) {
        uint8_t a2 = (uint8_t)(a * sat01((age - 1.1f) / 0.4f));
        texte(160, 148, revel_ligne, POLICE_13, JAUNE, a2, CENTRE);
    }
    if (age > 1.6f) texte(160, 178, "Touchez pour continuer", POLICE_11, TEXTE3, (uint8_t)(a * (0.4f + 0.6f * pulse(0.8f))), CENTRE);
}

// ============================ Ecran de demarrage ============================
static void dessiner_splash()
{
    float age = t - splash_t0;
    fond();
    // un "ping" part du centre, rebondit sur les bords, revient
    for (int k = 0; k < 4; k++) {
        float a = age - k * 0.22f;
        if (a < 0) continue;
        float r = 12 + 150 * sortie(a / 1.6f);
        anneau(160, 96, r, 2.2f, k % 2 ? MAGENTA : JAUNE, (uint8_t)(170 * (1 - sat01(a / 1.6f))));
    }
    float a = sat01((age - 0.4f) / 0.6f);
    halo(160, 92, 80, MAGENTA, (uint8_t)(60 * a));
    halo(160, 92, 50, JAUNE, (uint8_t)(50 * a));
    hp(110, 92, 2.6f, rgb(255, 236, 170), pulse(1.5f));
    texte(176, 76, "SON", POLICE_30, TEXTE, (uint8_t)(255 * a), CENTRE);
    texte(160, 128, "Mesurer la vitesse du son par écho", POLICE_13, TEXTE2, (uint8_t)(255 * sat01((age - 0.8f) / 0.5f)), CENTRE);
    texte(160, 146, "et des vitesses par effet Doppler", POLICE_13, TEXTE2, (uint8_t)(255 * sat01((age - 1.0f) / 0.5f)), CENTRE);
    texte(160, 166, "haut-parleur + micro de la carte ESP32-S3", POLICE_11, TEXTE3, (uint8_t)(255 * sat01((age - 1.2f) / 0.5f)), CENTRE);
    char buf[64];
    snprintf(buf, sizeof buf, "%s · audio : %s", SON_VERSION, plat_audio_etat());
    texte(160, 220, buf, POLICE_11, plat_audio_ok() ? TEXTE3 : ORANGE, (uint8_t)(200 * a), CENTRE);
    if (age > 2.8f) splash = false;
}

// ============================ Toucher =======================================
static void changer_mode(int m)
{
    if (m == R.mode) return;
    if (son::en_collecte()) son::arreter();
    son::radar = 0;
    R.mode = m;
    son::mode = m;
    sel_point = -1;
    onglet_t0 = t;
    aff_c = 0;
    sauver_reglages();
    toast(m == son::MODE_ECHO ? "Écho : vitesse du son" : "Doppler : vitesse de ce qui bouge", m ? CYAN : JAUNE);
}

static void tap(int x, int y)
{
    if (revel) { revel = false; return; }
    if (y < 26) {
        if (dans(x, y, 58, 0, 124, 26)) changer_mode(x < 58 + 62 ? son::MODE_ECHO : son::MODE_DOPPLER);
        return;
    }
    if (y >= 204) {
        int o = x / 64;
        if (o >= 0 && o < NB_ONGLETS && o != onglet) {
            onglet = o;
            onglet_t0 = t;
            sel_point = -1;
            conf_effacer = false;
        }
        return;
    }
    switch (onglet) {
    case O_MESURE: if (echo()) tap_mesure_echo(x, y); else tap_mesure_dop(x, y); break;
    case O_COURBE: if (echo()) tap_courbe_echo(x, y); break;
    case O_REGLAGES: tap_reglages(x, y); break;
    default: break;
    }
}

void toucher(bool appui, int x, int y)
{
    if (splash) {
        if (appui && t - splash_t0 > 0.4f) splash = false;
        tc_appui = appui;
        tc_cible = C_RIEN;
        return;
    }
    if (appui && !tc_appui) {
        tc_x0 = tc_x = tc_xp = x;
        tc_y0 = tc_y = y;
        tc_t0 = t;
        tc_bouge = false;
        tc_cible = C_RIEN;
        if (!revel && onglet == O_MESURE && echo() && dans(x, y, 46, 118, 228, 38) && !son::en_collecte()) {
            tc_cible = C_REGLE;
            regle_v = 0;
        }
        if (!revel && onglet == O_REGLAGES && R.page == 1 && dans(x, y, 166, RY[2] - 2, 146, 26)) {
            tc_cible = C_LUM;
            lum_toucher(x);
        }
        if (!revel && onglet == O_REGLAGES && R.page == 0 && dans(x, y, 166, RY[4] - 2, 146, 26)) {
            tc_cible = C_TEMP;
            temp_toucher(x);
        }
    } else if (appui && tc_appui) {
        if (abs(x - tc_x0) > 6 || abs(y - tc_y0) > 6) tc_bouge = true;
        if (tc_cible == C_REGLE) {
            float dm = -(x - tc_xp) / PX_M;
            regle_d += dm;
            if (regle_d < D_MIN) regle_d = D_MIN;
            if (regle_d > D_MAX) regle_d = D_MAX;
            regle_cible = roundf(regle_d * 20) / 20.0f;
            son::d_visee = regle_cible;
            float vinst = dt > 0 ? dm / dt : 0;
            regle_v = regle_v * 0.6f + vinst * 0.4f;
        } else if (tc_cible == C_LUM) {
            lum_toucher(x);
        } else if (tc_cible == C_TEMP) {
            temp_toucher(x);
        }
        tc_xp = x;
        tc_x = x;
        tc_y = y;
    } else if (!appui && tc_appui) {
        if (tc_cible == C_REGLE) {
            if (!tc_bouge) { regle_v = 0; tap(tc_x0, tc_y0); }
            else if (fabsf(regle_v) < 0.3f) regle_v = 0;
            sauver_reglages();
        } else if (tc_cible == C_LUM || tc_cible == C_TEMP) {
            sauver_reglages();
        } else if (!tc_bouge && t - tc_t0 < 0.8f) {
            tap(tc_x0, tc_y0);
        }
        tc_cible = C_RIEN;
    }
    tc_appui = appui;
}

// ============================ Boucle ========================================
void init(uint16_t *fond_buf)
{
    s_fond = fond_buf;
    if (s_fond) {
        uint16_t *cur = gfx::fb;
        gfx::fb = s_fond;
        dessiner_fond_complet();
        gfx::fb = cur;
    }
    init_etoiles();
    init_lut();
    Reglages lu;
    if (plat_nvs_lire("reglages", &lu, sizeof lu) && lu.magie == R.magie) {
        if (lu.mode >= 0 && lu.mode <= 1) R.mode = lu.mode;
        if (lu.source >= 0 && lu.source <= 1) R.source = lu.source;
        if (lu.plan >= 0 && lu.plan <= 3) R.plan = lu.plan;
        if (lu.ppi >= 0 && lu.ppi <= 2) R.ppi = lu.ppi;
        if (lu.f0i >= 0 && lu.f0i <= 2) R.f0i = lu.f0i;
        if (lu.vol >= 0 && lu.vol <= 2) R.vol = lu.vol;
        if (lu.gain >= 0 && lu.gain <= 2) R.gain = lu.gain;
        if (lu.lum >= 5 && lu.lum <= 100) R.lum = lu.lum;
        if (lu.page >= 0 && lu.page <= 1) R.page = lu.page;
        if (lu.distance >= D_MIN && lu.distance <= D_MAX) R.distance = lu.distance;
        if (lu.temperature >= 0 && lu.temperature <= 40) R.temperature = lu.temperature;
    }
    son::mode = R.mode;
    son::source = R.source;
    son::par_point = PAR_POINT[R.ppi];
    son::f0 = F0S[R.f0i];
    son::temperature = R.temperature;
    son::radar = 0;
    plat_luminosite(R.lum);
    plat_audio_niveaux(R.vol, R.gain);
    son::charger();
    for (int i = 0; i < son::MAX_PTS; i++) naissance[i] = -10;
    regle_cible = regle_d = R.distance;
    int s = plan_suivant();
    if (s >= 0 && R.plan != 0) regle_cible = regle_d = plan().d[s];
    son::d_visee = regle_cible;
    xm_aff = x_mur(regle_cible);
    onglet_aff = (float)onglet;
    mode_aff = (float)R.mode;
    page_aff = (float)R.page;
}

void image(uint32_t ms)
{
    if (ms_prec == 0) ms_prec = ms;
    dt = (ms - ms_prec) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.001f) dt = 0.001f;
    ms_prec = ms;
    t += dt;
    ips = ips * 0.95f + (1.0f / dt) * 0.05f;
    if (splash_t0 == 0 && splash) splash_t0 = t;
    if (t - snr_t > 0.3f) {
        snr_t = t;
        float s = son::echo_recent() ? son::snr_direct() : 0;
        if (snr_n < 40) snr_hist[snr_n++] = s;
        else { memmove(snr_hist, snr_hist + 1, 39 * sizeof(float)); snr_hist[39] = s; }
    }

    int ev = son::tick(ms);
    char buf[96], b2[48];
    if (ev == son::EV_POINT) {
        int idx = -1;
        for (int i = 0; i < son::nb_pts; i++) if (fabsf(son::pts[i].d - son::collecte_d()) < 0.005f) idx = i;
        if (idx >= 0) {
            for (int k = son::nb_pts - 1; k > idx; k--) naissance[k] = naissance[k - 1];
            naissance[idx] = t;
        }
        ligne_t0 = t;
        char d[16];
        fmt_distance(d, sizeof d, son::collecte_d());
        int s = plan_suivant();
        if (s >= 0 && R.plan != 0) {
            char d2[16];
            fmt_distance(d2, sizeof d2, plan().d[s]);
            snprintf(buf, sizeof buf, "✓ %s mesuré · suivant %s", d, d2);
            regler_distance(plan().d[s]);
        } else if (R.plan != 0) {
            snprintf(buf, sizeof buf, "✓ Plan terminé ! Va voir Résultat");
        } else {
            snprintf(buf, sizeof buf, "✓ Point %s ajouté", d);
        }
        toast(buf, VERT);
        if (son::aj.ok && son::nb_pts >= 2) {
            float ref = c_th();
            float ec = (float)(100.0 * (son::aj.c - ref) / ref);
            fmt(b2, sizeof b2, ec, 2);
            char r[16];
            fmt(r, sizeof r, ref, 1);
            snprintf(buf, sizeof buf, "écart %s%s %% · théorie %s m/s à %d °C", ec >= 0 ? "+" : "", b2, r,
                     (int)lroundf(R.temperature));
            lancer_revelation("vitesse du son mesurée", (float)son::aj.c, 1, "m/s", buf, JAUNE, 4.5f);
        }
    } else if (ev == son::EV_ECHEC) {
        toast("Mesure invalide : pas assez d'échos nets", ROUGE);
    } else if (ev == son::EV_PASSAGE && son::nb_passages > 0) {
        const son::Passage &p = son::passages[son::nb_passages - 1];
        uint16_t ca;
        const char *al = allure(p.vmax, ca);
        fmt(b2, sizeof b2, p.vmax * 3.6f, 1);
        snprintf(buf, sizeof buf, "%s km/h · %s · %s", b2, al, p.sens > 0 ? "▲ approche" : "▼ éloignement");
        char l[96], v[16];
        fmt(v, sizeof v, p.vmax, 2);
        snprintf(l, sizeof l, "Passage %s %s m/s · %s km/h", p.sens > 0 ? "▲" : "▼", v, b2);
        toast(l, p.sens > 0 ? CYAN : MAGENTA);
        vflash = 1;
        vflash_c = p.sens > 0 ? CYAN : MAGENTA;
        aff_c = 0;
    }

    if (splash) { dessiner_splash(); return; }

    fond();
    switch (onglet) {
    case O_MESURE: if (echo()) ecran_mesure_echo(); else ecran_mesure_dop(); break;
    case O_COURBE: if (echo()) ecran_courbe_echo(); else ecran_courbe_dop(); break;
    case O_RESULTAT: if (echo()) ecran_resultat_echo(); else ecran_resultat_dop(); break;
    case O_STATS: if (echo()) ecran_stats_echo(); else ecran_stats_dop(); break;
    case O_REGLAGES: ecran_reglages(); break;
    }
    float ft = (t - onglet_t0) / 0.25f;
    if (onglet_t0 >= 0 && ft < 1) rect(0, 26, L, 178, FOND_HAUT, (uint8_t)(230 * (1 - sortie(ft))));
    barre_haut();
    barre_onglets();
    if (revel) dessiner_revelation();
    else dessiner_toast();
}

}  // namespace ui
