/*
 * BOUCLIER 2050 - guerre et diplomatie : armees, offensives, ultimatums,
 * coalitions, IA des nations (guerres entre elles, offres de paix).
 */
#include "game_internal.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Puissance                                                           */
/* ------------------------------------------------------------------ */

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
    if (n != N_FRANCE) {
        m *= Clampf(0.4f + AvgWealth(g, n) * 1.5f, 0.4f, 1.0f);
        if (g->nat[n].failed) m *= 0.5f;
        if (g->nat[n].atWar) m *= 1.5f;   /* mobilisation generale face a la France */
        m *= 1 + g->aggressionMemory / 50.0f;  /* l'Europe se rearme face a un agresseur */
    }
    return m;
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
    return q;
}

bool War_Adjacent(const Game *g, int a, int b)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != a) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
            if (g->prov[PROVINCE_ADJ[p][k]].owner == b) return true;
    }
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

bool War_ProvinceAttackable(const Game *g, int p)
{
    int n = g->prov[p].owner;
    return n != N_FRANCE && g->nat[n].atWar && TouchesNation(g, p, N_FRANCE, NULL);
}

static float DefenseStrength(const Game *g, int p)
{
    int n = g->prov[p].owner;
    float army = n == N_FRANCE ? g->nat[n].army - g->divisionsUsed * 0.5f : g->nat[n].army;
    if (army < 0) army = 0;
    float d = army * 0.5f * War_Quality(g, n) * (g->prov[p].core == n ? 1.3f : 1.0f);
    if (n == N_FRANCE) d += Game_PostureUsed(g, D_TERRE) * 0.15f;
    /* garnisons, reservistes et defense territoriale */
    return d + 0.5f + g->prov[p].pop * 0.04f;
}

static float AttackStrength(const Game *g, int p, float divisions)
{
    bool seaOnly = false;
    TouchesNation(g, p, N_FRANCE, &seaOnly);
    float s = divisions * War_Quality(g, N_FRANCE);
    if (seaOnly) s *= g->capacity[D_MER] >= 6 ? 0.8f : 0.5f;
    return s;
}

/* probabilite que att*U(0.75,1.25) > def*U(0.75,1.25) */
static float Odds(float att, float def)
{
    if (att <= 0) return 0;
    int win = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            if (att * (0.75f + i / 30.0f) > def * (0.75f + j / 30.0f)) win++;
    return win / 256.0f;
}

float War_AttackOdds(const Game *g, int p, float divisions)
{
    return Odds(AttackStrength(g, p, divisions), DefenseStrength(g, p));
}

float War_AvailableDivisions(const Game *g)
{
    float a = g->nat[N_FRANCE].army - g->divisionsUsed;
    return a < 0 ? 0 : a;
}

/* ------------------------------------------------------------------ */
/* Conquete                                                            */
/* ------------------------------------------------------------------ */

void War_Capture(Game *g, int p, int newOwner)
{
    Province *pr = &g->prov[p];
    int old = pr->owner;
    pr->owner = newOwner;
    pr->integration = pr->core == newOwner ? 100 : 0;
    pr->damage = Clampf(pr->damage + 0.3f, 0, 1);
    pr->pop *= 0.97f;
    pr->revolt = 0;
    if (pr->core != newOwner)
        for (int s = 0; s < S_COUNT; s++) pr->mil[s] = Clampf(pr->mil[s] + 2, 0, 10);
    if (newOwner == N_FRANCE) g->statConquered++;
    if (old == N_FRANCE) g->statLost++;
    if (old == N_FRANCE && p == PV_FR_NORD) {
        AddRes(g, R_COHESION, -20);
        AddRes(g, R_SOUVERAINETE, -20);
    }
    if (old != N_FRANCE && Soc_NationProvinces(g, old) == 0) {
        Nation *na = &g->nat[old];
        na->alive = false;
        na->atWar = false;
        na->trade = false;
        na->warTarget = -1;
        na->army = 0;
        Game_Log(g, EV_WAR, "%s a cessé d'exister en tant qu'État.", NATIONS[old].name);
    }
}

