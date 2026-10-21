/*
 * BOUCLIER 2050 - societe : climat local, population par strates (parts,
 * militance, conscience politique), alphabetisation, ascension sociale,
 * migrations, revoltes, integration et changements de regime.
 */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>

/* satisfaction apportee a chaque strate par le courant au pouvoir */
static const float GOV_SAT[I_COUNT][S_COUNT] = {
    {  0.02f, 0.03f, -0.05f },   /* Ecologistes */
    {  0.06f, 0.02f, -0.06f },   /* Socialistes */
    { -0.03f, 0.03f,  0.06f },   /* Liberaux */
    {  0.02f, 0.02f,  0.03f },   /* Conservateurs */
    {  0.02f, 0.00f,  0.00f },   /* Souverainistes */
    {  0.00f, 0.00f,  0.00f },   /* Autoritaires */
};

/* ------------------------------------------------------------------ */
/* Provinces                                                           */
/* ------------------------------------------------------------------ */

float Soc_Habitability(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float arid = PROVINCES[p].aridity;
    if (pr->owner == N_FRANCE && g->proj[P_ADAPT].state == 2) arid *= 0.8f;
    float h = 100 - arid * (10 + 32 * (g->temp - 1.0f)) - pr->damage * 30 - pr->fallout * 60;
    return Clampf(h, 3, 100);
}

float Soc_ProvinceMilitancy(const Province *p)
{
    float m = 0;
    for (int s = 0; s < S_COUNT; s++) m += p->mil[s] * p->share[s];
    return m;
}

float Soc_ProvinceConsciousness(const Province *p)
{
    float c = 0;
    for (int s = 0; s < S_COUNT; s++) c += p->con[s] * p->share[s];
    return c;
}

/* une population consciente se souleve plus tot */
float Soc_RevoltThreshold(const Province *p)
{
    return 6.5f - Soc_ProvinceConsciousness(p) * 0.15f;
}

float Soc_NationPop(const Game *g, int n)
{
    float v = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) v += g->prov[p].pop;
    return v;
}

int Soc_NationProvinces(const Game *g, int n)
{
    int c = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) c++;
    return c;
}

/* ------------------------------------------------------------------ */
/* Regime                                                              */
/* ------------------------------------------------------------------ */

static int RegimeDistance(const Game *g, int r)
{
    int d = r - (int)g->nat[N_FRANCE].regime;
    return d < 0 ? -d : d;
}

float Soc_RegimeChangeCost(const Game *g, int r)
{
    return 5.0f + 5.0f * RegimeDistance(g, r);
}

const char *Soc_RegimeBlocked(const Game *g, int r)
{
    static char buf[96];
    if ((int)g->nat[N_FRANCE].regime == r) return "Régime actuel";
    if (g->regimeCooldown > 0) {
        snprintf(buf, sizeof(buf), "Le pays se remet du dernier bouleversement (%d tours)", g->regimeCooldown);
        return buf;
    }
    if (g->res[R_COHESION] <= Soc_RegimeChangeCost(g, r)) return "Cohésion insuffisante pour un tel bouleversement";
    return NULL;
}

void Soc_SetRegime(Game *g, Regime r)
{
    Nation *fr = &g->nat[N_FRANCE];
    fr->regime = r;
    if (r == RG_UTOPIE)
        for (int s = 0; s < S_COUNT; s++) if (g->taxRate[s] > 0.35f) g->taxRate[s] = 0.35f;
    if (!IsDemocratic(r) && fr->eu) {
        fr->eu = false;
        Game_Log(g, EV_BAD, "L'Union européenne suspend la France.");
    } else if (IsDemocratic(r) && !fr->eu && g->aggressionMemory < 30 && g->nukesUsed == 0) {
        bool warEu = false;
        for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar && g->nat[n].eu) warEu = true;
        if (!warEu) {
            fr->eu = true;
            Game_Log(g, EV_GOOD, "La France réintègre l'Union européenne.");
        }
    }
    g->nextElection = r == RG_DEMOCRATIE ? g->turn + 4 : -1;
    g->government = r == RG_UTOPIE ? I_ECOLO : r == RG_DEMOCRATIE ? I_LIBERAL : -1;
    g->decreeCooldown[0] = g->decreeCooldown[1] = 0;
}

