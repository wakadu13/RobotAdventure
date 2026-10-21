/*
 * BOUCLIER 2050 - interface raylib
 *
 * Rendu volontairement simple (formes 2D, une texture de carte generee,
 * police par defaut, aucun fichier externe) pour tourner sur un maximum de
 * machines. Resolution logique fixe 1280x720, mise a l'echelle automatique.
 */
#include "raylib.h"
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VW 1280
#define VH 720
#define SAVE_FILE "bouclier2050.sav"
#define AUTOSAVE_FILE "bouclier2050_auto.sav"

typedef enum { SCR_TITLE, SCR_HELP, SCR_GAME, SCR_END } Screen;
typedef enum {
    TAB_PROVINCE, TAB_ARMEE, TAB_NATIONS, TAB_DEFENSE, TAB_HYBRIDE,
    TAB_SOCIETE, TAB_ECONOMIE, TAB_POLITIQUE, TAB_PROJETS, TAB_COUNT
} Tab;
typedef enum { MAP_POLITIQUE, MAP_CLIMAT, MAP_MILITANCE, MAP_POPULATION, MAP_MODE_COUNT } MapMode;

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */
#define C_BG        (Color){  14,  18,  24, 255 }
#define C_PANEL     (Color){  24,  31,  40, 255 }
#define C_PANEL2    (Color){  33,  42,  54, 255 }
#define C_BORDER    (Color){  60,  74,  92, 255 }
#define C_TEXT      (Color){ 222, 228, 235, 255 }
#define C_DIM       (Color){ 138, 150, 165, 255 }
#define C_ACCENT    (Color){  86, 170, 255, 255 }
#define C_GOOD      (Color){  92, 200, 120, 255 }
#define C_BAD       (Color){ 232,  84,  72, 255 }
#define C_WARN      (Color){ 240, 170,  60, 255 }
#define C_CLIMATE   (Color){ 240, 120,  40, 255 }
#define C_EVENT     (Color){ 190, 140, 255, 255 }
#define C_WAR       (Color){ 255, 110, 110, 255 }
#define C_SEA       (Color){  18,  34,  52, 255 }
#define C_OUT       (Color){  38,  40,  46, 255 }
#define C_SEL       (Color){ 255, 220,  80, 255 }

static const Color DOMAIN_COLORS[D_COUNT] = {
    { 150, 170,  90, 255 }, {  70, 150, 230, 255 }, { 120, 210, 240, 255 }, { 200, 160, 255, 255 },
    {  80, 230, 170, 255 }, { 250, 200,  80, 255 }, { 250, 120, 200, 255 },
};

static const Color FACTION_COLORS[F_COUNT] = {
    { 200, 100, 100, 255 }, { 230, 180,  90, 255 }, {  80, 200, 200, 255 }, { 220, 100, 230, 255 },
};

static const Color IDEOLOGY_COLORS[I_COUNT] = {
    {  90, 190, 100, 255 }, { 220,  80,  90, 255 }, { 240, 200,  70, 255 },
    {  80, 130, 220, 255 }, { 120,  80, 180, 255 }, { 120, 120, 120, 255 },
};

/* ------------------------------------------------------------------ */
/* Etat de l'interface                                                 */
/* ------------------------------------------------------------------ */
static Game     game;
static Screen   screen = SCR_TITLE;
static Tab      tab = TAB_PROVINCE;
static MapMode  mapMode = MAP_POLITIQUE;
static int      selProvince = PV_FR_NORD;
static int      selNation = N_ALLEMAGNE;
static int      selTheater = TH_NORD;
static int      hoverProvince = -1;
static bool     uiSel[MAX_UNITS];
static int      newDifficulty = DIFF_NORMAL;
static bool     showReports = true;
static bool     strikeArmed;
static int      strikeProvince = -1;
static char     toast[160];
static float    toastTime;
static RenderTexture2D target;
static float    viewScale = 1;
static Vector2  viewOffset;
static Vector2  mouse;
static bool     clicked, rclicked;
static int      inputLayer, activeLayer;
static char     tooltip[600];
static float    timeAcc;
static int      logScroll;
static bool     quitRequested;
static bool     inProgress;

/* ------------------------------------------------------------------ */
/* Primitives d'interface                                              */
/* ------------------------------------------------------------------ */
static void Text(const char *s, float x, float y, int size, Color c)
{
    /* positions entieres : la police pixel se deforme sur des coordonnees fractionnaires */
    DrawTextEx(GetFontDefault(), s, (Vector2){ floorf(x), floorf(y) }, (float)size, size / 10.0f, c);
}

static int TextW(const char *s, int size)
{
    return (int)MeasureTextEx(GetFontDefault(), s, (float)size, size / 10.0f).x;
}

static int TextWrap(const char *s, float x, float y, float w, int size, Color c)
{
    char line[256], word[128];
    int lineLen = 0, lines = 0;
    line[0] = 0;
    const char *p = s;
    while (*p) {
        int wl = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && wl < 127) word[wl++] = *p++;
        word[wl] = 0;
        if (!wl) break;
        char test[256];
        snprintf(test, sizeof(test), "%s%s%s", line, lineLen ? " " : "", word);
        if (lineLen && TextW(test, size) > w) {
            Text(line, x, y + lines * (size + 4), size, c);
            lines++;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        lineLen = (int)strlen(line);
    }
    if (lineLen) { Text(line, x, y + lines * (size + 4), size, c); lines++; }
    return lines * (size + 4);
}

static bool Hover(Rectangle r)
{
    return inputLayer == activeLayer && CheckCollisionPointRec(mouse, r);
}

static void Panel(Rectangle r, Color fill)
{
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, C_BORDER);
}

static bool Button(Rectangle r, const char *label, bool enabled, int size)
{
    bool h = enabled && Hover(r);
    Color fill = !enabled ? (Color){ 30, 36, 44, 255 } : h ? (Color){ 52, 82, 120, 255 } : C_PANEL2;
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, enabled ? (h ? C_ACCENT : C_BORDER) : (Color){ 45, 52, 62, 255 });
    int tw = TextW(label, size);
    Text(label, r.x + (r.width - tw) / 2, r.y + (r.height - size) / 2, size, enabled ? C_TEXT : C_DIM);
    if (h && clicked) { clicked = false; return true; }
    return false;
}

static void Tooltip(Rectangle r, const char *text)
{
    if (Hover(r)) snprintf(tooltip, sizeof(tooltip), "%s", text);
}

/* bouton avec infobulle et raison d'indisponibilite */
static bool ActionButton(Rectangle r, const char *label, const char *blocked, const char *desc)
{
    bool res = Button(r, label, blocked == NULL, 10);
    char tip[500];
    snprintf(tip, sizeof(tip), "%s\n%s%s%s", label, desc, blocked ? "\nIndisponible : " : "", blocked ? blocked : "");
    Tooltip(r, tip);
    return res;
}

/* selecteur radio : renvoie true si clique */
static bool Choice(Rectangle r, const char *label, bool sel)
{
    bool h = Hover(r);
    DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
    DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
    Text(label, r.x + (r.width - TextW(label, 10)) / 2, r.y + (r.height - 10) / 2, 10, sel ? C_TEXT : C_DIM);
    if (h && clicked) { clicked = false; return true; }
    return false;
}

static void Bar(Rectangle r, float v, float max, Color c)
{
    DrawRectangleRec(r, (Color){ 10, 12, 16, 255 });
    float w = r.width * (v < 0 ? 0 : (v > max ? 1 : v / max));
    DrawRectangle((int)r.x, (int)r.y, (int)w, (int)r.height, c);
    DrawRectangleLinesEx(r, 1, C_BORDER);
}

static Color LevelColor(float v)
{
    if (v < 20) return C_BAD;
    if (v < 40) return C_WARN;
    return C_GOOD;
}

static Color MilColor(float m) { return m > 6 ? C_BAD : m > 4 ? C_WARN : C_GOOD; }

static Color NationColor(int n)
{
    const unsigned char *c = NATIONS[n].color;
    return (Color){ c[0], c[1], c[2], 255 };
}

static Color Shade(Color c, float k)
{
    return (Color){ (unsigned char)fminf(255, c.r * k), (unsigned char)fminf(255, c.g * k),
                    (unsigned char)fminf(255, c.b * k), 255 };
}

static Color Lerp3(Color a, Color b, Color c, float t)
{
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    Color x = t < 0.5f ? a : b, y = t < 0.5f ? b : c;
    float u = t < 0.5f ? t * 2 : (t - 0.5f) * 2;
    return (Color){ (unsigned char)(x.r + (y.r - x.r) * u), (unsigned char)(x.g + (y.g - x.g) * u),
                    (unsigned char)(x.b + (y.b - x.b) * u), 255 };
}

static void Toast(const char *msg)
{
    snprintf(toast, sizeof(toast), "%s", msg);
    toastTime = 3.0f;
}

/* ------------------------------------------------------------------ */
/* Selection des divisions                                             */
/* ------------------------------------------------------------------ */
static void SelectUnitsIn(int p, bool add)
{
    if (!add) memset(uiSel, 0, sizeof(uiSel));
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &game.units[i];
        if (u->active && u->owner == N_FRANCE && u->prov == p && !u->training) uiSel[i] = true;
    }
}

static int SelectedCount(void)
{
    int c = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (uiSel[i] && game.units[i].active && game.units[i].owner == N_FRANCE) c++;
    return c;
}

