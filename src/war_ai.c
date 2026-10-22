/*
 * BOUCLIER 2050 - IA militaire : offensives par front, reserves, aviation et flottes,
 * recrutement des nations de l'IA.
 */
#include "war_internal.h"

/* ------------------------------------------------------------------ */
/* IA militaire                                                        */
/* ------------------------------------------------------------------ */

bool CanAttack(const Game *g, int n, int e)
{
    int o = g->prov[e].ctrl;
    if (!War_AtWar(g, n, o)) return false;
    if (o == N_FRANCE && g->prov[e].core == N_FRANCE && g->nat[n].deterred) return false;
    return true;
}

/* valeur d'une province ennemie pour l'IA */
float TargetValue(const Game *g, int n, int e)
{
    float v = g->prov[e].pop + 2;
    if (g->prov[e].owner == n) v += 8;                     /* reprendre son propre territoire */
    if (e == Capital(g, g->prov[e].ctrl)) v += 5;
    for (int r = 0; r < SR_COUNT; r++) v += Strat_Deposit(e, r) * 2;
    return v;
}

/* groupe d'armees auquel se limitent les ordres automatiques (-1 : toutes les unites) */
int groupFilter = -1;

bool ReadyLand(const Game *g, const Unit *u, int n, const bool *used)
{
    return u->active && u->owner == n && !u->training && !used[u - g->units] && u->org > 0.4f
        && UNIT_TYPES[u->type].domain == DOM_LAND && (groupFilter < 0 || u->group == groupFilter);
}

/* puissance offensive estimee des unites de n adjacentes a e (ou a deux pas, ponderees) */
float StagedAttack(const Game *g, int n, int e, const bool *used, bool near)
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

void LaunchAttack(Game *g, int n, int e, bool *used)
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
void Stage(Game *g, int n, int e, bool *used)
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
float FrontThreat(const Game *g, int n, int f, int *enemyOut)
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
void StepToward(Game *g, Unit *u, const int dist[PROV_COUNT])
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
void FriendlyBfs(const Game *g, int n, const bool src[PROV_COUNT], int dist[PROV_COUNT])
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

void AiOrders(Game *g, int n)
{
    Nation *na = &g->nat[n];
    na->aiTarget = na->aiThreat = -1;
    if (!War_AtWarAny(g, n)) return;
    static bool used[MAX_UNITS];
    memset(used, 0, sizeof(used));
    float aggro = NATIONS[n].aggressiveness;

    /* 1. les points menaces (jusqu'a trois, un par front) */
    float worst = 0;
    int threats[3] = { -1, -1, -1 };
    float tval[3] = { 0, 0, 0 };
    for (int f = 0; f < PROV_COUNT; f++) {
        if (g->prov[f].ctrl != n) continue;
        float t = FrontThreat(g, n, f, NULL);
        if (t <= 0) continue;
        if (t > worst) { worst = t; na->aiThreat = (short)f; }
        /* un point par secteur : une province voisine d'un point retenu le remplace si elle est plus menacee */
        int slot = -1;
        for (int k = 0; k < 3; k++) if (threats[k] >= 0 && IsAdjacent(threats[k], f)) slot = k;
        if (slot < 0) {
            slot = 0;   /* sinon l'emplacement libre ou le moins menace */
            for (int k = 1; k < 3; k++) if (tval[k] < tval[slot]) slot = k;
        }
        if (t > tval[slot]) { threats[slot] = f; tval[slot] = t; }
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
    /* fronts secondaires : la meilleure cible chez chaque autre ennemi, pour mener des campagnes paralleles */
    for (int foe = 0; foe < N_COUNT; foe++) {
        if (!g->war[n][foe] || !g->nat[foe].alive) continue;
        bool covered = false;
        for (int k = 0; k < 3; k++) if (cand[k] >= 0 && g->prov[cand[k]].ctrl == foe) covered = true;
        if (covered) continue;
        int best = -1;
        float bsc = 0;
        for (int e = 0; e < PROV_COUNT; e++) {
            if (g->prov[e].ctrl != foe || !CanAttack(g, n, e)) continue;
            float att = StagedAttack(g, n, e, used, true);
            if (att <= 0) continue;
            float sc = att / DefensePower(g, e, n) * sqrtf(TargetValue(g, n, e));
            if (sc > bsc) { bsc = sc; best = e; }
        }
        if (best < 0) continue;
        /* remplace la candidate la plus faible : chaque front garde sa campagne */
        int weakest = 0;
        for (int k = 1; k < 3; k++) if (cand[k] < 0 || cscore[k] < cscore[weakest]) weakest = k;
        if (cand[weakest] < 0 || bsc > cscore[weakest] * 0.5f) { cand[weakest] = best; cscore[weakest] = bsc; }
    }
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
        } else if (k == 0 || g->prov[e].ctrl != g->prov[cand[0] >= 0 ? cand[0] : e].ctrl) {
            /* on prepare l'offensive principale et celle de chaque autre front */
            Stage(g, n, e, used);
            if (na->aiTarget < 0) na->aiTarget = (short)e;
        }
    }

    /* 3. reserves : vers le point menace, sinon vers le front */
    bool src[PROV_COUNT] = { false };
    int distThreat[PROV_COUNT], distFront[PROV_COUNT];
    for (int k = 0; k < 3; k++) if (threats[k] >= 0) src[threats[k]] = true;   /* reserves vers le point menace le plus proche */
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
        if (na->aiThreat >= 0 && worst > 0 && sent < 9 && distThreat[u->prov] > 0 && distThreat[u->prov] <= 4) {
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

void AiRecruit(Game *g, int n)
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
    if (site < 0 || na->manpower < UNIT_TYPES[type].capWeight) return;
    na->treasury -= cost;
    na->manpower -= UNIT_TYPES[type].capWeight;
    Strat_PayRecruit(g, n, type);
    War_SpawnUnit(g, n, type, site, UNIT_TYPES[type].train);
}