bool Soc_ChangeRegime(Game *g, int r)
{
    if (Soc_RegimeBlocked(g, r)) return false;
    int dist = RegimeDistance(g, r);
    AddRes(g, R_COHESION, -Soc_RegimeChangeCost(g, r));
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++)
            g->prov[p].mil[s] = Clampf(g->prov[p].mil[s] + 0.6f * dist, 0, 10);
    }
    if (dist >= 3) {
        AddRes(g, R_SOUVERAINETE, -5);
        Game_Log(g, EV_EVENT, "Révolution : la France bascule vers un nouveau régime, %s.", REGIMES[r].name);
    } else {
        Game_Log(g, EV_EVENT, "Changement de régime : %s.", REGIMES[r].name);
    }
    Soc_SetRegime(g, (Regime)r);
    g->regimeCooldown = 12;
    return true;
}

/* ------------------------------------------------------------------ */
/* Migrations                                                          */
/* ------------------------------------------------------------------ */

static float Attractiveness(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float a = pr->hab * 0.6f + PROVINCES[p].wealth * pr->prosperity * 80 - pr->damage * 60 - Soc_ProvinceMilitancy(pr) * 2;
    if (pr->revolt) a -= 20;
    if (g->nat[pr->owner].failed) a -= 15;
    return a;
}

/* theatre de defense franchi par un flux refoule vers la province francaise q */
static int BorderTheater(int q, bool sea)
{
    switch (q) {
    case PV_FR_NORD:  return sea ? TH_ATLANTIQUE : TH_NORD;
    case PV_FR_OUEST: return TH_ATLANTIQUE;
    case PV_FR_EST:   return TH_EST;
    case PV_FR_RA:    return TH_ALPES;
    case PV_FR_SO:    return TH_PYRENEES;
    case PV_FR_SUD:   return sea ? TH_MEDITERRANEE : TH_PYRENEES;
    case PV_FR_CORSE: return TH_MEDITERRANEE;
    }
    return PROVINCE_LABEL[q][1] > (int)((MAP_LAT1 - 44.0f) / MAP_STEP) ? TH_MEDITERRANEE : TH_EST;
}

