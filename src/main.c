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
#include <string.h>
#include <time.h>

#define VW 1280
#define VH 720

typedef enum { SCR_TITLE, SCR_HELP, SCR_GAME, SCR_END } Screen;
typedef enum {
    TAB_PROVINCE, TAB_NATIONS, TAB_DEFENSE, TAB_HYBRIDE, TAB_SOCIETE, TAB_POLITIQUE, TAB_PROJETS, TAB_COUNT
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

static const Color DOMAIN_COLORS[D_COUNT] = {
    { 150, 170,  90, 255 }, {  70, 150, 230, 255 }, { 120, 210, 240, 255 }, { 200, 160, 255, 255 },
    {  80, 230, 170, 255 }, { 250, 200,  80, 255 }, { 250, 120, 200, 255 },
};

static const Color FACTION_COLORS[F_COUNT] = {
    { 200, 100, 100, 255 }, { 230, 180,  90, 255 }, {  80, 200, 200, 255 }, { 220, 100, 230, 255 },
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
static float    attackDivs = 3;
static RenderTexture2D target;
static float    viewScale = 1;
static Vector2  viewOffset;
static Vector2  mouse;
static bool     clicked;
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
    char tip[400];
    snprintf(tip, sizeof(tip), "%s\n%s%s%s", label, desc, blocked ? "\nIndisponible : " : "", blocked ? blocked : "");
    Tooltip(r, tip);
    return res;
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
static float     nationLabel[N_COUNT][3];   /* x, y, nb de cellules */

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
        /* zebrures d'occupation : la couleur de la nation d'origine apparait */
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
    /* contour blanc de la province selectionnee */
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
                        mapPixels[(row * CELL_H + y) * MAP_PW + col * CELL_W + x] = (Color){ 255, 255, 255, 255 };
        }
    }
    UpdateTexture(mapTex, mapPixels);
}

static Vector2 ProvinceCenter(int p)
{
    return (Vector2){ MAP_X + PROVINCE_LABEL[p][0] * CELL_W + 1.5f, MAP_Y + PROVINCE_LABEL[p][1] * CELL_H + 2 };
}

static void DrawMap(void)
{
    unsigned sig = ComputeSignature();
    if (sig != mapSignature) { mapSignature = sig; RebuildMap(); }
    DrawTexture(mapTex, MAP_X, MAP_Y, WHITE);
    DrawRectangleLines(MAP_X - 1, MAP_Y - 1, MAP_PW + 2, MAP_PH + 2, C_BORDER);

    /* noms des nations */
    if (mapMode == MAP_POLITIQUE) {
        for (int n = 0; n < N_COUNT; n++) {
            if (nationLabel[n][2] < 60) continue;
            float x = MAP_X + nationLabel[n][0] / nationLabel[n][2] * CELL_W;
            float y = MAP_Y + nationLabel[n][1] / nationLabel[n][2] * CELL_H;
            const char *nm = NATIONS[n].name;
            int w = TextW(nm, 10);
            Text(nm, x - w / 2 + 1, y - 4, 10, (Color){ 0, 0, 0, 170 });
            Text(nm, x - w / 2, y - 5, 10, n == N_FRANCE ? WHITE : (Color){ 235, 235, 235, 220 });
        }
    }

    /* fronts et revoltes */
    float pulse = 0.5f + 0.5f * sinf(timeAcc * 5);
    for (int p = 0; p < PROV_COUNT; p++) {
        Vector2 c = ProvinceCenter(p);
        if (War_ProvinceAttackable(&game, p)) {
            DrawCircleV(c, 6 + pulse * 2, Fade(C_WAR, 0.35f));
            DrawLineEx((Vector2){ c.x - 4, c.y - 4 }, (Vector2){ c.x + 4, c.y + 4 }, 2, C_WAR);
            DrawLineEx((Vector2){ c.x - 4, c.y + 4 }, (Vector2){ c.x + 4, c.y - 4 }, 2, C_WAR);
        }
        if (game.prov[p].revolt) {
            DrawCircleV(c, 6, Fade((Color){ 190, 60, 220, 255 }, 0.5f + 0.4f * pulse));
            Text("!", c.x - 2, c.y - 5, 10, WHITE);
        }
    }

    /* survol et selection */
    hoverProvince = inputLayer == activeLayer ? ProvinceAt(mouse) : -1;
    if (hoverProvince >= 0) {
        const Province *pr = &game.prov[hoverProvince];
        char core[80] = "";
        if (pr->core != pr->owner) snprintf(core, sizeof(core), "\nOccupée - population %s", NATIONS[pr->core].name);
        snprintf(tooltip, sizeof(tooltip), "%s (%s)%s\n%.1f M hab. - habitabilité %.0f - militance %.1f%s",
                 PROVINCE_NAMES[hoverProvince], NATIONS[pr->owner].name, core, pr->pop, pr->hab,
                 Soc_ProvinceMilitancy(pr), pr->revolt ? "\nEN RÉVOLTE" : "");
        if (clicked) {
            selProvince = hoverProvince;
            if (pr->owner != N_FRANCE) selNation = pr->owner;
            if (tab != TAB_NATIONS) tab = TAB_PROVINCE;
            clicked = false;
        }
    }

    /* modes de carte */
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
        "Zébrures : territoire occupé.  X rouge : province ennemie attaquable.",
        "Habitabilité : vert = tempéré, rouge = invivable.",
        "Militance : vert = calme, rouge = agitée, violet = révolte.",
        "Densité de population.",
    };
    Text(legend[mapMode], MAP_X + 6, MAP_Y + 6, 10, (Color){ 230, 230, 230, 200 });
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
    Text("BOUCLIER 2050", 10, 6, 20, C_ACCENT);
    snprintf(buf, sizeof(buf), "%s - tour %d/%d - +%.2f°C", date, game.turn + 1, TOTAL_TURNS, game.temp);
    Text(buf, 10, 28, 10, C_TEXT);

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
            snprintf(buf, sizeof(buf), "%+.1f/tour", Soc_Income(&game) - Game_Upkeep(&game));
            Text(buf, area.x + TextW("000", 20) + 22, area.y + 20, 10, C_DIM);
        }
        char tip[300];
        if (r >= R_ENERGIE && r <= R_ALIM) {
            int k = r == R_ALIM ? 0 : r == R_EAU ? 1 : 2;
            snprintf(tip, sizeof(tip), "%s : %.1f\n%s\nProduction %.0f / besoins %.0f", RESOURCE_NAMES[r], v,
                     RESOURCE_DESC[r], game.prod[k], game.cons[k]);
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
    Text(value, x + 130, y, 10, c);
}

