#!/usr/bin/env python3
"""
narration.py - voix off francaise synthetisee pour la video (Piper TTS).

Voix : Piper fr_FR-siwis-medium (jeu de donnees SIWIS, licence CC-BY 4.0),
version publiee par le projet sherpa-onnx (gere les voyelles nasales).

Les textes sont ecrits pour etre BIEN PRONONCES (sigles epeles :
"E S P trente-deux"), les sous-titres utilisent l'ecriture normale.

  pip install piper-tts numpy
  python3 narration.py <modele.onnx> <dossier_sortie>     -> wav + narration.json

faire_video.py lit narration.json pour caler les sequences sur la voix,
puis --final fabrique la piste son (voix + musique douce) et les sous-titres.
"""
import json
import os
import sys
import wave

# (scene, debut souhaite en s, texte prononce, sous-titre)
# scene 10 = demonstration : debut = numero d'etape (0..13)
REPLIQUES = [
    (0, 1.6, "Le son est bien plus lent que la lumière.", None),
    (0, 4.6, "Aujourd'hui, on va mesurer sa vitesse avec une seule carte électronique, son haut-parleur, son micro… et un mur.", None),
    (0, 11.0, "Puis on mesurera la vitesse d'une personne, grâce à l'effet Doppler.", None),

    (1, 0.6, "Dans l'air, le son parcourt environ trois cent quarante-trois mètres par seconde.",
     "Dans l'air, le son parcourt environ 343 mètres par seconde."),
    (1, 5.2, "C'est presque un million de fois plus lent que la lumière.", None),
    (1, 9.0, "Pour traverser un mètre, il lui faut deux virgule neuf millisecondes.",
     "Pour traverser un mètre, il lui faut 2,9 millisecondes."),
    (1, 13.4, "Et sa vitesse dépend de la température de l'air.", None),

    (2, 0.6, "L'idée : un écho.", None),
    (2, 2.4, "La carte émet un petit tic. Le micro l'entend tout de suite : c'est le son direct.", None),
    (2, 7.4, "Puis le son rebondit sur un mur, et revient : c'est l'écho.", None),
    (2, 11.4, "Le retard entre les deux vaut deux fois la distance, divisée par c.", None),
    (2, 16.0, "À un mètre du mur, cela fait cinq virgule huit millisecondes.",
     "À 1 mètre du mur, cela fait 5,8 millisecondes."),

    (3, 0.6, "Pour chronométrer précisément, le tic n'est pas un simple bip : c'est un glissando, de mille cinq cents à six mille cinq cents hertz.",
     "Pour chronométrer précisément, le tic n'est pas un simple bip : c'est un glissando, de 1 500 à 6 500 hertz."),
    (3, 8.6, "Le micro enregistre quarante-huit mille échantillons par seconde.",
     "Le micro enregistre 48 000 échantillons par seconde."),
    (3, 12.6, "La carte compare l'enregistrement avec le tic, grâce à une transformée de Fourier : c'est le filtre adapté.", None),
    (3, 19.0, "Chaque arrivée du tic devient un pic très fin : le son direct, puis l'écho.", None),
    (3, 23.6, "En interpolant entre les échantillons, on repère chaque pic à environ une microseconde près.",
     "En interpolant entre les échantillons, on repère chaque pic à environ 1 microseconde près."),

    (4, 0.6, "Comme toute mesure, chaque écho tombe un peu à côté.", None),
    (4, 3.8, "La carte envoie trente pings par distance, rejette les valeurs aberrantes, et fait la moyenne.",
     "La carte envoie 30 pings par distance, rejette les valeurs aberrantes, et fait la moyenne."),
    (4, 9.8, "L'erreur de la moyenne diminue comme un sur racine de N : il reste quelques microsecondes.",
     "L'erreur de la moyenne diminue comme 1/√N : il reste quelques microsecondes."),

    (5, 0.6, "On répète la mesure à huit distances, de cinquante centimètres à quatre mètres.",
     "On répète la mesure à 8 distances, de 50 centimètres à 4 mètres."),
    (5, 6.0, "Les points s'alignent sur une droite, de pente deux sur c.", "Les points s'alignent sur une droite, de pente 2/c."),
    (5, 10.0, "Donc, c est égal à deux, divisé par la pente.", "Donc c = 2 / pente."),
    (5, 13.6, "L'écart entre le haut-parleur et le micro décale la droite, mais ne change pas sa pente.", None),
    (5, 19.0, "Et en bonus, la vitesse du son dépend de la température : on peut donc en déduire la température de l'air. Un thermomètre acoustique !", None),

    (6, 0.6, "Deuxième expérience : l'effet Doppler.", None),
    (6, 3.0, "La carte émet maintenant un son pur, de fréquence f zéro, en continu.",
     "La carte émet maintenant un son pur, de fréquence f0, en continu."),
    (6, 7.6, "Quand quelqu'un s'approche, il renvoie des ondes plus serrées : le son revient plus aigu.", None),
    (6, 13.2, "Quand il s'éloigne, les ondes sont étirées : le son revient plus grave.", None),
    (6, 18.2, "Le décalage vaut à peu près deux fois v, fois f zéro, divisé par c.",
     "Le décalage vaut à peu près Δf ≈ 2·v·f0 / c."),
    (6, 23.0, "À dix kilohertz, quelqu'un qui marche à un mètre quarante par seconde décale le son de quatre-vingt-deux hertz.",
     "À 10 kHz, quelqu'un qui marche à 1,4 m/s décale le son de 82 hertz."),

    (7, 0.6, "Pour voir ce petit décalage, la carte calcule douze spectres par seconde, avec une transformée de Fourier de huit mille cent quatre-vingt-douze points.",
     "Pour voir ce petit décalage, la carte calcule 12 spectres par seconde, avec une transformée de Fourier de 8 192 points."),
    (7, 9.4, "La porteuse, qui vient directement du haut-parleur, est très forte. L'écho de la personne apparaît juste à côté.", None),
    (7, 16.0, "La résolution est d'environ six hertz, soit dix centimètres par seconde.",
     "La résolution est d'environ 6 hertz, soit 10 centimètres par seconde."),
    (7, 20.6, "Et pour le calcul, la carte utilise la vitesse du son qu'elle vient de mesurer !", None),

    (8, 0.6, "Côté matériel, une seule carte suffit : la console et S trois C vingt-huit P, avec une puce et S P trente-deux S trois.",
     "Côté matériel, une seule carte suffit : la console ES3C28P, avec une puce ESP32-S3."),
    (8, 7.4, "Elle a un écran tactile, une carte SD, et surtout un codec audio, avec un haut-parleur et un micro intégrés.", None),
    (8, 13.6, "Le son est émis et enregistré avec la même horloge : le retard entre les deux reste constant.", None),
    (8, 19.0, "Un cœur s'occupe du son et des calculs, l'autre dessine l'interface.", None),

    (9, 0.6, "Tout le code est écrit pour Arduino, et le même code tourne aussi dans un simulateur sur PC.", None),

    (10, 0, "Au démarrage, la carte teste son haut-parleur et son micro, puis affiche l'écho en direct.", None),
    (10, 1, "Dans les réglages, on choisit le plan de mesure, le nombre de pings, la fréquence Doppler, et la température de la pièce.", None),
    (10, 2, "Une règle tactile indique la distance entre la carte et le mur, mesurée au mètre ruban.", None),
    (10, 3, "À cinquante centimètres, la carte envoie trente pings. On voit l'onde partir, rebondir sur le mur, et revenir.",
     "À 50 cm, la carte envoie 30 pings. On voit l'onde partir, rebondir sur le mur, et revenir."),
    (10, 4, "Dès la deuxième distance, la pente donne une première valeur de c.", None),
    (10, 5, "On continue jusqu'à quatre mètres. Chaque pastille verte est une distance mesurée.",
     "On continue jusqu'à 4 mètres. Chaque pastille verte est une distance mesurée."),
    (10, 6, "L'onglet Courbe montre la droite, et le détail de chaque point.", None),
    (10, 7, "Le résultat : c, son incertitude, l'écart à la théorie, et la température de l'air déduite de c.", None),
    (10, 8, "L'onglet Stats montre l'échogramme : le pic du son direct, la fenêtre de recherche, et l'écho.", None),
    (10, 9, "On passe en mode Doppler, et on lance le radar. Quelqu'un s'approche : la vitesse s'affiche, et la trace monte sur le spectrogramme.", None),
    (10, 10, "Il repart : le son revient plus grave. Puis il revient en courant !", None),
    (10, 11, "La courbe montre la vitesse pendant les vingt dernières secondes.",
     "La courbe montre la vitesse pendant les 20 dernières secondes."),
    (10, 12, "Chaque passage est enregistré, avec sa vitesse maximale, et son allure.", None),
    (10, 13, "Enfin, l'onglet Stats montre le spectre en direct, avec le calcul complet de la vitesse.", None),

    (11, 0.6, "Pour mesurer en classe : la carte est posée face à un grand mur plat, sans obstacle.", None),
    (11, 5.2, "On mesure la distance au mètre ruban, on la règle sur l'écran, et on lance la mesure, en silence.", None),
    (11, 10.8, "Pour le Doppler, on marche droit vers la carte, puis on repart.", None),
    (11, 14.6, "Pièges à éviter : les meubles, les murs latéraux trop proches, le bruit, et un micro saturé.", None),

    (12, 0.6, "Dans cet exemple, on obtient trois cent quarante-cinq virgule trois mètres par seconde : à zéro virgule un pour cent de la théorie, à vingt-trois degrés.",
     "Dans cet exemple, on obtient 345,3 mètres par seconde : à 0,1 % de la théorie, à 23 °C."),
    (12, 9.6, "Et des vitesses de marche autour d'un mètre et demi par seconde.", None),
    (12, 13.4, "Ces résultats viennent du simulateur. À vous de faire la vraie mesure !", None),

    (13, 0.6, "Tout est sur guitt heub : le code, le tutoriel, et un protocole pour les étudiants.", None),
    (13, 5.6, "Merci d'avoir regardé !", None),
]

