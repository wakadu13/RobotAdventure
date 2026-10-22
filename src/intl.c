/*
 * BOUCLIER 2050 - organisations internationales : Conseil de securite des
 * Nations unies (resolutions, vetos, sanctions, cessez-le-feu) et OTAN
 * (article 5, cohesion de l'Alliance, sortie et retour de la France).
 */
#include "game_internal.h"
#include "lang.h"

#include <stdio.h>

static const NationId NATO_2030[] = {
    N_FRANCE, N_UK, N_BENELUX, N_ALLEMAGNE, N_ITALIE, N_ESPAGNE, N_PORTUGAL, N_DANEMARK, N_SCANDINAVIE,
    N_BALTES, N_POLOGNE, N_EUROPE_C, N_ROUMANIE, N_BULGARIE, N_GRECE, N_TURQUIE, N_BALKANS,
};

void Intl_Init(Game *g)
{
    for (size_t i = 0; i < sizeof(NATO_2030) / sizeof(NATO_2030[0]); i++) g->nat[NATO_2030[i]].nato = true;
    g->natoCohesion = 70;
}

/* ------------------------------------------------------------------ */
/* Conseil de securite                                                 */
/* ------------------------------------------------------------------ */

/* un membre permanent (hors France) bloque-t-il une resolution contre n ? */
static int ForeignVeto(Game *g, int n)
{
    /* Royaume-Uni et Federation Boreale sur la carte ; Etats-Unis et Chine hors carte */
    if (g->nat[N_UK].alive && (n == N_UK || g->ally[N_UK][n] || Relation(g, N_UK, n) > 60)) return N_UK;
    if (g->nat[N_BOREALE].alive && (n == N_BOREALE || g->ally[N_BOREALE][n] || Relation(g, N_BOREALE, n) > 40)) return N_BOREALE;
    if (g->nat[n].nato && n != N_FRANCE && Randf(g) < 0.6f) return -2;   /* veto americain pour un allie */
    if (!IsDemocratic(g->nat[n].regime) && Randf(g) < 0.3f) return -3;    /* veto chinois pour un regime autoritaire */
    return -1;
}

static const char *VetoName(int v)
{
    return v == -2 ? "les États-Unis" : v == -3 ? "la Chine" : Tr(NATIONS[v].name);
}

static void Sanction(Game *g, int n)
{
    g->nat[n].sanctions = 16;
    g->nat[n].trade = false;
    for (int k = 0; k < N_COUNT; k++) if (k != n && g->nat[k].alive && !IsAlly(g, k, n)) AddAiRel(g, k, n, -10);
    Game_Log(g, EV_WAR, "Conseil de sécurité : sanctions internationales contre %s (4 ans).", Tr(NATIONS[n].name));
}

/* vote d'une resolution condamnant n ; renvoie true si elle passe */
static bool Vote(Game *g, int n, bool frenchProposal)
{
    if (n == N_FRANCE) {
        /* la France oppose toujours son veto, au prix de sa reputation */
        AddRes(g, R_INFLUENCE, -5);
        Game_Log(g, EV_BAD, "Conseil de sécurité : la France oppose son veto à une résolution qui la condamne.");
        return false;
    }
    int v = ForeignVeto(g, n);
    if (v != -1) {
        Game_Log(g, frenchProposal ? EV_BAD : EV_INFO, "Conseil de sécurité : %s oppose son veto à la résolution contre %s.",
                 VetoName(v), Tr(NATIONS[n].name));
        return false;
    }
    if (!frenchProposal && (g->ally[N_FRANCE][n] || g->nat[n].relation > 60) && g->pendingDilemma < 0) {
        g->pendingDilemma = DLM_UN_VETO;
        g->dilemmaParam = n;
        return false;
    }
    Sanction(g, n);
    return true;
}

void Intl_ResolveVeto(Game *g, bool veto)
{
    int n = g->dilemmaParam;
    if (veto) {
        AddRes(g, R_INFLUENCE, -6);
        AddRelation(g, n, 15);
        Game_Log(g, EV_INFO, "La France oppose son veto pour protéger %s.", Tr(NATIONS[n].name));
    } else {
        AddRelation(g, n, -20);
        Sanction(g, n);
    }
}

const char *Intl_ResolutionBlocked(const Game *g, int n)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (n <= 0 || n >= N_COUNT || !g->nat[n].alive) return "Nation invalide";
    if (g->nat[n].sanctions > 0) return "Déjà sous sanctions";
    bool cause = g->nat[n].nukeStatus == 1;
    for (int k = 0; k < N_COUNT && !cause; k++) if (g->aggressor[n][k]) cause = true;
    if (!cause) return "Aucun motif : ni agression ni programme nucléaire";
    if (g->unCooldown > 0) return "Le Conseil vient de siéger";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (g->res[R_INFLUENCE] < 10) return "Influence insuffisante (10)";
    return NULL;
}

bool Intl_ProposeResolution(Game *g, int n)
{
    if (Intl_ResolutionBlocked(g, n)) return false;
    g->commandPoints--;
    AddRes(g, R_INFLUENCE, -10);
    g->unCooldown = 4;
    Game_Log(g, EV_INFO, "La France saisit le Conseil de sécurité contre %s.", Tr(NATIONS[n].name));
    bool ok = Vote(g, n, true);
    if (ok) AddRes(g, R_INFLUENCE, 5);
    AddRelation(g, n, -15);
    return ok;
}

