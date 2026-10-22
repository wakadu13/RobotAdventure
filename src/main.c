/*
 * BOUCLIER 2050 - interface raylib
 *
 * Rendu volontairement simple (formes 2D, une texture de carte generee,
 * police par defaut, aucun fichier externe) pour tourner sur un maximum de
 * machines. Resolution logique fixe 1280x720, mise a l'echelle automatique.
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Etat de l'interface                                                 */
/* ------------------------------------------------------------------ */
Game     game;
Screen   screen = SCR_TITLE;
Tab      tab = TAB_PROVINCE;
MapMode  mapMode = MAP_POLITIQUE;
int      selProvince = PV_FR_IDF;
int      selNation = N_ALLEMAGNE;
int      selTheater = TH_NORD;
int      hoverProvince = -1;
bool     uiSel[MAX_UNITS];
int      newDifficulty = DIFF_NORMAL;
int      newScenario;         /* 0 : 2030 ; 1 : crise de 2040 */
bool     showReports = true;
bool     strikeArmed;
int      strikeProvince = -1;
char     toast[160];
float    toastTime;
RenderTexture2D target;
float    viewScale = 1;
Vector2  viewOffset;
Vector2  mouse;
bool     clicked, rclicked;
int      inputLayer, activeLayer;
char     tooltip[600];
float    timeAcc;
int      logScroll;
bool     quitRequested;
bool     inProgress;
bool     treatyOpen;
int      selGeneral = -1;
int      provScroll;
int      nationView;          /* 0 diplomatie, 1 operations, 2 historique */
bool     orderMode;           /* ordre au prochain toucher (ecrans tactiles) */
int      tutorial = -1;       /* etape du tutoriel (-1 : aucun) */
int      tutoUnits, tutoTurn, tutoProv;
Replay   session;             /* commandes de la partie : rejeu et futur multijoueur */
bool     hotseatPick[N_COUNT]; /* nations confiees a d'autres joueurs (hot-seat) */
int      handoffFor = -1;     /* ecran de passage de l'appareil au joueur suivant */
int      cmdrScroll;

/* joueur dont c'est le tour : la France, ou un commandant en hot-seat */
int Me(void) { return game.activePlayer; }

bool HotSeat(void)
{
    for (int n = 1; n < N_COUNT; n++) if (game.human[n]) return true;
    return false;
}

/* grande interface (telephones) : une region de l'ecran logique agrandie a toute la taille de l'ecran */
const Rectangle REGIONS[VR_COUNT] = {
    { 0, 0, VW, VH }, { 0, 44, 712, 546 }, { 704, 44, 576, 346 }, { 704, 368, 576, 352 },
    { 0, 584, 712, 136 }, { 0, 0, 1280, 46 }, { 180, 60, 920, 600 },
};
const char *REGION_NAMES[VR_COUNT] = { "Tout", "Carte", "Panneau", "Bas", "Journal", "Barre", "Fenêtre" };
bool       bigUi;
ViewRegion region = VR_MAP;
Rectangle  viewSrc = { 0, 0, VW, VH };
float      navH;

/* toute action du joueur passe par une commande, enregistree pour le rejeu */
bool Do(int type, int a, int b, int c, float f)
{
    Command cmd = { (unsigned char)type, (signed char)game.activePlayer, (short)a, (short)b, (short)c, f };
    bool ok = Cmd_Apply(&game, &cmd);
    if (ok) Replay_Record(&session, &cmd);
    if (ok && type == CMD_DECLARE_WAR) Snd_Play(SFX_WAR);
    if (ok && (type == CMD_STRIKE || type == CMD_TACTICAL)) Snd_Play(SFX_NUKE);
    return ok;
}
char     negoMsg[160];

/* camera de la carte : zoom et coin superieur gauche (en cellules) */
float    zoom = 1;
float    camX, camY;
bool     mapPressed, mapDragging;
Vector2  dragStart, dragCam;

/* ------------------------------------------------------------------ */
/* reglages de l'interface conserves d'une session a l'autre */
void LoadSettings(void)
{
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f) return;
    int big = bigUi, rep = showReports, snd = 1, lang = gLang;
    if (fscanf(f, "grande_interface=%d\nrapports=%d\nson=%d\nlangue=%d", &big, &rep, &snd, &lang) >= 1) {
        bigUi = big != 0;
        showReports = rep != 0;
        Snd_SetEnabled(snd != 0);
        gLang = lang == LANG_EN ? LANG_EN : LANG_FR;
    }
    fclose(f);
}

