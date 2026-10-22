/*
 * BOUCLIER 2050 - economie inspiree de Victoria : production des provinces,
 * emploi et chomage, stocks, marche mondial et prix, embargos et blocus,
 * besoins des strates, impots par strate, depenses publiques, droits de
 * douane, usines, reparations, tresor et investissements des IA.
 */
#include "game_internal.h"
#include "lang.h"

#include <math.h>
#include <stdio.h>

/* poids fiscal de chaque strate (un membre des elites paie bien plus qu'un travailleur) */
static const float TAX_WEIGHT[S_COUNT] = { 0.6f, 1.4f, 6.0f };
/* besoins en biens industriels par million d'habitants de chaque strate */
static const float GOODS_NEED[S_COUNT] = { 0.25f, 0.8f, 4.0f };
/* part des echanges captee par l'Etat (TVA, taxes, subventions) */
#define TRADE_STATE_SHARE 0.35f

/* satisfaction des besoins en ressources strategiques, calculee une fois par tour */
static float stratSat[N_COUNT][SR_COUNT];

/* ------------------------------------------------------------------ */
/* Production et besoins                                               */
/* ------------------------------------------------------------------ */

static float RawSupply(const Game *g, int p, int good)
{
    const Province *pr = &g->prov[p];
    const ProvinceDef *d = &PROVINCES[p];
    float e = ProvinceEfficiency(pr);
    switch (good) {
    case G_FOOD:     return d->food * pr->hab / 100.0f * e * (1 - pr->flooded);
    case G_WATER:    return d->water * pr->hab / 100.0f * e * (1 - pr->flooded * 0.5f);
    case G_ENERGY:   return d->energy * (0.8f + 0.2f * pr->hab / 100.0f) * e;
    case G_MINERALS: return PROVINCE_MINERALS[p] * e;
    default: {
        /* les usines tournent selon l'energie et les minerais disponibles */
        const NationEconomy *ne = &g->eco[pr->owner];
        float input = fminf(1, fminf(ne->sat[G_ENERGY], ne->sat[G_MINERALS]));
        /* une usine sans travailleurs ne produit pas */
        float staff = pr->jobs > 1 ? fmaxf(0.5f, 1 / pr->jobs) : 1;
        float chips = 0.8f + 0.2f * stratSat[pr->owner][SR_PUCES];
        return pr->industry * 2.0f * e * (0.3f + 0.7f * input) * staff * chips;
    }
    }
}

/* calibrage : en 2030 l'offre mondiale couvre environ 105% des besoins */
static const float PRODUCTIVITY[G_COUNT] = { 1.33f, 1.2f, 1.33f, 1.21f, 1.6f };

float Eco_ProvinceSupply(const Game *g, int p, int good)
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
        for (int k = 0; k < G_COUNT; k++) ne->supply[k] += Eco_ProvinceSupply(g, p, k);
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
        if (g->proj[P_SMR].state == 2)    ne->supply[G_ENERGY] += 12 * stratSat[N_FRANCE][SR_URANIUM];
        if (g->proj[P_ENR].state == 2)    ne->supply[G_ENERGY] += 7 * (0.6f + 0.4f * stratSat[N_FRANCE][SR_TERRES_RARES]);
        ne->supply[G_ENERGY] += g->importBonus;
    } else {
        ne->supply[G_FOOD]  += g->nat[n].foodBonus;
        ne->supply[G_WATER] += g->nat[n].waterBonus;
    }
    if (g->nat[n].outage > 0) ne->supply[G_ENERGY] *= 0.6f;   /* reseau electrique paralyse */
}

/* ------------------------------------------------------------------ */
/* Emploi                                                              */
/* ------------------------------------------------------------------ */

