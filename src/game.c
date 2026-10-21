/*
 * BOUCLIER 2050 - coeur de la simulation : initialisation, guerre hybride
 * (theatres M2MC, menaces, factions), projets, dilemmes, enchainement du tour.
 */
#include "game_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static void SpawnThreatFor(Game *g, int f);

void Game_Log(Game *g, LogKind kind, const char *fmt, ...)
{
    if (g->logCount == MAX_LOG) {
        memmove(&g->log[0], &g->log[1], sizeof(LogEntry) * (MAX_LOG - 1));
        g->logCount--;
    }
    LogEntry *e = &g->log[g->logCount++];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, LOG_LEN, fmt, ap);
    va_end(ap);
    e->turn = g->turn;
    e->kind = kind;
}

void Game_DateString(int turn, char *buf, int len)
{
    snprintf(buf, len, "T%d %d", turn % 4 + 1, START_YEAR + turn / 4);
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

void Game_Init(Game *g, unsigned seed)
{
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 12345u;
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
    static const int post[TH_COUNT] = { 2, 3, 2, 2, 3, 3, 2, 3, 1, 2, 1, 1 };
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
        na->warTarget = -1;
    }
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        pr->owner = pr->core = PROVINCES[p].nation;
        pr->pop = PROVINCES[p].pop;
        pr->integration = 100;
        pr->prosperity = 1;
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = 2.0f;
        pr->hab = Soc_Habitability(g, p);
        g->nat[pr->owner].startProvinces++;
    }
    for (int n = 0; n < N_COUNT; n++) g->nat[n].army = War_ArmyMax(g, n) * 0.8f;
    g->nat[N_FRANCE].army = 5;

    g->migrationDoctrine = 1;
    g->alertLevel = 0;
    g->commandPoints = BASE_CP;
    g->pendingDilemma = -1;
    g->state = GAME_RUNNING;

    Soc_UpdateResources(g);
    g->histPop[0] = Soc_NationPop(g, N_FRANCE);
    g->histCohesion[0] = g->res[R_COHESION];
    g->histTemp[0] = g->temp;

    Game_Log(g, EV_EVENT, "Janvier 2030. +1,5°C. Le Président vous confie le Conseil de défense et de sécurité.");
    Game_Log(g, EV_INFO, "Gérez la population, protégez les frontières dans tous les milieux et tenez jusqu'en 2060.");

    for (int i = 0; i < 2; i++) SpawnThreatFor(g, Rand(g) % F_COUNT);
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
    Game_Log(g, EV_INFO, "Montée en puissance %s lancée (+2 capacité dans 2 tours).", DOMAIN_NAMES[d]);
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
    const char *fn = FACTIONS[f].name;

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
    return PROJECTS[p].cost * REGIMES[g->nat[N_FRANCE].regime].projectCost;
}

bool Game_StartProject(Game *g, int p)
{
    float c = Game_ProjectCost(g, p);
    if (g->proj[p].state != 0 || g->res[R_BUDGET] < c) return false;
    g->res[R_BUDGET] -= c;
    g->proj[p].state = 1;
    g->proj[p].turnsLeft = PROJECTS[p].turns;
    Game_Log(g, EV_INFO, "Lancement du projet : %s (%d tours).", PROJECTS[p].name, PROJECTS[p].turns);
    return true;
}

void Game_SetMigrationDoctrine(Game *g, int d)
{
    if (d == g->migrationDoctrine) return;
    g->migrationDoctrine = d;
    AddRes(g, R_COHESION, -2);
    Game_Log(g, EV_INFO, "Nouvelle doctrine migratoire : %s.", MIGRATION_DOCTRINES[d]);
}

void Game_SetAlertLevel(Game *g, int a)
{
    if (a == g->alertLevel) return;
    g->alertLevel = a;
    Game_Log(g, EV_INFO, "Niveau d'alerte : %s.", ALERT_LEVELS[a]);
}

/* ------------------------------------------------------------------ */
/* Dilemmes                                                            */
/* ------------------------------------------------------------------ */

const DilemmaDef *Game_CurrentDilemma(const Game *g)
{
    int d = g->pendingDilemma;
    if (d < 0) return NULL;
    if (d >= DLM_ALLY_CALL) return &DYN_DILEMMAS[d - DLM_ALLY_CALL];
    return &DILEMMAS[d];
}

