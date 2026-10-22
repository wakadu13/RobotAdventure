/*
 * BOUCLIER 2050 - journal, fenetres modales, infobulles, tutoriel, ecrans (titre, aide,
 * fin, hot-seat).
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Journal, modales, infobulles                                        */
/* ------------------------------------------------------------------ */
/* message du journal dans la langue d'affichage : format traduit, arguments textuels traduits */
const char *LogText(const LogEntry *e)
{
    static char out[2 * LOG_LEN];
    if (gLang == LANG_FR || !e->fmt[0]) return e->text;
    const char *f = Tr(e->fmt);
    if (f == e->fmt) return e->text;
    const char *a = e->args;
    int o = 0;
    for (const char *p = f; *p && o < (int)sizeof(out) - 1; p++) {
        if (*p != '%') { out[o++] = *p; continue; }
        if (p[1] == '%') { out[o++] = '%'; p++; continue; }
        p++;
        while (*p && strchr("-+ #0123456789.lhz", *p)) p++;
        if (!*p) break;
        char piece[LOG_LEN];
        int k = 0;
        while (*a && *a != '\x1f' && k < LOG_LEN - 1) piece[k++] = *a++;
        piece[k] = 0;
        if (*a == '\x1f') a++;
        const char *t = *p == 's' ? Tr(piece) : piece;
        o += snprintf(out + o, sizeof(out) - o, "%s", t);
        if (o >= (int)sizeof(out)) o = (int)sizeof(out) - 1;
    }
    out[o] = 0;
    return out;
}

/* ------------------------------------------------------------------ */
Color LogColor(LogKind k)
{
    switch (k) {
    case EV_GOOD:    return C_GOOD;
    case EV_BAD:     return C_BAD;
    case EV_CLIMATE: return C_CLIMATE;
    case EV_EVENT:   return C_EVENT;
    case EV_WAR:     return C_WAR;
    default:         return C_TEXT;
    }
}

void DrawLog(void)
{
    Rectangle r = { 0, MAP_Y + MAP_VH + 2, 708, VH - MAP_Y - MAP_VH - 2 };
    Panel(r, C_PANEL);
    int lines = (int)((r.height - 8) / 13);
    if (Hover(r)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) logScroll++;
        if (wheel < 0) logScroll--;
    }
    if (logScroll > game.logCount - lines) logScroll = game.logCount - lines;
    if (logScroll < 0) logScroll = 0;
    for (int i = 0; i < lines; i++) {
        int idx = game.logCount - 1 - i - logScroll;
        if (idx < 0) break;
        const LogEntry *e = &game.log[idx];
        Color c = LogColor(e->kind);
        if (e->turn < game.turn - 1) c = Fade(c, 0.55f);
        Text(LogText(e), 8, r.y + 5 + i * 13, 10, c);
    }
}

