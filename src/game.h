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
#define MAX_THREATS   160
#define MAX_UNITS     900
#define MAX_GENERALS  64
#define MAX_LOG       200
#define LOG_LEN       170
#define BASE_CP       3       /* points de commandement par tour */
#define THREAT_SCALE  0.6f    /* intensite globale des effets des menaces */
#define HISTORY_LEN   (TOTAL_TURNS + 1)
#define SAVE_VERSION  6
#define GROUP_COUNT   4       /* groupes d'armees francais */
#define RATING_COUNT  7
#define STACK_LAND    10      /* divisions terrestres par province et par nation */
#define STACK_AIR     6
#define STACK_SEA     8
#define TECH_MAX      5

typedef enum { D_TERRE, D_MER, D_AIR, D_ESPACE, D_CYBER, D_INFO, D_EM, D_COUNT } Domain;

typedef enum {
    R_BUDGET, R_ENERGIE, R_EAU, R_ALIM, R_COHESION, R_INFLUENCE, R_SOUVERAINETE, R_COUNT
} Resource;

typedef enum { F_BOREALE, F_SABLES, F_HYDRA, F_NEBULA, F_COUNT } FactionId;

typedef enum {
    TH_NORD, TH_EST, TH_ALPES, TH_PYRENEES,
    TH_ATLANTIQUE, TH_MEDITERRANEE, TH_OUTREMER,
    TH_CIEL, TH_ORBITE, TH_CYBER, TH_INFO, TH_SPECTRE,
    TH_EXTERIEUR,
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
typedef enum { RL_CATHOLIQUES, RL_PROTESTANTS, RL_ORTHODOXES, RL_MUSULMANS, RL_SANS } Religion;
typedef enum { G_FOOD, G_WATER, G_ENERGY, G_MINERALS, G_GOODS, G_COUNT } Good;
typedef enum { SP_EDUCATION, SP_ADMIN, SP_SOCIAL, SP_COUNT } Spending;
typedef enum { CP_ASSIMILATION, CP_PLURALISME, CP_REPRESSION, CP_COUNT } CulturePolicy;
typedef enum { DIFF_FACILE, DIFF_NORMAL, DIFF_DIFFICILE, DIFF_COUNT } Difficulty;
typedef enum { DOM_LAND, DOM_AIR, DOM_SEA } UnitDomain;
typedef enum { SR_TERRES_RARES, SR_URANIUM, SR_PUCES, SR_COUNT } StratRes;
typedef enum { GM_NONE, GM_HOLD, GM_ATTACK, GM_COUNT } GroupMode;
typedef enum { NM_PATROL, NM_STRIKE, NM_RAID, NM_ESCORT, NM_COUNT } NavalMission;
typedef enum { AM_SUPPORT, AM_SUPERIORITY, AM_BOMBING, AM_COUNT } AirMission;
typedef enum { MIN_DEFENSE, MIN_ECONOMIE, MIN_INTERIEUR, MIN_ETRANGERES, MIN_COUNT } MinisterPost;
typedef enum { LB_INDUSTRIE, LB_ECOLOGISTES, LB_MILITAIRES, LB_SYNDICATS, LB_COUNT } Lobby;
typedef struct { unsigned char name, skill, lobby; } Minister;
typedef enum {
    OP_RESEAU, OP_VOL_TECH, OP_LOGISTIQUE, OP_INGERENCE, OP_DESTAB, OP_RECON, OP_BROUILLAGE, OP_ASAT, OP_COUNT
} OpType;

typedef enum {
    U_INF, U_MECA, U_BLINDE, U_MARINE, U_MONTAGNE, U_ARTILLERIE, U_DRONES, U_ROBOTS, U_FS, U_DCA, U_HYPER,
    U_CHASSE, U_MALE,
    U_FREGATE, U_SNA, U_PA, U_USV,
    U_TYPE_COUNT
} UnitType;

typedef enum { T_INF, T_BLINDES, T_AIR, T_MARINE, T_FEUX, T_GE, T_LOG, T_NUC, T_COUNT } TechArea;

typedef enum {
    GT_OFFENSIF, GT_DEFENSIF, GT_LOGISTICIEN, GT_BLINDES, GT_MONTAGNE, GT_COUNT
} GeneralTrait;

typedef enum {
    BL_TAXES, BL_TRADE, BL_TARIFF, BL_EU, BL_REPARATIONS,                          /* recettes */
    BL_DEFENSE, BL_ARMY, BL_SPENDING, BL_RESEARCH, BL_NUKE, BL_IMPORTS, BL_REP_PAID, /* depenses */
    BL_INTEREST, BL_COASTAL,
    BL_COUNT
} BudgetLine;
#define BL_FIRST_EXPENSE BL_DEFENSE

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
    float taxEff;
    float mobilization;
    float repression;
    float warWeariness;
    float aggressionCost;
    float influence;
    float assimilation;
    float growth;
    float projectCost;
    float quality;
    float sat[S_COUNT];
    const char *mechanic;
} RegimeDef;

