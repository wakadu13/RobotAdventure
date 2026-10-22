/*
 * BOUCLIER 2050 - gouvernement, lobbies et opinion.
 *
 * Quatre ministres, chacun avec sa competence et le lobby dont il est proche,
 * peuvent etre remplaces. Quatre lobbies jugent la politique menee ; leur
 * satisfaction, la cohesion, l'inflation, le chomage et la guerre forment la
 * cote de popularite du gouvernement, qui pese a son tour sur la cohesion.
 */
#include "game_internal.h"
#include "lang.h"

#include <math.h>
#include <stddef.h>

const char *MINISTER_POSTS[MIN_COUNT] = { "Défense", "Économie", "Intérieur", "Affaires étrangères" };
const char *MINISTER_EFFECTS[MIN_COUNT] = {
    "Qualité de l'armée +2 % par niveau de compétence.",
    "+0,4 Md par tour et inflation freinée, par niveau de compétence.",
    "Militance de toutes les strates en baisse chaque tour, selon la compétence.",
    "Influence en hausse chaque tour, selon la compétence.",
};
const char *LOBBY_NAMES[LB_COUNT] = { "Industriels", "Écologistes", "Militaires", "Syndicats" };
const char *LOBBY_DESC[LB_COUNT] = {
    "Aiment les impôts modérés sur les élites, une inflation contenue et des droits de douane bas. Satisfaits : +1 Md par tour ; "
    "mécontents : fuite des capitaux, -1 Md par tour.",
    "Aiment la lutte contre le réchauffement, les projets d'énergies renouvelables et d'adaptation, un gouvernement écologiste ; "
    "détestent l'arme nucléaire. Satisfaits : +Influence ; mécontents : -Cohésion.",
    "Aiment une armée nombreuse, une dissuasion crédible, la recherche militaire et la guerre ; détestent l'austérité. "
    "Satisfaits : crédibilité en hausse ; mécontents : crédibilité en baisse.",
    "Aiment les impôts modérés sur les travailleurs, les dépenses sociales, le plein emploi et des prix stables. Satisfaits : "
    "militance des travailleurs en baisse ; mécontents : en hausse.",
};
static const char *NAMES[] = {
    "Hélène Marchetti", "Paul Delacroix", "Inès Benali", "Marc Lefèvre-Roy", "Sophie Garnier", "Karim Aït-Saïd",
    "Claire Vasseur", "Julien Morvan", "Nadia Ferrand", "Thomas Kerboul", "Élise Duval", "Antoine Rocher",
    "Leïla Haddad", "Bruno Castel", "Camille Joubert", "Yann Le Goff", "Mathilde Saunier", "Hugo Barral",
    "Aïcha Diallo", "Pierre-Louis Vigier", "Margaux Tessier", "Samuel Ohana", "Chloé Bertin", "Victor Arnaud",
};
#define NAME_COUNT ((int)(sizeof(NAMES) / sizeof(NAMES[0])))

const char *Cab_Name(const Minister *m) { return NAMES[m->name % NAME_COUNT]; }

static Minister RandomMinister(Game *g, int skillMax)
{
    Minister m;
    m.name = (unsigned char)(Rand(g) % NAME_COUNT);
    m.skill = (unsigned char)RandRange(g, 1, skillMax);
    m.lobby = (unsigned char)(Rand(g) % LB_COUNT);
    return m;
}

static void NewCandidates(Game *g)
{
    for (int p = 0; p < MIN_COUNT; p++)
        for (int c = 0; c < 3; c++) g->candidates[p][c] = RandomMinister(g, 3);
}

void Cab_Init(Game *g)
{
    for (int p = 0; p < MIN_COUNT; p++) g->minister[p] = RandomMinister(g, 2);
    NewCandidates(g);
    for (int l = 0; l < LB_COUNT; l++) g->lobby[l] = 50;
    g->approval = 50;
}

int Cab_Skill(const Game *g, int post) { return g->minister[post].skill; }

const char *Cab_AppointBlocked(const Game *g, int post, int cand)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (post < 0 || post >= MIN_COUNT || cand < 0 || cand >= 3) return "Choix invalide";
    if (g->commandPoints < 1) return "Plus de points de commandement";
    return NULL;
}

