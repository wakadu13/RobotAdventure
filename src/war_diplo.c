/*
 * BOUCLIER 2050 - guerres, alliances et diplomatie ; score de guerre, traites et paix ;
 * routes commerciales ; commandants du hot-seat.
 */
#include "war_internal.h"

/* ------------------------------------------------------------------ */
/* Guerres, alliances, diplomatie                                      */
/* ------------------------------------------------------------------ */

void War_StartWar(Game *g, int a, int b, bool callAllies)
{
    if (a == b || g->war[a][b] || !g->nat[a].alive || !g->nat[b].alive) return;
    g->war[a][b] = g->war[b][a] = true;
    g->warSince[a][b] = g->warSince[b][a] = (short)g->turn;
    g->truce[a][b] = g->truce[b][a] = 0;
    g->battles[a][b] = g->battles[b][a] = 0;
    g->ally[a][b] = g->ally[b][a] = false;
    AddAiRel(g, a, b, -60);
    if (a == N_FRANCE || b == N_FRANCE) {
        int o = a == N_FRANCE ? b : a;
        g->nat[o].trade = false;
        g->nat[o].warned = false;
        g->nat[o].deterred = Nuke_Deters(g, o);
    }
    if (!callAllies) return;
    Intl_OnWar(g, a, b);

    /* les allies de la victime sont appeles aux armes */
    for (int k = 0; k < N_COUNT; k++) {
        if (k == a || k == b || !g->ally[b][k] || !g->nat[k].alive || g->war[k][a] || g->truce[k][a] > 0) continue;
        if (k == N_FRANCE) {
            if (g->pendingDilemma < 0) { g->pendingDilemma = DLM_ALLY_CALL; g->dilemmaParam = a; g->dilemmaParam2 = b; }
            continue;
        }
        if (Randf(g) < 0.8f) {
            War_StartWar(g, k, a, false);
            if (a == N_FRANCE || IsAlly(g, N_FRANCE, b))
                Game_Log(g, EV_WAR, "%s honore son alliance avec %s et entre en guerre contre %s.", (NATIONS[k].name), (NATIONS[b].name), (NATIONS[a].name));
        }
    }
    /* solidarite europeenne face a un agresseur exterieur */
    if (g->nat[b].eu && !g->nat[a].eu) {
        if (g->nat[N_FRANCE].eu && a != N_FRANCE && !g->war[N_FRANCE][a] && g->pendingDilemma < 0 && !g->ally[N_FRANCE][b]) {
            g->pendingDilemma = DLM_ALLY_CALL;
            g->dilemmaParam = a;
            g->dilemmaParam2 = b;
        }
        for (int k = 1; k < N_COUNT; k++)
            if (k != b && g->nat[k].eu && g->nat[k].alive && !g->war[k][a] && War_Adjacent(g, k, a) && Randf(g) < 0.2f)
                War_StartWar(g, k, a, false);
    }
}