static void Migrations(Game *g)
{
    float flows[PROV_COUNT] = { 0 };
    float refusedByTheater[TH_COUNT] = { 0 };
    g->migrantsIn = g->migrantsRefused = g->migrantsOut = 0;
    for (int p = 0; p < PROV_COUNT; p++) { g->prov[p].emig = 0; g->prov[p].immig = 0; }

    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        float push = 0.0015f + Clampf(75 - pr->hab, 0, 75) / 100.0f * 0.015f + pr->damage * 0.03f;
        if (pr->revolt) push += 0.01f;
        if (g->nat[pr->owner].failed) push += 0.005f;
        if (Soc_ProvinceMilitancy(pr) > 6) push += 0.003f;
        if (pr->owner == N_FRANCE && g->nat[N_FRANCE].regime == RG_TOTALITAIRE) push += 0.002f;

        float out = pr->pop * push;
        float aSrc = Attractiveness(g, p);
        float weights[MAX_ADJ], total = 0;
        for (int k = 0; k < MAX_ADJ; k++) {
            int q = PROVINCE_ADJ[p][k];
            weights[k] = 0;
            if (q < 0) break;
            float diff = Attractiveness(g, q) - aSrc - 5;
            if (diff > 0) { weights[k] = diff; total += diff; }
        }
        if (total <= 0) continue;

        for (int k = 0; k < MAX_ADJ; k++) {
            int q = PROVINCE_ADJ[p][k];
            if (q < 0) break;
            if (weights[k] <= 0) continue;
            float m = out * weights[k] / total;
            if (g->prov[q].owner == N_FRANCE && pr->owner != N_FRANCE) {
                float acc = MIGRATION_ACCEPT[g->migrationDoctrine];
                float refused = m * (1 - acc);
                refusedByTheater[BorderTheater(q, Map_SeaLink(p, q))] += refused;
                g->migrantsRefused += refused;
                m *= acc;
                g->migrantsIn += m;
            }
            if (pr->owner == N_FRANCE && g->prov[q].owner != N_FRANCE) g->migrantsOut += m;
            flows[p] -= m;
            flows[q] += m;
            pr->emig += m;
            g->prov[q].immig += m;
            if (g->prov[q].core != pr->core || pr->foreign > 0) {
                Province *dst = &g->prov[q];
                float share = g->prov[q].core != pr->core ? 1.0f : pr->foreign;
                dst->foreign = (dst->foreign * dst->pop + m * share) / (dst->pop + m);
            }
        }
    }
    for (int p = 0; p < PROV_COUNT; p++) {
        g->prov[p].pop += flows[p];
        if (g->prov[p].pop < 0.05f) g->prov[p].pop = 0.05f;
    }
    for (int t = 0; t < TH_COUNT; t++) {
        float r = refusedByTheater[t];
        while (r > 0.02f) {
            Hybrid_SpawnMigration(g, t, r > 0.6f ? 0.6f : r);
            r -= 0.6f;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Militance, conscience, revoltes                                     */
/* ------------------------------------------------------------------ */

static bool FranceAtWar(const Game *g)
{
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar) return true;
    return false;
}

static void FrancePopulation(Game *g, int p, float support, bool war)
{
    Province *pr = &g->prov[p];
    const RegimeDef *rg = &REGIMES[g->nat[N_FRANCE].regime];

    for (int s = 0; s < S_COUNT; s++) {
        float m = pr->mil[s];
        float need = Eco_NeedSat(g, N_FRANCE, s);
        float rate = g->taxRate[s];

        /* facteurs d'agitation, amplifies par la conscience politique */
        float up = need < 0.97f ? (0.97f - need) * 1.5f : 0;   /* petits manques toleres */
        if (war) up += rg->warWeariness * (s == S_ELITES ? 0.05f : 0.15f);
        if (pr->core != N_FRANCE) up += (1 - pr->integration / 100.0f) * 0.4f;
        if (s != S_ELITES) up += pr->foreign * 0.5f;
        if (g->res[R_BUDGET] < 0) up += s == S_ELITES ? 0.15f : 0.1f;
        if (g->res[R_COHESION] < 30) up += 0.1f;
        if (rate > 0.3f) up += (rate - 0.3f) * 0.6f;
        if (support < 0.45f) up += (0.45f - support) * 0.4f;
        up *= 1 + pr->con[s] / 10.0f;

        /* facteurs d'apaisement */
        float down = rg->sat[s] + rg->repression * 0.25f;
        if (g->government >= 0) down += GOV_SAT[g->government][s];
        if (rate < 0.3f) down += (0.3f - rate) * 0.4f;
        if (s != S_ELITES) down += (g->spending[SP_SOCIAL] - 0.4f) * 0.15f;
        if (g->proj[P_MEDIAS].state == 2) down += 0.03f;

        pr->mil[s] = Clampf(m + up - down - 0.06f * (m - 2.5f), 0, 10);

        /* la conscience monte avec l'instruction, les privations et la repression */
        float c = 0.015f * pr->literacy + (need < 0.97f ? (0.97f - need) * 0.3f : 0) + rg->repression * 0.04f - 0.03f;
        if (support < 0.45f) c += 0.05f;
        if (g->boostTurns > 0) c += 0.02f;
        pr->con[s] = Clampf(pr->con[s] + c, 0, 10);
    }
}

static void ForeignPopulation(Game *g, int p)
{
    Province *pr = &g->prov[p];
    const Nation *na = &g->nat[pr->owner];
    float target = 2;
    if (pr->core != pr->owner) target += 3 * (1 - pr->integration / 100.0f);
    if (pr->hab < 45) target += (45 - pr->hab) / 10;
    if (na->failed) target += 2;
    if (na->atWar || na->warTarget >= 0) target += 1;
    target -= REGIMES[na->regime].repression * 2;
    for (int s = 0; s < S_COUNT; s++) {
        pr->mil[s] = Clampf(pr->mil[s] + (target - pr->mil[s]) * 0.1f, 0, 10);
        pr->con[s] = Clampf(pr->con[s] + 0.01f * pr->literacy - 0.008f, 0, 10);
    }
}

static void Revolts(Game *g, int p)
{
    Province *pr = &g->prov[p];
    float mil = Soc_ProvinceMilitancy(pr);
    const RegimeDef *rg = &REGIMES[g->nat[pr->owner].regime];

    if (pr->owner != N_FRANCE) {
        if (pr->core != pr->owner && mil > 7.5f && g->nat[pr->core].alive && Randf(g) < 0.08f) {
            Game_Log(g, EV_WAR, "Soulèvement : %s se libère et rejoint %s.", PROVINCE_NAMES[p], NATIONS[pr->core].name);
            War_Capture(g, p, pr->core);
        }
        return;
    }

    float thr = Soc_RevoltThreshold(pr);
    if (!pr->revolt) {
        if (mil > thr && Randf(g) < (mil - thr + 0.5f) * 0.08f) {
            pr->revolt = 1;
            AddRes(g, R_COHESION, -3);
            Game_Log(g, EV_BAD, "Révolte en %s ! La province échappe au contrôle de l'État.", PROVINCE_NAMES[p]);
        }
        return;
    }

    pr->revolt++;
    AddRes(g, R_COHESION, -0.8f);
    int troops = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g->units[i].active && g->units[i].owner == N_FRANCE && g->units[i].prov == p && !g->units[i].training) troops++;
    float chance = 0.1f + rg->repression * 0.4f + g->spending[SP_ADMIN] * 0.15f + troops * 0.12f + 0.02f * pr->revolt;
    if (Randf(g) < chance) {
        pr->revolt = 0;
        for (int s = 0; s < S_COUNT; s++) {
            pr->mil[s] = Clampf(pr->mil[s] - 2.5f, 0, 10);
            pr->con[s] = Clampf(pr->con[s] + rg->repression, 0, 10);
        }
        pr->pop *= 1 - rg->repression * 0.01f;
        Game_Log(g, rg->repression >= 0.5f ? EV_BAD : EV_GOOD,
                 rg->repression >= 0.5f ? "Révolte écrasée dans le sang en %s." : "L'ordre est rétabli en %s.",
                 PROVINCE_NAMES[p]);
        return;
    }
    if (pr->revolt > 8) {
        if (pr->core != N_FRANCE && g->nat[pr->core].alive) {
            Game_Log(g, EV_WAR, "Sécession : %s rejoint %s.", PROVINCE_NAMES[p], NATIONS[pr->core].name);
            War_Capture(g, p, pr->core);
        } else {
            AddRes(g, R_COHESION, -2);
            AddRes(g, R_SOUVERAINETE, -1);
        }
    }
}

