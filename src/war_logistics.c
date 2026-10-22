/*
 * BOUCLIER 2050 - ravitaillement, depots, encerclement, fortifications, brouillard de
 * guerre, groupes d'armees francais, missions navales et aeriennes.
 */
#include "war_internal.h"

/* ------------------------------------------------------------------ */
/* Ravitaillement, depots, encerclement                                */
/* ------------------------------------------------------------------ */

float War_SupplyLevel(const Game *g, int n, int p)
{
    return g->supply[n][p] / 100.0f;
}

void War_UpdateSupply(Game *g)
{
    for (int n = 0; n < N_COUNT; n++) {
        unsigned char *sup = g->supply[n];
        memset(sup, 0, PROV_COUNT);
        if (!g->nat[n].alive) continue;
        int dist[PROV_COUNT], queue[PROV_COUNT], head = 0, tail = 0;
        for (int p = 0; p < PROV_COUNT; p++) {
            dist[p] = -1;
            const Province *pr = &g->prov[p];
            bool src = (pr->core == n && pr->ctrl == n) || (pr->depot == n + 1 && War_Friendly(g, n, p));
            if (src) { dist[p] = 0; queue[tail++] = p; }
        }
        int cap = Capital(g, n);
        if (cap >= 0 && dist[cap] < 0) { dist[cap] = 0; queue[tail++] = cap; }
        while (head < tail) {
            int p = queue[head++];
            for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) {
                int q = PROVINCE_ADJ[p][k];
                if (dist[q] >= 0 || !War_Friendly(g, n, q) || Map_SeaLink(p, q)) continue;
                dist[q] = dist[p] + 1;
                queue[tail++] = q;
            }
        }
        bool port = CoastalBase(g, n) >= 0 && g->nat[n].blockade < 0.5f;
        for (int p = 0; p < PROV_COUNT; p++) {
            if (!War_Friendly(g, n, p)) continue;
            int lvl;
            if (dist[p] >= 0) lvl = 100 - 12 * (dist[p] > 2 ? dist[p] - 2 : 0);
            else lvl = port && PROVINCE_COASTAL[p] ? 50 : 0;     /* poche ravitaillee par la mer, sinon encerclee */
            sup[p] = (unsigned char)(lvl < 25 && dist[p] >= 0 ? 25 : lvl);
        }
    }
}

int DepotCount(const Game *g, int n)
{
    int c = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].depot == n + 1) c++;
    return c;
}

const char *War_DepotBlocked(const Game *g, int p)
{
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->prov[p].ctrl != N_FRANCE) return "Province non tenue par nos forces";
    if (g->prov[p].depot == N_FRANCE + 1) return "Dépôt déjà établi";
    if (DepotCount(g, N_FRANCE) >= 3) return "Trois dépôts au plus";
    if (g->res[R_BUDGET] < 4) return "Budget insuffisant (4 Md)";
    return NULL;
}

bool War_BuildDepot(Game *g, int p)
{
    if (War_DepotBlocked(g, p)) return false;
    g->res[R_BUDGET] -= 4;
    g->prov[p].depot = N_FRANCE + 1;
    War_UpdateSupply(g);
    Game_Log(g, EV_INFO, "Dépôt logistique établi en %s.", (PROVINCE_NAMES[p]));
    return true;
}

/* ------------------------------------------------------------------ */
/* Fortifications                                                      */
/* ------------------------------------------------------------------ */

float War_FortCost(const Game *g, int p)
{
    return 4 + 3.0f * g->prov[p].fort;
}

const char *War_FortBlocked(const Game *g, int p)
{
    const Province *pr = &g->prov[p];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (pr->ctrl != N_FRANCE) return "Province non tenue par nos forces";
    if (pr->fort >= 3) return "Fortifications au niveau maximal";
    if (pr->fortBuild > 0) return "Travaux en cours";
    if (g->res[R_BUDGET] < War_FortCost(g, p)) return "Budget insuffisant";
    return NULL;
}

bool War_Fortify(Game *g, int p)
{
    if (War_FortBlocked(g, p)) return false;
    g->res[R_BUDGET] -= War_FortCost(g, p);
    g->prov[p].fortBuild = 3;
    Game_Log(g, EV_INFO, "Travaux de fortification en %s (3 tours).", (PROVINCE_NAMES[p]));
    return true;
}

