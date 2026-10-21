/*
 * BOUCLIER 2050 - operations ciblees contre une nation dans les milieux
 * cyber, informationnel, spatial et electromagnetique : sabotage du reseau
 * electrique, vol de technologies, paralysie logistique, ingerence
 * electorale, destabilisation, reconnaissance satellitaire, brouillage,
 * tir antisatellite. La France comme les autres nations y recourent ;
 * chaque operation peut etre attribuee a son auteur.
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

/* capacite d'une nation dans un milieu (la France : ses capacites M2MC) */
static int Capacity(const Game *g, int a, Domain d)
{
    if (a == N_FRANCE) return g->capacity[d];
    int c = 1 + g->nat[a].tech[T_GE] + (int)(AvgWealth(g, a) * 8);
    if (a == N_BOREALE && (d == D_CYBER || d == D_INFO)) c += 2;   /* doctrine de guerre informationnelle */
    if (g->nat[a].failed) c -= 2;
    return c;
}

/* defense de la cible contre un milieu */
static float Defense(const Game *g, int t, int op)
{
    const OpDef *o = &OPS[op];
    float d = 0.05f * g->nat[t].tech[T_GE];
    if (AvgWealth(g, t) > 0.4f) d += 0.1f;
    if (t == N_FRANCE) {
        d += 0.03f * g->capacity[o->domain];
        if (o->domain == D_CYBER && g->proj[P_CYBERCIV].state == 2) d += 0.15f;
        if (o->domain == D_INFO && g->proj[P_MEDIAS].state == 2) d += 0.15f;
        if (o->domain == D_ESPACE && g->proj[P_CONSTELL].state == 2) d += 0.1f;
    }
    return d;
}

static float Chance(const Game *g, int a, int t, int op)
{
    const OpDef *o = &OPS[op];
    float c = 0.5f + 0.05f * (Capacity(g, a, o->domain) - o->capacity);
    if (a == N_FRANCE && g->nat[t].recon > 0 && op != OP_RECON) c += 0.1f;
    c -= Defense(g, t, op);
    if (op == OP_INGERENCE && IsDemocratic(g->nat[t].regime)) c += 0.1f;   /* une societe ouverte est plus vulnerable */
    if (op == OP_DESTAB && g->nat[t].failed) c += 0.15f;
    return Clampf(c, 0.15f, 0.9f);
}

float Ops_Chance(const Game *g, int n, int op) { return Chance(g, N_FRANCE, n, op); }

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