static void OrderSelected(int p)
{
    int ok = 0, tried = 0;
    const char *why = NULL;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!uiSel[i] || !game.units[i].active || game.units[i].owner != N_FRANCE) continue;
        tried++;
        const char *b = War_OrderBlocked(&game, i, p);
        if (b) { why = b; continue; }
        War_Order(&game, i, p);
        ok++;
    }
    char buf[160];
    if (!tried) Toast("Aucune division sélectionnée (clic gauche sur une province française).");
    else if (!ok) { snprintf(buf, sizeof(buf), "Ordre impossible : %s.", why); Toast(buf); }
    else {
        bool enemy = game.prov[p].owner != N_FRANCE;
        snprintf(buf, sizeof(buf), "%d division(s) %s %s.", ok, enemy ? "lancée(s) à l'assaut de" : "en route vers", PROVINCE_NAMES[p]);
        Toast(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Carte                                                               */
/* ------------------------------------------------------------------ */
#define CELL_W 3
#define CELL_H 4
#define MAP_X 5
#define MAP_Y 48
#define MAP_PW (MAP_W * CELL_W)
#define MAP_PH (MAP_H * CELL_H)

static Texture2D mapTex;
static Color     mapPixels[MAP_PW * MAP_PH];
static unsigned  mapSignature = 1;
static float     nationLabel[N_COUNT][3];

static int ProvinceAt(Vector2 m)
{
    if (m.x < MAP_X || m.y < MAP_Y) return -1;
    int col = (int)((m.x - MAP_X) / CELL_W), row = (int)((m.y - MAP_Y) / CELL_H);
    if (col >= MAP_W || row >= MAP_H) return -1;
    int v = MAP_CELLS[row][col];
    return v < PROV_COUNT ? v : -1;
}

static Color ProvinceColor(int p, int col, int row)
{
    const Province *pr = &game.prov[p];
    switch (mapMode) {
    case MAP_CLIMAT:
        if (pr->fallout > 0.2f && (col + row) % 2) return (Color){ 120, 220, 60, 255 };
        return Lerp3((Color){ 170, 50, 40, 255 }, (Color){ 220, 190, 70, 255 }, (Color){ 70, 160, 90, 255 }, pr->hab / 100.0f);
    case MAP_MILITANCE:
        if (pr->revolt) return (col + row) % 3 ? (Color){ 150, 40, 160, 255 } : (Color){ 60, 10, 70, 255 };
        return Lerp3((Color){ 60, 150, 90, 255 }, (Color){ 220, 190, 70, 255 }, (Color){ 200, 50, 40, 255 },
                     Soc_ProvinceMilitancy(pr) / 8.0f);
    case MAP_POPULATION: {
        float dens = pr->pop / (PROVINCE_CELLS[p] > 0 ? PROVINCE_CELLS[p] : 1);
        float t = log10f(1 + dens * 100) / 2.2f;
        return Lerp3((Color){ 30, 50, 80, 255 }, (Color){ 80, 140, 210, 255 }, (Color){ 230, 240, 255, 255 }, t);
    }
    default: {
        Color c = NationColor(pr->owner);
        if (pr->owner != N_FRANCE) c = Shade(c, 0.8f);
        if (pr->core != pr->owner && (col + row) % 4 < 1) c = Shade(NationColor(pr->core), 0.7f);
        return c;
    }
    }
}

static unsigned ComputeSignature(void)
{
    unsigned h = 2166136261u;
    h = (h ^ (unsigned)mapMode) * 16777619u;
    h = (h ^ (unsigned)(selProvince + 1)) * 16777619u;
    h = (h ^ (unsigned)game.turn) * 16777619u;
    for (int p = 0; p < PROV_COUNT; p++) {
        h = (h ^ (unsigned)(game.prov[p].owner * 7 + (game.prov[p].revolt ? 1 : 0))) * 16777619u;
        h = (h ^ (unsigned)(game.prov[p].pop * 10)) * 16777619u;
    }
    return h;
}

static void RebuildMap(void)
{
    for (int i = 0; i < N_COUNT; i++) nationLabel[i][0] = nationLabel[i][1] = nationLabel[i][2] = 0;
    for (int row = 0; row < MAP_H; row++) {
        for (int col = 0; col < MAP_W; col++) {
            int v = MAP_CELLS[row][col];
            Color c;
            bool edgeR = false, edgeB = false, natR = false, natB = false;
            if (v == MAP_SEA) c = C_SEA;
            else if (v == MAP_OUT) c = C_OUT;
            else {
                c = ProvinceColor(v, col, row);
                if (v == selProvince) c = Shade(c, 1.35f);
                int own = game.prov[v].owner;
                nationLabel[own][0] += col;
                nationLabel[own][1] += row;
                nationLabel[own][2] += 1;
                if (col + 1 < MAP_W) {
                    int r = MAP_CELLS[row][col + 1];
                    if (r != v) { edgeR = true; natR = r >= PROV_COUNT || game.prov[r].owner != own; }
                }
                if (row + 1 < MAP_H) {
                    int b = MAP_CELLS[row + 1][col];
                    if (b != v) { edgeB = true; natB = b >= PROV_COUNT || game.prov[b].owner != own; }
                }
            }
            for (int y = 0; y < CELL_H; y++) {
                for (int x = 0; x < CELL_W; x++) {
                    Color px = c;
                    bool onR = x == CELL_W - 1, onB = y == CELL_H - 1;
                    if ((onR && natR) || (onB && natB)) px = (Color){ 15, 15, 20, 255 };
                    else if ((onR && edgeR) || (onB && edgeB)) px = Shade(c, 0.72f);
                    mapPixels[(row * CELL_H + y) * MAP_PW + col * CELL_W + x] = px;
                }
            }
        }
    }
    for (int row = 0; row < MAP_H; row++) {
        for (int col = 0; col < MAP_W; col++) {
            if (MAP_CELLS[row][col] != selProvince) continue;
            bool l = col == 0 || MAP_CELLS[row][col - 1] != selProvince;
            bool r = col == MAP_W - 1 || MAP_CELLS[row][col + 1] != selProvince;
            bool t = row == 0 || MAP_CELLS[row - 1][col] != selProvince;
            bool b = row == MAP_H - 1 || MAP_CELLS[row + 1][col] != selProvince;
            for (int y = 0; y < CELL_H; y++)
                for (int x = 0; x < CELL_W; x++)
                    if ((l && x == 0) || (r && x == CELL_W - 1) || (t && y == 0) || (b && y == CELL_H - 1))
                        mapPixels[(row * CELL_H + y) * MAP_PW + col * CELL_W + x] = WHITE;
        }
    }
    UpdateTexture(mapTex, mapPixels);
}

static Vector2 ProvinceCenter(int p)
{
    return (Vector2){ MAP_X + PROVINCE_LABEL[p][0] * CELL_W + 1.5f, MAP_Y + PROVINCE_LABEL[p][1] * CELL_H + 2 };
}

static void DrawArrow(Vector2 a, Vector2 b, Color c)
{
    Vector2 d = { b.x - a.x, b.y - a.y };
    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1) return;
    d.x /= len; d.y /= len;
    Vector2 s = { a.x + d.x * 8, a.y + d.y * 8 }, e = { b.x - d.x * 8, b.y - d.y * 8 };
    DrawLineEx(s, e, 3, Fade(BLACK, 0.5f));
    DrawLineEx(s, e, 2, c);
    Vector2 l = { e.x - d.x * 7 - d.y * 5, e.y - d.y * 7 + d.x * 5 };
    Vector2 r = { e.x - d.x * 7 + d.y * 5, e.y - d.y * 7 - d.x * 5 };
    DrawTriangle(e, l, r, c);
    DrawTriangle(e, r, l, c);
}

/* pions des divisions : un pion par nation presente dans la province */
static void DrawUnitCounters(void)
{
    for (int p = 0; p < PROV_COUNT; p++) {
        int count[N_COUNT] = { 0 }, ready[N_COUNT] = { 0 };
        bool sel = false;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &game.units[i];
            if (!u->active || u->prov != p) continue;
            count[u->owner]++;
            if (!u->training) ready[u->owner]++;
            if (uiSel[i] && u->owner == N_FRANCE) sel = true;
        }
        Vector2 c = ProvinceCenter(p);
        int slot = 0;
        for (int n = 0; n < N_COUNT; n++) {
            if (!count[n]) continue;
            Rectangle r = { c.x - 8 + slot * 18, c.y + 6, 17, 11 };
            Color border = n == N_FRANCE ? (sel ? C_SEL : WHITE) : War_AtWar(&game, N_FRANCE, n) ? C_WAR : (Color){ 20, 20, 20, 255 };
            DrawRectangleRec(r, n == N_FRANCE ? (Color){ 40, 90, 200, 255 } : Shade(NationColor(n), 0.85f));
            DrawRectangleLinesEx(r, 1, border);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", count[n]);
            Text(buf, r.x + (r.width - TextW(buf, 10)) / 2, r.y + 1, 10, ready[n] < count[n] && n == N_FRANCE ? C_DIM : WHITE);
            if (inputLayer == activeLayer && CheckCollisionPointRec(mouse, r)) {
                char tip[500];
                int len = snprintf(tip, sizeof(tip), "Divisions de %s en %s", NATIONS[n].name, PROVINCE_NAMES[p]);
                for (int i = 0; i < MAX_UNITS && len < (int)sizeof(tip) - 60; i++) {
                    const Unit *u = &game.units[i];
                    if (!u->active || u->prov != p || u->owner != n) continue;
                    len += snprintf(tip + len, sizeof(tip) - len, "\n%s  effectifs %.0f%%  organisation %.0f%%%s",
                                    UNIT_TYPES[u->type].name, u->str * 100, u->org * 100, u->training ? "  (formation)" : "");
                }
                snprintf(tooltip, sizeof(tooltip), "%s", tip);
            }
            slot++;
        }
    }
    /* ordres francais */
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &game.units[i];
        if (!u->active || u->owner != N_FRANCE || u->target < 0) continue;
        bool attack = game.prov[u->target].owner != N_FRANCE;
        DrawArrow(ProvinceCenter(u->prov), ProvinceCenter(u->target), attack ? C_WAR : C_GOOD);
    }
}

static void DrawMap(void)
{
    unsigned sig = ComputeSignature();
    if (sig != mapSignature) { mapSignature = sig; RebuildMap(); }
    DrawTexture(mapTex, MAP_X, MAP_Y, WHITE);
    DrawRectangleLines(MAP_X - 1, MAP_Y - 1, MAP_PW + 2, MAP_PH + 2, C_BORDER);

    if (mapMode == MAP_POLITIQUE) {
        for (int n = 0; n < N_COUNT; n++) {
            if (nationLabel[n][2] < 60) continue;
            float x = MAP_X + nationLabel[n][0] / nationLabel[n][2] * CELL_W;
            float y = MAP_Y + nationLabel[n][1] / nationLabel[n][2] * CELL_H - 10;
            const char *nm = NATIONS[n].name;
            int w = TextW(nm, 10);
            Text(nm, x - w / 2 + 1, y - 4, 10, (Color){ 0, 0, 0, 170 });
            Text(nm, x - w / 2, y - 5, 10, n == N_FRANCE ? WHITE : (Color){ 235, 235, 235, 220 });
        }
    }

    float pulse = 0.5f + 0.5f * sinf(timeAcc * 5);
    for (int p = 0; p < PROV_COUNT; p++) {
        Vector2 c = ProvinceCenter(p);
        if (War_AtWar(&game, N_FRANCE, game.prov[p].owner) && War_ProvinceAttackable(&game, p))
            DrawCircleLines((int)c.x, (int)c.y, 7 + pulse * 2, C_WAR);
        if (game.prov[p].revolt) {
            DrawCircleV((Vector2){ c.x, c.y - 8 }, 6, Fade((Color){ 190, 60, 220, 255 }, 0.5f + 0.4f * pulse));
            Text("!", c.x - 2, c.y - 13, 10, WHITE);
        }
    }

    hoverProvince = inputLayer == activeLayer ? ProvinceAt(mouse) : -1;
    DrawUnitCounters();

    if (hoverProvince >= 0) {
        const Province *pr = &game.prov[hoverProvince];
        if (!tooltip[0]) {
            char core[80] = "";
            if (pr->core != pr->owner) snprintf(core, sizeof(core), "\nOccupée - population %s", NATIONS[pr->core].name);
            snprintf(tooltip, sizeof(tooltip), "%s (%s)%s\n%.1f M hab. - habitabilité %.0f - militance %.1f%s%s",
                     PROVINCE_NAMES[hoverProvince], NATIONS[pr->owner].name, core, pr->pop, pr->hab,
                     Soc_ProvinceMilitancy(pr), pr->revolt ? "\nEN RÉVOLTE" : "",
                     SelectedCount() ? "\nClic droit : envoyer les divisions sélectionnées" : "");
        }
        if (clicked) {
            selProvince = hoverProvince;
            if (pr->owner != N_FRANCE) selNation = pr->owner;
            SelectUnitsIn(hoverProvince, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
            if (tab != TAB_NATIONS && tab != TAB_ARMEE) tab = TAB_PROVINCE;
            strikeArmed = false;
            clicked = false;
        }
        if (rclicked) { OrderSelected(hoverProvince); rclicked = false; }
    }

    static const char *modes[MAP_MODE_COUNT] = { "POLITIQUE", "CLIMAT", "MILITANCE", "POPULATION" };
    for (int i = 0; i < MAP_MODE_COUNT; i++) {
        Rectangle r = { MAP_X + 6 + i * 84, MAP_Y + MAP_PH - 24, 80, 18 };
        bool sel = mapMode == (MapMode)i;
        DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 240 } : (Color){ 16, 22, 30, 220 });
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(modes[i], r.x + (r.width - TextW(modes[i], 10)) / 2, r.y + 4, 10, sel ? C_TEXT : C_DIM);
        if (Hover(r)) {
            tooltip[0] = 0;
            if (clicked) { mapMode = (MapMode)i; clicked = false; }
        }
    }
    static const char *legend[MAP_MODE_COUNT] = {
        "Clic gauche : sélectionner.  Clic droit : envoyer les divisions.  Zébrures : territoire occupé.",
        "Habitabilité : vert = tempéré, rouge = invivable, vert vif = contamination.",
        "Militance : vert = calme, rouge = agitée, violet = révolte.",
        "Densité de population.",
    };
    Text(legend[mapMode], MAP_X + 6, MAP_Y + 6, 10, (Color){ 230, 230, 230, 200 });

    if (toastTime > 0) {
        int w = TextW(toast, 10);
        Rectangle r = { MAP_X + MAP_PW / 2 - w / 2 - 10, MAP_Y + MAP_PH - 50, (float)w + 20, 20 };
        DrawRectangleRec(r, Fade((Color){ 10, 14, 20, 255 }, 0.9f));
        DrawRectangleLinesEx(r, 1, C_ACCENT);
        Text(toast, r.x + 10, r.y + 5, 10, C_TEXT);
    }
}

