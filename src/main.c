/*
 * BOUCLIER 2050 - interface raylib
 *
 * Rendu volontairement simple (formes 2D + police par defaut, aucun fichier
 * externe) pour tourner sur un maximum de machines. Resolution logique fixe
 * 1280x720, mise a l'echelle automatique de la fenetre.
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
typedef enum { TAB_THEATRES, TAB_FACTIONS, TAB_RESILIENCE, TAB_DOCTRINE, TAB_COUNT } Tab;

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
#define C_CLIMATE   (Color){ 240, 120, 40, 255 }
#define C_EVENT     (Color){ 190, 140, 255, 255 }
#define C_SEA       (Color){  18,  34,  52, 255 }
#define C_LAND      (Color){  44,  56,  50, 255 }
#define C_LAND_FR   (Color){  58,  78,  70, 255 }

static const Color DOMAIN_COLORS[D_COUNT] = {
    { 150, 170,  90, 255 },  /* terre */
    {  70, 150, 230, 255 },  /* mer */
    { 120, 210, 240, 255 },  /* air */
    { 200, 160, 255, 255 },  /* espace */
    {  80, 230, 170, 255 },  /* cyber */
    { 250, 200,  80, 255 },  /* info */
    { 250, 120, 200, 255 },  /* em */
};

static const Color FACTION_COLORS[F_COUNT] = {
    { 120, 170, 255, 255 },
    { 230, 180,  90, 255 },
    {  80, 200, 200, 255 },
    { 220, 100, 230, 255 },
};

/* ------------------------------------------------------------------ */
/* Etat de l'interface                                                 */
/* ------------------------------------------------------------------ */
static Game     game;
static Screen   screen = SCR_TITLE;
static Tab      tab = TAB_THEATRES;
static int      selTheater = TH_NORD;
static RenderTexture2D target;  /* rendu logique 1280x720, mis a l'echelle */
static float    viewScale = 1;
static Vector2  viewOffset;
static Vector2  mouse;
static bool     clicked;
static int      inputLayer;      /* couche courante de dessin */
static int      activeLayer;     /* couche qui recoit la souris */
static char     tooltip[512];
static float    timeAcc;
static int      logScroll;
static bool     quitRequested;
static bool     inProgress;      /* une partie peut etre reprise */

/* ------------------------------------------------------------------ */
/* Primitives d'interface                                              */
/* ------------------------------------------------------------------ */
static void Text(const char *s, float x, float y, int size, Color c)
{
    DrawTextEx(GetFontDefault(), s, (Vector2){ x, y }, (float)size, size / 10.0f, c);
}

static int TextW(const char *s, int size)
{
    return (int)MeasureTextEx(GetFontDefault(), s, (float)size, size / 10.0f).x;
}

/* texte avec retour a la ligne ; renvoie la hauteur utilisee */
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

/* ------------------------------------------------------------------ */
/* Carte                                                               */
/* ------------------------------------------------------------------ */
#define MAP_X 0
#define MAP_Y 44
#define MAP_W 650
#define MAP_H 510

static Vector2 Proj(float lon, float lat)
{
    return (Vector2){ 40 + (lon + 5.2f) * 33.5f, 66 + (51.4f - lat) * 46.0f };
}

static const float FRANCE[][2] = {
    { 2.37f, 51.05f }, { 2.9f, 50.75f }, { 3.6f, 50.5f }, { 4.2f, 50.3f }, { 4.85f, 50.1f },
    { 4.85f, 49.85f }, { 5.8f, 49.55f }, { 6.4f, 49.47f }, { 7.0f, 49.15f }, { 8.2f, 48.97f },
    { 7.8f, 48.5f }, { 7.55f, 47.6f }, { 7.0f, 47.45f }, { 6.1f, 46.6f }, { 6.1f, 46.2f },
    { 6.8f, 46.4f }, { 6.95f, 45.9f }, { 7.15f, 45.5f }, { 6.65f, 45.1f }, { 7.0f, 44.8f },
    { 6.9f, 44.4f }, { 7.7f, 43.8f }, { 7.0f, 43.55f }, { 6.4f, 43.15f }, { 5.4f, 43.2f },
    { 4.6f, 43.4f }, { 3.9f, 43.5f }, { 3.1f, 43.1f }, { 3.15f, 42.45f }, { 2.5f, 42.35f },
    { 1.7f, 42.5f }, { 0.7f, 42.8f }, { -0.3f, 42.8f }, { -1.4f, 43.25f }, { -1.8f, 43.38f },
    { -1.25f, 44.6f }, { -1.2f, 45.7f }, { -1.5f, 46.2f }, { -2.1f, 46.8f }, { -2.2f, 47.2f },
    { -2.6f, 47.5f }, { -3.5f, 47.7f }, { -4.4f, 47.9f }, { -4.75f, 48.35f }, { -4.5f, 48.65f },
    { -3.6f, 48.8f }, { -2.7f, 48.55f }, { -1.9f, 48.7f }, { -1.4f, 48.65f }, { -1.6f, 49.2f },
    { -1.9f, 49.7f }, { -1.25f, 49.65f }, { -1.1f, 49.4f }, { 0.2f, 49.45f }, { 0.1f, 49.7f },
    { 1.4f, 50.1f }, { 1.6f, 50.4f }, { 1.6f, 50.9f },
};
#define FRANCE_N ((int)(sizeof(FRANCE) / sizeof(FRANCE[0])))

