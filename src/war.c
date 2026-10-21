/*
 * BOUCLIER 2050 - guerre et diplomatie, inspirees de Hearts of Iron :
 * divisions terrestres, aeriennes et navales positionnees sur la carte,
 * vitesse et empilement, batailles (organisation, terrain, appui aerien,
 * DCA, feux dans la profondeur, debarquements, generaux, technologie),
 * combats navals et blocus, ravitaillement, guerres multiples, alliances,
 * occupation, score de guerre et traites de paix.
 */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Outils                                                              */
/* ------------------------------------------------------------------ */

bool War_AtWar(const Game *g, int a, int b)
{
    if (a < 0 || b < 0 || a == b) return false;
    return g->war[a][b];
}

bool War_AtWarAny(const Game *g, int n)
{
    for (int k = 0; k < N_COUNT; k++) if (g->war[n][k]) return true;
    return false;
}

int War_Turns(const Game *g, int a, int b)
{
    return g->war[a][b] ? g->turn - g->warSince[a][b] : 0;
}

/* province tenue par la nation ou un allie */
bool War_Friendly(const Game *g, int n, int p)
{
    return IsAlly(g, n, g->prov[p].ctrl);
}

static bool Hostile(const Game *g, int n, int p)
{
    return War_AtWar(g, n, g->prov[p].ctrl);
}

static float AvgWealth(const Game *g, int n)
{
    float w = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != n) continue;
        w += PROVINCES[p].wealth * g->prov[p].prosperity * g->prov[p].pop;
        pop += g->prov[p].pop;
    }
    return pop > 0 ? w / pop : 0;
}

/* province la plus peuplee du coeur national tenue par la nation */
static int Capital(const Game *g, int n)
{
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != n || pr->ctrl != n) continue;
        if (best < 0 || (pr->core == n && g->prov[best].core != n)
            || (pr->core == g->prov[best].core && pr->pop > g->prov[best].pop))
            best = p;
    }
    return best;
}

static int CoastalBase(const Game *g, int n)
{
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++)
        if (PROVINCE_COASTAL[p] && g->prov[p].ctrl == n && (best < 0 || g->prov[p].pop > g->prov[best].pop)) best = p;
    return best;
}

static bool IsAdjacent(int a, int b)
{
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[a][k] >= 0; k++)
        if (PROVINCE_ADJ[a][k] == b) return true;
    return false;
}

static bool TouchesNation(const Game *g, int p, int n)
{
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
        if (g->prov[PROVINCE_ADJ[p][k]].ctrl == n) return true;
    return false;
}

/* arete maritime : liaison maritime ou deux provinces cotieres voisines */
static bool SeaEdge(int a, int b)
{
    return PROVINCE_COASTAL[a] && PROVINCE_COASTAL[b] && (IsAdjacent(a, b) || Map_SeaLink(a, b));
}

/* distance en provinces (terre ou mer), -1 au-dela de maxd */
static int Distance(int from, int to, int maxd, bool sea)
{
    if (from == to) return 0;
    int dist[PROV_COUNT], queue[PROV_COUNT], head = 0, tail = 0;
    for (int i = 0; i < PROV_COUNT; i++) dist[i] = -1;
    dist[from] = 0;
    queue[tail++] = from;
    while (head < tail) {
        int p = queue[head++];
        if (dist[p] >= maxd) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
            int q = PROVINCE_ADJ[p][k];
            if (dist[q] >= 0 || (sea && !SeaEdge(p, q))) continue;
            dist[q] = dist[p] + 1;
            if (q == to) return dist[q];
            queue[tail++] = q;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Puissance                                                           */
/* ------------------------------------------------------------------ */

float War_ArmyMax(const Game *g, int n)
{
    float pop = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != n) continue;
        pop += pr->pop * (pr->core == n ? 1.0f : 0.25f * pr->integration / 100.0f);
    }
    float m = pop * REGIMES[g->nat[n].regime].mobilization;
    if (n == N_FRANCE) {
        m *= 1.6f;   /* armee professionnelle et reserves operationnelles */
        if (g->mobilizationTurns > 0) m *= 1.5f;
        if (War_AtWarAny(g, N_FRANCE)) m *= 1.4f;
    } else {
        m *= Clampf(0.4f + AvgWealth(g, n) * 1.5f, 0.4f, 1.0f);
        if (g->nat[n].failed) m *= 0.5f;
        if (War_AtWarAny(g, n)) m *= 1.4f;
        m *= 1 + g->aggressionMemory / 50.0f;
    }
    return m;
}

float War_UnitLoad(const Game *g, int n)
{
    float l = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g->units[i].active && g->units[i].owner == n) l += UNIT_TYPES[g->units[i].type].capWeight;
    return l;
}

int War_UnitCount(const Game *g, int n)
{
    int c = 0;
    for (int i = 0; i < MAX_UNITS; i++) if (g->units[i].active && g->units[i].owner == n) c++;
    return c;
}

float War_Quality(const Game *g, int n)
{
    float q = REGIMES[g->nat[n].regime].quality;
    if (n == N_FRANCE) {
        q *= 1 + 0.015f * (g->capacity[D_AIR] + g->capacity[D_ESPACE] + g->capacity[D_EM] + g->capacity[D_CYBER]);
        if (g->proj[P_CONSTELL].state == 2) q *= 1.05f;
        if (g->aiDefense) q *= 1.05f;
    } else {
        q *= Clampf(0.7f + AvgWealth(g, n) * 0.8f, 0.7f, 1.2f);
    }
    if (g->eco[n].sat[G_GOODS] < 0.7f) q *= 0.85f;
    if (g->nat[n].asat > 0) q *= 0.9f;   /* plus de satellites militaires */
    return q;
}

bool War_Adjacent(const Game *g, int a, int b)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].ctrl == a && TouchesNation(g, p, b)) return true;
    return false;
}

bool War_ProvinceAttackable(const Game *g, int p)
{
    if (!Hostile(g, N_FRANCE, p)) return false;
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
        if (War_Friendly(g, N_FRANCE, PROVINCE_ADJ[p][k])) return true;
    return false;
}

int War_StackCount(const Game *g, int n, int p, UnitDomain dom)
{
    int c = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (u->active && u->owner == n && u->prov == p && UNIT_TYPES[u->type].domain == dom) c++;
    }
    return c;
}

static int StackLimit(UnitDomain d) { return d == DOM_LAND ? STACK_LAND : d == DOM_AIR ? STACK_AIR : STACK_SEA; }

static void UpdateArmyPower(Game *g)
{
    for (int n = 0; n < N_COUNT; n++) g->nat[n].army = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training) continue;
        const UnitDef *d = &UNIT_TYPES[u->type];
        float v = d->domain == DOM_SEA ? d->naval * 0.5f : (d->atk + d->def) / 6.0f;
        g->nat[u->owner].army += u->str * v;
    }
}

/* ------------------------------------------------------------------ */
/* Unites                                                              */
/* ------------------------------------------------------------------ */

int War_SpawnUnit(Game *g, int owner, int type, int prov, int training)
{
    if (prov < 0) return -1;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (u->active) continue;
        memset(u, 0, sizeof(*u));
        u->active = true;
        u->type = (unsigned char)type;
        u->owner = (signed char)owner;
        u->prov = (short)prov;
        u->target = -1;
        u->str = 1;
        u->org = training ? 0.5f : 1;
        u->training = training;
        return i;
    }
    return -1;
}

/* composition cible d'une armee selon la richesse, l'acces a la mer et le relief */
static int PickType(Game *g, int n, bool coastal)
{
    float w = AvgWealth(g, n);
    float want[U_TYPE_COUNT] = { 0 };
    want[U_INF] = 0.30f; want[U_MECA] = 0.14f; want[U_BLINDE] = 0.12f; want[U_ARTILLERIE] = 0.08f;
    want[U_DRONES] = 0.07f; want[U_DCA] = 0.05f; want[U_FS] = 0.02f;
    bool mountains = false;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n && PROVINCE_TERRAIN[p] == 2) mountains = true;
    if (mountains) want[U_MONTAGNE] = 0.05f;
    if (w > 0.3f) want[U_ROBOTS] = 0.04f;
    if (w > 0.35f) want[U_HYPER] = 0.02f;
    if (coastal) want[U_MARINE] = 0.03f;
    if (w > 0.15f) { want[U_CHASSE] = 0.07f; want[U_MALE] = 0.04f; }
    if (coastal && w > 0.15f) {
        want[U_FREGATE] = 0.06f; want[U_SNA] = 0.03f; want[U_USV] = 0.02f;
        if (w > 0.35f) want[U_PA] = 0.01f;
    }
    if (w < 0.15f) { want[U_INF] += 0.15f; want[U_BLINDE] *= 0.5f; }

    int have[U_TYPE_COUNT] = { 0 }, total = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g->units[i].active && g->units[i].owner == n) { have[g->units[i].type]++; total++; }
    int best = U_INF;
    float bestGap = -1e9f;
    for (int t = 0; t < U_TYPE_COUNT; t++) {
        if (want[t] <= 0 || !Mil_UnitUnlocked(g, n, t) || Strat_RecruitBlocked(g, n, t)) continue;
        float gap = want[t] * (total + 1) - have[t] + Randf(g) * 0.5f;
        if (gap > bestGap) { bestGap = gap; best = t; }
    }
    return best;
}

static int SpawnSite(const Game *g, int n, int type)
{
    UnitDomain d = UNIT_TYPES[type].domain;
    int c = d == DOM_SEA ? CoastalBase(g, n) : Capital(g, n);
    if (c >= 0 && War_StackCount(g, n, c, d) < StackLimit(d)) return c;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].ctrl == n && g->prov[p].owner == n && (d != DOM_SEA || PROVINCE_COASTAL[p])
            && War_StackCount(g, n, p, d) < StackLimit(d))
            return p;
    return -1;
}

static float AiRelTarget(const Game *g, int a, int b);

