/*
 * BOUCLIER 2050 - simulation (aucune dependance graphique)
 *
 * Le monde de 2050 : rechauffement climatique, penuries, guerre hybride.
 * La France defend ses frontieres et ses interets dans tous les milieux
 * et champs de conflictualite (M2MC) : terre, mer, air, espace, cyber,
 * informationnel et spectre electromagnetique.
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#define TOTAL_TURNS   40      /* 40 trimestres : 2050 -> 2060 */
#define START_YEAR    2050
#define MAX_THREATS   96
#define MAX_LOG       80
#define LOG_LEN       160
#define BASE_CP       3       /* points de commandement par tour */
#define THREAT_SCALE  0.5f    /* intensite globale des effets des menaces */

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

typedef enum { EV_INFO, EV_GOOD, EV_BAD, EV_CLIMATE, EV_EVENT } LogKind;

typedef enum {
    GAME_RUNNING, GAME_WON,
    GAME_LOST_COHESION, GAME_LOST_SOUVERAINETE, GAME_LOST_BUDGET
} GameState;

/* ---- definitions statiques ---- */
typedef struct {
    const char *name;
    const char *desc;
    Domain domain;
} TheaterDef;

typedef struct {
    const char *name;
    Domain domain;
    bool migration;
    float effects[R_COUNT];   /* par point de force et par tour */
    int minStr, maxStr;
    int duration;
} ThreatDef;

typedef struct {
    const char *name;
    const char *desc;
} FactionDef;

typedef struct {
    const char *name;
    const char *desc;
    float cost;
    int turns;
} ProjectDef;

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

extern const char       *DOMAIN_NAMES[D_COUNT];
extern const char       *RESOURCE_NAMES[R_COUNT];
extern const char       *RESOURCE_DESC[R_COUNT];
extern const TheaterDef  THEATERS[TH_COUNT];
extern const ThreatDef   THREATS[];
extern const int         THREAT_TYPE_COUNT;
extern const FactionDef  FACTIONS[F_COUNT];
extern const ProjectDef  PROJECTS[P_COUNT];
extern const ActionDef   ACTIONS[A_COUNT];
extern const DilemmaDef  DILEMMAS[];
extern const int         DILEMMA_COUNT;
extern const char       *MIGRATION_DOCTRINES[3];
extern const char       *MIGRATION_DESC[3];
extern const char       *ALERT_LEVELS[3];
extern const char       *ALERT_DESC[3];

/* ---- etat dynamique ---- */
typedef struct {
    bool active;
    int type;
    int theater;
    int faction;
    int strength;
    int turnsLeft;
    bool detected;
    bool fresh;               /* apparue ce tour-ci */
} Threat;

typedef struct {
    float hostility;          /* 0..100 */
    float capability;         /* 0..100 */
    int intelTurns;           /* tours restants de renseignement */
    bool actedThisTurn;
} Faction;

typedef struct {
    int state;                /* 0 = rien, 1 = en chantier, 2 = operationnel */
    int turnsLeft;
} Project;

typedef struct {
    char text[LOG_LEN];
    int turn;
    LogKind kind;
} LogEntry;

typedef struct {
    unsigned rng;
    int turn;                 /* 0 .. TOTAL_TURNS */
    float temp;               /* anomalie de temperature (degres C) */
    float res[R_COUNT];
    float lastDelta[R_COUNT];

    int capacity[D_COUNT];
    int pendingCap[D_COUNT];
    int pendingTurns[D_COUNT];

    int posture[TH_COUNT];
    float tension[TH_COUNT];  /* degats subis au dernier tour */

    Threat threats[MAX_THREATS];
    Faction fac[F_COUNT];
    Project proj[P_COUNT];

    int migrationDoctrine;    /* 0 accueil, 1 equilibre, 2 fermeture */
    int alertLevel;           /* 0 normale, 1 renforcee, 2 urgence */
    int commandPoints;

    bool aiDefense;
    float climateMitigation;  /* reduit la probabilite de catastrophes */
    float importBonus;        /* energie importee (accord gazier) */

    int pendingDilemma;       /* -1 si aucun */
    bool dilemmaUsed[16];

    LogEntry log[MAX_LOG];
    int logCount;

    GameState state;
    int statNeutralized;
    int statSuffered;
    int statDisasters;
} Game;

void        Game_Init(Game *g, unsigned seed);
void        Game_EndTurn(Game *g);

int         Game_PostureUsed(const Game *g, Domain d);
bool        Game_ChangePosture(Game *g, int theater, int delta);
float       Game_InvestCost(const Game *g, Domain d);
bool        Game_InvestCapacity(Game *g, Domain d);
float       Game_DefensePower(const Game *g, int theater);
float       Game_NeutralizeChance(const Game *g, const Threat *t);

const char *Game_ActionBlocked(const Game *g, int faction, int action); /* NULL si possible */
bool        Game_FactionAction(Game *g, int faction, int action);

bool        Game_StartProject(Game *g, int project);
void        Game_SetMigrationDoctrine(Game *g, int d);
void        Game_SetAlertLevel(Game *g, int a);
void        Game_ResolveDilemma(Game *g, int choice);

float       Game_Income(const Game *g);
float       Game_Upkeep(const Game *g);
int         Game_ThreatCount(const Game *g, int theater);
int         Game_ActiveThreats(const Game *g);
void        Game_DateString(int turn, char *buf, int len);
int         Game_Score(const Game *g);
void        Game_Log(Game *g, LogKind kind, const char *fmt, ...);

#endif