void DrawDilemma(void)
{
    const DilemmaDef *d = Game_CurrentDilemma(&game);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
    Rectangle r = { 290, 170, 700, 380 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, game.pendingDilemma >= DLM_ALLY_CALL && game.pendingDilemma < DLM_EVENT ? C_WAR : C_EVENT);
    Text("DÉCISION DU CONSEIL DE DÉFENSE", r.x + 20, r.y + 16, 10, C_EVENT);
    Text(Tr(d->title), r.x + 20, r.y + 32, 20, C_TEXT);
    float ty = r.y + 62;
    int dp = game.pendingDilemma, n = game.dilemmaParam;
    if (dp == DLM_ALLY_CALL || dp == DLM_PEACE_OFFER || dp == DLM_NUKE_BLACKMAIL || dp == DLM_AI_ALLIANCE || dp == DLM_AI_TRADE
        || dp == DLM_PEACE_BLACKMAIL) {
        char buf[200];
        if (dp == DLM_ALLY_CALL) snprintf(buf, sizeof(buf), Tr("%s envahit %s."), Tr(NATIONS[n].name), Tr(NATIONS[game.dilemmaParam2].name));
        else if (dp == DLM_PEACE_OFFER) {
            const Treaty *t = &game.treaty;
            int ceded = 0;
            for (int p = 0; p < PROV_COUNT; p++) if (t->cede[p]) ceded++;
            if (t->winner == N_FRANCE)
                snprintf(buf, sizeof(buf), Tr("%s propose de nous céder %d province(s)%s. Score de guerre %+.0f."), Tr(NATIONS[n].name), ceded,
                         t->reparations > 0 ? " et de verser des réparations" : "", War_Score(&game, N_FRANCE, n));
            else
                snprintf(buf, sizeof(buf), Tr("%s exige %d province(s)%s. Score de guerre %+.0f."), Tr(NATIONS[n].name), ceded,
                         t->reparations > 0 ? " et des réparations" : "", War_Score(&game, N_FRANCE, n));
        }
        else if (dp == DLM_AI_ALLIANCE || dp == DLM_AI_TRADE)
            snprintf(buf, sizeof(buf), Tr("Proposition de %s (relation %+.0f)."), Tr(NATIONS[n].name), game.nat[n].relation);
        else if (dp == DLM_PEACE_BLACKMAIL)
            snprintf(buf, sizeof(buf), Tr("%s menace. Crédibilité de notre dissuasion : %.0f/100."), Tr(NATIONS[n].name), game.credibility);
        else snprintf(buf, sizeof(buf), Tr("Crédibilité de notre dissuasion : %.0f/100."), game.credibility);
        Text(buf, r.x + 20, ty, 10, C_WAR);
        ty += 16;
    }
    TextWrap(Tr(d->text), r.x + 20, ty, r.width - 40, 10, C_DIM);
    for (int i = 0; i < 2; i++) {
        Rectangle b = { r.x + 20 + i * 340, r.y + 150, 320, 150 };
        bool h = Hover(b);
        DrawRectangleRec(b, h ? (Color){ 44, 66, 96, 255 } : C_PANEL2);
        DrawRectangleLinesEx(b, 1, h ? C_ACCENT : C_BORDER);
        TextWrap(i == 0 ? Tr(d->optA) : Tr(d->optB), b.x + 12, b.y + 12, b.width - 24, 20, C_TEXT);
        TextWrap(i == 0 ? Tr(d->effA) : Tr(d->effB), b.x + 12, b.y + 80, b.width - 24, 10, C_WARN);
        if (h && clicked) { clicked = false; Do(CMD_DILEMMA, i, 0, 0, 0); }
    }
    Text("Choisissez une option pour poursuivre.", r.x + 20, r.y + r.height - 26, 10, C_DIM);
}

void DeltaLine(float x, float y, const char *label, float before, float after, const char *fmt)
{
    char buf[64];
    Text(label, x, y, 10, C_DIM);
    snprintf(buf, sizeof(buf), fmt, after);
    Text(buf, x + 110, y, 10, C_TEXT);
    float d = after - before;
    if (fabsf(d) >= 0.05f) {
        snprintf(buf, sizeof(buf), "%+.1f", d);
        Text(buf, x + 160, y, 10, d > 0 ? C_GOOD : C_BAD);
    }
}

