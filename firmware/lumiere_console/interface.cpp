// interface.cpp - ecrans animes de la console "Vitesse de la lumiere"
//
// Onglets : MESURE (scene animee + regle tactile + bouton), COURBE (droite
// temps/distance animee), RESULTAT (c, jauge d'ecart, convergence), STATS
// (compteurs, histogramme du temps de vol), REGLAGES.
#include "interface.h"
#include "gfx.h"
#include "mesure.h"
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
static float ips = 30;                // images par seconde (lissee)
static uint32_t ms_prec = 0;
static bool splash = true;
static float splash_t0 = 0;

static int onglet = O_MESURE;
static float onglet_aff = 0;          // position animee du surlignage
static float onglet_t0 = -1;          // debut du fondu d'entree

// Reglages memorises
struct Reglages {
    uint32_t magie;
    int32_t source, plan, ppi, lum;
    float distance;
};
static Reglages R = { 0x52454731, mes::SRC_REEL, 2, 1, 80, 2.0f };
static const int PAR_POINT[3] = { 200, 600, 1500 };

// Plans de mesure
struct Plan { const char *nom; const char *desc; int n; float d[8]; };
static const Plan PLANS[4] = {
    { "Libre", "Distances au choix (raccourcis)", 7, { 1, 2, 5, 10, 20, 30, 50 } },
    { "10 m", "8 distances de 2 à 10 m", 8, { 2, 3, 4, 5, 6, 7, 8, 10 } },
    { "20 m", "8 distances de 2 à 20 m", 8, { 2, 4, 6, 8, 10, 12, 15, 20 } },
    { "50 m", "8 distances de 5 à 50 m (dehors)", 8, { 5, 10, 15, 20, 25, 30, 40, 50 } },
};

// Regle des distances
static const float PX_M = 48.0f;      // pixels par metre
static float regle_d = 1.0f;          // position affichee (continue)
static float regle_cible = 1.0f;      // valeur reglee (au 1/10 de m)
static float regle_v = 0;             // vitesse d'inertie (m/s)
static bool regle_lachee = true;

// Scene
static float xb_aff = 120;
struct Onde { float x, y, t0; uint16_t c; };
static Onde ondes[8];
static int nb_ondes = 0;
static float photon_prec = 0;
static float led_a = 0, led_b = 0;

// Courbe
static float naissance[mes::MAX_PTS];
static int sel_point = -1;
static float ax_max = 10, ay_min = 240, ay_max = 330;
static float aff_a = 0, aff_b = 0;
static float ligne_t0 = -10;
static bool ax_init = false;

// Resultat
static float aff_c = 0;
static float aig_ang = PI_F * 1.5f, aig_v = 0;

// Stats
static mes::Histo histo;
static float histo_t = -1;
static float aff_bins[48];
static float aff_cpt[4];
static float debit_hist[40];
static int debit_n = 0;
static float debit_t = 0;

// Reglages
static float seg_aff[3] = { -1, -1, -1 };
static bool conf_effacer = false;
static float conf_t0 = 0;

// Toast
static char toast_txt[64] = "";
static uint16_t toast_c = CYAN;
static float toast_t0 = -10;

// Revelation du resultat
struct Particule { float x, y, vx, vy, vie; uint16_t c; };
static Particule part[90];
static bool revel = false;
static float revel_t0 = 0;
static float revel_c = 0, revel_ecart = 0;

// Toucher
enum { C_RIEN, C_REGLE, C_LUM };
static bool tc_appui = false;
static int tc_x = 0, tc_y = 0, tc_x0 = 0, tc_y0 = 0, tc_xp = 0;
static float tc_t0 = 0;
static bool tc_bouge = false;
static int tc_cible = C_RIEN;

// Etoiles
struct Etoile { float x, y, v, ph; uint8_t lum; };
static Etoile etoiles[64];

// ============================ Outils ========================================
static inline float sat01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float approche(float v, float c, float k) { return v + (c - v) * (1.0f - expf(-k * dt)); }
static inline float sortie(float x) { x = sat01(x); return 1 - (1 - x) * (1 - x) * (1 - x); }
static inline float pulse(float f, float ph = 0) { return 0.5f + 0.5f * sinf(t * f * 2 * PI_F + ph); }

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
    fmt(b, n, d, d >= 100 ? 0 : 1);
    strncat(b, " m", n - strlen(b) - 1);
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
    for (int i = 0; i < mes::nb_pts; i++)
        if (fabsf(mes::pts[i].d - d) < 0.05f) return true;
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
    if (d < 0.1f) d = 0.1f;
    if (d > 200) d = 200;
    regle_cible = roundf(d * 10) / 10.0f;
    regle_v = 0;
    regle_lachee = true;
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
    for (int i = 0; i < 64; i++) {
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
    for (int i = 0; i < 64; i++) {
        Etoile &e = etoiles[i];
        e.x -= e.v * dt;
        if (e.x < 0) { e.x += L; e.y = alea() * H; }
        float a = e.lum * (0.55f + 0.45f * sinf(t * 1.7f + e.ph));
        pixel_ajout((int)e.x, (int)e.y, TEXTE, (uint8_t)a);
        if (e.lum > 190) halo(e.x + 0.5f, e.y + 0.5f, 3, CYAN, (uint8_t)(a * 0.5f));
    }
}

// ============================ Elements communs ==============================
static void panneau(int x, int y, int l, int h, uint8_t a = 150)
{
    rect_arrondi(x, y, l, h, 12, PANNEAU, a);
    contour_arrondi(x, y, l, h, 12, 1, BORD, 110);
}

