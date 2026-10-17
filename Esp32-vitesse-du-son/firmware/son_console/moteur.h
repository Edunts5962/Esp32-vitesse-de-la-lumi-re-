// moteur.h - la boucle de mesure (pings d'echo, analyse Doppler continue).
//
// Sur la carte : appele en boucle par la tache audio (coeur 0) ; les appels
// au codec sont bloquants. Sur le PC : appele a chaque image du simulateur.
// En mode simulation, le son est fabrique (dsp::synth_*) puis analyse par
// exactement le meme code que le vrai son du micro.
#pragma once
#include <stdint.h>

namespace mot {

void init();

// fait au plus une action ; renvoie le temps (ms) avant le prochain appel utile
uint32_t etape(uint32_t ms);

// vitesse de la personne simulee (pour verifier l'affichage)
float v_simulee();

}  // namespace mot