typedef struct { const char *name; const char *desc; int cooldown; float budget; } DecreeDef;

typedef struct {
    const char *name;
    unsigned char color[3];
    Regime regime;
    bool eu;
    float relation;        /* relation initiale avec la France */
    float aggressiveness;  /* propension a la guerre (IA) */
    bool nuclear;          /* puissance nucleaire en 2030 */
    unsigned char cultures[3];   /* cultures acceptees (255 = aucune) */
    const char *general[2];      /* noms de generaux */
} NationDef;

typedef struct {
    NationId nation;
    float pop;
    float growth;
    float aridity;
    float food, water, energy;
    float wealth;
} ProvinceDef;

typedef struct {
    const char *name;
    const char *shortName;
    const char *desc;
    UnitDomain domain;
    float atk, def;        /* combat terrestre (ou appui pour l'air) */
    float aa;              /* defense antiaerienne */
    float naval;           /* combat naval */
    float cost, upkeep, goods;
    int train;
    int speed;             /* provinces par tour */
    int range;             /* portee d'appui (artillerie, missiles, aviation) */
    TechArea tech;
    int techLevel;         /* niveau requis */
    float capWeight;       /* poids dans le plafond de mobilisation */
} UnitDef;

typedef struct { const char *name; const char *desc; } TechDef;

typedef struct {
    const char *name;
    const char *desc;
    Domain domain;         /* milieu de la capacite requise */
    int capacity;          /* niveau de capacite requis */
    float budget, influence;
    bool needWar;          /* reservee aux nations en guerre contre nous (ou tres hostiles) */
    float risk;            /* risque d'attribution en cas de succes */
    int duration;          /* tours d'effet (0 : immediat) */
} OpDef;

typedef struct { const char *name; const char *unit; const char *desc; float price; } StratDef;
typedef struct { short prov; unsigned char res; float amount; } DepositDef;

