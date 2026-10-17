// sim_tuto.cpp - "tournage" des ecrans pour le tutoriel et la video.
//
// Meme code de dessin et de calcul que la carte ES3C28P. Ici la source est
// "Carte A" : le simulateur fabrique des echantillons FTM realistes (paliers
// de 1,5625 ns, bruit, valeurs bloquees a 0, aberrations), comme ceux
// releves sur deux vrais ESP32-S3, et les fait passer par le meme chemin
// que la tache Wi-Fi (mes::ajouter / mes::session).
//
//   g++ -std=c++17 -O2 -I../../lumiere_console sim_tuto.cpp
//       ../../lumiere_console/{gfx,mesure,interface,polices}.cpp -o sim_tuto
//   ./sim_tuto <dossier>
//
// Sorties : <nom>.ppm (captures) et <segment>.rgb (images RGB888 320x240
// a la suite, 30 i/s) pour les animations.
#include "gfx.h"
#include "interface.h"
#include "mesure.h"
#include "plateforme.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static uint32_t g_ms = 0;
static std::map<std::string, std::vector<uint8_t>> g_nvs;
static bool g_carte_a = false;
static bool g_injecter = false;

uint32_t plat_ms() { return g_ms; }
void plat_verrou() {}
void plat_deverrou() {}
void plat_luminosite(int) {}
void plat_source(int) {}
bool plat_carte_a() { return g_carte_a; }
bool plat_sd() { return true; }
void plat_csv(float, const uint32_t *, const int8_t *, int) {}
bool plat_nvs_lire(const char *k, void *b, size_t n)
{
    auto it = g_nvs.find(k);
    if (it == g_nvs.end() || it->second.size() != n) return false;
    memcpy(b, it->second.data(), n);
    return true;
}
void plat_nvs_ecrire(const char *k, const void *b, size_t n)
{
    g_nvs[k] = std::vector<uint8_t>((const uint8_t *)b, (const uint8_t *)b + n);
}
uint32_t plat_memoire_libre() { return 186 * 1024; }

static uint16_t fb[320 * 240], fond[320 * 240];
static std::string g_dossier = ".";
static FILE *g_seg = nullptr;
static FILE *g_appuis = nullptr;          // positions du doigt (pour la video)
static int g_seg_pas = 1, g_seg_compteur = 0;

// ---- echantillons FTM realistes (meme modele que la simulation interne) ----
static std::mt19937 g_alea(7);
static float g_reste = 0;
static int g_compteur = 0;

static void injecter()
{
    if (!g_injecter || mes::source != mes::SRC_REEL) return;
    std::normal_distribution<double> n01(0, 1);
    std::uniform_real_distribution<double> u01(0, 1);
    float d = mes::en_collecte() ? mes::collecte_d() : ui::distance_reglee();
    g_reste += 0.033f * 115;
    int n = (int)g_reste;
    g_reste -= n;
    for (int i = 0; i < n; i++) {
        double ns = 2.0 * d / mes::C_REF * 1e9 + 1.2 + 0.55 * sin(d * 1.3 + 0.4) + 4.5 * n01(g_alea);
        if (u01(g_alea) < 0.03) ns += 15 + 60 * u01(g_alea);
        ns = std::floor(ns / 1.5625 + 0.5) * 1.5625;
        if (ns <= 0) ns = 0.001;
        int rssi = (int)(-38 - 20 * log10(d > 0.2 ? d : 0.2) + 2 * n01(g_alea));
        mes::ajouter((uint32_t)(ns * 1000), (int8_t)rssi);
        if (++g_compteur >= 16) { g_compteur = 0; mes::session(); }
    }
}

static void rgb888(uint8_t *out)
{
    for (int i = 0; i < 320 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
        out[3 * i] = r | (r >> 5);
        out[3 * i + 1] = g | (g >> 6);
        out[3 * i + 2] = b | (b >> 5);
    }
}

