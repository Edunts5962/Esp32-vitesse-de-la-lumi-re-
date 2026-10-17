// son.cpp - voir son.h
#include "son.h"
#include "plateforme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace son {

// ---- reglages ---------------------------------------------------------------
volatile int mode = MODE_ECHO;
volatile int source = SRC_REEL;
volatile int par_point = 30;
volatile int radar = 0;
volatile float f0 = 10000;
volatile float temperature = 20;
volatile float d_visee = 1.0f;

double c_theorie(float t) { return 331.3 * sqrt(1.0 + t / 273.15); }

bool c_mesuree() { return aj.ok && nb_pts >= 3 && aj.r2 > 0.99 && aj.c > 250 && aj.c < 450; }

float c_utilisee() { return c_mesuree() ? (float)aj.c : (float)c_theorie(temperature); }

float f0_emise() { return dsp::frequence_emise(f0); }

// ================================================================= ECHO
Point pts[MAX_PTS];
int nb_pts = 0;
Ajustement aj = { false, 0, 0, 0, 0, 0, 0 };
float conv_c[MAX_PTS];
int nb_conv = 0;
uint32_t nb_pings = 0, nb_valides = 0, nb_rejets_total = 0, nb_regressions = 0, nb_tris = 0;

// file d'attente tache audio -> interface
static const int FILE_E = 6;
static dsp::Echo f_echo[FILE_E];
static volatile int fe_tete = 0, fe_queue = 0;

void ajouter_ping(const dsp::Echo &e)
{
    plat_verrou();
    int s = (fe_tete + 1) % FILE_E;
    if (s != fe_queue) {
        f_echo[fe_tete] = e;
        fe_tete = s;
    }
    plat_deverrou();
}

static dsp::Echo dernier;
static float ech[MAX_PINGS];            // pings du point en cours / dernier point
static float ech_snr[MAX_PINGS];
static int nb_ech = 0;
static int essais = 0;
static float ech_d = 0;
static bool collecte = false;

static float recents[8], recents_d[8];
static int rec_n = 0, rec_i = 0;
static float snr_lisse = 0;
static uint32_t dernier_ok_ms = 0, nouveaux = 0;
static float cadence_lisse = 0;
static uint32_t cad_t0 = 0, cad_n0 = 0;

const dsp::Echo &dernier_echo() { return dernier; }
bool en_collecte() { return collecte; }
int collectes() { return nb_ech; }
float collecte_d() { return ech_d; }
float cadence() { return cadence_lisse; }
float snr_direct() { return snr_lisse; }
bool echo_recent() { return dernier_ok_ms && plat_ms() - dernier_ok_ms < 2500; }

uint32_t nb_nouveaux()
{
    uint32_t n = nouveaux;
    nouveaux = 0;
    return n;
}

static int cmp_f(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

float dt_direct()
{
    if (rec_n == 0 || !echo_recent()) return 0;
    float t[8];
    int k = 0;
    for (int i = 0; i < rec_n; i++)
        if (fabsf(recents_d[i] - d_visee) < 0.005f || (collecte && fabsf(recents_d[i] - ech_d) < 0.005f)) t[k++] = recents[i];
    if (k == 0) return 0;
    qsort(t, k, sizeof(float), cmp_f);
    return t[k / 2];
}

void demarrer(float d)
{
    ech_d = d;
    nb_ech = 0;
    essais = 0;
    collecte = true;
}

void arreter() { collecte = false; }

// mediane, rejet a plus de 3 ecarts absolus medians, moyenne
static bool calculer_point(Point &p)
{
    int n = nb_ech;
    if (n < 5) return false;
    static float t[MAX_PINGS], e[MAX_PINGS];
    memcpy(t, ech, n * sizeof(float));
    qsort(t, n, sizeof(float), cmp_f);
    float med = t[n / 2];
    for (int i = 0; i < n; i++) e[i] = fabsf(t[i] - med);
    qsort(e, n, sizeof(float), cmp_f);
    nb_tris += 2;
    float mad = e[n / 2] < 0.5f ? 0.5f : e[n / 2];
    float seuil = 3.0f * 1.4826f * mad;
    double s = 0, s2 = 0, sn = 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (fabsf(ech[i] - med) <= seuil) {
            s += ech[i];
            s2 += (double)ech[i] * ech[i];
            sn += ech_snr[i];
            k++;
        }
    }
    if (k < 3) return false;
    double moy = s / k;
    double var = s2 / k - moy * moy;
    if (var < 0) var = 0;
    p.d = ech_d;
    p.dt = (float)moy;
    p.sigma = (float)sqrt(var * k / (k - 1));
    p.err = p.sigma / sqrtf((float)k);
    if (p.err < 1.0f / 48.0f) p.err = 1.0f / 48.0f;   // jamais mieux qu'une fraction d'echantillon
    p.snr = (float)(sn / k);
    p.n = (uint16_t)k;
    p.rejets = (uint16_t)(n - k);
    nb_rejets_total += n - k;
    return true;
}