extern const char       *DOMAIN_NAMES[D_COUNT];
extern const char       *RESOURCE_NAMES[R_COUNT];
extern const char       *RESOURCE_DESC[R_COUNT];
extern const char       *STRATA_NAMES[S_COUNT];
extern const char       *IDEOLOGY_NAMES[I_COUNT];
extern const char       *IDEOLOGY_DESC[I_COUNT];
extern const float       IDEOLOGY_AFFINITY[RG_COUNT][I_COUNT];
extern const char       *RELIGION_NAMES[REL_COUNT];
extern const char       *CULTURE_POLICY_NAMES[CP_COUNT];
extern const char       *CULTURE_POLICY_DESC[CP_COUNT];
extern const int         CULTURE_HOMELAND[CULT_COUNT];
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
extern const TechDef     TECHS[T_COUNT];
extern const OpDef       OPS[OP_COUNT];
extern const StratDef    STRATS[SR_COUNT];
extern const DepositDef  DEPOSITS[];
extern const int         DEPOSIT_COUNT;
extern const unsigned char UNIT_STRAT[U_TYPE_COUNT][SR_COUNT];
extern const float       PROVINCE_SEA_EXPOSURE[PROV_COUNT];
extern const char       *GROUP_MODE_NAMES[GM_COUNT];
extern const char       *NAVAL_MISSION_NAMES[NM_COUNT];
extern const char       *NAVAL_MISSION_DESC[NM_COUNT];
extern const char       *AIR_MISSION_NAMES[AM_COUNT];
extern const char       *AIR_MISSION_DESC[AM_COUNT];
extern const char       *RATING_NAMES[RATING_COUNT];
extern const float       RATING_RATE[RATING_COUNT];
extern const char       *GENERAL_TRAITS[GT_COUNT];
extern const char       *FRENCH_GENERALS[20];
extern const DecreeDef   DECREES[RG_COUNT][2];
extern const TheaterDef  THEATERS[TH_COUNT];
extern const ThreatDef   THREATS[];
extern const int         THREAT_TYPE_COUNT;
extern const FactionDef  FACTIONS[F_COUNT];
extern const ProjectDef  PROJECTS[P_COUNT];
extern const ActionDef   ACTIONS[A_COUNT];
extern const DilemmaDef  DILEMMAS[];
extern const int         DILEMMA_COUNT;
extern const DilemmaDef  DYN_DILEMMAS[9];
extern const char       *SCENARIO_NAMES[2];
extern const char       *VET_RANKS[4];
extern const char       *SCENARIO_DESC[2];
extern const RegimeDef   REGIMES[RG_COUNT];
extern const NationDef   NATIONS[N_COUNT];
extern const ProvinceDef PROVINCES[PROV_COUNT];
extern const float       PROVINCE_MINERALS[PROV_COUNT];
extern const float       PROVINCE_INDUSTRY[PROV_COUNT];
extern const unsigned char PROVINCE_TERRAIN[PROV_COUNT];
extern const float       STRATA_SHARE[S_COUNT];
extern const char       *MIGRATION_DOCTRINES[3];
extern const char       *MIGRATION_DESC[3];
extern const float       MIGRATION_ACCEPT[3];
extern const char       *ALERT_LEVELS[3];
extern const char       *ALERT_DESC[3];

#define DLM_ALLY_CALL      100
#define DLM_PEACE_OFFER    101
#define DLM_COUP_LEGAL     102
#define DLM_NUKE_BLACKMAIL 103
#define DLM_BAILOUT        104
#define DLM_UN_VETO        105
#define DLM_AI_ALLIANCE    106   /* une IA propose une alliance */
#define DLM_AI_TRADE       107   /* une IA propose un accord commercial */
#define DLM_PEACE_BLACKMAIL 108  /* chantage nucleaire en temps de paix */
#define DLM_EVENT          200   /* evenements contextuels : DLM_EVENT + indice */
#define EVENT_MAX          96

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
    short prov;            /* province visee (territoires exterieurs), -1 sinon */
} Threat;

typedef struct {
    float hostility;
    float capability;
    int intelTurns;
    bool actedThisTurn;
} Faction;

typedef struct { int state; int turnsLeft; } Project;

typedef struct {
    char text[LOG_LEN];    /* texte en francais */
    char fmt[LOG_LEN];     /* format francais : l'interface le traduit a l'affichage */
    char args[LOG_LEN];    /* arguments mis en forme, separes par \x1f */
    int turn;
    LogKind kind;
} LogEntry;

typedef struct {
    int owner;             /* proprietaire legal */
    int ctrl;              /* controle militaire (occupant) */
    int core;              /* nation d'origine */
    float pop;
    float share[S_COUNT];
    float mil[S_COUNT];
    float con[S_COUNT];
    float ideo[S_COUNT][I_COUNT];
    float culture[CULT_COUNT];
    float religion[REL_COUNT];
    float literacy;
    float hab;
    float integration;
    float damage;
    float fallout;
    float prosperity;
    float industry;
    float unemployment;
    float jobs;            /* emplois / population active */
    int factoryBuild;
    int fabBuild;          /* fonderie de semi-conducteurs en construction */
    float fabs;            /* production de semi-conducteurs */
    bool mineClosed;       /* gisement inexploite */
    float flooded;         /* part des terres submergees */
    short depot;           /* depot logistique : nation + 1 (0 : aucun) */
    unsigned char fort;    /* niveau de fortification (0-3) */
    unsigned char fortBuild;
    int revolt;
    float emig, immig;
} Province;

typedef struct {
    bool active;
    unsigned char type;
    signed char owner;
    short prov;
    short target;          /* destination (-1 = aucun ordre) */
    float str;
    float org;
    int training;
    unsigned char group;   /* groupe d'armees francais (0 : aucun) */
    unsigned char mission; /* mission navale */
    unsigned char xp;      /* veterance 0-100 */
} Unit;

