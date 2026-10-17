# TP : mesurer la vitesse de la lumière avec deux ESP32

**Niveau :** lycée (1re/Terminale spécialité physique) · BUT / BTS · 1er cycle universitaire
**Durée :** 2 h (installation 15 min · mesures 45 min · exploitation 45 min · bilan 15 min)
**Groupes :** 3 à 4 élèves par paire de cartes

---

## 1. Objectifs

- Mesurer une durée de quelques **nanosecondes** et comprendre comment le Wi-Fi le permet (FTM).
- Comprendre pourquoi on mesure un **aller-retour** et pourquoi on exploite une **pente** plutôt qu'une mesure isolée.
- Relier la **dispersion** d'une mesure à l'**incertitude** d'une moyenne (loi en 1/√N).
- Déterminer **c** avec son incertitude et la comparer à la valeur de référence c = 299 792 458 m/s.

## 2. Matériel par groupe

- Carte **B** : ES3C28P (console à écran) avec le programme `lumiere_console`
- Carte **A** : ESP32-S3 avec le programme `lumiere_repondeur`
- 2 batteries USB
- 1 mètre ruban de 20 m (ou un décamètre)
- 2 supports à environ 1 m du sol : tabourets, trépieds, cartons…
- Facultatif : une carte microSD dans la carte B, et un ordinateur avec un tableur ou Python

## 3. Rappels

| Grandeur | Valeur |
|---|---|
| c | 299 792 458 m/s ≈ 3,00 × 10⁸ m/s |
| 1 ns | 10⁻⁹ s |
| Temps pour 1 m | 3,34 ns |
| Aller-retour sur une distance d | t = 2d / c |

**Principe FTM.** La carte A envoie une trame (instant t1), que la carte B reçoit (t2). La carte B renvoie un accusé (t3), que la carte A reçoit (t4). La radio horodate ces quatre instants. Le temps de vol aller-retour vaut :

> **t = (t4 − t1) − (t3 − t2)**

## 4. Préparation (15 min)

1. Choisir un lieu **dégagé** : une cour, un terrain, ou un long couloir à défaut. Noter le lieu sur la fiche.
2. Poser la **carte A** sur son support, sur batterie. Elle ne bougera plus.
3. Allumer la **carte B**. En haut à droite de l'écran, le badge doit passer au **vert** (« Carte A −xx dBm »).
4. Dans **Réglages** :
   - Source : **Carte A**
   - Plan de mesure : **20 m**
   - Mesures par point : **600**
5. Repérer sur chaque carte l'emplacement de l'antenne : le petit zigzag doré gravé sur le circuit. Les distances se mesurent **d'antenne à antenne**.

## 5. Mesures (45 min)

Pour chaque distance du plan (2, 4, 6, 8, 10, 12, 15 et 20 m) :

1. Placer la carte B à la distance voulue, à la **même hauteur** que la carte A, **antennes face à face**.
2. Mesurer la distance au mètre ruban et la noter : **d mesurée** (à 1 cm près).
3. Régler exactement cette distance sur la **règle** de l'écran, en glissant ou avec les flèches ‹ ›.
4. Appuyer sur **MESURER**, puis s'éloigner d'un pas sur le côté et **ne plus bouger** (environ 5 s).
5. Quand la pastille devient verte, aller dans **Courbe** et toucher le point. Relever :
   - le temps moyen **t** ;
   - son incertitude **± u(t)** ;
   - l'écart-type **σ** ;
   - le nombre de mesures **n**.

