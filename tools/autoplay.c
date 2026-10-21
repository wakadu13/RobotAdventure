/*
 * Banc d'essai d'equilibrage : joue des parties automatiques sans interface.
 *   make sim && ./build/autoplay [nombre de parties] [difficulte 0-2]
 */
#include "../src/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strategie passive : on ne fait que passer les tours */
static void PlayPassive(Game *g)
{
    if (g->pendingDilemma >= 0) Game_ResolveDilemma(g, 1);
}

static void ResolveDilemmas(Game *g, bool hawk)
{
    if (g->pendingDilemma < 0) return;
    int d = g->pendingDilemma, c = g->turn % 2;
    if (d == DLM_PEACE_OFFER || d == DLM_NUKE_BLACKMAIL) c = hawk ? 1 : 0;
    if (d == DLM_COUP_LEGAL) c = hawk ? 0 : 1;
    if (d == 12 || d == 13) c = 1;
    Game_ResolveDilemma(g, c);
}

/* les divisions inactives rejoignent les provinces frontalieres menacees */
static void BotDefend(Game *g)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->owner != N_FRANCE || u->training || u->target >= 0) continue;
        bool front = false;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++)
            if (War_AtWar(g, N_FRANCE, g->prov[PROVINCE_ADJ[u->prov][k]].ctrl)) front = true;
        if (front) continue;
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[u->prov][k] >= 0; k++) {
            int q = PROVINCE_ADJ[u->prov][k];
            if (g->prov[q].ctrl != N_FRANCE) continue;
            bool qFront = false;
            for (int j = 0; j < MAX_ADJ && PROVINCE_ADJ[q][j] >= 0; j++)
                if (War_AtWar(g, N_FRANCE, g->prov[PROVINCE_ADJ[q][j]].ctrl)) qFront = true;
            if (qFront) { War_Order(g, i, q); break; }
        }
    }
}

/* attaque une province ennemie si toutes les divisions voisines donnent assez de chances */
static void BotAttack(Game *g, float minOdds)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        if (!War_AtWar(g, N_FRANCE, g->prov[p].ctrl)) continue;
        int idx[MAX_UNITS], c = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (!u->active || u->owner != N_FRANCE || u->training || u->target >= 0 || u->org < 0.5f) continue;
            if (UNIT_TYPES[u->type].domain != DOM_LAND) continue;
            if (War_OrderBlocked(g, i, p)) continue;
            War_Order(g, i, p);
            idx[c++] = i;
        }
        if (c && War_BattleOdds(g, p, N_FRANCE) < minOdds)
            for (int k = 0; k < c; k++) War_Order(g, idx[k], g->units[idx[k]].prov);
    }
}

/* signe le traite par defaut des que le score le permet */
static void SeekPeace(Game *g, int n, int minTurns)
{
    if (War_Turns(g, N_FRANCE, n) < minTurns) return;
    War_TreatyDraft(g, n);
    for (int k = 0; k < 2 && g->treaty.active; k++) if (War_Negotiate(g) != 2) break;
    g->treaty.active = false;
}

/* gestion courante : defense M2MC, projets, usines, diplomatie, commerce */
static void Manage(Game *g)
{
    ResolveDilemmas(g, false);
    for (int t = 0; t < TH_COUNT; t++) {
        Domain d = THEATERS[t].domain;
        int want = 1 + 2 * Game_ThreatCount(g, t);
        while (g->posture[t] < want && Game_ChangePosture(g, t, 1)) {}
        if (g->posture[t] < want)
            for (int o = 0; o < TH_COUNT; o++)
                if (o != t && THEATERS[o].domain == d && Game_ThreatCount(g, o) == 0 && g->posture[o] > 1) {
                    Game_ChangePosture(g, o, -1);
                    Game_ChangePosture(g, t, 1);
                }
    }
    static const int order[] = { P_ENR, P_AGRO, P_DESSAL, P_ADAPT, P_SMR, P_CYBERCIV, P_MEDIAS, P_CONSTELL };
    for (int i = 0; i < P_COUNT; i++) {
        int p = order[i];
        if (g->proj[p].state == 0 && g->res[R_BUDGET] > Game_ProjectCost(g, p) + 15) Game_StartProject(g, p);
    }
    for (int d = 0; d < D_COUNT; d++)
        if (Game_PostureUsed(g, d) >= g->capacity[d] && g->res[R_BUDGET] > Game_InvestCost(g, d) + 30 && Eco_Net(g) > 2)
            Game_InvestCapacity(g, d);
    if (g->res[R_BUDGET] > 60 && Eco_Net(g) > 3) {
        int best = -1;
        for (int p = 0; p < PROV_COUNT; p++)
            if (!Eco_FactoryBlocked(g, p) && (best < 0 || g->prov[p].pop > g->prov[best].pop)) best = p;
        if (best >= 0) Eco_BuildFactory(g, best);
    }
    /* fiscalite : on ajuste les impots au solde */
    float net = Eco_Net(g);
    if (net < 0 && g->res[R_BUDGET] < 30) for (int s = 0; s < S_COUNT; s++) Eco_SetTax(g, s, g->taxRate[s] + 0.05f);
    if (net > 8 && g->res[R_BUDGET] > 80) for (int s = 0; s < S_COUNT; s++) Eco_SetTax(g, s, g->taxRate[s] - 0.05f);

    for (int r = 0; r < SR_COUNT; r++)
        if (g->nat[N_FRANCE].strat[r] < 4 && g->res[R_BUDGET] > 40) Strat_Buy(g, r);
    for (int n = 1; n < N_COUNT; n++) War_Diplomacy(g, n, 1);
    for (int k = 0; k < BASE_CP; k++) {
        int best = -1; float bv = 0;
        for (int f = 0; f < F_COUNT; f++) {
            float v = g->fac[f].hostility * g->fac[f].capability;
            if (!g->fac[f].actedThisTurn && v > bv) { bv = v; best = f; }
        }
        if (best < 0) break;
        if (!Game_FactionAction(g, best, A_INFLUENCE))
            if (!Game_FactionAction(g, best, A_DIPLO))
                Game_FactionAction(g, best, A_RENSEIGNEMENT);
    }
    for (int n = 1; n < N_COUNT; n++) {
        if (!War_AtWar(g, N_FRANCE, n)) continue;
        Nuke_Warning(g, n);
        SeekPeace(g, n, 4);
    }
    BotDefend(g);
    BotAttack(g, 0.85f);
}