void War_InitUnits(Game *g)
{
    /* relations initiales entre nations */
    for (int a = 1; a < N_COUNT; a++)
        for (int b = a + 1; b < N_COUNT; b++) g->airel[a][b] = g->airel[b][a] = AiRelTarget(g, a, b);

    memset(g->units, 0, sizeof(g->units));
    War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_IDF, 0);
    War_SpawnUnit(g, N_FRANCE, U_MECA, PV_FR_IDF, 0);
    War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_GES, 0);
    War_SpawnUnit(g, N_FRANCE, U_BLINDE, PV_FR_GES, 0);
    War_SpawnUnit(g, N_FRANCE, U_MONTAGNE, PV_FR_ARA, 0);
    War_SpawnUnit(g, N_FRANCE, U_MARINE, PV_FR_PAC, 0);
    War_SpawnUnit(g, N_FRANCE, U_CHASSE, PV_FR_GES, 0);
    War_SpawnUnit(g, N_FRANCE, U_MALE, PV_FR_NAQ, 0);
    War_SpawnUnit(g, N_FRANCE, U_FREGATE, PV_FR_PAC, 0);
    War_SpawnUnit(g, N_FRANCE, U_FREGATE, PV_FR_BRE, 0);
    War_SpawnUnit(g, N_FRANCE, U_SNA, PV_FR_BRE, 0);

    for (int n = 1; n < N_COUNT; n++) {
        int provs[PROV_COUNT], c = 0;
        bool coastal = false;
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == n) { provs[c++] = p; if (PROVINCE_COASTAL[p]) coastal = true; }
        for (int i = 0; i < c; i++)
            for (int j = i + 1; j < c; j++)
                if (g->prov[provs[j]].pop > g->prov[provs[i]].pop) { int t = provs[i]; provs[i] = provs[j]; provs[j] = t; }
        float budget = War_ArmyMax(g, n) * 0.8f;
        if (budget < 1) budget = 1;
        float load = 0;
        for (int k = 0; load < budget && k < 60; k++) {
            int type = PickType(g, n, coastal);
            int site = UNIT_TYPES[type].domain == DOM_SEA ? CoastalBase(g, n) : provs[k % c];
            if (site < 0 || War_StackCount(g, n, site, UNIT_TYPES[type].domain) >= StackLimit(UNIT_TYPES[type].domain)) continue;
            War_SpawnUnit(g, n, type, site, 0);
            load += UNIT_TYPES[type].capWeight;
        }
    }
    UpdateArmyPower(g);
}

float War_RecruitCost(const Game *g, int type)
{
    return UNIT_TYPES[type].cost * (g->nat[N_FRANCE].regime == RG_JUNTE ? 0.75f : 1.0f);
}

const char *War_RecruitBlocked(const Game *g, int type, int p)
{
    const UnitDef *d = &UNIT_TYPES[type];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (!Mil_UnitUnlocked(g, N_FRANCE, type)) return "Technologie insuffisante (onglet ÉTAT-MAJOR)";
    if (p < 0 || g->prov[p].owner != N_FRANCE || g->prov[p].ctrl != N_FRANCE) return "Choisissez une province française";
    if (g->prov[p].core != N_FRANCE) return "Recrutement impossible en territoire non intégré";
    if (g->prov[p].revolt) return "Province en révolte";
    if (d->domain == DOM_SEA && !PROVINCE_COASTAL[p]) return "Il faut une province côtière";
    if (War_StackCount(g, N_FRANCE, p, d->domain) >= StackLimit(d->domain)) return "Province saturée (limite d'empilement)";
    if (War_UnitLoad(g, N_FRANCE) + d->capWeight > War_ArmyMax(g, N_FRANCE) + 0.01f) return "Plafond de mobilisation atteint";
    if (g->res[R_BUDGET] < War_RecruitCost(g, type)) return "Budget insuffisant";
    return Strat_RecruitBlocked(g, N_FRANCE, type);
}

bool War_Recruit(Game *g, int type, int p)
{
    if (War_RecruitBlocked(g, type, p)) return false;
    if (War_SpawnUnit(g, N_FRANCE, type, p, UNIT_TYPES[type].train) < 0) return false;
    g->res[R_BUDGET] -= War_RecruitCost(g, type);
    Strat_PayRecruit(g, N_FRANCE, type);
    return true;
}

bool War_Disband(Game *g, int i)
{
    if (i < 0 || i >= MAX_UNITS || !g->units[i].active || g->units[i].owner != N_FRANCE) return false;
    g->units[i].active = false;
    UpdateArmyPower(g);
    return true;
}

float War_ArmyUpkeep(const Game *g)
{
    float u = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g->units[i].active && g->units[i].owner == N_FRANCE) u += UNIT_TYPES[g->units[i].type].upkeep;
    return u;
}

/* ------------------------------------------------------------------ */
/* Ordres et itineraires                                               */
/* ------------------------------------------------------------------ */

static bool CanCross(const Game *g, const Unit *u, int from, int to)
{
    if (!Map_SeaLink(from, to)) return true;
    if (u->type == U_MARINE) return true;
    return u->owner != N_FRANCE || g->capacity[D_MER] >= 3;
}

/* itineraire terrestre : provinces amies puis, au plus, une province ennemie a la fin */
static int LandPath(const Game *g, const Unit *u, int target, int path[], int maxLen)
{
    int prev[PROV_COUNT], dist[PROV_COUNT], queue[PROV_COUNT], head = 0, tail = 0;
    for (int i = 0; i < PROV_COUNT; i++) { prev[i] = -1; dist[i] = -1; }
    dist[u->prov] = 0;
    queue[tail++] = u->prov;
    while (head < tail) {
        int p = queue[head++];
        if (p == target || dist[p] >= maxLen) continue;
        if (p != u->prov && !War_Friendly(g, u->owner, p)) continue;   /* on ne traverse pas l'ennemi */
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
            int q = PROVINCE_ADJ[p][k];
            if (dist[q] >= 0 || !CanCross(g, u, p, q)) continue;
            if (!War_Friendly(g, u->owner, q) && !Hostile(g, u->owner, q)) continue;
            dist[q] = dist[p] + 1;
            prev[q] = p;
            queue[tail++] = q;
        }
    }
    if (dist[target] < 0) return 0;
    int len = dist[target];
    for (int p = target, i = len - 1; i >= 0; i--, p = prev[p]) path[i] = p;
    return len;
}

const char *War_OrderBlocked(const Game *g, int i, int t)
{
    const Unit *u = &g->units[i];
    const UnitDef *d = &UNIT_TYPES[u->type];
    if (!u->active || u->owner != N_FRANCE) return "Unité invalide";
    if (u->training) return "En formation";
    if (t == u->prov) return NULL;   /* annulation */
    if (d->domain == DOM_AIR) {
        if (!War_Friendly(g, u->owner, t)) return "Les escadres se redéploient vers une province amie";
        if (Distance(u->prov, t, 4, false) < 0) return "Hors de portée de redéploiement (4 provinces)";
        return NULL;
    }
    if (d->domain == DOM_SEA) {
        if (!PROVINCE_COASTAL[t] || !War_Friendly(g, u->owner, t)) return "Les flottes rejoignent un port ami";
        if (Distance(u->prov, t, d->speed, true) < 0) return "Trop loin par la mer";
        return NULL;
    }
    int o = g->prov[t].ctrl;
    if (!IsAlly(g, N_FRANCE, o) && !War_AtWar(g, N_FRANCE, o)) return "Territoire d'une nation avec laquelle nous sommes en paix";
    int path[PROV_COUNT];
    int len = LandPath(g, u, t, path, d->speed);
    if (len == 0) {
        if (IsAdjacent(u->prov, t) && Map_SeaLink(u->prov, t)) return "Traversée impossible : capacité navale insuffisante (3)";
        return d->speed > 1 ? "Hors de portée (2 provinces, à travers un territoire ami)" : "Province non adjacente";
    }
    return NULL;
}

bool War_Order(Game *g, int i, int t)
{
    if (War_OrderBlocked(g, i, t)) return false;
    g->units[i].target = (short)(t == g->units[i].prov ? -1 : t);
    return true;
}

/* ------------------------------------------------------------------ */
/* Combat                                                              */
/* ------------------------------------------------------------------ */

static float Readiness(const Unit *u) { return u->str * (0.3f + 0.7f * u->org); }

/* puissance aerienne d'un camp (nation et allies en guerre contre l'adversaire) a portee de p */
static float AirPower(const Game *g, int side, int enemy, int p)
{
    float a = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training) continue;
        if (u->owner != side && !(IsAlly(g, side, u->owner) && War_AtWar(g, u->owner, enemy))) continue;
        const UnitDef *d = &UNIT_TYPES[u->type];
        if (d->domain != DOM_AIR && u->type != U_PA) continue;
        int r = Distance(u->prov, p, d->range, false);
        if (r < 0) continue;
        a += d->atk * Readiness(u) * Mil_TechBonus(g, u->owner, u->type) * (g->nat[u->owner].jammed > 0 ? 0.6f : 1.0f);
    }
    return a;
}

static float AntiAir(const Game *g, int side, int p)
{
    float a = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || u->owner != side) continue;
        if (u->prov == p || (IsAdjacent(u->prov, p) && UNIT_TYPES[u->type].aa >= 3))
            a += UNIT_TYPES[u->type].aa * Readiness(u);
    }
    return a;
}

static float NavalPower(const Game *g, int n, int p, int radius)
{
    float s = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || u->owner != n || UNIT_TYPES[u->type].domain != DOM_SEA) continue;
        int d = Distance(u->prov, p, radius, true);
        if (d >= 0) s += UNIT_TYPES[u->type].naval * Readiness(u) * Mil_TechBonus(g, n, u->type);
    }
    return s;
}

static float TerrainFactor(int type, int terrain, bool attack)
{
    if (type == U_MONTAGNE) return terrain >= 1 ? 1.3f : 1.0f;
    if (!attack) return 1;
    if (type == U_BLINDE) return terrain == 2 ? 0.6f : terrain == 1 ? 0.85f : 1.15f;
    if (type == U_MECA) return terrain == 2 ? 0.8f : terrain == 1 ? 0.95f : 1.05f;
    return 1;
}

