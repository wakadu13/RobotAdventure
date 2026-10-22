/*
 * BOUCLIER 2050 - outils partages entre les modules de la guerre
 * (war.c, war_ai.c, war_diplo.c, war_logistics.c).
 */
#ifndef WAR_INTERNAL_H
#define WAR_INTERNAL_H

#include "game_internal.h"
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern int groupFilter;

bool Hostile(const Game *g, int n, int p);
float AvgWealth(const Game *g, int n);
int Capital(const Game *g, int n);
int CoastalBase(const Game *g, int n);
bool IsAdjacent(int a, int b);
bool TouchesNation(const Game *g, int p, int n);
bool SeaEdge(int a, int b);
int Distance(int from, int to, int maxd, bool sea);
void GainXp(Unit *u, int v);
int StackLimit(UnitDomain d);
void UpdateArmyPower(Game *g);
int PickType(Game *g, int n, bool coastal);
int SpawnSite(const Game *g, int n, int type);
bool CanCross(const Game *g, const Unit *u, int from, int to);
int LandPath(const Game *g, const Unit *u, int target, int path[], int maxLen);
const char *OrderBlocked(const Game *g, int n, int i, int t);
float Readiness(const Unit *u);
float AirPower(const Game *g, int side, int enemy, int p);
float AirSuperiority(const Game *g, int side, int enemy, int p);
float AntiAir(const Game *g, int side, int p);
float NavalPower(const Game *g, int n, int p, int radius);
float TerrainFactor(int type, int terrain, bool attack);
float UnitAttack(const Game *g, const Unit *u, int p, int defender);
float UnitDefense(const Game *g, const Unit *u, int p);
bool IsAttacker(const Game *g, const Unit *u, int p, int a);
bool IsDefender(const Game *g, const Unit *u, int p, int a);
float FireSupport(const Game *g, int side, int p, bool attack, int *hyper);
float Garrison(const Game *g, int p);
float DefensePower(const Game *g, int p, int a);
float AttackPower(const Game *g, int p, int a, bool ordered);
void AirFactors(const Game *g, int p, int a, float *fa, float *fd);
float Odds(float att, float def);
void Relocate(Game *g, Unit *u);
void RelocateForeign(Game *g, int p);
void KillNation(Game *g, int n);
void Battle(Game *g, int p, int a);
void NavalWarfare(Game *g);
bool CanAttack(const Game *g, int n, int e);
float TargetValue(const Game *g, int n, int e);
bool ReadyLand(const Game *g, const Unit *u, int n, const bool *used);
float StagedAttack(const Game *g, int n, int e, const bool *used, bool near);
void LaunchAttack(Game *g, int n, int e, bool *used);
void Stage(Game *g, int n, int e, bool *used);
float FrontThreat(const Game *g, int n, int f, int *enemyOut);
void StepToward(Game *g, Unit *u, const int dist[PROV_COUNT]);
void FriendlyBfs(const Game *g, int n, const bool src[PROV_COUNT], int dist[PROV_COUNT]);
void AiOrders(Game *g, int n);
void AiRecruit(Game *g, int n);
float Value(const Game *g, int p);
float NationValue(const Game *g, int n);
float CedeCost(const Game *g, int p);
float ExhaustionVs(const Game *g, int n, int e);
void FitTreaty(Game *g, Treaty *t, float limit);
void AiPeace(Game *g, int a, int b);
void AiPeaceOfferToFrance(Game *g, int n);
void AiProposals(Game *g, int n);
void Capitulations(Game *g);
float RelationTarget(const Game *g, int n);
float AiRelTarget(const Game *g, int a, int b);
float Coalition(const Game *g, int n);
void Movements(Game *g);
void Battles(Game *g);
void Supply(Game *g);
void Alliances(Game *g);
int DepotCount(const Game *g, int n);
void Works(Game *g);
void Reveal(bool *v, int p, int radius);
void FrenchGroups(Game *g);
void NavalMissions(Game *g);
void StrategicBombing(Game *g);
void AiMissions(Game *g, int n);

#endif
