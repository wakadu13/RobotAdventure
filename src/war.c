/*
 * BOUCLIER 2050 - guerre et diplomatie, inspirees de Hearts of Iron :
 * divisions positionnees sur les provinces, ordres de mouvement et
 * d'attaque, batailles (organisation, effectifs, terrain, debarquements),
 * ravitaillement et attrition, IA des fronts, declarations, paix,
 * ultimatums, coalitions.
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
    if (a == b || a < 0 || b < 0) return false;
    if (a == N_FRANCE) return g->nat[b].atWar;
    if (b == N_FRANCE) return g->nat[a].atWar;
    return g->nat[a].warTarget == b || g->nat[b].warTarget == a;
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

/* province la plus peuplee du coeur national (sinon n'importe laquelle) */
static int Capital(const Game *g, int n)
{
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != n) continue;
        if (best < 0 || (pr->core == n && g->prov[best].core != n)
            || (pr->core == g->prov[best].core && pr->pop > g->prov[best].pop))
            best = p;
    }
    return best;
}

static bool IsAdjacent(int a, int b)
{
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[a][k] >= 0; k++)
        if (PROVINCE_ADJ[a][k] == b) return true;
    return false;
}

/* la province p touche-t-elle un territoire de la nation n ? (et par la mer seulement ?) */
static bool TouchesNation(const Game *g, int p, int n, bool *seaOnly)
{
    bool land = false, sea = false;
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
        int q = PROVINCE_ADJ[p][k];
        if (g->prov[q].owner != n) continue;
        if (Map_SeaLink(p, q)) sea = true; else land = true;
    }
    if (seaOnly) *seaOnly = !land && sea;
    return land || sea;
}

/* ------------------------------------------------------------------ */
/* Puissance                                                           */
/* ------------------------------------------------------------------ */

float War_ArmyMax(const Game *g, int n)
{
    /* seule la population integree se mobilise vraiment */
    float pop = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != n) continue;
        pop += pr->pop * (pr->core == n ? 1.0f : 0.25f * pr->integration / 100.0f);
    }
    float m = pop * REGIMES[g->nat[n].regime].mobilization;
    if (n == N_FRANCE) {
        m *= 1.3f;   /* armee professionnelle et reserves operationnelles */
        if (g->mobilizationTurns > 0) m *= 1.5f;
        for (int k = 1; k < N_COUNT; k++)
            if (g->nat[k].atWar) { m *= 1.4f; break; }   /* rappel des reservistes */
    } else {
        m *= Clampf(0.4f + AvgWealth(g, n) * 1.5f, 0.4f, 1.0f);
        if (g->nat[n].failed) m *= 0.5f;
        if (g->nat[n].atWar) m *= 1.5f;                 /* mobilisation generale face a la France */
        m *= 1 + g->aggressionMemory / 50.0f;           /* l'Europe se rearme face a un agresseur */
    }
    return m;
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
        /* synergie M2MC : l'air, l'espace, le spectre et le cyber demultiplient les forces */
        q *= 1 + 0.015f * (g->capacity[D_AIR] + g->capacity[D_ESPACE] + g->capacity[D_EM] + g->capacity[D_CYBER]);
        if (g->proj[P_CONSTELL].state == 2) q *= 1.05f;
        if (g->aiDefense) q *= 1.05f;
    } else {
        q *= Clampf(0.7f + AvgWealth(g, n) * 0.8f, 0.7f, 1.2f);
    }
    if (g->eco[n].sat[G_GOODS] < 0.7f) q *= 0.85f;      /* penurie de materiel */
    return q;
}

bool War_Adjacent(const Game *g, int a, int b)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == a && TouchesNation(g, p, b, NULL)) return true;
    return false;
}

bool War_ProvinceAttackable(const Game *g, int p)
{
    int n = g->prov[p].owner;
    return n != N_FRANCE && g->nat[n].atWar && TouchesNation(g, p, N_FRANCE, NULL);
}

