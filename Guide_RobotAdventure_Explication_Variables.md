# Comprendre le projet GitHub RobotAdventure (BOUCLIER 2050)

**Guide de lecture du code C et explication des principales variables**  
**Dépôt :** [wakadu13/RobotAdventure](https://github.com/wakadu13/RobotAdventure)  
**Branche étudiée :** `claude/wonderful-curie-acvl8n`

> **Portée du document.** Ce guide reprend et reformule l'analyse des fichiers principaux du dépôt : il explique l'architecture, les structures de données et les variables majeures. Il ne constitue pas encore un inventaire exhaustif de toutes les variables locales de tous les fichiers du projet.

## 1. Présentation du jeu

Le projet, actuellement intitulé **BOUCLIER 2050**, est un jeu de grande stratégie développé en **langage C** avec la bibliothèque graphique **raylib**.

Le joueur dirige la France entre **2030 et 2060**. Il doit gérer l'économie, les finances publiques, les ressources, la population, les relations diplomatiques, la défense, les guerres et les crises climatiques.

Pour comprendre un code développé avec l'aide d'une IA, il est utile de distinguer :

1. **Les variables**, qui mémorisent une information (budget, population, position d'une unité, etc.).
2. **Les fonctions**, qui réalisent une opération (calculer les dépenses, déplacer une armée, terminer un tour).
3. **Les structures**, qui rassemblent plusieurs variables décrivant un même objet (province, nation, unité militaire, partie complète).

### Organisation des principaux fichiers

| Fichier | Fonction principale |
|---|---|
| [`src/main.c`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/main.c) | Démarrage, boucle principale, menus, interaction avec le joueur |
| [`src/ui.h`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/ui.h) | Définitions et variables partagées par les composants de l'interface |
| [`src/command.c`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/command.c) | Vérification et application des commandes des joueurs |
| [`src/game.h`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/game.h) | Définitions des types, énumérations et structures centrales |
| [`src/game.c`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/game.c) | Initialisation de la partie et déroulement des tours |
| [`src/game_internal.h`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/game_internal.h) | Fonctions utilitaires partagées entre les modules de simulation |
| `src/economy.c` | Production, commerce, fiscalité, dépenses et dette |
| `src/society.c` | Population, migrations, culture, intégration et société |
| `src/politics.c` | Vie politique et régimes |
| `src/war.c` | Armées, combats et conflits |
| `src/world.c` | Définitions du monde et données associées |
| `src/ui_map.c` | Carte interactive |
| `src/military.c` | Recherche et encadrement militaires |
| `src/nuclear.c` | Dissuasion et opérations nucléaires |
| `src/ops.c` | Opérations ciblées dans différents domaines |

L'architecture sépare les **données**, les **calculs** et leur **affichage**. Par exemple, les règles de calcul des recettes sont principalement dans `economy.c`, tandis que la manière de les présenter à l'écran est gérée par les fichiers d'interface.

```text
main.c / ui_*.c
    |  actions et affichage du joueur
    v
command.c
    |  validation des ordres
    v
game.c + game.h
    |  état et déroulement de la partie
    v
economy.c / society.c / war.c / politics.c / etc.
    |  calculs spécialisés
    v
Mise à jour des données de Game, puis affichage du nouvel état
```

## 2. La structure centrale : `Game`

Dans `src/main.c`, le programme déclare :

```c
Game game;
```

- **`Game`** est un type de structure défini dans `game.h`.
- **`game`** est la variable contenant l'état complet d'une partie.

On peut la comparer à un **grand registre** qui rassemble toutes les données nécessaires à la simulation : budget national, climat, provinces, nations, unités, technologies, historique, guerres et état de victoire.

Exemples :

```c
game.turn                 // numéro du tour
game.temp                 // réchauffement climatique simulé
game.res[R_BUDGET]        // trésorerie française
game.prov[0].pop          // population de la province d'indice 0
```

Chaque expression sélectionne une information dans la structure générale.

## 3. Lire les déclarations et les expressions en C

| Écriture | Signification |
|---|---|
| `int turn` | Nombre entier : numéro du tour |
| `float temp` | Nombre décimal : température |
| `bool active` | Booléen : `true` ou `false` |
| `char name[28]` | Tableau pouvant contenir une chaîne de caractères de 27 caractères maximum, avec son terminateur `\0` |
| `float res[7]` | Tableau de sept nombres décimaux |
| `Game game` | Variable de type structure `Game` |
| `game.turn` | Accès au champ `turn` d'une structure |
| `g->turn` | Accès au champ `turn` par un pointeur sur une structure |
| `#define MAX_UNITS 900` | Macro définissant une limite nommée |

### Exemple d'affectation

```c
g->res[R_BUDGET] = 35;
```

Cette instruction signifie :

- `g` : pointeur vers l'état de la partie ;
- `res` : tableau des ressources générales ;
- `R_BUDGET` : indice correspondant au budget ;
- `35` : valeur attribuée à la ressource.

**Attention :** `=` est une **affectation**. Pour tester l'égalité, le C emploie `==`.

### Les opérateurs à connaître

```c
x = 10;    // affecte 10 à x
x += 3;    // ajoute 3 à x : x vaut alors 13
x -= 2;    // retire 2 à x : x vaut alors 11
x++;       // ajoute 1 à x
x--;       // retire 1 à x
x == 11;   // compare x à 11 (vrai ou faux)
```

Les expressions d'égalité et de comparaison sont surtout utiles dans des conditions `if`.

## 4. Les sept ressources fondamentales

Le type `Resource` est défini dans `game.h` :

```c
typedef enum {
    R_BUDGET, R_ENERGIE, R_EAU, R_ALIM,
    R_COHESION, R_INFLUENCE, R_SOUVERAINETE,
    R_COUNT
} Resource;
```

Un **`enum`** donne des noms explicites à des valeurs entières. Ici, ces noms servent notamment d'indices dans `res[]`.

| Constante / indice | Rôle | Valeur initiale dans `Game_Init()` |
|---|---|---:|
| `R_BUDGET` | Trésorerie de l'État | 35 |
| `R_ENERGIE` | Sécurité de l'approvisionnement énergétique | 60 |
| `R_EAU` | Sécurité de l'approvisionnement en eau | 60 |
| `R_ALIM` | Sécurité alimentaire | 62 |
| `R_COHESION` | Cohésion sociale | 62 |
| `R_INFLUENCE` | Influence internationale | 55 |
| `R_SOUVERAINETE` | Souveraineté nationale | 75 |

Les six indicateurs autres que `R_BUDGET` sont limités à l'intervalle de **0 à 100** lors de leur modification par la fonction `AddRes()`. Le budget obéit à une logique financière distincte.

Par exemple :

```c
g->res[R_BUDGET] = 35;       // valeur de trésorerie
g->res[R_COHESION] = 62;     // indicateur de cohésion
```

Une crise énergétique peut faire diminuer `R_ENERGIE` ; une crise politique peut faire diminuer `R_COHESION`.

## 5. Dictionnaire des variables de `Game`

Le type `Game` est défini dans [`game.h`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/game.h). Les tableaux peuvent contenir plusieurs valeurs, une par catégorie, nation, province ou tour.

### 5.1. Temps, climat et ressources

| Variable | Rôle |
|---|---|
| `rng` | État du générateur pseudo-aléatoire |
| `turn` | Tour actuel, chaque tour correspondant à un trimestre |
| `difficulty` | Difficulté choisie |
| `temp` | Réchauffement climatique simulé, en °C |
| `res[R_COUNT]` | Valeurs des sept grandes ressources |
| `lastDelta[R_COUNT]` | Variation de chaque ressource au dernier tour |
| `seaLevel` | Élévation simulée du niveau marin |
| `climateMitigation` | Effet des mesures d'atténuation du changement climatique |
| `startTurn` | Tour auquel débute le scénario |

### 5.2. Défense et menaces

| Variable | Rôle |
|---|---|
| `capacity[D_COUNT]` | Capacités françaises par domaine militaire |
| `pendingCap[D_COUNT]` | Capacités supplémentaires en préparation |
| `pendingTurns[D_COUNT]` | Tours restant avant leur mise en service |
| `posture[TH_COUNT]` | Posture défensive de chaque théâtre |
| `tension[TH_COUNT]` | Tensions dans les théâtres |
| `threats[MAX_THREATS]` | Ensemble des menaces hybrides |
| `fac[F_COUNT]` | Situation des factions adverses |
| `proj[P_COUNT]` | Projets en cours ou achevés |
| `units[MAX_UNITS]` | Ensemble des unités militaires |
| `generals[MAX_GENERALS]` | Généraux disponibles |
| `groups[GROUP_COUNT]` | Groupes d'armées, leurs modes et leurs cibles |
| `visible[PROV_COUNT]` | Provinces visibles selon le renseignement et le brouillard de guerre |
| `supply[N_COUNT][PROV_COUNT]` | Ravitaillement par nation et province |
| `aggressionMemory` | Mémoire des comportements agressifs |
| `stratShock[SR_COUNT]` | Durée des perturbations mondiales de ressources stratégiques |

### 5.3. Économie et finances

| Variable | Rôle |
|---|---|
| `debt` | Dette publique française |
| `inflation` | Inflation annuelle française |
| `priceLevel` | Indice général des prix |
| `eco[N_COUNT]` | Situation économique de chaque nation |
| `price[G_COUNT]` | Prix mondiaux des biens |
| `taxRate[S_COUNT]` | Taux d'imposition par classe sociale |
| `spending[SP_COUNT]` | Niveau des dépenses publiques par secteur |
| `tariff` | Droits de douane |
| `budget[BL_COUNT]` | Lignes de recettes et de dépenses |
| `bailouts` | Plans de sauvetage financier |
| `austerityTurns` | Tours d'austérité restants |
| `migrantsIn` | Migrants entrants comptabilisés |
| `migrantsRefused` | Migrants refusés comptabilisés |
| `migrantsOut` | Migrants sortants comptabilisés |
| `importBonus` | Bonus appliqué aux importations |

### 5.4. Relations internationales

| Variable | Rôle |
|---|---|
| `nat[N_COUNT]` | État des nations |
| `war[N_COUNT][N_COUNT]` | Indique les paires de nations en guerre |
| `warSince[N_COUNT][N_COUNT]` | Début des guerres |
| `truce[N_COUNT][N_COUNT]` | Trêves en vigueur |
| `ally[N_COUNT][N_COUNT]` | Alliances militaires |
| `airel[N_COUNT][N_COUNT]` | Relations diplomatiques entre nations non françaises |
| `battles[N_COUNT][N_COUNT]` | Nombre de victoires militaires d'une nation contre une autre |
| `treaty` | Traité de paix en préparation |
| `natoCohesion` | Cohésion de l'OTAN |
| `unCooldown` | Délai avant certaines nouvelles actions à l'ONU |
| `route[N_COUNT]` | Qualité des routes commerciales vers la France |
| `routeKind[N_COUNT]` | Type de liaison commerciale |
| `aggressor[N_COUNT][N_COUNT]` | Historique des agressions entre nations |

### 5.5. Politique, recherche et décisions

| Variable | Rôle |
|---|---|
| `researchArea` | Domaine de recherche militaire sélectionné |
| `researchProgress` | Avancement de la recherche |
| `researchBudget` | Budget alloué à la recherche |
| `regimeCooldown` | Délai limitant les changements de régime |
| `decreeCooldown[2]` | Délai d'utilisation de certains décrets |
| `sobrietyTurns` | Durée restante de mesures de sobriété |
| `mobilizationTurns` | Durée restante d'une mobilisation |
| `boostTurns` | Durée d'un effet temporaire de stimulation |
| `propagandaTurns` | Durée d'un effet de propagande |
| `migrationDoctrine` | Doctrine migratoire choisie |
| `culturePolicy` | Politique culturelle |
| `alertLevel` | Niveau d'alerte nationale |
| `commandPoints` | Points de commandement disponibles |
| `nukePosture` | Posture nucléaire |
| `credibility` | Crédibilité de la dissuasion |
| `nukesUsed` | Frappes nucléaires stratégiques utilisées |
| `tacticalUsed` | Frappes nucléaires tactiques utilisées |
| `umbrella` | Activation du parapluie nucléaire |
| `aiDefense` | État de la défense pilotée par l'IA |
| `human[N_COUNT]` | Nations contrôlées par des joueurs humains |
| `activePlayer` | Nation dont c'est la phase |
| `scenario` | Scénario choisi |
| `minister[MIN_COUNT]` | Ministres nommés |
| `candidates[MIN_COUNT][3]` | Candidats aux postes ministériels |
| `lobby[LB_COUNT]` | Influence des groupes d'intérêt |
| `approval` | Approbation du gouvernement |

### 5.6. Événements, rapports, historiques et statistiques

| Variables | Rôle |
|---|---|
| `pendingDilemma`, `dilemmaParam`, `dilemmaParam2` | Dilemme en attente et paramètres associés |
| `dilemmaUsed[]`, `eventUsed[]` | Événements ou dilemmes déjà utilisés |
| `eventQueue[]`, `eventDue[]` | Événements différés et échéances |
| `reportReady`, `reportLogStart` | Disponibilité et point de départ du rapport trimestriel |
| `reportRes[]`, `reportPop`, `reportProv`, `reportUnits`, `reportScore` | Données utilisées pour le compte rendu du tour |
| `histPop[]`, `histTemp[]`, `histDebt[]`, `histBudget[]` | Séries historiques de population, température, dette et budget |
| `histCohesion[]`, `histScore[]`, `histInflation[]`, `histApproval[]` | Autres courbes historiques |
| `histNatPop[][]`, `histNatArmy[][]`, `histNatRel[][]` | Évolution des nations dans le temps |
| `histLobby[][]` | Historique des groupes d'intérêt |
| `log[]`, `logCount` | Journal de partie et nombre de messages |
| `state` | Partie en cours, gagnée ou perdue |
| `statNeutralized`, `statSuffered`, `statDisasters` | Statistiques de menaces et catastrophes |
| `statConquered`, `statLost` | Territoires gagnés ou perdus |
| `statBattlesWon`, `statBattlesLost` | Batailles remportées ou perdues |
| `everAtWar`, `leftDemocracy` | Indicateurs utilisés notamment pour les succès du joueur |

## 6. Structure `Province` : les caractéristiques d'une région

Le fichier `game.h` déclare notamment :

```c
typedef struct {
    int owner;
    int ctrl;
    int core;
    float pop;
    float share[S_COUNT];
    float mil[S_COUNT];
    float con[S_COUNT];
    float ideo[S_COUNT][I_COUNT];
    // Autres champs omis ici.
} Province;
```

| Variable | Rôle |
|---|---|
| `owner` | Nation propriétaire juridiquement |
| `ctrl` | Nation contrôlant militairement la province |
| `core` | Nation d'origine du territoire |
| `pop` | Population, en millions d'habitants |
| `share[]` | Répartition par classes sociales |
| `mil[]` | Militance par classe sociale |
| `con[]` | Conscience politique par classe |
| `ideo[][]` | Répartition des idéologies |
| `culture[]` | Répartition des cultures |
| `religion[]` | Répartition des religions |
| `literacy` | Alphabétisation |
| `hab` | Habitabilité |
| `integration` | Niveau d'intégration |
| `damage` | Dégâts subis |
| `fallout` | Contamination radioactive |
| `prosperity` | Niveau de prospérité |
| `industry` | Capacité industrielle |
| `unemployment` | Chômage |
| `jobs` | Emploi rapporté à la population active |
| `factoryBuild` | Tours restant avant l'achèvement d'une usine |
| `fabBuild` | Construction d'une fonderie de semi-conducteurs |
| `fabs` | Production de semi-conducteurs |
| `mineClosed` | Gisement actuellement non exploité |
| `flooded` | Part du territoire submergée |
| `depot` | Dépôt logistique éventuel |
| `fort` | Niveau de fortification |
| `fortBuild` | Chantier de fortification |
| `revolt` | État de révolte |
| `emig` | Émigration |
| `immig` | Immigration |

**Distinction essentielle :** une province peut légalement appartenir à la France (`owner`) mais être occupée par une autre nation (`ctrl`). La fonction `ProvinceEfficiency()` prévoit qu'une province occupée par l'ennemi ne produit plus pour son propriétaire.

## 7. Structure `Nation` : les États du jeu

Chaque nation dispose d'une structure `Nation`.

| Variable | Rôle |
|---|---|
| `alive` | La nation existe encore dans le jeu |
| `regime` | Régime politique |
| `relation` | Relations de la nation avec la France |
| `army` | Puissance militaire calculée |
| `eu` | Appartenance à l'Union européenne |
| `nato` | Appartenance à l'OTAN |
| `trade` | Situation des relations commerciales |
| `failed` | Situation de défaillance de l'État |
| `deterred` | État de dissuasion |
| `embargo` | Embargo français contre cette nation |
| `embargoFrance` | Embargo de cette nation contre la France |
| `government` | Courant politique au pouvoir |
| `nextElection` | Prochaine élection |
| `nukeStatus` | Statut du programme nucléaire |
| `nukeProgress` | Progression du programme nucléaire |
| `treasury` | Trésorerie nationale |
| `tech[]` | Niveaux des technologies militaires |
| `strat[]` | Stocks de ressources stratégiques |
| `manpower` | Réserve de mobilisation |
| `blockade` | Part du littoral sous blocus |
| `sanctions` | Durée des sanctions internationales |
| `aiTarget` | Objectif militaire de l'IA |
| `aiThreat` | Position considérée comme menacée par l'IA |

Autres champs : approvisionnements supplémentaires, réparations de guerre, tours de réparations, brouillage, panne électrique, reconnaissance satellitaire, négociations de paix, taxation pour les commandants humains, inflation nationale et état des actions du tour.

## 8. Structure `Unit` : les unités militaires

| Variable | Rôle |
|---|---|
| `active` | Unité active ou non |
| `type` | Type d'unité (infanterie, char, avion, navire, etc.) |
| `owner` | Nation propriétaire |
| `prov` | Province où se trouve l'unité |
| `target` | Province de destination, ou `-1` si aucun ordre |
| `str` | Force restante |
| `org` | Organisation de l'unité |
| `training` | Temps de formation restant |
| `group` | Groupe d'armées affecté |
| `mission` | Mission navale attribuée |
| `xp` | Expérience de l'unité |

D'autres structures décrivent les généraux (`General`), leurs compétences, leur trait et leur expérience.

## 9. Structure `Threat` : les menaces hybrides

| Variable | Rôle |
|---|---|
| `active` | Menace active |
| `type` | Type de menace |
| `theater` | Théâtre d'opérations |
| `faction` | Faction responsable |
| `strength` | Intensité de la menace |
| `turnsLeft` | Tours restant avant sa fin |
| `detected` | Menace détectée ou non |
| `fresh` | Indicateur de menace récente |
| `prov` | Province ciblée si applicable, `-1` sinon |

Autres structures importantes du jeu : `Faction`, `Project`, `NationEconomy`, `Treaty`, `LogEntry`.

## 10. Le déroulement d'un tour : `Game_EndTurn()`

Le cycle principal de simulation est dans [`src/game.c`, fonction `Game_EndTurn()`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/game.c#L944-L1058).

Les principales étapes sont :

1. **Budget** : calcul des recettes, dépenses et dettes avec `Eco_Budget()` et `Eco_Net()`.
2. **Projets** : avancement des constructions et mise en service de nouvelles capacités.
3. **Menaces hybrides** : traitement des menaces par `ResolveThreats()`.
4. **Climat et société** : calcul climatique puis population, économie et migrations avec `ClimateTurn()` et `Soc_Turn()`.
5. **Politique et défense** : traitement des systèmes politique, militaire, nucléaire, diplomatique et technologique.
6. **Factions** : actualisation des adversaires et apparition éventuelle de menaces.
7. **Historique** : mise à jour des statistiques du trimestre et incrémentation de `turn`.
8. **Fin de partie** : vérification des conditions de victoire ou de défaite, puis événements supplémentaires si la partie continue.

Extrait simplifié, fondé sur le code réel :

```c
/* Calcul du budget */
Eco_Budget(g, g->budget);
float net = Eco_Net(g);
g->res[R_BUDGET] += net;

/* Climat et population */
ClimateTurn(g);
Soc_Turn(g);

/* Guerres */
War_Turn(g);

/* Passage au trimestre suivant */
g->turn++;
```

L'instruction :

```c
g->res[R_BUDGET] += net;
```

équivaut à :

```c
g->res[R_BUDGET] = g->res[R_BUDGET] + net;
```

Elle ajoute au budget courant le résultat financier du trimestre.

## 11. Exemple complet : construire une usine

La fonction suivante provient de [`src/economy.c`](https://github.com/wakadu13/RobotAdventure/blob/claude/wonderful-curie-acvl8n/src/economy.c#L518-L524) :

```c
bool Eco_BuildFactory(Game *g, int p)
{
    if (Eco_FactoryBlocked(g, p))
        return false;

    g->res[R_BUDGET] -= Eco_FactoryCost(g);
    g->prov[p].factoryBuild = 4;

    Game_Log(g, EV_INFO,
        "Construction d'une usine en %s (4 tours).",
        PROVINCE_NAMES[p]);

    return true;
}
```

### Explication ligne par ligne

| Élément | Explication |
|---|---|
| `bool` | La fonction renvoie une valeur vraie ou fausse |
| `Eco_BuildFactory` | Nom de la fonction de construction |
| `Game *g` | Pointeur vers l'état complet de la partie |
| `int p` | Indice de la province concernée |
| `Eco_FactoryBlocked(g, p)` | Recherche un obstacle à la construction |
| `return false` | Construction refusée |
| `-= Eco_FactoryCost(g)` | Coût retiré du budget |
| `factoryBuild = 4` | Durée initiale du chantier : quatre tours |
| `Game_Log(...)` | Ajout d'un message au journal |
| `return true` | Construction lancée avec succès |

Pendant les tours suivants, la variable `factoryBuild` diminue. À son arrivée à zéro, `Eco_Turn()` augmente `industry` de 1.

On distingue donc :

- `factoryBuild` : **suivi du chantier** ;
- `industry` : **capacité industrielle déjà disponible**.

## 12. Variables de l'interface : `main.c`

Les variables d'affichage ne représentent pas nécessairement un état économique ou militaire : elles indiquent ce que l'utilisateur voit ou manipule.

| Variable | Rôle |
|---|---|
| `screen` | Écran actuellement affiché |
| `tab` | Onglet sélectionné |
| `mapMode` | Type de carte affichée |
| `selProvince` | Province sélectionnée |
| `selNation` | Nation sélectionnée |
| `selTheater` | Théâtre sélectionné |
| `hoverProvince` | Province survolée par la souris |
| `zoom` | Niveau de zoom de la carte |
| `camX`, `camY` | Position de la caméra |
| `mouse` | Position du pointeur |
| `clicked` | Clic gauche détecté |
| `rclicked` | Clic droit détecté |
| `toast` | Message temporaire |
| `toastTime` | Durée d'affichage restante du message |
| `newDifficulty` | Difficulté sélectionnée pour la prochaine partie |
| `newScenario` | Scénario sélectionné |
| `tutorial` | Étape du tutoriel |
| `logScroll` | Défilement du journal |
| `bigUi` | Activation de la grande interface |
| `timeAcc` | Temps accumulé pour des animations |
| `viewScale` | Facteur d'échelle de l'affichage |
| `target` | Texture dans laquelle l'interface est dessinée |
| `quitRequested` | Demande de fermeture de la partie |

### La boucle principale

La fonction `main()` démarre une fenêtre graphique puis effectue une boucle :

```c
while (!WindowShouldClose() && !quitRequested) {
    float dt = GetFrameTime();
    timeAcc += dt;
    UpdateView();
    NetUpdate();

    /* ... autres traitements et affichage ... */
}
```

Le programme vise **60 images par seconde** grâce à `SetTargetFPS(60)`.

**À ne pas confondre :** la boucle d'affichage est exécutée très fréquemment ; un **tour de jeu**, qui représente un trimestre, n'est calculé que lorsque la logique de passage au tour suivant est déclenchée.

## 13. Un exemple pédagogique de variation d'une ressource

Imaginons un budget initial de 35. L'État gagne 12 unités puis dépense 8 unités :

```c
float budget = 35;
budget += 12;  // 47
budget -= 8;   // 39
```

Le résultat est 39. Dans le jeu, l'accès au budget utilise le tableau `res[]` :

```c
g->res[R_BUDGET] = 35;
g->res[R_BUDGET] += 12;
g->res[R_BUDGET] -= 8;
// Résultat : 39
```

Cet exemple est **pédagogique** : il illustre les affectations, sans reproduire le calcul économique complet du jeu.

## 14. Méthode conseillée pour comprendre et modifier le projet

Pour progresser sans se perdre dans le nombre de fichiers, suivre cet ordre :

1. **Lire `game.h`** pour comprendre les types, `enum`, `struct` et tableaux.
2. **Lire `game.c`** pour comprendre comment la partie est initialisée et comment les tours avancent.
3. **Étudier `economy.c` puis `society.c`** pour voir des calculs concrets et suivre l'évolution des variables.
4. **Étudier `command.c`** pour comprendre comment les clics et décisions deviennent des actions validées.
5. **Étudier `main.c` et les fichiers `ui_*.c`** pour comprendre l'affichage et la souris.
6. **Étudier `war.c`, `military.c`, `nuclear.c` et `ops.c`** pour comprendre les systèmes militaires et stratégiques.

Pour chaque variable, noter systématiquement : **son type**, **sa valeur initiale**, **les fonctions qui la modifient**, **les fonctions qui l'utilisent** et **son effet visible sur le jeu**.

### Notions essentielles à approfondir

- `enum` : donner un nom à des constantes entières ;
- `struct` : regrouper des champs liés ;
- `[]` : accéder à un tableau ;
- `*` et `&` : pointeurs et adresses ;
- `->` : accéder aux champs via un pointeur ;
- `if`, `for` et `while` : conditions et boucles ;
- `return` : renvoyer un résultat ;
- `static` : restreindre la visibilité d'une fonction ou d'une variable au fichier, suivant le contexte.

---

## Conclusion

Le cœur de **BOUCLIER 2050** repose sur la structure `Game`. Les structures `Province`, `Nation`, `Unit` et `Threat` décrivent les différents objets du monde. Les fonctions de simulation modifient leurs variables au fur et à mesure des décisions du joueur et des tours. Les fichiers d'interface affichent ensuite ce nouvel état.

La suite la plus utile est une lecture détaillée de **`game.h`, ligne par ligne**, avant d'étendre l'analyse à chaque fonction et à chaque variable locale des modules du jeu.
