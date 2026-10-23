/*
 * BOUCLIER 2050 - carte zoomable, selection et ordres des divisions, barre superieure.
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Selection des divisions                                             */
/* ------------------------------------------------------------------ */
void SelectUnitsIn(int p, bool add)
{
    if (!add) memset(uiSel, 0, sizeof(uiSel));
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &game.units[i];
        if (u->active && u->owner == Me() && u->prov == p && !u->training) uiSel[i] = true;
    }
}

int SelectedCount(void)
{
    int c = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (uiSel[i] && game.units[i].active && game.units[i].owner == Me()) c++;
    return c;
}

void OrderSelected(int p)
{
    int ok = 0, tried = 0;
    const char *why = NULL;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!uiSel[i] || !game.units[i].active || game.units[i].owner != Me()) continue;
        tried++;
        const char *b = Cmdr_OrderBlocked(&game, Me(), i, p);
        if (b) { why = b; continue; }
        Do(CMD_ORDER, i, p, 0, 0);
        ok++;
    }
    char buf[160];
    if (!tried) Toast(Me() == N_FRANCE ? "Aucune division sélectionnée (clic gauche sur une province française)."
                                       : "Aucune division sélectionnée (clic gauche sur une de vos provinces).");
    else if (!ok) { snprintf(buf, sizeof(buf), Tr("Ordre impossible : %s."), why); Toast(buf); }
    else {
        bool enemy = !War_Friendly(&game, Me(), p);
        snprintf(buf, sizeof(buf), Tr("%d unité(s) %s %s."), ok, enemy ? "lancée(s) à l'assaut de" : "en route vers", Tr(PROVINCE_NAMES[p]));
        Toast(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Carte zoomable                                                      */
/* ------------------------------------------------------------------ */

const float ZOOMS[] = { 1, 1.5f, 2, 3, 4, 6 };

Texture2D mapTex;
Color     mapPixels[MAP_VW * MAP_VH];
unsigned  mapSignature = 1;
float     nationLabel[N_COUNT][3];
Color     provColor[PROV_COUNT];
short     colOf[MAP_VW + 1], rowOf[MAP_VH + 1];
unsigned char seaDist[MAP_H][MAP_W];     /* distance a la mer (cellules) */
unsigned short floodHist[PROV_COUNT][64];   /* cellules par distance a la mer */

/* distances a la mer, calculees une fois : les cellules les plus proches sont submergees d'abord */
void ComputeSeaDistance(void)
{
    static short qx[MAP_W * MAP_H], qy[MAP_W * MAP_H];
    int head = 0, tail = 0;
    for (int r = 0; r < MAP_H; r++)
        for (int c = 0; c < MAP_W; c++) {
            seaDist[r][c] = 255;
            if (MAP_CELLS[r][c] == MAP_SEA) { seaDist[r][c] = 0; qx[tail] = (short)c; qy[tail] = (short)r; tail++; }
        }
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int c = qx[head], r = qy[head];
        head++;
        for (int k = 0; k < 4; k++) {
            int nc = c + dx[k], nr = r + dy[k];
            if (nc < 0 || nr < 0 || nc >= MAP_W || nr >= MAP_H || seaDist[nr][nc] != 255) continue;
            if (MAP_CELLS[nr][nc] >= PROV_COUNT) continue;
            seaDist[nr][nc] = (unsigned char)(seaDist[r][c] < 254 ? seaDist[r][c] + 1 : 254);
            qx[tail] = (short)nc; qy[tail] = (short)nr; tail++;
        }
    }
    memset(floodHist, 0, sizeof(floodHist));
    for (int r = 0; r < MAP_H; r++)
        for (int c = 0; c < MAP_W; c++) {
            int v = MAP_CELLS[r][c];
            if (v < PROV_COUNT) floodHist[v][seaDist[r][c] < 63 ? seaDist[r][c] : 63]++;
        }
}

/* distance a la mer en deca de laquelle les terres de p sont submergees (-1 : aucune) */
int FloodThreshold(int p)
{
    float f = game.prov[p].flooded;
    if (f < 0.005f) return -1;
    int want = (int)(f * PROVINCE_CELLS[p]), cum = 0;
    for (int d = 1; d < 64; d++) {
        cum += floodHist[p][d];
        if (cum >= want) return d;
    }
    return 63;
}

float CellW(void) { return CW0 * zoom; }
float CellH(void) { return CH0 * zoom; }

void ClampCamera(void)
{
    float vw = MAP_W / zoom, vh = MAP_H / zoom;
    camX = Clampf2(camX, 0, MAP_W - vw);
    camY = Clampf2(camY, 0, MAP_H - vh);
}

Vector2 CellToScreen(float col, float row)
{
    return (Vector2){ MAP_X + (col - camX) * CellW(), MAP_Y + (row - camY) * CellH() };
}

bool InMap(Vector2 m)
{
    return m.x >= MAP_X && m.y >= MAP_Y && m.x < MAP_X + MAP_VW && m.y < MAP_Y + MAP_VH;
}

int ProvinceAt(Vector2 m)
{
    if (!InMap(m)) return -1;
    int col = (int)(camX + (m.x - MAP_X) / CellW()), row = (int)(camY + (m.y - MAP_Y) / CellH());
    if (col < 0 || row < 0 || col >= MAP_W || row >= MAP_H) return -1;
    int v = MAP_CELLS[row][col];
    return v < PROV_COUNT ? v : -1;
}

void SetZoom(int idx, Vector2 anchor)
{
    if (idx < 0) idx = 0;
    if (idx >= ZOOM_COUNT) idx = ZOOM_COUNT - 1;
    float col = camX + (anchor.x - MAP_X) / CellW(), row = camY + (anchor.y - MAP_Y) / CellH();
    zoom = ZOOMS[idx];
    camX = col - (anchor.x - MAP_X) / CellW();
    camY = row - (anchor.y - MAP_Y) / CellH();
    ClampCamera();
}

int ZoomIndex(void)
{
    for (int i = 0; i < ZOOM_COUNT; i++) if (ZOOMS[i] >= zoom - 0.01f) return i;
    return ZOOM_COUNT - 1;
}

/* centre la vue sur une province */
void FocusProvince(int p, int zoomIdx)
{
    zoom = ZOOMS[zoomIdx];
    camX = PROVINCE_LABEL[p][0] - MAP_W / zoom / 2;
    camY = PROVINCE_LABEL[p][1] - MAP_H / zoom / 2;
    ClampCamera();
}

/* couleurs des cultures sans Etat */
Color CultureColor(int c)
{
    int h = CULTURE_HOMELAND[c];
    if (h >= 0) return NationColor(h);
    switch (c) {
    case C_CORSE:     return (Color){ 120, 160, 230, 255 };
    case C_ECOSSAISE: return (Color){ 150,  90, 160, 255 };
    case C_CATALANE:  return (Color){ 240, 220,  80, 255 };
    case C_BASQUE:    return (Color){ 200,  50,  50, 255 };
    case C_KURDE:     return (Color){ 230, 140,  40, 255 };
    case C_AMAZIGHE:  return (Color){  90, 200, 210, 255 };
    default:          return (Color){ 160, 160, 160, 255 };
    }
}

Color ProvinceBaseColor(int p)
{
    const Province *pr = &game.prov[p];
    switch (mapMode) {
    case MAP_CLIMAT:
        return Lerp3((Color){ 170, 50, 40, 255 }, (Color){ 220, 190, 70, 255 }, (Color){ 70, 160, 90, 255 }, pr->hab / 100.0f);
    case MAP_MILITANCE:
        return Lerp3((Color){ 60, 150, 90, 255 }, (Color){ 220, 190, 70, 255 }, (Color){ 200, 50, 40, 255 },
                     Soc_ProvinceMilitancy(pr) / 8.0f);
    case MAP_POPULATION: {
        float dens = pr->pop / (PROVINCE_CELLS[p] > 0 ? PROVINCE_CELLS[p] : 1);
        float t = log10f(1 + dens * 400) / 2.6f;
        return Lerp3((Color){ 30, 50, 80, 255 }, (Color){ 80, 140, 210, 255 }, (Color){ 230, 240, 255, 255 }, t);
    }
    case MAP_CULTURES: {
        int c = Soc_MainCulture(pr);
        Color col = CultureColor(c);
        return Shade(col, 0.55f + 0.45f * pr->culture[c]);
    }
    case MAP_RELATIONS: {
        int o = pr->owner;
        if (o == N_FRANCE) return (Color){ 70, 120, 220, 255 };
        if (War_AtWar(&game, N_FRANCE, o)) return (Color){ 150, 30, 30, 255 };
        if (game.ally[N_FRANCE][o]) return (Color){ 60, 190, 200, 255 };
        return Lerp3((Color){ 200, 60, 50, 255 }, (Color){ 210, 200, 90, 255 }, (Color){ 70, 170, 90, 255 },
                     (game.nat[o].relation + 100) / 200.0f);
    }
    case MAP_LOGISTIQUE: {
        if (!War_Friendly(&game, N_FRANCE, p)) return (Color){ 70, 74, 82, 255 };
        float s = War_SupplyLevel(&game, N_FRANCE, p);
        return Lerp3((Color){ 190, 40, 40, 255 }, (Color){ 220, 190, 70, 255 }, (Color){ 60, 170, 90, 255 }, s);
    }
    case MAP_RESSOURCES: {
        static const Color rc[SR_COUNT] = { { 200, 120, 220, 255 }, { 120, 220, 90, 255 }, { 90, 170, 240, 255 } };
        int best = -1;
        float amount = 0;
        for (int r = 0; r < SR_COUNT; r++) {
            float d = Strat_Deposit(p, r) + (r == SR_PUCES ? pr->fabs : 0);
            if (d > amount) { amount = d; best = r; }
        }
        if (best < 0) return (Color){ 60, 64, 72, 255 };
        return Shade(rc[best], 0.6f + 0.3f * fminf(1, amount));
    }
    case MAP_COMMERCE: {
        int o = pr->owner;
        if (o == N_FRANCE) return (Color){ 70, 120, 220, 255 };
        if (War_AtWar(&game, N_FRANCE, o)) return (Color){ 120, 30, 30, 255 };
        if (game.nat[o].embargoFrance) return (Color){ 130, 50, 120, 255 };
        if (game.routeKind[o] == 0) return (Color){ 70, 74, 82, 255 };
        Color top = game.routeKind[o] == 1 ? (Color){ 70, 170, 90, 255 } : (Color){ 60, 170, 220, 255 };
        return Lerp3((Color){ 190, 40, 40, 255 }, (Color){ 220, 190, 70, 255 }, top, game.route[o]);
    }
    case MAP_DEGATS: {
        if (pr->damage < 0.02f) {   /* intact : gris teinte de la couleur du proprietaire */
            Color n = NationColor(pr->owner);
            return (Color){ (unsigned char)(50 + n.r / 6), (unsigned char)(54 + n.g / 6), (unsigned char)(62 + n.b / 6), 255 };
        }
        return Lerp3((Color){ 220, 200, 80, 255 }, (Color){ 230, 130, 40, 255 }, (Color){ 200, 30, 30, 255 }, pr->damage);
    }
    default: {
        Color c = NationColor(pr->owner);
        return pr->owner != N_FRANCE ? Shade(c, 0.8f) : c;
    }
    }
}

/* motifs : occupation (rayures a la couleur de l'occupant), revolte, contamination, minorite */
Color PatternColor(int p, Color c, int px, int py)
{
    const Province *pr = &game.prov[p];
    if ((mapMode == MAP_POLITIQUE || mapMode == MAP_LOGISTIQUE || mapMode == MAP_RELATIONS) && !War_Visible(&game, p) && (px + py) % 3 == 0)
        return Shade(c, 0.55f);   /* brouillard de guerre */
    switch (mapMode) {
    case MAP_POLITIQUE:
        if (pr->ctrl != pr->owner && ((px + py) / 3) % 2 == 0) return Shade(NationColor(pr->ctrl), 0.9f);
        if (pr->core != pr->owner && pr->ctrl == pr->owner && (px + py) % 7 == 0) return Shade(NationColor(pr->core), 0.7f);
        return c;
    case MAP_CLIMAT:
    case MAP_DEGATS:
        if (pr->fallout > 0.2f && (px + py) % 3 == 0) return (Color){ 120, 220, 60, 255 };
        if (mapMode == MAP_DEGATS && pr->ctrl != pr->owner && ((px + py) / 3) % 2 == 0) return Shade(NationColor(pr->ctrl), 0.7f);
        return c;
    case MAP_COMMERCE:
        /* routes maritimes : points clairs ; nations sous blocus : rayures sombres */
        if (pr->owner != N_FRANCE && game.routeKind[pr->owner] == 2 && px % 4 == 0 && py % 4 == 0) return (Color){ 220, 240, 250, 255 };
        if (game.nat[pr->owner].blockade > 0.2f && ((px - py) / 2) % 4 == 0) return Shade(c, 0.5f);
        return c;
    case MAP_MILITANCE:
        if (pr->revolt && ((px - py) / 3) % 2 == 0) return (Color){ 150, 40, 160, 255 };
        return c;
    case MAP_CULTURES: {
        /* deuxieme culture en pointille si elle depasse 15 % */
        int main = Soc_MainCulture(pr), second = -1;
        for (int k = 0; k < CULT_COUNT; k++)
            if (k != main && pr->culture[k] > 0.15f && (second < 0 || pr->culture[k] > pr->culture[second])) second = k;
        if (second >= 0 && (px % 4 == 0) && (py % 4 == 0)) return CultureColor(second);
        return c;
    }
    default:
        return c;
    }
}

unsigned ComputeSignature(void)
{
    unsigned h = 2166136261u;
    h = (h ^ (unsigned)mapMode) * 16777619u;
    h = (h ^ (unsigned)(selProvince + 1)) * 16777619u;
    h = (h ^ (unsigned)game.turn) * 16777619u;
    h = (h ^ (unsigned)(zoom * 100)) * 16777619u;
    h = (h ^ (unsigned)(camX * 64)) * 16777619u;
    h = (h ^ (unsigned)(camY * 64)) * 16777619u;
    for (int p = 0; p < PROV_COUNT; p++) {
        h = (h ^ (unsigned)(game.prov[p].owner * 31 + game.prov[p].ctrl * 7 + (game.prov[p].revolt ? 1 : 0))) * 16777619u;
        h = (h ^ (unsigned)(game.prov[p].pop * 10)) * 16777619u;
    }
    return h;
}

void RebuildMap(void)
{
    static bool seaReady;
    if (!seaReady) { ComputeSeaDistance(); seaReady = true; }
    int floodThr[PROV_COUNT];
    for (int p = 0; p < PROV_COUNT; p++) {
        provColor[p] = ProvinceBaseColor(p);
        if (p == selProvince) provColor[p] = Shade(provColor[p], 1.3f);
        floodThr[p] = FloodThreshold(p);
    }
    for (int x = 0; x <= MAP_VW; x++) {
        int c = (int)(camX + x / CellW());
        colOf[x] = (short)(c < MAP_W ? c : MAP_W - 1);
    }
    for (int y = 0; y <= MAP_VH; y++) {
        int r = (int)(camY + y / CellH());
        rowOf[y] = (short)(r < MAP_H ? r : MAP_H - 1);
    }
    for (int py = 0; py < MAP_VH; py++) {
        int row = rowOf[py], rowB = rowOf[py + 1];
        for (int px = 0; px < MAP_VW; px++) {
            int col = colOf[px], colR = colOf[px + 1];
            int v = MAP_CELLS[row][col];
            Color c;
            if (v == MAP_SEA) c = C_SEA;
            else if (v == MAP_OUT) c = C_OUT;
            else {
                c = PatternColor(v, provColor[v], px, py);
                if (floodThr[v] >= 0 && seaDist[row][col] <= floodThr[v])
                    c = (px + py) % 5 == 0 ? (Color){ 70, 120, 160, 255 } : (Color){ 34, 64, 98, 255 };   /* terres submergees */
                int r = colR != col ? MAP_CELLS[row][colR] : v;
                int b = rowB != row ? MAP_CELLS[rowB][col] : v;
                int own = game.prov[v].owner;
                bool natEdge = (r != v && (r >= PROV_COUNT || game.prov[r].owner != own))
                            || (b != v && (b >= PROV_COUNT || game.prov[b].owner != own));
                bool provEdge = r != v || b != v;
                bool selEdge = (v == selProvince && provEdge) || (r == selProvince && r != v) || (b == selProvince && b != v);
                if (selEdge) c = WHITE;
                else if (natEdge) c = (Color){ 15, 15, 20, 255 };
                else if (provEdge) c = Shade(c, zoom >= 2 ? 0.6f : 0.75f);
            }
            mapPixels[py * MAP_VW + px] = c;
        }
    }
    UpdateTexture(mapTex, mapPixels);

    for (int i = 0; i < N_COUNT; i++) nationLabel[i][0] = nationLabel[i][1] = nationLabel[i][2] = 0;
    for (int row = 0; row < MAP_H; row++)
        for (int col = 0; col < MAP_W; col++) {
            int v = MAP_CELLS[row][col];
            if (v >= PROV_COUNT) continue;
            int own = game.prov[v].owner;
            nationLabel[own][0] += col;
            nationLabel[own][1] += row;
            nationLabel[own][2] += 1;
        }
}

Vector2 ProvinceCenter(int p)
{
    return CellToScreen(PROVINCE_LABEL[p][0] + 0.5f, PROVINCE_LABEL[p][1] + 0.5f);
}

void DrawArrow(Vector2 a, Vector2 b, Color c)
{
    Vector2 d = { b.x - a.x, b.y - a.y };
    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1) return;
    d.x /= len; d.y /= len;
    Vector2 s = { a.x + d.x * 6, a.y + d.y * 6 }, e = { b.x - d.x * 6, b.y - d.y * 6 };
    DrawLineEx(s, e, 3, Fade(BLACK, 0.5f));
    DrawLineEx(s, e, 2, c);
    Vector2 l = { e.x - d.x * 7 - d.y * 5, e.y - d.y * 7 + d.x * 5 };
    Vector2 r = { e.x - d.x * 7 + d.y * 5, e.y - d.y * 7 - d.x * 5 };
    DrawTriangle(e, l, r, c);
    DrawTriangle(e, r, l, c);
}