static void UpdateArmyPower(Game *g)
{
    for (int n = 0; n < N_COUNT; n++) g->nat[n].army = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training) continue;
        const UnitDef *d = &UNIT_TYPES[u->type];
        g->nat[u->owner].army += u->str * (d->atk + d->def) / 6.0f;
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

static int PickType(Game *g, int n)
{
    float w = AvgWealth(g, n), r = Randf(g);
    if (w > 0.25f) return r < 0.5f ? U_INF : r < 0.75f ? U_MECA : r < 0.92f ? U_BLINDE : U_MARINE;
    return r < 0.75f ? U_INF : r < 0.92f ? U_MECA : U_BLINDE;
}

void War_InitUnits(Game *g)
{
    memset(g->units, 0, sizeof(g->units));
    War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_NORD, 0);
    War_SpawnUnit(g, N_FRANCE, U_MECA, PV_FR_NORD, 0);
    War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_EST, 0);
    War_SpawnUnit(g, N_FRANCE, U_BLINDE, PV_FR_EST, 0);
    War_SpawnUnit(g, N_FRANCE, U_MARINE, PV_FR_SUD, 0);
    War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_OUEST, 0);

    for (int n = 1; n < N_COUNT; n++) {
        int provs[PROV_COUNT], c = 0;
        for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) provs[c++] = p;
        /* tri par population decroissante */
        for (int i = 0; i < c; i++)
            for (int j = i + 1; j < c; j++)
                if (g->prov[provs[j]].pop > g->prov[provs[i]].pop) { int t = provs[i]; provs[i] = provs[j]; provs[j] = t; }
        int count = (int)(War_ArmyMax(g, n) * 0.8f + 0.5f);
        if (count < 1) count = 1;
        for (int k = 0; k < count; k++) War_SpawnUnit(g, n, PickType(g, n), provs[k % c], 0);
    }
    UpdateArmyPower(g);
}

float War_RecruitCost(const Game *g, int type)
{
    return UNIT_TYPES[type].cost * (g->nat[N_FRANCE].regime == RG_JUNTE ? 0.75f : 1.0f);
}

const char *War_RecruitBlocked(const Game *g, int type, int p)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (p < 0 || g->prov[p].owner != N_FRANCE) return "Choisissez une province française";
    if (g->prov[p].core != N_FRANCE) return "Recrutement impossible en territoire non intégré";
    if (g->prov[p].revolt) return "Province en révolte";
    if (War_UnitCount(g, N_FRANCE) + 1 > (int)War_ArmyMax(g, N_FRANCE)) return "Plafond de mobilisation atteint";
    if (g->res[R_BUDGET] < War_RecruitCost(g, type)) return "Budget insuffisant";
    return NULL;
}