/* courant le plus favorable au regime de a */
static int FriendlyIdeology(const Game *g, int a)
{
    Regime r = g->nat[a].regime;
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

/* faut-il annoncer l'operation au joueur ? */
static bool Visible(const Game *g, int a, int t)
{
    return a == N_FRANCE || t == N_FRANCE || IsAlly(g, N_FRANCE, t) || IsAlly(g, N_FRANCE, a);
}

static void Apply(Game *g, int a, int t, int op)
{
    Nation *na = &g->nat[t];
    const char *nm = NATIONS[t].name;
    bool fr = a == N_FRANCE, us = t == N_FRANCE, show = Visible(g, a, t);
    switch (op) {
    case OP_RESEAU:
        na->outage = (short)OPS[op].duration;
        if (fr) Game_Log(g, EV_GOOD, "Le réseau électrique de %s s'effondre : usines et casernes plongées dans le noir.", nm);
        else if (us) Game_Log(g, EV_BAD, "Cyberattaque contre notre réseau électrique : -40%% d'énergie pendant %d tours.", OPS[op].duration);
        else if (show) Game_Log(g, EV_EVENT, "Le réseau électrique de %s est paralysé par une cyberattaque.", nm);
        break;
    case OP_VOL_TECH: {
        int best = -1, gap = 0;
        for (int k = 0; k < T_COUNT; k++) {
            int d = na->tech[k] - g->nat[a].tech[k];
            if (d > gap && g->nat[a].tech[k] < TECH_MAX && k != T_NUC) { gap = d; best = k; }
        }
        if (fr) {
            if (best >= 0) {
                Mil_SetResearch(g, best);
                g->researchProgress += Mil_TechCost(g, best) * 0.6f;
                Game_Log(g, EV_GOOD, "Nos services dérobent les plans de %s : %s progresse.", nm, TECHS[best].name);
            } else {
                g->researchProgress += 6;
                Game_Log(g, EV_GOOD, "Intrusion réussie chez %s, mais leurs laboratoires ont peu à nous apprendre.", nm);
            }
        } else if (best >= 0) {
            g->nat[a].tech[best]++;
            if (us) Game_Log(g, EV_BAD, "Espionnage : %s dérobe nos travaux en %s.", NATIONS[a].name, TECHS[best].name);
        }
        break;
    }
    case OP_LOGISTIQUE:
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == t) g->units[i].org = Clampf(g->units[i].org - 0.3f, 0, 1);
        if (fr) Game_Log(g, EV_GOOD, "Les systèmes logistiques de %s sont paralysés : ses unités se désorganisent.", nm);
        else if (us) Game_Log(g, EV_BAD, "Nos systèmes logistiques sont paralysés : nos unités perdent leur organisation.");
        break;
    case OP_INGERENCE: {
        int ideo = FriendlyIdeology(g, a);
        for (int p = 0; p < PROV_COUNT; p++) {
            Province *pr = &g->prov[p];
            if (pr->owner != t) continue;
            for (int s = 0; s < S_COUNT; s++) { pr->ideo[s][ideo] += 0.07f; Normalize(pr->ideo[s]); }
        }
        if (IsDemocratic(na->regime) && na->nextElection > g->turn + 1) na->nextElection = g->turn + 1;
        if (fr) { AddRelation(g, t, 4); Game_Log(g, EV_GOOD, "Campagne d'influence chez %s : les %s progressent dans l'opinion.", nm, IDEOLOGY_NAMES[ideo]); }
        else if (us) Game_Log(g, EV_BAD, "Ingérence étrangère : une campagne de %s fait progresser les %s.", NATIONS[a].name, IDEOLOGY_NAMES[ideo]);
        break;
    }
    case OP_DESTAB:
        for (int p = 0; p < PROV_COUNT; p++) {
            Province *pr = &g->prov[p];
            if (pr->owner != t) continue;
            for (int s = 0; s < S_ELITES; s++) {
                pr->mil[s] = Clampf(pr->mil[s] + (us ? 0.8f : 1.2f), 0, 10);
                pr->con[s] = Clampf(pr->con[s] + 0.3f, 0, 10);
            }
        }
        if (fr) Game_Log(g, EV_GOOD, "Manifestations et émeutes se multiplient chez %s.", nm);
        else if (us) Game_Log(g, EV_BAD, "Déstabilisation : des réseaux liés à %s attisent les colères sociales.", NATIONS[a].name);
        else if (show) Game_Log(g, EV_EVENT, "%s est en proie à des troubles attisés de l'étranger.", nm);
        break;
    case OP_RECON:
        if (fr) { na->recon = (short)OPS[op].duration; Game_Log(g, EV_GOOD, "Nos satellites surveillent désormais les forces de %s.", nm); }
        break;
    case OP_BROUILLAGE:
        na->jammed = (short)OPS[op].duration;
        if (fr) Game_Log(g, EV_GOOD, "Brouillage : l'aviation et les drones de %s volent à l'aveugle.", nm);
        else if (us) Game_Log(g, EV_BAD, "Brouillage : notre aviation et nos drones sont aveuglés pendant %d tours.", OPS[op].duration);
        break;
    case OP_ASAT:
        na->asat = (short)OPS[op].duration;
        Game_Log(g, us ? EV_BAD : EV_WAR, "Tir antisatellite de %s : la constellation militaire de %s est détruite.", NATIONS[a].name, nm);
        /* les debris menacent tout le monde */
        for (int k = 1; k < N_COUNT; k++) if (k != a && g->nat[k].alive) AddAiRel(g, a, k, -8);
        if (fr) AddRes(g, R_INFLUENCE, -8);
        if (Randf(g) < 0.3f && g->capacity[D_ESPACE] > 0) {
            g->capacity[D_ESPACE]--;
            Game_Log(g, EV_BAD, "Syndrome de Kessler : les débris détruisent aussi un de nos satellites (Espace -1).");
        }
        break;
    }
}

