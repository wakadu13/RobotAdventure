/*
 * BOUCLIER 2050 - dissuasion nucleaire : posture et credibilite, parapluie
 * europeen, ultime avertissement, emploi de l'arme et riposte, chantage.
 *
 * L'arme reste abstraite : son interet est de ne jamais servir. Son emploi
 * fait de la France un paria et, contre une puissance nucleaire, provoque
 * un echange qui met fin a la partie.
 */
#include "game_internal.h"

#include <stdio.h>

float Nuke_Upkeep(const Game *g)
{
    static const float cost[3] = { 0.5f, 1.5f, 3.0f };
    return cost[g->nukePosture] + (g->umbrella ? 1.0f : 0);
}

void Nuke_SetPosture(Game *g, int posture)
{
    if (posture == g->nukePosture || posture < 0 || posture > 2) return;
    g->nukePosture = posture;
    if (posture == 0 && g->umbrella) g->umbrella = false;
    Game_Log(g, EV_INFO, "Posture de dissuasion : %s.", NUKE_POSTURES[posture]);
}

const char *Nuke_UmbrellaBlocked(const Game *g)
{
    if (g->umbrella) return NULL;
    if (!g->nat[N_FRANCE].eu) return "La France doit être membre de l'Union européenne";
    if (g->credibility < 50) return "Crédibilité insuffisante (50)";
    return NULL;
}

void Nuke_ToggleUmbrella(Game *g)
{
    if (!g->umbrella && Nuke_UmbrellaBlocked(g)) return;
    g->umbrella = !g->umbrella;
    if (g->umbrella) {
        AddRes(g, R_INFLUENCE, 5);
        Game_Log(g, EV_GOOD, "La dissuasion française s'étend désormais à nos partenaires européens.");
    } else {
        AddRes(g, R_INFLUENCE, -5);
        Game_Log(g, EV_BAD, "La France retire son parapluie nucléaire à l'Europe.");
    }
}

/* tirage : la nation n renonce-t-elle a toucher le sol national ? */
bool Nuke_Deters(Game *g, int n)
{
    float p = g->credibility / 100.0f * (NATIONS[n].nuclear ? 0.6f : 1.0f);
    return Randf(g) < p;
}

/* ------------------------------------------------------------------ */
/* Ultime avertissement                                                */
/* ------------------------------------------------------------------ */

static bool HoldsFrenchSoil(const Game *g, int n)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n && PROVINCES[p].nation == N_FRANCE) return true;
    return false;
}

const char *Nuke_WarningBlocked(const Game *g, int n)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (!g->nat[n].atWar) return "Pas en guerre";
    if (g->nukePosture < 1) return "Posture minimale : la menace ne serait pas crédible";
    if (g->nat[n].warned) return "Avertissement déjà adressé durant cette guerre";
    if (!HoldsFrenchSoil(g, n)) return "Nos intérêts vitaux ne sont pas menacés (aucune province française occupée)";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool Nuke_Warning(Game *g, int n)
{
    if (Nuke_WarningBlocked(g, n)) return false;
    g->commandPoints--;
    g->nat[n].warned = true;
    float chance = g->credibility / 100.0f * (NATIONS[n].nuclear ? 0.5f : 1.0f);
    if (Randf(g) < chance) {
        Game_Log(g, EV_WAR, "Ultime avertissement : %s se retire du territoire national et signe la paix.", NATIONS[n].name);
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == n && PROVINCES[p].nation == N_FRANCE) War_Capture(g, p, N_FRANCE);
        War_MakePeace(g, n, true);
        g->credibility = Clampf(g->credibility + 10, 0, 100);
        return true;
    }
    g->credibility = Clampf(g->credibility - 20, 0, 100);
    Game_Log(g, EV_BAD, "Ultime avertissement ignoré par %s : notre dissuasion perd en crédibilité.", NATIONS[n].name);
    return false;
}

/* ------------------------------------------------------------------ */
/* Emploi de l'arme                                                    */
/* ------------------------------------------------------------------ */

