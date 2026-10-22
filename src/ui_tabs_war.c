/*
 * BOUCLIER 2050 - onglets militaires et diplomatiques : province, armee, etat-major,
 * nations, operations, negociations.
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Panneau droit                                                       */
/* ------------------------------------------------------------------ */

void StatLine(float x, float y, const char *label, const char *value, Color c)
{
    Text(label, x, y, 10, C_DIM);
    Text(value, x + 120, y, 10, c);
}

const char *DECLARE_DESC =
    "Ouvre les hostilités (1 PC). Coûte de la cohésion selon le régime, de l'influence et de la réputation. "
    "Attaquer un membre de l'UE fait exclure la France et peut déclencher une coalition.";

void DrawProvinceTab(void)
{
    int p = selProvince;
    const Province *pr = &game.prov[p];
    const Nation *own = &game.nat[pr->owner];
    char buf[260];
    float x = RP_X + 10, y = RP_Y + 4;

    Text(Tr(PROVINCE_NAMES[p]), x, y, 20, C_TEXT);
    y += 24;
    DrawRectangle((int)x, (int)y + 1, 10, 10, NationColor(pr->owner));
    snprintf(buf, sizeof(buf), Tr("%s  -  %s%s%s"), Tr(NATIONS[pr->owner].name), Tr(REGIMES[own->regime].name),
             own->failed ? "  -  ÉTAT FAILLI" : "", Nuke_IsPower(&game, pr->owner) ? "  -  puissance nucléaire" : "");
    Text(buf, x + 16, y, 10, C_TEXT);
    y += 14;
    if (pr->ctrl != pr->owner) {
        snprintf(buf, sizeof(buf), Tr("OCCUPÉE par %s : la province ne produit plus rien pour %s."), Tr(NATIONS[pr->ctrl].name), Tr(NATIONS[pr->owner].name));
        Text(buf, x, y, 10, C_WAR);
        y += 14;
    }
    if (pr->core != pr->owner) {
        snprintf(buf, sizeof(buf), Tr("Territoire annexé (origine : %s). Intégration %.0f%%."), Tr(NATIONS[pr->core].name), pr->integration);
        Text(buf, x, y, 10, C_WARN);
        y += 14;
    }
    if (pr->revolt) {
        snprintf(buf, sizeof(buf), Tr("EN RÉVOLTE depuis %d tour(s) : production effondrée, recettes nulles."), pr->revolt);
        Text(buf, x, y, 10, (Color){ 220, 120, 240, 255 });
        y += 14;
    }
    y += 4;

    float c2 = x + 275;
    snprintf(buf, sizeof(buf), Tr("%.2f M"), pr->pop);                        StatLine(x, y, "Population", buf, C_TEXT);
    snprintf(buf, sizeof(buf), Tr("%.0f / 100"), pr->hab);                     StatLine(c2, y, "Habitabilité", buf, LevelColor(pr->hab));
    y += 13;
    snprintf(buf, sizeof(buf), "%.0f%%", pr->literacy * 100);              StatLine(x, y, "Alphabétisation", buf, C_TEXT);
    snprintf(buf, sizeof(buf), Tr("x%.2f"), pr->prosperity);                   StatLine(c2, y, "Prospérité", buf, C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), Tr("%.0f%%  (emplois %.0f%%)"), pr->unemployment * 100, pr->jobs * 100);
    StatLine(x, y, "Chômage", buf, pr->unemployment > 0.1f ? C_BAD : C_TEXT);
    snprintf(buf, sizeof(buf), "%.1f%s", pr->industry, pr->jobs > 1 ? " (main-d'oeuvre rare)" : "");
    StatLine(c2, y, "Usines", buf, pr->jobs > 1 ? C_WARN : C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), Tr("-%.0f k / +%.0f k"), pr->emig * 1000, pr->immig * 1000); StatLine(x, y, "Émigr. / immigr.", buf, C_TEXT);
    snprintf(buf, sizeof(buf), Tr("%.1f Md/tour"), Eco_ProvinceTax(&game, p)); StatLine(c2, y, "Recettes fiscales", buf, C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), Tr("%.0f / %.0f / %.0f / %.0f"), PROVINCES[p].food * pr->hab / 100, PROVINCES[p].water * pr->hab / 100,
             PROVINCES[p].energy, PROVINCE_MINERALS[p]);
    StatLine(x, y, "Alim/eau/énerg/min", buf, C_TEXT);
    static const char *terrain[3] = { "plaine", "collines", "montagne" };
    snprintf(buf, sizeof(buf), "%s%s%s", terrain[PROVINCE_TERRAIN[p]], PROVINCE_COASTAL[p] ? ", côtière" : "", pr->fallout > 0.05f ? ", contaminé" : "");
    StatLine(c2, y, "Terrain", buf, pr->fallout > 0.05f ? C_BAD : C_TEXT);
    y += 13;
    {
        int l2 = 0;
        char res[200] = "";
        for (int r = 0; r < SR_COUNT; r++) {
            float d = Strat_Deposit(p, r) + (r == SR_PUCES ? pr->fabs : 0);
            if (d > 0) l2 += snprintf(res + l2, sizeof(res) - l2, "%s%s %.1f", l2 ? ", " : "", Tr(STRATS[r].name), d);
        }
        if (l2 && pr->mineClosed) l2 += snprintf(res + l2, sizeof(res) - l2, "%s", Tr(" (fermé)"));
        StatLine(x, y, "Ressources", l2 ? res : "aucune", l2 ? C_ACCENT : C_DIM);
        if (PROVINCE_SEA_EXPOSURE[p] > 0) {
            snprintf(buf, sizeof(buf), Tr("%.0f%% (menacé %.0f%%)"), pr->flooded * 100, PROVINCE_SEA_EXPOSURE[p] * 100);
            StatLine(c2, y, "Terres submergées", buf, pr->flooded > 0.05f ? C_BAD : C_WARN);
        }
        y += 16;
    }

    /* cultures et religions */
    int order[CULT_COUNT];
    for (int c = 0; c < CULT_COUNT; c++) order[c] = c;
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < CULT_COUNT; j++)
            if (pr->culture[order[j]] > pr->culture[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    int len = snprintf(buf, sizeof(buf), "%s", Tr("Cultures :"));
    for (int i = 0; i < 3 && pr->culture[order[i]] >= 0.01f; i++)
        len += snprintf(buf + len, sizeof(buf) - len, Tr("  %s %.0f%%%s"), Tr(CULTURE_NAMES[order[i]]), pr->culture[order[i]] * 100,
                        Soc_Accepted(&game, pr->owner, order[i]) ? "" : "*");
    Text(buf, x, y, 10, C_TEXT);
    Tooltip((Rectangle){ x, y, RP_W - 20, 12 }, "* culture non acceptée par le propriétaire : minorité plus remuante, assimilée selon "
                                               "la politique culturelle, et revendiquée par sa patrie si elle devient majoritaire.");
    y += 13;
    len = snprintf(buf, sizeof(buf), "%s", Tr("Religions :"));
    for (int r = 0; r < REL_COUNT; r++)
        if (pr->religion[r] >= 0.05f) len += snprintf(buf + len, sizeof(buf) - len, "  %s %.0f%%", Tr(RELIGION_NAMES[r]), pr->religion[r] * 100);
    Text(buf, x, y, 10, C_DIM);
    y += 17;

    Text("STRATES", x, y, 10, C_DIM);
    Text("part", x + 110, y, 10, C_DIM);
    Text("militance", x + 150, y, 10, C_DIM);
    Text("conscience", x + 290, y, 10, C_DIM);
    Text("courant dominant", x + 420, y, 10, C_DIM);
    y += 13;
    for (int s = 0; s < S_COUNT; s++) {
        Text(Tr(STRATA_NAMES[s]), x, y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f%%", pr->share[s] * 100);
        Text(buf, x + 110, y, 10, C_TEXT);
        Bar((Rectangle){ x + 150, y + 1, 100, 8 }, pr->mil[s], 10, MilColor(pr->mil[s]));
        snprintf(buf, sizeof(buf), "%.1f", pr->mil[s]);
        Text(buf, x + 255, y, 10, C_TEXT);
        Bar((Rectangle){ x + 290, y + 1, 100, 8 }, pr->con[s], 10, C_EVENT);
        snprintf(buf, sizeof(buf), "%.1f", pr->con[s]);
        Text(buf, x + 395, y, 10, C_TEXT);
        int top = 0;
        for (int i = 1; i < I_COUNT; i++) if (pr->ideo[s][i] > pr->ideo[s][top]) top = i;
        snprintf(buf, sizeof(buf), "%s %.0f%%", Tr(IDEOLOGY_NAMES[top]), pr->ideo[s][top] * 100);
        Text(buf, x + 420, y, 10, IDEOLOGY_COLORS[top]);
        y += 13;
    }
    snprintf(buf, sizeof(buf), Tr("Seuil de révolte : %.1f (baisse avec la conscience politique)"), Soc_RevoltThreshold(pr));
    Text(buf, x, y + 2, 10, C_DIM);
    y += 16;
    if (War_Friendly(&game, N_FRANCE, p)) {
        float sl = War_SupplyLevel(&game, N_FRANCE, p);
        snprintf(buf, sizeof(buf), Tr("Ravitaillement %.0f%%%s - fortifications %d%s%s"), sl * 100, sl < 0.25f ? " (ENCERCLÉE)" : "", pr->fort,
                 pr->fortBuild ? " (travaux)" : "", pr->depot == N_FRANCE + 1 ? " - dépôt logistique" : "");
        Text(buf, x, y, 10, sl < 0.25f ? C_BAD : C_DIM);
        if (pr->ctrl == N_FRANCE) {
            snprintf(buf, sizeof(buf), Tr("Fortifier (%.0f Md)"), War_FortCost(&game, p));
            if (ActionButton((Rectangle){ x + 330, y - 3, 105, 16 }, buf, War_FortBlocked(&game, p),
                             "+1 niveau de fortification (3 au plus, 3 tours de travaux) : +15% de défense et une garnison renforcée. "
                             "Une fortification prise perd un niveau."))
                Do(CMD_FORTIFY, p, 0, 0, 0);
            if (ActionButton((Rectangle){ x + 440, y - 3, 100, 16 }, "Dépôt (4 Md)", War_DepotBlocked(&game, p),
                             "Dépôt logistique (3 au plus) : point de ravitaillement avancé, même en territoire conquis. Sans "
                             "ravitaillement, une poche encerclée perd organisation et effectifs."))
                Do(CMD_DEPOT, p, 0, 0, 0);
        }
    } else if (!War_Visible(&game, p)) {
        Text("Hors de vue : forces ennemies inconnues (reconnaissance satellitaire, aviation, flottes).", x, y, 10, C_WARN);
    }
    y += 16;

    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, VH - 60 - y }, C_PANEL2);
    y += 8;
    if (pr->owner == N_FRANCE && pr->ctrl == N_FRANCE) {
        Text("PROVINCE FRANÇAISE", x, y, 10, C_ACCENT);
        y += 16;
        if (pr->factoryBuild > 0) {
            snprintf(buf, sizeof(buf), Tr("Usine en construction : %d tour(s)"), pr->factoryBuild);
            Text(buf, x, y + 6, 10, C_WARN);
        } else {
            snprintf(buf, sizeof(buf), Tr("Construire une usine (%.0f Md, 4 tours)"), Eco_FactoryCost(&game));
            if (ActionButton((Rectangle){ x, y, 300, 24 }, buf, Eco_FactoryBlocked(&game, p),
                             "+1 usine : 2 biens industriels par tour (consomme énergie et minerais) et environ 0,4 M d'emplois. "
                             "Les biens satisfont les classes moyennes et les élites, équipent l'armée et soutiennent la prospérité. "
                             "Une usine sans main-d'oeuvre tourne au ralenti."))
                Do(CMD_FACTORY, p, 0, 0, 0);
        }
        if (pr->fabBuild > 0) {
            snprintf(buf, sizeof(buf), Tr("Fonderie en construction : %d tour(s)"), pr->fabBuild);
            Text(buf, x + 310, y + 6, 10, C_WARN);
        } else if (ActionButton((Rectangle){ x + 310, y, 220, 24 }, "Fonderie de puces (15 Md)", Strat_FabBlocked(&game, p),
                                "Semi-conducteurs : +0,6 lot par tour (6 tours de chantier). Il faut un tissu industriel (3 usines) "
                                "et de l'énergie."))
            Do(CMD_FAB, p, 0, 0, 0);
        y += 30;
        if (pr->mineClosed) {
            if (ActionButton((Rectangle){ x, y, 300, 24 }, "Rouvrir les mines (10 Md)", Strat_MineBlocked(&game, p),
                             "Exploite le gisement de la province : souveraineté accrue, mais les riverains protestent."))
                Do(CMD_MINE, p, 0, 0, 0);
            y += 30;
        }
        TextWrap("Clic droit sur la carte pour déplacer les unités sélectionnées. Recrutement : onglet ARMÉE ; généraux : ÉT.-MAJOR.",
                 x, y, RP_W - 30, 10, C_DIM);
        return;
    }

    int foe = pr->ctrl != N_FRANCE ? pr->ctrl : pr->owner;
    int enemyUnits = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (game.units[i].active && game.units[i].prov == p && War_AtWar(&game, N_FRANCE, game.units[i].owner)) enemyUnits++;

    if (War_AtWar(&game, N_FRANCE, foe)) {
        Text("FRONT", x, y, 20, C_WAR);
        snprintf(buf, sizeof(buf), Tr("Score de guerre contre %s : %+.0f"), Tr(NATIONS[foe].name), War_Score(&game, N_FRANCE, foe));
        Text(buf, x + 90, y + 6, 10, War_Score(&game, N_FRANCE, foe) >= 0 ? C_GOOD : C_BAD);
        y += 26;
        int ordered = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].target == p) ordered++;
        if (War_Visible(&game, p)) snprintf(buf, sizeof(buf), Tr("Unités ennemies sur place : %d.  Nos unités engagées : %d."), enemyUnits, ordered);
        else snprintf(buf, sizeof(buf), Tr("Unités ennemies : inconnues (hors de vue).  Nos unités engagées : %d."), ordered);
        Text(buf, x, y, 10, C_TEXT);
        y += 14;
        if (War_Visible(&game, p))
            snprintf(buf, sizeof(buf), Tr("Chances de percée estimées : %.0f%% %s"), War_BattleOdds(&game, p, N_FRANCE) * 100,
                     ordered ? "(unités engagées)" : "(toutes les divisions à portée)");
        else snprintf(buf, sizeof(buf), "%s", Tr("Chances de percée : inconnues sans renseignement (assaut à l'aveugle -10%%)"));
        Text(buf, x, y, 10, C_WARN);
        y += 14;
        TextWrap("Sélectionnez des divisions (clic gauche) puis clic droit ici. Blindés et mécanisées avancent de 2 régions par tour. "
                 "Artillerie, missiles et aviation appuient à distance ; la DCA et la guerre électronique protègent.", x, y, RP_W - 30, 10, C_DIM);
        y += 44;
        if (strikeArmed && strikeProvince == p) {
            if (Button((Rectangle){ x, y, 260, 26 }, "CONFIRMER LA FRAPPE STRATÉGIQUE", true, 10)) {
                Do(CMD_STRIKE, p, 0, 0, 0);
                strikeArmed = false;
            }
            if (Button((Rectangle){ x + 270, y, 120, 26 }, "Annuler", true, 10)) strikeArmed = false;
        } else {
            if (ActionButton((Rectangle){ x, y, 260, 26 }, "Frappe tactique", Nuke_TacticalBlocked(&game, p),
                             "Arme nucléaire de théâtre (technologie nucléaire 3, 1 PC) : les unités ennemies présentes perdent 75% "
                             "de leurs effectifs, retombées sur la province. Réprobation internationale ; contre une puissance "
                             "nucléaire, risque de riposte tactique, voire d'escalade stratégique."))
                Do(CMD_TACTICAL, p, 0, 0, 0);
            if (ActionButton((Rectangle){ x + 270, y, 260, 26 }, "Frappe stratégique", Nuke_StrikeBlocked(&game, p),
                             "Dévaste la province et contraint un ennemi non nucléaire à capituler sans conditions. La France devient "
                             "un paria (relations, influence, UE). Contre une puissance nucléaire : riposte et fin de partie.")) {
                strikeArmed = true;
                strikeProvince = p;
            }
        }
        return;
    }

    snprintf(buf, sizeof(buf), Tr("Relation de %s avec la France : %+.0f"), Tr(NATIONS[pr->owner].name), own->relation);
    Text(buf, x, y, 10, own->relation < 0 ? C_BAD : C_GOOD);
    y += 18;
    if (ActionButton((Rectangle){ x, y, 260, 26 }, "Ultimatum : exiger la cession", War_UltimatumBlocked(&game, p),
                     "Exige la province sous menace de guerre (1 PC). Réussit si notre armée et nos alliés pèsent 2,5 fois plus "
                     "(une dissuasion renforcée aide). Échec : relation -25, Influence -5."))
        Do(CMD_ULTIMATUM, p, 0, 0, 0);
    if (ActionButton((Rectangle){ x + 270, y, 260, 26 }, "Déclarer la guerre", War_DeclareBlocked(&game, pr->owner), DECLARE_DESC))
        Do(CMD_DECLARE_WAR, pr->owner, 0, 0, 0);
    y += 36;
    snprintf(buf, sizeof(buf), Tr("%s : puissance %.1f (qualité %.2f), %d unités. La nôtre : %.1f (qualité %.2f)."),
             Tr(NATIONS[pr->owner].name), own->army, War_Quality(&game, pr->owner), War_UnitCount(&game, pr->owner),
             game.nat[N_FRANCE].army, War_Quality(&game, N_FRANCE));
    TextWrap(buf, x, y, RP_W - 30, 10, C_DIM);
}

