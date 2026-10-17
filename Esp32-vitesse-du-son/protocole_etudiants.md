# TP : mesurer la vitesse du son par écho, puis une vitesse par effet Doppler

**Niveau :** lycée (2de à Terminale, enseignement scientifique et spécialité physique) · BUT / BTS · 1er cycle universitaire
**Durée :** 2 h, découpées ainsi :
- installation : 10 min ;
- partie A, écho : 45 min ;
- partie B, Doppler : 25 min ;
- exploitation et bilan : 40 min.

**Groupes :** 3 à 4 élèves par carte

---

## 1. Objectifs

- Mesurer une durée de quelques **millisecondes**, au **microseconde** près, avec un haut-parleur et un micro.
- Comprendre pourquoi on mesure l'écart **son direct → écho**, et pourquoi on exploite une **pente** plutôt qu'une mesure isolée.
- Relier la **dispersion** des mesures à l'**incertitude** d'une moyenne (loi en 1/√N).
- Déterminer **c** avec son incertitude, la comparer à la valeur théorique **c(T)**, puis en déduire la **température de l'air**.
- Mesurer la **vitesse d'une personne** par **effet Doppler**, et la vérifier au chronomètre.

## 2. Matériel par groupe

- Une carte **ES3C28P** avec le programme `son_console`. Elle intègre le haut-parleur, le micro et l'écran tactile.
- Une batterie USB ou un câble USB assez long.
- Un mètre ruban de 5 m, ou un décamètre.
- Un support stable pour la carte (une boîte ou des livres), à environ 1 m du sol.
- Un **grand mur plat et dégagé**, d'au moins 2 m × 2 m.
- Un thermomètre d'ambiance.
- Pour la partie B : un chronomètre (un téléphone suffit) et du ruban adhésif pour marquer le sol.
- Facultatif : une carte microSD dans la carte, et un ordinateur avec un tableur ou Python.

## 3. Rappels

| Grandeur | Valeur |
|---|---|
| Vitesse du son dans l'air sec | c = 331,3 × √(1 + T / 273,15) m/s (T en °C) |
| à 15 / 20 / 25 °C | 340,3 / 343,2 / 346,1 m/s |
| 1 ms | 10⁻³ s · 1 µs = 10⁻⁶ s |
| Temps pour 1 m | ≈ 2,9 ms |
| Écho sur un mur à la distance d | Δt = 2d / c |
| Échantillonnage du micro | 48 000 échantillons/s, soit 1 échantillon toutes les 20,8 µs |
| Effet Doppler (réflexion sur un objet de vitesse v) | f' = f₀ (c + v)/(c − v), d'où **Δf ≈ 2 v f₀ / c** |

**Principe de l'écho.** La carte émet un « tic » de 10 ms : un glissando de 1 500 à 6 500 Hz. Le micro l'entend d'abord directement (le son direct), puis réfléchi par le mur (l'écho). La carte compare l'enregistrement au tic émis : c'est une corrélation, appelée **filtre adapté**. Chaque arrivée devient alors un pic très fin. L'écart entre les deux pics vaut **Δt = 2d/c**, plus un petit décalage constant.

## 4. Préparation (10 min)

1. Relever la **température** de la salle et la noter sur la fiche.
2. Poser la carte sur son support, **haut-parleur tourné vers le mur**. Le mur doit être face à la carte, perpendiculaire à l'axe de mesure.
3. Dégager la zone entre la carte et le mur. Éloigner la carte des murs latéraux (au moins 1,5 m si possible).
4. Allumer la carte. Sur l'écran d'accueil, le test audio doit indiquer « **OK · direct XX dB** ».
5. Dans **Réglages › Expérience** :
   - Source : **Micro** ;
   - Plan (écho) : **4 m** ;
   - Pings par point : **30** ;
   - Température : la valeur mesurée.
6. Dans **Réglages › Appareil**, régler le **volume** sur **Moyen**. S'il s'affiche « micro saturé », baisser le gain.

## 5. Partie A : la vitesse du son par écho (45 min)

Pour chaque distance du plan (0,5 · 1 · 1,5 · 2 · 2,5 · 3 · 3,5 · 4 m) :