/* ------------------------------------------------------------------ */
/* OTAN                                                                */
/* ------------------------------------------------------------------ */

const char *Intl_NatoBlocked(const Game *g)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    if (!g->nat[N_FRANCE].nato) {
        if (!IsDemocratic(g->nat[N_FRANCE].regime)) return "L'Alliance n'accepte que les démocraties";
        if (g->aggressionMemory > 30) return "Nos agressions récentes inquiètent les Alliés";
    }
    return NULL;
}

void Intl_ToggleNato(Game *g)
{
    if (Intl_NatoBlocked(g)) return;
    Nation *fr = &g->nat[N_FRANCE];
    g->commandPoints--;
    fr->nato = !fr->nato;
    for (int k = 1; k < N_COUNT; k++) if (g->nat[k].nato) AddRelation(g, k, fr->nato ? 10 : -15);
    if (fr->nato) {
        g->natoCohesion = Clampf(g->natoCohesion + 10, 0, 100);
        Game_Log(g, EV_GOOD, "La France réintègre le commandement intégré de l'OTAN.");
    } else {
        g->natoCohesion = Clampf(g->natoCohesion - 15, 0, 100);
        AddRes(g, R_SOUVERAINETE, 5);
        AddRes(g, R_INFLUENCE, -5);
        Game_Log(g, EV_EVENT, "La France quitte l'OTAN : indépendance stratégique, mais plus de garantie de l'article 5.");
    }
}

/* une guerre commence : article 5 et saisine du Conseil de securite */
void Intl_OnWar(Game *g, int a, int b)
{
    g->aggressor[a][b] = true;
    if (g->nat[a].nato && g->nat[b].nato) {
        g->natoCohesion = Clampf(g->natoCohesion - 20, 0, 100);
        Game_Log(g, EV_WAR, "Crise de l'OTAN : %s attaque %s, un autre membre de l'Alliance.", Tr(NATIONS[a].name), Tr(NATIONS[b].name));
    } else if (g->nat[b].nato) {
        /* article 5 */
        int joined = 0;
        for (int k = 1; k < N_COUNT; k++) {
            Nation *nk = &g->nat[k];
            if (k == a || k == b || !nk->alive || !nk->nato || g->war[k][a] || nk->failed) continue;
            float p = g->natoCohesion / 100.0f * (IsDemocratic(nk->regime) ? 0.9f : 0.5f) * (War_Adjacent(g, k, a) ? 1.0f : 0.6f);
            if (Randf(g) < p) { War_StartWar(g, k, a, false); joined++; }
        }
        if (g->nat[N_FRANCE].nato && a != N_FRANCE && !g->war[N_FRANCE][a] && g->pendingDilemma < 0) {
            g->pendingDilemma = DLM_ALLY_CALL;
            g->dilemmaParam = a;
            g->dilemmaParam2 = b;
        }
        if (joined || b == N_FRANCE || IsAlly(g, N_FRANCE, b))
            Game_Log(g, EV_WAR, "Article 5 : %d membre(s) de l'OTAN entrent en guerre contre %s pour défendre %s.", joined,
                     Tr(NATIONS[a].name), b == N_FRANCE ? "la France" : Tr(NATIONS[b].name));
    }
    /* le Conseil de securite se saisit d'une agression sur deux */
    if (Randf(g) < 0.5f && g->unCooldown == 0) {
        g->unCooldown = 2;
        Vote(g, a, false);
    }
}

void Intl_Turn(Game *g)
{
    if (g->unCooldown > 0) g->unCooldown--;
    for (int n = 0; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (na->sanctions <= 0) continue;
        if (--na->sanctions == 0) Game_Log(g, EV_INFO, "Levée des sanctions contre %s.", Tr(NATIONS[n].name));
        else if (n == N_FRANCE) AddRes(g, R_INFLUENCE, -0.5f);
    }
    /* cohesion de l'Alliance */
    float target = 70;
    if (!g->nat[N_FRANCE].nato) target -= 10;
    for (int a = 1; a < N_COUNT; a++)
        for (int b = a + 1; b < N_COUNT; b++)
            if (g->war[a][b] && g->nat[a].nato && g->nat[b].nato) target -= 15;
    if (g->nat[N_BOREALE].alive && War_AtWarAny(g, N_BOREALE)) target += 10;   /* la menace soude l'Alliance */
    g->natoCohesion = Clampf(g->natoCohesion + (target - g->natoCohesion) * 0.1f, 0, 100);

    /* casques bleus : un long conflit entre IA finit par un cessez-le-feu negocie a l'ONU */
    for (int a = 1; a < N_COUNT; a++)
        for (int b = a + 1; b < N_COUNT; b++)
            if (g->war[a][b] && War_Turns(g, a, b) > 12 && Randf(g) < 0.04f) {
                War_WhitePeace(g, a, b, false);
                Game_Log(g, EV_INFO, "Cessez-le-feu de l'ONU entre %s et %s : déploiement de casques bleus.", Tr(NATIONS[a].name), Tr(NATIONS[b].name));
            }
}