static float UnitAttack(const Game *g, const Unit *u, int p, int defender)
{
    const UnitDef *d = &UNIT_TYPES[u->type];
    float a = d->atk * Readiness(u) * War_Quality(g, u->owner) * Mil_TechBonus(g, u->owner, u->type)
            * TerrainFactor(u->type, PROVINCE_TERRAIN[p], true) * Mil_GeneralBonus(g, u->owner, u->prov, u->type, true);
    if (Map_SeaLink(u->prov, p)) {
        /* debarquement : la superiorite navale change tout */
        bool superiority = NavalPower(g, u->owner, p, 2) > NavalPower(g, defender, p, 2);
        if (u->type == U_MARINE) a *= superiority ? 1.0f : 0.6f;
        else a *= superiority ? 0.7f : 0.35f;
    }
    if (u->type == U_DRONES) {
        float jam = AntiAir(g, defender, p) * 0.04f + g->nat[defender].tech[T_GE] * 0.05f;
        a *= 1 - Clampf(jam, 0, 0.6f);
        if (g->nat[u->owner].jammed > 0) a *= 0.7f;
    }
    /* reconnaissance satellitaire francaise */
    if (u->owner == N_FRANCE && g->nat[defender].recon > 0) a *= 1.15f;
    return a;
}

static float UnitDefense(const Game *g, const Unit *u, int p)
{
    const UnitDef *d = &UNIT_TYPES[u->type];
    return d->def * Readiness(u) * War_Quality(g, u->owner) * Mil_TechBonus(g, u->owner, u->type)
         * TerrainFactor(u->type, PROVINCE_TERRAIN[p], false) * Mil_GeneralBonus(g, u->owner, p, u->type, false);
}

static bool IsAttacker(const Game *g, const Unit *u, int p, int a)
{
    (void)g;
    return u->active && !u->training && u->owner == a && u->target == p && IsAdjacent(u->prov, p)
        && UNIT_TYPES[u->type].domain == DOM_LAND;
}

static bool IsDefender(const Game *g, const Unit *u, int p, int a)
{
    return u->active && !u->training && u->prov == p && UNIT_TYPES[u->type].domain == DOM_LAND && War_AtWar(g, u->owner, a);
}

/* feux dans la profondeur : artillerie et missiles du camp a portee */
static float FireSupport(const Game *g, int side, int p, bool attack, int *hyper)
{
    float s = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || u->owner != side) continue;
        if (u->type != U_ARTILLERIE && u->type != U_HYPER) continue;
        if (attack && u->target == p) continue;   /* deja comptee parmi les assaillants */
        int r = Distance(u->prov, p, UNIT_TYPES[u->type].range, false);
        if (r < 0) continue;
        s += UNIT_TYPES[u->type].atk * Readiness(u) * Mil_TechBonus(g, side, u->type) * (attack ? 0.6f : 0.4f);
        if (u->type == U_HYPER && hyper) (*hyper)++;
    }
    return s;
}

static float Garrison(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    float garrison = (1.0f + pr->pop * 0.06f) * (pr->core == pr->ctrl ? 1.2f : 0.5f) * (1 + 0.2f * PROVINCE_TERRAIN[p]);
    if (pr->ctrl == N_FRANCE && pr->core == N_FRANCE) garrison += Game_PostureUsed(g, D_TERRE) * 0.15f;
    return garrison;
}

static float DefensePower(const Game *g, int p, int a)
{
    float d = 0;
    int defender = g->prov[p].ctrl;
    for (int i = 0; i < MAX_UNITS; i++)
        if (IsDefender(g, &g->units[i], p, a)) d += UnitDefense(g, &g->units[i], p);
    d *= 1 + 0.2f * PROVINCE_TERRAIN[p];
    d += FireSupport(g, defender, p, false, NULL);
    d += Garrison(g, p);
    if (defender == N_FRANCE && g->nat[a].recon > 0) d *= 1.1f;
    return d;
}

static float AttackPower(const Game *g, int p, int a, bool ordered)
{
    float s = 0;
    int defender = g->prov[p].ctrl;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || u->owner != a || UNIT_TYPES[u->type].domain != DOM_LAND) continue;
        if (ordered ? IsAttacker(g, u, p, a) : (IsAdjacent(u->prov, p) && u->target < 0)) s += UnitAttack(g, u, p, defender);
    }
    if (s > 0) s += FireSupport(g, a, p, true, NULL);
    return s;
}

static void AirFactors(const Game *g, int p, int a, float *fa, float *fd)
{
    int defender = g->prov[p].ctrl;
    float airA = AirPower(g, a, defender, p) * (1 - Clampf(g->nat[defender].tech[T_GE] * 0.05f, 0, 0.3f));
    float airD = AirPower(g, defender, a, p) * (1 - Clampf(g->nat[a].tech[T_GE] * 0.05f, 0, 0.3f));
    float effA = fmaxf(0, airA - 0.5f * AntiAir(g, defender, p));
    float effD = fmaxf(0, airD - 0.5f * AntiAir(g, a, p));
    *fa = 1 + 0.35f * effA / (effA + effD + 3);
    *fd = 1 + 0.35f * effD / (effA + effD + 3);
}

/* probabilite que att*U(0.85,1.15) > def*U(0.85,1.15) */
static float Odds(float att, float def)
{
    if (att <= 0) return 0;
    int win = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            if (att * (0.85f + i * 0.02f) > def * (0.85f + j * 0.02f)) win++;
    return win / 256.0f;
}

float War_BattleOdds(const Game *g, int p, int a)
{
    float att = AttackPower(g, p, a, true);
    if (att <= 0) att = AttackPower(g, p, a, false);
    float fa, fd;
    AirFactors(g, p, a, &fa, &fd);
    return Odds(att * fa, DefensePower(g, p, a) * fd);
}

static void Relocate(Game *g, Unit *u)
{
    UnitDomain d = UNIT_TYPES[u->type].domain;
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
        int q = PROVINCE_ADJ[u->prov][k];
        if (War_Friendly(g, u->owner, q) && (d != DOM_SEA || PROVINCE_COASTAL[q])) {
            u->prov = (short)q;
            u->target = -1;
            return;
        }
    }
    int c = d == DOM_SEA ? CoastalBase(g, u->owner) : Capital(g, u->owner);
    if (c < 0) u->active = false;
    else { u->prov = (short)c; u->target = -1; }
}

static void RelocateForeign(Game *g, int p)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (u->active && u->prov == p && !War_Friendly(g, u->owner, p)) Relocate(g, u);
    }
}

void War_Occupy(Game *g, int p, int c)
{
    Province *pr = &g->prov[p];
    int old = pr->ctrl;
    pr->ctrl = c;
    pr->damage = Clampf(pr->damage + 0.2f, 0, 1);
    pr->pop *= 0.98f;
    pr->factoryBuild = 0;
    if (c == N_FRANCE && pr->owner != N_FRANCE) g->statConquered++;
    if (old == N_FRANCE && pr->owner == N_FRANCE) g->statLost++;
    if (c != pr->owner)
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = Clampf(pr->mil[s] + 1.5f, 0, 10);
    RelocateForeign(g, p);
}

static void KillNation(Game *g, int n)
{
    Nation *na = &g->nat[n];
    na->alive = false;
    na->trade = false;
    na->army = 0;
    for (int k = 0; k < N_COUNT; k++) {
        g->war[n][k] = g->war[k][n] = false;
        g->ally[n][k] = g->ally[k][n] = false;
    }
    for (int i = 0; i < MAX_UNITS; i++) if (g->units[i].owner == n) g->units[i].active = false;
    for (int i = 0; i < MAX_GENERALS; i++) if (g->generals[i].owner == n) g->generals[i].active = false;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].ctrl == n) g->prov[p].ctrl = g->prov[p].owner;
    Game_Log(g, EV_WAR, "%s a cessé d'exister en tant qu'État.", NATIONS[n].name);
}

void War_Annex(Game *g, int p, int newOwner)
{
    Province *pr = &g->prov[p];
    int old = pr->owner;
    if (old == N_FRANCE && pr->ctrl == N_FRANCE) g->statLost++;
    pr->owner = pr->ctrl = newOwner;
    pr->integration = pr->core == newOwner ? 100 : 0;
    pr->revolt = 0;
    pr->factoryBuild = 0;
    if (pr->core != newOwner)
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = Clampf(pr->mil[s] + 1, 0, 10);
    RelocateForeign(g, p);
    if (old != N_FRANCE && Soc_NationProvinces(g, old) == 0 && g->nat[old].alive) KillNation(g, old);
}

