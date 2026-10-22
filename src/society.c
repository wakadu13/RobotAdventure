/*
 * BOUCLIER 2050 - societe : climat local, population par strates (parts,
 * militance, conscience politique), cultures et religions, minorites,
 * chomage, alphabetisation, ascension sociale, migrations, revoltes,
 * secessions, integration et changements de regime.
 */
#include "game_internal.h"
#include "lang.h"

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
    float h = 100 - arid * (10 + 32 * (g->temp - 1.0f)) - pr->damage * 30 - pr->fallout * 60 - pr->flooded * 70;
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
/* Cultures et religions                                               */
/* ------------------------------------------------------------------ */

bool Soc_Accepted(const Game *g, int n, int c)
{
    (void)g;
    for (int k = 0; k < 3; k++) if (NATIONS[n].cultures[k] == c) return true;
    return false;
}

/* part de la population d'une culture non acceptee par le proprietaire */
float Soc_NonAccepted(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float v = 0;
    for (int c = 0; c < CULT_COUNT; c++) if (!Soc_Accepted(g, pr->owner, c)) v += pr->culture[c];
    return v;
}

/* poids politique des minorites : les citoyens europeens au sein de l'Union s'agitent peu */
static float MinorityWeight(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    bool eu = g->nat[pr->owner].eu;
    float v = 0;
    for (int c = 0; c < CULT_COUNT; c++) {
        if (Soc_Accepted(g, pr->owner, c) || pr->culture[c] <= 0) continue;
        int h = CULTURE_HOMELAND[c];
        v += pr->culture[c] * (eu && h >= 0 && g->nat[h].eu ? 0.25f : 1.0f);
    }
    return v;
}

int Soc_MainCulture(const Province *pr)
{
    int best = 0;
    for (int c = 1; c < CULT_COUNT; c++) if (pr->culture[c] > pr->culture[best]) best = c;
    return best;
}

int Soc_MainReligion(const Province *pr)
{
    int best = 0;
    for (int r = 1; r < REL_COUNT; r++) if (pr->religion[r] > pr->religion[best]) best = r;
    return best;
}

/* religion majoritaire d'une nation */
static int NationReligion(const Game *g, int n)
{
    float v[REL_COUNT] = { 0 };
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n)
            for (int r = 0; r < REL_COUNT; r++) v[r] += g->prov[p].religion[r] * g->prov[p].pop;
    int best = 0;
    for (int r = 1; r < REL_COUNT; r++) if (v[r] > v[best]) best = r;
    return best;
}

static void NormalizeShares(float *v, int count)
{
    float t = 0;
    for (int i = 0; i < count; i++) { if (v[i] < 0) v[i] = 0; t += v[i]; }
    if (t <= 0) return;
    for (int i = 0; i < count; i++) v[i] /= t;
}

const char *Soc_CulturePolicyBlocked(const Game *g, int policy)
{
    if (policy == g->culturePolicy) return "Politique actuelle";
    if (policy == CP_REPRESSION && g->nat[N_FRANCE].regime < RG_AUTORITAIRE) return "Réservée aux régimes autoritaires";
    if (g->res[R_COHESION] < 5) return "Cohésion insuffisante";
    return NULL;
}

void Soc_SetCulturePolicy(Game *g, int policy)
{
    if (Soc_CulturePolicyBlocked(g, policy)) return;
    g->culturePolicy = policy;
    AddRes(g, R_COHESION, -3);
    Game_Log(g, EV_EVENT, "Politique culturelle : %s.", (CULTURE_POLICY_NAMES[policy]));
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
        snprintf(buf, sizeof(buf), Tr("Le pays se remet du dernier bouleversement (%d tours)"), g->regimeCooldown);
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
        for (int n = 1; n < N_COUNT; n++) if (War_AtWar(g, N_FRANCE, n) && g->nat[n].eu) warEu = true;
        if (!warEu) {
            fr->eu = true;
            Game_Log(g, EV_GOOD, "La France réintègre l'Union européenne.");
        }
    }
    fr->nextElection = r == RG_DEMOCRATIE ? g->turn + 4 : -1;
    if (r < RG_AUTORITAIRE && g->culturePolicy == CP_REPRESSION) g->culturePolicy = CP_ASSIMILATION;
    g->nat[N_FRANCE].government = r == RG_UTOPIE ? I_ECOLO : r == RG_DEMOCRATIE ? I_LIBERAL : -1;
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
        Game_Log(g, EV_EVENT, "Révolution : la France bascule vers un nouveau régime, %s.", (REGIMES[r].name));
    } else {
        Game_Log(g, EV_EVENT, "Changement de régime : %s.", (REGIMES[r].name));
    }
    Soc_SetRegime(g, (Regime)r);
    g->regimeCooldown = 12;
    return true;
}