/* emplois offerts par une province */
static float Jobs(const ProvinceDef *d, float pop, float hab, float industry, float prosperity, float literacy)
{
    float agri = 0.12f * d->food * hab / 100.0f;
    float factories = 0.4f * industry;
    float services = pop * (0.22f * sqrtf(fmaxf(prosperity, 0)) * (0.6f + 0.4f * literacy) + 0.25f * fmaxf(0, 0.5f - d->wealth));
    return agri + factories + services;
}

/* calibrage : en 2030, chaque province connait un chomage frictionnel d'environ 5% ;
   le chomage mesure ensuite l'ecart a cette situation (croissance, migrations, climat, guerre) */
static float JobCalibration(int p)
{
    static float k[PROV_COUNT];
    static bool done;
    if (!done) {
        for (int q = 0; q < PROV_COUNT; q++) {
            const ProvinceDef *d = &PROVINCES[q];
            float hab = Clampf(100 - d->aridity * 26, 3, 100);
            float lit = Clampf(0.45f + d->wealth * 1.2f, 0.4f, 0.98f);
            float j = Jobs(d, d->pop, hab, PROVINCE_INDUSTRY[q], 1, lit);
            k[q] = j > 0 ? d->pop * 0.48f * 0.95f / j : 1;
        }
        done = true;
    }
    return k[p];
}

static void Employment(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        float labor = pr->pop * 0.48f;
        if (labor <= 0) { pr->jobs = 1; pr->unemployment = 0; continue; }
        float e = pr->ctrl == pr->owner ? 1.0f : 0.6f;
        float jobs = Jobs(&PROVINCES[p], pr->pop, pr->hab, pr->industry, pr->prosperity, pr->literacy)
                   * JobCalibration(p) * e * (1 - pr->damage * 0.4f);
        pr->jobs = jobs / labor;
        pr->unemployment = Clampf(1 - pr->jobs, 0, 0.6f);
    }
}

float Eco_NationUnemployment(const Game *g, int n)
{
    float u = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n) { u += g->prov[p].unemployment * g->prov[p].pop; pop += g->prov[p].pop; }
    return pop > 0 ? u / pop : 0;
}

/* ------------------------------------------------------------------ */
/* Embargos                                                            */
/* ------------------------------------------------------------------ */

bool Eco_TradeAllowed(const Game *g, int n)
{
    const Nation *na = &g->nat[n];
    return na->alive && !na->embargo && !na->embargoFrance && !War_AtWar(g, N_FRANCE, n) && na->sanctions <= 0;
}