static void Battle(Game *g, int p, int a)
{
    int defender = g->prov[p].ctrl;
    float att = AttackPower(g, p, a, true);
    if (att <= 0) return;
    float fa, fd;
    AirFactors(g, p, a, &fa, &fd);

    /* actions preparatoires : missiles hypersoniques et forces speciales */
    int hyper = 0, fs = 0;
    FireSupport(g, a, p, true, &hyper);
    for (int i = 0; i < MAX_UNITS; i++)
        if (IsAttacker(g, &g->units[i], p, a) && g->units[i].type == U_FS) fs++;
    float shock = Clampf(hyper * 0.06f, 0, 0.3f) + Clampf(fs * 0.05f, 0, 0.2f);
    for (int i = 0; i < MAX_UNITS; i++)
        if (IsDefender(g, &g->units[i], p, a)) g->units[i].org = Clampf(g->units[i].org - shock, 0, 1);

    float def = DefensePower(g, p, a);
    float r = att * fa * (0.85f + Randf(g) * 0.3f) / (def * fd * (0.85f + Randf(g) * 0.3f));

    int defUnits = 0;
    float defOrg = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (IsDefender(g, u, p, a)) {
            u->org = Clampf(u->org - fminf(0.6f, 0.25f * r), 0, 1);
            u->str = Clampf(u->str - fminf(0.3f, 0.04f * r), 0, 1);
            defUnits++;
            defOrg += u->org;
        } else if (IsAttacker(g, u, p, a)) {
            u->org = Clampf(u->org - fminf(0.6f, 0.25f / r), 0, 1);
            u->str = Clampf(u->str - fminf(0.3f, 0.04f / r), 0, 1);
        }
    }
    /* usure des escadres engagees face a la DCA */
    float aaD = AntiAir(g, defender, p), aaA = AntiAir(g, a, p);
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->training || UNIT_TYPES[u->type].domain != DOM_AIR) continue;
        if (Distance(u->prov, p, UNIT_TYPES[u->type].range, false) < 0) continue;
        if (u->owner == a) { u->org = Clampf(u->org - 0.1f, 0, 1); u->str -= 0.03f * aaD / (aaD + 6); }
        else if (u->owner == defender) { u->org = Clampf(u->org - 0.1f, 0, 1); u->str -= 0.03f * aaA / (aaA + 6); }
    }

    bool won = r > 1 && (defUnits == 0 || defOrg / defUnits < 0.35f);
    Mil_BattleXp(g, a, p, won);
    Mil_BattleXp(g, defender, p, !won);
    if (won) {
        g->battles[a][defender]++;
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (IsDefender(g, u, p, a) || (u->active && u->prov == p && War_AtWar(g, u->owner, a))) {
                u->org *= 0.5f;
                int before = u->prov;
                Relocate(g, u);
                if (u->active && u->prov == before) u->active = false;
            }
        }
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (IsAttacker(g, u, p, a)) {
                u->prov = (short)p;
                u->target = -1;
                u->org = Clampf(u->org - 0.1f, 0, 1);
            }
        }
        if (a == N_FRANCE) { g->statBattlesWon++; Game_Log(g, EV_WAR, "Bataille de %s : victoire, la province est occupée (rapport %.1f contre 1).", PROVINCE_NAMES[p], r); }
        else if (defender == N_FRANCE) { g->statBattlesLost++; Game_Log(g, EV_WAR, "Bataille de %s : %s perce nos lignes (rapport %.1f contre 1) !", PROVINCE_NAMES[p], NATIONS[a].name, r); }
        else if (IsAlly(g, N_FRANCE, defender) || IsAlly(g, N_FRANCE, a))
            Game_Log(g, EV_WAR, "%s occupe %s (%s).", NATIONS[a].name, PROVINCE_NAMES[p], NATIONS[defender].name);
        War_Occupy(g, p, a);
    } else {
        g->battles[defender][a]++;
        for (int i = 0; i < MAX_UNITS; i++)
            if (IsAttacker(g, &g->units[i], p, a)) g->units[i].target = -1;
        if (a == N_FRANCE) {
            g->statBattlesLost++;
            if (r > 1) Game_Log(g, EV_WAR, "Bataille de %s : l'ennemi plie mais tient (rapport %.1f contre 1).", PROVINCE_NAMES[p], r);
            else Game_Log(g, EV_WAR, "Bataille de %s : notre offensive est repoussée (rapport %.1f contre 1).", PROVINCE_NAMES[p], r);
        } else if (defender == N_FRANCE) {
            g->statBattlesWon++;
            Game_Log(g, EV_WAR, "Bataille de %s : l'offensive de %s est repoussée (rapport %.1f contre 1).", PROVINCE_NAMES[p], NATIONS[a].name, r);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Guerre navale et blocus                                             */
/* ------------------------------------------------------------------ */

static void NavalWarfare(Game *g)
{
    for (int a = 0; a < N_COUNT; a++) {
        for (int b = a + 1; b < N_COUNT; b++) {
            if (!g->war[a][b]) continue;
            float pa = 0, pb = 0;
            bool contact = false;
            for (int i = 0; i < MAX_UNITS && !contact; i++) {
                const Unit *u = &g->units[i];
                if (!u->active || u->training || UNIT_TYPES[u->type].domain != DOM_SEA || (u->owner != a && u->owner != b)) continue;
                int enemy = u->owner == a ? b : a;
                if (NavalPower(g, enemy, u->prov, 2) > 0) contact = true;
            }
            if (!contact) continue;
            for (int i = 0; i < MAX_UNITS; i++) {
                const Unit *u = &g->units[i];
                if (!u->active || u->training || UNIT_TYPES[u->type].domain != DOM_SEA) continue;
                float v = UNIT_TYPES[u->type].naval * Readiness(u) * Mil_TechBonus(g, u->owner, u->type);
                if (u->owner == a) pa += v; else if (u->owner == b) pb += v;
            }
            if (pa <= 0 || pb <= 0) continue;
            float r = pa * (0.8f + Randf(g) * 0.4f) / (pb * (0.8f + Randf(g) * 0.4f));
            for (int i = 0; i < MAX_UNITS; i++) {
                Unit *u = &g->units[i];
                if (!u->active || UNIT_TYPES[u->type].domain != DOM_SEA || (u->owner != a && u->owner != b)) continue;
                float hit = u->owner == a ? 1 / r : r;
                u->org = Clampf(u->org - fminf(0.5f, 0.2f * hit), 0, 1);
                u->str -= fminf(0.35f, 0.08f * hit);
                if (u->str < 0.08f) {
                    u->active = false;
                    if (u->owner == N_FRANCE) Game_Log(g, EV_BAD, "Unité navale perdue : %s.", UNIT_TYPES[u->type].name);
                }
            }
            if (a == N_FRANCE || b == N_FRANCE) {
                bool win = (a == N_FRANCE) == (r > 1);
                Game_Log(g, win ? EV_WAR : EV_BAD, "Bataille navale contre %s : %s.", NATIONS[a == N_FRANCE ? b : a].name,
                         win ? "nos flottes dominent" : "nos flottes sont mises en difficulté");
            }
        }
    }

    /* blocus : cotes tenues par une nation, dominees par une flotte ennemie */
    for (int n = 0; n < N_COUNT; n++) {
        float tot = 0, blk = 0;
        if (!g->nat[n].alive || !War_AtWarAny(g, n)) { g->nat[n].blockade = 0; continue; }
        for (int p = 0; p < PROV_COUNT; p++) {
            if (!PROVINCE_COASTAL[p] || g->prov[p].ctrl != n) continue;
            tot += g->prov[p].pop;
            float enemy = 0;
            for (int k = 0; k < N_COUNT; k++) if (g->war[n][k]) enemy += NavalPower(g, k, p, 1);
            if (enemy > 0 && enemy > NavalPower(g, n, p, 1)) blk += g->prov[p].pop;
        }
        g->nat[n].blockade = tot > 0 ? blk / tot : 0;
    }
}

/* ------------------------------------------------------------------ */
/* IA militaire                                                        */
/* ------------------------------------------------------------------ */

static bool CanAttack(const Game *g, int n, int e)
{
    int o = g->prov[e].ctrl;
    if (!War_AtWar(g, n, o)) return false;
    if (o == N_FRANCE && g->prov[e].core == N_FRANCE && g->nat[n].deterred) return false;
    return true;
}

/* valeur d'une province ennemie pour l'IA */
static float TargetValue(const Game *g, int n, int e)
{
    float v = g->prov[e].pop + 2;
    if (g->prov[e].owner == n) v += 8;                     /* reprendre son propre territoire */
    if (e == Capital(g, g->prov[e].ctrl)) v += 5;
    for (int r = 0; r < SR_COUNT; r++) v += Strat_Deposit(e, r) * 2;
    return v;
}

static bool ReadyLand(const Game *g, const Unit *u, int n, const bool *used)
{
    return u->active && u->owner == n && !u->training && !used[u - g->units] && u->org > 0.4f
        && UNIT_TYPES[u->type].domain == DOM_LAND;
}

/* puissance offensive estimee des unites de n adjacentes a e (ou a deux pas, ponderees) */
static float StagedAttack(const Game *g, int n, int e, const bool *used, bool near)
{
    float att = 0;
    int defender = g->prov[e].ctrl;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!ReadyLand(g, u, n, used)) continue;
        if (IsAdjacent(u->prov, e) && CanCross(g, u, u->prov, e)) { att += UnitAttack(g, u, e, defender); continue; }
        if (!near || UNIT_TYPES[u->type].speed < 1) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
            int q = PROVINCE_ADJ[u->prov][k];
            if (War_Friendly(g, n, q) && IsAdjacent(q, e)) { att += 0.8f * UnitAttack(g, u, e, defender); break; }
        }
    }
    return att;
}

static void LaunchAttack(Game *g, int n, int e, bool *used)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (ReadyLand(g, u, n, used) && IsAdjacent(u->prov, e) && CanCross(g, u, u->prov, e)) {
            u->target = (short)e;
            used[i] = true;
        }
    }
}

/* les unites a deux pas se rapprochent de l'objectif */
static void Stage(Game *g, int n, int e, bool *used)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!ReadyLand(g, u, n, used) || IsAdjacent(u->prov, e)) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
            int q = PROVINCE_ADJ[u->prov][k];
            if (War_Friendly(g, n, q) && IsAdjacent(q, e) && War_StackCount(g, n, q, DOM_LAND) < STACK_LAND) {
                u->target = (short)q;
                used[i] = true;
                break;
            }
        }
    }
}

/* menace pesant sur une province amie : attaque ennemie voisine moins notre defense */
static float FrontThreat(const Game *g, int n, int f, int *enemyOut)
{
    float enemy[N_COUNT] = { 0 };
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || UNIT_TYPES[u->type].domain != DOM_LAND || !War_AtWar(g, n, u->owner)) continue;
        if (IsAdjacent(u->prov, f)) enemy[u->owner] += UNIT_TYPES[u->type].atk * Readiness(u);
    }
    int e = -1;
    float tot = 0;
    for (int k = 0; k < N_COUNT; k++) { tot += enemy[k]; if (enemy[k] > 0 && (e < 0 || enemy[k] > enemy[e])) e = k; }
    if (e < 0) return 0;
    if (enemyOut) *enemyOut = e;
    return tot * 1.1f - DefensePower(g, f, e);
}

