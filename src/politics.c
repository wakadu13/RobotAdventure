/*
 * BOUCLIER 2050 - vie politique : courants ideologiques, elections et
 * gouvernements, soutien au regime, decrets propres a chaque regime.
 */
#include "game_internal.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Initialisation d'une province                                       */
/* ------------------------------------------------------------------ */

static void Normalize(float v[I_COUNT])
{
    float s = 0;
    for (int i = 0; i < I_COUNT; i++) { if (v[i] < 0.01f) v[i] = 0.01f; s += v[i]; }
    for (int i = 0; i < I_COUNT; i++) v[i] /= s;
}

void Pol_InitProvince(Game *g, int p)
{
    Province *pr = &g->prov[p];
    float w = PROVINCES[p].wealth;
    pr->share[S_ELITES] = 0.05f;
    pr->share[S_MOYENNES] = Clampf(0.12f + w * 0.5f, 0.1f, 0.45f);
    pr->share[S_TRAVAILLEURS] = 1 - pr->share[S_ELITES] - pr->share[S_MOYENNES];
    pr->literacy = Clampf(0.45f + w * 1.2f, 0.4f, 0.98f);
    for (int s = 0; s < S_COUNT; s++) pr->con[s] = 1.5f + pr->literacy * 1.5f;
    static const float base[I_COUNT] = { 0.10f, 0.20f, 0.20f, 0.25f, 0.15f, 0.10f };
    Regime r = g->nat[pr->owner].regime;
    for (int i = 0; i < I_COUNT; i++) pr->ideo[i] = base[i] + 0.15f * IDEOLOGY_AFFINITY[r][i];
    Normalize(pr->ideo);
}

/* ------------------------------------------------------------------ */
/* Courants et soutien au regime                                       */
/* ------------------------------------------------------------------ */

void Pol_NationalIdeology(const Game *g, float out[I_COUNT])
{
    float pop = 0;
    for (int i = 0; i < I_COUNT; i++) out[i] = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE) continue;
        float w = pr->pop * (pr->core == N_FRANCE ? 1.0f : 0.4f);
        for (int i = 0; i < I_COUNT; i++) out[i] += pr->ideo[i] * w;
        pop += w;
    }
    for (int i = 0; i < I_COUNT; i++) out[i] = pop > 0 ? out[i] / pop : 0;
}

float Pol_RegimeSupport(const Game *g)
{
    float nat[I_COUNT], s = 0;
    Pol_NationalIdeology(g, nat);
    Regime r = g->nat[N_FRANCE].regime;
    for (int i = 0; i < I_COUNT; i++) s += nat[i] * IDEOLOGY_AFFINITY[r][i];
    return s;
}

static void DriftIdeology(Game *g, int p, int threats)
{
    Province *pr = &g->prov[p];
    bool fr = pr->owner == N_FRANCE;
    float needW = fr ? Eco_NeedSat(g, N_FRANCE, S_TRAVAILLEURS) : Clampf(pr->hab / 80.0f, 0, 1);
    float mil = Soc_ProvinceMilitancy(pr);
    float t[I_COUNT];
    t[I_ECOLO]   = 0.06f + (g->temp - 1.5f) * 0.10f + pr->literacy * 0.06f;
    t[I_SOCIAL]  = 0.10f + (1 - needW) * 1.2f;
    t[I_LIBERAL] = 0.08f + (pr->prosperity - 0.9f) * 0.4f + pr->literacy * 0.08f;
    t[I_CONSERV] = 0.12f + pr->share[S_ELITES] * 1.2f + 0.05f * (1 - pr->foreign);
    t[I_SOUVER]  = 0.06f + pr->foreign * 0.6f;
    t[I_AUTOR]   = 0.03f + (mil > 3.5f ? (mil - 3.5f) * 0.06f : 0) + (pr->revolt ? 0.1f : 0);
    if (fr) {
        t[I_SOCIAL] += Clampf(g->taxRate[S_TRAVAILLEURS] - g->taxRate[S_ELITES], -0.3f, 0.3f) * 0.4f;
        if (g->spending[SP_SOCIAL] < 0.3f) t[I_SOCIAL] += 0.05f;
        t[I_SOUVER] += (threats < 15 ? threats : 15) * 0.01f + g->statLost * 0.02f;
        if (g->res[R_COHESION] < 35) t[I_AUTOR] += 0.08f;
        if (g->propagandaTurns > 0) t[I_AUTOR] += 0.2f;
    }
    Normalize(t);
    for (int i = 0; i < I_COUNT; i++) pr->ideo[i] += (t[i] - pr->ideo[i]) * 0.04f;
    Normalize(pr->ideo);
}

/* ------------------------------------------------------------------ */
/* Elections et gouvernement                                           */
/* ------------------------------------------------------------------ */

void Pol_ScheduleElection(Game *g)
{
    Regime r = g->nat[N_FRANCE].regime;
    if (r == RG_DEMOCRATIE) {
        if (g->nextElection <= g->turn) g->nextElection = g->turn + 16;
    } else {
        g->nextElection = -1;
        g->government = r == RG_UTOPIE ? I_ECOLO : -1;
    }
}

static void Election(Game *g)
{
    float nat[I_COUNT];
    Pol_NationalIdeology(g, nat);
    int win = 0;
    for (int i = 1; i < I_COUNT; i++) if (nat[i] > nat[win]) win = i;
    g->nextElection = g->turn + 16;
    if (win == I_AUTOR) {
        if (g->pendingDilemma < 0) g->pendingDilemma = DLM_COUP_LEGAL;
        Game_Log(g, EV_EVENT, "Élections : les Autoritaires arrivent en tête avec %.0f%% des voix !", nat[win] * 100);
        return;
    }
    if (win != g->government)
        Game_Log(g, EV_EVENT, "Élections : les %s forment le gouvernement (%.0f%% des voix).", IDEOLOGY_NAMES[win], nat[win] * 100);
    else
        Game_Log(g, EV_EVENT, "Élections : les %s sont reconduits (%.0f%% des voix).", IDEOLOGY_NAMES[win], nat[win] * 100);
    g->government = win;
}

