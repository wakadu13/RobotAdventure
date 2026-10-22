/*
 * BOUCLIER 2050 - onglets de l'Etat : defense M2MC, guerre hybride, societe, economie,
 * politique, gouvernement, projets.
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Onglets de l'Etat                                                   */
/* ------------------------------------------------------------------ */
void DrawDefenseTab(void)
{
    int t = selTheater;
    Domain d = THEATERS[t].domain;
    float y = RP_Y + 4;
    char buf[200];
    Text(Tr(THEATERS[t].name), RP_X + 10, y, 20, DOMAIN_COLORS[d]);
    snprintf(buf, sizeof(buf), Tr("Milieu : %s"), Tr(DOMAIN_NAMES[d]));
    Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y + 6, 10, C_DIM);
    y += 24;
    y += TextWrap(Tr(THEATERS[t].desc), RP_X + 10, y, RP_W - 20, 10, C_DIM) + 4;

    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, 60 }, C_PANEL2);
    snprintf(buf, sizeof(buf), Tr("Posture : %d"), game.posture[t]);
    Text(buf, RP_X + 16, y + 8, 20, C_TEXT);
    if (Button((Rectangle){ RP_X + 170, y + 6, 26, 24 }, "-", game.posture[t] > 0, 20)) Do(CMD_POSTURE, t, -1, 0, 0);
    if (Button((Rectangle){ RP_X + 200, y + 6, 26, 24 }, "+", Game_PostureUsed(&game, d) < game.capacity[d], 20))
        Do(CMD_POSTURE, t, 1, 0, 0);
    snprintf(buf, sizeof(buf), Tr("Capacité %s : %d / %d - défense %.1f"), Tr(DOMAIN_NAMES[d]), Game_PostureUsed(&game, d),
             game.capacity[d], Game_DefensePower(&game, t));
    Text(buf, RP_X + 16, y + 40, 10, C_DIM);
    if (game.pendingTurns[d] > 0) {
        snprintf(buf, sizeof(buf), Tr("+%d capacité dans %d tour(s)"), game.pendingCap[d], game.pendingTurns[d]);
        Text(buf, RP_X + 290, y + 14, 10, C_GOOD);
    } else {
        float cost = Game_InvestCost(&game, d);
        snprintf(buf, sizeof(buf), Tr("Investir +2 capacité (%.0f Md)"), cost);
        Rectangle br = { RP_X + 290, y + 8, 256, 22 };
        if (Button(br, buf, game.res[R_BUDGET] >= cost, 10)) Do(CMD_INVEST, d, 0, 0, 0);
        Tooltip(br, "Disponible après 2 tours. Capacité : 0,1 Md/tour, posture engagée : 0,6 Md/tour. Air, Espace, Cyber et EM "
                    "renforcent aussi les divisions ; Mer permet les traversées et les débarquements ; Espace et Mer rendent la "
                    "dissuasion plus crédible.");
    }
    y += 66;

    Text("MENACES SUR CE THÉÂTRE", RP_X + 10, y, 10, C_DIM);
    y += 14;
    int shown = 0;
    for (int i = 0; i < MAX_THREATS && shown < 5; i++) {
        const Threat *th = &game.threats[i];
        if (!th->active || th->theater != t) continue;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 32 };
        Panel(row, (Color){ 40, 28, 30, 255 });
        if (th->detected) {
            const ThreatDef *td = &THREATS[th->type];
            if (th->prov >= 0) snprintf(buf, sizeof(buf), Tr("%s  [force %d] - %s"), Tr(td->name), th->strength, Tr(PROVINCE_NAMES[th->prov]));
            else snprintf(buf, sizeof(buf), Tr("%s  [force %d]"), Tr(td->name), th->strength);
            Text(buf, row.x + 8, row.y + 4, 10, C_TEXT);
            snprintf(buf, sizeof(buf), Tr("%s - encore %d tour(s) - neutralisation %d%%"), Tr(FACTIONS[th->faction].name),
                     th->turnsLeft, (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 17, 10, FACTION_COLORS[th->faction]);
            char tip[300], eff[200] = "";
            for (int r = 0; r < R_COUNT; r++) {
                if (td->effects[r] == 0) continue;
                char e[40];
                snprintf(e, sizeof(e), "%s %.1f  ", Tr(RESOURCE_NAMES[r]), td->effects[r] * th->strength * THREAT_SCALE);
                strncat(eff, e, sizeof(eff) - strlen(eff) - 1);
            }
            snprintf(tip, sizeof(tip), Tr("Effets par tour si non neutralisée :\n%s"), eff);
            Tooltip(row, tip);
        } else {
            Text("Activité suspecte non identifiée", row.x + 8, row.y + 4, 10, C_WARN);
            snprintf(buf, sizeof(buf), Tr("Neutralisation %d%% (x0,35 tant que non détectée)"), (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 17, 10, C_DIM);
        }
        y += 35;
        shown++;
    }
    if (!shown) Text("Aucune menace détectée.", RP_X + 14, y, 10, C_GOOD);

    if (t == TH_EXTERIEUR)
        TextWrap("Les divisions stationnées dans la province visée aident à neutraliser l'insurrection (forces spéciales surtout).",
                 RP_X + 10, 408, RP_W - 20, 10, C_DIM);
    y = 446;
    Text("THÉÂTRES (cliquer pour sélectionner)", RP_X + 10, y, 10, C_DIM);
    y += 14;
    for (int i = 0; i < TH_COUNT; i++) {
        int col = i % 2, row = i / 2;
        Rectangle r = { RP_X + 6 + col * 276, y + row * 22, 272, 20 };
        bool h = Hover(r);
        DrawRectangleRec(r, i == selTheater ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, DOMAIN_COLORS[THEATERS[i].domain]);
        Text(Tr(THEATERS[i].name), r.x + 10, r.y + 5, 10, C_TEXT);
        snprintf(buf, sizeof(buf), Tr("P%d"), game.posture[i]);
        Text(buf, r.x + 160, r.y + 5, 10, C_DIM);
        int n = Game_ThreatCount(&game, i);
        if (n) {
            snprintf(buf, sizeof(buf), Tr("%d menace%s"), n, n > 1 ? "s" : "");
            Text(buf, r.x + 196, r.y + 5, 10, C_BAD);
        }
        if (h && clicked) { selTheater = i; clicked = false; }
    }
}