void DrawReport(void)
{
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.55f));
    Rectangle r = { 190, 90, 900, 540 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, C_ACCENT);
    char date[32], buf[200];
    Game_DateString(game.turn, date, sizeof(date));
    snprintf(buf, sizeof(buf), Tr("RAPPORT DU TRIMESTRE - %s"), date);
    Text(buf, r.x + 20, r.y + 16, 20, C_TEXT);

    float x = r.x + 20, y = r.y + 50;
    for (int i = 0; i < R_COUNT; i++) {
        DeltaLine(x, y, Tr(RESOURCE_NAMES[i]), game.reportRes[i], game.res[i], "%.0f");
        y += 15;
    }
    y += 6;
    DeltaLine(x, y, "Population (M)", game.reportPop, Soc_NationPop(&game, N_FRANCE), "%.1f"); y += 15;
    DeltaLine(x, y, "Provinces", (float)game.reportProv, (float)Soc_NationProvinces(&game, N_FRANCE), "%.0f"); y += 15;
    DeltaLine(x, y, "Divisions", (float)game.reportUnits, (float)War_UnitCount(&game, N_FRANCE), "%.0f"); y += 15;
    DeltaLine(x, y, "Score", (float)game.reportScore, (float)Game_Score(&game, NULL), "%.0f"); y += 22;
    snprintf(buf, sizeof(buf), Tr("+%.2f°C"), game.temp);
    Text("Climat", x, y, 10, C_DIM);
    Text(buf, x + 110, y, 10, C_CLIMATE);

    /* evenements notables du tour */
    float ex = r.x + 250, ey = r.y + 50;
    Text("ÉVÉNEMENTS", ex, ey, 10, C_ACCENT);
    ey += 16;
    int shown = 0;
    for (int i = game.reportLogStart; i < game.logCount && shown < 30; i++) {
        const LogEntry *e = &game.log[i];
        if (e->kind == EV_INFO) continue;
        ey += TextWrap(LogText(e), ex, ey, r.width - 270, 10, LogColor(e->kind));
        shown++;
        if (ey > r.y + r.height - 60) break;
    }
    if (!shown) Text("Trimestre calme.", ex, ey, 10, C_DIM);

    Rectangle cb = { r.x + 20, r.y + r.height - 36, 16, 16 };
    DrawRectangleLinesEx(cb, 1, C_TEXT);
    if (showReports) DrawRectangle((int)cb.x + 4, (int)cb.y + 4, 8, 8, C_ACCENT);
    Text("Afficher ce rapport à chaque tour", cb.x + 24, cb.y + 3, 10, C_DIM);
    if (Hover((Rectangle){ cb.x, cb.y, 220, 16 }) && clicked) { showReports = !showReports; clicked = false; SaveSettings(); }

    if (Button((Rectangle){ r.x + r.width - 180, r.y + r.height - 44, 160, 32 }, "CONTINUER", true, 20)
        || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
        game.reportReady = false;
}

void DrawTooltip(void)
{
    if (!tooltip[0]) return;
    char copy[600];
    snprintf(copy, sizeof(copy), "%s", tooltip);
    const char *lines[14];
    int n = 0, w = 0;
    char *s = copy;
    while (s && n < 14) {
        lines[n++] = s;
        char *nl = strchr(s, '\n');
        if (nl) { *nl = 0; s = nl + 1; } else s = NULL;
    }
    for (int i = 0; i < n; i++) { int lw = TextW(lines[i], 10); if (lw > w) w = lw; }
    if (w > 420) w = 420;
    int h = 0;
    for (int i = 0; i < n; i++) {
        int lw = TextW(lines[i], 10);
        h += (lw > 420 ? (lw / 420 + 1) : 1) * 14;
    }
    float x = mouse.x + 16, y = mouse.y + 16;
    if (x + w + 20 > VW) x = VW - w - 20;
    if (y + h + 12 > VH) y = mouse.y - h - 16;
    if (y < 0) y = 0;
    Panel((Rectangle){ x, y, (float)w + 16, (float)h + 10 }, (Color){ 12, 16, 22, 245 });
    float yy = y + 6;
    for (int i = 0; i < n; i++)
        yy += TextWrap(lines[i], x + 8, yy, 420, 10, i == 0 ? C_ACCENT : C_TEXT);
}

/* ------------------------------------------------------------------ */
/* Tutoriel                                                            */
/* ------------------------------------------------------------------ */

