/*
 * BOUCLIER 2050 - societe : climat local, production, population par
 * strates, migrations, militance, revoltes, integration et regimes.
 */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Provinces                                                           */
/* ------------------------------------------------------------------ */

float Soc_Habitability(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float arid = PROVINCES[p].aridity;
    if (pr->owner == N_FRANCE && g->proj[P_ADAPT].state == 2) arid *= 0.8f;
    float h = 100 - arid * (10 + 32 * (g->temp - 1.0f)) - pr->damage * 30;
    return Clampf(h, 3, 100);
}

float Soc_ProvinceMilitancy(const Province *p)
{
    float m = 0;
    for (int s = 0; s < S_COUNT; s++) m += p->mil[s] * STRATA_SHARE[s];
    return m;
}

/* efficacite d'une province pour son proprietaire (integration, revolte, guerre) */
static float Efficiency(const Province *pr)
{
    float e = pr->core == pr->owner ? 1.0f : 0.4f + 0.6f * pr->integration / 100.0f;
    if (pr->revolt) e *= 0.3f;
    return e * (1 - pr->damage * 0.5f);
}

static float ProvinceProd(const Game *g, int p, int r)
{
    const Province *pr = &g->prov[p];
    const ProvinceDef *d = &PROVINCES[p];
    float base = r == 0 ? d->food : r == 1 ? d->water : d->energy;
    float climate = r == 2 ? 0.8f + 0.2f * pr->hab / 100.0f : pr->hab / 100.0f;
    return base * climate * Efficiency(pr);
}

float Soc_ProvinceIncome(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float tax = REGIMES[g->nat[pr->owner].regime].taxEff;
    float v = pr->pop * PROVINCES[p].wealth * pr->prosperity * tax * (0.5f + 0.5f * pr->hab / 100.0f)
            * (1 - Soc_ProvinceMilitancy(pr) / 25.0f) * Efficiency(pr);
    if (pr->revolt) v = 0;
    return v;
}

float Soc_Income(const Game *g)
{
    float v = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) v += Soc_ProvinceIncome(g, p);
    v *= 0.7f + 0.005f * g->res[R_ENERGIE];   /* l'energie fait tourner l'economie */
    if (g->nat[N_FRANCE].eu) v += 2;
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].trade) v += 0.4f;
    return v;
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

static void NationBalance(const Game *g, int n, float prod[3], float *cons)
{
    prod[0] = prod[1] = prod[2] = 0;
    *cons = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != n) continue;
        const Province *pr = &g->prov[p];
        for (int r = 0; r < 3; r++) prod[r] += ProvinceProd(g, p, r);
        /* une population occupee passe apres les besoins du coeur national */
        *cons += pr->pop * (pr->core == n ? 1.0f : 0.3f + 0.7f * pr->integration / 100.0f);
    }
}

/* ------------------------------------------------------------------ */
/* Ressources nationales                                               */
/* ------------------------------------------------------------------ */