// droite des moindres carres sur les k premiers points
static bool droite(int k, double &a, double &b, double &sa, double &r2)
{
    if (k < 2) return false;
    double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
    for (int i = 0; i < k; i++) {
        double x = pts[i].d, y = pts[i].dt;
        sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y;
    }
    double den = k * sxx - sx * sx;
    if (fabs(den) < 1e-9) return false;
    a = (k * sxy - sx * sy) / den;
    b = (sy - a * sx) / k;
    double ss = 0, st = 0, my = sy / k;
    for (int i = 0; i < k; i++) {
        double r = pts[i].dt - (a * pts[i].d + b);
        ss += r * r;
        st += (pts[i].dt - my) * (pts[i].dt - my);
    }
    r2 = st > 0 ? 1 - ss / st : 1;
    sa = k > 2 ? sqrt(ss / (k - 2) / (sxx - sx * sx / k)) : 0;
    nb_regressions++;
    return a > 0;
}

static void recalculer()
{
    // points tries par distance
    for (int i = 1; i < nb_pts; i++)
        for (int j = i; j > 0 && pts[j].d < pts[j - 1].d; j--) {
            Point t = pts[j]; pts[j] = pts[j - 1]; pts[j - 1] = t;
        }
    aj.ok = false;
    double a, b, sa, r2;
    if (droite(nb_pts, a, b, sa, r2)) {
        aj.ok = true;
        aj.a = a; aj.b = b; aj.sa = sa; aj.r2 = r2;
        aj.c = 2e6 / a;
        aj.sc = aj.c * sa / a;
    }
    // convergence : c avec 2, 3, ... points (dans l'ordre de la mesure, approxime ici par la distance)
    nb_conv = 0;
    for (int k = 2; k <= nb_pts; k++) {
        if (droite(k, a, b, sa, r2)) conv_c[nb_conv++] = (float)(2e6 / a);
    }
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
    nb_conv = 0;
    aj.ok = false;
    nb_passages = 0;
    v_max_session = 0;
    sauver();
}

struct Sauvegarde { uint32_t magie; int32_t n; Point p[MAX_PTS]; };

void sauver()
{
    static Sauvegarde s;
    s.magie = 0x534F4E31;
    s.n = nb_pts;
    memcpy(s.p, pts, sizeof pts);
    plat_nvs_ecrire("points", &s, sizeof s);
}

void charger()
{
    static Sauvegarde s;
    if (plat_nvs_lire("points", &s, sizeof s) && s.magie == 0x534F4E31 && s.n >= 0 && s.n <= MAX_PTS) {
        nb_pts = s.n;
        memcpy(pts, s.p, sizeof pts);
        recalculer();
    }
}

void histogramme(Histo &h, int nb_bins)
{
    h.ok = false;
    int n = nb_ech;
    if (n < 3) return;
    static float t[MAX_PINGS];
    memcpy(t, ech, n * sizeof(float));
    qsort(t, n, sizeof(float), cmp_f);
    h.med = t[n / 2];
    // bornes : +- 4 sigma robustes autour de la mediane
    float q1 = t[n / 4], q3 = t[(3 * n) / 4];
    float l = (q3 - q1) * 3.0f;
    if (l < 20) l = 20;
    h.lo = h.med - l;
    h.hi = h.med + l;
    if (nb_bins > 48) nb_bins = 48;
    h.nb_bins = nb_bins;
    memset(h.bins, 0, sizeof h.bins);
    double s = 0, s2 = 0;
    for (int i = 0; i < n; i++) {
        s += ech[i]; s2 += (double)ech[i] * ech[i];
        int b = (int)((ech[i] - h.lo) / (h.hi - h.lo) * nb_bins);
        if (b >= 0 && b < nb_bins) h.bins[b]++;
    }
    h.nb = n;
    h.moy = (float)(s / n);
    double v = s2 / n - (double)h.moy * h.moy;
    h.sig = (float)sqrt(v > 0 ? v : 0);
    h.nmax = 1;
    for (int b = 0; b < nb_bins; b++) if (h.bins[b] > h.nmax) h.nmax = h.bins[b];
    h.ok = true;
}