const char *DOMAIN_LABELS[3] = { "terre", "air", "mer" };

const char *StratCostText(int type)
{
    static char buf[120];
    int len = 0;
    buf[0] = 0;
    for (int r = 0; r < SR_COUNT; r++)
        if (UNIT_STRAT[type][r])
            len += snprintf(buf + len, sizeof(buf) - len, "%s%d %s", len ? ", " : "\nRessources : ", UNIT_STRAT[type][r], Tr(STRATS[r].name));
    return buf;
}

void DrawArmyTab(void)
{
    char buf[260];
    float x = RP_X + 10, y = RP_Y + 4;
    snprintf(buf, sizeof(buf), Tr("UNITÉS : %d  -  mobilisation %.1f / %.0f  -  qualité %.2f  -  entretien %.1f Md/tour"),
             War_UnitCount(&game, N_FRANCE), War_UnitLoad(&game, N_FRANCE), War_ArmyMax(&game, N_FRANCE),
             War_Quality(&game, N_FRANCE), War_ArmyUpkeep(&game));
    Text(buf, x, y, 10, C_TEXT);
    y += 14;
    {
        const Nation *fr = &game.nat[N_FRANCE];
        snprintf(buf, sizeof(buf), Tr("Réserve de main-d'oeuvre : %.0f 000 hommes sur %.0f 000 (+%.0f 000 par tour)"),
                 fr->manpower * MEN_PER_POINT / 1000, War_ManpowerMax(&game, N_FRANCE) * MEN_PER_POINT / 1000,
                 War_ManpowerGain(&game, N_FRANCE) * MEN_PER_POINT / 1000);
        Rectangle mr = { x, y, RP_W - 20, 12 };
        Text(buf, x, y, 10, fr->manpower < War_ManpowerMax(&game, N_FRANCE) * 0.2f ? C_BAD : C_DIM);
        Tooltip(mr, "Les recrues viennent de la population : chaque division consomme la réserve à sa création, et les "
                    "renforts qui comblent les pertes y puisent aussi. La réserve se reconstitue chaque tour, plus vite en "
                    "guerre. Les unités gagnent en expérience au combat (jusqu'à +20 %) ; les renforts la diluent.");
        y += 14;
    }
    snprintf(buf, sizeof(buf), Tr("Recruter en : %s  (%d/%d divisions, %d/%d escadres, %d/%d navires)"), Tr(PROVINCE_NAMES[selProvince]),
             War_StackCount(&game, N_FRANCE, selProvince, DOM_LAND), STACK_LAND, War_StackCount(&game, N_FRANCE, selProvince, DOM_AIR),
             STACK_AIR, War_StackCount(&game, N_FRANCE, selProvince, DOM_SEA), STACK_SEA);
    Text(buf, x, y, 10, C_DIM);
    y += 14;
    float bw = (RP_W - 20 - 3 * 4) / 4.0f;
    for (int t = 0; t < U_TYPE_COUNT; t++) {
        const UnitDef *d = &UNIT_TYPES[t];
        snprintf(buf, sizeof(buf), Tr("%s %.0f Md"), Tr(d->shortName), War_RecruitCost(&game, t));
        char desc[420];
        snprintf(desc, sizeof(desc), Tr("%s (%s) : %s\nAttaque %.1f, défense %.1f, antiaérien %.1f, naval %.1f. Vitesse %d, portée %d.\n"
                 "Entretien %.2f Md/tour, %.1f bien/tour, poids %.2f. Formation %d tours.\nTechnologie : %s niveau %d.%s"),
                 Tr(d->name), DOMAIN_LABELS[d->domain], Tr(d->desc), d->atk, d->def, d->aa, d->naval, d->speed, d->range,
                 d->upkeep, d->goods, d->capWeight, d->train, Tr(TECHS[d->tech].name), d->techLevel, StratCostText(t));
        Rectangle r = { x + (t % 4) * (bw + 4), y + (t / 4) * 24, bw, 21 };
        if (ActionButton(r, buf, War_RecruitBlocked(&game, t, selProvince), desc)) Do(CMD_RECRUIT, t, selProvince, 0, 0);
    }
    y += ((U_TYPE_COUNT + 3) / 4) * 24 + 6;

    /* unites de la province selectionnee */
    snprintf(buf, sizeof(buf), Tr("UNITÉS EN %s"), Tr(PROVINCE_NAMES[selProvince]));
    Text(buf, x, y, 10, C_ACCENT);
    y += 14;
    int shown = 0, total = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &game.units[i];
        if (!u->active || u->owner != N_FRANCE || u->prov != selProvince) continue;
        total++;
        if (shown >= 8) continue;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 17 };
        bool h = Hover(row);
        DrawRectangleRec(row, uiSel[i] ? (Color){ 60, 56, 30, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLines((int)row.x + 4, (int)row.y + 3, 10, 10, uiSel[i] ? C_SEL : C_DIM);
        if (uiSel[i]) DrawRectangle((int)row.x + 6, (int)row.y + 5, 6, 6, C_SEL);
        Text(Tr(UNIT_TYPES[u->type].name), row.x + 20, row.y + 3, 10, C_TEXT);
        if (u->group) { snprintf(buf, sizeof(buf), Tr("G%d"), u->group); Text(buf, row.x + 166, row.y + 3, 10, C_SEL); }
        Bar((Rectangle){ row.x + 190, row.y + 4, 70, 7 }, u->str, 1, C_GOOD);
        Bar((Rectangle){ row.x + 266, row.y + 4, 70, 7 }, u->org, 1, C_ACCENT);
        bool air = UNIT_TYPES[u->type].domain == DOM_AIR;
        if ((UNIT_TYPES[u->type].domain == DOM_SEA || air) && !u->training) {
            Rectangle mb = { row.x + 344, row.y + 1, 150, 15 };
            bool mh = Hover(mb);
            int count = air ? AM_COUNT : NM_COUNT, ms = u->mission % count;
            const char *mname = air ? AIR_MISSION_NAMES[ms] : NAVAL_MISSION_NAMES[ms];
            const char *mdesc = air ? AIR_MISSION_DESC[ms] : NAVAL_MISSION_DESC[ms];
            DrawRectangleRec(mb, mh ? (Color){ 52, 82, 120, 255 } : C_PANEL2);
            Text(Tr(mname), mb.x + 4, mb.y + 2, 10, C_ACCENT);
            char tip[400];
            snprintf(tip, sizeof(tip), Tr("Mission : %s\n%s\nClic : mission suivante."), Tr(mname), Tr(mdesc));
            Tooltip(mb, tip);
            if (mh && clicked) {
                clicked = false;
                for (int k = 1; k <= count; k++)
                    if (Do(CMD_MISSION, i, (ms + k) % count, 0, 0)) break;
            }
            y += 18;
            shown++;
            continue;
        }
        if (u->training) snprintf(buf, sizeof(buf), Tr("formation : %d tour(s)"), u->training);
        else if (u->target >= 0 && u->target != u->prov) snprintf(buf, sizeof(buf), Tr("> %s"), Tr(PROVINCE_NAMES[u->target]));
        else snprintf(buf, sizeof(buf), "%s", Tr("en position"));
        Text(buf, row.x + 344, row.y + 3, 10, u->target >= 0 ? C_WARN : C_DIM);
        Tooltip(row, "Clic : sélectionner / désélectionner. Barres : effectifs (vert) et organisation (bleu).");
        if (h && clicked && !u->training) { uiSel[i] = !uiSel[i]; clicked = false; }
        y += 18;
        shown++;
    }
    if (!total) { Text("Aucune unité française ici.", x, y, 10, C_DIM); y += 16; }
    else if (total > shown) { snprintf(buf, sizeof(buf), Tr("... et %d autre(s)"), total - shown); Text(buf, x, y, 10, C_DIM); y += 14; }
    y += 2;
    if (Button((Rectangle){ x, y, 130, 20 }, "Tout sélectionner", true, 10)) SelectUnitsIn(selProvince, true);
    if (Button((Rectangle){ x + 136, y, 130, 20 }, "Annuler les ordres", SelectedCount() > 0, 10))
        for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i] && game.units[i].active) game.units[i].target = -1;
    if (Button((Rectangle){ x + 272, y, 130, 20 }, "Dissoudre", SelectedCount() > 0, 10))
        for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i]) { Do(CMD_DISBAND, i, 0, 0, 0); uiSel[i] = false; }
    snprintf(buf, sizeof(buf), Tr("%d sélectionnée(s)"), SelectedCount());
    Text(buf, x + 412, y + 5, 10, C_SEL);
    y += 24;
    Text("Groupe :", x, y + 4, 10, C_DIM);
    for (int k = 0; k <= GROUP_COUNT; k++) {
        snprintf(buf, sizeof(buf), k ? "G%d" : "Aucun", k);
        Rectangle gb = { x + 60 + k * 54, y, 50, 18 };
        if (Button(gb, buf, SelectedCount() > 0, 10))
            for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i] && game.units[i].active) Do(CMD_GROUP, i, k, 0, 0);
        Tooltip(gb, "Affecte les divisions sélectionnées à un groupe d'armées ; ses ordres se donnent dans l'onglet ÉT.-MAJ.");
    }
    y += 24;

    /* positions */
    Text("POSITIONS (cliquer pour sélectionner et centrer)", x, y, 10, C_DIM);
    y += 13;
    int col = 0;
    for (int p = 0; p < PROV_COUNT && col < 12; p++) {
        int n = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].prov == p) n++;
        if (!n) continue;
        Rectangle r = { RP_X + 6 + (col % 3) * 182, y + (col / 3) * 17, 178, 15 };
        bool h = Hover(r);
        DrawRectangleRec(r, p == selProvince ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        snprintf(buf, sizeof(buf), "%s (%d)", Tr(PROVINCE_NAMES[p]), n);
        Text(buf, r.x + 4, r.y + 2, 10, game.prov[p].owner == N_FRANCE ? C_TEXT : C_WAR);
        if (h && clicked) { selProvince = p; SelectUnitsIn(p, false); FocusProvince(p, ZoomIndex() < 2 ? 2 : ZoomIndex()); clicked = false; }
        col++;
    }
    y = 540;

    /* dissuasion */
    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, VH - 58 - y }, C_PANEL2);
    y += 6;
    Text("DISSUASION NUCLÉAIRE", x, y, 10, C_ACCENT);
    snprintf(buf, sizeof(buf), Tr("Crédibilité %.0f/100%s%s"), game.credibility, game.nukesUsed ? "  -  ARME EMPLOYÉE" : "",
             game.tacticalUsed ? "  -  frappes tactiques" : "");
    Text(buf, x + 160, y, 10, game.nukesUsed ? C_BAD : C_TEXT);
    Bar((Rectangle){ x + 400, y + 1, 140, 8 }, game.credibility, 100, C_EVENT);
    y += 16;
    static const char *postureDesc[3] = {
        "0,5 Md/tour. Crédibilité faible : nos adversaires n'hésiteront pas à frapper le sol national. Pas d'armes tactiques.",
        "1,5 Md/tour. Sous-marins et composante aérienne en alerte : la plupart des agresseurs renoncent à envahir la France.",
        "3 Md/tour. Posture maximale : crédibilité très élevée, ultimatums plus efficaces.",
    };
    for (int i = 0; i < 3; i++) {
        Rectangle r = { x + i * 120, y, 114, 20 };
        if (Choice(r, Tr(NUKE_POSTURES[i]), game.nukePosture == i)) Do(CMD_NUKE_POSTURE, i, 0, 0, 0);
        Tooltip(r, postureDesc[i]);
    }
    const char *ub = Nuke_UmbrellaBlocked(&game);
    if (ActionButton((Rectangle){ x + 368, y, 172, 20 }, game.umbrella ? "Retirer le parapluie UE" : "Parapluie européen",
                     ub, "Étend la dissuasion aux partenaires de l'UE (+1 Md/tour) : influence, relations, les agresseurs "
                         "hésitent à envahir un État membre, et les Européens renoncent à leurs propres programmes."))
        Do(CMD_UMBRELLA, 0, 0, 0, 0);
    y += 26;
    TextWrap("Ultime avertissement et contre-prolifération : onglet NATIONS. Frappes tactique et stratégique : depuis une province "
             "ennemie (onglet PROVINCE).", x, y, RP_W - 30, 10, C_DIM);
}