const char *DOMAIN_TAGS[3] = { "", "A", "N" };

/* pions : un pion par nation et par milieu (terre, air, mer) present dans la province */
void DrawUnitCounters(void)
{
    float chipW = zoom >= 2 ? 20 : 15, chipH = zoom >= 2 ? 12 : 10;
    int font = 10;
    for (int p = 0; p < PROV_COUNT; p++) {
        Vector2 c = ProvinceCenter(p);
        if (c.x < MAP_X - 20 || c.y < MAP_Y - 20 || c.x > MAP_X + MAP_VW + 20 || c.y > MAP_Y + MAP_VH + 20) continue;
        int count[N_COUNT][3] = { { 0 } }, ready[N_COUNT][3] = { { 0 } };
        bool sel[3] = { false };
        bool seen = War_Visible(&game, p);
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &game.units[i];
            if (!u->active || u->prov != p) continue;
            if (!seen && u->owner != Me() && !game.ally[Me()][u->owner]) continue;   /* brouillard de guerre */
            int d = UNIT_TYPES[u->type].domain;
            count[u->owner][d]++;
            if (!u->training) ready[u->owner][d]++;
            if (uiSel[i] && u->owner == Me()) sel[d] = true;
        }
        int slot = 0, nslots = 0;
        for (int n = 0; n < N_COUNT; n++) for (int d = 0; d < 3; d++) if (count[n][d]) nslots++;
        float x0 = c.x - nslots * (chipW + 1) / 2;
        for (int n = 0; n < N_COUNT; n++) {
            for (int d = 0; d < 3; d++) {
                if (!count[n][d]) continue;
                Rectangle r = { x0 + slot * (chipW + 1), c.y + 4, chipW, chipH };
                Color fill = n == N_FRANCE ? (Color){ 40, 90, 200, 255 } : Shade(NationColor(n), 0.85f);
                bool mine = n == Me();
                if (d == DOM_AIR) fill = Shade(fill, 1.25f);
                if (d == DOM_SEA) fill = Shade(fill, 0.6f);
                Color border = mine ? (sel[d] ? C_SEL : WHITE) : War_AtWar(&game, Me(), n) ? C_WAR : (Color){ 20, 20, 20, 255 };
                DrawRectangleRec(r, fill);
                DrawRectangleLinesEx(r, 1, border);
                char buf[8];
                snprintf(buf, sizeof(buf), "%s%d", DOMAIN_TAGS[d], count[n][d]);
                Text(buf, r.x + (r.width - TextW(buf, font)) / 2, r.y + (chipH - font) / 2 + 1, font,
                     ready[n][d] < count[n][d] && mine ? C_DIM : WHITE);
                if (inputLayer == activeLayer && CheckCollisionPointRec(mouse, r)) {
                    char tip[600];
                    int len = snprintf(tip, sizeof(tip), Tr("%s de %s en %s"), d == DOM_LAND ? "Divisions" : d == DOM_AIR ? "Escadres" : "Flottes",
                                       Tr(NATIONS[n].name), Tr(PROVINCE_NAMES[p]));
                    int gi = Mil_GeneralAt(&game, n, p);
                    if (gi >= 0 && d == DOM_LAND)
                        len += snprintf(tip + len, sizeof(tip) - len, Tr("\nCommandement : %s (%s, compétence %d)"), game.generals[gi].name,
                                        Tr(GENERAL_TRAITS[game.generals[gi].trait]), game.generals[gi].skill);
                    for (int i = 0; i < MAX_UNITS && len < (int)sizeof(tip) - 70; i++) {
                        const Unit *u = &game.units[i];
                        if (!u->active || u->prov != p || u->owner != n || (int)UNIT_TYPES[u->type].domain != d) continue;
                        len += snprintf(tip + len, sizeof(tip) - len, Tr("\n%s  effectifs %.0f%%  organisation %.0f%%%s"),
                                        Tr(UNIT_TYPES[u->type].name), u->str * 100, u->org * 100, u->training ? "  (formation)" : "");
                        if (d == DOM_LAND && len < (int)sizeof(tip) - 30)
                            len += snprintf(tip + len, sizeof(tip) - len, "  %s", Tr(VET_RANKS[War_VetRank(u)]));
                    }
                    snprintf(tooltip, sizeof(tooltip), "%s", tip);
                }
                slot++;
            }
        }
        /* fortifications et depots */
        if (game.prov[p].fort > 0 && (zoom >= 2 || mapMode == MAP_LOGISTIQUE)) {
            Rectangle fr = { c.x - 5, c.y - 12, 10, 8 };
            DrawRectangleRec(fr, (Color){ 120, 120, 130, 255 });
            for (int k = 0; k < game.prov[p].fort; k++) DrawRectangle((int)fr.x + 1 + k * 3, (int)fr.y - 2, 2, 2, (Color){ 120, 120, 130, 255 });
            DrawRectangleLinesEx(fr, 1, BLACK);
        }
        if (game.prov[p].depot && (zoom >= 2 || mapMode == MAP_LOGISTIQUE) && (game.prov[p].depot == N_FRANCE + 1 || seen)) {
            Vector2 a = { c.x + 8, c.y - 12 }, b = { c.x + 3, c.y - 4 }, d2 = { c.x + 13, c.y - 4 };
            DrawTriangle(a, b, d2, game.prov[p].depot == N_FRANCE + 1 ? C_GOOD : C_WAR);
        }
        /* general francais present */
        if (Mil_GeneralAt(&game, N_FRANCE, p) >= 0) {
            Vector2 g = { x0 - 6, c.y + 4 + chipH / 2 };
            DrawCircleV(g, 4, C_SEL);
            DrawCircleLines((int)g.x, (int)g.y, 4, BLACK);
        }
    }
    /* ordres du joueur actif */
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &game.units[i];
        if (!u->active || u->owner != Me() || u->target < 0 || u->target == u->prov) continue;
        bool attack = !War_Friendly(&game, Me(), u->target);
        Color col = attack ? C_WAR : UNIT_TYPES[u->type].domain == DOM_LAND ? C_GOOD : C_ACCENT;
        DrawArrow(ProvinceCenter(u->prov), ProvinceCenter(u->target), col);
    }
}

