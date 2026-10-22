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
- **IA militaire** : armées interarmes dosées selon la richesse et l'accès à la mer, offensives préparées
  (concentration des divisions à deux pas, aviation placée à portée, flottes pour couvrir les
  débarquements), renforts vers le point le plus menacé, généraux affectés selon leur trait.
- **Logistique** : le ravitaillement part du territoire national et des dépôts ; une poche coupée est
  encerclée et s'use. Fortifications (trois niveaux) et dépôts se construisent région par région.
- **Brouillard de guerre** : l'ennemi n'est visible qu'au contact, à portée de l'aviation et des flottes,
  ou sous reconnaissance satellitaire ; un assaut à l'aveugle est pénalisé.
- **Groupes d'armées** : quatre groupes auxquels on confie un front à tenir ou une offensive contre une
  nation ; les ordres se répartissent automatiquement.
- **Missions navales** : patrouille et blocus, frappe à terre (porte-avions, frégates, sous-marins),
  guerre au commerce contre les convois ennemis, escorte des nôtres.
- **Négociation** : l'ennemi pèse sa lassitude (durée de la guerre, blocus, armée épuisée, agitation,
  dissuasion) ; il accepte, ou fait une contre-proposition qu'on peut accepter ou amender (trois
  propositions par trimestre). Une offre de paix refusée revient plus tard, plus généreuse.

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

