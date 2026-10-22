/*
 * BOUCLIER 2050 - coeur de la simulation : initialisation, guerre hybride
 * (theatres M2MC, menaces, factions), projets, dilemmes, enchainement du tour.
 */
#include "game_internal.h"
#include "lang.h"
#include "save_fields.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC "B2050SV"

/* faction -> types de menaces privilegies */
static const int FACTION_THREATS[F_COUNT][8] = {
    {  3,  5,  7,  8,  9, 10, 12, 16 },   /* Boreale */
    {  1,  6,  0,  2,  4, 15,  1,  6 },   /* Sables */
    {  0,  4, 13, 17,  5, 11, 17,  4 },   /* Hydra */
    { 14, 15, 11, 12, 13, 14, 16, 15 },   /* Nebula */
};

/* poids geographiques par faction et theatre */
static const int FACTION_GEO[F_COUNT][TH_COUNT] = {
    /* N  E  A  P  At Me OM Ci Or Cy In Sp */
    {  3, 4, 1, 0, 3, 1, 1, 1, 1, 1, 1, 1 },
    {  1, 1, 2, 3, 1, 4, 1, 1, 1, 1, 1, 1 },
    {  2, 1, 3, 3, 2, 2, 4, 1, 1, 1, 1, 1 },
    {  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};
/* (le theatre des territoires exterieurs n'est alimente que par l'occupation) */


void Game_Log(Game *g, LogKind kind, const char *fmt, ...)
{
    if (g->logCount == MAX_LOG) {
        memmove(&g->log[0], &g->log[1], sizeof(LogEntry) * (MAX_LOG - 1));
        g->logCount--;
    }
    LogEntry *e = &g->log[g->logCount++];
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    vsnprintf(e->text, LOG_LEN, fmt, ap);
    va_end(ap);
    /* le format et chaque argument mis en forme, pour reconstituer le message dans une autre langue */
    snprintf(e->fmt, LOG_LEN, "%s", fmt);
    int len = 0;
    e->args[0] = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') continue;
        if (p[1] == '%') { p++; continue; }
        char spec[16];
        int k = 0;
        spec[k++] = *p++;
        while (*p && strchr("-+ #0123456789.", *p) && k < 12) spec[k++] = *p++;
        int lng = 0;
        while (*p == 'l' || *p == 'h' || *p == 'z') { if (*p == 'l') lng++; if (k < 13) spec[k++] = *p; p++; }
        if (!*p) break;
        spec[k++] = *p;
        spec[k] = 0;
        char piece[LOG_LEN];
        switch (*p) {
        case 'd': case 'i': case 'c':
            if (lng) snprintf(piece, sizeof(piece), spec, va_arg(aq, long));
            else snprintf(piece, sizeof(piece), spec, va_arg(aq, int));
            break;
        case 'u': case 'x': case 'X':
            if (lng) snprintf(piece, sizeof(piece), spec, va_arg(aq, unsigned long));
            else snprintf(piece, sizeof(piece), spec, va_arg(aq, unsigned));
            break;
        case 'f': case 'g': case 'e': snprintf(piece, sizeof(piece), spec, va_arg(aq, double)); break;
        case 's': snprintf(piece, sizeof(piece), "%s", va_arg(aq, const char *)); break;
        default: piece[0] = 0; (void)va_arg(aq, void *); break;
        }
        if (len < LOG_LEN - 1) len += snprintf(e->args + len, LOG_LEN - len, "%s\x1f", piece);
    }
    va_end(aq);
    e->turn = g->turn;
    e->kind = kind;
}

void Game_DateString(int turn, char *buf, int len)
{
    snprintf(buf, len, Tr("T%d %d"), turn % 4 + 1, START_YEAR + turn / 4);
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

void Game_Init(Game *g, unsigned seed, int difficulty)
{
    memset(g, 0, sizeof(*g));
    Map_Init();
    g->rng = seed ? seed : 12345u;
    g->difficulty = difficulty < 0 || difficulty >= DIFF_COUNT ? DIFF_NORMAL : difficulty;
    g->temp = 1.5f;

    g->res[R_BUDGET]       = 35;
    g->res[R_ENERGIE]      = 60;
    g->res[R_EAU]          = 60;
    g->res[R_ALIM]         = 62;
    g->res[R_COHESION]     = 62;
    g->res[R_INFLUENCE]    = 55;
    g->res[R_SOUVERAINETE] = 75;

    static const int cap[D_COUNT] = { 9, 7, 4, 2, 3, 2, 2 };
    memcpy(g->capacity, cap, sizeof(cap));
    static const int post[TH_COUNT] = { 2, 3, 2, 2, 3, 3, 2, 3, 1, 2, 1, 1, 0 };
    memcpy(g->posture, post, sizeof(post));

    g->fac[F_BOREALE] = (Faction){ 50, 55, 0, false };
    g->fac[F_SABLES]  = (Faction){ 35, 35, 0, false };
    g->fac[F_HYDRA]   = (Faction){ 35, 45, 0, false };
    g->fac[F_NEBULA]  = (Faction){ 45, 40, 0, false };

    for (int n = 0; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        na->alive = true;
        na->regime = NATIONS[n].regime;
        na->relation = NATIONS[n].relation;
        na->eu = NATIONS[n].eu;
        na->government = IsDemocratic(na->regime) ? I_LIBERAL : -1;
        na->nextElection = IsDemocratic(na->regime) ? 4 + (int)(Rand(g) % 16) : -1;
        na->nukeStatus = NATIONS[n].nuclear ? 2 : 0;
        na->treasury = 5;
        na->inflation = 2;
    }
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        pr->owner = pr->ctrl = pr->core = PROVINCES[p].nation;
        pr->pop = PROVINCES[p].pop;
        pr->jobs = 1;
        for (int k = 0; k < 3; k++)
            if (PROVINCE_CULTURE[p][k] < CULT_COUNT) pr->culture[PROVINCE_CULTURE[p][k]] += PROVINCE_CULTURE_SHARE[p][k];
        for (int r = 0; r < REL_COUNT; r++) pr->religion[r] = PROVINCE_RELIGION[p][r];
        pr->integration = 100;
        pr->prosperity = 1;
        pr->industry = PROVINCE_INDUSTRY[p];
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = 2.0f;
        pr->hab = Soc_Habitability(g, p);
        Pol_InitProvince(g, p);
        g->nat[pr->owner].startProvinces++;
    }

    /* economie */
    for (int s = 0; s < S_COUNT; s++) g->taxRate[s] = 0.3f;
    for (int s = 0; s < SP_COUNT; s++) g->spending[s] = 0.4f;
    g->tariff = 0.05f;
    for (int k = 0; k < G_COUNT; k++) g->price[k] = GOOD_BASE_PRICE[k];
    for (int n = 0; n < N_COUNT; n++)
        for (int k = 0; k < G_COUNT; k++) g->eco[n].sat[k] = 1;

    /* politique et dissuasion */
    g->nat[N_FRANCE].government = I_LIBERAL;
    g->nat[N_FRANCE].nextElection = 16;
    g->nukePosture = 1;
    g->credibility = 60;
    g->culturePolicy = CP_ASSIMILATION;

    g->seaLevel = 0.2f;
    static const float debt0[DIFF_COUNT] = { 220, 300, 420 };   /* dette heritee selon la difficulte */
    g->debt = debt0[g->difficulty];
    g->inflation = 2;
    g->priceLevel = 1;
    Intl_Init(g);
    Cab_Init(g);
    Strat_Init(g);
    Mil_Init(g);
    War_InitUnits(g);

    g->migrationDoctrine = 1;
    g->alertLevel = 0;
    g->commandPoints = BASE_CP;
    g->pendingDilemma = -1;
    g->state = GAME_RUNNING;
    for (int i = 0; i < MAX_THREATS; i++) g->threats[i].prov = -1;

    Eco_Turn(g);
    g->res[R_ALIM] = 62; g->res[R_EAU] = 60; g->res[R_ENERGIE] = 60;
    g->histScore[0] = (float)Game_Score(g, NULL);
    g->histPop[0] = Soc_NationPop(g, N_FRANCE);
    g->histCohesion[0] = g->res[R_COHESION];
    g->histTemp[0] = g->temp;
    for (int n = 0; n < N_COUNT; n++) {
        g->histNatPop[n][0] = Soc_NationPop(g, n);
        g->histNatArmy[n][0] = g->nat[n].army;
        g->histNatRel[n][0] = g->nat[n].relation;
    }

    Game_Log(g, EV_EVENT, "Janvier 2030. +1,5°C. Le Président vous confie le Conseil de défense et de sécurité.");
    Game_Log(g, EV_INFO, "Gérez la population, protégez les frontières dans tous les milieux et tenez jusqu'en 2060.");

    for (int i = 0; i < 2; i++) SpawnThreatFor(g, Rand(g) % F_COUNT);
    g->human[N_FRANCE] = true;
    g->activePlayer = N_FRANCE;
    War_UpdateSupply(g);
    War_UpdateIntel(g);
}

