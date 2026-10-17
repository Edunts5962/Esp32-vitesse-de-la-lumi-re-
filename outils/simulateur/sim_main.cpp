// sim_main.cpp - simulateur PC de lumiere_console (meme code de dessin et de
// calcul que la carte). Rejoue un scenario tactile et enregistre des images.
//
//   g++ -std=c++17 -O2 -I../../lumiere_console sim_main.cpp
//       ../../lumiere_console/{gfx,mesure,interface,polices}.cpp -o sim
//   ./sim <dossier_images>
#include "gfx.h"
#include "interface.h"
#include "mesure.h"
#include "plateforme.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static uint32_t g_ms = 0;
static std::map<std::string, std::vector<uint8_t>> g_nvs;
static int g_source = 0;

uint32_t plat_ms() { return g_ms; }
void plat_verrou() {}
void plat_deverrou() {}
void plat_luminosite(int) {}
void plat_source(int s) { g_source = s; }
bool plat_carte_a() { return false; }
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
uint32_t plat_memoire_libre() { return 183 * 1024; }

static uint16_t fb[320 * 240], fond[320 * 240];
static const char *g_dossier = ".";
static int g_gif = 0;

static void ppm(const char *chemin)
{
    FILE *f = fopen(chemin, "wb");
    fprintf(f, "P6\n320 240\n255\n");
    for (int i = 0; i < 320 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
        r |= r >> 5; g |= g >> 6; b |= b >> 5;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}

static void capture(const char *nom)
{
    char c[512];
    snprintf(c, sizeof c, "%s/%s.ppm", g_dossier, nom);
    ppm(c);
    printf("capture %s (t=%.2f s, points=%d)\n", nom, g_ms / 1000.0, mes::nb_pts);
}

static bool g_gif_actif = false;
static int g_gif_pas = 0;

// une image a 30 i/s, doigt (appui, x, y)
static void image(bool appui = false, int x = 0, int y = 0)
{
    g_ms += 33;
    ui::toucher(appui, x, y);
    ui::image(g_ms);
    if (g_gif_actif && (g_gif_pas++ % 2 == 0)) {
        char c[512];
        snprintf(c, sizeof c, "%s/gif_%04d.ppm", g_dossier, g_gif++);
        ppm(c);
    }
}

static void attendre(float s) { for (int i = 0; i < (int)(s * 30); i++) image(); }

static void taper(int x, int y)
{
    image(true, x, y);
    image(true, x, y);
    image(false, x, y);
}

static void glisser(int x0, int y0, int x1, int y1, int n)
{
    for (int i = 0; i <= n; i++) image(true, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
    image(false, x1, y1);
}

int main(int argc, char **argv)
{
    if (argc > 1) g_dossier = argv[1];
    gfx::init(fb);
    ui::init(fond);

    attendre(1.3f);
    capture("00_demarrage");
    attendre(1.6f);
    capture("01_attente_carte_a");

    // Reglages -> Simulation
    taper(288, 222);
    attendre(0.6f);
    taper(270, 48);
    attendre(0.8f);
    capture("02_reglages");
    taper(32, 222);
    attendre(0.6f);

    // glissement de la regle
    g_gif_actif = true;
    glisser(220, 137, 120, 137, 12);
    attendre(1.0f);
    capture("03_regle");
    // retour a la premiere distance du plan
    taper(150 + 10, 171);
    attendre(0.8f);

    // premiere mesure
    taper(76, 181);
    attendre(2.2f);
    capture("04_collecte");
    attendre(3.6f);
    g_gif_actif = false;
    capture("05_premier_point");

    // les distances suivantes du plan
    for (int k = 0; k < 7; k++) {
        taper(76, 181);
        attendre(5.8f);
        if (k == 0) {
            attendre(0.9f);
            capture("06_revelation");
        }
        attendre(2.0f);
        taper(160, 120);          // ferme la revelation
        attendre(0.4f);
    }
    capture("07_mesure_plan_fini");

    taper(96, 222);               // Courbe
    attendre(1.5f);
    capture("08_courbe");
    taper(138, 122);              // le point a 8 m
    attendre(0.6f);
    capture("08b_courbe_point");
    taper(32 + 64, 222);
    attendre(0.2f);

    taper(160, 222);              // Resultat
    g_gif_actif = true;
    attendre(2.5f);
    g_gif_actif = false;
    capture("09_resultat");

    taper(224, 222);              // Stats
    attendre(1.5f);
    capture("10_stats");

    // une mesure en cours vue depuis Stats
    taper(32, 222);
    attendre(0.3f);
    taper(270, 137);              // fleche +0,1
    taper(76, 181);
    taper(224, 222);
    attendre(3.0f);
    capture("11_stats_en_cours");

    taper(96, 222);
    attendre(1.0f);
    return 0;
}
