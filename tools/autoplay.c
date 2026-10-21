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

/* gestion courante : defense, projets, diplomatie, commerce */
static void Manage(Game *g)
{
    if (g->pendingDilemma >= 0) Game_ResolveDilemma(g, g->pendingDilemma == DLM_PEACE_OFFER ? 0 : (g->turn % 2));

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

    static const int order[] = { P_ENR, P_AGRO, P_DESSAL, P_ADAPT, P_SMR, P_CYBERCIV, P_MEDIAS, P_CONSTELL };
    for (int i = 0; i < P_COUNT; i++) {
        int p = order[i];
        if (g->proj[p].state == 0 && g->res[R_BUDGET] > Game_ProjectCost(g, p) + 15) Game_StartProject(g, p);
    }
    for (int d = 0; d < D_COUNT; d++)
        if (Game_PostureUsed(g, d) >= g->capacity[d] && g->res[R_BUDGET] > Game_InvestCost(g, d) + 30
            && Soc_Income(g) - Game_Upkeep(g) > 2)
            Game_InvestCapacity(g, d);

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
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar) War_ProposePeace(g, n);
}

/* strategie conquerante : regime militaire, armee, invasions des voisins faibles */
static void PlayConqueror(Game *g)
{
    if (g->pendingDilemma >= 0) Game_ResolveDilemma(g, g->pendingDilemma == 13 || g->pendingDilemma == 9 ? 1 : 0);
    if (g->turn == 2) Soc_ChangeRegime(g, RG_AUTORITAIRE);
    if (g->turn == 16) Soc_ChangeRegime(g, RG_JUNTE);

    static const int order[] = { P_ENR, P_AGRO, P_DESSAL, P_ADAPT };
    for (int i = 0; i < 4; i++)
        if (g->proj[order[i]].state == 0 && g->res[R_BUDGET] > Game_ProjectCost(g, order[i]) + 15)
            Game_StartProject(g, order[i]);
    while (g->res[R_BUDGET] > 20 && Soc_Income(g) - Game_Upkeep(g) > 3 && War_Recruit(g)) {}

    bool atWar = false;
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar) atWar = true;
    if (!atWar && g->turn > 10 && g->nat[N_FRANCE].army >= 10) {
        int best = -1; float bestArmy = 1e9f;
        for (int n = 1; n < N_COUNT; n++)
            if (!War_DeclareBlocked(g, n) && g->nat[n].army < bestArmy) { bestArmy = g->nat[n].army; best = n; }
        if (best >= 0 && bestArmy < g->nat[N_FRANCE].army * 0.6f) War_Declare(g, best);
    }
    for (int p = 0; p < PROV_COUNT; p++)
        if (War_ProvinceAttackable(g, p) && War_AttackOdds(g, p, War_AvailableDivisions(g)) > 0.6f)
            War_Attack(g, p, War_AvailableDivisions(g));
    for (int n = 1; n < N_COUNT; n++)
        if (g->nat[n].atWar && g->nat[n].warTurns > 12) War_ProposePeace(g, n);
}

/* conquete moderee : gestion courante + quelques guerres pour des provinces riches */
static void PlayStrategist(Game *g)
{
    Manage(g);
    if (g->turn == 4) Soc_ChangeRegime(g, RG_AUTORITAIRE);
    while (g->res[R_BUDGET] > 40 && Soc_Income(g) - Game_Upkeep(g) > 4 && War_Recruit(g)) {}
    bool atWar = false;
    for (int n = 1; n < N_COUNT; n++) if (g->nat[n].atWar) atWar = true;
    if (!atWar && Soc_NationProvinces(g, N_FRANCE) < 12 && g->turn > 12 && g->res[R_COHESION] > 50) {
        int best = -1; float bestArmy = 1e9f;
        for (int n = 1; n < N_COUNT; n++)
            if (!War_DeclareBlocked(g, n) && g->nat[n].army < bestArmy) { bestArmy = g->nat[n].army; best = n; }
        if (best >= 0 && bestArmy < g->nat[N_FRANCE].army * 0.5f) War_Declare(g, best);
    }
    for (int p = 0; p < PROV_COUNT; p++)
        if (War_ProvinceAttackable(g, p) && War_AttackOdds(g, p, War_AvailableDivisions(g)) > 0.7f)
            War_Attack(g, p, War_AvailableDivisions(g));
}

static void Run(const char *label, void (*strat)(Game *), int games)
{
    int wins = 0, causes[6] = { 0 }, turnsSum = 0, provSum = 0;
    float resSum[R_COUNT] = { 0 }, popSum = 0, tempSum = 0;
    for (int i = 0; i < games; i++) {
        Game g;
        Game_Init(&g, 1000u + (unsigned)i * 7919u);
        while (g.state == GAME_RUNNING) {
            strat(&g);
            if (g.pendingDilemma >= 0) Game_ResolveDilemma(&g, 0);
            Game_EndTurn(&g);
        }
        causes[g.state]++;
        if (g.state == GAME_WON) wins++;
        turnsSum += g.turn;
        provSum += Soc_NationProvinces(&g, N_FRANCE);
        popSum += Soc_NationPop(&g, N_FRANCE);
        tempSum += g.temp;
        for (int r = 0; r < R_COUNT; r++) resSum[r] += g.res[r];
    }
    printf("%-12s victoires %3d%%  tours %.0f  provinces %.1f  pop %.0f M  +%.2f°C | coh %d souv %d budget %d conquete %d\n",
           label, wins * 100 / games, turnsSum / (float)games, provSum / (float)games, popSum / games, tempSum / games,
           causes[GAME_LOST_COHESION], causes[GAME_LOST_SOUVERAINETE], causes[GAME_LOST_BUDGET], causes[GAME_LOST_CONQUEST]);
    printf("             fin moy :");
    for (int r = 0; r < R_COUNT; r++) printf(" %s=%.0f", RESOURCE_NAMES[r], resSum[r] / games);
    printf("\n");
}

int main(void)
{
    Run("passive", PlayPassive, 200);
    Run("gestionnaire", Manage, 200);
    Run("conquerant", PlayConqueror, 200);
    Run("stratege", PlayStrategist, 200);
    return 0;
}