bool War_Recruit(Game *g, int type, int p)
{
    if (War_RecruitBlocked(g, type, p)) return false;
    if (War_SpawnUnit(g, N_FRANCE, type, p, UNIT_TYPES[type].train) < 0) return false;
    g->res[R_BUDGET] -= War_RecruitCost(g, type);
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
/* Ordres                                                              */
/* ------------------------------------------------------------------ */

const char *War_OrderBlocked(const Game *g, int i, int t)
{
    const Unit *u = &g->units[i];
    if (!u->active || u->owner != N_FRANCE) return "Unité invalide";
    if (u->training) return "En formation";
    if (t == u->prov) return NULL;   /* annulation */
    if (!IsAdjacent(u->prov, t)) return "Province non adjacente";
    int o = g->prov[t].owner;
    if (o != N_FRANCE && !War_AtWar(g, N_FRANCE, o)) return "Territoire d'une nation avec laquelle nous sommes en paix";
    if (Map_SeaLink(u->prov, t) && u->type != U_MARINE && g->capacity[D_MER] < 3)
        return "Traversée impossible : capacité navale insuffisante (3)";
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

static float UnitAttack(const Game *g, const Unit *u, int p)
{
    const UnitDef *d = &UNIT_TYPES[u->type];
    float a = d->atk * u->str * (0.3f + 0.7f * u->org) * War_Quality(g, u->owner);
    int terr = PROVINCE_TERRAIN[p];
    if (u->type == U_BLINDE) a *= terr == 2 ? 0.6f : terr == 1 ? 0.85f : 1.15f;
    if (Map_SeaLink(u->prov, p) && u->type != U_MARINE)
        a *= (u->owner == N_FRANCE && g->capacity[D_MER] >= 6) ? 0.6f : 0.4f;   /* debarquement */
    return a;
}

/* defense d'une province face a la nation attaquante a */
static float ProvinceDefense(const Game *g, int p, int a)
{
    const Province *pr = &g->prov[p];
    float d = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->prov != p || u->training || !War_AtWar(g, u->owner, a)) continue;
        d += UNIT_TYPES[u->type].def * u->str * (0.3f + 0.7f * u->org) * War_Quality(g, u->owner);
    }
    d *= 1 + 0.2f * PROVINCE_TERRAIN[p];
    /* garnisons, reservistes et defense territoriale */
    float garrison = (1.0f + pr->pop * 0.06f) * (pr->core == pr->owner ? 1.2f : 0.5f) * (1 + 0.2f * PROVINCE_TERRAIN[p]);
    if (pr->owner == N_FRANCE && pr->core == N_FRANCE) garrison += Game_PostureUsed(g, D_TERRE) * 0.15f;
    return d + garrison;
}

static float AttackPower(const Game *g, int p, int a, bool ordered)
{
    float s = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->owner != a || u->training) continue;
        if (ordered ? u->target == p : (IsAdjacent(u->prov, p) && u->target < 0)) s += UnitAttack(g, u, p);
    }
    return s;
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
    return Odds(att, ProvinceDefense(g, p, a));
}

static void Relocate(Game *g, Unit *u)
{
    /* une province voisine du meme proprietaire, sinon la capitale */
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
        int q = PROVINCE_ADJ[u->prov][k];
        if (g->prov[q].owner == u->owner) { u->prov = (short)q; u->target = -1; return; }
    }
    int c = Capital(g, u->owner);
    if (c < 0) u->active = false;
    else { u->prov = (short)c; u->target = -1; }
}

void War_Capture(Game *g, int p, int newOwner)
{
    Province *pr = &g->prov[p];
    int old = pr->owner;
    pr->owner = newOwner;
    pr->integration = pr->core == newOwner ? 100 : 0;
    pr->damage = Clampf(pr->damage + 0.3f, 0, 1);
    pr->pop *= 0.97f;
    pr->revolt = 0;
    pr->factoryBuild = 0;
    if (pr->core != newOwner)
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = Clampf(pr->mil[s] + 2, 0, 10);
    if (newOwner == N_FRANCE) g->statConquered++;
    if (old == N_FRANCE) g->statLost++;
    if (old == N_FRANCE && p == PV_FR_NORD) {
        AddRes(g, R_COHESION, -20);
        AddRes(g, R_SOUVERAINETE, -20);
    }
    /* les troupes etrangeres presentes regagnent leurs lignes */
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (u->active && u->prov == p && u->owner != newOwner) Relocate(g, u);
    }
    if (old != N_FRANCE && Soc_NationProvinces(g, old) == 0) {
        Nation *na = &g->nat[old];
        na->alive = false;
        na->atWar = false;
        na->trade = false;
        na->warTarget = -1;
        na->army = 0;
        for (int i = 0; i < MAX_UNITS; i++) if (g->units[i].owner == old) g->units[i].active = false;
        for (int n = 1; n < N_COUNT; n++) if (g->nat[n].warTarget == old) g->nat[n].warTarget = -1;
        Game_Log(g, EV_WAR, "%s a cessé d'exister en tant qu'État.", NATIONS[old].name);
    }
}

