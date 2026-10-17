// dsp.cpp - voir dsp.h
#include "dsp.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace dsp {

static const int N = 8192;                 // N_ECHO == N_DOP
static const int LOG2N = 13;
static const double PI_D = 3.14159265358979323846;

static float *cosT = nullptr, *sinT = nullptr;   // N/2 facteurs de rotation
static uint16_t *inv_bits = nullptr;
static float *re = nullptr, *im = nullptr;       // travail
static float *chirp_re = nullptr, *chirp_im = nullptr;   // spectre du chirp
static float *fenetre = nullptr;              // Blackman-Harris (lobes secondaires -92 dB)
static int16_t *emission = nullptr;
static float tri[2048];
static uint32_t s_nb_fft = 0;

uint32_t nb_fft() { return s_nb_fft; }
const int16_t *chirp() { return emission; }

// ---------------------------------------------------------------- FFT
// FFT complexe sur place, rayon 2. inverse=true : sans division par N.
static void fft(float *xr, float *xi, bool inverse)
{
    for (int i = 0; i < N; i++) {
        int j = inv_bits[i];
        if (j > i) {
            float t = xr[i]; xr[i] = xr[j]; xr[j] = t;
            t = xi[i]; xi[i] = xi[j]; xi[j] = t;
        }
    }
    float sg = inverse ? 1.0f : -1.0f;
    for (int taille = 2; taille <= N; taille <<= 1) {
        int moitie = taille >> 1, pas = N / taille;
        for (int debut = 0; debut < N; debut += taille) {
            for (int k = 0; k < moitie; k++) {
                float wr = cosT[k * pas], wi = sg * sinT[k * pas];
                int a = debut + k, b = a + moitie;
                float tr = xr[b] * wr - xi[b] * wi;
                float ti = xr[b] * wi + xi[b] * wr;
                xr[b] = xr[a] - tr; xi[b] = xi[a] - ti;
                xr[a] += tr;        xi[a] += ti;
            }
        }
    }
    s_nb_fft++;
}

// ---------------------------------------------------------------- chirp
static float chirp_val(double t)
{
    const double T = CHIRP_N / (double)FS;
    if (t < 0 || t >= T) return 0;
    const double k = (CHIRP_F1 - CHIRP_F0) / T;
    // fenetre de Tukey (bords adoucis sur 15 %) : pas de "clic", pic de correlation propre
    double u = t / T, w = 1;
    if (u < 0.15) w = 0.5 - 0.5 * cos(PI_D * u / 0.15);
    else if (u > 0.85) w = 0.5 - 0.5 * cos(PI_D * (1 - u) / 0.15);
    return (float)(w * sin(2 * PI_D * (CHIRP_F0 * t + 0.5 * k * t * t)));
}

void init()
{
    if (re) return;
    cosT = (float *)malloc(N / 2 * sizeof(float));
    sinT = (float *)malloc(N / 2 * sizeof(float));
    inv_bits = (uint16_t *)malloc(N * sizeof(uint16_t));
    re = (float *)malloc(N * sizeof(float));
    im = (float *)malloc(N * sizeof(float));
    chirp_re = (float *)malloc(N * sizeof(float));
    chirp_im = (float *)malloc(N * sizeof(float));
    fenetre = (float *)malloc(N * sizeof(float));
    emission = (int16_t *)malloc(N * sizeof(int16_t));
    for (int k = 0; k < N / 2; k++) {
        cosT[k] = (float)cos(2 * PI_D * k / N);
        sinT[k] = (float)sin(2 * PI_D * k / N);
    }
    for (int i = 0; i < N; i++) {
        int r = 0;
        for (int b = 0; b < LOG2N; b++) if (i & (1 << b)) r |= 1 << (LOG2N - 1 - b);
        inv_bits[i] = (uint16_t)r;
        double x = 2 * PI_D * i / N;
        fenetre[i] = (float)(0.35875 - 0.48829 * cos(x) + 0.14128 * cos(2 * x) - 0.01168 * cos(3 * x));
    }
    memset(emission, 0, N * sizeof(int16_t));
    for (int i = 0; i < CHIRP_N; i++)
        emission[CHIRP_DEBUT + i] = (int16_t)(0.70f * 32767 * chirp_val(i / (double)FS));
    // spectre du chirp de reference (place au debut du tampon)
    for (int i = 0; i < N; i++) {
        chirp_re[i] = i < CHIRP_N ? chirp_val(i / (double)FS) : 0;
        chirp_im[i] = 0;
    }
    fft(chirp_re, chirp_im, false);
    s_nb_fft = 0;
}