static const float CORSE[][2] = {
    { 9.4f, 43.0f }, { 9.55f, 42.6f }, { 9.4f, 41.4f }, { 9.1f, 41.4f }, { 8.6f, 41.9f }, { 8.7f, 42.6f },
};

/* positions des noeuds de theatre (en pixels logiques) */
static Vector2 NodePos(int t)
{
    switch (t) {
    case TH_NORD:          return Proj(3.4f, 50.0f);
    case TH_EST:           return Proj(7.1f, 48.5f);
    case TH_ALPES:         return Proj(6.5f, 45.4f);
    case TH_PYRENEES:      return Proj(0.6f, 43.1f);
    case TH_ATLANTIQUE:    return Proj(-4.0f, 46.6f);
    case TH_MEDITERRANEE:  return Proj(5.3f, 42.4f);
    case TH_OUTREMER:      return (Vector2){ 92, 488 };
    case TH_CIEL:          return Proj(2.35f, 48.3f);
    case TH_ORBITE:        return (Vector2){ 590, 110 };
    case TH_CYBER:         return (Vector2){ 590, 220 };
    case TH_INFO:          return (Vector2){ 590, 330 };
    case TH_SPECTRE:       return (Vector2){ 590, 440 };
    }
    return (Vector2){ 0, 0 };
}

static const char *NODE_LABEL[TH_COUNT] = {
    "NORD", "EST", "ALPES", "PYRÉNÉES", "ATLANTIQUE", "MÉDITERRANÉE", "OUTRE-MER",
    "CIEL", "ORBITE", "CYBER", "INFO", "SPECTRE EM",
};

static void FillPolygon(const float pts[][2], int n, Vector2 center, Color c)
{
    for (int i = 0; i < n; i++) {
        Vector2 a = Proj(pts[i][0], pts[i][1]);
        Vector2 b = Proj(pts[(i + 1) % n][0], pts[(i + 1) % n][1]);
        /* raylib attend un ordre anti-horaire : on corrige si besoin */
        float cross = (a.x - center.x) * (b.y - center.y) - (a.y - center.y) * (b.x - center.x);
        if (cross < 0) DrawTriangle(center, a, b, c);
        else           DrawTriangle(center, b, a, c);
    }
}

static void OutlinePolygon(const float pts[][2], int n, Color c)
{
    for (int i = 0; i < n; i++) {
        Vector2 a = Proj(pts[i][0], pts[i][1]);
        Vector2 b = Proj(pts[(i + 1) % n][0], pts[(i + 1) % n][1]);
        DrawLineEx(a, b, 2, c);
    }
}

static void DrawThreatMarkers(int t, Vector2 p)
{
    int k = 0;
    for (int i = 0; i < MAX_THREATS; i++) {
        const Threat *th = &game.threats[i];
        if (!th->active || th->theater != t) continue;
        float ang = -1.2f + k * 0.55f;
        Vector2 q = { p.x + cosf(ang) * 34, p.y + sinf(ang) * 34 };
        if (th->detected) {
            Color fc = FACTION_COLORS[th->faction];
            DrawPoly(q, 4, 7, 45, C_BAD);
            DrawPolyLines(q, 4, 7, 45, fc);
        } else {
            DrawCircleV(q, 7, (Color){ 120, 90, 30, 255 });
            Text("?", q.x - 3, q.y - 5, 10, C_WARN);
        }
        k++;
    }
}