static void Battle(Game *g, int p, int a)
{
    int defender = g->prov[p].owner;
    float att = AttackPower(g, p, a, true);
    if (att <= 0) return;
    float def = ProvinceDefense(g, p, a);
    float r = att * (0.85f + Randf(g) * 0.3f) / (def * (0.85f + Randf(g) * 0.3f));

    int defUnits = 0;
    float defOrg = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (u->active && u->prov == p && !u->training && War_AtWar(g, u->owner, a)) {
            u->org = Clampf(u->org - fminf(0.6f, 0.25f * r), 0, 1);
            u->str = Clampf(u->str - fminf(0.3f, 0.04f * r), 0, 1);
            defUnits++;
            defOrg += u->org;
        }
        if (u->active && u->owner == a && u->target == p) {
            u->org = Clampf(u->org - fminf(0.6f, 0.25f / r), 0, 1);
            u->str = Clampf(u->str - fminf(0.3f, 0.04f / r), 0, 1);
        }
    }
    bool won = r > 1 && (defUnits == 0 || defOrg / defUnits < 0.35f);

    if (won) {
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (u->active && u->prov == p && War_AtWar(g, u->owner, a)) {
                u->org *= 0.5f;
                int before = u->prov;
                Relocate(g, u);
                if (u->owner == N_FRANCE && u->active && u->prov == before) u->active = false;
            }
        }
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (u->active && u->owner == a && u->target == p) {
                u->prov = (short)p;
                u->target = -1;
                u->org = Clampf(u->org - 0.1f, 0, 1);
            }
        }
        if (a == N_FRANCE) g->statBattlesWon++;
        if (defender == N_FRANCE) g->statBattlesLost++;
        if (a == N_FRANCE)
            Game_Log(g, EV_WAR, "Bataille de %s : victoire, la province tombe (rapport %.1f contre 1).", PROVINCE_NAMES[p], r);
        else if (defender == N_FRANCE)
            Game_Log(g, EV_WAR, "Bataille de %s : %s perce nos lignes (rapport %.1f contre 1) !", PROVINCE_NAMES[p], NATIONS[a].name, r);
        else
            Game_Log(g, EV_WAR, "%s s'empare de %s aux dépens de %s.", NATIONS[a].name, PROVINCE_NAMES[p], NATIONS[defender].name);
        War_Capture(g, p, a);
    } else {
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == a && g->units[i].target == p) g->units[i].target = -1;
        if (a == N_FRANCE) g->statBattlesLost++;
        if (defender == N_FRANCE) g->statBattlesWon++;
        if (a == N_FRANCE && r > 1)
            Game_Log(g, EV_WAR, "Bataille de %s : l'ennemi plie mais tient (rapport %.1f contre 1), son organisation s'effondre.",
                     PROVINCE_NAMES[p], r);
        else if (a == N_FRANCE)
            Game_Log(g, EV_WAR, "Bataille de %s : notre offensive est repoussée (rapport %.1f contre 1).", PROVINCE_NAMES[p], r);
        else if (defender == N_FRANCE)
            Game_Log(g, EV_WAR, "Bataille de %s : l'offensive de %s est repoussée (rapport %.1f contre 1).",
                     PROVINCE_NAMES[p], NATIONS[a].name, r);
    }
}

/* ------------------------------------------------------------------ */
/* IA militaire                                                        */
/* ------------------------------------------------------------------ */

static bool HasEnemy(const Game *g, int n)
{
    for (int k = 0; k < N_COUNT; k++) if (g->nat[k].alive && War_AtWar(g, n, k)) return true;
    return false;
}

static bool CanAttack(const Game *g, int n, int e)
{
    int o = g->prov[e].owner;
    if (!War_AtWar(g, n, o)) return false;
    /* une nation dissuadee ne touche pas au sol national francais */
    if (o == N_FRANCE && g->prov[e].core == N_FRANCE && g->nat[n].deterred) return false;
    return true;
}