static void barre_haut()
{
    rect(0, 0, L, 24, PANNEAU, 190);
    degrade_h(0, 24, L, 1, CYAN, VIOLET, 140);
    // logo : une petite source de lumiere qui respire
    halo(12, 12, 11 + 2 * pulse(0.7f), JAUNE, (uint8_t)(110 + 60 * pulse(0.7f)));
    disque(12, 12, 3.5f, rgb(255, 245, 200));
    texte(24, 4, "LUMIÈRE", POLICE_13, TEXTE);
    texte(24 + largeur_texte("LUMIÈRE", POLICE_13) + 6, 6, "mesure de c", POLICE_11, TEXTE3);

    char buf[40];
    uint16_t c;
    bool clignote = false;
    if (mes::source == mes::SRC_SIM) { snprintf(buf, sizeof buf, "SIMULATION"); c = VIOLET; }
    else if (plat_carte_a() && mes::signal_recent()) { snprintf(buf, sizeof buf, "Carte A  %d dBm", mes::rssi_direct()); c = VERT; }
    else if (plat_carte_a()) { snprintf(buf, sizeof buf, "Carte A trouvée"); c = JAUNE; }
    else { snprintf(buf, sizeof buf, "Recherche carte A…"); c = ORANGE; clignote = true; }
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
        rect_arrondi((int)cx - 9, (int)cy - 4, 5, 8, 1, c);
        rect_arrondi((int)cx + 4, (int)cy - 4, 5, 8, 1, c);
        trait(cx - 3, cy, cx + 3, cy, 1.2f, c);
        disque(cx, cy, 1.6f, c);
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
    degrade_h(0, 204, L, 1, VIOLET, CYAN, 120);
    onglet_aff = approche(onglet_aff, (float)onglet, 14);
    int hx = (int)(4 + onglet_aff * 64);
    rect_arrondi_degrade(hx, 207, 56, 31, 10, rgb(40, 70, 170), rgb(70, 40, 150), 200);
    contour_arrondi(hx, 207, 56, 31, 10, 1, CYAN, 150);
    halo(hx + 28, 239, 26, CYAN, 70);
    for (int i = 0; i < NB_ONGLETS; i++) {
        bool actif = i == onglet;
        float cx = i * 64 + 32;
        icone(i, cx, 216, actif ? CYAN : TEXTE3);
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
    int y = (int)(-20 + 50 * e);             // descend depuis la barre du haut
    uint8_t a = (uint8_t)(255 * sat01(e));
    rect_arrondi(x, y, l, 24, 12, PANNEAU2, (uint8_t)(a * 0.95f));
    contour_arrondi(x, y, l, 24, 12, 1.2f, toast_c, a);
    halo(x + 14, y + 12, 10, toast_c, (uint8_t)(a * 0.5f));
    disque(x + 14, y + 12, 4, toast_c, a);
    texte(x + 24, y + 5, toast_txt, POLICE_13, TEXTE, a);
}

// Controle a segments (reglages)
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

// ============================ Ecran MESURE ==================================
static float x_carte_b(float d)
{
    float f = (logf(d < 0.3f ? 0.3f : d) - logf(0.3f)) / (logf(60.0f) - logf(0.3f));
    return 92 + sat01(f) * (288 - 92);
}

static void carte(float x, float y, const char *nom, uint16_t c, float led)
{
    halo(x, y, 26, c, 30);
    trait(x - 6, y - 16, x - 6, y - 24, 2, c);
    disque(x - 6, y - 25, 2.2f, c);
    rect_arrondi_degrade((int)x - 12, (int)y - 16, 24, 32, 5, rgb(36, 48, 104), rgb(14, 20, 48));
    contour_arrondi((int)x - 12, (int)y - 16, 24, 32, 5, 1.2f, c, 200);
    rect_arrondi((int)x - 7, (int)y - 4, 14, 14, 2, rgb(44, 54, 100));
    texte((int)x, (int)y - 3, nom, POLICE_11, TEXTE, 255, CENTRE);
    if (led > 0.02f) halo(x + 6, y - 11, 6, VERT, (uint8_t)(160 * led));
    disque(x + 6, y - 11, 1.6f, led > 0.1f ? VERT : rgb(30, 70, 50));
}

static void onde(float x, float y, uint16_t c)
{
    if (nb_ondes < 8) ondes[nb_ondes++] = { x, y, t, c };
}

static void ecran_mesure()
{
    // ---------- scene ----------
    panneau(6, 28, 308, 86, 140);
    float d_aff = mes::en_collecte() ? mes::collecte_d() : regle_cible;
    xb_aff = approche(xb_aff, x_carte_b(d_aff), 6);
    const float xa = 34, yc = 58;
    float xb = xb_aff;
    float x0 = xa + 13, x1 = xb - 13;

    decoupe(7, 29, 313, 113);
    // onde Wi-Fi
    trait(x0, yc, x1, yc, 1, CYAN, 40);
    float ph = t * 9;
    float px = x0, py = yc;
    for (float x = x0 + 3; x <= x1; x += 3) {
        float env = sinf(PI_F * (x - x0) / (x1 - x0 > 1 ? x1 - x0 : 1));
        float y = yc + 5 * env * sinf((x - x0) / 18.0f * 2 * PI_F - ph);
        trait(px, py, x, y, 1.2f, CYAN, 80);
        px = x; py = y;
    }
    // photons : aller (jaune) puis retour (cyan)
    float periode = mes::en_collecte() ? 1.1f : 1.8f;
    int nph = mes::en_collecte() ? 3 : 1;
    for (int k = 0; k < nph; k++) {
        float u = fmodf(t / periode + k / (float)nph, 1.0f);
        bool aller = u < 0.5f;
        float v = aller ? u * 2 : (u - 0.5f) * 2;
        float e = v * v * (3 - 2 * v);
        float x = aller ? x0 + (x1 - x0) * e : x1 - (x1 - x0) * e;
        uint16_t c = aller ? JAUNE : CYAN;
        float dir = aller ? -1 : 1;
        for (int q = 6; q >= 1; q--)
            disque(x + dir * q * 3.2f, yc, 2.6f - q * 0.3f, c, (uint8_t)(150 - q * 22));
        halo(x, yc, 16, c, 140);
        disque(x, yc, 3, rgb(255, 255, 235));
        if (k == 0) {
            if (photon_prec < 0.5f && u >= 0.5f) onde(x1 + 13, yc, JAUNE);
            if (photon_prec > u) onde(xa, yc, CYAN);
            photon_prec = u;
        }
    }
    // ondes de choc a l'arrivee
    for (int i = 0; i < nb_ondes;) {
        float age = t - ondes[i].t0;
        if (age > 0.7f) { ondes[i] = ondes[--nb_ondes]; continue; }
        anneau(ondes[i].x, ondes[i].y, 6 + 30 * sortie(age / 0.7f), 1.5f, ondes[i].c,
               (uint8_t)(200 * (1 - age / 0.7f)));
        i++;
    }
    // activite des cartes
    if (mes::nb_nouveaux() > 0) { led_a = 1; led_b = 1; }
    led_a = approche(led_a, 0, 8);
    led_b = approche(led_b, 0, 6);
    carte(xa, yc, "A", CYAN, led_a);
    carte(xb, yc, "B", JAUNE, led_b);

    // cote (distance)
    float yd = 80;
    trait(xa, yd, xb, yd, 1, TEXTE3, 200);
    triangle(xa, yd, xa + 6, yd - 3, xa + 6, yd + 3, TEXTE3);
    triangle(xb, yd, xb - 6, yd - 3, xb - 6, yd + 3, TEXTE3);
    char buf[96];
    fmt_distance(buf, sizeof buf, d_aff);
    int lb = largeur_texte(buf, POLICE_13) + 14;
    int bx = (int)((xa + xb) / 2 - lb / 2.0f);
    rect_arrondi(bx, (int)yd - 9, lb, 18, 9, NOIR_BLEU, 240);
    contour_arrondi(bx, (int)yd - 9, lb, 18, 9, 1, JAUNE, 180);
    texte(bx + lb / 2, (int)yd - 7, buf, POLICE_13, JAUNE, 255, CENTRE);

    // lectures en direct
    float rtt = mes::rtt_direct();
    int lx = texte(14, 95, "Temps de vol ", POLICE_11, TEXTE3);
    if (rtt > 0 && mes::signal_recent()) {
        fmt(buf, sizeof buf, rtt, 1);
        strcat(buf, " ns");
        texte(14 + lx, 93, buf, POLICE_13, CYAN);
    } else {
        texte(14 + lx, 93, "—", POLICE_13, TEXTE3);
    }
    double vrai = 2.0 * d_aff / mes::C_REF;                 // s
    double ralenti = periode / vrai;
    if (ralenti >= 1e9) { fmt(buf, sizeof buf, ralenti / 1e9, 1); strcat(buf, " milliards ×"); }
    else                { fmt(buf, sizeof buf, ralenti / 1e6, 0); strcat(buf, " millions ×"); }
    int lr = largeur_texte(buf, POLICE_11);
    texte(306, 95, buf, POLICE_11, TEXTE2, 255, DROITE);
    texte(306 - lr - 4, 95, "ralenti", POLICE_11, TEXTE3, 255, DROITE);
    sans_decoupe();

    // ---------- regle des distances ----------
    bool verrou = mes::en_collecte();
    panneau(6, 118, 308, 38, 150);
    if (tc_cible != C_REGLE) {
        if (fabsf(regle_v) > 0.02f) {                       // inertie
            regle_d += regle_v * dt;
            regle_v *= expf(-3.5f * dt);
            if (regle_d < 0.1f) { regle_d = 0.1f; regle_v = 0; }
            if (regle_d > 200) { regle_d = 200; regle_v = 0; }
            regle_cible = roundf(regle_d * 10) / 10.0f;
        } else {
            regle_v = 0;
            regle_d = approche(regle_d, regle_cible, 12);
        }
    }
    decoupe(8, 119, 312, 155);
    float m0 = regle_d - 160 / PX_M, m1 = regle_d + 160 / PX_M;
    for (int k = (int)floorf(m0 * 10); k <= (int)ceilf(m1 * 10); k++) {
        if (k < 1) continue;
        float m = k / 10.0f;
        float x = 160 + (m - regle_d) * PX_M;
        int h = (k % 10 == 0) ? 14 : (k % 5 == 0 ? 9 : 5);
        uint16_t c = (k % 10 == 0) ? TEXTE2 : TEXTE3;
        trait(x, 154 - h, x, 154, 1, c, verrou ? 90 : 220);
        if (k % 10 == 0 && fabsf(x - 160) > 9) {        // le repere jaune cache le centre
            snprintf(buf, sizeof buf, "%d", k / 10);
            texte((int)x, 122, buf, POLICE_11, TEXTE2, verrou ? 90 : 230, CENTRE);
        }
    }
    for (int i = 0; i < 40; i++) {                          // fondu des bords
        uint8_t a = (uint8_t)(230 * (1 - i / 40.0f));
        rect(8 + i, 119, 1, 36, PANNEAU, a);
        rect(311 - i, 119, 1, 36, PANNEAU, a);
    }
    sans_decoupe();
    halo(160, 140, 16, JAUNE, verrou ? 40 : 90);
    trait(160, 126, 160, 154, 2, JAUNE, verrou ? 120 : 255);
    triangle(154, 120, 166, 120, 160, 127, JAUNE, verrou ? 120 : 255);
    texte(18, 127, "‹", POLICE_22, TEXTE2, verrou ? 60 : 200, CENTRE);
    texte(302, 127, "›", POLICE_22, TEXTE2, verrou ? 60 : 200, CENTRE);

    // ---------- bouton ----------
    const int BX = 6, BY = 160, BL = 140, BH = 42;
    bool pret = mes::source == mes::SRC_SIM || plat_carte_a();
    if (mes::en_collecte()) {
        float p = mes::collectes() / (float)mes::par_point;
        rect_arrondi(BX, BY, BL, BH, 12, PANNEAU2, 230);
        decoupe(BX, BY, BX + (int)(BL * p), BY + BH);
        rect_arrondi_degrade(BX, BY, BL, BH, 12, rgb(30, 190, 255), rgb(110, 70, 240));
        float sx = BX + fmodf(t * 160, BL + 60) - 30;
        trait(sx, BY, sx - 18, BY + BH, 12, TEXTE, 50);
        sans_decoupe();
        contour_arrondi(BX, BY, BL, BH, 12, 1.2f, CYAN, 200);
        arc(BX + 20, BY + 21, 8, 2.5f, t * 6, t * 6 + 4.2f, JAUNE);
        texte(BX + 34, BY + 5, "ARRÊTER", POLICE_16, TEXTE);
        char n1[16], n2[16];
        fmt_entier(n1, sizeof n1, mes::collectes());
        fmt_entier(n2, sizeof n2, mes::par_point);
        snprintf(buf, sizeof buf, "%s / %s", n1, n2);
        texte(BX + 34, BY + 24, buf, POLICE_11, TEXTE2);
    } else if (mes::nb_pts >= mes::MAX_PTS) {
        rect_arrondi(BX, BY, BL, BH, 12, PANNEAU2, 230);
        texte(BX + BL / 2, BY + 6, "COMPLET", POLICE_16, TEXTE2, 255, CENTRE);
        texte(BX + BL / 2, BY + 25, "24 points maximum", POLICE_11, TEXTE3, 255, CENTRE);
    } else if (!pret) {
        rect_arrondi(BX, BY, BL, BH, 12, PANNEAU2, 230);
        contour_arrondi(BX, BY, BL, BH, 12, 1, ORANGE, (uint8_t)(80 + 80 * pulse(1)));
        texte(BX + BL / 2, BY + 6, "EN ATTENTE", POLICE_16, TEXTE2, 255, CENTRE);
        texte(BX + BL / 2, BY + 25, "Allume la carte A", POLICE_11, TEXTE3, 255, CENTRE);
    } else {
        float g = pulse(0.8f);
        contour_arrondi(BX - 3, BY - 3, BL + 6, BH + 6, 15, 2, CYAN, (uint8_t)(50 + 90 * g));
        halo(BX + BL / 2, BY + BH / 2, 80, CYAN, (uint8_t)(25 + 25 * g));
        rect_arrondi_degrade(BX, BY, BL, BH, 12, rgb(70, 225, 255), rgb(100, 90, 255));
        decoupe(BX, BY, BX + BL, BY + BH);
        float sx = BX - 40 + fmodf(t * 110, 360);
        trait(sx, BY - 2, sx - 22, BY + BH + 2, 14, TEXTE, 70);
        sans_decoupe();
        texte(BX + BL / 2, BY + 5, "MESURER", POLICE_16, NOIR_BLEU, 255, CENTRE);
        char n1[16];
        fmt_entier(n1, sizeof n1, mes::par_point);
        snprintf(buf, sizeof buf, "%s mesures · ", n1);
        char d[16];
        fmt_distance(d, sizeof d, regle_cible);
        strncat(buf, d, sizeof buf - strlen(buf) - 1);
        texte(BX + BL / 2, BY + 25, buf, POLICE_11, NOIR_BLEU, 210, CENTRE);
    }

    // ---------- plan + histogramme en direct ----------
    panneau(150, 160, 164, 42, 140);
    const Plan &p = plan();
    int suiv = plan_suivant();
    int cw = 19, cg = 1;
    int cx0 = 154 + (156 - (p.n * (cw + cg) - cg)) / 2;
    for (int i = 0; i < p.n; i++) {
        int x = cx0 + i * (cw + cg);
        bool fait = distance_faite(p.d[i]);
        bool choisi = fabsf(regle_cible - p.d[i]) < 0.05f;
        snprintf(buf, sizeof buf, "%g", p.d[i]);
        if (fait) {
            rect_arrondi(x, 164, cw, 15, 7, VERT, 200);
            texte(x + cw / 2, 165, buf, POLICE_11, NOIR_BLEU, 255, CENTRE);
        } else {
            rect_arrondi(x, 164, cw, 15, 7, PANNEAU2, 230);
            texte(x + cw / 2, 165, buf, POLICE_11, choisi ? TEXTE : TEXTE2, 255, CENTRE);
        }
        if (i == suiv && R.plan != 0) {
            contour_arrondi(x, 164, cw, 15, 7, 1.2f, JAUNE, (uint8_t)(110 + 140 * pulse(1.2f)));
        }
        if (choisi) contour_arrondi(x - 1, 163, cw + 2, 17, 8, 1.2f, CYAN, 230);
    }
    // histogramme
    if (t - histo_t > 0.2f) { mes::histogramme(histo, 40); histo_t = t; }
    if (histo.ok) {
        int hx = 158, hy = 183, hw = 100, hh = 16;
        for (int b = 0; b < histo.nb_bins; b++) {
            aff_bins[b] = approche(aff_bins[b], histo.bins[b] / (float)histo.nmax, 10);
            int h = (int)(hh * aff_bins[b] + 0.5f);
            if (h < 1) continue;
            int x = hx + b * hw / histo.nb_bins;
            rect(x, hy + hh - h, 2, h, melange(VIOLET, CYAN, (uint8_t)(255 * aff_bins[b])));
        }
        fmt(buf, sizeof buf, histo.sig, 1);
        char s2[32];
        snprintf(s2, sizeof s2, "σ %s ns", buf);
        texte(308, 185, s2, POLICE_11, TEXTE2, 255, DROITE);
    } else {
        texte(232, 186, "Histogramme en direct", POLICE_11, TEXTE3, 255, CENTRE);
    }
}

static void tap_mesure(int x, int y)
{
    // bouton
    if (dans(x, y, 6, 160, 140, 42)) {
        if (mes::en_collecte()) {
            mes::arreter();
            toast("Mesure arrêtée", ORANGE);
        } else if (mes::nb_pts >= mes::MAX_PTS) {
            toast("24 points maximum", ORANGE);
        } else if (mes::source == mes::SRC_SIM || plat_carte_a()) {
            mes::demarrer(regle_cible);
            sauver_reglages();
        } else {
            toast("Carte A introuvable", ORANGE);
        }
        return;
    }
    if (mes::en_collecte()) {
        if (y > 116) toast("Mesure en cours…", CYAN);
        return;
    }
    // pastilles du plan
    const Plan &p = plan();
    int cw = 19, cg = 1;
    int cx0 = 154 + (156 - (p.n * (cw + cg) - cg)) / 2;
    if (dans(x, y, 150, 160, 164, 24)) {
        int i = (x - cx0) / (cw + cg);
        if (i >= 0 && i < p.n) regler_distance(p.d[i]);
        return;
    }
    // fleches de la regle
    if (dans(x, y, 6, 118, 40, 38)) { regler_distance(regle_cible - 0.1f); return; }
    if (dans(x, y, 274, 118, 40, 38)) { regler_distance(regle_cible + 0.1f); return; }
}

// ============================ Ecran COURBE ==================================
static float pas_joli(float etendue, int n)
{
    float brut = etendue / n;
    float p = powf(10, floorf(log10f(brut)));
    float f = brut / p;
    return (f < 1.5f ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * p;
}

static const int GX0 = 46, GX1 = 304, GY0 = 36, GY1 = 176;

static float gx(float d) { return GX0 + (GX1 - GX0) * d / ax_max; }
static float gy(float v) { return GY1 - (GY1 - GY0) * (v - ay_min) / (ay_max - ay_min); }

static void cibles_axes(float &xm, float &ymin, float &ymax)
{
    xm = 5;
    ymin = 1e9f; ymax = -1e9f;
    for (int i = 0; i < mes::nb_pts; i++) {
        if (mes::pts[i].d > xm) xm = mes::pts[i].d;
        ymin = fminf(ymin, mes::pts[i].rtt);
        ymax = fmaxf(ymax, mes::pts[i].rtt);
    }
    xm *= 1.12f;
    if (mes::aj.ok) {
        ymin = fminf(ymin, (float)mes::aj.b);
        ymax = fmaxf(ymax, (float)(mes::aj.b + mes::aj.a * xm));
    }
    if (ymin > ymax) { ymin = 250; ymax = 300; }
    float m = (ymax - ymin) * 0.12f + 2;
    ymin -= m; ymax += m;
}

static void ecran_courbe()
{
    panneau(6, 28, 308, 174, 150);
    char buf[96], b2[48];
    if (mes::nb_pts == 0) {
        float a = 0.6f + 0.4f * pulse(0.5f);
        halo(160, 92, 40, CYAN, (uint8_t)(60 * a));
        trait(140, 104, 152, 92, 2, CYAN, (uint8_t)(200 * a));
        trait(152, 92, 160, 98, 2, CYAN, (uint8_t)(200 * a));
        trait(160, 98, 180, 78, 2, CYAN, (uint8_t)(200 * a));
        texte(160, 122, "Aucun point pour l'instant", POLICE_16, TEXTE, 255, CENTRE);
        texte(160, 144, "Onglet Mesure : règle la distance,", POLICE_11, TEXTE2, 255, CENTRE);
        texte(160, 158, "puis appuie sur MESURER.", POLICE_11, TEXTE2, 255, CENTRE);
        return;
    }
    float xm, ymin, ymax;
    cibles_axes(xm, ymin, ymax);
    if (!ax_init) { ax_max = xm; ay_min = ymin; ay_max = ymax; ax_init = true; }
    ax_max = approche(ax_max, xm, 5);
    ay_min = approche(ay_min, ymin, 5);
    ay_max = approche(ay_max, ymax, 5);

    // grille
    float sx = pas_joli(ax_max, 5), sy = pas_joli(ay_max - ay_min, 4);
    for (float v = 0; v <= ax_max; v += sx) {
        int x = (int)gx(v);
        rect(x, GY0, 1, GY1 - GY0, GRILLE, 160);
        fmt(buf, sizeof buf, v, sx < 1 ? 1 : 0);
        texte(x, GY1 + 3, buf, POLICE_11, TEXTE3, 255, CENTRE);
    }
    for (float v = ceilf(ay_min / sy) * sy; v <= ay_max; v += sy) {
        int y = (int)gy(v);
        rect(GX0, y, GX1 - GX0, 1, GRILLE, 160);
        fmt(buf, sizeof buf, v, sy < 1 ? 1 : 0);
        texte(GX0 - 4, y - 7, buf, POLICE_11, TEXTE3, 255, DROITE);
    }
    rect(GX0, GY0, 1, GY1 - GY0, TEXTE3);
    rect(GX0, GY1, GX1 - GX0, 1, TEXTE3);
    texte(12, 30, "ns", POLICE_11, TEXTE3);
    texte(GX1, GY1 + 14, "distance (m)", POLICE_11, TEXTE3, 255, DROITE);

    decoupe(GX0 + 1, GY0, GX1 + 1, GY1);
    if (mes::aj.ok) {
        if (aff_a == 0) { aff_a = (float)mes::aj.a; aff_b = (float)mes::aj.b; }
        aff_a = approche(aff_a, (float)mes::aj.a, 6);
        aff_b = approche(aff_b, (float)mes::aj.b, 6);
        // bande de confiance (95 %)
        float mx = 0;
        for (int i = 0; i < mes::nb_pts; i++) mx += mes::pts[i].d;
        mx /= mes::nb_pts;
        float my = aff_b + aff_a * mx;
        for (int x = GX0 + 1; x < GX1; x++) {
            float d = (x - GX0) * ax_max / (GX1 - GX0);
            float yc = aff_b + aff_a * d;
            float e = 2.0f * (float)mes::aj.sa * fabsf(d - mx) + 0.15f;
            int y0 = (int)gy(yc + e), y1 = (int)gy(yc - e);
            if (y1 - y0 >= 1) rect(x, y0, 1, y1 - y0, CYAN, 38);
        }
        (void)my;
        // theorie
        float ar = (float)(2e9 / mes::C_REF);
        trait_pointille(gx(0), gy(aff_b), gx(ax_max), gy(aff_b + ar * ax_max), 1, 5, 4, TEXTE2, 140);
        // droite ajustee, tracee progressivement
        float prog = sortie((t - ligne_t0) / 0.9f);
        float xe = ax_max * prog;
        trait(gx(0), gy(aff_b), gx(xe), gy(aff_b + aff_a * xe), 6, JAUNE, 45);
        trait(gx(0), gy(aff_b), gx(xe), gy(aff_b + aff_a * xe), 2, JAUNE);
        if (prog < 1) halo(gx(xe), gy(aff_b + aff_a * xe), 14, JAUNE, 200);
    }
    // points
    for (int i = 0; i < mes::nb_pts; i++) {
        const mes::Point &p = mes::pts[i];
        float x = gx(p.d), y = gy(p.rtt);
        float age = t - naissance[i];
        float s = age < 0.5f ? 1 + 0.6f * sinf(sat01(age / 0.5f) * PI_F) : 1;
        if (age < 0.8f) anneau(x, y, 4 + 22 * sortie(age / 0.8f), 1.5f, CYAN, (uint8_t)(220 * (1 - age / 0.8f)));
        float e = (gy(p.rtt - 2 * p.err) - gy(p.rtt + 2 * p.err)) / 2;
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

    // carte d'information
    if (mes::aj.ok) {
        rect_arrondi(GX0 + 6, GY0 + 4, 150, 44, 8, PANNEAU2, 215);
        contour_arrondi(GX0 + 6, GY0 + 4, 150, 44, 8, 1, BORD, 160);
        fmt(buf, sizeof buf, aff_a, 3);
        fmt(b2, sizeof b2, mes::aj.sa, 3);
        char l1[96];
        snprintf(l1, sizeof l1, "%s ± %s ns/m", buf, b2);
        texte(GX0 + 12, GY0 + 7, "pente", POLICE_11, TEXTE3);
        texte(GX0 + 58, GY0 + 7, l1, POLICE_11, JAUNE);
        fmt(buf, sizeof buf, 2e9 / mes::C_REF, 3);
        snprintf(l1, sizeof l1, "%s ns/m", buf);
        texte(GX0 + 12, GY0 + 20, "théorie", POLICE_11, TEXTE3);
        texte(GX0 + 58, GY0 + 20, l1, POLICE_11, TEXTE2);
        fmt(buf, sizeof buf, mes::aj.r2, 5);
        texte(GX0 + 12, GY0 + 33, "R²", POLICE_11, TEXTE3);
        texte(GX0 + 58, GY0 + 33, buf, POLICE_11, TEXTE2);
    } else {
        rect_arrondi(GX0 + 6, GY0 + 4, 196, 20, 8, PANNEAU2, 215);
        texte(GX0 + 14, GY0 + 7, "Encore une distance pour la droite", POLICE_11, TEXTE2);
    }

    // bulle du point choisi
    if (sel_point >= 0 && sel_point < mes::nb_pts) {
        const mes::Point &p = mes::pts[sel_point];
        float x = gx(p.d), y = gy(p.rtt);
        int bw = 150, bh = 66;
        int bx = x > 170 ? (int)x - bw - 10 : (int)x + 10;
        int by = (int)y - bh / 2;
        if (by < 32) by = 32;
        if (by + bh > 198) by = 198 - bh;
        rect_arrondi(bx, by, bw, bh, 10, NOIR_BLEU, 240);
        contour_arrondi(bx, by, bw, bh, 10, 1.2f, JAUNE, 220);
        fmt_distance(buf, sizeof buf, p.d);
        texte(bx + 8, by + 4, buf, POLICE_13, JAUNE);
        fmt(buf, sizeof buf, p.rtt, 2);
        fmt(b2, sizeof b2, p.err, 2);
        char l[96];
        snprintf(l, sizeof l, "%s ns ± %s", buf, b2);
        texte(bx + 8, by + 21, l, POLICE_11, TEXTE);
        fmt(buf, sizeof buf, p.sigma, 1);
        snprintf(l, sizeof l, "σ %s ns · n %u · rejets %u", buf, p.n, p.rejets);
        texte(bx + 8, by + 34, l, POLICE_11, TEXTE2);
        rect_arrondi(bx + 8, by + 48, 70, 14, 7, ROUGE, 220);
        texte(bx + 43, by + 48, "Supprimer", POLICE_11, TEXTE, 255, CENTRE);
    }
}

static void tap_courbe(int x, int y)
{
    if (sel_point >= 0 && sel_point < mes::nb_pts) {
        const mes::Point &p = mes::pts[sel_point];
        float px = gx(p.d), py = gy(p.rtt);
        int bw = 150, bh = 66;
        int bx = px > 170 ? (int)px - bw - 10 : (int)px + 10;
        int by = (int)py - bh / 2;
        if (by < 32) by = 32;
        if (by + bh > 198) by = 198 - bh;
        if (dans(x, y, bx + 4, by + 44, 80, 22)) {
            char buf[96], d[16];
            fmt_distance(d, sizeof d, p.d);
            snprintf(buf, sizeof buf, "Point %s supprimé", d);
            for (int k = sel_point; k < mes::nb_pts - 1; k++) naissance[k] = naissance[k + 1];
            mes::supprimer(sel_point);
            sel_point = -1;
            ligne_t0 = t;
            toast(buf, ORANGE);
            return;
        }
        if (dans(x, y, bx, by, bw, bh)) return;
    }
    int best = -1;
    float bd = 16 * 16;
    for (int i = 0; i < mes::nb_pts; i++) {
        float dx = gx(mes::pts[i].d) - x, dy = gy(mes::pts[i].rtt) - y;
        if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; }
    }
    sel_point = best;
}

// ============================ Ecran RESULTAT ================================
static void ecran_resultat()
{
    char buf[96], b2[48];
    bool ok = mes::aj.ok;
    double c6 = ok ? mes::aj.c / 1e6 : 0;
    double ecart = ok ? 100.0 * (mes::aj.c - mes::C_REF) / mes::C_REF : 0;

    // ----- carte principale -----
    panneau(6, 28, 308, 66, 160);
    texte(16, 32, "VITESSE DE LA LUMIÈRE MESURÉE", POLICE_11, TEXTE3);
    if (ok) {
        aff_c = approche(aff_c, (float)c6, 3.5f);
        fmt(buf, sizeof buf, aff_c, 1);
        int l = largeur_texte(buf, POLICE_40);
        halo(16 + l / 2, 66, 60, CYAN, (uint8_t)(50 + 25 * pulse(0.5f)));
        texte(14, 44, buf, POLICE_40, TEXTE);
        texte(22 + l, 50, "× 10⁶ m/s", POLICE_13, TEXTE2);
        fmt(b2, sizeof b2, mes::aj.sc / 1e6, 1);
        snprintf(buf, sizeof buf, "± %s", b2);
        texte(22 + l, 68, buf, POLICE_16, CYAN);
        // qualite
        const char *q;
        uint16_t qc;
        int etoiles;
        double ae = fabs(ecart);
        if (ae < 1)       { q = "EXCELLENT"; qc = VERT; etoiles = 3; }
        else if (ae < 5)  { q = "BON"; qc = CYAN; etoiles = 2; }
        else if (ae < 15) { q = "MOYEN"; qc = ORANGE; etoiles = 1; }
        else              { q = "À AMÉLIORER"; qc = ROUGE; etoiles = 0; }
        int ql = largeur_texte(q, POLICE_11) + 16;
        rect_arrondi(306 - ql, 32, ql, 16, 8, qc, 210);
        texte(306 - ql / 2, 33, q, POLICE_11, NOIR_BLEU, 255, CENTRE);
        for (int k = 0; k < 3; k++) {
            float x = 306 - 8 - k * 13, y = 60;
            if (k < etoiles) { halo(x, y, 8, qc, 90); disque(x, y, 4, qc); }
            else anneau(x, y, 3.5f, 1.2f, TEXTE3);
        }
        snprintf(buf, sizeof buf, "%d points", mes::nb_pts);
        texte(306, 72, buf, POLICE_11, TEXTE3, 255, DROITE);
    } else {
        texte(14, 44, "—", POLICE_40, TEXTE3);
        texte(70, 54, "Mesure au moins 2 distances", POLICE_13, TEXTE2);
        texte(70, 72, "(3 ou plus pour l'incertitude)", POLICE_11, TEXTE3);
    }

    // ----- jauge d'ecart -----
    panneau(6, 98, 150, 104, 150);
    texte(14, 101, "Écart à la vraie valeur", POLICE_11, TEXTE3);
    const float JX = 81, JY = 168, JR = 48;
    struct Seg { float a, b; uint16_t c; };
    const Seg segs[5] = { { -10, -5, ROUGE }, { -5, -1, ORANGE }, { -1, 1, VERT }, { 1, 5, ORANGE }, { 5, 10, ROUGE } };
    arc(JX, JY, JR, 12, PI_F, 2 * PI_F, PANNEAU2);
    for (int k = 0; k < 5; k++) {
        float a0 = PI_F + (segs[k].a + 10) / 20 * PI_F + 0.02f;
        float a1 = PI_F + (segs[k].b + 10) / 20 * PI_F - 0.02f;
        arc(JX, JY, JR, 9, a0, a1, segs[k].c, 210);
    }
    texte(14, 172, "-10 %", POLICE_11, TEXTE3);
    texte(148, 172, "+10 %", POLICE_11, TEXTE3, 255, DROITE);
    float e = ok ? (float)ecart : 0;
    if (e < -10.5f) e = -10.5f;
    if (e > 10.5f) e = 10.5f;
    float cible = PI_F + (e + 10) / 20 * PI_F;
    // aiguille a ressort
    float acc = 60 * (cible - aig_ang) - 7 * aig_v;
    aig_v += acc * dt;
    aig_ang += aig_v * dt;
    float tx = JX + cosf(aig_ang) * (JR - 12), ty = JY + sinf(aig_ang) * (JR - 12);
    halo(tx, ty, 12, ok ? JAUNE : TEXTE3, 120);
    trait(JX, JY, tx, ty, 3, ok ? TEXTE : TEXTE3);
    disque(JX, JY, 5, ok ? JAUNE : TEXTE3);
    if (ok) {
        fmt(b2, sizeof b2, ecart, 2);
        snprintf(buf, sizeof buf, "%s%s %%", ecart >= 0 ? "+" : "", b2);
        texte((int)JX, 180, buf, POLICE_13, fabs(ecart) < 1 ? VERT : (fabs(ecart) < 5 ? CYAN : ORANGE), 255, CENTRE);
    }

    // ----- convergence -----
    panneau(160, 98, 154, 104, 150);
    texte(168, 101, "Convergence de c", POLICE_11, TEXTE3);
    const int CX0 = 188, CX1 = 306, CY0 = 120, CY1 = 186;
    float ref = (float)(mes::C_REF / 1e6);
    float amp = 3;
    for (int i = 0; i < mes::nb_conv; i++)
        if (mes::conv_c[i] > 0) amp = fmaxf(amp, fabsf(mes::conv_c[i] - ref) * 1.25f);
    if (amp > 120) amp = 120;
    auto cy = [&](float v) { return CY1 - (CY1 - CY0) * (v - (ref - amp)) / (2 * amp); };
    float b1 = cy(ref * 1.01f), b0 = cy(ref * 0.99f);
    rect(CX0, (int)b1, CX1 - CX0, (int)(b0 - b1) + 1, VERT, 30);
    trait_pointille(CX0, cy(ref), CX1, cy(ref), 1, 4, 3, VERT, 170);
    fmt(buf, sizeof buf, ref, 1);
    texte(CX0 - 3, (int)cy(ref) - 7, buf, POLICE_11, VERT, 220, DROITE);
    fmt(buf, sizeof buf, ref + amp, 0);
    texte(CX0 - 3, CY0 - 6, buf, POLICE_11, TEXTE3, 255, DROITE);
    fmt(buf, sizeof buf, ref - amp, 0);
    texte(CX0 - 3, CY1 - 8, buf, POLICE_11, TEXTE3, 255, DROITE);
    rect(CX0, CY1, CX1 - CX0, 1, TEXTE3);
    if (mes::nb_conv >= 1) {
        int n = mes::nb_conv;
        float pxp = 0, pyp = 0;
        decoupe(CX0, CY0 - 4, CX1 + 4, CY1 + 1);
        for (int i = 0; i < n; i++) {
            float x = n == 1 ? (CX0 + CX1) / 2.0f : CX0 + 4 + (CX1 - CX0 - 8) * i / (float)(n - 1);
            float v = mes::conv_c[i];
            float y = cy(v);
            if (i > 0) trait(pxp, pyp, x, y, 1.8f, CYAN);
            pxp = x; pyp = y;
        }
        for (int i = 0; i < n; i++) {
            float x = n == 1 ? (CX0 + CX1) / 2.0f : CX0 + 4 + (CX1 - CX0 - 8) * i / (float)(n - 1);
            float y = cy(mes::conv_c[i]);
            bool dernier = i == n - 1;
            if (dernier) halo(x, y, 10 + 3 * pulse(1), JAUNE, 150);
            disque(x, y, dernier ? 3.5f : 2.2f, dernier ? JAUNE : CYAN);
        }
        sans_decoupe();
        snprintf(buf, sizeof buf, "2 → %d points", n + 1);
        texte(CX1, CY1 + 3, buf, POLICE_11, TEXTE3, 255, DROITE);
    } else {
        texte((CX0 + CX1) / 2, 150, "en attente", POLICE_11, TEXTE3, 255, CENTRE);
    }
}

// ============================ Ecran STATS ===================================
static const int CARTE_X[4] = { 6, 94, 180, 246 };
static const int CARTE_L[4] = { 84, 82, 62, 68 };

static void carte_stat(int i, const char *titre, uint32_t val, const char *sous, uint16_t c)
{
    int x = CARTE_X[i], y = 28, l = CARTE_L[i], h = 50;
    panneau(x, y, l, h, 150);
    texte(x + 7, y + 4, titre, POLICE_11, TEXTE3);
    aff_cpt[i] = approche(aff_cpt[i], (float)val, 6);
    if (fabsf(aff_cpt[i] - val) < 1) aff_cpt[i] = (float)val;
    char buf[24];
    fmt_entier(buf, sizeof buf, (uint32_t)(aff_cpt[i] + 0.5f));
    halo(x + 20, y + 28, 22, c, 40);
    texte(x + 7, y + 17, buf, POLICE_16, c);
    if (sous) texte(x + 7, y + 35, sous, POLICE_11, TEXTE3);
}

static void ecran_stats()
{
    char s1[32], s2[32], s3[32];
    snprintf(s1, sizeof s1, "tris %lu", (unsigned long)mes::nb_tris);
    snprintf(s2, sizeof s2, "régress. %lu", (unsigned long)mes::nb_regressions);
    float pct = mes::nb_echant_total ? 100.0f * mes::nb_rejets_total / mes::nb_echant_total : 0;
    fmt(s3, sizeof s3, pct, 1);
    strcat(s3, " %");
    carte_stat(0, "Échantillons", mes::nb_echant_total, s1, CYAN);
    carte_stat(1, "Sessions FTM", mes::nb_sessions, s2, VIOLET);
    carte_stat(2, "Rejetés", mes::nb_rejets_total, s3, ORANGE);
    // debit + petite courbe (historique tenu dans image())
    carte_stat(3, "Débit /s", (uint32_t)(mes::debit() + 0.5f), nullptr, VERT);
    float dmax = 20;
    for (int i = 0; i < debit_n; i++) dmax = fmaxf(dmax, debit_hist[i]);
    for (int i = 1; i < debit_n; i++) {
        float x0 = CARTE_X[3] + 7 + (i - 1) * 1.4f, x1 = x0 + 1.4f;
        trait(x0, 74 - 9 * debit_hist[i - 1] / dmax, x1, 74 - 9 * debit_hist[i] / dmax, 1.2f, VERT, 200);
    }

    // histogramme
    panneau(6, 82, 308, 120, 150);
    if (t - histo_t > 0.2f) { mes::histogramme(histo, 44); histo_t = t; }
    char buf[96], b2[24], b3[24];
    char d[16];
    fmt_distance(d, sizeof d, mes::collecte_d());
    snprintf(buf, sizeof buf, "Temps de vol · point %s%s", d, mes::en_collecte() ? " (en cours)" : "");
    texte(14, 86, buf, POLICE_11, TEXTE2);
    const int HX0 = 16, HX1 = 304, HY0 = 102, HY1 = 172;
    if (!histo.ok) {
        texte(160, 130, "Lance une mesure pour voir la distribution", POLICE_11, TEXTE3, 255, CENTRE);
        return;
    }
    int nb = histo.nb_bins;
    float bw = (HX1 - HX0) / (float)nb;
    for (int b = 0; b < nb; b++) {
        aff_bins[b] = approche(aff_bins[b], histo.bins[b] / (float)histo.nmax, 8);
        float h = (HY1 - HY0) * aff_bins[b];
        if (h < 0.5f) continue;
        int x = (int)(HX0 + b * bw);
        uint16_t c = melange(VIOLET, CYAN, (uint8_t)(255 * aff_bins[b]));
        rect_arrondi_degrade(x + 1, (int)(HY1 - h), (int)bw - 1, (int)h + 1, 2, c, melange(c, PANNEAU, 140));
    }
    // courbe de Gauss
    if (histo.sig > 0.05f) {
        float px = 0, py = 0;
        float largeur_bin = (histo.hi - histo.lo) / nb;
        float n_tot = 0;
        for (int b = 0; b < nb; b++) n_tot += histo.bins[b];
        for (int x = HX0; x <= HX1; x += 3) {
            float v = histo.lo + (x - HX0) * (histo.hi - histo.lo) / (HX1 - HX0);
            float z = (v - histo.moy) / histo.sig;
            float pdf = expf(-0.5f * z * z) / (histo.sig * 2.5066f);
            float y = HY1 - (HY1 - HY0) * (pdf * n_tot * largeur_bin) / histo.nmax;
            if (x > HX0) trait(px, py, (float)x, y, 1.6f, JAUNE, 230);
            px = (float)x; py = y;
        }
    }
    float xm = HX0 + (histo.med - histo.lo) / (histo.hi - histo.lo) * (HX1 - HX0);
    trait_pointille(xm, HY0 - 2, xm, HY1, 1, 3, 3, TEXTE, 160);
    rect(HX0, HY1, HX1 - HX0, 1, TEXTE3);
    fmt(buf, sizeof buf, histo.lo, 1);
    texte(HX0, HY1 + 2, buf, POLICE_11, TEXTE3);
    fmt(buf, sizeof buf, histo.hi, 1);
    texte(HX1, HY1 + 2, buf, POLICE_11, TEXTE3, 255, DROITE);
    fmt(b2, sizeof b2, histo.med, 2);
    snprintf(buf, sizeof buf, "médiane %s ns", b2);
    texte((int)xm, HY1 + 2, buf, POLICE_11, TEXTE2, 255, CENTRE);
    fmt(b2, sizeof b2, histo.moy, 2);
    fmt(b3, sizeof b3, histo.sig, 2);
    snprintf(buf, sizeof buf, "μ %s ns   σ %s ns   n %d", b2, b3, histo.nb);
    texte(160, 187, buf, POLICE_11, TEXTE, 255, CENTRE);
}

// ============================ Ecran REGLAGES ================================
static const char *const LAB_SRC[2] = { "Carte A", "Simulation" };
static const char *const LAB_PLAN[4] = { "Libre", "10 m", "20 m", "50 m" };
static const char *const LAB_PP[3] = { "200", "600", "1 500" };

static void ecran_reglages()
{
    panneau(6, 28, 308, 174, 150);
    char buf[96];
    // source
    texte(16, 34, "Source des mesures", POLICE_13, TEXTE);
    texte(16, 50, R.source ? "Données simulées réalistes" : "Wi-Fi FTM avec la carte A", POLICE_11, TEXTE3);
    segments(0, 168, 36, 138, 24, LAB_SRC, 2, R.source);
    rect(14, 67, 292, 1, GRILLE, 160);
    // plan
    texte(16, 72, "Plan de mesure", POLICE_13, TEXTE);
    texte(16, 88, plan().desc, POLICE_11, TEXTE3);
    segments(1, 168, 74, 138, 24, LAB_PLAN, 4, R.plan);
    rect(14, 105, 292, 1, GRILLE, 160);
    // echantillons par point
    texte(16, 110, "Mesures par point", POLICE_13, TEXTE);
    float deb = mes::debit() > 10 ? mes::debit() : 115;
    fmt(buf, sizeof buf, PAR_POINT[R.ppi] / deb, 0);
    char l[96];
    snprintf(l, sizeof l, "≈ %s s par distance", buf);
    texte(16, 126, l, POLICE_11, TEXTE3);
    segments(2, 168, 112, 138, 24, LAB_PP, 3, R.ppi);
    rect(14, 143, 292, 1, GRILLE, 160);
    // luminosite
    texte(16, 150, "Luminosité", POLICE_13, TEXTE);
    snprintf(buf, sizeof buf, "%d %%", (int)R.lum);
    texte(130, 152, buf, POLICE_11, TEXTE2, 255, DROITE);
    int sx0 = 150, sx1 = 300;
    float kx = sx0 + (sx1 - sx0) * (R.lum - 5) / 95.0f;
    rect_arrondi(sx0, 155, sx1 - sx0, 6, 3, PANNEAU2);
    rect_arrondi_degrade(sx0, 155, (int)(kx - sx0) + 3, 6, 3, BLEU, CYAN);
    halo(kx, 158, 14, CYAN, tc_cible == C_LUM ? 160 : 80);
    disque(kx, 158, 7, TEXTE);
    disque(kx, 158, 3, CYAN);
    rect(14, 170, 292, 1, GRILLE, 160);
    // effacer + infos
    if (conf_effacer && t - conf_t0 > 3) conf_effacer = false;
    if (conf_effacer) {
        rect_arrondi(14, 175, 136, 22, 11, ROUGE, 230);
        texte(82, 179, "Confirmer ?", POLICE_13, TEXTE, 255, CENTRE);
    } else {
        rect_arrondi(14, 175, 136, 22, 11, PANNEAU2, 230);
        contour_arrondi(14, 175, 136, 22, 11, 1, ROUGE, 180);
        if (mes::nb_pts) snprintf(buf, sizeof buf, "Effacer %d point%s", mes::nb_pts, mes::nb_pts > 1 ? "s" : "");
        else snprintf(buf, sizeof buf, "Aucun point");
        texte(82, 180, buf, POLICE_11, mes::nb_pts ? ROUGE : TEXTE3, 255, CENTRE);
    }
    snprintf(buf, sizeof buf, "SD : %s", plat_sd() ? "lumiere.csv ✓" : "absente");
    texte(306, 174, buf, POLICE_11, plat_sd() ? VERT : TEXTE3, 255, DROITE);
    snprintf(buf, sizeof buf, "%s · %d i/s · %lu Ko libres", LUMIERE_VERSION, (int)(ips + 0.5f),
             (unsigned long)(plat_memoire_libre() / 1024));
    texte(306, 187, buf, POLICE_11, TEXTE3, 255, DROITE);
}

static void changer_source(int s)
{
    if (s == R.source) return;
    if (mes::en_collecte()) mes::arreter();
    R.source = s;
    mes::source = s;
    plat_source(s);
    sauver_reglages();
    toast(s ? "Mode simulation" : "Mode carte A réelle", s ? VIOLET : VERT);
}

static void tap_reglages(int x, int y)
{
    if (dans(x, y, 168, 34, 138, 28)) { changer_source((x - 168) * 2 / 138); return; }
    if (dans(x, y, 168, 72, 138, 28)) {
        int p = (x - 168) * 4 / 138;
        if (p != R.plan) {
            R.plan = p;
            int s = plan_suivant();
            if (s >= 0 && p != 0) regler_distance(PLANS[p].d[s]);
            sauver_reglages();
        }
        return;
    }
    if (dans(x, y, 168, 110, 138, 28)) {
        int p = (x - 168) * 3 / 138;
        if (p != R.ppi && !mes::en_collecte()) {
            R.ppi = p;
            mes::par_point = PAR_POINT[p];
            sauver_reglages();
        } else if (mes::en_collecte()) {
            toast("Mesure en cours…", CYAN);
        }
        return;
    }
    if (dans(x, y, 14, 173, 136, 26)) {
        if (!conf_effacer) {
            if (mes::nb_pts > 0) { conf_effacer = true; conf_t0 = t; }
        } else {
            conf_effacer = false;
            mes::effacer();
            sel_point = -1;
            ax_init = false;
            aff_a = aff_b = 0;
            toast("Tous les points effacés", ORANGE);
        }
        return;
    }
}

static void lum_toucher(int x)
{
    int v = 5 + (int)((x - 150) * 95.0f / 150.0f + 0.5f);
    if (v < 5) v = 5;
    if (v > 100) v = 100;
    if (v != R.lum) { R.lum = v; plat_luminosite(v); }
}

// ============================ Revelation ====================================
static void lancer_revelation()
{
    revel = true;
    revel_t0 = t;
    revel_c = (float)(mes::aj.c / 1e6);
    revel_ecart = (float)(100.0 * (mes::aj.c - mes::C_REF) / mes::C_REF);
    const uint16_t cols[4] = { CYAN, JAUNE, MAGENTA, VERT };
    for (int i = 0; i < 90; i++) {
        float a = alea() * 2 * PI_F, v = 40 + alea() * 160;
        part[i] = { 160, 100, cosf(a) * v, sinf(a) * v - 30, 1.2f + alea() * 1.3f, cols[i % 4] };
    }
}

static void dessiner_revelation()
{
    float age = t - revel_t0;
    if (age > 4.5f) { revel = false; return; }
    float f = age < 0.3f ? age / 0.3f : (age > 4.0f ? 1 - (age - 4.0f) / 0.5f : 1);
    f = sat01(f);
    rect(0, 0, L, H, NOIR_BLEU, (uint8_t)(200 * f));
    // onde de choc
    if (age < 1.2f) {
        anneau(160, 100, 10 + 200 * sortie(age / 1.2f), 3, CYAN, (uint8_t)(200 * (1 - age / 1.2f)));
        anneau(160, 100, 6 + 140 * sortie(age / 1.2f), 2, JAUNE, (uint8_t)(160 * (1 - age / 1.2f)));
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
    texte(160, 46, "c mesurée", POLICE_16, TEXTE2, a, CENTRE);
    float v = revel_c * sortie(age / 1.4f);
    char buf[96], b2[48];
    fmt(buf, sizeof buf, v, 1);
    halo(160, 100, 90, CYAN, (uint8_t)(70 * f));
    texte(160, 72, buf, POLICE_40, TEXTE, a, CENTRE);
    texte(160, 122, "× 10⁶ m/s", POLICE_16, CYAN, a, CENTRE);
    if (age > 1.3f) {
        fmt(b2, sizeof b2, revel_ecart, 2);
        snprintf(buf, sizeof buf, "écart %s%s %% · vraie valeur 299,79", revel_ecart >= 0 ? "+" : "", b2);
        uint8_t a2 = (uint8_t)(a * sat01((age - 1.3f) / 0.4f));
        texte(160, 148, buf, POLICE_13, fabsf(revel_ecart) < 1 ? VERT : JAUNE, a2, CENTRE);
    }
    if (age > 2) texte(160, 178, "Touchez pour continuer", POLICE_11, TEXTE3, (uint8_t)(a * (0.4f + 0.6f * pulse(0.8f))), CENTRE);
}

// ============================ Ecran de demarrage ============================
static void dessiner_splash()
{
    float age = t - splash_t0;
    fond();
    float bx = -40 + 400 * sortie(age / 1.2f);
    degrade_h(0, 118, (int)bx, 2, BLEU, CYAN, 220);
    halo(bx, 119, 40, CYAN, 200);
    halo(bx, 119, 14, TEXTE, 220);
    float a = sat01((age - 0.5f) / 0.6f);
    halo(160, 92, 80, VIOLET, (uint8_t)(70 * a));
    texte(160, 74, "LUMIÈRE", POLICE_30, TEXTE, (uint8_t)(255 * a), CENTRE);
    texte(160, 132, "Mesurer la vitesse de la lumière", POLICE_13, TEXTE2, (uint8_t)(255 * sat01((age - 0.8f) / 0.5f)), CENTRE);
    texte(160, 150, "avec deux ESP32 et le Wi-Fi FTM", POLICE_11, TEXTE3, (uint8_t)(255 * sat01((age - 1.0f) / 0.5f)), CENTRE);
    texte(160, 220, LUMIERE_VERSION " · ES3C28P", POLICE_11, TEXTE3, (uint8_t)(200 * a), CENTRE);
    if (age > 2.6f) splash = false;
}

// ============================ Toucher =======================================
static void tap(int x, int y)
{
    if (revel) { revel = false; return; }
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
    case O_MESURE: tap_mesure(x, y); break;
    case O_COURBE: tap_courbe(x, y); break;
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
    if (appui && !tc_appui) {                      // pose du doigt
        tc_x0 = tc_x = tc_xp = x;
        tc_y0 = tc_y = y;
        tc_t0 = t;
        tc_bouge = false;
        tc_cible = C_RIEN;
        if (!revel && onglet == O_MESURE && dans(x, y, 46, 118, 228, 38) && !mes::en_collecte()) {
            tc_cible = C_REGLE;
            regle_v = 0;
        }
        if (!revel && onglet == O_REGLAGES && dans(x, y, 140, 146, 172, 24)) {
            tc_cible = C_LUM;
            lum_toucher(x);
        }
    } else if (appui && tc_appui) {                // glissement
        if (abs(x - tc_x0) > 6 || abs(y - tc_y0) > 6) tc_bouge = true;
        if (tc_cible == C_REGLE) {
            float dm = -(x - tc_xp) / PX_M;
            regle_d += dm;
            if (regle_d < 0.1f) regle_d = 0.1f;
            if (regle_d > 200) regle_d = 200;
            regle_cible = roundf(regle_d * 10) / 10.0f;
            float vinst = dt > 0 ? dm / dt : 0;
            regle_v = regle_v * 0.6f + vinst * 0.4f;
        } else if (tc_cible == C_LUM) {
            lum_toucher(x);
        }
        tc_xp = x;
        tc_x = x;
        tc_y = y;
    } else if (!appui && tc_appui) {               // doigt leve
        if (tc_cible == C_REGLE) {
            if (!tc_bouge) { regle_v = 0; tap(tc_x0, tc_y0); }
            else if (fabsf(regle_v) < 0.3f) regle_v = 0;
            regle_lachee = true;
            sauver_reglages();
        } else if (tc_cible == C_LUM) {
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
    Reglages lu;
    if (plat_nvs_lire("reglages", &lu, sizeof lu) && lu.magie == R.magie) {
        if (lu.source >= 0 && lu.source <= 1) R.source = lu.source;
        if (lu.plan >= 0 && lu.plan <= 3) R.plan = lu.plan;
        if (lu.ppi >= 0 && lu.ppi <= 2) R.ppi = lu.ppi;
        if (lu.lum >= 5 && lu.lum <= 100) R.lum = lu.lum;
        if (lu.distance >= 0.1f && lu.distance <= 200) R.distance = lu.distance;
    }
    mes::source = R.source;
    mes::par_point = PAR_POINT[R.ppi];
    plat_source(R.source);
    plat_luminosite(R.lum);
    mes::charger();
    for (int i = 0; i < mes::MAX_PTS; i++) naissance[i] = -10;
    regle_cible = regle_d = R.distance;
    int s = plan_suivant();
    if (s >= 0 && R.plan != 0) regle_cible = regle_d = plan().d[s];
    xb_aff = x_carte_b(regle_cible);
    onglet_aff = (float)onglet;
}

float distance_reglee() { return regle_cible; }

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
    if (t - debit_t > 0.25f) {                       // historique du debit (onglet Stats)
        debit_t = t;
        if (debit_n < 40) debit_hist[debit_n++] = mes::debit();
        else { memmove(debit_hist, debit_hist + 1, 39 * sizeof(float)); debit_hist[39] = mes::debit(); }
    }

    int ev = mes::tick(ms, regle_cible);
    if (ev == mes::EV_POINT) {
        naissance[mes::nb_pts - 1] = t;
        ligne_t0 = t;
        char buf[96], d[16];
        fmt_distance(d, sizeof d, mes::pts[mes::nb_pts - 1].d);
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
        if (mes::part_tronquee > 0.05f) {
            snprintf(buf, sizeof buf, "Trop près : %d %% des mesures à 0, recule (> 2 m)",
                     (int)(mes::part_tronquee * 100 + 0.5f));
            toast(buf, ORANGE);
        } else {
            toast(buf, VERT);
        }
        if (mes::aj.ok && mes::nb_pts >= 2) lancer_revelation();
    } else if (ev == mes::EV_ECHEC) {
        toast("Mesure invalide (pas assez de données)", ROUGE);
    }

    if (splash) { dessiner_splash(); return; }

    fond();
    switch (onglet) {
    case O_MESURE: ecran_mesure(); break;
    case O_COURBE: ecran_courbe(); break;
    case O_RESULTAT: ecran_resultat(); break;
    case O_STATS: ecran_stats(); break;
    case O_REGLAGES: ecran_reglages(); break;
    }
    // fondu d'entree a chaque changement d'onglet
    float ft = (t - onglet_t0) / 0.25f;
    if (onglet_t0 >= 0 && ft < 1) rect(0, 26, L, 178, FOND_HAUT, (uint8_t)(230 * (1 - sortie(ft))));
    barre_haut();
    barre_onglets();
    dessiner_toast();
    if (revel) dessiner_revelation();
}

}  // namespace ui