const char *War_DeclareBlocked(const Game *g, int n)
{
    if (n == N_FRANCE || !g->nat[n].alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "Déjà en guerre";
    if (g->truce[N_FRANCE][n] > 0) return "Trêve en cours";
    if (g->ally[N_FRANCE][n]) return "Nation alliée";
    if (!War_Adjacent(g, N_FRANCE, n)) return "Aucune frontière terrestre ou maritime commune";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool War_Declare(Game *g, int n)
{
    if (War_DeclareBlocked(g, n)) return false;
    Nation *fr = &g->nat[N_FRANCE];
    g->commandPoints--;
    g->aggressionMemory = Clampf(g->aggressionMemory + 25, 0, 100);
    AddRes(g, R_COHESION, -REGIMES[fr->regime].aggressionCost);
    AddRes(g, R_INFLUENCE, -8);
    Game_Log(g, EV_WAR, "La France déclare la guerre à %s !", (NATIONS[n].name));
    War_StartWar(g, N_FRANCE, n, true);
    g->nat[n].relation = -80;

    if (g->nat[n].eu && fr->eu) {
        fr->eu = false;
        g->umbrella = false;
        Game_Log(g, EV_BAD, "Agression contre un État membre : la France est exclue de l'Union européenne.");
        for (int k = 1; k < N_COUNT; k++) {
            Nation *o = &g->nat[k];
            if (!o->alive || !o->eu || k == n) continue;
            AddRelation(g, k, -30);
            if (!g->war[k][N_FRANCE] && o->army >= 2 && War_Adjacent(g, N_FRANCE, k) && Randf(g) < 0.3f) {
                War_StartWar(g, k, N_FRANCE, false);
                Game_Log(g, EV_WAR, "%s rejoint la coalition contre la France !", (NATIONS[k].name));
            }
        }
    }
    return true;
}

const char *War_DiploBlocked(const Game *g, int n, int action)
{
    const Nation *na = &g->nat[n];
    if (!na->alive || n == N_FRANCE) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "En guerre";
    if (na->actedThisTurn) return "Déjà une action diplomatique ce tour";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (action == 0 && g->res[R_INFLUENCE] < 5) return "Influence insuffisante (5)";
    if (action == 1) {
        if (na->trade) return "Accord déjà en vigueur";
        if (na->embargo) return "Levez d'abord l'embargo";
        if (na->relation < 25) return "Relation insuffisante (25)";
    }
    if (action == 2) {
        if (g->ally[N_FRANCE][n]) return "Déjà alliée";
        if (na->relation < 60) return "Relation insuffisante (60)";
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
        Game_Log(g, EV_GOOD, "Rapprochement diplomatique avec %s.", (NATIONS[n].name));
    } else if (action == 1) {
        na->trade = true;
        Game_Log(g, EV_GOOD, "Accord commercial signé avec %s : la France est servie en priorité sur leurs excédents.", (NATIONS[n].name));
    } else {
        g->ally[N_FRANCE][n] = g->ally[n][N_FRANCE] = true;
        Game_Log(g, EV_GOOD, "Alliance défensive conclue avec %s.", (NATIONS[n].name));
    }
    return true;
}

const char *War_UltimatumBlocked(const Game *g, int p)
{
    int n = g->prov[p].owner;
    if (n == N_FRANCE || !g->nat[n].alive) return "Impossible";
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->war[N_FRANCE][n]) return "Déjà en guerre : il faut l'occuper puis négocier";
    if (g->truce[N_FRANCE][n] > 0) return "Trêve en cours";
    if (g->ally[N_FRANCE][n]) return "Nation alliée";
    if (!TouchesNation(g, p, N_FRANCE)) return "Province non frontalière";
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
    for (int k = 1; k < N_COUNT; k++) if (k != n && g->ally[n][k] && g->nat[k].alive) theirs += g->nat[k].army * 0.5f;
    if (g->nukePosture == 2 && !Nuke_IsPower(g, n)) ours *= 1 + g->credibility / 200.0f;
    g->aggressionMemory = Clampf(g->aggressionMemory + 12, 0, 100);
    if (ours > theirs * 2.5f) {
        Game_Log(g, EV_WAR, "Ultimatum : %s cède %s sans combat.", (NATIONS[n].name), (PROVINCE_NAMES[p]));
        AddRelation(g, n, -40);
        AddRes(g, R_COHESION, -REGIMES[g->nat[N_FRANCE].regime].aggressionCost * 0.3f);
        War_Annex(g, p, N_FRANCE);
        g->statConquered++;
        return true;
    }
    AddRelation(g, n, -25);
    AddRes(g, R_INFLUENCE, -5);
    Game_Log(g, EV_BAD, "Ultimatum rejeté par %s. Notre crédibilité en souffre.", (NATIONS[n].name));
    return false;
}

/* ------------------------------------------------------------------ */
/* Score de guerre et traites                                          */
/* ------------------------------------------------------------------ */

float Value(const Game *g, int p) { return g->prov[p].pop + 2; }

float NationValue(const Game *g, int n)
{
    float v = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].owner == n) v += Value(g, p);
    return v > 0 ? v : 1;
}

/* score de guerre de a contre b (-100..100) */
float War_Score(const Game *g, int a, int b)
{
    float occA = 0, occB = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner == b && pr->ctrl == a) occA += Value(g, p);
        if (pr->owner == a && pr->ctrl == b) occB += Value(g, p);
    }
    float s = occA / NationValue(g, b) * 100 - occB / NationValue(g, a) * 100;
    s += Clampf((g->battles[a][b] - g->battles[b][a]) * 3.0f, -25, 25);
    s += (g->nat[b].blockade - g->nat[a].blockade) * 10;
    return Clampf(s, -100, 100);
}