void SaveSettings(void)
{
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f) return;
    fprintf(f, "grande_interface=%d\nrapports=%d\nson=%d\nlangue=%d\n", bigUi ? 1 : 0, showReports ? 1 : 0, Snd_Enabled() ? 1 : 0, gLang);
    fclose(f);
}

void ResetUi(void)
{
    selProvince = PV_FR_IDF;
    selNation = N_ALLEMAGNE;
    selGeneral = -1;
    treatyOpen = false;
    FocusProvince(PV_FR_IDF, 2);
    tab = TAB_PROVINCE;
    logScroll = 0;
    mapSignature = 0;
    memset(uiSel, 0, sizeof(uiSel));
    strikeArmed = false;
    screen = SCR_GAME;
    inProgress = true;
}

void NewGame(void)
{
    unsigned seed = uiTestDir ? 2030u : (unsigned)time(NULL);   /* graine fixe pour les tests d'interface */
    Game_Init(&game, seed, newDifficulty);
    handoffFor = -1;
    Replay_Clear(&session);
    session.seed = seed;
    session.difficulty = newDifficulty;
    if (newScenario) Do(CMD_SCENARIO, newScenario, 0, 0, 0);
    ResetUi();
}

void LoadGame(const char *path)
{
    if (Game_Load(&game, path)) {
        Replay_Clear(&session);   /* le rejeu repart d'une nouvelle partie seulement */
        ResetUi();
        Toast("Partie chargée.");
        if (HotSeat()) handoffFor = Me();
        if (game.state != GAME_RUNNING) screen = SCR_END;
    } else {
        Toast("Sauvegarde illisible ou d'une autre version.");
    }
}

void SaveGame(void)
{
    Toast(Game_Save(&game, SAVE_FILE) ? "Partie sauvegardée (" SAVE_FILE ")." : "Échec de la sauvegarde.");
}

void DrawBackdrop(void)
{
    ClearBackground(C_BG);
    for (int i = 0; i < 40; i++) {
        float x = fmodf(i * 97.0f + timeAcc * (8 + i % 5), VW);
        float y = fmodf(i * 53.0f, VH);
        DrawCircle((int)x, (int)y, 1 + i % 2, Fade(C_ACCENT, 0.25f));
    }
    for (int y = 0; y < VH; y += 4) DrawLine(0, y, VW, y, (Color){ 0, 0, 0, 40 });
}

