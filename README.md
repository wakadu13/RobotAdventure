# BOUCLIER 2050

Jeu de grande stratégie et de guerre hybride au tour par tour, en C avec [raylib](https://www.raylib.com/).

**2030.** Le monde a franchi +1,5 °C et le réchauffement s'accélère. Sécheresses, mégafeux et
effondrement agricole vident le Sud et jettent des millions de personnes sur les routes. Vous dirigez
la France pendant trente ans, jusqu'en 2060 : vous gérez une population vivante et son économie,
choisissez le régime, menez les divisions, maniez la dissuasion et défendez les frontières dans tous
les **milieux et champs (M2MC)** : terre, mer, air, espace, cyber, informationnel, électromagnétique.

## Principes de jeu

### Carte
130 régions et 27 nations, du Maroc à la Volga et de la Scandinavie à l'Égypte : les 13 régions
françaises, les Länder allemands, les régions italiennes, espagnoles, britanniques, polonaises,
turques... La carte se zoome (jusqu'à x6) et se déplace ; les noms des régions apparaissent à partir du
zoom x2. Cinq modes : politique (rayures pour les territoires occupés), climat, militance, population,
cultures.

### Armée (à la Hearts of Iron)
- 17 types d'unités tirés de l'actualité et des prévisions pour 2050 :
  - terre : infanterie, mécanisée, blindée (MGCS), infanterie de marine, troupes de montagne, feux
    longue portée, essaims de drones, robots de combat, forces spéciales, défense sol-air, missiles
    hypersoniques ;
  - air : chasseurs (SCAF), drones MALE ;
  - mer : frégates (FDI), sous-marins nucléaires d'attaque, porte-avions, drones navals.
- Chaque unité a une vitesse (les blindés et les mécanisées avancent de deux régions par tour), une
  portée d'appui, des valeurs terrestres, antiaériennes et navales, un coût, un entretien et un poids
  dans le plafond de mobilisation. Une région accueille au plus 10 divisions, 6 escadres et 8 navires.
- Les batailles mêlent appui aérien, défense sol-air, brouillage, feux dans la profondeur, frappes
  hypersoniques, terrain, débarquements (protégés par la supériorité navale) et ravitaillement. Les
  flottes se livrent bataille et imposent des blocus.
- **Généraux** (compétence, trait, expérience) affectés à une région, et **technologies militaires** en
  huit domaines qui débloquent les unités et renforcent l'armée.
- Une région conquise est d'abord **occupée** : elle ne produit plus pour son propriétaire. La paix se
  négocie : un **traité** fixe les cessions et les réparations dans la limite du **score de guerre**
  (occupations, batailles, blocus).
- Les IA mènent plusieurs guerres à la fois, forment des **alliances** entre elles et appellent leurs
  alliés aux armes.

### Dissuasion nucléaire
- La posture (minimale, crédible, renforcée) fixe la crédibilité. Une nation dissuadée ne touche
  pas au sol national. Parapluie européen, ultime avertissement, chantage d'une puissance acculée.
- **Prolifération** : la Turquie, l'Algérie, l'Égypte, la Pologne, l'Ukraine... peuvent lancer un
  programme militaire quand elles se sentent menacées. Contre-mesures : pression diplomatique, sabotage
  cyber, ou occupation de leur capitale.
- **Armes tactiques** (technologie nucléaire 3) contre les forces ennemies d'une région, avec risque de
  riposte et d'escalade face à une puissance nucléaire. Une IA acculée peut aussi y recourir.
- La frappe stratégique fait de la France un paria ; contre une puissance nucléaire, elle déclenche
  un échange qui met fin à la partie.

### Population (à la Victoria)
- Trois strates sociales (travailleurs, classes moyennes, élites), chacune avec sa part de la
  population, ses besoins, sa militance, sa conscience politique et **ses propres courants politiques**.
- **32 cultures et 5 religions** par région. Les cultures non acceptées forment des minorités dont
  l'agitation et l'assimilation dépendent de la politique culturelle (assimilation, pluralisme,
  répression réservée aux régimes autoritaires). Les migrants emportent leur culture ; une région
  devenue majoritairement étrangère peut faire sécession vers sa patrie, qui la revendique.
- Alphabétisation, ascension sociale, migrations climatiques, intégration des territoires annexés,
  révoltes et sécessions.

### Économie (à la Victoria)
- Production d'alimentation, d'eau, d'énergie, de minerais et de biens industriels.
- **Emploi et chômage** : agriculture, usines et services offrent des emplois ; une usine sans
  main-d'oeuvre tourne au ralenti, le chômage nourrit la militance et l'émigration.
- **Stocks** : la moitié des excédents est stockée et comble les pénuries suivantes.
- Marché mondial avec des prix qui montent quand le monde manque d'un bien, accords commerciaux,
  **embargos** (on n'achète pas à un ennemi) et **blocus** navals qui coupent jusqu'à 70 % des échanges.
- Les autres nations ont un **trésor** et investissent (eau, alimentation, usines là où le chômage est
  le plus fort).
- Impôts par strate, dépenses d'éducation, d'administration et de protection sociale, droits de
  douane, réparations et réquisitions, recherche militaire, budget détaillé.

### Politique
- Six courants : écologistes, socialistes, libéraux, conservateurs, souverainistes, autoritaires.
- En démocratie, des élections tous les quatre ans désignent un gouvernement qui applique son
  programme. Une victoire des autoritaires peut emporter le régime.
- Six régimes, de la Commune écologique à l'État totalitaire, chacun avec sa mécanique propre et deux
  décrets.
- Les autres nations ont aussi leur vie politique : élections, révolutions, coups d'État militaires,
  transitions démocratiques, suspension ou retour dans l'Union européenne.

### Climat, défense hybride, projets
- Réchauffement et catastrophes, États faillis.
- 13 théâtres de défense M2MC et 4 factions qui mènent la guerre sous le seuil. Les territoires
  conquis ou occupés connaissent guérillas, attentats et sabotages : les divisions stationnées sur place
  aident à les neutraliser.
- 8 grands projets de résilience et 16 dilemmes du Conseil de défense.

### Score et victoire
- Score détaillé en neuf catégories : survie, population, prospérité, territoire, stabilité,
  résilience, puissance, réputation et pénalités.
- Il est multiplié selon la difficulté et selon le type de victoire : survie, transition réussie,
  hégémonie, utopie réalisée ou ordre nouveau.
- **Défaite** si la Cohésion ou la Souveraineté tombe à 0, si le Budget passe sous -60, si Paris
  est occupée, ou en cas d'échange nucléaire.

### Confort
- Rapport de fin de tour, désactivable.
- Trois niveaux de difficulté.
- Sauvegarde manuelle (`F5`, menu) et automatique tous les 4 tours ; chargement par `F9` ou par le
  menu.

### Commandes

| Action | Commande |
|---|---|
| Sélectionner une région et ses unités | clic gauche (Maj : ajouter) |
| Déplacer / attaquer | clic droit sur la destination |
| Zoomer / déplacer la carte | molette ou `+`/`-` ; glisser ; flèches |
| Recentrer sur la France / vue d'ensemble | `F` / `Origine` |
| Fin du tour | `FIN DU TOUR`, `Espace` ou `Entrée` |
| Onglets | `1` à `9`, `0` |
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

- `make test` vérifie les mécaniques rares : frappes stratégiques et tactiques, riposte, prolifération et
  contre-prolifération, avertissement, chantage, les douze décrets, élections, usines, recrutement,
  parapluie européen, traités, défaite à Paris, généraux, technologies, embargo, cultures, sauvegarde.
- `make sim` (ou `./build/autoplay [parties] [difficulté 0-2]`) joue des parties automatiques sans
  interface.

Taux de victoire indicatifs des bots (qui jouent leur stratégie sans erreur) :

| Stratégie | Facile | Normale | Difficile |
|---|---|---|---|
| Passive (ne fait rien) | 0 % | 0 %, effondrement vers 2041 | 0 % |
| Gestionnaire défensif | 100 % | 100 % | 83 % |
| Stratège (régime autoritaire, armée, guerres choisies) | 100 % | 26 % | 0 % |
| Conquérant total (junte, guerres en chaîne) | 0 % | 0 % : coalitions, révoltes, faillite | 0 % |

Les bots ne négocient que les traités par défaut, n'affectent pas de généraux et ne gèrent ni les
minorités ni l'occupation : les conquêtes leur coûtent cher. Un joueur qui stationne des troupes dans
les territoires occupés et choisit sa politique culturelle s'en sort mieux.

## Organisation du code

```
src/game.h          types et API de la simulation
src/game_internal.h outils partagés entre modules
src/world.c         données : nations, provinces, régimes, unités, idéologies, décrets, dilemmes
src/game.c          tour de jeu, guerre hybride M2MC, projets, dilemmes, score, sauvegarde
src/society.c       climat, population, strates, cultures, religions, migrations, révoltes, régimes
src/economy.c       production, emploi, stocks, marché mondial, embargos, blocus, impôts, usines, trésor des IA
src/politics.c      courants par strate, élections, décrets, vie politique des IA
src/war.c           unités, batailles, blocus, ravitaillement, IA militaire, alliances, traités
src/military.c      généraux et technologies militaires
src/nuclear.c       dissuasion, prolifération, armes tactiques
src/map_data.c/.h   carte générée (grille 0,125°, 130 régions, adjacences, cultures, religions)
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