# graphies "pour la voix" -> ecriture normale dans les sous-titres
_PROPRE = [("guitt heub", "GitHub"), ("et S P trente-deux S trois", "ESP32-S3"),
           ("et S trois C vingt-huit P", "ES3C28P"), ("f zéro", "f0")]


def propre(s):
    for a, b in _PROPRE:
        s = s.replace(a, b)
    return s


VITESSE = 1.05      # > 1 : plus lent (length_scale de Piper)


def synthetiser(modele, sortie):
    from piper import PiperVoice
    from piper.config import SynthesisConfig
    voix = PiperVoice.load(modele)
    cfg = SynthesisConfig(length_scale=VITESSE)
    infos = []
    for i, (sc, debut, dit, st) in enumerate(REPLIQUES):
        chemin = os.path.join(sortie, f"r{i:02d}.wav")
        with wave.open(chemin, "wb") as w:
            voix.synthesize_wav(dit, w, syn_config=cfg)
        with wave.open(chemin) as w:
            duree = w.getnframes() / w.getframerate()
        infos.append({"scene": sc, "debut": debut, "dit": dit, "sous_titre": propre(st or dit),
                      "fichier": os.path.basename(chemin), "duree": round(duree, 3)})
        print(f"{i:02d} scene {sc:2d}  {duree:5.2f} s  {dit[:60]}")
    json.dump(infos, open(os.path.join(sortie, "narration.json"), "w"), ensure_ascii=False, indent=1)


if __name__ == "__main__":
    os.makedirs(sys.argv[2], exist_ok=True)
    synthetiser(sys.argv[1], sys.argv[2])