const TutoStep TUTO[] = {
    { "Bienvenue au Conseil de défense",
      "2030 : vous dirigez la France pendant trente ans. La barre du haut résume l'essentiel : budget, énergie, eau, "
      "alimentation, cohésion, influence et souveraineté. Si la cohésion ou la souveraineté tombe à zéro, la partie est perdue." },
    { "La carte",
      "Zoomez avec la molette (ou en pinçant l'écran) et faites glisser pour vous déplacer. Zoomez au moins à x2 pour voir les "
      "noms des régions." },
    { "Sélectionner une région",
      "Cliquez sur une autre région française que l'Île-de-France : ses statistiques s'affichent dans l'onglet PROV." },
    { "L'armée",
      "Ouvrez l'onglet ARMÉE (touche 2). On y recrute 17 types d'unités terrestres, aériennes et navales." },
    { "Recruter",
      "Recrutez une division dans la région sélectionnée, par exemple de l'infanterie (INF). Elle sera opérationnelle après sa formation." },
    { "Donner un ordre",
      "Cliquez sur une région où se trouvent nos divisions pour les sélectionner, puis faites un clic droit sur une région voisine "
      "(ou touchez ORDRE, puis la destination) pour les y envoyer." },
    { "La défense M2MC",
      "Ouvrez l'onglet DÉFENSE : treize théâtres, de la frontière au cyberespace, où les factions mènent la guerre hybride. "
      "La posture de chaque théâtre augmente les chances de neutraliser les menaces." },
    { "L'économie",
      "Ouvrez l'onglet ÉCO. : impôts par groupe social, dépenses, budget, marché mondial, ressources stratégiques et dette publique." },
    { "Le monde",
      "Ouvrez l'onglet NATIONS : diplomatie, alliances, opérations ciblées, Conseil de sécurité, OTAN et traités de paix." },
    { "Le gouvernement",
      "Ouvrez l'onglet POLIT., vue « Gouvernement et opinion » : popularité, lobbies à ménager et ministres à remanier." },
    { "Fin du tour",
      "Cliquez sur FIN DU TOUR (ou Espace). Le trimestre se résout : batailles, économie, climat, menaces. Un rapport s'affiche." },
    { "À vous de jouer",
      "Vous connaissez l'essentiel. L'aide du menu détaille le reste : généraux et groupes d'armées (ÉT.-MAJ.), régimes et "
      "décrets (POLIT.), projets de résilience (PROJETS). Bonne chance !" },
};

int FrenchUnitCount(void)
{
    int c = 0;
    for (int i = 0; i < MAX_UNITS; i++) if (game.units[i].active && game.units[i].owner == N_FRANCE) c++;
    return c;
}

void StartTutorial(void)
{
    tutorial = 0;
    tutoUnits = FrenchUnitCount();
    tutoTurn = game.turn;
    tutoProv = selProvince;
}

/* l'etape courante est-elle accomplie ? */
bool TutoDone(int step)
{
    switch (step) {
    case 1: return zoom >= 2;
    case 2: return selProvince != tutoProv && game.prov[selProvince].owner == N_FRANCE;
    case 3: return tab == TAB_ARMEE;
    case 4: return FrenchUnitCount() > tutoUnits;
    case 5: for (int i = 0; i < MAX_UNITS; i++) if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].target >= 0) return true;
            return false;
    case 6: return tab == TAB_DEFENSE;
    case 7: return tab == TAB_ECONOMIE;
    case 8: return tab == TAB_NATIONS;
    case 9: return tab == TAB_POLITIQUE && polView == 1;
    case 10: return game.turn > tutoTurn;
    default: return false;
    }
}

void DrawTutorial(void)
{
    if (tutorial < 0 || tutorial >= TUTO_COUNT) return;
    const TutoStep *st = &TUTO[tutorial];
    bool manual = tutorial == 0 || tutorial == TUTO_COUNT - 1;
    if (!manual && TutoDone(tutorial)) { tutorial++; return; }
    Rectangle r = { MAP_X + 10, MAP_Y + 26, 420, 132 };
    DrawRectangleRec(r, Fade((Color){ 12, 18, 28, 255 }, 0.95f));
    DrawRectangleLinesEx(r, 2, C_SEL);
    char head[96];
    snprintf(head, sizeof(head), Tr("TUTORIEL %d/%d - %s"), tutorial + 1, TUTO_COUNT, Tr(st->title));
    Text(head, r.x + 10, r.y + 8, 10, C_SEL);
    TextWrap(Tr(st->text), r.x + 10, r.y + 26, r.width - 20, 10, C_TEXT);
    if (manual) {
        if (Button((Rectangle){ r.x + r.width - 110, r.y + r.height - 28, 100, 20 }, tutorial == TUTO_COUNT - 1 ? "Terminer" : "Suivant", true, 10))
            tutorial = tutorial == TUTO_COUNT - 1 ? -1 : tutorial + 1;
    } else {
        Text("En attente de votre action...", r.x + 10, r.y + r.height - 22, 10, C_DIM);
    }
    if (Button((Rectangle){ r.x + r.width - 220, r.y + r.height - 28, 100, 20 }, "Quitter", true, 10)) tutorial = -1;
}