static void AiOrders(Game *g, int n)
{
    if (!HasEnemy(g, n)) return;
    bool used[MAX_UNITS] = { false };

    /* 1. attaques : les provinces ennemies les plus vulnerables d'abord */
    for (int attempt = 0; attempt < 3; attempt++) {
        int best = -1;
        float bestRatio = 1.25f;
        for (int e = 0; e < PROV_COUNT; e++) {
            if (!CanAttack(g, n, e)) continue;
            float att = 0;
            for (int i = 0; i < MAX_UNITS; i++) {
                const Unit *u = &g->units[i];
                if (u->active && u->owner == n && !u->training && !used[i] && u->org > 0.4f && IsAdjacent(u->prov, e))
                    att += UnitAttack(g, u, e);
            }
            float ratio = att / ProvinceDefense(g, e, n);
            if (ratio > bestRatio) { bestRatio = ratio; best = e; }
        }
        if (best < 0) break;
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (u->active && u->owner == n && !u->training && !used[i] && u->org > 0.4f && IsAdjacent(u->prov, best)) {
                u->target = (short)best;
                used[i] = true;
            }
        }
    }

    /* 2. les autres unites rejoignent le front (parcours en largeur sur le territoire national) */
    int dist[PROV_COUNT], queue[PROV_COUNT], head = 0, tail = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        dist[p] = -1;
        if (g->prov[p].owner != n) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
            if (War_AtWar(g, n, g->prov[PROVINCE_ADJ[p][k]].owner)) { dist[p] = 0; queue[tail++] = p; break; }
    }
    while (head < tail) {
        int p = queue[head++];
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
            int q = PROVINCE_ADJ[p][k];
            if (g->prov[q].owner == n && dist[q] < 0) { dist[q] = dist[p] + 1; queue[tail++] = q; }
        }
    }
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->owner != n || u->training || used[i] || u->target >= 0) continue;
        int d = dist[u->prov];
        if (d <= 0) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
            int q = PROVINCE_ADJ[u->prov][k];
            if (dist[q] >= 0 && dist[q] < d) { u->target = (short)q; break; }
        }
    }
}

static void AiRecruit(Game *g, int n)
{
    int count = War_UnitCount(g, n);
    if (count >= (int)War_ArmyMax(g, n)) return;
    if (Randf(g) > (HasEnemy(g, n) ? 0.5f : 0.15f)) return;
    int type = PickType(g, n);
    War_SpawnUnit(g, n, type, Capital(g, n), UNIT_TYPES[type].train);
}

/* ------------------------------------------------------------------ */
/* Declarations, paix, diplomatie                                      */
/* ------------------------------------------------------------------ */

