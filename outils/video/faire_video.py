#!/usr/bin/env python3
"""
faire_video.py - video de presentation du projet (1920x1080, 30 i/s, ~5 min).

Dessin vectoriel lisse avec pycairo, encodage H.264 avec ffmpeg.
Les sequences "Demonstration" utilisent les images du simulateur
(outils/simulateur/sim_tuto.cpp : meme code d'interface que la carte).

  pip install pycairo numpy
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> [n_scene ...]
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> --assembler
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> --images   (vues fixes)

  Avec la voix off (narration.py d'abord) : ajouter  --voix <dossier_narration>
  aux commandes ci-dessus, puis  --final  (piste son + sous-titres + MP4 final).
"""
import math
import os
import random
import subprocess
import sys

import cairo
import numpy as np

W, H, FPS = 1920, 1080, 30
POLICE = "Inter"
POLICE_TITRE = "Inter Display"

# ------------------------------------------------------------------ palette
def c(r, g, b): return (r / 255, g / 255, b / 255)
FOND_HAUT, FOND_BAS = c(4, 6, 18), c(16, 12, 42)
PANNEAU, PANNEAU2, BORD = c(16, 22, 52), c(28, 36, 80), c(52, 66, 130)
TEXTE, TEXTE2, TEXTE3 = c(236, 242, 255), c(156, 168, 210), c(96, 108, 156)
CYAN, BLEU, VIOLET, MAGENTA = c(40, 228, 255), c(70, 120, 255), c(140, 92, 255), c(255, 64, 200)
JAUNE, ORANGE, ROUGE, VERT = c(255, 212, 64), c(255, 140, 60), c(255, 78, 106), c(60, 255, 156)
BLANC = (1, 1, 1)

# ------------------------------------------------------------------ outils
def sat(x): return 0.0 if x < 0 else (1.0 if x > 1 else x)
def sortie(x): x = sat(x); return 1 - (1 - x) ** 3
def doux(x): x = sat(x); return x * x * (3 - 2 * x)
def app(t, t0, d=0.6): return sortie((t - t0) / d)
def disp(t, t0, d=0.5): return 1 - sortie((t - t0) / d)
def mix(a, b, k): return tuple(a[i] + (b[i] - a[i]) * k for i in range(3))


def fmt(v, dec):
    s = f"{v:.{dec}f}".replace(".", ",")
    return s


def grouper(n):
    s = f"{int(n):d}"
    out = ""
    for i, ch in enumerate(s):
        if i and (len(s) - i) % 3 == 0:
            out += " "
        out += ch
    return out