float CedeCost(const Game *g, int p)
{
    int loser = g->prov[p].owner;
    float c = Value(g, p) / NationValue(g, loser) * 100;
    int winner = g->treaty.winner;
    return g->prov[p].ctrl == winner ? c : c * 2;   /* exiger une province non occupee coute double */
}

void War_TreatyDraft(Game *g, int other)
{
    Treaty *t = &g->treaty;
    memset(t, 0, sizeof(*t));
    t->active = true;
    t->other = other;
    float s = War_Score(g, N_FRANCE, other);
    t->winner = s >= 0 ? N_FRANCE : other;
    int loser = t->winner == N_FRANCE ? other : N_FRANCE;
    float budget = fabsf(s);
    /* par defaut : les provinces occupees, les plus precieuses d'abord, dans la limite du score */
    for (;;) {
        int best = -1;
        for (int p = 0; p < PROV_COUNT; p++)
            if (!t->cede[p] && g->prov[p].owner == loser && g->prov[p].ctrl == t->winner && CedeCost(g, p) <= budget
                && (best < 0 || Value(g, p) > Value(g, best)))
                best = p;
        if (best < 0) break;
        budget -= CedeCost(g, best);
        t->cede[best] = true;
    }
    t->reparationTurns = 16;
    if (budget >= 20) t->reparations = 4;
    else if (budget >= 10) t->reparations = 2;
}

float War_TreatyCost(const Game *g, const Treaty *t)
{
    float c = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (t->cede[p]) c += CedeCost(g, p);
    return c + t->reparations * 5;
}

const char *War_TreatyBlocked(const Game *g, const Treaty *t)
{
    static char buf[120];
    if (!t->active || !g->war[N_FRANCE][t->other]) return "Pas en guerre";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    float s = War_Score(g, N_FRANCE, t->other);
    float cost = War_TreatyCost(g, t);
    if (t->winner == N_FRANCE && cost > s + 5) {
        snprintf(buf, sizeof(buf), Tr("Exigences trop lourdes : %.0f points pour un score de guerre de %.0f"), cost, s);
        return buf;
    }
    if (t->winner != N_FRANCE && cost < -s - 10 && War_Turns(g, N_FRANCE, t->other) < 16) {
        snprintf(buf, sizeof(buf), Tr("L'ennemi exige au moins %.0f points de concessions"), -s - 10);
        return buf;
    }
    return NULL;
}

void War_WhitePeace(Game *g, int a, int b, bool log)
{
    g->war[a][b] = g->war[b][a] = false;
    g->truce[a][b] = g->truce[b][a] = 16;
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if ((pr->owner == a && pr->ctrl == b) || (pr->owner == b && pr->ctrl == a)) {
            pr->ctrl = pr->owner;
            RelocateForeign(g, p);
        }
    }
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active) continue;
        if ((u->owner == a || u->owner == b) && !War_Friendly(g, u->owner, u->prov)) Relocate(g, u);
        if ((u->owner == a || u->owner == b) && u->target >= 0) u->target = -1;
    }
    AddAiRel(g, a, b, 30);
    int o = a == N_FRANCE ? b : b == N_FRANCE ? a : -1;
    if (o >= 0) { g->nat[o].peaceRefusals = 0; g->nat[o].peaceCooldown = 0; }
    g->aggressor[a][b] = g->aggressor[b][a] = false;
    if (log) Game_Log(g, EV_WAR, "Paix signée entre %s et %s. Trêve de 4 ans.", (NATIONS[a].name), (NATIONS[b].name));
}

