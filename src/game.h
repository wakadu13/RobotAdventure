/*
 * BOUCLIER 2050 - simulation (aucune dependance graphique)
 *
 * 2030-2060 : rechauffement climatique, penuries, migrations, guerre hybride
 * et conflits ouverts en Europe et en Mediterranee. La France defend ses
 * frontieres dans tous les milieux et champs (M2MC), gere sa population et
 * son economie, choisit son regime politique comme ses guerres.
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "map_data.h"

#define TOTAL_TURNS   120     /* 120 trimestres : 2030 -> 2060 */
#define START_YEAR    2030
#define MAX_THREATS   128
#define MAX_UNITS     640
#define MAX_LOG       160
#define LOG_LEN       170
#define BASE_CP       3       /* points de commandement par tour */
#define THREAT_SCALE  0.6f    /* intensite globale des effets des menaces */
#define HISTORY_LEN   (TOTAL_TURNS + 1)
#define SAVE_VERSION  3

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
typedef enum { I_ECOLO, I_SOCIAL, I_LIBERAL, I_CONSERV, I_SOUVER, I_AUTOR, I_COUNT } Ideology;
typedef enum { G_FOOD, G_WATER, G_ENERGY, G_MINERALS, G_GOODS, G_COUNT } Good;
typedef enum { SP_EDUCATION, SP_ADMIN, SP_SOCIAL, SP_COUNT } Spending;
typedef enum { U_INF, U_MECA, U_BLINDE, U_MARINE, U_TYPE_COUNT } UnitType;
typedef enum { DIFF_FACILE, DIFF_NORMAL, DIFF_DIFFICILE, DIFF_COUNT } Difficulty;

typedef enum {
    BL_TAXES, BL_TRADE, BL_TARIFF, BL_EU,                     /* recettes */
    BL_DEFENSE, BL_ARMY, BL_SPENDING, BL_NUKE, BL_IMPORTS,     /* depenses */
    BL_COUNT
} BudgetLine;

typedef enum {
    SC_SURVIE, SC_POPULATION, SC_PROSPERITE, SC_TERRITOIRE, SC_STABILITE,
    SC_RESILIENCE, SC_PUISSANCE, SC_REPUTATION, SC_PENALITES, SC_COUNT
} ScorePart;

typedef enum { VIC_NONE, VIC_SURVIE, VIC_TRANSITION, VIC_HEGEMONIE, VIC_UTOPIE, VIC_ORDRE, VIC_COUNT } VictoryType;

typedef enum { EV_INFO, EV_GOOD, EV_BAD, EV_CLIMATE, EV_EVENT, EV_WAR } LogKind;

typedef enum {
    GAME_RUNNING, GAME_WON,
    GAME_LOST_COHESION, GAME_LOST_SOUVERAINETE, GAME_LOST_BUDGET, GAME_LOST_CONQUEST, GAME_LOST_NUCLEAR
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
    const char *mechanic;  /* mecanique propre au regime */
} RegimeDef;

typedef struct { const char *name; const char *desc; int cooldown; float budget; } DecreeDef;

typedef struct {
    const char *name;
    unsigned char color[3];
    Regime regime;
    bool eu;
    float relation;        /* relation initiale avec la France */
    float aggressiveness;  /* propension a la guerre (IA) */
    bool nuclear;          /* puissance nucleaire */
} NationDef;

typedef struct {
    NationId nation;
    float pop;             /* millions d'habitants en 2030 */
    float growth;          /* croissance naturelle (%/tour) */
    float aridity;         /* exposition au rechauffement 0..1 */
    float food, water, energy;   /* production (1 unite = besoin d'1 million d'habitants) */
    float wealth;          /* recettes fiscales par million d'habitants (Md/tour) */
} ProvinceDef;

typedef struct {
    const char *name;
    const char *shortName;
    const char *desc;
    float atk, def;        /* valeurs d'attaque et de defense */
    float cost;            /* cout de recrutement (Md) */
    float upkeep;          /* entretien (Md/tour) */
    float goods;           /* biens industriels consommes par tour */
    int train;             /* tours de formation */
} UnitDef;