bool War_Attack(Game *g, int p, float divisions)
{
    if (!War_ProvinceAttackable(g, p) || g->state != GAME_RUNNING) return false;
    float avail = War_AvailableDivisions(g);
    if (divisions > avail) divisions = avail;
    if (divisions < 0.5f) return false;

    int n = g->prov[p].owner;
    float att = AttackStrength(g, p, divisions) * (0.75f + Randf(g) * 0.5f);
    float def = DefenseStrength(g, p) * (0.75f + Randf(g) * 0.5f);
    g->divisionsUsed += divisions;

    if (att > def) {
        float loss = divisions * 0.12f * Clampf(def / att, 0.2f, 1.0f);
        g->nat[N_FRANCE].army -= loss;
        g->nat[n].army *= 0.82f;
        Game_Log(g, EV_WAR, "Victoire : nos forces s'emparent de %s (%s). Pertes : %.1f div.",
                 PROVINCE_NAMES[p], NATIONS[n].name, loss);
        War_Capture(g, p, N_FRANCE);
    } else {
        float loss = divisions * 0.25f;
        g->nat[N_FRANCE].army -= loss;
        g->nat[n].army *= 0.95f;
        Game_Log(g, EV_WAR, "Échec de l'offensive sur %s. Pertes : %.1f div.", PROVINCE_NAMES[p], loss);
    }
    if (g->nat[N_FRANCE].army < 0) g->nat[N_FRANCE].army = 0;
    return true;
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

bool War_Declare(Game *g, int n)
{
    if (War_DeclareBlocked(g, n)) return false;
    Nation *na = &g->nat[n];
    Nation *fr = &g->nat[N_FRANCE];
    g->commandPoints--;
    na->atWar = true;
    na->warTurns = 0;
    na->trade = false;
    na->relation = -80;
    g->aggressionMemory = Clampf(g->aggressionMemory + 25, 0, 100);
    AddRes(g, R_COHESION, -REGIMES[fr->regime].aggressionCost);
    AddRes(g, R_INFLUENCE, -8);
    Game_Log(g, EV_WAR, "La France déclare la guerre à %s !", NATIONS[n].name);

    if (na->eu && fr->eu) {
        fr->eu = false;
        Game_Log(g, EV_BAD, "Agression contre un État membre : la France est exclue de l'Union européenne.");
        for (int k = 1; k < N_COUNT; k++) {
            Nation *o = &g->nat[k];
            if (!o->alive || !o->eu || k == n) continue;
            AddRelation(g, k, -30);
            if (!o->atWar && o->army >= 2 && War_Adjacent(g, N_FRANCE, k) && Randf(g) < 0.3f) {
                o->atWar = true;
                o->warTurns = 0;
                o->trade = false;
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
    if (log) Game_Log(g, EV_WAR, "Paix signée avec %s. Trêve de 4 ans.", NATIONS[n].name);
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
        Game_Log(g, EV_GOOD, "Accord commercial signé avec %s : leurs excédents alimentent nos marchés.", NATIONS[n].name);
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

static float RecruitCost(const Game *g)
{
    return 4.0f * (g->nat[N_FRANCE].regime >= RG_JUNTE ? 0.75f : 1.0f);
}

bool War_Recruit(Game *g)
{
    Nation *fr = &g->nat[N_FRANCE];
    float c = RecruitCost(g);
    if (g->state != GAME_RUNNING || fr->army + 1 > War_ArmyMax(g, N_FRANCE) || g->res[R_BUDGET] < c) return false;
    g->res[R_BUDGET] -= c;
    fr->army += 1;
    return true;
}

bool War_Disband(Game *g)
{
    Nation *fr = &g->nat[N_FRANCE];
    if (fr->army < 1 || War_AvailableDivisions(g) < 1) return false;
    fr->army -= 1;
    return true;
}

float War_ArmyUpkeep(const Game *g)
{
    return g->nat[N_FRANCE].army * 0.5f;
}

/* ------------------------------------------------------------------ */
/* IA                                                                  */
/* ------------------------------------------------------------------ */

static float RelationTarget(const Game *g, int n)
{
    const Nation *na = &g->nat[n], *fr = &g->nat[N_FRANCE];
    float t = NATIONS[n].relation;
    t += IsDemocratic(na->regime) == IsDemocratic(fr->regime) ? 10 : -15;
    if (na->regime == fr->regime) t += 5;
    if (na->eu && fr->eu) t += 15;
    int cores = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE && g->prov[p].core == n) cores++;
    t -= cores * 35 > 80 ? 80 : cores * 35;
    t -= g->aggressionMemory * 0.4f;
    if (na->trade) t += 5;
    if (n >= N_MAROC || n == N_TURQUIE) {
        if (g->migrationDoctrine == 0) t += 8;
        if (g->migrationDoctrine == 2) t -= 12;
    }
    return Clampf(t, -100, 100);
}

static int RandomBorderProvince(Game *g, int victim, int attacker)
{
    int cand[PROV_COUNT], c = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == victim && TouchesNation(g, p, attacker, NULL)) cand[c++] = p;
    return c ? cand[Rand(g) % (unsigned)c] : -1;
}

static void AiAttackFrance(Game *g, int n)
{
    Nation *na = &g->nat[n], *fr = &g->nat[N_FRANCE];
    int p = RandomBorderProvince(g, N_FRANCE, n);
    if (p < 0) return;
    float att = na->army * 0.5f * War_Quality(g, n);
    float def = DefenseStrength(g, p);
    if (att < def * 0.7f) return;
    att *= 0.75f + Randf(g) * 0.5f;
    def *= 0.75f + Randf(g) * 0.5f;
    if (att > def) {
        fr->army *= 0.92f;
        na->army *= 0.9f;
        Game_Log(g, EV_WAR, "%s perce nos lignes et s'empare de %s !", NATIONS[n].name, PROVINCE_NAMES[p]);
        War_Capture(g, p, n);
    } else {
        fr->army *= 0.96f;
        na->army *= 0.85f;
        Game_Log(g, EV_WAR, "Offensive de %s repoussée en %s.", NATIONS[n].name, PROVINCE_NAMES[p]);
    }
}

static void AiWarOnNeighbour(Game *g, int n)
{
    Nation *na = &g->nat[n];
    int t = na->warTarget;
    Nation *tg = &g->nat[t];
    na->warTargetTurns++;
    if (!tg->alive || na->warTargetTurns > 12 || na->army < War_ArmyMax(g, n) * 0.4f) {
        if (tg->alive) Game_Log(g, EV_WAR, "Cessez-le-feu entre %s et %s.", NATIONS[n].name, NATIONS[t].name);
        na->warTarget = -1;
        return;
    }
    int p = RandomBorderProvince(g, t, n);
    if (p < 0) { na->warTarget = -1; return; }
    float att = na->army * 0.45f * War_Quality(g, n) * (0.75f + Randf(g) * 0.5f);
    float def = tg->army * 0.5f * War_Quality(g, t) * 1.3f * (0.75f + Randf(g) * 0.5f) + 0.3f;
    if (att > def) {
        na->army *= 0.92f;
        tg->army *= 0.8f;
        Game_Log(g, EV_WAR, "%s conquiert %s aux dépens de %s.", NATIONS[n].name, PROVINCE_NAMES[p], NATIONS[t].name);
        War_Capture(g, p, n);
    } else {
        na->army *= 0.88f;
        tg->army *= 0.95f;
    }
}

void War_Turn(Game *g)
{
    if (g->aggressionMemory > 0) g->aggressionMemory -= 0.6f;

    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;

        /* Etat failli */
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

        /* armee */
        float max = War_ArmyMax(g, n);
        float rate = (na->atWar || na->warTarget >= 0) ? 0.03f : 0.06f;
        na->army += (max - na->army) * rate;

        /* relations */
        if (!na->atWar) {
            na->relation += (RelationTarget(g, n) - na->relation) * 0.08f + (Randf(g) - 0.5f) * 2;
            na->relation = Clampf(na->relation, -100, 100);
            if (na->trade && na->relation < 10) {
                na->trade = false;
                Game_Log(g, EV_BAD, "%s rompt l'accord commercial.", NATIONS[n].name);
            }
        }
        if (na->truce > 0) na->truce--;

        if (na->atWar) {
            na->warTurns++;
            AiAttackFrance(g, n);
            if (!na->alive) continue;
            int lost = na->startProvinces - Soc_NationProvinces(g, n);
            bool wantsPeace = na->army < max * 0.35f || lost * 2 >= na->startProvinces
                           || (na->warTurns > 14 && Randf(g) < 0.1f);
            if (wantsPeace && g->pendingDilemma < 0 && Randf(g) < 0.4f) {
                g->pendingDilemma = DLM_PEACE_OFFER;
                g->dilemmaParam = n;
            }
        } else if (na->truce == 0 && g->aggressionMemory > 35 && na->relation < 0 && War_Adjacent(g, n, N_FRANCE)
                   && Randf(g) < 0.03f + g->aggressionMemory * 0.001f) {
            na->atWar = true;
            na->warTurns = 0;
            na->trade = false;
            Game_Log(g, EV_WAR, "Face à l'expansionnisme français, %s rejoint la coalition et nous déclare la guerre !",
                     NATIONS[n].name);
        } else if (na->truce == 0 && na->relation < -55 && War_Adjacent(g, n, N_FRANCE)
                   && na->army * War_Quality(g, n) > g->nat[N_FRANCE].army * War_Quality(g, N_FRANCE) * 1.2f
                   && Randf(g) < NATIONS[n].aggressiveness * 0.15f) {
            na->atWar = true;
            na->warTurns = 0;
            na->trade = false;
            Game_Log(g, EV_WAR, "%s déclare la guerre à la France !", NATIONS[n].name);
        }

        /* guerres entre nations */
        if (na->warTarget >= 0) {
            AiWarOnNeighbour(g, n);
        } else if (g->turn > 8 && Randf(g) < NATIONS[n].aggressiveness * 0.015f * (na->failed ? 2 : 1)) {
            int best = -1;
            float bestArmy = 1e9f;
            for (int t = 1; t < N_COUNT; t++) {
                const Nation *tg = &g->nat[t];
                if (t == n || !tg->alive || (tg->eu && na->eu) || !War_Adjacent(g, n, t)) continue;
                if (na->army * War_Quality(g, n) < tg->army * War_Quality(g, t) * 1.6f) continue;
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
    }
}