static float mediane(float *v, int n)
{
    std::nth_element(v, v + n / 2, v + n);
    return v[n / 2];
}

// sommet d'une parabole passant par 3 points (decalage -0,5 .. +0,5)
static float parabole(float a, float b, float c)
{
    float d = a - 2 * b + c;
    if (fabsf(d) < 1e-12f) return 0;
    float x = 0.5f * (a - c) / d;
    return x < -0.5f ? -0.5f : (x > 0.5f ? 0.5f : x);
}

// ---------------------------------------------------------------- echo
void analyser_echo(const int16_t *rx, float d_att, float c_att, Echo &r)
{
    memset(&r, 0, sizeof r);
    r.d = d_att;
    for (int i = 0; i < N; i++) {
        if (rx[i] >= 32000 || rx[i] <= -32000) r.satur++;
        re[i] = rx[i] * (1.0f / 32768.0f);
        im[i] = 0;
    }
    fft(re, im, false);
    // filtre adapte (x conj du chirp) + signal analytique (frequences negatives a zero)
    for (int k = 0; k < N; k++) {
        if (k == 0 || k >= N / 2) { re[k] = im[k] = 0; continue; }
        float a = re[k], b = im[k], cr = chirp_re[k], ci = chirp_im[k];
        re[k] = 2 * (a * cr + b * ci);
        im[k] = 2 * (b * cr - a * ci);
    }
    fft(re, im, true);
    float *env = re;                       // enveloppe de la correlation
    for (int i = 0; i < N; i++) env[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);

    // plancher de bruit : mediane (1 point sur 4)
    int nt = 0;
    for (int i = 0; i < N && nt < 2048; i += 4) tri[nt++] = env[i];
    float bruit = mediane(tri, nt);
    if (bruit < 1e-9f) bruit = 1e-9f;

    // son direct : le pic le plus fort de la premiere moitie
    int id = 0;
    for (int i = 1; i < N / 2; i++) if (env[i] > env[id]) id = i;
    float vd = env[id];
    r.i_direct = id + (id > 0 && id < N - 1 ? parabole(env[id - 1], env[id], env[id + 1]) : 0);
    r.snr_direct_db = 20 * log10f(vd / bruit);

    // fenetre de recherche de l'echo, centree sur le retard attendu
    float tau = 2.0f * d_att / (c_att > 100 ? c_att : 343.0f) * FS;
    float demi = 0.18f * tau;
    if (demi < 30) demi = 30;
    int w0 = (int)(r.i_direct + tau - demi), w1 = (int)(r.i_direct + tau + demi);
    if (w0 < id + 40) w0 = id + 40;                // le pic direct deborde un peu
    if (w1 > N - 2) w1 = N - 2;

    int ie = -1;
    if (w1 > w0 + 2) {
        ie = w0;
        for (int i = w0; i <= w1; i++) if (env[i] > env[ie]) ie = i;
        float ve = env[ie];
        bool sommet = ie > w0 && ie < w1 && env[ie] >= env[ie - 1] && env[ie] >= env[ie + 1];
        r.snr_db = 20 * log10f(ve / bruit);
        r.amp_echo = ve / vd;
        r.i_echo = ie + parabole(env[ie - 1], env[ie], env[ie + 1]);
        r.dt_us = (r.i_echo - r.i_direct) * 1e6f / FS;
        r.ok = sommet && r.snr_db > 10.0f && r.snr_direct_db > 20.0f;
    }

    // echogramme pour l'ecran
    float duree = tau * 1.7f;
    if (duree < 0.004f * FS) duree = 0.004f * FS;
    float marge = 0.10f * duree;
    float debut = r.i_direct - marge, longueur = duree + marge;
    r.aff_ms = duree * 1000.0f / FS;
    for (int k = 0; k < ENV_AFF; k++) {
        int a = (int)(debut + longueur * k / ENV_AFF), b = (int)(debut + longueur * (k + 1) / ENV_AFF);
        if (b <= a) b = a + 1;
        float m = 0;
        for (int i = a; i < b; i++) if (i >= 0 && i < N && env[i] > m) m = env[i];
        float v = sqrtf(m / vd);                   // compression : l'echo reste visible
        r.env[k] = (uint8_t)(v > 1 ? 255 : v * 255);
    }
    r.f_echo = ie >= 0 ? (r.i_echo - debut) / longueur : -1;
    r.f_fen0 = (w0 - debut) / longueur;
    r.f_fen1 = (w1 - debut) / longueur;
}