/* scenario de depart en crise : 2040, a appliquer juste apres Game_Init */
bool Game_ApplyScenario(Game *g, int scenario)
{
    if (scenario != 1 || g->turn != 0 || g->startTurn != 0) return false;
    g->scenario = 1;
    g->turn = g->startTurn = 40;
    g->temp = 2.05f;
    g->seaLevel = 0.2f + 0.45f * (g->temp - 1.5f);
    g->res[R_BUDGET] = 15;
    g->res[R_ENERGIE] = 48;
    g->res[R_EAU] = 45;
    g->res[R_ALIM] = 50;
    g->res[R_COHESION] = 45;
    g->res[R_INFLUENCE] = 42;
    g->res[R_SOUVERAINETE] = 66;
    /* dix ans de reformes : des capacites hybrides plus solides qu'en 2030 */
    for (int d = D_ESPACE; d <= D_EM; d++) g->capacity[d] += 2;
    g->debt += 120;
    g->inflation = 6;
    g->priceLevel = 1.3f;
    g->natoCohesion = 50;
    g->credibility = 50;
    for (int f = 0; f < F_COUNT; f++) AddFac(g, f, 12, 12);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) for (int s = 0; s < S_COUNT; s++) pr->mil[s] += 1.5f;
        pr->hab = Soc_Habitability(g, p);
    }
    Nation *ly = &g->nat[N_LIBYE];
    ly->failed = true;
    ly->regime = RG_JUNTE;
    g->nat[N_ALGERIE].nukeStatus = 1;
    g->nat[N_ALGERIE].nukeProgress = 0.45f;
    for (int n = N_MAROC; n <= N_EGYPTE; n++) AddRelation(g, n, -15);
    for (int i = 0; i < 4; i++) SpawnThreatFor(g, Rand(g) % F_COUNT);
    for (int t = 0; t <= g->turn; t++) {
        g->histPop[t] = Soc_NationPop(g, N_FRANCE);
        g->histCohesion[t] = g->res[R_COHESION];
        g->histTemp[t] = g->temp;
        g->histScore[t] = (float)Game_Score(g, NULL);
        for (int n = 0; n < N_COUNT; n++) {
            g->histNatPop[n][t] = Soc_NationPop(g, n);
            g->histNatArmy[n][t] = g->nat[n].army;
            g->histNatRel[n][t] = g->nat[n].relation;
        }
    }
    Game_Log(g, EV_EVENT, "Janvier 2040. +2°C. Le Président vous rappelle aux affaires : le pays est au bord du gouffre.");
    Game_Log(g, EV_WAR, "La Fédération Boréale franchit la frontière balte.");
    War_StartWar(g, N_BOREALE, N_BALTES, true);
    War_UpdateSupply(g);
    War_UpdateIntel(g);
    return true;
}

/* ------------------------------------------------------------------ */
/* Forces de defense (theatres M2MC)                                   */
/* ------------------------------------------------------------------ */

int Game_PostureUsed(const Game *g, Domain d)
{
    int n = 0;
    for (int t = 0; t < TH_COUNT; t++)
        if (THEATERS[t].domain == d) n += g->posture[t];
    return n;
}

bool Game_ChangePosture(Game *g, int theater, int delta)
{
    Domain d = THEATERS[theater].domain;
    int nv = g->posture[theater] + delta;
    if (nv < 0) return false;
    if (delta > 0 && Game_PostureUsed(g, d) + delta > g->capacity[d]) return false;
    g->posture[theater] = nv;
    return true;
}

float Game_InvestCost(const Game *g, Domain d)
{
    static const float base[D_COUNT] = { 5, 7, 7, 9, 5, 4, 5 };
    return base[d] + g->capacity[d] * 0.4f;
}

bool Game_InvestCapacity(Game *g, Domain d)
{
    if (g->pendingTurns[d] > 0) return false;
    float c = Game_InvestCost(g, d);
    if (g->res[R_BUDGET] < c) return false;
    g->res[R_BUDGET] -= c;
    g->pendingCap[d] = 2;
    g->pendingTurns[d] = 2;
    Game_Log(g, EV_INFO, "Montée en puissance %s lancée (+2 capacité dans 2 tours).", (DOMAIN_NAMES[d]));
    return true;
}

static int ActiveThreatsOfDomain(const Game *g, Domain d)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++)
        if (g->threats[i].active && THREATS[g->threats[i].type].domain == d) n++;
    return n;
}

float Game_DefensePower(const Game *g, int theater)
{
    float p = g->posture[theater] * 1.5f;
    float mult = 1.0f + 0.15f * g->alertLevel;
    if (g->aiDefense) mult += 0.12f;
    mult += 0.25f * REGIMES[g->nat[N_FRANCE].regime].repression;   /* Etat policier */
    Domain d = THEATERS[theater].domain;
    if (d == D_AIR || d == D_MER) {
        int em = ActiveThreatsOfDomain(g, D_EM);
        float pen = 0.15f * em;
        if (pen > 0.45f) pen = 0.45f;
        mult -= pen;
    }
    if (theater == TH_ORBITE && g->proj[P_CONSTELL].state == 2) p += 3;
    return p * mult;
}