static void DrawMap(void)
{
    DrawRectangle(MAP_X, MAP_Y, MAP_W, MAP_H, C_SEA);

    /* quadrillage */
    for (int x = 0; x < MAP_W; x += 40) DrawLine(x, MAP_Y, x, MAP_Y + MAP_H, (Color){ 24, 42, 62, 255 });
    for (int y = MAP_Y; y < MAP_Y + MAP_H; y += 40) DrawLine(0, y, MAP_W, y, (Color){ 24, 42, 62, 255 });

    /* pays voisins (masses schematiques) */
    DrawRectangleV(Proj(2.6f, 53.0f), (Vector2){ 300, 70 }, C_LAND);
    DrawRectangleV(Proj(8.3f, 51.4f), (Vector2){ 60, 200 }, C_LAND);
    DrawRectangleV(Proj(7.6f, 47.5f), (Vector2){ 70, 80 }, C_LAND);
    DrawTriangle(Proj(7.6f, 46.2f), Proj(7.9f, 43.6f), Proj(9.8f, 46.2f), C_LAND);
    DrawRectangleV(Proj(-2.2f, 42.9f), (Vector2){ 260, 60 }, C_LAND);
    DrawRectangleV(Proj(-5.2f, 51.4f), (Vector2){ 120, 30 }, C_LAND);
    Text("ROYAUME-UNI", Proj(-4.9f, 51.3f).x, Proj(-4.9f, 51.3f).y, 10, C_DIM);
    Text("BENELUX", Proj(4.0f, 51.2f).x, Proj(4.0f, 51.2f).y, 10, C_DIM);
    Text("ALL.", Proj(8.6f, 50.5f).x, Proj(8.6f, 50.5f).y, 10, C_DIM);
    Text("SUISSE", Proj(7.8f, 47.2f).x, Proj(7.8f, 47.2f).y, 10, C_DIM);
    Text("ITALIE", Proj(8.0f, 45.4f).x, Proj(8.0f, 45.4f).y, 10, C_DIM);
    Text("ESPAGNE", Proj(-1.5f, 42.0f).x, Proj(-1.5f, 42.0f).y, 10, C_DIM);

    /* France metropolitaine */
    Vector2 center = Proj(2.4f, 46.7f);
    FillPolygon(FRANCE, FRANCE_N, center, C_LAND_FR);
    OutlinePolygon(FRANCE, FRANCE_N, (Color){ 140, 180, 160, 255 });
    FillPolygon(CORSE, 6, Proj(9.1f, 42.2f), C_LAND_FR);
    OutlinePolygon(CORSE, 6, (Color){ 140, 180, 160, 255 });
    DrawCircleV(Proj(2.35f, 48.85f), 3, C_TEXT);
    Text("Paris", Proj(2.35f, 48.85f).x + 5, Proj(2.35f, 48.85f).y - 4, 10, C_TEXT);

    /* encart outre-mer */
    Panel((Rectangle){ 20, 440, 145, 100 }, (Color){ 16, 30, 46, 255 });
    Text("Outre-mer & ZEE", 28, 446, 10, C_DIM);
    DrawCircle(46, 515, 4, C_LAND_FR);
    DrawCircle(132, 470, 3, C_LAND_FR);
    DrawCircle(140, 520, 5, C_LAND_FR);

    /* colonne des milieux immateriels */
    Panel((Rectangle){ 540, 56, 100, 440 }, (Color){ 20, 26, 36, 230 });
    Text("CHAMPS", 568, 62, 10, C_DIM);

    /* liens orbitaux / cyber */
    for (int t = 0; t < TH_COUNT; t++) {
        if (THEATERS[t].domain == D_TERRE || THEATERS[t].domain == D_MER)
            DrawLineEx(NodePos(TH_CIEL), NodePos(t), 1, (Color){ 70, 90, 110, 120 });
    }

    /* noeuds */
    for (int t = 0; t < TH_COUNT; t++) {
        Vector2 p = NodePos(t);
        Color dc = DOMAIN_COLORS[THEATERS[t].domain];
        int nThreat = Game_ThreatCount(&game, t);
        float r = 22;

        if (nThreat > 0) {
            float pulse = 0.5f + 0.5f * sinf(timeAcc * 4 + t);
            DrawCircleV(p, r + 6 + pulse * 4, Fade(C_BAD, 0.25f + 0.2f * pulse));
        }
        if (game.tension[t] > 0.5f) DrawCircleLines((int)p.x, (int)p.y, r + 12, Fade(C_WARN, 0.8f));

        DrawCircleV(p, r, (Color){ 16, 22, 30, 255 });
        DrawCircleLines((int)p.x, (int)p.y, r, dc);
        DrawCircleLines((int)p.x, (int)p.y, r - 1, dc);
        if (t == selTheater) {
            DrawCircleLines((int)p.x, (int)p.y, r + 3, C_TEXT);
            DrawCircleLines((int)p.x, (int)p.y, r + 4, C_TEXT);
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%d", game.posture[t]);
        Text(buf, p.x - TextW(buf, 20) / 2, p.y - 10, 20, C_TEXT);
        int lw = TextW(NODE_LABEL[t], 10);
        DrawRectangle((int)(p.x - lw / 2 - 2), (int)(p.y + r + 3), lw + 4, 12, (Color){ 10, 14, 20, 200 });
        Text(NODE_LABEL[t], p.x - lw / 2, p.y + r + 4, 10, dc);

        DrawThreatMarkers(t, p);

        if (inputLayer == activeLayer && CheckCollisionPointCircle(mouse, p, r)) {
            snprintf(tooltip, sizeof(tooltip), "%s (%s)\nPosture %d - Défense %.1f - %d menace(s)\nClic : sélectionner",
                     THEATERS[t].name, DOMAIN_NAMES[THEATERS[t].domain], game.posture[t],
                     Game_DefensePower(&game, t), nThreat);
            if (clicked) {
                selTheater = t;
                tab = TAB_THEATRES;
                clicked = false;
            }
        }
    }

    /* legende */
    Text("Chiffre = posture engagée   Losange rouge = menace identifiée   ? = activité suspecte",
         12, MAP_Y + MAP_H - 14, 10, C_DIM);
}

/* ------------------------------------------------------------------ */
/* Barre superieure                                                    */
/* ------------------------------------------------------------------ */
static void DrawTopBar(void)
{
    DrawRectangle(0, 0, VW, 44, C_PANEL);
    DrawLine(0, 44, VW, 44, C_BORDER);

    char date[32], buf[128];
    Game_DateString(game.turn, date, sizeof(date));
    Text("BOUCLIER 2050", 10, 6, 20, C_ACCENT);
    snprintf(buf, sizeof(buf), "%s  -  tour %d/%d  -  +%.2f°C", date, game.turn + 1, TOTAL_TURNS, game.temp);
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
            snprintf(buf, sizeof(buf), "%+.1f/tour", Game_Income(&game) - Game_Upkeep(&game));
            Text(buf, area.x + TextW("000", 20) + 22, area.y + 20, 10, C_DIM);
        }
        char tip[256];
        snprintf(tip, sizeof(tip), "%s : %.1f\n%s", RESOURCE_NAMES[r], v, RESOURCE_DESC[r]);
        Tooltip(area, tip);
    }
}

/* ------------------------------------------------------------------ */
/* Onglet theatres                                                     */
/* ------------------------------------------------------------------ */
#define RP_X 660
#define RP_Y 88
#define RP_W 610