static int traiter_ping(const dsp::Echo &e, uint32_t ms)
{
    dernier = e;
    nb_pings++;
    nouveaux++;
    if (!e.ok) {
        if (collecte && ++essais > par_point * 3 + 10) {
            collecte = false;
            return EV_ECHEC;
        }
        return EV_RIEN;
    }
    nb_valides++;
    dernier_ok_ms = ms;
    recents[rec_i] = e.dt_us;
    recents_d[rec_i] = e.d;
    rec_i = (rec_i + 1) % 8;
    if (rec_n < 8) rec_n++;
    snr_lisse = snr_lisse == 0 ? e.snr_db : snr_lisse * 0.8f + e.snr_db * 0.2f;
    if (!collecte) return EV_RIEN;
    essais++;
    if (nb_ech < MAX_PINGS) {
        ech[nb_ech] = e.dt_us;
        ech_snr[nb_ech] = e.snr_db;
        nb_ech++;
    }
    char l[96];
    snprintf(l, sizeof l, "echo,%.2f,%.2f,%.1f,%s", ech_d, e.dt_us, e.snr_db, source ? "sim" : "reel");
    plat_csv(l);
    if (nb_ech >= par_point) {
        collecte = false;
        Point p;
        if (!calculer_point(p)) return EV_ECHEC;
        // une distance deja mesuree est remplacee
        int idx = -1;
        for (int i = 0; i < nb_pts; i++) if (fabsf(pts[i].d - p.d) < 0.005f) idx = i;
        if (idx < 0) {
            if (nb_pts >= MAX_PTS) return EV_ECHEC;
            idx = nb_pts++;
        }
        pts[idx] = p;
        recalculer();
        sauver();
        return EV_POINT;
    }
    return EV_RIEN;
}

// ================================================================= DOPPLER
uint8_t spectro[SPEC_COLS][dsp::NB_SPECTRE];
int spectro_tete = 0;
uint32_t spectro_total = 0;
float hist_v[HIST_V];
int hist_tete = 0;
uint32_t hist_total = 0;
Passage passages[MAX_PASSAGES];
int nb_passages = 0;
uint32_t nb_passages_total = 0;
float v_max_session = 0;
uint32_t nb_trames = 0, nb_detections = 0;

static const int FILE_D = 6;
static dsp::Doppler f_dop[FILE_D];
static volatile int fd_tete = 0, fd_queue = 0;
static dsp::Doppler trame;
static float v_lisse = 0, df_lisse = 0;
static bool v_ok = false;
static float v_recent[3];
static int v_rn = 0;
static uint32_t trame_ms = 0;
static float tps_lisse = 0;
static uint32_t tps_t0 = 0, tps_n0 = 0;

// passage en cours
static bool pass_actif = false;
static float pass_vmax = 0, pass_somme = 0, pass_signe = 0;
static int pass_n = 0, pass_trou = 0;

void ajouter_trame(const dsp::Doppler &r)
{
    plat_verrou();
    int s = (fd_tete + 1) % FILE_D;
    if (s != fd_queue) {
        f_dop[fd_tete] = r;
        fd_tete = s;
    }
    plat_deverrou();
}

const dsp::Doppler &derniere_trame() { return trame; }
float v_direct() { return v_ok ? v_lisse : 0; }
bool v_detecte() { return v_ok && plat_ms() - trame_ms < 600; }
float df_direct() { return v_ok ? df_lisse : 0; }
float trames_par_s() { return tps_lisse; }
bool en_passage() { return pass_actif; }

