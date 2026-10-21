/*
 * BOUCLIER 2050 - economie inspiree de Victoria : production des provinces,
 * marche mondial et prix, besoins des strates, impots par strate, depenses
 * publiques, droits de douane, usines.
 */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>

/* poids fiscal de chaque strate (un membre des elites paie bien plus qu'un travailleur) */
static const float TAX_WEIGHT[S_COUNT] = { 0.6f, 1.4f, 6.0f };
/* besoins en biens industriels par million d'habitants de chaque strate */
static const float GOODS_NEED[S_COUNT] = { 0.25f, 0.8f, 4.0f };
/* part des echanges captee par l'Etat (TVA, taxes, subventions) */
#define TRADE_STATE_SHARE 0.35f

/* ------------------------------------------------------------------ */
/* Production et besoins                                               */
/* ------------------------------------------------------------------ */

static float RawSupply(const Game *g, int p, int good)
{
    const Province *pr = &g->prov[p];
    const ProvinceDef *d = &PROVINCES[p];
    float e = ProvinceEfficiency(pr);
    switch (good) {
    case G_FOOD:     return d->food * pr->hab / 100.0f * e;
    case G_WATER:    return d->water * pr->hab / 100.0f * e;
    case G_ENERGY:   return d->energy * (0.8f + 0.2f * pr->hab / 100.0f) * e;
    case G_MINERALS: return PROVINCE_MINERALS[p] * e;
    default: {
        /* les usines tournent selon l'energie et les minerais disponibles */
        const NationEconomy *ne = &g->eco[pr->owner];
        float input = fminf(1, fminf(ne->sat[G_ENERGY], ne->sat[G_MINERALS]));
        return pr->industry * 2.0f * e * (0.3f + 0.7f * input);
    }
    }
}

/* calibrage : en 2030 l'offre mondiale couvre environ 105% des besoins */
static const float PRODUCTIVITY[G_COUNT] = { 1.33f, 1.2f, 1.33f, 1.21f, 1.6f };

static float ProvinceSupply(const Game *g, int p, int good)
{
    return RawSupply(g, p, good) * PRODUCTIVITY[good];
}

static float GoodsNeed(const Province *pr)
{
    float n = 0;
    for (int s = 0; s < S_COUNT; s++) n += pr->share[s] * GOODS_NEED[s];
    return n;
}

static void NationSupplyDemand(const Game *g, int n, NationEconomy *ne)
{
    for (int k = 0; k < G_COUNT; k++) ne->supply[k] = ne->demand[k] = 0;
    bool sober = n == N_FRANCE && g->sobrietyTurns > 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != n) continue;
        for (int k = 0; k < G_COUNT; k++) ne->supply[k] += ProvinceSupply(g, p, k);
        float w = pr->pop * NeedWeight(pr);
        ne->demand[G_FOOD]     += w;
        ne->demand[G_WATER]    += w;
        ne->demand[G_ENERGY]   += w * (sober ? 0.75f : 1.0f) + pr->industry * 0.5f;
        ne->demand[G_MINERALS] += pr->industry;
        ne->demand[G_GOODS]    += w * GoodsNeed(pr) * (sober ? 0.75f : 1.0f);
    }
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (u->active && u->owner == n) ne->demand[G_GOODS] += UNIT_TYPES[u->type].goods;
    }
    if (n == N_FRANCE) {
        if (g->proj[P_AGRO].state == 2)   ne->supply[G_FOOD] += 8;
        if (g->proj[P_DESSAL].state == 2) ne->supply[G_WATER] += 9;
        if (g->proj[P_SMR].state == 2)    ne->supply[G_ENERGY] += 12;
        if (g->proj[P_ENR].state == 2)    ne->supply[G_ENERGY] += 7;
        ne->supply[G_ENERGY] += g->importBonus;
    }
}

/* ------------------------------------------------------------------ */
/* Marche mondial                                                      */
/* ------------------------------------------------------------------ */

