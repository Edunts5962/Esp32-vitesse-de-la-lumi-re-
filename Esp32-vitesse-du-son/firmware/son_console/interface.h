// interface.h - ecrans animes et tactile de la console "Vitesse du son"
#pragma once
#include <stdint.h>

#define SON_VERSION "v1.1"

namespace ui {

// fond : tampon L*H facultatif (PSRAM) pour le fond pre-calcule, ou nullptr
void init(uint16_t *fond);

// a chaque image : etat du doigt (coordonnees ecran 320x240)
void toucher(bool appui, int x, int y);

// dessine une image complete dans gfx::fb
void image(uint32_t ms);

}  // namespace ui
