/*
 * BOUCLIER 2050 - etat et outils partages par les modules de l'interface
 * (main.c, ui_widgets.c, ui_map.c, ui_tabs_war.c, ui_tabs_state.c, ui_screens.c).
 */
#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "game.h"
#include "lang.h"
#include "sound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VW 1280
#define VH 720
#define SAVE_FILE "bouclier2050.sav"
#define AUTOSAVE_FILE "bouclier2050_auto.sav"
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
#define MAP_X 5
#define MAP_Y 48
#define MAP_VW 696                    /* fenetre de la carte (pixels) */
#define MAP_VH 536
#define CW0 ((float)MAP_VW / MAP_W)   /* taille d'une cellule au zoom 1 */
#define CH0 ((float)MAP_VH / MAP_H)
#define ZOOM_COUNT 6
#define RP_X 714
#define RP_Y 86
#define RP_W 560
#define TUTO_COUNT ((int)(sizeof(TUTO) / sizeof(TUTO[0])))
#define SETTINGS_FILE "bouclier2050.cfg"

typedef enum { SCR_TITLE, SCR_HELP, SCR_GAME, SCR_END, SCR_HOTSEAT, SCR_NET } Screen;
typedef enum {
    TAB_PROVINCE, TAB_ARMEE, TAB_ETATMAJOR, TAB_NATIONS, TAB_DEFENSE, TAB_HYBRIDE,
    TAB_SOCIETE, TAB_ECONOMIE, TAB_POLITIQUE, TAB_PROJETS, TAB_COUNT
} Tab;
typedef enum {
    MAP_POLITIQUE, MAP_CLIMAT, MAP_MILITANCE, MAP_POPULATION, MAP_CULTURES, MAP_RELATIONS, MAP_LOGISTIQUE, MAP_RESSOURCES,
    MAP_MODE_COUNT
} MapMode;
typedef enum { VR_ALL, VR_MAP, VR_PANEL, VR_LOWER, VR_LOG, VR_TOPBAR, VR_MODAL, VR_COUNT } ViewRegion;
typedef struct { const char *title; const char *text; } TutoStep;

extern const Color DOMAIN_COLORS[D_COUNT];
extern const Color FACTION_COLORS[F_COUNT];
extern const Color IDEOLOGY_COLORS[I_COUNT];
extern Game game;
extern Screen screen;
extern Tab tab;
extern MapMode mapMode;
extern int selProvince;
extern int selNation;
extern int selTheater;
extern int hoverProvince;
extern bool uiSel[MAX_UNITS];
extern int newDifficulty;
extern int newScenario;
extern bool showReports;
extern bool strikeArmed;
extern int strikeProvince;
extern char toast[160];
extern float toastTime;
extern RenderTexture2D target;
extern float viewScale;
extern Vector2 viewOffset;
extern Vector2 mouse;
extern bool clicked, rclicked;
extern int inputLayer, activeLayer;
extern char tooltip[600];
extern float timeAcc;
extern int logScroll;
extern bool quitRequested;
extern bool inProgress;
extern bool treatyOpen;
extern int selGeneral;
extern int provScroll;
extern int nationView;
extern bool orderMode;
extern int tutorial;
extern int tutoUnits, tutoTurn, tutoProv;
extern Replay session;
extern bool hotseatPick[N_COUNT];
extern int handoffFor;
extern int cmdrScroll;
extern const Rectangle REGIONS[VR_COUNT];
extern const char *REGION_NAMES[VR_COUNT];
extern bool bigUi;
extern ViewRegion region;
extern Rectangle viewSrc;
extern float navH;
extern char negoMsg[160];
extern float zoom;
extern float camX, camY;
extern bool mapPressed, mapDragging;
extern Vector2 dragStart, dragCam;
extern const float ZOOMS[];
extern Texture2D mapTex;
extern Color mapPixels[MAP_VW * MAP_VH];
extern unsigned mapSignature;
extern float nationLabel[N_COUNT][3];
extern Color provColor[PROV_COUNT];
extern short colOf[MAP_VW + 1], rowOf[MAP_VH + 1];
extern unsigned char seaDist[MAP_H][MAP_W];
extern unsigned short floodHist[PROV_COUNT][64];
extern const char *DOMAIN_TAGS[3];
extern const char *DECLARE_DESC;
extern const char *DOMAIN_LABELS[3];
extern int polView;
extern const TutoStep TUTO[];