static void WorldMarket(Game *g)
{
    for (int n = 0; n < N_COUNT; n++) {
        NationEconomy *ne = &g->eco[n];
        if (g->nat[n].alive) NationSupplyDemand(g, n, ne);
        for (int k = 0; k < G_COUNT; k++) ne->imports[k] = ne->exports[k] = 0;
    }

    for (int k = 0; k < G_COUNT; k++) {
        if (k == G_WATER) { g->price[k] = 0; continue; }   /* l'eau ne s'exporte pas */
        float sumS = 0, sumD = 0, surplus = 0, deficit = 0;
        for (int n = 0; n < N_COUNT; n++) {
            if (!g->nat[n].alive) continue;
            const NationEconomy *ne = &g->eco[n];
            sumS += ne->supply[k];
            sumD += ne->demand[k];
            float b = ne->supply[k] - ne->demand[k];
            if (b > 0) surplus += b; else deficit -= b;
        }
        float ratio = sumS > 0 ? sumD / sumS : 2;
        g->price[k] = GOOD_BASE_PRICE[k] * Clampf(ratio * ratio * ratio, 0.4f, 3.5f);
        float fill = deficit > 0 ? fminf(1, surplus / deficit) : 1;
        float sell = surplus > 0 ? fminf(1, deficit / surplus) : 0;

        /* accords commerciaux : la France est servie en priorite par ses partenaires */
        float partner = 0;
        for (int n = 1; n < N_COUNT; n++) {
            const Nation *na = &g->nat[n];
            if (!na->trade || !na->alive || na->atWar) continue;
            float b = g->eco[n].supply[k] - g->eco[n].demand[k];
            if (b > 0) partner += b * 0.5f;
        }
        for (int n = 0; n < N_COUNT; n++) {
            if (!g->nat[n].alive) continue;
            NationEconomy *ne = &g->eco[n];
            float b = ne->supply[k] - ne->demand[k];
            if (b > 0) {
                ne->exports[k] = b * sell;
            } else {
                float f = fill;
                if (n == N_FRANCE && -b > 0) f = fminf(1, fill + partner / -b);
                ne->imports[k] = -b * f;
            }
        }
    }

    for (int n = 0; n < N_COUNT; n++) {
        NationEconomy *ne = &g->eco[n];
        for (int k = 0; k < G_COUNT; k++)
            ne->sat[k] = ne->demand[k] > 0 ? Clampf((ne->supply[k] + ne->imports[k]) / ne->demand[k], 0, 1.5f) : 1;
    }
}

float Eco_NeedSat(const Game *g, int n, int s)
{
    const NationEconomy *ne = &g->eco[n];
    float life = fminf(1, fminf(ne->sat[G_FOOD], ne->sat[G_WATER]));
    float every = fminf(1, ne->sat[G_ENERGY]);
    float lux = fminf(1, ne->sat[G_GOODS]);
    static const float W[S_COUNT][3] = { { 0.6f, 0.3f, 0.1f }, { 0.4f, 0.35f, 0.25f }, { 0.3f, 0.3f, 0.4f } };
    return life * W[s][0] + every * W[s][1] + lux * W[s][2];
}

/* ------------------------------------------------------------------ */
/* Fiscalite                                                           */
/* ------------------------------------------------------------------ */

static float TaxMultiplier(const Game *g, const Province *pr)
{
    float norm = 0, v = 0;
    for (int s = 0; s < S_COUNT; s++) {
        norm += STRATA_SHARE[s] * TAX_WEIGHT[s] * 0.3f;
        float rate = pr->owner == N_FRANCE ? g->taxRate[s] : 0.3f;
        v += pr->share[s] * TAX_WEIGHT[s] * rate;
    }
    return v / norm;
}

float Eco_ProvinceTax(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->revolt) return 0;
    float tax = REGIMES[g->nat[pr->owner].regime].taxEff;
    return pr->pop * PROVINCES[p].wealth * pr->prosperity * tax * (0.5f + 0.5f * pr->hab / 100.0f)
         * (1 - Soc_ProvinceMilitancy(pr) / 25.0f) * ProvinceEfficiency(pr) * TaxMultiplier(g, pr);
}

float Eco_SpendingCost(const Game *g, int sp)
{
    static const float perMillion[SP_COUNT] = { 0.025f, 0.02f, 0.04f };
    return Soc_NationPop(g, N_FRANCE) * perMillion[sp] * g->spending[sp];
}

static float TradeValue(const Game *g, bool imports)
{
    float v = 0;
    for (int k = 0; k < G_COUNT; k++)
        v += (imports ? g->eco[N_FRANCE].imports[k] : g->eco[N_FRANCE].exports[k]) * g->price[k];
    return v;
}

