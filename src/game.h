/*
 * BOUCLIER 2050 - simulation (aucune dependance graphique)
 *
 * 2030-2060 : rechauffement climatique, penuries, migrations, guerre hybride
 * et conflits ouverts en Europe et en Mediterranee. La France defend ses
 * frontieres dans tous les milieux et champs (M2MC), gere sa population et
 * peut choisir son regime politique comme ses guerres.
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "map_data.h"

#define TOTAL_TURNS   120     /* 120 trimestres : 2030 -> 2060 */
#define START_YEAR    2030
#define MAX_THREATS   128
#define MAX_LOG       120
#define LOG_LEN       170
#define BASE_CP       3       /* points de commandement par tour */
#define THREAT_SCALE  0.6f    /* intensite globale des effets des menaces */
#define HISTORY_LEN   (TOTAL_TURNS + 1)

typedef enum { D_TERRE, D_MER, D_AIR, D_ESPACE, D_CYBER, D_INFO, D_EM, D_COUNT } Domain;

typedef enum {
    R_BUDGET, R_ENERGIE, R_EAU, R_ALIM, R_COHESION, R_INFLUENCE, R_SOUVERAINETE, R_COUNT
} Resource;

typedef enum { F_BOREALE, F_SABLES, F_HYDRA, F_NEBULA, F_COUNT } FactionId;

typedef enum {
    TH_NORD, TH_EST, TH_ALPES, TH_PYRENEES,
    TH_ATLANTIQUE, TH_MEDITERRANEE, TH_OUTREMER,
    TH_CIEL, TH_ORBITE, TH_CYBER, TH_INFO, TH_SPECTRE,
    TH_COUNT
} TheaterId;

typedef enum {
    P_SMR, P_ENR, P_DESSAL, P_AGRO, P_CYBERCIV, P_MEDIAS, P_CONSTELL, P_ADAPT, P_COUNT
} ProjectId;

typedef enum { A_DIPLO, A_SANCTION, A_CYBEROFF, A_INFLUENCE, A_RENSEIGNEMENT, A_COUNT } FactionAction;

typedef enum {
    N_FRANCE, N_UK, N_IRLANDE, N_BENELUX, N_ALLEMAGNE, N_SUISSE, N_AUTRICHE, N_ITALIE,
    N_ESPAGNE, N_PORTUGAL, N_DANEMARK, N_SCANDINAVIE, N_BALTES, N_POLOGNE, N_EUROPE_C,
    N_BALKANS, N_ROUMANIE, N_BULGARIE, N_GRECE, N_TURQUIE, N_UKRAINE, N_BOREALE,
    N_MAROC, N_ALGERIE, N_TUNISIE, N_LIBYE, N_EGYPTE, N_COUNT
} NationId;

typedef enum {
    RG_UTOPIE, RG_DEMOCRATIE, RG_TECHNOCRATIE, RG_AUTORITAIRE, RG_JUNTE, RG_TOTALITAIRE, RG_COUNT
} Regime;

typedef enum { S_TRAVAILLEURS, S_MOYENNES, S_ELITES, S_COUNT } Strata;

typedef enum { EV_INFO, EV_GOOD, EV_BAD, EV_CLIMATE, EV_EVENT, EV_WAR } LogKind;

typedef enum {
    GAME_RUNNING, GAME_WON,
    GAME_LOST_COHESION, GAME_LOST_SOUVERAINETE, GAME_LOST_BUDGET, GAME_LOST_CONQUEST
} GameState;

/* ---- definitions statiques ---- */
typedef struct { const char *name; const char *desc; Domain domain; } TheaterDef;

typedef struct {
    const char *name;
    Domain domain;
    bool migration;
    float effects[R_COUNT];   /* par point de force et par tour */
    int minStr, maxStr;
    int duration;
} ThreatDef;

typedef struct { const char *name; const char *desc; } FactionDef;
typedef struct { const char *name; const char *desc; float cost; int turns; } ProjectDef;

typedef struct {
    const char *name;
    const char *desc;
    int cp;
    float budget;
    float influence;
    Domain needDomain;
    int needCapacity;
} ActionDef;

typedef struct {
    const char *title;
    const char *text;
    const char *optA, *effA;
    const char *optB, *effB;
} DilemmaDef;

