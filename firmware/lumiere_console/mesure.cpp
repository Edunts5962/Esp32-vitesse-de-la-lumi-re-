// mesure.cpp - voir mesure.h
#include "mesure.h"
#include "plateforme.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

namespace mes {

Point pts[MAX_PTS];
int nb_pts = 0;
Ajustement aj = { false, 0, 0, 0, 0, 0, 0 };
float conv_c[MAX_PTS];
int nb_conv = 0;

uint32_t nb_echant_total = 0, nb_sessions = 0, nb_rejets_total = 0, nb_regressions = 0, nb_tris = 0;

int source = SRC_REEL;
float part_tronquee = 0;
int par_point = 600;

// ---- file d'attente entre la tache Wi-Fi et l'interface -------------------
static const int FILE_N = 1024;
static uint32_t f_rtt[FILE_N];
static int8_t f_rssi[FILE_N];
static volatile int f_tete = 0, f_queue = 0;
static volatile uint32_t f_sessions = 0;

void ajouter(uint32_t rtt_ps, int8_t rssi)
{
    plat_verrou();
    int t = f_tete, s = (t + 1) % FILE_N;
    if (s != f_queue) {
        f_rtt[t] = rtt_ps;
        f_rssi[t] = rssi;
        f_tete = s;
    }
    plat_deverrou();
}

void session()
{
    plat_verrou();
    f_sessions++;
    plat_deverrou();
}

// ---- etat interne ----------------------------------------------------------
static uint32_t ech[MAX_ECH];          // echantillons du point en cours / dernier point
static int8_t ech_rssi[MAX_ECH];
static int nb_ech = 0;
static float ech_d = 0;
static bool collecte = false;

static uint32_t direct[64];             // derniers echantillons (affichage en direct)
static int direct_n = 0, direct_i = 0;
static int rssi_dir = -60;
static uint32_t dernier_ms = 0, nouveaux = 0;
static float debit_lisse = 0;
static uint32_t debit_t0 = 0, debit_n0 = 0;

// ---- simulation ------------------------------------------------------------
static uint32_t alea_etat = 0x12345678;
static inline uint32_t alea()
{
    alea_etat ^= alea_etat << 13;
    alea_etat ^= alea_etat >> 17;
    alea_etat ^= alea_etat << 5;
    return alea_etat;
}
static inline float alea01() { return (alea() >> 8) * (1.0f / 16777216.0f); }
static float gauss()
{
    float u = alea01() + 1e-7f, v = alea01();
    return sqrtf(-2.0f * logf(u)) * cosf(6.2831853f * v);
}

static uint32_t sim_t = 0;
static float sim_reste = 0;
static int sim_compteur = 0;

static void simuler(uint32_t ms, float d)
{
    if (sim_t == 0) sim_t = ms;
    float dt = (ms - sim_t) / 1000.0f;
    sim_t = ms;
    if (dt > 0.5f) dt = 0.5f;
    sim_reste += dt * 115.0f;                         // ~115 echantillons/s
    int n = (int)sim_reste;
    sim_reste -= n;
    for (int i = 0; i < n; i++) {
        // Comme les vraies puces : temps deja corrige des retards internes,
        // par paliers de 1,5625 ns, et bloque a ~0 quand le bruit le rend negatif.
        double vrai_ns = 2.0 * d / C_REF * 1e9;
        double biais = 0.55 * sin(d * 1.3 + 0.4);       // reflexions : petit biais selon la distance
        double ns = vrai_ns + 1.2 + biais + 4.5 * gauss();
        if (alea01() < 0.03f) ns += 15 + 60 * alea01(); // aberrations (trajets indirects)
        ns = floor(ns / 1.5625 + 0.5) * 1.5625;
        if (ns <= 0) ns = 0.001;
        int rssi = (int)(-38 - 20 * log10f(d > 0.2f ? d : 0.2f) + 2 * gauss());
        ajouter((uint32_t)(ns * 1000.0), (int8_t)rssi);
        if (++sim_compteur >= 32) { sim_compteur = 0; session(); }
    }
}

// ---- calculs ---------------------------------------------------------------
static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static uint32_t tri_tmp[MAX_ECH];

// Mediane, rejet des aberrations a plus de 3 ecarts absolus medians, moyenne.
static bool calculer_point(float d, Point *p)
{
    int n = nb_ech;
    if (n < 20) return false;
    int bas = 0;
    for (int i = 0; i < n; i++) if (ech[i] <= 1) bas++;
    part_tronquee = (float)bas / n;
    memcpy(tri_tmp, ech, n * sizeof(uint32_t));
    qsort(tri_tmp, n, sizeof(uint32_t), cmp_u32);
    nb_tris++;
    double med = tri_tmp[n / 2];
    static uint32_t ecarts[MAX_ECH];
    for (int i = 0; i < n; i++) ecarts[i] = (uint32_t)fabs((double)tri_tmp[i] - med);
    qsort(ecarts, n, sizeof(uint32_t), cmp_u32);
    nb_tris++;
    double mad = ecarts[n / 2] < 1 ? 1 : ecarts[n / 2];
    double seuil = 3.0 * 1.4826 * mad;
    double s = 0, s2 = 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (fabs((double)tri_tmp[i] - med) <= seuil) {
            s += tri_tmp[i];
            s2 += (double)tri_tmp[i] * tri_tmp[i];
            k++;
        }
    }
    if (k < 10) return false;
    double moy = s / k;
    double var = (s2 / k - moy * moy) * k / (k - 1);
    if (var < 0) var = 0;
    p->d = d;
    p->rtt = (float)(moy / 1000.0);
    p->sigma = (float)(sqrt(var) / 1000.0);
    p->err = (float)(sqrt(var / k) / 1000.0);
    p->n = (uint16_t)k;
    p->rejets = (uint16_t)(n - k);
    nb_rejets_total += n - k;
    return true;
}

