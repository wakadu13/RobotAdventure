/*
 * BOUCLIER 2050 - ressources strategiques : terres rares, uranium et
 * semi-conducteurs. Gisements et fonderies, importations (partenaires et
 * marche hors carte), crises d'approvisionnement, consommation par l'armee,
 * la dissuasion, l'energie nucleaire, la recherche et l'industrie.
 */
#include "game_internal.h"
#include "lang.h"

#include <math.h>
#include <stdio.h>

/* importations de base depuis le reste du monde (hors carte), pour la France */
static const float WORLD_SUPPLY[SR_COUNT] = { 1.0f, 1.2f, 1.2f };
/* stock maximal : quelques annees de consommation */
#define STRAT_CAP 60

float Strat_Deposit(int p, int r)
{
    float v = 0;
    for (int i = 0; i < DEPOSIT_COUNT; i++)
        if (DEPOSITS[i].prov == p && DEPOSITS[i].res == r) v += DEPOSITS[i].amount;
    return v;
}

static float ProvinceStrat(const Game *g, int p, int r)
{
    const Province *pr = &g->prov[p];
    float eff = ProvinceEfficiency(pr);
    if (r == SR_PUCES) {
        /* les fonderies ont besoin d'energie */
        float energy = fminf(1, g->eco[pr->owner].sat[G_ENERGY]);
        float out = Strat_Deposit(p, r) + pr->fabs;
        if (g->nat[pr->owner].outage > 0) energy *= 0.6f;
        return out * eff * (0.4f + 0.6f * energy);
    }
    if (pr->mineClosed) return 0;
    return Strat_Deposit(p, r) * eff;
}

float Strat_Production(const Game *g, int n, int r)
{
    float v = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) v += ProvinceStrat(g, p, r);
    return v;
}

static float WorldShare(const Game *g, int r)
{
    return g->stratShock[r] > 0 ? 0.3f : 1.0f;
}

/* taille economique relative d'une nation (la France vaut 1) */
static float Weight(const Game *g, int n)
{
    float w = 0, fr = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner == n) w += g->prov[p].pop * PROVINCES[p].wealth;
        if (g->prov[p].owner == N_FRANCE) fr += g->prov[p].pop * PROVINCES[p].wealth;
    }
    return fr > 0 ? w / fr : 0;
}

float Strat_Imports(const Game *g, int n, int r)
{
    const Nation *na = &g->nat[n];
    float flow = (1 - 0.7f * na->blockade) * (1 - na->raid);
    float v = WORLD_SUPPLY[r] * WorldShare(g, r) * (n == N_FRANCE ? 1.0f : Weight(g, n) * 0.8f);
    if (na->sanctions > 0) v *= 0.3f;   /* sanctions internationales */
    if (n == N_FRANCE) {
        /* nos partenaires nous cedent une part de leur production */
        for (int k = 1; k < N_COUNT; k++) {
            if (!Eco_TradeAllowed(g, k)) continue;
            float share = g->nat[k].trade ? 0.35f : (g->nat[k].eu && na->eu) ? 0.2f : 0.05f;
            if (g->ally[N_FRANCE][k]) share += 0.1f;
            v += Strat_Production(g, k, r) * share;
        }
    } else {
        /* les IA echangent avec leurs allies et leurs partenaires europeens */
        for (int k = 1; k < N_COUNT; k++) {
            if (k == n || !g->nat[k].alive || War_AtWar(g, n, k) || g->nat[k].sanctions > 0) continue;
            float share = g->ally[n][k] ? 0.2f : (g->nat[k].eu && na->eu) ? 0.12f : Relation(g, n, k) > 40 ? 0.05f : 0;
            if (share > 0) v += Strat_Production(g, k, r) * share;
        }
    }
    return v * flow;
}