void DrawStaffTab(void)
{
    char buf[260];
    float x = RP_X + 10, y = RP_Y + 4;
    Text("GÉNÉRAUX", x, y, 10, C_ACCENT);
    if (ActionButton((Rectangle){ RP_X + RP_W - 196, y - 2, 186, 18 }, "Promouvoir un général (3 Md)", Mil_PromoteBlocked(&game),
                     "Nomme un nouveau général (compétence 1 ou 2, trait aléatoire). 8 au maximum."))
        Do(CMD_PROMOTE, 0, 0, 0, 0);
    y += 18;
    int shown = 0;
    for (int i = 0; i < MAX_GENERALS; i++) {
        const General *gen = &game.generals[i];
        if (!gen->active || gen->owner != N_FRANCE || shown >= 5) continue;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 20 };
        bool h = Hover(row);
        DrawRectangleRec(row, i == selGeneral ? (Color){ 60, 56, 30, 255 } : h ? C_PANEL2 : C_PANEL);
        Text(gen->name, row.x + 8, row.y + 5, 10, C_TEXT);
        for (int k = 0; k < 5; k++)
            DrawRectangle((int)row.x + 130 + k * 9, (int)row.y + 6, 7, 7, k < gen->skill ? C_SEL : (Color){ 50, 56, 66, 255 });
        Text(Tr(GENERAL_TRAITS[gen->trait]), row.x + 186, row.y + 5, 10, C_ACCENT);
        snprintf(buf, sizeof(buf), Tr("xp %.0f"), gen->xp);
        Text(buf, row.x + 280, row.y + 5, 10, C_DIM);
        Text(gen->prov >= 0 ? Tr(PROVINCE_NAMES[gen->prov]) : "en réserve", row.x + 330, row.y + 5, 10, gen->prov >= 0 ? C_TEXT : C_DIM);
        static const char *traitDesc[GT_COUNT] = {
            "+12% en attaque.", "+12% en défense.", "Organisation récupérée +20% dans sa province.",
            "+15% pour les blindés et mécanisées.", "+15% en collines et en montagne.",
        };
        char tip[300];
        snprintf(tip, sizeof(tip), Tr("%s : compétence %d (+%d%% au combat), %s\nIl gagne de l'expérience dans les batailles de sa province "
                 "et des provinces voisines. Clic : sélectionner, puis affecter."), gen->name, gen->skill, gen->skill * 5, traitDesc[gen->trait]);
        Tooltip(row, tip);
        if (h && clicked) { selGeneral = i; clicked = false; }
        y += 22;
        shown++;
    }
    if (!shown) { Text("Aucun général.", x, y, 10, C_DIM); y += 16; }
    y += 2;
    if (selGeneral >= 0 && game.generals[selGeneral].active && game.generals[selGeneral].owner == N_FRANCE) {
        snprintf(buf, sizeof(buf), Tr("Affecter à %s"), Tr(PROVINCE_NAMES[selProvince]));
        if (ActionButton((Rectangle){ x, y, 260, 22 }, buf, Mil_AssignBlocked(&game, selGeneral, selProvince),
                         "Le général commande toutes nos divisions de la province sélectionnée sur la carte."))
            Do(CMD_ASSIGN, selGeneral, selProvince, 0, 0);
        if (Button((Rectangle){ x + 270, y, 140, 22 }, "Mettre en réserve", game.generals[selGeneral].prov >= 0, 10))
            Do(CMD_ASSIGN, selGeneral, -1, 0, 0);
    } else {
        Text("Sélectionnez un général puis une province sur la carte pour l'y affecter.", x, y + 5, 10, C_DIM);
    }
    y += 30;

    Text("GROUPES D'ARMÉES", x, y, 10, C_ACCENT);
    Text("divisions affectées depuis l'onglet ARMÉE", x + 130, y, 10, C_DIM);
    y += 15;
    for (int k = 1; k <= GROUP_COUNT; k++) {
        int members = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].group == k) members++;
        snprintf(buf, sizeof(buf), Tr("G%d : %d div."), k, members);
        Text(buf, x, y + 4, 10, members ? C_TEXT : C_DIM);
        int mode = game.groups[k - 1].mode, tgt = game.groups[k - 1].target;
        for (int m = 0; m < GM_COUNT; m++) {
            Rectangle mr = { x + 90 + m * 100, y, 96, 18 };
            if (Choice(mr, Tr(GROUP_MODE_NAMES[m]), mode == m)) Do(CMD_GROUP_ORDER, k, m, tgt > 0 ? tgt : selNation, 0);
        }
        Rectangle tr = { x + 395, y, 145, 18 };
        snprintf(buf, sizeof(buf), Tr("> %s"), tgt > 0 ? Tr(NATIONS[tgt].name) : "cible ?");
        if (Button(tr, buf, true, 10)) {
            /* nation suivante : en guerre contre nous ou voisine */
            for (int step = 1; step < N_COUNT; step++) {
                int c = (tgt + step) % N_COUNT;
                if (c == N_FRANCE || !game.nat[c].alive) continue;
                if (War_AtWar(&game, N_FRANCE, c) || War_Adjacent(&game, N_FRANCE, c)) { Do(CMD_GROUP_ORDER, k, mode, c, 0); break; }
            }
        }
        Tooltip(tr, "Nation visée : le groupe tient le front face à elle, ou y mène l'offensive (en guerre seulement). "
                    "Clic : nation suivante (en guerre ou frontalière).");
        y += 21;
    }
    y += 4;

    Text("TECHNOLOGIES MILITAIRES", x, y, 10, C_ACCENT);
    snprintf(buf, sizeof(buf), Tr("%.1f points/tour"), Mil_ResearchPoints(&game));
    Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y, 10, C_DIM);
    y += 16;
    for (int t = 0; t < T_COUNT; t++) {
        Rectangle row = { RP_X + 6, y, RP_W - 12, 19 };
        bool cur = game.researchArea == t, h = Hover(row);
        int lvl = game.nat[N_FRANCE].tech[t];
        DrawRectangleRec(row, cur ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLinesEx(row, 1, cur ? C_ACCENT : C_BORDER);
        Text(Tr(TECHS[t].name), row.x + 8, row.y + 5, 10, C_TEXT);
        for (int k = 0; k < TECH_MAX; k++)
            DrawRectangle((int)row.x + 170 + k * 16, (int)row.y + 5, 14, 9, k < lvl ? C_GOOD : (Color){ 50, 56, 66, 255 });
        if (lvl >= TECH_MAX) snprintf(buf, sizeof(buf), "%s", Tr("maîtrisée"));
        else if (cur) snprintf(buf, sizeof(buf), Tr("en cours : %.0f / %.0f"), game.researchProgress, Mil_TechCost(&game, t));
        else snprintf(buf, sizeof(buf), Tr("niveau %d : %.0f points"), lvl + 1, Mil_TechCost(&game, t));
        Text(buf, row.x + 260, row.y + 5, 10, cur ? C_SEL : C_DIM);
        if (cur && lvl < TECH_MAX) Bar((Rectangle){ row.x + 420, row.y + 6, 120, 8 }, game.researchProgress, Mil_TechCost(&game, t), C_SEL);
        char tip[300];
        snprintf(tip, sizeof(tip), Tr("%s\n%s\nClic : orienter la recherche vers ce domaine."), Tr(TECHS[t].name), Tr(TECHS[t].desc));
        Tooltip(row, tip);
        if (h && clicked) { Do(CMD_RESEARCH, t, 0, 0, 0); clicked = false; }
        y += 21;
    }
    y += 4;
    float v;
    Stepper(x, y, "Budget de recherche", game.researchBudget / 100.0f, &v, 0.005f, "%.1f Md",
            "Crédits consacrés à la recherche militaire chaque tour. Les points dépendent aussi de l'alphabétisation et du régime "
            "(la technocratie est la plus efficace).");
    if (v != game.researchBudget / 100.0f) Do(CMD_RESEARCH_BUDGET, 0, 0, 0, v * 100.0f);
    y += 26;
    snprintf(buf, sizeof(buf), Tr("Les autres nations progressent aussi : %s en est au niveau moyen %.1f."),
             Tr(NATIONS[selNation].name), (game.nat[selNation].tech[0] + game.nat[selNation].tech[1] + game.nat[selNation].tech[2]
              + game.nat[selNation].tech[3] + game.nat[selNation].tech[4] + game.nat[selNation].tech[5] + game.nat[selNation].tech[6]) / 7.0f);
    TextWrap(buf, x, y, RP_W - 20, 10, C_DIM);
}

