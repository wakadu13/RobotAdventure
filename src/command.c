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

bool Cmd_Apply(Game *g, const Command *c)
{
    if (c->player != N_FRANCE) return false;   /* un seul camp jouable pour l'instant */
    switch (c->type) {
    case CMD_END_TURN:        if (g->pendingDilemma >= 0) return false; Game_EndTurn(g); return true;
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
    case CMD_TAX:             Eco_SetTax(g, c->a, c->f); return true;
    case CMD_SPENDING:        Eco_SetSpending(g, c->a, c->f); return true;
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