static void DrawTheaterTab(void)
{
    int t = selTheater;
    Domain d = THEATERS[t].domain;
    Color dc = DOMAIN_COLORS[d];
    float y = RP_Y + 8;
    char buf[160];

    Text(THEATERS[t].name, RP_X + 10, y, 20, dc);
    snprintf(buf, sizeof(buf), "Milieu : %s", DOMAIN_NAMES[d]);
    Text(buf, RP_X + RP_W - 10 - TextW(buf, 10), y + 6, 10, C_DIM);
    y += 26;
    y += TextWrap(THEATERS[t].desc, RP_X + 10, y, RP_W - 20, 10, C_DIM) + 6;

    /* posture */
    Panel((Rectangle){ RP_X + 8, y, RP_W - 16, 64 }, C_PANEL2);
    snprintf(buf, sizeof(buf), "Posture : %d", game.posture[t]);
    Text(buf, RP_X + 18, y + 8, 20, C_TEXT);
    if (Button((Rectangle){ RP_X + 180, y + 6, 28, 26 }, "-", game.posture[t] > 0, 20))
        Game_ChangePosture(&game, t, -1);
    bool canPlus = Game_PostureUsed(&game, d) < game.capacity[d];
    if (Button((Rectangle){ RP_X + 212, y + 6, 28, 26 }, "+", canPlus, 20))
        Game_ChangePosture(&game, t, 1);
    snprintf(buf, sizeof(buf), "Capacité %s : %d / %d engagés", DOMAIN_NAMES[d], Game_PostureUsed(&game, d), game.capacity[d]);
    Text(buf, RP_X + 18, y + 38, 10, C_DIM);
    snprintf(buf, sizeof(buf), "Défense : %.1f", Game_DefensePower(&game, t));
    Text(buf, RP_X + 18, y + 50, 10, C_ACCENT);

    if (game.pendingTurns[d] > 0) {
        snprintf(buf, sizeof(buf), "+%d capacité dans %d tour(s)", game.pendingCap[d], game.pendingTurns[d]);
        Text(buf, RP_X + 300, y + 14, 10, C_GOOD);
    } else {
        float cost = Game_InvestCost(&game, d);
        snprintf(buf, sizeof(buf), "Investir +2 capacité (%.0f Md)", cost);
        Rectangle br = { RP_X + 300, y + 8, 290, 24 };
        if (Button(br, buf, game.res[R_BUDGET] >= cost, 10)) Game_InvestCapacity(&game, d);
        Tooltip(br, "Recrutement, équipement et formation. Disponible après 2 tours. Chaque point de capacité coûte 0,15 Md/tour d'entretien, chaque point de posture engagé 0,6 Md/tour.");
    }
    Text("1 point de posture = 0,6 Md/tour", RP_X + 300, y + 40, 10, C_DIM);
    y += 72;

    /* menaces */
    Text("MENACES SUR CE THÉÂTRE", RP_X + 10, y, 10, C_DIM);
    y += 14;
    int shown = 0;
    for (int i = 0; i < MAX_THREATS; i++) {
        const Threat *th = &game.threats[i];
        if (!th->active || th->theater != t) continue;
        Rectangle row = { RP_X + 8, y, RP_W - 16, 34 };
        Panel(row, (Color){ 40, 28, 30, 255 });
        if (th->detected) {
            const ThreatDef *td = &THREATS[th->type];
            snprintf(buf, sizeof(buf), "%s  [force %d]", td->name, th->strength);
            Text(buf, row.x + 8, row.y + 4, 10, C_TEXT);
            snprintf(buf, sizeof(buf), "%s  -  encore %d tour(s)  -  neutralisation %d%%",
                     FACTIONS[th->faction].name, th->turnsLeft, (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 18, 10, FACTION_COLORS[th->faction]);
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
            snprintf(buf, sizeof(buf), "Neutralisation %d%% (x0,35 tant que non détectée). Orbite et renseignement aident.",
                     (int)(Game_NeutralizeChance(&game, th) * 100));
            Text(buf, row.x + 8, row.y + 18, 10, C_DIM);
        }
        y += 38;
        if (++shown >= 5) break;
    }
    if (!shown) { Text("Aucune menace détectée.", RP_X + 14, y, 10, C_GOOD); y += 16; }

    /* vue d'ensemble */
    y = 420;
    Text("VUE D'ENSEMBLE (cliquer pour sélectionner)", RP_X + 10, y, 10, C_DIM);
    y += 14;
    for (int i = 0; i < TH_COUNT; i++) {
        int col = i % 2, row = i / 2;
        Rectangle r = { RP_X + 8 + col * 300, y + row * 22, 294, 20 };
        bool h = Hover(r);
        DrawRectangleRec(r, i == selTheater ? (Color){ 44, 60, 80, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, DOMAIN_COLORS[THEATERS[i].domain]);
        Text(THEATERS[i].name, r.x + 10, r.y + 5, 10, C_TEXT);
        int n = Game_ThreatCount(&game, i);
        snprintf(buf, sizeof(buf), "P%d  D%.0f", game.posture[i], Game_DefensePower(&game, i));
        Text(buf, r.x + 170, r.y + 5, 10, C_DIM);
        if (n) {
            snprintf(buf, sizeof(buf), "%d menace%s", n, n > 1 ? "s" : "");
            Text(buf, r.x + 226, r.y + 5, 10, C_BAD);
        }
        if (h && clicked) { selTheater = i; clicked = false; }
    }
}

/* ------------------------------------------------------------------ */
/* Onglet factions                                                     */
/* ------------------------------------------------------------------ */
static void DrawFactionTab(void)
{
    char buf[160];
    float y = RP_Y + 6;
    snprintf(buf, sizeof(buf), "Points de commandement : %d / %d   (une action par faction et par tour)", game.commandPoints, BASE_CP);
    Text(buf, RP_X + 10, y, 10, C_ACCENT);
    y += 16;

    for (int f = 0; f < F_COUNT; f++) {
        Rectangle card = { RP_X + 8, y, RP_W - 16, 118 };
        Panel(card, C_PANEL2);
        DrawRectangle((int)card.x, (int)card.y, 4, (int)card.height, FACTION_COLORS[f]);
        Text(FACTIONS[f].name, card.x + 12, card.y + 6, 20, FACTION_COLORS[f]);
        if (game.fac[f].intelTurns > 0) {
            snprintf(buf, sizeof(buf), "Renseignement actif : %d tour(s)", game.fac[f].intelTurns);
            Text(buf, card.x + card.width - 10 - TextW(buf, 10), card.y + 10, 10, C_GOOD);
        }
        TextWrap(FACTIONS[f].desc, card.x + 12, card.y + 28, card.width - 24, 10, C_DIM);

        Text("Hostilité", card.x + 12, card.y + 58, 10, C_DIM);
        Bar((Rectangle){ card.x + 80, card.y + 58, 180, 9 }, game.fac[f].hostility, 100, C_BAD);
        snprintf(buf, sizeof(buf), "%.0f", game.fac[f].hostility);
        Text(buf, card.x + 266, card.y + 58, 10, C_TEXT);
        Text("Capacité", card.x + 300, card.y + 58, 10, C_DIM);
        Bar((Rectangle){ card.x + 360, card.y + 58, 180, 9 }, game.fac[f].capability, 100, C_WARN);
        snprintf(buf, sizeof(buf), "%.0f", game.fac[f].capability);
        Text(buf, card.x + 546, card.y + 58, 10, C_TEXT);

        int active = 0;
        for (int i = 0; i < MAX_THREATS; i++) if (game.threats[i].active && game.threats[i].faction == f) active++;
        float rate = (0.2f + 1.5f * game.fac[f].hostility / 100.0f * game.fac[f].capability / 100.0f)
                   * (1.0f + 0.6f * game.turn / (float)TOTAL_TURNS);
        snprintf(buf, sizeof(buf), "Opérations en cours : %d   -   menaces attendues/tour : %.1f", active, rate);
        Text(buf, card.x + 12, card.y + 72, 10, C_TEXT);

        float bw = (card.width - 24 - 4 * 6) / A_COUNT;
        for (int a = 0; a < A_COUNT; a++) {
            Rectangle br = { card.x + 12 + a * (bw + 6), card.y + 88, bw, 24 };
            const char *blocked = Game_ActionBlocked(&game, f, a);
            if (Button(br, ACTIONS[a].name, blocked == NULL, 10)) Game_FactionAction(&game, f, a);
            char tip[300], cost[80] = "";
            if (ACTIONS[a].budget > 0) snprintf(cost, sizeof(cost), "Coût : 1 PC, %.0f Md", ACTIONS[a].budget);
            else if (ACTIONS[a].influence > 0) snprintf(cost, sizeof(cost), "Coût : 1 PC, %.0f Influence", ACTIONS[a].influence);
            else snprintf(cost, sizeof(cost), "Coût : 1 PC");
            snprintf(tip, sizeof(tip), "%s\n%s\n%s%s", ACTIONS[a].name, ACTIONS[a].desc, cost,
                     blocked ? (blocked[0] ? "\nIndisponible : " : "") : "");
            if (blocked) strncat(tip, blocked, sizeof(tip) - strlen(tip) - 1);
            Tooltip(br, tip);
        }
        y += 124;
    }
}

/* ------------------------------------------------------------------ */
/* Onglet resilience                                                   */
/* ------------------------------------------------------------------ */
static void DrawResilienceTab(void)
{
    char buf[160];
    float y = RP_Y + 6;
    Text("Projets civils de résilience : investissements uniques, effets permanents.", RP_X + 10, y, 10, C_DIM);
    y += 16;
    for (int p = 0; p < P_COUNT; p++) {
        Rectangle row = { RP_X + 8, y, RP_W - 16, 54 };
        const Project *pr = &game.proj[p];
        Panel(row, pr->state == 2 ? (Color){ 26, 48, 36, 255 } : C_PANEL2);
        Text(PROJECTS[p].name, row.x + 10, row.y + 6, 20, pr->state == 2 ? C_GOOD : C_TEXT);
        Text(PROJECTS[p].desc, row.x + 10, row.y + 32, 10, C_DIM);
        Rectangle br = { row.x + row.width - 170, row.y + 8, 160, 24 };
        if (pr->state == 0) {
            snprintf(buf, sizeof(buf), "Lancer : %.0f Md / %d t", PROJECTS[p].cost, PROJECTS[p].turns);
            if (Button(br, buf, game.res[R_BUDGET] >= PROJECTS[p].cost, 10)) Game_StartProject(&game, p);
        } else if (pr->state == 1) {
            snprintf(buf, sizeof(buf), "Chantier : %d tour(s)", pr->turnsLeft);
            Text(buf, br.x + 10, br.y + 6, 10, C_WARN);
            Bar((Rectangle){ br.x + 10, br.y + 20, 140, 5 },
                (float)(PROJECTS[p].turns - pr->turnsLeft), (float)PROJECTS[p].turns, C_WARN);
        } else {
            Text("OPÉRATIONNEL", br.x + 30, br.y + 6, 10, C_GOOD);
        }
        y += 58;
    }
}

/* ------------------------------------------------------------------ */
/* Onglet doctrine                                                     */
/* ------------------------------------------------------------------ */
static void DrawDoctrineTab(void)
{
    char buf[160];
    float y = RP_Y + 6;
    Text("DOCTRINE MIGRATOIRE", RP_X + 10, y, 20, C_ACCENT);
    y += 24;
    Text("Le climat pousse des millions de personnes sur les routes ; des acteurs hostiles les instrumentalisent.",
         RP_X + 10, y, 10, C_DIM);
    y += 16;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { RP_X + 8, y, RP_W - 16, 50 };
        bool sel = game.migrationDoctrine == i;
        bool h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(MIGRATION_DOCTRINES[i], r.x + 10, r.y + 5, 10, sel ? C_TEXT : C_DIM);
        TextWrap(MIGRATION_DESC[i], r.x + 10, r.y + 19, r.width - 20, 10, C_DIM);
        if (h && clicked) { Game_SetMigrationDoctrine(&game, i); clicked = false; }
        y += 54;
    }
    Text("Changer de doctrine coûte 2 points de Cohésion.", RP_X + 10, y, 10, C_DIM);
    y += 22;

    Text("NIVEAU D'ALERTE NATIONAL", RP_X + 10, y, 20, C_ACCENT);
    y += 26;
    for (int i = 0; i < 3; i++) {
        Rectangle r = { RP_X + 8, y, RP_W - 16, 34 };
        bool sel = game.alertLevel == i;
        bool h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(ALERT_LEVELS[i], r.x + 10, r.y + 5, 10, sel ? C_TEXT : C_DIM);
        Text(ALERT_DESC[i], r.x + 10, r.y + 19, 10, C_DIM);
        if (h && clicked) { Game_SetAlertLevel(&game, i); clicked = false; }
        y += 38;
    }
    y += 10;

    Text("PRÉVISIONS BUDGÉTAIRES DU TRIMESTRE", RP_X + 10, y, 20, C_ACCENT);
    y += 26;
    snprintf(buf, sizeof(buf), "Recettes : +%.1f Md   (dépendent de l'Énergie et de la Cohésion)", Game_Income(&game));
    Text(buf, RP_X + 14, y, 10, C_GOOD);
    y += 14;
    snprintf(buf, sizeof(buf), "Dépenses : -%.1f Md   (postures, capacités, alerte, doctrine)", Game_Upkeep(&game));
    Text(buf, RP_X + 14, y, 10, C_BAD);
    y += 14;
    float net = Game_Income(&game) - Game_Upkeep(&game);
    snprintf(buf, sizeof(buf), "Solde : %+.1f Md / tour%s", net, game.res[R_BUDGET] < 0 ? "   - dette : intérêts 3%/tour, -1 Cohésion/tour" : "");
    Text(buf, RP_X + 14, y, 10, net >= 0 ? C_TEXT : C_WARN);
    y += 20;
    snprintf(buf, sizeof(buf), "Climat : +%.2f°C. Catastrophes : %d. Menaces neutralisées : %d, subies : %d.",
             game.temp, game.statDisasters, game.statNeutralized, game.statSuffered);
    Text(buf, RP_X + 14, y, 10, C_DIM);
}

/* ------------------------------------------------------------------ */
/* Journal                                                             */
/* ------------------------------------------------------------------ */
static void DrawLog(void)
{
    Rectangle r = { 0, MAP_Y + MAP_H, MAP_W, VH - MAP_Y - MAP_H };
    Panel(r, C_PANEL);
    Text("JOURNAL DE SITUATION (molette pour défiler)", 10, r.y + 6, 10, C_DIM);

    int lines = 10;
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
        default:          c = C_TEXT; break;
        }
        if (e->turn < game.turn - 1) c = Fade(c, 0.55f);
        Text(e->text, 12, r.y + 22 + i * 14, 10, c);
    }
}

