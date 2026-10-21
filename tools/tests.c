/*
 * Tests des mecaniques rarement atteintes par les bots : frappes et riposte
 * nucleaires, armes tactiques, proliferation, avertissement, chantage,
 * decrets de chaque regime, elections, usines, recrutement, parapluie
 * europeen, traites, generaux, technologies, embargo, cultures.
 *   make test
 */
#include "../src/game_internal.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { printf("ECHEC ligne %d : %s\n", __LINE__, #c); fails++; } } while (0)
int main(void){ int fails=0; Game g;
 /* frappe contre une nation non nucleaire */
 Game_Init(&g, 7, 1); g.commandPoints=3; CHECK(War_Declare(&g, N_ALGERIE));
 CHECK(Nuke_Strike(&g, PV_DZ_ALG)); CHECK(g.nukesUsed==1); CHECK(!War_AtWar(&g, N_FRANCE, N_ALGERIE)); CHECK(g.state==GAME_RUNNING);
 CHECK(g.prov[PV_DZ_ALG].fallout>0.5f);
 for(int i=0;i<10;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 /* frappe contre une puissance nucleaire : riposte */
 Game_Init(&g, 8, 1); g.commandPoints=3; CHECK(War_Declare(&g, N_UK)); CHECK(Nuke_Strike(&g, PV_UK_MID)); CHECK(g.state==GAME_LOST_NUCLEAR);
 /* ultime avertissement */
 Game_Init(&g, 9, 1); g.commandPoints=3; War_Declare(&g, N_ESPAGNE); War_Occupy(&g, PV_FR_OCC, N_ESPAGNE);
 CHECK(Nuke_WarningBlocked(&g, N_ESPAGNE)==NULL); Nuke_Warning(&g, N_ESPAGNE); CHECK(g.nat[N_ESPAGNE].warned || !War_AtWar(&g, N_FRANCE, N_ESPAGNE));
 /* chantage */
 for(int k=0;k<2;k++){ Game_Init(&g, 10+k, 1); War_StartWar(&g, N_BOREALE, N_FRANCE, false); g.pendingDilemma=DLM_NUKE_BLACKMAIL; g.dilemmaParam=N_BOREALE; Game_ResolveDilemma(&g,k); }
 /* decrets de chaque regime */
 for(int r=0;r<RG_COUNT;r++) for(int k=0;k<2;k++){ Game_Init(&g, 20+r, 1); g.res[R_BUDGET]=100; Soc_SetRegime(&g,(Regime)r); g.commandPoints=3;
   CHECK(Pol_DecreeBlocked(&g,k)==NULL); CHECK(Pol_Decree(&g,k)); for(int i=0;i<5;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} }
 /* elections et coup legal */
 Game_Init(&g, 30, 1); for(int i=0;i<40;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 g.pendingDilemma=DLM_COUP_LEGAL; Game_ResolveDilemma(&g,0); CHECK(g.nat[N_FRANCE].regime==RG_AUTORITAIRE);
 /* usine, recrutement, ordres */
 Game_Init(&g, 31, 1); g.res[R_BUDGET]=100; float ind=g.prov[PV_FR_IDF].industry; CHECK(Eco_BuildFactory(&g, PV_FR_IDF)); for(int i=0;i<4;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 CHECK(g.prov[PV_FR_IDF].industry>ind+0.9f);
 g.res[R_BUDGET]=100; CHECK(War_Recruit(&g, U_BLINDE, PV_FR_GES)); CHECK(War_RecruitBlocked(&g, U_INF, PV_ES_CAT)!=NULL);
 CHECK(War_RecruitBlocked(&g, U_FREGATE, PV_FR_IDF)!=NULL);
 /* parapluie */
 Game_Init(&g, 32, 1); g.credibility=70; CHECK(Nuke_UmbrellaBlocked(&g)==NULL); Nuke_ToggleUmbrella(&g); CHECK(g.umbrella);
 /* traite : occupation puis cession */
 Game_Init(&g, 40, 1); g.commandPoints=3; CHECK(War_Declare(&g, N_ESPAGNE)); War_Occupy(&g, PV_ES_CAT, N_FRANCE);
 War_TreatyDraft(&g, N_ESPAGNE); CHECK(g.treaty.winner==N_FRANCE); CHECK(g.treaty.cede[PV_ES_CAT]);
 CHECK(War_ProposeTreaty(&g)); CHECK(g.prov[PV_ES_CAT].owner==N_FRANCE); CHECK(!War_AtWar(&g, N_FRANCE, N_ESPAGNE));
 /* paris occupe : defaite */
 Game_Init(&g, 41, 1); War_StartWar(&g, N_ALLEMAGNE, N_FRANCE, false); War_Occupy(&g, PV_FR_IDF, N_ALLEMAGNE);
 g.pendingDilemma=-1; Game_EndTurn(&g); CHECK(g.state==GAME_LOST_CONQUEST);
 /* arme tactique et escalade */
 Game_Init(&g, 42, 1); g.commandPoints=3; g.nat[N_FRANCE].tech[T_NUC]=3; War_Declare(&g, N_ALGERIE);
 War_SpawnUnit(&g, N_ALGERIE, U_INF, PV_FR_PAC, 0); CHECK(Nuke_TacticalBlocked(&g, PV_FR_PAC)==NULL);
 CHECK(Nuke_Tactical(&g, PV_FR_PAC)); CHECK(g.tacticalUsed==1); CHECK(g.prov[PV_FR_PAC].fallout>0.2f);
 /* proliferation et contre-proliferation */
 Game_Init(&g, 43, 1); g.nat[N_TURQUIE].nukeStatus=1; g.nat[N_TURQUIE].nukeProgress=50; g.commandPoints=3; g.capacity[D_CYBER]=5;
 CHECK(Nuke_CounterBlocked(&g, N_TURQUIE, 1)==NULL); Nuke_Counter(&g, N_TURQUIE, 1); CHECK(g.nat[N_TURQUIE].nukeProgress<=50);
 g.nat[N_TURQUIE].nukeProgress=99.9f; for(int i=0;i<3;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} CHECK(Nuke_IsPower(&g, N_TURQUIE));
 /* generaux et technologies */
 Game_Init(&g, 44, 1); g.res[R_BUDGET]=100; CHECK(Mil_Promote(&g)); { int gi=-1; for(int i=0;i<MAX_GENERALS;i++) if(g.generals[i].active&&g.generals[i].owner==N_FRANCE&&g.generals[i].prov<0) gi=i;
   CHECK(gi>=0); CHECK(Mil_Assign(&g, gi, PV_FR_HDF)); CHECK(Mil_GeneralAt(&g, N_FRANCE, PV_FR_HDF)==gi); CHECK(!Mil_Assign(&g, gi, PV_DE_BW)); }
 Mil_SetResearch(&g, T_FEUX); Mil_SetResearchBudget(&g, 12); int lvl=g.nat[N_FRANCE].tech[T_FEUX];
 for(int i=0;i<8;i++){ g.pendingDilemma=-1; g.res[R_BUDGET]=100; Game_EndTurn(&g);} CHECK(g.nat[N_FRANCE].tech[T_FEUX]>lvl);
 /* embargo : interdit dans l'UE, possible hors UE */
 Game_Init(&g, 45, 1); g.commandPoints=3; CHECK(Eco_EmbargoBlocked(&g, N_ALLEMAGNE)!=NULL); CHECK(Eco_ToggleEmbargo(&g, N_BOREALE)); CHECK(!Eco_TradeAllowed(&g, N_BOREALE));
 /* cultures */
 Game_Init(&g, 46, 1); CHECK(Soc_MainCulture(&g.prov[PV_FR_COR])==C_CORSE || Soc_MainCulture(&g.prov[PV_FR_COR])==C_FRANCAISE);
 CHECK(Soc_NonAccepted(&g, PV_FR_IDF)<0.5f); CHECK(Soc_CulturePolicyBlocked(&g, CP_REPRESSION)!=NULL);
 /* sauvegarde */
 Game_Init(&g, 47, 1); for(int i=0;i<6;i++){ g.pendingDilemma=-1; Game_EndTurn(&g);} 
 { static Game h; CHECK(Game_Save(&g, "build/test.sav")); CHECK(Game_Load(&h, "build/test.sav")); CHECK(memcmp(&g,&h,sizeof g)==0); }
 printf("%s (%d echec(s))\n", fails?"TESTS EN ECHEC":"TOUS LES TESTS PASSENT", fails); return fails!=0;
}