void Works(Game *g)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        Province *pr = &g->prov[p];
        if (pr->fortBuild > 0 && --pr->fortBuild == 0 && pr->fort < 3) {
            pr->fort++;
            if (pr->ctrl == N_FRANCE) Game_Log(g, EV_GOOD, "Fortifications de %s achevées (niveau %d).", (PROVINCE_NAMES[p]), pr->fort);
        }
    }
    /* les IA fortifient le point menace et installent un depot pres de leur offensive */
    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive || !War_AtWarAny(g, n)) continue;
        int t = na->aiThreat;
        if (t >= 0 && g->prov[t].ctrl == n && g->prov[t].fort < 2 && !g->prov[t].fortBuild && na->treasury > 5 && Randf(g) < 0.3f) {
            na->treasury -= 5;
            g->prov[t].fortBuild = 3;
        }
        int e = na->aiTarget;
        if (e >= 0 && na->treasury > 2) {
            int best = -1, bestCount = 0;
            for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[e][k] >= 0; k++) {
                int q = PROVINCE_ADJ[e][k];
                if (g->prov[q].ctrl != n) continue;
                int c = War_StackCount(g, n, q, DOM_LAND);
                if (c > bestCount) { bestCount = c; best = q; }
            }
            if (best >= 0 && g->supply[n][best] < 70 && g->prov[best].depot != n + 1) {
                for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].depot == n + 1) g->prov[p].depot = 0;
                g->prov[best].depot = (short)(n + 1);
                na->treasury -= 2;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Brouillard de guerre (vue francaise)                                */
/* ------------------------------------------------------------------ */

void Reveal(bool *v, int p, int radius)
{
    v[p] = true;
    if (radius <= 0) return;
    for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++) Reveal(v, PROVINCE_ADJ[p][k], radius - 1);
}

/* vue du joueur actif (la France, ou le commandant dont c'est le tour en hot-seat) */
void War_UpdateIntel(Game *g) { War_UpdateIntelFor(g, g->activePlayer); }

void War_UpdateIntelFor(Game *g, int n0)
{
    bool *v = g->visible;
    memset(v, 0, sizeof(g->visible));
    int radius = n0 == N_FRANCE && g->proj[P_CONSTELL].state == 2 ? 2 : 1;
    for (int p = 0; p < PROV_COUNT; p++)
        if (War_Friendly(g, n0, p)) Reveal(v, p, radius);
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->owner != n0) continue;
        UnitDomain d = UNIT_TYPES[u->type].domain;
        if (d == DOM_AIR) Reveal(v, u->prov, UNIT_TYPES[u->type].range);
        else if (d == DOM_SEA) Reveal(v, u->prov, 2);
    }
    for (int n = 1; n < N_COUNT && n0 == N_FRANCE; n++) {
        if (g->nat[n].recon <= 0) continue;
        for (int p = 0; p < PROV_COUNT; p++) if (g->prov[p].ctrl == n || g->prov[p].owner == n) v[p] = true;
    }
}

bool War_Visible(const Game *g, int p)
{
    return g->visible[p];
}

/* ------------------------------------------------------------------ */
/* Groupes d'armees francais                                           */
/* ------------------------------------------------------------------ */

void War_SetGroup(Game *g, int i, int grp)
{
    if (i < 0 || i >= MAX_UNITS || grp < 0 || grp > GROUP_COUNT) return;
    Unit *u = &g->units[i];
    if (!u->active || u->owner != N_FRANCE || UNIT_TYPES[u->type].domain != DOM_LAND) return;
    u->group = (unsigned char)grp;
}

void War_SetGroupOrder(Game *g, int grp, int mode, int target)
{
    if (grp < 1 || grp > GROUP_COUNT || mode < 0 || mode >= GM_COUNT) return;
    g->groups[grp - 1].mode = mode;
    g->groups[grp - 1].target = target;
}