const char *Eco_EmbargoBlocked(const Game *g, int n)
{
    if (n <= 0 || n >= N_COUNT || !g->nat[n].alive) return "Nation invalide";
    if (g->nat[n].embargo) return NULL;
    if (g->nat[n].eu && g->nat[N_FRANCE].eu) return "Impossible au sein du marché unique européen";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool Eco_ToggleEmbargo(Game *g, int n)
{
    if (Eco_EmbargoBlocked(g, n)) return false;
    Nation *na = &g->nat[n];
    if (na->embargo) {
        na->embargo = false;
        AddRelation(g, n, 10);
        Game_Log(g, EV_INFO, "Levée de l'embargo contre %s.", Tr(NATIONS[n].name));
        return true;
    }
    g->commandPoints--;
    na->embargo = true;
    na->trade = false;
    AddRelation(g, n, -25);
    Game_Log(g, EV_WAR, "La France décrète un embargo commercial contre %s.", Tr(NATIONS[n].name));
    return true;
}

/* ------------------------------------------------------------------ */
/* Marche mondial                                                      */
/* ------------------------------------------------------------------ */

static void WorldMarket(Game *g)
{
    for (int n = 0; n < N_COUNT; n++)
        for (int r = 0; r < SR_COUNT; r++) stratSat[n][r] = g->nat[n].alive ? Strat_Sat(g, n, r) : 1;
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

        /* la France n'achete qu'aux nations avec lesquelles elle peut commercer ;
           ses partenaires commerciaux la servent en priorite */
        float allowed = 0, partner = 0;
        for (int n = 1; n < N_COUNT; n++) {
            if (!Eco_TradeAllowed(g, n)) continue;
            float b = g->eco[n].supply[k] - g->eco[n].demand[k];
            if (b <= 0) continue;
            allowed += b;
            if (g->nat[n].trade) partner += b * 0.5f;
        }
        float frShare = surplus > 0 ? allowed / surplus : 0;
        for (int n = 0; n < N_COUNT; n++) {
            if (!g->nat[n].alive) continue;
            NationEconomy *ne = &g->eco[n];
            float b = ne->supply[k] - ne->demand[k];
            float flow = (1 - 0.7f * g->nat[n].blockade) * (1 - g->nat[n].raid);   /* blocus et guerre au commerce */
            if (g->nat[n].sanctions > 0) flow *= 0.6f;                                 /* sanctions internationales */
            if (n != N_FRANCE && g->nat[n].embargo) flow *= 0.8f;
            if (n != N_FRANCE && g->nat[n].embargoFrance) flow *= 0.9f;
            if (b > 0) {
                ne->exports[k] = b * sell * flow;
            } else {
                float f = fill;
                if (n == N_FRANCE && -b > 0) f = fminf(1, fill * frShare + partner / -b);
                ne->imports[k] = -b * f * flow;
            }
        }
    }

    /* stocks : la moitie des excedents est stockee, les stocks comblent les penuries */
    for (int n = 0; n < N_COUNT; n++) {
        NationEconomy *ne = &g->eco[n];
        for (int k = 0; k < G_COUNT; k++) {
            float avail = ne->supply[k] + ne->imports[k];
            float left = avail - ne->exports[k] - ne->demand[k];
            if (!g->nat[n].alive) left = 0;
            if (left > 0) {
                ne->stock[k] = fminf(ne->demand[k] * 2, ne->stock[k] + left * 0.5f);
            } else {
                float draw = fminf(ne->stock[k], -left);
                ne->stock[k] -= draw;
                avail += draw;
            }
            ne->sat[k] = ne->demand[k] > 0 ? Clampf(avail / ne->demand[k], 0, 1.5f) : 1;
        }
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

/* requisitions : un quart de ce que rapporterait une province occupee par la France */
static float Requisitions(const Game *g)
{
    float r = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->ctrl != N_FRANCE || pr->owner == N_FRANCE) continue;
        r += pr->pop * PROVINCES[p].wealth * pr->prosperity * (0.5f + 0.5f * pr->hab / 100.0f) * (1 - pr->damage * 0.5f);
    }
    return r * 0.25f * REGIMES[g->nat[N_FRANCE].regime].taxEff;
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

static float Taxes(const Game *g)
{
    float taxes = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) taxes += Eco_ProvinceTax(g, p);
    taxes *= 1.4f * (0.9f + 0.2f * g->spending[SP_ADMIN]) * DiffIncome(g) * (0.7f + 0.005f * g->res[R_ENERGIE]);
    if (g->nat[N_FRANCE].government == I_SOCIAL) taxes *= 0.95f;
    if (g->boostTurns > 0) taxes *= 1.1f;
    return taxes;
}

/* ------------------------------------------------------------------ */
/* Dette publique                                                      */
/* ------------------------------------------------------------------ */

/* dette rapportee aux recettes fiscales annuelles */
float Eco_DebtRatio(const Game *g)
{
    return g->debt / fmaxf(20, Taxes(g) * 4);
}

int Eco_Rating(const Game *g)
{
    static const float thr[RATING_COUNT - 1] = { 1.5f, 2.5f, 3.5f, 5.0f, 6.5f, 8.0f };
    float r = Eco_DebtRatio(g);
    int i = 0;
    while (i < RATING_COUNT - 1 && r >= thr[i]) i++;
    if (g->bailouts > 0 && i < 3) i = 3;   /* les marches se souviennent */
    return i;
}

float Eco_InterestRate(const Game *g)
{
    return RATING_RATE[Eco_Rating(g)];
}