static const char *DECLARE_DESC =
    "Ouvre les hostilités (1 PC). Coûte de la cohésion selon le régime, de l'influence et de la réputation. "
    "Attaquer un membre de l'UE fait exclure la France et peut déclencher une coalition.";

static void DrawProvinceTab(void)
{
    int p = selProvince;
    const Province *pr = &game.prov[p];
    const Nation *own = &game.nat[pr->owner];
    char buf[200];
    float x = RP_X + 10, y = RP_Y + 6;

    Text(PROVINCE_NAMES[p], x, y, 20, C_TEXT);
    y += 24;
    DrawRectangle((int)x, (int)y + 1, 10, 10, NationColor(pr->owner));
    snprintf(buf, sizeof(buf), "%s  -  %s%s", NATIONS[pr->owner].name, REGIMES[own->regime].name,
             own->failed ? "  -  ÉTAT FAILLI" : "");
    Text(buf, x + 16, y, 10, C_TEXT);
    y += 14;
    if (pr->core != pr->owner) {
        snprintf(buf, sizeof(buf), "Territoire occupé : population de culture %s. Intégration %.0f%%.",
                 NATIONS[pr->core].name, pr->integration);
        Text(buf, x, y, 10, C_WARN);
        y += 14;
    }
    if (pr->revolt) {
        snprintf(buf, sizeof(buf), "EN RÉVOLTE depuis %d tour(s) : production effondrée, recettes nulles.", pr->revolt);
        Text(buf, x, y, 10, (Color){ 220, 120, 240, 255 });
        y += 14;
    }
    y += 6;

    float c2 = x + 280;
    snprintf(buf, sizeof(buf), "%.2f M", pr->pop);
    StatLine(x, y, "Population", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.0f / 100", pr->hab);
    StatLine(c2, y, "Habitabilité", buf, LevelColor(pr->hab));
    y += 14;
    snprintf(buf, sizeof(buf), "%.1f / 10", Soc_ProvinceMilitancy(pr));
    StatLine(x, y, "Militance", buf, Soc_ProvinceMilitancy(pr) > 6 ? C_BAD : C_TEXT);
    snprintf(buf, sizeof(buf), "x%.2f", pr->prosperity);
    StatLine(c2, y, "Prospérité", buf, C_TEXT);
    y += 14;
    snprintf(buf, sizeof(buf), "%.0f%%", pr->foreign * 100);
    StatLine(x, y, "Pop. à intégrer", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.0f%%", pr->damage * 100);
    StatLine(c2, y, "Dévastation", buf, pr->damage > 0.05f ? C_BAD : C_TEXT);
    y += 14;
    snprintf(buf, sizeof(buf), "-%.0f k / +%.0f k", pr->emig * 1000, pr->immig * 1000);
    StatLine(x, y, "Émigr. / immigr.", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.1f Md/tour", Soc_ProvinceIncome(&game, p));
    StatLine(c2, y, "Recettes fiscales", buf, C_TEXT);
    y += 14;
    snprintf(buf, sizeof(buf), "%.0f / %.0f / %.0f", PROVINCES[p].food * pr->hab / 100, PROVINCES[p].water * pr->hab / 100,
             PROVINCES[p].energy);
    StatLine(x, y, "Alim / eau / énergie", buf, C_TEXT);
    snprintf(buf, sizeof(buf), "%.0f%%", PROVINCES[p].aridity * 100);
    StatLine(c2, y, "Exposition climat", buf, C_TEXT);
    y += 22;

    Text("STRATES SOCIALES (militance)", x, y, 10, C_DIM);
    y += 14;
    for (int s = 0; s < S_COUNT; s++) {
        snprintf(buf, sizeof(buf), "%s (%.0f%%)", STRATA_NAMES[s], STRATA_SHARE[s] * 100);
        Text(buf, x, y, 10, C_TEXT);
        Bar((Rectangle){ x + 170, y + 1, 260, 8 }, pr->mil[s], 10, pr->mil[s] > 6 ? C_BAD : pr->mil[s] > 4 ? C_WARN : C_GOOD);
        snprintf(buf, sizeof(buf), "%.1f", pr->mil[s]);
        Text(buf, x + 440, y, 10, C_TEXT);
        y += 14;
    }
    y += 10;

    if (pr->owner == N_FRANCE) {
        TextWrap("Province française. Sa stabilité dépend du régime, des pénuries, de la guerre et de l'intégration "
                 "des nouveaux arrivants.", x, y, RP_W - 20, 10, C_DIM);
        return;
    }
    Panel((Rectangle){ RP_X + 6, y, RP_W - 12, 156 }, C_PANEL2);
    y += 8;
    if (War_ProvinceAttackable(&game, p)) {
        float avail = War_AvailableDivisions(&game);
        if (attackDivs > avail) attackDivs = floorf(avail);
        if (attackDivs < 1 && avail >= 1) attackDivs = 1;
        Text("OFFENSIVE", x, y, 20, C_WAR);
        y += 26;
        snprintf(buf, sizeof(buf), "Divisions engagées : %.0f / %.1f disponibles", attackDivs, avail);
        Text(buf, x, y + 6, 10, C_TEXT);
        if (Button((Rectangle){ x + 300, y, 26, 22 }, "-", attackDivs > 1, 20)) attackDivs -= 1;
        if (Button((Rectangle){ x + 330, y, 26, 22 }, "+", attackDivs + 1 <= avail, 20)) attackDivs += 1;
        if (Button((Rectangle){ x + 360, y, 60, 22 }, "Toutes", avail >= 1, 10)) attackDivs = floorf(avail);
        y += 30;
        float odds = War_AttackOdds(&game, p, attackDivs);
        snprintf(buf, sizeof(buf), "Lancer l'offensive (%.0f%% de succès)", odds * 100);
        Rectangle br = { x, y, 300, 28 };
        if (Button(br, buf, avail >= 1 && attackDivs >= 1, 10)) War_Attack(&game, p, attackDivs);
        Tooltip(br, "Puissance = divisions x qualité (régime, synergies air/espace/cyber/EM). Une attaque par la mer seule est "
                    "pénalisée sans capacité navale de 6. Victoire : la province est annexée mais doit être intégrée.");
        y += 36;
        Text("Les divisions engagées ne défendent qu'à moitié le territoire national ce tour.", x, y, 10, C_DIM);
        return;
    }
    snprintf(buf, sizeof(buf), "Relation de %s avec la France : %+.0f", NATIONS[pr->owner].name, own->relation);
    Text(buf, x, y, 10, own->relation < 0 ? C_BAD : C_GOOD);
    y += 18;
    if (ActionButton((Rectangle){ x, y, 260, 26 }, "Ultimatum : exiger la cession", War_UltimatumBlocked(&game, p),
                     "Exige la province sous menace de guerre (1 PC). Réussit si notre armée est 2,5 fois plus puissante. "
                     "Échec : relation -25, Influence -5. Réputation d'agresseur accrue dans tous les cas."))
        War_Ultimatum(&game, p);
    if (ActionButton((Rectangle){ x + 270, y, 260, 26 }, "Déclarer la guerre", War_DeclareBlocked(&game, pr->owner), DECLARE_DESC))
        War_Declare(&game, pr->owner);
    y += 36;
    snprintf(buf, sizeof(buf), "Armée %s : %.1f divisions (qualité %.2f). La nôtre : %.1f (qualité %.2f).",
             NATIONS[pr->owner].name, own->army, War_Quality(&game, pr->owner),
             game.nat[N_FRANCE].army, War_Quality(&game, N_FRANCE));
    TextWrap(buf, x, y, RP_W - 30, 10, C_DIM);
}

static void DrawNationsTab(void)
{
    char buf[200];
    Nation *fr = &game.nat[N_FRANCE];
    float x = RP_X + 10, y = RP_Y + 6;

    Panel((Rectangle){ RP_X + 6, y - 2, RP_W - 12, 58 }, C_PANEL2);
    snprintf(buf, sizeof(buf), "ARMÉE : %.1f divisions (max %.0f)  -  qualité %.2f  -  disponibles %.1f",
             fr->army, War_ArmyMax(&game, N_FRANCE), War_Quality(&game, N_FRANCE), War_AvailableDivisions(&game));
    Text(buf, x, y + 4, 10, C_TEXT);
    snprintf(buf, sizeof(buf), "Entretien %.1f Md/tour. Réputation d'agresseur : %.0f/100", War_ArmyUpkeep(&game), game.aggressionMemory);
    Text(buf, x, y + 18, 10, C_DIM);
    float rc = fr->regime >= RG_JUNTE ? 3 : 4;
    snprintf(buf, sizeof(buf), "Recruter (%.0f Md)", rc);
    Rectangle rb = { x, y + 32, 160, 20 };
    if (Button(rb, buf, fr->army + 1 <= War_ArmyMax(&game, N_FRANCE) && game.res[R_BUDGET] >= rc, 10)) War_Recruit(&game);
    Tooltip(rb, "+1 division. Le plafond dépend de la population intégrée et du régime (mobilisation). Entretien 0,5 Md/tour.");
    if (Button((Rectangle){ x + 170, y + 32, 120, 20 }, "Démobiliser", War_AvailableDivisions(&game) >= 1, 10)) War_Disband(&game);
    y += 64;

    float lx = RP_X + 6, lw = 250;
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &game.nat[n];
        Rectangle r = { lx, y + (n - 1) * 19, lw, 18 };
        bool h = Hover(r);
        DrawRectangleRec(r, n == selNation ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, NationColor(n));
        Text(NATIONS[n].name, r.x + 8, r.y + 4, 10, na->alive ? C_TEXT : C_DIM);
        if (!na->alive) {
            Text("disparu", r.x + 140, r.y + 4, 10, C_DIM);
        } else {
            snprintf(buf, sizeof(buf), "%+.0f", na->relation);
            Text(buf, r.x + 140, r.y + 4, 10, na->relation < -20 ? C_BAD : na->relation > 20 ? C_GOOD : C_TEXT);
            const char *st = na->atWar ? "GUERRE" : na->warTarget >= 0 ? "conflit" : na->truce ? "trêve" : na->trade ? "accord" : "";
            Text(st, r.x + 180, r.y + 4, 10, na->atWar ? C_WAR : C_DIM);
            if (na->eu) Text("UE", r.x + 232, r.y + 4, 10, C_ACCENT);
        }
        if (h && clicked) { selNation = n; clicked = false; }
    }

    int n = selNation == N_FRANCE ? N_ALLEMAGNE : selNation;
    const Nation *na = &game.nat[n];
    float dx = lx + lw + 8, dw = RP_X + RP_W - dx - 6;
    Panel((Rectangle){ dx, y, dw, 26 * 19 - 4 }, C_PANEL2);
    dx += 8;
    float dy = y + 6, bw = dw - 16;
    Text(NATIONS[n].name, dx, dy, 20, NationColor(n));
    dy += 26;
    snprintf(buf, sizeof(buf), "%s%s%s", REGIMES[na->regime].name, na->eu ? " - UE" : "", na->failed ? " - FAILLI" : "");
    Text(buf, dx, dy, 10, C_TEXT); dy += 16;
    snprintf(buf, sizeof(buf), "Relation : %+.0f", na->relation);
    Text(buf, dx, dy, 10, na->relation < 0 ? C_BAD : C_GOOD); dy += 14;
    snprintf(buf, sizeof(buf), "Population : %.1f M (%d prov.)", Soc_NationPop(&game, n), Soc_NationProvinces(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    snprintf(buf, sizeof(buf), "Armée : %.1f div. (max %.0f)", na->army, War_ArmyMax(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    snprintf(buf, sizeof(buf), "Qualité militaire : %.2f", War_Quality(&game, n));
    Text(buf, dx, dy, 10, C_TEXT); dy += 14;
    if (na->atWar) { snprintf(buf, sizeof(buf), "En guerre contre nous (%d tours)", na->warTurns); Text(buf, dx, dy, 10, C_WAR); dy += 14; }
    if (na->truce) { snprintf(buf, sizeof(buf), "Trêve : %d tours", na->truce); Text(buf, dx, dy, 10, C_DIM); dy += 14; }
    if (na->warTarget >= 0) { snprintf(buf, sizeof(buf), "Envahit : %s", NATIONS[na->warTarget].name); Text(buf, dx, dy, 10, C_WAR); dy += 14; }
    if (na->trade) { Text("Accord commercial actif", dx, dy, 10, C_GOOD); dy += 14; }
    dy += 8;
    if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Rapprochement (5 Influence)", War_DiploBlocked(&game, n, 0),
                     "Améliore la relation de 12 (1 PC)."))
        War_Diplomacy(&game, n, 0);
    dy += 28;
    if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Accord commercial", War_DiploBlocked(&game, n, 1),
                     "Importe 25% de leurs excédents d'alimentation, d'eau et d'énergie, +0,4 Md/tour (1 PC). Relation 25 requise."))
        War_Diplomacy(&game, n, 1);
    dy += 28;
    if (na->atWar) {
        if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Proposer la paix", War_PeaceBlocked(&game, n),
                         "Acceptée si l'ennemi est en difficulté ou si la guerre s'éternise. La France garde ses conquêtes (1 PC)."))
            War_ProposePeace(&game, n);
    } else if (ActionButton((Rectangle){ dx, dy, bw, 24 }, "Déclarer la guerre", War_DeclareBlocked(&game, n), DECLARE_DESC)) {
        War_Declare(&game, n);
    }
    dy += 34;
    TextWrap("En guerre, cliquez sur la carte une province ennemie marquée d'un X pour lancer une offensive.", dx, dy, bw, 10, C_DIM);
}