typedef struct {
    const char *name;
    const char *desc;
    float taxEff;          /* multiplicateur des recettes */
    float mobilization;    /* divisions mobilisables par million d'habitants */
    float repression;      /* baisse de militance par tour */
    float warWeariness;    /* cohesion perdue par tour de guerre */
    float aggressionCost;  /* cohesion perdue en declarant une guerre */
    float influence;       /* influence gagnee/perdue par tour */
    float assimilation;    /* vitesse d'integration des territoires et des migrants */
    float growth;          /* modificateur de croissance demographique (%/tour) */
    float projectCost;     /* multiplicateur du cout des projets */
    float quality;         /* qualite militaire */
    float sat[S_COUNT];    /* satisfaction de chaque strate (reduit la militance) */
} RegimeDef;

typedef struct {
    const char *name;
    unsigned char color[3];
    Regime regime;
    bool eu;
    float relation;        /* relation initiale avec la France */
    float aggressiveness;  /* propension a la guerre (IA) */
} NationDef;

typedef struct {
    NationId nation;
    float pop;             /* millions d'habitants en 2030 */
    float growth;          /* croissance naturelle (%/tour) */
    float aridity;         /* exposition au rechauffement 0..1 */
    float food, water, energy;   /* production (1 unite = besoin d'1 million d'habitants) */
    float wealth;          /* recettes fiscales par million d'habitants (Md/tour) */
} ProvinceDef;

extern const char       *DOMAIN_NAMES[D_COUNT];
extern const char       *RESOURCE_NAMES[R_COUNT];
extern const char       *RESOURCE_DESC[R_COUNT];
extern const char       *STRATA_NAMES[S_COUNT];
extern const TheaterDef  THEATERS[TH_COUNT];
extern const ThreatDef   THREATS[];
extern const int         THREAT_TYPE_COUNT;
extern const FactionDef  FACTIONS[F_COUNT];
extern const ProjectDef  PROJECTS[P_COUNT];
extern const ActionDef   ACTIONS[A_COUNT];
extern const DilemmaDef  DILEMMAS[];
extern const int         DILEMMA_COUNT;
extern const DilemmaDef  DYN_DILEMMAS[2];
extern const RegimeDef   REGIMES[RG_COUNT];
extern const NationDef   NATIONS[N_COUNT];
extern const ProvinceDef PROVINCES[PROV_COUNT];
extern const float       STRATA_SHARE[S_COUNT];
extern const char       *MIGRATION_DOCTRINES[3];
extern const char       *MIGRATION_DESC[3];
extern const float       MIGRATION_ACCEPT[3];
extern const char       *ALERT_LEVELS[3];
extern const char       *ALERT_DESC[3];

/* dilemmes dynamiques (hors tirage aleatoire) */
#define DLM_ALLY_CALL   100   /* un allie europeen est attaque */
#define DLM_PEACE_OFFER 101   /* un ennemi propose la paix */

/* ---- etat dynamique ---- */
typedef struct {
    bool active;
    int type;
    int theater;
    int faction;
    int strength;
    int turnsLeft;
    bool detected;
    bool fresh;
} Threat;

typedef struct {
    float hostility;
    float capability;
    int intelTurns;
    bool actedThisTurn;
} Faction;

typedef struct { int state; int turnsLeft; } Project;

typedef struct {
    char text[LOG_LEN];
    int turn;
    LogKind kind;
} LogEntry;

typedef struct {
    int owner;
    int core;              /* nation d'origine de la population */
    float pop;
    float foreign;         /* part de population non integree (0..1) */
    float mil[S_COUNT];    /* militance par strate (0..10) */
    float hab;             /* habitabilite (0..100) */
    float integration;     /* integration d'un territoire conquis (0..100) */
    float damage;          /* devastation par la guerre (0..1) */
    float prosperity;      /* multiplicateur de richesse, evolue avec la stabilite */
    int revolt;            /* tours de revolte en cours (0 = calme) */
    float emig, immig;     /* flux du dernier tour (millions) */
} Province;

typedef struct {
    bool alive;
    Regime regime;
    float relation;        /* -100..100 envers la France */
    float army;            /* divisions */
    bool atWar;            /* en guerre contre la France */
    int warTurns;
    int truce;             /* tours de treve restants */
    int warTarget;         /* guerre IA contre IA (-1 aucune) */
    int warTargetTurns;
    bool eu;
    bool trade;            /* accord commercial avec la France */
    bool failed;           /* Etat failli */
    int startProvinces;
    bool actedThisTurn;    /* action diplomatique de la France ce tour */
} Nation;