void War_SignTreaty(Game *g, const Treaty *t)
{
    int other = t->other;
    int loser = t->winner == N_FRANCE ? other : N_FRANCE;
    int ceded = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (!t->cede[p] || g->prov[p].owner != loser) continue;
        War_Annex(g, p, t->winner);
        ceded++;
    }
    if (t->reparations > 0) {
        Nation *o = &g->nat[other];
        o->reparations = t->winner == N_FRANCE ? t->reparations : -t->reparations;
        o->reparationTurns = t->reparationTurns;
    }
    if (g->nat[other].alive) War_WhitePeace(g, N_FRANCE, other, false);
    Game_Log(g, EV_WAR, "Traité de paix avec %s : %d province(s) cédée(s) à %s%s.", (NATIONS[other].name), ceded,
             (NATIONS[t->winner].name), t->reparations > 0 ? ", réparations" : "");
    g->treaty.active = false;
}

bool War_ProposeTreaty(Game *g)
{
    Treaty *t = &g->treaty;
    if (War_TreatyBlocked(g, t)) return false;
    g->commandPoints--;
    War_SignTreaty(g, t);
    return true;
}

/* lassitude de n face a l'ennemi e : ce qu'elle est prete a ceder (ou a renoncer a exiger) en plus du score */
float ExhaustionVs(const Game *g, int n, int e)
{
    const Nation *na = &g->nat[n];
    float x = Clampf((War_Turns(g, e, n) - 6) * 1.5f, 0, 15);
    x += na->blockade * 15 + na->raid * 10;
    if (na->army < War_ArmyMax(g, n) * 0.4f) x += 10;
    float mil = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n) { mil += Soc_ProvinceMilitancy(&g->prov[p]) * g->prov[p].pop; pop += g->prov[p].pop; }
    if (pop > 0 && mil / pop > 5) x += 8;
    if (na->failed) x += 10;
    if (na->outage > 0) x += 4;
    if (na->sanctions > 0) x += 6;
    if (e == N_FRANCE && Nuke_IsPower(g, N_FRANCE) && !Nuke_IsPower(g, n)) x += g->credibility / 20;
    if (e != N_FRANCE && Nuke_IsPower(g, e) && !Nuke_IsPower(g, n)) x += 5;
    for (int k = 0; k < N_COUNT; k++) if (k != e && k != n && g->war[n][k]) x += 4;   /* plusieurs fronts */
    if (IsDemocratic(na->regime)) x += 4;     /* l'opinion pese */
    if (na->regime == RG_TOTALITAIRE || na->regime == RG_JUNTE) x -= 5;
    return Clampf(x, 0, 40);
}

float War_Exhaustion(const Game *g, int n) { return ExhaustionVs(g, n, N_FRANCE); }

/* ajuste un traite avec la France a une limite de cout : retire ou ajoute des exigences */
void FitTreaty(Game *g, Treaty *t, float limit)
{
    int n = t->other;
    if (t->winner == N_FRANCE) {
        while (War_TreatyCost(g, t) > limit) {
            if (t->reparations > 0) { t->reparations -= 2; if (t->reparations < 0.1f) t->reparations = 0; continue; }
            int drop = -1;
            float worst = -1;
            for (int p = 0; p < PROV_COUNT; p++) {
                if (!t->cede[p]) continue;
                float c = CedeCost(g, p) * (g->prov[p].ctrl == N_FRANCE ? 1 : 3);
                if (c > worst) { worst = c; drop = p; }
            }
            if (drop < 0) break;
            t->cede[drop] = false;
        }
        /* une offre genereuse ajoute des provinces occupees puis des reparations */
        for (int p = 0; p < PROV_COUNT; p++) {
            if (t->cede[p] || g->prov[p].owner != n || g->prov[p].ctrl != N_FRANCE) continue;
            t->cede[p] = true;
            if (War_TreatyCost(g, t) > limit) t->cede[p] = false;
        }
        while (t->reparations < 6 && War_TreatyCost(g, t) + 10 <= limit) { t->reparations += 2; t->reparationTurns = 16; }
    } else {
        /* le vainqueur ennemi : exigences ramenees a la limite */
        while (War_TreatyCost(g, t) > limit + 5) {
            if (t->reparations > 0) { t->reparations -= 2; if (t->reparations < 0.1f) t->reparations = 0; continue; }
            int drop = -1;
            float worst = -1;
            for (int p = 0; p < PROV_COUNT; p++) if (t->cede[p] && CedeCost(g, p) > worst) { worst = CedeCost(g, p); drop = p; }
            if (drop < 0) break;
            t->cede[drop] = false;
        }
        for (int p = 0; p < PROV_COUNT && War_TreatyCost(g, t) < limit; p++)
            if (g->prov[p].owner == N_FRANCE && g->prov[p].ctrl == n && !t->cede[p]) t->cede[p] = true;
        while (War_TreatyCost(g, t) < limit && t->reparations < 6) { t->reparations += 2; t->reparationTurns = 16; }
    }
}