void MapInput(void)
{
    bool over = inputLayer == activeLayer && InMap(mouse);
    float wheel = over ? GetMouseWheelMove() : 0;
#if defined(PLATFORM_ANDROID)
    if (over && IsGestureDetected(GESTURE_PINCH_OUT)) wheel = 1;
    if (over && IsGestureDetected(GESTURE_PINCH_IN)) wheel = -1;
#endif
    if (wheel > 0) SetZoom(ZoomIndex() + 1, mouse);
    if (wheel < 0) SetZoom(ZoomIndex() - 1, mouse);
    if (inputLayer == activeLayer && !chatTyping) {
        float step = 12 / zoom;
        if (IsKeyDown(KEY_LEFT))  camX -= step * 0.25f;
        if (IsKeyDown(KEY_RIGHT)) camX += step * 0.25f;
        if (IsKeyDown(KEY_UP))    camY -= step * 0.25f;
        if (IsKeyDown(KEY_DOWN))  camY += step * 0.25f;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) SetZoom(ZoomIndex() + 1, (Vector2){ MAP_X + MAP_VW / 2, MAP_Y + MAP_VH / 2 });
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) SetZoom(ZoomIndex() - 1, (Vector2){ MAP_X + MAP_VW / 2, MAP_Y + MAP_VH / 2 });
        if (KeyHit(KA_OVERVIEW)) { zoom = 1; camX = camY = 0; }
        if (KeyHit(KA_CENTER)) FocusProvince(PV_FR_IDF, 3);
        ClampCamera();
    }
    /* glisser avec le bouton gauche ou celui du milieu pour deplacer la carte */
    if (over && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))) {
        mapPressed = true;
        mapDragging = false;
        dragStart = mouse;
        dragCam = (Vector2){ camX, camY };
    }
    if (mapPressed && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))) {
        float dx = mouse.x - dragStart.x, dy = mouse.y - dragStart.y;
        if (dx * dx + dy * dy > 36 || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) mapDragging = true;
        if (mapDragging) {
            camX = dragCam.x - dx / CellW();
            camY = dragCam.y - dy / CellH();
            ClampCamera();
        }
    }
}

