# BOUCLIER 2050

Jeu de grande stratégie et de guerre hybride au tour par tour, en C avec [raylib](https://www.raylib.com/).

**2030.** Le monde a franchi +1,5 °C et le réchauffement s'accélère. Sécheresses, mégafeux et
effondrement agricole vident le Sud et jettent des millions de personnes sur les routes ; l'eau,
l'énergie et la nourriture deviennent des armes. Vous dirigez la France pendant trente ans, jusqu'en
2060 : vous gérez sa population, choisissez son régime, défendez ses frontières dans tous les
**milieux et champs (M2MC)** (terre, mer, air, espace, cyber, informationnel, électromagnétique),
ou vous conquérez celles des autres.

## Principes de jeu

- **Carte Europe & Méditerranée** : 66 provinces, 27 nations, du Maroc à la Volga et de la
  Scandinavie à l'Égypte. Quatre modes de carte : politique, climat, militance, population.
- **Population façon Victoria** : chaque province a ses habitants, sa croissance, sa prospérité,
  sa part de population à intégrer et la militance de trois strates (travailleurs, classes
  moyennes, élites). Pénuries, guerre, dette, occupation et régime les font monter ou baisser.
  Au-delà de 6,5 : révoltes, sécessions de territoires conquis.
- **Climat** : la température passe d'environ +1,5 °C à +3,3 °C. L'habitabilité baisse, d'abord
  au Sud. Les récoltes et l'eau diminuent, des États deviennent faillis et les populations migrent
  vers les provinces plus vivables et plus riches.
- **Migrations** : la doctrine (accueil, équilibre, fermeture) décide combien de migrants entrent.
  Les refoulés deviennent des flux clandestins aux frontières, que la Ligue des Sables instrumentalise.
- **Ressources** : l'énergie, l'eau et l'alimentation comparent la production de vos provinces
  (plus les projets et les accords commerciaux) aux besoins de la population. Conquérir des
  provinces riches (gaz algérien, pétrole libyen, greniers à blé) change l'équation.
- **Régimes** : Commune écologique, Démocratie libérale, Technocratie verte, Régime autoritaire,
  Junte militaire, État totalitaire. Chacun modifie l'impôt, la mobilisation, la répression,
  la lassitude de guerre, l'intégration, la diplomatie et la satisfaction de chaque strate.
- **Guerre et diplomatie** : armée en divisions, déclarations de guerre, offensives province par
  province, ultimatums, paix, accords commerciaux. Les IA se font aussi la guerre entre elles :
  la Fédération Boréale peut envahir un allié européen et déclencher l'article 42.7. L'agression
  a un prix : exclusion de l'UE, réarmement de l'Europe, coalitions, territoires à intégrer.
- **Défense M2MC** : 12 théâtres (frontières, mers, ciel, orbite, cyberespace, champ
  informationnel, spectre électromagnétique) et 18 types de menaces hybrides de 4 factions.
  Les capacités air, espace, cyber et EM renforcent aussi l'armée conventionnelle.
- **Projets de résilience et dilemmes** : 8 grands projets et 16 décisions du Conseil de défense,
  dont des coups d'État et des révolutions.

**Défaite** si la Cohésion ou la Souveraineté tombe à 0, si le Budget passe sous -60 ou si Paris tombe.

### Commandes

| Action | Commande |
|---|---|
| Inspecter une province | clic sur la carte |
| Attaquer | province ennemie marquée d'un X, onglet Province |
| Fin du tour | `FIN DU TOUR`, `Espace` ou `Entrée` |
| Onglets | `1` à `7` |
| Mode de carte | `M` ou boutons sous la carte |
| Plein écran / menu | `F11` / `Échap` |

Survolez un élément pour afficher une infobulle (raison d'indisponibilité comprise).

## Compilation

Le jeu n'utilise que des formes 2D, une texture de carte générée et la police intégrée de raylib :
aucun fichier externe. La résolution logique est de 1280×720, mise à l'échelle automatiquement.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/bouclier2050
# ou
make RAYLIB_PATH=~/raylib/src && make run
```

CMake télécharge raylib 5.0 s'il n'est pas installé. Dépendances Linux (Debian/Ubuntu) :
`sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
Sous Windows : [w64devkit](https://github.com/skeeto/w64devkit) puis `make RAYLIB_PATH=C:/raylib/raylib/src`.

Machines anciennes : compilez raylib en OpenGL 2.1
(`make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_21` dans `raylib/src`).

## Équilibrage

`make sim` joue 800 parties automatiques sans interface. Résultats indicatifs :

| Stratégie | Victoires |
|---|---|
| Passive (ne fait rien) | 0 %, effondrement vers 2042 |
| Gestionnaire défensif | ~90 % |
| Stratège (régime autoritaire, quelques conquêtes) | ~80 % |
| Conquérant total (junte, guerres en chaîne) | 0 % : coalitions, révoltes, effondrement |

## Organisation du code

```
src/game.h          types et API de la simulation
src/world.c         données : nations, provinces, régimes, menaces, dilemmes
src/game.c          cœur : tour de jeu, guerre hybride M2MC, projets, dilemmes
src/society.c       climat, production, population, migrations, militance, régimes
src/war.c           armées, combats, diplomatie, IA des nations
src/map_data.c/.h   carte générée (grille 0,25°, provinces, adjacences)
src/main.c          interface raylib
tools/gen_map.py    générateur de la carte depuis un GeoJSON des pays
tools/autoplay.c    banc d'essai d'équilibrage
```

Pour régénérer la carte : `python3 tools/gen_map.py countries.geo.json`, avec le fichier
`countries.geo.json` de [world.geo.json](https://github.com/johan/world.geo.json)
(contours Natural Earth, domaine public).

Les factions hybrides (Fédération Boréale, Ligue des Sables, Consortium Hydra, Réseau Nébula)
et tous les événements sont fictifs.
