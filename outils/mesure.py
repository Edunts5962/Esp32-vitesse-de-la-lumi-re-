#!/usr/bin/env python3
"""
Mesurer la vitesse de la lumière avec deux ESP32-S3 (FTM Wi-Fi).

1) Acquisition : python mesure.py acquerir --port /dev/ttyACM0
   Le script te demande la distance (en mètres, mesurée au mètre ruban),
   enregistre N mesures de temps de vol, puis redemande une distance.
   Tape 'fin' pour terminer. Résultat dans mesures.csv.

2) Analyse : python mesure.py analyser mesures.csv
   Régression linéaire temps aller-retour = (2/c) * distance + décalage.
   Le décalage (retards internes des puces) s'élimine : seule la pente compte.
"""
import argparse
import csv
import statistics
import sys

C_REF = 299_792_458.0  # m/s


def acquerir(port, sortie, n, baud):
    import serial  # pip install pyserial
    ser = serial.Serial(port, baud, timeout=2)
    with open(sortie, "a", newline="") as f:
        w = csv.writer(f)
        if f.tell() == 0:
            w.writerow(["distance_m", "rtt_ps", "rssi"])
        while True:
            rep = input("\nDistance en mètres (ou 'fin') : ").strip().replace(",", ".")
            if rep.lower() == "fin":
                break
            try:
                d = float(rep)
            except ValueError:
                print("Nombre invalide.")
                continue
            ser.reset_input_buffer()
            nb = 0
            while nb < n:
                ligne = ser.readline().decode(errors="ignore").strip()
                if not ligne.startswith("FTM,"):
                    continue
                parts = ligne.split(",")
                if len(parts) != 5:
                    continue
                w.writerow([d, parts[3], parts[4]])
                nb += 1
                if nb % 50 == 0:
                    print(f"  {nb}/{n}", end="\r")
            f.flush()
            print(f"  {n} mesures enregistrées à {d} m")
    ser.close()


def filtrer(valeurs):
    """Garde les valeurs à moins de 3 MAD de la médiane (rejette les aberrations)."""
    med = statistics.median(valeurs)
    mad = statistics.median(abs(v - med) for v in valeurs) or 1.0
    return [v for v in valeurs if abs(v - med) <= 3 * 1.4826 * mad]


def analyser(fichier, graphe):
    par_distance = {}
    with open(fichier) as f:
        for row in csv.DictReader(f):
            par_distance.setdefault(float(row["distance_m"]), []).append(float(row["rtt_ps"]))

    if len(par_distance) < 3:
        sys.exit("Il faut au moins 3 distances différentes.")

    xs, ys = [], []
    print(f"{'distance (m)':>12} {'n':>6} {'RTT médian (ns)':>16}")
    for d in sorted(par_distance):
        v = filtrer(par_distance[d])
        rtt_ns = statistics.median(v) / 1000.0
        xs.append(d)
        ys.append(rtt_ns)
        print(f"{d:12.2f} {len(v):6d} {rtt_ns:16.2f}")

    # Régression linéaire y = a*x + b
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    a = sxy / sxx
    b = my - a * mx
    residus = [y - (a * x + b) for x, y in zip(xs, ys)]
    sigma_a = (sum(r * r for r in residus) / (n - 2) / sxx) ** 0.5 if n > 2 else 0.0

    if a <= 0:
        sys.exit("Pente négative ou nulle : mesures trop bruitées (multitrajets ?).")

    c = 2.0 / (a * 1e-9)              # a en ns/m, aller-retour
    sigma_c = c * sigma_a / a

    print(f"\nPente : {a:.3f} ± {sigma_a:.3f} ns/m (théorie : {2e9 / C_REF:.3f})")
    print(f"Décalage matériel : {b:.1f} ns")
    print(f"\n  c mesurée = {c / 1e6:.1f} ± {sigma_c / 1e6:.1f} × 10^6 m/s")
    print(f"  c réelle  = {C_REF / 1e6:.1f} × 10^6 m/s  (écart {100 * (c - C_REF) / C_REF:+.1f} %)")

    if graphe:
        import matplotlib.pyplot as plt
        plt.plot(xs, ys, "o", label="mesures (médianes)")
        plt.plot([0, max(xs)], [b, a * max(xs) + b], "-",
                 label=f"ajustement : c = {c / 1e6:.0f}×10⁶ m/s")
        plt.xlabel("distance (m)")
        plt.ylabel("temps aller-retour (ns)")
        plt.title("Vitesse de la lumière avec deux ESP32")
        plt.legend()
        plt.grid(alpha=0.3)
        plt.savefig("vitesse_lumiere.png", dpi=150)
        print("Graphe : vitesse_lumiere.png")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    pa = sp.add_parser("acquerir")
    pa.add_argument("--port", required=True)
    pa.add_argument("--sortie", default="mesures.csv")
    pa.add_argument("--n", type=int, default=1000, help="mesures par distance")
    pa.add_argument("--baud", type=int, default=115200)
    pn = sp.add_parser("analyser")
    pn.add_argument("fichier")
    pn.add_argument("--graphe", action="store_true")
    a = p.parse_args()
    if a.cmd == "acquerir":
        acquerir(a.port, a.sortie, a.n, a.baud)
    else:
        analyser(a.fichier, a.graphe)
