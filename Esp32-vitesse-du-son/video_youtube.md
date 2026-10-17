## Titre

**J'ai mesuré la vitesse du son avec un ESP32 (un haut-parleur, un micro et un mur)**

Variantes :
- *343 m/s ? Mesurer la vitesse du son par écho avec un ESP32-S3 + l'effet Doppler*
- *Un radar Doppler et un sonar avec une carte ESP32 à écran — TP de physique DIY*
- *Après la lumière, le son : je chronomètre un écho à la microseconde avec un ESP32*

## Description (à coller)

```
Combien de temps met le son pour aller jusqu'à un mur et revenir ?
Une seule carte ESP32-S3 à écran tactile (ES3C28P), son haut-parleur et son
micro intégrés suffisent à le mesurer à la microseconde près.

La carte émet un « tic » (un glissando de 1,5 à 6,5 kHz), entend le son direct
puis l'écho du mur, et compare les deux par FFT (filtre adapté). En répétant la
mesure à plusieurs distances, la pente de la droite donne c… et c donne la
température de l'air !

Deuxième expérience : l'effet Doppler. La carte émet un son pur ; quelqu'un qui
marche devant renvoie un son un peu plus aigu ou plus grave. Le décalage
(≈ 82 Hz pour 1,4 m/s à 10 kHz) donne sa vitesse, en direct, avec un
spectrogramme.

📂 Code, tutoriel, protocole pour étudiants, simulateur :
https://github.com/Edunts5962/Esp32-vitesse-du-son

⚡ Le projet précédent, la vitesse de la lumière avec deux ESP32 (Wi-Fi FTM) :
https://github.com/Edunts5962/Esp32-vitesse-de-la-lumi-re-

Chapitres
0:00 Introduction
0:16 La question : 343 m/s
0:34 L'idée : un écho
0:56 Le chronomètre : le filtre adapté
1:27 Le bruit et la statistique
1:43 La droite qui donne c (et la température)
2:10 L'effet Doppler
2:40 Mesurer le décalage : la FFT
3:07 Le matériel
3:34 Le logiciel
3:42 Démonstration : écho
4:54 Démonstration : radar Doppler
5:31 Mode opératoire pour une classe
5:54 Résultat
6:13 Fin

Matériel : une carte ES3C28P (ESP32-S3, écran 2,8" tactile, codec ES8311,
haut-parleur et micro intégrés), un mètre ruban, un mur.
Arduino IDE, paquet ESP32 3.x.
Voix de synthèse : Piper, voix SIWIS (CC-BY 4.0). Police Inter (SIL OFL).
Les écrans de la démonstration viennent du simulateur PC (même code que la
carte, sons simulés).

#ESP32 #physique #vitesseduson #effetDoppler #DIY #Arduino #science
```

## Tags

ESP32, ESP32-S3, vitesse du son, speed of sound, effet Doppler, Doppler effect, écho, sonar, radar Doppler, FFT, filtre adapté, matched filter, chirp, ES8311, ES3C28P, Arduino, physique, TP physique, expérience, DIY, électronique, mesure, thermomètre acoustique

## Fichiers à téléverser

| Fichier | Rôle |
|---|---|
| `son_esp32_presentation_voix.mp4` | la vidéo (1920 × 1080, 30 i/s, avec la voix off et la musique) |
| `son_esp32_presentation.fr.srt` | sous-titres français (YouTube › Sous-titres › Importer un fichier) |
| `images/miniature_youtube.png` | miniature (1280 × 720) |

Catégorie : **Science et technologie**. Public : **non destiné aux enfants**. Langue : français.

Tous ces fichiers se refabriquent depuis le dépôt (voir `outils/video/`) :
```
cd outils/simulateur && g++ -std=c++17 -O2 -I../../firmware/son_console sim_tuto.cpp \
   ../../firmware/son_console/{gfx,son,dsp,moteur,interface,polices}.cpp -o sim_tuto && ./sim_tuto tournage
cd ../video
python3 narration.py fr_FR-siwis-medium.onnx voix
for i in $(seq 0 13); do python3 faire_video.py ../simulateur/tournage sortie $i --voix voix; done
python3 faire_video.py ../simulateur/tournage sortie --assembler --voix voix
python3 faire_video.py ../simulateur/tournage sortie --final --voix voix
python3 faire_video.py ../simulateur/tournage sortie --miniature --voix voix
```

---

## Texte de la voix off (tel qu'il est dit dans la vidéo)

La voix est générée par `outils/video/narration.py`. Pour la changer, modifie le texte, puis relance `narration.py` et `faire_video.py`. Si tu préfères enregistrer ta propre voix, garde les mêmes temps de départ.