void Soc_UpdateResources(Game *g)
{
    float prod[3], cons;
    NationBalance(g, N_FRANCE, prod, &cons);
    if (g->proj[P_AGRO].state == 2)   prod[0] += 8;
    if (g->proj[P_DESSAL].state == 2) prod[1] += 9;
    if (g->proj[P_SMR].state == 2)    prod[2] += 12;
    if (g->proj[P_ENR].state == 2)    prod[2] += 7;
    prod[2] += g->importBonus;

    /* importations des partenaires commerciaux excedentaires */
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &g->nat[n];
        if (!na->trade || !na->alive || na->atWar) continue;
        float np[3], nc;
        NationBalance(g, n, np, &nc);
        for (int r = 0; r < 3; r++)
            if (np[r] > nc) prod[r] += (np[r] - nc) * 0.25f;
    }

    static const Resource map[3] = { R_ALIM, R_EAU, R_ENERGIE };
    for (int r = 0; r < 3; r++) {
        g->prod[r] = prod[r];
        g->cons[r] = cons;
        float ratio = cons > 0 ? prod[r] / cons : 1;
        float target = Clampf(60 + (ratio - 1) * 150, 0, 100);
        g->res[map[r]] += (target - g->res[map[r]]) * 0.3f;
        g->res[map[r]] = Clampf(g->res[map[r]], 0, 100);
    }
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
    if (!IsDemocratic(r) && fr->eu) {
        fr->eu = false;
        Game_Log(g, EV_BAD, "L'Union européenne suspend la France.");
    } else if (IsDemocratic(r) && !fr->eu && g->aggressionMemory < 30) {
        bool warEu = false;
        for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar && g->nat[n].eu) warEu = true;
        if (!warEu) {
            fr->eu = true;
            Game_Log(g, EV_GOOD, "La France réintègre l'Union européenne.");
        }
    }
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
    /* territoire conquis : au sud de 44 degres, la Mediterranee, sinon l'Est */
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
        if (push <= 0) continue;

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
            bool toFrance = g->prov[q].owner == N_FRANCE && pr->owner != N_FRANCE;
            if (toFrance) {
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
            /* les nouveaux arrivants ne partagent pas la culture du pays d'accueil */
            if (g->prov[q].core != pr->core || pr->foreign > 0) {
                Province *dst = &g->prov[q];
                float share = (g->prov[q].core != pr->core) ? 1.0f : pr->foreign;
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
            float chunk = r > 0.6f ? 0.6f : r;
            Hybrid_SpawnMigration(g, t, chunk);
            r -= 0.6f;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Militance, revoltes, integration                                    */
/* ------------------------------------------------------------------ */

static bool FranceAtWar(const Game *g)
{
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar) return true;
    return false;
}

static void FranceMilitancy(Game *g, int p)
{
    Province *pr = &g->prov[p];
    const RegimeDef *rg = &REGIMES[g->nat[N_FRANCE].regime];
    float food = g->res[R_ALIM], energy = g->res[R_ENERGIE], water = g->res[R_EAU];
    bool war = FranceAtWar(g);
    static const float foodW[S_COUNT] = { 1.0f, 0.5f, 0.1f };
    static const float enerW[S_COUNT] = { 0.4f, 0.8f, 0.2f };

    for (int s = 0; s < S_COUNT; s++) {
        float m = pr->mil[s];
        float d = 0.06f * (2.5f - m);
        if (food < 40)   d += (40 - food) / 40 * foodW[s];
        if (energy < 40) d += (40 - energy) / 40 * enerW[s];
        if (water < 40)  d += (40 - water) / 40 * 0.6f;
        if (war) d += rg->warWeariness * (s == S_ELITES ? 0.05f : 0.15f);
        if (pr->core != N_FRANCE) d += (1 - pr->integration / 100.0f) * 0.4f;
        if (s != S_ELITES) d += pr->foreign * 0.5f;
        if (g->res[R_BUDGET] < 0) d += s == S_ELITES ? 0.15f : 0.1f;
        if (g->res[R_COHESION] < 30) d += 0.1f;
        if (g->proj[P_MEDIAS].state == 2) d -= 0.03f;
        d -= rg->sat[s];
        d -= rg->repression * 0.25f;
        pr->mil[s] = Clampf(m + d, 0, 10);
    }
}

static void ForeignMilitancy(Game *g, int p)
{
    Province *pr = &g->prov[p];
    const Nation *na = &g->nat[pr->owner];
    float target = 2;
    if (pr->core != pr->owner) target += 3 * (1 - pr->integration / 100.0f);
    if (pr->hab < 45) target += (45 - pr->hab) / 10;
    if (na->failed) target += 2;
    if (na->atWar || na->warTarget >= 0) target += 1;
    target -= REGIMES[na->regime].repression * 2;
    for (int s = 0; s < S_COUNT; s++)
        pr->mil[s] = Clampf(pr->mil[s] + (target - pr->mil[s]) * 0.1f, 0, 10);
}

static void Revolts(Game *g, int p)
{
    Province *pr = &g->prov[p];
    float mil = Soc_ProvinceMilitancy(pr);
    const RegimeDef *rg = &REGIMES[g->nat[pr->owner].regime];

    if (pr->owner != N_FRANCE) {
        /* soulevement de liberation dans une province occupee par une IA */
        if (pr->core != pr->owner && mil > 7.5f && g->nat[pr->core].alive && Randf(g) < 0.08f) {
            Game_Log(g, EV_WAR, "Soulèvement : %s se libère et rejoint %s.", PROVINCE_NAMES[p], NATIONS[pr->core].name);
            War_Capture(g, p, pr->core);
        }
        return;
    }

    if (!pr->revolt) {
        if (mil > 6.5f && Randf(g) < (mil - 6) * 0.08f) {
            pr->revolt = 1;
            AddRes(g, R_COHESION, -3);
            Game_Log(g, EV_BAD, "Révolte en %s ! La province échappe au contrôle de l'État.", PROVINCE_NAMES[p]);
        }
        return;
    }

    pr->revolt++;
    AddRes(g, R_COHESION, -0.8f);
    float avail = War_AvailableDivisions(g);
    float chance = 0.1f + rg->repression * 0.4f + Clampf(avail * 0.01f, 0, 0.25f) + 0.02f * pr->revolt;
    if (Randf(g) < chance) {
        pr->revolt = 0;
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = Clampf(pr->mil[s] - 2.5f, 0, 10);
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

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Soc_Turn(Game *g)
{
    /* 1. habitabilite */
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        pr->damage *= 0.93f;
        pr->hab = Soc_Habitability(g, p);
        float stab = 1 - Soc_ProvinceMilitancy(pr) / 6.0f;
        pr->prosperity = Clampf(pr->prosperity + 0.002f * stab * pr->hab / 100.0f - pr->damage * 0.01f
                                - (pr->revolt ? 0.01f : 0), 0.4f, 2.5f);
    }

    /* 2. croissance demographique */
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        const Nation *na = &g->nat[pr->owner];
        float gr = PROVINCES[p].growth + REGIMES[na->regime].growth;
        if (pr->owner == N_FRANCE) {
            gr += (g->res[R_ALIM] - 50) / 100.0f * 0.2f;
            if (g->res[R_ALIM] < 25) gr -= 0.3f;
        } else {
            gr += (pr->hab - 60) / 100.0f * 0.3f;
        }
        if (pr->damage > 0.1f) gr -= pr->damage;
        pr->pop *= 1 + gr / 100.0f;
    }

    /* 3. migrations */
    Migrations(g);

    /* 4. militance, revoltes, integration */
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) FranceMilitancy(g, p);
        else ForeignMilitancy(g, p);
        Revolts(g, p);

        float assim = REGIMES[g->nat[pr->owner].regime].assimilation;
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

    /* 5. cohesion et souverainete */
    float milSum = 0, popSum = 0;
    int occupied = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) {
            float w = pr->pop * (pr->core == N_FRANCE ? 1.0f : 0.4f);
            milSum += Soc_ProvinceMilitancy(pr) * w;
            popSum += w;
        }
        else if (PROVINCES[p].nation == N_FRANCE) occupied++;
    }
    float avgMil = popSum > 0 ? milSum / popSum : 5;
    float target = Clampf(88 - avgMil * 12, 0, 100);
    g->res[R_COHESION] += (target - g->res[R_COHESION]) * 0.12f;
    AddRes(g, R_COHESION, 0);
    if (occupied) AddRes(g, R_SOUVERAINETE, -2.0f * occupied);
    if (Game_ActiveThreats(g) < 6) AddRes(g, R_SOUVERAINETE, 0.5f);

    /* 6. influence */
    AddRes(g, R_INFLUENCE, 0.2f + REGIMES[g->nat[N_FRANCE].regime].influence * 0.5f - g->aggressionMemory * 0.01f);

    /* 7. ressources */
    Soc_UpdateResources(g);
}