> ⚠️ **Pièges à éviter**
> - Se tenir entre les cartes pendant la mesure (le corps absorbe et réfléchit les ondes).
> - Mesurer à moins de 2 m (la puce bloque une partie des mesures à 0 : l'écran affiche « Trop près »).
> - Se placer près d'un mur, d'une voiture ou d'un grillage (échos, ou « multitrajets »).

### Fiche de relevés

| # | d mesurée (m) | t moyen (ns) | u(t) (ns) | σ (ns) | n | t théorique = 2d/c (ns) |
|---|---|---|---|---|---|---|
| 1 | 2,00 | | | | | 13,34 |
| 2 | 4,00 | | | | | 26,69 |
| 3 | 6,00 | | | | | 40,03 |
| 4 | 8,00 | | | | | 53,38 |
| 5 | 10,00 | | | | | 66,71 |
| 6 | 12,00 | | | | | 80,06 |
| 7 | 15,00 | | | | | 100,07 |
| 8 | 20,00 | | | | | 133,43 |

Lieu : ……………………  Météo / intérieur : ……………………  Hauteur des cartes : ……… m

## 6. Exploitation (45 min)

### 6.1 Lecture directe
Relever dans **Résultat** la valeur de c affichée, son incertitude et l'écart en %.

### 6.2 Calcul par les élèves (tableur ou calculatrice)
1. Tracer **t (ns)** en fonction de **d (m)**.
2. Modéliser par une droite **t = a·d + b**, puis relever a et b.
3. Calculer **c = 2 / a**. Attention aux unités : si a est en ns/m, c = 2 / (a × 10⁻⁹) m/s.
4. Comparer à la valeur de référence : écart relatif = (c − c_réf) / c_réf.

### 6.3 Incertitudes
- Sur l'écran Stats, relever σ pour une distance. Calculer σ/√n et comparer à u(t) affiché.
- Recommencer **une** distance avec 200 puis 1 500 mesures par point (onglet Réglages). Comment évolue u(t) ? Vérifier la loi en 1/√N.

### 6.4 Analyse des données brutes (facultatif)
Copier `lumiere.csv` depuis la carte SD, puis :
```bash
python outils/mesure.py analyser lumiere.csv --graphe
```
Comparer avec le résultat de la carte.

## 7. Questions

1. Pourquoi mesure-t-on un **aller-retour** plutôt qu'un aller simple ? *(Indice : les deux cartes n'ont pas la même horloge.)*
2. Pourquoi retire-t-on **(t3 − t2)** ?
3. L'horloge de mesure avance par paliers de **1,5625 ns**. À quelle distance aller-retour cela correspond-il ? Comment peut-on mesurer plus finement que ce palier ?
4. Pourquoi la droite ne passe-t-elle pas forcément par l'origine ? Pourquoi cela ne gêne-t-il pas la mesure de c ?
5. Pourquoi une seule mesure, même très soignée, ne suffirait-elle pas ?
6. Comparer un résultat obtenu en intérieur et un autre en extérieur. Expliquer la différence.
7. Les ondes Wi-Fi (2,4 GHz) et la lumière visible (≈ 500 THz) vont-elles à la même vitesse ? Qu'ont-elles en commun ?

## 8. Éléments de correction (enseignant)

1. Les horloges des deux cartes ne sont pas synchronisées à la nanoseconde près. Dans l'aller-retour, chaque carte ne compare que **ses propres** instants : t1 et t4 pour A, t2 et t3 pour B. Le décalage entre les deux horloges s'annule.
2. t3 − t2 est le temps de traitement de la carte B : ce n'est pas un temps de propagation.
3. c × 1,5625 ns ≈ 0,47 m aller-retour, soit **23 cm** de distance. Le bruit (≈ 4,5 ns) « dithère » les mesures : la moyenne de centaines de valeurs quantifiées donne une précision bien inférieure au palier.
4. Il reste de petits retards fixes (électronique, antennes). Ils ajoutent une constante b, mais ne modifient pas la pente a = 2/c.
5. Une mesure isolée a σ ≈ 4,5 ns, soit ± 70 cm. On a besoin de la moyenne (u = σ/√n) et de plusieurs distances (la pente élimine b).
6. En intérieur, les échos sur les murs (multitrajets) allongent certains trajets et biaisent les moyennes. Le résultat est souvent moins bon.
7. Oui : ce sont toutes deux des ondes électromagnétiques, qui se propagent à c dans le vide (et presque à c dans l'air, avec un indice d'environ 1,0003).

**Ordres de grandeur attendus**
- En extérieur, 600 mesures par point, de 2 à 20 m : c à **quelques %** près.
- En intérieur : de 5 à 20 % selon les échos.
- Pour mieux faire : des distances plus longues (plan 50 m) et 1 500 mesures par point.

## 9. Prolongements

- **Influence du nombre de mesures :** tracer u(t) en fonction de 1/√N.
- **Influence des obstacles :** une personne, puis un sac d'eau, entre les cartes.
- **Indice de l'air :** est-il mesurable ? (non : 0,03 %, c'est sous la précision de la mesure, et c'est une bonne discussion)
- **Programmation :** lire `mesure.cpp` (filtre par la médiane, régression linéaire) et le comparer à son propre calcul.