/* deplacement d'une unite d'un pas vers la destination la plus proche selon des distances BFS */
static void StepToward(Game *g, Unit *u, const int dist[PROV_COUNT])
{
    UnitDomain dom = UNIT_TYPES[u->type].domain;
    int d = dist[u->prov];
    if (d <= 0) return;
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
        int q = PROVINCE_ADJ[u->prov][k];
        if (dist[q] >= 0 && dist[q] < d && War_StackCount(g, u->owner, q, dom) < StackLimit(dom)) { u->target = (short)q; return; }
    }
}

/* distances a travers le territoire ami depuis un ensemble de provinces sources */
static void FriendlyBfs(const Game *g, int n, const bool src[PROV_COUNT], int dist[PROV_COUNT])
{
    int queue[PROV_COUNT], head = 0, tail = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        dist[p] = -1;
        if (src[p] && War_Friendly(g, n, p)) { dist[p] = 0; queue[tail++] = p; }
    }
    while (head < tail) {
        int p = queue[head++];
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
            int q = PROVINCE_ADJ[p][k];
            if (War_Friendly(g, n, q) && dist[q] < 0) { dist[q] = dist[p] + 1; queue[tail++] = q; }
        }
    }
}

static void AiOrders(Game *g, int n)
{
    Nation *na = &g->nat[n];
    na->aiTarget = na->aiThreat = -1;
    if (!War_AtWarAny(g, n)) return;
    static bool used[MAX_UNITS];
    memset(used, 0, sizeof(used));
    float aggro = NATIONS[n].aggressiveness;

    /* 1. le point le plus menace */
    float worst = 0;
    for (int f = 0; f < PROV_COUNT; f++) {
        if (g->prov[f].ctrl != n) continue;
        float t = FrontThreat(g, n, f, NULL);
        if (t > worst) { worst = t; na->aiThreat = (short)f; }
    }

    /* 2. offensive principale : la cible la plus rentable, preparee si besoin */
    int cand[3] = { -1, -1, -1 };
    float cscore[3] = { 0, 0, 0 };
    for (int e = 0; e < PROV_COUNT; e++) {
        if (!CanAttack(g, n, e)) continue;
        float att = StagedAttack(g, n, e, used, true);
        if (att <= 0) continue;
        float sc = att / DefensePower(g, e, n) * sqrtf(TargetValue(g, n, e));
        for (int k = 0; k < 3; k++)
            if (sc > cscore[k]) {
                for (int j = 2; j > k; j--) { cand[j] = cand[j - 1]; cscore[j] = cscore[j - 1]; }
                cand[k] = e; cscore[k] = sc;
                break;
            }
    }
    float need = 0.62f - aggro * 0.2f;   /* les nations agressives prennent plus de risques */
    for (int k = 0; k < 3; k++) {
        int e = cand[k];
        if (e < 0) continue;
        float now = StagedAttack(g, n, e, used, false);
        if (now <= 0 && k > 0) continue;
        float fa, fd;
        AirFactors(g, e, n, &fa, &fd);
        float odds = Odds(now * fa, DefensePower(g, e, n) * fd);
        if (odds >= need) {
            LaunchAttack(g, n, e, used);
            if (na->aiTarget < 0) na->aiTarget = (short)e;
        } else if (k == 0) {
            Stage(g, n, e, used);
            na->aiTarget = (short)e;
        }
    }

    /* 3. reserves : vers le point menace, sinon vers le front */
    bool src[PROV_COUNT] = { false };
    int distThreat[PROV_COUNT], distFront[PROV_COUNT];
    if (na->aiThreat >= 0) src[na->aiThreat] = true;
    FriendlyBfs(g, n, src, distThreat);
    for (int p = 0; p < PROV_COUNT; p++) {
        src[p] = false;
        if (!War_Friendly(g, n, p)) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
            if (Hostile(g, n, PROVINCE_ADJ[p][k])) { src[p] = true; break; }
    }
    FriendlyBfs(g, n, src, distFront);
    int sent = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->owner != n || u->training || used[i] || u->target >= 0) continue;
        UnitDomain dom = UNIT_TYPES[u->type].domain;
        if (dom != DOM_LAND) continue;
        if (na->aiThreat >= 0 && worst > 0 && sent < 6 && distThreat[u->prov] > 0 && distThreat[u->prov] <= 4) {
            StepToward(g, u, distThreat);
            sent++;
        } else {
            StepToward(g, u, distFront);
        }
    }

    /* 4. aviation : a portee de l'offensive, sinon du point menace */
    int focus = na->aiTarget >= 0 ? na->aiTarget : na->aiThreat;
    for (int i = 0; i < MAX_UNITS && focus >= 0; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->owner != n || u->training || UNIT_TYPES[u->type].domain != DOM_AIR) continue;
        int range = UNIT_TYPES[u->type].range;
        if (Distance(u->prov, focus, range, false) >= 0) continue;
        int best = -1, bd = 99;
        for (int q = 0; q < PROV_COUNT; q++) {
            if (!War_Friendly(g, n, q) || War_StackCount(g, n, q, DOM_AIR) >= STACK_AIR) continue;
            if (Distance(q, focus, range, false) < 0) continue;
            int d = Distance(u->prov, q, 4, false);
            if (d >= 0 && d < bd) { bd = d; best = q; }
        }
        if (best >= 0) u->target = (short)best;
    }

    /* 5. flottes : appuyer un debarquement ou bloquer les cotes ennemies */
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->owner != n || u->training || UNIT_TYPES[u->type].domain != DOM_SEA || u->target >= 0) continue;
        int best = -1, bd = 99;
        bool landing = na->aiTarget >= 0 && PROVINCE_COASTAL[na->aiTarget];
        for (int p = 0; p < PROV_COUNT; p++) {
            if (!PROVINCE_COASTAL[p] || !War_Friendly(g, n, p) || War_StackCount(g, n, p, DOM_SEA) >= STACK_SEA) continue;
            if (Distance(u->prov, p, UNIT_TYPES[u->type].speed, true) < 0) continue;
            int d = 99;
            if (landing) d = Distance(p, na->aiTarget, 2, true);
            if (d < 0 || d == 99) {
                for (int e = 0; e < PROV_COUNT; e++) {
                    if (!PROVINCE_COASTAL[e] || !Hostile(g, n, e)) continue;
                    int de = Distance(p, e, 2, true);
                    if (de >= 0 && de + 1 < d) d = de + 1;
                }
            }
            if (d >= 0 && d < bd) { bd = d; best = p; }
        }
        if (best >= 0 && best != u->prov) u->target = (short)best;
    }
}

static void AiRecruit(Game *g, int n)
{
    Nation *na = &g->nat[n];
    if (War_UnitLoad(g, n) >= War_ArmyMax(g, n)) return;
    bool war = War_AtWarAny(g, n);
    if (Randf(g) > (war ? 0.5f : 0.15f)) return;
    bool coastal = CoastalBase(g, n) >= 0;
    int type = PickType(g, n, coastal);
    float cost = UNIT_TYPES[type].cost * 0.5f;
    if (na->treasury < cost && !war) return;
    int site = SpawnSite(g, n, type);
    if (site < 0) return;
    na->treasury -= cost;
    Strat_PayRecruit(g, n, type);
    War_SpawnUnit(g, n, type, site, UNIT_TYPES[type].train);
}

/* ------------------------------------------------------------------ */
/* Guerres, alliances, diplomatie                                      */
/* ------------------------------------------------------------------ */

void War_StartWar(Game *g, int a, int b, bool callAllies)
{
    if (a == b || g->war[a][b] || !g->nat[a].alive || !g->nat[b].alive) return;
    g->war[a][b] = g->war[b][a] = true;
    g->warSince[a][b] = g->warSince[b][a] = (short)g->turn;
    g->truce[a][b] = g->truce[b][a] = 0;
    g->battles[a][b] = g->battles[b][a] = 0;
    g->ally[a][b] = g->ally[b][a] = false;
    AddAiRel(g, a, b, -60);
    if (a == N_FRANCE || b == N_FRANCE) {
        int o = a == N_FRANCE ? b : a;
        g->nat[o].trade = false;
        g->nat[o].warned = false;
        g->nat[o].deterred = Nuke_Deters(g, o);
    }
    if (!callAllies) return;

    /* les allies de la victime sont appeles aux armes */
    for (int k = 0; k < N_COUNT; k++) {
        if (k == a || k == b || !g->ally[b][k] || !g->nat[k].alive || g->war[k][a] || g->truce[k][a] > 0) continue;
        if (k == N_FRANCE) {
            if (g->pendingDilemma < 0) { g->pendingDilemma = DLM_ALLY_CALL; g->dilemmaParam = a; g->dilemmaParam2 = b; }
            continue;
        }
        if (Randf(g) < 0.8f) {
            War_StartWar(g, k, a, false);
            if (a == N_FRANCE || IsAlly(g, N_FRANCE, b))
                Game_Log(g, EV_WAR, "%s honore son alliance avec %s et entre en guerre contre %s.", NATIONS[k].name, NATIONS[b].name, NATIONS[a].name);
        }
    }
    /* solidarite europeenne face a un agresseur exterieur */
    if (g->nat[b].eu && !g->nat[a].eu) {
        if (g->nat[N_FRANCE].eu && a != N_FRANCE && !g->war[N_FRANCE][a] && g->pendingDilemma < 0 && !g->ally[N_FRANCE][b]) {
            g->pendingDilemma = DLM_ALLY_CALL;
            g->dilemmaParam = a;
            g->dilemmaParam2 = b;
        }
        for (int k = 1; k < N_COUNT; k++)
            if (k != b && g->nat[k].eu && g->nat[k].alive && !g->war[k][a] && War_Adjacent(g, k, a) && Randf(g) < 0.2f)
                War_StartWar(g, k, a, false);
    }
}