const char *Pol_GovernmentEffect(int ideology)
{
    return ideology >= 0 ? IDEOLOGY_DESC[ideology] : "Pas de gouvernement partisan.";
}

void Pol_Turn(Game *g)
{
    int threats = Game_ActiveThreats(g);
    for (int p = 0; p < PROV_COUNT; p++) DriftIdeology(g, p, threats);

    Regime r = g->nat[N_FRANCE].regime;
    if (r == RG_DEMOCRATIE && g->nextElection >= 0 && g->turn >= g->nextElection) Election(g);
    if (r != RG_DEMOCRATIE) Pol_ScheduleElection(g);

    if (g->government == I_SOUVER) {
        AddRes(g, R_SOUVERAINETE, 0.3f);
        AddRes(g, R_INFLUENCE, -0.3f);
    }
    if (g->propagandaTurns > 0) AddRes(g, R_COHESION, 0.8f);

    if (g->sobrietyTurns > 0) g->sobrietyTurns--;
    if (g->mobilizationTurns > 0) g->mobilizationTurns--;
    if (g->boostTurns > 0) g->boostTurns--;
    if (g->propagandaTurns > 0) g->propagandaTurns--;
    for (int k = 0; k < 2; k++) if (g->decreeCooldown[k] > 0) g->decreeCooldown[k]--;
}

/* ------------------------------------------------------------------ */
/* Decrets propres aux regimes                                         */
/* ------------------------------------------------------------------ */

const char *Pol_DecreeBlocked(const Game *g, int k)
{
    static char buf[64];
    const DecreeDef *d = &DECREES[g->nat[N_FRANCE].regime][k];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->decreeCooldown[k] > 0) {
        snprintf(buf, sizeof(buf), "Disponible dans %d tours", g->decreeCooldown[k]);
        return buf;
    }
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (g->res[R_BUDGET] < d->budget) return "Budget insuffisant";
    return NULL;
}

static void FranceMil(Game *g, float mil, float con)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++) {
            pr->mil[s] = Clampf(pr->mil[s] + mil, 0, 10);
            pr->con[s] = Clampf(pr->con[s] + con, 0, 10);
        }
    }
}

static void FranceProsperity(Game *g, float v)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) g->prov[p].prosperity = Clampf(g->prov[p].prosperity + v, 0.4f, 2.5f);
}

static void EndRevolts(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE) g->prov[p].revolt = 0;
}

bool Pol_Decree(Game *g, int k)
{
    if (Pol_DecreeBlocked(g, k)) return false;
    Regime r = g->nat[N_FRANCE].regime;
    const DecreeDef *d = &DECREES[r][k];
    g->commandPoints--;
    g->res[R_BUDGET] -= d->budget;
    g->decreeCooldown[k] = d->cooldown;

    switch (r * 2 + k) {
    case RG_UTOPIE * 2:     FranceMil(g, -1.5f, 0); break;
    case RG_UTOPIE * 2 + 1: g->sobrietyTurns = 8; FranceProsperity(g, -0.02f); break;
    case RG_DEMOCRATIE * 2:
        AddRes(g, R_COHESION, Pol_RegimeSupport(g) > 0.5f ? 6 : -6);
        break;
    case RG_DEMOCRATIE * 2 + 1: FranceProsperity(g, 0.05f); break;
    case RG_TECHNOCRATIE * 2: {
        bool done[PROV_COUNT] = { false };
        for (int n = 0; n < 3; n++) {
            int best = -1;
            for (int p = 0; p < PROV_COUNT; p++) {
                if (g->prov[p].owner != N_FRANCE || done[p]) continue;
                if (best < 0 || g->prov[p].pop > g->prov[best].pop) best = p;
            }
            if (best < 0) break;
            g->prov[best].industry += 1;
            done[best] = true;
        }
    } break;
    case RG_TECHNOCRATIE * 2 + 1: g->boostTurns = 8; FranceMil(g, 0, 0.5f); break;
    case RG_AUTORITAIRE * 2:
        for (int i = 0; i < MAX_THREATS; i++)
            if (g->threats[i].active && THREATS[g->threats[i].type].domain == D_INFO) g->threats[i].active = false;
        AddFac(g, F_NEBULA, 0, -10);
        FranceMil(g, 0, -1);
        AddRes(g, R_INFLUENCE, -5);
        break;
    case RG_AUTORITAIRE * 2 + 1: EndRevolts(g); FranceMil(g, -2, 0); AddRes(g, R_COHESION, -3); break;
    case RG_JUNTE * 2: {
        g->mobilizationTurns = 12;
        FranceProsperity(g, -0.1f);
        for (int i = 0; i < 3; i++) War_SpawnUnit(g, N_FRANCE, U_INF, PV_FR_NORD, 1);
    } break;
    case RG_JUNTE * 2 + 1: EndRevolts(g); FranceMil(g, -3, 1); break;
    case RG_TOTALITAIRE * 2:
        FranceMil(g, -2, -2);
        for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == N_FRANCE) g->prov[p].pop *= 0.998f;
        break;
    case RG_TOTALITAIRE * 2 + 1: g->propagandaTurns = 12; AddRes(g, R_INFLUENCE, -6); break;
    }
    Game_Log(g, EV_EVENT, "Décret : %s.", d->name);
    return true;
}