float Game_NeutralizeChance(const Game *g, const Threat *t)
{
    float def = Game_DefensePower(g, t->theater);
    if (t->prov >= 0) {
        /* les divisions stationnees sur place traquent les insurges */
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (u->active && u->owner == N_FRANCE && u->prov == t->prov && UNIT_TYPES[u->type].domain == DOM_LAND)
                def += u->type == U_FS ? 2.5f : 1.2f;
        }
    }
    if (def <= 0) return 0;
    float p = def / (def + t->strength * 1.2f);
    if (g->fac[t->faction].intelTurns > 0) p *= 1.2f;
    if (THREATS[t->type].migration && g->migrationDoctrine == 2) p *= 1.4f;
    if (!t->detected) p *= 0.35f;
    return Clampf(p, 0, 0.95f);
}

static float DetectChance(const Game *g, const Threat *t)
{
    float p = 0.45f + 0.05f * g->posture[TH_ORBITE] + 0.03f * g->posture[t->theater];
    if (g->proj[P_CONSTELL].state == 2) p += 0.15f;
    if (g->fac[t->faction].intelTurns > 0) p += 0.35f;
    return Clampf(p, 0, 0.98f);
}

int Game_ThreatCount(const Game *g, int theater)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++)
        if (g->threats[i].active && g->threats[i].theater == theater) n++;
    return n;
}

int Game_ActiveThreats(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++) if (g->threats[i].active) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Actions contre les factions hybrides                                */
/* ------------------------------------------------------------------ */

const char *Game_ActionBlocked(const Game *g, int f, int a)
{
    const ActionDef *ad = &ACTIONS[a];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->commandPoints < ad->cp) return "Plus de points de commandement";
    if (g->fac[f].actedThisTurn) return "Déjà une action ce tour sur cette faction";
    if (g->res[R_BUDGET] < ad->budget) return "Budget insuffisant";
    if (g->res[R_INFLUENCE] < ad->influence) return "Influence insuffisante";
    if (ad->needDomain != D_COUNT && g->capacity[ad->needDomain] < ad->needCapacity)
        return ad->needDomain == D_CYBER ? "Capacité Cyber insuffisante (5)" : "Capacité Info insuffisante (3)";
    return NULL;
}

bool Game_FactionAction(Game *g, int f, int a)
{
    if (Game_ActionBlocked(g, f, a)) return false;
    const ActionDef *ad = &ACTIONS[a];
    g->commandPoints -= ad->cp;
    g->res[R_BUDGET] -= ad->budget;
    g->res[R_INFLUENCE] -= ad->influence;
    g->fac[f].actedThisTurn = true;
    const char *fn = Tr(FACTIONS[f].name);

    switch (a) {
    case A_DIPLO:
        if (Randf(g) < 0.45f + g->res[R_INFLUENCE] / 150.0f) {
            AddFac(g, f, -(8 + g->res[R_INFLUENCE] * 0.1f), 0);
            if (f == F_BOREALE) AddRelation(g, N_BOREALE, 8);
            Game_Log(g, EV_GOOD, "Diplomatie : désescalade obtenue avec %s.", fn);
        } else {
            AddFac(g, f, -2, 0);
            Game_Log(g, EV_BAD, "Diplomatie : %s rejette nos propositions.", fn);
        }
        break;
    case A_SANCTION:
        AddFac(g, f, 5, -6);
        if (f == F_BOREALE) { g->nat[N_BOREALE].army *= 0.97f; AddRelation(g, N_BOREALE, -6); }
        Game_Log(g, EV_INFO, "Sanctions contre %s : leurs moyens s'érodent, la rancoeur grandit.", fn);
        break;
    case A_CYBEROFF:
        AddFac(g, f, 6, -10);
        if (Randf(g) < 0.25f) {
            AddFac(g, f, 6, 0);
            AddRes(g, R_INFLUENCE, -6);
            Game_Log(g, EV_BAD, "Cyber-offensive contre %s attribuée publiquement à la France !", fn);
        } else {
            Game_Log(g, EV_GOOD, "Cyber-offensive réussie : infrastructures de %s neutralisées.", fn);
        }
        break;
    case A_INFLUENCE:
        AddFac(g, f, -4, -4);
        AddRes(g, R_COHESION, 1);
        Game_Log(g, EV_GOOD, "Contre-influence : les récits de %s perdent du terrain.", fn);
        break;
    case A_RENSEIGNEMENT:
        g->fac[f].intelTurns = 3;
        for (int i = 0; i < MAX_THREATS; i++)
            if (g->threats[i].active && g->threats[i].faction == f) g->threats[i].detected = true;
        Game_Log(g, EV_GOOD, "Renseignement : les réseaux de %s sont cartographiés.", fn);
        break;
    }
    return true;
}

float Game_ProjectCost(const Game *g, int p)
{
    return PROJECTS[p].cost * REGIMES[g->nat[N_FRANCE].regime].projectCost * (g->nat[N_FRANCE].government == I_ECOLO ? 0.85f : 1.0f);
}

bool Game_StartProject(Game *g, int p)
{
    float c = Game_ProjectCost(g, p);
    if (g->proj[p].state != 0 || g->res[R_BUDGET] < c) return false;
    g->res[R_BUDGET] -= c;
    g->proj[p].state = 1;
    g->proj[p].turnsLeft = PROJECTS[p].turns;
    Game_Log(g, EV_INFO, "Lancement du projet : %s (%d tours).", (PROJECTS[p].name), PROJECTS[p].turns);
    return true;
}

void Game_SetMigrationDoctrine(Game *g, int d)
{
    if (d == g->migrationDoctrine) return;
    g->migrationDoctrine = d;
    AddRes(g, R_COHESION, -2);
    Game_Log(g, EV_INFO, "Nouvelle doctrine migratoire : %s.", (MIGRATION_DOCTRINES[d]));
}

void Game_SetAlertLevel(Game *g, int a)
{
    if (a == g->alertLevel) return;
    g->alertLevel = a;
    Game_Log(g, EV_INFO, "Niveau d'alerte : %s.", (ALERT_LEVELS[a]));
}

/* ------------------------------------------------------------------ */
/* Dilemmes                                                            */
/* ------------------------------------------------------------------ */

const DilemmaDef *Game_CurrentDilemma(const Game *g)
{
    int d = g->pendingDilemma;
    if (d < 0) return NULL;
    if (d >= DLM_EVENT) return Ev_Dilemma(d - DLM_EVENT);
    if (d >= DLM_ALLY_CALL) return &DYN_DILEMMAS[d - DLM_ALLY_CALL];
    return &DILEMMAS[d];
}

void AddMilitancy(Game *g, int strata, float v)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++)
            if (strata < 0 || s == strata) g->prov[p].mil[s] = Clampf(g->prov[p].mil[s] + v, 0, 10);
    }
}

