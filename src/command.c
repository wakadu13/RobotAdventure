/*
 * BOUCLIER 2050 - couche de commandes, premiere brique du multijoueur.
 *
 * Toute action d'un joueur peut s'exprimer comme une commande de taille
 * fixe, serialisable et rejouable. La simulation etant deterministe (un
 * generateur pseudo-aleatoire dans l'etat de jeu), des clients qui
 * appliquent les memes commandes dans le meme ordre obtiennent le meme
 * etat : c'est le principe du lockstep. La somme de controle detecte une
 * desynchronisation. Voir docs/MULTIJOUEUR.md.
 */
#include "game_internal.h"

#include <stdio.h>
#include <string.h>

#define IN(v, n) ((v) >= 0 && (v) < (n))

/* fin de la phase du joueur actif : on passe au commandant suivant, ou on resout le tour */
static bool EndPhase(Game *g)
{
    if (g->activePlayer == N_FRANCE && g->pendingDilemma >= 0) return false;
    int next = War_NextPlayer(g, g->activePlayer == N_FRANCE ? 0 : g->activePlayer);
    if (next >= 0) {
        g->activePlayer = next;
        War_UpdateIntel(g);
        return true;
    }
    g->activePlayer = N_FRANCE;
    War_UpdateIntel(g);   /* la resolution du tour se fait avec la vue francaise */
    Game_EndTurn(g);
    return true;
}

/* actions d'un commandant (nation dirigee par un autre joueur) */
static bool CommanderApply(Game *g, const Command *c)
{
    int n = c->player;
    switch (c->type) {
    case CMD_RECRUIT:     return Cmdr_Recruit(g, n, c->a, c->b);
    case CMD_ORDER:       return Cmdr_Order(g, n, c->a, c->b);
    case CMD_DISBAND:     return Cmdr_Disband(g, n, c->a);
    case CMD_DECLARE_WAR: return Cmdr_Declare(g, n, c->a);
    case CMD_PEACE:       return Cmdr_Peace(g, n, c->a);
    case CMD_DIPLOMACY:   return c->b == 2 && Cmdr_Ally(g, n, c->a);
    case CMD_CMDR_TAX:    return Cmdr_SetTax(g, n, c->a);
    case CMD_CMDR_TREATY: return Cmdr_AnswerTreaty(g, n, c->a != 0);
    case CMD_CMDR_INVEST: return c->b >= -1 && c->b < PROV_COUNT && Cmdr_Invest(g, n, c->a, c->b);
    default:              return false;
    }
}

/* bornes des arguments : une commande venue du reseau ne doit jamais sortir des tableaux */
static bool ArgsValid(const Command *c)
{
    int a = c->a, b = c->b;
    switch (c->type) {
    case CMD_RECRUIT:        return IN(a, U_TYPE_COUNT) && IN(b, PROV_COUNT);
    case CMD_ORDER:          return IN(a, MAX_UNITS) && IN(b, PROV_COUNT);
    case CMD_DISBAND:        return IN(a, MAX_UNITS);
    case CMD_DECLARE_WAR:    case CMD_EMBARGO: case CMD_RESOLUTION: case CMD_WARNING: case CMD_PEACE: case CMD_ADD_PLAYER: case CMD_DROP_PLAYER: case CMD_JOIN_PLAYER: return IN(a, N_COUNT);
    case CMD_DIPLOMACY:      return IN(a, N_COUNT) && IN(b, 3);
    case CMD_OPERATION:      return IN(a, N_COUNT) && IN(b, OP_COUNT);
    case CMD_COUNTER:        return IN(a, N_COUNT) && IN(b, 2);
    case CMD_POSTURE:        return IN(a, TH_COUNT) && (b == 1 || b == -1);
    case CMD_INVEST:         return IN(a, D_COUNT);
    case CMD_FACTION_ACTION: return IN(a, F_COUNT) && IN(b, A_COUNT);
    case CMD_PROJECT:        return IN(a, P_COUNT);
    case CMD_FORTIFY:        case CMD_DEPOT: case CMD_FACTORY: case CMD_FAB: case CMD_MINE:
    case CMD_STRIKE:         case CMD_TACTICAL: case CMD_ULTIMATUM: return IN(a, PROV_COUNT);
    case CMD_GROUP:          return IN(a, MAX_UNITS);
    case CMD_MISSION:        return IN(a, MAX_UNITS) && IN(b, NM_COUNT);
    case CMD_ASSIGN:         return IN(a, MAX_GENERALS) && b >= -1 && b < PROV_COUNT;
    case CMD_CULTURE:        return IN(a, CP_COUNT);
    case CMD_STRAT_BUY:      return IN(a, SR_COUNT);
    case CMD_REGIME:         return IN(a, RG_COUNT);
    case CMD_DECREE:         return IN(a, 2);
    default:                 return true;
    }
}