const char *War_DeclareBlocked(const Game *g, int n)
{
    const Nation *na = &g->nat[n];
    if (n == N_FRANCE || !na->alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (na->atWar) return "Déjà en guerre";
    if (na->truce > 0) return "Trêve en cours";
    if (!War_Adjacent(g, N_FRANCE, n)) return "Aucune frontière terrestre ou maritime commune";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

static void StartWarWithFrance(Game *g, int n)
{
    Nation *na = &g->nat[n];
    na->atWar = true;
    na->warTurns = 0;
    na->trade = false;
    na->warned = false;
    na->deterred = Nuke_Deters(g, n);
}

bool War_Declare(Game *g, int n)
{
    if (War_DeclareBlocked(g, n)) return false;
    Nation *na = &g->nat[n];
    Nation *fr = &g->nat[N_FRANCE];
    g->commandPoints--;
    StartWarWithFrance(g, n);
    na->relation = -80;
    g->aggressionMemory = Clampf(g->aggressionMemory + 25, 0, 100);
    AddRes(g, R_COHESION, -REGIMES[fr->regime].aggressionCost);
    AddRes(g, R_INFLUENCE, -8);
    Game_Log(g, EV_WAR, "La France déclare la guerre à %s !", NATIONS[n].name);

    if (na->eu && fr->eu) {
        fr->eu = false;
        g->umbrella = false;
        Game_Log(g, EV_BAD, "Agression contre un État membre : la France est exclue de l'Union européenne.");
        for (int k = 1; k < N_COUNT; k++) {
            Nation *o = &g->nat[k];
            if (!o->alive || !o->eu || k == n) continue;
            AddRelation(g, k, -30);
            if (!o->atWar && o->army >= 2 && War_Adjacent(g, N_FRANCE, k) && Randf(g) < 0.3f) {
                StartWarWithFrance(g, k);
                Game_Log(g, EV_WAR, "%s rejoint la coalition contre la France !", NATIONS[k].name);
            }
        }
    }
    return true;
}

void War_MakePeace(Game *g, int n, bool log)
{
    Nation *na = &g->nat[n];
    na->atWar = false;
    na->truce = 16;
    na->warTurns = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active) continue;
        if ((u->owner == N_FRANCE && g->prov[u->prov].owner == n) || (u->owner == n && g->prov[u->prov].owner == N_FRANCE))
            Relocate(g, u);
        if ((u->owner == N_FRANCE || u->owner == n) && u->target >= 0 && !War_AtWar(g, u->owner, g->prov[u->target].owner)
            && g->prov[u->target].owner != u->owner)
            u->target = -1;
    }
    if (log) Game_Log(g, EV_WAR, "Paix signée avec %s. Trêve de 4 ans.", NATIONS[n].name);
}

static void EndAiWar(Game *g, int n)
{
    int t = g->nat[n].warTarget;
    g->nat[n].warTarget = -1;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active) continue;
        if ((u->owner == n && g->prov[u->prov].owner == t) || (u->owner == t && g->prov[u->prov].owner == n)) Relocate(g, u);
        if (u->owner == n || u->owner == t) u->target = -1;
    }
}

const char *War_PeaceBlocked(const Game *g, int n)
{
    if (!g->nat[n].atWar) return "Pas en guerre";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool War_ProposePeace(Game *g, int n)
{
    if (War_PeaceBlocked(g, n)) return false;
    g->commandPoints--;
    Nation *na = &g->nat[n];
    bool losing = na->army * War_Quality(g, n) < g->nat[N_FRANCE].army * War_Quality(g, N_FRANCE) * 0.8f
               || Soc_NationProvinces(g, n) < na->startProvinces;
    if (losing || na->warTurns > 10 || Randf(g) < 0.15f) {
        War_MakePeace(g, n, true);
        return true;
    }
    Game_Log(g, EV_WAR, "%s rejette notre proposition de paix.", NATIONS[n].name);
    return false;
}

const char *War_DiploBlocked(const Game *g, int n, int action)
{
    const Nation *na = &g->nat[n];
    if (!na->alive || n == N_FRANCE) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (na->atWar) return "En guerre";
    if (na->actedThisTurn) return "Déjà une action diplomatique ce tour";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (action == 0 && g->res[R_INFLUENCE] < 5) return "Influence insuffisante (5)";
    if (action == 1) {
        if (na->trade) return "Accord déjà en vigueur";
        if (na->relation < 25) return "Relation insuffisante (25)";
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
    } else {
        na->trade = true;
        Game_Log(g, EV_GOOD, "Accord commercial signé avec %s : la France est servie en priorité sur leurs excédents.", NATIONS[n].name);
    }
    return true;
}

const char *War_UltimatumBlocked(const Game *g, int p)
{
    int n = g->prov[p].owner;
    if (n == N_FRANCE || !g->nat[n].alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->nat[n].atWar) return "Déjà en guerre : il faut la conquérir";
    if (g->nat[n].truce > 0) return "Trêve en cours";
    if (!TouchesNation(g, p, N_FRANCE, NULL)) return "Province non frontalière";
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
    if (g->nukePosture == 2 && !NATIONS[n].nuclear) ours *= 1 + g->credibility / 200.0f;   /* diplomatie de la canonniere */
    g->aggressionMemory = Clampf(g->aggressionMemory + 12, 0, 100);
    if (ours > theirs * 2.5f) {
        Game_Log(g, EV_WAR, "Ultimatum : %s cède %s sans combat.", NATIONS[n].name, PROVINCE_NAMES[p]);
        AddRelation(g, n, -40);
        AddRes(g, R_COHESION, -REGIMES[g->nat[N_FRANCE].regime].aggressionCost * 0.3f);
        War_Capture(g, p, N_FRANCE);
        g->prov[p].damage = 0;
        return true;
    }
    AddRelation(g, n, -25);
    AddRes(g, R_INFLUENCE, -5);
    Game_Log(g, EV_BAD, "Ultimatum rejeté par %s. Notre crédibilité en souffre.", NATIONS[n].name);
    return false;
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
    int cores = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE && g->prov[p].core == n) cores++;
    t -= cores * 35 > 80 ? 80 : cores * 35;
    t -= g->aggressionMemory * 0.4f + g->nukesUsed * 40;
    if (na->trade) t += 5;
    if (n >= N_MAROC || n == N_TURQUIE) {
        if (g->migrationDoctrine == 0) t += 8;
        if (g->migrationDoctrine == 2) t -= 12;
    }
    return Clampf(t, -100, 100);
}

