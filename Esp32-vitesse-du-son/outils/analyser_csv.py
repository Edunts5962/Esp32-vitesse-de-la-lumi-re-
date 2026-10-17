#!/usr/bin/env python3
"""
analyser_csv.py - exploite le fichier son.csv de la carte SD.

  python3 analyser_csv.py son.csv            -> c par regression, passages Doppler
  python3 analyser_csv.py son.csv --graphe   -> idem + graphique (matplotlib)
  python3 analyser_csv.py son.csv --source sim  (ou reel) -> ne garde qu'une source

Lignes lues :
  echo,distance_m,dt_us,snr_db,source
  passage,f0_Hz,vmax,vmoy,duree_s,sens,c,source
Meme methode que la carte : mediane, rejet a 3 ecarts absolus medians,
moyenne par distance, puis droite dt = a.d + b et c = 2 / a.
"""
import math
import sys
from collections import defaultdict


def lire(chemin, source=None):
    echos, passages = defaultdict(list), []
    for ligne in open(chemin, encoding="utf-8", errors="replace"):
        ligne = ligne.strip()
        if not ligne or ligne.startswith("#"):
            continue
        p = ligne.split(",")
        if source and p[-1] != source:
            continue
        try:
            if p[0] == "echo":
                echos[round(float(p[1]), 3)].append(float(p[2]))
            elif p[0] == "passage":
                passages.append(dict(f0=float(p[1]), vmax=float(p[2]), vmoy=float(p[3]), duree=float(p[4]),
                                     sens=int(p[5]), c=float(p[6])))
        except (ValueError, IndexError):
            pass
    return echos, passages


def point(vals):
    v = sorted(vals)
    n = len(v)
    med = v[n // 2]
    mad = sorted(abs(x - med) for x in v)[n // 2] or 0.5
    gardes = [x for x in vals if abs(x - med) <= 3 * 1.4826 * mad]
    k = len(gardes)
    moy = sum(gardes) / k
    sig = math.sqrt(sum((x - moy) ** 2 for x in gardes) / (k - 1)) if k > 1 else 0
    return moy, sig / math.sqrt(k) if k else 0, sig, k, n - k


def droite(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    a = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    b = my - a * mx
    res = [y - (a * x + b) for x, y in zip(xs, ys)]
    sa = math.sqrt(sum(r * r for r in res) / (n - 2) / sxx) if n > 2 else 0
    st = sum((y - my) ** 2 for y in ys)
    r2 = 1 - sum(r * r for r in res) / st if st else 1
    return a, b, sa, r2


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    source = None
    if "--source" in sys.argv:
        source = sys.argv[sys.argv.index("--source") + 1]
    echos, passages = lire(sys.argv[1], source)
    if echos:
        print("ECHO : points par distance")
        print(f"{'d (m)':>7} {'dt (ms)':>10} {'u (us)':>8} {'sigma (us)':>11} {'n':>4} {'rejets':>7}")
        xs, ys = [], []
        for d in sorted(echos):
            if len(echos[d]) < 5:
                continue
            moy, u, sig, k, rej = point(echos[d])
            xs.append(d)
            ys.append(moy)
            print(f"{d:7.3f} {moy / 1000:10.4f} {u:8.2f} {sig:11.2f} {k:4d} {rej:7d}")
        if len(xs) >= 2:
            a, b, sa, r2 = droite(xs, ys)
            c = 2e6 / a
            print(f"\npente a = {a / 1000:.5f} ms/m  (± {sa / 1000:.5f})   b = {b:.1f} us   R² = {r2:.6f}")
            print(f"c = 2 / a = {c:.2f} m/s  ± {c * sa / a:.2f}")
            t = 273.15 * ((c / 331.3) ** 2 - 1)
            print(f"temperature de l'air correspondante : {t:.1f} °C")
            if "--graphe" in sys.argv:
                import matplotlib.pyplot as plt
                plt.plot(xs, [y / 1000 for y in ys], "o", label="points")
                xx = [0, max(xs) * 1.05]
                plt.plot(xx, [(a * x + b) / 1000 for x in xx], "-", label=f"c = {c:.1f} m/s")
                plt.xlabel("distance (m)")
                plt.ylabel("Δt (ms)")
                plt.grid(alpha=0.3)
                plt.legend()
                plt.show()
    if passages:
        print(f"\nDOPPLER : {len(passages)} passages")
        for i, p in enumerate(passages, 1):
            sens = "approche" if p["sens"] > 0 else "eloignement"
            print(f"{i:3d}  f0 {p['f0']:.0f} Hz  vmax {p['vmax']:.2f} m/s ({p['vmax'] * 3.6:.1f} km/h)  "
                  f"vmoy {p['vmoy']:.2f}  {p['duree']:.1f} s  {sens}  (c = {p['c']:.1f})")


if __name__ == "__main__":
    main()
