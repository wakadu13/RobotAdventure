/*
 * BOUCLIER 2050 - etat-major : technologies militaires (8 domaines) et
 * generaux (competence, trait, experience, affectation a une province).
 */
#include "game_internal.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Technologies                                                        */
/* ------------------------------------------------------------------ */

float Mil_TechCost(const Game *g, int area)
{
    return 20.0f * (g->nat[N_FRANCE].tech[area] + 1);
}

float Mil_ResearchPoints(const Game *g)
{
    float lit = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) { lit += g->prov[p].literacy * g->prov[p].pop; pop += g->prov[p].pop; }
    lit = pop > 0 ? lit / pop : 0.5f;
    static const float regime[RG_COUNT] = { 1.1f, 1.0f, 1.3f, 1.0f, 0.9f, 0.9f };
    return g->researchBudget * 1.6f * (0.5f + lit) * regime[g->nat[N_FRANCE].regime];
}

void Mil_SetResearch(Game *g, int area)
{
    if (area >= 0 && area < T_COUNT && g->nat[N_FRANCE].tech[area] < TECH_MAX) g->researchArea = area;
}

void Mil_SetResearchBudget(Game *g, float md)
{
    g->researchBudget = Clampf(md, 0, 12);
}

bool Mil_UnitUnlocked(const Game *g, int n, int type)
{
    return g->nat[n].tech[UNIT_TYPES[type].tech] >= UNIT_TYPES[type].techLevel;
}

float Mil_TechBonus(const Game *g, int n, int type)
{
    return 1 + 0.06f * g->nat[n].tech[UNIT_TYPES[type].tech];
}

/* ------------------------------------------------------------------ */
/* Generaux                                                            */
/* ------------------------------------------------------------------ */

int Mil_GeneralAt(const Game *g, int n, int p)
{
    for (int i = 0; i < MAX_GENERALS; i++)
        if (g->generals[i].active && g->generals[i].owner == n && g->generals[i].prov == p) return i;
    return -1;
}

/* multiplicateur du general commandant en p ; type < 0 : logistique */
float Mil_GeneralBonus(const Game *g, int n, int p, int type, bool attack)
{
    int i = Mil_GeneralAt(g, n, p);
    if (i < 0) return 1;
    const General *gen = &g->generals[i];
    if (type < 0) return gen->trait == GT_LOGISTICIEN ? 1.2f : 1.05f;
    float k = 1 + 0.05f * gen->skill;
    if (gen->trait == GT_OFFENSIF && attack) k += 0.12f;
    if (gen->trait == GT_DEFENSIF && !attack) k += 0.12f;
    if (gen->trait == GT_BLINDES && (type == U_MECA || type == U_BLINDE)) k += 0.15f;
    if (gen->trait == GT_MONTAGNE && PROVINCE_TERRAIN[p] >= 1) k += 0.15f;
    return k;
}

static int SpawnGeneral(Game *g, int n, const char *name, int prov, int skill)
{
    for (int i = 0; i < MAX_GENERALS; i++) {
        General *gen = &g->generals[i];
        if (gen->active) continue;
        memset(gen, 0, sizeof(*gen));
        gen->active = true;
        gen->owner = (signed char)n;
        gen->prov = (short)prov;
        gen->skill = (unsigned char)skill;
        gen->trait = (unsigned char)(Rand(g) % GT_COUNT);
        snprintf(gen->name, sizeof(gen->name), "%s", name);
        return i;
    }
    return -1;
}

static int FrenchGeneralCount(const Game *g)
{
    int c = 0;
    for (int i = 0; i < MAX_GENERALS; i++) if (g->generals[i].active && g->generals[i].owner == N_FRANCE) c++;
    return c;
}

const char *Mil_PromoteBlocked(const Game *g)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (FrenchGeneralCount(g) >= 8) return "État-major complet (8 généraux)";
    if (g->res[R_BUDGET] < 3) return "Budget insuffisant (3 Md)";
    return NULL;
}

bool Mil_Promote(Game *g)
{
    if (Mil_PromoteBlocked(g)) return false;
    /* un nom encore libre */
    const char *name = FRENCH_GENERALS[Rand(g) % 20];
    for (int tries = 0; tries < 40; tries++) {
        bool used = false;
        for (int i = 0; i < MAX_GENERALS; i++)
            if (g->generals[i].active && strcmp(g->generals[i].name, name) == 0) used = true;
        if (!used) break;
        name = FRENCH_GENERALS[Rand(g) % 20];
    }
    int i = SpawnGeneral(g, N_FRANCE, name, -1, 1 + (int)(Rand(g) % 2));
    if (i < 0) return false;
    g->res[R_BUDGET] -= 3;
    Game_Log(g, EV_GOOD, "%s est promu (%s, compétence %d). Affectez-le à une province.",
             g->generals[i].name, GENERAL_TRAITS[g->generals[i].trait], g->generals[i].skill);
    return true;
}

const char *Mil_AssignBlocked(const Game *g, int i, int p)
{
    if (i < 0 || i >= MAX_GENERALS || !g->generals[i].active || g->generals[i].owner != N_FRANCE) return "Général invalide";
    if (p < 0) return NULL;
    if (!War_Friendly(g, N_FRANCE, p)) return "Province non tenue par la France ou un allié";
    return NULL;
}

bool Mil_Assign(Game *g, int i, int p)
{
    if (Mil_AssignBlocked(g, i, p)) return false;
    int other = p >= 0 ? Mil_GeneralAt(g, N_FRANCE, p) : -1;
    if (other >= 0 && other != i) g->generals[other].prov = -1;
    g->generals[i].prov = (short)p;
    return true;
}

