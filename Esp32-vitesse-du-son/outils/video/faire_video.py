#!/usr/bin/env python3
"""
faire_video.py - video de presentation du projet "vitesse du son" (1920x1080, 30 i/s, ~6 min).

Dessin vectoriel lisse avec pycairo, encodage H.264 avec ffmpeg.
Les sequences "Demonstration" utilisent les images du simulateur
(outils/simulateur/sim_tuto.cpp : meme code d'interface et de traitement
du signal que la carte).

  pip install pycairo numpy
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> [n_scene ...]
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> --assembler
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> --images   (vues fixes)
  python3 faire_video.py <dossier_sim_tuto> <dossier_sortie> --miniature (vignette YouTube)

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
    """nom = "a" ou "a+b" (segments mis bout a bout)."""
    if nom not in _segments:
        imgs, appuis, base = [], {}, 0
        for part in nom.split("+"):
            brut = np.fromfile(os.path.join(DOSSIER_SIM, part + ".rgb"), dtype=np.uint8)
            n = len(brut) // (320 * 240 * 3)
            imgs.append(brut[: n * 320 * 240 * 3].reshape(n, 240, 320, 3))
            chemin = os.path.join(DOSSIER_SIM, part + ".appuis")
            if os.path.exists(chemin):
                for ligne in open(chemin):
                    f, x, y = map(int, ligne.split())
                    appuis[base + f] = (x, y)
            base += n
        _segments[nom] = (np.concatenate(imgs), appuis)
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

# ------------------------------------------------------------------ elements propres au son
def haut_parleur(ctx, x, y, s, coul, a=1.0, ondes=0.0):
    """Petit haut-parleur (x, y = centre de la membrane), ondes = phase 0..1."""
    if a <= 0:
        return
    ctx.set_source_rgba(*coul, a)
    ctx.rectangle(x - 20 * s, y - 12 * s, 14 * s, 24 * s)
    ctx.fill()
    ctx.move_to(x - 6 * s, y - 12 * s)
    ctx.line_to(x + 12 * s, y - 28 * s)
    ctx.line_to(x + 12 * s, y + 28 * s)
    ctx.line_to(x - 6 * s, y + 12 * s)
    ctx.close_path()
    ctx.fill()
    if ondes is not None:
        for k in range(3):
            r = (24 + 16 * k + 16 * ondes) * s
            ctx.save()
            ctx.set_line_width(4 * s)
            ctx.set_line_cap(cairo.LINE_CAP_ROUND)
            ctx.arc(x + 12 * s, y, r, -0.7, 0.7)
            ctx.set_source_rgba(*coul, a * (0.9 - 0.25 * k - 0.2 * ondes))
            ctx.stroke()
            ctx.restore()


def front(ctx, cx, cy, r, a0, a1, ep, coul, a=1.0, lueur=True, hmax=None):
    if a <= 0 or r <= 1:
        return
    if hmax is not None:                        # limite la hauteur du front (grands rayons)
        m = (a0 + a1) / 2
        dem = min((a1 - a0) / 2, math.asin(min(1.0, hmax / r)))
        a0, a1 = m - dem, m + dem
    ctx.save()
    ctx.new_path()
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    if lueur:
        ctx.set_line_width(ep * 4)
        ctx.arc(cx, cy, r, a0, a1)
        ctx.set_source_rgba(*coul, 0.18 * a)
        ctx.stroke()
    ctx.set_line_width(ep)
    ctx.arc(cx, cy, r, a0, a1)
    ctx.set_source_rgba(*coul, a)
    ctx.stroke()
    ctx.restore()


def mur(ctx, x, y0, y1, a=1.0, flash=0.0):
    if a <= 0:
        return
    halo(ctx, x + 10, (y0 + y1) / 2, 220, CYAN, 0.6 * flash * a)
    g = cairo.LinearGradient(x, 0, x + 26, 0)
    g.add_color_stop_rgba(0, 0.55, 0.58, 0.72, a)
    g.add_color_stop_rgba(1, 0.25, 0.27, 0.42, a)
    ctx.rectangle(x, y0, 26, y1 - y0)
    ctx.set_source(g)
    ctx.fill()
    for y in range(int(y0), int(y1) - 10, 22):
        trait(ctx, x + 26, y + 20, x + 44, y + 2, 2.5, TEXTE3, a)
    texte(ctx, "mur", x + 13, y0 - 18, 26, TEXTE2, a, True, "c")


def carte_son(ctx, x, y, s=1.0, a=1.0, led=0.0, surf=None):
    """La carte ES3C28P vue de face : ecran, haut-parleur, micro. (x, y) = coin."""
    w, h = 240 * s, 300 * s
    if a <= 0:
        return (x + 70 * s, y + h - 62 * s), (x + w - 64 * s, y + h - 62 * s)
    halo(ctx, x + w / 2, y + h / 2, 260 * s, JAUNE, 0.18 * a)
    rrect(ctx, x, y, w, h, 22 * s)
    g = cairo.LinearGradient(x, y, x, y + h)
    g.add_color_stop_rgba(0, 0.14, 0.18, 0.36, a)
    g.add_color_stop_rgba(1, 0.05, 0.07, 0.17, a)
    ctx.set_source(g)
    ctx.fill_preserve()
    ctx.set_source_rgba(*JAUNE, 0.8 * a)
    ctx.set_line_width(2.5)
    ctx.stroke()
    ex, ey, ew, eh = x + 20 * s, y + 22 * s, w - 40 * s, (w - 40 * s) * 0.75
    rrect(ctx, ex - 6, ey - 6, ew + 12, eh + 12, 8)
    ctx.set_source_rgba(0.02, 0.02, 0.05, a)
    ctx.fill()
    if surf is not None:
        ctx.save()
        ctx.translate(ex, ey)
        ctx.scale(ew / 320, eh / 240)
        ctx.set_source_surface(surf, 0, 0)
        ctx.get_source().set_filter(cairo.FILTER_GOOD)
        ctx.paint_with_alpha(a)
        ctx.restore()
    else:
        g2 = cairo.LinearGradient(ex, ey, ex + ew, ey + eh)
        g2.add_color_stop_rgba(0, 0.10, 0.20, 0.45, a)
        g2.add_color_stop_rgba(1, 0.20, 0.08, 0.40, a)
        ctx.rectangle(ex, ey, ew, eh)
        ctx.set_source(g2)
        ctx.fill()
    # haut-parleur (grille) et micro
    hx, hy = x + 70 * s, y + h - 62 * s
    disque(ctx, hx, hy, 30 * s, c(20, 24, 50), a)
    for k in range(3):
        ctx.arc(hx, hy, (10 + 9 * k) * s, 0, 2 * math.pi)
        ctx.set_source_rgba(*TEXTE3, a)
        ctx.set_line_width(2)
        ctx.stroke()
    texte(ctx, "HP", hx, hy + 52 * s, 18 * s, TEXTE3, a, True, "c")
    mx, my = x + w - 64 * s, y + h - 62 * s
    if led > 0.01:
        halo(ctx, mx, my, 40 * s, VERT, led * a)
    disque(ctx, mx, my, 9 * s, VERT if led > 0.1 else c(30, 90, 60), a)
    texte(ctx, "micro", mx, my + 52 * s, 18 * s, TEXTE3, a, True, "c")
    return (hx, hy), (mx, my)


def bonhomme(ctx, x, yb, h, ph, coul, a=1.0, v=0.0):
    """Personnage qui marche ; yb = sol ; v>0 : il va vers la gauche."""
    if a <= 0:
        return
    sens = -1 if v >= 0 else 1
    amp = min(0.8, 0.25 + 0.2 * abs(v))
    sw = math.sin(ph) * amp if abs(v) > 0.05 else 0.25
    course = 1 if abs(v) > 2.5 else 0
    halo(ctx, x, yb - h * 0.5, h * 0.9, coul, 0.35 * a)
    hanche = yb - h * 0.42
    epaule = yb - h * 0.78
    jl = h * 0.42
    for k, al in ((1, 1.0), (-1, 0.6)):
        trait(ctx, x, hanche, x + k * jl * math.sin(sw), hanche + jl * math.cos(sw), h * 0.06, coul, a * al)
    px = x + sens * h * 0.08 * course
    trait(ctx, x, hanche, px, epaule, h * 0.075, coul, a)
    bl = h * 0.33
    for k, al in ((-1, 1.0), (1, 0.6)):
        trait(ctx, px, epaule + 4, px + k * bl * math.sin(sw), epaule + bl * 0.95, h * 0.05, coul, a * al)
    disque(ctx, px + sens * 2, epaule - h * 0.13, h * 0.1, coul, a)
    disque(ctx, px + sens * h * 0.06, epaule - h * 0.15, h * 0.02, FOND_HAUT, a)


def thermometre(ctx, x, y, h, f, coul, a=1.0):
    rrect(ctx, x - 9, y, 18, h, 9)
    ctx.set_source_rgba(*PANNEAU2, a)
    ctx.fill()
    hh = (h - 10) * sat(f)
    rrect(ctx, x - 5, y + h - hh - 2, 10, hh + 2, 5)
    ctx.set_source_rgba(*coul, a)
    ctx.fill()
    disque(ctx, x, y + h + 10, 18, coul, a)


# ------------------------------------------------------------------ signaux (filtre adapte)
_sig = None


def signaux():
    """Chirp, enregistrement simule (direct + echo + bruit) et sortie du filtre adapte."""
    global _sig
    if _sig is None:
        from scipy.signal import hilbert
        fs, n = 48000, 1100
        T = 0.010
        tc = np.arange(int(T * fs)) / fs
        k = (6500 - 1500) / T
        w = np.ones_like(tc)
        u = tc / T
        w[u < 0.15] = 0.5 - 0.5 * np.cos(np.pi * u[u < 0.15] / 0.15)
        w[u > 0.85] = 0.5 - 0.5 * np.cos(np.pi * (1 - u[u > 0.85]) / 0.15)
        chirp = w * np.sin(2 * np.pi * (1500 * tc + 0.5 * k * tc * tc))
        rng = np.random.default_rng(4)
        x = 0.012 * rng.standard_normal(n)
        d0, d1 = 60, 60 + 280                       # direct, echo (5,83 ms a 1 m)
        x[d0:d0 + len(chirp)] += 0.9 * chirp
        x[d1:d1 + len(chirp)] += 0.28 * chirp
        corr = np.correlate(x, chirp, mode="full")[len(chirp) - 1:]
        env = np.abs(hilbert(corr))
        env /= env.max()
        _sig = (chirp, x, env, d0, d1)
    return _sig


def courbe(ctx, ys, x0, y0, w, h, coul, a=1.0, ep=2.5, remplir=False, prog=1.0):
    n = len(ys)
    m = max(2, int(n * sat(prog)))
    ctx.save()
    ctx.set_line_width(ep)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    for i in range(m):
        xx = x0 + w * i / (n - 1)
        yy = y0 - h * ys[i]
        if i == 0:
            ctx.move_to(xx, yy)
        else:
            ctx.line_to(xx, yy)
    if remplir:
        ctx.line_to(x0 + w * (m - 1) / (n - 1), y0)
        ctx.line_to(x0, y0)
        ctx.close_path()
        ctx.set_source_rgba(*coul, 0.18 * a)
        ctx.fill_preserve()
        ctx.new_path()
        for i in range(m):
            xx = x0 + w * i / (n - 1)
            yy = y0 - h * ys[i]
            if i == 0:
                ctx.move_to(xx, yy)
            else:
                ctx.line_to(xx, yy)
    ctx.set_source_rgba(*coul, a)
    ctx.stroke()
    ctx.restore()


# ================================================================== SCENES
GITHUB = "github.com/Edunts5962/Esp32-vitesse-du-son"


def s_intro(ctx, t, D):
    # un ping part de la gauche, frappe un mur a droite et revient
    hx, hy = 220, 540
    haut_parleur(ctx, hx, hy, 2.2, JAUNE, app(t, 0.2), (t * 1.5) % 1)
    xw = 1700
    mur(ctx, xw, 330, 750, app(t, 0.2) * (1 - app(t, 4.6, 1.0)) * 0.9, 1 if 2.0 < t < 2.5 else 0)
    if t < 4.6:
        u = t / 2.2
        if u < 1:
            r = (xw - hx - 40) * u
            for q in range(4):
                front(ctx, hx + 40, hy, r - q * 22, -0.55, 0.55, 5, JAUNE, 0.9 - q * 0.2, hmax=220)
        elif u < 2:
            rr = (xw - hx - 40) * (u - 1)
            for q in range(4):
                front(ctx, 2 * xw - hx - 40, hy, (xw - hx - 40) + rr - q * 22, math.pi - 0.55, math.pi + 0.55, 5, CYAN,
                      0.9 - q * 0.2, hmax=220)
    s = "LA VITESSE DU SON"
    police(ctx, 112, True, POLICE_TITRE)
    lt = ctx.text_extents(s).x_advance
    x = W / 2 - lt / 2
    halo(ctx, W / 2, 440, 700, MAGENTA, 0.30 * app(t, 3.0, 1.5))
    halo(ctx, W / 2, 440, 500, JAUNE, 0.22 * app(t, 3.0, 1.5))
    for i, ch in enumerate(s):
        a = app(t, 3.0 + i * 0.045, 0.45)
        pref = ctx.text_extents(s[:i]).x_advance if i else 0
        texte(ctx, ch, x + pref, 480 + 26 * (1 - a), 112, TEXTE, a, True, "g", POLICE_TITRE)
        police(ctx, 112, True, POLICE_TITRE)
    texte(ctx, "mesurée par écho avec un ESP32  ·  et l'effet Doppler", W / 2, 620, 46, JAUNE, app(t, 4.4), True, "c")
    texte(ctx, "Projet DIY  ·  physique  ·  haut-parleur + micro + un mur", W / 2, 690, 28, TEXTE2, app(t, 5.2), False, "c")
    a = app(t, 6.4, 0.8)
    if a > 0:
        texte(ctx, "c ≈ 343 m/s", W / 2 - 300, 880, 44, CYAN, a, True, "c", POLICE_TITRE)
        texte(ctx, "v = c · Δf / (2 f0)", W / 2 + 300, 880, 44, MAGENTA, app(t, 7.4, 0.8), True, "c", POLICE_TITRE)


def s_question(ctx, t, D):
    chapitre(ctx, t, D, 1, "LA QUESTION")
    texte(ctx, "Dans l'air, le son voyage à", W / 2, 280, 50, TEXTE, app(t, 0.3), True, "c")
    v = 343 * sortie((t - 0.8) / 2.4)
    a = app(t, 0.8)
    halo(ctx, W / 2, 400, 480, JAUNE, 0.35 * a)
    l = chiffres(ctx, f"{int(v)}", W / 2 - 60, 450, 150, JAUNE, a, "c")
    texte(ctx, "m/s", W / 2 - 60 + l / 2 + 30, 450, 64, TEXTE2, a, True)
    a2 = app(t, 5.0) * (1 - app(t, 8.8, 0.5))
    if a2 > 0:
        texte(ctx, "la lumière : 299 792 458 m/s", W / 2, 560, 40, CYAN, a2, True, "c")
        texte(ctx, "le son est ≈ 874 000 fois plus lent", W / 2, 620, 40, TEXTE2, app(t, 5.8) * a2, False, "c")
    a3 = app(t, 9.0) * (1 - app(t, 13.2, 0.5))
    if a3 > 0:
        x0, x1, y = 560, 1360, 690
        trait(ctx, x0, y, x1, y, 3, TEXTE2, a3)
        for k in range(11):
            xx = x0 + (x1 - x0) * k / 10
            trait(ctx, xx, y, xx, y - (26 if k % 5 == 0 else 14), 2, TEXTE2, a3)
        texte(ctx, "1 mètre", W / 2, y - 50, 34, TEXTE, a3, True, "c")
        u = ((t - 9.0) / 2.4) % 1
        xs = x0 + (x1 - x0) * u
        front(ctx, xs - 40, y - 14, 40, -0.9, 0.9, 5, JAUNE, a3)
        texte(ctx, "en 2,9 millisecondes", W / 2, 800, 60, JAUNE, a3, True, "c")
        texte(ctx, "1 ms = un millième de seconde", W / 2, 860, 30, TEXTE3, a3, False, "c")
    a4 = app(t, 13.4)
    if a4 > 0:
        texte(ctx, "c = 331,3 × √(1 + T / 273,15)", W / 2, 640, 54, TEXTE, a4, True, "c", POLICE_TITRE)
        for i, (T, cc) in enumerate(((0, "331,3"), (20, "343,2"), (40, "355,1"))):
            ai = app(t, 14.0 + i * 0.5)
            x = W / 2 - 420 + i * 420
            thermometre(ctx, x - 90, 720, 150, (T + 10) / 55, ORANGE if T else BLEU, ai)
            texte(ctx, f"{T} °C", x - 50, 790, 40, TEXTE, ai, True)
            texte(ctx, f"{cc} m/s", x - 50, 845, 36, JAUNE, ai, True)


def s_idee(ctx, t, D):
    chapitre(ctx, t, D, 2, "L'IDÉE : UN ÉCHO")
    titre(ctx, "Un aller-retour du son, jusqu'au mur", t)
    a = app(t, 0.5)
    (hx, hy), (mx, my) = carte_son(ctx, 170, 300, 1.0, a, 1 if (t * 0.6) % 1 < 0.1 else 0)
    xw = 1550
    flash = 0
    sx, sy = 170 + 240, 450
    a2 = app(t, 2.2)
    if a2 > 0:
        per = 3.2
        u = ((t - 2.2) / per) % 1
        dist = xw - sx
        if u < 0.5:
            r = dist * u * 2
            for q in range(4):
                front(ctx, sx, sy, r - q * 20, -0.6, 0.6, 5, JAUNE, a2 * (0.9 - 0.2 * q), hmax=170)
            if u > 0.47:
                flash = 1
        else:
            r = dist + dist * (u - 0.5) * 2
            for q in range(4):
                front(ctx, 2 * xw - sx, sy, r - q * 20, math.pi - 0.6, math.pi + 0.6, 5, CYAN, a2 * (0.9 - 0.2 * q),
                      hmax=170)
        texte(ctx, "aller" if u < 0.5 else "retour : l'écho", W / 2 + 60, 300, 32, JAUNE if u < 0.5 else CYAN, a2, True, "c")
        texte(ctx, "ralenti ≈ 500 fois", 1860, 140, 24, TEXTE3, a2, False, "d")
    mur(ctx, xw, 270, 650, a, flash)
    a3 = app(t, 4.0)
    yd = 690
    fleche(ctx, W / 2, yd, sx + 10, yd, 3, TEXTE2, a3)
    fleche(ctx, W / 2, yd, xw - 6, yd, 3, TEXTE2, a3)
    pastille(ctx, "distance d", W / 2, yd, 30, JAUNE, a3)
    # chronogramme
    a4 = app(t, 7.0)
    if a4 > 0:
        X0, X1, Y = 420, 1500, 935
        trait(ctx, X0, Y, X1, Y, 2, TEXTE3, a4)
        texte(ctx, "micro", X0 - 30, Y + 10, 26, TEXTE2, a4, True, "d")
        for xx, hh, col, nom, t0 in ((X0 + 80, 90, JAUNE, "son direct", 7.2), (X0 + 80 + 700, 45, CYAN, "écho", 8.2)):
            ai = app(t, t0)
            for k in range(-12, 13):
                amp = hh * math.exp(-(k / 7) ** 2) * math.cos(k * 1.3)
                trait(ctx, xx + k * 5, Y, xx + k * 5, Y - amp, 3, col, ai)
            texte(ctx, nom, xx, Y + 72, 28, col, ai, True, "c")
        a5 = app(t, 11.4)
        fleche(ctx, X0 + 80, Y - 112, X0 + 780, Y - 112, 3, TEXTE, a5)
        fleche(ctx, X0 + 780, Y - 112, X0 + 80, Y - 112, 3, TEXTE, a5)
        texte(ctx, "Δt = 2 × d / c", X0 + 430, Y - 128, 42, TEXTE, a5, True, "c", POLICE_TITRE)
        texte(ctx, "d = 1 m  →  Δt = 5,83 ms", 1800, Y - 140, 34, JAUNE, app(t, 16.0), True, "d")


def s_chrono(ctx, t, D):
    chapitre(ctx, t, D, 3, "LE CHRONOMÈTRE : FILTRE ADAPTÉ")
    chirp, x, env, d0, d1 = signaux()
    # 1. le tic : un glissando
    a = app(t, 0.5)
    panneau(ctx, 120, 150, 760, 250, a)
    texte(ctx, "Le « tic » émis : un glissando", 150, 200, 30, JAUNE, a, True)
    texte(ctx, "1 500 → 6 500 Hz  ·  10 ms", 150, 240, 24, TEXTE2, a, False)
    ys = 0.5 + 0.42 * chirp
    courbe(ctx, ys, 150, 380, 700, 120, JAUNE, a, 2, prog=sortie((t - 0.8) / 2.5))
    # 2. l'enregistrement
    a2 = app(t, 8.0)
    panneau(ctx, 940, 150, 860, 250, a2)
    texte(ctx, "Ce qu'entend le micro", 970, 200, 30, TEXTE, a2, True)
    texte(ctx, "48 000 échantillons par seconde", 970, 240, 24, TEXTE2, a2, False)
    ys2 = 0.5 + 0.42 * x / np.abs(x).max()
    courbe(ctx, ys2, 970, 380, 800, 120, BLEU, a2, 1.6, prog=sortie((t - 8.4) / 2.0))
    # 3. la sortie du filtre adapte
    a3 = app(t, 12.6)
    panneau(ctx, 120, 440, 1680, 420, a3, bord=VERT)
    texte(ctx, "Comparaison avec le tic (corrélation par FFT)  →  un pic par arrivée", 150, 495, 30, TEXTE, a3, True)
    X0, Y0, Wd, Hd = 160, 820, 1600, 280
    prog = sortie((t - 13.4) / 3.0)
    courbe(ctx, env, X0, Y0, Wd, Hd, VERT, a3, 2.5, remplir=True, prog=prog)
    n = len(env)
    a4 = app(t, 19.0)
    for (dd, nom, col) in ((d0, "son direct", JAUNE), (d1, "écho", CYAN)):
        k = dd + np.argmax(env[dd:dd + 40])
        xx = X0 + Wd * k / (n - 1)
        yy = Y0 - Hd * env[k]
        halo(ctx, xx, yy, 60, col, a4)
        texte(ctx, nom, xx + 18, yy + 8, 30, col, a4, True)
    k0 = d0 + np.argmax(env[d0:d0 + 40])
    k1 = d1 + np.argmax(env[d1:d1 + 40])
    xa, xb = X0 + Wd * k0 / (n - 1), X0 + Wd * k1 / (n - 1)
    fleche(ctx, xa, 560, xb, 560, 3, TEXTE, a4)
    fleche(ctx, xb, 560, xa, 560, 3, TEXTE, a4)
    texte(ctx, "Δt", (xa + xb) / 2, 545, 36, TEXTE, a4, True, "c")
    # 4. zoom : interpolation
    a5 = app(t, 23.6)
    if a5 > 0:
        bx, by, bw, bh = 1260, 900, 540, 160
        panneau(ctx, bx, by, bw, bh, a5, bord=CYAN)
        texte(ctx, "zoom sur un pic", bx + 20, by + 36, 24, CYAN, a5, True)
        pts = [0.62, 0.86, 1.0, 0.9, 0.66]
        for i, p in enumerate(pts):
            xx = bx + 120 + i * 70
            yy = by + bh - 20 - 90 * p
            trait(ctx, xx, by + bh - 20, xx, yy, 2, TEXTE3, a5)
            disque(ctx, xx, yy, 7, TEXTE, a5)
        ctx.save()
        ctx.set_line_width(2.5)
        for i in range(41):
            u = -2 + 4 * i / 40
            p = 1.003 - 0.085 * (u - 0.08) ** 2
            xx = bx + 260 + u * 70
            yy = by + bh - 20 - 90 * p
            ctx.line_to(xx, yy) if i else ctx.move_to(xx, yy)
        ctx.set_source_rgba(*JAUNE, a5)
        ctx.stroke()
        ctx.restore()
        texte(ctx, "1 échantillon = 20,8 µs", bx + 20, by + bh + 34, 24, TEXTE2, a5, False)
        texte(ctx, "sommet interpolé : ≈ 1 µs", bx + bw - 10, by + bh + 34, 24, JAUNE, a5, True, "d")
    texte(ctx, "Le son direct sert de référence : peu importent les retards de la chaîne audio.",
          140, 1010, 26, TEXTE2, app(t, 25.5) * (1 - a5) + 0, False)


def s_bruit(ctx, t, D):
    chapitre(ctx, t, D, 4, "LE BRUIT ET LA STATISTIQUE")
    titre(ctx, "30 pings par distance", t)
    rng = random.Random(9)
    X0, X1, Y = 360, 1560, 820
    mu, sig = 5828.0, 5.0
    vals = [rng.gauss(mu, sig) for _ in range(30)]
    lo, hi = mu - 4 * sig, mu + 4 * sig
    nb = 17
    bins = [0] * nb
    a = app(t, 0.4)
    trait(ctx, X0, Y, X1, Y, 2, TEXTE3, a)
    for k in range(5):
        v = lo + (hi - lo) * k / 4
        xx = X0 + (X1 - X0) * k / 4
        trait(ctx, xx, Y, xx, Y + 12, 2, TEXTE3, a)
        texte(ctx, fmt(v / 1000, 3) + " ms", xx, Y + 48, 26, TEXTE3, a, False, "c")
    hb = 60
    n_vis = int(30 * sat((t - 1.0) / 6.0))
    for i in range(n_vis):
        b = min(nb - 1, max(0, int((vals[i] - lo) / (hi - lo) * nb)))
        bins[b] += 1
    for b in range(nb):
        for k in range(bins[b]):
            xx = X0 + (X1 - X0) * (b + 0.5) / nb
            yy = Y - 20 - k * 44
            rrect(ctx, xx - 28, yy - 20, 56, 38, 8)
            ctx.set_source_rgba(*mix(VIOLET, JAUNE, k / 6), 0.9 * a)
            ctx.fill()
    if n_vis < 30 and t > 1.0:
        i = n_vis
        tt = ((t - 1.0) / 6.0 * 30) % 1
        xx = X0 + (X1 - X0) * (vals[i] - lo) / (hi - lo)
        disque(ctx, xx, 260 + 400 * tt, 12, JAUNE, a)
        halo(ctx, xx, 260 + 400 * tt, 50, JAUNE, a)
    a2 = app(t, 8.0)
    if a2 > 0:
        ctx.save()
        ctx.set_line_width(4)
        for i in range(121):
            u = -4 + 8 * i / 120
            xx = X0 + (X1 - X0) * (u + 4) / 8
            yy = Y - 20 - 6.3 * 44 * math.exp(-0.5 * u * u) * 0.9
            ctx.line_to(xx, yy) if i else ctx.move_to(xx, yy)
        ctx.set_source_rgba(*JAUNE, a2)
        ctx.stroke()
        ctx.restore()
        texte(ctx, "σ ≈ 5 µs pour un ping", 1700, 340, 34, TEXTE, a2, True, "d")
    a3 = app(t, 10.0)
    texte(ctx, "erreur de la moyenne : u = σ / √N", 1700, 410, 38, JAUNE, a3, True, "d", POLICE_TITRE)
    texte(ctx, "N = 30  →  u ≈ 1 µs  (≈ 0,2 mm de distance)", 1700, 460, 30, TEXTE2, app(t, 11.0), False, "d")


def s_droite(ctx, t, D):
    chapitre(ctx, t, D, 5, "LA DROITE QUI DONNE c")
    X0, X1, Y0, Y1 = 300, 1250, 230, 900
    dmax, tmax = 4.4, 26.0
    gx = lambda d: X0 + (X1 - X0) * d / dmax
    gy = lambda v: Y1 - (Y1 - Y0) * v / tmax
    a = app(t, 0.3)
    trait(ctx, X0, Y1, X1, Y1, 2, TEXTE3, a)
    trait(ctx, X0, Y1, X0, Y0, 2, TEXTE3, a)
    for d in range(5):
        texte(ctx, str(d), gx(d), Y1 + 40, 26, TEXTE3, a, False, "c")
    for v in range(0, 26, 5):
        texte(ctx, str(v), X0 - 16, gy(v) + 9, 26, TEXTE3, a, False, "d")
        trait(ctx, X0, gy(v), X1, gy(v), 1, GRILLE if False else TEXTE3, 0.18 * a)
    texte(ctx, "distance (m)", X1, Y1 + 80, 26, TEXTE2, a, False, "d")
    texte(ctx, "Δt (ms)", X0 - 20, Y0 - 24, 26, TEXTE2, a, False)
    c_mes = 345.3
    pente = 2000 / c_mes
    b = -0.10
    ds = [0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4]
    for i, d in enumerate(ds):
        ai = app(t, 0.6 + i * 0.6, 0.4)
        if ai <= 0:
            continue
        v = b + pente * d + 0.02 * math.sin(i * 2.1)
        halo(ctx, gx(d), gy(v), 34, CYAN, ai)
        disque(ctx, gx(d), gy(v), 9 * ai, CYAN, ai)
        disque(ctx, gx(d), gy(v), 4, BLANC, ai)
    pr = sortie((t - 6.0) / 1.4)
    if pr > 0:
        x_e = dmax * pr
        trait(ctx, gx(0), gy(b), gx(x_e), gy(b + pente * x_e), 14, JAUNE, 0.18)
        trait(ctx, gx(0), gy(b), gx(x_e), gy(b + pente * x_e), 4, JAUNE)
    PX = 1340
    a2 = app(t, 6.4)
    texte(ctx, "pente = 2 / c", PX, 330, 54, JAUNE, a2, True, "g", POLICE_TITRE)
    texte(ctx, "= 5,792 ms par mètre", PX, 390, 32, TEXTE2, app(t, 7.4), False)
    a3 = app(t, 10.0)
    panneau(ctx, PX - 30, 440, 520, 150, a3, bord=JAUNE)
    texte(ctx, "c = 2 / pente", PX, 505, 50, TEXTE, a3, True, "g", POLICE_TITRE)
    texte(ctx, "= 345,3 m/s", PX, 565, 44, JAUNE, app(t, 11.0), True)
    a4 = app(t, 13.6)
    texte_bloc(ctx, "Le décalage à l'origine (haut-parleur ↔ micro, retards de la chaîne audio) "
                    "ne change pas la pente.", PX, 650, 500, 26, TEXTE2, a4)
    a5 = app(t, 19.0)
    if a5 > 0:
        panneau(ctx, PX - 30, 780, 520, 200, a5, bord=ORANGE)
        thermometre(ctx, PX + 10, 805, 120, 0.62, ORANGE, a5)
        texte(ctx, "Thermomètre acoustique", PX + 60, 830, 30, ORANGE, a5, True)
        texte(ctx, "T = 273,15 × ((c/331,3)² − 1)", PX + 60, 880, 26, TEXTE, a5, False)
        texte(ctx, "= 23,6 °C", PX + 60, 935, 44, ORANGE, app(t, 20.5), True)


def s_doppler(ctx, t, D):
    chapitre(ctx, t, D, 6, "L'EFFET DOPPLER")
    titre(ctx, "Un son pur, renvoyé par quelqu'un qui bouge", t, y=160, taille=52)
    a = app(t, 0.6)
    (hx, hy), _ = carte_son(ctx, 110, 330, 0.9, a, 0.5 + 0.5 * math.sin(t * 6))
    sx, sy = 110 + 216, 490
    # phase : approche (7..13), eloignement (13..18), sinon immobile
    if t < 7.6:
        v = 0.0
    elif t < 13.2:
        v = 1.4
    elif t < 18.4:
        v = -1.4
    else:
        v = 1.4 if ((t - 18.4) % 8) < 4 else -1.4
    # position de la personne
    if t < 7.6:
        xp = 1500
    elif t < 13.2:
        xp = 1500 - (t - 7.6) * 70
    elif t < 18.4:
        xp = 1108 + (t - 13.2) * 70
    else:
        u = (t - 18.4) % 8
        xp = 1472 - u * 70 if u < 4 else 1192 + (u - 4) * 70
    LAM, VIT = 46, 160
    a2 = app(t, 3.0)
    if a2 > 0:
        rmax = xp - 60 - sx
        off = (t * VIT) % LAM
        r = off
        while r < rmax:
            front(ctx, sx, sy - 55, r, -0.6, 0.6, 3.5, JAUNE, a2 * (1 - 0.6 * r / rmax), False, hmax=85)
            r += LAM
        texte(ctx, "émis : f0 →", sx + 40, sy - 160, 34, JAUNE, a2, True)
    col = TEXTE2 if v == 0 else (CYAN if v > 0 else MAGENTA)
    lam2 = LAM * (1 - 0.22 * v)
    if a2 > 0 and v != 0:
        cx = xp - 60
        off = (t * VIT) % lam2
        r = off
        while r < cx - sx:
            front(ctx, cx, sy + 85, r, math.pi - 0.6, math.pi + 0.6, 3.5, col, a2 * (1 - 0.7 * r / (cx - sx)), False,
                  hmax=75)
            r += lam2
        texte(ctx, "← renvoyé, plus aigu : f0 + Δf" if v > 0 else "← renvoyé, plus grave : f0 − Δf", cx - 40, sy + 200, 34,
              col, a2, True, "d")
    bonhomme(ctx, xp, 690, 230, t * 7.5, col if v else TEXTE2, a, v)
    a3 = app(t, 18.2)
    panneau(ctx, 300, 780, 1320, 220, a3, bord=JAUNE)
    texte(ctx, "f' = f0 · (c + v) / (c − v)", 340, 850, 44, TEXTE, a3, True, "g", POLICE_TITRE)
    texte(ctx, "→  Δf ≈ 2 · v · f0 / c", 1000, 850, 44, JAUNE, app(t, 19.2), True, "g", POLICE_TITRE)
    texte(ctx, "Exemple : f0 = 10 kHz, v = 1,4 m/s (marche)  →  Δf ≈ 82 Hz", 340, 940, 34, TEXTE2,
          app(t, 23.0), False)


def s_fft(ctx, t, D):
    chapitre(ctx, t, D, 7, "MESURER LE DÉCALAGE : LA FFT")
    titre(ctx, "Le spectre autour de f0", t, y=160)
    X0, X1, Y0, Y1 = 220, 1700, 260, 640
    a = app(t, 0.6)
    panneau(ctx, X0 - 40, Y0 - 30, X1 - X0 + 80, Y1 - Y0 + 110, a)
    nb = 129
    rng = np.random.default_rng(2)
    bruit = -82 + 3 * rng.standard_normal(nb)
    pic = 14 + 2 * math.sin(t * 1.3)          # personne a +82 Hz ~ +14 cases
    bw = (X1 - X0) / nb
    lo, hi = -90, 0
    yv = lambda db: Y1 - (Y1 - Y0) * sat((db - lo) / (hi - lo))
    prog = sortie((t - 0.8) / 1.6)
    for j in range(nb):
        k = j - 64
        if abs(k) <= 4:
            db = -7 - 18 * abs(k) * abs(k) / 4
            col = JAUNE
        else:
            db = bruit[j] + 46 * math.exp(-((k - pic) / 1.8) ** 2) * app(t, 9.4) + 30 * math.exp(-((k - pic * 1.5) / 3) ** 2) * app(t, 9.4) * 0.4
            col = CYAN if db > -70 else mix(VIOLET, TEXTE3, 0.5)
        if j / nb > prog:
            continue
        x = X0 + j * bw
        y = yv(db)
        ctx.rectangle(x, y, bw - 1.5, Y1 - y)
        ctx.set_source_rgba(*col, 0.9 * a)
        ctx.fill()
    trait(ctx, X0, Y1, X1, Y1, 2, TEXTE3, a)
    for k, lab in ((-64, "−375 Hz"), (0, "f0"), (64, "+375 Hz")):
        texte(ctx, lab, X0 + (k + 64.5) * bw, Y1 + 40, 28, JAUNE if k == 0 else TEXTE3, a, True, "c")
    a2 = app(t, 9.4)
    xc = X0 + 64.5 * bw
    texte(ctx, "porteuse : le son direct, très fort", xc - 30, yv(-5) - 10, 30, JAUNE, app(t, 9.0), True, "d")
    xp = X0 + (64.5 + pic) * bw
    halo(ctx, xp, yv(-40), 80, CYAN, a2)
    texte(ctx, "écho de la personne : +82 Hz", xp + 30, yv(-38), 30, CYAN, a2, True)
    trait(ctx, xc, yv(-60), xp, yv(-60), 3, TEXTE, a2)
    texte(ctx, "Δf", (xc + xp) / 2, yv(-60) - 14, 30, TEXTE, a2, True, "c")
    a3 = app(t, 16.0)
    for i, s in enumerate(["FFT de 8 192 points, 12 fois par seconde",
                           "une case = 5,9 Hz  →  ≈ 0,1 m/s à 10 kHz",
                           "v = c · Δf / (2 f0 + Δf), avec le c mesuré à l'écho !"]):
        ai = app(t, 16.0 + i * 1.6)
        disque(ctx, 260, 830 + i * 62 - 10, 9, [CYAN, JAUNE, VERT][i], ai)
        texte(ctx, s, 290, 830 + i * 62, 34, TEXTE, ai, i == 2)


def s_materiel(ctx, t, D):
    chapitre(ctx, t, D, 8, "LE MATÉRIEL")
    titre(ctx, "Une seule carte, rien à câbler", t, y=160)
    a = app(t, 0.8)
    carte_esp(ctx, 220, 220, 560, 400, "ES3C28P", JAUNE, a, 0.6, (capture("c01_mesure_echo"), 1.25))
    texte(ctx, "ES3C28P — ESP32-S3", 500, 668, 34, JAUNE, a, True, "c")
    texte(ctx, "8 Mo de PSRAM · écran 2,8\" tactile · microSD", 500, 708, 24, TEXTE2, a, False, "c")
    a2 = app(t, 7.4)
    for i, (s, col) in enumerate([("Codec audio ES8311", MAGENTA), ("Haut-parleur intégré", JAUNE),
                                  ("Micro MEMS intégré", VERT), ("+ un mètre ruban et un mur", TEXTE)]):
        ai = app(t, 7.4 + i * 0.6)
        disque(ctx, 980, 300 + i * 80 - 10, 9, col, ai)
        texte(ctx, s, 1005, 300 + i * 80, 36, TEXTE if i < 3 else TEXTE2, ai, True)
    a3 = app(t, 13.6)
    panneau(ctx, 220, 760, 1480, 70, a3, bord=CYAN)
    texte(ctx, "Émission et enregistrement à 48 kHz sur la même horloge : le retard entre les deux est constant",
          960, 805, 26, CYAN, a3, True, "c")
    boites = [("Cœur 0", "son I2S en continu, FFT, analyse, envoi de l'image", MAGENTA),
              ("Cœur 1", "tactile, calculs, dessin de l'interface animée", JAUNE),
              ("PSRAM", "2 images (double tampon), tampons de la FFT", VIOLET)]
    for i, (n, d, col) in enumerate(boites):
        ai = app(t, 19.0 + i * 0.8)
        x = 220 + i * 500
        panneau(ctx, x, 870, 480, 130, ai, bord=col)
        texte(ctx, n, x + 30, 920, 32, col, ai, True)
        texte_bloc(ctx, d, x + 30, 962, 430, 23, TEXTE2, ai)


def s_logiciel(ctx, t, D):
    chapitre(ctx, t, D, 9, "LE LOGICIEL")
    titre(ctx, "Le code : Arduino, ESP32 3.x", t, y=160)
    lignes = [
        (0, "firmware/son_console/", "un seul programme pour la carte", JAUNE, True),
        (1, "son_console.ino", "codec ES8311, son I2S, écran, tactile, SD", TEXTE, False),
        (1, "dsp.cpp", "FFT, filtre adapté, analyse Doppler", TEXTE, False),
        (1, "son.cpp", "points, droite, c, passages, statistiques", TEXTE, False),
        (1, "interface.cpp", "les écrans animés et la règle tactile", TEXTE, False),
        (1, "gfx.cpp · polices.cpp", "moteur de dessin et police Inter", TEXTE, False),
        (0, "outils/", "simulateur PC, polices, cette vidéo", VIOLET, True),
    ]
    y = 300
    for i, (ind, nom, desc, col, dossier) in enumerate(lignes):
        a = app(t, 0.6 + i * 0.5)
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
        texte(ctx, desc, 1020, y, 28, TEXTE2, a, False)
        y += 84


DEMO = [
    ("s01_demarrage", "Démarrage", "Test du haut-parleur et du micro, puis l'écho s'affiche en direct."),
    ("s02_reglages", "Réglages", "Plan de mesure, pings par point, fréquence Doppler, température de la pièce."),
    ("s03_regle", "La règle tactile", "On indique la distance carte–mur, mesurée au mètre ruban."),
    ("s04_premiere_mesure", "Première mesure", "30 pings à 0,50 m : l'onde part, rebondit sur le mur et revient."),
    ("s05_revelation+s05_revelation_b", "Premier résultat", "Dès deux distances, la pente donne c."),
    ("s06_plan_accelere", "Le plan complet (×4)", "De 0,5 à 4 m : chaque pastille verte est une distance mesurée."),
    ("s07_courbe", "Courbe", "La droite Δt / distance, sa pente et le détail de chaque point."),
    ("s08_resultat", "Résultat", "c, son incertitude, l'écart à la théorie et la température de l'air."),
    ("s09_stats", "Échogramme", "Le pic du son direct, la fenêtre de recherche et l'écho."),
    ("s10_radar", "Radar Doppler", "Son continu à 10 kHz : quelqu'un s'approche."),
    ("s11_passages", "Passages", "Il repart (son plus grave), puis revient en courant."),
    ("s12_dop_courbe", "Vitesse dans le temps", "v(t) sur les 20 dernières secondes."),
    ("s13_dop_resultat", "Allures", "Vitesse maximale de chaque passage, de la marche à la course."),
    ("s14_dop_stats", "Spectre en direct", "La porteuse, l'écho décalé de Δf et le calcul de v."),
]
SCENE_DEMO = 10


def duree_demo():
    return sum(len(charger_segment(n)[0]) for n, _, _ in DEMO) / FPS


def s_demo(ctx, t, D):
    chapitre(ctx, t, D, 10, "DÉMONSTRATION SUR LA CARTE")
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
    texte(ctx, "ES3C28P · ESP32-S3 · ES8311", SX, SY - 28, 22, TEXTE3, a, True)
    surf, _keep = surface_rgb(imgs[fi])
    ctx.save()
    ctx.translate(SX, SY)
    ctx.scale(ECH, ECH)
    ctx.set_source_surface(surf, 0, 0)
    ctx.get_source().set_filter(cairo.FILTER_NEAREST)
    ctx.paint_with_alpha(a)
    ctx.restore()
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
    texte(ctx, "Images du programme réel (simulateur PC, même code que la carte, sons simulés)",
          SX + 480, SY + 812, 20, TEXTE3, a, False, "c")
    PX = 1250
    ae = app(t_etape, 0.0, 0.5)
    texte(ctx, f"ÉTAPE {k + 1} / {len(DEMO)}", PX, 240, 24, JAUNE if k < 9 else CYAN, a, True)
    texte(ctx, tit, PX + 30 * (1 - ae), 305, 52, TEXTE, ae, True, "g", POLICE_TITRE)
    texte_bloc(ctx, desc, PX, 365, 600, 30, TEXTE2, ae)
    y = 520
    for i, (_, ti, _) in enumerate(DEMO):
        if i == 9:
            y += 14
        fait, cour = i < k, i == k
        col = VERT if fait else ((JAUNE if i < 9 else CYAN) if cour else TEXTE3)
        if cour:
            halo(ctx, PX + 10, y - 7, 24, col, 0.8)
        disque(ctx, PX + 10, y - 7, 7, col, a)
        texte(ctx, ti, PX + 32, y, 22, TEXTE if cour else (TEXTE2 if fait else TEXTE3), a, cour)
        y += 34
    texte(ctx, "ÉCHO", 1840, 534, 18, JAUNE, 0.7 * a, True, "d")
    texte(ctx, "DOPPLER", 1840, 534 + 9 * 34 + 14, 18, CYAN, 0.7 * a, True, "d")


def s_protocole(ctx, t, D):
    chapitre(ctx, t, D, 11, "MODE OPÉRATOIRE")
    titre(ctx, "Mesurer c en classe ou en club", t, y=160)
    etapes = ["Carte posée face à un grand mur plat, rien entre les deux",
              "Mesurer la distance au mètre ruban (depuis le haut-parleur)",
              "Régler la règle, appuyer sur MESURER, rester silencieux",
              "Recommencer de 0,5 à 4 m (plan « 4 m »)",
              "Lire c dans Résultat, comparer à c(T)",
              "Doppler : marcher droit vers la carte, puis repartir",
              "Exploiter son.csv (carte SD) sur un tableur"]
    y = 280
    for i, e in enumerate(etapes):
        a = app(t, 0.8 + i * 1.3)
        x = 160 + 30 * (1 - a)
        disque(ctx, x + 26, y - 12, 26, JAUNE if i < 5 else CYAN, a)
        texte(ctx, str(i + 1), x + 26, y - 1, 28, c(6, 10, 30), a, True, "c")
        texte(ctx, e, x + 74, y, 30, TEXTE, a, False)
        y += 92
    a2 = app(t, 14.6)
    panneau(ctx, 1340, 260, 520, 520, a2, bord=ORANGE)
    texte(ctx, "Pièges à éviter", 1380, 330, 38, ORANGE, a2, True)
    pieges = ["Meubles ou personnes entre la carte et le mur", "Mur latéral trop proche (échos parasites)",
              "Bruit pendant la série", "Micro saturé : baisser le gain"]
    yy = 410
    for i, p in enumerate(pieges):
        ai = app(t, 15.2 + i * 0.9)
        trait(ctx, 1385, yy - 18, 1401, yy - 2, 4, ORANGE, ai)
        trait(ctx, 1401, yy - 18, 1385, yy - 2, 4, ORANGE, ai)
        yy = texte_bloc(ctx, p, 1420, yy, 410, 28, TEXTE, ai) + 26


def s_resultat(ctx, t, D):
    chapitre(ctx, t, D, 12, "RÉSULTAT")
    a = app(t, 0.3)
    carte_esp(ctx, 110, 170, 800, 700, "ES3C28P", JAUNE, a, 0, (capture("c08_resultat"), 2.3))
    texte(ctx, "Exemple de résultat", 1340, 260, 34, TEXTE2, app(t, 0.8), False, "c")
    a2 = app(t, 1.4)
    v = 345.3 * sortie((t - 1.4) / 1.8)
    halo(ctx, 1340, 380, 380, JAUNE, 0.45 * a2)
    chiffres(ctx, fmt(v, 1), 1340, 430, 130, TEXTE, a2, "c")
    texte(ctx, "m/s   ± 0,5", 1340, 500, 42, JAUNE, a2, True, "c")
    texte(ctx, "théorie à 23 °C : 345,0 m/s  ·  écart +0,09 %", 1340, 575, 32, VERT, app(t, 3.5), True, "c")
    a3 = app(t, 9.6)
    panneau(ctx, 1010, 640, 660, 130, a3, bord=CYAN)
    texte(ctx, "Doppler : marche 1,41 m/s · course 3,3 m/s", 1340, 720, 32, CYAN, a3, True, "c")
    texte_bloc(ctx, "Obtenu dans le simulateur (sons fabriqués et analysés par le même code que la carte). "
                    "À vous de faire la vraie mesure !",
               1010, 840, 660, 26, TEXTE3, app(t, 13.4), align="c")


def s_outro(ctx, t, D):
    bx = -200 + 2500 * sortie(t / 2.0)
    g = cairo.LinearGradient(0, 0, bx, 0)
    g.add_color_stop_rgba(0, *MAGENTA, 0)
    g.add_color_stop_rgba(1, *JAUNE, 0.8)
    ctx.set_source(g)
    ctx.rectangle(0, 638, max(0, min(bx, W)), 3)
    ctx.fill()
    if t < 2.2:
        halo(ctx, bx, 640, 140, JAUNE, 1)
    texte(ctx, "Tout est sur GitHub", W / 2, 420, 88, TEXTE, app(t, 0.6), True, "c", POLICE_TITRE)
    texte(ctx, "code  ·  tutoriel  ·  protocole pour étudiants  ·  simulateur", W / 2, 510, 36, JAUNE,
          app(t, 1.4), False, "c")
    pastille(ctx, GITHUB, W / 2, 580, 30, JAUNE, app(t, 2.0))
    texte(ctx, "Projet précédent : la vitesse de la lumière avec deux ESP32 (Wi-Fi FTM)", W / 2, 720, 28, CYAN,
          app(t, 2.6), False, "c")
    texte(ctx, "ESP32-S3  ·  Arduino  ·  ES8311  ·  voix Piper (SIWIS, CC-BY)  ·  police Inter (SIL OFL)", W / 2, 790,
          24, TEXTE3, app(t, 3.0), False, "c")
    texte(ctx, "Merci !", W / 2, 900, 60, JAUNE, app(t, 4.0), True, "c", POLICE_TITRE)


SCENES = [
    (s_intro, 14), (s_question, 18), (s_idee, 20), (s_chrono, 30), (s_bruit, 15), (s_droite, 25),
    (s_doppler, 28), (s_fft, 23), (s_materiel, 24), (s_logiciel, 8), (s_demo, None), (s_protocole, 21),
    (s_resultat, 17), (s_outro, 10),
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
        if r["scene"] == SCENE_DEMO:
            k = int(r["debut"])
            etapes[k] = max(etapes[k], 0.3 + r["duree"] + 0.9)
    ds = []
    for i, (_, d) in enumerate(SCENES):
        ds.append(sum(etapes) + 1.0 if d is None else d)
    places = []
    for i in range(len(SCENES)):
        fin = 0.0
        for r in sorted([r for r in repl if r["scene"] == i], key=lambda r: r["debut"]):
            if i == SCENE_DEMO:
                debut = sum(etapes[: int(r["debut"])]) + 0.3
            else:
                debut = max(r["debut"], fin + 0.35)
            fin = debut + r["duree"]
            places.append((i, debut, r))
        if repl and i != SCENE_DEMO:
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
    vues = [(2, 19.0, "principe_echo"), (3, 28.0, "principe_filtre_adapte"),
            (4, 13.0, "principe_statistique"), (5, 24.0, "principe_droite"),
            (6, 25.0, "principe_doppler"), (7, 21.0, "principe_fft"),
            (8, 22.0, "materiel_architecture"), (0, 9.0, "titre")]
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
    final = os.path.join(sortie_dir, "son_esp32_presentation.mp4")
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
    with open(os.path.join(sortie_dir, "son_esp32_presentation.fr.srt"), "w", encoding="utf-8") as f:
        for k, (t0, r) in enumerate(repliques, 1):
            f.write(f"{k}\n{hms(t0)} --> {hms(t0 + r['duree'] + 0.2)}\n{r['sous_titre']}\n\n")
    print(f"son : {chemin} ({total:.1f} s), {len(repliques)} repliques")
    return chemin


def video_finale(sortie_dir):
    muette = os.path.join(sortie_dir, "son_esp32_presentation.mp4")
    son = piste_son(sortie_dir)
    finale = os.path.join(sortie_dir, "son_esp32_presentation_voix.mp4")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", muette, "-i", son, "-c:v", "copy",
                    "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", finale], check=True)
    print("video avec voix :", finale)

def miniature(sortie_dir):
    """Vignette YouTube 1280x720."""
    MW, MH = 1280, 720
    surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    ctx = cairo.Context(surf)
    fond(ctx, 3.0)
    for q in range(5):
        front(ctx, 1180, 560, 180 + q * 70, -0.5, 0.5, 9, JAUNE, 0.9 - 0.15 * q)
    mur(ctx, 1760, 300, 860, 1, 1)
    for q in range(4):
        front(ctx, 2 * 1760 - 1180, 560, 1760 - 1180 + 120 + q * 70, math.pi - 0.35, math.pi + 0.35, 9, CYAN,
              0.8 - 0.15 * q)
    carte_esp(ctx, 120, 300, 900, 680, "ES3C28P", JAUNE, 1, 0.8, (capture("c08_resultat"), 2.1))
    texte(ctx, "LA VITESSE", 90, 170, 140, TEXTE, 1, True, "g", POLICE_TITRE)
    texte(ctx, "DU SON", 1080, 170, 140, JAUNE, 1, True, "g", POLICE_TITRE)
    halo(ctx, 1500, 120, 300, JAUNE, 0.4)
    pastille(ctx, "+ effet Doppler", 1460, 270, 54, CYAN, 1, True)
    texte(ctx, "343 m/s ?", 1460, 900, 90, JAUNE, 1, True, "c", POLICE_TITRE)
    texte(ctx, "avec un ESP32", 1460, 1010, 72, TEXTE, 1, True, "c", POLICE_TITRE)
    petite = cairo.ImageSurface(cairo.FORMAT_ARGB32, MW, MH)
    c2 = cairo.Context(petite)
    c2.scale(MW / W, MH / H)
    c2.set_source_surface(surf, 0, 0)
    c2.get_source().set_filter(cairo.FILTER_BEST)
    c2.paint()
    chemin = os.path.join(sortie_dir, "miniature_youtube.png")
    petite.write_to_png(chemin)
    print("miniature :", chemin)


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
    elif args and args[0] == "--miniature":
        miniature(sortie_dir)
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