void Eco_Budget(const Game *g, float l[BL_COUNT])
{
    float taxes = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) taxes += Eco_ProvinceTax(g, p);
    taxes *= 1.1f * (0.9f + 0.2f * g->spending[SP_ADMIN]) * DiffIncome(g) * (0.7f + 0.005f * g->res[R_ENERGIE]);
    if (g->government == I_SOCIAL) taxes *= 0.95f;
    if (g->boostTurns > 0) taxes *= 1.1f;

    float imp = TradeValue(g, true);
    l[BL_TAXES]   = taxes;
    l[BL_TRADE]   = TradeValue(g, false) * TRADE_STATE_SHARE;
    l[BL_TARIFF]  = imp * g->tariff;
    l[BL_EU]      = g->nat[N_FRANCE].eu ? 2.0f : 0;
    l[BL_DEFENSE] = Game_DefenseUpkeep(g);
    l[BL_ARMY]    = War_ArmyUpkeep(g);
    l[BL_SPENDING] = 0;
    for (int s = 0; s < SP_COUNT; s++) l[BL_SPENDING] += Eco_SpendingCost(g, s);
    l[BL_NUKE]    = Nuke_Upkeep(g);
    l[BL_IMPORTS] = imp * TRADE_STATE_SHARE;
}

float Eco_Net(const Game *g)
{
    float l[BL_COUNT];
    Eco_Budget(g, l);
    float net = 0;
    for (int i = 0; i < BL_COUNT; i++) net += i < BL_DEFENSE ? l[i] : -l[i];
    return net;
}

void Eco_SetTax(Game *g, int s, float rate)
{
    float max = g->nat[N_FRANCE].regime == RG_UTOPIE ? 0.35f : 0.7f;
    g->taxRate[s] = Clampf(rate, 0.05f, max);
}

void Eco_SetSpending(Game *g, int sp, float level) { g->spending[sp] = Clampf(level, 0, 1); }
void Eco_SetTariff(Game *g, float t) { g->tariff = Clampf(t, 0, 0.5f); }

/* ------------------------------------------------------------------ */
/* Usines                                                              */
/* ------------------------------------------------------------------ */

float Eco_FactoryCost(const Game *g)
{
    float c = 8;
    if (g->nat[N_FRANCE].regime == RG_TECHNOCRATIE) c *= 0.75f;
    if (g->government == I_LIBERAL) c *= 0.9f;
    return c;
}

const char *Eco_FactoryBlocked(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->owner != N_FRANCE) return "Province étrangère";
    if (pr->revolt) return "Province en révolte";
    if (pr->factoryBuild > 0) return "Chantier déjà en cours";
    if (g->res[R_BUDGET] < Eco_FactoryCost(g)) return "Budget insuffisant";
    return NULL;
}

bool Eco_BuildFactory(Game *g, int p)
{
    if (Eco_FactoryBlocked(g, p)) return false;
    g->res[R_BUDGET] -= Eco_FactoryCost(g);
    g->prov[p].factoryBuild = 4;
    Game_Log(g, EV_INFO, "Construction d'une usine en %s (4 tours).", PROVINCE_NAMES[p]);
    return true;
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Eco_Turn(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->factoryBuild > 0 && --pr->factoryBuild == 0) {
            if (pr->owner == N_FRANCE) {
                pr->industry += 1;
                Game_Log(g, EV_GOOD, "Nouvelle usine en service en %s.", PROVINCE_NAMES[p]);
            }
        }
        /* industrialisation lente des pays stables */
        if (pr->owner != N_FRANCE && pr->industry > 0 && !g->nat[pr->owner].failed)
            pr->industry += 0.003f * pr->prosperity;
    }

    WorldMarket(g);

    /* indices nationaux de securite des approvisionnements */
    static const Resource map[3] = { R_ALIM, R_EAU, R_ENERGIE };
    static const int goods[3] = { G_FOOD, G_WATER, G_ENERGY };
    for (int r = 0; r < 3; r++) {
        float target = Clampf(55 + (g->eco[N_FRANCE].sat[goods[r]] - 1) * 100, 0, 100);
        g->res[map[r]] = Clampf(g->res[map[r]] + (target - g->res[map[r]]) * 0.3f, 0, 100);
    }

    /* les droits de douane protegent mais freinent la prosperite */
    if (g->tariff > 0)
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == N_FRANCE) g->prov[p].prosperity -= g->tariff * 0.004f;
}