/* negociation : l'IA accepte, refuse ou fait une contre-proposition */
int War_Negotiate(Game *g)
{
    Treaty *t = &g->treaty;
    int n = t->other;
    if (!t->active || !g->war[N_FRANCE][n]) return 0;
    if (g->commandPoints < 1) return 0;
    Nation *na = &g->nat[n];
    if (na->negoTurn != g->turn) { na->negoTurn = g->turn; na->negoRounds = 0; }
    if (na->negoRounds >= 3) {
        Game_Log(g, EV_BAD, "%s quitte la table des négociations pour ce trimestre.", (NATIONS[n].name));
        return 0;
    }
    if (na->negoRounds == 0) g->commandPoints--;
    na->negoRounds++;
    float s = War_Score(g, N_FRANCE, n), ex = War_Exhaustion(g, n), cost = War_TreatyCost(g, t);
    bool frWins = t->winner == N_FRANCE;
    /* ce que l'ennemi accepte : nos exigences dans la limite du score et de sa lassitude ;
       s'il gagne, il se contente de moins quand il est las */
    float limit = frWins ? s + 5 + ex : -s - 10 - ex;
    bool ok = frWins ? cost <= limit : cost >= limit;
    if (ok) {
        War_SignTreaty(g, t);
        return 1;
    }
    /* contre-proposition */
    if (frWins) FitTreaty(g, t, limit);
    else {
        FitTreaty(g, t, limit);
        /* l'ennemi exige alors davantage : nos provinces frontalieres */
        for (int p = 0; p < PROV_COUNT && War_TreatyCost(g, t) < limit; p++)
            if (g->prov[p].owner == N_FRANCE && !t->cede[p] && TouchesNation(g, p, n)) t->cede[p] = true;
    }
    t->countered = true;
    Game_Log(g, EV_WAR, "%s fait une contre-proposition (lassitude %.0f).", (NATIONS[n].name), ex);
    return 2;
}

/* paix entre deux IA : le vainqueur annexe ce qu'il occupe, dans la limite du score */
void AiPeace(Game *g, int a, int b)
{
    float s = War_Score(g, a, b);
    int winner = s >= 0 ? a : b, loser = s >= 0 ? b : a;
    float budget = fabsf(s) + ExhaustionVs(g, loser, winner) * 0.5f;
    int ceded = 0;
    for (int p = 0; p < PROV_COUNT && budget > 0; p++) {
        if (g->prov[p].owner != loser || g->prov[p].ctrl != winner) continue;
        float c = Value(g, p) / NationValue(g, loser) * 100;
        if (c > budget) continue;
        budget -= c;
        War_Annex(g, p, winner);
        ceded++;
    }
    /* reparations entre IA : un transfert de tresor */
    if (budget >= 10 && g->nat[loser].alive) {
        float amount = fminf(g->nat[loser].treasury + 10, budget * 0.3f);
        g->nat[loser].treasury -= amount;
        g->nat[winner].treasury += amount;
    }
    if (g->nat[a].alive && g->nat[b].alive) War_WhitePeace(g, a, b, false);
    bool show = IsAlly(g, N_FRANCE, a) || IsAlly(g, N_FRANCE, b) || ceded >= 2;
    if (ceded) Game_Log(g, show ? EV_WAR : EV_INFO, "Paix entre %s et %s : %s annexe %d province(s).", (NATIONS[a].name), (NATIONS[b].name),
                        (NATIONS[winner].name), ceded);
    else Game_Log(g, EV_INFO, "Paix blanche entre %s et %s.", (NATIONS[a].name), (NATIONS[b].name));
}

