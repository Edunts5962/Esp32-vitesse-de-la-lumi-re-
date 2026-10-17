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
puis --audio fabrique la piste son (voix + musique douce) et les sous-titres.
"""
import json
import math
import os
import sys
import wave

import numpy as np

# (scene, debut souhaite en s, texte prononce, sous-titre)
# scene 8 = demonstration : debut = numero d'etape (0..9)
REPLIQUES = [
    (0, 1.6, "La lumière est ce qu'il y a de plus rapide dans l'Univers.", None),
    (0, 5.6, "Aujourd'hui, on va la chronométrer, avec deux petites cartes électroniques à quelques euros, et le Wi-Fi.", None),

    (1, 0.6, "La lumière voyage à près de trois cent mille kilomètres par seconde.", None),
    (1, 4.2, "C'est sept fois et demie le tour de la Terre, en une seule seconde.", None),
    (1, 8.6, "Pour traverser un mètre, il lui faut à peine trois virgule trente-quatre nano-secondes.",
     "Pour traverser un mètre, il lui faut à peine 3,34 nano-secondes."),
    (1, 13.8, "Alors, comment chronométrer ça, avec une carte à dix euros ?",
     "Alors, comment chronométrer ça, avec une carte à 10 € ?"),

    (2, 0.6, "L'idée : faire un aller-retour avec une onde radio.", None),
    (2, 2.6, "La carte A reste fixe. La carte B, la console à écran, est placée à une distance d.", None),
    (2, 6.8, "Les ondes Wi-Fi sont de la lumière invisible : elles voyagent exactement à la même vitesse.", None),
    (2, 11.0, "Le temps d'aller-retour vaut deux fois la distance, divisée par c.",
     "Le temps d'aller-retour vaut deux fois la distance, divisée par c."),
    (2, 15.5, "À dix mètres, cela fait soixante-six nano-secondes. On va mesurer ce temps à plusieurs distances.",
     "À 10 mètres, cela fait 66,7 nano-secondes. On va mesurer ce temps à plusieurs distances."),

    (3, 0.6, "Et le Wi-Fi sait mesurer ce temps ! C'est la mesure fine du temps, appelée F T M dans la norme Wi-Fi.",
     "Et le Wi-Fi sait mesurer ce temps ! C'est la mesure fine du temps, appelée FTM dans la norme Wi-Fi."),
    (3, 5.0, "La carte A envoie une trame, à l'instant t un. La carte B la reçoit à t deux.",
     "La carte A envoie une trame, à l'instant t1. La carte B la reçoit à t2."),
    (3, 9.0, "Puis la carte B renvoie un accusé de réception à t trois, reçu par A à t quatre.",
     "Puis la carte B renvoie un accusé de réception à t3, reçu par A à t4."),
    (3, 13.0, "On prend t quatre moins t un, et on retire le temps de réponse de B, t trois moins t deux. Il ne reste que le trajet de l'onde.",
     "On prend t4 − t1, et on retire le temps de réponse de B, t3 − t2. Il ne reste que le trajet de l'onde."),
    (3, 21.0, "L'horloge avance par paliers d'une nano-seconde et demie, environ vingt-trois centimètres.",
     "L'horloge avance par paliers de 1,5625 ns, environ 23 centimètres."),
    (3, 25.5, "La carte fait seize échanges par session, soit environ cent quinze mesures par seconde, et tout est intégré dans la puce.",
     "La carte fait 16 échanges par session, soit environ 115 mesures par seconde, et tout est intégré dans la puce."),

    (4, 0.6, "Problème : une seule mesure est imprécise.", None),
    (4, 2.6, "Chaque valeur tombe un peu à côté : plus ou moins quatre nano-secondes et demie, soit soixante-dix centimètres.",
     "Chaque valeur tombe un peu à côté : ± 4,5 ns, soit 70 centimètres."),
    (4, 7.6, "Mais en accumulant les mesures, l'histogramme dessine une courbe en cloche, centrée sur la vraie valeur.", None),
    (4, 13.0, "L'erreur de la moyenne diminue comme un sur racine de N.",
     "L'erreur de la moyenne diminue comme 1/√N."),
    (4, 16.8, "Avec six cents mesures, on descend à environ trois centimètres.",
     "Avec 600 mesures, on descend à environ 3 centimètres."),

    (5, 0.6, "On répète la mesure à huit distances, de deux à vingt mètres.",
     "On répète la mesure à 8 distances, de 2 à 20 mètres."),
    (5, 6.6, "Les points s'alignent sur une droite.", None),
    (5, 9.6, "Sa pente vaut deux divisé par c.", "Sa pente vaut 2 / c."),
    (5, 13.0, "Donc, c est égal à deux, divisé par la pente.", "Donc c = 2 / pente."),
    (5, 17.0, "Ici, on trouve environ trois cents millions de mètres par seconde.",
     "Ici, on trouve environ 300 millions de mètres par seconde."),
    (5, 20.6, "Et les petits retards fixes de l'électronique ne changent pas la pente : seule la pente compte.", None),

    (6, 0.6, "Côté matériel : la carte B est une console et S trois C vingt-huit P, avec une puce et S P trente-deux S trois, et un écran tactile.",
     "Côté matériel : la carte B est une console ES3C28P, avec une puce ESP32-S3 et un écran tactile."),
    (6, 6.6, "La carte A est une simple carte et S P trente-deux S trois, qui sert de répondeur.",
     "La carte A est une simple carte ESP32-S3, qui sert de répondeur."),
    (6, 10.4, "Attention : l'et S P trente-deux classique, et les premiers C six, ne savent pas faire de F T M.",
     "Attention : l'ESP32 classique et les premiers C6 ne savent pas faire de FTM."),
    (6, 14.6, "Dans la console, un cœur gère le Wi-Fi et l'écran, l'autre calcule et dessine l'interface.", None),

    (7, 0.6, "Tout le code est écrit pour Arduino : le programme des deux cartes, l'interface animée, les calculs, et même un simulateur pour PC.", None),

    (8, 0, "Au démarrage, la console cherche la carte A, s'y connecte, puis affiche le temps de vol en direct.", None),
    (8, 1, "Dans les réglages, on choisit la source des mesures, le plan de mesure, le nombre de mesures par point, et la luminosité.", None),
    (8, 2, "Une règle tactile permet d'indiquer la distance, mesurée au mètre ruban.", None),
    (8, 3, "Premier piège : trop près, à cinquante centimètres, une partie des mesures est bloquée à zéro. L'écran prévient, et on supprime ce point.",
     "Premier piège : trop près, à 50 cm, une partie des mesures est bloquée à zéro. L'écran prévient, et on supprime ce point."),
    (8, 4, "À deux mètres, la carte enregistre six cents mesures en cinq secondes, avec l'histogramme en direct.",
     "À 2 mètres, la carte enregistre 600 mesures en 5 secondes, avec l'histogramme en direct."),
    (8, 5, "Dès la deuxième distance, la console calcule une première valeur de c, et la dévoile à l'écran.", None),
    (8, 6, "On continue ainsi jusqu'à vingt mètres. Chaque pastille verte est une distance mesurée.",
     "On continue ainsi jusqu'à 20 mètres. Chaque pastille verte est une distance mesurée."),
    (8, 7, "L'onglet Courbe montre la droite, sa pente, et le détail de chaque point.", None),
    (8, 8, "L'onglet Résultat donne c, son incertitude, et l'écart à la vraie valeur.", None),
    (8, 9, "Enfin, l'onglet Stats montre tout ce que la carte a calculé : échantillons, sessions, rejets, et la distribution des temps de vol.", None),

    (9, 0.6, "Pour mesurer en classe : on se place dehors, ou dans un long couloir.", None),
    (9, 4.4, "Les cartes sont à un mètre du sol, antennes face à face, et la carte A reste fixe.",
     "Les cartes sont à 1 mètre du sol, antennes face à face, et la carte A reste fixe."),
    (9, 9.0, "On mesure la distance au mètre ruban, on la règle sur l'écran, et on lance la mesure, sans bouger.", None),
    (9, 14.0, "Pièges à éviter : moins de deux mètres, des murs trop proches, et des personnes entre les cartes.",
     "Pièges à éviter : moins de 2 mètres, des murs trop proches, et des personnes entre les cartes."),

    (10, 0.6, "Dans cet exemple, on obtient deux cent quatre-vingt-dix-huit virgule neuf millions de mètres par seconde : à zéro virgule trois pour cent de la valeur exacte.",
     "Dans cet exemple, on obtient 298,9 millions de mètres par seconde : à 0,3 % de la valeur exacte."),
    (10, 8.5, "Ce résultat vient du simulateur. À vous de faire la vraie mesure !", None),

    (11, 0.6, "Tout est sur guitt heub : le code, le tutoriel, et un protocole pour les étudiants.", None),
    (11, 5.6, "Merci d'avoir regardé !", None),
]

SR = 22050

# graphies "pour la voix" -> ecriture normale dans les sous-titres
_PROPRE = [("nano-second", "nanosecond"), ("pico-second", "picosecond"), ("guitt heub", "GitHub"),
           ("et S P trente-deux S trois", "ESP32-S3"), ("l'et S P trente-deux", "l'ESP32"),
           ("et S trois C vingt-huit P", "ES3C28P"), ("F T M", "FTM")]


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