void DrawMap(void)
{
    MapInput();
    unsigned sig = ComputeSignature();
    if (sig != mapSignature) { mapSignature = sig; RebuildMap(); }
    DrawTexture(mapTex, MAP_X, MAP_Y, WHITE);
    DrawRectangleLines(MAP_X - 1, MAP_Y - 1, MAP_VW + 2, MAP_VH + 2, C_BORDER);

    BeginScissorMode(MAP_X, MAP_Y, MAP_VW, MAP_VH);
    if (mapMode == MAP_POLITIQUE && zoom < 2) {
        for (int n = 0; n < N_COUNT; n++) {
            if (nationLabel[n][2] < 150) continue;
            Vector2 v = CellToScreen(nationLabel[n][0] / nationLabel[n][2], nationLabel[n][1] / nationLabel[n][2]);
            const char *nm = Tr(NATIONS[n].name);
            int w = TextW(nm, 10);
            Text(nm, v.x - w / 2 + 1, v.y - 13, 10, (Color){ 0, 0, 0, 170 });
            Text(nm, v.x - w / 2, v.y - 14, 10, n == N_FRANCE ? WHITE : (Color){ 235, 235, 235, 220 });
        }
    }
    if (zoom >= 2) {
        /* noms des regions */
        for (int p = 0; p < PROV_COUNT; p++) {
            Vector2 c = ProvinceCenter(p);
            if (c.x < MAP_X || c.y < MAP_Y || c.x > MAP_X + MAP_VW || c.y > MAP_Y + MAP_VH) continue;
            if (PROVINCE_CELLS[p] * zoom * zoom < 220) continue;
            const char *nm = Tr(PROVINCE_NAMES[p]);
            int w = TextW(nm, 10);
            Text(nm, c.x - w / 2 + 1, c.y - 13, 10, (Color){ 0, 0, 0, 180 });
            Text(nm, c.x - w / 2, c.y - 14, 10, game.prov[p].owner == N_FRANCE ? WHITE : (Color){ 230, 230, 230, 230 });
        }
    }

    float pulse = 0.5f + 0.5f * sinf(timeAcc * 5);
    for (int p = 0; p < PROV_COUNT; p++) {
        Vector2 c = ProvinceCenter(p);
        if (War_AtWar(&game, Me(), game.prov[p].ctrl) && (Me() != N_FRANCE || War_ProvinceAttackable(&game, p)))
            DrawCircleLines((int)c.x, (int)c.y, 6 + pulse * 2, C_WAR);
        if (game.prov[p].revolt) {
            DrawCircleV((Vector2){ c.x, c.y - 8 }, 5, Fade((Color){ 190, 60, 220, 255 }, 0.5f + 0.4f * pulse));
            Text("!", c.x - 2, c.y - 13, 10, WHITE);
        }
    }

    hoverProvince = inputLayer == activeLayer && !mapDragging ? ProvinceAt(mouse) : -1;
    DrawUnitCounters();
    EndScissorMode();

    if (hoverProvince >= 0) {
        const Province *pr = &game.prov[hoverProvince];
        if (!tooltip[0] && mapMode == MAP_LOGISTIQUE) {
            snprintf(tooltip, sizeof(tooltip), Tr("%s\nRavitaillement : %.0f%%%s\nFortifications : %d%s%s"), Tr(PROVINCE_NAMES[hoverProvince]),
                     War_SupplyLevel(&game, N_FRANCE, hoverProvince) * 100,
                     War_Friendly(&game, N_FRANCE, hoverProvince) && War_SupplyLevel(&game, N_FRANCE, hoverProvince) < 0.25f ? " - ENCERCLÉE" : "",
                     pr->fort, pr->depot == N_FRANCE + 1 ? "\nDépôt logistique français" : "",
                     War_Visible(&game, hoverProvince) ? "" : "\nHors de vue de nos capteurs");
        } else if (!tooltip[0] && mapMode == MAP_RESSOURCES) {
            char res[160] = "";
            int l2 = 0;
            for (int r = 0; r < SR_COUNT; r++) {
                float d = Strat_Deposit(hoverProvince, r) + (r == SR_PUCES ? pr->fabs : 0);
                if (d > 0) l2 += snprintf(res + l2, sizeof(res) - l2, Tr("\n%s : %.1f par tour"), Tr(STRATS[r].name), d);
            }
            snprintf(tooltip, sizeof(tooltip), "%s (%s)%s", Tr(PROVINCE_NAMES[hoverProvince]), Tr(NATIONS[pr->owner].name), l2 ? res : "\nAucune ressource stratégique");
        } else if (!tooltip[0] && mapMode == MAP_COMMERCE) {
            int o = pr->owner;
            static const char *kinds[3] = { "coupée", "terrestre", "maritime" };
            if (o == N_FRANCE)
                snprintf(tooltip, sizeof(tooltip), Tr("%s\nBlocus de nos côtes : %.0f%%, raids sur nos convois : %.0f%%"), Tr(PROVINCE_NAMES[hoverProvince]),
                         game.nat[N_FRANCE].blockade * 100, game.nat[N_FRANCE].raid * 100);
            else
                snprintf(tooltip, sizeof(tooltip), Tr("%s (%s)\nRoute commerciale %s : %.0f%%\nBlocus subi : %.0f%%%s"), Tr(PROVINCE_NAMES[hoverProvince]),
                         Tr(NATIONS[o].name), Tr(kinds[game.routeKind[o]]), game.route[o] * 100, game.nat[o].blockade * 100,
                         game.nat[o].embargoFrance ? Tr("\nEmbargo contre la France") : "");
        } else if (!tooltip[0] && mapMode == MAP_DEGATS) {
            snprintf(tooltip, sizeof(tooltip), Tr("%s (%s)\nDégâts : %.0f%%\nContamination : %.0f%%\nHabitabilité : %.0f"), Tr(PROVINCE_NAMES[hoverProvince]),
                     Tr(NATIONS[pr->owner].name), pr->damage * 100, pr->fallout * 100, pr->hab);
        }
        if (!tooltip[0]) {
            char occ[96] = "";
            if (pr->ctrl != pr->owner) snprintf(occ, sizeof(occ), Tr("\nOCCUPÉE par %s"), Tr(NATIONS[pr->ctrl].name));
            int mc = Soc_MainCulture(pr), mr = Soc_MainReligion(pr);
            snprintf(tooltip, sizeof(tooltip), Tr("%s (%s)%s\n%.1f M hab. - habitabilité %.0f - militance %.1f - chômage %.0f%%\n"
                     "Culture : %s %.0f%% - %s %.0f%%%s%s"),
                     Tr(PROVINCE_NAMES[hoverProvince]), Tr(NATIONS[pr->owner].name), occ, pr->pop, pr->hab,
                     Soc_ProvinceMilitancy(pr), pr->unemployment * 100, Tr(CULTURE_NAMES[mc]), pr->culture[mc] * 100,
                     Tr(RELIGION_NAMES[mr]), pr->religion[mr] * 100, pr->revolt ? "\nEN RÉVOLTE" : "",
                     SelectedCount() ? "\nClic droit : envoyer les unités sélectionnées" : "");
        }
        if (rclicked) { OrderSelected(hoverProvince); rclicked = false; }
    }
    /* selection au relachement si la carte n'a pas ete glissee */
    if (mapPressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (!mapDragging && hoverProvince >= 0 && orderMode) {
            OrderSelected(hoverProvince);
            orderMode = false;
        } else if (!mapDragging && hoverProvince >= 0) {
            const Province *pr = &game.prov[hoverProvince];
            selProvince = hoverProvince;
            if (pr->owner != Me()) selNation = pr->owner;
            else if (pr->ctrl != Me()) selNation = pr->ctrl;
            SelectUnitsIn(hoverProvince, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
            if (tab != TAB_NATIONS && tab != TAB_ARMEE && tab != TAB_ETATMAJOR) tab = TAB_PROVINCE;
            strikeArmed = false;
            provScroll = 0;
        }
        mapPressed = mapDragging = false;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) mapPressed = mapDragging = false;
    if (clicked && InMap(mouse)) clicked = false;

    static const char *modes[MAP_MODE_COUNT] = { "POLITIQUE", "CLIMAT", "MILITANCE", "POPULAT.", "CULTURES", "RELATIONS", "LOGISTIQUE",
                                                 "RESSOURC.", "COMMERCE", "DÉGÂTS" };
    bool big = bigUi;   /* grande interface : commandes de la carte agrandies pour le doigt */
    static bool modeList;
    if (!big) {
        modeList = false;
        float mw = (MAP_VW - 62) / (float)MAP_MODE_COUNT;
        for (int i = 0; i < MAP_MODE_COUNT; i++) {
            Rectangle r = { MAP_X + 4 + i * mw, MAP_Y + MAP_VH - 24, mw - 2, 18 };
            bool sel = mapMode == (MapMode)i;
            DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 240 } : (Color){ 16, 22, 30, 220 });
            DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
            Text(modes[i], r.x + (r.width - TextW(modes[i], 10)) / 2, r.y + 4, 10, sel ? C_TEXT : C_DIM);
            if (Hover(r)) {
                tooltip[0] = 0;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { mapMode = (MapMode)i; mapPressed = false; }
            }
        }
    } else {
        /* un bouton qui deroule la liste des modes */
        char label[64];
        snprintf(label, sizeof(label), "%s %s", Tr(modes[mapMode]), modeList ? "v" : "^");
        Rectangle r = { MAP_X + 4, MAP_Y + MAP_VH - 40, 190, 34 };
        DrawRectangleRec(r, modeList ? (Color){ 44, 66, 96, 245 } : (Color){ 16, 22, 30, 230 });
        DrawRectangleLinesEx(r, 2, C_ACCENT);
        Text(label, r.x + (r.width - TextW(label, 20)) / 2, r.y + 7, 20, C_TEXT);
        if (Hover(r)) {
            tooltip[0] = 0;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { modeList = !modeList; mapPressed = false; }
        }
        if (modeList) {
            for (int i = 0; i < MAP_MODE_COUNT; i++) {
                Rectangle o = { MAP_X + 4, MAP_Y + MAP_VH - 40 - (MAP_MODE_COUNT - i) * 36, 190, 34 };
                bool sel = mapMode == (MapMode)i;
                DrawRectangleRec(o, sel ? (Color){ 44, 66, 96, 245 } : (Color){ 16, 22, 30, 240 });
                DrawRectangleLinesEx(o, 1, sel ? C_ACCENT : C_BORDER);
                Text(modes[i], o.x + (o.width - TextW(modes[i], 20)) / 2, o.y + 7, 20, sel ? C_TEXT : C_DIM);
                if (Hover(o)) {
                    tooltip[0] = 0;
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { mapMode = (MapMode)i; modeList = false; mapPressed = false; }
                }
            }
        }
    }
    /* ordre tactile : le prochain toucher sur la carte envoie les unites selectionnees */
    {
        Rectangle r = big ? (Rectangle){ MAP_X + MAP_VW - 104, MAP_Y + MAP_VH - 80, 96, 34 }
                          : (Rectangle){ MAP_X + MAP_VW - 52, MAP_Y + MAP_VH - 46, 44, 18 };
        int fs = big ? 20 : 10;
        DrawRectangleRec(r, orderMode ? (Color){ 150, 60, 50, 240 } : Hover(r) ? (Color){ 44, 66, 96, 240 } : (Color){ 16, 22, 30, 220 });
        DrawRectangleLinesEx(r, big ? 2 : 1, orderMode ? C_WAR : C_BORDER);
        Text("ORDRE", r.x + (r.width - TextW("ORDRE", fs)) / 2, r.y + (r.height - fs) / 2, fs, C_TEXT);
        if (Hover(r)) {
            snprintf(tooltip, sizeof(tooltip), "%s", Tr("Ordre au prochain toucher\nSur écran tactile (sans clic droit) : activez, puis touchez la "
                     "destination des unités sélectionnées."));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { orderMode = !orderMode; mapPressed = false; }
        }
    }
    /* boutons de zoom */
    for (int i = 0; i < 2; i++) {
        Rectangle r = big ? (Rectangle){ MAP_X + MAP_VW - 104 + i * 50, MAP_Y + MAP_VH - 40, 46, 34 }
                          : (Rectangle){ MAP_X + MAP_VW - 52 + i * 24, MAP_Y + MAP_VH - 24, 20, 18 };
        int fs = big ? 20 : 10;
        DrawRectangleRec(r, Hover(r) ? (Color){ 44, 66, 96, 240 } : (Color){ 16, 22, 30, 220 });
        DrawRectangleLinesEx(r, big ? 2 : 1, C_BORDER);
        Text(i ? "+" : "-", r.x + (r.width - TextW("+", fs)) / 2, r.y + (r.height - fs) / 2, fs, C_TEXT);
        if (Hover(r)) {
            tooltip[0] = 0;
            snprintf(tooltip, sizeof(tooltip), Tr("Zoom x%.1f\nMolette ou +/- : zoomer. Glisser : déplacer. Flèches : défiler.\n"
                     "F : centrer sur la France. Origine : vue d'ensemble."), zoom);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                SetZoom(ZoomIndex() + (i ? 1 : -1), (Vector2){ MAP_X + MAP_VW / 2, MAP_Y + MAP_VH / 2 });
                mapPressed = false;
            }
        }
    }
    static const char *legend[MAP_MODE_COUNT] = {
        "Clic : sélectionner. Clic droit : envoyer. Molette : zoom. Rayures : territoire occupé.",
        "Habitabilité : vert = tempéré, rouge = invivable, points verts = contamination, bleu = terres submergées.",
        "Militance : vert = calme, rouge = agitée, violet = révolte.",
        "Densité de population.",
        "Culture majoritaire (couleur de sa nation) ; points : seconde culture au-delà de 15%.",
        "Relations avec la France : rouge = hostile, vert = amical, cyan = allié, bordeaux = en guerre. Pointillés : hors de vue.",
        "Ravitaillement français : vert = assuré, rouge = coupé (encerclement). Carrés : forts ; triangles : dépôts.",
        "Ressources stratégiques : violet = terres rares, vert = uranium, bleu = semi-conducteurs.",
        "Routes commerciales vers la France : vert = terrestre, bleu pointillé = maritime, rouge = menacée, gris = coupée.",
        "Dégâts (combats, bombardements, frappes) : jaune = légers, rouge = ruines ; points verts = contamination.",
    };
    DrawRectangle(MAP_X, MAP_Y, MAP_VW, 18, Fade((Color){ 10, 14, 20, 255 }, 0.6f));
    Text(legend[mapMode], MAP_X + 6, MAP_Y + 4, 10, (Color){ 230, 230, 230, 220 });

    if (toastTime > 0) {
        int w = TextW(toast, 10);
        Rectangle r = { MAP_X + MAP_VW / 2 - w / 2 - 10, MAP_Y + MAP_VH - 50, (float)w + 20, 20 };
        DrawRectangleRec(r, Fade((Color){ 10, 14, 20, 255 }, 0.9f));
        DrawRectangleLinesEx(r, 1, C_ACCENT);
        Text(toast, r.x + 10, r.y + 5, 10, C_TEXT);
    }
}

