# Vidéo YouTube : « Mesurer la vitesse de la lumière avec deux ESP32 »

Fichier : [lumiere_esp32_presentation.mp4](https://github.com/Edunts5962/Esp32-vitesse-de-la-lumi-re-/releases/latest) (page Releases), en 1920 × 1080, 30 images/s, d'une durée de **5 min 36 s**, avec une **voix off française synthétisée** et une musique d'ambiance discrète. Les sous-titres sont dans lumiere_esp32_presentation.fr.srt (page Releases) (YouTube Studio > Sous-titres > Importer un fichier).

Miniature proposée (1280 × 720) : [`images/miniature_youtube.png`](images/miniature_youtube.png).

---

## Titre

**J'ai mesuré la vitesse de la lumière avec deux ESP32 (et le Wi-Fi)**

Variantes :
- *La vitesse de la lumière mesurée avec une carte à 10 € — ESP32-S3 + Wi-Fi FTM*
- *Chronométrer la lumière au milliardième de seconde avec un ESP32*

## Description (à coller)

```
Peut-on mesurer la vitesse de la lumière avec deux cartes ESP32 à quelques euros ?
Oui ! Le Wi-Fi cache un chronomètre au milliardième de seconde : le Fine Timing
Measurement (FTM, IEEE 802.11mc). En mesurant le temps d'aller-retour d'une onde
radio à plusieurs distances, la pente de la droite donne c.

Une console à écran tactile (ES3C28P, ESP32-S3) guide la mesure et affiche en
direct les courbes, les statistiques et le résultat.

📂 Code, tutoriel, protocole pour étudiants, simulateur :
https://github.com/Edunts5962/Esp32-vitesse-de-la-lumi-re-

Chapitres
0:00 Introduction
0:14 La question : 3,34 ns par mètre
0:33 L'idée : un aller-retour d'onde radio
1:00 Le chronomètre caché du Wi-Fi : FTM
1:38 Le bruit et la statistique
2:04 La droite qui donne c
2:32 Le matériel
3:01 Le logiciel
3:13 Démonstration sur la carte
4:48 Mode opératoire pour une classe
5:12 Résultat
5:26 Fin

Matériel : ESP32-S3 (ES3C28P avec écran 2,8" tactile) + ESP32-S3 DevKit,
mètre ruban, 2 batteries USB. Arduino IDE, paquet ESP32 3.x.
Police Inter (SIL OFL).

#ESP32 #physique #vitessedelalumiere #DIY #Arduino #wifi #science
```

## Tags

ESP32, ESP32-S3, vitesse de la lumière, speed of light, Wi-Fi FTM, 802.11mc, fine timing measurement, Arduino, physique, expérience, DIY, électronique, TP physique, mesure, nanoseconde

---

## Texte de la voix off (tel qu'il est dit dans la vidéo)

La voix est générée par `outils/video/narration.py`. Pour la changer, modifie le texte, puis relance `narration.py` et `faire_video.py` (voir l'en-tête des scripts). Si tu préfères enregistrer ta propre voix, garde les mêmes temps de départ.

| Temps | Texte |
|---|---|
| 0:01 | La lumière est ce qu'il y a de plus rapide dans l'Univers. |
| 0:05 | Aujourd'hui, on va la chronométrer, avec deux petites cartes électroniques à quelques euros, et le Wi-Fi. |
| 0:14 | La lumière voyage à près de trois cent mille kilomètres par seconde. |
| 0:18 | C'est sept fois et demie le tour de la Terre, en une seule seconde. |
| 0:22 | Pour traverser un mètre, il lui faut à peine 3,34 nanosecondes. |
| 0:27 | Alors, comment chronométrer ça, avec une carte à 10 € ? |
| 0:33 | L'idée : faire un aller-retour avec une onde radio. |
| 0:36 | La carte A reste fixe. La carte B, la console à écran, est placée à une distance d. |
| 0:43 | Les ondes Wi-Fi sont de la lumière invisible : elles voyagent exactement à la même vitesse. |
| 0:49 | Le temps d'aller-retour vaut deux fois la distance, divisée par c. |
| 0:53 | À 10 mètres, cela fait 66,7 nanosecondes. On va mesurer ce temps à plusieurs distances. |
| 1:01 | Et le Wi-Fi sait mesurer ce temps ! C'est la mesure fine du temps, appelée FTM dans la norme Wi-Fi. |
| 1:07 | La carte A envoie une trame, à l'instant t1. La carte B la reçoit à t2. |
| 1:12 | Puis la carte B renvoie un accusé de réception à t3, reçu par A à t4. |
| 1:17 | On prend t4 − t1, et on retire le temps de réponse de B, t3 − t2. Il ne reste que le trajet de l'onde. |
| 1:25 | L'horloge avance par paliers de 1,5625 ns, environ 23 centimètres. |
| 1:30 | La carte fait 16 échanges par session, soit environ 115 mesures par seconde, et tout est intégré dans la puce. |
| 1:39 | Problème : une seule mesure est imprécise. |
| 1:42 | Chaque valeur tombe un peu à côté : ± 4,5 ns, soit 70 centimètres. |
| 1:49 | Mais en accumulant les mesures, l'histogramme dessine une courbe en cloche, centrée sur la vraie valeur. |
| 1:55 | L'erreur de la moyenne diminue comme 1/√N. |
| 1:59 | Avec 600 mesures, on descend à environ 3 centimètres. |
| 2:04 | On répète la mesure à 8 distances, de 2 à 20 mètres. |
| 2:10 | Les points s'alignent sur une droite. |
| 2:13 | Sa pente vaut 2 / c. |
| 2:17 | Donc c = 2 / pente. |
| 2:21 | Ici, on trouve environ 300 millions de mètres par seconde. |
| 2:25 | Et les petits retards fixes de l'électronique ne changent pas la pente : seule la pente compte. |
| 2:32 | Côté matériel : la carte B est une console ES3C28P, avec une puce ESP32-S3 et un écran tactile. |
| 2:41 | La carte A est une simple carte ESP32-S3, qui sert de répondeur. |
| 2:47 | Attention : l'ESP32 classique et les premiers C6 ne savent pas faire de FTM. |
| 2:53 | Dans la console, un cœur gère le Wi-Fi et l'écran, l'autre calcule et dessine l'interface. |
| 3:01 | Tout le code est écrit pour Arduino : le programme des deux cartes, l'interface animée, les calculs, et même un simulateur pour PC. |
| 3:13 | Au démarrage, la console cherche la carte A, s'y connecte, puis affiche le temps de vol en direct. |
| 3:22 | Dans les réglages, on choisit la source des mesures, le plan de mesure, le nombre de mesures par point, et la luminosité. |
| 3:36 | Une règle tactile permet d'indiquer la distance, mesurée au mètre ruban. |
| 3:42 | Premier piège : trop près, à 50 cm, une partie des mesures est bloquée à zéro. L'écran prévient, et on supprime ce point. |
| 3:54 | À 2 mètres, la carte enregistre 600 mesures en 5 secondes, avec l'histogramme en direct. |
| 4:02 | Dès la deuxième distance, la console calcule une première valeur de c, et la dévoile à l'écran. |
| 4:12 | On continue ainsi jusqu'à 20 mètres. Chaque pastille verte est une distance mesurée. |
| 4:26 | L'onglet Courbe montre la droite, sa pente, et le détail de chaque point. |
| 4:32 | L'onglet Résultat donne c, son incertitude, et l'écart à la vraie valeur. |
| 4:38 | Enfin, l'onglet Stats montre tout ce que la carte a calculé : échantillons, sessions, rejets, et la distribution des temps de vol. |
| 4:49 | Pour mesurer en classe : on se place dehors, ou dans un long couloir. |
| 4:53 | Les cartes sont à 1 mètre du sol, antennes face à face, et la carte A reste fixe. |
| 4:59 | On mesure la distance au mètre ruban, on la règle sur l'écran, et on lance la mesure, sans bouger. |
| 5:05 | Pièges à éviter : moins de 2 mètres, des murs trop proches, et des personnes entre les cartes. |
| 5:12 | Dans cet exemple, on obtient 298,9 millions de mètres par seconde : à 0,3 % de la valeur exacte. |
| 5:21 | Ce résultat vient du simulateur. À vous de faire la vraie mesure ! |
| 5:26 | Tout est sur GitHub : le code, le tutoriel, et un protocole pour les étudiants. |
| 5:32 | Merci d'avoir regardé ! |

> Le résultat de 298,9 × 10⁶ m/s montré dans la vidéo vient du simulateur, dont les données sont calées sur de vraies mesures FTM entre deux ESP32-S3 : la vidéo le dit. Si tu filmes ta vraie mesure, remplace cette séquence par ton résultat.
