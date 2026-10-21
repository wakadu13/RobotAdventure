/*
 * Banc d'essai d'equilibrage : joue des parties automatiques sans interface.
 *   make sim && ./build/autoplay
 */
#include "../src/game.h"

#include <stdio.h>

/* strategie passive : on ne fait que passer les tours */
static void PlayPassive(Game *g)
{
    if (g->pendingDilemma >= 0) Game_ResolveDilemma(g, 1);
}

/* strategie heuristique simple : renforce les theatres menaces, investit */
static void PlayHeuristic(Game *g)
{
    if (g->pendingDilemma >= 0) Game_ResolveDilemma(g, g->turn % 2);

    /* redeploiement vers les theatres menaces */
    for (int t = 0; t < TH_COUNT; t++) {
        Domain d = THEATERS[t].domain;
        int want = 1 + 2 * Game_ThreatCount(g, t);
        while (g->posture[t] < want && Game_ChangePosture(g, t, 1)) {}
        if (g->posture[t] < want) {
            for (int o = 0; o < TH_COUNT; o++) {
                if (o == t || THEATERS[o].domain != d) continue;
                if (Game_ThreatCount(g, o) == 0 && g->posture[o] > 1) {
                    Game_ChangePosture(g, o, -1);
                    Game_ChangePosture(g, t, 1);
                }
            }
        }
    }

    /* projets prioritaires selon les ressources en danger */
    static const int order[] = { P_ADAPT, P_DESSAL, P_AGRO, P_ENR, P_CYBERCIV, P_MEDIAS, P_SMR, P_CONSTELL };
    for (int i = 0; i < P_COUNT; i++) {
        int p = order[i];
        if (g->proj[p].state == 0 && g->res[R_BUDGET] > PROJECTS[p].cost + 8) Game_StartProject(g, p);
    }

    /* capacites */
    for (int d = 0; d < D_COUNT; d++) {
        if (Game_PostureUsed(g, d) >= g->capacity[d] && g->res[R_BUDGET] > Game_InvestCost(g, d) + 15)
            Game_InvestCapacity(g, d);
    }

    /* diplomatie / renseignement sur les factions les plus dangereuses */
    for (int k = 0; k < BASE_CP; k++) {
        int best = -1; float bv = 0;
        for (int f = 0; f < F_COUNT; f++) {
            float v = g->fac[f].hostility * g->fac[f].capability;
            if (!g->fac[f].actedThisTurn && v > bv) { bv = v; best = f; }
        }
        if (best < 0) break;
        if (!Game_FactionAction(g, best, A_DIPLO))
            if (!Game_FactionAction(g, best, A_INFLUENCE))
                Game_FactionAction(g, best, A_RENSEIGNEMENT);
    }
}

static void Run(const char *label, void (*strat)(Game *), int games)
{
    int wins = 0, causes[5] = { 0 }, turnsSum = 0;
    float resSum[R_COUNT] = { 0 };
    for (int i = 0; i < games; i++) {
        Game g;
        Game_Init(&g, 1000u + (unsigned)i * 7919u);
        while (g.state == GAME_RUNNING) {
            strat(&g);
            Game_EndTurn(&g);
        }
        causes[g.state]++;
        if (g.state == GAME_WON) wins++;
        turnsSum += g.turn;
        for (int r = 0; r < R_COUNT; r++) resSum[r] += g.res[r];
    }
    printf("%-12s victoires %3d%%  tours moy %.1f  | cohesion %d souv %d budget %d\n",
           label, wins * 100 / games, turnsSum / (float)games,
           causes[GAME_LOST_COHESION], causes[GAME_LOST_SOUVERAINETE], causes[GAME_LOST_BUDGET]);
    printf("             fin moy :");
    for (int r = 0; r < R_COUNT; r++) printf(" %s=%.0f", RESOURCE_NAMES[r], resSum[r] / games);
    printf("\n");
}

int main(void)
{
    Run("passive", PlayPassive, 500);
    Run("heuristique", PlayHeuristic, 500);
    return 0;
}
