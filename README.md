# BOUCLIER 2050

Wargame de stratégie au tour par tour, en C avec [raylib](https://www.raylib.com/).

**2050.** Le monde a dépassé +2 °C. Sécheresses, mégafeux et effondrement agricole jettent des
millions de personnes sur les routes ; l'eau, l'énergie et la nourriture sont devenues des armes.
Puissances, cartels et mercenaires numériques mènent contre la France une guerre hybride sans
déclaration. Vous dirigez le **Bouclier national** : tenez dix ans, jusqu'en 2060.

Le jeu couvre l'ensemble des **milieux et champs de conflictualité (M2MC)** :
terre, mer, air, espace, cyber, informationnel et spectre électromagnétique.

## Principes de jeu

- **12 théâtres d'opérations** : frontières Nord, Est, Alpes, Pyrénées ; Manche-Atlantique,
  Méditerranée, Outre-mer et ZEE ; espace aérien ; orbite ; cyberespace ; champ
  informationnel ; spectre électromagnétique.
- **Posture** : chaque milieu a une capacité que vous répartissez entre ses théâtres.
  Plus la posture est forte, plus les menaces sont neutralisées. Elle coûte du budget
  à chaque tour, et augmenter la capacité demande d'investir.
- **Menaces hybrides** (18 types) : trafics, flux migratoires instrumentalisés, sabotages,
  milices, pillage de la ZEE, sabotage de câbles, harcèlement naval, essaims de drones,
  ASAT, rançongiciels, attaques du réseau électrique, désinformation, deepfakes, brouillage
  GNSS, accaparement de l'eau... Une menace non identifiée (`?`) est difficile à contrer.
- **Synergies M2MC** : la posture orbitale améliore la détection partout ; un spectre
  électromagnétique brouillé dégrade la défense aérienne et navale.
- **4 factions imaginaires** (Fédération Boréale, Ligue des Sables, Consortium Hydra,
  Réseau Nébula). Avec 3 points de commandement par tour : diplomatie, sanctions,
  cyber-offensive, contre-influence, renseignement.
- **Climat et ressources** : le réchauffement érode chaque tour l'énergie, l'eau et
  l'alimentation, et multiplie les catastrophes. 8 grands projets de résilience
  (SMR, renouvelables, dessalement, agro-résilience, bouclier cyber, éducation aux médias,
  constellation souveraine, plan d'adaptation).
- **Doctrine** : politique migratoire (accueil / équilibre / fermeture) et niveau
  d'alerte national, chacun avec ses coûts politiques, budgétaires et diplomatiques.
- **Dilemmes** : 12 décisions du Conseil de défense, sans bonne réponse.

**Défaite** si la Cohésion ou la Souveraineté tombe à 0, ou si le Budget passe sous -60.

### Commandes

| Action | Commande |
|---|---|
| Sélectionner un théâtre | clic sur la carte ou dans la vue d'ensemble |
| Fin du tour | bouton `FIN DU TOUR`, `Espace` ou `Entrée` |
| Onglets | `1` `2` `3` `4` |
| Plein écran | `F11` |
| Menu | `Échap` |

Survolez un élément (ressource, menace, action) pour afficher une infobulle.

## Compilation

Le jeu n'utilise que des formes 2D et la police intégrée de raylib : aucun fichier externe,
résolution logique 1280×720 mise à l'échelle automatiquement à la taille de la fenêtre.

### Avec CMake (télécharge raylib si besoin)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bouclier2050
```

### Avec make

```sh
make                              # raylib installé (pkg-config)
make RAYLIB_PATH=~/raylib/src     # raylib compilé à la main
make run
```

Dépendances Linux (Debian/Ubuntu) pour compiler raylib :
`sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`

Windows : utilisez [w64devkit](https://github.com/skeeto/w64devkit) ou l'installeur raylib,
puis `make RAYLIB_PATH=C:/raylib/raylib/src`.

### Machines anciennes

Pour du matériel ne supportant pas OpenGL 3.3, compilez raylib en OpenGL 2.1 :

```sh
cd raylib/src && make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_21
```

## Équilibrage

`make sim` lance 1000 parties automatiques sans interface (une stratégie passive, une
heuristique simple). À titre indicatif : la stratégie passive perd vers 2056, l'heuristique
gagne environ 3 parties sur 4.

## Organisation du code

```
src/game.h, src/game.c   simulation (aucune dépendance graphique)
src/main.c               interface raylib : carte, panneaux, modales
tools/autoplay.c         banc d'essai d'équilibrage
```

Les factions, menaces et événements sont fictifs.
