// moteur.cpp - voir moteur.h
#include "moteur.h"
#include "dsp.h"
#include "son.h"
#include "plateforme.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace mot {

static int16_t *rx = nullptr;             // enregistrement d'un ping / fenetre Doppler
static uint32_t prochain_ping = 0;
static uint32_t graine = 0xC0FFEE11;

// Doppler
static bool tonalite_on = false;
static float f_en_cours = 0;
static uint32_t sim_t_ms = 0;
static double sim_t = 0;                  // horloge du scenario simule (s)
static float sim_dist = 4.0f;
static float sim_v = 0;
static dsp::EtatSynthDop etat_sd = { 0, 0, 0, 0, 0x1234567 };

void init()
{
    dsp::init();
    if (!rx) rx = (int16_t *)malloc(dsp::N_ECHO * sizeof(int16_t));
    memset(rx, 0, dsp::N_ECHO * sizeof(int16_t));
}

float v_simulee() { return sim_v; }

// petite erreur de placement de la carte (comme un eleve avec un metre ruban)
static float erreur_placement(float d)
{
    return 0.006f * sinf(d * 7.3f + 1.1f);
}

// scenario simule : une personne qui va et vient devant la carte (boucle de 17 s)
static float vitesse_scenario(double t)
{
    struct Seg { float t0, t1, v; };
    static const Seg segs[] = {
        { 0.0f, 1.5f, 0.0f },     // immobile
        { 1.5f, 4.6f, 1.30f },    // marche vers la carte
        { 4.6f, 6.0f, 0.0f },
        { 6.0f, 9.2f, -1.05f },   // repart en marchant
        { 9.2f, 10.4f, 0.0f },
        { 10.4f, 11.9f, 3.10f },  // arrive en courant
        { 11.9f, 13.0f, 0.0f },
        { 13.0f, 15.4f, -1.70f }, // repart d'un pas rapide
        { 15.4f, 17.0f, 0.0f },
    };
    double u = fmod(t, 17.0);
    for (const Seg &s : segs) {
        if (u >= s.t0 && u < s.t1) {
            float a = (float)(u - s.t0), b = (float)(s.t1 - u), r = 0.45f;
            float k = fminf(1.0f, fminf(a / r, b / r));          // demarrage et arret progressifs
            k = k * k * (3 - 2 * k);
            return s.v * k;
        }
    }
    return 0;
}

static void arreter_tonalite()
{
    if (tonalite_on) {
        plat_audio_tonalite(0, 0);
        tonalite_on = false;
    }
}

static uint32_t etape_echo(uint32_t ms)
{
    arreter_tonalite();
    uint32_t periode = son::en_collecte() ? 230 : 700;
    if ((int32_t)(ms - prochain_ping) < 0) return prochain_ping - ms;
    prochain_ping = ms + periode;

    float d = son::en_collecte() ? son::collecte_d() : son::d_visee;
    float c_att = (float)son::c_theorie(son::temperature);
    if (son::source == son::SRC_SIM) {
        // l'air de la piece est un peu plus chaud que la valeur reglee
        float c_vrai = (float)son::c_theorie(son::temperature + 0.8f);
        dsp::synth_echo(rx, d + erreur_placement(d), c_vrai, graine);
    } else {
        if (!plat_audio_ok()) return 200;
        if (!plat_audio_ping(dsp::chirp(), rx, dsp::N_ECHO)) return 100;
    }
    dsp::Echo e;
    dsp::analyser_echo(rx, d, c_att, e);
    son::ajouter_ping(e);
    return 5;
}

static void decaler_et_analyser()
{
    dsp::Doppler r;
    dsp::analyser_doppler(rx, f_en_cours, r);
    son::ajouter_trame(r);
}

static uint32_t etape_doppler(uint32_t ms)
{
    const int N = dsp::N_DOP, S = dsp::SAUT_DOP;
    float f = son::f0_emise();
    if (son::source == son::SRC_SIM) {
        arreter_tonalite();
        if (f != f_en_cours) { f_en_cours = f; sim_t_ms = ms; }
        if (sim_t_ms == 0) sim_t_ms = ms;
        int faits = 0;
        while ((int32_t)(ms - sim_t_ms) >= (int32_t)(S * 1000 / dsp::FS) && faits < 3) {
            sim_t_ms += S * 1000 / dsp::FS;
            float c = (float)son::c_theorie(son::temperature + 0.8f);
            sim_v = vitesse_scenario(sim_t);
            memmove(rx, rx + S, (N - S) * sizeof(int16_t));
            dsp::synth_doppler(rx + N - S, S, f, sim_v, sim_dist, c, etat_sd);
            sim_t += (double)S / dsp::FS;
            sim_dist -= sim_v * S / (float)dsp::FS;
            if (sim_dist < 0.7f) sim_dist = 0.7f;
            if (sim_dist > 6.0f) sim_dist = 6.0f;
            decaler_et_analyser();
            faits++;
        }
        if (faits == 3) sim_t_ms = ms;               // trop de retard : on rattrape
        return 20;
    }
    if (!plat_audio_ok()) return 200;
    if (!tonalite_on || f != f_en_cours) {
        f_en_cours = f;
        plat_audio_tonalite(f, 0.40f);   // plafonne selon le volume par la plateforme
        tonalite_on = true;
        memset(rx, 0, N * sizeof(int16_t));
    }
    memmove(rx, rx + S, (N - S) * sizeof(int16_t));
    if (!plat_audio_lire(rx + N - S, S)) return 50;
    decaler_et_analyser();
    return 0;
}

uint32_t etape(uint32_t ms)
{
    if (son::mode == son::MODE_DOPPLER) {
        if (son::radar) return etape_doppler(ms);
        arreter_tonalite();
        sim_t_ms = 0;
        sim_v = 0;
        return 50;
    }
    return etape_echo(ms);
}

}  // namespace mot