static void Movements(Game *g)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->training || u->target < 0) continue;
        int o = g->prov[u->target].owner;
        if (o == u->owner) {
            u->prov = u->target;
            u->target = -1;
            u->org = Clampf(u->org - 0.05f, 0, 1);
        } else if (!War_AtWar(g, u->owner, o)) {
            u->target = -1;
        }
    }
}

static void Battles(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        for (int a = 0; a < N_COUNT; a++) {
            bool any = false;
            for (int i = 0; i < MAX_UNITS && !any; i++)
                any = g->units[i].active && g->units[i].owner == a && g->units[i].target == p;
            if (any && War_AtWar(g, a, g->prov[p].owner)) Battle(g, p, a);
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
                    Game_Log(g, EV_GOOD, "Division %s opérationnelle en %s.", UNIT_TYPES[u->type].name, PROVINCE_NAMES[u->prov]);
            }
            continue;
        }
        bool goods = g->eco[u->owner].sat[G_GOODS] >= 0.7f;
        float recov = 0.15f * (g->nat[u->owner].regime == RG_JUNTE ? 1.3f : 1.0f) * (goods ? 1 : 0.5f);
        if (g->prov[u->prov].owner == u->owner) {
            u->org = Clampf(u->org + recov, 0, 1);
            bool funds = u->owner != N_FRANCE || g->res[R_BUDGET] > 0;
            u->str = Clampf(u->str + (funds ? 0.05f : 0.02f), 0, 1);
        } else if (TouchesNation(g, u->prov, u->owner, NULL)) {
            u->org = Clampf(u->org + 0.05f, 0, 1);
        } else {
            u->org = Clampf(u->org - 0.08f, 0, 1);   /* ravitaillement coupe */
            u->str = Clampf(u->str - 0.02f, 0, 1);
        }
        if (PROVINCES[u->prov].aridity >= 1.0f && g->prov[u->prov].owner != u->owner) u->str -= 0.01f;
        if (u->str < 0.08f) {
            u->active = false;
            if (u->owner == N_FRANCE) Game_Log(g, EV_BAD, "Une division %s a été anéantie.", UNIT_TYPES[u->type].name);
        }
    }
}