const char *War_DeclareBlocked(const Game *g, int n)
{
    if (n == N_FRANCE || !g->nat[n].alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "Déjà en guerre";
    if (g->truce[N_FRANCE][n] > 0) return "Trêve en cours";
    if (g->ally[N_FRANCE][n]) return "Nation alliée";
    if (!War_Adjacent(g, N_FRANCE, n)) return "Aucune frontière terrestre ou maritime commune";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool War_Declare(Game *g, int n)
{
    if (War_DeclareBlocked(g, n)) return false;
    Nation *fr = &g->nat[N_FRANCE];
    g->commandPoints--;
    g->aggressionMemory = Clampf(g->aggressionMemory + 25, 0, 100);
    AddRes(g, R_COHESION, -REGIMES[fr->regime].aggressionCost);
    AddRes(g, R_INFLUENCE, -8);
    Game_Log(g, EV_WAR, "La France déclare la guerre à %s !", NATIONS[n].name);
    War_StartWar(g, N_FRANCE, n, true);
    g->nat[n].relation = -80;

    if (g->nat[n].eu && fr->eu) {
        fr->eu = false;
        g->umbrella = false;
        Game_Log(g, EV_BAD, "Agression contre un État membre : la France est exclue de l'Union européenne.");
        for (int k = 1; k < N_COUNT; k++) {
            Nation *o = &g->nat[k];
            if (!o->alive || !o->eu || k == n) continue;
            AddRelation(g, k, -30);
            if (!g->war[k][N_FRANCE] && o->army >= 2 && War_Adjacent(g, N_FRANCE, k) && Randf(g) < 0.3f) {
                War_StartWar(g, k, N_FRANCE, false);
                Game_Log(g, EV_WAR, "%s rejoint la coalition contre la France !", NATIONS[k].name);
            }
        }
    }
    return true;
}

const char *War_DiploBlocked(const Game *g, int n, int action)
{
    const Nation *na = &g->nat[n];
    if (!na->alive || n == N_FRANCE) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "En guerre";
    if (na->actedThisTurn) return "Déjà une action diplomatique ce tour";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (action == 0 && g->res[R_INFLUENCE] < 5) return "Influence insuffisante (5)";
    if (action == 1) {
        if (na->trade) return "Accord déjà en vigueur";
        if (na->embargo) return "Levez d'abord l'embargo";
        if (na->relation < 25) return "Relation insuffisante (25)";
    }
    if (action == 2) {
        if (g->ally[N_FRANCE][n]) return "Déjà alliée";
        if (na->relation < 60) return "Relation insuffisante (60)";
    }
    return NULL;
}

bool War_Diplomacy(Game *g, int n, int action)
{
    if (War_DiploBlocked(g, n, action)) return false;
    Nation *na = &g->nat[n];
    g->commandPoints--;
    na->actedThisTurn = true;
    if (action == 0) {
        AddRes(g, R_INFLUENCE, -5);
        AddRelation(g, n, 12);
        Game_Log(g, EV_GOOD, "Rapprochement diplomatique avec %s.", NATIONS[n].name);
    } else if (action == 1) {
        na->trade = true;
        Game_Log(g, EV_GOOD, "Accord commercial signé avec %s : la France est servie en priorité sur leurs excédents.", NATIONS[n].name);
    } else {
        g->ally[N_FRANCE][n] = g->ally[n][N_FRANCE] = true;
        Game_Log(g, EV_GOOD, "Alliance défensive conclue avec %s.", NATIONS[n].name);
    }
    return true;
}

const char *War_UltimatumBlocked(const Game *g, int p)
{
    int n = g->prov[p].owner;
    if (n == N_FRANCE || !g->nat[n].alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "Déjà en guerre : il faut l'occuper puis négocier";
    if (g->truce[N_FRANCE][n] > 0) return "Trêve en cours";
    if (g->ally[N_FRANCE][n]) return "Nation alliée";
    if (!TouchesNation(g, p, N_FRANCE)) return "Province non frontalière";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool War_Ultimatum(Game *g, int p)
{
    if (War_UltimatumBlocked(g, p)) return false;
    int n = g->prov[p].owner;
    g->commandPoints--;
    float ours = g->nat[N_FRANCE].army * War_Quality(g, N_FRANCE);
    float theirs = g->nat[n].army * War_Quality(g, n);
    for (int k = 1; k < N_COUNT; k++) if (k != n && g->ally[n][k] && g->nat[k].alive) theirs += g->nat[k].army * 0.5f;
    if (g->nukePosture == 2 && !Nuke_IsPower(g, n)) ours *= 1 + g->credibility / 200.0f;
    g->aggressionMemory = Clampf(g->aggressionMemory + 12, 0, 100);
    if (ours > theirs * 2.5f) {
        Game_Log(g, EV_WAR, "Ultimatum : %s cède %s sans combat.", NATIONS[n].name, PROVINCE_NAMES[p]);
        AddRelation(g, n, -40);
        AddRes(g, R_COHESION, -REGIMES[g->nat[N_FRANCE].regime].aggressionCost * 0.3f);
        War_Annex(g, p, N_FRANCE);
        g->statConquered++;
        return true;
    }
    AddRelation(g, n, -25);
    AddRes(g, R_INFLUENCE, -5);
    Game_Log(g, EV_BAD, "Ultimatum rejeté par %s. Notre crédibilité en souffre.", NATIONS[n].name);
    return false;
}

/* ------------------------------------------------------------------ */
/* Score de guerre et traites                                          */
/* ------------------------------------------------------------------ */

static float Value(const Game *g, int p) { return g->prov[p].pop + 2; }

static float NationValue(const Game *g, int n)
{
    float v = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) v += Value(g, p);
    return v > 0 ? v : 1;
}

/* score de guerre de a contre b (-100..100) */
float War_Score(const Game *g, int a, int b)
{
    float occA = 0, occB = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner == b && pr->ctrl == a) occA += Value(g, p);
        if (pr->owner == a && pr->ctrl == b) occB += Value(g, p);
    }
    float s = occA / NationValue(g, b) * 100 - occB / NationValue(g, a) * 100;
    s += Clampf((g->battles[a][b] - g->battles[b][a]) * 3.0f, -25, 25);
    s += (g->nat[b].blockade - g->nat[a].blockade) * 10;
    return Clampf(s, -100, 100);
}

static float CedeCost(const Game *g, int p)
{
    int loser = g->prov[p].owner;
    float c = Value(g, p) / NationValue(g, loser) * 100;
    int winner = g->treaty.winner;
    return g->prov[p].ctrl == winner ? c : c * 2;   /* exiger une province non occupee coute double */
}

void War_TreatyDraft(Game *g, int other)
{
    Treaty *t = &g->treaty;
    memset(t, 0, sizeof(*t));
    t->active = true;
    t->other = other;
    float s = War_Score(g, N_FRANCE, other);
    t->winner = s >= 0 ? N_FRANCE : other;
    int loser = t->winner == N_FRANCE ? other : N_FRANCE;
    float budget = fabsf(s);
    /* par defaut : les provinces occupees, les plus precieuses d'abord, dans la limite du score */
    for (;;) {
        int best = -1;
        for (int p = 0; p < PROV_COUNT; p++)
            if (!t->cede[p] && g->prov[p].owner == loser && g->prov[p].ctrl == t->winner && CedeCost(g, p) <= budget
                && (best < 0 || Value(g, p) > Value(g, best)))
                best = p;
        if (best < 0) break;
        budget -= CedeCost(g, best);
        t->cede[best] = true;
    }
    t->reparationTurns = 16;
    if (budget >= 20) t->reparations = 4;
    else if (budget >= 10) t->reparations = 2;
}

float War_TreatyCost(const Game *g, const Treaty *t)
{
    float c = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (t->cede[p]) c += CedeCost(g, p);
    return c + t->reparations * 5;
}

const char *War_TreatyBlocked(const Game *g, const Treaty *t)
{
    static char buf[120];
    if (!t->active || !g->war[N_FRANCE][t->other]) return "Pas en guerre";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    float s = War_Score(g, N_FRANCE, t->other);
    float cost = War_TreatyCost(g, t);
    if (t->winner == N_FRANCE && cost > s + 5) {
        snprintf(buf, sizeof(buf), "Exigences trop lourdes : %.0f points pour un score de guerre de %.0f", cost, s);
        return buf;
    }
    if (t->winner != N_FRANCE && cost < -s - 10 && War_Turns(g, N_FRANCE, t->other) < 16) {
        snprintf(buf, sizeof(buf), "L'ennemi exige au moins %.0f points de concessions", -s - 10);
        return buf;
    }
    return NULL;
}

void War_WhitePeace(Game *g, int a, int b, bool log)
{
    g->war[a][b] = g->war[b][a] = false;
    g->truce[a][b] = g->truce[b][a] = 16;
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if ((pr->owner == a && pr->ctrl == b) || (pr->owner == b && pr->ctrl == a)) {
            pr->ctrl = pr->owner;
            RelocateForeign(g, p);
        }
    }
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active) continue;
        if ((u->owner == a || u->owner == b) && !War_Friendly(g, u->owner, u->prov)) Relocate(g, u);
        if ((u->owner == a || u->owner == b) && u->target >= 0) u->target = -1;
    }
    AddAiRel(g, a, b, 30);
    if (log) Game_Log(g, EV_WAR, "Paix signée entre %s et %s. Trêve de 4 ans.", NATIONS[a].name, NATIONS[b].name);
}

void War_SignTreaty(Game *g, const Treaty *t)
{
    int other = t->other;
    int loser = t->winner == N_FRANCE ? other : N_FRANCE;
    int ceded = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (!t->cede[p] || g->prov[p].owner != loser) continue;
        War_Annex(g, p, t->winner);
        ceded++;
    }
    if (t->reparations > 0) {
        Nation *o = &g->nat[other];
        o->reparations = t->winner == N_FRANCE ? t->reparations : -t->reparations;
        o->reparationTurns = t->reparationTurns;
    }
    if (g->nat[other].alive) War_WhitePeace(g, N_FRANCE, other, false);
    Game_Log(g, EV_WAR, "Traité de paix avec %s : %d province(s) cédée(s) à %s%s.", NATIONS[other].name, ceded,
             NATIONS[t->winner].name, t->reparations > 0 ? ", réparations" : "");
    g->treaty.active = false;
}

