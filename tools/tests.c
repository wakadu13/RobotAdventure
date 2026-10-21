/*
 * Tests des mecaniques rarement atteintes par les bots : frappes et riposte
 * nucleaires, avertissement, chantage, decrets de chaque regime, elections,
 * usines, recrutement, parapluie europeen.
 *   make test
 */
#include "../src/game_internal.h"
#include <stdio.h>
#define CHECK(c) do { if (!(c)) { printf("ECHEC ligne %d : %s\n", __LINE__, #c); fails++; } } while (0)
int main(void){ int fails=0; Game g;
 /* frappe contre une nation non nucleaire */
 Game_Init(&g, 7, 1); g.commandPoints=3; CHECK(War_Declare(&g, N_ALGERIE));
 CHECK(Nuke_Strike(&g, PV_DZ_N)); CHECK(g.nukesUsed==1); CHECK(!g.nat[N_ALGERIE].atWar); CHECK(g.state==GAME_RUNNING);
 CHECK(g.prov[PV_DZ_N].fallout>0.5f);
 for(int i=0;i<10;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 /* frappe contre une puissance nucleaire : riposte */
 Game_Init(&g, 8, 1); g.commandPoints=3; CHECK(War_Declare(&g, N_UK)); CHECK(Nuke_Strike(&g, PV_UK_ANG)); CHECK(g.state==GAME_LOST_NUCLEAR);
 /* ultime avertissement */
 Game_Init(&g, 9, 1); g.commandPoints=3; War_Declare(&g, N_ESPAGNE); War_Capture(&g, PV_FR_SO, N_ESPAGNE);
 CHECK(Nuke_WarningBlocked(&g, N_ESPAGNE)==NULL); Nuke_Warning(&g, N_ESPAGNE); CHECK(g.nat[N_ESPAGNE].warned || !g.nat[N_ESPAGNE].atWar);
 /* chantage */
 for(int k=0;k<2;k++){ Game_Init(&g, 10+k, 1); g.nat[N_BOREALE].atWar=true; g.pendingDilemma=DLM_NUKE_BLACKMAIL; g.dilemmaParam=N_BOREALE; Game_ResolveDilemma(&g,k); }
 /* decrets de chaque regime */
 for(int r=0;r<RG_COUNT;r++) for(int k=0;k<2;k++){ Game_Init(&g, 20+r, 1); g.res[R_BUDGET]=100; Soc_SetRegime(&g,(Regime)r); g.commandPoints=3;
   CHECK(Pol_DecreeBlocked(&g,k)==NULL); CHECK(Pol_Decree(&g,k)); for(int i=0;i<5;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} }
 /* elections et coup legal */
 Game_Init(&g, 30, 1); for(int i=0;i<40;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 g.pendingDilemma=DLM_COUP_LEGAL; Game_ResolveDilemma(&g,0); CHECK(g.nat[N_FRANCE].regime==RG_AUTORITAIRE);
 /* usine, recrutement, ordres */
 Game_Init(&g, 31, 1); g.res[R_BUDGET]=100; CHECK(Eco_BuildFactory(&g, PV_FR_NORD)); for(int i=0;i<4;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 CHECK(g.prov[PV_FR_NORD].industry>6.5f);
 g.res[R_BUDGET]=100; CHECK(War_Recruit(&g, U_BLINDE, PV_FR_EST)); CHECK(War_RecruitBlocked(&g, U_INF, PV_ES_CAT)!=NULL);
 /* parapluie */
 Game_Init(&g, 32, 1); g.credibility=70; CHECK(Nuke_UmbrellaBlocked(&g)==NULL); Nuke_ToggleUmbrella(&g); CHECK(g.umbrella);
 printf("%s (%d echec(s))\n", fails?"TESTS EN ECHEC":"TOUS LES TESTS PASSENT", fails); return fails!=0;
}