const char *Eco_BorrowBlocked(const Game *g)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (Eco_Rating(g) >= RATING_COUNT - 1) return "Les marchés refusent de nous prêter";
    if (g->austerityTurns > 0) return "Plan d'austérité en cours";
    return NULL;
}

bool Eco_Borrow(Game *g)
{
    if (Eco_BorrowBlocked(g)) return false;
    g->debt += 10;
    g->res[R_BUDGET] += 10;
    return true;
}

const char *Eco_RepayBlocked(const Game *g)
{
    if (g->debt < 10) return "Dette presque nulle";
    if (g->res[R_BUDGET] < 10) return "Trésorerie insuffisante (10 Md)";
    return NULL;
}

bool Eco_Repay(Game *g)
{
    if (Eco_RepayBlocked(g)) return false;
    g->debt -= 10;
    g->res[R_BUDGET] -= 10;
    return true;
}

void Eco_ResolveBailout(Game *g, bool accept)
{
    if (!accept) {
        Game_Log(g, EV_BAD, "La France fait défaut sur sa dette.");
        g->state = GAME_LOST_BUDGET;
        return;
    }
    g->bailouts++;
    g->debt *= 0.7f;
    g->res[R_BUDGET] = 5;
    g->austerityTurns = 12;
    for (int s = 0; s < SP_COUNT; s++) if (g->spending[s] > 0.2f) g->spending[s] = 0.2f;
    AddRes(g, R_SOUVERAINETE, -15);
    AddRes(g, R_COHESION, -8);
    Game_Log(g, EV_BAD, "Plan de sauvetage : la troïka impose l'austérité pour trois ans.");
}

/* cout d'adaptation des cotes face a la montee des eaux */
static float CoastalCost(const Game *g, int n)
{
    float c = 0, rise = fmaxf(0, g->seaLevel - 0.2f);
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != n || PROVINCE_SEA_EXPOSURE[p] <= 0) continue;
        c += PROVINCE_SEA_EXPOSURE[p] * g->prov[p].pop * rise * 0.25f + g->prov[p].flooded * g->prov[p].pop * 0.05f;
    }
    return c;
}

void Eco_Budget(const Game *g, float l[BL_COUNT])
{
    float taxes = Taxes(g);

    float imp = TradeValue(g, true);
    l[BL_TAXES]   = taxes;
    l[BL_TRADE]   = TradeValue(g, false) * TRADE_STATE_SHARE;
    l[BL_TARIFF]  = imp * g->tariff;
    l[BL_EU]      = g->nat[N_FRANCE].eu ? 2.0f : 0;
    l[BL_REPARATIONS] = Requisitions(g);
    l[BL_REP_PAID] = 0;
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &g->nat[n];
        if (na->reparationTurns <= 0) continue;
        if (na->reparations > 0) l[BL_REPARATIONS] += na->reparations;
        else l[BL_REP_PAID] -= na->reparations;
    }
    l[BL_RESEARCH] = g->researchBudget;
    l[BL_INTEREST] = g->debt * Eco_InterestRate(g) / 4;
    l[BL_COASTAL] = CoastalCost(g, N_FRANCE);
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
    for (int i = 0; i < BL_COUNT; i++) net += i < BL_FIRST_EXPENSE ? l[i] : -l[i];
    return net;
}

void Eco_SetTax(Game *g, int s, float rate)
{
    float max = g->nat[N_FRANCE].regime == RG_UTOPIE ? 0.35f : 0.7f;
    g->taxRate[s] = Clampf(rate, 0.05f, max);
}

void Eco_SetSpending(Game *g, int sp, float level) { g->spending[sp] = Clampf(level, 0, g->austerityTurns > 0 ? 0.25f : 1); }
void Eco_SetTariff(Game *g, float t) { g->tariff = Clampf(t, 0, 0.5f); }

/* ------------------------------------------------------------------ */
/* Usines                                                              */
/* ------------------------------------------------------------------ */