bool Cab_Appoint(Game *g, int post, int cand)
{
    if (Cab_AppointBlocked(g, post, cand)) return false;
    Minister old = g->minister[post];
    g->minister[post] = g->candidates[post][cand];
    g->candidates[post][cand] = old;
    g->commandPoints--;
    AddRes(g, R_COHESION, -1);
    g->lobby[g->minister[post].lobby] = Clampf(g->lobby[g->minister[post].lobby] + 8, 0, 100);
    g->lobby[old.lobby] = Clampf(g->lobby[old.lobby] - 5, 0, 100);
    Game_Log(g, EV_INFO, "Remaniement : %s devient ministre (%s).", Cab_Name(&g->minister[post]), Tr(MINISTER_POSTS[post]));
    return true;
}

float Cab_LobbyTarget(const Game *g, int l)
{
    float infl = fmaxf(0, g->inflation - 3);
    float t = 50;
    switch (l) {
    case LB_INDUSTRIE:
        t = 50 + (0.3f - g->taxRate[S_ELITES]) * 120 - infl * 3 + (g->tariff < 0.1f ? 5 : -5);
        break;
    case LB_ECOLOGISTES:
        t = 35 + g->climateMitigation * 150 + (g->proj[P_ENR].state == 2 ? 10 : 0) + (g->proj[P_ADAPT].state == 2 ? 10 : 0)
          - (g->nukesUsed + g->tacticalUsed) * 20 + (g->nat[N_FRANCE].government == I_ECOLO ? 15 : 0);
        break;
    case LB_MILITAIRES: {
        float load = War_UnitLoad(g, N_FRANCE) / fmaxf(1, War_ArmyMax(g, N_FRANCE));
        t = 30 + 40 * fminf(1, load) + (g->credibility - 50) * 0.3f + g->researchBudget * 2 + (War_AtWarAny(g, N_FRANCE) ? 10 : 0)
          - (g->austerityTurns > 0 ? 10 : 0);
    } break;
    case LB_SYNDICATS:
        t = 50 + (0.3f - g->taxRate[S_TRAVAILLEURS]) * 120 + (g->spending[SP_SOCIAL] - 0.4f) * 50
          - Eco_NationUnemployment(g, N_FRANCE) * 150 - infl * 3;
        break;
    default: break;
    }
    for (int p = 0; p < MIN_COUNT; p++) if (g->minister[p].lobby == l) t += 10;
    return Clampf(t, 0, 100);
}

float Cab_ApprovalTarget(const Game *g)
{
    float lob = 0;
    for (int l = 0; l < LB_COUNT; l++) lob += g->lobby[l];
    lob /= LB_COUNT;
    float t = 50 + (g->res[R_COHESION] - 50) * 0.4f - fmaxf(0, g->inflation - 3) * 1.5f - Eco_NationUnemployment(g, N_FRANCE) * 100
            + (lob - 50) * 0.3f - (War_AtWarAny(g, N_FRANCE) ? 5 : 0);
    return Clampf(t, 0, 100);
}

void Cab_Turn(Game *g)
{
    if (g->turn % 4 == 0) NewCandidates(g);
    for (int l = 0; l < LB_COUNT; l++) g->lobby[l] += (Cab_LobbyTarget(g, l) - g->lobby[l]) * 0.2f;

    /* exigences des lobbies */
    if (g->lobby[LB_INDUSTRIE] < 30) AddRes(g, R_BUDGET, -1);
    else if (g->lobby[LB_INDUSTRIE] > 70) AddRes(g, R_BUDGET, 1);
    if (g->lobby[LB_ECOLOGISTES] < 30) AddRes(g, R_COHESION, -0.2f);
    else if (g->lobby[LB_ECOLOGISTES] > 70) AddRes(g, R_INFLUENCE, 0.2f);
    if (g->lobby[LB_MILITAIRES] < 30) g->credibility = Clampf(g->credibility - 0.5f, 0, 100);
    else if (g->lobby[LB_MILITAIRES] > 70) g->credibility = Clampf(g->credibility + 0.3f, 0, 100);
    if (g->lobby[LB_SYNDICATS] < 30) AddMilitancy(g, S_TRAVAILLEURS, 0.04f);
    else if (g->lobby[LB_SYNDICATS] > 70) AddMilitancy(g, S_TRAVAILLEURS, -0.03f);

    /* action des ministres */
    AddRes(g, R_BUDGET, 0.4f * Cab_Skill(g, MIN_ECONOMIE));
    AddMilitancy(g, -1, -0.01f * Cab_Skill(g, MIN_INTERIEUR));
    AddRes(g, R_INFLUENCE, 0.15f * Cab_Skill(g, MIN_ETRANGERES));

    /* opinion */
    g->approval += (Cab_ApprovalTarget(g) - g->approval) * 0.3f;
    AddRes(g, R_COHESION, (g->approval - 50) * 0.006f);
}