void AddImmigrants(Game *g, float millions)
{
    float total = Soc_NationPop(g, N_FRANCE);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE) continue;
        float m = millions * pr->pop / total;
        float k = m / (pr->pop + m);
        for (int c = 0; c < CULT_COUNT; c++) pr->culture[c] *= 1 - k;
        pr->culture[C_ARABE_MAGHREB] += k * 0.7f;
        pr->culture[C_AMAZIGHE] += k * 0.3f;
        for (int r = 0; r < REL_COUNT; r++) pr->religion[r] *= 1 - k;
        pr->religion[RL_MUSULMANS] += k;
        pr->pop += m;
    }
}


void Game_ResolveDilemma(Game *g, int c)
{
    int d = g->pendingDilemma;
    if (d < 0) return;
    const DilemmaDef *dd = Game_CurrentDilemma(g);
    g->pendingDilemma = -1;
    bool A = (c == 0);
    int prm = g->dilemmaParam;
    switch (d) {
    case 0: if (A) { AddRes(g, R_BUDGET, 15); AddRes(g, R_EAU, 5); AddRes(g, R_SOUVERAINETE, -8); AddFac(g, F_HYDRA, -10, 0); }
            else   { AddFac(g, F_HYDRA, 10, 0); } break;
    case 1: if (A) { AddRes(g, R_INFLUENCE, 8); AddRes(g, R_BUDGET, -6); AddImmigrants(g, 0.3f); }
            else   { AddRes(g, R_BUDGET, -4); AddRes(g, R_INFLUENCE, -6); AddFac(g, F_SABLES, -10, 10); } break;
    case 2: if (A) { AddRes(g, R_ENERGIE, 6); AddMilitancy(g, S_MOYENNES, 1.5f); }
            else   { AddRes(g, R_BUDGET, -10); AddRes(g, R_ENERGIE, 2); } break;
    case 3: if (A) { AddRes(g, R_COHESION, 4); AddRes(g, R_SOUVERAINETE, -3); }
            else   { AddRes(g, R_COHESION, -6); AddFac(g, F_NEBULA, 10, 0); } break;
    case 4: if (A) { AddRes(g, R_BUDGET, -8); g->capacity[D_ESPACE] += 2; AddRes(g, R_INFLUENCE, 5); }
            else   { AddRes(g, R_SOUVERAINETE, 3); } break;
    case 5: if (A) { AddRes(g, R_BUDGET, -8); AddMilitancy(g, S_TRAVAILLEURS, -2); }
            else   { AddMilitancy(g, S_TRAVAILLEURS, 2); AddRes(g, R_BUDGET, 2); } break;
    case 6: if (A) { AddRes(g, R_BUDGET, -10); AddFac(g, F_SABLES, 15, -12); AddRes(g, R_INFLUENCE, -3); }
            else   { AddRes(g, R_INFLUENCE, 2); AddFac(g, F_SABLES, 0, 8); } break;
    case 7: if (A) { g->importBonus += 6; AddRelation(g, N_BOREALE, 30); AddFac(g, F_BOREALE, -15, 0);
                     AddRes(g, R_SOUVERAINETE, -5); AddRes(g, R_INFLUENCE, -5); }
            else   { AddRes(g, R_SOUVERAINETE, 2); } break;
    case 8: if (A) { g->aiDefense = true; AddRes(g, R_COHESION, -4); AddRes(g, R_INFLUENCE, -3); }
            else   { AddRes(g, R_COHESION, 2); } break;
    case 9: if (A) { AddRes(g, R_BUDGET, -12); AddRes(g, R_COHESION, 5); AddRes(g, R_INFLUENCE, 2); }
            else   { AddRes(g, R_COHESION, -6); AddRes(g, R_INFLUENCE, -3); } break;
    case 10: if (A) { AddRes(g, R_BUDGET, -6); g->capacity[D_CYBER] += 3; AddRes(g, R_INFLUENCE, -4); }
             else   { AddFac(g, F_NEBULA, 8, -6); } break;
    case 11: if (A) { AddRes(g, R_BUDGET, -10); AddRes(g, R_INFLUENCE, 10); g->climateMitigation += 0.06f; }
             else   { AddRes(g, R_BUDGET, 1); AddRes(g, R_INFLUENCE, -4); } break;
    case 12: if (A) { Soc_SetRegime(g, RG_JUNTE); AddRes(g, R_COHESION, -8); g->regimeCooldown = 12; }
             else   {
                 AddRes(g, R_COHESION, 3);
                 for (int i = 0; i < MAX_UNITS; i++)
                     if (g->units[i].active && g->units[i].owner == N_FRANCE) { War_Disband(g, i); break; }
             } break;
    case 13: if (A) { Soc_SetRegime(g, RG_UTOPIE); AddMilitancy(g, S_ELITES, 3); g->regimeCooldown = 12; }
             else   { AddMilitancy(g, S_MOYENNES, 2); AddRes(g, R_COHESION, -4); } break;
    case 14: if (A) { AddRes(g, R_BUDGET, -10); AddRes(g, R_SOUVERAINETE, 6); AddRes(g, R_EAU, 4); AddFac(g, F_HYDRA, 15, 0); }
             else   { AddMilitancy(g, S_TRAVAILLEURS, 1); } break;
    case 15: if (A) { AddImmigrants(g, 1.0f); AddRes(g, R_INFLUENCE, 8); AddRes(g, R_COHESION, -4);
                      for (int n = N_MAROC; n <= N_EGYPTE; n++) AddRelation(g, n, 10); }
             else   { AddRes(g, R_INFLUENCE, -8); AddFac(g, F_SABLES, 10, 0);
                      for (int i = 0; i < 3; i++) SpawnThreatFor(g, F_SABLES); } break;
    case DLM_ALLY_CALL: {
        int victim = g->dilemmaParam2;
        if (A && g->nat[prm].alive && !War_AtWar(g, N_FRANCE, prm)) {
            AddRelation(g, victim, 30);
            AddRes(g, R_INFLUENCE, 5);
            War_StartWar(g, N_FRANCE, prm, false);
            Game_Log(g, EV_WAR, "La France entre en guerre contre %s pour défendre %s.", (NATIONS[prm].name), (NATIONS[victim].name));
        } else if (!A) {
            AddRes(g, R_INFLUENCE, -4);
            AddRelation(g, victim, -15);
            if (g->ally[N_FRANCE][victim]) {
                g->ally[N_FRANCE][victim] = g->ally[victim][N_FRANCE] = false;
                Game_Log(g, EV_BAD, "La France abandonne son allié %s : l'alliance est rompue.", (NATIONS[victim].name));
            }
        }
    } break;
    case DLM_PEACE_OFFER:
        if (A && g->treaty.active && War_AtWar(g, N_FRANCE, prm)) War_SignTreaty(g, &g->treaty);
        else {
            g->treaty.active = false;
            g->nat[prm].peaceRefusals++;
            g->nat[prm].peaceCooldown = 4;
            Game_Log(g, EV_WAR, "La France rejette l'offre de paix de %s. Elle reviendra avec d'autres conditions.", (NATIONS[prm].name));
        }
        break;
    case DLM_COUP_LEGAL:
        if (A) {
            Soc_SetRegime(g, RG_AUTORITAIRE);
            g->regimeCooldown = 12;
            Game_Log(g, EV_EVENT, "Les Autoritaires prennent le pouvoir par les urnes.");
        } else {
            AddRes(g, R_COHESION, -10);
            for (int p = 0; p < PROV_COUNT; p++)
                if (g->prov[p].owner == N_FRANCE)
                    for (int s = 0; s < S_COUNT; s++) g->prov[p].con[s] = Clampf(g->prov[p].con[s] + 1, 0, 10);
            Game_Log(g, EV_EVENT, "Le scrutin est annulé. La rue gronde.");
        }
        break;
    case DLM_NUKE_BLACKMAIL:
        Nuke_ResolveBlackmail(g, A);
        break;
    case DLM_BAILOUT:
        Eco_ResolveBailout(g, A);
        break;
    case DLM_UN_VETO:
        Intl_ResolveVeto(g, A);
        break;
    case DLM_AI_ALLIANCE:
        if (!g->nat[prm].alive || War_AtWar(g, N_FRANCE, prm)) break;
        if (A) {
            g->ally[N_FRANCE][prm] = g->ally[prm][N_FRANCE] = true;
            AddRelation(g, prm, 10);
            AddRes(g, R_INFLUENCE, 2);
            Game_Log(g, EV_GOOD, "Alliance défensive conclue avec %s.", (NATIONS[prm].name));
        } else AddRelation(g, prm, -8);
        break;
    case DLM_AI_TRADE:
        if (!g->nat[prm].alive || War_AtWar(g, N_FRANCE, prm)) break;
        if (A) {
            g->nat[prm].trade = true;
            AddRelation(g, prm, 5);
            Game_Log(g, EV_GOOD, "Accord commercial signé avec %s : la France est servie en priorité sur leurs excédents.", (NATIONS[prm].name));
        } else AddRelation(g, prm, -4);
        break;
    case DLM_PEACE_BLACKMAIL:
        if (!g->nat[prm].alive) break;
        if (A) {
            AddRes(g, R_BUDGET, -12);
            AddRes(g, R_COHESION, -3);
            g->credibility = Clampf(g->credibility - 8, 0, 100);
            AddRelation(g, prm, 15);
        } else {
            AddRelation(g, prm, -15);
            if (!War_AtWar(g, N_FRANCE, prm) && Randf(g) > g->credibility / 100.0f + 0.3f) {
                War_StartWar(g, prm, N_FRANCE, true);
                Game_Log(g, EV_WAR, "%s met ses menaces à exécution et déclare la guerre à la France !", (NATIONS[prm].name));
            } else {
                g->credibility = Clampf(g->credibility + 4, 0, 100);
                Game_Log(g, EV_GOOD, "La France ne cède pas au chantage de %s.", (NATIONS[prm].name));
            }
        }
        break;
    default:
        if (d >= DLM_EVENT) Ev_Apply(g, d - DLM_EVENT, c);
        break;
    }
    if (d < DLM_ALLY_CALL || d >= DLM_EVENT)
        Game_Log(g, EV_EVENT, "%s : %s.", (dd->title), A ? (dd->optA) : (dd->optB));
}