bool War_ProposeTreaty(Game *g)
{
    Treaty *t = &g->treaty;
    if (War_TreatyBlocked(g, t)) return false;
    g->commandPoints--;
    War_SignTreaty(g, t);
    return true;
}

/* lassitude de n face a la France : ce qu'elle est prete a ceder (ou a renoncer a exiger) en plus du score */
float War_Exhaustion(const Game *g, int n)
{
    const Nation *na = &g->nat[n];
    float e = Clampf((War_Turns(g, N_FRANCE, n) - 6) * 1.5f, 0, 15);
    e += na->blockade * 15;
    if (na->army < War_ArmyMax(g, n) * 0.4f) e += 10;
    float mil = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n) { mil += Soc_ProvinceMilitancy(&g->prov[p]) * g->prov[p].pop; pop += g->prov[p].pop; }
    if (pop > 0 && mil / pop > 5) e += 8;
    if (na->failed) e += 10;
    if (na->outage > 0) e += 4;
    if (Nuke_IsPower(g, N_FRANCE) && !Nuke_IsPower(g, n)) e += g->credibility / 20;
    if (IsDemocratic(na->regime)) e += 4;     /* l'opinion pese */
    if (na->regime == RG_TOTALITAIRE || na->regime == RG_JUNTE) e -= 5;
    return Clampf(e, 0, 40);
}

/* negociation : l'IA accepte, refuse ou fait une contre-proposition */
int War_Negotiate(Game *g)
{
    Treaty *t = &g->treaty;
    int n = t->other;
    if (!t->active || !g->war[N_FRANCE][n]) return 0;
    if (g->commandPoints < 1) return 0;
    Nation *na = &g->nat[n];
    if (na->negoTurn != g->turn) { na->negoTurn = g->turn; na->negoRounds = 0; }
    if (na->negoRounds >= 3) {
        Game_Log(g, EV_BAD, "%s quitte la table des négociations pour ce trimestre.", NATIONS[n].name);
        return 0;
    }
    if (na->negoRounds == 0) g->commandPoints--;
    na->negoRounds++;
    float s = War_Score(g, N_FRANCE, n), ex = War_Exhaustion(g, n), cost = War_TreatyCost(g, t);
    bool frWins = t->winner == N_FRANCE;
    /* ce que l'ennemi accepte : nos exigences dans la limite du score et de sa lassitude ;
       s'il gagne, il se contente de moins quand il est las */
    float limit = frWins ? s + 5 + ex : -s - 10 - ex;
    bool ok = frWins ? cost <= limit : cost >= limit;
    if (ok) {
        War_SignTreaty(g, t);
        return 1;
    }
    /* contre-proposition */
    if (frWins) {
        while (War_TreatyCost(g, t) > limit) {
            if (t->reparations > 0) { t->reparations -= 2; if (t->reparations < 0.1f) t->reparations = 0; continue; }
            /* rendre d'abord la province la plus couteuse non occupee, puis la plus couteuse */
            int drop = -1;
            float worst = -1;
            for (int p = 0; p < PROV_COUNT; p++) {
                if (!t->cede[p]) continue;
                float c = CedeCost(g, p) * (g->prov[p].ctrl == N_FRANCE ? 1 : 3);
                if (c > worst) { worst = c; drop = p; }
            }
            if (drop < 0) break;
            t->cede[drop] = false;
        }
    } else {
        /* l'ennemi exige davantage : nos provinces qu'il occupe, puis des reparations */
        for (int p = 0; p < PROV_COUNT && War_TreatyCost(g, t) < limit; p++)
            if (g->prov[p].owner == N_FRANCE && g->prov[p].ctrl == n && !t->cede[p]) t->cede[p] = true;
        while (War_TreatyCost(g, t) < limit && t->reparations < 6) { t->reparations += 2; t->reparationTurns = 16; }
        for (int p = 0; p < PROV_COUNT && War_TreatyCost(g, t) < limit; p++)
            if (g->prov[p].owner == N_FRANCE && !t->cede[p] && TouchesNation(g, p, n)) t->cede[p] = true;
    }
    t->countered = true;
    Game_Log(g, EV_WAR, "%s fait une contre-proposition (lassitude %.0f).", NATIONS[n].name, ex);
    return 2;
}

/* paix entre deux IA : le vainqueur annexe ce qu'il occupe, dans la limite du score */
static void AiPeace(Game *g, int a, int b)
{
    float s = War_Score(g, a, b);
    int winner = s >= 0 ? a : b, loser = s >= 0 ? b : a;
    float budget = fabsf(s);
    int ceded = 0;
    for (int p = 0; p < PROV_COUNT && budget > 0; p++) {
        if (g->prov[p].owner != loser || g->prov[p].ctrl != winner) continue;
        float c = Value(g, p) / NationValue(g, loser) * 100;
        if (c > budget) continue;
        budget -= c;
        War_Annex(g, p, winner);
        ceded++;
    }
    if (g->nat[a].alive && g->nat[b].alive) War_WhitePeace(g, a, b, false);
    if (ceded) Game_Log(g, EV_WAR, "Paix entre %s et %s : %s annexe %d province(s).", NATIONS[a].name, NATIONS[b].name,
                        NATIONS[winner].name, ceded);
    else Game_Log(g, EV_WAR, "Paix blanche entre %s et %s.", NATIONS[a].name, NATIONS[b].name);
}

/* l'IA propose (ou impose) un traite a la France */
static void AiPeaceOfferToFrance(Game *g, int n)
{
    War_TreatyDraft(g, n);
    g->treaty.proposedByAi = true;
    g->pendingDilemma = DLM_PEACE_OFFER;
    g->dilemmaParam = n;
}

/* une nation qui ne tient plus aucune de ses provinces capitule */
static void Capitulations(Game *g)
{
    for (int n = 1; n < N_COUNT; n++) {
        if (!g->nat[n].alive || !War_AtWarAny(g, n)) continue;
        bool holds = false;
        for (int p = 0; p < PROV_COUNT && !holds; p++) if (g->prov[p].owner == n && g->prov[p].ctrl == n) holds = true;
        if (holds) continue;
        Game_Log(g, EV_WAR, "%s capitule.", NATIONS[n].name);
        for (int p = 0; p < PROV_COUNT; p++) {
            int c = g->prov[p].ctrl;
            if (g->prov[p].owner == n && c != n && g->nat[n].alive) War_Annex(g, p, c);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

static float RelationTarget(const Game *g, int n)
{
    const Nation *na = &g->nat[n], *fr = &g->nat[N_FRANCE];
    float t = NATIONS[n].relation;
    t += IsDemocratic(na->regime) == IsDemocratic(fr->regime) ? 10 : -15;
    if (na->regime == fr->regime) t += 5;
    if (na->eu && fr->eu) t += 15;
    if (g->umbrella && na->eu) t += 10;
    if (g->ally[N_FRANCE][n]) t += 15;
    if (na->embargo) t -= 20;
    int cores = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE && g->prov[p].core == n) cores++;
    t -= cores * 35 > 80 ? 80 : cores * 35;
    t -= g->aggressionMemory * 0.4f + g->nukesUsed * 40 + g->tacticalUsed * 15;
    if (na->trade) t += 5;
    if (n >= N_MAROC || n == N_TURQUIE) {
        if (g->migrationDoctrine == 0) t += 8;
        if (g->migrationDoctrine == 2) t -= 12;
    }
    return Clampf(t, -100, 100);
}

static float AiRelTarget(const Game *g, int a, int b)
{
    float t = 0;
    if (g->nat[a].eu && g->nat[b].eu) t += 35;
    t += IsDemocratic(g->nat[a].regime) == IsDemocratic(g->nat[b].regime) ? 8 : -8;
    if (g->ally[a][b]) t += 20;
    if (g->truce[a][b] > 0) t -= 30;
    /* rivalites historiques */
    static const int RIVALS[][3] = {
        { N_BOREALE, N_UKRAINE, -80 }, { N_BOREALE, N_BALTES, -60 }, { N_BOREALE, N_POLOGNE, -60 },
        { N_BOREALE, N_SCANDINAVIE, -35 }, { N_TURQUIE, N_GRECE, -40 }, { N_ALGERIE, N_MAROC, -45 },
        { N_EGYPTE, N_LIBYE, -20 }, { N_BOREALE, N_BALKANS, 35 }, { N_BOREALE, N_ROUMANIE, -30 },
        { N_UK, N_IRLANDE, 20 }, { N_ESPAGNE, N_PORTUGAL, 30 }, { N_EUROPE_C, N_ROUMANIE, -15 },
    };
    for (unsigned i = 0; i < sizeof(RIVALS) / sizeof(RIVALS[0]); i++)
        if ((RIVALS[i][0] == a && RIVALS[i][1] == b) || (RIVALS[i][0] == b && RIVALS[i][1] == a)) t += RIVALS[i][2];
    /* annexions recentes : la rancune */
    for (int p = 0; p < PROV_COUNT; p++)
        if ((g->prov[p].owner == a && g->prov[p].core == b) || (g->prov[p].owner == b && g->prov[p].core == a)) t -= 25;
    return Clampf(t, -100, 100);
}

static float Coalition(const Game *g, int n)
{
    float p = g->nat[n].army * War_Quality(g, n);
    for (int k = 0; k < N_COUNT; k++) if (k != n && g->ally[n][k] && g->nat[k].alive) p += g->nat[k].army * 0.6f;
    return p;
}

static void Movements(Game *g)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->training || u->target < 0) continue;
        const UnitDef *d = &UNIT_TYPES[u->type];
        int t = u->target;
        if (d->domain != DOM_LAND) {
            bool ok = War_Friendly(g, u->owner, t) && War_StackCount(g, u->owner, t, d->domain) < StackLimit(d->domain)
                   && (d->domain == DOM_AIR ? Distance(u->prov, t, 4, false) >= 0
                                            : PROVINCE_COASTAL[t] && Distance(u->prov, t, d->speed, true) >= 0);
            if (ok) { u->prov = (short)t; u->org = Clampf(u->org - 0.1f, 0, 1); }
            u->target = -1;
            continue;
        }
        int path[PROV_COUNT];
        int len = LandPath(g, u, t, path, d->speed);
        if (!len) { u->target = -1; continue; }
        for (int s = 0; s < len; s++) {
            int q = path[s];
            if (!War_Friendly(g, u->owner, q)) break;   /* province ennemie : on attaque depuis la position atteinte */
            if (War_StackCount(g, u->owner, q, DOM_LAND) >= STACK_LAND) break;
            u->prov = (short)q;
            u->org = Clampf(u->org - 0.05f, 0, 1);
        }
        if (u->prov == t) u->target = -1;
        else if (Hostile(g, u->owner, t)) { if (!IsAdjacent(u->prov, t)) u->target = -1; }   /* sinon : assaut */
        else if (!War_Friendly(g, u->owner, t)) u->target = -1;
        /* destination amie non atteinte (empilement plein) : on reessaiera au tour suivant */
    }
}