static int traiter_trame(const dsp::Doppler &r, uint32_t ms)
{
    trame = r;
    trame_ms = ms;
    nb_trames++;
    memcpy(spectro[spectro_tete], r.colonne, dsp::NB_SPECTRE);
    spectro_tete = (spectro_tete + 1) % SPEC_COLS;
    spectro_total++;

    float fe = f0_emise();
    float c = c_utilisee();
    float v = r.detect ? c * r.df_hz / (2 * fe + r.df_hz) : 0;
    bool det = r.detect && fabsf(v) >= 0.2f;
    if (det) {
        nb_detections++;
        v_recent[v_rn % 3] = v;
        v_rn++;
        // mediane des 3 dernieres valeurs : enleve les a-coups des bras et jambes
        float a = v_recent[0], b = v_recent[1 % 3], d = v_recent[2 % 3];
        float med = v;
        if (v_rn >= 3) med = fmaxf(fminf(a, b), fminf(fmaxf(a, b), d));
        v_lisse = v_ok ? v_lisse * 0.35f + med * 0.65f : v;
        df_lisse = v_ok ? df_lisse * 0.35f + r.df_hz * 0.65f : r.df_hz;
        v_ok = true;
    } else {
        v_ok = false;
        v_rn = 0;
    }
    hist_v[hist_tete] = det ? v_lisse : NAN;
    hist_tete = (hist_tete + 1) % HIST_V;
    hist_total++;

    // passages : une suite de detections (au moins 4 trames, coupee par 0,5 s de calme)
    int ev = EV_RIEN;
    if (det) {
        if (!pass_actif) {
            pass_actif = true;
            pass_vmax = pass_somme = pass_signe = 0;
            pass_n = 0;
        }
        pass_trou = 0;
        pass_n++;
        pass_somme += fabsf(v_lisse);
        pass_signe += v_lisse;
        if (fabsf(v_lisse) > pass_vmax) pass_vmax = fabsf(v_lisse);
    } else if (pass_actif && ++pass_trou > 6) {
        pass_actif = false;
        if (pass_n >= 4) {
            Passage p = { pass_vmax, pass_somme / pass_n, pass_n * SAUT_S, (int8_t)(pass_signe >= 0 ? 1 : -1) };
            if (nb_passages >= MAX_PASSAGES) {
                memmove(passages, passages + 1, (MAX_PASSAGES - 1) * sizeof(Passage));
                nb_passages = MAX_PASSAGES - 1;
            }
            passages[nb_passages++] = p;
            nb_passages_total++;
            if (p.vmax > v_max_session) v_max_session = p.vmax;
            char l[96];
            snprintf(l, sizeof l, "passage,%.0f,%.3f,%.3f,%.2f,%d,%.1f,%s", fe, p.vmax, p.vmoy, p.duree, p.sens,
                     c, source ? "sim" : "reel");
            plat_csv(l);
            ev = EV_PASSAGE;
        }
    }
    return ev;
}

// ================================================================= tick
int tick(uint32_t ms)
{
    int ev = EV_RIEN;
    for (;;) {
        dsp::Echo e;
        bool a = false;
        plat_verrou();
        if (fe_queue != fe_tete) {
            e = f_echo[fe_queue];
            fe_queue = (fe_queue + 1) % FILE_E;
            a = true;
        }
        plat_deverrou();
        if (!a) break;
        int r = traiter_ping(e, ms);
        if (r != EV_RIEN) ev = r;
    }
    for (;;) {
        static dsp::Doppler d;
        bool a = false;
        plat_verrou();
        if (fd_queue != fd_tete) {
            d = f_dop[fd_queue];
            fd_queue = (fd_queue + 1) % FILE_D;
            a = true;
        }
        plat_deverrou();
        if (!a) break;
        int r = traiter_trame(d, ms);
        if (r != EV_RIEN) ev = r;
    }
    // cadences
    if (cad_t0 == 0) { cad_t0 = ms; cad_n0 = nb_pings; tps_t0 = ms; tps_n0 = nb_trames; }
    if (ms - cad_t0 >= 1000) {
        float inst = (nb_pings - cad_n0) * 1000.0f / (ms - cad_t0);
        cadence_lisse = cadence_lisse * 0.5f + inst * 0.5f;
        cad_t0 = ms; cad_n0 = nb_pings;
        float it = (nb_trames - tps_n0) * 1000.0f / (ms - tps_t0);
        tps_lisse = tps_lisse * 0.5f + it * 0.5f;
        tps_t0 = ms; tps_n0 = nb_trames;
    }
    if (!radar || mode != MODE_DOPPLER) v_ok = false;
    return ev;
}

}  // namespace son