/* ------------------------------------------------------------------ */
/* Menaces hybrides                                                    */
/* ------------------------------------------------------------------ */

static int AddThreat(Game *g, int type, int theater, int f, int strength)
{
    for (int i = 0; i < MAX_THREATS; i++) {
        Threat *th = &g->threats[i];
        if (th->active) continue;
        th->active = true;
        th->type = type;
        th->theater = theater;
        th->faction = f;
        th->strength = strength < 1 ? 1 : strength;
        th->turnsLeft = THREATS[type].duration;
        th->detected = g->fac[f].intelTurns > 0 || Randf(g) < 0.3f;
        th->fresh = true;
        th->prov = -1;
        return i;
    }
    return -1;
}

void SpawnThreatFor(Game *g, int f)
{
    int type = FACTION_THREATS[f][Rand(g) % 8];
    Domain d = THREATS[type].domain;

    int total = 0;
    for (int t = 0; t < TH_COUNT; t++) if (THEATERS[t].domain == d) total += FACTION_GEO[f][t];
    int theater = -1;
    if (total > 0) {
        int r = (int)(Rand(g) % (unsigned)total);
        for (int t = 0; t < TH_COUNT; t++) {
            if (THEATERS[t].domain != d) continue;
            r -= FACTION_GEO[f][t];
            if (r < 0) { theater = t; break; }
        }
    }
    if (theater < 0)
        for (int t = 0; t < TH_COUNT; t++) if (THEATERS[t].domain == d) { theater = t; break; }

    int s = RandRange(g, THREATS[type].minStr, THREATS[type].maxStr) + (int)(g->fac[f].capability / 40.0f);
    if (g->fac[f].intelTurns > 0) s--;
    AddThreat(g, type, theater, f, s);
}

/* flux migratoire refoule par la doctrine : devient une menace aux frontieres */
void Hybrid_SpawnMigration(Game *g, int theater, float millions)
{
    int type = THEATERS[theater].domain == D_MER ? 6 : 1;
    int s = 2 + (int)(millions * 8.0f);
    if (s > 9) s = 9;
    AddThreat(g, type, theater, F_SABLES, s);
}

/* insurrection dans une province occupee ou conquise par la France */
void Hybrid_SpawnExterior(Game *g, int p, int strength)
{
    int core = g->prov[p].core;
    int f = core >= N_MAROC || core == N_TURQUIE ? F_SABLES : core == N_BOREALE || core == N_UKRAINE ? F_BOREALE : F_NEBULA;
    int type = 18 + (int)(Rand(g) % 3);
    int i = AddThreat(g, type, TH_EXTERIEUR, f, strength);
    if (i >= 0) g->threats[i].prov = (short)p;
}

static void ExteriorUnrest(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->ctrl != N_FRANCE || pr->core == N_FRANCE) continue;
        if (pr->owner == N_FRANCE && pr->integration > 70) continue;
        float mil = Soc_ProvinceMilitancy(pr);
        int already = 0;
        for (int i = 0; i < MAX_THREATS; i++) if (g->threats[i].active && g->threats[i].prov == p) already++;
        if (already < 1 && Randf(g) < (mil - 3) * 0.04f * DiffThreat(g))
            Hybrid_SpawnExterior(g, p, 1 + (int)(mil / 3));
    }
}

