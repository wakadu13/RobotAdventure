/*
 * BOUCLIER 2050 - outils partages entre les modules de simulation
 */
#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

#include "game.h"

static inline unsigned Rand(Game *g)
{
    unsigned x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x ? x : 0x9E3779B9u;
    return g->rng;
}
static inline float Randf(Game *g) { return (Rand(g) & 0xFFFFFF) / 16777216.0f; }
static inline int RandRange(Game *g, int lo, int hi) { return lo + (int)(Rand(g) % (unsigned)(hi - lo + 1)); }
static inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline void AddRes(Game *g, Resource r, float v)
{
    g->res[r] += v;
    if (r != R_BUDGET) g->res[r] = Clampf(g->res[r], 0, 100);
}

static inline void AddFac(Game *g, int f, float hostility, float capability)
{
    g->fac[f].hostility  = Clampf(g->fac[f].hostility + hostility, 5, 100);
    g->fac[f].capability = Clampf(g->fac[f].capability + capability, 5, 100);
}

static inline void AddRelation(Game *g, int n, float v)
{
    g->nat[n].relation = Clampf(g->nat[n].relation + v, -100, 100);
}

static inline bool IsDemocratic(Regime r) { return r <= RG_TECHNOCRATIE; }

/* passerelles entre modules */
void Hybrid_SpawnMigration(Game *g, int theater, float millions);
void Soc_SetRegime(Game *g, Regime r);

#endif