/* ------------------------------------------------------------------ */
/* Barre superieure                                                    */
/* ------------------------------------------------------------------ */
static void DrawTopBar(void)
{
    DrawRectangle(0, 0, VW, 44, C_PANEL);
    DrawLine(0, 44, VW, 44, C_BORDER);

    char date[32], buf[160];
    Game_DateString(game.turn, date, sizeof(date));
    Text("BOUCLIER 2050", 10, 4, 20, C_ACCENT);
    snprintf(buf, sizeof(buf), "%s - %d/%d - +%.2f°C", date, game.turn + 1, TOTAL_TURNS, game.temp);
    Text(buf, 10, 26, 10, C_TEXT);
    snprintf(buf, sizeof(buf), "Score %d", Game_Score(&game, NULL));
    Rectangle sr = { 176, 26, 70, 12 };
    Text(buf, sr.x, sr.y, 10, C_SEL);
    Tooltip(sr, "Score de victoire : détail dans l'onglet POLITIQUE.");

    float x = 250;
    float w = (VW - x - 10) / R_COUNT;
    for (int r = 0; r < R_COUNT; r++) {
        Rectangle area = { x + r * w, 2, w - 6, 40 };
        float v = game.res[r];
        Color c = r == R_BUDGET ? (v < 0 ? C_BAD : C_TEXT) : LevelColor(v);
        Text(RESOURCE_NAMES[r], area.x, area.y + 3, 10, C_DIM);
        snprintf(buf, sizeof(buf), "%.0f", v);
        Text(buf, area.x, area.y + 15, 20, c);
        float d = game.lastDelta[r];
        if (game.turn > 0 && fabsf(d) >= 0.5f) {
            snprintf(buf, sizeof(buf), "%+.0f", d);
            Text(buf, area.x + TextW("000", 20) + 6, area.y + 20, 10, d > 0 ? C_GOOD : C_BAD);
        }
        if (r != R_BUDGET) Bar((Rectangle){ area.x, area.y + 36, area.width - 6, 3 }, v, 100, c);
        else {
            snprintf(buf, sizeof(buf), "%+.1f/tour", Eco_Net(&game));
            Text(buf, area.x + TextW("000", 20) + 22, area.y + 20, 10, C_DIM);
        }
        char tip[300];
        if (r >= R_ENERGIE && r <= R_ALIM) {
            int k = r == R_ALIM ? G_FOOD : r == R_EAU ? G_WATER : G_ENERGY;
            const NationEconomy *e = &game.eco[N_FRANCE];
            snprintf(tip, sizeof(tip), "%s : %.1f\n%s\nProduction %.0f + importations %.0f / besoins %.0f", RESOURCE_NAMES[r], v,
                     RESOURCE_DESC[r], e->supply[k], e->imports[k], e->demand[k]);
        } else {
            snprintf(tip, sizeof(tip), "%s : %.1f\n%s", RESOURCE_NAMES[r], v, RESOURCE_DESC[r]);
        }
        Tooltip(area, tip);
    }
}

/* ------------------------------------------------------------------ */
/* Panneau droit                                                       */
/* ------------------------------------------------------------------ */
#define RP_X 714
#define RP_Y 86
#define RP_W 560

static void StatLine(float x, float y, const char *label, const char *value, Color c)
{
    Text(label, x, y, 10, C_DIM);
    Text(value, x + 120, y, 10, c);
}

static const char *DECLARE_DESC =
    "Ouvre les hostilités (1 PC). Coûte de la cohésion selon le régime, de l'influence et de la réputation. "
    "Attaquer un membre de l'UE fait exclure la France et peut déclencher une coalition.";

static void DrawProvinceTab(void)
{
    int p = selProvince;
    const Province *pr = &game.prov[p];
    const Nation *own = &game.nat[pr->owner];
    char buf[220];
    float x = RP_X + 10, y = RP_Y + 4;

    Text(PROVINCE_NAMES[p], x, y, 20, C_TEXT);
    y += 24;
    DrawRectangle((int)x, (int)y + 1, 10, 10, NationColor(pr->owner));
    snprintf(buf, sizeof(buf), "%s  -  %s%s%s", NATIONS[pr->owner].name, REGIMES[own->regime].name,
             own->failed ? "  -  ÉTAT FAILLI" : "", NATIONS[pr->owner].nuclear ? "  -  puissance nucléaire" : "");
    Text(buf, x + 16, y, 10, C_TEXT);
    y += 14;
    if (pr->core != pr->owner) {
        snprintf(buf, sizeof(buf), "Territoire occupé : population de culture %s. Intégration %.0f%%.", NATIONS[pr->core].name, pr->integration);
        Text(buf, x, y, 10, C_WARN);
        y += 14;
    }
    if (pr->revolt) {
        snprintf(buf, sizeof(buf), "EN RÉVOLTE depuis %d tour(s) : production effondrée, recettes nulles.", pr->revolt);
        Text(buf, x, y, 10, (Color){ 220, 120, 240, 255 });
        y += 14;
    }
    y += 4;

    float c2 = x + 275;
    snprintf(buf, sizeof(buf), "%.2f M", pr->pop);                        StatLine(x, y, "Population", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.0f / 100", pr->hab);                     StatLine(c2, y, "Habitabilité", buf, LevelColor(pr->hab));
    y += 13;
    snprintf(buf, sizeof(buf), "%.0f%%", pr->literacy * 100);              StatLine(x, y, "Alphabétisation", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "x%.2f", pr->prosperity);                   StatLine(c2, y, "Prospérité", buf, C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), "%.0f%%", pr->foreign * 100);               StatLine(x, y, "Pop. à intégrer", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.1f", pr->industry);                      StatLine(c2, y, "Usines", buf, C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), "-%.0f k / +%.0f k", pr->emig * 1000, pr->immig * 1000); StatLine(x, y, "Émigr. / immigr.", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.1f Md/tour", Eco_ProvinceTax(&game, p)); StatLine(c2, y, "Recettes fiscales", buf, C_TEXT);
    y += 13;
    snprintf(buf, sizeof(buf), "%.0f / %.0f / %.0f / %.0f", PROVINCES[p].food * pr->hab / 100, PROVINCES[p].water * pr->hab / 100,
             PROVINCES[p].energy, PROVINCE_MINERALS[p]);
    StatLine(x, y, "Alim/eau/énerg/min", buf, C_TEXT);
    static const char *terrain[3] = { "plaine", "collines", "montagne" };
    snprintf(buf, sizeof(buf), "%s%s", terrain[PROVINCE_TERRAIN[p]], pr->fallout > 0.05f ? ", contaminé" : "");
    StatLine(c2, y, "Terrain", buf, pr->fallout > 0.05f ? C_BAD : C_TEXT);
    y += 18;

    Text("STRATES", x, y, 10, C_DIM);
    Text("part", x + 130, y, 10, C_DIM);
    Text("militance", x + 175, y, 10, C_DIM);
    Text("conscience", x + 355, y, 10, C_DIM);
    y += 13;
    for (int s = 0; s < S_COUNT; s++) {
        Text(STRATA_NAMES[s], x, y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f%%", pr->share[s] * 100);
        Text(buf, x + 130, y, 10, C_TEXT);
        Bar((Rectangle){ x + 175, y + 1, 140, 8 }, pr->mil[s], 10, MilColor(pr->mil[s]));
        snprintf(buf, sizeof(buf), "%.1f", pr->mil[s]);
        Text(buf, x + 320, y, 10, C_TEXT);
        Bar((Rectangle){ x + 355, y + 1, 140, 8 }, pr->con[s], 10, C_EVENT);
        snprintf(buf, sizeof(buf), "%.1f", pr->con[s]);
        Text(buf, x + 500, y, 10, C_TEXT);
        y += 13;
    }
    snprintf(buf, sizeof(buf), "Seuil de révolte : %.1f (baisse avec la conscience politique)", Soc_RevoltThreshold(pr));
    Text(buf, x, y + 2, 10, C_DIM);
    y += 20;

    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, VH - 60 - y }, C_PANEL2);
    y += 8;
    if (pr->owner == N_FRANCE) {
        Text("PROVINCE FRANÇAISE", x, y, 10, C_ACCENT);
        y += 16;
        if (pr->factoryBuild > 0) {
            snprintf(buf, sizeof(buf), "Usine en construction : %d tour(s)", pr->factoryBuild);
            Text(buf, x, y + 6, 10, C_WARN);
        } else {
            snprintf(buf, sizeof(buf), "Construire une usine (%.0f Md, 4 tours)", Eco_FactoryCost(&game));
            if (ActionButton((Rectangle){ x, y, 300, 24 }, buf, Eco_FactoryBlocked(&game, p),
                             "+1 usine : 2 biens industriels par tour (consomme énergie et minerais). Les biens satisfont "
                             "les classes moyennes et les élites, équipent l'armée et soutiennent la prospérité."))
                Eco_BuildFactory(&game, p);
        }
        y += 32;
        TextWrap("Clic droit sur la carte pour déplacer les divisions sélectionnées. Recrutement : onglet ARMÉE.",
                 x, y, RP_W - 30, 10, C_DIM);
        return;
    }

    int enemyUnits = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (game.units[i].active && game.units[i].prov == p && game.units[i].owner == pr->owner) enemyUnits++;

    if (War_AtWar(&game, N_FRANCE, pr->owner)) {
        Text("FRONT", x, y, 20, C_WAR);
        y += 26;
        int ordered = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].target == p) ordered++;
        snprintf(buf, sizeof(buf), "Divisions ennemies sur place : %d.  Nos divisions engagées : %d.", enemyUnits, ordered);
        Text(buf, x, y, 10, C_TEXT);
        y += 14;
        snprintf(buf, sizeof(buf), "Chances de percée estimées : %.0f%% %s", War_BattleOdds(&game, p, N_FRANCE) * 100,
                 ordered ? "(divisions engagées)" : "(toutes les divisions voisines)");
        Text(buf, x, y, 10, C_WARN);
        y += 14;
        TextWrap("Sélectionnez des divisions voisines (clic gauche sur leur province) puis clic droit ici. La bataille a lieu "
                 "en fin de tour : il faut briser l'organisation des défenseurs pour prendre la province.", x, y, RP_W - 30, 10, C_DIM);
        y += 44;
        if (strikeArmed && strikeProvince == p) {
            if (Button((Rectangle){ x, y, 260, 26 }, "CONFIRMER LA FRAPPE NUCLÉAIRE", true, 10)) {
                Nuke_Strike(&game, p);
                strikeArmed = false;
            }
            if (Button((Rectangle){ x + 270, y, 120, 26 }, "Annuler", true, 10)) strikeArmed = false;
        } else if (ActionButton((Rectangle){ x, y, 260, 26 }, "Frappe nucléaire", Nuke_StrikeBlocked(&game, p),
                                "Dévaste la province et contraint un ennemi non nucléaire à capituler. La France devient un paria "
                                "(relations, influence, UE). Contre une puissance nucléaire : riposte et fin de partie.")) {
            strikeArmed = true;
            strikeProvince = p;
        }
        return;
    }

    snprintf(buf, sizeof(buf), "Relation de %s avec la France : %+.0f", NATIONS[pr->owner].name, own->relation);
    Text(buf, x, y, 10, own->relation < 0 ? C_BAD : C_GOOD);
    y += 18;
    if (ActionButton((Rectangle){ x, y, 260, 26 }, "Ultimatum : exiger la cession", War_UltimatumBlocked(&game, p),
                     "Exige la province sous menace de guerre (1 PC). Réussit si notre armée est 2,5 fois plus puissante "
                     "(une dissuasion renforcée aide). Échec : relation -25, Influence -5."))
        War_Ultimatum(&game, p);
    if (ActionButton((Rectangle){ x + 270, y, 260, 26 }, "Déclarer la guerre", War_DeclareBlocked(&game, pr->owner), DECLARE_DESC))
        War_Declare(&game, pr->owner);
    y += 36;
    snprintf(buf, sizeof(buf), "%s : %d divisions sur place, puissance totale %.1f (qualité %.2f). La nôtre : %.1f (qualité %.2f).",
             NATIONS[pr->owner].name, enemyUnits, own->army, War_Quality(&game, pr->owner),
             game.nat[N_FRANCE].army, War_Quality(&game, N_FRANCE));
    TextWrap(buf, x, y, RP_W - 30, 10, C_DIM);
}