/* ------------------------------------------------------------------ */
/* Barre superieure                                                    */
/* ------------------------------------------------------------------ */
void DrawCommanderBar(void)
{
    int n = Me();
    const Nation *na = &game.nat[n];
    char date[32], buf[200];
    Game_DateString(game.turn, date, sizeof(date));
    Text("BOUCLIER 2050", 10, 4, 20, C_ACCENT);
    snprintf(buf, sizeof(buf), Tr("%s - %d/%d - hot-seat"), date, game.turn + 1, TOTAL_TURNS);
    Text(buf, 10, 26, 10, C_TEXT);
    snprintf(buf, sizeof(buf), Tr("COMMANDANT : %s"), Tr(NATIONS[n].name));
    Text(buf, 250, 6, 20, NationColor(n));
    int wars = 0;
    for (int k = 0; k < N_COUNT; k++) if (game.war[n][k]) wars++;
    snprintf(buf, sizeof(buf), Tr("Trésor %.0f Md  -  armée %.1f / %.1f  -  %d province(s), %.1f M hab.  -  %d guerre(s)  -  %s"),
             na->treasury, War_UnitLoad(&game, n), War_ArmyMax(&game, n), Soc_NationProvinces(&game, n), Soc_NationPop(&game, n),
             wars, Tr(REGIMES[na->regime].name));
    Text(buf, 250, 28, 10, C_TEXT);
}