void Mil_BattleXp(Game *g, int n, int p, bool won)
{
    for (int i = 0; i < MAX_GENERALS; i++) {
        General *gen = &g->generals[i];
        if (!gen->active || gen->owner != n || gen->prov < 0) continue;
        bool here = gen->prov == p;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0 && !here; k++)
            if (PROVINCE_ADJ[p][k] == gen->prov) here = true;
        if (!here) continue;
        gen->xp += won ? 2 : 1;
        static const float levels[5] = { 0, 5, 12, 25, 40 };
        if (gen->skill < 5 && gen->xp >= levels[gen->skill]) {
            gen->skill++;
            if (n == N_FRANCE) Game_Log(g, EV_GOOD, "%s gagne en expérience (compétence %d).", gen->name, gen->skill);
        }
        if (!won && Randf(g) < 0.03f) {
            gen->active = false;
            if (n == N_FRANCE) Game_Log(g, EV_BAD, "%s est tombé au combat.", gen->name);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Initialisation et tour                                              */
/* ------------------------------------------------------------------ */

static float NationWealth(const Game *g, int n)
{
    float w = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n) { w += PROVINCES[p].wealth * g->prov[p].pop; pop += g->prov[p].pop; }
    return pop > 0 ? w / pop : 0;
}

static int NationCapital(const Game *g, int n)
{
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n && g->prov[p].ctrl == n && (best < 0 || g->prov[p].pop > g->prov[best].pop)) best = p;
    return best;
}

void Mil_Init(Game *g)
{
    static const unsigned char france[T_COUNT] = { 1, 1, 1, 1, 1, 1, 1, 2 };
    memcpy(g->nat[N_FRANCE].tech, france, sizeof(france));
    for (int n = 1; n < N_COUNT; n++) {
        int base = (int)(NationWealth(g, n) * 5 + 0.5f);
        if (base > 3) base = 3;
        for (int t = 0; t < T_COUNT; t++) g->nat[n].tech[t] = (unsigned char)(t == T_NUC ? (g->nat[n].nukeStatus >= 2 ? 3 : 0) : base);
    }
    g->researchArea = T_AIR;
    g->researchBudget = 1.0f;

    memset(g->generals, 0, sizeof(g->generals));
    SpawnGeneral(g, N_FRANCE, FRENCH_GENERALS[0], PV_FR_IDF, 3);
    SpawnGeneral(g, N_FRANCE, FRENCH_GENERALS[1], PV_FR_GES, 2);
    for (int n = 1; n < N_COUNT; n++) {
        int cap = NationCapital(g, n);
        SpawnGeneral(g, n, NATIONS[n].general[0], cap, 2);
        if (War_ArmyMax(g, n) > 10) SpawnGeneral(g, n, NATIONS[n].general[1], cap, 1);
    }
}

void Mil_Turn(Game *g)
{
    /* recherche francaise */
    int a = g->researchArea;
    if (g->nat[N_FRANCE].tech[a] < TECH_MAX) {
        g->researchProgress += Mil_ResearchPoints(g);
        if (g->researchProgress >= Mil_TechCost(g, a)) {
            g->researchProgress -= Mil_TechCost(g, a);
            g->nat[N_FRANCE].tech[a]++;
            Game_Log(g, EV_GOOD, "Percée technologique : %s niveau %d.", TECHS[a].name, g->nat[N_FRANCE].tech[a]);
            if (g->nat[N_FRANCE].tech[a] >= TECH_MAX)
                for (int t = 0; t < T_COUNT; t++) if (g->nat[N_FRANCE].tech[t] < TECH_MAX) { g->researchArea = t; break; }
        }
    }

    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;
        /* progres technique des autres nations */
        if (Randf(g) < 0.02f + NationWealth(g, n) * 0.08f) {
            int t = (int)(Rand(g) % T_COUNT);
            if (t != T_NUC && na->tech[t] < TECH_MAX) na->tech[t]++;
        }
        /* les generaux rejoignent le front le plus garni */
        int best = -1, bestCount = 0;
        if (War_AtWarAny(g, n)) {
            for (int p = 0; p < PROV_COUNT; p++) {
                if (!War_Friendly(g, n, p) || Mil_GeneralAt(g, n, p) >= 0) continue;
                int c = War_StackCount(g, n, p, DOM_LAND);
                if (c > bestCount) { bestCount = c; best = p; }
            }
        }
        bool has = false;
        for (int i = 0; i < MAX_GENERALS; i++) {
            General *gen = &g->generals[i];
            if (!gen->active || gen->owner != n) continue;
            has = true;
            if (gen->prov >= 0 && !War_Friendly(g, n, gen->prov)) gen->prov = (short)NationCapital(g, n);
            if (best >= 0 && (gen->prov < 0 || War_StackCount(g, n, gen->prov, DOM_LAND) < bestCount)) {
                gen->prov = (short)best;
                best = -1;
            }
        }
        if (!has && Randf(g) < 0.1f) SpawnGeneral(g, n, NATIONS[n].general[Rand(g) % 2], NationCapital(g, n), 1);
    }
    /* un general francais dont la province est perdue revient en reserve */
    for (int i = 0; i < MAX_GENERALS; i++) {
        General *gen = &g->generals[i];
        if (gen->active && gen->owner == N_FRANCE && gen->prov >= 0 && !War_Friendly(g, N_FRANCE, gen->prov)) gen->prov = -1;
    }
}