static void DrawArmyTab(void)
{
    char buf[220];
    float x = RP_X + 10, y = RP_Y + 4;
    int count = War_UnitCount(&game, N_FRANCE);
    snprintf(buf, sizeof(buf), "DIVISIONS : %d / %d   -   qualité %.2f   -   entretien %.1f Md/tour",
             count, (int)War_ArmyMax(&game, N_FRANCE), War_Quality(&game, N_FRANCE), War_ArmyUpkeep(&game));
    Text(buf, x, y, 10, C_TEXT);
    y += 16;
    snprintf(buf, sizeof(buf), "Recruter en : %s", PROVINCE_NAMES[selProvince]);
    Text(buf, x, y, 10, C_DIM);
    y += 14;
    float bw = (RP_W - 20 - 3 * 6) / 4.0f;
    for (int t = 0; t < U_TYPE_COUNT; t++) {
        const UnitDef *d = &UNIT_TYPES[t];
        snprintf(buf, sizeof(buf), "%s %.0f Md", d->shortName, War_RecruitCost(&game, t));
        char desc[300];
        snprintf(desc, sizeof(desc), "%s : %s\nAttaque %.1f, défense %.1f. Entretien %.2f Md/tour, %.1f bien/tour. Formation %d tours.",
                 d->name, d->desc, d->atk, d->def, d->upkeep, d->goods, d->train);
        if (ActionButton((Rectangle){ x + t * (bw + 6), y, bw, 24 }, buf, War_RecruitBlocked(&game, t, selProvince), desc))
            War_Recruit(&game, t, selProvince);
    }
    y += 32;

    /* divisions de la province selectionnee */
    snprintf(buf, sizeof(buf), "DIVISIONS EN %s", PROVINCE_NAMES[selProvince]);
    Text(buf, x, y, 10, C_ACCENT);
    y += 14;
    int shown = 0;
    for (int i = 0; i < MAX_UNITS && shown < 8; i++) {
        const Unit *u = &game.units[i];
        if (!u->active || u->owner != N_FRANCE || u->prov != selProvince) continue;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 18 };
        bool h = Hover(row);
        DrawRectangleRec(row, uiSel[i] ? (Color){ 60, 56, 30, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLines((int)row.x + 4, (int)row.y + 4, 10, 10, uiSel[i] ? C_SEL : C_DIM);
        if (uiSel[i]) DrawRectangle((int)row.x + 6, (int)row.y + 6, 6, 6, C_SEL);
        Text(UNIT_TYPES[u->type].name, row.x + 20, row.y + 4, 10, C_TEXT);
        Bar((Rectangle){ row.x + 170, row.y + 5, 80, 7 }, u->str, 1, C_GOOD);
        Bar((Rectangle){ row.x + 260, row.y + 5, 80, 7 }, u->org, 1, C_ACCENT);
        if (u->training) snprintf(buf, sizeof(buf), "formation : %d tour(s)", u->training);
        else if (u->target >= 0) snprintf(buf, sizeof(buf), "> %s", PROVINCE_NAMES[u->target]);
        else snprintf(buf, sizeof(buf), "en position");
        Text(buf, row.x + 350, row.y + 4, 10, u->target >= 0 ? C_WARN : C_DIM);
        Tooltip(row, "Clic : sélectionner / désélectionner. Barres : effectifs (vert) et organisation (bleu).");
        if (h && clicked && !u->training) { uiSel[i] = !uiSel[i]; clicked = false; }
        y += 20;
        shown++;
    }
    if (!shown) { Text("Aucune division française ici.", x, y, 10, C_DIM); y += 16; }
    y += 4;
    if (Button((Rectangle){ x, y, 130, 22 }, "Tout sélectionner", true, 10)) SelectUnitsIn(selProvince, true);
    if (Button((Rectangle){ x + 136, y, 130, 22 }, "Annuler les ordres", SelectedCount() > 0, 10))
        for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i] && game.units[i].active) game.units[i].target = -1;
    if (Button((Rectangle){ x + 272, y, 130, 22 }, "Dissoudre", SelectedCount() > 0, 10))
        for (int i = 0; i < MAX_UNITS; i++) if (uiSel[i]) { War_Disband(&game, i); uiSel[i] = false; }
    snprintf(buf, sizeof(buf), "%d sélectionnée(s)", SelectedCount());
    Text(buf, x + 412, y + 6, 10, C_SEL);
    y += 30;

    /* positions */
    Text("POSITIONS (cliquer pour sélectionner)", x, y, 10, C_DIM);
    y += 14;
    int col = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        int n = 0;
        for (int i = 0; i < MAX_UNITS; i++)
            if (game.units[i].active && game.units[i].owner == N_FRANCE && game.units[i].prov == p) n++;
        if (!n) continue;
        Rectangle r = { RP_X + 6 + (col % 3) * 182, y + (col / 3) * 18, 178, 16 };
        bool h = Hover(r);
        DrawRectangleRec(r, p == selProvince ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        snprintf(buf, sizeof(buf), "%s (%d)", PROVINCE_NAMES[p], n);
        Text(buf, r.x + 4, r.y + 3, 10, game.prov[p].owner == N_FRANCE ? C_TEXT : C_WAR);
        if (h && clicked) { selProvince = p; SelectUnitsIn(p, false); clicked = false; }
        col++;
        if (col >= 12) break;
    }
    y = 520;

    /* dissuasion */
    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, VH - 58 - y }, C_PANEL2);
    y += 6;
    Text("DISSUASION NUCLÉAIRE", x, y, 10, C_ACCENT);
    snprintf(buf, sizeof(buf), "Crédibilité %.0f/100%s", game.credibility, game.nukesUsed ? "  -  ARME EMPLOYÉE" : "");
    Text(buf, x + 200, y, 10, game.nukesUsed ? C_BAD : C_TEXT);
    Bar((Rectangle){ x + 380, y + 1, 160, 8 }, game.credibility, 100, C_EVENT);
    y += 16;
    static const char *postureDesc[3] = {
        "0,5 Md/tour. Crédibilité faible : nos adversaires n'hésiteront pas à frapper le sol national.",
        "1,5 Md/tour. Sous-marins et composante aérienne en alerte : la plupart des agresseurs renoncent à envahir la France.",
        "3 Md/tour. Posture maximale : crédibilité très élevée, ultimatums plus efficaces.",
    };
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%s", NUKE_POSTURES[i]);
        Rectangle r = { x + i * 120, y, 114, 20 };
        if (Choice(r, buf, game.nukePosture == i)) Nuke_SetPosture(&game, i);
        Tooltip(r, postureDesc[i]);
    }
    const char *ub = Nuke_UmbrellaBlocked(&game);
    if (ActionButton((Rectangle){ x + 368, y, 172, 20 }, game.umbrella ? "Retirer le parapluie UE" : "Parapluie européen",
                     ub, "Étend la dissuasion aux partenaires de l'UE (+1 Md/tour) : influence, relations, et les agresseurs "
                         "hésitent à envahir un État membre."))
        Nuke_ToggleUmbrella(&game);
    y += 26;
    TextWrap("La crédibilité dissuade les agresseurs de toucher au sol national. L'ultime avertissement se donne depuis "
             "l'onglet NATIONS, la frappe depuis une province ennemie.", x, y, RP_W - 30, 10, C_DIM);
}

static void DrawNationsTab(void)
{
    char buf[200];
    float y = RP_Y + 4;
    float lx = RP_X + 6, lw = 250;
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &game.nat[n];
        Rectangle r = { lx, y + (n - 1) * 20, lw, 19 };
        bool h = Hover(r);
        DrawRectangleRec(r, n == selNation ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, NationColor(n));
        Text(NATIONS[n].name, r.x + 8, r.y + 5, 10, na->alive ? C_TEXT : C_DIM);
        if (!na->alive) {
            Text("disparu", r.x + 140, r.y + 5, 10, C_DIM);
        } else {
            snprintf(buf, sizeof(buf), "%+.0f", na->relation);
            Text(buf, r.x + 140, r.y + 5, 10, na->relation < -20 ? C_BAD : na->relation > 20 ? C_GOOD : C_TEXT);
            const char *st = na->atWar ? "GUERRE" : na->warTarget >= 0 ? "conflit" : na->truce ? "trêve" : na->trade ? "accord" : "";
            Text(st, r.x + 178, r.y + 5, 10, na->atWar ? C_WAR : C_DIM);
            if (na->eu) Text("UE", r.x + 232, r.y + 5, 10, C_ACCENT);
        }
        if (h && clicked) { selNation = n; clicked = false; }
    }

    int n = selNation == N_FRANCE ? N_ALLEMAGNE : selNation;
    const Nation *na = &game.nat[n];
    float dx = lx + lw + 8, dw = RP_X + RP_W - dx - 6;
    Panel((Rectangle){ dx, y, dw, 26 * 20 }, C_PANEL2);
    dx += 8;
    float dy = y + 6, bw = dw - 16;
    Text(NATIONS[n].name, dx, dy, 20, NationColor(n));
    dy += 26;
    snprintf(buf, sizeof(buf), "%s%s%s", REGIMES[na->regime].name, na->eu ? " - UE" : "", na->failed ? " - FAILLI" : "");
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    if (NATIONS[n].nuclear) { Text("Puissance nucléaire", dx, dy, 10, C_WARN); dy += 14; }
    snprintf(buf, sizeof(buf), "Relation : %+.0f", na->relation);
    Text(buf, dx, dy, 10, na->relation < 0 ? C_BAD : C_GOOD); dy += 14;
    snprintf(buf, sizeof(buf), "Population : %.1f M (%d prov.)", Soc_NationPop(&game, n), Soc_NationProvinces(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    snprintf(buf, sizeof(buf), "Divisions : %d (max %.0f)", War_UnitCount(&game, n), War_ArmyMax(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    snprintf(buf, sizeof(buf), "Puissance %.1f, qualité %.2f", na->army, War_Quality(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    if (na->atWar) {
        snprintf(buf, sizeof(buf), "En guerre contre nous (%d tours)", na->warTurns);
        Text(buf, dx, dy, 10, C_WAR); dy += 14;
        Text(na->deterred ? "Dissuadée de toucher au sol national" : "Prête à frapper le sol national", dx, dy, 10,
             na->deterred ? C_GOOD : C_BAD);
        dy += 14;
    }
    if (na->truce) { snprintf(buf, sizeof(buf), "Trêve : %d tours", na->truce); Text(buf, dx, dy, 10, C_DIM); dy += 14; }
    if (na->warTarget >= 0) { snprintf(buf, sizeof(buf), "Envahit : %s", NATIONS[na->warTarget].name); Text(buf, dx, dy, 10, C_WAR); dy += 14; }
    if (na->trade) { Text("Accord commercial actif", dx, dy, 10, C_GOOD); dy += 14; }
    dy += 6;
    if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Rapprochement (5 Influence)", War_DiploBlocked(&game, n, 0), "Améliore la relation de 12 (1 PC)."))
        War_Diplomacy(&game, n, 0);
    dy += 28;
    if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Accord commercial", War_DiploBlocked(&game, n, 1),
                     "La France est servie en priorité sur leurs excédents au marché mondial, +5 relation (1 PC). Relation 25 requise."))
        War_Diplomacy(&game, n, 1);
    dy += 28;
    if (na->atWar) {
        if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Proposer la paix", War_PeaceBlocked(&game, n),
                         "Acceptée si l'ennemi est en difficulté ou si la guerre s'éternise. Chacun garde ses conquêtes (1 PC)."))
            War_ProposePeace(&game, n);
        dy += 28;
        if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Ultime avertissement nucléaire", Nuke_WarningBlocked(&game, n),
                         "Menace d'emploi si l'ennemi ne quitte pas le sol national (une fois par guerre, 1 PC). Succès selon la "
                         "crédibilité ; un bluff percé coûte 20 points de crédibilité."))
            Nuke_Warning(&game, n);
    } else if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Déclarer la guerre", War_DeclareBlocked(&game, n), DECLARE_DESC)) {
        War_Declare(&game, n);
    }
}