static void ResolveThreats(Game *g)
{
    for (int t = 0; t < TH_COUNT; t++) g->tension[t] = 0;

    for (int i = 0; i < MAX_THREATS; i++) {
        Threat *t = &g->threats[i];
        if (!t->active) continue;
        t->fresh = false;
        const ThreatDef *td = &THREATS[t->type];

        if (!t->detected && Randf(g) < DetectChance(g, t)) t->detected = true;

        /* une insurrection s'eteint si la province n'est plus tenue par la France */
        if (t->prov >= 0 && g->prov[t->prov].ctrl != N_FRANCE) { t->active = false; continue; }

        if (Randf(g) < Game_NeutralizeChance(g, t)) {
            t->active = false;
            g->statNeutralized++;
            AddRes(g, R_SOUVERAINETE, 0.4f);
            AddFac(g, t->faction, 0, -0.5f);   /* chaque reseau demantele use la faction */
            Game_Log(g, EV_GOOD, "%s neutralisé(e) - %s.", (td->name), (THEATERS[t->theater].name));
            continue;
        }

        float mod = 1.0f;
        if (td->domain == D_CYBER && g->proj[P_CYBERCIV].state == 2) mod *= 0.65f;
        if (td->domain == D_INFO  && g->proj[P_MEDIAS].state == 2)   mod *= 0.65f;

        float dmg = 0;
        for (int r = 0; r < R_COUNT; r++) {
            float e = td->effects[r] * t->strength * mod * THREAT_SCALE;
            if (td->migration && r == R_COHESION && g->migrationDoctrine == 0) e *= 0.5f;
            AddRes(g, (Resource)r, e);
            dmg -= e;
        }
        if (td->migration && g->migrationDoctrine == 2 && Randf(g) < 0.15f) {
            AddRes(g, R_INFLUENCE, -4);
            Game_Log(g, EV_BAD, "Drame humanitaire à la frontière (%s) : indignation internationale.",
                     (THEATERS[t->theater].name));
        }
        g->tension[t->theater] += dmg;
        g->statSuffered++;
        if (t->prov >= 0) {
            Province *pr = &g->prov[t->prov];
            pr->damage = Clampf(pr->damage + 0.02f * t->strength, 0, 1);
            for (int u = 0; u < MAX_UNITS; u++) {
                Unit *un = &g->units[u];
                if (un->active && un->owner == N_FRANCE && un->prov == t->prov) un->org = Clampf(un->org - 0.03f * t->strength, 0, 1);
            }
        }

        if (--t->turnsLeft <= 0) {
            t->active = false;
            Game_Log(g, EV_BAD, "%s (%s) a frappé sans être stoppé(e).", (td->name), (THEATERS[t->theater].name));
        }
    }
}

static void FactionsTurn(Game *g)
{
    /* la Federation Boreale hybride suit l'etat de la nation boreale */
    const Nation *bo = &g->nat[N_BOREALE];
    float capT = bo->alive ? Clampf(bo->army / War_ArmyMax(g, N_BOREALE) * 75.0f, 5, 100) : 5;
    float hosT = Clampf(55 - bo->relation * 0.5f, 5, 100);
    g->fac[F_BOREALE].capability += (capT - g->fac[F_BOREALE].capability) * 0.15f;
    g->fac[F_BOREALE].hostility  += (hosT - g->fac[F_BOREALE].hostility) * 0.15f;

    /* la Ligue des Sables se nourrit de l'effondrement du Sud */
    float habSud = 0, popSud = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].core >= N_MAROC) { habSud += g->prov[p].hab * g->prov[p].pop; popSud += g->prov[p].pop; }
    }
    habSud = popSud > 0 ? habSud / popSud : 80;
    AddFac(g, F_SABLES, 0.3f * (Randf(g) * 2 - 0.8f), Clampf((75 - habSud) * 0.02f, -0.2f, 1.0f));
    if (g->migrationDoctrine == 2) AddFac(g, F_SABLES, 0.4f, 0);

    AddFac(g, F_HYDRA, Randf(g) * 1.2f - 0.5f, 0.1f + (g->temp - 1.5f) * 0.1f);
    AddFac(g, F_NEBULA, Randf(g) * 1.6f - 0.7f, 0.15f);

    for (int f = 0; f < F_COUNT; f++) {
        if (g->fac[f].intelTurns > 0) g->fac[f].intelTurns--;
        g->fac[f].actedThisTurn = false;
    }

    float escalation = 1.0f + 1.25f * g->turn / (float)TOTAL_TURNS;
    for (int f = 0; f < F_COUNT; f++) {
        float h = g->fac[f].hostility / 100.0f, c = g->fac[f].capability / 100.0f;
        /* une faction deja engagee sur de nombreux fronts a moins de moyens pour en ouvrir d'autres */
        int active = 0;
        for (int i = 0; i < MAX_THREATS; i++) if (g->threats[i].active && g->threats[i].faction == f) active++;
        float expected = (0.08f + 1.1f * h * c) * escalation * DiffThreat(g) / (1 + active / 9.0f);
        int n = (int)expected;
        if (Randf(g) < expected - n) n++;
        for (int k = 0; k < n; k++) SpawnThreatFor(g, f);
    }
}

/* ------------------------------------------------------------------ */
/* Climat                                                              */
/* ------------------------------------------------------------------ */

static void ClimateTurn(Game *g)
{
    float warming = (0.011f + 0.00008f * g->turn) * DiffClimate(g) * (g->nat[N_FRANCE].government == I_ECOLO ? 0.9f : 1.0f);
    g->temp += warming - g->climateMitigation * 0.03f;

    float chance = (0.12f + (g->temp - 1.2f) * 0.14f) * DiffClimate(g) - g->climateMitigation;
    if (Randf(g) >= chance) return;
    float k = g->proj[P_ADAPT].state == 2 ? 0.55f : 1.0f;
    g->statDisasters++;
    switch (Rand(g) % 5) {
    case 0:
        AddRes(g, R_ENERGIE, -7 * k); AddRes(g, R_EAU, -7 * k); AddRes(g, R_COHESION, -3 * k);
        Game_Log(g, EV_CLIMATE, "Canicule historique : %.0f°C à Lyon, réseaux à saturation.", 42 + g->temp * 2);
        break;
    case 1:
        AddRes(g, R_EAU, -10 * k); AddRes(g, R_ALIM, -8 * k);
        Game_Log(g, EV_CLIMATE, "Sécheresse : la Loire est à sec, récoltes perdues.");
        break;
    case 2:
        AddRes(g, R_BUDGET, -6 * k); AddRes(g, R_COHESION, -2 * k);
        Game_Log(g, EV_CLIMATE, "Inondations majeures dans le bassin du Rhône.");
        break;
    case 3:
        AddRes(g, R_BUDGET, -4 * k); AddRes(g, R_ALIM, -3 * k); AddRes(g, R_COHESION, -3 * k);
        Game_Log(g, EV_CLIMATE, "Mégafeux dans les Landes et en Provence.");
        break;
    default:
        AddRes(g, R_ENERGIE, -5 * k); AddRes(g, R_BUDGET, -4 * k);
        Game_Log(g, EV_CLIMATE, "Tempête atlantique : littoral dévasté, réseau coupé.");
        break;
    }
    AddFac(g, F_SABLES, 1.0f, 0.5f);
}

/* ------------------------------------------------------------------ */
/* Budget                                                              */
/* ------------------------------------------------------------------ */