/* ------------------------------------------------------------------ */
/* Ecrans                                                              */
/* ------------------------------------------------------------------ */
/* Hot-seat : panneau du commandant et passage de l'appareil           */
/* ------------------------------------------------------------------ */
void DrawCommanderPanel(void)
{
    int n = Me();
    char buf[200];
    float x = RP_X + 10, y = 56;
    Text("ARMÉE ET DIPLOMATIE MILITAIRE", x, y, 10, C_ACCENT);
    y += 14;
    y += TextWrap("Vous dirigez l'armée de votre nation : recrutement sur le trésor, ordres (clic sur vos provinces puis clic droit "
                  "ou ORDRE), guerre, paix et alliances. L'économie et la politique restent gérées automatiquement.",
                  x, y, RP_W - 20, 10, C_DIM) + 6;
    const Province *pr = &game.prov[selProvince];
    snprintf(buf, sizeof(buf), Tr("RECRUTER EN %s%s"), Tr(PROVINCE_NAMES[selProvince]), pr->owner == n ? "" : " (choisissez une de vos provinces)");
    Text(buf, x, y, 10, C_ACCENT);
    y += 14;
    for (int t = 0; t < U_TYPE_COUNT; t++) {
        Rectangle r = { x + (t % 3) * 182, y + (t / 3) * 22, 176, 20 };
        snprintf(buf, sizeof(buf), Tr("%s  %.0f Md"), Tr(UNIT_TYPES[t].shortName), Cmdr_RecruitCost(t));
        char desc[300];
        snprintf(desc, sizeof(desc), Tr("%s\n%s\nFormation %d tour(s)."), Tr(UNIT_TYPES[t].name), Tr(UNIT_TYPES[t].desc), UNIT_TYPES[t].train);
        if (ActionButton(r, buf, Cmdr_RecruitBlocked(&game, n, t, selProvince), desc)) Do(CMD_RECRUIT, t, selProvince, 0, 0);
    }
    y += ((U_TYPE_COUNT + 2) / 3) * 22 + 6;
    int sel = 0;
    for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i] && game.units[i].active && game.units[i].owner == n) sel++;
    snprintf(buf, sizeof(buf), Tr("%d unité(s) sélectionnée(s)"), sel);
    Text(buf, x, y + 5, 10, C_TEXT);
    if (Button((Rectangle){ x + 200, y, 160, 20 }, "Dissoudre la sélection", sel > 0, 10))
        for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i]) { Do(CMD_DISBAND, i, 0, 0, 0); uiSel[i] = false; }
    y += 30;

    Text("NATIONS VOISINES ET ENNEMIES", x, y, 10, C_ACCENT);
    y += 16;
    int list[N_COUNT], c = 0;
    for (int k = 0; k < N_COUNT; k++)
        if (k != n && game.nat[k].alive && (game.war[n][k] || game.ally[n][k] || War_Adjacent(&game, n, k))) list[c++] = k;
    int rows = (int)((VH - 60 - y) / 24);
    if (cmdrScroll > c - rows) cmdrScroll = c - rows;
    if (cmdrScroll < 0) cmdrScroll = 0;
    Rectangle area = { x, y, RP_W - 20, rows * 24.0f };
    if (Hover(area)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) cmdrScroll--;
        if (wheel < 0) cmdrScroll++;
    }
    for (int i = 0; i < rows && cmdrScroll + i < c; i++) {
        int k = list[cmdrScroll + i];
        float ry = y + i * 24;
        DrawRectangle((int)x, (int)ry, 6, 20, NationColor(k));
        const char *st = game.war[n][k] ? "EN GUERRE" : game.ally[n][k] ? "alliée" : game.truce[n][k] > 0 ? "trêve" : "paix";
        snprintf(buf, sizeof(buf), "%s%s", Tr(NATIONS[k].name), game.human[k] ? " (joueur)" : "");
        Text(buf, x + 12, ry + 2, 10, C_TEXT);
        if (game.war[n][k]) snprintf(buf, sizeof(buf), Tr("%s - score %+.0f"), st, War_Score(&game, n, k));
        else snprintf(buf, sizeof(buf), Tr("%s - relations %+.0f"), st, k == N_FRANCE ? game.nat[n].relation : game.airel[n][k]);
        Text(buf, x + 12, ry + 12, 10, game.war[n][k] ? C_BAD : C_DIM);
        float bx = x + RP_W - 20 - 3 * 92;
        if (ActionButton((Rectangle){ bx, ry, 88, 20 }, "Guerre", Cmdr_DeclareBlocked(&game, n, k),
                         "Déclarer la guerre (une décision diplomatique par tour). Les alliés de la victime peuvent entrer en guerre.")) Do(CMD_DECLARE_WAR, k, 0, 0, 0);
        if (ActionButton((Rectangle){ bx + 92, ry, 88, 20 }, "Paix", Cmdr_PeaceBlocked(&game, n, k),
                         "Proposer la paix. Une IA accepte si la guerre tourne mal pour elle ; la France ou un autre joueur "
                         "reçoit la proposition (un joueur doit la proposer en retour).")) Do(CMD_PEACE, k, 0, 0, 0);
        if (ActionButton((Rectangle){ bx + 184, ry, 88, 20 }, "Alliance", Cmdr_AllyBlocked(&game, n, k),
                         "Alliance défensive avec une nation dirigée par l'IA (relations 50 au moins).")) Do(CMD_DIPLOMACY, k, 2, 0, 0);
    }
}