static void DrawDefenseTab(void)
{
    int t = selTheater;
    Domain d = THEATERS[t].domain;
    float y = RP_Y + 4;
    char buf[200];
    Text(THEATERS[t].name, RP_X + 10, y, 20, DOMAIN_COLORS[d]);
    snprintf(buf, sizeof(buf), "Milieu : %s", DOMAIN_NAMES[d]);
    Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y + 6, 10, C_DIM);
    y += 24;
    y += TextWrap(THEATERS[t].desc, RP_X + 10, y, RP_W - 20, 10, C_DIM) + 4;

    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, 60 }, C_PANEL2);
    snprintf(buf, sizeof(buf), "Posture : %d", game.posture[t]);
    Text(buf, RP_X + 16, y + 8, 20, C_TEXT);
    if (Button((Rectangle){ RP_X + 170, y + 6, 26, 24 }, "-", game.posture[t] > 0, 20)) Game_ChangePosture(&game, t, -1);
    if (Button((Rectangle){ RP_X + 200, y + 6, 26, 24 }, "+", Game_PostureUsed(&game, d) < game.capacity[d], 20))
        Game_ChangePosture(&game, t, 1);
    snprintf(buf, sizeof(buf), "Capacité %s : %d / %d - défense %.1f", DOMAIN_NAMES[d], Game_PostureUsed(&game, d),
             game.capacity[d], Game_DefensePower(&game, t));
    Text(buf, RP_X + 16, y + 40, 10, C_DIM);
    if (game.pendingTurns[d] > 0) {
        snprintf(buf, sizeof(buf), "+%d capacité dans %d tour(s)", game.pendingCap[d], game.pendingTurns[d]);
        Text(buf, RP_X + 290, y + 14, 10, C_GOOD);
    } else {
        float cost = Game_InvestCost(&game, d);
        snprintf(buf, sizeof(buf), "Investir +2 capacité (%.0f Md)", cost);
        Rectangle br = { RP_X + 290, y + 8, 256, 22 };
        if (Button(br, buf, game.res[R_BUDGET] >= cost, 10)) Game_InvestCapacity(&game, d);
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
            snprintf(buf, sizeof(buf), "%s  [force %d]", td->name, th->strength);
            Text(buf, row.x + 8, row.y + 4, 10, C_TEXT);
            snprintf(buf, sizeof(buf), "%s - encore %d tour(s) - neutralisation %d%%", FACTIONS[th->faction].name,
                     th->turnsLeft, (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 17, 10, FACTION_COLORS[th->faction]);
            char tip[300], eff[200] = "";
            for (int r = 0; r < R_COUNT; r++) {
                if (td->effects[r] == 0) continue;
                char e[40];
                snprintf(e, sizeof(e), "%s %.1f  ", RESOURCE_NAMES[r], td->effects[r] * th->strength * THREAT_SCALE);
                strncat(eff, e, sizeof(eff) - strlen(eff) - 1);
            }
            snprintf(tip, sizeof(tip), "Effets par tour si non neutralisée :\n%s", eff);
            Tooltip(row, tip);
        } else {
            Text("Activité suspecte non identifiée", row.x + 8, row.y + 4, 10, C_WARN);
            snprintf(buf, sizeof(buf), "Neutralisation %d%% (x0,35 tant que non détectée)", (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 17, 10, C_DIM);
        }
        y += 35;
        shown++;
    }
    if (!shown) Text("Aucune menace détectée.", RP_X + 14, y, 10, C_GOOD);

    y = 446;
    Text("THÉÂTRES (cliquer pour sélectionner)", RP_X + 10, y, 10, C_DIM);
    y += 14;
    for (int i = 0; i < TH_COUNT; i++) {
        int col = i % 2, row = i / 2;
        Rectangle r = { RP_X + 6 + col * 276, y + row * 22, 272, 20 };
        bool h = Hover(r);
        DrawRectangleRec(r, i == selTheater ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, DOMAIN_COLORS[THEATERS[i].domain]);
        Text(THEATERS[i].name, r.x + 10, r.y + 5, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "P%d", game.posture[i]);
        Text(buf, r.x + 160, r.y + 5, 10, C_DIM);
        int n = Game_ThreatCount(&game, i);
        if (n) {
            snprintf(buf, sizeof(buf), "%d menace%s", n, n > 1 ? "s" : "");
            Text(buf, r.x + 196, r.y + 5, 10, C_BAD);
        }
        if (h && clicked) { selTheater = i; clicked = false; }
    }
}

static void DrawHybridTab(void)
{
    char buf[200];
    float y = RP_Y + 4;
    snprintf(buf, sizeof(buf), "Points de commandement : %d / %d (partagés avec la diplomatie, la guerre et les décrets)", game.commandPoints, BASE_CP);
    Text(buf, RP_X + 10, y, 10, C_ACCENT);
    y += 16;
    for (int f = 0; f < F_COUNT; f++) {
        Rectangle card = { RP_X + 6, y, RP_W - 12, 126 };
        Panel(card, C_PANEL2);
        DrawRectangle((int)card.x, (int)card.y, 4, (int)card.height, FACTION_COLORS[f]);
        Text(FACTIONS[f].name, card.x + 12, card.y + 6, 20, FACTION_COLORS[f]);
        if (game.fac[f].intelTurns > 0) {
            snprintf(buf, sizeof(buf), "Renseignement : %d tour(s)", game.fac[f].intelTurns);
            Text(buf, card.x + card.width - 10 - TextW(buf, 10), card.y + 10, 10, C_GOOD);
        }
        TextWrap(FACTIONS[f].desc, card.x + 12, card.y + 28, card.width - 24, 10, C_DIM);
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
        snprintf(buf, sizeof(buf), "Opérations en cours : %d", active);
        Text(buf, card.x + 12, card.y + 75, 10, C_TEXT);
        float bw = (card.width - 24 - 4 * 6) / A_COUNT;
        for (int a = 0; a < A_COUNT; a++) {
            Rectangle br = { card.x + 12 + a * (bw + 6), card.y + 94, bw, 24 };
            char desc[300], cost[60];
            if (ACTIONS[a].budget > 0) snprintf(cost, sizeof(cost), "Coût : 1 PC, %.0f Md", ACTIONS[a].budget);
            else if (ACTIONS[a].influence > 0) snprintf(cost, sizeof(cost), "Coût : 1 PC, %.0f Influence", ACTIONS[a].influence);
            else snprintf(cost, sizeof(cost), "Coût : 1 PC");
            snprintf(desc, sizeof(desc), "%s\n%s", ACTIONS[a].desc, cost);
            if (ActionButton(br, ACTIONS[a].name, Game_ActionBlocked(&game, f, a), desc)) Game_FactionAction(&game, f, a);
        }
        y += 132;
    }
}

static void Sparkline(Rectangle r, const float *v, int n, float lo, float hi, Color c, const char *label)
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

static void DrawSocietyTab(void)
{
    char buf[220];
    float x = RP_X + 10, y = RP_Y + 4;
    float pop = Soc_NationPop(&game, N_FRANCE), w = 0;
    float share[S_COUNT] = { 0 }, mil[S_COUNT] = { 0 }, con[S_COUNT] = { 0 }, lit = 0, foreign = 0;
    int revolts = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &game.prov[p];
        if (pr->owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++) {
            share[s] += pr->share[s] * pr->pop;
            mil[s] += pr->mil[s] * pr->pop;
            con[s] += pr->con[s] * pr->pop;
        }
        lit += pr->literacy * pr->pop;
        foreign += pr->foreign * pr->pop;
        if (pr->core != N_FRANCE) foreign += pr->pop * (1 - pr->foreign) * (1 - pr->integration / 100.0f);
        if (pr->revolt) revolts++;
        w += pr->pop;
    }
    Text("POPULATION FRANÇAISE", x, y, 20, C_ACCENT);
    y += 24;
    snprintf(buf, sizeof(buf), "%.1f M d'habitants, %d provinces. Alphabétisation %.0f%%. Non intégrés %.0f%%. Révoltes : %d.",
             pop, Soc_NationProvinces(&game, N_FRANCE), lit / w * 100, foreign / w * 100, revolts);
    Text(buf, x, y, 10, C_TEXT); y += 13;
    snprintf(buf, sizeof(buf), "Migrants admis %.0f k, refoulés %.0f k, départs %.0f k (dernier trimestre).",
             game.migrantsIn * 1000, game.migrantsRefused * 1000, game.migrantsOut * 1000);
    Text(buf, x, y, 10, C_TEXT); y += 18;

    Text("STRATE", x, y, 10, C_DIM);
    Text("part", x + 125, y, 10, C_DIM);
    Text("besoins", x + 165, y, 10, C_DIM);
    Text("militance", x + 230, y, 10, C_DIM);
    Text("conscience", x + 390, y, 10, C_DIM);
    y += 13;
    for (int s = 0; s < S_COUNT; s++) {
        Text(STRATA_NAMES[s], x, y, 10, C_TEXT);
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
    y += 2 + TextWrap("Besoins : vitaux (alimentation, eau), courants (énergie), biens industriels. Un manque fait monter la "
                      "militance et la conscience ; la conscience amplifie la colère et abaisse le seuil de révolte.",
                      x, y + 2, RP_W - 20, 10, C_DIM);
    y += 8;

    float ideo[I_COUNT];
    Pol_NationalIdeology(&game, ideo);
    Text("COURANTS POLITIQUES", x, y, 10, C_DIM);
    snprintf(buf, sizeof(buf), "Soutien au régime : %.0f%%", Pol_RegimeSupport(&game) * 100);
    Text(buf, x + 330, y, 10, Pol_RegimeSupport(&game) < 0.45f ? C_BAD : C_GOOD);
    y += 14;
    for (int i = 0; i < I_COUNT; i++) {
        Rectangle row = { x, y, RP_W - 20, 12 };
        Text(IDEOLOGY_NAMES[i], x, y, 10, game.government == i ? C_SEL : C_TEXT);
        Bar((Rectangle){ x + 120, y + 1, 300, 8 }, ideo[i], 0.6f, IDEOLOGY_COLORS[i]);
        snprintf(buf, sizeof(buf), "%.0f%%%s", ideo[i] * 100, game.government == i ? "  au pouvoir" : "");
        Text(buf, x + 430, y, 10, game.government == i ? C_SEL : C_TEXT);
        Tooltip(row, IDEOLOGY_DESC[i]);
        y += 13;
    }
    if (game.nat[N_FRANCE].regime == RG_DEMOCRATIE)
        snprintf(buf, sizeof(buf), "Prochaines élections dans %d tours.", game.nextElection - game.turn);
    else
        snprintf(buf, sizeof(buf), "Pas d'élections sous ce régime.");
    Text(buf, x, y + 2, 10, C_DIM);
    y += 20;

    int n = game.turn + 1 < HISTORY_LEN ? game.turn + 1 : HISTORY_LEN;
    float pmax = 0;
    for (int i = 0; i < n; i++) if (game.histPop[i] > pmax) pmax = game.histPop[i];
    float h = VH - 60 - y;
    if (h > 110) h = 110;
    Sparkline((Rectangle){ RP_X + 6, y, (RP_W - 18) / 3, h }, game.histPop, n, 0, pmax * 1.1f + 1, C_ACCENT, "Population");
    Sparkline((Rectangle){ RP_X + 9 + (RP_W - 18) / 3, y, (RP_W - 18) / 3, h }, game.histCohesion, n, 0, 100, C_GOOD, "Cohésion");
    Sparkline((Rectangle){ RP_X + 12 + 2 * (RP_W - 18) / 3, y, (RP_W - 18) / 3, h }, game.histTemp, n, 1, 4, C_CLIMATE, "Température");
}