float Game_DefenseUpkeep(const Game *g)
{
    float u = 0;
    for (int t = 0; t < TH_COUNT; t++) u += g->posture[t] * 0.6f;
    for (int d = 0; d < D_COUNT; d++) u += g->capacity[d] * 0.1f;
    static const float alert[3] = { 0, 3, 7 };
    u += alert[g->alertLevel];
    if (g->migrationDoctrine == 2) u += 1.5f;
    return u;
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Game_EndTurn(Game *g)
{
    if (g->state != GAME_RUNNING || g->pendingDilemma >= 0) return;

    float before[R_COUNT];
    memcpy(before, g->res, sizeof(before));
    memcpy(g->reportRes, g->res, sizeof(g->reportRes));
    g->reportPop = Soc_NationPop(g, N_FRANCE);
    g->reportProv = Soc_NationProvinces(g, N_FRANCE);
    g->reportUnits = War_UnitCount(g, N_FRANCE);
    g->reportScore = Game_Score(g, NULL);
    g->reportLogStart = g->logCount;

    /* 1. budget de l'Etat */
    Eco_Budget(g, g->budget);
    float net = Eco_Net(g);
    g->res[R_BUDGET] += net;
    Eco_Inflation(g, net);
    Cab_Turn(g);
    /* un deficit de tresorerie se finance par l'emprunt, tant que les marches suivent */
    if (g->res[R_BUDGET] < 0) {
        if (Eco_Rating(g) < RATING_COUNT - 1) {
            g->debt -= g->res[R_BUDGET];
            g->res[R_BUDGET] = 0;
        } else if (g->nat[N_FRANCE].eu && g->bailouts == 0 && g->pendingDilemma < 0) {
            g->pendingDilemma = DLM_BAILOUT;
        } else if (g->pendingDilemma != DLM_BAILOUT) {
            Game_Log(g, EV_BAD, "Plus personne ne prête à la France : défaut de paiement.");
            g->state = GAME_LOST_BUDGET;
        }
    }
    if (Eco_Rating(g) >= 4) AddRes(g, R_COHESION, -0.3f);
    if (g->austerityTurns > 0) {
        g->austerityTurns--;
        for (int s = 0; s < SP_COUNT; s++) if (g->spending[s] > 0.25f) g->spending[s] = 0.25f;
    }
    if (g->migrationDoctrine == 0) AddRes(g, R_INFLUENCE, 1.0f);
    if (g->migrationDoctrine == 2) AddRes(g, R_INFLUENCE, -1.5f);
    if (g->alertLevel == 1) AddRes(g, R_COHESION, -0.4f);
    if (g->alertLevel == 2) AddRes(g, R_COHESION, -1.2f);

    /* 2. projets et montees en puissance */
    for (int p = 0; p < P_COUNT; p++) {
        if (g->proj[p].state == 1 && --g->proj[p].turnsLeft <= 0) {
            g->proj[p].state = 2;
            Game_Log(g, EV_GOOD, "Projet opérationnel : %s.", (PROJECTS[p].name));
        }
    }
    for (int d = 0; d < D_COUNT; d++) {
        if (g->pendingTurns[d] > 0 && --g->pendingTurns[d] == 0) {
            g->capacity[d] += g->pendingCap[d];
            g->pendingCap[d] = 0;
            Game_Log(g, EV_GOOD, "Nouvelles capacités %s disponibles.", (DOMAIN_NAMES[d]));
        }
    }

    /* 3. menaces hybrides */
    ResolveThreats(g);

    /* 4. climat, population, migrations, ressources */
    ClimateTurn(g);
    Soc_Turn(g);

    /* 5. vie politique, guerres, diplomatie, etat-major, dissuasion */
    Pol_Turn(g);
    War_Turn(g);
    Mil_Turn(g);
    Nuke_Turn(g);
    Ops_Turn(g);
    Strat_Turn(g);
    Intl_Turn(g);

    /* 6. factions et nouvelles menaces */
    FactionsTurn(g);
    ExteriorUnrest(g);

    if (g->regimeCooldown > 0) g->regimeCooldown--;
    g->commandPoints = BASE_CP;
    for (int n = 0; n < N_COUNT; n++) g->nat[n].actedThisTurn = false;
    g->turn++;

    for (int r = 0; r < R_COUNT; r++) g->lastDelta[r] = g->res[r] - before[r];
    if (g->turn < HISTORY_LEN) {
        g->histPop[g->turn] = Soc_NationPop(g, N_FRANCE);
        g->histCohesion[g->turn] = g->res[R_COHESION];
        g->histTemp[g->turn] = g->temp;
        g->histScore[g->turn] = (float)Game_Score(g, NULL);
        for (int n = 0; n < N_COUNT; n++) {
            g->histNatPop[n][g->turn] = Soc_NationPop(g, n);
            g->histNatArmy[n][g->turn] = g->nat[n].army;
            g->histNatRel[n][g->turn] = g->nat[n].relation;
        }
    }
    g->reportReady = true;

    /* 7. fin de partie */
    if (g->reportLogStart > g->logCount) g->reportLogStart = 0;
    if (g->state != GAME_RUNNING) return;
    if (g->prov[PV_FR_IDF].ctrl != N_FRANCE) g->state = GAME_LOST_CONQUEST;
    else if (g->res[R_COHESION] < 1)          g->state = GAME_LOST_COHESION;
    else if (g->res[R_SOUVERAINETE] < 1)      g->state = GAME_LOST_SOUVERAINETE;
    else if (g->turn >= TOTAL_TURNS)          g->state = GAME_WON;
    if (g->state != GAME_RUNNING) return;

    char date[32];
    Game_DateString(g->turn, date, sizeof(date));
    Game_Log(g, EV_INFO, "--- %s : %d menaces actives, +%.2f°C, %.1f M d'habitants ---",
             date, Game_ActiveThreats(g), g->temp, Soc_NationPop(g, N_FRANCE));

    /* 8. dilemme aleatoire (si aucun dilemme dynamique n'est deja en attente) */
    if (g->pendingDilemma < 0 && g->turn >= 3 && Randf(g) < 0.13f) {
        int avail[32], n = 0;
        for (int d = 0; d < DILEMMA_COUNT; d++) {
            if (g->dilemmaUsed[d]) continue;
            if (d == 12 && g->nat[N_FRANCE].regime >= RG_JUNTE) continue;
            if (d == 13 && g->nat[N_FRANCE].regime == RG_UTOPIE) continue;
            if (d == 7 && !g->nat[N_BOREALE].alive) continue;
            avail[n++] = d;
        }
        if (n > 0) {
            int d = avail[Rand(g) % (unsigned)n];
            g->dilemmaUsed[d] = true;
            g->pendingDilemma = d;
        }
    }
    /* 9. evenement contextuel */
    if (g->pendingDilemma < 0 && g->turn >= 2 && Randf(g) < 0.16f) Ev_Trigger(g);
}

/* ------------------------------------------------------------------ */
/* Score et victoire                                                   */
/* ------------------------------------------------------------------ */

VictoryType Game_Victory(const Game *g)
{
    if (g->state != GAME_WON && g->state != GAME_RUNNING) return VIC_NONE;
    float total = 0;
    for (int p = 0; p < PROV_COUNT; p++) total += g->prov[p].pop;
    int provs = Soc_NationProvinces(g, N_FRANCE);
    bool projects = true;
    for (int p = 0; p < P_COUNT; p++) if (g->proj[p].state != 2) projects = false;
    Regime r = g->nat[N_FRANCE].regime;
    if (r == RG_UTOPIE && g->res[R_COHESION] >= 60) return VIC_UTOPIE;
    if (Soc_NationPop(g, N_FRANCE) >= total * 0.25f) return VIC_HEGEMONIE;
    if (projects && g->res[R_ENERGIE] >= 55 && g->res[R_EAU] >= 55 && g->res[R_ALIM] >= 55) return VIC_TRANSITION;
    if (r >= RG_JUNTE && provs >= 15) return VIC_ORDRE;
    return VIC_SURVIE;
}

int Game_Score(const Game *g, int parts[SC_COUNT])
{
    float sc[SC_COUNT] = { 0 };
    float popCore = 0, popOther = 0, prosp = 0, lit = 0, wsum = 0;
    int provs = 0, integrated = 0, revolts = 0, done = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE || pr->ctrl != N_FRANCE) continue;
        if (pr->core == N_FRANCE) popCore += pr->pop; else popOther += pr->pop;
        prosp += pr->prosperity * pr->pop;
        lit += pr->literacy * pr->pop;
        wsum += pr->pop;
        if (PROVINCES[p].nation != N_FRANCE) { provs++; if (pr->core == N_FRANCE) integrated++; }
        if (pr->revolt) revolts++;
    }
    for (int p = 0; p < P_COUNT; p++) if (g->proj[p].state == 2) done++;
    int capa = 0;
    for (int d = 0; d < D_COUNT; d++) capa += g->capacity[d];

    sc[SC_SURVIE]      = g->turn * 4.0f;
    sc[SC_POPULATION]  = popCore * 2 + popOther;
    sc[SC_PROSPERITE]  = wsum > 0 ? prosp / wsum * 150 + lit / wsum * 100 : 0;
    sc[SC_TERRITOIRE]  = provs * 25.0f + integrated * 15.0f;
    sc[SC_STABILITE]   = g->res[R_COHESION] * 3 + g->res[R_SOUVERAINETE] * 2 - revolts * 20;
    sc[SC_RESILIENCE]  = g->res[R_ENERGIE] + g->res[R_EAU] + g->res[R_ALIM] + done * 20
                       + (g->res[R_BUDGET] > 0 ? (g->res[R_BUDGET] < 200 ? g->res[R_BUDGET] : 200) * 0.5f : g->res[R_BUDGET]);
    sc[SC_PUISSANCE]   = g->nat[N_FRANCE].army * 8 + g->credibility + capa * 2;
    sc[SC_REPUTATION]  = g->res[R_INFLUENCE] * 2 - g->aggressionMemory * 2 + (g->nat[N_FRANCE].eu ? 50 : 0);
    sc[SC_PENALITES]   = -g->nukesUsed * 500.0f - g->tacticalUsed * 120.0f - g->statLost * 30.0f
                       - (Eco_DebtRatio(g) > 2.6f ? (Eco_DebtRatio(g) - 2.6f) * 40 : 0) - g->bailouts * 150.0f;

    float total = 0;
    for (int i = 0; i < SC_COUNT; i++) {
        if (parts) parts[i] = (int)sc[i];
        total += sc[i];
    }
    static const float diff[DIFF_COUNT] = { 0.8f, 1.0f, 1.3f };
    total *= diff[g->difficulty];
    if (g->state == GAME_WON) {
        static const float bonus[VIC_COUNT] = { 1, 1, 1.25f, 1.25f, 1.3f, 1.15f };
        total *= bonus[Game_Victory(g)];
    }
    return total < 0 ? 0 : (int)total;
}