static Ajustement ajuster(int n)
{
    Ajustement r = { false, 0, 0, 0, 0, 0, 0 };
    if (n < 2) return r;
    nb_regressions++;
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += pts[i].d; my += pts[i].rtt; }
    mx /= n; my /= n;
    double sxx = 0, sxy = 0, syy = 0;
    for (int i = 0; i < n; i++) {
        double dx = pts[i].d - mx, dy = pts[i].rtt - my;
        sxx += dx * dx; sxy += dx * dy; syy += dy * dy;
    }
    if (sxx < 1e-6) return r;
    r.a = sxy / sxx;
    r.b = my - r.a * mx;
    double ss = 0;
    for (int i = 0; i < n; i++) {
        double e = pts[i].rtt - (r.a * pts[i].d + r.b);
        ss += e * e;
    }
    if (n > 2) {
        r.sa = sqrt(ss / (n - 2) / sxx);
    } else {                                           // 2 points : erreurs des moyennes
        double e = sqrt((double)pts[0].err * pts[0].err + (double)pts[1].err * pts[1].err);
        r.sa = e / fabs((double)pts[1].d - pts[0].d);
    }
    r.r2 = syy > 0 ? 1.0 - ss / syy : 1.0;
    if (r.a > 0) {
        r.c = 2.0 / (r.a * 1e-9);
        r.sc = r.c * r.sa / r.a;
        r.ok = true;
    }
    return r;
}

static void recalculer()
{
    nb_conv = 0;
    for (int k = 2; k <= nb_pts; k++) {
        Ajustement t = ajuster(k);
        conv_c[nb_conv++] = t.ok ? (float)(t.c / 1e6) : 0.0f;
    }
    aj = ajuster(nb_pts);
}

// ---- interface publique ----------------------------------------------------
void demarrer(float d)
{
    if (nb_pts >= MAX_PTS) return;
    ech_d = d;
    nb_ech = 0;
    collecte = true;
}

void arreter() { collecte = false; }
bool en_collecte() { return collecte; }
int collectes() { return nb_ech; }
float collecte_d() { return ech_d; }

int tick(uint32_t ms, float d_reglee)
{
    if (source == SRC_SIM) simuler(ms, collecte ? ech_d : d_reglee);
    else sim_t = 0;

    // vider la file d'attente
    static uint32_t tmp_rtt[FILE_N];
    static int8_t tmp_rssi[FILE_N];
    int n = 0;
    uint32_t sess;
    plat_verrou();
    while (f_queue != f_tete && n < FILE_N) {
        tmp_rtt[n] = f_rtt[f_queue];
        tmp_rssi[n] = f_rssi[f_queue];
        f_queue = (f_queue + 1) % FILE_N;
        n++;
    }
    sess = f_sessions;
    f_sessions = 0;
    plat_deverrou();

    nb_sessions += sess;
    int ev = EV_RIEN;
    for (int i = 0; i < n; i++) {
        uint32_t v = tmp_rtt[i];
        if (v == 0) continue;
        nb_echant_total++;
        nouveaux++;
        direct[direct_i] = v;
        direct_i = (direct_i + 1) & 63;
        if (direct_n < 64) direct_n++;
        rssi_dir = tmp_rssi[i];
        dernier_ms = ms;
        if (collecte && nb_ech < par_point && nb_ech < MAX_ECH) {
            ech[nb_ech] = v;
            ech_rssi[nb_ech] = tmp_rssi[i];
            nb_ech++;
        }
    }

    if (collecte && (nb_ech >= par_point || nb_ech >= MAX_ECH)) {
        collecte = false;
        Point p;
        if (nb_pts < MAX_PTS && calculer_point(ech_d, &p)) {
            pts[nb_pts++] = p;
            plat_csv(ech_d, ech, ech_rssi, nb_ech);
            recalculer();
            sauver();
            ev = EV_POINT;
        } else {
            ev = EV_ECHEC;
        }
    }

    // debit (echantillons/s), lisse
    if (debit_t0 == 0) { debit_t0 = ms; debit_n0 = nb_echant_total; }
    if (ms - debit_t0 >= 500) {
        float inst = (nb_echant_total - debit_n0) * 1000.0f / (ms - debit_t0);
        debit_lisse = debit_lisse * 0.6f + inst * 0.4f;
        debit_t0 = ms;
        debit_n0 = nb_echant_total;
    }
    return ev;
}

