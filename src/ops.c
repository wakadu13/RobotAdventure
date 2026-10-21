/*
 * BOUCLIER 2050 - operations ciblees contre une nation dans les milieux
 * cyber, informationnel, spatial et electromagnetique : sabotage du reseau
 * electrique, vol de technologies, paralysie logistique, ingerence
 * electorale, destabilisation, reconnaissance satellitaire, brouillage,
 * tir antisatellite. Chaque operation peut etre attribuee a la France.
 */
#include "game_internal.h"

#include <stdio.h>

static float AvgWealth(const Game *g, int n)
{
    float w = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n) { w += PROVINCES[p].wealth * g->prov[p].pop; pop += g->prov[p].pop; }
    return pop > 0 ? w / pop : 0;
}

float Ops_Chance(const Game *g, int n, int op)
{
    const OpDef *o = &OPS[op];
    float c = 0.5f + 0.05f * (g->capacity[o->domain] - o->capacity);
    if (g->nat[n].recon > 0 && op != OP_RECON) c += 0.1f;
    c -= 0.05f * g->nat[n].tech[T_GE];
    if (AvgWealth(g, n) > 0.4f) c -= 0.1f;   /* defenses d'une puissance riche */
    if (op == OP_INGERENCE && IsDemocratic(g->nat[n].regime)) c += 0.1f;   /* une societe ouverte est plus vulnerable */
    if (op == OP_DESTAB && g->nat[n].failed) c += 0.15f;
    return Clampf(c, 0.15f, 0.9f);
}

const char *Ops_Blocked(const Game *g, int n, int op)
{
    static char buf[96];
    const OpDef *o = &OPS[op];
    const Nation *na = &g->nat[n];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (n <= 0 || n >= N_COUNT || !na->alive) return "Nation invalide";
    if (na->opsThisTurn) return "Déjà une opération contre elle ce tour";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (g->capacity[o->domain] < o->capacity) {
        snprintf(buf, sizeof(buf), "Capacité %s insuffisante (%d requis)", DOMAIN_NAMES[o->domain], o->capacity);
        return buf;
    }
    if (o->needWar && !War_AtWar(g, N_FRANCE, n) && na->relation > -40) return "Réservée à un ennemi (guerre ou relation < -40)";
    if (op == OP_RECON && na->recon > 0) return "Surveillance déjà en cours";
    if (op == OP_ASAT && na->asat > 0) return "Ses satellites sont déjà détruits";
    if (g->res[R_BUDGET] < o->budget) return "Budget insuffisant";
    if (g->res[R_INFLUENCE] < o->influence) return "Influence insuffisante";
    return NULL;
}

/* courant le plus favorable a notre regime */
static int FriendlyIdeology(const Game *g)
{
    Regime r = g->nat[N_FRANCE].regime;
    int best = 0;
    for (int i = 1; i < I_COUNT; i++) if (IDEOLOGY_AFFINITY[r][i] > IDEOLOGY_AFFINITY[r][best]) best = i;
    return best;
}

static void Normalize(float v[I_COUNT])
{
    float s = 0;
    for (int i = 0; i < I_COUNT; i++) { if (v[i] < 0.01f) v[i] = 0.01f; s += v[i]; }
    for (int i = 0; i < I_COUNT; i++) v[i] /= s;
}