static void DrawDefenseTab(void)
{
    int t = selTheater;
    Domain d = THEATERS[t].domain;
    Color dc = DOMAIN_COLORS[d];
    float y = RP_Y + 6;
    char buf[200];

    Text(THEATERS[t].name, RP_X + 10, y, 20, dc);
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
        Tooltip(br, "Recrutement et équipement, disponible après 2 tours. Capacité : 0,1 Md/tour, posture engagée : 0,6 Md/tour. "
                    "Les capacités Air, Espace, Cyber et EM renforcent aussi l'armée conventionnelle.");
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
    float y = RP_Y + 6;
    snprintf(buf, sizeof(buf), "Points de commandement : %d / %d (partagés avec la diplomatie et la guerre)", game.commandPoints, BASE_CP);
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
    char buf[200];
    float x = RP_X + 10, y = RP_Y + 6;
    float pop = Soc_NationPop(&game, N_FRANCE);
    float mil[S_COUNT] = { 0 }, foreign = 0, popCore = 0;
    int revolts = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &game.prov[p];
        if (pr->owner != N_FRANCE) continue;
        for (int s = 0; s < S_COUNT; s++) mil[s] += pr->mil[s] * pr->pop;
        foreign += pr->foreign * pr->pop;
        if (pr->core == N_FRANCE) popCore += pr->pop;
        else foreign += pr->pop * (1 - pr->foreign) * (1 - pr->integration / 100.0f);
        if (pr->revolt) revolts++;
    }
    Text("POPULATION FRANÇAISE", x, y, 20, C_ACCENT);
    y += 26;
    snprintf(buf, sizeof(buf), "%.1f millions d'habitants dans %d provinces (%.1f M de culture française)", pop,
             Soc_NationProvinces(&game, N_FRANCE), popCore);
    Text(buf, x, y, 10, C_TEXT); y += 14;
    snprintf(buf, sizeof(buf), "Population non intégrée (migrants, territoires conquis) : %.0f%%   -   révoltes : %d",
             foreign / pop * 100, revolts);
    Text(buf, x, y, 10, C_TEXT); y += 14;
    snprintf(buf, sizeof(buf), "Migrants admis : %.0f k   refoulés : %.0f k   départs : %.0f k  (dernier trimestre)",
             game.migrantsIn * 1000, game.migrantsRefused * 1000, game.migrantsOut * 1000);
    Text(buf, x, y, 10, C_TEXT); y += 22;

    Text("MILITANCE PAR STRATE", x, y, 10, C_DIM); y += 14;
    for (int s = 0; s < S_COUNT; s++) {
        float m = mil[s] / pop;
        Text(STRATA_NAMES[s], x, y, 10, C_TEXT);
        Bar((Rectangle){ x + 130, y + 1, 300, 8 }, m, 10, m > 6 ? C_BAD : m > 4 ? C_WARN : C_GOOD);
        snprintf(buf, sizeof(buf), "%.1f", m);
        Text(buf, x + 440, y, 10, C_TEXT);
        y += 14;
    }
    y += 2 + TextWrap("Travailleurs : sensibles à l'alimentation. Classes moyennes : à l'énergie et à la guerre. Élites : à "
                      "l'impôt et à la dette. Le régime satisfait ou réprime chaque strate. Au-delà de 6,5, les révoltes éclatent.",
                      x, y + 2, RP_W - 20, 10, C_DIM);
    y += 10;

    Text("RESSOURCES (production / besoins)", x, y, 10, C_DIM); y += 14;
    static const char *rn[3] = { "Alimentation", "Eau", "Énergie" };
    for (int r = 0; r < 3; r++) {
        snprintf(buf, sizeof(buf), "%s %.0f / %.0f", rn[r], game.prod[r], game.cons[r]);
        Text(buf, x + r * 180, y, 10, game.prod[r] >= game.cons[r] ? C_GOOD : C_BAD);
    }
    y += 14;
    snprintf(buf, sizeof(buf), "Recettes %.1f Md - dépenses %.1f Md (dont armée %.1f)", Soc_Income(&game), Game_Upkeep(&game),
             War_ArmyUpkeep(&game));
    Text(buf, x, y, 10, C_TEXT); y += 22;

    int n = game.turn + 1 < HISTORY_LEN ? game.turn + 1 : HISTORY_LEN;
    float pmax = 0;
    for (int i = 0; i < n; i++) if (game.histPop[i] > pmax) pmax = game.histPop[i];
    Sparkline((Rectangle){ RP_X + 6, y, RP_W - 12, 74 }, game.histPop, n, 0, pmax * 1.1f + 1, C_ACCENT, "Population (M)");
    y += 80;
    Sparkline((Rectangle){ RP_X + 6, y, (RP_W - 18) / 2, 74 }, game.histCohesion, n, 0, 100, C_GOOD, "Cohésion");
    Sparkline((Rectangle){ RP_X + 12 + (RP_W - 18) / 2, y, (RP_W - 18) / 2, 74 }, game.histTemp, n, 1, 4, C_CLIMATE, "Température (+°C)");
}