void DrawHybridTab(void)
{
    char buf[200];
    float y = RP_Y + 4;
    snprintf(buf, sizeof(buf), Tr("Points de commandement : %d / %d (partagés avec la diplomatie, la guerre et les décrets)"), game.commandPoints, BASE_CP);
    Text(buf, RP_X + 10, y, 10, C_ACCENT);
    y += 16;
    for (int f = 0; f < F_COUNT; f++) {
        Rectangle card = { RP_X + 6, y, RP_W - 12, 126 };
        Panel(card, C_PANEL2);
        DrawRectangle((int)card.x, (int)card.y, 4, (int)card.height, FACTION_COLORS[f]);
        Text(Tr(FACTIONS[f].name), card.x + 12, card.y + 6, 20, FACTION_COLORS[f]);
        if (game.fac[f].intelTurns > 0) {
            snprintf(buf, sizeof(buf), Tr("Renseignement : %d tour(s)"), game.fac[f].intelTurns);
            Text(buf, card.x + card.width - 10 - TextW(buf, 10), card.y + 10, 10, C_GOOD);
        }
        TextWrap(Tr(FACTIONS[f].desc), card.x + 12, card.y + 28, card.width - 24, 10, C_DIM);
        Text("Hostilité", card.x + 12, card.y + 60, 10, C_DIM);
        Bar((Rectangle){ card.x + 75, card.y + 60, 160, 9 }, game.fac[f].hostility, 100, C_BAD);
        snprintf(buf, sizeof(buf), "%.0f", game.fac[f].hostility);
        Text(buf, card.x + 240, card.y + 60, 10, C_TEXT);
        Text("Capacité", card.x + 280, card.y + 60, 10, C_DIM);
        Bar((Rectangle){ card.x + 340, card.y + 60, 160, 9 }, game.fac[f].capability, 100, C_WARN);
        snprintf(buf, sizeof(buf), "%.0f", game.fac[f].capability);
        Text(buf, card.x + 505, card.y + 60, 10, C_TEXT);
        int active = 0;
        for (int i = 0; i < MAX_THREATS; i++) if (game.threats[i].active && game.threats[i].faction == f) active++;
        snprintf(buf, sizeof(buf), Tr("Opérations en cours : %d"), active);
        Text(buf, card.x + 12, card.y + 75, 10, C_TEXT);
        float bw = (card.width - 24 - 4 * 6) / A_COUNT;
        for (int a = 0; a < A_COUNT; a++) {
            Rectangle br = { card.x + 12 + a * (bw + 6), card.y + 94, bw, 24 };
            char desc[300], cost[60];
            if (ACTIONS[a].budget > 0) snprintf(cost, sizeof(cost), Tr("Coût : 1 PC, %.0f Md"), ACTIONS[a].budget);
            else if (ACTIONS[a].influence > 0) snprintf(cost, sizeof(cost), Tr("Coût : 1 PC, %.0f Influence"), ACTIONS[a].influence);
            else snprintf(cost, sizeof(cost), "%s", Tr("Coût : 1 PC"));
            snprintf(desc, sizeof(desc), "%s\n%s", Tr(ACTIONS[a].desc), cost);
            if (ActionButton(br, Tr(ACTIONS[a].name), Game_ActionBlocked(&game, f, a), desc)) Do(CMD_FACTION_ACTION, f, a, 0, 0);
        }
        y += 132;
    }
}

void Sparkline(Rectangle r, const float *v, int n, float lo, float hi, Color c, const char *label)
{
    Panel(r, C_BG);
    Text(label, r.x + 4, r.y + 3, 10, C_DIM);
    for (int i = 1; i < n; i++) {
        float x0 = r.x + 4 + (r.width - 8) * (i - 1) / TOTAL_TURNS, x1 = r.x + 4 + (r.width - 8) * i / TOTAL_TURNS;
        float y0 = r.y + r.height - 4 - (r.height - 18) * (v[i - 1] - lo) / (hi - lo);
        float y1 = r.y + r.height - 4 - (r.height - 18) * (v[i] - lo) / (hi - lo);
        DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, 2, c);
    }
}