void FrenchGroups(Game *g)
{
    static bool used[MAX_UNITS];
    for (int grp = 1; grp <= GROUP_COUNT; grp++) {
        int mode = g->groups[grp - 1].mode, e = g->groups[grp - 1].target;
        if (mode == GM_NONE || e <= 0 || e >= N_COUNT || !g->nat[e].alive) continue;
        memset(used, 0, sizeof(used));
        /* les unites deja en mouvement (ordres manuels) ne sont pas reaffectees */
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (!u->active || u->owner != N_FRANCE || u->group != grp || u->target >= 0) used[i] = true;
        }
        groupFilter = grp;
        if (mode == GM_ATTACK && War_AtWar(g, N_FRANCE, e)) {
            int best = -1;
            float bestScore = 0;
            for (int p = 0; p < PROV_COUNT; p++) {
                if (g->prov[p].ctrl != e || !CanAttack(g, N_FRANCE, p)) continue;
                float att = StagedAttack(g, N_FRANCE, p, used, true);
                if (att <= 0) continue;
                float sc = att / DefensePower(g, p, N_FRANCE) * sqrtf(TargetValue(g, N_FRANCE, p));
                if (sc > bestScore) { bestScore = sc; best = p; }
            }
            if (best >= 0) {
                float fa, fd;
                AirFactors(g, best, N_FRANCE, &fa, &fd);
                float odds = Odds(StagedAttack(g, N_FRANCE, best, used, false) * fa, DefensePower(g, best, N_FRANCE) * fd);
                if (odds >= 0.6f) LaunchAttack(g, N_FRANCE, best, used);
                else Stage(g, N_FRANCE, best, used);
            }
        }
        /* tenir le front (et suivre l'offensive pour les unites restantes) */
        bool front[PROV_COUNT] = { false };
        int count[PROV_COUNT] = { 0 }, nfront = 0;
        for (int p = 0; p < PROV_COUNT; p++) {
            if (!War_Friendly(g, N_FRANCE, p)) continue;
            for (int k = 0; k < MAX_ADJ && PROVINCE_ADJ[p][k] >= 0; k++)
                if (g->prov[PROVINCE_ADJ[p][k]].ctrl == e) { front[p] = true; nfront++; break; }
        }
        if (!nfront) { groupFilter = -1; continue; }
        int members = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (u->active && u->owner == N_FRANCE && u->group == grp) { members++; if (front[u->prov]) count[u->prov]++; }
        }
        int per = members / nfront + 1;
        bool needy[PROV_COUNT] = { false };
        for (int p = 0; p < PROV_COUNT; p++) {
            int want = per + (FrontThreat(g, N_FRANCE, p, NULL) > 0 ? 2 : 0);
            if (front[p] && count[p] < want) needy[p] = true;
        }
        int dist[PROV_COUNT];
        FriendlyBfs(g, N_FRANCE, needy, dist);
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (used[i] || !u->active || u->training || UNIT_TYPES[u->type].domain != DOM_LAND) continue;
            if (front[u->prov] && count[u->prov] <= per + 2) continue;
            StepToward(g, u, dist);
            if (u->target >= 0 && front[u->prov]) count[u->prov]--;
        }
        groupFilter = -1;
    }
}

/* ------------------------------------------------------------------ */
/* Missions navales                                                    */
/* ------------------------------------------------------------------ */

const char *War_MissionBlocked(const Game *g, int i, int m)
{
    const Unit *u = &g->units[i];
    if (u->active && u->owner == N_FRANCE && UNIT_TYPES[u->type].domain == DOM_AIR) {
        if (m < 0 || m >= AM_COUNT) return "Mission inconnue";
        if (m == AM_SUPERIORITY && u->type == U_MALE) return "Les drones ne mènent pas de combat aérien";
        return NULL;
    }
    if (!u->active || u->owner != N_FRANCE || UNIT_TYPES[u->type].domain != DOM_SEA) return "Unité navale requise";
    (void)g;
    switch (m) {
    case NM_STRIKE: return u->type == U_PA || u->type == U_FREGATE || u->type == U_SNA ? NULL : "Pas de missiles de croisière";
    case NM_RAID:   return u->type == U_SNA || u->type == U_USV || u->type == U_FREGATE ? NULL : "Bâtiment inadapté";
    case NM_ESCORT: return u->type == U_FREGATE || u->type == U_USV || u->type == U_PA ? NULL : "Bâtiment inadapté";
    default:        return NULL;
    }
}