| Temps | Texte |
|---|---|
| 0:01 | Le son est bien plus lent que la lumière. |
| 0:04 | Aujourd'hui, on va mesurer sa vitesse avec une seule carte électronique, son haut-parleur, son micro… et un mur. |
| 0:11 | Puis on mesurera la vitesse d'une personne, grâce à l'effet Doppler. |
| 0:17 | Dans l'air, le son parcourt environ 343 mètres par seconde. |
| 0:21 | C'est presque un million de fois plus lent que la lumière. |
| 0:25 | Pour traverser un mètre, il lui faut 2,9 millisecondes. |
| 0:29 | Et sa vitesse dépend de la température de l'air. |
| 0:35 | L'idée : un écho. |
| 0:37 | La carte émet un petit tic. Le micro l'entend tout de suite : c'est le son direct. |
| 0:42 | Puis le son rebondit sur un mur, et revient : c'est l'écho. |
| 0:46 | Le retard entre les deux vaut deux fois la distance, divisée par c. |
| 0:51 | À 1 mètre du mur, cela fait 5,8 millisecondes. |
| 0:56 | Pour chronométrer précisément, le tic n'est pas un simple bip : c'est un glissando, de 1 500 à 6 500 hertz. |
| 1:04 | Le micro enregistre 48 000 échantillons par seconde. |
| 1:08 | La carte compare l'enregistrement avec le tic, grâce à une transformée de Fourier : c'est le filtre adapté. |
| 1:15 | Chaque arrivée du tic devient un pic très fin : le son direct, puis l'écho. |
| 1:20 | En interpolant entre les échantillons, on repère chaque pic à environ 1 microseconde près. |
| 1:27 | Comme toute mesure, chaque écho tombe un peu à côté. |
| 1:30 | La carte envoie 30 pings par distance, rejette les valeurs aberrantes, et fait la moyenne. |
| 1:36 | L'erreur de la moyenne diminue comme 1/√N : il reste quelques microsecondes. |
| 1:43 | On répète la mesure à 8 distances, de 50 centimètres à 4 mètres. |
| 1:49 | Les points s'alignent sur une droite, de pente 2/c. |
| 1:53 | Donc c = 2 / pente. |
| 1:56 | L'écart entre le haut-parleur et le micro décale la droite, mais ne change pas sa pente. |
| 2:02 | Et en bonus, la vitesse du son dépend de la température : on peut donc en déduire la température de l'air. Un thermomètre acoustique ! |
| 2:11 | Deuxième expérience : l'effet Doppler. |
| 2:14 | La carte émet maintenant un son pur, de fréquence f0, en continu. |
| 2:19 | Quand quelqu'un s'approche, il renvoie des ondes plus serrées : le son revient plus aigu. |
| 2:24 | Quand il s'éloigne, les ondes sont étirées : le son revient plus grave. |
| 2:29 | Le décalage vaut à peu près Δf ≈ 2·v·f0 / c. |
| 2:33 | À 10 kHz, quelqu'un qui marche à 1,4 m/s décale le son de 82 hertz. |
| 2:41 | Pour voir ce petit décalage, la carte calcule 12 spectres par seconde, avec une transformée de Fourier de 8 192 points. |
| 2:50 | La porteuse, qui vient directement du haut-parleur, est très forte. L'écho de la personne apparaît juste à côté. |
| 2:57 | La résolution est d'environ 6 hertz, soit 10 centimètres par seconde. |
| 3:02 | Et pour le calcul, la carte utilise la vitesse du son qu'elle vient de mesurer ! |
| 3:08 | Côté matériel, une seule carte suffit : la console ES3C28P, avec une puce ESP32-S3. |
| 3:16 | Elle a un écran tactile, une carte SD, et surtout un codec audio, avec un haut-parleur et un micro intégrés. |
| 3:23 | Le son est émis et enregistré avec la même horloge : le retard entre les deux reste constant. |
| 3:29 | Un cœur s'occupe du son et des calculs, l'autre dessine l'interface. |
| 3:35 | Tout le code est écrit pour Arduino, et le même code tourne aussi dans un simulateur sur PC. |
| 3:42 | Au démarrage, la carte teste son haut-parleur et son micro, puis affiche l'écho en direct. |
| 3:50 | Dans les réglages, on choisit le plan de mesure, le nombre de pings, la fréquence Doppler, et la température de la pièce. |
| 3:59 | Une règle tactile indique la distance entre la carte et le mur, mesurée au mètre ruban. |
| 4:05 | À 50 cm, la carte envoie 30 pings. On voit l'onde partir, rebondir sur le mur, et revenir. |
| 4:13 | Dès la deuxième distance, la pente donne une première valeur de c. |
| 4:21 | On continue jusqu'à 4 mètres. Chaque pastille verte est une distance mesurée. |
| 4:35 | L'onglet Courbe montre la droite, et le détail de chaque point. |
| 4:40 | Le résultat : c, son incertitude, l'écart à la théorie, et la température de l'air déduite de c. |
| 4:47 | L'onglet Stats montre l'échogramme : le pic du son direct, la fenêtre de recherche, et l'écho. |
| 4:54 | On passe en mode Doppler, et on lance le radar. Quelqu'un s'approche : la vitesse s'affiche, et la trace monte sur le spectrogramme. |
| 5:03 | Il repart : le son revient plus grave. Puis il revient en courant ! |
| 5:12 | La courbe montre la vitesse pendant les 20 dernières secondes. |
| 5:18 | Chaque passage est enregistré, avec sa vitesse maximale, et son allure. |
| 5:24 | Enfin, l'onglet Stats montre le spectre en direct, avec le calcul complet de la vitesse. |
| 5:31 | Pour mesurer en classe : la carte est posée face à un grand mur plat, sans obstacle. |
| 5:37 | On mesure la distance au mètre ruban, on la règle sur l'écran, et on lance la mesure, en silence. |
| 5:43 | Pour le Doppler, on marche droit vers la carte, puis on repart. |
| 5:47 | Pièges à éviter : les meubles, les murs latéraux trop proches, le bruit, et un micro saturé. |
| 5:55 | Dans cet exemple, on obtient 345,3 mètres par seconde : à 0,1 % de la théorie, à 23 °C. |
| 6:04 | Et des vitesses de marche autour d'un mètre et demi par seconde. |
| 6:07 | Ces résultats viennent du simulateur. À vous de faire la vraie mesure ! |
| 6:13 | Tout est sur GitHub : le code, le tutoriel, et un protocole pour les étudiants. |
| 6:18 | Merci d'avoir regardé ! |

> Les résultats montrés dans la vidéo (345,3 m/s, et les vitesses de marche) viennent du simulateur : les sons sont fabriqués, puis analysés par le même code que la carte. La vidéo le dit. Si tu filmes ta vraie mesure, remplace ces séquences par tes résultats.
