# BOUCLIER 2050

Jeu de grande stratégie et de guerre hybride au tour par tour, en C avec [raylib](https://www.raylib.com/).

**2030.** Le monde a franchi +1,5 °C et le réchauffement s'accélère. Sécheresses, mégafeux et
effondrement agricole vident le Sud et jettent des millions de personnes sur les routes. Vous dirigez
la France pendant trente ans, jusqu'en 2060 : vous gérez une population vivante et son économie,
choisissez le régime, menez les divisions, maniez la dissuasion et défendez les frontières dans tous
les **milieux et champs (M2MC)** : terre, mer, air, espace, cyber, informationnel, électromagnétique.

## Principes de jeu

### Carte
66 provinces et 27 nations, du Maroc à la Volga et de la Scandinavie à l'Égypte. Quatre modes :
politique, climat, militance, population.

### Armée (à la Hearts of Iron)
- Divisions placées sur la carte : infanterie, mécanisée, blindée, infanterie de marine. Chacune a
  ses valeurs d'attaque et de défense, ses effectifs, son organisation, son entretien et sa
  consommation de biens industriels.
- Clic gauche sur une province pour sélectionner ses divisions (Maj pour en ajouter), clic droit sur
  une province voisine pour s'y déplacer ou l'attaquer.
- Les batailles se résolvent en fin de tour. Pour prendre une province, il faut briser
  l'organisation des défenseurs ; les vaincus se replient.
- Comptent aussi : le terrain (la montagne gêne les blindés), les débarquements (pénalisés sans
  infanterie de marine ni capacité navale), le ravitaillement (attrition hors de portée), les
  garnisons et la pénurie de matériel.
- L'IA tient des fronts, attaque les provinces vulnérables, se mobilise contre un agresseur et
  forme des coalitions.

### Dissuasion nucléaire
- La posture (minimale, crédible, renforcée) fixe la crédibilité. Une nation dissuadée ne touche
  pas au sol national.
- On peut aussi étendre un parapluie nucléaire à l'Europe, adresser un ultime avertissement à un
  occupant, ou subir le chantage de la Fédération Boréale.
- L'emploi de l'arme fait de la France un paria. Contre une puissance nucléaire, il déclenche une
  riposte qui met fin à la partie.

### Population (à la Victoria)
- Trois strates sociales (travailleurs, classes moyennes, élites), chacune avec :
  - sa part de la population, qui évolue par ascension sociale ;
  - ses besoins (vitaux, courants, biens industriels) ;
  - sa militance ;
  - sa conscience politique, qui amplifie la colère et abaisse le seuil de révolte.
- L'alphabétisation progresse avec l'éducation. La répression calme la militance mais accumule la
  conscience.
- Migrations climatiques, intégration des migrants et des territoires conquis, révoltes et
  sécessions.

### Économie (à la Victoria)
- Production d'alimentation, d'eau, d'énergie, de minerais et de biens industriels (usines).
- Marché mondial avec des prix qui montent quand le monde manque d'un bien, des importations et des
  exportations, et des accords commerciaux prioritaires.
- Impôts par strate, dépenses d'éducation, d'administration et de protection sociale, droits de
  douane, budget détaillé.

### Politique
- Six courants : écologistes, socialistes, libéraux, conservateurs, souverainistes, autoritaires.
  Leur poids évolue selon la situation.
- En démocratie, des élections tous les quatre ans désignent un gouvernement qui applique son
  programme. Une victoire des autoritaires peut emporter le régime.
- Six régimes, de la Commune écologique à l'État totalitaire. Chacun a sa mécanique propre et deux
  décrets : budget participatif, sobriété, référendum, plan de relance, plan quinquennal,
  gouvernance algorithmique, censure, état d'urgence, mobilisation générale, loi martiale, police
  politique, propagande totale.

### Climat, défense hybride, projets
- Réchauffement et catastrophes, États faillis.
- 12 théâtres de défense M2MC et 4 factions qui mènent la guerre sous le seuil.
- 8 grands projets de résilience et 16 dilemmes du Conseil de défense.

### Score et victoire
- Score détaillé en neuf catégories : survie, population, prospérité, territoire, stabilité,
  résilience, puissance, réputation et pénalités.
- Il est multiplié selon la difficulté et selon le type de victoire : survie, transition réussie,
  hégémonie, utopie réalisée ou ordre nouveau.
- **Défaite** si la Cohésion ou la Souveraineté tombe à 0, si le Budget passe sous -60, si Paris
  tombe, ou en cas d'échange nucléaire.

### Confort
- Rapport de fin de tour, désactivable.
- Trois niveaux de difficulté.
- Sauvegarde manuelle (`F5`, menu) et automatique tous les 4 tours ; chargement par `F9` ou par le
  menu.

### Commandes

| Action | Commande |
|---|---|
| Sélectionner une province et ses divisions | clic gauche (Maj : ajouter) |
| Déplacer / attaquer | clic droit sur une province voisine |
| Fin du tour | `FIN DU TOUR`, `Espace` ou `Entrée` |
| Onglets | `1` à `9` |
| Mode de carte | `M` |
| Sauvegarder / charger | `F5` / `F9` |
| Plein écran / menu | `F11` / `Échap` |

## Compilation

Le jeu n'utilise que des formes 2D, une texture de carte générée et la police intégrée de raylib :
aucun fichier externe. La résolution logique est de 1280×720, mise à l'échelle automatiquement.

Avec CMake (télécharge raylib 5.0 s'il n'est pas installé) :

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/bouclier2050
```

Ou avec make, soit avec raylib installé (`brew install raylib` sous macOS, trouvé via pkg-config) :

```sh
make run
```

soit avec raylib compilé à la main (`make` dans `~/raylib/src` doit avoir produit `libraylib.a`) :

```sh
make RAYLIB_PATH=$HOME/raylib/src run
```

Après un échec de configuration CMake, supprimez `build/` avant de relancer. Dépendances Linux (Debian/Ubuntu) :
`sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
Sous Windows : [w64devkit](https://github.com/skeeto/w64devkit) puis `make RAYLIB_PATH=C:/raylib/raylib/src`.

Machines anciennes : compilez raylib en OpenGL 2.1
(`make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_21` dans `raylib/src`).

## Tests et équilibrage

- `make test` vérifie les mécaniques rares : frappes et riposte nucléaires, avertissement, chantage,
  les douze décrets, élections, usines, recrutement, parapluie européen.
- `make sim` (ou `./build/autoplay [parties] [difficulté 0-2]`) joue des parties automatiques sans
  interface.

Taux de victoire indicatifs des bots (qui jouent leur stratégie sans erreur) :

| Stratégie | Facile | Normale | Difficile |
|---|---|---|---|
| Passive (ne fait rien) | 0 % | 0 %, effondrement vers 2041 | 0 % |
| Gestionnaire défensif | 97 % | 95 % | 30 % |
| Stratège (régime autoritaire, armée, guerres choisies) | 100 % | 88 % | 15 % |
| Conquérant total (junte, guerres en chaîne) | 0 % | 0 % : coalitions, révoltes, faillite | 0 % |

## Organisation du code

```
src/game.h          types et API de la simulation
src/game_internal.h outils partagés entre modules
src/world.c         données : nations, provinces, régimes, unités, idéologies, décrets, dilemmes
src/game.c          tour de jeu, guerre hybride M2MC, projets, dilemmes, score, sauvegarde
src/society.c       climat, population, strates, conscience, migrations, révoltes, régimes
src/economy.c       production, marché mondial, besoins, impôts, dépenses, usines
src/politics.c      courants politiques, élections, gouvernements, décrets
src/war.c           divisions, batailles, ravitaillement, IA militaire, diplomatie
src/nuclear.c       dissuasion nucléaire
src/map_data.c/.h   carte générée (grille 0,25°, provinces, adjacences)
src/main.c          interface raylib
tools/gen_map.py    générateur de la carte depuis un GeoJSON des pays
tools/autoplay.c    banc d'essai d'équilibrage
tools/tests.c       tests des mécaniques
```

Pour régénérer la carte : `python3 tools/gen_map.py countries.geo.json`, avec le fichier
`countries.geo.json` de [world.geo.json](https://github.com/johan/world.geo.json)
(contours Natural Earth, domaine public).

Les factions hybrides (Fédération Boréale, Ligue des Sables, Consortium Hydra, Réseau Nébula) et
tous les événements sont fictifs.