/* ------------------------------------------------------------------ */
/* Sauvegarde                                                          */
/* ------------------------------------------------------------------ */

/*
 * Format champ par champ : en-tete, puis un enregistrement par entree de
 * SAVE_FIELDS (nom, nombre d'elements, taille d'un element, donnees). Au
 * chargement, l'etat part des valeurs par defaut de Game_Init ; un champ
 * inconnu ou dont la taille a change est ignore, un champ absent garde sa
 * valeur par defaut. Les sauvegardes restent ainsi lisibles d'une version a
 * l'autre tant qu'un champ garde son nom et son type.
 */
#define SAVE_FORMAT 2

static bool PutU32(FILE *f, unsigned v)
{
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    return fwrite(b, 4, 1, f) == 1;
}

static bool GetU32(FILE *f, unsigned *v)
{
    unsigned char b[4];
    if (fread(b, 4, 1, f) != 1) return false;
    *v = (unsigned)b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24;
    return true;
}

bool Game_Save(const Game *g, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(SAVE_MAGIC, 8, 1, f) == 1 && PutU32(f, SAVE_FORMAT) && PutU32(f, SAVE_VERSION)
           && PutU32(f, (unsigned)g->difficulty) && PutU32(f, (unsigned)SAVE_FIELD_COUNT);
    const unsigned char *base = (const unsigned char *)g;
    for (int i = 0; i < SAVE_FIELD_COUNT && ok; i++) {
        const SaveField *sf = &SAVE_FIELDS[i];
        unsigned len = (unsigned)strlen(sf->name);
        ok = PutU32(f, len) && fwrite(sf->name, len, 1, f) == 1 && PutU32(f, (unsigned)sf->count) && PutU32(f, (unsigned)sf->size);
        for (size_t k = 0; k < sf->count && ok; k++)
            ok = fwrite(base + sf->base + k * sf->stride + sf->offset, sf->size, 1, f) == 1;
    }
    fclose(f);
    return ok;
}

static const SaveField *FindField(const char *name)
{
    for (int i = 0; i < SAVE_FIELD_COUNT; i++) if (strcmp(SAVE_FIELDS[i].name, name) == 0) return &SAVE_FIELDS[i];
    return NULL;
}

bool Game_Load(Game *g, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static Game tmp;
    char magic[8];
    unsigned format = 0, version = 0, difficulty = 0, fields = 0;
    bool ok = fread(magic, 8, 1, f) == 1 && memcmp(magic, SAVE_MAGIC, 8) == 0 && GetU32(f, &format) && format == SAVE_FORMAT
           && GetU32(f, &version) && GetU32(f, &difficulty) && GetU32(f, &fields) && difficulty < DIFF_COUNT;
    if (ok) Game_Init(&tmp, 1, (int)difficulty);
    unsigned char *base = (unsigned char *)&tmp;
    unsigned char buf[4096];
    for (unsigned i = 0; i < fields && ok; i++) {
        unsigned len, count, size;
        char name[64];
        ok = GetU32(f, &len) && len < sizeof(name) && fread(name, len, 1, f) == 1 && GetU32(f, &count) && GetU32(f, &size)
          && size <= sizeof(buf);
        if (!ok) break;
        name[len] = 0;
        const SaveField *sf = FindField(name);
        bool use = sf && sf->size == size;
        for (unsigned k = 0; k < count && ok; k++) {
            ok = fread(buf, size, 1, f) == 1;
            if (ok && use && k < sf->count) memcpy(base + sf->base + k * sf->stride + sf->offset, buf, size);
        }
    }
    fclose(f);
    if (ok) {
        *g = tmp;
        War_UpdateSupply(g);
        War_UpdateIntel(g);
    }
    return ok;
}