static void AddMilitancy(Game *g, int strata, float v)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++)
            if (strata < 0 || s == strata) g->prov[p].mil[s] = Clampf(g->prov[p].mil[s] + v, 0, 10);
    }
}

static void AddImmigrants(Game *g, float millions)
{
    float total = Soc_NationPop(g, N_FRANCE);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE) continue;
        float m = millions * pr->pop / total;
        pr->foreign = (pr->foreign * pr->pop + m) / (pr->pop + m);
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
             else   { AddRes(g, R_COHESION, 3); g->nat[N_FRANCE].army = g->nat[N_FRANCE].army > 1 ? g->nat[N_FRANCE].army - 1 : 0; } break;
    case 13: if (A) { Soc_SetRegime(g, RG_UTOPIE); AddMilitancy(g, S_ELITES, 3); g->regimeCooldown = 12; }
             else   { AddMilitancy(g, S_MOYENNES, 2); AddRes(g, R_COHESION, -4); } break;
    case 14: if (A) { AddRes(g, R_BUDGET, -10); AddRes(g, R_SOUVERAINETE, 6); AddRes(g, R_EAU, 4); AddFac(g, F_HYDRA, 15, 0); }
             else   { AddMilitancy(g, S_TRAVAILLEURS, 1); } break;
    case 15: if (A) { AddImmigrants(g, 1.0f); AddRes(g, R_INFLUENCE, 8); AddRes(g, R_COHESION, -4);
                      for (int n = N_MAROC; n <= N_EGYPTE; n++) AddRelation(g, n, 10); }
             else   { AddRes(g, R_INFLUENCE, -8); AddFac(g, F_SABLES, 10, 0);
                      for (int i = 0; i < 3; i++) SpawnThreatFor(g, F_SABLES); } break;
    case DLM_ALLY_CALL:
        if (A) {
            AddRelation(g, g->nat[prm].warTarget, 30);
            AddRes(g, R_INFLUENCE, 5);
            g->nat[prm].atWar = true;
            g->nat[prm].warTurns = 0;
            AddRelation(g, prm, -40);
            Game_Log(g, EV_WAR, "La France entre en guerre contre %s pour défendre %s.",
                     NATIONS[prm].name, NATIONS[g->nat[prm].warTarget].name);
        } else {
            AddRes(g, R_BUDGET, -6);
            AddRes(g, R_INFLUENCE, -4);
        }
        break;
    case DLM_PEACE_OFFER:
        if (A) War_MakePeace(g, prm, true);
        else Game_Log(g, EV_WAR, "La France rejette l'offre de paix de %s.", NATIONS[prm].name);
        break;
    }
    if (d < DLM_ALLY_CALL)
        Game_Log(g, EV_EVENT, "%s : %s.", dd->title, A ? dd->optA : dd->optB);
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
        return i;
    }
    return -1;
}

static void SpawnThreatFor(Game *g, int f)
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