static void DrawPolitiqueTab(void)
{
    char buf[200];
    float x = RP_X + 10, y = RP_Y + 4;
    Text("RÉGIME POLITIQUE", x, y, 20, C_ACCENT);
    if (game.regimeCooldown > 0) {
        snprintf(buf, sizeof(buf), "prochain changement dans %d tours", game.regimeCooldown);
        Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y + 6, 10, C_DIM);
    }
    y += 24;
    for (int r = 0; r < RG_COUNT; r++) {
        const RegimeDef *rg = &REGIMES[r];
        bool cur = game.nat[N_FRANCE].regime == (Regime)r;
        Rectangle row = { RP_X + 6, y, RP_W - 12, 46 };
        DrawRectangleRec(row, cur ? (Color){ 40, 62, 92, 255 } : C_PANEL2);
        DrawRectangleLinesEx(row, 1, cur ? C_ACCENT : C_BORDER);
        Text(rg->name, row.x + 8, row.y + 4, 10, cur ? C_TEXT : C_DIM);
        snprintf(buf, sizeof(buf), "Impôt x%.2f  Mobilisation %.2f  Répression %.2f  Qualité x%.2f  Influence %+.1f",
                 rg->taxEff, rg->mobilization, rg->repression, rg->quality, rg->influence);
        Text(buf, row.x + 8, row.y + 18, 10, C_DIM);
        snprintf(buf, sizeof(buf), "Lassitude de guerre %.1f  Coût d'agression %.0f  Intégration x%.1f  Projets x%.2f",
                 rg->warWeariness, rg->aggressionCost, rg->assimilation, rg->projectCost);
        Text(buf, row.x + 8, row.y + 31, 10, C_DIM);
        if (!cur) {
            snprintf(buf, sizeof(buf), "Adopter (-%.0f coh.)", Soc_RegimeChangeCost(&game, r));
            Rectangle br = { row.x + row.width - 130, row.y + 4, 124, 20 };
            char desc[400];
            snprintf(desc, sizeof(desc), "%s\nMilitance accrue pendant la transition. Quitter la démocratie fait suspendre la France de l'UE.",
                     rg->desc);
            if (ActionButton(br, buf, Soc_RegimeBlocked(&game, r), desc)) Soc_ChangeRegime(&game, r);
        } else {
            Tooltip(row, rg->desc);
        }
        y += 49;
    }
    y += 4;
    Text("DOCTRINE MIGRATOIRE", x, y, 10, C_ACCENT);
    y += 14;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { RP_X + 6 + i * 184, y, 180, 22 };
        bool sel = game.migrationDoctrine == i, h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(MIGRATION_DOCTRINES[i], r.x + (r.width - TextW(MIGRATION_DOCTRINES[i], 10)) / 2, r.y + 6, 10, sel ? C_TEXT : C_DIM);
        Tooltip(r, MIGRATION_DESC[i]);
        if (h && clicked) { Game_SetMigrationDoctrine(&game, i); clicked = false; }
    }
    y += 26;
    TextWrap(MIGRATION_DESC[game.migrationDoctrine], x, y, RP_W - 20, 10, C_DIM);
    y += 34;
    Text("NIVEAU D'ALERTE", x, y, 10, C_ACCENT);
    y += 14;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { RP_X + 6 + i * 184, y, 180, 22 };
        bool sel = game.alertLevel == i, h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(ALERT_LEVELS[i], r.x + (r.width - TextW(ALERT_LEVELS[i], 10)) / 2, r.y + 6, 10, sel ? C_TEXT : C_DIM);
        Tooltip(r, ALERT_DESC[i]);
        if (h && clicked) { Game_SetAlertLevel(&game, i); clicked = false; }
    }
    y += 26;
    Text(ALERT_DESC[game.alertLevel], x, y, 10, C_DIM);
}