const char *Nuke_StrikeBlocked(const Game *g, int p)
{
    int n = g->prov[p].owner;
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (n == N_FRANCE) return "Territoire national";
    if (!g->nat[n].atWar) return "Pas en guerre avec cette nation";
    if (g->nukePosture < 1) return "Posture minimale : pas de capacité de frappe opérationnelle";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

static void Devastate(Game *g, int p, float popLoss)
{
    Province *pr = &g->prov[p];
    pr->pop *= 1 - popLoss;
    pr->damage = 1;
    pr->fallout = Clampf(pr->fallout + 0.8f, 0, 1);
    pr->prosperity *= 0.5f;
    pr->industry *= 0.3f;
    for (int i = 0; i < MAX_UNITS; i++)
        if (g->units[i].active && g->units[i].prov == p) g->units[i].active = false;
}

bool Nuke_Strike(Game *g, int p)
{
    if (Nuke_StrikeBlocked(g, p)) return false;
    int n = g->prov[p].owner;
    g->commandPoints--;
    g->nukesUsed++;
    Devastate(g, p, 0.4f);
    Game_Log(g, EV_WAR, "Frappe nucléaire sur %s. Le monde retient son souffle.", PROVINCE_NAMES[p]);

    /* la France devient un paria */
    for (int k = 1; k < N_COUNT; k++) if (g->nat[k].alive) AddRelation(g, k, -60);
    g->aggressionMemory = 100;
    AddRes(g, R_INFLUENCE, -50);
    g->nat[N_FRANCE].eu = false;
    g->umbrella = false;
    Regime r = g->nat[N_FRANCE].regime;
    AddRes(g, R_COHESION, r == RG_TOTALITAIRE ? -5 : r == RG_JUNTE ? -10 : -25);
    g->temp += 0.02f;

    if (NATIONS[n].nuclear && g->nat[n].alive) {
        /* riposte : l'echange nucleaire met fin a la partie */
        Devastate(g, PV_FR_NORD, 0.5f);
        Devastate(g, PV_FR_RA, 0.4f);
        Devastate(g, PV_FR_SUD, 0.4f);
        Game_Log(g, EV_WAR, "%s riposte. Paris, Lyon et Marseille sont frappées.", NATIONS[n].name);
        g->state = GAME_LOST_NUCLEAR;
        return true;
    }
    if (g->nat[n].alive) {
        Game_Log(g, EV_WAR, "%s capitule sans conditions.", NATIONS[n].name);
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == n) g->units[i].org = 0;
        War_MakePeace(g, n, true);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Nuke_Turn(Game *g)
{
    static const float base[3] = { 25, 60, 85 };
    float target = base[g->nukePosture] + g->capacity[D_ESPACE] * 1.5f + g->capacity[D_MER] * 1.0f;
    Regime r = g->nat[N_FRANCE].regime;
    if (r == RG_JUNTE || r == RG_TOTALITAIRE) target += 8;   /* imprevisibilite */
    if (g->nukesUsed) target += 15;                           /* elle a deja servi */
    target = Clampf(target, 0, 100);
    g->credibility += (target - g->credibility) * 0.1f;
    if (g->umbrella) AddRes(g, R_INFLUENCE, 0.5f);

    /* chantage de la Federation Boreale lorsqu'elle perd la guerre */
    const Nation *bo = &g->nat[N_BOREALE];
    if (bo->alive && bo->atWar && bo->warTurns > 4 && g->pendingDilemma < 0
        && (bo->army < g->nat[N_FRANCE].army || Soc_NationProvinces(g, N_BOREALE) < bo->startProvinces)
        && Randf(g) < 0.15f) {
        g->pendingDilemma = DLM_NUKE_BLACKMAIL;
        g->dilemmaParam = N_BOREALE;
    }
}

/* resolution du chantage (appelee depuis les dilemmes) */
void Nuke_ResolveBlackmail(Game *g, bool give)
{
    int n = g->dilemmaParam;
    if (give) {
        War_MakePeace(g, n, true);
        AddRes(g, R_COHESION, -8);
        g->credibility = Clampf(g->credibility - 10, 0, 100);
        return;
    }
    float strike = (1 - g->credibility / 100.0f) * 0.3f;
    if (Randf(g) < strike) {
        int targets[] = { PV_FR_EST, PV_FR_OUEST, PV_FR_SO };
        int p = targets[Rand(g) % 3];
        Devastate(g, p, 0.3f);
        AddRes(g, R_COHESION, -25);
        AddRes(g, R_SOUVERAINETE, -15);
        for (int k = 1; k < N_COUNT; k++) if (k != n) AddRelation(g, k, 30);
        Game_Log(g, EV_WAR, "%s a frappé %s. Le monde se range derrière la France.", NATIONS[n].name, PROVINCE_NAMES[p]);
    } else {
        g->credibility = Clampf(g->credibility + 10, 0, 100);
        Game_Log(g, EV_GOOD, "Le chantage de %s échoue : notre dissuasion a tenu.", NATIONS[n].name);
    }
}