typedef struct {
    unsigned rng;
    int turn;
    float temp;
    float res[R_COUNT];
    float lastDelta[R_COUNT];

    int capacity[D_COUNT];
    int pendingCap[D_COUNT];
    int pendingTurns[D_COUNT];
    int posture[TH_COUNT];
    float tension[TH_COUNT];
    Threat threats[MAX_THREATS];
    Faction fac[F_COUNT];
    Project proj[P_COUNT];

    Nation nat[N_COUNT];
    Province prov[PROV_COUNT];
    float divisionsUsed;   /* divisions deja engagees ce tour */
    int regimeCooldown;
    float aggressionMemory;/* reputation d'agresseur (0..100) */

    /* bilans du dernier tour pour la France */
    float prod[3], cons[3];          /* alimentation, eau, energie */
    float migrantsIn, migrantsRefused, migrantsOut;

    int migrationDoctrine;
    int alertLevel;
    int commandPoints;

    bool aiDefense;
    float climateMitigation;
    float importBonus;

    int pendingDilemma;    /* -1 si aucun */
    int dilemmaParam;
    bool dilemmaUsed[32];

    float histPop[HISTORY_LEN];
    float histCohesion[HISTORY_LEN];
    float histTemp[HISTORY_LEN];

    LogEntry log[MAX_LOG];
    int logCount;

    GameState state;
    int statNeutralized;
    int statSuffered;
    int statDisasters;
    int statConquered;
    int statLost;
} Game;

/* game.c : coeur, guerre hybride, projets, dilemmes */
void        Game_Init(Game *g, unsigned seed);
void        Game_EndTurn(Game *g);
int         Game_PostureUsed(const Game *g, Domain d);
bool        Game_ChangePosture(Game *g, int theater, int delta);
float       Game_InvestCost(const Game *g, Domain d);
bool        Game_InvestCapacity(Game *g, Domain d);
float       Game_DefensePower(const Game *g, int theater);
float       Game_NeutralizeChance(const Game *g, const Threat *t);
const char *Game_ActionBlocked(const Game *g, int faction, int action);
bool        Game_FactionAction(Game *g, int faction, int action);
float       Game_ProjectCost(const Game *g, int project);
bool        Game_StartProject(Game *g, int project);
void        Game_SetMigrationDoctrine(Game *g, int d);
void        Game_SetAlertLevel(Game *g, int a);
void        Game_ResolveDilemma(Game *g, int choice);
const DilemmaDef *Game_CurrentDilemma(const Game *g);
float       Game_Upkeep(const Game *g);
int         Game_ThreatCount(const Game *g, int theater);
int         Game_ActiveThreats(const Game *g);
void        Game_DateString(int turn, char *buf, int len);
int         Game_Score(const Game *g);
void        Game_Log(Game *g, LogKind kind, const char *fmt, ...);

/* society.c : climat, population, migrations, regime */
float       Soc_Habitability(const Game *g, int p);
float       Soc_ProvinceMilitancy(const Province *p);
float       Soc_ProvinceIncome(const Game *g, int p);
float       Soc_Income(const Game *g);
float       Soc_NationPop(const Game *g, int n);
int         Soc_NationProvinces(const Game *g, int n);
float       Soc_RegimeChangeCost(const Game *g, int regime);
const char *Soc_RegimeBlocked(const Game *g, int regime);
bool        Soc_ChangeRegime(Game *g, int regime);
void        Soc_UpdateResources(Game *g);
void        Soc_Turn(Game *g);

/* war.c : armees, diplomatie, IA */
float       War_ArmyMax(const Game *g, int n);
float       War_Quality(const Game *g, int n);
bool        War_Adjacent(const Game *g, int nationA, int nationB);
bool        War_ProvinceAttackable(const Game *g, int p);
float       War_AttackOdds(const Game *g, int p, float divisions);
bool        War_Attack(Game *g, int p, float divisions);
const char *War_DeclareBlocked(const Game *g, int n);
bool        War_Declare(Game *g, int n);
const char *War_PeaceBlocked(const Game *g, int n);
bool        War_ProposePeace(Game *g, int n);
const char *War_DiploBlocked(const Game *g, int n, int action);
bool        War_Diplomacy(Game *g, int n, int action);   /* 0 relations, 1 commerce */
const char *War_UltimatumBlocked(const Game *g, int p);
bool        War_Ultimatum(Game *g, int p);
float       War_AvailableDivisions(const Game *g);
bool        War_Recruit(Game *g);
bool        War_Disband(Game *g);
float       War_ArmyUpkeep(const Game *g);
void        War_Turn(Game *g);
void        War_MakePeace(Game *g, int n, bool log);
void        War_Capture(Game *g, int p, int newOwner);

#endif