1. Placer la carte à la distance voulue du mur, en déplaçant la carte et non le mur.
2. Mesurer au mètre ruban la distance entre la **grille du haut-parleur** et le mur. La noter dans la colonne **d mesurée**, à 0,5 cm près.
3. Régler exactement cette distance sur la **règle** de l'écran, en la faisant glisser ou avec les flèches ‹ ›.
4. Vérifier que le SNR (rapport signal/bruit) affiché sous l'échogramme dépasse 15 dB.
5. Appuyer sur **MESURER** et **rester silencieux** environ 7 s.
6. Quand la pastille devient verte, aller dans **Courbe** et toucher le point. Relever :
   - Δt moyen (en ms) ;
   - son incertitude ± u (en µs) ;
   - l'écart-type σ (en µs) ;
   - le nombre de pings gardés n.

> ⚠️ **Pièges à éviter**
> - Rester entre la carte et le mur, ou y laisser une chaise ou un sac.
> - Parler ou faire du bruit pendant une série.
> - Mesurer trop près d'un mur latéral : son écho peut arriver avant celui du mur visé.
> - Laisser le micro saturer : le pic du son direct s'élargit, et la mesure se dégrade.

### Fiche de relevés (partie A)

Température de la salle : ……… °C → c théorique = ……… m/s

| # | d mesurée (m) | Δt moyen (ms) | u (µs) | σ (µs) | n | Δt théorique à 20 °C = 2d/c (ms) |
|---|---|---|---|---|---|---|
| 1 | 0,50 | | | | | 2,914 |
| 2 | 1,00 | | | | | 5,827 |
| 3 | 1,50 | | | | | 8,741 |
| 4 | 2,00 | | | | | 11,655 |
| 5 | 2,50 | | | | | 14,568 |
| 6 | 3,00 | | | | | 17,482 |
| 7 | 3,50 | | | | | 20,395 |
| 8 | 4,00 | | | | | 23,309 |

## 6. Partie B : une vitesse par effet Doppler (25 min)

1. En haut de l'écran, passer en mode **Doppler**. Dans Réglages, laisser f₀ sur **10 kHz**.
2. Marquer au sol, avec du ruban adhésif, deux repères **à 3 m l'un de l'autre** dans l'axe de la carte : par exemple à 1 m et à 4 m de la carte.
3. Appuyer sur **RADAR**. La carte émet un son continu.
4. Un élève part à 5 m de la carte et **marche droit vers elle**, à vitesse régulière. Un deuxième élève chronomètre le passage entre les deux repères. Un troisième relève la vitesse affichée.
5. Refaire la mesure en s'éloignant, puis en marchant vite, puis en trottinant.
6. Dans **Résultat**, relever pour chaque passage la vitesse maximale, la moyenne et le sens.

### Fiche de relevés (partie B)

| # | Allure | Sens | v affichée max (m/s) | v moyenne (m/s) | Temps chrono sur 3 m (s) | v chrono = 3 / t (m/s) |
|---|---|---|---|---|---|---|
| 1 | marche | approche | | | | |
| 2 | marche | éloignement | | | | |
| 3 | marche rapide | approche | | | | |
| 4 | course légère | approche | | | | |

## 7. Exploitation (40 min)

### 7.1 Lecture directe
Relever dans **Résultat** (mode écho) la valeur de c, son incertitude, l'écart à la théorie, et la « température de l'air » déduite de c.

### 7.2 Calcul par les élèves (tableur ou calculatrice)
1. Tracer **Δt (ms)** en fonction de **d (m)**.
2. Modéliser par une droite **Δt = a·d + b**, puis relever a et b.
3. Calculer **c = 2 / a**. Attention aux unités : si a est en ms/m, alors c = 2 / (a × 10⁻³) m/s.
4. Comparer à c(T) : écart relatif = (c − c(T)) / c(T).
5. En déduire la température de l'air : **T = 273,15 × ((c / 331,3)² − 1)**. La comparer au thermomètre.

### 7.3 Incertitudes
- Pour une distance, comparer σ/√n à l'incertitude u affichée.
- Refaire **une** distance avec 10, puis 60 pings par point. Comment évolue u ? Vérifier la loi en 1/√N.
- Une erreur de 5 mm sur d fait-elle plus d'effet que l'incertitude sur Δt ? (indice : 5 mm aller-retour ↔ 29 µs)

### 7.4 Doppler
1. Pour un passage, calculer le décalage attendu Δf = 2 v f₀ / c, et le comparer à Δf affiché dans Stats.
2. Comparer la vitesse Doppler à la vitesse au chronomètre. Pourquoi la vitesse Doppler « maximale » est-elle un peu plus grande ?