/* ------------------------------------------------------------------ */
/* Modales                                                             */
/* ------------------------------------------------------------------ */
static void DrawDilemma(void)
{
    const DilemmaDef *d = &DILEMMAS[game.pendingDilemma];
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
    Rectangle r = { 290, 170, 700, 380 };
    Panel(r, C_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 4, C_EVENT);
    Text("DÉCISION DU CONSEIL DE DÉFENSE", r.x + 20, r.y + 16, 10, C_EVENT);
    Text(d->title, r.x + 20, r.y + 32, 20, C_TEXT);
    TextWrap(d->text, r.x + 20, r.y + 66, r.width - 40, 10, C_DIM);

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
    /* decoupe en lignes sur '\n' */
    char copy[512];
    snprintf(copy, sizeof(copy), "%s", tooltip);
    const char *lines[8];
    int n = 0, w = 0;
    char *s = copy;
    while (s && n < 8) {
        lines[n++] = s;
        char *nl = strchr(s, '\n');
        if (nl) { *nl = 0; s = nl + 1; } else s = NULL;
    }
    for (int i = 0; i < n; i++) { int lw = TextW(lines[i], 10); if (lw > w) w = lw; }
    if (w > 460) w = 460;
    float x = mouse.x + 16, y = mouse.y + 16;
    int h = 0;
    for (int i = 0; i < n; i++) {
        int lw = TextW(lines[i], 10);
        h += (lw > 460 ? (lw / 460 + 1) : 1) * 14;
    }
    if (x + w + 20 > VW) x = VW - w - 20;
    if (y + h + 12 > VH) y = mouse.y - h - 16;
    Panel((Rectangle){ x, y, (float)w + 16, (float)h + 10 }, (Color){ 12, 16, 22, 245 });
    float yy = y + 6;
    for (int i = 0; i < n; i++)
        yy += TextWrap(lines[i], x + 8, yy, 460, 10, i == 0 ? C_ACCENT : C_TEXT);
}

