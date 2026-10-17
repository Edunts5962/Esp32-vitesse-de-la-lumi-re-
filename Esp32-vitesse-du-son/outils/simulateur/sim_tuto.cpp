// sim_tuto.cpp - "tournage" des ecrans pour la video et le tutoriel.
//
// Meme code de dessin, de calcul et de traitement du signal que la carte
// ES3C28P. Ici la source est "Micro" (comme sur la vraie carte) : le simulateur
// fabrique le son que le micro entendrait (dsp::synth_echo / synth_doppler)
// et le fait passer par le meme chemin que la tache audio
// (plat_audio_ping, plat_audio_tonalite, plat_audio_lire).
//
//   g++ -std=c++17 -O2 -I../../firmware/son_console sim_tuto.cpp
//       ../../firmware/son_console/{gfx,son,dsp,moteur,interface,polices}.cpp -o sim_tuto
//   ./sim_tuto <dossier>
//
// Sorties : <nom>.ppm (captures), <segment>.rgb (images RGB888 320x240 a la
// suite, 30 i/s) et <segment>.appuis (positions du doigt) pour la video.
#include "gfx.h"
#include "interface.h"
#include "son.h"
#include "dsp.h"
#include "moteur.h"
#include "plateforme.h"
#include <cmath>
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
bool plat_audio_ok() { return true; }
const char *plat_audio_etat() { return "OK · direct 52 dB"; }
void plat_audio_niveaux(int, int) {}
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

// ---- le "micro" simule ------------------------------------------------------
static uint32_t g_graine = 0xBADC0DE5;
static float c_air() { return (float)son::c_theorie(son::temperature + 0.6f); }

bool plat_audio_ping(const int16_t *, int16_t *rx, int)
{
    float d = son::en_collecte() ? son::collecte_d() : son::d_visee;
    d += 0.005f * sinf(d * 7.3f + 1.1f);          // placement au metre ruban
    dsp::synth_echo(rx, d, c_air(), g_graine);
    return true;
}

static float g_f0 = 0;
static uint32_t g_ton_ms = 0;
static double g_consomme = 0;                      // echantillons deja lus
static float g_dist = 4.5f;
static dsp::EtatSynthDop g_sd = { 0, 0, 0, 0, 0x7777 };

// une personne va et vient devant la carte
static float scenario(double t)
{
    struct Seg { float t0, t1, v; };
    static const Seg segs[] = {
        { 0.0f, 1.2f, 0.0f }, { 1.2f, 4.4f, 1.35f }, { 4.4f, 5.6f, 0.0f }, { 5.6f, 8.8f, -1.10f },
        { 8.8f, 9.8f, 0.0f }, { 9.8f, 11.3f, 3.20f }, { 11.3f, 12.4f, 0.0f }, { 12.4f, 14.8f, -1.80f },
        { 14.8f, 16.0f, 0.0f },
    };
    double u = fmod(t, 16.0);
    for (const Seg &s : segs)
        if (u >= s.t0 && u < s.t1) {
            float a = (float)(u - s.t0), b = (float)(s.t1 - u);
            float k = fminf(1.0f, fminf(a / 0.45f, b / 0.45f));
            return s.v * k * k * (3 - 2 * k);
        }
    return 0;
}

void plat_audio_tonalite(float f0, float)
{
    g_f0 = f0;
    g_ton_ms = g_ms;
    g_consomme = 0;
}

bool plat_audio_lire(int16_t *rx, int n)
{
    double dispo = (g_ms - g_ton_ms) * (dsp::FS / 1000.0) - g_consomme;
    if (g_f0 <= 0 || dispo < n) return false;
    double t = g_consomme / dsp::FS;
    float v = scenario(t);
    dsp::synth_doppler(rx, n, g_f0, v, g_dist, c_air(), g_sd);
    g_dist -= v * n / (float)dsp::FS;
    if (g_dist < 0.7f) g_dist = 0.7f;
    if (g_dist > 6) g_dist = 6;
    g_consomme += n;
    return true;
}

// ---- enregistrement -----------------------------------------------------------
static uint16_t fb[320 * 240], fond[320 * 240];
static std::string g_dossier = ".";
static FILE *g_seg = nullptr, *g_appuis = nullptr;
static int g_pas = 1, g_compteur = 0, g_n = 0;

static void rgb(FILE *f)
{
    static uint8_t ligne[320 * 240 * 3];
    for (int i = 0; i < 320 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
        ligne[3 * i] = r | r >> 5; ligne[3 * i + 1] = g | g >> 6; ligne[3 * i + 2] = b | b >> 5;
    }
    fwrite(ligne, 1, sizeof ligne, f);
}