static void Recruit(Game *g, float reserve)
{
    static const int types[] = { U_INF, U_MECA, U_BLINDE, U_ARTILLERIE, U_DRONES, U_INF };
    static const int sites[] = { PV_FR_GES, PV_FR_HDF, PV_FR_BFC, PV_FR_ARA };
    for (int k = 0; k < 6; k++)
        if (g->res[R_BUDGET] > reserve && Eco_Net(g) > 3)
            for (int s = 0; s < 4; s++)
                if (War_Recruit(g, types[(g->turn + k) % 6], sites[(g->turn + s) % 4])) break;
}

static void WarOnWeakest(Game *g, float ratio)
{
    int best = -1; float bestArmy = 1e9f;
    for (int n = 1; n < N_COUNT; n++)
        if (!War_DeclareBlocked(g, n) && g->nat[n].army < bestArmy) { bestArmy = g->nat[n].army; best = n; }
    if (best >= 0 && bestArmy < g->nat[N_FRANCE].army * ratio) War_Declare(g, best);
}

/* conquete moderee : gestion courante + quelques guerres contre des voisins faibles */
/* le stratege joue comme un joueur attentif : groupes d'armees, generaux, operations, garnisons */
static void StrategistStaff(Game *g)
{
    int enemy = -1;
    for (int n = 1; n < N_COUNT && enemy < 0; n++) if (War_AtWar(g, N_FRANCE, n)) enemy = n;
    /* un groupe d'assaut (2/3 des divisions) et un groupe de garnison */
    int k = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->owner != N_FRANCE || UNIT_TYPES[u->type].domain != DOM_LAND) continue;
        War_SetGroup(g, i, (k++ % 3) == 2 ? 2 : 1);
    }
    if (enemy >= 0) {
        War_SetGroupOrder(g, 1, GM_ATTACK, enemy);
        War_SetGroupOrder(g, 2, GM_HOLD, enemy);
        /* reconnaissance puis brouillage de l'ennemi */
        if (!Ops_Blocked(g, enemy, OP_RECON)) Ops_Run(g, enemy, OP_RECON);
        else if (!Ops_Blocked(g, enemy, OP_BROUILLAGE)) Ops_Run(g, enemy, OP_BROUILLAGE);
        /* les sous-marins menent la guerre au commerce */
        for (int i = 0; i < MAX_UNITS; i++)
            if (g->units[i].active && g->units[i].owner == N_FRANCE && g->units[i].type == U_SNA) War_SetMission(g, i, NM_RAID);
    } else {
        War_SetGroupOrder(g, 1, GM_NONE, 0);
        War_SetGroupOrder(g, 2, GM_NONE, 0);
    }
    /* generaux : la ou se trouvent le plus de divisions */
    int gi = 0;
    for (int i = 0; i < MAX_GENERALS; i++) {
        const General *gen = &g->generals[i];
        if (!gen->active || gen->owner != N_FRANCE) continue;
        int best = -1, bc = 0;
        for (int p = 0; p < PROV_COUNT; p++) {
            if (!War_Friendly(g, N_FRANCE, p) || Mil_GeneralAt(g, N_FRANCE, p) >= 0) continue;
            int c = War_StackCount(g, N_FRANCE, p, DOM_LAND);
            if (c > bc) { bc = c; best = p; }
        }
        if (best >= 0 && (gen->prov < 0 || War_StackCount(g, N_FRANCE, gen->prov, DOM_LAND) < bc)) Mil_Assign(g, i, best);
        gi++;
    }
    if (gi < 3) Mil_Promote(g);
    /* territoires conquis : pluralisme et garnisons */
    bool conquered = false;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == N_FRANCE && g->prov[p].core != N_FRANCE) conquered = true;
    if (conquered && g->culturePolicy != CP_PLURALISME) Soc_SetCulturePolicy(g, CP_PLURALISME);
    for (int t = 0; t < TH_COUNT; t++)
        if (THEATERS[t].domain == D_TERRE && t == TH_EXTERIEUR && Game_ThreatCount(g, t) > 0) Game_ChangePosture(g, t, 1);
}