bool War_SetMission(Game *g, int i, int m)
{
    if (m < 0 || m >= NM_COUNT || War_MissionBlocked(g, i, m)) return false;
    g->units[i].mission = (unsigned char)m;
    return true;
}

void NavalMissions(Game *g)
{
    /* guerre au commerce */
    for (int n = 0; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        na->raid = 0;
        if (!na->alive || !War_AtWarAny(g, n)) continue;
        float r = 0, e = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &g->units[i];
            if (!u->active || u->training || UNIT_TYPES[u->type].domain != DOM_SEA) continue;
            float v = UNIT_TYPES[u->type].naval * Readiness(u);
            if (u->mission == NM_RAID && War_AtWar(g, u->owner, n)) r += v;
            else if (u->owner == n) e += u->mission == NM_ESCORT ? v : u->mission == NM_PATROL ? v * 0.3f : 0;
        }
        if (r <= 0) continue;
        na->raid = Clampf(0.5f * r / (r + e + 4), 0, 0.5f);
        for (int i = 0; i < MAX_UNITS; i++) {
            Unit *u = &g->units[i];
            if (u->active && UNIT_TYPES[u->type].domain == DOM_SEA && u->mission == NM_RAID && War_AtWar(g, u->owner, n)) u->str -= 0.02f * e / (r + e + 1);
        }
        if (n == N_FRANCE && na->raid > 0.1f)
            Game_Log(g, EV_BAD, "Guerre au commerce : nos convois sont attaqués (-%.0f%% d'échanges).", na->raid * 100);
    }
    /* frappes a terre */
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &g->units[i];
        if (!u->active || u->training || u->mission != NM_STRIKE || UNIT_TYPES[u->type].domain != DOM_SEA) continue;
        int best = -1, bestCount = 0;
        for (int e = 0; e < PROV_COUNT; e++) {
            if (!PROVINCE_COASTAL[e] || !Hostile(g, u->owner, e) || Distance(u->prov, e, 2, true) < 0) continue;
            int c = 0;
            for (int j = 0; j < MAX_UNITS; j++)
                if (g->units[j].active && g->units[j].prov == e && War_AtWar(g, u->owner, g->units[j].owner)) c++;
            if (c > bestCount) { bestCount = c; best = e; }
        }
        if (best < 0) continue;
        float hit = (u->type == U_PA ? 0.12f : 0.06f) * Readiness(u);
        for (int j = 0; j < MAX_UNITS; j++) {
            Unit *v = &g->units[j];
            if (v->active && v->prov == best && War_AtWar(g, u->owner, v->owner)) v->org = Clampf(v->org - hit, 0, 1);
        }
        g->prov[best].damage = Clampf(g->prov[best].damage + 0.02f, 0, 1);
        if (u->owner == N_FRANCE) Game_Log(g, EV_WAR, "%s : frappes navales sur %s.", (UNIT_TYPES[u->type].name), (PROVINCE_NAMES[best]));
    }
}

/* missions navales choisies par l'IA */
/* bombardement strategique : chaque escadre frappe la province ennemie la plus industrielle a sa portee */
void StrategicBombing(Game *g)
{
    int frRaids = 0, onFrance = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        const UnitDef *d = &UNIT_TYPES[u->type];
        if (!u->active || u->training || d->domain != DOM_AIR || u->mission != AM_BOMBING || !War_AtWarAny(g, u->owner)) continue;
        int best = -1;
        float bestV = 0;
        for (int p = 0; p < PROV_COUNT; p++) {
            const Province *pr = &g->prov[p];
            if (!War_AtWar(g, u->owner, pr->ctrl) || Distance(u->prov, p, d->range, false) < 0) continue;
            float v = pr->industry * pr->prosperity + pr->pop * 0.1f;
            if (v > bestV) { bestV = v; best = p; }
        }
        if (best < 0) continue;
        int e = g->prov[best].ctrl;
        float aa = AntiAir(g, e, best), sup = AirSuperiority(g, e, u->owner, best);
        float hit = d->atk * Readiness(u) * Mil_TechBonus(g, u->owner, u->type) / (1 + 0.15f * aa + 0.3f * sup);
        Province *pr = &g->prov[best];
        pr->prosperity = Clampf(pr->prosperity - 0.012f * hit, 0.4f, 2.5f);
        pr->damage = Clampf(pr->damage + 0.008f * hit, 0, 1);
        if (e == N_FRANCE) { AddRes(g, R_COHESION, -0.15f * hit); onFrance++; }
        u->str = Clampf(u->str - 0.02f - 0.03f * (aa / (aa + 6) + sup / (sup + 3)), 0, 1);
        u->org = Clampf(u->org - 0.1f, 0, 1);
        if (u->owner == N_FRANCE) frRaids++;
    }
    if (frRaids) Game_Log(g, EV_WAR, "Bombardements stratégiques : %d raid(s) de nos escadres sur l'industrie ennemie.", frRaids);
    if (onFrance) Game_Log(g, EV_BAD, "Bombardements ennemis sur notre territoire : %d raid(s).", onFrance);
}