bool Cmd_Apply(Game *g, const Command *c)
{
    if (c->type >= CMD_COUNT || !ArgsValid(c) || c->player < 0 || c->player >= N_COUNT) return false;
    /* un joueur deconnecte : sa nation revient a l'IA (emis par l'hote, meme hors de sa phase) */
    if (c->type == CMD_DROP_PLAYER && c->player == N_FRANCE && c->a > 0 && c->a < N_COUNT && g->human[c->a]) {
        g->human[c->a] = false;
        if (g->activePlayer == c->a) {   /* c'etait sa phase : on passe au suivant */
            int next = War_NextPlayer(g, c->a);
            g->activePlayer = next >= 0 ? next : N_FRANCE;
            War_UpdateIntel(g);
            if (next < 0) Game_EndTurn(g);
        }
        return true;
    }
    /* un joueur rejoint une partie en cours : sa nation passe sous son commandement (emis par l'hote) */
    if (c->type == CMD_JOIN_PLAYER) {
        if (c->player != N_FRANCE || c->a <= 0 || !g->nat[c->a].alive || g->human[c->a]) return false;
        g->human[c->a] = true;
        return true;
    }
    if (c->player != g->activePlayer) return false;   /* ce n'est pas son tour */
    if (c->type == CMD_END_TURN) return EndPhase(g);
    if (c->player != N_FRANCE) return CommanderApply(g, c);
    switch (c->type) {
    case CMD_SCENARIO:        return Game_ApplyScenario(g, c->a);
    case CMD_MINISTER:        return Cab_Appoint(g, c->a, c->b);
    case CMD_ADD_PLAYER:
        if (g->turn != g->startTurn || c->a <= 0 || c->a >= N_COUNT || !g->nat[c->a].alive) return false;
        g->human[c->a] = !g->human[c->a];
        return true;
    case CMD_RECRUIT:         return War_Recruit(g, c->a, c->b);
    case CMD_ORDER:           return War_Order(g, c->a, c->b);
    case CMD_DISBAND:         return War_Disband(g, c->a);
    case CMD_DECLARE_WAR:     return War_Declare(g, c->a);
    case CMD_DIPLOMACY:       return War_Diplomacy(g, c->a, c->b);
    case CMD_OPERATION:       return Ops_Run(g, c->a, c->b);
    case CMD_POSTURE:         return Game_ChangePosture(g, c->a, c->b);
    case CMD_INVEST:          return Game_InvestCapacity(g, (Domain)c->a);
    case CMD_FACTION_ACTION:  return Game_FactionAction(g, c->a, c->b);
    case CMD_PROJECT:         return Game_StartProject(g, c->a);
    case CMD_TAX:             if (c->a < 0 || c->a >= S_COUNT) return false; Eco_SetTax(g, c->a, c->f); return true;
    case CMD_SPENDING:        if (c->a < 0 || c->a >= SP_COUNT) return false; Eco_SetSpending(g, c->a, c->f); return true;
    case CMD_RESEARCH:        Mil_SetResearch(g, c->a); return true;
    case CMD_FORTIFY:         return War_Fortify(g, c->a);
    case CMD_DEPOT:           return War_BuildDepot(g, c->a);
    case CMD_GROUP:           War_SetGroup(g, c->a, c->b); return true;
    case CMD_GROUP_ORDER:     War_SetGroupOrder(g, c->a, c->b, c->c); return true;
    case CMD_MISSION:         return War_SetMission(g, c->a, c->b);
    case CMD_DILEMMA:         if (g->pendingDilemma < 0) return false; Game_ResolveDilemma(g, c->a); return true;
    case CMD_BORROW:          return Eco_Borrow(g);
    case CMD_REPAY:           return Eco_Repay(g);
    case CMD_RESOLUTION:      return Intl_ProposeResolution(g, c->a);
    case CMD_FACTORY:         return Eco_BuildFactory(g, c->a);
    case CMD_FAB:             return Strat_BuildFab(g, c->a);
    case CMD_MINE:            return Strat_OpenMine(g, c->a);
    case CMD_STRIKE:          return Nuke_Strike(g, c->a);
    case CMD_TACTICAL:        return Nuke_Tactical(g, c->a);
    case CMD_ULTIMATUM:       return War_Ultimatum(g, c->a);
    case CMD_NUKE_POSTURE:    Nuke_SetPosture(g, c->a); return true;
    case CMD_UMBRELLA:        if (Nuke_UmbrellaBlocked(g)) return false; Nuke_ToggleUmbrella(g); return true;
    case CMD_PROMOTE:         return Mil_Promote(g);
    case CMD_ASSIGN:          return Mil_Assign(g, c->a, c->b);
    case CMD_RESEARCH_BUDGET: Mil_SetResearchBudget(g, c->f); return true;
    case CMD_TREATY_DRAFT:    if (c->a <= 0 || c->a >= N_COUNT || !g->war[N_FRANCE][c->a]) return false; War_TreatyDraft(g, c->a); return true;
    case CMD_TREATY_CEDE:     if (!g->treaty.active || c->a < 0 || c->a >= PROV_COUNT) return false; g->treaty.cede[c->a] = !g->treaty.cede[c->a]; return true;
    case CMD_TREATY_REPARATIONS:
        if (!g->treaty.active || c->f < 0 || c->f > 10) return false;
        g->treaty.reparations = c->f;
        g->treaty.reparationTurns = 16;
        return true;
    case CMD_NEGOTIATE:       if (!g->treaty.active) return false; War_Negotiate(g); return true;
    case CMD_NATO:            if (Intl_NatoBlocked(g)) return false; Intl_ToggleNato(g); return true;
    case CMD_EMBARGO:         return Eco_ToggleEmbargo(g, c->a);
    case CMD_COUNTER:         return Nuke_Counter(g, c->a, c->b);
    case CMD_WARNING:         return Nuke_Warning(g, c->a);
    case CMD_CULTURE:         if (Soc_CulturePolicyBlocked(g, c->a)) return false; Soc_SetCulturePolicy(g, c->a); return true;
    case CMD_TARIFF:          Eco_SetTariff(g, c->f); return true;
    case CMD_STRAT_BUY:       return Strat_Buy(g, c->a);
    case CMD_REGIME:          return Soc_ChangeRegime(g, c->a);
    case CMD_DECREE:          return Pol_Decree(g, c->a);
    case CMD_MIGRATION:       if (c->a < 0 || c->a >= 3) return false; Game_SetMigrationDoctrine(g, c->a); return true;
    case CMD_ALERT:           if (c->a < 0 || c->a >= 3) return false; Game_SetAlertLevel(g, c->a); return true;
    default:                  return false;
    }
}

