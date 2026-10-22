/*
 * BOUCLIER 2050 - dissuasion nucleaire : posture et credibilite, parapluie
 * europeen, ultime avertissement, frappe strategique et riposte, armes
 * tactiques et escalade, proliferation et contre-proliferation, chantage.
 *
 * L'arme strategique reste abstraite : son interet est de ne jamais servir.
 * Son emploi fait de la France un paria et, contre une puissance nucleaire,
 * provoque un echange qui met fin a la partie.
 */
#include "game_internal.h"
#include "lang.h"

#include <stdio.h>
#include <string.h>

bool Nuke_IsPower(const Game *g, int n)
{
    return g->nat[n].nukeStatus >= 2;
}

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
    Game_Log(g, EV_INFO, "Posture de dissuasion : %s.", (NUKE_POSTURES[posture]));
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
    float p = g->credibility / 100.0f * (Nuke_IsPower(g, n) ? 0.6f : 1.0f);
    return Randf(g) < p;
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

/* la France devient un paria */
static void Pariah(Game *g, float rel, float influence, float cohesion)
{
    for (int k = 1; k < N_COUNT; k++) if (g->nat[k].alive) AddRelation(g, k, -rel);
    g->aggressionMemory = Clampf(g->aggressionMemory + rel, 0, 100);
    AddRes(g, R_INFLUENCE, -influence);
    Regime r = g->nat[N_FRANCE].regime;
    AddRes(g, R_COHESION, r == RG_TOTALITAIRE ? -cohesion * 0.2f : r == RG_JUNTE ? -cohesion * 0.4f : -cohesion);
}

/* la riposte strategique d'une puissance nucleaire : fin de partie */
static void Exchange(Game *g, int n)
{
    Devastate(g, PV_FR_IDF, 0.5f);
    Devastate(g, PV_FR_ARA, 0.4f);
    Devastate(g, PV_FR_PAC, 0.4f);
    Game_Log(g, EV_WAR, "%s riposte. Paris, Lyon et Marseille sont frappées.", (NATIONS[n].name));
    g->state = GAME_LOST_NUCLEAR;
}

/* capitulation sans conditions : la France annexe ce qu'elle occupe et touche des reparations */
static void Surrender(Game *g, int n)
{
    Treaty t;
    memset(&t, 0, sizeof(t));
    t.active = true;
    t.other = n;
    t.winner = N_FRANCE;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n && g->prov[p].ctrl == N_FRANCE) t.cede[p] = true;
    t.reparations = 4;
    t.reparationTurns = 16;
    War_SignTreaty(g, &t);
}

/* ------------------------------------------------------------------ */
/* Ultime avertissement                                                */
/* ------------------------------------------------------------------ */

static bool HoldsFrenchSoil(const Game *g, int n)
{
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == N_FRANCE && g->prov[p].core == N_FRANCE && g->prov[p].ctrl == n) return true;
    return false;
}

const char *Nuke_WarningBlocked(const Game *g, int n)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (!War_AtWar(g, N_FRANCE, n)) return "Pas en guerre";
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
    float chance = g->credibility / 100.0f * (Nuke_IsPower(g, n) ? 0.5f : 1.0f);
    if (Randf(g) < chance) {
        Game_Log(g, EV_WAR, "Ultime avertissement : %s se retire du territoire national et signe la paix.", (NATIONS[n].name));
        War_WhitePeace(g, N_FRANCE, n, false);
        g->credibility = Clampf(g->credibility + 10, 0, 100);
        return true;
    }
    g->credibility = Clampf(g->credibility - 20, 0, 100);
    Game_Log(g, EV_BAD, "Ultime avertissement ignoré par %s : notre dissuasion perd en crédibilité.", (NATIONS[n].name));
    return false;
}

/* ------------------------------------------------------------------ */
/* Frappe strategique                                                  */
/* ------------------------------------------------------------------ */

/* nation ennemie visee par une frappe sur p (-1 si aucune) */
static int StrikeEnemy(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (pr->ctrl != N_FRANCE && War_AtWar(g, N_FRANCE, pr->ctrl)) return pr->ctrl;
    if (pr->owner != N_FRANCE && War_AtWar(g, N_FRANCE, pr->owner)) return pr->owner;
    return -1;
}

