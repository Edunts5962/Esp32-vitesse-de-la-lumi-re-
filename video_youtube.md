# Vidéo YouTube : « Mesurer la vitesse de la lumière avec deux ESP32 »

Fichier : [`video/lumiere_esp32_presentation.mp4`](video/lumiere_esp32_presentation.mp4), en 1920 × 1080, 30 images/s, d'une durée de **5 min 11 s**. La vidéo est **sans son** : tu peux enregistrer la voix off ci-dessous, puis ajouter une musique libre de droits (bibliothèque audio de YouTube Studio).

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
https://github.com/votre-compte/vitesse-lumiere-esp32

Chapitres
0:00 Introduction
0:14 La question : 3,34 ns par mètre
0:32 L'idée : un aller-retour d'onde radio
0:56 Le chronomètre caché du Wi-Fi : FTM
1:30 Le bruit et la statistique
1:54 La droite qui donne c
2:22 Le matériel
2:42 Le logiciel
2:54 Démonstration sur la carte
4:26 Mode opératoire pour une classe
4:48 Résultat
5:00 Fin

Matériel : ESP32-S3 (ES3C28P avec écran 2,8" tactile) + ESP32-S3 DevKit,
mètre ruban, 2 batteries USB. Arduino IDE, paquet ESP32 3.x.
Police Inter (SIL OFL).

#ESP32 #physique #vitessedelalumiere #DIY #Arduino #wifi #science
```

## Tags

ESP32, ESP32-S3, vitesse de la lumière, speed of light, Wi-Fi FTM, 802.11mc, fine timing measurement, Arduino, physique, expérience, DIY, électronique, TP physique, mesure, nanoseconde

---

## Script de la voix off (calé sur la vidéo)

| Temps | Ce qu'on voit | Voix off |
|---|---|---|
| **0:00** | Faisceau lumineux, titre | « La lumière est la chose la plus rapide de l'Univers. Aujourd'hui, on va la chronométrer… avec deux cartes électroniques à quelques euros. » |
| **0:14** | Compteur 299 792 458 m/s, la Terre | « La lumière parcourt près de trois cent mille kilomètres par seconde : sept fois et demie le tour de la Terre en une seconde. » |
| **0:22** | Règle de 1 m, photon | « Pour traverser un mètre, elle met 3,34 nanosecondes, soit trois milliardièmes de seconde. » |
| **0:28** | Question | « Alors comment chronométrer ça avec un microcontrôleur ? » |
| **0:32** | Carte A, carte B, onde | « L'idée : utiliser le Wi-Fi. Les ondes radio sont de la lumière invisible : elles voyagent exactement à la même vitesse. Une carte envoie un signal, l'autre répond. » |
| **0:44** | Formule 2d/c | « Le temps d'aller-retour vaut deux fois la distance divisée par c. À 10 mètres, cela fait 66,7 nanosecondes. » |
| **0:56** | Diagramme t1… t4 | « Et le Wi-Fi sait mesurer ce temps ! C'est le Fine Timing Measurement, prévu pour la géolocalisation en intérieur. La radio horodate chaque trame : départ t1, arrivée t2, réponse t3, retour t4. » |
| **1:10** | Formule | « On calcule t4 moins t1, et on retire le temps de réponse de l'autre carte, t3 moins t2. Il ne reste que le trajet de l'onde. » |
| **1:20** | Liste | « L'ESP32-S3 fait tout ça en matériel, par paliers de 1,56 nanoseconde, environ 115 fois par seconde. » |
| **1:30** | Histogramme qui se remplit | « Problème : une seule mesure est bruitée, à plus ou moins 4,5 nanosecondes, soit 70 centimètres. Mais en moyennant 600 mesures, l'erreur est divisée par racine de 600 : on descend à 3 centimètres. » |
| **1:54** | Points, puis droite | « On répète à huit distances, de 2 à 20 mètres. Les points s'alignent sur une droite. Sa pente vaut 2 sur c… donc c égale 2 divisé par la pente. Et les petits retards fixes de l'électronique ne changent pas la pente. » |
| **2:22** | Les cartes | « Le matériel : une console ES3C28P, avec un ESP32-S3 et un écran tactile, et une deuxième carte ESP32-S3 qui sert de répondeur. Plus un mètre ruban. Attention : l'ESP32 classique et les premiers ESP32-C6 ne savent pas faire de FTM. » |
| **2:35** | Cœurs et PSRAM | « Dans la console, un cœur s'occupe du Wi-Fi et de l'écran, l'autre calcule et dessine l'interface. » |
| **2:42** | Arborescence | « Tout le code est en Arduino et il est sur GitHub : le programme des deux cartes, un simulateur PC et cette vidéo. » |
| **2:54** | Démonstration : démarrage | « Au démarrage, la console cherche la carte A et s'y connecte. Le temps de vol s'affiche en direct. » |
| **3:03** | Réglages | « Dans les réglages : la source des mesures, le plan de mesure, le nombre de mesures par point, la luminosité. » |
| **3:17** | Règle | « On indique la distance mesurée au mètre ruban avec une règle tactile. » |
| **3:22** | Trop près | « Premier piège : trop près, à 50 centimètres, une partie des mesures est bloquée à zéro. L'écran prévient, et on supprime ce point. » |
| **3:35** | Première mesure | « À 2 mètres : 600 mesures en 5 secondes, avec l'histogramme en direct. » |
| **3:42** | Révélation | « Dès la deuxième distance, la console calcule une première valeur de c. » |
| **3:53** | Plan accéléré | « On continue jusqu'à 20 mètres… » |
| **4:07** | Courbe | « Les points s'alignent parfaitement, et chaque point garde ses statistiques. » |
| **4:12** | Résultat | « Résultat : 298,9 millions de mètres par seconde, à 0,3 % de la valeur exacte. » |
| **4:16** | Stats | « Et l'onglet Stats montre tout ce que la carte a calculé. » |
| **4:26** | Mode opératoire | « Pour le faire en classe : dehors, les cartes à un mètre du sol, antennes face à face, de 2 à 20 mètres, sans personne entre les cartes. » |
| **4:48** | Résultat | « Ce résultat vient du simulateur, calé sur de vraies mesures. À vous de faire la vraie mesure ! » |
| **5:00** | GitHub | « Tout est sur GitHub : le code, ce tutoriel et un TP clé en main. Merci d'avoir regardé ! » |

> Le résultat de 298,9 × 10⁶ m/s montré dans la vidéo vient du simulateur, dont les données sont calées sur de vraies mesures FTM entre deux ESP32-S3 : la vidéo le précise. Si tu filmes ta vraie mesure, remplace cette séquence par ton résultat.