def police(ctx, taille, gras=True, famille=POLICE):
    ctx.select_font_face(famille, cairo.FONT_SLANT_NORMAL,
                         cairo.FONT_WEIGHT_BOLD if gras else cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(taille)


def largeur(ctx, s, taille, gras=True, famille=POLICE):
    police(ctx, taille, gras, famille)
    return ctx.text_extents(s).x_advance


def texte(ctx, s, x, y, taille, coul=TEXTE, a=1.0, gras=True, align="g", famille=POLICE):
    """y = ligne de base ; align g/c/d."""
    if a <= 0.003 or not s:
        return 0
    police(ctx, taille, gras, famille)
    l = ctx.text_extents(s).x_advance
    if align == "c":
        x -= l / 2
    elif align == "d":
        x -= l
    ctx.set_source_rgba(*coul, a)
    ctx.move_to(x, y)
    ctx.show_text(s)
    return l


def texte_bloc(ctx, s, x, y, larg, taille, coul=TEXTE2, a=1.0, gras=False, interligne=1.35, align="g"):
    """Texte coupe en lignes ; renvoie le y de la ligne suivante."""
    police(ctx, taille, gras)
    mots, ligne, lignes = s.split(), "", []
    for m in mots:
        essai = (ligne + " " + m).strip()
        if ctx.text_extents(essai).x_advance > larg and ligne:
            lignes.append(ligne)
            ligne = m
        else:
            ligne = essai
    if ligne:
        lignes.append(ligne)
    for li in lignes:
        xx = x + larg / 2 if align == "c" else x
        texte(ctx, li, xx, y, taille, coul, a, gras, align)
        y += taille * interligne
    return y


def chiffres(ctx, s, x, y, taille, coul, a=1.0, align="c", famille=POLICE_TITRE):
    """Nombre a chasse fixe (ne tremble pas pendant un comptage)."""
    police(ctx, taille, True, famille)
    cell = ctx.text_extents("0").x_advance
    larg = sum(cell if ch.isdigit() else ctx.text_extents(ch).x_advance for ch in s)
    if align == "c":
        x -= larg / 2
    elif align == "d":
        x -= larg
    ctx.set_source_rgba(*coul, a)
    for ch in s:
        w = cell if ch.isdigit() else ctx.text_extents(ch).x_advance
        e = ctx.text_extents(ch)
        ctx.move_to(x + (w - e.x_advance) / 2, y)
        ctx.show_text(ch)
        x += w
    return larg


def rrect(ctx, x, y, w, h, r):
    r = min(r, w / 2, h / 2)
    ctx.new_sub_path()
    ctx.arc(x + w - r, y + r, r, -math.pi / 2, 0)
    ctx.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
    ctx.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
    ctx.arc(x + r, y + r, r, math.pi, 1.5 * math.pi)
    ctx.close_path()


def panneau(ctx, x, y, w, h, a=1.0, r=22, bord=BORD):
    if a <= 0:
        return
    rrect(ctx, x, y, w, h, r)
    ctx.set_source_rgba(*PANNEAU, 0.78 * a)
    ctx.fill_preserve()
    ctx.set_source_rgba(*bord, 0.7 * a)
    ctx.set_line_width(1.5)
    ctx.stroke()


def halo(ctx, x, y, r, coul, a=1.0):
    if a <= 0 or r <= 0:
        return
    g = cairo.RadialGradient(x, y, 0, x, y, r)
    g.add_color_stop_rgba(0, *coul, 0.55 * a)
    g.add_color_stop_rgba(0.35, *coul, 0.22 * a)
    g.add_color_stop_rgba(1, *coul, 0)
    ctx.save()
    ctx.set_operator(cairo.OPERATOR_ADD)
    ctx.set_source(g)
    ctx.rectangle(x - r, y - r, 2 * r, 2 * r)
    ctx.fill()
    ctx.restore()


def disque(ctx, x, y, r, coul, a=1.0):
    ctx.arc(x, y, r, 0, 2 * math.pi)
    ctx.set_source_rgba(*coul, a)
    ctx.fill()


def trait(ctx, x0, y0, x1, y1, ep, coul, a=1.0, pointille=None):
    ctx.save()
    if pointille:
        ctx.set_dash(pointille)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_width(ep)
    ctx.set_source_rgba(*coul, a)
    ctx.move_to(x0, y0)
    ctx.line_to(x1, y1)
    ctx.stroke()
    ctx.restore()


def fleche(ctx, x0, y0, x1, y1, ep, coul, a=1.0, tete=16):
    trait(ctx, x0, y0, x1, y1, ep, coul, a)
    ang = math.atan2(y1 - y0, x1 - x0)
    ctx.move_to(x1, y1)
    ctx.line_to(x1 - tete * math.cos(ang - 0.4), y1 - tete * math.sin(ang - 0.4))
    ctx.line_to(x1 - tete * math.cos(ang + 0.4), y1 - tete * math.sin(ang + 0.4))
    ctx.close_path()
    ctx.set_source_rgba(*coul, a)
    ctx.fill()


def photon(ctx, x, y, coul, a=1.0, dirx=0.0, taille=1.0):
    for q in range(8, 0, -1):
        disque(ctx, x - dirx * q * 9 * taille, y, (7 - q * 0.6) * taille, coul, a * (0.55 - q * 0.06))
    halo(ctx, x, y, 70 * taille, coul, a)
    disque(ctx, x, y, 7 * taille, (1, 1, 0.92), a)


def pastille(ctx, s, x, y, taille, coul, a=1.0, plein=False, align="c"):
    police(ctx, taille, True)
    l = ctx.text_extents(s).x_advance
    pad = taille * 0.7
    w, h = l + 2 * pad, taille * 1.7
    if align == "c":
        x0 = x - w / 2
    elif align == "d":
        x0 = x - w
    else:
        x0 = x
    rrect(ctx, x0, y - h / 2, w, h, h / 2)
    if plein:
        ctx.set_source_rgba(*coul, 0.9 * a)
        ctx.fill()
        texte(ctx, s, x0 + pad, y + taille * 0.36, taille, c(6, 10, 30), a)
    else:
        ctx.set_source_rgba(*PANNEAU2, 0.9 * a)
        ctx.fill_preserve()
        ctx.set_source_rgba(*coul, 0.9 * a)
        ctx.set_line_width(2)
        ctx.stroke()
        texte(ctx, s, x0 + pad, y + taille * 0.36, taille, coul, a)
    return w


def onde(ctx, x0, x1, y, amp, lam, phase, coul, a=1.0, ep=3):
    ctx.save()
    ctx.set_line_width(ep)
    ctx.set_source_rgba(*coul, a)
    n = max(2, int((x1 - x0) / 4))
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i / n
        env = math.sin(math.pi * i / n)
        yy = y + amp * env * math.sin((x - x0) / lam * 2 * math.pi - phase)
        if i == 0:
            ctx.move_to(x, yy)
        else:
            ctx.line_to(x, yy)
    ctx.stroke()
    ctx.restore()


def carte_esp(ctx, x, y, w, h, nom, coul, a=1.0, led=0.0, ecran=None):
    """Carte electronique stylisee ; ecran = (surface, echelle) facultatif."""
    if a <= 0:
        return
    halo(ctx, x + w / 2, y + h / 2, max(w, h) * 0.8, coul, 0.25 * a)
    rrect(ctx, x, y, w, h, 18)
    g = cairo.LinearGradient(x, y, x, y + h)
    g.add_color_stop_rgba(0, 0.13, 0.17, 0.33, a)
    g.add_color_stop_rgba(1, 0.05, 0.07, 0.16, a)
    ctx.set_source(g)
    ctx.fill_preserve()
    ctx.set_source_rgba(*coul, 0.85 * a)
    ctx.set_line_width(2.5)
    ctx.stroke()
    for (hx, hy) in ((x + 14, y + 14), (x + w - 14, y + 14), (x + 14, y + h - 14), (x + w - 14, y + h - 14)):
        ctx.arc(hx, hy, 6, 0, 2 * math.pi)
        ctx.set_source_rgba(0.6, 0.6, 0.65, 0.6 * a)
        ctx.set_line_width(2)
        ctx.stroke()
    if ecran is not None:
        surf, ech = ecran
        sw, sh = 320 * ech, 240 * ech
        ex, ey = x + (w - sw) / 2, y + 22
        rrect(ctx, ex - 10, ey - 10, sw + 20, sh + 20, 10)
        ctx.set_source_rgba(0.02, 0.02, 0.05, a)
        ctx.fill()
        ctx.save()
        ctx.translate(ex, ey)
        ctx.scale(ech, ech)
        ctx.set_source_surface(surf, 0, 0)
        ctx.get_source().set_filter(cairo.FILTER_GOOD)
        ctx.paint_with_alpha(a)
        ctx.restore()
        mx, my = x + w / 2 - 50, y + h - 62
    else:
        mx, my = x + w / 2 - 50, y + h * 0.42
    # module ESP32 (blindage metallique + antenne gravee)
    rrect(ctx, mx, my, 100, 46, 6)
    gm = cairo.LinearGradient(mx, my, mx, my + 46)
    gm.add_color_stop_rgba(0, 0.75, 0.77, 0.82, a)
    gm.add_color_stop_rgba(1, 0.45, 0.47, 0.53, a)
    ctx.set_source(gm)
    ctx.fill()
    ctx.save()
    ctx.set_source_rgba(*JAUNE, 0.85 * a)
    ctx.set_line_width(2.5)
    ax, ay = mx + 8, my - 6
    ctx.move_to(ax, ay)
    for k in range(6):
        ctx.line_to(ax + k * 14 + 7, ay - 14 if k % 2 == 0 else ay)
    ctx.stroke()
    ctx.restore()
    texte(ctx, "ESP32-S3", mx + 50, my + 30, 15, c(30, 34, 50), a, True, "c")
    if led > 0.01:
        halo(ctx, x + w - 34, y + h - 34, 26, VERT, led * a)
    disque(ctx, x + w - 34, y + h - 34, 5, VERT if led > 0.1 else c(30, 70, 50), a)
    texte(ctx, nom, x + 30, y + h - 26, 22, coul, a, True)


# ------------------------------------------------------------------ fond
_fond = None
_etoiles = None


def preparer_fond():
    global _fond, _etoiles
    _fond = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    ctx = cairo.Context(_fond)
    g = cairo.LinearGradient(0, 0, 0, H)
    g.add_color_stop_rgb(0, *FOND_HAUT)
    g.add_color_stop_rgb(1, FOND_BAS[0], FOND_BAS[1], FOND_BAS[2])
    ctx.set_source(g)
    ctx.paint()
    halo(ctx, 1500, 230, 1000, VIOLET, 0.35)
    halo(ctx, 260, 900, 900, BLEU, 0.28)
    halo(ctx, 1000, 1200, 700, MAGENTA, 0.14)
    rnd = random.Random(3)
    _etoiles = [(rnd.random() * W, rnd.random() * H, 4 + rnd.random() * 22, rnd.random() * 6.3,
                 0.3 + rnd.random() * 0.7, 0.6 + rnd.random() * 1.4) for _ in range(240)]


def fond(ctx, T):
    ctx.set_source_surface(_fond, 0, 0)
    ctx.paint()
    for (x, y, v, ph, lum, r) in _etoiles:
        xx = (x - v * T) % W
        a = lum * (0.55 + 0.45 * math.sin(T * 1.6 + ph))
        disque(ctx, xx, y, r, TEXTE, a * 0.85)
        if r > 1.6:
            halo(ctx, xx, y, 10, CYAN, a * 0.5)


# ------------------------------------------------------------------ ecrans du simulateur
DOSSIER_SIM = "."
_segments = {}


def charger_segment(nom):
    if nom not in _segments:
        brut = np.fromfile(os.path.join(DOSSIER_SIM, nom + ".rgb"), dtype=np.uint8)
        n = len(brut) // (320 * 240 * 3)
        img = brut[: n * 320 * 240 * 3].reshape(n, 240, 320, 3)
        appuis = {}
        chemin = os.path.join(DOSSIER_SIM, nom + ".appuis")
        if os.path.exists(chemin):
            for ligne in open(chemin):
                f, x, y = map(int, ligne.split())
                appuis[f] = (x, y)
        _segments[nom] = (img, appuis)
    return _segments[nom]


def surface_rgb(rgb):
    h, w, _ = rgb.shape
    bgra = np.empty((h, w, 4), dtype=np.uint8)
    bgra[..., 0] = rgb[..., 2]
    bgra[..., 1] = rgb[..., 1]
    bgra[..., 2] = rgb[..., 0]
    bgra[..., 3] = 255
    return cairo.ImageSurface.create_for_data(bgra, cairo.FORMAT_ARGB32, w, h), bgra


_captures = {}


def capture(nom):
    if nom not in _captures:
        with open(os.path.join(DOSSIER_SIM, nom + ".ppm"), "rb") as f:
            f.readline(); f.readline(); f.readline()
            rgb = np.frombuffer(f.read(), dtype=np.uint8).reshape(240, 320, 3)
        _captures[nom] = surface_rgb(rgb)
    return _captures[nom][0]


# ------------------------------------------------------------------ habillage commun
CHAPITRES = []


def chapitre(ctx, t, D, num, nom):
    a = app(t, 0.2, 0.5) * (1 - app(t, D - 0.6, 0.4))
    texte(ctx, f"{num:02d}", 60, 70, 22, CYAN, a, True)
    texte(ctx, nom, 100, 70, 22, TEXTE2, a, True)
    trait(ctx, 60, 84, 60 + 180 * app(t, 0.3, 0.8), 84, 2, CYAN, 0.6 * a)


def barre_progression(ctx, T, total):
    ctx.set_source_rgba(*PANNEAU2, 0.9)
    ctx.rectangle(0, H - 5, W, 5)
    ctx.fill()
    g = cairo.LinearGradient(0, 0, W, 0)
    g.add_color_stop_rgb(0, *CYAN)
    g.add_color_stop_rgb(1, *VIOLET)
    ctx.set_source(g)
    ctx.rectangle(0, H - 5, W * T / total, 5)
    ctx.fill()


def titre(ctx, s, t, t0=0.3, y=170, taille=58):
    a = app(t, t0, 0.7)
    texte(ctx, s, W / 2, y + 18 * (1 - a), taille, TEXTE, a, True, "c", POLICE_TITRE)


# ================================================================== SCENES
def s_intro(ctx, t, D):
    bx = -200 + 2500 * sortie(t / 2.4)
    ab = 1 if t < 2.4 else 0.35 + 0.65 * math.exp(-(t - 2.4))
    g = cairo.LinearGradient(0, 0, bx, 0)
    g.add_color_stop_rgba(0, *BLEU, 0)
    g.add_color_stop_rgba(1, *CYAN, 0.9 * ab)
    ctx.set_source(g)
    ctx.rectangle(0, 538, max(0, min(bx, W)), 4)
    ctx.fill()
    if t < 2.6:
        halo(ctx, bx, 540, 160, CYAN, 1)
        halo(ctx, bx, 540, 40, BLANC, 0.9)
    s = "LA VITESSE DE LA LUMIÈRE"
    police(ctx, 108, True, POLICE_TITRE)
    lt = ctx.text_extents(s).x_advance
    x = W / 2 - lt / 2
    halo(ctx, W / 2, 440, 700, VIOLET, 0.35 * app(t, 1.6, 1.5))
    for i, ch in enumerate(s):
        a = app(t, 1.6 + i * 0.045, 0.45)
        pref = ctx.text_extents(s[:i]).x_advance if i else 0
        texte(ctx, ch, x + pref, 470 + 26 * (1 - a), 108, TEXTE, a, True, "g", POLICE_TITRE)
        police(ctx, 108, True, POLICE_TITRE)
    texte(ctx, "mesurée avec deux ESP32 et le Wi-Fi", W / 2, 620, 50, CYAN, app(t, 3.4), True, "c")
    texte(ctx, "Projet DIY  ·  physique  ·  électronique embarquée", W / 2, 690, 28, TEXTE2, app(t, 4.2), False, "c")
    a = app(t, 5.6, 0.8)
    if a > 0:
        carte_esp(ctx, 640, 790, 150, 120, "A", CYAN, a)
        carte_esp(ctx, 1130, 790, 150, 120, "B", JAUNE, a)
        u = ((t - 5.6) / 1.6) % 1
        aller = u < 0.5
        v = doux(u * 2 if aller else (u - 0.5) * 2)
        px = 800 + 320 * v if aller else 1120 - 320 * v
        onde(ctx, 800, 1120, 850, 10, 46, t * 8, CYAN, 0.35 * a, 2)
        photon(ctx, px, 850, JAUNE if aller else CYAN, a, -1 if aller else 1, 0.6)
        texte(ctx, "c = 299 792 458 m/s", W / 2, 980, 30, TEXTE2, a, True, "c")


def s_question(ctx, t, D):
    chapitre(ctx, t, D, 1, "LA QUESTION")
    texte(ctx, "La lumière voyage à", W / 2, 300, 50, TEXTE, app(t, 0.3), True, "c")
    v = 299792458 * sortie((t - 0.8) / 2.6)
    a = app(t, 0.8)
    halo(ctx, W / 2, 420, 520, JAUNE, 0.35 * a)
    l = chiffres(ctx, grouper(v), W / 2 - 80, 470, 140, JAUNE, a, "c")
    texte(ctx, "m/s", W / 2 - 80 + l / 2 + 30, 470, 60, TEXTE2, a, True)
    a2 = app(t, 4.0)
    texte(ctx, "soit 7,5 fois le tour de la Terre… en une seconde", W / 2, 570, 40, TEXTE2, a2, False, "c")
    if a2 > 0:
        ex, ey, er = 1690, 300, 95
        g = cairo.RadialGradient(ex - 30, ey - 30, 10, ex, ey, er)
        g.add_color_stop_rgba(0, 0.3, 0.55, 1, a2)
        g.add_color_stop_rgba(1, 0.05, 0.15, 0.45, a2)
        ctx.arc(ex, ey, er, 0, 2 * math.pi)
        ctx.set_source(g)
        ctx.fill()
        ang = (t - 4) * 2 * math.pi * 0.9
        ctx.save()
        ctx.set_line_width(5)
        for k in range(14):
            aa = ang - k * 0.09
            ctx.arc(ex, ey, er + 26, aa - 0.09, aa)
            ctx.set_source_rgba(*JAUNE, a2 * (1 - k / 14) * 0.9)
            ctx.stroke()
        ctx.restore()
        halo(ctx, ex + (er + 26) * math.cos(ang), ey + (er + 26) * math.sin(ang), 40, JAUNE, a2)
    a3 = app(t, 8.5) * (1 - app(t, 13.6, 0.5))
    if a3 > 0:
        x0, x1, y = 560, 1360, 760
        trait(ctx, x0, y, x1, y, 3, TEXTE2, a3)
        for k in range(11):
            x = x0 + (x1 - x0) * k / 10
            trait(ctx, x, y, x, y - (26 if k % 5 == 0 else 14), 2, TEXTE2, a3)
        texte(ctx, "1 mètre", W / 2, y - 50, 34, TEXTE, a3, True, "c")
        u = ((t - 8.5) / 1.8) % 1
        photon(ctx, x0 + (x1 - x0) * doux(u), y - 14, JAUNE, a3, 1, 0.8)
        texte(ctx, "en 3,34 nanosecondes", W / 2, 860, 60, CYAN, a3, True, "c")
        texte(ctx, "1 ns = un milliardième de seconde", W / 2, 920, 30, TEXTE3, a3, False, "c")
    a4 = app(t, 13.8)
    texte(ctx, "Comment chronométrer ça", W / 2, 800, 64, TEXTE, a4, True, "c", POLICE_TITRE)
    texte(ctx, "avec une carte à 10 € ?", W / 2, 880, 64, JAUNE, app(t, 14.4), True, "c", POLICE_TITRE)


def s_idee(ctx, t, D):
    chapitre(ctx, t, D, 2, "L'IDÉE")
    titre(ctx, "Un aller-retour d'onde radio", t)
    a = app(t, 0.6)
    ax, bx, y = 330, 1350, 360
    led = 1 if (t * 1.6) % 1 < 0.15 else 0
    carte_esp(ctx, ax, y, 240, 250, "A", CYAN, a, led)
    carte_esp(ctx, bx, y, 240, 250, "B", JAUNE, a, led)
    texte(ctx, "Carte A", ax + 120, y + 300, 36, CYAN, a, True, "c")
    texte(ctx, "répondeur fixe", ax + 120, y + 340, 26, TEXTE2, a, False, "c")
    texte(ctx, "Carte B", bx + 120, y + 300, 36, JAUNE, a, True, "c")
    texte(ctx, "console à écran", bx + 120, y + 340, 26, TEXTE2, a, False, "c")
    x0, x1, yc = ax + 260, bx - 20, y + 110
    a2 = app(t, 1.4)
    onde(ctx, x0, x1, yc, 22, 90, t * 7, CYAN, 0.45 * a2)
    if a2 > 0:
        u = ((t - 1.4) / 2.6) % 1
        aller = u < 0.5
        v = doux(u * 2 if aller else (u - 0.5) * 2)
        px = x0 + (x1 - x0) * v if aller else x1 - (x1 - x0) * v
        photon(ctx, px, yc, JAUNE if aller else CYAN, a2, 1 if aller else -1)
        texte(ctx, "trame FTM →" if aller else "← accusé de réception", W / 2, yc - 60, 28,
              JAUNE if aller else CYAN, a2, True, "c")
        texte(ctx, "ralenti ≈ 30 millions de fois", 1860, 140, 24, TEXTE3, a2, False, "d")
    a3 = app(t, 2.4)
    yd = y + 400
    fleche(ctx, W / 2, yd, ax + 120, yd, 3, TEXTE2, a3)
    fleche(ctx, W / 2, yd, bx + 120, yd, 3, TEXTE2, a3)
    pastille(ctx, "distance d", W / 2, yd, 30, JAUNE, a3)
    a5 = app(t, 10)
    texte(ctx, "Les ondes Wi-Fi (2,4 GHz) sont de la lumière invisible :", W / 2, 885, 34, TEXTE2,
          app(t, 4.5) * (1 - a5), False, "c")
    texte(ctx, "elles voyagent exactement à la vitesse c.", W / 2, 930, 34, TEXTE2, app(t, 5.0) * (1 - a5), False, "c")
    ctx.save()
    ctx.rectangle(0, 850, W, 120)
    ctx.set_source_rgba(*FOND_HAUT, 0.92 * a5)
    ctx.fill()
    ctx.restore()
    texte(ctx, "temps aller-retour  =  2 × d  /  c", W / 2, 920, 62, JAUNE, a5, True, "c", POLICE_TITRE)
    texte(ctx, "Exemple : d = 10 m  →  66,7 ns.  On le mesure à plusieurs distances…",
          W / 2, 990, 32, TEXTE2, app(t, 14), False, "c")


def s_ftm(ctx, t, D):
    chapitre(ctx, t, D, 3, "LE CHRONOMÈTRE : FTM")
    titre(ctx, "Fine Timing Measurement (Wi-Fi 802.11mc)", t, y=160, taille=52)
    texte(ctx, "chaque échange est horodaté par la radio elle-même", W / 2, 210, 30, TEXTE2, app(t, 0.8), False, "c")
    A, B = 520, 1060
    yh, yb = 290, 980
    a = app(t, 1.0)
    for x, nom, coul in ((A, "Carte A (répondeur)", CYAN), (B, "Carte B (initiateur)", JAUNE)):
        pastille(ctx, nom, x, yh, 28, coul, a)
        trait(ctx, x, yh + 30, x, yh + 30 + (yb - yh - 30) * app(t, 1.2, 1.0), 2, coul, 0.5 * a, [10, 10])
    texte(ctx, "temps", 150, 380, 26, TEXTE3, a, False)
    fleche(ctx, 180, 400, 180, 900, 2, TEXTE3, a)

    def message(t0, xa, ya, xb, yb, coul, lab, la, lb, nom):
        p = sortie((t - t0) / 1.5)
        if p <= 0:
            return
        xe, ye = xa + (xb - xa) * p, ya + (yb - ya) * p
        fleche(ctx, xa, ya, xe, ye, 4, coul, 1, 18 if p > 0.1 else 0)
        if p < 1:
            photon(ctx, xe, ye, coul, 1, 0, 0.7)
        texte(ctx, nom, (xa + xb) / 2, (ya + yb) / 2 - 22, 26, coul, app(t, t0 + 0.4), True, "c")
        pastille(ctx, la, xa + (-50 if xa < xb else 50), ya, 26, coul, app(t, t0), True)
        if p > 0.95:
            pastille(ctx, lb, xb + (50 if xa < xb else -50), yb, 26, coul, app(t, t0 + 1.4), True)

    message(2.0, A, 380, B, 500, JAUNE, "", "t1", "t2", "trame FTM")
    message(5.0, B, 620, A, 740, CYAN, "", "t3", "t4", "accusé (ACK)")
    # boucle : petits paquets qui repassent
    if t > 9:
        u = ((t - 9) / 2.2) % 1
        if u < 0.5:
            v = u * 2
            photon(ctx, A + (B - A) * v, 380 + 120 * v, JAUNE, 0.7, 0, 0.45)
        else:
            v = (u - 0.5) * 2
            photon(ctx, B + (A - B) * v, 620 + 120 * v, CYAN, 0.7, 0, 0.45)
    a3 = app(t, 7.5)
    if a3 > 0:
        trait(ctx, B + 110, 500, B + 110, 620, 4, ORANGE, a3)
        trait(ctx, B + 98, 500, B + 122, 500, 4, ORANGE, a3)
        trait(ctx, B + 98, 620, B + 122, 620, 4, ORANGE, a3)
        texte(ctx, "t3 − t2", B + 135, 552, 30, ORANGE, a3, True)
        texte(ctx, "réponse de B", B + 135, 588, 24, TEXTE2, a3, False)
    a4 = app(t, 9.0)
    if a4 > 0:
        trait(ctx, A - 110, 380, A - 110, 740, 4, CYAN, a4)
        trait(ctx, A - 122, 380, A - 98, 380, 4, CYAN, a4)
        trait(ctx, A - 122, 740, A - 98, 740, 4, CYAN, a4)
        texte(ctx, "t4 − t1", A - 130, 552, 30, CYAN, a4, True, "d")
        texte(ctx, "durée totale", A - 130, 588, 24, TEXTE2, a4, False, "d")
    a5 = app(t, 11)
    panneau(ctx, 1400, 300, 460, 300, a5)
    texte(ctx, "Temps de vol aller-retour", 1630, 360, 28, TEXTE2, a5, False, "c")
    texte(ctx, "(t4 − t1) − (t3 − t2)", 1630, 440, 44, JAUNE, a5, True, "c")
    texte(ctx, "= 2 × d / c", 1630, 510, 40, CYAN, app(t, 13), True, "c")
    texte(ctx, "les délais de B s'annulent", 1630, 560, 24, TEXTE3, app(t, 14), False, "c")
    a6 = app(t, 16)
    panneau(ctx, 1400, 630, 460, 360, a6)
    faits = ["Horloge : paliers de 1,5625 ns (≈ 23 cm aller-retour)",
             "16 échanges par session, ≈ 115 mesures par seconde",
             "Retards internes des puces déjà retirés",
             "Intégré dans l'ESP32-S3 : rien à ajouter"]
    yy = 690
    for i, f in enumerate(faits):
        ai = app(t, 16.5 + i * 2.2)
        disque(ctx, 1430, yy - 9, 6, CYAN, ai)
        yy = texte_bloc(ctx, f, 1450, yy, 390, 25, TEXTE, ai) + 18


_rnd_bruit = np.random.default_rng(11)
_ech_bruit = None


def s_bruit(ctx, t, D):
    global _ech_bruit
    chapitre(ctx, t, D, 4, "LE BRUIT ET LA STATISTIQUE")
    titre(ctx, "Une seule mesure est imprécise…", t)
    vrai = 66.7
    if _ech_bruit is None:
        e = vrai + 1.2 + 4.5 * _rnd_bruit.standard_normal(600)
        _ech_bruit = np.round(e / 1.5625) * 1.5625
    X0, X1, lo, hi = 260, 1660, 48.0, 88.0
    def px(v): return X0 + (X1 - X0) * (v - lo) / (hi - lo)
    YB = 820
    a = app(t, 0.6)
    trait(ctx, X0, YB, X1, YB, 3, TEXTE2, a)
    for v in np.arange(50, 88, 5):
        trait(ctx, px(v), YB, px(v), YB + 12, 2, TEXTE3, a)
        texte(ctx, f"{v:.0f}", px(v), YB + 44, 24, TEXTE3, a, False, "c")
    texte(ctx, "temps de vol (ns)", X1, YB + 84, 24, TEXTE3, a, False, "d")
    trait(ctx, px(vrai + 1.2), 300, px(vrai + 1.2), YB, 2, VERT, 0.8 * a, [8, 8])
    texte(ctx, "valeur vraie", px(vrai + 1.2) + 14, 312, 24, VERT, a, True, "g")
    # nombre d'echantillons visibles
    if t < 1.5:
        n = 0
    elif t < 5.5:
        n = int((t - 1.5) * 2.5)
    else:
        n = int(min(600, 10 + 590 * sortie((t - 5.5) / 8)))
    q = 1.5625
    if n > 0:
        vals = _ech_bruit[:n]
        bins = {}
        for v in vals:
            bins[v] = bins.get(v, 0) + 1
        hmax = max(12, max(bins.values()))
        for v, k in bins.items():
            h = (YB - 360) * k / hmax
            x = px(v - q / 2)
            w = px(v + q / 2) - x - 3
            g = cairo.LinearGradient(0, YB - h, 0, YB)
            g.add_color_stop_rgba(0, *CYAN, 0.95)
            g.add_color_stop_rgba(1, *VIOLET, 0.6)
            rrect(ctx, x + 1.5, YB - h, w, h, 4)
            ctx.set_source(g)
            ctx.fill()
        # le dernier echantillon "tombe"
        if t < 6:
            v = vals[-1]
            f = ((t - 1.5) * 2.5) % 1
            photon(ctx, px(v), 330 + (YB - 360) * doux(f), JAUNE, 1 - f * 0.5, 0, 0.5)
        moy = float(np.mean(vals))
        am = app(t, 6.5)
        trait(ctx, px(moy), 340, px(moy), YB, 4, JAUNE, am)
        pastille(ctx, "moyenne", px(moy) - 14, 350, 24, JAUNE, am, True, "d")
        if n > 80:
            ag = app(t, 9)
            s = 4.5
            ctx.save()
            ctx.set_line_width(4)
            ctx.set_source_rgba(*JAUNE, 0.8 * ag)
            for i in range(201):
                v = lo + (hi - lo) * i / 200
                pdf = math.exp(-0.5 * ((v - vrai - 1.2) / s) ** 2)
                y = YB - (YB - 360) * pdf * (n * q / (s * 2.5066)) / hmax
                (ctx.move_to if i == 0 else ctx.line_to)(px(v), y)
            ctx.stroke()
            ctx.restore()
    panneau(ctx, 1240, 380, 600, 250, app(t, 2.0))
    texte(ctx, "1 mesure : ± 4,5 ns  ≈  ± 70 cm", 1270, 440, 30, TEXTE, app(t, 2.0), True)
    texte(ctx, "moyenne de N mesures : ± 4,5 / √N", 1270, 500, 30, CYAN, app(t, 9.5), True)
    err = 4.5 / math.sqrt(max(1, n))
    texte(ctx, f"N = {n}  →  ± {fmt(err, 2)} ns  ≈  ± {fmt(err * 15, 0)} cm", 1270, 570, 34, JAUNE,
          app(t, 10.5), True)


def s_droite(ctx, t, D):
    chapitre(ctx, t, D, 5, "LA DROITE")
    titre(ctx, "…mais une droite ne ment pas", t)
    X0, X1, Y0, Y1 = 300, 1220, 270, 900
    dmax, tmax = 22, 160
    def px(d): return X0 + (X1 - X0) * d / dmax
    def py(v): return Y1 - (Y1 - Y0) * v / tmax
    a = app(t, 0.5)
    for d in range(0, 23, 5):
        trait(ctx, px(d), Y0, px(d), Y1, 1, GRILLE_C, 0.8 * a)
        texte(ctx, str(d), px(d), Y1 + 38, 26, TEXTE3, a, False, "c")
    for v in range(0, 161, 40):
        trait(ctx, X0, py(v), X1, py(v), 1, GRILLE_C, 0.8 * a)
        texte(ctx, str(v), X0 - 16, py(v) + 9, 26, TEXTE3, a, False, "d")
    trait(ctx, X0, Y0, X0, Y1, 2, TEXTE3, a)
    trait(ctx, X0, Y1, X1, Y1, 2, TEXTE3, a)
    texte(ctx, "distance (m)", X1, Y1 + 80, 26, TEXTE2, a, False, "d")
    texte(ctx, "temps aller-retour (ns)", X0 - 10, Y0 - 24, 26, TEXTE2, a, False, "g")
    ds = [2, 4, 6, 8, 10, 12, 15, 20]
    bruit = [0.3, -0.4, 0.2, 0.5, -0.3, 0.1, -0.2, 0.2]
    vs = [2 * d / 299792458 * 1e9 + 1.2 + b for d, b in zip(ds, bruit)]
    nvis = sum(1 for i in range(8) if t > 1.5 + 0.8 * i)
    # ajustement sur tous les points
    mx, my = sum(ds) / 8, sum(vs) / 8
    pente = sum((d - mx) * (v - my) for d, v in zip(ds, vs)) / sum((d - mx) ** 2 for d in ds)
    b = my - pente * mx
    al = sortie((t - 8.5) / 1.4)
    if al > 0:
        xe = dmax * al
        trait(ctx, px(0), py(b), px(xe), py(b + pente * xe), 12, JAUNE, 0.18)
        trait(ctx, px(0), py(b), px(xe), py(b + pente * xe), 4, JAUNE, 1)
        if al < 1:
            halo(ctx, px(xe), py(b + pente * xe), 60, JAUNE, 1)
    at = app(t, 11)
    if at > 0:
        ar = 2e9 / 299792458
        trait(ctx, px(0), py(b), px(dmax), py(b + ar * dmax), 2, TEXTE2, 0.6 * at, [12, 10])
    for i in range(nvis):
        age = t - (1.5 + 0.8 * i)
        s = 1 + 0.6 * math.sin(sat(age / 0.45) * math.pi) if age < 0.45 else 1
        x, y = px(ds[i]), py(vs[i])
        if age < 0.8:
            ctx.arc(x, y, 10 + 60 * sortie(age / 0.8), 0, 2 * math.pi)
            ctx.set_source_rgba(*CYAN, 0.8 * (1 - age / 0.8))
            ctx.set_line_width(3)
            ctx.stroke()
        halo(ctx, x, y, 40, CYAN, 0.8)
        disque(ctx, x, y, 10 * s, CYAN)
        disque(ctx, x, y, 4 * s, BLANC)
    a2 = app(t, 11.5)
    if a2 > 0:
        d0, d1 = 8, 16
        x0, x1 = px(d0), px(d1)
        y0, y1 = py(b + pente * d0), py(b + pente * d1)
        trait(ctx, x0, y0, x1, y0, 3, ORANGE, a2)
        trait(ctx, x1, y0, x1, y1, 3, ORANGE, a2)
        texte(ctx, f"Δd = {d1 - d0} m", (x0 + x1) / 2, y0 + 40, 26, ORANGE, a2, True, "c")
        texte(ctx, f"Δt = {fmt(pente * (d1 - d0), 1)} ns", x1 + 18, (y0 + y1) / 2, 26, ORANGE, a2, True)
    a3 = app(t, 10.5)
    panneau(ctx, 1310, 290, 550, 610, a3)
    texte(ctx, "pente de la droite", 1585, 360, 28, TEXTE2, a3, False, "c")
    texte(ctx, f"a = {fmt(pente, 2)} ns/m", 1585, 425, 48, JAUNE, a3, True, "c")
    texte(ctx, "aller-retour : t = 2d / c", 1585, 510, 30, TEXTE2, app(t, 12.5), False, "c")
    texte(ctx, "c = 2 / a", 1585, 600, 70, CYAN, app(t, 13.5), True, "c", POLICE_TITRE)
    texte(ctx, f"= 2 / {fmt(pente, 2)} ns/m", 1585, 670, 34, TEXTE, app(t, 15.5), True, "c")
    cval = 2 / pente * 1e3
    ac = app(t, 17)
    halo(ctx, 1585, 760, 240, JAUNE, 0.4 * ac)
    texte(ctx, f"c ≈ {fmt(cval, 1)} × 10⁶ m/s", 1585, 775, 46, JAUNE, ac, True, "c")
    texte_bloc(ctx, "Le décalage à l'origine (retards fixes) ne change pas la pente : seule la pente compte.",
               1340, 840, 490, 22, TEXTE3, app(t, 20))


GRILLE_C = c(34, 44, 92)


def s_materiel(ctx, t, D):
    chapitre(ctx, t, D, 6, "LE MATÉRIEL")
    titre(ctx, "Deux cartes et un mètre ruban", t, y=160)
    a = app(t, 0.8)
    carte_esp(ctx, 250, 220, 560, 400, "ES3C28P", JAUNE, a, 0.6, (capture("c03_pret"), 1.25))
    texte(ctx, "Carte B — ES3C28P", 530, 668, 34, JAUNE, a, True, "c")
    texte(ctx, "ESP32-S3 · 8 Mo de PSRAM · écran 2,8\" tactile · microSD", 530, 708, 24, TEXTE2, a, False, "c")
    a2 = app(t, 2.0)
    carte_esp(ctx, 1010, 300, 300, 240, "DevKit", CYAN, a2, 0.6)
    texte(ctx, "Carte A — ESP32-S3", 1160, 668, 34, CYAN, a2, True, "c")
    texte(ctx, "répondeur FTM, fixe, sur batterie", 1160, 708, 24, TEXTE2, a2, False, "c")
    a3 = app(t, 3.4)
    for i, s in enumerate(["+ un mètre ruban (20 m)", "+ deux batteries USB", "+ Arduino IDE, ESP32 3.x"]):
        ai = app(t, 3.4 + i * 0.6)
        disque(ctx, 1450, 360 + i * 80 - 10, 8, VERT, ai)
        texte(ctx, s, 1475, 360 + i * 80, 32, TEXTE, ai, True)
    a4 = app(t, 7)
    panneau(ctx, 250, 760, 1420, 70, a4, bord=ORANGE)
    texte(ctx, "Compatibles FTM : ESP32-S2, S3 et C3 récents   ·   PAS l'ESP32 « classique »   ·   ESP32-C6 v0.1 : défaut matériel",
          960, 805, 25, ORANGE, a4, True, "c")
    a5 = app(t, 11)
    boites = [("Cœur 0", "tâche Wi-Fi FTM  +  envoi de l'image à l'écran", CYAN),
              ("Cœur 1", "calculs  +  dessin de l'interface animée", JAUNE),
              ("PSRAM", "2 images (double tampon)  +  fond pré-calculé", VIOLET)]
    for i, (n, d, col) in enumerate(boites):
        ai = app(t, 11 + i * 0.8)
        x = 250 + i * 480
        panneau(ctx, x, 870, 460, 130, ai, bord=col)
        texte(ctx, n, x + 30, 920, 32, col, ai, True)
        texte_bloc(ctx, d, x + 30, 962, 410, 23, TEXTE2, ai)


def s_logiciel(ctx, t, D):
    chapitre(ctx, t, D, 7, "LE LOGICIEL")
    titre(ctx, "Le code : Arduino, ESP32 3.x", t, y=160)
    lignes = [
        (0, "firmware/lumiere_console/", "carte B : l'instrument à écran", JAUNE, True),
        (1, "lumiere_console.ino", "Wi-Fi FTM, écran, tactile, carte SD", TEXTE, False),
        (1, "interface.cpp", "les 5 écrans animés et la règle tactile", TEXTE, False),
        (1, "mesure.cpp", "points, droite, c, statistiques, simulation", TEXTE, False),
        (1, "gfx.cpp", "moteur de dessin lissé (halos, transparence)", TEXTE, False),
        (1, "polices.cpp", "police Inter avec les accents", TEXTE, False),
        (0, "firmware/lumiere_repondeur/", "carte A : le répondeur FTM", CYAN, True),
        (0, "outils/", "simulateur PC, analyse Python, cette vidéo", VIOLET, True),
    ]
    y = 290
    for i, (ind, nom, desc, col, dossier) in enumerate(lignes):
        a = app(t, 0.8 + i * 0.6)
        x = 300 + ind * 60 + 40 * (1 - a)
        if dossier:
            rrect(ctx, x, y - 26, 34, 26, 4)
            ctx.set_source_rgba(*col, 0.8 * a)
            ctx.fill()
        else:
            trait(ctx, x - 30, y - 10, x - 6, y - 10, 2, TEXTE3, a)
            rrect(ctx, x, y - 30, 22, 28, 3)
            ctx.set_source_rgba(*TEXTE2, 0.7 * a)
            ctx.set_line_width(2)
            ctx.stroke()
        texte(ctx, nom, x + 50, y, 34, col, a, True)
        texte(ctx, desc, 1000, y, 28, TEXTE2, a, False)
        y += 78


DEMO = [
    ("s01_demarrage", "Démarrage",
     "La carte B cherche la carte A, s'y connecte, puis affiche en direct le temps de vol mesuré."),
    ("s02_reglages", "Réglages",
     "Source (carte A réelle ou simulation), plan de mesure, nombre de mesures par point, luminosité."),
    ("s03_regle", "La règle tactile",
     "On fait glisser la règle pour indiquer la distance mesurée au mètre ruban entre les antennes."),
    ("s04_trop_pres", "Piège : trop près",
     "À 0,5 m, une partie des mesures est bloquée à zéro : l'écran prévient. On supprime ce point dans Courbe."),
    ("s05_premiere_mesure", "Première mesure",
     "600 temps de vol en 5 secondes, histogramme en direct, puis la règle passe à la distance suivante."),
    ("s06_revelation", "Premier résultat",
     "Dès deux distances, la pente donne c. L'écran dévoile chaque nouvelle valeur."),
    ("s07_plan_accelere", "Le plan complet (×4)",
     "De 2 à 20 m : chaque pastille verte est une distance mesurée."),
    ("s08_courbe", "Courbe",
     "La droite temps/distance, sa pente, la droite théorique et le détail de chaque point."),
    ("s09_resultat", "Résultat",
     "c avec son incertitude, l'écart à la vraie valeur et la convergence au fil des points."),
    ("s10_stats", "Statistiques",
     "Échantillons, sessions FTM, rejets, débit et distribution des temps de vol."),
]


def duree_demo():
    return sum(len(charger_segment(n)[0]) for n, _, _ in DEMO) / FPS


def s_demo(ctx, t, D):
    chapitre(ctx, t, D, 8, "DÉMONSTRATION SUR LA CARTE")
    etapes = chronologie()[1]
    k, base = len(DEMO) - 1, 0.0
    for i, d in enumerate(etapes):
        if t < base + d:
            k = i
            break
        base += d
    else:
        base = sum(etapes[:-1])
    nom, tit, desc = DEMO[k]
    imgs, appuis = charger_segment(nom)
    t_etape = t - base
    fi = min(int(t_etape * FPS), len(imgs) - 1)
    # carte
    SX, SY, ECH = 150, 205, 3
    a = app(t, 0.2)
    rrect(ctx, SX - 60, SY - 75, 960 + 120, 720 + 150, 40)
    g = cairo.LinearGradient(0, SY - 75, 0, SY + 795)
    g.add_color_stop_rgba(0, 0.12, 0.15, 0.30, a)
    g.add_color_stop_rgba(1, 0.05, 0.06, 0.14, a)
    ctx.set_source(g)
    ctx.fill_preserve()
    ctx.set_source_rgba(*JAUNE, 0.5 * a)
    ctx.set_line_width(2)
    ctx.stroke()
    for (hx, hy) in ((SX - 30, SY - 45), (SX + 990, SY - 45), (SX - 30, SY + 765), (SX + 990, SY + 765)):
        ctx.arc(hx, hy, 9, 0, 2 * math.pi)
        ctx.set_source_rgba(0.6, 0.6, 0.65, 0.6 * a)
        ctx.stroke()
    texte(ctx, "ES3C28P · ESP32-S3", SX, SY - 28, 22, TEXTE3, a, True)
    surf, _keep = surface_rgb(imgs[fi])
    ctx.save()
    ctx.translate(SX, SY)
    ctx.scale(ECH, ECH)
    ctx.set_source_surface(surf, 0, 0)
    ctx.get_source().set_filter(cairo.FILTER_NEAREST)
    ctx.paint_with_alpha(a)
    ctx.restore()
    # doigt
    for df in range(0, 12):
        if fi - df in appuis:
            x, y = appuis[fi - df]
            X, Y = SX + x * ECH, SY + y * ECH
            if df == 0:
                disque(ctx, X, Y, 30, BLANC, 0.28)
                ctx.arc(X, Y, 30, 0, 2 * math.pi)
                ctx.set_source_rgba(1, 1, 1, 0.8)
                ctx.set_line_width(3)
                ctx.stroke()
            else:
                ctx.arc(X, Y, 30 + df * 5, 0, 2 * math.pi)
                ctx.set_source_rgba(1, 1, 1, 0.5 * (1 - df / 12))
                ctx.set_line_width(2)
                ctx.stroke()
            break
    texte(ctx, "Images du programme réel (simulateur PC, même code que la carte, données de mesure simulées)",
          SX + 480, SY + 812, 20, TEXTE3, a, False, "c")
    # panneau d'explications
    PX = 1250
    ae = app(t_etape, 0.0, 0.5)
    texte(ctx, f"ÉTAPE {k + 1} / {len(DEMO)}", PX, 250, 24, CYAN, a, True)
    texte(ctx, tit, PX + 30 * (1 - ae), 320, 56, TEXTE, ae, True, "g", POLICE_TITRE)
    texte_bloc(ctx, desc, PX, 385, 600, 32, TEXTE2, ae)
    y = 610
    for i, (_, ti, _) in enumerate(DEMO):
        fait, cour = i < k, i == k
        col = VERT if fait else (JAUNE if cour else TEXTE3)
        if cour:
            halo(ctx, PX + 10, y - 8, 26, JAUNE, 0.8)
        disque(ctx, PX + 10, y - 8, 8, col, a)
        texte(ctx, ti, PX + 34, y, 25, TEXTE if cour else (TEXTE2 if fait else TEXTE3), a, cour)
        y += 40


def s_protocole(ctx, t, D):
    chapitre(ctx, t, D, 9, "MODE OPÉRATOIRE")
    titre(ctx, "Mesurer c en classe ou en club", t, y=160)
    etapes = ["Dehors ou dans un long couloir, rien entre les cartes",
              "Cartes à environ 1 m du sol, antennes face à face",
              "Carte A fixe sur batterie, on déplace la carte B",
              "Mesurer la distance antenne–antenne au mètre ruban",
              "Régler la règle, appuyer sur MESURER (600 mesures, sans bouger)",
              "Recommencer de 2 à 20 m (plan « 20 m »)",
              "Lire c dans Résultat, exploiter lumiere.csv sur PC"]
    y = 280
    for i, e in enumerate(etapes):
        a = app(t, 0.8 + i * 1.3)
        x = 160 + 30 * (1 - a)
        disque(ctx, x + 26, y - 12, 26, CYAN, a)
        texte(ctx, str(i + 1), x + 26, y - 1, 28, c(6, 10, 30), a, True, "c")
        texte(ctx, e, x + 74, y, 31, TEXTE, a, False)
        y += 92
    a2 = app(t, 11)
    panneau(ctx, 1320, 260, 540, 520, a2, bord=ORANGE)
    texte(ctx, "Pièges à éviter", 1360, 330, 38, ORANGE, a2, True)
    pieges = ["Moins de 2 m : mesures bloquées à 0", "Murs et plafond : échos (multitrajets)",
              "Personnes entre les cartes", "Bouger pendant une série"]
    yy = 410
    for i, p in enumerate(pieges):
        ai = app(t, 11.6 + i * 0.9)
        trait(ctx, 1365, yy - 18, 1381, yy - 2, 4, ORANGE, ai)
        trait(ctx, 1381, yy - 18, 1365, yy - 2, 4, ORANGE, ai)
        yy = texte_bloc(ctx, p, 1400, yy, 430, 28, TEXTE, ai) + 26


def s_resultat(ctx, t, D):
    chapitre(ctx, t, D, 10, "RÉSULTAT")
    a = app(t, 0.3)
    carte_esp(ctx, 110, 170, 800, 700, "ES3C28P", JAUNE, a, 0, (capture("c20_resultat"), 2.3))
    texte(ctx, "Exemple de résultat", 1340, 300, 34, TEXTE2, app(t, 0.8), False, "c")
    a2 = app(t, 1.4)
    v = 298.9 * sortie((t - 1.4) / 1.8)
    halo(ctx, 1340, 420, 380, CYAN, 0.45 * a2)
    chiffres(ctx, fmt(v, 1), 1340, 470, 130, TEXTE, a2, "c")
    texte(ctx, "× 10⁶ m/s   ± 1,5", 1340, 540, 42, CYAN, a2, True, "c")
    texte(ctx, "écart à la valeur exacte : −0,3 %", 1340, 620, 36, VERT, app(t, 3.5), True, "c")
    texte_bloc(ctx, "Obtenu dans le simulateur avec des données calées sur de vraies mesures FTM "
                    "entre deux ESP32-S3. À vous de faire la vraie mesure !",
               1000, 720, 680, 26, TEXTE3, app(t, 4.5), align="c")


def s_outro(ctx, t, D):
    bx = -200 + 2500 * sortie(t / 2.0)
    g = cairo.LinearGradient(0, 0, bx, 0)
    g.add_color_stop_rgba(0, *VIOLET, 0)
    g.add_color_stop_rgba(1, *CYAN, 0.8)
    ctx.set_source(g)
    ctx.rectangle(0, 638, max(0, min(bx, W)), 3)
    ctx.fill()
    if t < 2.2:
        halo(ctx, bx, 640, 140, CYAN, 1)
    texte(ctx, "Tout est sur GitHub", W / 2, 420, 88, TEXTE, app(t, 0.6), True, "c", POLICE_TITRE)
    texte(ctx, "code  ·  tutoriel  ·  protocole pour étudiants  ·  simulateur", W / 2, 510, 36, CYAN,
          app(t, 1.4), False, "c")
    pastille(ctx, "github.com/Edunts5962/Esp32-vitesse-de-la-lumi-re-", W / 2, 580, 30, JAUNE, app(t, 2.0))
    texte(ctx, "ESP32-S3  ·  Arduino  ·  Wi-Fi FTM  ·  police Inter (SIL OFL)", W / 2, 760, 26, TEXTE3,
          app(t, 3.0), False, "c")
    texte(ctx, "Merci !", W / 2, 880, 60, JAUNE, app(t, 4.0), True, "c", POLICE_TITRE)


SCENES = [
    (s_intro, 14), (s_question, 18), (s_idee, 24), (s_ftm, 34), (s_bruit, 24), (s_droite, 28),
    (s_materiel, 20), (s_logiciel, 12), (s_demo, None), (s_protocole, 22), (s_resultat, 12), (s_outro, 10),
]


DOSSIER_VOIX = None     # dossier de narration.json (voix off), facultatif
_chrono = None


def chronologie():
    """Durees des scenes, etapes de la demo et instants de chaque replique.
    Une scene s'allonge si sa voix off depasse ; une etape de la demo garde
    sa derniere image le temps du commentaire."""
    global _chrono
    if _chrono is not None:
        return _chrono
    import json
    repl = []
    if DOSSIER_VOIX and os.path.exists(os.path.join(DOSSIER_VOIX, "narration.json")):
        repl = json.load(open(os.path.join(DOSSIER_VOIX, "narration.json")))
    n_etapes = [len(charger_segment(n)[0]) / FPS for n, _, _ in DEMO]
    etapes = list(n_etapes)
    for r in repl:
        if r["scene"] == 8:
            k = int(r["debut"])
            etapes[k] = max(etapes[k], 0.3 + r["duree"] + 0.9)
    ds = []
    for i, (_, d) in enumerate(SCENES):
        ds.append(sum(etapes) + 1.0 if d is None else d)
    places = []
    for i in range(len(SCENES)):
        fin = 0.0
        for r in sorted([r for r in repl if r["scene"] == i], key=lambda r: r["debut"]):
            if i == 8:
                debut = sum(etapes[: int(r["debut"])]) + 0.3
            else:
                debut = max(r["debut"], fin + 0.35)
            fin = debut + r["duree"]
            places.append((i, debut, r))
        if repl and i != 8:
            ds[i] = max(ds[i], fin + 1.2)
    ds = [round(d, 3) for d in ds]
    debuts = [sum(ds[:i]) for i in range(len(ds))]
    repliques = [(debuts[i] + t, r) for i, t, r in places]
    _chrono = (ds, etapes, repliques)
    return _chrono


def durees():
    return chronologie()[0]


def rendre_image(ctx, i, t, D, T, total):
    fond(ctx, T)
    SCENES[i][0](ctx, t, D)
    f = min(app(t, 0, 0.5), 1 - app(t, D - 0.5, 0.5))
    if i == len(SCENES) - 1:
        f = min(f, 1 - app(t, D - 1.5, 1.5))
    if f < 1:
        ctx.set_source_rgba(*FOND_HAUT, 1 - f)
        ctx.paint()
    barre_progression(ctx, T, total)


def rendre_scene(i, sortie_dir):
    ds = durees()
    total = sum(ds)
    D = ds[i]
    T0 = sum(ds[:i])
    chemin = os.path.join(sortie_dir, f"scene_{i:02d}.mp4")
    proc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "bgra",
                             "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-", "-c:v", "libx264",
                             "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p", chemin],
                            stdin=subprocess.PIPE)
    surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    ctx = cairo.Context(surf)
    n = int(round(D * FPS))
    for k in range(n):
        t = k / FPS
        rendre_image(ctx, i, t, D, T0 + t, total)
        surf.flush()
        proc.stdin.write(bytes(surf.get_data()))
    proc.stdin.close()
    proc.wait()
    print(f"scene {i:02d} : {D:.1f} s -> {chemin}", flush=True)