const char *Nuke_StrikeBlocked(const Game *g, int p)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->prov[p].owner == N_FRANCE) return "Territoire national";
    if (StrikeEnemy(g, p) < 0) return "Pas en guerre avec cette nation";
    if (g->nukePosture < 1) return "Posture minimale : pas de capacité de frappe opérationnelle";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool Nuke_Strike(Game *g, int p)
{
    if (Nuke_StrikeBlocked(g, p)) return false;
    int n = StrikeEnemy(g, p);
    g->commandPoints--;
    g->nukesUsed++;
    Devastate(g, p, 0.4f);
    Game_Log(g, EV_WAR, "Frappe nucléaire sur %s. Le monde retient son souffle.", (PROVINCE_NAMES[p]));

    Pariah(g, 60, 50, 25);
    g->aggressionMemory = 100;
    g->nat[N_FRANCE].eu = false;
    g->umbrella = false;
    g->temp += 0.02f;

    if (Nuke_IsPower(g, n) && g->nat[n].alive) {
        Exchange(g, n);
        return true;
    }
    if (g->nat[n].alive) {
        Game_Log(g, EV_WAR, "%s capitule sans conditions.", (NATIONS[n].name));
        Surrender(g, n);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Armes tactiques                                                     */
/* ------------------------------------------------------------------ */

/* principale nation ennemie presente en p */
static int EnemyIn(const Game *g, int side, int p)
{
    float best = 0;
    int who = -1;
    float str[N_COUNT] = { 0 };
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (u->active && u->prov == p && War_AtWar(g, side, u->owner)) str[u->owner] += u->str;
    }
    for (int n = 0; n < N_COUNT; n++) if (str[n] > best) { best = str[n]; who = n; }
    return who;
}

const char *Nuke_TacticalBlocked(const Game *g, int p)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->nat[N_FRANCE].tech[T_NUC] < 3) return "Technologie nucléaire niveau 3 requise";
    if (g->nukePosture < 1) return "Posture minimale : pas d'armes tactiques déployées";
    if (EnemyIn(g, N_FRANCE, p) < 0) return "Aucune division ennemie dans cette province";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

/* frappe tactique de side sur la province p */
static void TacticalHit(Game *g, int side, int p)
{
    Province *pr = &g->prov[p];
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->prov != p) continue;
        if (War_AtWar(g, side, u->owner)) { u->str *= 0.25f; u->org = 0; }
        else u->str *= 0.6f;
        if (u->str < 0.08f) u->active = false;
    }
    pr->pop *= 0.92f;
    pr->damage = Clampf(pr->damage + 0.4f, 0, 1);
    pr->fallout = Clampf(pr->fallout + 0.3f, 0, 1);
    pr->prosperity *= 0.85f;
}

/* province tenue par la France ou ses allies ou se trouve la plus grosse concentration francaise */
static int FrenchConcentration(const Game *g)
{
    float str[PROV_COUNT] = { 0 };
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (u->active && u->owner == N_FRANCE && UNIT_TYPES[u->type].domain == DOM_LAND) str[u->prov] += u->str;
    }
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++)
        if (str[p] > 0 && (best < 0 || str[p] > str[best])) best = p;
    return best;
}

bool Nuke_Tactical(Game *g, int p)
{
    if (Nuke_TacticalBlocked(g, p)) return false;
    int n = EnemyIn(g, N_FRANCE, p);
    g->commandPoints--;
    g->tacticalUsed++;
    TacticalHit(g, N_FRANCE, p);
    Game_Log(g, EV_WAR, "Frappe nucléaire tactique sur les forces de %s à %s.", (NATIONS[n].name), (PROVINCE_NAMES[p]));
    Pariah(g, 25, 20, 8);
    if (g->prov[p].owner == N_FRANCE) AddRes(g, R_COHESION, -10);   /* sur notre propre sol */
    g->credibility = Clampf(g->credibility + 5, 0, 100);

    /* escalade face a une puissance nucleaire */
    if (Nuke_IsPower(g, n) && g->nat[n].alive && Randf(g) < 0.4f) {
        int q = FrenchConcentration(g);
        if (q >= 0) {
            TacticalHit(g, n, q);
            Game_Log(g, EV_WAR, "%s riposte par une frappe tactique sur nos forces à %s.", (NATIONS[n].name), (PROVINCE_NAMES[q]));
            AddRes(g, R_COHESION, -8);
        }
        if (Randf(g) < 0.15f) {
            Game_Log(g, EV_WAR, "L'escalade échappe à tout contrôle : échange stratégique avec %s.", (NATIONS[n].name));
            Exchange(g, n);
        }
    }
    return true;
}