typedef struct {
    bool active;
    signed char owner;
    short prov;            /* province de commandement (-1 = reserve) */
    unsigned char skill;   /* 1..5 */
    unsigned char trait;
    float xp;
    char name[28];
} General;

typedef struct {
    float supply[G_COUNT], demand[G_COUNT];
    float imports[G_COUNT], exports[G_COUNT];
    float stock[G_COUNT];
    float sat[G_COUNT];
} NationEconomy;

typedef struct {
    bool alive;
    Regime regime;
    float relation;        /* -100..100 envers la France */
    float army;            /* puissance de l'armee (derivee des unites) */
    bool eu;
    bool trade;
    bool failed;
    bool deterred;
    bool warned;
    bool embargo;          /* embargo francais */
    bool embargoFrance;    /* embargo contre la France */
    int startProvinces;
    bool actedThisTurn;
    int government;        /* courant au pouvoir (-1 aucun) */
    int nextElection;
    int nukeStatus;        /* 0 aucun, 1 programme, 2 puissance nucleaire */
    float nukeProgress;
    float treasury;
    float foodBonus, waterBonus;
    float blockade;        /* part du littoral sous blocus */
    float reparations;     /* versees a la France par tour (negatif : la France paie) */
    int reparationTurns;
    unsigned char tech[T_COUNT];
    float strat[SR_COUNT];     /* stocks de ressources strategiques */
    short outage;              /* reseau electrique paralyse (tours) */
    short jammed;              /* aviation et drones brouilles */
    short recon;               /* sous surveillance satellitaire francaise */
    short asat;                /* satellites detruits */
    bool opsThisTurn;
    int negoTurn, negoRounds;  /* negociations de paix en cours */
    short aiTarget, aiThreat;  /* objectif offensif et point menace (IA) */
    bool nato;                 /* membre de l'OTAN */
    short sanctions;           /* sanctions internationales (tours) */
    short peaceCooldown;       /* delai avant une nouvelle offre de paix */
    short peaceRefusals;       /* offres refusees par la France */
    float raid;                /* pertes commerciales dues a la guerre au commerce */
    short peaceAsk;            /* commandant humain : demande de paix a la nation (indice + 1) */
    float manpower;            /* reserve de main-d'oeuvre (en points de mobilisation) */
    short offerCooldown;       /* delai avant une nouvelle proposition a la France */
    float inflation;           /* inflation annuelle (%) des nations de l'IA */
} Nation;

typedef struct {
    bool active;
    int other;
    bool proposedByAi;
    bool cede[PROV_COUNT];  /* provinces cedees au vainqueur */
    int winner;
    float reparations;
    int reparationTurns;
    bool countered;            /* contre-proposition de l'IA */
} Treaty;