/* ------------------------------------------------------------------ */
/* Montee des eaux                                                     */
/* ------------------------------------------------------------------ */

static void SeaLevel(Game *g)
{
    g->seaLevel = 0.2f + 0.45f * fmaxf(0, g->temp - 1.5f);
    float rise = Clampf((g->seaLevel - 0.2f) / 0.8f, 0, 1.2f);
    for (int p = 0; p < PROV_COUNT; p++) {
        float exp = PROVINCE_SEA_EXPOSURE[p];
        if (exp <= 0) continue;
        Province *pr = &g->prov[p];
        /* digues : les pays riches se protegent, le plan d'adaptation francais aussi */
        float dikes = 1 - 0.45f * Clampf(PROVINCES[p].wealth * pr->prosperity * 2, 0, 1);
        if (pr->owner == N_FRANCE && g->proj[P_ADAPT].state == 2) dikes *= 0.5f;
        float target = Clampf(exp * rise * dikes, 0, 0.95f);
        if (target > pr->flooded) {
            float before = pr->flooded, d = fminf(target - pr->flooded, 0.01f);
            pr->flooded += d;
            pr->prosperity = Clampf(pr->prosperity - d * 2, 0.4f, 2.5f);
            static const float steps[3] = { 0.1f, 0.25f, 0.5f };
            for (int k = 0; k < 3; k++)
                if (before < steps[k] && pr->flooded >= steps[k] && (pr->owner == N_FRANCE || exp >= 0.3f))
                    Game_Log(g, pr->owner == N_FRANCE ? EV_BAD : EV_CLIMATE, "Montée des eaux : %.0f%% des terres de %s sont submergées.",
                             steps[k] * 100, (PROVINCE_NAMES[p]));
        }
        /* submersions marines lors des tempetes */
        if (Randf(g) < exp * g->seaLevel * 0.03f * DiffClimate(g)) {
            float k = pr->owner == N_FRANCE && g->proj[P_ADAPT].state == 2 ? 0.5f : 1.0f;
            pr->damage = Clampf(pr->damage + 0.25f * k, 0, 1);
            pr->pop *= 1 - 0.004f * k;
            if (pr->owner == N_FRANCE || exp >= 0.3f)
                Game_Log(g, pr->owner == N_FRANCE ? EV_BAD : EV_CLIMATE, "Submersion marine en %s : digues rompues, quartiers évacués.",
                         (PROVINCE_NAMES[p]));
        }
    }
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
    a -= pr->unemployment * 40;
    if (pr->ctrl != pr->owner) a -= 15;
    return a;
}