static void capture(const char *nom)
{
    std::string c = g_dossier + "/" + nom + ".ppm";
    FILE *f = fopen(c.c_str(), "wb");
    fprintf(f, "P6\n320 240\n255\n");
    rgb(f);
    fclose(f);
    printf("capture %-22s t=%6.2f s  points=%d  c=%.2f  passages=%d\n", nom, g_ms / 1000.0, son::nb_pts,
           son::aj.ok ? son::aj.c : 0.0, son::nb_passages);
}

static void segment(const char *nom, int pas = 1)
{
    if (g_seg) { fclose(g_seg); fclose(g_appuis); printf("  segment : %d images\n", g_n); }
    g_seg = g_appuis = nullptr;
    if (!nom) return;
    g_seg = fopen((g_dossier + "/" + nom + ".rgb").c_str(), "wb");
    g_appuis = fopen((g_dossier + "/" + nom + ".appuis").c_str(), "w");
    g_pas = pas;
    g_compteur = 0;
    g_n = 0;
    printf("segment %s (x%d)\n", nom, pas);
}

static void image(bool appui = false, int x = 0, int y = 0)
{
    g_ms += 33;
    mot::etape(g_ms);
    ui::toucher(appui, x, y);
    ui::image(g_ms);
    if (g_seg && g_compteur++ % g_pas == 0) {
        rgb(g_seg);
        if (appui) fprintf(g_appuis, "%d %d %d\n", g_n, x, y);
        g_n++;
    }
}

static void attendre(float s) { for (int i = 0; i < (int)(s * 30); i++) image(); }
static void taper(int x, int y) { image(true, x, y); image(true, x, y); image(true, x, y); image(false, x, y); }
static void glisser(int x0, int y0, int x1, int y1, int n)
{
    for (int i = 0; i <= n; i++) image(true, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
    image(false, x1, y1);
}
static void onglet(int i) { taper(32 + 64 * i, 222); }

int main(int argc, char **argv)
{
    if (argc > 1) g_dossier = argv[1];
    gfx::init(fb);
    mot::init();
    ui::init(fond);

    segment("s01_demarrage");
    attendre(1.4f);
    capture("c00_demarrage");
    attendre(3.4f);
    capture("c01_mesure_echo");

    segment("s02_reglages");
    onglet(4);
    attendre(1.0f);
    glisser(238, 182, 247, 182, 10);              // 21 degres
    attendre(0.8f);
    capture("c02_reglages");
    taper(150, 41);                               // page Appareil
    attendre(1.4f);
    taper(60, 41);
    attendre(0.6f);
    onglet(0);
    attendre(0.6f);

    segment("s03_regle");
    glisser(220, 137, 120, 137, 14);
    attendre(1.2f);
    taper(156, 171);                              // pastille 0,5 m
    attendre(1.6f);
    capture("c03_visee");

    segment("s04_premiere_mesure");
    taper(76, 181);
    attendre(3.4f);
    capture("c04_collecte");
    attendre(4.3f);

    segment("s05_revelation", 2);
    taper(76, 181);
    attendre(7.2f);
    segment("s05_revelation_b");                  // fin du segment a vitesse normale (ralenti)
    attendre(2.2f);
    capture("c05_revelation");
    attendre(0.6f);
    taper(160, 120);
    attendre(0.6f);

    segment("s06_plan_accelere", 4);
    for (int k = 0; k < 6; k++) {
        taper(76, 181);
        attendre(7.6f);
        attendre(1.2f);
        taper(160, 120);
        attendre(0.3f);
    }
    attendre(0.8f);
    capture("c06_plan_termine");

    segment("s07_courbe");
    onglet(1);
    attendre(1.4f);
    taper(46 + (int)((304 - 46) * 3.0 / (4 * 1.12)), 80);
    attendre(1.8f);
    capture("c07_courbe");
    taper(5, 60);
    attendre(0.4f);

    segment("s08_resultat");
    onglet(2);
    attendre(3.6f);
    capture("c08_resultat");

    segment("s09_stats");
    onglet(3);
    attendre(3.6f);
    capture("c09_stats");

    segment("s10_radar");
    onglet(0);
    taper(150, 12);                               // Doppler
    attendre(1.2f);
    capture("c10_radar_arrete");
    taper(76, 181);                               // RADAR
    attendre(4.3f);
    capture("c11_approche");
    attendre(1.5f);

    segment("s11_passages");
    attendre(3.4f);
    capture("c12_eloigne");
    attendre(2.6f);
    capture("c13_course");
    attendre(3.0f);

    segment("s12_dop_courbe");
    attendre(3.0f);                               // laisse finir le passage
    onglet(1);
    attendre(3.4f);
    capture("c14_dop_courbe");

    segment("s13_dop_resultat");
    onglet(2);
    attendre(3.6f);
    capture("c15_dop_resultat");

    segment("s14_dop_stats");
    onglet(3);
    attendre(4.0f);
    capture("c16_dop_stats");
    segment(nullptr);
    return 0;
}