extern const char       *DOMAIN_NAMES[D_COUNT];
extern const char       *RESOURCE_NAMES[R_COUNT];
extern const char       *RESOURCE_DESC[R_COUNT];
extern const char       *STRATA_NAMES[S_COUNT];
extern const char       *IDEOLOGY_NAMES[I_COUNT];
extern const char       *IDEOLOGY_DESC[I_COUNT];
extern const float       IDEOLOGY_AFFINITY[RG_COUNT][I_COUNT];
extern const char       *GOOD_NAMES[G_COUNT];
extern const float       GOOD_BASE_PRICE[G_COUNT];
extern const char       *SPENDING_NAMES[SP_COUNT];
extern const char       *SPENDING_DESC[SP_COUNT];
extern const char       *DIFFICULTY_NAMES[DIFF_COUNT];
extern const char       *BUDGET_LINE_NAMES[BL_COUNT];
extern const char       *SCORE_PART_NAMES[SC_COUNT];
extern const char       *VICTORY_NAMES[VIC_COUNT];
extern const char       *VICTORY_DESC[VIC_COUNT];
extern const char       *NUKE_POSTURES[3];
extern const UnitDef     UNIT_TYPES[U_TYPE_COUNT];
extern const DecreeDef   DECREES[RG_COUNT][2];
extern const TheaterDef  THEATERS[TH_COUNT];
extern const ThreatDef   THREATS[];
extern const int         THREAT_TYPE_COUNT;
extern const FactionDef  FACTIONS[F_COUNT];
extern const ProjectDef  PROJECTS[P_COUNT];
extern const ActionDef   ACTIONS[A_COUNT];
extern const DilemmaDef  DILEMMAS[];
extern const int         DILEMMA_COUNT;
extern const DilemmaDef  DYN_DILEMMAS[4];
extern const RegimeDef   REGIMES[RG_COUNT];
extern const NationDef   NATIONS[N_COUNT];
extern const ProvinceDef PROVINCES[PROV_COUNT];
extern const float       PROVINCE_MINERALS[PROV_COUNT];
extern const float       PROVINCE_INDUSTRY[PROV_COUNT];
extern const unsigned char PROVINCE_TERRAIN[PROV_COUNT];   /* 0 plaine, 1 collines, 2 montagne */
extern const float       STRATA_SHARE[S_COUNT];
extern const char       *MIGRATION_DOCTRINES[3];
extern const char       *MIGRATION_DESC[3];
extern const float       MIGRATION_ACCEPT[3];
extern const char       *ALERT_LEVELS[3];
extern const char       *ALERT_DESC[3];

/* dilemmes dynamiques (hors tirage aleatoire) */
#define DLM_ALLY_CALL      100   /* un allie europeen est attaque */
#define DLM_PEACE_OFFER    101   /* un ennemi propose la paix */
#define DLM_COUP_LEGAL     102   /* les autoritaires gagnent les elections */
#define DLM_NUKE_BLACKMAIL 103   /* chantage nucleaire */

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
    float share[S_COUNT];  /* part de chaque strate */
    float mil[S_COUNT];    /* militance par strate (0..10) */
    float con[S_COUNT];    /* conscience politique par strate (0..10) */
    float literacy;        /* alphabetisation (0..1) */
    float ideo[I_COUNT];   /* soutien aux courants politiques */
    float foreign;         /* part de population non integree (0..1) */
    float hab;             /* habitabilite (0..100) */
    float integration;     /* integration d'un territoire conquis (0..100) */
    float damage;          /* devastation par la guerre (0..1) */
    float fallout;         /* contamination nucleaire durable (0..1) */
    float prosperity;      /* multiplicateur de richesse */
    float industry;        /* usines */
    int factoryBuild;      /* tours restants de construction d'usine (0 = aucune) */
    int revolt;            /* tours de revolte en cours */
    float emig, immig;     /* flux du dernier tour (millions) */
} Province;