void DrawTitle(void)
{
    DrawBackdrop();
    Text("BOUCLIER 2050", VW / 2 - TextW("BOUCLIER 2050", 60) / 2, 70, 60, C_ACCENT);
    const char *sub = "Grande stratégie et guerre hybride en Europe et en Méditerranée, 2030-2060";
    Text(sub, VW / 2 - TextW(sub, 20) / 2, 146, 20, C_TEXT);
    const char *pitch =
        "2030. Le monde a franchi +1,5°C et le réchauffement s'accélère. Sécheresses, mégafeux et effondrement agricole "
        "vident le Sud et jettent des millions de personnes sur les routes. Gérez une population vivante et son économie, "
        "choisissez votre régime, de la commune utopique à l'État totalitaire, menez vos divisions, maniez la dissuasion "
        "et défendez vos frontières dans tous les milieux. Négociez vos traités, nommez vos généraux, surveillez la "
        "prolifération. Tenez trente ans.";
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
    if (Button((Rectangle){ bx, y, 182, 38 }, "NOUVELLE PARTIE", true, 20)) { NewGame(); tutorial = -1; }
    if (Button((Rectangle){ bx + 188, y, 92, 38 }, "TUTORIEL", true, 10)) { NewGame(); StartTutorial(); }
    y += 46;
    char buf[64];
    snprintf(buf, sizeof(buf), Tr("Difficulté : %s"), Tr(DIFFICULTY_NAMES[newDifficulty]));
    Rectangle dr = { bx, y, 182, 30 };
    if (Button(dr, buf, true, 10)) newDifficulty = (newDifficulty + 1) % DIFF_COUNT;
    Rectangle ur = { bx + 188, y, 92, 30 };
    if (Button(ur, bigUi ? "Interface : grande" : "Interface : normale", true, 10)) { bigUi = !bigUi; SaveSettings(); }
    Tooltip(ur, "Grande interface (téléphones, petits écrans) : une partie de l'écran à la fois, agrandie ; une barre de "
                "navigation en bas de l'écran passe de la carte au panneau, au journal ou à la barre des ressources. F2 pour basculer.");
    Tooltip(dr, "Facile : menaces et climat atténués, recettes accrues.\nDifficile : menaces, climat et IA plus agressifs, "
                "recettes réduites ; le score est multiplié par 1,3.");
    y += 38;
    snprintf(buf, sizeof(buf), Tr("Scénario : %s"), Tr(SCENARIO_NAMES[newScenario]));
    Rectangle scr = { bx, y, 182, 30 };
    if (Button(scr, buf, true, 10)) newScenario = (newScenario + 1) % 2;
    Tooltip(scr, Tr(SCENARIO_DESC[newScenario]));
    Rectangle sor = { bx + 188, y, 92, 30 };
    if (Button(sor, Snd_Enabled() ? "Son : activé" : "Son : coupé", true, 10)) { Snd_SetEnabled(!Snd_Enabled()); SaveSettings(); }
    Tooltip(sor, "Effets sonores et nappe d'ambiance, synthétisés par le jeu (aucun fichier audio).");
    y += 38;
    Rectangle lr = { bx, y, 280, 30 };
    if (Button(lr, gLang == LANG_EN ? "Language: English" : "Langue : français", true, 10)) {
        gLang = gLang == LANG_EN ? LANG_FR : LANG_EN;
        mapSignature = 0;
        SaveSettings();
    }
    y += 38;
    if (Button((Rectangle){ bx, y, 88, 38 }, "AIDE", true, 20)) screen = SCR_HELP;
    Rectangle hr = { bx + 96, y, 88, 38 };
    if (Button(hr, "HOT-SEAT", true, 10)) screen = SCR_HOTSEAT;
    Tooltip(hr, "Partie à plusieurs sur le même appareil : la France et jusqu'à trois nations dirigées par d'autres joueurs.");
    if (Button((Rectangle){ bx + 192, y, 88, 38 }, "QUITTER", true, 20)) quitRequested = true;

    const char *dom = "Terre - Mer - Air - Espace - Cyber - Informationnel - Electromagnétique";
    Text(dom, VW / 2 - TextW(dom, 10) / 2, 660, 10, C_DIM);
    Text("Fiction prospective. Contours : Natural Earth (domaine public).", 10, VH - 18, 10, C_DIM);
    if (toastTime > 0) Text(toast, VW / 2 - TextW(toast, 10) / 2, 640, 10, C_SEL);
}

static int helpPage;   /* page de l aide (0 ou 1) */