static void Stepper(float x, float y, const char *label, float value, float *out, float step, const char *fmt, const char *tip)
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

static void DrawEconomyTab(void)
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
        Stepper(x, y, STRATA_NAMES[s], game.taxRate[s], &v, 0.05f, "%.0f%%", taxTip[s]);
        if (v != game.taxRate[s]) Eco_SetTax(&game, s, v);
        y += 22;
    }
    y += 6;
    Text("DÉPENSES PUBLIQUES", x, y, 10, C_ACCENT);
    y += 14;
    for (int s = 0; s < SP_COUNT; s++) {
        char tip[300];
        snprintf(tip, sizeof(tip), "%s\nCoût actuel : %.1f Md/tour.", SPENDING_DESC[s], Eco_SpendingCost(&game, s));
        Stepper(x, y, SPENDING_NAMES[s], game.spending[s], &v, 0.1f, "%.0f%%", tip);
        if (v != game.spending[s]) Eco_SetSpending(&game, s, v);
        y += 22;
    }
    Stepper(x, y, "Droits de douane", game.tariff, &v, 0.05f, "%.0f%%",
            "Prélèvement sur les importations : recettes immédiates, mais prospérité en baisse.");
    if (v != game.tariff) Eco_SetTariff(&game, v);
    y += 30;

    /* budget */
    float l[BL_COUNT];
    Eco_Budget(&game, l);
    float bx = RP_X + 330, by = RP_Y + 4;
    Text("BUDGET PRÉVISIONNEL", bx, by, 10, C_ACCENT);
    by += 14;
    float inc = 0, exp = 0;
    for (int i = 0; i < BL_COUNT; i++) {
        bool income = i < BL_DEFENSE;
        snprintf(buf, sizeof(buf), "%s%.1f", income ? "+" : "-", l[i]);
        Text(BUDGET_LINE_NAMES[i], bx, by, 10, C_DIM);
        Text(buf, bx + 160, by, 10, income ? C_GOOD : C_BAD);
        if (income) inc += l[i]; else exp += l[i];
        by += 13;
    }
    snprintf(buf, sizeof(buf), "Solde : %+.1f Md/tour", inc - exp);
    Text(buf, bx, by + 4, 10, inc - exp >= 0 ? C_TEXT : C_WARN);
    if (game.res[R_BUDGET] < 0) Text("Dette : intérêts 3%/tour", bx, by + 18, 10, C_BAD);

    /* marche */
    Text("MARCHÉ MONDIAL", x, y, 10, C_ACCENT);
    y += 14;
    static const char *cols[] = { "Bien", "Prix", "Prod.", "Besoins", "Import", "Export", "Couvert" };
    static const float cx[] = { 0, 130, 190, 245, 310, 370, 430 };
    for (int c = 0; c < 7; c++) Text(cols[c], x + cx[c], y, 10, C_DIM);
    y += 13;
    const NationEconomy *e = &game.eco[N_FRANCE];
    for (int k = 0; k < G_COUNT; k++) {
        Text(GOOD_NAMES[k], x, y, 10, C_TEXT);
        if (k == G_WATER) snprintf(buf, sizeof(buf), "local");
        else snprintf(buf, sizeof(buf), "%.0f%%", game.price[k] / GOOD_BASE_PRICE[k] * 100);
        Text(buf, x + cx[1], y, 10, k != G_WATER && game.price[k] > GOOD_BASE_PRICE[k] * 1.3f ? C_BAD : C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->supply[k]);  Text(buf, x + cx[2], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->demand[k]);  Text(buf, x + cx[3], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->imports[k]); Text(buf, x + cx[4], y, 10, C_TEXT);
        snprintf(buf, sizeof(buf), "%.0f", e->exports[k]); Text(buf, x + cx[5], y, 10, C_TEXT);
        float sat = e->sat[k];
        snprintf(buf, sizeof(buf), "%.0f%%", (sat > 1 ? 1 : sat) * 100);
        Text(buf, x + cx[6], y, 10, sat < 0.97f ? C_BAD : C_GOOD);
        y += 13;
    }
    y += 6;
    float ind = 0;
    for (int p = 0; p < PROV_COUNT; p++) if (game.prov[p].owner == N_FRANCE) ind += game.prov[p].industry;
    snprintf(buf, sizeof(buf), "Industrie : %.0f usines. Prix en %% du prix de référence de 2030.", ind);
    Text(buf, x, y, 10, C_DIM);
    y += 14;
    TextWrap("Les prix montent quand le monde manque d'un bien. Les excédents s'exportent, les déficits s'importent tant "
             "que le marché le permet ; les accords commerciaux nous servent en priorité. Les usines se construisent "
             "depuis l'onglet PROVINCE.", x, y, RP_W - 20, 10, C_DIM);
}

static void DrawPolitiqueTab(void)
{
    char buf[220];
    float x = RP_X + 10, y = RP_Y + 4;
    Regime cur = game.nat[N_FRANCE].regime;
    Text("RÉGIME", x, y, 10, C_ACCENT);
    if (game.regimeCooldown > 0) {
        snprintf(buf, sizeof(buf), "prochain changement dans %d tours", game.regimeCooldown);
        Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y, 10, C_DIM);
    }
    y += 14;
    for (int r = 0; r < RG_COUNT; r++) {
        const RegimeDef *rg = &REGIMES[r];
        bool isCur = cur == (Regime)r;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 30 };
        DrawRectangleRec(row, isCur ? (Color){ 40, 62, 92, 255 } : C_PANEL2);
        DrawRectangleLinesEx(row, 1, isCur ? C_ACCENT : C_BORDER);
        Text(rg->name, row.x + 8, row.y + 3, 10, isCur ? C_TEXT : C_DIM);
        snprintf(buf, sizeof(buf), "Impôt x%.2f  Mobil. %.2f  Répression %.2f  Qualité x%.2f  Influence %+.1f",
                 rg->taxEff, rg->mobilization, rg->repression, rg->quality, rg->influence);
        Text(buf, row.x + 8, row.y + 16, 10, C_DIM);
        char desc[600];
        snprintf(desc, sizeof(desc), "%s\n%s\nLassitude de guerre %.1f, coût d'agression %.0f, intégration x%.1f, projets x%.2f.",
                 rg->desc, rg->mechanic, rg->warWeariness, rg->aggressionCost, rg->assimilation, rg->projectCost);
        if (!isCur) {
            snprintf(buf, sizeof(buf), "Adopter (-%.0f coh.)", Soc_RegimeChangeCost(&game, r));
            if (ActionButton((Rectangle){ row.x + row.width - 128, row.y + 5, 122, 20 }, buf, Soc_RegimeBlocked(&game, r), desc))
                Soc_ChangeRegime(&game, r);
        } else {
            Tooltip(row, desc);
        }
        y += 33;
    }
    y += 2;
    y += TextWrap(REGIMES[cur].mechanic, x, y, RP_W - 20, 10, C_SEL) + 4;
    snprintf(buf, sizeof(buf), "Gouvernement : %s", game.government >= 0 ? IDEOLOGY_NAMES[game.government] : "aucun");
    Text(buf, x, y, 10, C_TEXT);
    y += 16;
    for (int k = 0; k < 2; k++) {
        const DecreeDef *d = &DECREES[cur][k];
        char desc[300];
        snprintf(desc, sizeof(desc), "%s\nCoût : 1 PC%s. Recharge : %d tours.", d->desc, d->budget > 0 ? ", budget" : "", d->cooldown);
        snprintf(buf, sizeof(buf), d->budget > 0 ? "%s (%.0f Md)" : "%s", d->name, d->budget);
        if (ActionButton((Rectangle){ x + k * 274, y, 266, 24 }, buf, Pol_DecreeBlocked(&game, k), desc)) Pol_Decree(&game, k);
    }
    y += 32;

    Text("DOCTRINE MIGRATOIRE", x, y, 10, C_ACCENT);
    Text("NIVEAU D'ALERTE", x + 280, y, 10, C_ACCENT);
    y += 14;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { x, y + i * 22, 260, 20 };
        if (Choice(r, MIGRATION_DOCTRINES[i], game.migrationDoctrine == i)) Game_SetMigrationDoctrine(&game, i);
        Tooltip(r, MIGRATION_DESC[i]);
        Rectangle a = { x + 280, y + i * 22, 260, 20 };
        if (Choice(a, ALERT_LEVELS[i], game.alertLevel == i)) Game_SetAlertLevel(&game, i);
        Tooltip(a, ALERT_DESC[i]);
    }
    y += 72;

    int parts[SC_COUNT];
    int score = Game_Score(&game, parts);
    VictoryType v = Game_Victory(&game);
    snprintf(buf, sizeof(buf), "SCORE %d  -  victoire visée : %s", score, VICTORY_NAMES[v]);
    Rectangle sr = { x, y, RP_W - 20, 12 };
    Text(buf, x, y, 10, C_SEL);
    Tooltip(sr, VICTORY_DESC[v]);
    y += 14;
    for (int i = 0; i < SC_COUNT; i++) {
        snprintf(buf, sizeof(buf), "%s %d", SCORE_PART_NAMES[i], parts[i]);
        Text(buf, x + (i % 3) * 180, y + (i / 3) * 12, 10, parts[i] < 0 ? C_BAD : C_DIM);
    }
}

static void DrawProjectsTab(void)
{
    char buf[160];
    float y = RP_Y + 4;
    Text("Projets civils de résilience : investissements uniques, effets permanents.", RP_X + 10, y, 10, C_DIM);
    y += 16;
    for (int p = 0; p < P_COUNT; p++) {
        Rectangle row = { RP_X + 6, y, RP_W - 12, 54 };
        const Project *pr = &game.proj[p];
        Panel(row, pr->state == 2 ? (Color){ 26, 48, 36, 255 } : C_PANEL2);
        Text(PROJECTS[p].name, row.x + 10, row.y + 6, 20, pr->state == 2 ? C_GOOD : C_TEXT);
        Text(PROJECTS[p].desc, row.x + 10, row.y + 32, 10, C_DIM);
        Rectangle br = { row.x + row.width - 170, row.y + 8, 160, 24 };
        if (pr->state == 0) {
            float c = Game_ProjectCost(&game, p);
            snprintf(buf, sizeof(buf), "Lancer : %.0f Md / %d t", c, PROJECTS[p].turns);
            if (Button(br, buf, game.res[R_BUDGET] >= c, 10)) Game_StartProject(&game, p);
        } else if (pr->state == 1) {
            snprintf(buf, sizeof(buf), "Chantier : %d tour(s)", pr->turnsLeft);
            Text(buf, br.x + 10, br.y + 4, 10, C_WARN);
            Bar((Rectangle){ br.x + 10, br.y + 18, 140, 5 }, (float)(PROJECTS[p].turns - pr->turnsLeft), (float)PROJECTS[p].turns, C_WARN);
        } else {
            Text("OPÉRATIONNEL", br.x + 30, br.y + 6, 10, C_GOOD);
        }
        y += 58;
    }
}