// ---------------------------------------------------------------- doppler
float frequence_emise(float f0)
{
    return (float)(lroundf(f0 * N / FS) * (double)FS / N);
}

void analyser_doppler(const int16_t *rx, float f0, Doppler &r)
{
    memset(&r, 0, sizeof r);
    for (int i = 0; i < N; i++) {
        re[i] = rx[i] * (1.0f / 32768.0f) * fenetre[i];
        im[i] = 0;
    }
    fft(re, im, false);
    const float g = N * 0.35875f / 2;                     // sinus plein echelle -> 0 dB
    const float norm = 1.0f / (g * g);
    int k0 = (int)lroundf(f0 * N / FS);
    const int B = BANDE_DOP;
    float p[2 * B + 1];
    for (int j = -B; j <= B; j++) {
        int k = k0 + j;
        float v = (k > 0 && k < N / 2) ? (re[k] * re[k] + im[k] * im[k]) * norm : 0;
        p[j + B] = v;
        r.spectre_db[j + B] = 10 * log10f(v + 1e-13f);
    }
    r.porteuse_db = fmaxf(r.spectre_db[B], fmaxf(r.spectre_db[B - 1], r.spectre_db[B + 1]));
    int nt = 0;
    for (int j = -B; j <= B; j++) if (abs(j) > EXCLU_DOP) tri[nt++] = r.spectre_db[j + B];
    r.plancher_db = mediane(tri, nt);
    float seuil_db = r.plancher_db + 10.0f;
    float pseuil = powf(10.0f, seuil_db / 10.0f);

    // energie au-dessus du seuil de chaque cote de la porteuse
    float E[2] = { 0, 0 }, M[2] = { 0, 0 }, pic[2] = { 0, 0 };
    int jpic[2] = { 0, 0 }, nb[2] = { 0, 0 };
    for (int j = EXCLU_DOP + 1; j <= B; j++) {
        for (int s = 0; s < 2; s++) {
            int jj = s ? j : -j;
            float v = p[jj + B];
            if (v > pseuil) { E[s] += v - pseuil; M[s] += (v - pseuil) * jj; nb[s]++; }
            if (v > pic[s]) { pic[s] = v; jpic[s] = jj; }
        }
    }
    int dom = E[1] >= E[0] ? 1 : 0;                // 1 = au-dessus de f0 (approche)
    float snr = 10 * log10f(pic[dom] + 1e-13f) - r.plancher_db;
    r.snr_db = snr;
    r.detect = nb[dom] >= 2 && E[dom] > 1.5f * E[1 - dom] && snr > 13.0f;
    if (r.detect) {
        r.df_hz = M[dom] / E[dom] * FS / N;
        int j = jpic[dom];
        float d = 0;
        if (abs(j) > 0 && abs(j) < B) d = parabole(r.spectre_db[j - 1 + B], r.spectre_db[j + B], r.spectre_db[j + 1 + B]);
        r.df_pic_hz = (j + d) * FS / N;
    }
    for (int c = 0; c < NB_SPECTRE; c++) {
        int j = c - NB_SPECTRE / 2;
        float v = r.spectre_db[j + B];
        float u = (v - r.plancher_db - 4) / 40.0f;
        r.colonne[c] = (uint8_t)(u < 0 ? 0 : (u > 1 ? 255 : u * 255));
    }
}

// ---------------------------------------------------------------- generation
void tonalite(int16_t *tx, int n, float f0, float amp, double &phase)
{
    double w = 2 * PI_D * f0 / FS;
    for (int i = 0; i < n; i++) {
        tx[i] = (int16_t)(amp * 32767 * sin(phase));
        phase += w;
        if (phase > 2 * PI_D) phase -= 2 * PI_D;
    }
}