void DrawHelp(void)
{
    DrawBackdrop();
    Panel((Rectangle){ 90, 20, 1100, 680 }, C_PANEL);
    Text(helpPage ? "COMMENT JOUER (2/2)" : "COMMENT JOUER (1/2)", 110, 32, 20, C_ACCENT);
    static const char *help2[] = {
        "SCÉNARIOS : Janvier 2030 (trente ans à tenir) ou La crise de 2040 (+2°C, invasion boréale des pays baltes, Libye effondrée, dette record, vingt ans à tenir). Choix dans le menu.",
        "HOT-SEAT : jusqu'à trois autres joueurs dirigent chacun une nation (armée, guerre, paix, alliances) sur le même appareil. Chaque trimestre, la France joue, puis chaque commandant (FIN DE PHASE), puis le tour se résout ; un écran de passage cache la carte.",
        "GOUVERNEMENT (POLIT., vue Gouvernement et opinion) : quatre ministres, chacun avec sa compétence et son lobby ; un remaniement coûte 1 PC. Industriels, écologistes, militaires et syndicats jugent votre politique, récompensent ou sanctionnent. La popularité tire la cohésion vers le haut ou vers le bas.",
        "INFLATION ET DETTE : le déficit, les pénuries et la cherté des marchés font monter l'inflation, qui érode la dette mais relève les taux et attise la militance. ROUTES COMMERCIALES : on ne reçoit les excédents d'une nation que par une route terrestre ou maritime ouverte (onglet NATIONS).",
        "ARMÉE : chaque nation puise ses recrues et ses renforts dans une réserve de main-d'oeuvre tirée de sa population. Les unités gagnent de l'expérience au combat (recrues, aguerris, vétérans, élite : jusqu'à +20 %). Missions aériennes (onglet ARMÉE) : appui au sol, supériorité aérienne, bombardement stratégique de l'industrie ennemie.",
        "DIPLOMATIE DE L'IA : les nations amies proposent d'elles-mêmes alliances et accords commerciaux ; une puissance nucléaire hostile peut exiger un tribut, même en temps de paix. L'IA mène ses offensives sur plusieurs fronts à la fois.",
        "ÉVÉNEMENTS : plus de soixante événements ne surviennent que lorsque leurs conditions sont réunies (climat, guerre, régime, dette, factions...) ; chaque option affiche ses effets exacts.",
        "RÉGLAGES (menu) : langue (français, anglais), son, grande interface pour téléphones (F2 : une partie de l'écran à la fois, barre de navigation en bas), rapport de fin de tour.",
    };
    if (helpPage == 1) {
        float y = 62;
        for (int i = 0; i < (int)(sizeof(help2) / sizeof(help2[0])); i++)
            y += TextWrap(help2[i], 110, y, 1060, 10, i % 2 ? C_TEXT : (Color){ 200, 210, 220, 255 }) + 8;
        if (Button((Rectangle){ VW / 2 - 210, 648, 200, 38 }, "< PAGE 1", true, 20)) helpPage = 0;
        if (Button((Rectangle){ VW / 2 + 10, 648, 200, 38 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE)) { helpPage = 0; screen = SCR_TITLE; }
        return;
    }
    static const char *help[] = {
        "OBJECTIF : tenir 120 trimestres (2030-2060) et maximiser le score. Défaite si la Cohésion ou la Souveraineté tombe à 0, en cas de défaut sur la dette, si Paris est occupée ou en cas d'échange nucléaire. Victoires spéciales : Transition, Hégémonie, Utopie, Ordre nouveau.",
        "CARTE : 130 régions (les 13 régions françaises, les Länder, les régions italiennes et espagnoles...). Molette ou +/- pour zoomer, glisser pour déplacer, flèches pour défiler, F pour revenir sur la France. Les noms des régions apparaissent à partir du zoom x2. Rayures : territoire occupé.",
        "TOUR : agissez puis FIN DU TOUR (Espace). Un rapport résume le trimestre. 3 points de commandement (PC) par tour pour la diplomatie, la guerre, les décrets et la lutte hybride.",
        "ARMÉE : 17 types d'unités terrestres, aériennes et navales (drones, robots, missiles hypersoniques, SCAF, porte-avions, drones navals...). Clic gauche : sélectionner (Maj pour ajouter) ; clic droit : déplacer ou attaquer. Blindés, mécanisées et drones avancent de 2 régions par tour ; l'aviation agit à 4 régions et appuie les batailles, la flotte bloque les côtes et protège les débarquements. 10 divisions au plus par région.",
        "ÉTAT-MAJOR : les généraux commandent les divisions d'une région et gagnent de l'expérience ; la recherche militaire (8 domaines) débloque les unités et renforce l'armée.",
        "GUERRE ET PAIX : plusieurs guerres peuvent se mener à la fois ; les alliés sont appelés aux armes. Une région conquise est d'abord occupée ; le traité de paix (onglet NATIONS) fixe les cessions et les réparations dans la limite du score de guerre.",
        "DISSUASION : la posture fixe la crédibilité. D'autres nations peuvent se doter de l'arme (Turquie, Algérie, Égypte, Pologne...) : pression diplomatique, sabotage cyber ou occupation de leur capitale. Armes tactiques au niveau nucléaire 3 ; frapper une puissance nucléaire risque l'escalade.",
        "POPULATION : trois strates avec leur part, leurs besoins, leur militance, leur conscience et leurs propres courants politiques. 32 cultures et 5 religions ; les minorités s'agitent selon la politique culturelle, et une région dont la culture devient majoritairement étrangère peut faire sécession vers sa patrie.",
        "ÉCONOMIE : production, emplois et chômage, stocks, marché mondial avec prix, embargos et blocus. Les autres nations ont un trésor et investissent. Impôts par strate, dépenses publiques, droits de douane, réparations et réquisitions.",
        "POLITIQUE : en démocratie, des élections désignent le gouvernement ; chaque régime a ses mécaniques et deux décrets. Les autres nations ont aussi leurs élections, révolutions et coups d'État.",
        "DÉFENSE HYBRIDE : 13 théâtres M2MC, dont les territoires extérieurs où naissent guérillas et attentats contre l'occupant. OPÉRATIONS (onglet NATIONS) : frappe sur le réseau électrique, vol de technologies, paralysie logistique, ingérence électorale, déstabilisation, reconnaissance satellitaire, brouillage, tir antisatellite ; chacune peut être attribuée à la France.",
        "RESSOURCES ET CLIMAT : terres rares, uranium et semi-conducteurs (gisements, fonderies, importations, crises mondiales) conditionnent les unités modernes, les SMR, la recherche et la dissuasion. La mer monte avec le réchauffement : deltas et côtes basses sont submergés peu à peu.",
        "LOGISTIQUE : une poche coupée de ses dépôts et de son territoire est encerclée et s'use ; fortifications (+15% par niveau) et dépôts se construisent depuis l'onglet PROV. Brouillard de guerre : on ne voit l'ennemi qu'au contact, à portée de l'aviation et des flottes, ou par reconnaissance satellitaire. Groupes d'armées (ÉT.-MAJ.) : tenir un front ou mener une offensive automatiquement. Missions navales : frappe à terre, guerre au commerce, escorte.",
        "MONDE : OTAN (article 5, cohésion), Conseil de sécurité (résolutions, vetos, sanctions, casques bleus), dette publique notée par les marchés (crise de la dette et plan de sauvetage).",
        "RACCOURCIS : Espace/Entrée = fin du tour, 1-9 et 0 = onglets, M = mode de carte, F5 = sauvegarder, F9 = charger, F11 = plein écran, Échap = menu. Sur écran tactile : pincer pour zoomer, bouton ORDRE pour envoyer les unités.",
    };
    float y = 62;
    for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
        y += TextWrap(help[i], 110, y, 1060, 10, i % 2 ? C_TEXT : (Color){ 200, 210, 220, 255 }) + 6;
    if (Button((Rectangle){ VW / 2 - 210, 648, 200, 38 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE))
        screen = SCR_TITLE;
    if (Button((Rectangle){ VW / 2 + 10, 648, 200, 38 }, "PAGE 2 >", true, 20)) helpPage = 1;
}

void DrawEnd(void)
{
    DrawBackdrop();
    bool won = game.state == GAME_WON;
    const char *title = won ? "LA FRANCE A TENU" : "DÉFAITE";
    Text(title, VW / 2 - TextW(title, 60) / 2, 50, 60, won ? C_GOOD : C_BAD);
    const char *why = "";
    switch (game.state) {
    case GAME_WON: why = Tr(VICTORY_DESC[Game_Victory(&game)]); break;
    case GAME_LOST_COHESION: why = "Effondrement social : émeutes, sécessions locales, le pays se fracture."; break;
    case GAME_LOST_SOUVERAINETE: why = "Perte de souveraineté : la France ne décide plus de son destin."; break;
    case GAME_LOST_BUDGET: why = "Défaut de paiement : les créanciers dictent désormais la politique nationale."; break;
    case GAME_LOST_CONQUEST: why = "Paris est tombée. Le gouvernement capitule."; break;
    case GAME_LOST_NUCLEAR: why = "Échange nucléaire. Il n'y a pas de vainqueur."; break;
    default: break;
    }
    char buf[220], date[32];
    if (won) {
        snprintf(buf, sizeof(buf), Tr("Victoire : %s"), Tr(VICTORY_NAMES[Game_Victory(&game)]));
        Text(buf, VW / 2 - TextW(buf, 20) / 2, 122, 20, C_SEL);
    }
    Text(why, VW / 2 - TextW(why, 10) / 2, 150, 10, C_TEXT);

    Game_DateString(game.turn, date, sizeof(date));
    Panel((Rectangle){ 240, 176, 800, 330 }, C_PANEL);
    float x = 260, y = 192;
    snprintf(buf, sizeof(buf), Tr("Fin : %s (+%.2f°C) - régime : %s - difficulté : %s"), date, game.temp,
             Tr(REGIMES[game.nat[N_FRANCE].regime].name), Tr(DIFFICULTY_NAMES[game.difficulty]));
    Text(buf, x, y, 10, C_TEXT); y += 16;
    snprintf(buf, sizeof(buf), Tr("Population : %.1f M dans %d provinces. Conquises : %d, perdues : %d."),
             Soc_NationPop(&game, N_FRANCE), Soc_NationProvinces(&game, N_FRANCE), game.statConquered, game.statLost);
    Text(buf, x, y, 10, C_TEXT); y += 16;
    snprintf(buf, sizeof(buf), Tr("Batailles gagnées : %d, perdues : %d. Menaces neutralisées : %d, subies : %d. Catastrophes : %d."),
             game.statBattlesWon, game.statBattlesLost, game.statNeutralized, game.statSuffered, game.statDisasters);
    Text(buf, x, y, 10, C_TEXT); y += 16;
    if (game.nukesUsed || game.tacticalUsed) {
        snprintf(buf, sizeof(buf), Tr("Armes nucléaires employées : %d stratégique(s), %d tactique(s)."), game.nukesUsed, game.tacticalUsed);
        Text(buf, x, y, 10, C_BAD);
    }
    y += 22;

    int parts[SC_COUNT];
    int score = Game_Score(&game, parts);
    Text("DÉTAIL DU SCORE", x, y, 10, C_ACCENT);
    y += 16;
    int maxPart = 1;
    for (int i = 0; i < SC_COUNT; i++) if (abs(parts[i]) > maxPart) maxPart = abs(parts[i]);
    for (int i = 0; i < SC_COUNT; i++) {
        Text(Tr(SCORE_PART_NAMES[i]), x, y, 10, C_DIM);
        Bar((Rectangle){ x + 120, y + 1, 500, 8 }, (float)abs(parts[i]), (float)maxPart, parts[i] < 0 ? C_BAD : C_ACCENT);
        snprintf(buf, sizeof(buf), "%d", parts[i]);
        Text(buf, x + 630, y, 10, parts[i] < 0 ? C_BAD : C_TEXT);
        y += 14;
    }
    y += 8;
    snprintf(buf, sizeof(buf), Tr("SCORE FINAL : %d"), score);
    Text(buf, 640 - TextW(buf, 20) / 2, y, 20, C_SEL);

    float by = 530;
    for (int n = 1; n < N_COUNT; n++) {
        if (!game.human[n]) continue;
        snprintf(buf, sizeof(buf), Tr("Commandant %s : %s, %d province(s) (%d au départ), %.1f M hab."), Tr(NATIONS[n].name),
                 game.nat[n].alive ? "survit" : "anéanti", Soc_NationProvinces(&game, n), game.nat[n].startProvinces, Soc_NationPop(&game, n));
        Text(buf, VW / 2 - TextW(buf, 10) / 2, by - 18, 10, NationColor(n));
        by += 14;
    }
    if (Button((Rectangle){ VW / 2 - 140, by, 280, 40 }, "REJOUER", true, 20)) NewGame();
    if (Button((Rectangle){ VW / 2 - 140, by + 50, 280, 40 }, "MENU", true, 20)) screen = SCR_TITLE;
}

/* ------------------------------------------------------------------ */
/* Tests d'interface                                                   */
/* ------------------------------------------------------------------ */
/*
 * bouclier2050 --ui-test DOSSIER : rend une suite d'ecrans scriptes avec une
 * graine fixe, animations figees et souris hors champ, et ecrit le rendu
 * logique 1280x720 de chacun dans DOSSIER. tools/ui_compare.c les compare a
 * des images de reference (tests/ui/).
 */
const char *uiTestDir;

typedef struct { const char *name; void (*setup)(void); } UiShot;

static void ShotTitle(void)    { screen = SCR_TITLE; }
static void ShotMap(void)
{
    NewGame();
    tutorial = -1;
    for (int i = 0; i < 3; i++) {
        if (game.pendingDilemma >= 0) Do(CMD_DILEMMA, 0, 0, 0, 0);
        Do(CMD_END_TURN, 0, 0, 0, 0);
    }
    game.pendingDilemma = -1;
    game.reportReady = false;
    tab = TAB_PROVINCE;
}
static void ShotArmy(void)     { tab = TAB_ARMEE; }
static void ShotNations(void)  { tab = TAB_NATIONS; selNation = N_ALLEMAGNE; }
static void ShotEconomy(void)  { tab = TAB_ECONOMIE; }
static void ShotCabinet(void)  { tab = TAB_POLITIQUE; polView = 1; }
static void ShotEvent(void)    { polView = 0; game.pendingDilemma = DLM_EVENT; }
static void ShotReport(void)   { game.pendingDilemma = -1; game.reportReady = true; }
static void ShotHotseat(void)  { game.reportReady = false; hotseatPick[N_ALLEMAGNE] = true; screen = SCR_HOTSEAT; }
static void ShotEnglish(void)  { hotseatPick[N_ALLEMAGNE] = false; gLang = LANG_EN; screen = SCR_GAME; tab = TAB_DEFENSE; }
static void ShotHelp(void)     { gLang = LANG_FR; screen = SCR_HELP; helpPage = 1; }

static const UiShot UI_SHOTS[] = {
    { "01_titre", ShotTitle }, { "02_carte", ShotMap }, { "03_armee", ShotArmy }, { "04_nations", ShotNations },
    { "05_economie", ShotEconomy }, { "06_gouvernement", ShotCabinet }, { "07_evenement", ShotEvent },
    { "08_rapport", ShotReport }, { "09_hotseat", ShotHotseat }, { "10_anglais", ShotEnglish }, { "11_aide", ShotHelp },
};
#define UI_SHOT_COUNT ((int)(sizeof(UI_SHOTS) / sizeof(UI_SHOTS[0])))

/* appelee apres chaque image : prepare un ecran, le laisse se stabiliser, puis l'enregistre */
static bool UiTestFrame(void)
{
    static int frame;
    int step = frame / 4, phase = frame % 4;
    frame++;
    if (step >= UI_SHOT_COUNT) return false;
    if (phase == 0) UI_SHOTS[step].setup();
    if (phase == 3) {
        Image im = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&im);
        ExportImage(im, TextFormat("%s/%s.png", uiTestDir, UI_SHOTS[step].name));
        UnloadImage(im);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Boucle principale                                                   */
/* ------------------------------------------------------------------ */
/* region affichee : toute la scene, ou en grande interface la partie choisie (les fenetres modales d'abord) */
Rectangle CurrentRegion(void)
{
    if (!bigUi) return REGIONS[VR_ALL];
    if (screen == SCR_TITLE) return (Rectangle){ 320, 40, 640, 660 };
    if (screen == SCR_HOTSEAT) return (Rectangle){ 180, 40, 920, 640 };
    if (screen != SCR_GAME) return REGIONS[VR_ALL];
    if (handoffFor >= 0) return REGIONS[VR_MODAL];
    bool modal = game.pendingDilemma >= 0 || treatyOpen || (game.reportReady && showReports && game.state == GAME_RUNNING);
    return REGIONS[modal ? VR_MODAL : region];
}

void UpdateView(void)
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    navH = bigUi ? fmaxf(56, sh * 0.1f) : 0;
    viewSrc = CurrentRegion();
    float s = fminf(sw / viewSrc.width, (sh - navH) / viewSrc.height);
    if (s != viewScale) {
        bool integer = fabsf(s - roundf(s)) < 0.01f;
        SetTextureFilter(target.texture, integer ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    }
    viewScale = s;
    viewOffset = (Vector2){ (sw - viewSrc.width * s) / 2, (sh - navH - viewSrc.height * s) / 2 };
    Vector2 m = GetMousePosition();
    mouse = (Vector2){ viewSrc.x + (m.x - viewOffset.x) / s, viewSrc.y + (m.y - viewOffset.y) / s };
    clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    rclicked = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    /* un toucher sur la barre de navigation ne traverse pas jusqu'au jeu */
    if (bigUi && m.y >= sh - navH) { clicked = rclicked = false; mouse = (Vector2){ -1000, -1000 }; }
}

/* barre de navigation de la grande interface, en pixels d'ecran */
void DrawNavBar(void)
{
    if (!bigUi) return;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    DrawRectangle(0, (int)(sh - navH), (int)sw, (int)navH, C_PANEL);
    DrawLine(0, (int)(sh - navH), (int)sw, (int)(sh - navH), C_BORDER);
    static const ViewRegion order[] = { VR_MAP, VR_PANEL, VR_LOWER, VR_LOG, VR_TOPBAR, VR_ALL };
    int count = screen == SCR_GAME ? 6 : 0;
    float bw = sw / (count + 1);
    int fs = (int)(navH * 0.32f);
    Vector2 m = GetMousePosition();
    bool press = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.y >= sh - navH;
    for (int i = 0; i <= count; i++) {
        Rectangle r = { i * bw + 4, sh - navH + 6, bw - 8, navH - 12 };
        const char *label = i < count ? REGION_NAMES[order[i]] : "Normal";
        bool sel = i < count && region == order[i];
        DrawRectangleRec(r, sel ? (Color){ 44, 66, 96, 255 } : C_PANEL2);
        DrawRectangleLinesEx(r, 2, sel ? C_ACCENT : C_BORDER);
        int tw = MeasureText(label, fs);
        DrawText(label, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - fs) / 2), fs, sel ? C_TEXT : C_DIM);
        if (press && CheckCollisionPointRec(m, r)) {
            if (i < count) region = order[i];
            else bigUi = false;
        }
    }
}

#if defined(PLATFORM_ANDROID)
#include <sys/stat.h>
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--ui-test") == 0) uiTestDir = argv[i + 1];
#if defined(PLATFORM_ANDROID)
    /* sauvegardes dans le dossier prive de l'application */
    mkdir("/data/data/fr.bouclier2050.game/files", 0700);
    if (chdir("/data/data/fr.bouclier2050.game/files") != 0) { /* on reste dans le dossier courant */ }
#endif
    if (!uiTestDir) SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(VW, VH, "Bouclier 2050");
    if (!IsWindowReady()) { fprintf(stderr, "Impossible d'ouvrir la fenêtre (pas d'écran ?)\n"); return 1; }
    if (!uiTestDir) Snd_Init();
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
#if defined(PLATFORM_ANDROID)
    bigUi = true;   /* sur telephone, la grande interface par defaut */
#endif
    if (!uiTestDir) LoadSettings();
    viewScale = 0;
    Image img = GenImageColor(MAP_VW, MAP_VH, C_SEA);
    mapTex = LoadTextureFromImage(img);
    UnloadImage(img);
    Game_Init(&game, (unsigned)time(NULL), DIFF_NORMAL);

    while (!WindowShouldClose() && !quitRequested) {
        float dt = GetFrameTime();
        timeAcc += dt;
        if (toastTime > 0) toastTime -= dt;
        UpdateView();
        if (uiTestDir) {   /* rendu deterministe : pas d'animation, souris hors champ */
            timeAcc = 0;
            mouse = (Vector2){ -1000, -1000 };
            clicked = rclicked = false;
        }
        Snd_Update(screen == SCR_GAME);
        tooltip[0] = 0;
        inputLayer = activeLayer = 0;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F2)) { bigUi = !bigUi; SaveSettings(); }

        BeginTextureMode(target);
        switch (screen) {
        case SCR_TITLE: DrawTitle(); break;
        case SCR_HELP:  DrawHelp(); break;
        case SCR_GAME:  DrawGame(); break;
        case SCR_END:   DrawEnd(); break;
        case SCR_HOTSEAT: DrawHotseatSetup(); break;
        }
        DrawTooltip();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        /* la texture de rendu est inversee verticalement */
        DrawTexturePro(target.texture, (Rectangle){ viewSrc.x, VH - viewSrc.y - viewSrc.height, viewSrc.width, -viewSrc.height },
                       (Rectangle){ viewOffset.x, viewOffset.y, viewSrc.width * viewScale, viewSrc.height * viewScale },
                       (Vector2){ 0, 0 }, 0, WHITE);
        DrawNavBar();
        EndDrawing();
        if (uiTestDir && !UiTestFrame()) break;
    }
    UnloadTexture(mapTex);
    UnloadRenderTexture(target);
    Snd_Close();
    CloseWindow();
    return 0;
}