int Me(void);
bool HotSeat(void);
bool Do(int type, int a, int b, int c, float f);
void Text(const char *s, float x, float y, int size, Color c);
int TextW(const char *s, int size);
int TextWrap(const char *s, float x, float y, float w, int size, Color c);
bool Hover(Rectangle r);
void Panel(Rectangle r, Color fill);
bool Button(Rectangle r, const char *label, bool enabled, int size);
void Tooltip(Rectangle r, const char *text);
bool ActionButton(Rectangle r, const char *label, const char *blocked, const char *desc);
bool Choice(Rectangle r, const char *label, bool sel);
void Bar(Rectangle r, float v, float max, Color c);
float Clampf2(float v, float lo, float hi);
Color LevelColor(float v);
Color MilColor(float m);
Color NationColor(int n);
Color Shade(Color c, float k);
Color Lerp3(Color a, Color b, Color c, float t);
void Toast(const char *msg);
void SelectUnitsIn(int p, bool add);
int SelectedCount(void);
void OrderSelected(int p);
void ComputeSeaDistance(void);
int FloodThreshold(int p);
float CellW(void);
float CellH(void);
void ClampCamera(void);
Vector2 CellToScreen(float col, float row);
bool InMap(Vector2 m);
int ProvinceAt(Vector2 m);
void SetZoom(int idx, Vector2 anchor);
int ZoomIndex(void);
void FocusProvince(int p, int zoomIdx);
Color CultureColor(int c);
Color ProvinceBaseColor(int p);
Color PatternColor(int p, Color c, int px, int py);
unsigned ComputeSignature(void);
void RebuildMap(void);
Vector2 ProvinceCenter(int p);
void DrawArrow(Vector2 a, Vector2 b, Color c);
void DrawUnitCounters(void);
void MapInput(void);
void DrawMap(void);
void DrawCommanderBar(void);
void DrawTopBar(void);
void StatLine(float x, float y, const char *label, const char *value, Color c);
void DrawProvinceTab(void);
const char *StratCostText(int type);
void DrawArmyTab(void);
void DrawStaffTab(void);
void DrawOpsPanel(int n, float dx, float dy, float bw);
void OpenTreaty(int n);
void DrawNationsTab(void);
void DrawTreaty(void);
void DrawDefenseTab(void);
void DrawHybridTab(void);
void Sparkline(Rectangle r, const float *v, int n, float lo, float hi, Color c, const char *label);
void DrawSocietyTab(void);
void Stepper(float x, float y, const char *label, float value, float *out, float step, const char *fmt, const char *tip);
void DrawEconomyTab(void);
void DrawCabinet(float x, float y);
void DrawPolitiqueTab(void);
void DrawProjectsTab(void);
Color LogColor(LogKind k);
void DrawLog(void);
void DrawDilemma(void);
void DeltaLine(float x, float y, const char *label, float before, float after, const char *fmt);
void DrawReport(void);
void DrawTooltip(void);
int FrenchUnitCount(void);
void StartTutorial(void);
bool TutoDone(int step);
void DrawTutorial(void);
void LoadSettings(void);
void SaveSettings(void);
void ResetUi(void);
void NewGame(void);
void LoadGame(const char *path);
void SaveGame(void);
void DrawBackdrop(void);
void DrawTitle(void);
void DrawHelp(void);
void DrawEnd(void);
void DrawCommanderPanel(void);
void DrawHandoff(void);
void DrawHotseatSetup(void);
void DrawGame(void);
Rectangle CurrentRegion(void);
void UpdateView(void);
void DrawNavBar(void);

const char *LogText(const LogEntry *e);
/* ui_net.c : partie en reseau */
extern bool netPlaying;
extern int  localNation;
bool NetDo(const Command *cmd);
void NetUpdate(void);
void NetBanner(void);
void NetLeave(void);
void DrawNetLobby(void);
void NetHostStart(void);
bool NetJoin(const char *addr, int nation);
extern const char *uiTestDir;   /* mode test d'interface : dossier des captures (NULL sinon) */

#endif