static void PlayStrategist(Game *g)
{
    if (g->turn > 8 && War_UnitLoad(g, N_FRANCE) < War_ArmyMax(g, N_FRANCE) * 0.8f) Recruit(g, 12);
    Manage(g);
    if (g->turn == 4) Soc_ChangeRegime(g, RG_AUTORITAIRE);
    bool atWar = War_AtWarAny(g, N_FRANCE);
    if (g->turn == 5) Mil_SetResearchBudget(g, 3);
    if (!atWar && Soc_NationProvinces(g, N_FRANCE) < 22 && g->turn > 12 && g->res[R_COHESION] > 50) WarOnWeakest(g, 0.5f);
    StrategistStaff(g);
}

/* conquerant : junte, armee maximale, guerres en chaine */
static void PlayConqueror(Game *g)
{
    ResolveDilemmas(g, true);
    if (g->turn == 2) Soc_ChangeRegime(g, RG_AUTORITAIRE);
    if (g->turn == 16) Soc_ChangeRegime(g, RG_JUNTE);
    Recruit(g, 15);
    bool atWar = War_AtWarAny(g, N_FRANCE);
    if (!atWar && g->turn > 10) WarOnWeakest(g, 0.7f);
    BotDefend(g);
    BotAttack(g, 0.5f);
    for (int n = 1; n < N_COUNT; n++) if (War_AtWar(g, N_FRANCE, n)) SeekPeace(g, n, 12);
}

static void Run(const char *label, void (*strat)(Game *), int games, int diff)
{
    int wins = 0, causes[7] = { 0 }, turnsSum = 0, provSum = 0, scoreSum = 0, vic[VIC_COUNT] = { 0 };
    float resSum[R_COUNT] = { 0 }, popSum = 0;
    for (int i = 0; i < games; i++) {
        Game g;
        Game_Init(&g, 1000u + (unsigned)i * 7919u, diff);
        while (g.state == GAME_RUNNING) {
            strat(&g);
            if (g.pendingDilemma >= 0) Game_ResolveDilemma(&g, 0);
            Game_EndTurn(&g);
        }
        causes[g.state]++;
        if (g.state == GAME_WON) { wins++; vic[Game_Victory(&g)]++; }
        turnsSum += g.turn;
        provSum += Soc_NationProvinces(&g, N_FRANCE);
        popSum += Soc_NationPop(&g, N_FRANCE);
        scoreSum += Game_Score(&g, NULL);
        for (int r = 0; r < R_COUNT; r++) resSum[r] += g.res[r];
    }
    printf("%-12s victoires %3d%%  tours %3.0f  prov %4.1f  pop %3.0f M  score %5d | coh %d souv %d budget %d conq %d nuc %d\n",
           label, wins * 100 / games, turnsSum / (float)games, provSum / (float)games, popSum / games, scoreSum / games,
           causes[GAME_LOST_COHESION], causes[GAME_LOST_SOUVERAINETE], causes[GAME_LOST_BUDGET],
           causes[GAME_LOST_CONQUEST], causes[GAME_LOST_NUCLEAR]);
    printf("             fin moy :");
    for (int r = 0; r < R_COUNT; r++) printf(" %s=%.0f", RESOURCE_NAMES[r], resSum[r] / games);
    printf("\n             victoires :");
    for (int v = 1; v < VIC_COUNT; v++) if (vic[v]) printf(" %s=%d", VICTORY_NAMES[v], vic[v]);
    printf("\n");
}

int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : 100;
    int diff = argc > 2 ? atoi(argv[2]) : DIFF_NORMAL;
    printf("Difficulté : %s, %d parties par stratégie\n", DIFFICULTY_NAMES[diff], games);
    Run("passive", PlayPassive, games, diff);
    Run("gestionnaire", Manage, games, diff);
    Run("stratege", PlayStrategist, games, diff);
    Run("conquerant", PlayConqueror, games, diff);
    return 0;
}