/* ------------------------------------------------------------------ */
/* Ecrans                                                              */
/* ------------------------------------------------------------------ */
static void NewGame(void)
{
    Game_Init(&game, (unsigned)time(NULL));
    selTheater = TH_NORD;
    tab = TAB_THEATRES;
    logScroll = 0;
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
    Text("BOUCLIER 2050", VW / 2 - TextW("BOUCLIER 2050", 60) / 2, 120, 60, C_ACCENT);
    const char *sub = "Wargame de guerre hybride multi-milieux, multi-champs";
    Text(sub, VW / 2 - TextW(sub, 20) / 2, 196, 20, C_TEXT);

    const char *pitch =
        "2050. Le monde a dépassé +2°C. Sécheresses, mégafeux et effondrement agricole jettent des millions "
        "de personnes sur les routes. L'eau, l'énergie et la nourriture sont devenues des armes. "
        "Puissances, cartels et mercenaires numériques mènent contre la France une guerre sans déclaration : "
        "sabotages, drones, cyberattaques, désinformation, brouillage, pression migratoire. "
        "Vous dirigez le Bouclier national. Tenez dix ans.";
    TextWrap(pitch, 290, 250, 700, 10, C_DIM);

    float y = 350;
    if (inProgress) {
        if (Button((Rectangle){ VW / 2 - 140, y, 280, 44 }, "CONTINUER", true, 20)) screen = SCR_GAME;
        y += 54;
    }
    if (Button((Rectangle){ VW / 2 - 140, y, 280, 44 }, "NOUVELLE PARTIE", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, y + 54, 280, 44 }, "COMMENT JOUER", true, 20)) screen = SCR_HELP;
    if (Button((Rectangle){ VW / 2 - 140, y + 108, 280, 44 }, "QUITTER", true, 20)) quitRequested = true;

    const char *dom = "Terre - Mer - Air - Espace - Cyber - Informationnel - Electromagnétique";
    Text(dom, VW / 2 - TextW(dom, 10) / 2, 580, 10, C_DIM);
    Text("Fiction prospective. Factions imaginaires.", 10, VH - 18, 10, C_DIM);
}

