#!/usr/bin/env python3
"""
Genere polices.h / polices.cpp pour lumiere_console et son_console (polices lissees 4 bits).

Police : Inter (Rasmus Andersson, licence SIL Open Font License 1.1).
Usage  : python3 generer_polices.py <dossier_des_otf_inter> <dossier_sortie>
Chaque glyphe est stocke en 4 bits par pixel (16 niveaux d'opacite), ce qui
donne un texte lisse sur l'ecran. Les chiffres ont tous la meme largeur
(chiffres "tabulaires") : un nombre qui change ne tremble pas.
"""
import sys
import os
from PIL import Image, ImageDraw, ImageFont

BASE = ("".join(chr(c) for c in range(32, 127))
        + "àâäçéèêëîïôöùûüÀÂÇÉÈÊÔÙ°±×µμσ·…⁶²³√≈→←‹›✓▲▼●−–—Δλ½")
CHIFFRES = "0123456789,.-+% ×−"

POLICES = [
    # nom C,         fichier,                 taille px, jeu de caracteres
    ("POLICE_11",   "Inter-Medium.otf",        11, BASE),
    ("POLICE_13",   "Inter-SemiBold.otf",      13, BASE),
    ("POLICE_16",   "Inter-Bold.otf",          16, BASE),
    ("POLICE_22",   "InterDisplay-Bold.otf",   22, BASE),
    ("POLICE_30",   "InterDisplay-Bold.otf",   30, "ABCDEFGHIJKLMNOPQRSTUVWXYZÈÉ "),
    ("POLICE_40",   "InterDisplay-Bold.otf",   40, CHIFFRES),
]


def rendre(police, ch, taille):
    pad = taille
    img = Image.new("L", (taille * 3, taille * 3), 0)
    d = ImageDraw.Draw(img)
    base_y = pad + taille
    d.text((pad, base_y), ch, font=police, fill=255, anchor="ls")
    bbox = img.getbbox()
    avance = police.getlength(ch)
    if bbox is None:
        return None, 0, 0, 0, 0, avance
    x0, y0, x1, y1 = bbox
    return img.crop(bbox), x1 - x0, y1 - y0, x0 - pad, y0 - base_y, avance


def generer(dossier_otf, sortie):
    h = ["// polices.h - GENERE par outils/generer_polices.py, ne pas modifier",
         "// Police Inter (c) Rasmus Andersson - SIL Open Font License 1.1",
         "#pragma once", "#include <stdint.h>", "",
         "struct GlypheAA { uint16_t code; uint8_t l, h; int8_t dx, dy; uint8_t avance; uint32_t pos; };",
         "struct PoliceAA { const uint8_t *bits; const GlypheAA *glyphes; uint16_t nb;",
         "                  int8_t ascendante; int8_t descendante; uint8_t interligne; };", ""]
    c = ['// polices.cpp - GENERE par outils/generer_polices.py, ne pas modifier',
         '// Police Inter (c) Rasmus Andersson - SIL Open Font License 1.1',
         '#include "polices.h"', ""]
    total = 0
    for nom, fichier, taille, jeu in POLICES:
        police = ImageFont.truetype(os.path.join(dossier_otf, fichier), taille)
        asc, desc = police.getmetrics()
        jeu = sorted(set(jeu), key=ord)
        rendus = {ch: rendre(police, ch, taille) for ch in jeu}
        av_chiffre = max([rendus[ch][5] for ch in "0123456789" if ch in rendus] or [0])
        octets = bytearray()
        glyphes = []
        for ch in jeu:
            img, l, hh, dx, dy, av = rendus[ch]
            if ch in "0123456789":                # chiffres tabulaires
                dx += round((av_chiffre - av) / 2)
                av = av_chiffre
            pos = len(octets)
            if img is not None:
                px = list(img.tobytes())
                for i in range(0, len(px), 2):
                    a = min(15, (px[i] + 8) >> 4)
                    b = min(15, (px[i + 1] + 8) >> 4) if i + 1 < len(px) else 0
                    octets.append((a << 4) | b)
            glyphes.append((ord(ch), l, hh, dx, dy, int(round(av)), pos))
        total += len(octets) + len(glyphes) * 12
        b = nom.lower()
        c.append(f"static const uint8_t {b}_bits[{len(octets)}] = {{")
        for i in range(0, len(octets), 24):
            c.append("  " + ",".join(f"0x{v:02X}" for v in octets[i:i + 24]) + ",")
        c.append("};")
        c.append(f"static const GlypheAA {b}_glyphes[{len(glyphes)}] = {{")
        for g in glyphes:
            c.append("  {%d,%d,%d,%d,%d,%d,%d}," % g)
        c.append("};")
        c.append(f"const PoliceAA {nom} = {{ {b}_bits, {b}_glyphes, {len(glyphes)}, {asc}, {desc}, {asc + desc} }};")
        c.append("")
        h.append(f"extern const PoliceAA {nom};   // {fichier} {taille} px")
    open(os.path.join(sortie, "polices.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(sortie, "polices.cpp"), "w").write("\n".join(c) + "\n")
    print(f"polices generees : {total / 1024:.1f} Ko")


if __name__ == "__main__":
    generer(sys.argv[1], sys.argv[2])