/* une puissance nucleaire acculee peut employer l'arme tactique contre nos forces */
static void AiTactical(Game *g, int n)
{
    if (g->nat[n].tech[T_NUC] < 3 || !War_AtWar(g, N_FRANCE, n)) return;
    if (War_Score(g, N_FRANCE, n) < 40) return;
    /* une concentration francaise sur le sol de n */
    int best = -1;
    float bestStr = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        if (g->prov[p].owner != n) continue;
        float s = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (u->active && u->prov == p && u->owner == N_FRANCE) s += u->str;
        }
        if (s > bestStr) { bestStr = s; best = p; }
    }
    if (best < 0 || bestStr < 2) return;
    if (Randf(g) > 0.06f * (1 - g->credibility / 150.0f)) return;
    TacticalHit(g, n, best);
    Game_Log(g, EV_WAR, "%s emploie l'arme nucléaire tactique contre nos divisions à %s !", (NATIONS[n].name), (PROVINCE_NAMES[best]));
    AddRes(g, R_COHESION, -6);
    for (int k = 1; k < N_COUNT; k++) if (k != n && g->nat[k].alive) AddAiRel(g, k, n, -30);
}

/* ------------------------------------------------------------------ */
/* Proliferation                                                       */
/* ------------------------------------------------------------------ */

static int NukeCapital(const Game *g, int n)
{
    int best = -1;
    for (int p = 0; p < PROV_COUNT; p++)
        if (g->prov[p].owner == n && (best < 0 || g->prov[p].pop > g->prov[best].pop)) best = p;
    return best;
}

/* motivation d'une nation a se doter de l'arme */
static float NukeMotive(const Game *g, int n)
{
    static const float base[N_COUNT] = {
        [N_TURQUIE] = 0.5f, [N_ALGERIE] = 0.4f, [N_EGYPTE] = 0.4f, [N_POLOGNE] = 0.3f,
        [N_ALLEMAGNE] = 0.1f, [N_LIBYE] = 0.15f, [N_MAROC] = 0.1f, [N_UKRAINE] = 0.25f,
    };
    float m = base[n];
    if (m <= 0) return 0;
    for (int k = 0; k < N_COUNT; k++) {
        if (k == n || !g->nat[k].alive || !Nuke_IsPower(g, k)) continue;
        if (War_AtWar(g, n, k)) m += 0.5f;
        else if (Relation(g, n, k) < -30 && War_Adjacent(g, n, k)) m += 0.25f;
    }
    if (g->nat[n].eu && g->umbrella) m -= 0.3f;
    if (g->nat[n].regime >= RG_AUTORITAIRE) m += 0.15f;
    if (g->nukesUsed || g->tacticalUsed) m += 0.2f;
    return m;
}

const char *Nuke_CounterBlocked(const Game *g, int n, int kind)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (n <= 0 || n >= N_COUNT || !g->nat[n].alive || g->nat[n].nukeStatus != 1) return "Aucun programme nucléaire en cours";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (kind == 0 && g->res[R_INFLUENCE] < 15) return "Influence insuffisante (15)";
    if (kind == 1 && g->capacity[D_CYBER] < 3) return "Capacité cyber insuffisante (3)";
    if (kind == 1 && g->res[R_BUDGET] < 2) return "Budget insuffisant (2 Md)";
    return NULL;
}

bool Nuke_Counter(Game *g, int n, int kind)
{
    if (Nuke_CounterBlocked(g, n, kind)) return false;
    Nation *na = &g->nat[n];
    g->commandPoints--;
    if (kind == 0) {
        AddRes(g, R_INFLUENCE, -15);
        float chance = 0.35f + g->res[R_INFLUENCE] / 200.0f + (na->trade ? 0.15f : 0) + (na->eu ? 0.2f : 0);
        if (Randf(g) < chance) {
            na->nukeProgress = Clampf(na->nukeProgress - 25, 0, 100);
            Game_Log(g, EV_GOOD, "Pression diplomatique : %s ralentit son programme nucléaire.", (NATIONS[n].name));
        } else {
            Game_Log(g, EV_BAD, "%s rejette nos pressions sur son programme nucléaire.", (NATIONS[n].name));
        }
        AddRelation(g, n, -8);
    } else {
        AddRes(g, R_BUDGET, -2);
        float chance = 0.45f + g->capacity[D_CYBER] * 0.05f;
        if (Randf(g) < chance) {
            na->nukeProgress = Clampf(na->nukeProgress - 40, 0, 100);
            Game_Log(g, EV_GOOD, "Sabotage cyber réussi : les centrifugeuses de %s sont détruites.", (NATIONS[n].name));
        } else {
            AddRelation(g, n, -20);
            AddRes(g, R_INFLUENCE, -5);
            Game_Log(g, EV_BAD, "Le sabotage du programme de %s est découvert : scandale international.", (NATIONS[n].name));
        }
    }
    return true;
}