void DrawSocietyTab(void)
{
    char buf[260];
    float x = RP_X + 10, y = RP_Y + 4;
    float pop = Soc_NationPop(&game, N_FRANCE), w = 0;
    float share[S_COUNT] = { 0 }, mil[S_COUNT] = { 0 }, con[S_COUNT] = { 0 }, lit = 0, minor = 0, unemp = 0;
    float cult[CULT_COUNT] = { 0 }, rel[REL_COUNT] = { 0 };
    int revolts = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &game.prov[p];
        if (pr->owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++) {
            share[s] += pr->share[s] * pr->pop;
            mil[s] += pr->mil[s] * pr->pop;
            con[s] += pr->con[s] * pr->pop;
        }
        for (int c = 0; c < CULT_COUNT; c++) cult[c] += pr->culture[c] * pr->pop;
        for (int r = 0; r < REL_COUNT; r++) rel[r] += pr->religion[r] * pr->pop;
        lit += pr->literacy * pr->pop;
        minor += Soc_NonAccepted(&game, p) * pr->pop;
        unemp += pr->unemployment * pr->pop;
        if (pr->revolt) revolts++;
        w += pr->pop;
    }
    if (w <= 0) w = 1;
    Text("POPULATION FRANÇAISE", x, y, 20, C_ACCENT);
    y += 24;
    snprintf(buf, sizeof(buf), Tr("%.1f M d'habitants, %d provinces. Alphabétisation %.0f%%. Chômage %.0f%%. Révoltes : %d."),
             pop, Soc_NationProvinces(&game, N_FRANCE), lit / w * 100, unemp / w * 100, revolts);
    Text(buf, x, y, 10, C_TEXT); y += 13;
    snprintf(buf, sizeof(buf), Tr("Migrants admis %.0f k, refoulés %.0f k, départs %.0f k (dernier trimestre)."),
             game.migrantsIn * 1000, game.migrantsRefused * 1000, game.migrantsOut * 1000);
    Text(buf, x, y, 10, C_TEXT); y += 16;

    Text("STRATE", x, y, 10, C_DIM);
    Text("part", x + 125, y, 10, C_DIM);
    Text("besoins", x + 165, y, 10, C_DIM);
    Text("militance", x + 230, y, 10, C_DIM);
    Text("conscience", x + 390, y, 10, C_DIM);
    y += 13;
    for (int s = 0; s < S_COUNT; s++) {
        Text(Tr(STRATA_NAMES[s]), x, y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f%%", share[s] / w * 100);
        Text(buf, x + 125, y, 10, C_TEXT);
        float need = Eco_NeedSat(&game, N_FRANCE, s);
        snprintf(buf, sizeof(buf), "%.0f%%", need * 100);
        Text(buf, x + 165, y, 10, need < 0.97f ? C_BAD : C_GOOD);
        Bar((Rectangle){ x + 230, y + 1, 120, 8 }, mil[s] / w, 10, MilColor(mil[s] / w));
        snprintf(buf, sizeof(buf), "%.1f", mil[s] / w);
        Text(buf, x + 355, y, 10, C_TEXT);
        Bar((Rectangle){ x + 390, y + 1, 120, 8 }, con[s] / w, 10, C_EVENT);
        snprintf(buf, sizeof(buf), "%.1f", con[s] / w);
        Text(buf, x + 515, y, 10, C_TEXT);
        y += 13;
    }
    y += 4;

    /* cultures et religions */
    int order[CULT_COUNT];
    for (int c = 0; c < CULT_COUNT; c++) order[c] = c;
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < CULT_COUNT; j++)
            if (cult[order[j]] > cult[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    int len = snprintf(buf, sizeof(buf), "%s", Tr("Cultures :"));
    for (int i = 0; i < 4 && cult[order[i]] / w >= 0.005f; i++)
        len += snprintf(buf + len, sizeof(buf) - len, "  %s %.1f%%", Tr(CULTURE_NAMES[order[i]]), cult[order[i]] / w * 100);
    snprintf(buf + len, sizeof(buf) - len, Tr("   (minorités non acceptées : %.1f%%)"), minor / w * 100);
    y += TextWrap(buf, x, y, RP_W - 20, 10, C_TEXT);
    len = snprintf(buf, sizeof(buf), "%s", Tr("Religions :"));
    for (int r = 0; r < REL_COUNT; r++)
        if (rel[r] / w >= 0.01f) len += snprintf(buf + len, sizeof(buf) - len, "  %s %.0f%%", Tr(RELIGION_NAMES[r]), rel[r] / w * 100);
    Text(buf, x, y, 10, C_DIM);
    y += 16;
    Text("POLITIQUE CULTURELLE", x, y, 10, C_ACCENT);
    y += 14;
    for (int i = 0; i < CP_COUNT; i++) {
        Rectangle r = { x + i * 182, y, 176, 20 };
        const char *b = Soc_CulturePolicyBlocked(&game, i);
        bool cur = game.culturePolicy == i;
        if (Choice(r, Tr(CULTURE_POLICY_NAMES[i]), cur) && b == NULL) Do(CMD_CULTURE, i, 0, 0, 0);
        char tip[300];
        snprintf(tip, sizeof(tip), "%s\n%s%s%s", Tr(CULTURE_POLICY_NAMES[i]), Tr(CULTURE_POLICY_DESC[i]),
                 b && !cur ? "\nIndisponible : " : "", b && !cur ? b : "");
        Tooltip(r, tip);
    }
    y += 28;

    float ideo[I_COUNT];
    Pol_NationalIdeology(&game, ideo);
    Text("COURANTS POLITIQUES", x, y, 10, C_DIM);
    Text("trav.", x + 330, y, 10, C_DIM);
    Text("moy.", x + 375, y, 10, C_DIM);
    Text("élites", x + 420, y, 10, C_DIM);
    snprintf(buf, sizeof(buf), Tr("Soutien %.0f%%"), Pol_RegimeSupport(&game) * 100);
    Text(buf, x + 470, y, 10, Pol_RegimeSupport(&game) < 0.45f ? C_BAD : C_GOOD);
    y += 14;
    /* moyenne par strate */
    float bys[S_COUNT][I_COUNT] = { { 0 } }, ws[S_COUNT] = { 0 };
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &game.prov[p];
        if (pr->owner != N_FRANCE || pr->ctrl != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++) {
            float k = pr->pop * pr->share[s];
            ws[s] += k;
            for (int i = 0; i < I_COUNT; i++) bys[s][i] += pr->ideo[s][i] * k;
        }
    }
    int gov = game.nat[N_FRANCE].government;
    for (int i = 0; i < I_COUNT; i++) {
        Rectangle row = { x, y, RP_W - 20, 12 };
        Text(Tr(IDEOLOGY_NAMES[i]), x, y, 10, gov == i ? C_SEL : C_TEXT);
        Bar((Rectangle){ x + 120, y + 1, 160, 8 }, ideo[i], 0.6f, IDEOLOGY_COLORS[i]);
        snprintf(buf, sizeof(buf), "%.0f%%", ideo[i] * 100);
        Text(buf, x + 288, y, 10, gov == i ? C_SEL : C_TEXT);
        for (int s = 0; s < S_COUNT; s++) {
            snprintf(buf, sizeof(buf), "%.0f%%", ws[s] > 0 ? bys[s][i] / ws[s] * 100 : 0);
            Text(buf, x + 330 + s * 45, y, 10, C_DIM);
        }
        if (gov == i) Text("au pouvoir", x + 470, y, 10, C_SEL);
        Tooltip(row, Tr(IDEOLOGY_DESC[i]));
        y += 13;
    }
    if (game.nat[N_FRANCE].regime == RG_DEMOCRATIE)
        snprintf(buf, sizeof(buf), Tr("Prochaines élections dans %d tours."), game.nat[N_FRANCE].nextElection - game.turn);
    else
        snprintf(buf, sizeof(buf), "%s", Tr("Pas d'élections sous ce régime."));
    Text(buf, x, y + 2, 10, C_DIM);
    y += 18;

    int n = game.turn + 1 < HISTORY_LEN ? game.turn + 1 : HISTORY_LEN;
    float pmax = 0;
    for (int i = 0; i < n; i++) if (game.histPop[i] > pmax) pmax = game.histPop[i];
    float h = VH - 60 - y;
    if (h > 100) h = 100;
    if (h < 40) return;
    Sparkline((Rectangle){ RP_X + 6, y, (RP_W - 18) / 3, h }, game.histPop, n, 0, pmax * 1.1f + 1, C_ACCENT, "Population");
    Sparkline((Rectangle){ RP_X + 9 + (RP_W - 18) / 3, y, (RP_W - 18) / 3, h }, game.histCohesion, n, 0, 100, C_GOOD, "Cohésion");
    Sparkline((Rectangle){ RP_X + 12 + 2 * (RP_W - 18) / 3, y, (RP_W - 18) / 3, h }, game.histTemp, n, 1, 4, C_CLIMATE, "Température");
}