/* alphabetisation, ascension sociale et prosperite */
static void SocialProgress(Game *g, int p)
{
    Province *pr = &g->prov[p];
    bool fr = pr->owner == N_FRANCE;
    float lit = fr ? (g->spending[SP_EDUCATION] - 0.3f) * 0.004f : (g->nat[pr->owner].failed ? -0.001f : 0.0006f);
    pr->literacy = Clampf(pr->literacy + lit, 0.3f, 0.99f);

    float targetM = Clampf(0.1f + PROVINCES[p].wealth * pr->prosperity * 0.45f + pr->literacy * 0.08f, 0.08f, 0.55f);
    pr->share[S_MOYENNES] += (targetM - pr->share[S_MOYENNES]) * 0.01f;
    pr->share[S_TRAVAILLEURS] = 1 - pr->share[S_MOYENNES] - pr->share[S_ELITES];

    float stab = 1 - Soc_ProvinceMilitancy(pr) / 6.0f;
    float growth = 0.002f * stab * pr->hab / 100.0f + 0.001f * (pr->literacy - 0.6f);
    if (fr) {
        if (g->government == I_LIBERAL) growth *= 1.5f;
        growth += (g->spending[SP_EDUCATION] - 0.5f) * 0.001f;
        growth *= 0.7f + 0.6f * fminf(1, g->eco[N_FRANCE].sat[G_GOODS]);
    }
    pr->prosperity = Clampf(pr->prosperity + growth - pr->damage * 0.01f - (pr->revolt ? 0.01f : 0), 0.4f, 2.5f);
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Soc_Turn(Game *g)
{
    /* 1. habitabilite, contamination */
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        pr->damage *= 0.93f;
        pr->fallout *= 0.99f;
        pr->hab = Soc_Habitability(g, p);
    }

    /* 2. production, marche mondial, approvisionnements */
    Eco_Turn(g);

    /* 3. croissance demographique */
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        const Nation *na = &g->nat[pr->owner];
        float gr = PROVINCES[p].growth + REGIMES[na->regime].growth;
        float food = fminf(1.2f, g->eco[pr->owner].sat[G_FOOD]);
        gr += (food - 1) * 0.6f;
        if (pr->owner != N_FRANCE) gr += (pr->hab - 60) / 100.0f * 0.2f;
        if (pr->damage > 0.1f) gr -= pr->damage;
        pr->pop *= 1 + gr / 100.0f;
    }

    /* 4. migrations */
    Migrations(g);

    /* 5. militance, conscience, revoltes, integration, progres social */
    float support = Pol_RegimeSupport(g);
    bool war = FranceAtWar(g);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) FrancePopulation(g, p, support, war);
        else ForeignPopulation(g, p);
        Revolts(g, p);
        SocialProgress(g, p);

        float assim = REGIMES[g->nat[pr->owner].regime].assimilation;
        if (pr->owner == N_FRANCE && g->government == I_CONSERV) assim *= 0.8f;
        if (pr->core != pr->owner && !pr->revolt) {
            pr->integration = Clampf(pr->integration + 2.0f * assim, 0, 100);
            if (pr->integration >= 100) {
                pr->core = pr->owner;
                pr->foreign = Clampf(pr->foreign + 0.3f, 0, 1);
                if (pr->owner == N_FRANCE)
                    Game_Log(g, EV_GOOD, "%s est désormais pleinement intégrée à la France.", PROVINCE_NAMES[p]);
            }
        }
        pr->foreign = Clampf(pr->foreign - 0.004f * assim, 0, 1);
    }

    /* 6. cohesion et souverainete */
    float milSum = 0, popSum = 0;
    int occupied = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) {
            float w = pr->pop * (pr->core == N_FRANCE ? 1.0f : 0.4f);
            milSum += Soc_ProvinceMilitancy(pr) * w;
            popSum += w;
        } else if (PROVINCES[p].nation == N_FRANCE) {
            occupied++;
        }
    }
    float avgMil = popSum > 0 ? milSum / popSum : 5;
    float target = Clampf(88 - avgMil * 12, 0, 100);
    if (g->nat[N_FRANCE].regime == RG_TOTALITAIRE) target += 10;   /* la peur tient lieu de cohesion */
    g->res[R_COHESION] = Clampf(g->res[R_COHESION] + (target - g->res[R_COHESION]) * 0.12f, 0, 100);
    if (occupied) AddRes(g, R_SOUVERAINETE, -2.0f * occupied);
    if (Game_ActiveThreats(g) < 6) AddRes(g, R_SOUVERAINETE, 0.5f);

    /* 7. influence */
    AddRes(g, R_INFLUENCE, 0.2f + REGIMES[g->nat[N_FRANCE].regime].influence * 0.5f - g->aggressionMemory * 0.01f);
}