/* ecran opaque entre deux joueurs : on ne voit pas la carte de l'autre */
void DrawHandoff(void)
{
    DrawBackdrop();
    Rectangle r = { 240, 160, 800, 400 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, NationColor(handoffFor));
    char buf[160];
    Text("PASSEZ L'APPAREIL", r.x + 400 - TextW("PASSEZ L'APPAREIL", 20) / 2, r.y + 40, 20, C_DIM);
    snprintf(buf, sizeof(buf), "%s", handoffFor == N_FRANCE ? "FRANCE" : Tr(NATIONS[handoffFor].name));
    Text(buf, r.x + 400 - TextW(buf, 40) / 2, r.y + 90, 40, NationColor(handoffFor));
    char date[32];
    Game_DateString(game.turn, date, sizeof(date));
    snprintf(buf, sizeof(buf), "%s - %s", date, handoffFor == N_FRANCE ? "Conseil de défense" : "phase du commandant");
    Text(buf, r.x + 400 - TextW(buf, 10) / 2, r.y + 150, 10, C_TEXT);
    const char *tip = handoffFor == N_FRANCE ? "Le trimestre a été résolu. Les autres joueurs ne doivent pas regarder l'écran."
                                             : "Les ordres des joueurs précédents restent secrets. Les autres joueurs détournent les yeux.";
    Text(tip, r.x + 400 - TextW(tip, 10) / 2, r.y + 172, 10, C_DIM);
    if (Button((Rectangle){ r.x + 260, r.y + 260, 280, 44 }, "COMMENCER", true, 20) || IsKeyPressed(KEY_ENTER)) {
        handoffFor = -1;
        memset(uiSel, 0, sizeof(uiSel));
        selProvince = -1;
        for (int p = 0; p < PROV_COUNT && selProvince < 0; p++) if (game.prov[p].owner == Me()) selProvince = p;
        if (selProvince < 0) selProvince = PV_FR_IDF;
        FocusProvince(selProvince, 2);
        mapSignature = 0;
    }
}