void Stepper(float x, float y, const char *label, float value, float *out, float step, const char *fmt, const char *tip)
{
    char buf[64];
    Rectangle row = { x, y, 300, 20 };
    Text(label, x, y + 5, 10, C_TEXT);
    snprintf(buf, sizeof(buf), fmt, value * 100);
    Text(buf, x + 170, y + 5, 10, C_SEL);
    *out = value;
    if (Button((Rectangle){ x + 220, y, 22, 20 }, "-", true, 10)) *out = value - step;
    if (Button((Rectangle){ x + 246, y, 22, 20 }, "+", true, 10)) *out = value + step;
    Tooltip(row, tip);
}

void DrawEconomyTab(void)
{
    char buf[200];
    float x = RP_X + 10, y = RP_Y + 4;
    float v;

    Text("IMPÔTS PAR STRATE", x, y, 10, C_ACCENT);
    y += 14;
    static const char *taxTip[S_COUNT] = {
        "Les travailleurs sont nombreux mais pauvres. Au-delà de 30%, leur militance monte.",
        "Les classes moyennes portent une large part de l'impôt.",
        "Peu nombreuses, les élites pèsent lourd dans les recettes. Trop les taxer les fait basculer.",
    };
    for (int s = 0; s < S_COUNT; s++) {
        Stepper(x, y, Tr(STRATA_NAMES[s]), game.taxRate[s], &v, 0.05f, "%.0f%%", taxTip[s]);
        if (v != game.taxRate[s]) Do(CMD_TAX, s, 0, 0, v);
        y += 22;
    }
    y += 6;
    Text("DÉPENSES PUBLIQUES", x, y, 10, C_ACCENT);
    y += 14;
    for (int s = 0; s < SP_COUNT; s++) {
        char tip[300];
        snprintf(tip, sizeof(tip), Tr("%s\nCoût actuel : %.1f Md/tour."), Tr(SPENDING_DESC[s]), Eco_SpendingCost(&game, s));
        Stepper(x, y, Tr(SPENDING_NAMES[s]), game.spending[s], &v, 0.1f, "%.0f%%", tip);
        if (v != game.spending[s]) Do(CMD_SPENDING, s, 0, 0, v);
        y += 22;
    }
    Stepper(x, y, "Droits de douane", game.tariff, &v, 0.05f, "%.0f%%",
            "Prélèvement sur les importations : recettes immédiates, mais prospérité en baisse.");
    if (v != game.tariff) Do(CMD_TARIFF, 0, 0, 0, v);
    y += 30;

    /* budget */
    float l[BL_COUNT];
    Eco_Budget(&game, l);
    float bx = RP_X + 320, by = RP_Y + 4;
    Text("BUDGET PRÉVISIONNEL", bx, by, 10, C_ACCENT);
    by += 14;
    float inc = 0, exp = 0;
    for (int i = 0; i < BL_COUNT; i++) {
        bool income = i < BL_FIRST_EXPENSE;
        snprintf(buf, sizeof(buf), "%s%.1f", income ? "+" : "-", l[i]);
        Text(Tr(BUDGET_LINE_NAMES[i]), bx, by, 10, C_DIM);
        Text(buf, bx + 170, by, 10, income ? C_GOOD : C_BAD);
        if (income) inc += l[i]; else exp += l[i];
        by += 12;
    }
    snprintf(buf, sizeof(buf), Tr("Solde : %+.1f Md/tour"), inc - exp);
    Text(buf, bx, by + 4, 10, inc - exp >= 0 ? C_TEXT : C_WARN);
    if (game.res[R_BUDGET] < 0) Text("Dette : intérêts 3%/tour", bx, by + 18, 10, C_BAD);

    /* marche */
    Text("MARCHÉ MONDIAL", x, y, 10, C_ACCENT);
    y += 14;
    static const char *cols[] = { "Bien", "Prix", "Prod.", "Besoins", "Import", "Export", "Stock", "Couvert" };
    static const float cx[] = { 0, 115, 165, 215, 275, 330, 385, 440 };
    for (int c = 0; c < 8; c++) Text(cols[c], x + cx[c], y, 10, C_DIM);
    y += 13;
    const NationEconomy *e = &game.eco[N_FRANCE];
    for (int k = 0; k < G_COUNT; k++) {
        Text(Tr(GOOD_NAMES[k]), x, y, 10, C_TEXT);
        if (k == G_WATER) snprintf(buf, sizeof(buf), "%s", Tr("local"));
        else snprintf(buf, sizeof(buf), "%.0f%%", game.price[k] / GOOD_BASE_PRICE[k] * 100);
        Text(buf, x + cx[1], y, 10, k != G_WATER && game.price[k] > GOOD_BASE_PRICE[k] * 1.3f ? C_BAD : C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->supply[k]);  Text(buf, x + cx[2], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->demand[k]);  Text(buf, x + cx[3], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->imports[k]); Text(buf, x + cx[4], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->exports[k]); Text(buf, x + cx[5], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->stock[k]);   Text(buf, x + cx[6], y, 10, C_ACCENT);
        float sat = e->sat[k];
        snprintf(buf, sizeof(buf), "%.0f%%", (sat > 1 ? 1 : sat) * 100);
        Text(buf, x + cx[7], y, 10, sat < 0.97f ? C_BAD : C_GOOD);
        y += 13;
    }
    y += 6;
    float ind = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (game.prov[p].owner == N_FRANCE) ind += game.prov[p].industry;
    int embargos = 0;
    for (int n = 1; n < N_COUNT; n++) if (game.nat[n].alive && (game.nat[n].embargo || game.nat[n].embargoFrance)) embargos++;
    snprintf(buf, sizeof(buf), Tr("Industrie : %.0f usines. Chômage %.0f%%. Blocus de nos côtes : %.0f%%. Embargos : %d."), ind,
             Eco_NationUnemployment(&game, N_FRANCE) * 100, game.nat[N_FRANCE].blockade * 100, embargos);
    Text(buf, x, y, 10, game.nat[N_FRANCE].blockade > 0.1f ? C_WARN : C_DIM);
    y += 14;
    Tooltip((Rectangle){ x, y - 14, RP_W - 20, 14 },
            "Prix en % de 2030 : ils montent quand le monde manque d'un bien. Les excédents s'exportent, la moitié du reste est "
            "stockée (jusqu'à deux tours de besoins) et les stocks comblent les pénuries. On n'achète pas à un ennemi ni sous "
            "embargo ; un blocus naval coupe jusqu'à 70% des échanges. Une usine demande des travailleurs : le chômage alimente "
            "la militance et l'émigration.");
    y += 6;
    Text("RESSOURCES STRATÉGIQUES", x, y, 10, C_ACCENT);
    y += 14;
    static const char *scol[] = { "Ressource", "Stock", "Prod.", "Import", "Besoins", "Couvert" };
    static const float sx[] = { 0, 120, 170, 220, 275, 335 };
    for (int c = 0; c < 6; c++) Text(scol[c], x + sx[c], y, 10, C_DIM);
    y += 13;
    for (int r = 0; r < SR_COUNT; r++) {
        const Nation *fr = &game.nat[N_FRANCE];
        Text(Tr(STRATS[r].name), x, y + 4, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.1f", fr->strat[r]);                          Text(buf, x + sx[1], y + 4, 10, fr->strat[r] < 2 ? C_BAD : C_TEXT);
        snprintf(buf, sizeof(buf), "%.1f", Strat_Production(&game, N_FRANCE, r));  Text(buf, x + sx[2], y + 4, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.1f", Strat_Imports(&game, N_FRANCE, r));     Text(buf, x + sx[3], y + 4, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.1f", Strat_Use(&game, N_FRANCE, r));         Text(buf, x + sx[4], y + 4, 10, C_TEXT);
        float sat = Strat_Sat(&game, N_FRANCE, r);
        snprintf(buf, sizeof(buf), "%.0f%%%s", sat * 100, game.stratShock[r] > 0 ? " CRISE" : "");
        Text(buf, x + sx[5], y + 4, 10, sat < 0.99f || game.stratShock[r] > 0 ? C_BAD : C_GOOD);
        snprintf(buf, sizeof(buf), Tr("+2 : %.1f Md"), Strat_Price(&game, r) * 2);
        char tip[300];
        snprintf(tip, sizeof(tip), Tr("%s : %s\nAchat de 2 %s sur le marché mondial (prix x2,5 en période de crise). Le recrutement des "
                 "unités modernes en consomme ; une pénurie bride l'énergie nucléaire, la recherche, l'industrie ou la dissuasion."),
                 Tr(STRATS[r].name), Tr(STRATS[r].desc), STRATS[r].unit);
        if (ActionButton((Rectangle){ x + 420, y, 120, 18 }, buf, Strat_BuyBlocked(&game, r), tip)) Do(CMD_STRAT_BUY, r, 0, 0, 0);
        y += 20;
    }
    y += 4;
    Text("DETTE PUBLIQUE", x, y, 10, C_ACCENT);
    y += 14;
    int rating = Eco_Rating(&game);
    snprintf(buf, sizeof(buf), Tr("%.0f Md (%.1f ans de recettes) - note %s - taux %.1f%%/an - intérêts %.1f Md/tour%s"), game.debt,
             Eco_DebtRatio(&game), Tr(RATING_NAMES[rating]), Eco_InterestRate(&game) * 100, game.debt * Eco_InterestRate(&game) / 4,
             game.austerityTurns > 0 ? " - AUSTÉRITÉ" : "");
    Text(buf, x, y, 10, rating >= 4 ? C_BAD : rating >= 3 ? C_WARN : C_TEXT);
    y += 14;
    snprintf(buf, sizeof(buf), Tr("Inflation : %.1f %%/an - indice des prix %.0f (2030 = 100)"), game.inflation, game.priceLevel * 100);
    Text(buf, x, y, 10, game.inflation > 8 ? C_BAD : game.inflation > 4 ? C_WARN : C_DIM);
    Tooltip((Rectangle){ x, y, 400, 12 }, "L'inflation monte avec le déficit, les pénuries d'énergie et d'alimentation, la cherté "
                                          "des marchés mondiaux et l'excès de dette ; l'austérité la freine. Elle érode la dette, "
                                          "mais renchérit les taux d'intérêt et attise la militance des travailleurs et des classes "
                                          "moyennes ; au-delà de 10 %, la cohésion s'effrite.");
    y += 16;
    if (ActionButton((Rectangle){ x, y, 150, 18 }, "Emprunter 10 Md", Eco_BorrowBlocked(&game),
                     "Émet des obligations : +10 Md de trésorerie, +10 Md de dette. Un déficit est de toute façon financé par "
                     "l'emprunt tant que la note le permet ; au-delà (CCC), c'est la crise de la dette."))
        Do(CMD_BORROW, 0, 0, 0, 0);
    if (ActionButton((Rectangle){ x + 160, y, 150, 18 }, "Rembourser 10 Md", Eco_RepayBlocked(&game),
                     "Réduit la dette, donc les intérêts, et améliore la note."))
        Do(CMD_REPAY, 0, 0, 0, 0);
}

int polView;   /* 0 regime et doctrines, 1 gouvernement et opinion */

void DrawCabinet(float x, float y)
{
    char buf[260];
    float w = RP_W - 20;
    snprintf(buf, sizeof(buf), Tr("POPULARITÉ DU GOUVERNEMENT : %.0f %%"), game.approval);
    Text(buf, x, y, 10, C_ACCENT);
    y += 14;
    Bar((Rectangle){ x, y, w, 8 }, game.approval, 100, game.approval < 35 ? C_BAD : game.approval < 50 ? C_WARN : C_GOOD);
    Tooltip((Rectangle){ x, y - 14, w, 22 }, "La popularité suit la cohésion, l'inflation, le chômage, la satisfaction des lobbies et la "
                                            "guerre. Elle tire la cohésion vers le haut au-dessus de 50 %, vers le bas en dessous.");
    y += 18;
    Text("LOBBIES", x, y, 10, C_ACCENT);
    y += 14;
    for (int l = 0; l < LB_COUNT; l++) {
        Rectangle row = { x, y, w, 18 };
        Text(Tr(LOBBY_NAMES[l]), x, y + 3, 10, C_TEXT);
        float v = game.lobby[l];
        Bar((Rectangle){ x + 110, y + 5, 300, 8 }, v, 100, v < 30 ? C_BAD : v > 70 ? C_GOOD : C_ACCENT);
        snprintf(buf, sizeof(buf), Tr("%.0f (tendance %+.0f)"), v, Cab_LobbyTarget(&game, l) - v);
        Text(buf, x + 420, y + 3, 10, v < 30 ? C_BAD : v > 70 ? C_GOOD : C_DIM);
        Tooltip(row, Tr(LOBBY_DESC[l]));
        y += 20;
    }
    y += 8;
    Text("GOUVERNEMENT", x, y, 10, C_ACCENT);
    y += 14;
    for (int p = 0; p < MIN_COUNT; p++) {
        const Minister *m = &game.minister[p];
        Rectangle row = { RP_X + 6, y, RP_W - 12, 64 };
        DrawRectangleRec(row, C_PANEL2);
        snprintf(buf, sizeof(buf), Tr("%s : %s - compétence %d/3 - proche des %s"), Tr(MINISTER_POSTS[p]), Cab_Name(m), m->skill,
                 Tr(LOBBY_NAMES[m->lobby]));
        Text(buf, row.x + 6, y + 4, 10, C_TEXT);
        Text(Tr(MINISTER_EFFECTS[p]), row.x + 6, y + 18, 10, C_DIM);
        for (int c = 0; c < 3; c++) {
            const Minister *k = &game.candidates[p][c];
            snprintf(buf, sizeof(buf), Tr("Nommer %s (%d/3)"), Cab_Name(k), k->skill);
            char tip[300];
            snprintf(tip, sizeof(tip), Tr("Proche des %s. Remaniement : 1 PC, -1 Cohésion ; son lobby est flatté, celui du sortant vexé. "
                                          "De nouveaux candidats se présentent chaque année."), Tr(LOBBY_NAMES[k->lobby]));
            if (ActionButton((Rectangle){ row.x + 6 + c * 180, y + 36, 174, 22 }, buf, Cab_AppointBlocked(&game, p, c), tip))
                Do(CMD_MINISTER, p, c, 0, 0);
        }
        y += 70;
    }
}

void DrawPolitiqueTab(void)
{
    char buf[220];
    float x = RP_X + 10, y = RP_Y + 4;
    Regime cur = game.nat[N_FRANCE].regime;
    static const char *views[2] = { "Régime et doctrines", "Gouvernement et opinion" };
    for (int v = 0; v < 2; v++)
        if (Choice((Rectangle){ x + v * 200, y, 194, 18 }, views[v], polView == v)) polView = v;
    y += 24;
    if (polView == 1) { DrawCabinet(x, y); return; }
    Text("RÉGIME", x, y, 10, C_ACCENT);
    if (game.regimeCooldown > 0) {
        snprintf(buf, sizeof(buf), Tr("prochain changement dans %d tours"), game.regimeCooldown);
        Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y, 10, C_DIM);
    }
    y += 14;
    for (int r = 0; r < RG_COUNT; r++) {
        const RegimeDef *rg = &REGIMES[r];
        bool isCur = cur == (Regime)r;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 30 };
        DrawRectangleRec(row, isCur ? (Color){ 40, 62, 92, 255 } : C_PANEL2);
        DrawRectangleLinesEx(row, 1, isCur ? C_ACCENT : C_BORDER);
        Text(Tr(rg->name), row.x + 8, row.y + 3, 10, isCur ? C_TEXT : C_DIM);
        snprintf(buf, sizeof(buf), Tr("Impôt x%.2f  Mobil. %.2f  Répression %.2f  Qualité x%.2f  Influence %+.1f"),
                 rg->taxEff, rg->mobilization, rg->repression, rg->quality, rg->influence);
        Text(buf, row.x + 8, row.y + 16, 10, C_DIM);
        char desc[600];
        snprintf(desc, sizeof(desc), Tr("%s\n%s\nLassitude de guerre %.1f, coût d'agression %.0f, intégration x%.1f, projets x%.2f."),
                 Tr(rg->desc), Tr(rg->mechanic), rg->warWeariness, rg->aggressionCost, rg->assimilation, rg->projectCost);
        if (!isCur) {
            snprintf(buf, sizeof(buf), Tr("Adopter (-%.0f coh.)"), Soc_RegimeChangeCost(&game, r));
            if (ActionButton((Rectangle){ row.x + row.width - 128, row.y + 5, 122, 20 }, buf, Soc_RegimeBlocked(&game, r), desc))
                Do(CMD_REGIME, r, 0, 0, 0);
        } else {
            Tooltip(row, desc);
        }
        y += 33;
    }
    y += 2;
    y += TextWrap(Tr(REGIMES[cur].mechanic), x, y, RP_W - 20, 10, C_SEL) + 4;
    int gov = game.nat[N_FRANCE].government;
    snprintf(buf, sizeof(buf), Tr("Gouvernement : %s"), gov >= 0 ? Tr(IDEOLOGY_NAMES[gov]) : "aucun");
    Text(buf, x, y, 10, C_TEXT);
    y += 16;
    for (int k = 0; k < 2; k++) {
        const DecreeDef *d = &DECREES[cur][k];
        char desc[300];
        snprintf(desc, sizeof(desc), Tr("%s\nCoût : 1 PC%s. Recharge : %d tours."), Tr(d->desc), d->budget > 0 ? ", budget" : "", d->cooldown);
        snprintf(buf, sizeof(buf), d->budget > 0 ? "%s (%.0f Md)" : "%s", Tr(d->name), d->budget);
        if (ActionButton((Rectangle){ x + k * 274, y, 266, 24 }, buf, Pol_DecreeBlocked(&game, k), desc)) Do(CMD_DECREE, k, 0, 0, 0);
    }
    y += 32;

    Text("DOCTRINE MIGRATOIRE", x, y, 10, C_ACCENT);
    Text("NIVEAU D'ALERTE", x + 280, y, 10, C_ACCENT);
    y += 14;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { x, y + i * 22, 260, 20 };
        if (Choice(r, Tr(MIGRATION_DOCTRINES[i]), game.migrationDoctrine == i)) Do(CMD_MIGRATION, i, 0, 0, 0);
        Tooltip(r, Tr(MIGRATION_DESC[i]));
        Rectangle a = { x + 280, y + i * 22, 260, 20 };
        if (Choice(a, Tr(ALERT_LEVELS[i]), game.alertLevel == i)) Do(CMD_ALERT, i, 0, 0, 0);
        Tooltip(a, Tr(ALERT_DESC[i]));
    }
    y += 72;

    int parts[SC_COUNT];
    int score = Game_Score(&game, parts);
    VictoryType v = Game_Victory(&game);
    snprintf(buf, sizeof(buf), Tr("SCORE %d  -  victoire visée : %s"), score, Tr(VICTORY_NAMES[v]));
    Rectangle sr = { x, y, RP_W - 20, 12 };
    Text(buf, x, y, 10, C_SEL);
    Tooltip(sr, Tr(VICTORY_DESC[v]));
    y += 14;
    for (int i = 0; i < SC_COUNT; i++) {
        snprintf(buf, sizeof(buf), "%s %d", Tr(SCORE_PART_NAMES[i]), parts[i]);
        Text(buf, x + (i % 3) * 180, y + (i / 3) * 12, 10, parts[i] < 0 ? C_BAD : C_DIM);
    }
}