float rtt_direct()
{
    if (direct_n == 0) return 0;
    uint32_t t[64];
    int n = direct_n < 32 ? direct_n : 32;
    for (int i = 0; i < n; i++) t[i] = direct[(direct_i - 1 - i + 64) & 63];
    qsort(t, n, sizeof(uint32_t), cmp_u32);
    return t[n / 2] / 1000.0f;
}

int rssi_direct() { return rssi_dir; }
float debit() { return debit_lisse; }
bool signal_recent() { return dernier_ms != 0 && plat_ms() - dernier_ms < 1500; }

uint32_t nb_nouveaux()
{
    uint32_t n = nouveaux;
    nouveaux = 0;
    return n;
}

void histogramme(Histo &h, int nb_bins)
{
    h.ok = false;
    h.nb = nb_ech;
    h.nb_bins = nb_bins > 48 ? 48 : nb_bins;
    if (nb_ech < 8) return;
    int n = nb_ech;
    memcpy(tri_tmp, ech, n * sizeof(uint32_t));
    qsort(tri_tmp, n, sizeof(uint32_t), cmp_u32);
    nb_tris++;
    float med = tri_tmp[n / 2] / 1000.0f;
    float q1 = tri_tmp[n / 4] / 1000.0f, q3 = tri_tmp[3 * n / 4] / 1000.0f;
    float sig_robuste = (q3 - q1) / 1.349f;
    if (sig_robuste < 0.5f) sig_robuste = 0.5f;
    h.med = med;
    // colonnes alignees sur les paliers de la puce (1,5625 ns) : pas de "peigne"
    const float Q = 1.5625f;
    float lo = med - 4 * sig_robuste, hi = med + 4 * sig_robuste;
    float pas = Q * ceilf((hi - lo) / h.nb_bins / Q);
    h.lo = floorf(lo / Q) * Q - Q / 2;
    int nb = (int)ceilf((hi - h.lo) / pas);
    if (nb > 48) nb = 48;
    if (nb < 4) nb = 4;
    h.nb_bins = nb;
    h.hi = h.lo + nb * pas;
    memset(h.bins, 0, sizeof(h.bins));
    double s = 0, s2 = 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        float v = tri_tmp[i] / 1000.0f;
        if (v < h.lo || v >= h.hi) continue;
        int b = (int)((v - h.lo) / (h.hi - h.lo) * h.nb_bins);
        if (b >= 0 && b < h.nb_bins) h.bins[b]++;
        s += v; s2 += (double)v * v; k++;
    }
    h.nmax = 1;
    for (int b = 0; b < h.nb_bins; b++) if (h.bins[b] > h.nmax) h.nmax = h.bins[b];
    h.moy = k ? (float)(s / k) : med;
    double var = k > 1 ? (s2 / k - (s / k) * (s / k)) : 0;
    h.sig = (float)sqrt(var > 0 ? var : 0);
    h.ok = true;
}

void supprimer(int i)
{
    if (i < 0 || i >= nb_pts) return;
    for (int k = i; k < nb_pts - 1; k++) pts[k] = pts[k + 1];
    nb_pts--;
    recalculer();
    sauver();
}

void effacer()
{
    nb_pts = 0;
    nb_ech = 0;
    recalculer();
    sauver();
}

struct Sauvegarde {
    uint32_t magie;
    int32_t nb;
    Point p[MAX_PTS];
};
static const uint32_t MAGIE = 0x4C554D32;   // "LUM2"

void sauver()
{
    static Sauvegarde s;
    s.magie = MAGIE;
    s.nb = nb_pts;
    memcpy(s.p, pts, sizeof(pts));
    plat_nvs_ecrire("points", &s, sizeof(s));
}

void charger()
{
    static Sauvegarde s;
    if (plat_nvs_lire("points", &s, sizeof(s)) && s.magie == MAGIE && s.nb >= 0 && s.nb <= MAX_PTS) {
        nb_pts = s.nb;
        memcpy(pts, s.p, sizeof(pts));
    }
    recalculer();
}

}  // namespace mes