// ---------------------------------------------------------------- simulation
static uint32_t xs(uint32_t &g) { g ^= g << 13; g ^= g >> 17; g ^= g << 5; return g; }
static float u01(uint32_t &g) { return (xs(g) >> 8) * (1.0f / 16777216.0f); }
static float gauss(uint32_t &g)
{
    float u = u01(g) + 1e-7f, v = u01(g);
    return sqrtf(-2.0f * logf(u)) * cosf(6.2831853f * v);
}

static void ajouter_chirp(float *acc, double t_arrivee, float amp)
{
    int i0 = (int)floor(t_arrivee * FS);
    for (int i = i0; i <= i0 + CHIRP_N + 1; i++) {
        if (i < 0 || i >= N) continue;
        acc[i] += amp * chirp_val(i / (double)FS - t_arrivee);
    }
}

void synth_echo(int16_t *rx, float d, float c, uint32_t &g)
{
    float *acc = im;                               // tampon de travail libre ici
    for (int i = 0; i < N; i++) acc[i] = 0;
    // retard de la chaine audio (tampons DMA) + petite gigue
    double t0 = (CHIRP_DEBUT + 960 + 12 * gauss(g)) / (double)FS;
    double hp_micro = 0.035;                       // haut-parleur -> micro sur la carte
    ajouter_chirp(acc, t0 + hp_micro / c, 0.62f);
    // echo du mur
    float a_mur = 0.42f * 0.30f / fmaxf(2 * d, 0.35f) * (1 + 0.06f * gauss(g));
    ajouter_chirp(acc, t0 + (2 * d) / c, a_mur);
    // table sous la carte, plafond, mur lateral : echos parasites
    ajouter_chirp(acc, t0 + 2 * 0.22 / c, 0.10f);
    ajouter_chirp(acc, t0 + 2 * 1.35 / c, 0.035f);
    double lat = 2 * sqrt(d * d + 1.6 * 1.6);
    ajouter_chirp(acc, t0 + lat / c, a_mur * 0.35f);
    // reverberation de la piece + bruit de fond + ronflement secteur
    int id0 = (int)(t0 * FS);
    for (int i = 0; i < N; i++) {
        float v = acc[i];
        if (i > id0) v += 0.010f * expf(-(i - id0) / (0.045f * FS)) * gauss(g);
        v += 0.0045f * gauss(g) + 0.002f * sinf(6.2831853f * 50.0f * i / FS);
        int s = (int)lroundf(v * 32767);
        rx[i] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
    }
}

void synth_doppler(int16_t *rx, int n, float f0, float v, float dist, float c, EtatSynthDop &e)
{
    double w_dir = 2 * PI_D * f0 / FS;
    // reflexion sur un objet mobile : f' = f0 (c + v) / (c - v)
    double f_corps = f0 * (c + v) / (c - v);
    float a_corps = 0.020f / powf(fmaxf(dist, 0.6f), 1.5f);
    for (int i = 0; i < n; i++) {
        // bras et jambes : vitesse qui oscille autour de celle du corps (micro-Doppler)
        double t = e.t;
        e.t += 1.0 / FS;
        double v_bras = v * (1.0 + 0.7 * sin(2 * PI_D * 1.9 * t));
        double f_bras = f0 * (c + v_bras) / (c - v_bras);
        e.ph_corps += 2 * PI_D * f_corps / FS;
        e.ph_bras += 2 * PI_D * f_bras / FS;
        float s = 0.45f * (float)sin(e.ph_dir)
                + (fabsf(v) > 0.02f ? a_corps * (float)sin(e.ph_corps) + 0.35f * a_corps * (float)sin(e.ph_bras) : 0)
                + 0.0035f * gauss(e.graine);
        e.ph_dir += w_dir;
        int q = (int)lroundf(s * 32767);
        rx[i] = (int16_t)(q > 32767 ? 32767 : (q < -32768 ? -32768 : q));
    }
    // garde les phases bornees
    e.ph_dir = fmod(e.ph_dir, 2 * PI_D);
    e.ph_corps = fmod(e.ph_corps, 2 * PI_D);
    e.ph_bras = fmod(e.ph_bras, 2 * PI_D);
}

}  // namespace dsp