void AiMissions(Game *g, int n)
{
    const Nation *na = &g->nat[n];
    bool war = War_AtWarAny(g, n);
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g->units[i];
        if (!u->active || u->owner != n) continue;
        if (UNIT_TYPES[u->type].domain == DOM_AIR) {
            /* en guerre, une escadre sur trois prend l'air pour la superiorite, une sur quatre bombarde si l'IA est forte */
            int m = AM_SUPPORT;
            if (war && u->type == U_CHASSE && i % 3 == 0) m = AM_SUPERIORITY;
            else if (war && na->army > 8 && i % 4 == 1) m = AM_BOMBING;
            u->mission = (unsigned char)m;
            continue;
        }
        if (UNIT_TYPES[u->type].domain != DOM_SEA) continue;
        int m = NM_PATROL;
        if (war) {
            if (u->type == U_SNA || u->type == U_USV) m = NM_RAID;
            else if (u->type == U_PA && na->aiTarget >= 0 && PROVINCE_COASTAL[na->aiTarget]) m = NM_STRIKE;
            else if (u->type == U_FREGATE && na->raid > 0.1f) m = NM_ESCORT;
        }
        u->mission = (unsigned char)m;
    }
}

void War_Turn(Game *g)
{
    War_UpdateSupply(g);
    if (g->aggressionMemory > 0) g->aggressionMemory -= 0.6f;
    for (int a = 0; a < N_COUNT; a++)
        for (int b = 0; b < N_COUNT; b++)
            if (g->truce[a][b] > 0) g->truce[a][b]--;
    Alliances(g);

    for (int n = 1; n < N_COUNT; n++) {
        Nation *na = &g->nat[n];
        if (!na->alive) continue;

        float hab = 0, pop = 0;
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == n) { hab += g->prov[p].hab * g->prov[p].pop; pop += g->prov[p].pop; }
        hab = pop > 0 ? hab / pop : 50;
        if (!na->failed && hab < 35) {
            na->failed = true;
            na->regime = RG_JUNTE;
            na->eu = false;
            Game_Log(g, EV_CLIMATE, "Effondrement : %s devient un État failli, une junte prend le pouvoir.", (NATIONS[n].name));
        }

        if (!g->war[N_FRANCE][n]) {
            na->relation += (RelationTarget(g, n) - na->relation) * 0.08f + (Randf(g) - 0.5f) * 2;
            na->relation = Clampf(na->relation, -100, 100);
            if (na->trade && na->relation < 10) {
                na->trade = false;
                Game_Log(g, EV_BAD, "%s rompt l'accord commercial.", (NATIONS[n].name));
            }
        }

        float aggro = NATIONS[n].aggressiveness * DiffAggro(g);
        float deter = 1 - g->credibility / 100.0f * (Nuke_IsPower(g, n) ? 0.4f : 0.8f);

        bool human = g->human[n];
        if (na->offerCooldown > 0) na->offerCooldown--;
        if (!human) AiProposals(g, n);
        if (human && g->war[N_FRANCE][n] && na->peaceAsk == N_FRANCE + 1 && g->pendingDilemma < 0) AiPeaceOfferToFrance(g, n);
        if (human) na->peaceAsk = 0;

        /* guerre contre la France */
        if (human) {
            /* un commandant decide lui-meme de la guerre et de la paix */
        } else if (g->war[N_FRANCE][n]) {
            int wt = War_Turns(g, N_FRANCE, n);
            if (wt > 0 && wt % 8 == 0) na->deterred = Nuke_Deters(g, n);
            float s = War_Score(g, N_FRANCE, n);
            if (na->peaceCooldown > 0) na->peaceCooldown--;
            bool wants = (fabsf(s) >= 25 && wt >= 4) || na->army < War_ArmyMax(g, n) * 0.25f || ExhaustionVs(g, n, N_FRANCE) > 20
                      || (wt > 16 && Randf(g) < 0.1f);
            if (wants && na->peaceCooldown == 0 && g->pendingDilemma < 0 && Randf(g) < 0.35f) AiPeaceOfferToFrance(g, n);
        } else if (g->truce[N_FRANCE][n] == 0 && !g->ally[N_FRANCE][n] && War_Adjacent(g, n, N_FRANCE)) {
            if (g->aggressionMemory > 35 && na->relation < 0 && Randf(g) < (0.03f + g->aggressionMemory * 0.001f) * deter) {
                War_StartWar(g, n, N_FRANCE, true);
                Game_Log(g, EV_WAR, "Face à l'expansionnisme français, %s nous déclare la guerre !", (NATIONS[n].name));
            } else if (na->relation < -55 && Coalition(g, n) > Coalition(g, N_FRANCE) * 1.2f && Randf(g) < aggro * 0.15f * deter) {
                War_StartWar(g, n, N_FRANCE, true);
                Game_Log(g, EV_WAR, "%s déclare la guerre à la France !", (NATIONS[n].name));
            }
        }

        /* guerres entre nations */
        for (int t = 1; t < N_COUNT; t++) {
            if (t == n || !g->war[n][t] || t < n || human || g->human[t]) continue;
            int wt = War_Turns(g, n, t);
            float s = War_Score(g, n, t);
            float tired = (ExhaustionVs(g, n, t) + ExhaustionVs(g, t, n)) / 80.0f + fabsf(s) / 150.0f;
            if (fabsf(s) > 60 || (wt >= 4 && Randf(g) < Clampf(tired, 0, 0.5f) * 0.4f) || wt > 24) AiPeace(g, n, t);
        }
        if (!human && g->turn > 8 && Randf(g) < aggro * 0.015f * (na->failed ? 2 : 1)) {
            int best = -1;
            float bestPow = 1e9f;
            for (int t = 1; t < N_COUNT; t++) {
                const Nation *tg = &g->nat[t];
                if (t == n || !tg->alive || g->war[n][t] || g->truce[n][t] || g->ally[n][t] || (tg->eu && na->eu)) continue;
                if (g->airel[n][t] > -15 || !War_Adjacent(g, n, t)) continue;
                if (Nuke_IsPower(g, t) && !Nuke_IsPower(g, n)) continue;
                if (Coalition(g, n) < Coalition(g, t) * 1.5f) continue;
                if (tg->eu && g->umbrella && Randf(g) < g->credibility / 100.0f) continue;
                if (tg->nato && !na->nato && Randf(g) < g->natoCohesion / 100.0f) continue;   /* dissuasion de l'OTAN */
                if (Coalition(g, t) < bestPow) { bestPow = Coalition(g, t); best = t; }
            }
            if (best >= 0) {
                Game_Log(g, EV_WAR, "%s envahit %s !", (NATIONS[n].name), (NATIONS[best].name));
                War_StartWar(g, n, best, true);
            }
        }

        if (!human) {
            AiRecruit(g, n);
            AiOrders(g, n);
        }
        AiMissions(g, n);
    }

    FrenchGroups(g);
    Movements(g);
    War_UpdateSupply(g);
    NavalWarfare(g);
    NavalMissions(g);
    StrategicBombing(g);
    Battles(g);
    Supply(g);
    Works(g);
    Capitulations(g);
    War_UpdateSupply(g);
    War_UpdateIntel(g);
    UpdateArmyPower(g);
}