static void capture(const char *nom)
{
    static uint8_t buf[320 * 240 * 3];
    rgb888(buf);
    std::string c = g_dossier + "/" + nom + ".ppm";
    FILE *f = fopen(c.c_str(), "wb");
    fprintf(f, "P6\n320 240\n255\n");
    fwrite(buf, 1, sizeof buf, f);
    fclose(f);
    printf("capture %-26s t=%6.2f s  points=%d\n", nom, g_ms / 1000.0, mes::nb_pts);
}

static void seg_debut(const char *nom, int pas = 1)
{
    std::string c = g_dossier + "/" + nom + ".rgb";
    g_seg = fopen(c.c_str(), "wb");
    g_appuis = fopen((g_dossier + "/" + nom + ".appuis").c_str(), "w");
    g_seg_pas = pas;
    g_seg_compteur = 0;
}

static void seg_fin()
{
    if (g_seg) fclose(g_seg);
    if (g_appuis) fclose(g_appuis);
    g_seg = nullptr;
    g_appuis = nullptr;
}

static void image(bool appui = false, int x = 0, int y = 0)
{
    g_ms += 33;
    injecter();
    ui::toucher(appui, x, y);
    ui::image(g_ms);
    if (g_seg && appui && g_appuis && g_seg_compteur % g_seg_pas == 0)
        fprintf(g_appuis, "%d %d %d\n", g_seg_compteur / g_seg_pas, x, y);
    if (g_seg && (g_seg_compteur++ % g_seg_pas == 0)) {
        static uint8_t buf[320 * 240 * 3];
        rgb888(buf);
        fwrite(buf, 1, sizeof buf, g_seg);
    }
}

static void attendre(float s) { for (int i = 0; i < (int)(s * 30); i++) image(); }

static void taper(int x, int y)
{
    for (int i = 0; i < 3; i++) image(true, x, y);
    image(false, x, y);
}