/* configuration d'une partie a plusieurs sur le meme appareil */
void DrawHotseatSetup(void)
{
    DrawBackdrop();
    Rectangle r = { 180, 40, 920, 640 };
    Panel(r, C_PANEL);
    Text("PARTIE À PLUSIEURS (HOT-SEAT)", r.x + 20, r.y + 16, 20, C_ACCENT);
    TextWrap("Le premier joueur dirige la France. Jusqu'à trois autres joueurs prennent chacun une nation et en commandent "
             "l'armée et la diplomatie militaire. Chaque trimestre, la France joue, puis chaque commandant à son tour, puis le "
             "trimestre se résout. Un écran de passage cache la carte entre deux joueurs.", r.x + 20, r.y + 44, r.width - 40, 10, C_DIM);
    int picked = 0;
    for (int n = 1; n < N_COUNT; n++) if (hotseatPick[n]) picked++;
    char buf[96];
    int col = 0, row = 0;
    for (int n = 1; n < N_COUNT; n++) {
        Rectangle b = { r.x + 20 + col * 222, r.y + 96 + row * 26, 214, 22 };
        snprintf(buf, sizeof(buf), "%s%s", hotseatPick[n] ? "[X] " : "[ ] ", Tr(NATIONS[n].name));
        if (Choice(b, buf, hotseatPick[n]) && (hotseatPick[n] || picked < 3)) hotseatPick[n] = !hotseatPick[n];
        DrawRectangle((int)b.x + (int)b.width - 8, (int)b.y + 4, 4, 14, NationColor(n));
        if (++col == 4) { col = 0; row++; }
    }
    snprintf(buf, sizeof(buf), Tr("%d commandant(s) choisi(s) sur 3 - difficulté : %s"), picked, Tr(DIFFICULTY_NAMES[newDifficulty]));
    Text(buf, r.x + 20, r.y + r.height - 84, 10, C_TEXT);
    if (Button((Rectangle){ r.x + 20, r.y + r.height - 60, 200, 40 }, "RETOUR", true, 20)) screen = SCR_TITLE;
    if (Button((Rectangle){ r.x + r.width - 300, r.y + r.height - 60, 280, 40 }, "LANCER LA PARTIE", picked > 0, 20)) {
        NewGame();
        tutorial = -1;
        for (int n = 1; n < N_COUNT; n++) if (hotseatPick[n]) Do(CMD_ADD_PLAYER, n, 0, 0, 0);
        handoffFor = N_FRANCE;
    }
}