static void Apply(Game *g, int n, int op)
{
    Nation *na = &g->nat[n];
    const char *nm = NATIONS[n].name;
    switch (op) {
    case OP_RESEAU:
        na->outage = (short)OPS[op].duration;
        Game_Log(g, EV_GOOD, "Le réseau électrique de %s s'effondre : usines et casernes plongées dans le noir.", nm);
        break;
    case OP_VOL_TECH: {
        int best = g->researchArea, gap = 0;
        for (int t = 0; t < T_COUNT; t++) {
            int d = na->tech[t] - g->nat[N_FRANCE].tech[t];
            if (d > gap && g->nat[N_FRANCE].tech[t] < TECH_MAX) { gap = d; best = t; }
        }
        if (gap > 0) {
            Mil_SetResearch(g, best);
            g->researchProgress += Mil_TechCost(g, best) * 0.6f;
            Game_Log(g, EV_GOOD, "Nos services dérobent les plans de %s : %s progresse.", nm, TECHS[best].name);
        } else {
            g->researchProgress += 6;
            Game_Log(g, EV_GOOD, "Intrusion réussie chez %s, mais leurs laboratoires ont peu à nous apprendre.", nm);
        }
        break;
    }
    case OP_LOGISTIQUE:
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == n) g->units[i].org = Clampf(g->units[i].org - 0.3f, 0, 1);
        Game_Log(g, EV_GOOD, "Les systèmes logistiques de %s sont paralysés : ses unités se désorganisent.", nm);
        break;
    case OP_INGERENCE: {
        int ideo = FriendlyIdeology(g);
        for (int p = 0; p < PROV_COUNT; p++) {
            Province *pr = &g->prov[p];
            if (pr->owner != n) continue;
            for (int s = 0; s < S_COUNT; s++) { pr->ideo[s][ideo] += 0.07f; Normalize(pr->ideo[s]); }
        }
        if (IsDemocratic(na->regime)) na->nextElection = g->turn + 1;
        AddRelation(g, n, 4);
        Game_Log(g, EV_GOOD, "Campagne d'influence chez %s : les %s progressent dans l'opinion.", nm, IDEOLOGY_NAMES[ideo]);
        break;
    }
    case OP_DESTAB:
        for (int p = 0; p < PROV_COUNT; p++) {
            Province *pr = &g->prov[p];
            if (pr->owner != n) continue;
            for (int s = 0; s < S_ELITES; s++) {
                pr->mil[s] = Clampf(pr->mil[s] + 1.2f, 0, 10);
                pr->con[s] = Clampf(pr->con[s] + 0.3f, 0, 10);
            }
        }
        Game_Log(g, EV_GOOD, "Manifestations et émeutes se multiplient chez %s.", nm);
        break;
    case OP_RECON:
        na->recon = (short)OPS[op].duration;
        Game_Log(g, EV_GOOD, "Nos satellites surveillent désormais les forces de %s.", nm);
        break;
    case OP_BROUILLAGE:
        na->jammed = (short)OPS[op].duration;
        Game_Log(g, EV_GOOD, "Brouillage : l'aviation et les drones de %s volent à l'aveugle.", nm);
        break;
    case OP_ASAT:
        na->asat = (short)OPS[op].duration;
        Game_Log(g, EV_WAR, "Tir antisatellite : la constellation militaire de %s est détruite.", nm);
        /* les debris menacent tout le monde */
        for (int k = 1; k < N_COUNT; k++) if (k != n && g->nat[k].alive) AddRelation(g, k, -8);
        AddRes(g, R_INFLUENCE, -8);
        if (Randf(g) < 0.3f && g->capacity[D_ESPACE] > 0) {
            g->capacity[D_ESPACE]--;
            Game_Log(g, EV_BAD, "Syndrome de Kessler : les débris détruisent aussi un de nos satellites (Espace -1).");
        }
        break;
    }
}

/* l'operation est attribuee a la France : riposte */
static void Attribution(Game *g, int n, int op)
{
    Nation *na = &g->nat[n];
    AddRelation(g, n, op == OP_ASAT ? -30 : -18);
    AddRes(g, R_INFLUENCE, -4);
    g->aggressionMemory = Clampf(g->aggressionMemory + 5, 0, 100);
    Game_Log(g, EV_BAD, "%s attribue publiquement l'opération « %s » à la France.", NATIONS[n].name, OPS[op].name);
    if (War_AtWar(g, N_FRANCE, n)) return;
    if (na->relation < -60 && War_Adjacent(g, n, N_FRANCE) && Randf(g) < 0.25f * DiffAggro(g) && !g->ally[N_FRANCE][n]
        && g->truce[N_FRANCE][n] == 0 && !na->deterred) {
        War_StartWar(g, n, N_FRANCE, true);
        Game_Log(g, EV_WAR, "Casus belli : %s déclare la guerre à la France !", NATIONS[n].name);
    } else if (AvgWealth(g, n) > 0.2f && Randf(g) < 0.5f) {
        g->nat[N_FRANCE].outage = 2;
        Game_Log(g, EV_BAD, "Riposte cyber de %s : notre réseau électrique est touché.", NATIONS[n].name);
    }
}

bool Ops_Run(Game *g, int n, int op)
{
    if (Ops_Blocked(g, n, op)) return false;
    const OpDef *o = &OPS[op];
    g->commandPoints--;
    g->res[R_BUDGET] -= o->budget;
    g->res[R_INFLUENCE] -= o->influence;
    g->nat[n].opsThisTurn = true;
    bool ok = Randf(g) < Ops_Chance(g, n, op);
    if (ok) Apply(g, n, op);
    else Game_Log(g, EV_BAD, "Échec de l'opération « %s » contre %s.", o->name, NATIONS[n].name);
    float risk = ok ? o->risk : o->risk + 0.3f;
    if (Randf(g) < risk) Attribution(g, n, op);
    return ok;
}

void Ops_Turn(Game *g)
{
    for (int n = 0; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        na->opsThisTurn = false;
        if (na->outage > 0) na->outage--;
        if (na->jammed > 0) na->jammed--;
        if (na->recon > 0) na->recon--;
        if (na->asat > 0) na->asat--;
    }
    /* les ennemis capables menent aussi des cyberattaques contre la France */
    for (int n = 1; n < N_COUNT; n++) {
        if (!g->nat[n].alive || !War_AtWar(g, N_FRANCE, n) || AvgWealth(g, n) < 0.2f) continue;
        float defense = g->capacity[D_CYBER] * 0.03f + (g->proj[P_CYBERCIV].state == 2 ? 0.3f : 0);
        if (Randf(g) < 0.06f * DiffThreat(g) * (1 - Clampf(defense, 0, 0.8f))) {
            g->nat[N_FRANCE].outage = 2;
            Game_Log(g, EV_BAD, "Cyberattaque de %s contre notre réseau électrique : -40%% d'énergie pendant 2 tours.", NATIONS[n].name);
        }
    }
}