/* l'IA propose (ou impose) un traite a la France */
void AiPeaceOfferToFrance(Game *g, int n)
{
    Nation *na = &g->nat[n];
    War_TreatyDraft(g, n);
    Treaty *t = &g->treaty;
    float s = War_Score(g, N_FRANCE, n), ex = ExhaustionVs(g, n, N_FRANCE);
    /* chaque refus rend l'offre suivante plus genereuse, a mesure que la lassitude grandit */
    float give = ex * (0.3f + 0.25f * na->peaceRefusals);
    if (t->winner == N_FRANCE) FitTreaty(g, t, fabsf(s) + give);
    else FitTreaty(g, t, fmaxf(0, -s - give));
    t->proposedByAi = true;
    g->pendingDilemma = DLM_PEACE_OFFER;
    g->dilemmaParam = n;
}

/* l'IA prend l'initiative envers la France : alliance, accord commercial, chantage nucleaire */
void AiProposals(Game *g, int n)
{
    Nation *na = &g->nat[n];
    if (g->pendingDilemma >= 0 || na->offerCooldown > 0 || g->war[N_FRANCE][n] || g->turn < 4) return;
    int kind = -1;
    if (!g->ally[N_FRANCE][n] && na->relation > 65 && IsDemocratic(na->regime) == IsDemocratic(g->nat[N_FRANCE].regime)
        && Randf(g) < 0.04f)
        kind = DLM_AI_ALLIANCE;
    else if (!na->trade && !na->embargo && !na->embargoFrance && na->relation > 30 && Eco_TradeAllowed(g, n) && Randf(g) < 0.04f)
        kind = DLM_AI_TRADE;
    else if (Nuke_IsPower(g, n) && na->relation < -40 && War_Adjacent(g, n, N_FRANCE)
             && g->turn > 12 && Randf(g) < 0.012f * (1.5f - g->credibility / 100.0f))
        kind = DLM_PEACE_BLACKMAIL;
    if (kind < 0) return;
    g->pendingDilemma = kind;
    g->dilemmaParam = n;
    na->offerCooldown = 12;
}