void DrawGame(void)
{
    ClearBackground(C_BG);
    if (handoffFor >= 0) { inputLayer = activeLayer = 0; DrawHandoff(); return; }
    bool france = Me() == N_FRANCE;
    bool report = france && game.reportReady && showReports && game.pendingDilemma < 0 && game.state == GAME_RUNNING;
    if (!showReports) game.reportReady = false;
    if (treatyOpen && (!game.treaty.active || game.pendingDilemma >= 0)) treatyOpen = false;
    if (!france) treatyOpen = false;
    activeLayer = ((france && game.pendingDilemma >= 0) || report || treatyOpen) ? 1 : 0;
    inputLayer = 0;

    DrawMap();
    DrawLog();
    DrawTopBar();

    Panel((Rectangle){ RP_X - 4, 48, RP_W + 8, VH - 52 }, C_PANEL);
    static const char *tabNames[TAB_COUNT] = {
        "PROV.", "ARMÉE", "ÉT.-MAJ.", "NATIONS", "DÉFENSE", "HYBRIDE", "SOCIÉTÉ", "ÉCO.", "POLIT.", "PROJETS"
    };
    float tw = (RP_W - (TAB_COUNT - 1) * 2) / (float)TAB_COUNT;
    for (int i = 0; i < TAB_COUNT && france; i++) {
        Rectangle r = { RP_X + i * (tw + 2), 52, tw, 28 };
        bool sel = tab == (Tab)i, h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 255 } : h ? C_PANEL2 : C_BG);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(tabNames[i], r.x + (r.width - TextW(tabNames[i], 10)) / 2, r.y + 9, 10, sel ? C_TEXT : C_DIM);
        if (h && clicked) { tab = (Tab)i; clicked = false; }
    }
    switch (france ? tab : TAB_COUNT) {
    case TAB_PROVINCE:  DrawProvinceTab(); break;
    case TAB_ARMEE:     DrawArmyTab(); break;
    case TAB_ETATMAJOR: DrawStaffTab(); break;
    case TAB_NATIONS:   DrawNationsTab(); break;
    case TAB_DEFENSE:   DrawDefenseTab(); break;
    case TAB_HYBRIDE:   DrawHybridTab(); break;
    case TAB_SOCIETE:   DrawSocietyTab(); break;
    case TAB_ECONOMIE:  DrawEconomyTab(); break;
    case TAB_POLITIQUE: DrawPolitiqueTab(); break;
    case TAB_PROJETS:   DrawProjectsTab(); break;
    default: DrawCommanderPanel(); break;
    }

    Rectangle eb = { RP_X + RP_W - 200, VH - 46, 200, 38 };
    bool last = War_NextPlayer(&game, france ? 0 : Me()) < 0;
    bool endTurn = Button(eb, last ? "FIN DU TOUR  >>" : "FIN DE PHASE  >>", game.state == GAME_RUNNING, 20);
    Tooltip(eb, last ? "Résoudre le trimestre (Espace ou Entrée)" : "Passer la main au joueur suivant (Espace ou Entrée)");
    char lbl[64];
    if (france) snprintf(lbl, sizeof(lbl), Tr("PC restants : %d"), game.commandPoints);
    else snprintf(lbl, sizeof(lbl), Tr("Trésor : %.0f Md"), game.nat[Me()].treasury);
    Text(lbl, RP_X + 10, VH - 32, 10, C_ACCENT);
    if (activeLayer == 0 && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) endTurn = true;
    if (endTurn && activeLayer == 0) {
        int turnBefore = game.turn, warsBefore = 0, nukesBefore = game.nukesUsed + game.tacticalUsed;
        int battlesBefore = game.statBattlesWon + game.statBattlesLost;
        float cohBefore = game.res[R_COHESION];
        for (int n = 0; n < N_COUNT; n++) warsBefore += game.war[N_FRANCE][n];
        if (Do(CMD_END_TURN, 0, 0, 0, 0) && HotSeat() && game.state == GAME_RUNNING) handoffFor = Me();
        if (game.turn != turnBefore) {
            int wars = 0;
            for (int n = 0; n < N_COUNT; n++) wars += game.war[N_FRANCE][n];
            if (game.state != GAME_RUNNING) Snd_Play(game.state == GAME_WON ? SFX_VICTORY : SFX_DEFEAT);
            else if (game.nukesUsed + game.tacticalUsed > nukesBefore) Snd_Play(SFX_NUKE);
            else if (wars > warsBefore || game.statBattlesWon + game.statBattlesLost > battlesBefore) Snd_Play(SFX_WAR);
            else if (game.pendingDilemma >= 0) Snd_Play(SFX_ALERT);
            else if (game.res[R_COHESION] < cohBefore - 3) Snd_Play(SFX_BAD);
            else Snd_Play(SFX_TURN);
        }
        logScroll = 0;
        strikeArmed = false;
        for (int i = 0; i < MAX_UNITS; i++) if (!game.units[i].active) uiSel[i] = false;
        if (game.turn != turnBefore && game.turn % 4 == 0 && game.state == GAME_RUNNING) Game_Save(&game, AUTOSAVE_FILE);
    }
    if (activeLayer == 0 && france) {
        for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) tab = (Tab)i;
        if (IsKeyPressed(KEY_ZERO)) tab = TAB_PROJETS;
        if (IsKeyPressed(KEY_M)) mapMode = (MapMode)((mapMode + 1) % MAP_MODE_COUNT);
        if (IsKeyPressed(KEY_F5)) SaveGame();
        if (IsKeyPressed(KEY_F6))
            Toast(session.count && Replay_Save(&session, "bouclier2050.replay") ? "Rejeu enregistré (bouclier2050.replay)."
                                                                               : "Aucun rejeu à enregistrer (partie chargée ?).");
        if (IsKeyPressed(KEY_F9) && FileExists(SAVE_FILE)) LoadGame(SAVE_FILE);
    }

    if (activeLayer == 0 && france) DrawTutorial();
    if (france && game.pendingDilemma >= 0) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawDilemma();
    } else if (treatyOpen) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawTreaty();
    } else if (report) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawReport();
    }

    if (game.state != GAME_RUNNING) { screen = SCR_END; inProgress = false; }
    else if (activeLayer == 0 && IsKeyPressed(KEY_ESCAPE)) screen = SCR_TITLE;
}