def images_fixes(sortie_dir):
    """Vues fixes pour le README (schemas du principe)."""
    ds = durees()
    total = sum(ds)
    vues = [(2, 13.0, "principe_aller_retour"), (3, 30.0, "principe_ftm"),
            (4, 22.0, "principe_statistique"), (5, 23.0, "principe_droite"),
            (6, 16.0, "materiel_architecture"), (0, 9.0, "titre")]
    surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    ctx = cairo.Context(surf)
    for i, t, nom in vues:
        rendre_image(ctx, i, t, ds[i], sum(ds[:i]) + t, total)
        surf.write_to_png(os.path.join(sortie_dir, nom + ".png"))
        print("image", nom)


def assembler(sortie_dir):
    ds = durees()
    liste = os.path.join(sortie_dir, "liste.txt")
    with open(liste, "w") as f:
        for i in range(len(SCENES)):
            f.write(f"file 'scene_{i:02d}.mp4'\n")
    final = os.path.join(sortie_dir, "lumiere_esp32_presentation.mp4")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", liste,
                    "-c", "copy", "-movflags", "+faststart", final], check=True)
    print(f"video : {final} ({sum(ds):.0f} s)")



# ================================================================== SON
SR_SON = 44100


def musique(duree):
    """Nappe d'ambiance douce (accords La m - Fa - Do - Sol), stereo."""
    n = int(duree * SR_SON)
    t = np.arange(n) / SR_SON
    out = np.zeros((n, 2))
    accords = [(110.0, 130.81, 164.81, 220.0), (87.31, 110.0, 130.81, 174.61),
               (130.81, 164.81, 196.0, 261.63), (98.0, 123.47, 146.83, 196.0)]
    pas = 8.0
    for k in range(int(duree / pas) + 2):
        acc = accords[k % 4]
        t0 = k * pas - 1.5
        i0, i1 = max(0, int(t0 * SR_SON)), min(n, int((t0 + pas + 3.0) * SR_SON))
        if i0 >= i1:
            continue
        tt = t[i0:i1] - t0
        env = np.minimum(1, tt / 2.5) * np.minimum(1, (pas + 3.0 - tt) / 2.5)
        env = np.clip(env, 0, 1) ** 2
        for j, f in enumerate(acc):
            pan = 0.3 + 0.4 * (j / 3)
            onde_ = (np.sin(2 * np.pi * f * tt) + np.sin(2 * np.pi * (f + 0.18) * tt)
                     + 0.3 * np.sin(2 * np.pi * 2 * f * tt) + 0.1 * np.sin(2 * np.pi * 3 * f * tt))
            out[i0:i1, 0] += onde_ * env * (1 - pan)
            out[i0:i1, 1] += onde_ * env * pan
        # note aigue scintillante
        fa = acc[3] * 2
        sc = np.sin(2 * np.pi * fa * tt) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.25 * tt)) * 0.35
        out[i0:i1, 0] += sc * env
        out[i0:i1, 1] += sc * env
    # echos (reverberation simple)
    rev = out.copy()
    for d, g in ((0.29, 0.35), (0.47, 0.25), (0.71, 0.15)):
        k = int(d * SR_SON)
        rev[k:] += out[:-k] * g
    out = rev
    fondu = np.minimum(1, t / 3.0) * np.minimum(1, (duree - t) / 5.0)
    out *= np.clip(fondu, 0, 1)[:, None]
    return out / (np.abs(out).max() + 1e-9)