typedef struct {
    unsigned rng;
    int turn;
    int difficulty;
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
    Unit units[MAX_UNITS];
    General generals[MAX_GENERALS];
    float aggressionMemory;
    float seaLevel;            /* elevation du niveau de la mer (m) */
    int stratShock[SR_COUNT];  /* crise d'approvisionnement mondiale (tours) */

    /* armee */
    struct { int mode; int target; } groups[GROUP_COUNT];
    bool visible[PROV_COUNT];                 /* brouillard de guerre (vue francaise) */
    unsigned char supply[N_COUNT][PROV_COUNT]; /* ravitaillement 0-100 */
    bool aggressor[N_COUNT][N_COUNT];         /* a a attaque b */
    float route[N_COUNT];                     /* qualite de la route commerciale vers la France (0-1) */
    unsigned char routeKind[N_COUNT];         /* 0 coupee, 1 terrestre, 2 maritime */

    /* organisations internationales et dette */
    float natoCohesion;
    int unCooldown;
    float debt;
    float inflation;           /* % par an */
    float priceLevel;          /* indice des prix (2030 = 1) */
    int bailouts, austerityTurns;

    /* relations internationales */
    bool war[N_COUNT][N_COUNT];
    short warSince[N_COUNT][N_COUNT];
    short truce[N_COUNT][N_COUNT];
    bool ally[N_COUNT][N_COUNT];
    float airel[N_COUNT][N_COUNT];     /* relations entre nations */
    short battles[N_COUNT][N_COUNT];   /* batailles gagnees par la ligne contre la colonne */
    Treaty treaty;

    /* economie */
    NationEconomy eco[N_COUNT];
    float price[G_COUNT];
    float taxRate[S_COUNT];
    float spending[SP_COUNT];
    float tariff;
    float budget[BL_COUNT];
    float migrantsIn, migrantsRefused, migrantsOut;

    /* recherche militaire */
    int researchArea;
    float researchProgress;
    float researchBudget;

    /* politique */
    int regimeCooldown;
    int decreeCooldown[2];
    int sobrietyTurns, mobilizationTurns, boostTurns, propagandaTurns;
    int migrationDoctrine;
    int culturePolicy;
    int alertLevel;
    int commandPoints;

    /* dissuasion */
    int nukePosture;
    float credibility;
    int nukesUsed;
    int tacticalUsed;
    bool umbrella;

    bool aiDefense;

    /* hot-seat : nations dirigees par des joueurs ; la France joue d'abord, puis chaque commandant */
    bool human[N_COUNT];
    int activePlayer;
    int scenario;              /* 0 : janvier 2030 ; 1 : crise de 2040 */
    /* gouvernement, lobbies et opinion */
    Minister minister[MIN_COUNT];
    Minister candidates[MIN_COUNT][3];
    float lobby[LB_COUNT];
    float approval;
    int startTurn;
    float climateMitigation;
    float importBonus;

    int pendingDilemma;
    int dilemmaParam;
    int dilemmaParam2;
    bool dilemmaUsed[32];
    bool eventUsed[EVENT_MAX];

    bool reportReady;
    int reportLogStart;
    float reportRes[R_COUNT];
    float reportPop;
    int reportProv, reportUnits, reportScore;

    float histPop[HISTORY_LEN];
    float histCohesion[HISTORY_LEN];
    float histTemp[HISTORY_LEN];
    float histScore[HISTORY_LEN];
    float histNatPop[N_COUNT][HISTORY_LEN];
    float histNatArmy[N_COUNT][HISTORY_LEN];
    float histNatRel[N_COUNT][HISTORY_LEN];

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

/* game.c */
void        Game_Init(Game *g, unsigned seed, int difficulty);
bool        Game_ApplyScenario(Game *g, int scenario);
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

/* cabinet.c : ministres, lobbies et opinion */
extern const char *MINISTER_POSTS[MIN_COUNT];
extern const char *MINISTER_EFFECTS[MIN_COUNT];
extern const char *LOBBY_NAMES[LB_COUNT];
extern const char *LOBBY_DESC[LB_COUNT];
void        Cab_Init(Game *g);
void        Cab_Turn(Game *g);
const char *Cab_Name(const Minister *m);
int         Cab_Skill(const Game *g, int post);
const char *Cab_AppointBlocked(const Game *g, int post, int cand);
bool        Cab_Appoint(Game *g, int post, int cand);
float       Cab_LobbyTarget(const Game *g, int l);
float       Cab_ApprovalTarget(const Game *g);

/* events.c : evenements contextuels decrits par des donnees */
extern const int  EVENT_COUNT;
const DilemmaDef *Ev_Dilemma(int k);
void        Ev_Apply(Game *g, int k, int choice);
bool        Ev_Trigger(Game *g);
bool        Ev_Eligible(const Game *g, int k);

/* economy.c */
void        Eco_Turn(Game *g);
void        Eco_Budget(const Game *g, float lines[BL_COUNT]);
float       Eco_Net(const Game *g);
float       Eco_ProvinceTax(const Game *g, int p);
float       Eco_SpendingCost(const Game *g, int sp);
float       Eco_NeedSat(const Game *g, int nation, int strata);
float       Eco_NationUnemployment(const Game *g, int nation);
float       Eco_FactoryCost(const Game *g);
const char *Eco_FactoryBlocked(const Game *g, int p);
bool        Eco_BuildFactory(Game *g, int p);
void        Eco_SetTax(Game *g, int strata, float rate);
void        Eco_SetSpending(Game *g, int sp, float level);
void        Eco_SetTariff(Game *g, float t);
const char *Eco_EmbargoBlocked(const Game *g, int n);
bool        Eco_ToggleEmbargo(Game *g, int n);
bool        Eco_TradeAllowed(const Game *g, int n);
int         Eco_Rating(const Game *g);
float       Eco_InterestRate(const Game *g);   /* taux annuel */
float       Eco_DebtRatio(const Game *g);
void        Eco_Inflation(Game *g, float net);
const char *Eco_BorrowBlocked(const Game *g);
bool        Eco_Borrow(Game *g);
const char *Eco_RepayBlocked(const Game *g);
bool        Eco_Repay(Game *g);
void        Eco_ResolveBailout(Game *g, bool accept);

/* society.c */
float       Soc_Habitability(const Game *g, int p);
float       Soc_ProvinceMilitancy(const Province *p);
float       Soc_ProvinceConsciousness(const Province *p);
float       Soc_RevoltThreshold(const Province *p);
float       Soc_NationPop(const Game *g, int n);
int         Soc_NationProvinces(const Game *g, int n);
bool        Soc_Accepted(const Game *g, int n, int culture);
float       Soc_NonAccepted(const Game *g, int p);
int         Soc_MainCulture(const Province *p);
int         Soc_MainReligion(const Province *p);
float       Soc_RegimeChangeCost(const Game *g, int regime);
const char *Soc_RegimeBlocked(const Game *g, int regime);
bool        Soc_ChangeRegime(Game *g, int regime);
const char *Soc_CulturePolicyBlocked(const Game *g, int policy);
void        Soc_SetCulturePolicy(Game *g, int policy);
void        Soc_Turn(Game *g);

/* politics.c */
void        Pol_InitProvince(Game *g, int p);
void        Pol_Turn(Game *g);
void        Pol_NationIdeology(const Game *g, int n, float out[I_COUNT]);
void        Pol_NationalIdeology(const Game *g, float out[I_COUNT]);
float       Pol_StrataSupport(const Game *g, int p, int strata);
float       Pol_RegimeSupport(const Game *g);
float       Pol_NationSupport(const Game *g, int n);
const char *Pol_DecreeBlocked(const Game *g, int k);
bool        Pol_Decree(Game *g, int k);
const char *Pol_GovernmentEffect(int ideology);

/* war.c : armees, batailles, guerres, alliances, traites */
bool        War_AtWar(const Game *g, int a, int b);
bool        War_AtWarAny(const Game *g, int n);
int         War_Turns(const Game *g, int a, int b);
float       War_Score(const Game *g, int a, int b);
float       War_ArmyMax(const Game *g, int n);
float       War_UnitLoad(const Game *g, int n);
int         War_UnitCount(const Game *g, int n);
float       War_Quality(const Game *g, int n);
float       War_ManpowerMax(const Game *g, int n);
float       War_ManpowerGain(const Game *g, int n);
float       War_VetBonus(const Unit *u);
int         War_VetRank(const Unit *u);
#define     MEN_PER_POINT 15000   /* hommes par point de mobilisation (affichage) */
bool        War_Adjacent(const Game *g, int nationA, int nationB);
bool        War_Friendly(const Game *g, int nation, int p);
bool        War_ProvinceAttackable(const Game *g, int p);
int         War_StackCount(const Game *g, int n, int p, UnitDomain dom);
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
const char *War_DiploBlocked(const Game *g, int n, int action);
bool        War_Diplomacy(Game *g, int n, int action);   /* 0 relations, 1 commerce, 2 alliance */
const char *War_UltimatumBlocked(const Game *g, int p);
bool        War_Ultimatum(Game *g, int p);
void        War_StartWar(Game *g, int a, int b, bool callAllies);
void        War_InitUnits(Game *g);
void        War_Turn(Game *g);
void        War_Occupy(Game *g, int p, int ctrl);
void        War_Annex(Game *g, int p, int newOwner);
/* traites */
void        War_TreatyDraft(Game *g, int other);
float       War_TreatyCost(const Game *g, const Treaty *t);
const char *War_TreatyBlocked(const Game *g, const Treaty *t);
bool        War_ProposeTreaty(Game *g);
int         War_Negotiate(Game *g);      /* 0 refus, 1 signe, 2 contre-proposition */
float       War_Exhaustion(const Game *g, int n);
void        War_SignTreaty(Game *g, const Treaty *t);
void        War_WhitePeace(Game *g, int a, int b, bool log);

/* war.c : ravitaillement, brouillard, fortifications, groupes, missions navales */
float       War_SupplyLevel(const Game *g, int n, int p);
bool        War_Visible(const Game *g, int p);
void        War_UpdateIntel(Game *g);
void        War_UpdateSupply(Game *g);
float       War_FortCost(const Game *g, int p);
const char *War_FortBlocked(const Game *g, int p);
bool        War_Fortify(Game *g, int p);
const char *War_DepotBlocked(const Game *g, int p);
bool        War_BuildDepot(Game *g, int p);
void        War_SetGroup(Game *g, int unit, int group);
void        War_SetGroupOrder(Game *g, int group, int mode, int target);
const char *War_MissionBlocked(const Game *g, int unit, int mission);
bool        War_SetMission(Game *g, int unit, int mission);
bool        War_PortFlooded(const Game *g, int p);
void        War_UpdateTradeRoutes(Game *g);

/* war.c : commandants (nations dirigees par d'autres joueurs en hot-seat) */
int         War_NextPlayer(const Game *g, int after);   /* -1 : plus personne apres */
void        War_UpdateIntelFor(Game *g, int n);
float       Cmdr_RecruitCost(int type);
const char *Cmdr_RecruitBlocked(const Game *g, int n, int type, int p);
bool        Cmdr_Recruit(Game *g, int n, int type, int p);
const char *Cmdr_OrderBlocked(const Game *g, int n, int unit, int target);
bool        Cmdr_Order(Game *g, int n, int unit, int target);
bool        Cmdr_Disband(Game *g, int n, int unit);
const char *Cmdr_DeclareBlocked(const Game *g, int n, int t);
bool        Cmdr_Declare(Game *g, int n, int t);
const char *Cmdr_PeaceBlocked(const Game *g, int n, int t);
bool        Cmdr_Peace(Game *g, int n, int t);
const char *Cmdr_AllyBlocked(const Game *g, int n, int t);
bool        Cmdr_Ally(Game *g, int n, int t);

/* intl.c : ONU, OTAN, sanctions */
void        Intl_Init(Game *g);
void        Intl_Turn(Game *g);
void        Intl_OnWar(Game *g, int a, int b);
const char *Intl_ResolutionBlocked(const Game *g, int n);
bool        Intl_ProposeResolution(Game *g, int n);
const char *Intl_NatoBlocked(const Game *g);
void        Intl_ToggleNato(Game *g);
void        Intl_ResolveVeto(Game *g, bool veto);

/* military.c : generaux et technologies */
float       Mil_TechCost(const Game *g, int area);
float       Mil_ResearchPoints(const Game *g);
void        Mil_SetResearch(Game *g, int area);
void        Mil_SetResearchBudget(Game *g, float md);
bool        Mil_UnitUnlocked(const Game *g, int n, int type);
float       Mil_TechBonus(const Game *g, int n, int type);
int         Mil_GeneralAt(const Game *g, int n, int p);
float       Mil_GeneralBonus(const Game *g, int n, int p, int type, bool attack);
const char *Mil_PromoteBlocked(const Game *g);
bool        Mil_Promote(Game *g);
const char *Mil_AssignBlocked(const Game *g, int gen, int p);
bool        Mil_Assign(Game *g, int gen, int p);
void        Mil_BattleXp(Game *g, int n, int p, bool won);
void        Mil_Init(Game *g);
void        Mil_Turn(Game *g);

/* ops.c : operations cyber, informationnelles, spatiales et electromagnetiques */
float       Ops_Chance(const Game *g, int n, int op);
const char *Ops_Blocked(const Game *g, int n, int op);
bool        Ops_Run(Game *g, int n, int op);
void        Ops_Turn(Game *g);

/* resources.c : ressources strategiques */
float       Strat_Deposit(int p, int r);
float       Strat_Production(const Game *g, int n, int r);
float       Strat_Imports(const Game *g, int n, int r);
float       Strat_Use(const Game *g, int n, int r);
float       Strat_Sat(const Game *g, int n, int r);
float       Strat_Price(const Game *g, int r);
const char *Strat_BuyBlocked(const Game *g, int r);
bool        Strat_Buy(Game *g, int r);
const char *Strat_FabBlocked(const Game *g, int p);
bool        Strat_BuildFab(Game *g, int p);
const char *Strat_MineBlocked(const Game *g, int p);
bool        Strat_OpenMine(Game *g, int p);
const char *Strat_RecruitBlocked(const Game *g, int n, int type);
void        Strat_PayRecruit(Game *g, int n, int type);
void        Strat_Init(Game *g);
void        Strat_Turn(Game *g);

/* command.c : commandes serialisables, somme de controle, rejeu (base du multijoueur) */
typedef enum {
    CMD_END_TURN, CMD_RECRUIT, CMD_ORDER, CMD_DISBAND, CMD_DECLARE_WAR, CMD_DIPLOMACY, CMD_OPERATION, CMD_POSTURE,
    CMD_INVEST, CMD_FACTION_ACTION, CMD_PROJECT, CMD_TAX, CMD_SPENDING, CMD_RESEARCH, CMD_FORTIFY, CMD_DEPOT,
    CMD_GROUP, CMD_GROUP_ORDER, CMD_MISSION, CMD_DILEMMA, CMD_BORROW, CMD_REPAY, CMD_RESOLUTION, CMD_FACTORY,
    CMD_FAB, CMD_MINE, CMD_STRIKE, CMD_TACTICAL, CMD_ULTIMATUM, CMD_NUKE_POSTURE, CMD_UMBRELLA, CMD_PROMOTE,
    CMD_ASSIGN, CMD_RESEARCH_BUDGET, CMD_TREATY_DRAFT, CMD_TREATY_CEDE, CMD_TREATY_REPARATIONS, CMD_NEGOTIATE,
    CMD_NATO, CMD_EMBARGO, CMD_COUNTER, CMD_WARNING, CMD_CULTURE, CMD_TARIFF, CMD_STRAT_BUY, CMD_REGIME,
    CMD_DECREE, CMD_MIGRATION, CMD_ALERT, CMD_ADD_PLAYER, CMD_PEACE, CMD_SCENARIO, CMD_MINISTER, CMD_DROP_PLAYER,
    CMD_COUNT
} CmdType;

typedef struct {
    unsigned char type;
    signed char player;    /* nation du joueur */
    short a, b, c;
    float f;
} Command;

#define CMD_BYTES  12
#define REPLAY_MAX 8192

typedef struct {
    unsigned seed;
    int difficulty;
    int count;
    Command cmds[REPLAY_MAX];
} Replay;

bool        Cmd_Apply(Game *g, const Command *c);
void        Cmd_Serialize(const Command *c, unsigned char out[CMD_BYTES]);
bool        Cmd_Deserialize(const unsigned char in[CMD_BYTES], Command *c);
unsigned    Game_Checksum(const Game *g);
void        Replay_Clear(Replay *r);
bool        Replay_Record(Replay *r, const Command *c);
void        Replay_Run(const Replay *r, Game *g);
bool        Replay_Save(const Replay *r, const char *path);

/* nuclear.c */
bool        Nuke_IsPower(const Game *g, int n);
float       Nuke_Upkeep(const Game *g);
void        Nuke_SetPosture(Game *g, int posture);
const char *Nuke_UmbrellaBlocked(const Game *g);
void        Nuke_ToggleUmbrella(Game *g);
const char *Nuke_StrikeBlocked(const Game *g, int p);
bool        Nuke_Strike(Game *g, int p);
const char *Nuke_TacticalBlocked(const Game *g, int p);
bool        Nuke_Tactical(Game *g, int p);
const char *Nuke_WarningBlocked(const Game *g, int n);
bool        Nuke_Warning(Game *g, int n);
const char *Nuke_CounterBlocked(const Game *g, int n, int kind);
bool        Nuke_Counter(Game *g, int n, int kind);   /* 0 pression diplomatique, 1 sabotage cyber */
bool        Nuke_Deters(Game *g, int n);
void        Nuke_Turn(Game *g);

#endif