typedef struct {
    bool active;
    unsigned char type;
    signed char owner;
    short prov;
    short target;          /* province visee (-1 = aucun ordre) */
    float str;             /* effectifs (0..1) */
    float org;             /* organisation (0..1) */
    int training;          /* tours de formation restants */
} Unit;

typedef struct {
    float supply[G_COUNT], demand[G_COUNT];
    float imports[G_COUNT], exports[G_COUNT];
    float sat[G_COUNT];    /* (production + importations) / besoins */
} NationEconomy;

typedef struct {
    bool alive;
    Regime regime;
    float relation;        /* -100..100 envers la France */
    float army;            /* puissance de l'armee (divisions equivalentes, derivee des unites) */
    bool atWar;            /* en guerre contre la France */
    int warTurns;
    int truce;
    int warTarget;         /* guerre IA contre IA (-1 aucune) */
    int warTargetTurns;
    bool eu;
    bool trade;
    bool failed;
    bool deterred;         /* dissuadee de toucher au sol francais */
    bool warned;           /* ultime avertissement nucleaire deja adresse */
    int startProvinces;
    bool actedThisTurn;
} Nation;

typedef struct {
    unsigned rng;
    int turn;
    int difficulty;
    float temp;
    float res[R_COUNT];
    float lastDelta[R_COUNT];

    /* defense M2MC et guerre hybride */
    int capacity[D_COUNT];
    int pendingCap[D_COUNT];
    int pendingTurns[D_COUNT];
    int posture[TH_COUNT];
    float tension[TH_COUNT];
    Threat threats[MAX_THREATS];
    Faction fac[F_COUNT];
    Project proj[P_COUNT];

    /* monde */
    Nation nat[N_COUNT];
    Province prov[PROV_COUNT];
    Unit units[MAX_UNITS];
    float aggressionMemory;

    /* economie */
    NationEconomy eco[N_COUNT];
    float price[G_COUNT];
    float taxRate[S_COUNT];
    float spending[SP_COUNT];
    float tariff;
    float budget[BL_COUNT];  /* dernier bilan budgetaire */
    float migrantsIn, migrantsRefused, migrantsOut;

    /* politique */
    int government;          /* courant au pouvoir (-1 aucun) */
    int nextElection;
    int regimeCooldown;
    int decreeCooldown[2];
    int sobrietyTurns, mobilizationTurns, boostTurns, propagandaTurns;
    int migrationDoctrine;
    int alertLevel;
    int commandPoints;

    /* dissuasion */
    int nukePosture;         /* 0 minimale, 1 credible, 2 renforcee */
    float credibility;
    int nukesUsed;
    bool umbrella;           /* parapluie nucleaire europeen */

    bool aiDefense;
    float climateMitigation;
    float importBonus;

    int pendingDilemma;
    int dilemmaParam;
    bool dilemmaUsed[32];

    /* rapport de fin de tour */
    bool reportReady;
    int reportLogStart;
    float reportRes[R_COUNT];
    float reportPop;
    int reportProv, reportUnits, reportScore;

    float histPop[HISTORY_LEN];
    float histCohesion[HISTORY_LEN];
    float histTemp[HISTORY_LEN];
    float histScore[HISTORY_LEN];

    LogEntry log[MAX_LOG];
    int logCount;

    GameState state;
    int statNeutralized;
    int statSuffered;
    int statDisasters;
    int statConquered;
    int statLost;
    int statBattlesWon;
    int statBattlesLost;
} Game;

/* game.c : coeur, guerre hybride, projets, dilemmes, sauvegarde, score */
void        Game_Init(Game *g, unsigned seed, int difficulty);
void        Game_EndTurn(Game *g);
bool        Game_Save(const Game *g, const char *path);
bool        Game_Load(Game *g, const char *path);
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
float       Game_DefenseUpkeep(const Game *g);
int         Game_ThreatCount(const Game *g, int theater);
int         Game_ActiveThreats(const Game *g);
void        Game_DateString(int turn, char *buf, int len);
int         Game_Score(const Game *g, int parts[SC_COUNT]);
VictoryType Game_Victory(const Game *g);
void        Game_Log(Game *g, LogKind kind, const char *fmt, ...);