static void DrawHelp(void)
{
    DrawBackdrop();
    Panel((Rectangle){ 140, 40, 1000, 620 }, C_PANEL);
    Text("COMMENT JOUER", 160, 56, 20, C_ACCENT);
    static const char *help[] = {
        "OBJECTIF : tenir 40 trimestres (2050-2060) sans que la Cohésion ou la Souveraineté ne tombent à 0, ni le Budget sous -60.",
        "TOUR DE JEU : faites vos choix puis cliquez FIN DU TOUR (ou Espace). Les menaces sont alors résolues, le climat évolue, de nouvelles menaces apparaissent.",
        "CARTE : chaque cercle est un théâtre d'opérations dans un milieu (terre, mer, air, espace) ou un champ (cyber, informationnel, électromagnétique). Le chiffre est la posture engagée.",
        "POSTURE : répartissez la capacité de chaque milieu entre ses théâtres (+/-). Plus la posture est élevée, plus les menaces sont neutralisées vite. Chaque point engagé coûte 0,6 Md/tour.",
        "MENACES : un '?' est une activité suspecte non identifiée (neutralisation très difficile). L'Orbite, la constellation souveraine et le renseignement les révèlent.",
        "SYNERGIES M2MC : la posture Orbite améliore la détection partout ; un spectre électromagnétique brouillé dégrade la défense aérienne et navale.",
        "FACTIONS : 3 points de commandement par tour pour la diplomatie, les sanctions, la cyber-offensive, la contre-influence ou le renseignement. Leur hostilité x capacité détermine le nombre de menaces.",
        "RÉSILIENCE : le réchauffement érode chaque tour Énergie, Eau et Alimentation. Seuls les grands projets civils compensent durablement. Une pénurie (<35) mine la cohésion.",
        "DOCTRINE : choisissez votre politique migratoire et le niveau d'alerte national. Chaque option a un prix politique, budgétaire ou diplomatique.",
        "DILEMMES : le Conseil de défense vous soumettra des décisions difficiles. Il n'y a pas de bon choix, seulement des conséquences.",
        "RACCOURCIS : Espace/Entrée = fin du tour, 1-4 = onglets, F11 = plein écran, Échap = menu.",
    };
    float y = 92;
    for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
        y += TextWrap(help[i], 160, y, 960, 10, i % 2 ? C_TEXT : (Color){ 200, 210, 220, 255 }) + 10;
    if (Button((Rectangle){ VW / 2 - 100, 600, 200, 40 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE))
        screen = SCR_TITLE;
}

static void DrawEnd(void)
{
    DrawBackdrop();
    bool won = game.state == GAME_WON;
    const char *title = won ? "LA FRANCE A TENU" : "DÉFAITE";
    Text(title, VW / 2 - TextW(title, 60) / 2, 110, 60, won ? C_GOOD : C_BAD);
    const char *why = "";
    switch (game.state) {
    case GAME_WON: why = "2060 : malgré les tempêtes, la République est debout. L'histoire retiendra votre sang-froid."; break;
    case GAME_LOST_COHESION: why = "Effondrement social : émeutes, sécessions locales, le pays se fracture."; break;
    case GAME_LOST_SOUVERAINETE: why = "Perte de souveraineté : la France ne décide plus de son destin."; break;
    case GAME_LOST_BUDGET: why = "Défaut de paiement : les créanciers dictent désormais la politique nationale."; break;
    default: break;
    }
    Text(why, VW / 2 - TextW(why, 10) / 2, 190, 10, C_TEXT);

    char buf[200], date[32];
    Game_DateString(game.turn, date, sizeof(date));
    Panel((Rectangle){ 390, 230, 500, 230 }, C_PANEL);
    float y = 246;
    snprintf(buf, sizeof(buf), "Fin de partie : %s (+%.2f°C)", date, game.temp); Text(buf, 410, y, 10, C_TEXT); y += 18;
    snprintf(buf, sizeof(buf), "Menaces neutralisées : %d", game.statNeutralized); Text(buf, 410, y, 10, C_GOOD); y += 18;
    snprintf(buf, sizeof(buf), "Menaces subies : %d", game.statSuffered); Text(buf, 410, y, 10, C_BAD); y += 18;
    snprintf(buf, sizeof(buf), "Catastrophes climatiques : %d", game.statDisasters); Text(buf, 410, y, 10, C_CLIMATE); y += 24;
    for (int r = 0; r < R_COUNT; r++) {
        snprintf(buf, sizeof(buf), "%s : %.0f", RESOURCE_NAMES[r], game.res[r]);
        Text(buf, 410 + (r % 2) * 240, y, 10, C_DIM);
        if (r % 2) y += 16;
    }
    y += 26;
    snprintf(buf, sizeof(buf), "SCORE : %d", Game_Score(&game));
    Text(buf, 640 - TextW(buf, 20) / 2, y, 20, C_ACCENT);

    if (Button((Rectangle){ VW / 2 - 140, 490, 280, 44 }, "REJOUER", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, 544, 280, 44 }, "MENU", true, 20)) screen = SCR_TITLE;
}

