// sim_main.cpp - simulateur PC de son_console (meme code de dessin, de calcul
// et de traitement du signal que la carte). Rejoue un scenario tactile et
// enregistre des captures (PPM) et des images pour les animations.
//
//   g++ -std=c++17 -O2 -I../../firmware/son_console sim_main.cpp
//       ../../firmware/son_console/{gfx,son,dsp,moteur,interface,polices}.cpp -o sim
//   ./sim <dossier_images>
//
// Les GIF : ffmpeg -framerate 15 -i <dossier>/<nom>_%04d.ppm <nom>.gif
#include "gfx.h"
#include "interface.h"
#include "son.h"
#include "moteur.h"
#include "plateforme.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static uint32_t g_ms = 0;
static std::map<std::string, std::vector<uint8_t>> g_nvs;

uint32_t plat_ms() { return g_ms; }
void plat_verrou() {}
void plat_deverrou() {}
void plat_luminosite(int) {}
bool plat_audio_ok() { return false; }
const char *plat_audio_etat() { return "pas de codec (PC)"; }
void plat_audio_niveaux(int, int) {}
bool plat_audio_ping(const int16_t *, int16_t *, int) { return false; }
void plat_audio_tonalite(float, float) {}
bool plat_audio_lire(int16_t *, int) { return false; }
bool plat_sd() { return true; }
void plat_csv(const char *) {}
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
uint32_t plat_memoire_libre() { return 171 * 1024; }

static uint16_t fb[320 * 240], fond[320 * 240];
static const char *g_dossier = ".";

static void ppm(const char *chemin)
{
    FILE *f = fopen(chemin, "wb");
    if (!f) return;
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
    printf("capture %-26s t=%6.2f s  points=%d  c=%.2f  passages=%d\n", nom, g_ms / 1000.0, son::nb_pts,
           son::aj.ok ? son::aj.c : 0.0, son::nb_passages);
}

static std::string g_gif;
static int g_gif_n = 0, g_gif_pas = 0;

static void gif(const char *nom) { g_gif = nom ? nom : ""; g_gif_n = 0; g_gif_pas = 0; }

// une image a 30 i/s, doigt (appui, x, y)
static void image(bool appui = false, int x = 0, int y = 0)
{
    g_ms += 33;
    mot::etape(g_ms);
    ui::toucher(appui, x, y);
    ui::image(g_ms);
    if (!g_gif.empty() && (g_gif_pas++ % 2 == 0)) {
        char c[512];
        snprintf(c, sizeof c, "%s/%s_%04d.ppm", g_dossier, g_gif.c_str(), g_gif_n++);
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

static void onglet(int i) { taper(32 + 64 * i, 222); attendre(0.5f); }

int main(int argc, char **argv)
{
    if (argc > 1) g_dossier = argv[1];
    gfx::init(fb);
    mot::init();
    ui::init(fond);

    attendre(1.4f);
    capture("e00_demarrage");
    attendre(1.8f);
    capture("e01_audio_absent");

    // Reglages -> Simulation
    onglet(4);
    taper(270, 66);
    attendre(0.8f);
    capture("e02_reglages_experience");
    onglet(0);

    // visee : la regle, les pings en continu
    gif("anim_regle");
    glisser(220, 137, 140, 137, 14);
    attendre(1.2f);
    taper(154 + 2, 171);                 // pastille 0,5 m
    attendre(1.5f);
    gif(nullptr);
    capture("e03_visee");

    // plan 4 m : 8 distances
    gif("anim_mesure_echo");
    taper(76, 181);
    attendre(3.2f);
    capture("e04_collecte");
    attendre(4.4f);
    gif(nullptr);
    capture("e05_premier_point");
    for (int k = 0; k < 7; k++) {
        taper(76, 181);
        attendre(7.6f);
        if (k == 0) {
            attendre(0.7f);
            capture("e06_revelation");
        }
        attendre(1.0f);
        taper(160, 120);                 // ferme la revelation
        attendre(0.3f);
    }
    capture("e07_plan_termine");

    onglet(1);
    attendre(1.2f);
    capture("e08_courbe");
    taper(46 + (int)((304 - 46) * 2.5 / (4 * 1.12)), 100);
    attendre(0.2f);
    // trouve le point le plus proche quel que soit y : on tape sur la droite
    capture("e08b_courbe_point");
    taper(5, 60);
    onglet(2);
    gif("anim_resultat_echo");
    attendre(2.5f);
    gif(nullptr);
    capture("e09_resultat");
    onglet(3);
    attendre(1.0f);
    capture("e10_stats");
    onglet(0);
    taper(76, 181);
    onglet(3);
    attendre(3.0f);
    capture("e11_stats_en_cours");
    onglet(0);
    taper(76, 181);                      // arrete

    // ===================== DOPPLER =====================
    taper(150, 12);
    attendre(0.8f);
    capture("d00_radar_arrete");
    taper(76, 181);                      // radar
    gif("anim_doppler");
    attendre(3.6f);
    capture("d01_approche");
    attendre(4.0f);
    capture("d02_eloigne");
    attendre(4.0f);
    capture("d03_course");
    gif(nullptr);
    attendre(6.0f);
    onglet(1);
    attendre(3.0f);
    capture("d04_courbe");
    onglet(2);
    attendre(1.5f);
    capture("d05_resultat");
    onglet(3);
    attendre(2.6f);
    capture("d06_stats");
    onglet(4);
    taper(150, 41);                      // page Appareil
    attendre(0.8f);
    capture("d07_reglages_appareil");
    onglet(0);
    attendre(1.0f);
    return 0;
}