/* consommation courante (hors recrutement) */
float Strat_Use(const Game *g, int n, int r)
{
    float u = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *un = &g->units[i];
        if (!un->active || un->owner != n) continue;
        if (r == SR_URANIUM && (un->type == U_SNA || un->type == U_PA)) u += 0.1f;
        else if (UNIT_STRAT[un->type][r]) u += 0.02f * UNIT_STRAT[un->type][r];
    }
    if (n == N_FRANCE) {
        if (r == SR_URANIUM) {
            static const float posture[3] = { 0.3f, 0.6f, 1.0f };
            u += posture[g->nukePosture];
            if (g->proj[P_SMR].state == 2) u += 1.0f;
        }
        if (r == SR_TERRES_RARES && g->proj[P_ENR].state == 2) u += 0.5f;
        if (r == SR_PUCES) {
            float ind = 0;
            for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == N_FRANCE) ind += g->prov[p].industry;
            u += ind * 0.02f + g->researchBudget * 0.1f;
        }
    } else {
        if (r == SR_URANIUM && Nuke_IsPower(g, n)) u += 0.4f;
        if (r == SR_URANIUM && g->nat[n].nukeStatus == 1) u += 0.3f;
        if (r == SR_PUCES) u += Weight(g, n) * 0.3f;
    }
    return u;
}

/* satisfaction des besoins courants (0..1) */
float Strat_Sat(const Game *g, int n, int r)
{
    float use = Strat_Use(g, n, r);
    if (use <= 0.01f) return 1;
    float avail = g->nat[n].strat[r] + Strat_Production(g, n, r) + Strat_Imports(g, n, r);
    return Clampf(avail / use, 0, 1);
}

float Strat_Price(const Game *g, int r)
{
    return STRATS[r].price * (g->stratShock[r] > 0 ? 2.5f : 1.0f);
}

const char *Strat_BuyBlocked(const Game *g, int r)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->nat[N_FRANCE].strat[r] >= STRAT_CAP - 2) return "Réserves pleines";
    if (g->res[R_BUDGET] < Strat_Price(g, r) * 2) return "Budget insuffisant";
    if (g->nat[N_FRANCE].blockade > 0.7f) return "Nos côtes sont sous blocus";
    return NULL;
}

bool Strat_Buy(Game *g, int r)
{
    if (Strat_BuyBlocked(g, r)) return false;
    g->res[R_BUDGET] -= Strat_Price(g, r) * 2;
    g->nat[N_FRANCE].strat[r] += 2;
    return true;
}

const char *Strat_FabBlocked(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->owner != N_FRANCE || pr->ctrl != N_FRANCE) return "Province étrangère ou occupée";
    if (pr->fabBuild > 0) return "Fonderie déjà en construction";
    if (pr->industry < 3) return "Tissu industriel insuffisant (3 usines)";
    if (pr->fabs >= 1.5f) return "Capacité maximale atteinte";
    if (g->res[R_BUDGET] < 15) return "Budget insuffisant (15 Md)";
    return NULL;
}

bool Strat_BuildFab(Game *g, int p)
{
    if (Strat_FabBlocked(g, p)) return false;
    g->res[R_BUDGET] -= 15;
    g->prov[p].fabBuild = 6;
    Game_Log(g, EV_INFO, "Construction d'une fonderie de semi-conducteurs en %s (6 tours).", (PROVINCE_NAMES[p]));
    return true;
}

const char *Strat_MineBlocked(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->owner != N_FRANCE || pr->ctrl != N_FRANCE) return "Province étrangère ou occupée";
    if (!pr->mineClosed) return "Aucun gisement fermé";
    if (g->res[R_BUDGET] < 10) return "Budget insuffisant (10 Md)";
    return NULL;
}