static void DrawGame(void)
{
    ClearBackground(C_BG);
    activeLayer = game.pendingDilemma >= 0 ? 1 : 0;
    inputLayer = 0;

    DrawMap();
    DrawLog();
    DrawTopBar();

    /* panneau droit */
    Panel((Rectangle){ RP_X - 4, 48, RP_W + 12, VH - 52 }, C_PANEL);
    static const char *tabNames[TAB_COUNT] = { "THÉÂTRES", "FACTIONS", "RÉSILIENCE", "DOCTRINE" };
    float tw = (RP_W - 6) / (float)TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        Rectangle r = { RP_X + i * (tw + 2), 52, tw, 30 };
        bool sel = tab == (Tab)i, h = Hover(r);
        DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 255 } : h ? C_PANEL2 : C_BG);
        DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
        Text(tabNames[i], r.x + (r.width - TextW(tabNames[i], 10)) / 2, r.y + 10, 10, sel ? C_TEXT : C_DIM);
        if (h && clicked) { tab = (Tab)i; clicked = false; }
    }
    switch (tab) {
    case TAB_THEATRES:   DrawTheaterTab(); break;
    case TAB_FACTIONS:   DrawFactionTab(); break;
    case TAB_RESILIENCE: DrawResilienceTab(); break;
    case TAB_DOCTRINE:   DrawDoctrineTab(); break;
    default: break;
    }

    /* fin de tour */
    Rectangle eb = { RP_X + RP_W - 220, VH - 50, 220, 40 };
    char lbl[64];
    snprintf(lbl, sizeof(lbl), "FIN DU TOUR  >>");
    bool endTurn = Button(eb, lbl, game.state == GAME_RUNNING, 20);
    Tooltip(eb, "Résoudre le trimestre (Espace ou Entrée)");
    snprintf(lbl, sizeof(lbl), "PC restants : %d", game.commandPoints);
    Text(lbl, RP_X + 10, VH - 36, 10, C_ACCENT);
    if (activeLayer == 0 && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) endTurn = true;
    if (endTurn) {
        Game_EndTurn(&game);
        logScroll = 0;
    }

    if (activeLayer == 0) {
        if (IsKeyPressed(KEY_ONE))   tab = TAB_THEATRES;
        if (IsKeyPressed(KEY_TWO))   tab = TAB_FACTIONS;
        if (IsKeyPressed(KEY_THREE)) tab = TAB_RESILIENCE;
        if (IsKeyPressed(KEY_FOUR))  tab = TAB_DOCTRINE;
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
    /* echelle entiere : pixels nets ; sinon lissage bilineaire */
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

    /* fenetre adaptee aux petits ecrans */
    int mon = GetCurrentMonitor();
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    if (mw > 0 && mh > 0 && (mw < VW + 40 || mh < VH + 80)) {
        float s = fminf((mw - 40) / (float)VW, (mh - 80) / (float)VH);
        SetWindowSize((int)(VW * s), (int)(VH * s));
    }

    target = LoadRenderTexture(VW, VH);
    viewScale = 0;
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
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