float Eco_FactoryCost(const Game *g)
{
    float c = 8;
    if (g->nat[N_FRANCE].regime == RG_TECHNOCRATIE) c *= 0.75f;
    if (g->nat[N_FRANCE].government == I_LIBERAL) c *= 0.9f;
    return c;
}

const char *Eco_FactoryBlocked(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->owner != N_FRANCE || pr->ctrl != N_FRANCE) return "Province étrangère ou occupée";
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
    Game_Log(g, EV_INFO, "Construction d'une usine en %s (4 tours).", Tr(PROVINCE_NAMES[p]));
    return true;
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

/* tresor et investissements des autres nations */
static void AiEconomy(Game *g)
{
    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;
        float income = 0;
        for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) income += Eco_ProvinceTax(g, p);
        float upkeep = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == n) upkeep += UNIT_TYPES[g->units[i].type].upkeep;
        float trade = 0;
        for (int k = 0; k < G_COUNT; k++) trade += (g->eco[n].exports[k] - g->eco[n].imports[k]) * g->price[k];
        float rep = na->reparationTurns > 0 ? -na->reparations : 0;
        na->treasury += income * 0.35f - upkeep * 0.5f + trade * 0.15f + rep - na->raid * 3 - CoastalCost(g, n) * 0.5f
                      - (na->sanctions > 0 ? 1.5f : 0);
        if (na->failed) na->treasury = fminf(na->treasury, 4);
        na->treasury = Clampf(na->treasury, -20, 60);

        /* investissements : eau et alimentation d'abord, puis industrie */
        const NationEconomy *ne = &g->eco[n];
        if (na->treasury > 6 && ne->sat[G_WATER] < 0.95f) {
            na->treasury -= 5;
            na->waterBonus += 0.5f;
        } else if (na->treasury > 6 && ne->sat[G_FOOD] < 0.95f) {
            na->treasury -= 5;
            na->foodBonus += 0.6f;
        } else if (na->treasury > 10 && !War_AtWarAny(g, n)) {
            /* une usine la ou le chomage est le plus fort */
            int best = -1;
            for (int p = 0; p < PROV_COUNT; p++) {
                const Province *pr = &g->prov[p];
                if (pr->owner != n || pr->ctrl != n || pr->factoryBuild > 0 || pr->revolt) continue;
                if (best < 0 || pr->unemployment * pr->pop > g->prov[best].unemployment * g->prov[best].pop) best = p;
            }
            if (best >= 0) {
                na->treasury -= 8;
                g->prov[best].factoryBuild = 4;
            }
        }
    }
}

void Eco_Turn(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->factoryBuild > 0 && --pr->factoryBuild == 0) {
            pr->industry += 1;
            if (pr->owner == N_FRANCE) Game_Log(g, EV_GOOD, "Nouvelle usine en service en %s.", Tr(PROVINCE_NAMES[p]));
        }
    }

    Employment(g);
    WorldMarket(g);
    AiEconomy(g);

    /* reparations et embargos */
    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (na->reparationTurns > 0 && --na->reparationTurns == 0) {
            na->reparations = 0;
            Game_Log(g, EV_INFO, "Fin des réparations de guerre avec %s.", Tr(NATIONS[n].name));
        }
        if (!na->alive) { na->embargoFrance = false; continue; }
        if (!na->embargoFrance && (na->relation < -55 || (na->embargo && na->relation < -30)) && Randf(g) < 0.1f
            && !(na->eu && g->nat[N_FRANCE].eu)) {
            na->embargoFrance = true;
            na->trade = false;
            Game_Log(g, EV_BAD, "%s décrète un embargo contre la France.", Tr(NATIONS[n].name));
        } else if (na->embargoFrance && na->relation > -20 && !na->embargo) {
            na->embargoFrance = false;
            Game_Log(g, EV_INFO, "%s lève son embargo contre la France.", Tr(NATIONS[n].name));
        }
    }

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