void DrawTopBar(void)
{
    DrawRectangle(0, 0, VW, 44, C_PANEL);
    DrawLine(0, 44, VW, 44, C_BORDER);
    if (Me() != N_FRANCE) { DrawCommanderBar(); return; }

    char date[32], buf[160];
    Game_DateString(game.turn, date, sizeof(date));
    Text("BOUCLIER 2050", 10, 4, 20, C_ACCENT);
    snprintf(buf, sizeof(buf), Tr("%s - %d/%d - +%.2f°C - mer +%.2f m"), date, game.turn + 1, TOTAL_TURNS, game.temp, game.seaLevel);
    Text(buf, 10, 26, 10, C_TEXT);
    snprintf(buf, sizeof(buf), Tr("Score %d"), Game_Score(&game, NULL));
    Rectangle sr = { 176, 4, 70, 12 };
    Text(buf, sr.x, sr.y, 10, C_SEL);
    Tooltip(sr, "Score de victoire : détail dans l'onglet POLITIQUE.\nClic (ou touche G) : courbes de la partie.");
    if (Hover(sr) && clicked) { graphsOpen = true; clicked = false; }

    float x = 250;
    float w = (VW - x - 10) / R_COUNT;
    for (int r = 0; r < R_COUNT; r++) {
        Rectangle area = { x + r * w, 2, w - 6, 40 };
        float v = game.res[r];
        Color c = r == R_BUDGET ? (v < 0 ? C_BAD : C_TEXT) : LevelColor(v);
        Text(Tr(RESOURCE_NAMES[r]), area.x, area.y + 3, 10, C_DIM);
        snprintf(buf, sizeof(buf), "%.0f", v);
        Text(buf, area.x, area.y + 15, 20, c);
        float d = game.lastDelta[r];
        if (game.turn > 0 && fabsf(d) >= 0.5f) {
            snprintf(buf, sizeof(buf), "%+.0f", d);
            Text(buf, area.x + TextW("000", 20) + 6, area.y + 20, 10, d > 0 ? C_GOOD : C_BAD);
        }
        if (r != R_BUDGET) Bar((Rectangle){ area.x, area.y + 36, area.width - 6, 3 }, v, 100, c);
        else {
            snprintf(buf, sizeof(buf), Tr("%+.1f/tour"), Eco_Net(&game));
            Text(buf, area.x + TextW("000", 20) + 22, area.y + 20, 10, C_DIM);
        }
        char tip[300];
        if (r >= R_ENERGIE && r <= R_ALIM) {
            int k = r == R_ALIM ? G_FOOD : r == R_EAU ? G_WATER : G_ENERGY;
            const NationEconomy *e = &game.eco[N_FRANCE];
            snprintf(tip, sizeof(tip), Tr("%s : %.1f\n%s\nProduction %.0f + importations %.0f / besoins %.0f"), Tr(RESOURCE_NAMES[r]), v,
                     Tr(RESOURCE_DESC[r]), e->supply[k], e->imports[k], e->demand[k]);
        } else {
            if (r == R_BUDGET)
                snprintf(tip, sizeof(tip), Tr("%s : %.1f\n%s\nDette : %.0f Md, note %s."), Tr(RESOURCE_NAMES[r]), v, Tr(RESOURCE_DESC[r]), game.debt,
                         Tr(RATING_NAMES[Eco_Rating(&game)]));
            else snprintf(tip, sizeof(tip), Tr("%s : %.1f\n%s"), Tr(RESOURCE_NAMES[r]), v, Tr(RESOURCE_DESC[r]));
        }
        Tooltip(area, tip);
    }
}