static void Battles(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        for (int a = 0; a < N_COUNT; a++) {
            bool any = false;
            for (int i = 0; i < MAX_UNITS && !any; i++) any = IsAttacker(g, &g->units[i], p, a);
            if (any && Hostile(g, a, p)) Battle(g, p, a);
        }
    }
}

static void Supply(Game *g)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active) continue;
        if (u->training) {
            if (--u->training == 0) {
                u->org = 1;
                if (u->owner == N_FRANCE)
                    Game_Log(g, EV_GOOD, "%s opérationnelle en %s.", UNIT_TYPES[u->type].name, PROVINCE_NAMES[u->prov]);
            }
            continue;
        }
        int n = u->owner;
        bool goods = g->eco[n].sat[G_GOODS] >= 0.7f;
        float recov = 0.15f * (g->nat[n].regime == RG_JUNTE ? 1.3f : 1.0f) * (goods ? 1 : 0.5f)
                    * (1 + 0.03f * g->nat[n].tech[T_LOG]) * Mil_GeneralBonus(g, n, u->prov, -1, false)
                    * (g->nat[n].outage > 0 ? 0.6f : 1.0f);
        bool blockaded = PROVINCE_COASTAL[u->prov] && g->nat[n].blockade > 0.5f && UNIT_TYPES[u->type].domain == DOM_SEA;
        if (War_Friendly(g, n, u->prov)) {
            u->org = Clampf(u->org + recov, 0, 1);
            bool funds = n != N_FRANCE || g->res[R_BUDGET] > 0;
            if (!blockaded) u->str = Clampf(u->str + (funds ? 0.05f : 0.02f), 0, 1);
        } else if (TouchesNation(g, u->prov, n)) {
            u->org = Clampf(u->org + 0.05f, 0, 1);
        } else {
            float attr = 0.02f * (1 - 0.1f * g->nat[n].tech[T_LOG]);
            u->org = Clampf(u->org - 0.08f, 0, 1);
            u->str = Clampf(u->str - attr, 0, 1);
        }
        /* partisans dans les provinces occupees */
        const Province *pr = &g->prov[u->prov];
        if (pr->ctrl == n && pr->owner != n && Soc_ProvinceMilitancy(pr) > 5) u->org = Clampf(u->org - 0.04f, 0, 1);
        if (PROVINCES[u->prov].aridity >= 1.0f && pr->ctrl != n) u->str -= 0.01f;
        if (u->str < 0.08f) {
            u->active = false;
            if (n == N_FRANCE) Game_Log(g, EV_BAD, "Une unité %s a été anéantie.", UNIT_TYPES[u->type].name);
        }
    }
}

static void Alliances(Game *g)
{
    for (int a = 1; a < N_COUNT; a++) {
        if (!g->nat[a].alive) continue;
        for (int b = a + 1; b < N_COUNT; b++) {
            if (!g->nat[b].alive) continue;
            if (!g->war[a][b]) g->airel[a][b] = g->airel[b][a] =
                Clampf(g->airel[a][b] + (AiRelTarget(g, a, b) - g->airel[a][b]) * 0.05f, -100, 100);
            if (g->ally[a][b] && g->airel[a][b] < 10) {
                g->ally[a][b] = g->ally[b][a] = false;
                Game_Log(g, EV_INFO, "L'alliance entre %s et %s est rompue.", NATIONS[a].name, NATIONS[b].name);
            } else if (!g->ally[a][b] && !g->war[a][b] && g->airel[a][b] > 55 && Randf(g) < 0.03f) {
                /* un ennemi commun rapproche */
                bool rival = false;
                for (int k = 0; k < N_COUNT && !rival; k++)
                    if (k != a && k != b && g->nat[k].alive && Relation(g, a, k) < -30 && Relation(g, b, k) < -30) rival = true;
                if (rival) {
                    g->ally[a][b] = g->ally[b][a] = true;
                    Game_Log(g, EV_EVENT, "Alliance défensive entre %s et %s.", NATIONS[a].name, NATIONS[b].name);
                }
            }
        }
    }
    /* la France perd ses allies si la relation s'effondre */
    for (int n = 1; n < N_COUNT; n++)
        if (g->ally[N_FRANCE][n] && g->nat[n].relation < 20) {
            g->ally[N_FRANCE][n] = g->ally[n][N_FRANCE] = false;
            Game_Log(g, EV_BAD, "%s rompt son alliance avec la France.", NATIONS[n].name);
        }
}

void War_Turn(Game *g)
{
    if (g->aggressionMemory > 0) g->aggressionMemory -= 0.6f;
    for (int a = 0; a < N_COUNT; a++)
        for (int b = 0; b < N_COUNT; b++)
            if (g->truce[a][b] > 0) g->truce[a][b]--;
    Alliances(g);

    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;

        float hab = 0, pop = 0;
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == n) { hab += g->prov[p].hab * g->prov[p].pop; pop += g->prov[p].pop; }
        hab = pop > 0 ? hab / pop : 50;
        if (!na->failed && hab < 35) {
            na->failed = true;
            na->regime = RG_JUNTE;
            na->eu = false;
            Game_Log(g, EV_CLIMATE, "Effondrement : %s devient un État failli, une junte prend le pouvoir.", NATIONS[n].name);
        }

        if (!g->war[N_FRANCE][n]) {
            na->relation += (RelationTarget(g, n) - na->relation) * 0.08f + (Randf(g) - 0.5f) * 2;
            na->relation = Clampf(na->relation, -100, 100);
            if (na->trade && na->relation < 10) {
                na->trade = false;
                Game_Log(g, EV_BAD, "%s rompt l'accord commercial.", NATIONS[n].name);
            }
        }

        float aggro = NATIONS[n].aggressiveness * DiffAggro(g);
        float deter = 1 - g->credibility / 100.0f * (Nuke_IsPower(g, n) ? 0.4f : 0.8f);

        /* guerre contre la France */
        if (g->war[N_FRANCE][n]) {
            int wt = War_Turns(g, N_FRANCE, n);
            if (wt > 0 && wt % 8 == 0) na->deterred = Nuke_Deters(g, n);
            float s = War_Score(g, N_FRANCE, n);
            bool wants = (fabsf(s) >= 25 && wt >= 4) || na->army < War_ArmyMax(g, n) * 0.25f || (wt > 16 && Randf(g) < 0.1f);
            if (wants && g->pendingDilemma < 0 && Randf(g) < 0.35f) AiPeaceOfferToFrance(g, n);
        } else if (g->truce[N_FRANCE][n] == 0 && !g->ally[N_FRANCE][n] && War_Adjacent(g, n, N_FRANCE)) {
            if (g->aggressionMemory > 35 && na->relation < 0 && Randf(g) < (0.03f + g->aggressionMemory * 0.001f) * deter) {
                War_StartWar(g, n, N_FRANCE, true);
                Game_Log(g, EV_WAR, "Face à l'expansionnisme français, %s nous déclare la guerre !", NATIONS[n].name);
            } else if (na->relation < -55 && Coalition(g, n) > Coalition(g, N_FRANCE) * 1.2f && Randf(g) < aggro * 0.15f * deter) {
                War_StartWar(g, n, N_FRANCE, true);
                Game_Log(g, EV_WAR, "%s déclare la guerre à la France !", NATIONS[n].name);
            }
        }

        /* guerres entre nations */
        for (int t = 1; t < N_COUNT; t++) {
            if (t == n || !g->war[n][t] || t < n) continue;
            int wt = War_Turns(g, n, t);
            float s = War_Score(g, n, t);
            if (wt > 14 || fabsf(s) > 60 || (wt > 6 && Randf(g) < 0.05f)) AiPeace(g, n, t);
        }
        if (g->turn > 8 && Randf(g) < aggro * 0.015f * (na->failed ? 2 : 1)) {
            int best = -1;
            float bestPow = 1e9f;
            for (int t = 1; t < N_COUNT; t++) {
                const Nation *tg = &g->nat[t];
                if (t == n || !tg->alive || g->war[n][t] || g->truce[n][t] || g->ally[n][t] || (tg->eu && na->eu)) continue;
                if (g->airel[n][t] > -15 || !War_Adjacent(g, n, t)) continue;
                if (Nuke_IsPower(g, t) && !Nuke_IsPower(g, n)) continue;
                if (Coalition(g, n) < Coalition(g, t) * 1.5f) continue;
                if (tg->eu && g->umbrella && Randf(g) < g->credibility / 100.0f) continue;
                if (Coalition(g, t) < bestPow) { bestPow = Coalition(g, t); best = t; }
            }
            if (best >= 0) {
                Game_Log(g, EV_WAR, "%s envahit %s !", NATIONS[n].name, NATIONS[best].name);
                War_StartWar(g, n, best, true);
            }
        }

        AiRecruit(g, n);
        AiOrders(g, n);
    }

    Movements(g);
    NavalWarfare(g);
    Battles(g);
    Supply(g);
    Capitulations(g);
    UpdateArmyPower(g);
}