void War_Turn(Game *g)
{
    if (g->aggressionMemory > 0) g->aggressionMemory -= 0.6f;

    /* 1. decisions diplomatiques et militaires des IA */
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

        if (!na->atWar) {
            na->relation += (RelationTarget(g, n) - na->relation) * 0.08f + (Randf(g) - 0.5f) * 2;
            na->relation = Clampf(na->relation, -100, 100);
            if (na->trade && na->relation < 10) {
                na->trade = false;
                Game_Log(g, EV_BAD, "%s rompt l'accord commercial.", NATIONS[n].name);
            }
        }
        if (na->truce > 0) na->truce--;

        float aggro = NATIONS[n].aggressiveness * DiffAggro(g);
        float deter = 1 - g->credibility / 100.0f * (NATIONS[n].nuclear ? 0.4f : 0.8f);
        if (na->atWar) {
            na->warTurns++;
            if (na->warTurns % 8 == 0) na->deterred = Nuke_Deters(g, n);
            int lost = na->startProvinces - Soc_NationProvinces(g, n);
            bool wantsPeace = na->army < War_ArmyMax(g, n) * 0.3f || lost * 2 >= na->startProvinces
                           || (na->warTurns > 14 && Randf(g) < 0.1f);
            if (wantsPeace && g->pendingDilemma < 0 && Randf(g) < 0.4f) {
                g->pendingDilemma = DLM_PEACE_OFFER;
                g->dilemmaParam = n;
            }
        } else if (na->truce == 0 && g->aggressionMemory > 35 && na->relation < 0 && War_Adjacent(g, n, N_FRANCE)
                   && Randf(g) < (0.03f + g->aggressionMemory * 0.001f) * deter) {
            StartWarWithFrance(g, n);
            Game_Log(g, EV_WAR, "Face à l'expansionnisme français, %s rejoint la coalition et nous déclare la guerre !",
                     NATIONS[n].name);
        } else if (na->truce == 0 && na->relation < -55 && War_Adjacent(g, n, N_FRANCE)
                   && na->army * War_Quality(g, n) > g->nat[N_FRANCE].army * War_Quality(g, N_FRANCE) * 1.2f
                   && Randf(g) < aggro * 0.15f * deter) {
            StartWarWithFrance(g, n);
            Game_Log(g, EV_WAR, "%s déclare la guerre à la France !", NATIONS[n].name);
        }

        /* guerres entre nations */
        if (na->warTarget >= 0) {
            na->warTargetTurns++;
            if (!g->nat[na->warTarget].alive || na->warTargetTurns > 14 || na->army < War_ArmyMax(g, n) * 0.35f) {
                if (g->nat[na->warTarget].alive)
                    Game_Log(g, EV_WAR, "Cessez-le-feu entre %s et %s.", NATIONS[n].name, NATIONS[na->warTarget].name);
                EndAiWar(g, n);
            }
        } else if (g->turn > 8 && Randf(g) < aggro * 0.015f * (na->failed ? 2 : 1)) {
            int best = -1;
            float bestArmy = 1e9f;
            for (int t = 1; t < N_COUNT; t++) {
                const Nation *tg = &g->nat[t];
                if (t == n || !tg->alive || (tg->eu && na->eu) || !War_Adjacent(g, n, t)) continue;
                if (na->army * War_Quality(g, n) < tg->army * War_Quality(g, t) * 1.6f) continue;
                if (tg->eu && g->umbrella && Randf(g) < g->credibility / 100.0f) continue;   /* parapluie francais */
                if (tg->army < bestArmy) { bestArmy = tg->army; best = t; }
            }
            if (best >= 0) {
                na->warTarget = best;
                na->warTargetTurns = 0;
                Game_Log(g, EV_WAR, "%s envahit %s !", NATIONS[n].name, NATIONS[best].name);
                if (g->nat[best].eu && g->nat[N_FRANCE].eu && !na->atWar && g->pendingDilemma < 0) {
                    g->pendingDilemma = DLM_ALLY_CALL;
                    g->dilemmaParam = n;
                }
            }
        }

        AiRecruit(g, n);
        AiOrders(g, n);
    }

    /* 2. mouvements, batailles, ravitaillement */
    Movements(g);
    Battles(g);
    Supply(g);
    UpdateArmyPower(g);
}