static void DrawProjectsTab(void)
{
    char buf[160];
    float y = RP_Y + 6;
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
        Color c = C_TEXT;
        switch (e->kind) {
        case EV_GOOD:    c = C_GOOD; break;
        case EV_BAD:     c = C_BAD; break;
        case EV_CLIMATE: c = C_CLIMATE; break;
        case EV_EVENT:   c = C_EVENT; break;
        case EV_WAR:     c = C_WAR; break;
        default:         c = C_TEXT; break;
        }
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
    if (game.pendingDilemma >= DLM_ALLY_CALL) {
        char buf[200];
        int n = game.dilemmaParam;
        if (game.pendingDilemma == DLM_ALLY_CALL)
            snprintf(buf, sizeof(buf), "%s envahit %s.", NATIONS[n].name, NATIONS[game.nat[n].warTarget].name);
        else
            snprintf(buf, sizeof(buf), "%s demande la fin des combats.", NATIONS[n].name);
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

static void DrawTooltip(void)
{
    if (!tooltip[0]) return;
    char copy[600];
    snprintf(copy, sizeof(copy), "%s", tooltip);
    const char *lines[10];
    int n = 0, w = 0;
    char *s = copy;
    while (s && n < 10) {
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
    Panel((Rectangle){ x, y, (float)w + 16, (float)h + 10 }, (Color){ 12, 16, 22, 245 });
    float yy = y + 6;
    for (int i = 0; i < n; i++)
        yy += TextWrap(lines[i], x + 8, yy, 420, 10, i == 0 ? C_ACCENT : C_TEXT);
}

/* ------------------------------------------------------------------ */
/* Ecrans                                                              */
/* ------------------------------------------------------------------ */
static void NewGame(void)
{
    Game_Init(&game, (unsigned)time(NULL));
    selProvince = PV_FR_NORD;
    selNation = N_ALLEMAGNE;
    tab = TAB_PROVINCE;
    logScroll = 0;
    mapSignature = 0;
    screen = SCR_GAME;
    inProgress = true;
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
    Text("BOUCLIER 2050", VW / 2 - TextW("BOUCLIER 2050", 60) / 2, 100, 60, C_ACCENT);
    const char *sub = "Grande stratégie et guerre hybride en Europe et en Méditerranée, 2030-2060";
    Text(sub, VW / 2 - TextW(sub, 20) / 2, 176, 20, C_TEXT);
    const char *pitch =
        "2030. Le monde a franchi +1,5°C et le réchauffement s'accélère. Sécheresses, mégafeux et effondrement agricole "
        "vident le Sud et jettent des millions de personnes sur les routes. L'eau, l'énergie et la nourriture deviennent des "
        "armes. Puissances, cartels et mercenaires numériques mènent une guerre hybride dans tous les milieux. "
        "Gérez votre population, choisissez votre régime, de la commune utopique à l'État totalitaire, défendez "
        "vos frontières ou conquérez celles des autres. Tenez trente ans.";
    TextWrap(pitch, 280, 222, 720, 10, C_DIM);

    float y = 330;
    if (inProgress) {
        if (Button((Rectangle){ VW / 2 - 140, y, 280, 44 }, "CONTINUER", true, 20)) screen = SCR_GAME;
        y += 54;
    }
    if (Button((Rectangle){ VW / 2 - 140, y, 280, 44 }, "NOUVELLE PARTIE", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, y + 54, 280, 44 }, "COMMENT JOUER", true, 20)) screen = SCR_HELP;
    if (Button((Rectangle){ VW / 2 - 140, y + 108, 280, 44 }, "QUITTER", true, 20)) quitRequested = true;

    const char *dom = "Terre - Mer - Air - Espace - Cyber - Informationnel - Electromagnétique";
    Text(dom, VW / 2 - TextW(dom, 10) / 2, 600, 10, C_DIM);
    Text("Fiction prospective. Contours : Natural Earth (domaine public).", 10, VH - 18, 10, C_DIM);
}

static void DrawHelp(void)
{
    DrawBackdrop();
    Panel((Rectangle){ 110, 30, 1060, 660 }, C_PANEL);
    Text("COMMENT JOUER", 130, 44, 20, C_ACCENT);
    static const char *help[] = {
        "OBJECTIF : tenir 120 trimestres (2030-2060). Défaite si la Cohésion ou la Souveraineté tombe à 0, si le Budget passe sous -60 ou si Paris tombe.",
        "TOUR : agissez puis FIN DU TOUR (Espace). Menaces, climat, population, migrations et guerres sont alors résolus. 3 points de commandement (PC) par tour pour la diplomatie, la guerre et la lutte hybride.",
        "CARTE : 66 provinces en Europe et en Méditerranée, 27 nations. Cliquez une province pour l'inspecter. Modes : politique, climat (habitabilité), militance, population.",
        "POPULATION : chaque province a ses habitants, leur croissance, leur part à intégrer et la militance de trois strates (travailleurs, classes moyennes, élites). Pénuries, guerre, dette et occupation les agitent ; au-delà de 6,5 éclatent des révoltes.",
        "CLIMAT : le réchauffement réduit l'habitabilité, d'abord au Sud. Récoltes et eau baissent, les populations migrent vers les provinces plus vivables et plus riches. Votre doctrine décide combien entrent ; les refoulés deviennent des flux clandestins aux frontières.",
        "RESSOURCES : énergie, eau et alimentation comparent la production de vos provinces (plus projets et accords commerciaux) aux besoins de la population.",
        "GUERRE : déclarez la guerre à un voisin (terre ou mer), puis cliquez ses provinces marquées d'un X pour lancer des offensives. Les conquêtes apportent ressources et recettes mais doivent être intégrées. L'agression ruine votre réputation : l'Europe se réarme et forme des coalitions.",
        "RÉGIME : de la Commune écologique à l'État totalitaire. Chaque régime modifie l'impôt, la mobilisation, la répression, l'intégration, la diplomatie et la satisfaction des strates.",
        "DÉFENSE M2MC : répartissez la capacité de chaque milieu entre 12 théâtres. L'orbite améliore la détection, un spectre brouillé affaiblit l'air et la mer ; air, espace, cyber et EM renforcent aussi l'armée.",
        "HYBRIDE : 4 factions mènent la guerre sous le seuil : Fédération Boréale, Ligue des Sables, Consortium Hydra, Réseau Nébula.",
        "RACCOURCIS : Espace/Entrée = fin du tour, 1-7 = onglets, M = mode de carte, F11 = plein écran, Échap = menu.",
    };
    float y = 76;
    for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
        y += TextWrap(help[i], 130, y, 1020, 10, i % 2 ? C_TEXT : (Color){ 200, 210, 220, 255 }) + 9;
    if (Button((Rectangle){ VW / 2 - 100, 636, 200, 40 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE))
        screen = SCR_TITLE;
}

static void DrawEnd(void)
{
    DrawBackdrop();
    bool won = game.state == GAME_WON;
    const char *title = won ? "LA FRANCE A TENU" : "DÉFAITE";
    Text(title, VW / 2 - TextW(title, 60) / 2, 100, 60, won ? C_GOOD : C_BAD);
    const char *why = "";
    switch (game.state) {
    case GAME_WON: why = "2060 : malgré les tempêtes, la France est debout. L'histoire jugera le prix payé."; break;
    case GAME_LOST_COHESION: why = "Effondrement social : émeutes, sécessions locales, le pays se fracture."; break;
    case GAME_LOST_SOUVERAINETE: why = "Perte de souveraineté : la France ne décide plus de son destin."; break;
    case GAME_LOST_BUDGET: why = "Défaut de paiement : les créanciers dictent désormais la politique nationale."; break;
    case GAME_LOST_CONQUEST: why = "Paris est tombée. Le gouvernement capitule."; break;
    default: break;
    }
    Text(why, VW / 2 - TextW(why, 10) / 2, 178, 10, C_TEXT);

    char buf[200], date[32];
    Game_DateString(game.turn, date, sizeof(date));
    Panel((Rectangle){ 390, 210, 500, 270 }, C_PANEL);
    float y = 226;
    snprintf(buf, sizeof(buf), "Fin : %s (+%.2f°C) - régime : %s", date, game.temp, REGIMES[game.nat[N_FRANCE].regime].name);
    Text(buf, 410, y, 10, C_TEXT); y += 18;
    snprintf(buf, sizeof(buf), "Population : %.1f M dans %d provinces", Soc_NationPop(&game, N_FRANCE), Soc_NationProvinces(&game, N_FRANCE));
    Text(buf, 410, y, 10, C_TEXT); y += 18;
    snprintf(buf, sizeof(buf), "Provinces conquises : %d - perdues : %d", game.statConquered, game.statLost);
    Text(buf, 410, y, 10, C_WAR); y += 18;
    snprintf(buf, sizeof(buf), "Menaces neutralisées : %d - subies : %d", game.statNeutralized, game.statSuffered);
    Text(buf, 410, y, 10, C_GOOD); y += 18;
    snprintf(buf, sizeof(buf), "Catastrophes climatiques : %d", game.statDisasters);
    Text(buf, 410, y, 10, C_CLIMATE); y += 24;
    for (int r = 0; r < R_COUNT; r++) {
        snprintf(buf, sizeof(buf), "%s : %.0f", RESOURCE_NAMES[r], game.res[r]);
        Text(buf, 410 + (r % 2) * 240, y, 10, C_DIM);
        if (r % 2) y += 16;
    }
    y += 26;
    snprintf(buf, sizeof(buf), "SCORE : %d", Game_Score(&game));
    Text(buf, 640 - TextW(buf, 20) / 2, y, 20, C_ACCENT);

    if (Button((Rectangle){ VW / 2 - 140, 510, 280, 44 }, "REJOUER", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, 564, 280, 44 }, "MENU", true, 20)) screen = SCR_TITLE;
}

static void DrawGame(void)
{
    ClearBackground(C_BG);
    activeLayer = game.pendingDilemma >= 0 ? 1 : 0;
    inputLayer = 0;

    DrawMap();
    DrawLog();
    DrawTopBar();

    Panel((Rectangle){ RP_X - 4, 48, RP_W + 8, VH - 52 }, C_PANEL);
    static const char *tabNames[TAB_COUNT] = { "PROVINCE", "NATIONS", "DÉFENSE", "HYBRIDE", "SOCIÉTÉ", "POLITIQUE", "PROJETS" };
    float tw = (RP_W - 6 * 2) / (float)TAB_COUNT;
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
    case TAB_NATIONS:   DrawNationsTab(); break;
    case TAB_DEFENSE:   DrawDefenseTab(); break;
    case TAB_HYBRIDE:   DrawHybridTab(); break;
    case TAB_SOCIETE:   DrawSocietyTab(); break;
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
    if (endTurn) {
        Game_EndTurn(&game);
        logScroll = 0;
    }
    if (activeLayer == 0) {
        for (int i = 0; i < TAB_COUNT; i++) if (IsKeyPressed(KEY_ONE + i)) tab = (Tab)i;
        if (IsKeyPressed(KEY_M)) mapMode = (MapMode)((mapMode + 1) % MAP_MODE_COUNT);
    }

    if (game.pendingDilemma >= 0) {
        tooltip[0] = 0;
        inputLayer = 1;
        DrawDilemma();
    }

    if (game.state != GAME_RUNNING) { screen = SCR_END; inProgress = false; }
    if (IsKeyPressed(KEY_ESCAPE)) screen = SCR_TITLE;
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
    Game_Init(&game, (unsigned)time(NULL));

    while (!WindowShouldClose() && !quitRequested) {
        timeAcc += GetFrameTime();
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