### Opérations ciblées (M2MC)
Huit opérations contre une nation précise (onglet NATIONS, vue Opérations), selon nos capacités :
- cyber : frappe sur le réseau électrique (-40 % d'énergie), vol de technologies, paralysie logistique ;
- informationnel : ingérence électorale (élections anticipées), déstabilisation ;
- espace : reconnaissance satellitaire (+15 % en attaque, +10 % en défense face à elle), tir
  antisatellite (débris et réprobation) ;
- électromagnétique : brouillage de son aviation et de ses drones.

Chaque opération peut être attribuée à son auteur : relations, influence, riposte cyber, voire casus
belli. Les autres nations y recourent aussi, entre elles comme contre la France.

### Organisations internationales et dette
- **OTAN** : article 5 (les membres volent au secours d'un allié attaqué selon la cohésion de
  l'Alliance), crises quand deux membres s'affrontent, sortie et retour de la France.
- **Conseil de sécurité** : une agression peut valoir une résolution ; les membres permanents
  (France, Royaume-Uni, Fédération Boréale, États-Unis et Chine hors carte) peuvent opposer leur veto.
  Les **sanctions** coupent le commerce et les ressources ; les longues guerres finissent par un
  cessez-le-feu des casques bleus. La France peut saisir le Conseil.
- **Dette publique** : les déficits sont financés par l'emprunt ; la note (AAA à CCC) fixe le taux
  d'intérêt. Au-delà, crise de la dette : plan de sauvetage européen avec austérité, ou défaut.

### Ressources stratégiques et montée des eaux
- **Terres rares, uranium, semi-conducteurs** : gisements (Kiruna, Kola, Ukraine, Turquie, Limousin…),
  fonderies (Eindhoven, Saxe, Grenoble, Irlande…), importations des partenaires et du reste du monde,
  crises mondiales (embargo chinois, blocus de Taïwan, coups d'État au Sahel). Les unités modernes en
  consomment au recrutement ; une pénurie bride les SMR, l'éolien, la recherche, l'industrie et la
  crédibilité de la dissuasion. On peut acheter sur le marché, rouvrir les mines françaises ou bâtir
  des fonderies.
- **Montée des eaux** liée au réchauffement : les deltas et les côtes basses (Pays-Bas, Flandre, Venise,
  delta du Nil, Camargue, Vendée…) sont submergés peu à peu, visible sur la carte ; submersions marines
  lors des tempêtes. Les digues des pays riches et le Plan Adaptation Climat limitent les dégâts. Les ports
  trop submergés ne peuvent plus accueillir de flotte, et l'adaptation des côtes a un coût.
- Les autres nations achètent aussi leurs ressources sur le marché et les échangent entre alliés.

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
- Plus de 60 événements contextuels (canicules, submersions, krachs, émeutes, rançongiciels, essais
  nucléaires, sommets de l'OTAN...) qui ne surviennent que lorsque leurs conditions sont réunies
  (température, guerre, régime, dette, chômage, factions, voisins...). Ils sont décrits par des données
  dans `src/events.c` : le texte de leurs effets est généré à partir de ce qui est appliqué.

### Langues
- Français et anglais (bouton du menu, réglage conservé). Toutes les chaînes passent par `Tr()` ;
  la table anglaise est tenue dans `tools/lang/en.tsv` (une ligne par chaîne : français, tabulation,
  anglais). `python3 tools/lang_extract.py` liste les chaînes non traduites,
  `python3 tools/lang_build.py` vérifie les formats (`%d`, `%s`...) et régénère `src/lang_en.c`.

### Son
- Effets sonores (boutons, fin de tour, alertes, batailles, frappes nucléaires, victoire, défaite) et
  nappe d'ambiance synthétisés au lancement par `src/sound.c` : aucun fichier audio. Activable ou
  coupé depuis le menu (réglage conservé).

### Scénarios
- **Janvier 2030** : +1,5°C, trente ans à tenir.
- **La crise de 2040** : +2°C, mer +45 cm, invasion boréale des pays baltes, Libye effondrée, Algérie
  au seuil nucléaire, trésor vide et dette record ; vingt ans à tenir. À choisir dans le menu.

### Score et victoire
- Score détaillé en neuf catégories : survie, population, prospérité, territoire, stabilité,
  résilience, puissance, réputation et pénalités.
- Il est multiplié selon la difficulté et selon le type de victoire : survie, transition réussie,
  hégémonie, utopie réalisée ou ordre nouveau.
- **Défaite** si la Cohésion ou la Souveraineté tombe à 0, en cas de défaut sur la dette, si Paris
  est occupée, ou en cas d'échange nucléaire.

### Confort
- Tutoriel pas à pas (menu principal).
- Huit modes de carte : politique, climat, militance, population, cultures, relations, logistique,
  ressources ; historique de chaque nation (population, armée, relation).
- Écrans tactiles : pincer pour zoomer, bouton ORDRE pour envoyer les unités.
- **Grande interface** (par défaut sur Android, `F2` ou menu ailleurs) : une partie de l'écran à la fois
  (carte, panneau, bas du panneau, journal, barre des ressources), agrandie à toute la taille de
  l'écran ; une barre de navigation en bas passe de l'une à l'autre. Les fenêtres (dilemmes, rapports,
  négociations) s'affichent automatiquement en grand.
- Rapport de fin de tour, désactivable.
- Trois niveaux de difficulté.
- Sauvegarde manuelle (`F5`, menu) et automatique tous les 4 tours ; chargement par `F9` ou par le
  menu. Le format est enregistré champ par champ (nom, taille, données) : une sauvegarde reste
  lisible après une mise à jour du jeu, les champs nouveaux prenant leur valeur par défaut. Après
  toute modification de la structure `Game`, régénérer le registre : `python3 tools/gen_save_fields.py`
  (la CI vérifie qu'il est à jour). Les sauvegardes de la version 1.0.0 ne sont plus lisibles.

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
| Enregistrer le rejeu de la partie | `F6` |
| Plein écran / menu | `F11` / `Échap` |
| Grande interface | `F2` |

## Téléchargement

Les releases GitHub fournissent des paquets prêts à jouer pour **Windows** (x64), **Linux** (x64),
**macOS** (Apple Silicon) et **Android** (arm64). Ils sont construits par
`.github/workflows/release.yml` quand le fichier `VERSION` change (le tag est alors créé automatiquement)
ou à chaque tag `v*`, avec `tools/release/build.sh`, utilisable aussi
en local (`tools/release/build.sh windows ~/raylib` compile la version Windows depuis Linux avec
MinGW-w64). Pour signer l'APK avec une clé stable, ajoutez les secrets `ANDROID_KEYSTORE_B64` et
`ANDROID_KEYSTORE_PASS` ; sinon une clé de débogage est générée.

L'intégration continue (`.github/workflows/ci.yml`) vérifie chaque push : tests, tests sous
AddressSanitizer et UBSan, quelques parties automatiques, puis compilation pour les quatre
plateformes (paquets téléchargeables 7 jours dans l'onglet Actions).

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
  parapluie européen, traités et contre-propositions, défaite à Paris, généraux, technologies, embargo,
  cultures, opérations ciblées, ressources stratégiques, montée des eaux, ravitaillement et encerclement,
  brouillard, fortifications, groupes d'armées, missions navales, dette et plan de sauvetage, ONU,
  article 5, offres de paix, commandes et rejeu déterministe, sauvegarde.
- `make sim` (ou `./build/autoplay [parties] [difficulté 0-2] [scénario 0-1]`) joue des parties
  automatiques sans interface.

Taux de victoire indicatifs des bots (qui jouent leur stratégie sans erreur) :

| Stratégie | Facile | Normale | Difficile |
|---|---|---|---|
| Passive (ne fait rien) | 0 % | 0 %, effondrement vers 2041 | 0 % |
| Gestionnaire défensif | 100 % | 100 % | 20 % |
| Stratège (régime autoritaire, groupes d'armées, opérations, guerres choisies) | 100 % | 30 % | 0 % |
| Gestionnaire, crise de 2040 | 100 % | 60 % | 40 % |
| Stratège, crise de 2040 | 100 % | 40 % | 10 % |
| Conquérant total (junte, guerres en chaîne) | 0 % | 0 % : coalitions, révoltes, faillite | 0 % |

Le stratège utilise les groupes d'armées, les généraux, la reconnaissance et le brouillage, la guerre
au commerce et le pluralisme culturel ; il dépense en revanche toute son influence et finit souvent
emporté par la guerre hybride de fin de partie. La dette héritée dépend de la difficulté (220, 300 ou
420 Md).

## Multijoueur

**Hot-seat** (plusieurs joueurs sur le même appareil) : bouton `HOT-SEAT` du menu. Le premier joueur
dirige la France ; jusqu'à trois autres choisissent chacun une nation et en commandent l'armée et la
diplomatie militaire : recrutement payé sur le trésor national, ordres, déclarations de guerre,
propositions de paix, alliances avec les nations de l'IA. L'économie, la politique et les services
de ces nations restent automatiques. Chaque trimestre, la France joue, puis chaque commandant à son
tour (bouton `FIN DE PHASE`), puis le trimestre se résout ; un écran de passage masque la carte entre
deux joueurs et chacun ne voit que son propre brouillard de guerre. Une paix entre deux joueurs
exige que chacun la propose ; une paix proposée à la France lui est présentée comme une offre de
traité.

Toutes les actions de l'interface passent par des commandes sérialisables et validées, la
simulation est déterministe et vérifiable par somme de contrôle, et une partie peut être rejouée :
c'est la base d'un futur jeu en réseau. Voir [docs/MULTIJOUEUR.md](docs/MULTIJOUEUR.md).

## Organisation du code

```
src/game.h          types et API de la simulation
src/game_internal.h outils partagés entre modules
src/world.c         données : nations, provinces, régimes, unités, idéologies, décrets, dilemmes, scénarios
src/cabinet.c       ministres, lobbies et popularité du gouvernement
src/events.c        événements contextuels (conditions et effets décrits par des données)
src/game.c          tour de jeu, guerre hybride M2MC, projets, dilemmes, score, sauvegarde
src/society.c       climat, montée des eaux, population, strates, cultures, migrations, révoltes, régimes
src/economy.c       production, emploi, stocks, marché mondial, embargos, blocus, impôts, usines, trésor des IA
src/politics.c      courants par strate, élections, décrets, vie politique des IA
src/war.c           unités, batailles, ravitaillement, brouillard, forts, groupes, missions navales, IA, traités
src/military.c      généraux et technologies militaires
src/ops.c           opérations cyber, informationnelles, spatiales et électromagnétiques
src/resources.c     ressources stratégiques (terres rares, uranium, semi-conducteurs)
src/nuclear.c       dissuasion, prolifération, armes tactiques
src/intl.c          ONU, OTAN, sanctions
src/command.c       commandes, phases du hot-seat, somme de contrôle et rejeu
src/save_fields.c/.h registre des champs sauvegardés (généré)
src/map_data.c/.h   carte générée (grille 0,125°, 130 régions, adjacences, cultures, religions)
src/main.c          interface raylib
src/sound.c         sons synthétisés (effets et ambiance)
tools/gen_save_fields.py générateur du registre de sauvegarde
tools/gen_map.py    générateur de la carte depuis un GeoJSON des pays
tools/autoplay.c    banc d'essai d'équilibrage
tools/tests.c       tests des mécaniques
tools/release/      paquets Windows, Linux, macOS et Android
docs/MULTIJOUEUR.md architecture prévue pour le multijoueur
```

Pour régénérer la carte : `python3 tools/gen_map.py countries.geo.json`, avec le fichier
`countries.geo.json` de [world.geo.json](https://github.com/johan/world.geo.json)
(contours Natural Earth, domaine public).

Les factions hybrides (Fédération Boréale, Ligue des Sables, Consortium Hydra, Réseau Nébula) et
tous les événements sont fictifs.