/* ------------------------------------------------------------------ */
/* Journal, modales, infobulles                                        */
/* ------------------------------------------------------------------ */
static Color LogColor(LogKind k)
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

static void DrawLog(void)
{
    Rectangle r = { 0, MAP_Y + MAP_PH + 2, 708, VH - MAP_Y - MAP_PH - 2 };
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
        Text(e->text, 8, r.y + 5 + i * 13, 10, c);
    }
}

static void DrawDilemma(void)
{
    const DilemmaDef *d = Game_CurrentDilemma(&game);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
    Rectangle r = { 290, 170, 700, 380 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, game.pendingDilemma >= DLM_ALLY_CALL ? C_WAR : C_EVENT);
    Text("DÉCISION DU CONSEIL DE DÉFENSE", r.x + 20, r.y + 16, 10, C_EVENT);
    Text(d->title, r.x + 20, r.y + 32, 20, C_TEXT);
    float ty = r.y + 62;
    int dp = game.pendingDilemma, n = game.dilemmaParam;
    if (dp == DLM_ALLY_CALL || dp == DLM_PEACE_OFFER || dp == DLM_NUKE_BLACKMAIL) {
        char buf[200];
        if (dp == DLM_ALLY_CALL) snprintf(buf, sizeof(buf), "%s envahit %s.", NATIONS[n].name, NATIONS[game.nat[n].warTarget].name);
        else if (dp == DLM_PEACE_OFFER) snprintf(buf, sizeof(buf), "%s demande la fin des combats.", NATIONS[n].name);
        else snprintf(buf, sizeof(buf), "Crédibilité de notre dissuasion : %.0f/100.", game.credibility);
        Text(buf, r.x + 20, ty, 10, C_WAR);
        ty += 16;
    }
    TextWrap(d->text, r.x + 20, ty, r.width - 40, 10, C_DIM);
    for (int i = 0; i < 2; i++) {
        Rectangle b = { r.x + 20 + i * 340, r.y + 150, 320, 150 };
        bool h = Hover(b);
        DrawRectangleRec(b, h ? (Color){ 44, 66, 96, 255 } : C_PANEL2);
        DrawRectangleLinesEx(b, 1, h ? C_ACCENT : C_BORDER);
        TextWrap(i == 0 ? d->optA : d->optB, b.x + 12, b.y + 12, b.width - 24, 20, C_TEXT);
        TextWrap(i == 0 ? d->effA : d->effB, b.x + 12, b.y + 80, b.width - 24, 10, C_WARN);
        if (h && clicked) { clicked = false; Game_ResolveDilemma(&game, i); }
    }
    Text("Choisissez une option pour poursuivre.", r.x + 20, r.y + r.height - 26, 10, C_DIM);
}

static void DeltaLine(float x, float y, const char *label, float before, float after, const char *fmt)
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

static void DrawReport(void)
{
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.55f));
    Rectangle r = { 190, 90, 900, 540 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, C_ACCENT);
    char date[32], buf[200];
    Game_DateString(game.turn, date, sizeof(date));
    snprintf(buf, sizeof(buf), "RAPPORT DU TRIMESTRE - %s", date);
    Text(buf, r.x + 20, r.y + 16, 20, C_TEXT);

    float x = r.x + 20, y = r.y + 50;
    for (int i = 0; i < R_COUNT; i++) {
        DeltaLine(x, y, RESOURCE_NAMES[i], game.reportRes[i], game.res[i], "%.0f");
        y += 15;
    }
    y += 6;
    DeltaLine(x, y, "Population (M)", game.reportPop, Soc_NationPop(&game, N_FRANCE), "%.1f"); y += 15;
    DeltaLine(x, y, "Provinces", (float)game.reportProv, (float)Soc_NationProvinces(&game, N_FRANCE), "%.0f"); y += 15;
    DeltaLine(x, y, "Divisions", (float)game.reportUnits, (float)War_UnitCount(&game, N_FRANCE), "%.0f"); y += 15;
    DeltaLine(x, y, "Score", (float)game.reportScore, (float)Game_Score(&game, NULL), "%.0f"); y += 22;
    snprintf(buf, sizeof(buf), "+%.2f°C", game.temp);
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
        ey += TextWrap(e->text, ex, ey, r.width - 270, 10, LogColor(e->kind));
        shown++;
        if (ey > r.y + r.height - 60) break;
    }
    if (!shown) Text("Trimestre calme.", ex, ey, 10, C_DIM);

    Rectangle cb = { r.x + 20, r.y + r.height - 36, 16, 16 };
    DrawRectangleLinesEx(cb, 1, C_TEXT);
    if (showReports) DrawRectangle((int)cb.x + 4, (int)cb.y + 4, 8, 8, C_ACCENT);
    Text("Afficher ce rapport à chaque tour", cb.x + 24, cb.y + 3, 10, C_DIM);
    if (Hover((Rectangle){ cb.x, cb.y, 220, 16 }) && clicked) { showReports = !showReports; clicked = false; }

    if (Button((Rectangle){ r.x + r.width - 180, r.y + r.height - 44, 160, 32 }, "CONTINUER", true, 20)
        || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
        game.reportReady = false;
}

static void DrawTooltip(void)
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
/* Ecrans                                                              */
/* ------------------------------------------------------------------ */
static void ResetUi(void)
{
    selProvince = PV_FR_NORD;
    selNation = N_ALLEMAGNE;
    tab = TAB_PROVINCE;
    logScroll = 0;
    mapSignature = 0;
    memset(uiSel, 0, sizeof(uiSel));
    strikeArmed = false;
    screen = SCR_GAME;
    inProgress = true;
}

static void NewGame(void)
{
    Game_Init(&game, (unsigned)time(NULL), newDifficulty);
    ResetUi();
}

static void LoadGame(const char *path)
{
    if (Game_Load(&game, path)) {
        ResetUi();
        Toast("Partie chargée.");
        if (game.state != GAME_RUNNING) screen = SCR_END;
    } else {
        Toast("Sauvegarde illisible ou d'une autre version.");
    }
}

static void SaveGame(void)
{
    Toast(Game_Save(&game, SAVE_FILE) ? "Partie sauvegardée (" SAVE_FILE ")." : "Échec de la sauvegarde.");
}

static void DrawBackdrop(void)
{
    ClearBackground(C_BG);
    for (int i = 0; i < 40; i++) {
        float x = fmodf(i * 97.0f + timeAcc * (8 + i % 5), VW);
        float y = fmodf(i * 53.0f, VH);
        DrawCircle((int)x, (int)y, 1 + i % 2, Fade(C_ACCENT, 0.25f));
    }
    for (int y = 0; y < VH; y += 4) DrawLine(0, y, VW, y, (Color){ 0, 0, 0, 40 });
}

static void DrawTitle(void)
{
    DrawBackdrop();
    Text("BOUCLIER 2050", VW / 2 - TextW("BOUCLIER 2050", 60) / 2, 70, 60, C_ACCENT);
    const char *sub = "Grande stratégie et guerre hybride en Europe et en Méditerranée, 2030-2060";
    Text(sub, VW / 2 - TextW(sub, 20) / 2, 146, 20, C_TEXT);
    const char *pitch =
        "2030. Le monde a franchi +1,5°C et le réchauffement s'accélère. Sécheresses, mégafeux et effondrement agricole "
        "vident le Sud et jettent des millions de personnes sur les routes. Gérez une population vivante et son économie, "
        "choisissez votre régime, de la commune utopique à l'État totalitaire, menez vos divisions, maniez la dissuasion "
        "et défendez vos frontières dans tous les milieux. Tenez trente ans.";
    TextWrap(pitch, 280, 186, 720, 10, C_DIM);

    float y = 262, bx = VW / 2 - 140;
    if (inProgress) {
        if (Button((Rectangle){ bx, y, 280, 38 }, "CONTINUER", true, 20)) screen = SCR_GAME;
        y += 46;
        if (Button((Rectangle){ bx, y, 280, 38 }, "SAUVEGARDER", true, 20)) SaveGame();
        y += 46;
    }
    if (Button((Rectangle){ bx, y, 136, 38 }, "CHARGER", FileExists(SAVE_FILE), 20)) LoadGame(SAVE_FILE);
    if (Button((Rectangle){ bx + 144, y, 136, 38 }, "AUTO", FileExists(AUTOSAVE_FILE), 20)) LoadGame(AUTOSAVE_FILE);
    Tooltip((Rectangle){ bx + 144, y, 136, 38 }, "Sauvegarde automatique (tous les 4 tours).");
    y += 46;
    if (Button((Rectangle){ bx, y, 280, 38 }, "NOUVELLE PARTIE", true, 20)) NewGame();
    y += 46;
    char buf[64];
    snprintf(buf, sizeof(buf), "Difficulté : %s", DIFFICULTY_NAMES[newDifficulty]);
    Rectangle dr = { bx, y, 280, 30 };
    if (Button(dr, buf, true, 10)) newDifficulty = (newDifficulty + 1) % DIFF_COUNT;
    Tooltip(dr, "Facile : menaces et climat atténués, recettes accrues.\nDifficile : menaces, climat et IA plus agressifs, "
                "recettes réduites ; le score est multiplié par 1,3.");
    y += 38;
    if (Button((Rectangle){ bx, y, 136, 38 }, "AIDE", true, 20)) screen = SCR_HELP;
    if (Button((Rectangle){ bx + 144, y, 136, 38 }, "QUITTER", true, 20)) quitRequested = true;

    const char *dom = "Terre - Mer - Air - Espace - Cyber - Informationnel - Electromagnétique";
    Text(dom, VW / 2 - TextW(dom, 10) / 2, 660, 10, C_DIM);
    Text("Fiction prospective. Contours : Natural Earth (domaine public).", 10, VH - 18, 10, C_DIM);
    if (toastTime > 0) Text(toast, VW / 2 - TextW(toast, 10) / 2, 640, 10, C_SEL);
}