bool Strat_OpenMine(Game *g, int p)
{
    if (Strat_MineBlocked(g, p)) return false;
    g->res[R_BUDGET] -= 10;
    g->prov[p].mineClosed = false;
    /* une mine n'est jamais bien accueillie */
    for (int s = 0; s < S_COUNT; s++) g->prov[p].mil[s] = Clampf(g->prov[p].mil[s] + 0.8f, 0, 10);
    AddRes(g, R_SOUVERAINETE, 2);
    Game_Log(g, EV_GOOD, "Réouverture des mines de %s : souveraineté accrue, riverains mécontents.", (PROVINCE_NAMES[p]));
    return true;
}

const char *Strat_RecruitBlocked(const Game *g, int n, int type)
{
    static char buf[96];
    for (int r = 0; r < SR_COUNT; r++) {
        if (UNIT_STRAT[type][r] && g->nat[n].strat[r] < UNIT_STRAT[type][r]) {
            snprintf(buf, sizeof(buf), Tr("%s insuffisant(e)s (%d requis)"), Tr(STRATS[r].name), UNIT_STRAT[type][r]);
            return buf;
        }
    }
    return NULL;
}

void Strat_PayRecruit(Game *g, int n, int type)
{
    for (int r = 0; r < SR_COUNT; r++) g->nat[n].strat[r] = fmaxf(0, g->nat[n].strat[r] - UNIT_STRAT[type][r]);
}

void Strat_Init(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE && Strat_Deposit(p, SR_URANIUM) > 0) g->prov[p].mineClosed = true;
    static const float fr[SR_COUNT] = { 8, 14, 8 };
    for (int n = 0; n < N_COUNT; n++)
        for (int r = 0; r < SR_COUNT; r++)
            g->nat[n].strat[r] = n == N_FRANCE ? fr[r] : 4 * Weight(g, n) + Strat_Production(g, n, r) * 4;
}

/* crises mondiales d'approvisionnement */
static void Shocks(Game *g)
{
    static const char *start[SR_COUNT] = {
        "La Chine suspend ses exportations de terres rares : les cours s'envolent.",
        "Coups d'État au Sahel et en Asie centrale : l'uranium se raréfie.",
        "Blocus du détroit de Taïwan : la production mondiale de puces s'effondre.",
    };
    static const int length[SR_COUNT] = { 8, 6, 10 };
    for (int r = 0; r < SR_COUNT; r++) {
        if (g->stratShock[r] > 0) {
            if (--g->stratShock[r] == 0) Game_Log(g, EV_INFO, "Fin de la crise des %s.", (STRATS[r].name));
            continue;
        }
        if (g->turn > 6 && Randf(g) < 0.012f) {
            g->stratShock[r] = length[r];
            Game_Log(g, EV_EVENT, "%s", start[r]);
        }
    }
}

void Strat_Turn(Game *g)
{
    Shocks(g);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->fabBuild > 0 && --pr->fabBuild == 0) {
            if (pr->owner == N_FRANCE) {
                pr->fabs += 0.6f;
                Game_Log(g, EV_GOOD, "Nouvelle fonderie de semi-conducteurs en service en %s.", (PROVINCE_NAMES[p]));
            }
        }
    }
    for (int n = 0; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;
        for (int r = 0; r < SR_COUNT; r++) {
            float v = na->strat[r] + Strat_Production(g, n, r) + Strat_Imports(g, n, r) - Strat_Use(g, n, r);
            na->strat[r] = Clampf(v, 0, STRAT_CAP);
            /* les IA achetent sur le marche quand leurs reserves baissent */
            if (n != N_FRANCE && na->strat[r] < 3 && na->sanctions == 0 && na->treasury > Strat_Price(g, r) * 3) {
                na->treasury -= Strat_Price(g, r) * 2;
                na->strat[r] += 2;
            }
        }
    }
    /* alertes francaises */
    for (int r = 0; r < SR_COUNT; r++)
        if (Strat_Sat(g, N_FRANCE, r) < 0.99f && g->nat[N_FRANCE].strat[r] < 0.5f)
            Game_Log(g, EV_BAD, "Pénurie de %s : les besoins ne sont plus couverts.", (STRATS[r].name));
}