/* theatre de defense franchi par un flux refoule vers la province francaise q */
static int BorderTheater(int q, bool sea)
{
    switch (q) {
    case PV_FR_HDF: case PV_FR_IDF: case PV_FR_CVL: return sea ? TH_ATLANTIQUE : TH_NORD;
    case PV_FR_NOR: case PV_FR_BRE: case PV_FR_PDL: return TH_ATLANTIQUE;
    case PV_FR_GES: case PV_FR_BFC: return TH_EST;
    case PV_FR_ARA: return TH_ALPES;
    case PV_FR_PAC: return sea ? TH_MEDITERRANEE : TH_ALPES;
    case PV_FR_OCC: return sea ? TH_MEDITERRANEE : TH_PYRENEES;
    case PV_FR_NAQ: return sea ? TH_ATLANTIQUE : TH_PYRENEES;
    case PV_FR_COR: return TH_MEDITERRANEE;
    }
    return TH_EXTERIEUR;
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
        push += pr->unemployment * 0.01f + pr->flooded * 0.02f;
        if (pr->owner == N_FRANCE && g->culturePolicy == CP_REPRESSION) push += Soc_NonAccepted(g, p) * 0.004f;

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
            if (g->prov[q].owner == N_FRANCE && g->prov[q].ctrl == N_FRANCE && pr->owner != N_FRANCE) {
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
            /* les migrants emportent leur culture et leur religion */
            Province *dst = &g->prov[q];
            if (m > 0 && dst->pop + m > 0) {
                float k = m / (dst->pop + m);
                for (int c = 0; c < CULT_COUNT; c++) dst->culture[c] += (pr->culture[c] - dst->culture[c]) * k;
                for (int r = 0; r < REL_COUNT; r++) dst->religion[r] += (pr->religion[r] - dst->religion[r]) * k;
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

/* agitation propre aux minorites selon la politique culturelle */
static float MinorityUnrest(const Game *g, int p, int s)
{
    const Province *pr = &g->prov[p];
    if (s == S_ELITES || pr->core != pr->owner) return 0;   /* territoire non integre : voir l'integration */
    float na = MinorityWeight(g, p);
    float k = 0.3f;
    if (pr->owner == N_FRANCE) {
        static const float pol[CP_COUNT] = { 0.4f, 0.15f, 0.06f };
        k = pol[g->culturePolicy];
    } else if (g->nat[pr->owner].regime >= RG_AUTORITAIRE) {
        k = 0.15f;
    }
    /* une minorite religieuse est plus remuante sous un regime autoritaire */
    float rel = 1 - pr->religion[NationReligion(g, pr->owner)];
    if (rel > 0.3f && g->nat[pr->owner].regime >= RG_AUTORITAIRE) k += 0.1f;
    return na * k;
}

static void FrancePopulation(Game *g, int p, float support, bool war)
{
    Province *pr = &g->prov[p];
    const RegimeDef *rg = &REGIMES[g->nat[N_FRANCE].regime];
    static const float JOBLESS[S_COUNT] = { 0.6f, 0.3f, 0.05f };

    for (int s = 0; s < S_COUNT; s++) {
        float m = pr->mil[s];
        float need = Eco_NeedSat(g, N_FRANCE, s);
        float rate = g->taxRate[s];

        /* facteurs d'agitation, amplifies par la conscience politique */
        float up = need < 0.97f ? (0.97f - need) * 1.5f : 0;   /* petits manques toleres */
        if (war) up += rg->warWeariness * (s == S_ELITES ? 0.05f : 0.15f);
        if (pr->core != N_FRANCE) up += (1 - pr->integration / 100.0f) * 0.4f;
        up += MinorityUnrest(g, p, s);
        up += fmaxf(0, pr->unemployment - 0.07f) * JOBLESS[s];
        int rating = Eco_Rating(g);
        if (rating >= 4) up += (rating - 3) * (s == S_ELITES ? 0.05f : 0.03f);   /* crainte d'une crise de la dette */
        if (g->austerityTurns > 0 && s != S_ELITES) up += 0.08f;
        if (g->res[R_COHESION] < 30) up += 0.1f;
        if (rate > 0.3f) up += (rate - 0.3f) * 0.6f;
        float sup = Pol_StrataSupport(g, p, s);
        if (sup < 0.45f) up += (0.45f - sup) * 0.3f;
        if (support < 0.45f) up += (0.45f - support) * 0.15f;
        up *= 1 + pr->con[s] / 10.0f;

        /* facteurs d'apaisement */
        float down = rg->sat[s] + rg->repression * 0.25f;
        if (g->nat[N_FRANCE].government >= 0) down += GOV_SAT[g->nat[N_FRANCE].government][s];
        if (rate < 0.3f) down += (0.3f - rate) * 0.4f;
        if (s != S_ELITES) down += (g->spending[SP_SOCIAL] - 0.4f) * 0.15f;
        if (g->proj[P_MEDIAS].state == 2) down += 0.03f;

        pr->mil[s] = Clampf(m + up - down - 0.06f * (m - 2.5f), 0, 10);

        /* la conscience monte avec l'instruction, les privations et la repression */
        /* la repression eveille surtout les consciences la ou la colere gronde deja */
        float c = 0.015f * pr->literacy + (need < 0.97f ? (0.97f - need) * 0.3f : 0) + rg->repression * (m > 4 ? 0.04f : 0.015f) - 0.03f;
        if (support < 0.45f) c += 0.03f;
        if (g->boostTurns > 0) c += 0.02f;
        if (g->culturePolicy == CP_REPRESSION && s != S_ELITES) c += Soc_NonAccepted(g, p) * 0.05f;
        pr->con[s] = Clampf(pr->con[s] + c, 0, 10);
    }
}

/* meme modele que pour la France, avec la fiscalite et les depenses par defaut */
static void AiPopulation(Game *g, int p)
{
    Province *pr = &g->prov[p];
    int n = pr->owner;
    const Nation *na = &g->nat[n];
    const RegimeDef *rg = &REGIMES[na->regime];
    float support = Pol_NationSupport(g, n);
    bool war = War_AtWarAny(g, n);
    static const float JOBLESS[S_COUNT] = { 0.6f, 0.3f, 0.05f };
    for (int s = 0; s < S_COUNT; s++) {
        float m = pr->mil[s];
        float need = Eco_NeedSat(g, n, s);
        float up = need < 0.97f ? (0.97f - need) * 1.5f : 0;
        if (war) up += rg->warWeariness * (s == S_ELITES ? 0.05f : 0.12f);
        if (pr->core != n) up += (1 - pr->integration / 100.0f) * 0.4f;
        if (pr->ctrl != n) up += 0.3f;   /* occupation etrangere */
        up += MinorityUnrest(g, p, s);
        up += fmaxf(0, pr->unemployment - 0.07f) * JOBLESS[s];
        if (na->failed) up += 0.15f;
        if (pr->hab < 45) up += (45 - pr->hab) / 100.0f;
        float sup = Pol_StrataSupport(g, p, s);
        if (sup < 0.45f) up += (0.45f - sup) * 0.3f;
        if (support < 0.45f) up += (0.45f - support) * 0.15f;
        up *= 1 + pr->con[s] / 10.0f;
        float down = rg->sat[s] + rg->repression * 0.25f + 0.05f;
        pr->mil[s] = Clampf(m + up - down - 0.06f * (m - 2.5f), 0, 10);
        float c = 0.012f * pr->literacy + (need < 0.97f ? (0.97f - need) * 0.3f : 0) + rg->repression * 0.04f - 0.03f;
        pr->con[s] = Clampf(pr->con[s] + c, 0, 10);
    }
}

/* nation qui revendique une province (patrie de sa culture majoritaire ou nation d'origine) */
static int Claimant(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->core != pr->owner && g->nat[pr->core].alive) return pr->core;
    int c = Soc_MainCulture(pr);
    int h = CULTURE_HOMELAND[c];
    if (h >= 0 && h != pr->owner && g->nat[h].alive && !Soc_Accepted(g, pr->owner, c) && pr->culture[c] > 0.6f) return h;
    return -1;
}

static void Revolts(Game *g, int p)
{
    Province *pr = &g->prov[p];
    float mil = Soc_ProvinceMilitancy(pr);
    const RegimeDef *rg = &REGIMES[g->nat[pr->owner].regime];

    if (pr->ctrl != pr->owner) return;   /* l'occupation est l'affaire des partisans */
    if (pr->owner != N_FRANCE) {
        int c = Claimant(g, p);
        if (c >= 0 && mil > 7.5f && !War_AtWar(g, pr->owner, c) && Randf(g) < 0.06f) {
            Game_Log(g, EV_WAR, "Soulèvement : %s se libère et rejoint %s.", (PROVINCE_NAMES[p]), (NATIONS[c].name));
            War_Annex(g, p, c);
            pr->core = c;
            pr->integration = 100;
        }
        return;
    }

    float thr = Soc_RevoltThreshold(pr);
    if (!pr->revolt) {
        if (mil > thr && Randf(g) < (mil - thr + 0.5f) * 0.08f) {
            pr->revolt = 1;
            AddRes(g, R_COHESION, -3);
            Game_Log(g, EV_BAD, "Révolte en %s ! La province échappe au contrôle de l'État.", (PROVINCE_NAMES[p]));
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
                 (PROVINCE_NAMES[p]));
        return;
    }
    if (pr->revolt > 8) {
        int c = Claimant(g, p);
        if (c >= 0) {
            Game_Log(g, EV_WAR, "Sécession : %s rejoint %s.", (PROVINCE_NAMES[p]), (NATIONS[c].name));
            War_Annex(g, p, c);
            pr->core = c;
            pr->integration = 100;
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
        if (g->nat[N_FRANCE].government == I_LIBERAL) growth *= 1.5f;
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
    /* 1. montee des eaux, habitabilite, contamination */
    SeaLevel(g);
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

    /* 5. militance, conscience, revoltes, integration, assimilation, progres social */
    float support = Pol_RegimeSupport(g);
    bool war = War_AtWarAny(g, N_FRANCE);
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE) FrancePopulation(g, p, support, war);
        else AiPopulation(g, p);
        Revolts(g, p);
        SocialProgress(g, p);

        int o = pr->owner;
        float assim = REGIMES[g->nat[o].regime].assimilation;
        if (o == N_FRANCE && g->nat[N_FRANCE].government == I_CONSERV) assim *= 0.8f;
        if (pr->core != o && pr->ctrl == o && !pr->revolt) {
            pr->integration = Clampf(pr->integration + 2.0f * assim, 0, 100);
            if (pr->integration >= 100) {
                pr->core = o;
                if (o == N_FRANCE)
                    Game_Log(g, EV_GOOD, "%s est désormais pleinement intégrée à la France.", (PROVINCE_NAMES[p]));
            }
        }
        /* assimilation des cultures non acceptees vers la culture principale du proprietaire */
        float rate = 0.005f * assim;
        if (o == N_FRANCE) {
            static const float pol[CP_COUNT] = { 2.0f, 0.6f, 2.5f };
            rate *= pol[g->culturePolicy];
        }
        int target = NATIONS[o].cultures[0];
        if (pr->ctrl == o && target < CULT_COUNT) {
            float moved = 0;
            for (int c = 0; c < CULT_COUNT; c++) {
                if (Soc_Accepted(g, o, c) || pr->culture[c] <= 0) continue;
                float d = pr->culture[c] * rate;
                pr->culture[c] -= d;
                moved += d;
            }
            pr->culture[target] += moved;
        }
        /* secularisation lente avec l'instruction */
        float sec = 0.0004f * pr->literacy * pr->prosperity;
        for (int r = 0; r < RL_SANS; r++) {
            float d = pr->religion[r] * sec;
            pr->religion[r] -= d;
            pr->religion[RL_SANS] += d;
        }
        NormalizeShares(pr->culture, CULT_COUNT);
        NormalizeShares(pr->religion, REL_COUNT);

        /* irredentisme : la patrie d'une minorite importante en veut au proprietaire */
        int mc = Soc_MainCulture(pr);
        int h = CULTURE_HOMELAND[mc];
        if (h >= 0 && h != o && g->nat[h].alive && !Soc_Accepted(g, o, mc)) AddAiRel(g, h, o, -0.3f);
    }
    if (g->culturePolicy == CP_REPRESSION) AddRes(g, R_INFLUENCE, -0.3f);

    /* 6. cohesion et souverainete */
    float milSum = 0, popSum = 0;
    int occupied = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner == N_FRANCE && pr->ctrl == N_FRANCE) {
            float w = pr->pop * (pr->core == N_FRANCE ? 1.0f : 0.4f);
            milSum += Soc_ProvinceMilitancy(pr) * w;
            popSum += w;
        }
        if (pr->owner == N_FRANCE && pr->core == N_FRANCE && pr->ctrl != N_FRANCE) occupied++;
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