static void DrawHelp(void)
{
    DrawBackdrop();
    Panel((Rectangle){ 90, 20, 1100, 680 }, C_PANEL);
    Text("COMMENT JOUER", 110, 32, 20, C_ACCENT);
    static const char *help[] = {
        "OBJECTIF : tenir 120 trimestres (2030-2060) et maximiser le score. Défaite si la Cohésion ou la Souveraineté tombe à 0, si le Budget passe sous -60, si Paris tombe ou en cas d'échange nucléaire. Victoires spéciales : Transition, Hégémonie, Utopie, Ordre nouveau.",
        "TOUR : agissez puis FIN DU TOUR (Espace). Un rapport résume le trimestre. 3 points de commandement (PC) par tour pour la diplomatie, la guerre, les décrets et la lutte hybride.",
        "ARMÉE : chaque pion est un ensemble de divisions. Clic gauche sur une province : sélectionner nos divisions (Maj pour ajouter). Clic droit sur une province voisine : s'y déplacer ou l'attaquer. Les batailles se jouent en fin de tour : il faut briser l'organisation des défenseurs. Le terrain, les débarquements (infanterie de marine, capacité Mer) et le ravitaillement comptent.",
        "DISSUASION : la posture fixe la crédibilité. Une nation dissuadée ne touche pas au sol national. Ultime avertissement depuis NATIONS, frappe depuis une province ennemie : la France devient alors un paria, et frapper une puissance nucléaire provoque la riposte.",
        "POPULATION : trois strates (travailleurs, classes moyennes, élites) avec leur part, leurs besoins, leur militance et leur conscience politique. L'éducation fait monter l'alphabétisation et l'ascension sociale, mais aussi la conscience ; la répression calme la militance mais accumule la conscience.",
        "ÉCONOMIE : provinces produisant alimentation, eau, énergie, minerais et biens industriels (usines). Marché mondial avec prix, importations et exportations. Impôts par strate, dépenses d'éducation, d'administration et de protection sociale, droits de douane.",
        "POLITIQUE : six courants (écologistes, socialistes, libéraux, conservateurs, souverainistes, autoritaires) évoluent selon la situation. En démocratie, des élections désignent le gouvernement. Chaque régime a ses mécaniques et deux décrets propres.",
        "CLIMAT ET MIGRATIONS : le réchauffement réduit l'habitabilité, d'abord au Sud ; les populations migrent. La doctrine migratoire décide combien entrent ; les refoulés deviennent des flux clandestins.",
        "DÉFENSE M2MC ET HYBRIDE : 12 théâtres (terre, mer, air, espace, cyber, informationnel, électromagnétique) et 4 factions qui mènent la guerre sous le seuil.",
        "RACCOURCIS : Espace/Entrée = fin du tour, 1-9 = onglets, M = mode de carte, F5 = sauvegarder, F9 = charger, F11 = plein écran, Échap = menu.",
    };
    float y = 62;
    for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
        y += TextWrap(help[i], 110, y, 1060, 10, i % 2 ? C_TEXT : (Color){ 200, 210, 220, 255 }) + 8;
    if (Button((Rectangle){ VW / 2 - 100, 648, 200, 38 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE))
        screen = SCR_TITLE;
}

static void DrawEnd(void)
{
    DrawBackdrop();
    bool won = game.state == GAME_WON;
    const char *title = won ? "LA FRANCE A TENU" : "DÉFAITE";
    Text(title, VW / 2 - TextW(title, 60) / 2, 50, 60, won ? C_GOOD : C_BAD);
    const char *why = "";
    switch (game.state) {
    case GAME_WON: why = VICTORY_DESC[Game_Victory(&game)]; break;
    case GAME_LOST_COHESION: why = "Effondrement social : émeutes, sécessions locales, le pays se fracture."; break;
    case GAME_LOST_SOUVERAINETE: why = "Perte de souveraineté : la France ne décide plus de son destin."; break;
    case GAME_LOST_BUDGET: why = "Défaut de paiement : les créanciers dictent désormais la politique nationale."; break;
    case GAME_LOST_CONQUEST: why = "Paris est tombée. Le gouvernement capitule."; break;
    case GAME_LOST_NUCLEAR: why = "Échange nucléaire. Il n'y a pas de vainqueur."; break;
    default: break;
    }
    char buf[220], date[32];
    if (won) {
        snprintf(buf, sizeof(buf), "Victoire : %s", VICTORY_NAMES[Game_Victory(&game)]);
        Text(buf, VW / 2 - TextW(buf, 20) / 2, 122, 20, C_SEL);
    }
    Text(why, VW / 2 - TextW(why, 10) / 2, 150, 10, C_TEXT);

    Game_DateString(game.turn, date, sizeof(date));
    Panel((Rectangle){ 240, 176, 800, 330 }, C_PANEL);
    float x = 260, y = 192;
    snprintf(buf, sizeof(buf), "Fin : %s (+%.2f°C) - régime : %s - difficulté : %s", date, game.temp,
             REGIMES[game.nat[N_FRANCE].regime].name, DIFFICULTY_NAMES[game.difficulty]);
    Text(buf, x, y, 10, C_TEXT); y += 16;
    snprintf(buf, sizeof(buf), "Population : %.1f M dans %d provinces. Conquises : %d, perdues : %d.",
             Soc_NationPop(&game, N_FRANCE), Soc_NationProvinces(&game, N_FRANCE), game.statConquered, game.statLost);
    Text(buf, x, y, 10, C_TEXT); y += 16;
    snprintf(buf, sizeof(buf), "Batailles gagnées : %d, perdues : %d. Menaces neutralisées : %d, subies : %d. Catastrophes : %d.",
             game.statBattlesWon, game.statBattlesLost, game.statNeutralized, game.statSuffered, game.statDisasters);
    Text(buf, x, y, 10, C_TEXT); y += 16;
    if (game.nukesUsed) { snprintf(buf, sizeof(buf), "Armes nucléaires employées : %d.", game.nukesUsed); Text(buf, x, y, 10, C_BAD); }
    y += 22;

    int parts[SC_COUNT];
    int score = Game_Score(&game, parts);
    Text("DÉTAIL DU SCORE", x, y, 10, C_ACCENT);
    y += 16;
    int maxPart = 1;
    for (int i = 0; i < SC_COUNT; i++) if (abs(parts[i]) > maxPart) maxPart = abs(parts[i]);
    for (int i = 0; i < SC_COUNT; i++) {
        Text(SCORE_PART_NAMES[i], x, y, 10, C_DIM);
        Bar((Rectangle){ x + 120, y + 1, 500, 8 }, (float)abs(parts[i]), (float)maxPart, parts[i] < 0 ? C_BAD : C_ACCENT);
        snprintf(buf, sizeof(buf), "%d", parts[i]);
        Text(buf, x + 630, y, 10, parts[i] < 0 ? C_BAD : C_TEXT);
        y += 14;
    }
    y += 8;
    snprintf(buf, sizeof(buf), "SCORE FINAL : %d", score);
    Text(buf, 640 - TextW(buf, 20) / 2, y, 20, C_SEL);

    if (Button((Rectangle){ VW / 2 - 140, 530, 280, 40 }, "REJOUER", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, 580, 280, 40 }, "MENU", true, 20)) screen = SCR_TITLE;
}

static void DrawGame(void)
{
    ClearBackground(C_BG);
    bool report = game.reportReady && showReports && game.pendingDilemma < 0 && game.state == GAME_RUNNING;
    if (!showReports) game.reportReady = false;
    activeLayer = (game.pendingDilemma >= 0 || report) ? 1 : 0;
    inputLayer = 0;

    DrawMap();
    DrawLog();
    DrawTopBar();

    Panel((Rectangle){ RP_X - 4, 48, RP_W + 8, VH - 52 }, C_PANEL);
    static const char *tabNames[TAB_COUNT] = {
        "PROVINCE", "ARMÉE", "NATIONS", "DÉFENSE", "HYBRIDE", "SOCIÉTÉ", "ÉCONOMIE", "POLITIQUE", "PROJETS"
    };
    float tw = (RP_W - (TAB_COUNT - 1) * 2) / (float)TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        Rectangle r = { RP_X + i * (tw + 2), 52, tw, 28 };
        bool sel = tab == (Tab)i, h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 255 } : h ? C_PANEL2 : C_BG);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(tabNames[i], r.x + (r.width - TextW(tabNames[i], 10)) / 2, r.y + 9, 10, sel ? C_TEXT : C_DIM);
        if (h && clicked) { tab = (Tab)i; clicked = false; }
    }
    switch (tab) {
    case TAB_PROVINCE:  DrawProvinceTab(); break;
    case TAB_ARMEE:     DrawArmyTab(); break;
    case TAB_NATIONS:   DrawNationsTab(); break;
    case TAB_DEFENSE:   DrawDefenseTab(); break;
    case TAB_HYBRIDE:   DrawHybridTab(); break;
    case TAB_SOCIETE:   DrawSocietyTab(); break;
    case TAB_ECONOMIE:  DrawEconomyTab(); break;
    case TAB_POLITIQUE: DrawPolitiqueTab(); break;
    case TAB_PROJETS:   DrawProjectsTab(); break;
    default: break;
    }

    Rectangle eb = { RP_X + RP_W - 200, VH - 46, 200, 38 };
    bool endTurn = Button(eb, "FIN DU TOUR  >>", game.state == GAME_RUNNING, 20);
    Tooltip(eb, "Résoudre le trimestre (Espace ou Entrée)");
    char lbl[64];
    snprintf(lbl, sizeof(lbl), "PC restants : %d", game.commandPoints);
    Text(lbl, RP_X + 10, VH - 32, 10, C_ACCENT);
    if (activeLayer == 0 && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) endTurn = true;
    if (endTurn && activeLayer == 0) {
        Game_EndTurn(&game);
        logScroll = 0;
        strikeArmed = false;
        for (int i = 0; i < MAX_UNITS; i++) if (!game.units[i].active) uiSel[i] = false;
        if (game.turn % 4 == 0 && game.state == GAME_RUNNING) Game_Save(&game, AUTOSAVE_FILE);
    }
    if (activeLayer == 0) {
        for (int i = 0; i < TAB_COUNT; i++) if (IsKeyPressed(KEY_ONE + i)) tab = (Tab)i;
        if (IsKeyPressed(KEY_M)) mapMode = (MapMode)((mapMode + 1) % MAP_MODE_COUNT);
        if (IsKeyPressed(KEY_F5)) SaveGame();
        if (IsKeyPressed(KEY_F9) && FileExists(SAVE_FILE)) LoadGame(SAVE_FILE);
    }

    if (game.pendingDilemma >= 0) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawDilemma();
    } else if (report) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawReport();
    }

    if (game.state != GAME_RUNNING) { screen = SCR_END; inProgress = false; }
    else if (activeLayer == 0 && IsKeyPressed(KEY_ESCAPE)) screen = SCR_TITLE;
}

/* ------------------------------------------------------------------ */
/* Boucle principale                                                   */
/* ------------------------------------------------------------------ */
static void UpdateView(void)
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float s = fminf(sw / VW, sh / VH);
    if (s != viewScale) {
        bool integer = fabsf(s - roundf(s)) < 0.01f;
        SetTextureFilter(target.texture, integer ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    }
    viewScale = s;
    viewOffset = (Vector2){ (sw - VW * s) / 2, (sh - VH * s) / 2 };
    Vector2 m = GetMousePosition();
    mouse = (Vector2){ (m.x - viewOffset.x) / s, (m.y - viewOffset.y) / s };
    clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    rclicked = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(VW, VH, "Bouclier 2050");
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    int mon = GetCurrentMonitor();
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    if (mw > 0 && mh > 0 && (mw < VW + 40 || mh < VH + 80)) {
        float s = fminf((mw - 40) / (float)VW, (mh - 80) / (float)VH);
        SetWindowSize((int)(VW * s), (int)(VH * s));
    }

    target = LoadRenderTexture(VW, VH);
    viewScale = 0;
    Image img = GenImageColor(MAP_PW, MAP_PH, C_SEA);
    mapTex = LoadTextureFromImage(img);
    UnloadImage(img);
    Game_Init(&game, (unsigned)time(NULL), DIFF_NORMAL);

    while (!WindowShouldClose() && !quitRequested) {
        float dt = GetFrameTime();
        timeAcc += dt;
        if (toastTime > 0) toastTime -= dt;
        UpdateView();
        tooltip[0] = 0;
        inputLayer = activeLayer = 0;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        BeginTextureMode(target);
        switch (screen) {
        case SCR_TITLE: DrawTitle(); break;
        case SCR_HELP:  DrawHelp(); break;
        case SCR_GAME:  DrawGame(); break;
        case SCR_END:   DrawEnd(); break;
        }
        DrawTooltip();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, (Rectangle){ 0, 0, VW, -VH },
                       (Rectangle){ viewOffset.x, viewOffset.y, VW * viewScale, VH * viewScale },
                       (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    UnloadTexture(mapTex);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