void DrawOpsPanel(int n, float dx, float dy, float bw)
{
    char buf[260];
    const Nation *na = &game.nat[n];
    Text("OPÉRATIONS CIBLÉES (1 PC)", dx, dy, 10, C_ACCENT);
    dy += 14;
    int len = 0;
    buf[0] = 0;
    if (na->outage > 0) len += snprintf(buf + len, sizeof(buf) - len, Tr("réseau paralysé %d t  "), na->outage);
    if (na->jammed > 0) len += snprintf(buf + len, sizeof(buf) - len, Tr("brouillée %d t  "), na->jammed);
    if (na->recon > 0) len += snprintf(buf + len, sizeof(buf) - len, Tr("sous surveillance %d t  "), na->recon);
    if (na->asat > 0) len += snprintf(buf + len, sizeof(buf) - len, Tr("sans satellites %d t"), na->asat);
    Text(len ? buf : "Aucune opération en cours contre elle.", dx, dy, 10, len ? C_GOOD : C_DIM);
    dy += 16;
    for (int op = 0; op < OP_COUNT; op++) {
        const OpDef *o = &OPS[op];
        Rectangle r = { dx, dy, bw, 36 };
        const char *b = Ops_Blocked(&game, n, op);
        bool h = Hover(r) && !b;
        DrawRectangleRec(r, h ? (Color){ 52, 82, 120, 255 } : C_PANEL);
        DrawRectangleLinesEx(r, 1, h ? C_ACCENT : C_BORDER);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, DOMAIN_COLORS[o->domain]);
        Text(Tr(o->name), r.x + 10, r.y + 5, 10, b ? C_DIM : C_TEXT);
        static const char *SHORT_DOMAINS[D_COUNT] = { "Terre", "Mer", "Air", "Espace", "Cyber", "Info", "EM" };
        snprintf(buf, sizeof(buf), Tr("%s %d - %.0f Md%s - réussite %.0f%% - attrib. %.0f%%"), SHORT_DOMAINS[o->domain], o->capacity,
                 o->budget, o->influence > 0 ? "+infl." : "", Ops_Chance(&game, n, op) * 100, fminf(1, o->risk) * 100);
        Text(buf, r.x + 10, r.y + 20, 10, b ? (Color){ 90, 100, 112, 255 } : C_DIM);
        char tip[500];
        snprintf(tip, sizeof(tip), Tr("%s\n%s\nCapacité %s %d requise, %.0f Md%s.%s%s%s"), Tr(o->name), Tr(o->desc), Tr(DOMAIN_NAMES[o->domain]), o->capacity,
                 o->budget, o->influence > 0 ? ", de l'influence" : "", o->needWar ? " Contre un ennemi seulement." : "",
                 b ? "\nIndisponible : " : "", b ? b : "");
        Tooltip(r, tip);
        if (h && clicked) { clicked = false; Do(CMD_OPERATION, n, op, 0, 0); }
        dy += 39;
    }
    TextWrap("Une opération attribuée à la France dégrade la relation et l'influence ; une nation très hostile peut riposter "
             "par une cyberattaque, voire déclarer la guerre. Investissez dans les capacités Cyber, Info, Espace et EM "
             "(onglet DÉFENSE).", dx, dy + 2, bw, 10, C_DIM);
}