/* economy.c : marche mondial, impots, depenses, usines */
void        Eco_Turn(Game *g);
void        Eco_Budget(const Game *g, float lines[BL_COUNT]);   /* bilan previsionnel */
float       Eco_Net(const Game *g);
float       Eco_ProvinceTax(const Game *g, int p);
float       Eco_SpendingCost(const Game *g, int sp);
float       Eco_NeedSat(const Game *g, int nation, int strata);
float       Eco_FactoryCost(const Game *g);
const char *Eco_FactoryBlocked(const Game *g, int p);
bool        Eco_BuildFactory(Game *g, int p);
void        Eco_SetTax(Game *g, int strata, float rate);
void        Eco_SetSpending(Game *g, int sp, float level);
void        Eco_SetTariff(Game *g, float t);

/* society.c : climat, population, migrations, regime */
float       Soc_Habitability(const Game *g, int p);
float       Soc_ProvinceMilitancy(const Province *p);
float       Soc_ProvinceConsciousness(const Province *p);
float       Soc_RevoltThreshold(const Province *p);
float       Soc_NationPop(const Game *g, int n);
int         Soc_NationProvinces(const Game *g, int n);
float       Soc_RegimeChangeCost(const Game *g, int regime);
const char *Soc_RegimeBlocked(const Game *g, int regime);
bool        Soc_ChangeRegime(Game *g, int regime);
void        Soc_Turn(Game *g);

/* politics.c : ideologies, elections, decrets de regime */
void        Pol_InitProvince(Game *g, int p);
void        Pol_Turn(Game *g);
void        Pol_NationalIdeology(const Game *g, float out[I_COUNT]);
float       Pol_RegimeSupport(const Game *g);
const char *Pol_DecreeBlocked(const Game *g, int k);
bool        Pol_Decree(Game *g, int k);
const char *Pol_GovernmentEffect(int ideology);

/* war.c : armees, batailles, diplomatie, IA */
bool        War_AtWar(const Game *g, int a, int b);
float       War_ArmyMax(const Game *g, int n);
int         War_UnitCount(const Game *g, int n);
float       War_Quality(const Game *g, int n);
bool        War_Adjacent(const Game *g, int nationA, int nationB);
bool        War_ProvinceAttackable(const Game *g, int p);
float       War_RecruitCost(const Game *g, int type);
const char *War_RecruitBlocked(const Game *g, int type, int p);
bool        War_Recruit(Game *g, int type, int p);
bool        War_Disband(Game *g, int unit);
const char *War_OrderBlocked(const Game *g, int unit, int target);
bool        War_Order(Game *g, int unit, int target);
float       War_BattleOdds(const Game *g, int p, int attacker);
float       War_ArmyUpkeep(const Game *g);
const char *War_DeclareBlocked(const Game *g, int n);
bool        War_Declare(Game *g, int n);
const char *War_PeaceBlocked(const Game *g, int n);
bool        War_ProposePeace(Game *g, int n);
const char *War_DiploBlocked(const Game *g, int n, int action);
bool        War_Diplomacy(Game *g, int n, int action);   /* 0 relations, 1 commerce */
const char *War_UltimatumBlocked(const Game *g, int p);
bool        War_Ultimatum(Game *g, int p);
void        War_InitUnits(Game *g);
void        War_Turn(Game *g);
void        War_MakePeace(Game *g, int n, bool log);
void        War_Capture(Game *g, int p, int newOwner);

/* nuclear.c : dissuasion */
float       Nuke_Upkeep(const Game *g);
void        Nuke_SetPosture(Game *g, int posture);
const char *Nuke_UmbrellaBlocked(const Game *g);
void        Nuke_ToggleUmbrella(Game *g);
const char *Nuke_StrikeBlocked(const Game *g, int p);
bool        Nuke_Strike(Game *g, int p);
const char *Nuke_WarningBlocked(const Game *g, int n);
bool        Nuke_Warning(Game *g, int n);
bool        Nuke_Deters(Game *g, int n);
void        Nuke_Turn(Game *g);

#endif