/* une nation qui ne tient plus aucune de ses provinces capitule */
void Capitulations(Game *g)
{
    for (int n = 1; n < N_COUNT; n++) {
        if (!g->nat[n].alive || !War_AtWarAny(g, n)) continue;
        bool holds = false;
        for (int p = 0; p < PROV_COUNT && !holds; p++) if (g->prov[p].owner == n && g->prov[p].ctrl == n) holds = true;
        if (holds) continue;
        Game_Log(g, EV_WAR, "%s capitule.", (NATIONS[n].name));
        for (int p = 0; p < PROV_COUNT; p++) {
            int c = g->prov[p].ctrl;
            if (g->prov[p].owner == n && c != n && g->nat[n].alive) War_Annex(g, p, c);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Routes commerciales                                                 */
/* ------------------------------------------------------------------ */
/*
 * Une nation commerce avec la France par voie de terre si une chaine de pays
 * voisins non hostiles les relie, sinon par la mer si toutes deux ont un port :
 * la route maritime souffre alors du blocus et de la guerre au commerce.
 */
void War_UpdateTradeRoutes(Game *g)
{
    bool adj[N_COUNT][N_COUNT] = { { false } };
    for (int p = 0; p < PROV_COUNT; p++)
        for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
            int a = g->prov[p].ctrl, b = g->prov[PROVINCE_ADJ[p][k]].ctrl;
            if (a != b && !Map_SeaLink(p, PROVINCE_ADJ[p][k])) adj[a][b] = adj[b][a] = true;
        }
    /* parcours terrestre depuis la France a travers les pays qui ne la combattent pas */
    bool land[N_COUNT] = { false };
    int queue[N_COUNT], head = 0, tail = 0;
    land[N_FRANCE] = true;
    queue[tail++] = N_FRANCE;
    while (head < tail) {
        int a = queue[head++];
        for (int b = 0; b < N_COUNT; b++) {
            if (land[b] || !adj[a][b] || !g->nat[b].alive || War_AtWar(g, N_FRANCE, b) || g->nat[b].embargoFrance) continue;
            land[b] = true;
            queue[tail++] = b;
        }
    }
    bool frPort = CoastalBase(g, N_FRANCE) >= 0;
    float frSea = (1 - 0.7f * g->nat[N_FRANCE].blockade) * (1 - g->nat[N_FRANCE].raid);
    for (int n = 0; n < N_COUNT; n++) {
        g->route[n] = 0;
        g->routeKind[n] = 0;
        if (!g->nat[n].alive) continue;
        if (land[n]) { g->route[n] = 1; g->routeKind[n] = 1; continue; }
        if (frPort && CoastalBase(g, n) >= 0) {
            g->route[n] = Clampf(frSea * (1 - 0.5f * g->nat[n].blockade), 0, 1);
            g->routeKind[n] = 2;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Commandants : nations dirigees par d'autres joueurs (hot-seat)      */
/* ------------------------------------------------------------------ */
/*
 * Un commandant dirige l'armee et la diplomatie militaire de sa nation :
 * recrutement sur son tresor, ordres, declarations de guerre, paix,
 * alliances. Son economie, sa politique et ses services restent geres
 * automatiquement, comme pour les autres nations.
 */

int War_NextPlayer(const Game *g, int after)
{
    for (int n = after + 1; n < N_COUNT; n++) if (n != N_FRANCE && g->human[n] && g->nat[n].alive) return n;
    return -1;
}

float Cmdr_RecruitCost(int type) { return UNIT_TYPES[type].cost * 0.5f; }

const char *Cmdr_RecruitBlocked(const Game *g, int n, int type, int p)
{
    const UnitDef *d = &UNIT_TYPES[type];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (!Mil_UnitUnlocked(g, n, type)) return "Technologie insuffisante";
    if (p < 0 || g->prov[p].owner != n || g->prov[p].ctrl != n) return "Choisissez une province de votre nation";
    if (g->prov[p].revolt) return "Province en révolte";
    if (d->domain == DOM_SEA && !PROVINCE_COASTAL[p]) return "Il faut une province côtière";
    if (d->domain == DOM_SEA && War_PortFlooded(g, p)) return "Port submergé par la montée des eaux";
    if (War_StackCount(g, n, p, d->domain) >= StackLimit(d->domain)) return "Province saturée (limite d'empilement)";
    if (War_UnitLoad(g, n) + d->capWeight > War_ArmyMax(g, n) + 0.01f) return "Plafond de mobilisation atteint";
    if (g->nat[n].manpower < d->capWeight) return "Réserve de main-d'oeuvre épuisée";
    if (g->nat[n].treasury < Cmdr_RecruitCost(type)) return "Trésor insuffisant";
    return Strat_RecruitBlocked(g, n, type);
}

bool Cmdr_Recruit(Game *g, int n, int type, int p)
{
    if (Cmdr_RecruitBlocked(g, n, type, p)) return false;
    if (War_SpawnUnit(g, n, type, p, UNIT_TYPES[type].train) < 0) return false;
    g->nat[n].treasury -= Cmdr_RecruitCost(type);
    g->nat[n].manpower -= UNIT_TYPES[type].capWeight;
    Strat_PayRecruit(g, n, type);
    return true;
}

const char *Cmdr_OrderBlocked(const Game *g, int n, int i, int t) { return OrderBlocked(g, n, i, t); }

bool Cmdr_Order(Game *g, int n, int i, int t)
{
    if (OrderBlocked(g, n, i, t)) return false;
    g->units[i].target = (short)(t == g->units[i].prov ? -1 : t);
    return true;
}

bool Cmdr_Disband(Game *g, int n, int i)
{
    if (!g->units[i].active || g->units[i].owner != n) return false;
    g->units[i].active = false;
    UpdateArmyPower(g);
    return true;
}

const char *Cmdr_DeclareBlocked(const Game *g, int n, int t)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (t == n || !g->nat[t].alive) return "Impossible";
    if (g->war[n][t]) return "Déjà en guerre";
    if (g->truce[n][t] > 0) return "Trêve en cours";
    if (g->ally[n][t]) return "Nation alliée";
    if (!War_Adjacent(g, n, t)) return "Aucune frontière terrestre ou maritime commune";
    if (g->nat[n].actedThisTurn) return "Déjà une décision diplomatique ce tour";
    return NULL;
}

bool Cmdr_Declare(Game *g, int n, int t)
{
    if (Cmdr_DeclareBlocked(g, n, t)) return false;
    g->nat[n].actedThisTurn = true;
    Game_Log(g, EV_WAR, "%s déclare la guerre à %s !", (NATIONS[n].name), t == N_FRANCE ? "la France" : (NATIONS[t].name));
    War_StartWar(g, n, t, true);
    if (t == N_FRANCE) g->nat[n].relation = -80;
    return true;
}

const char *Cmdr_PeaceBlocked(const Game *g, int n, int t)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (t == n || !g->war[n][t]) return "Pas en guerre";
    if (War_Turns(g, n, t) < 2) return "La guerre vient de commencer";
    if (g->nat[n].actedThisTurn) return "Déjà une décision diplomatique ce tour";
    if (g->nat[n].peaceAsk == t + 1) return "Demande déjà transmise";
    return NULL;
}

/* la France et les autres commandants recoivent la demande ; une IA l'accepte si la guerre tourne mal pour elle */
bool Cmdr_Peace(Game *g, int n, int t)
{
    if (Cmdr_PeaceBlocked(g, n, t)) return false;
    Nation *na = &g->nat[n];
    na->actedThisTurn = true;
    if (t == N_FRANCE) {
        na->peaceAsk = (short)(t + 1);   /* offre presentee a la France a la resolution du tour */
        return true;
    }
    if (g->human[t]) {
        if (g->nat[t].peaceAsk == n + 1) { AiPeace(g, n, t); g->nat[t].peaceAsk = 0; }
        else na->peaceAsk = (short)(t + 1);
        return true;
    }
    float s = War_Score(g, n, t);
    if (s >= 10 || ExhaustionVs(g, t, n) > 15 || (War_Turns(g, n, t) >= 8 && s > -15)) AiPeace(g, n, t);
    else Game_Log(g, EV_INFO, "%s refuse la paix proposée par %s.", (NATIONS[t].name), (NATIONS[n].name));
    return true;
}

const char *Cmdr_AllyBlocked(const Game *g, int n, int t)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (t == n || t == N_FRANCE || !g->nat[t].alive || g->human[t]) return "Alliance avec une nation dirigée par l'IA";
    if (g->ally[n][t]) return "Déjà alliée";
    if (g->war[n][t]) return "En guerre";
    if (g->nat[n].actedThisTurn) return "Déjà une décision diplomatique ce tour";
    if (g->airel[n][t] < 50) return "Relations insuffisantes (50)";
    return NULL;
}

bool Cmdr_SetTax(Game *g, int n, int level)
{
    if (level < -2 || level > 2 || g->state != GAME_RUNNING) return false;
    g->nat[n].taxLevel = (signed char)level;
    return true;
}

float Cmdr_InvestCost(int kind) { return kind == 2 ? 8 : 5; }

const char *Cmdr_InvestBlocked(const Game *g, int n, int kind, int p)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (kind < 0 || kind > 2) return "Choix invalide";
    if (g->nat[n].treasury < Cmdr_InvestCost(kind)) return "Trésor insuffisant";
    if (kind == 2) {
        if (p < 0 || p >= PROV_COUNT || g->prov[p].owner != n || g->prov[p].ctrl != n) return "Choisissez une province de votre nation";
        if (g->prov[p].factoryBuild > 0) return "Chantier déjà en cours";
        if (g->prov[p].revolt) return "Province en révolte";
    }
    return NULL;
}

bool Cmdr_Invest(Game *g, int n, int kind, int p)
{
    if (Cmdr_InvestBlocked(g, n, kind, p)) return false;
    Nation *na = &g->nat[n];
    na->treasury -= Cmdr_InvestCost(kind);
    if (kind == 0) na->waterBonus += 0.5f;
    else if (kind == 1) na->foodBonus += 0.6f;
    else g->prov[p].factoryBuild = 4;
    return true;
}

bool Cmdr_Ally(Game *g, int n, int t)
{
    if (Cmdr_AllyBlocked(g, n, t)) return false;
    g->nat[n].actedThisTurn = true;
    g->ally[n][t] = g->ally[t][n] = true;
    Game_Log(g, EV_INFO, "Alliance entre %s et %s.", (NATIONS[n].name), (NATIONS[t].name));
    return true;
}