def piste_son(sortie_dir):
    import wave as wv
    from scipy.signal import resample_poly
    ds, _, repliques = chronologie()
    total = sum(ds)
    n = int(total * SR_SON) + SR_SON
    voix = np.zeros(n)
    for t0, r in repliques:
        with wv.open(os.path.join(DOSSIER_VOIX, r["fichier"])) as w:
            sr = w.getframerate()
            x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
        x = resample_poly(x, SR_SON, sr)
        i0 = int(t0 * SR_SON)
        voix[i0:i0 + len(x)] += x[: n - i0]
    voix *= 0.89 / (np.abs(voix).max() + 1e-9)
    # baisse de la musique sous la voix (enveloppe lissee)
    def moyenne_glissante(x, k):
        c = np.cumsum(np.concatenate(([0.0], x)))
        y = (c[k:] - c[:-k]) / k
        return np.concatenate((np.full(k // 2, y[0]), y, np.full(len(x) - len(y) - k // 2, y[-1])))
    presence = moyenne_glissante((np.abs(voix) > 0.02).astype(float), SR_SON // 4)
    presence = np.clip(presence * 3, 0, 1)
    presence = moyenne_glissante(presence, int(SR_SON * 0.6))
    mus = musique(n / SR_SON)[:n]
    gain = 0.16 * (1 - 0.55 * presence)
    mix = mus * gain[:, None] + voix[:, None]
    mix /= max(1.0, np.abs(mix).max() / 0.97)
    chemin = os.path.join(sortie_dir, "piste_son.wav")
    with wv.open(chemin, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR_SON)
        w.writeframes((mix * 32767).astype(np.int16).tobytes())
    # sous-titres
    def hms(x):
        h, m = int(x // 3600), int(x % 3600 // 60)
        return f"{h:02d}:{m:02d}:{int(x % 60):02d},{int(round((x % 1) * 1000)) % 1000:03d}"
    with open(os.path.join(sortie_dir, "lumiere_esp32_presentation.fr.srt"), "w", encoding="utf-8") as f:
        for k, (t0, r) in enumerate(repliques, 1):
            f.write(f"{k}\n{hms(t0)} --> {hms(t0 + r['duree'] + 0.2)}\n{r['sous_titre']}\n\n")
    print(f"son : {chemin} ({total:.1f} s), {len(repliques)} repliques")
    return chemin


def video_finale(sortie_dir):
    muette = os.path.join(sortie_dir, "lumiere_esp32_presentation.mp4")
    son = piste_son(sortie_dir)
    finale = os.path.join(sortie_dir, "lumiere_esp32_presentation_voix.mp4")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", muette, "-i", son, "-c:v", "copy",
                    "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", finale], check=True)
    print("video avec voix :", finale)

if __name__ == "__main__":
    DOSSIER_SIM = sys.argv[1]
    sortie_dir = sys.argv[2]
    os.makedirs(sortie_dir, exist_ok=True)
    preparer_fond()
    args = sys.argv[3:]
    if "--voix" in args:
        k = args.index("--voix")
        DOSSIER_VOIX = args[k + 1]
        del args[k:k + 2]
    if args and args[0] == "--final":
        video_finale(sortie_dir)
        sys.exit(0)
    if args and args[0] == "--assembler":
        assembler(sortie_dir)
    elif args and args[0] == "--images":
        images_fixes(sortie_dir)
    elif args and args[0] == "--test":
        # une image par scene, a mi-parcours et vers la fin
        ds = durees()
        surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
        ctx = cairo.Context(surf)
        for i in range(len(SCENES)):
            for frac in (0.35, 0.8):
                t = ds[i] * frac
                rendre_image(ctx, i, t, ds[i], sum(ds[:i]) + t, sum(ds))
                surf.write_to_png(os.path.join(sortie_dir, f"test_{i:02d}_{int(frac * 100)}.png"))
        print("durees", ds, "total", sum(ds))
    else:
        scenes = [int(a) for a in args] if args else range(len(SCENES))
        for i in scenes:
            rendre_scene(i, sortie_dir)