void DrawProjectsTab(void)
{
    char buf[160];
    float y = RP_Y + 4;
    Text("Projets civils de résilience : investissements uniques, effets permanents.", RP_X + 10, y, 10, C_DIM);
    y += 16;
    for (int p = 0; p < P_COUNT; p++) {
        Rectangle row = { RP_X + 6, y, RP_W - 12, 54 };
        const Project *pr = &game.proj[p];
        Panel(row, pr->state == 2 ? (Color){ 26, 48, 36, 255 } : C_PANEL2);
        Text(Tr(PROJECTS[p].name), row.x + 10, row.y + 6, 20, pr->state == 2 ? C_GOOD : C_TEXT);
        Text(Tr(PROJECTS[p].desc), row.x + 10, row.y + 32, 10, C_DIM);
        Rectangle br = { row.x + row.width - 170, row.y + 8, 160, 24 };
        if (pr->state == 0) {
            float c = Game_ProjectCost(&game, p);
            snprintf(buf, sizeof(buf), Tr("Lancer : %.0f Md / %d t"), c, PROJECTS[p].turns);
            if (Button(br, buf, game.res[R_BUDGET] >= c, 10)) Do(CMD_PROJECT, p, 0, 0, 0);
        } else if (pr->state == 1) {
            snprintf(buf, sizeof(buf), Tr("Chantier : %d tour(s)"), pr->turnsLeft);
            Text(buf, br.x + 10, br.y + 4, 10, C_WARN);
            Bar((Rectangle){ br.x + 10, br.y + 18, 140, 5 }, (float)(PROJECTS[p].turns - pr->turnsLeft), (float)PROJECTS[p].turns, C_WARN);
        } else {
            Text("OPÉRATIONNEL", br.x + 30, br.y + 6, 10, C_GOOD);
        }
        y += 58;
    }
}