static void Proliferation(Game *g)
{
    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive || na->nukeStatus >= 2) continue;
        float motive = NukeMotive(g, n);
        if (na->nukeStatus == 0) {
            if (g->turn > 6 && motive > 0.3f && Randf(g) < motive * 0.02f) {
                na->nukeStatus = 1;
                na->nukeProgress = 0;
                Game_Log(g, EV_WAR, "Renseignement : %s lance un programme nucléaire militaire.", (NATIONS[n].name));
            }
            continue;
        }
        /* programme en cours */
        int cap = NukeCapital(g, n);
        if (cap >= 0 && g->prov[cap].ctrl != n) {
            if (na->nukeProgress > 0) Game_Log(g, EV_GOOD, "L'occupation de %s anéantit le programme nucléaire de %s.",
                                               (PROVINCE_NAMES[cap]), (NATIONS[n].name));
            na->nukeProgress = 0;
            continue;
        }
        if (motive <= 0.05f && Randf(g) < 0.05f) {
            na->nukeStatus = 0;
            na->nukeProgress = 0;
            Game_Log(g, EV_GOOD, "%s renonce à son programme nucléaire.", (NATIONS[n].name));
            continue;
        }
        float rate = 0.6f + na->tech[T_LOG] * 0.15f + (na->treasury > 5 ? 0.4f : 0) + motive * 0.6f;
        na->nukeProgress += rate;
        if (na->nukeProgress >= 100) {
            na->nukeStatus = 2;
            na->tech[T_NUC] = 3;
            Game_Log(g, EV_WAR, "%s procède à son premier essai nucléaire. Le club atomique s'agrandit.", (NATIONS[n].name));
            AddRes(g, R_INFLUENCE, -5);
            for (int k = 1; k < N_COUNT; k++)
                if (k != n && g->nat[k].alive && War_Adjacent(g, n, k)) AddAiRel(g, n, k, -15);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Nuke_Turn(Game *g)
{
    static const float base[3] = { 25, 60, 85 };
    float target = base[g->nukePosture] + g->capacity[D_ESPACE] * 1.5f + g->capacity[D_MER] * 1.0f
                 + g->nat[N_FRANCE].tech[T_NUC] * 2;
    Regime r = g->nat[N_FRANCE].regime;
    if (r == RG_JUNTE || r == RG_TOTALITAIRE) target += 8;   /* imprevisibilite */
    if (g->nukesUsed) target += 15;                           /* elle a deja servi */
    if (g->tacticalUsed) target += 5;
    if (Strat_Sat(g, N_FRANCE, SR_URANIUM) < 0.5f) target -= 15;   /* penurie d'uranium */
    target = Clampf(target, 0, 100);
    g->credibility += (target - g->credibility) * 0.1f;
    if (g->umbrella) AddRes(g, R_INFLUENCE, 0.5f);

    Proliferation(g);

    for (int n = 1; n < N_COUNT; n++) {
        if (!g->nat[n].alive || !Nuke_IsPower(g, n) || !War_AtWar(g, N_FRANCE, n)) continue;
        AiTactical(g, n);
        if (g->state != GAME_RUNNING) return;
        /* chantage d'une puissance nucleaire qui perd la guerre */
        if (War_Turns(g, N_FRANCE, n) > 4 && g->pendingDilemma < 0 && War_Score(g, N_FRANCE, n) > 20
            && Randf(g) < 0.15f) {
            g->pendingDilemma = DLM_NUKE_BLACKMAIL;
            g->dilemmaParam = n;
        }
    }
}

/* resolution du chantage (appelee depuis les dilemmes) */
void Nuke_ResolveBlackmail(Game *g, bool give)
{
    int n = g->dilemmaParam;
    if (give) {
        War_WhitePeace(g, N_FRANCE, n, true);
        AddRes(g, R_COHESION, -8);
        g->credibility = Clampf(g->credibility - 10, 0, 100);
        return;
    }
    float strike = (1 - g->credibility / 100.0f) * 0.3f;
    if (Randf(g) < strike) {
        static const int targets[] = { PV_FR_GES, PV_FR_BRE, PV_FR_OCC };
        int p = targets[Rand(g) % 3];
        Devastate(g, p, 0.3f);
        AddRes(g, R_COHESION, -25);
        AddRes(g, R_SOUVERAINETE, -15);
        for (int k = 1; k < N_COUNT; k++) if (k != n) AddRelation(g, k, 30);
        Game_Log(g, EV_WAR, "%s a frappé %s. Le monde se range derrière la France.", (NATIONS[n].name), (PROVINCE_NAMES[p]));
    } else {
        g->credibility = Clampf(g->credibility + 10, 0, 100);
        Game_Log(g, EV_GOOD, "Le chantage de %s échoue : notre dissuasion a tenu.", (NATIONS[n].name));
    }
}