void OpenTreaty(int n)
{
    negoMsg[0] = 0;
    Do(CMD_TREATY_DRAFT, n, 0, 0, 0);
    treatyOpen = true;
    provScroll = 0;
}

void DrawNationsTab(void)
{
    char buf[260];
    float y = RP_Y + 4;
    float lx = RP_X + 6, lw = 250;
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &game.nat[n];
        Rectangle r = { lx, y + (n - 1) * 20, lw, 19 };
        bool h = Hover(r);
        DrawRectangleRec(r, n == selNation ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, NationColor(n));
        Text(Tr(NATIONS[n].name), r.x + 8, r.y + 5, 10, na->alive ? C_TEXT : C_DIM);
        if (!na->alive) {
            Text("disparu", r.x + 128, r.y + 5, 10, C_DIM);
        } else {
            snprintf(buf, sizeof(buf), "%+.0f", na->relation);
            Text(buf, r.x + 128, r.y + 5, 10, na->relation < -20 ? C_BAD : na->relation > 20 ? C_GOOD : C_TEXT);
            bool otherWar = false;
            for (int k = 1; k < N_COUNT; k++) if (k != n && game.war[n][k]) otherWar = true;
            const char *st = game.war[N_FRANCE][n] ? "GUERRE" : game.ally[N_FRANCE][n] ? "allié" : otherWar ? "en guerre"
                           : game.truce[N_FRANCE][n] ? "trêve" : na->embargo || na->embargoFrance ? "embargo" : na->trade ? "accord" : "";
            Text(st, r.x + 162, r.y + 5, 10, game.war[N_FRANCE][n] ? C_WAR : game.ally[N_FRANCE][n] ? C_GOOD : otherWar ? C_WARN : C_DIM);
            if (na->eu) Text("UE", r.x + 218, r.y + 5, 10, C_ACCENT);
            if (na->nukeStatus) Text(na->nukeStatus == 2 ? "N" : "n", r.x + 238, r.y + 5, 10, na->nukeStatus == 2 ? C_WARN : C_DIM);
        }
        if (h && clicked) { selNation = n; clicked = false; }
    }
    Tooltip((Rectangle){ lx, y, lw, 26 * 20 }, "Relation avec la France, statut, UE ; N : puissance nucléaire, n : programme en cours.");
    {
        float ny = y + 26 * 20 + 2;
        snprintf(buf, sizeof(buf), Tr("OTAN : cohésion %.0f%%"), game.natoCohesion);
        Text(buf, lx, ny + 4, 10, game.natoCohesion < 40 ? C_BAD : C_TEXT);
        if (ActionButton((Rectangle){ lx + 140, ny, lw - 140, 18 }, game.nat[N_FRANCE].nato ? "Quitter l'OTAN" : "Réintégrer l'OTAN",
                         Intl_NatoBlocked(&game), game.nat[N_FRANCE].nato
                         ? "Indépendance stratégique (+5 Souveraineté) mais plus de garantie de l'article 5 ; relations avec les Alliés -15. 1 PC."
                         : "Retour dans le commandement intégré : protection de l'article 5, relations avec les Alliés +10. 1 PC."))
            Do(CMD_NATO, 0, 0, 0, 0);
    }

    int n = selNation == N_FRANCE ? N_ALLEMAGNE : selNation;
    const Nation *na = &game.nat[n];
    float dx = lx + lw + 8, dw = RP_X + RP_W - dx - 6;
    Panel((Rectangle){ dx, y, dw, 26 * 20 }, C_PANEL2);
    dx += 8;
    float dy = y + 6, bw = dw - 16;
    Text(Tr(NATIONS[n].name), dx, dy, 20, NationColor(n));
    dy += 24;
    static const char *views[3] = { "Diplomatie", "Opérations", "Historique" };
    for (int v = 0; v < 3; v++)
        if (Choice((Rectangle){ dx + v * (bw / 3), dy, bw / 3 - 4, 18 }, views[v], nationView == v)) nationView = v;
    dy += 24;
    if (nationView == 1 && na->alive) { DrawOpsPanel(n, dx, dy, bw); return; }
    if (nationView == 2) {
        int hn = game.turn + 1 < HISTORY_LEN ? game.turn + 1 : HISTORY_LEN;
        float pmax = 1, amax = 1;
        for (int i = 0; i < hn; i++) {
            if (game.histNatPop[n][i] > pmax) pmax = game.histNatPop[n][i];
            if (game.histNatArmy[n][i] > amax) amax = game.histNatArmy[n][i];
        }
        Sparkline((Rectangle){ dx, dy, bw, 110 }, game.histNatPop[n], hn, 0, pmax * 1.1f, C_ACCENT, "Population");
        Sparkline((Rectangle){ dx, dy + 116, bw, 110 }, game.histNatArmy[n], hn, 0, amax * 1.1f, C_WAR, "Puissance militaire");
        Sparkline((Rectangle){ dx, dy + 232, bw, 110 }, game.histNatRel[n], hn, -100, 100, C_GOOD, "Relation avec la France");
        snprintf(buf, sizeof(buf), Tr("Depuis 2030 : population %.1f -> %.1f M, puissance %.1f -> %.1f."), game.histNatPop[n][0],
                 Soc_NationPop(&game, n), game.histNatArmy[n][0], na->army);
        TextWrap(buf, dx, dy + 350, bw, 10, C_DIM);
        return;
    }
    snprintf(buf, sizeof(buf), "%s%s%s", Tr(REGIMES[na->regime].name), na->eu ? " - UE" : "", na->failed ? " - FAILLI" : "");
    Text(buf, dx, dy, 10, C_TEXT); dy += 13;
    if (na->government >= 0) snprintf(buf, sizeof(buf), Tr("Gouvernement : %s%s"), Tr(IDEOLOGY_NAMES[na->government]),
                                      na->nextElection > game.turn ? "" : "");
    else snprintf(buf, sizeof(buf), "%s", Tr("Pas de gouvernement élu"));
    Text(buf, dx, dy, 10, C_DIM); dy += 13;
    if (na->nextElection >= 0 && na->nextElection > game.turn) {
        snprintf(buf, sizeof(buf), Tr("Élections dans %d tours"), na->nextElection - game.turn);
        Text(buf, dx, dy, 10, C_DIM); dy += 13;
    }
    if (na->nukeStatus) {
        if (na->nukeStatus == 1) snprintf(buf, sizeof(buf), Tr("Programme nucléaire : %.0f%%"), na->nukeProgress);
        else snprintf(buf, sizeof(buf), "%s", Tr("Puissance nucléaire"));
        Text(buf, dx, dy, 10, C_WARN); dy += 13;
    }
    snprintf(buf, sizeof(buf), Tr("Relation : %+.0f"), na->relation);
    Text(buf, dx, dy, 10, na->relation < 0 ? C_BAD : C_GOOD); dy += 13;
    snprintf(buf, sizeof(buf), Tr("Pop. %.1f M (%d prov.), chômage %.0f%%"), Soc_NationPop(&game, n), Soc_NationProvinces(&game, n),
             Eco_NationUnemployment(&game, n) * 100);
    Text(buf, dx, dy, 10, C_TEXT); dy += 13;
    snprintf(buf, sizeof(buf), Tr("Armée %.1f (%d unités), trésor %.0f"), na->army, War_UnitCount(&game, n), na->treasury);
    Text(buf, dx, dy, 10, C_TEXT); dy += 13;
    if (na->blockade > 0.05f) { snprintf(buf, sizeof(buf), Tr("Côtes sous blocus : %.0f%%"), na->blockade * 100); Text(buf, dx, dy, 10, C_WARN); dy += 13; }
    {
        static const char *ROUTES[3] = { "Route commerciale coupée", "Route commerciale terrestre", "Route commerciale maritime : %.0f%%" };
        int rk = game.routeKind[n];
        snprintf(buf, sizeof(buf), Tr(ROUTES[rk]), game.route[n] * 100);
        Rectangle rr = { dx, dy, bw, 12 };
        Text(buf, dx, dy, 10, rk == 0 ? C_BAD : game.route[n] < 0.6f ? C_WARN : C_DIM);
        Tooltip(rr, "Les excédents d'une nation ne nous parviennent que par une route : par la terre à travers des pays qui ne "
                    "nous combattent pas, ou par la mer entre deux ports, au gré du blocus et de la guerre au commerce.");
        dy += 13;
    }
    /* alliances et guerres */
    int len = snprintf(buf, sizeof(buf), "%s", Tr("Alliés :"));
    int cnt = 0;
    for (int k = 0; k < N_COUNT; k++) if (k != n && game.ally[n][k]) { len += snprintf(buf + len, sizeof(buf) - len, " %s", Tr(NATIONS[k].name)); cnt++; }
    if (cnt) { dy += TextWrap(buf, dx, dy, bw, 10, C_GOOD); }
    len = snprintf(buf, sizeof(buf), "%s", Tr("En guerre contre :"));
    cnt = 0;
    for (int k = 0; k < N_COUNT; k++) if (k != n && game.war[n][k]) { len += snprintf(buf + len, sizeof(buf) - len, " %s", Tr(NATIONS[k].name)); cnt++; }
    if (cnt) { dy += TextWrap(buf, dx, dy, bw, 10, C_WAR); }
    if (game.war[N_FRANCE][n]) {
        snprintf(buf, sizeof(buf), Tr("Score de guerre : %+.0f (%d tours)"), War_Score(&game, N_FRANCE, n), War_Turns(&game, N_FRANCE, n));
        Text(buf, dx, dy, 10, War_Score(&game, N_FRANCE, n) >= 0 ? C_GOOD : C_BAD); dy += 13;
        Text(na->deterred ? "Dissuadée de toucher au sol national" : "Prête à frapper le sol national", dx, dy, 10,
             na->deterred ? C_GOOD : C_BAD);
        dy += 13;
    }
    if (game.truce[N_FRANCE][n]) { snprintf(buf, sizeof(buf), Tr("Trêve : %d tours"), game.truce[N_FRANCE][n]); Text(buf, dx, dy, 10, C_DIM); dy += 13; }
    if (na->reparationTurns > 0) {
        snprintf(buf, sizeof(buf), na->reparations > 0 ? "Nous verse %.0f Md/tour (%d tours)" : "Reçoit %.0f Md/tour de nous (%d tours)",
                 fabsf(na->reparations), na->reparationTurns);
        Text(buf, dx, dy, 10, na->reparations > 0 ? C_GOOD : C_BAD); dy += 13;
    }
    if (na->embargoFrance) { Text("Embargo contre la France", dx, dy, 10, C_BAD); dy += 13; }
    if (na->sanctions > 0) { snprintf(buf, sizeof(buf), Tr("Sanctions de l'ONU : %d tours"), na->sanctions); Text(buf, dx, dy, 10, C_WARN); dy += 13; }
    if (na->nato) { Text("Membre de l'OTAN", dx, dy, 10, C_ACCENT); dy += 13; }
    if (na->raid > 0.05f) { snprintf(buf, sizeof(buf), Tr("Commerce attaqué : -%.0f%%"), na->raid * 100); Text(buf, dx, dy, 10, C_WARN); dy += 13; }
    dy += 4;

    float half = (bw - 6) / 2;
    if (ActionButton((Rectangle){ dx, dy, half, 22 }, "Rapprochement", War_DiploBlocked(&game, n, 0), "Améliore la relation de 12 (1 PC, 5 Influence)."))
        Do(CMD_DIPLOMACY, n, 0, 0, 0);
    if (ActionButton((Rectangle){ dx + half + 6, dy, half, 22 }, "Accord commercial", War_DiploBlocked(&game, n, 1),
                     "La France est servie en priorité sur leurs excédents au marché mondial (1 PC). Relation 25 requise."))
        Do(CMD_DIPLOMACY, n, 1, 0, 0);
    dy += 26;
    if (ActionButton((Rectangle){ dx, dy, half, 22 }, "Alliance défensive", War_DiploBlocked(&game, n, 2),
                     "Chacun s'engage à entrer en guerre si l'autre est attaqué (1 PC). Relation 60 requise. Nos alliés comptent "
                     "dans les ultimatums et dissuadent les agresseurs."))
        Do(CMD_DIPLOMACY, n, 2, 0, 0);
    if (ActionButton((Rectangle){ dx + half + 6, dy, half, 22 }, na->embargo ? "Lever l'embargo" : "Embargo", Eco_EmbargoBlocked(&game, n),
                     "Coupe le commerce avec cette nation (1 PC) : leurs exportations chutent, mais nos importations aussi. Relation -25, "
                     "risque d'embargo en retour. Impossible entre membres de l'UE."))
        Do(CMD_EMBARGO, n, 0, 0, 0);
    dy += 26;
    if (na->nukeStatus == 1) {
        if (ActionButton((Rectangle){ dx, dy, half, 22 }, "Pression diplomatique", Nuke_CounterBlocked(&game, n, 0),
                         "Contre-prolifération (1 PC, 15 Influence) : freine le programme si la pression porte (accords, UE, influence)."))
            Do(CMD_COUNTER, n, 0, 0, 0);
        if (ActionButton((Rectangle){ dx + half + 6, dy, half, 22 }, "Sabotage cyber", Nuke_CounterBlocked(&game, n, 1),
                         "Contre-prolifération (1 PC, 2 Md, capacité cyber 3) : détruit une partie du programme ; s'il est découvert, "
                         "scandale international. Occuper leur capitale anéantit le programme."))
            Do(CMD_COUNTER, n, 1, 0, 0);
        dy += 26;
    }
    if (ActionButton((Rectangle){ dx, dy, bw, 22 }, "Saisir le Conseil de sécurité", Intl_ResolutionBlocked(&game, n),
                     "Propose une résolution condamnant cette nation (agression ou programme nucléaire) : sanctions internationales "
                     "pendant 4 ans si aucun membre permanent n'oppose son veto (Royaume-Uni, Fédération Boréale, États-Unis, Chine). "
                     "1 PC, 10 Influence."))
        Do(CMD_RESOLUTION, n, 0, 0, 0);
    dy += 26;
    if (game.war[N_FRANCE][n]) {
        if (ActionButton((Rectangle){ dx, dy, half, 22 }, "Négocier la paix", NULL,
                         "Ouvre la table des négociations : provinces cédées et réparations, dans la limite du score de guerre."))
            OpenTreaty(n);
        if (ActionButton((Rectangle){ dx + half + 6, dy, half, 22 }, "Ultime avertissement", Nuke_WarningBlocked(&game, n),
                         "Menace d'emploi si l'ennemi ne quitte pas le sol national (une fois par guerre, 1 PC). Succès selon la "
                         "crédibilité ; un bluff percé coûte 20 points de crédibilité."))
            Do(CMD_WARNING, n, 0, 0, 0);
    } else if (ActionButton((Rectangle){ dx, dy, bw, 22 }, "Déclarer la guerre", War_DeclareBlocked(&game, n), DECLARE_DESC)) {
        Do(CMD_DECLARE_WAR, n, 0, 0, 0);
    }
}