/* l'operation est attribuee a son auteur : riposte */
static void Attribution(Game *g, int a, int t, int op)
{
    AddAiRel(g, a, t, op == OP_ASAT ? -30 : -18);
    if (a == N_FRANCE) {
        AddRes(g, R_INFLUENCE, -4);
        g->aggressionMemory = Clampf(g->aggressionMemory + 5, 0, 100);
    }
    if (Visible(g, a, t))
        Game_Log(g, a == N_FRANCE ? EV_BAD : EV_EVENT, "%s attribue publiquement l'opération « %s » à %s.", NATIONS[t].name, OPS[op].name,
                 a == N_FRANCE ? "la France" : NATIONS[a].name);
    if (War_AtWar(g, a, t)) return;
    Nation *nt = &g->nat[t];
    float rel = Relation(g, t, a);
    if (t != N_FRANCE && rel < -60 && War_Adjacent(g, t, a) && Randf(g) < 0.25f * NATIONS[t].aggressiveness * 2 * DiffAggro(g)
        && !IsAlly(g, t, a) && g->truce[t][a] == 0 && !(a == N_FRANCE && nt->deterred)) {
        War_StartWar(g, t, a, true);
        if (Visible(g, a, t)) Game_Log(g, EV_WAR, "Casus belli : %s déclare la guerre à %s !", NATIONS[t].name, a == N_FRANCE ? "la France" : NATIONS[a].name);
    } else if (t != N_FRANCE && AvgWealth(g, t) > 0.2f && Randf(g) < 0.5f) {
        g->nat[a].outage = 2;
        if (a == N_FRANCE) Game_Log(g, EV_BAD, "Riposte cyber de %s : notre réseau électrique est touché.", NATIONS[t].name);
    }
}

static bool Execute(Game *g, int a, int t, int op)
{
    bool ok = Randf(g) < Chance(g, a, t, op);
    if (ok) Apply(g, a, t, op);
    else if (a == N_FRANCE) Game_Log(g, EV_BAD, "Échec de l'opération « %s » contre %s.", OPS[op].name, NATIONS[t].name);
    else if (t == N_FRANCE) Game_Log(g, EV_GOOD, "Nos défenses déjouent une opération « %s » de %s.", OPS[op].name, NATIONS[a].name);
    float risk = ok ? OPS[op].risk : OPS[op].risk + 0.3f;
    if (t == N_FRANCE) risk += 0.05f * g->capacity[D_CYBER];   /* nos services remontent la piste */
    if (Randf(g) < risk) Attribution(g, a, t, op);
    return ok;
}

bool Ops_Run(Game *g, int n, int op)
{
    if (Ops_Blocked(g, n, op)) return false;
    const OpDef *o = &OPS[op];
    g->commandPoints--;
    g->res[R_BUDGET] -= o->budget;
    g->res[R_INFLUENCE] -= o->influence;
    g->nat[n].opsThisTurn = true;
    return Execute(g, N_FRANCE, n, op);
}

/* une nation choisit une operation contre une cible */
static void AiOps(Game *g, int a)
{
    Nation *na = &g->nat[a];
    if (!na->alive || na->failed || AvgWealth(g, a) < 0.12f) return;
    if (na->treasury < 2) return;
    int cand[N_COUNT], c = 0;
    for (int t = 0; t < N_COUNT; t++) {
        if (t == a || !g->nat[t].alive) continue;
        if (War_AtWar(g, a, t) || Relation(g, a, t) < -40) cand[c++] = t;
    }
    if (!c) return;
    float freq = 0.05f + NATIONS[a].aggressiveness * 0.15f;
    if (Randf(g) > freq) return;
    int t = cand[Rand(g) % (unsigned)c];
    bool war = War_AtWar(g, a, t);
    static const int warOps[] = { OP_LOGISTIQUE, OP_BROUILLAGE, OP_RESEAU, OP_RESEAU, OP_ASAT };
    static const int peaceOps[] = { OP_INGERENCE, OP_DESTAB, OP_VOL_TECH, OP_VOL_TECH, OP_RESEAU };
    int op = war ? warOps[Rand(g) % 5] : peaceOps[Rand(g) % 5];
    if (op == OP_ASAT && (!Nuke_IsPower(g, a) || g->nat[t].asat > 0)) op = OP_RESEAU;
    if (Capacity(g, a, OPS[op].domain) < OPS[op].capacity - 1) return;
    na->treasury -= OPS[op].budget * 0.5f;
    Execute(g, a, t, op);
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
    for (int a = 1; a < N_COUNT; a++) AiOps(g, a);
}