static void glisser(int x0, int y0, int x1, int y1, int n)
{
    for (int i = 0; i <= n; i++) image(true, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
    for (int i = 0; i < 4; i++) image(true, x1, y1);
    image(false, x1, y1);
}

// onglets
static void onglet(int i) { taper(32 + 64 * i, 222); }
static const int MESURE = 0, COURBE = 1, RESULTAT = 2, STATS = 3, REGLAGES = 4;
static void mesurer() { taper(76, 181); }

// ecran d'etalonnage (dessine par lumiere_console.ino : meme code ici)
static void etalonnage()
{
    gfx::degrade_v(0, 0, 320, 240, gfx::rgb(4, 6, 18), gfx::rgb(16, 12, 42));
    gfx::texte(160, 96, "Étalonnage de l'écran", POLICE_16, gfx::rgb(236, 242, 255), 255, gfx::CENTRE);
    gfx::texte(160, 120, "Touche le centre de la croix", POLICE_13, gfx::rgb(156, 168, 210), 255, gfx::CENTRE);
    gfx::halo(30, 30, 18, gfx::rgb(255, 212, 64), 160);
    gfx::trait(18, 30, 42, 30, 2, gfx::rgb(255, 212, 64));
    gfx::trait(30, 18, 30, 42, 2, gfx::rgb(255, 212, 64));
}

int main(int argc, char **argv)
{
    if (argc > 1) g_dossier = argv[1];
    gfx::init(fb);
    etalonnage();
    capture("c00_etalonnage");
    ui::init(fond);

    // 1. demarrage, recherche puis connexion de la carte A
    seg_debut("s01_demarrage");
    attendre(1.3f);
    capture("c01_demarrage");
    attendre(2.2f);
    attendre(1.5f);
    capture("c02_attente_carte_a");
    attendre(1.0f);
    g_carte_a = true;
    g_injecter = true;
    attendre(2.5f);
    capture("c03_pret");
    attendre(0.5f);
    seg_fin();

    // 2. reglages
    seg_debut("s02_reglages");
    onglet(REGLAGES);
    attendre(1.5f);
    capture("c04_reglages");
    taper(186, 86);  attendre(1.2f);            // plan Libre
    capture("c05_reglages_plan_libre");
    taper(255, 86);  attendre(1.0f);            // plan 20 m
    taper(283, 124); attendre(1.0f);            // 1 500 mesures
    taper(237, 124); attendre(0.8f);            // 600 mesures
    glisser(270, 158, 205, 158, 25); attendre(0.6f);   // luminosite
    capture("c06_reglages_luminosite");
    glisser(205, 158, 270, 158, 20); attendre(0.6f);
    taper(270, 48);  attendre(1.0f);            // Simulation
    capture("c07_mode_simulation");
    attendre(0.8f);
    taper(200, 48);  attendre(1.4f);            // Carte A
    onglet(MESURE);
    attendre(1.0f);
    seg_fin();

    // 3. regle des distances : on va (volontairement) trop pres, 0,5 m
    seg_debut("s03_regle");
    for (int k = 0; k < 6; k++) glisser(130, 137, 142, 137, 12);   // glissement lent
    attendre(0.8f);
    capture("c08_regle");
    attendre(0.6f);
    seg_fin();

    // 4. piege : trop pres
    seg_debut("s04_trop_pres");
    mesurer();
    attendre(2.0f);
    capture("c09_collecte_trop_pres");
    attendre(3.6f);
    capture("c10_avertissement_trop_pres");
    attendre(1.2f);
    onglet(COURBE);
    attendre(1.2f);
    capture("c11_courbe_un_point");
    taper(69, 106);
    attendre(0.8f);
    capture("c12_point_a_supprimer");
    taper(122, 128);
    attendre(1.2f);
    capture("c13_point_supprime");
    onglet(MESURE);
    attendre(0.6f);
    taper(162, 171);                            // pastille 2 m
    attendre(1.5f);
    seg_fin();

    // 5. premiere vraie mesure
    seg_debut("s05_premiere_mesure");
    mesurer();
    attendre(2.5f);
    capture("c14_collecte");
    attendre(3.0f);
    capture("c15_premier_point");
    attendre(1.5f);
    seg_fin();

    // 6. deuxieme mesure : premiere valeur de c
    seg_debut("s06_revelation");
    mesurer();
    attendre(5.6f);
    attendre(1.6f);
    capture("c16_revelation");
    attendre(2.2f);
    taper(160, 120);
    attendre(0.8f);
    seg_fin();

    // 7. reste du plan en accelere (x4)
    seg_debut("s07_plan_accelere", 4);
    for (int k = 0; k < 6; k++) {
        mesurer();
        attendre(5.6f);
        attendre(3.0f);
        taper(160, 120);
        attendre(0.5f);
    }
    capture("c17_plan_termine");
    attendre(1.0f);
    seg_fin();

    // 8. courbe
    seg_debut("s08_courbe");
    onglet(COURBE);
    attendre(2.5f);
    capture("c18_courbe");
    taper(138, 122);
    attendre(1.5f);
    capture("c19_courbe_detail_point");
    taper(250, 60);
    attendre(0.8f);
    seg_fin();

    // 9. resultat
    seg_debut("s09_resultat");
    onglet(RESULTAT);
    attendre(4.0f);
    capture("c20_resultat");
    seg_fin();

    // 10. stats, puis une mesure suivie en direct depuis Stats
    seg_debut("s10_stats");
    onglet(STATS);
    attendre(2.0f);
    capture("c21_stats");
    onglet(MESURE);
    attendre(0.5f);
    mesurer();
    onglet(STATS);
    attendre(3.0f);
    capture("c22_stats_en_direct");
    attendre(3.0f);
    taper(160, 120);
    attendre(0.6f);
    seg_fin();

    // 11. effacement (sans confirmer)
    onglet(REGLAGES);
    attendre(0.8f);
    taper(80, 186);
    attendre(0.4f);
    capture("c23_effacer_confirmation");
    attendre(3.5f);
    return 0;
}