/* table des negociations */
void DrawTreaty(void)
{
    Treaty *t = &game.treaty;
    if (!t->active || !game.war[N_FRANCE][t->other]) { treatyOpen = false; return; }
    int n = t->other;
    bool win = t->winner == N_FRANCE;
    int loser = win ? n : N_FRANCE;
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
    Rectangle r = { 240, 70, 800, 580 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, C_WAR);
    char buf[260];
    snprintf(buf, sizeof(buf), Tr("NÉGOCIATIONS AVEC %s"), Tr(NATIONS[n].name));
    Text(buf, r.x + 20, r.y + 16, 20, C_TEXT);
    float score = War_Score(&game, N_FRANCE, n), cost = War_TreatyCost(&game, t);
    snprintf(buf, sizeof(buf), Tr("Score de guerre %+.0f  -  coût des exigences %.0f  -  %s"), score, cost,
             win ? "nous dictons les conditions" : "l'ennemi a l'avantage : il faut concéder");
    Text(buf, r.x + 20, r.y + 42, 10, win ? C_GOOD : C_BAD);
    Bar((Rectangle){ r.x + 20, r.y + 58, r.width - 40, 8 }, cost, fmaxf(5, fabsf(score) + 10), cost <= fabsf(score) + 5 ? C_GOOD : C_BAD);
    TextWrap(win ? "Cochez les provinces que l'ennemi nous cède. Une province que nous n'occupons pas coûte le double. Les réparations "
                   "coûtent 5 points par milliard et par tour, pendant 4 ans. Un ennemi las de la guerre cède davantage."
                 : "L'ennemi exige des concessions à la mesure de son avantage : cochez les provinces cédées et les réparations versées.",
             r.x + 20, r.y + 72, r.width - 40, 10, C_DIM);

    /* liste des provinces du perdant, celles occupees par le vainqueur d'abord */
    int list[PROV_COUNT], c = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int p = 0; p < PROV_COUNT; p++) {
            if (game.prov[p].owner != loser) continue;
            bool occ = game.prov[p].ctrl == t->winner;
            if ((pass == 0) == occ) list[c++] = p;
        }
    int rows = 18;
    if (provScroll > c - rows) provScroll = c - rows;
    if (provScroll < 0) provScroll = 0;
    Rectangle area = { r.x + 20, r.y + 104, 470, rows * 22 };
    if (Hover(area)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) provScroll--;
        if (wheel < 0) provScroll++;
    }
    for (int i = 0; i < rows && provScroll + i < c; i++) {
        int p = list[provScroll + i];
        Rectangle row = { area.x, area.y + i * 22, area.width, 20 };
        bool h = Hover(row);
        DrawRectangleRec(row, t->cede[p] ? (Color){ 60, 56, 30, 255 } : h ? C_PANEL2 : C_BG);
        DrawRectangleLines((int)row.x + 4, (int)row.y + 5, 10, 10, t->cede[p] ? C_SEL : C_DIM);
        if (t->cede[p]) DrawRectangle((int)row.x + 6, (int)row.y + 7, 6, 6, C_SEL);
        Text(Tr(PROVINCE_NAMES[p]), row.x + 20, row.y + 5, 10, C_TEXT);
        Treaty tmp = *t;
        tmp.cede[p] = !tmp.cede[p];
        float delta = fabsf(War_TreatyCost(&game, &tmp) - cost);
        snprintf(buf, sizeof(buf), Tr("%.1f M  %s  %.0f pts"), game.prov[p].pop, game.prov[p].ctrl == t->winner ? "occupée" : "        ", delta);
        Text(buf, row.x + 250, row.y + 5, 10, game.prov[p].ctrl == t->winner ? C_GOOD : C_DIM);
        if (h && clicked) { Do(CMD_TREATY_CEDE, p, 0, 0, 0); clicked = false; }
    }
    if (c > rows) {
        snprintf(buf, sizeof(buf), Tr("%d-%d sur %d (molette)"), provScroll + 1, provScroll + rows, c);
        Text(buf, area.x, area.y + rows * 22 + 2, 10, C_DIM);
    }

    float x2 = r.x + 510, y2 = r.y + 104;
    Text(win ? "RÉPARATIONS EXIGÉES" : "RÉPARATIONS VERSÉES", x2, y2, 10, C_ACCENT);
    y2 += 16;
    static const float reps[4] = { 0, 2, 4, 6 };
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), i ? "%.0f Md/tour" : "Aucune", reps[i]);
        if (Choice((Rectangle){ x2, y2 + i * 24, 260, 20 }, buf, fabsf(t->reparations - reps[i]) < 0.1f)) {
            Do(CMD_TREATY_REPARATIONS, 0, 0, 0, reps[i]);
        }
    }
    y2 += 110;
    int ceded = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (t->cede[p]) ceded++;
    snprintf(buf, sizeof(buf), Tr("%d province(s) cédée(s) à %s"), ceded, Tr(NATIONS[t->winner].name));
    Text(buf, x2, y2, 10, C_TEXT);
    y2 += 16;
    float ex = War_Exhaustion(&game, n);
    float limit = win ? score + 5 + ex : -score - 10 - ex;
    snprintf(buf, sizeof(buf), win ? "Lassitude de l'ennemi : %.0f. Il cédera jusqu'à environ %.0f points." :
                                     "Lassitude de l'ennemi : %.0f. Il se contentera d'environ %.0f points.", ex, limit);
    y2 += TextWrap(buf, x2, y2, 260, 10, C_WARN) + 4;
    const Nation *na = &game.nat[n];
    int rounds = na->negoTurn == game.turn ? na->negoRounds : 0;
    const char *blocked = NULL;
    if (rounds >= 3) blocked = "L'ennemi a quitté la table pour ce trimestre";
    else if (rounds == 0 && game.commandPoints < 1) blocked = "Plus de points de commandement";
    if (negoMsg[0]) y2 += TextWrap(negoMsg, x2, y2, 260, 10, t->countered ? C_SEL : C_BAD) + 4;
    y2 += 6;
    snprintf(buf, sizeof(buf), Tr("PROPOSER (%d/3)"), rounds);
    if (ActionButton((Rectangle){ x2, y2, 260, 30 }, buf, blocked,
                     "Soumet le traité (1 PC pour la séance, 3 propositions par trimestre). L'ennemi accepte s'il reste dans "
                     "la limite de son score et de sa lassitude ; sinon il fait une contre-proposition, à accepter telle quelle "
                     "ou à amender.")) {
        Do(CMD_NEGOTIATE, 0, 0, 0, 0);
        int r = !game.treaty.active ? 1 : game.treaty.countered ? 2 : 0;
        if (r == 1) { treatyOpen = false; Toast("Traité signé."); }
        else if (r == 2) snprintf(negoMsg, sizeof(negoMsg), Tr("Contre-proposition de %s : conditions modifiées ci-contre. Proposez-les pour signer."), Tr(NATIONS[n].name));
        else snprintf(negoMsg, sizeof(negoMsg), "%s", Tr("Négociations rompues pour ce trimestre."));
    }
    y2 += 36;
    if (Button((Rectangle){ x2, y2, 260, 26 }, "Conditions par défaut", true, 10)) { Do(CMD_TREATY_DRAFT, n, 0, 0, 0); negoMsg[0] = 0; }
    y2 += 32;
    if (Button((Rectangle){ x2, y2, 260, 26 }, "Quitter la table", true, 10) || IsKeyPressed(KEY_ESCAPE)) {
        treatyOpen = false;
        t->active = false;
    }
}