static void ResolveThreats(Game *g)
{
    for (int t = 0; t < TH_COUNT; t++) g->tension[t] = 0;

    for (int i = 0; i < MAX_THREATS; i++) {
        Threat *t = &g->threats[i];
        if (!t->active) continue;
        t->fresh = false;
        const ThreatDef *td = &THREATS[t->type];

        if (!t->detected && Randf(g) < DetectChance(g, t)) t->detected = true;

        if (Randf(g) < Game_NeutralizeChance(g, t)) {
            t->active = false;
            g->statNeutralized++;
            AddRes(g, R_SOUVERAINETE, 0.4f);
            Game_Log(g, EV_GOOD, "%s neutralisé(e) - %s.", td->name, THEATERS[t->theater].name);
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
                     THEATERS[t->theater].name);
        }
        g->tension[t->theater] += dmg;
        g->statSuffered++;

        if (--t->turnsLeft <= 0) {
            t->active = false;
            Game_Log(g, EV_BAD, "%s (%s) a frappé sans être stoppé(e).", td->name, THEATERS[t->theater].name);
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

    float escalation = 1.0f + 1.4f * g->turn / (float)TOTAL_TURNS;
    for (int f = 0; f < F_COUNT; f++) {
        float h = g->fac[f].hostility / 100.0f, c = g->fac[f].capability / 100.0f;
        float expected = (0.08f + 1.1f * h * c) * escalation;
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
    g->temp += 0.011f + 0.00008f * g->turn - g->climateMitigation * 0.03f;

    float chance = 0.12f + (g->temp - 1.2f) * 0.14f - g->climateMitigation;
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

float Game_Upkeep(const Game *g)
{
    float u = 0;
    for (int t = 0; t < TH_COUNT; t++) u += g->posture[t] * 0.6f;
    for (int d = 0; d < D_COUNT; d++) u += g->capacity[d] * 0.1f;
    static const float alert[3] = { 0, 3, 7 };
    u += alert[g->alertLevel];
    if (g->migrationDoctrine == 2) u += 1.5f;
    u += War_ArmyUpkeep(g);
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

    /* 1. economie */
    g->res[R_BUDGET] += Soc_Income(g) - Game_Upkeep(g);
    if (g->res[R_BUDGET] < 0) {
        g->res[R_BUDGET] *= 1.03f;
        AddRes(g, R_COHESION, -0.5f);
    }
    if (g->migrationDoctrine == 0) AddRes(g, R_INFLUENCE, 1.0f);
    if (g->migrationDoctrine == 2) AddRes(g, R_INFLUENCE, -1.5f);
    if (g->alertLevel == 1) AddRes(g, R_COHESION, -0.4f);
    if (g->alertLevel == 2) AddRes(g, R_COHESION, -1.2f);

    /* 2. projets et montees en puissance */
    for (int p = 0; p < P_COUNT; p++) {
        if (g->proj[p].state == 1 && --g->proj[p].turnsLeft <= 0) {
            g->proj[p].state = 2;
            Game_Log(g, EV_GOOD, "Projet opérationnel : %s.", PROJECTS[p].name);
        }
    }
    for (int d = 0; d < D_COUNT; d++) {
        if (g->pendingTurns[d] > 0 && --g->pendingTurns[d] == 0) {
            g->capacity[d] += g->pendingCap[d];
            g->pendingCap[d] = 0;
            Game_Log(g, EV_GOOD, "Nouvelles capacités %s disponibles.", DOMAIN_NAMES[d]);
        }
    }

    /* 3. menaces hybrides */
    ResolveThreats(g);

    /* 4. climat, population, migrations, ressources */
    ClimateTurn(g);
    Soc_Turn(g);

    /* 5. guerres et diplomatie */
    War_Turn(g);

    /* 6. factions et nouvelles menaces */
    FactionsTurn(g);

    if (g->regimeCooldown > 0) g->regimeCooldown--;
    g->commandPoints = BASE_CP;
    g->divisionsUsed = 0;
    for (int n = 0; n < N_COUNT; n++) g->nat[n].actedThisTurn = false;
    g->turn++;

    for (int r = 0; r < R_COUNT; r++) g->lastDelta[r] = g->res[r] - before[r];
    if (g->turn < HISTORY_LEN) {
        g->histPop[g->turn] = Soc_NationPop(g, N_FRANCE);
        g->histCohesion[g->turn] = g->res[R_COHESION];
        g->histTemp[g->turn] = g->temp;
    }

    /* 7. fin de partie */
    if (g->state != GAME_RUNNING) return;
    if (g->prov[PV_FR_NORD].owner != N_FRANCE) g->state = GAME_LOST_CONQUEST;
    else if (g->res[R_COHESION] < 1)          g->state = GAME_LOST_COHESION;
    else if (g->res[R_SOUVERAINETE] < 1)      g->state = GAME_LOST_SOUVERAINETE;
    else if (g->res[R_BUDGET] < -60)          g->state = GAME_LOST_BUDGET;
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
}

int Game_Score(const Game *g)
{
    float s = g->res[R_COHESION] * 2 + g->res[R_SOUVERAINETE] * 2 + g->res[R_INFLUENCE]
            + g->res[R_ENERGIE] + g->res[R_EAU] + g->res[R_ALIM]
            + g->res[R_BUDGET] * 0.5f + g->statNeutralized * 2 + g->turn * 4
            + Soc_NationPop(g, N_FRANCE) * 2 + Soc_NationProvinces(g, N_FRANCE) * 15;
    return s < 0 ? 0 : (int)s;
}