### 7.5 Données brutes (facultatif)
Copier `son.csv` depuis la carte SD. Ce fichier contient une ligne par ping (`echo,distance,Δt,SNR,source`) et une ligne par passage Doppler. Retrouver c avec un tableur à partir de **tous** les pings.

## 8. Questions

1. Pourquoi mesure-t-on l'écart **son direct → écho**, et non le temps écoulé depuis l'émission ? *(Indice : le son passe par plusieurs mémoires tampons avant d'être joué.)*
2. Pourquoi émet-on un **glissando** de 1,5 à 6,5 kHz plutôt qu'un bip à une seule fréquence ?
3. Le micro prend un échantillon toutes les 20,8 µs. À quelle distance correspond un échantillon, pour l'aller-retour ? Comment la carte fait-elle mieux ?
4. Pourquoi la droite ne passe-t-elle pas exactement par l'origine ? Pourquoi cela ne gêne-t-il pas la mesure de c ?
5. Votre c est-il compatible avec la température de la salle ? Quelle température faudrait-il pour expliquer votre écart ?
6. Pourquoi le décalage Doppler fait-il intervenir **2v**, et non v ?
7. À 10 kHz, la FFT a une case de 5,9 Hz. Quelle résolution en vitesse cela donne-t-il ? Pourquoi 17 kHz fait-il mieux ?
8. Pourquoi les bras et les jambes élargissent-ils le pic Doppler ?

## 9. Éléments de correction (enseignant)

1. Entre l'instant où le programme envoie le tic et l'instant où le haut-parleur le joue, il y a des tampons (DMA) et le codec. Ce retard est mal connu. Le son direct traverse **la même chaîne** que l'écho : en mesurant l'écart entre les deux, ce retard s'annule.
2. Un glissando étale l'énergie sur 5 kHz de bande. Après corrélation, le pic a une largeur d'environ 1/B ≈ 0,2 ms : il est fin et bien au-dessus du bruit. Un bip pur donnerait un pic large et des ambiguïtés d'une période.
3. 20,8 µs × 343 m/s ≈ 7,1 mm de trajet aller-retour, soit **3,6 mm de distance**. La carte interpole une parabole sur le sommet du pic (précision ≈ 1 µs), puis fait la moyenne de 30 pings.
4. Le haut-parleur et le micro ne sont pas au même endroit (environ 3,5 cm). La distance se mesure depuis la grille, et non depuis un point idéal. Ces écarts ajoutent une constante b, mais la pente a = 2/c ne change pas.
5. Un écart de +1 % sur c correspond à environ +6 °C. Avec de bonnes mesures, on retrouve la température de la salle à 1 ou 2 °C près.
6. La personne **reçoit** une onde décalée, car elle se déplace dans l'onde : premier effet, ≈ v/c. Elle la **réémet** comme une source en mouvement : deuxième effet, ≈ v/c. D'où Δf ≈ 2 v f₀ / c.
7. Δv = c · δf / (2 f₀) = 343 × 5,9 / 20 000 ≈ **0,10 m/s** à 10 kHz, et 0,06 m/s à 17 kHz. Le décalage est proportionnel à f₀.
8. Les membres vont plus vite (vers l'avant) puis plus lentement que le tronc : on voit un « micro-Doppler » autour de la vitesse du corps. La carte prend le barycentre de l'énergie. C'est aussi pourquoi la vitesse maximale dépasse un peu la vitesse moyenne au chronomètre.

**Ordres de grandeur attendus**
- Partie A, avec un mur plat, 0,5 à 4 m et 30 pings : c à **1 % près** environ, parfois mieux.
- Partie B : la marche donne 1,1 à 1,6 m/s. L'accord avec le chronomètre est de 0,1 à 0,2 m/s.

## 10. Prolongements

- **Thermomètre acoustique :** refaire la mesure près d'un radiateur, ou dehors l'hiver.
- **Influence du nombre de pings :** tracer u en fonction de 1/√N.
- **Absorption :** recouvrir le mur d'un tissu épais ou d'une mousse. Que deviennent le SNR et l'écho ?
- **Doppler et fréquence :** comparer 5, 10 et 17 kHz pour la même allure (résolution, vitesse minimale détectée).
- **Programmation :** lire `dsp.cpp` (FFT, filtre adapté, interpolation parabolique) et `son.cpp` (médiane, rejet, régression linéaire).
- **Lien avec la vitesse de la lumière :** même méthode (aller-retour, pente de la droite), mais un rapport de presque 1 000 000 entre les vitesses.