/* format reseau : 12 octets, petit-boutiste, independant du compilateur */
static void Put16(unsigned char *o, int v) { o[0] = (unsigned char)(v & 0xFF); o[1] = (unsigned char)((v >> 8) & 0xFF); }
static int Get16(const unsigned char *i) { return (short)(i[0] | (i[1] << 8)); }

void Cmd_Serialize(const Command *c, unsigned char out[CMD_BYTES])
{
    out[0] = c->type;
    out[1] = (unsigned char)c->player;
    Put16(out + 2, c->a);
    Put16(out + 4, c->b);
    Put16(out + 6, c->c);
    unsigned u;
    memcpy(&u, &c->f, 4);
    for (int k = 0; k < 4; k++) out[8 + k] = (unsigned char)((u >> (8 * k)) & 0xFF);
}

bool Cmd_Deserialize(const unsigned char in[CMD_BYTES], Command *c)
{
    if (in[0] >= CMD_COUNT) return false;
    c->type = in[0];
    c->player = (signed char)in[1];
    c->a = (short)Get16(in + 2);
    c->b = (short)Get16(in + 4);
    c->c = (short)Get16(in + 6);
    unsigned u = 0;
    for (int k = 0; k < 4; k++) u |= (unsigned)in[8 + k] << (8 * k);
    memcpy(&c->f, &u, 4);
    return true;
}

/* somme de controle de l'etat de la simulation (FNV-1a) : deux clients synchronises obtiennent la meme */
unsigned Game_Checksum(const Game *g)
{
    unsigned h = 2166136261u;
    h = (h ^ g->rng) * 16777619u;
    h = (h ^ (unsigned)g->turn) * 16777619u;
    for (int r = 0; r < R_COUNT; r++) h = (h ^ (unsigned)(g->res[r] * 1000)) * 16777619u;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        h = (h ^ (unsigned)(pr->owner * 64 + pr->ctrl)) * 16777619u;
        h = (h ^ (unsigned)(pr->pop * 1000)) * 16777619u;
    }
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active) continue;
        h = (h ^ (unsigned)(i * 131 + u->type * 7 + u->owner)) * 16777619u;
        h = (h ^ (unsigned)(u->prov * 1000 + (int)(u->str * 999))) * 16777619u;
    }
    for (int n = 0; n < N_COUNT; n++) h = (h ^ (unsigned)(g->nat[n].relation * 100 + 10000)) * 16777619u;
    return h;
}

void Replay_Clear(Replay *r) { r->count = 0; }

bool Replay_Record(Replay *r, const Command *c)
{
    if (r->count >= REPLAY_MAX) return false;
    r->cmds[r->count++] = *c;
    return true;
}

/* fichier de rejeu : en-tete puis commandes serialisees */
bool Replay_Save(const Replay *r, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    unsigned char head[12] = { 'B', '2', '0', 'R', 'P', 1 };
    unsigned v[2] = { r->seed, (unsigned)r->difficulty };
    for (int k = 0; k < 2; k++)
        for (int b = 0; b < 3; b++) head[6 + k * 3 + b] = (unsigned char)((v[k] >> (8 * b)) & 0xFF);
    bool ok = fwrite(head, 12, 1, f) == 1;
    for (int i = 0; i < r->count && ok; i++) {
        unsigned char buf[CMD_BYTES];
        Cmd_Serialize(&r->cmds[i], buf);
        ok = fwrite(buf, CMD_BYTES, 1, f) == 1;
    }
    fclose(f);
    return ok;
}

/* rejoue une partie : meme graine, meme difficulte, memes commandes */
void Replay_Run(const Replay *r, Game *g)
{
    Game_Init(g, r->seed, r->difficulty);
    for (int i = 0; i < r->count; i++) Cmd_Apply(g, &r->cmds[i]);
}
