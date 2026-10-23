/*
 * BOUCLIER 2050 - options (accessibilite, raccourcis clavier) et emplacements de sauvegarde.
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Accessibilite                                                       */
/* ------------------------------------------------------------------ */
bool  colorBlind, bigTooltips;
Color uiGood = { 92, 200, 120, 255 }, uiBad = { 232, 84, 72, 255 }, uiWarn = { 240, 170, 60, 255 };

void SetColorBlind(bool on)
{
    colorBlind = on;
    /* palette d'Okabe et Ito : bleu ciel, vermillon et jaune se distinguent sans le rouge-vert */
    uiGood = on ? (Color){ 86, 180, 233, 255 } : (Color){ 92, 200, 120, 255 };
    uiBad  = on ? (Color){ 230, 110, 20, 255 } : (Color){ 232, 84, 72, 255 };
    uiWarn = on ? (Color){ 240, 228, 66, 255 } : (Color){ 240, 170, 60, 255 };
    mapSignature = 0;
}

static Color Tint(Color base, float lum)
{
    float baseLum = (base.r * 0.3f + base.g * 0.59f + base.b * 0.11f) / 255;
    float k = Clampf2(lum / baseLum, 0.3f, 1.6f);
    return (Color){ (unsigned char)fminf(255, base.r * k), (unsigned char)fminf(255, base.g * k), (unsigned char)fminf(255, base.b * k), 255 };
}

/* degrades de la carte : verts -> vert bleute, rouges -> vermillon, a luminosite egale (le bleu reste a la France) */
Color CbColor(Color c)
{
    if (!colorBlind) return c;
    float lum = (c.r * 0.3f + c.g * 0.59f + c.b * 0.11f) / 255;
    if (c.g > c.r + 30 && c.g >= c.b) return Tint((Color){ 0, 158, 115, 255 }, lum);
    if (c.r > c.g + 50 && c.r > c.b + 30) return Tint((Color){ 213, 94, 0, 255 }, lum);
    return c;
}

/* ------------------------------------------------------------------ */
/* Raccourcis clavier                                                  */
/* ------------------------------------------------------------------ */
typedef struct { const char *label; int key; } KeyAction;
static const KeyAction KEY_ACTIONS[KA_COUNT] = {
    { "Fin du tour", KEY_SPACE },
    { "Mode de carte suivant", KEY_M },
    { "Courbes de la partie", KEY_G },
    { "Centrer sur la France", KEY_F },
    { "Vue d'ensemble", KEY_HOME },
    { "Sauvegarde rapide", KEY_F5 },
    { "Chargement rapide", KEY_F9 },
    { "Enregistrer le rejeu", KEY_F6 },
    { "Plein écran", KEY_F11 },
    { "Grande interface", KEY_F2 },
};
/* lang:ignore-begin */
static const char *KEY_IDS[KA_COUNT] = {
    "fin_tour", "mode_carte", "courbes", "centrer", "vue_ensemble", "sauver", "charger", "rejeu", "plein_ecran", "grande_interface"
};
/* lang:ignore-end */
int keyBind[KA_COUNT];

void ResetKeys(void)
{
    for (int i = 0; i < KA_COUNT; i++) keyBind[i] = KEY_ACTIONS[i].key;
}

bool KeyHit(int action) { return keyBind[action] > 0 && IsKeyPressed(keyBind[action]); }

const char *KeyName(int key)
{
    static char buf[24];
    if (key >= KEY_A && key <= KEY_Z) { snprintf(buf, sizeof(buf), "%c", 'A' + key - KEY_A); return buf; }
    if (key >= KEY_ZERO && key <= KEY_NINE) { snprintf(buf, sizeof(buf), "%c", '0' + key - KEY_ZERO); return buf; }
    if (key >= KEY_F1 && key <= KEY_F12) { snprintf(buf, sizeof(buf), "F%d", 1 + key - KEY_F1); return buf; }
    switch (key) {
    case KEY_SPACE:     return Tr("Espace");
    case KEY_HOME:      return Tr("Origine");
    case KEY_END:       return Tr("Fin");
    case KEY_TAB:       return "Tab";
    case KEY_INSERT:    return Tr("Inser");
    case KEY_DELETE:    return Tr("Suppr");
    case KEY_PAGE_UP:   return Tr("Page préc.");
    case KEY_PAGE_DOWN: return Tr("Page suiv.");
    case 0:             return "-";
    default: snprintf(buf, sizeof(buf), Tr("Touche %d"), key); return buf;
    }
}

/* lang:ignore-begin */
/* lecture et ecriture des reglages propres a ce module (fichier de reglages, cle=valeur) */
bool OptionsSetting(const char *name, int value)
{
    if (strcmp(name, "daltonien") == 0) { SetColorBlind(value != 0); return true; }
    if (strcmp(name, "grandes_infobulles") == 0) { bigTooltips = value != 0; return true; }
    for (int i = 0; i < KA_COUNT; i++) {
        char id[48];
        snprintf(id, sizeof(id), "touche_%s", KEY_IDS[i]);
        if (strcmp(name, id) == 0) { keyBind[i] = value; return true; }
    }
    return false;
}

void OptionsWrite(FILE *f)
{
    fprintf(f, "daltonien=%d\ngrandes_infobulles=%d\n", colorBlind ? 1 : 0, bigTooltips ? 1 : 0);
    for (int i = 0; i < KA_COUNT; i++) fprintf(f, "touche_%s=%d\n", KEY_IDS[i], keyBind[i]);
}

/* lang:ignore-end */

static int waitingKey = -1;   /* action en attente d'une touche */

static bool Toggle(Rectangle r, const char *label, bool on, const char *tip)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s : %s", Tr(label), Tr(on ? "oui" : "non"));
    Tooltip(r, tip);
    return Button(r, buf, true, 10);
}

void DrawOptions(void)
{
    DrawBackdrop();
    Rectangle pr = { 190, 40, 900, 640 };
    Panel(pr, C_PANEL);
    DrawRectangle((int)pr.x, (int)pr.y, (int)pr.width, 4, C_ACCENT);
    Text("OPTIONS", pr.x + 24, pr.y + 18, 20, C_TEXT);

    float x = pr.x + 24, y = pr.y + 60;
    Text("AFFICHAGE ET ACCESSIBILITÉ", x, y, 10, C_ACCENT);
    y += 18;
    Rectangle r = { x, y, 380, 30 };
    if (Button(r, LANG_LABELS[gLang], true, 10)) { gLang = (gLang + 1) % LANG_COUNT; mapSignature = 0; SaveSettings(); }
    y += 36;
    if (Toggle((Rectangle){ x, y, 380, 30 }, "Mode daltonien", colorBlind,
               "Remplace le rouge et le vert des jauges, des alertes et des cartes par du vermillon, du bleu et du vert bleuté, "
               "distinguables quelle que soit la perception des couleurs.")) { SetColorBlind(!colorBlind); SaveSettings(); }
    y += 36;
    if (Toggle((Rectangle){ x, y, 380, 30 }, "Infobulles agrandies", bigTooltips, "Texte des infobulles en taille double."))
        { bigTooltips = !bigTooltips; SaveSettings(); }
    y += 36;
    if (Toggle((Rectangle){ x, y, 380, 30 }, "Grande interface", bigUi,
               "Une partie de l'écran à la fois, agrandie, avec une barre de navigation : pour les téléphones et les petits écrans."))
        { bigUi = !bigUi; SaveSettings(); }
    y += 36;
    if (Toggle((Rectangle){ x, y, 380, 30 }, "Rapport à chaque tour", showReports, "Affiche le rapport du trimestre après chaque fin de tour."))
        { showReports = !showReports; SaveSettings(); }
    y += 36;
    if (Toggle((Rectangle){ x, y, 380, 30 }, "Son", Snd_Enabled(), "Effets sonores et nappe d'ambiance, synthétisés par le jeu (aucun fichier audio)."))
        { Snd_SetEnabled(!Snd_Enabled()); SaveSettings(); }

    /* raccourcis */
    float kx = pr.x + 460, ky = pr.y + 60;
    Text("RACCOURCIS CLAVIER", kx, ky, 10, C_ACCENT);
    Text("Cliquez sur une touche puis appuyez sur la nouvelle (Échap : annuler).", kx, ky + 14, 10, C_DIM);
    ky += 34;
    for (int i = 0; i < KA_COUNT; i++) {
        Text(KEY_ACTIONS[i].label, kx, ky + 7, 10, C_TEXT);
        Rectangle kr = { kx + 250, ky, 150, 24 };
        bool dup = false;
        for (int j = 0; j < KA_COUNT; j++) if (j != i && keyBind[j] == keyBind[i] && keyBind[i] > 0) dup = true;
        if (Button(kr, waitingKey == i ? "..." : KeyName(keyBind[i]), true, 10)) waitingKey = i;
        if (dup) {
            Text("!", kr.x + kr.width + 6, ky + 7, 10, C_BAD);
            Tooltip((Rectangle){ kr.x + kr.width, ky, 16, 24 }, "Touche déjà utilisée par une autre action.");
        }
        ky += 28;
    }
    if (waitingKey >= 0) {
        int k = GetKeyPressed();
        if (k == KEY_ESCAPE) waitingKey = -1;
        else if (k > 0) { keyBind[waitingKey] = k; waitingKey = -1; SaveSettings(); }
    }
    Text("Fixes : Entrée = fin du tour, 1-9 et 0 = onglets, flèches et +/- = carte.", kx, ky + 8, 10, C_DIM);
    if (Button((Rectangle){ kx, ky + 28, 250, 28 }, "Touches par défaut", true, 10)) { ResetKeys(); waitingKey = -1; SaveSettings(); }

    if (Button((Rectangle){ pr.x + pr.width - 224, pr.y + pr.height - 54, 200, 38 }, "RETOUR", true, 20)
        || (waitingKey < 0 && IsKeyPressed(KEY_ESCAPE))) {
        waitingKey = -1;
        screen = SCR_TITLE;
    }
}

/* ------------------------------------------------------------------ */
/* Emplacements de sauvegarde                                          */
/* ------------------------------------------------------------------ */
#define SLOT_COUNT 8          /* 0 : rapide (F5), 1 : automatique, 2-7 : nommes */

/* lang:ignore-begin */
/* en test d'interface, les emplacements vivent dans le dossier des captures (fichiers .tmp) */
static const char *NamesFile(void)
{
    static char path[512];
    if (!uiTestDir) return "bouclier2050_noms.txt";
    snprintf(path, sizeof(path), "%s/noms.tmp", uiTestDir);
    return path;
}

typedef struct { bool exists; char name[40]; char info[128]; } SlotInfo;
static SlotInfo slots[SLOT_COUNT];
static bool slotsDirty = true;
static int  editing = -1;    /* emplacement dont on edite le nom */
static int  confirmDelete = -1;

const char *SlotPath(int s)
{
    static char path[512];
    if (uiTestDir) { snprintf(path, sizeof(path), "%s/slot_%d.tmp", uiTestDir, s); return path; }
    if (s == 0) return SAVE_FILE;
    if (s == 1) return AUTOSAVE_FILE;
    snprintf(path, sizeof(path), "bouclier2050_%d.sav", s - 1);
    return path;
}

static void SaveNames(void)
{
    FILE *f = fopen(NamesFile(), "w");
    if (!f) return;
    for (int s = 2; s < SLOT_COUNT; s++) if (slots[s].name[0]) fprintf(f, "%d=%s\n", s, slots[s].name);
    fclose(f);
}

/* lang:ignore-end */

static void RefreshSlots(void)
{
    static Game tmp;   /* lecture des resumes : grosse structure, hors de la pile */
    for (int s = 0; s < SLOT_COUNT; s++) { slots[s].exists = false; slots[s].name[0] = 0; slots[s].info[0] = 0; }
    FILE *f = fopen(NamesFile(), "r");
    if (f) {
        char line[96];
        while (fgets(line, sizeof(line), f)) {
            int s = atoi(line);
            char *eq = strchr(line, '=');
            if (s < 2 || s >= SLOT_COUNT || !eq) continue;
            eq[strcspn(eq, "\r\n")] = 0;
            snprintf(slots[s].name, sizeof(slots[s].name), "%s", eq + 1);
        }
        fclose(f);
    }
    for (int s = 0; s < SLOT_COUNT; s++) {
        const char *p = SlotPath(s);
        if (!FileExists(p)) continue;
        slots[s].exists = true;
        if (!Game_Load(&tmp, p)) { snprintf(slots[s].info, sizeof(slots[s].info), "%s", Tr("Sauvegarde illisible ou d'une autre version.")); continue; }
        char date[32], when[32] = "";
        Game_DateString(tmp.turn, date, sizeof(date));
        time_t t = (time_t)GetFileModTime(p);
        struct tm *lt = localtime(&t);
        if (lt && !uiTestDir) strftime(when, sizeof(when), "%d/%m/%Y %H:%M", lt);
        else snprintf(when, sizeof(when), "--");
        snprintf(slots[s].info, sizeof(slots[s].info), Tr("%s - %s - %s - score %d - enregistrée le %s"), date,
                 Tr(SCENARIO_NAMES[tmp.scenario >= 0 && tmp.scenario < SCENARIO_COUNT ? tmp.scenario : 0]),
                 Tr(DIFFICULTY_NAMES[tmp.difficulty]), Game_Score(&tmp, NULL), when);
    }
    slotsDirty = false;
}

/* lang:ignore-begin */
/* test d'interface : un emplacement nomme rempli, les autres vides */
void SavesTestSetup(bool fill)
{
    for (int s = 0; s < SLOT_COUNT; s++) remove(SlotPath(s));
    remove(NamesFile());
    if (fill) {
        Game_Save(&game, SlotPath(2));
        FILE *f = fopen(NamesFile(), "w");
        if (f) { fprintf(f, "2=Partie de test\n"); fclose(f); }
    }
    slotsDirty = true;
}

/* lang:ignore-end */

void OpenSaves(void)
{
    slotsDirty = true;
    editing = confirmDelete = -1;
    screen = SCR_SAVES;
}

/* saisie du nom : Latin-1 seulement (la police n'a pas d'autres glyphes) */
static void EditName(char *name, int cap)
{
    int c;
    while ((c = GetCharPressed()) > 0) {
        int len = (int)strlen(name), sz = 0;
        if (c < 32 || c > 255 || (c >= 127 && c < 160)) continue;
        const char *u = CodepointToUTF8(c, &sz);
        if (len + sz < cap) { memcpy(name + len, u, (size_t)sz); name[len + sz] = 0; }
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        int len = (int)strlen(name);
        while (len > 0 && (name[len - 1] & 0xC0) == 0x80) len--;   /* octets de continuation UTF-8 */
        if (len > 0) len--;
        name[len] = 0;
    }
}

void DrawSaves(void)
{
    if (slotsDirty) RefreshSlots();
    DrawBackdrop();
    Rectangle pr = { 140, 40, 1000, 640 };
    Panel(pr, C_PANEL);
    DrawRectangle((int)pr.x, (int)pr.y, (int)pr.width, 4, C_ACCENT);
    Text("PARTIES SAUVEGARDÉES", pr.x + 24, pr.y + 18, 20, C_TEXT);
    Text("Emplacement rapide (F5 / F9), automatique (tous les 4 tours) et six emplacements nommés.", pr.x + 24, pr.y + 44, 10, C_DIM);

    bool canSave = inProgress && !netPlaying && game.state == GAME_RUNNING;
    float y = pr.y + 70;
    for (int s = 0; s < SLOT_COUNT; s++) {
        Rectangle row = { pr.x + 20, y, pr.width - 40, 56 };
        Panel(row, s == editing ? C_PANEL2 : C_BG);
        char title[96];
        if (s == 0) snprintf(title, sizeof(title), "%s", Tr("Sauvegarde rapide"));
        else if (s == 1) snprintf(title, sizeof(title), "%s", Tr("Sauvegarde automatique"));
        else if (s == editing) snprintf(title, sizeof(title), "%s%s", slots[s].name, fmodf(GetTime(), 1) < 0.5f ? "_" : "");
        else if (slots[s].name[0]) snprintf(title, sizeof(title), "%s", slots[s].name);
        else snprintf(title, sizeof(title), Tr("Emplacement %d"), s - 1);
        DrawTextEx(GetFontDefault(), title, (Vector2){ row.x + 12, row.y + 10 }, 20, 2, s == editing ? C_SEL : C_TEXT);
        Text(slots[s].exists ? slots[s].info : "Vide", row.x + 12, row.y + 36, 10, slots[s].exists ? C_DIM : C_BORDER);

        float bx = row.x + row.width - 12;
        if (s >= 2) {
            bx -= 90;
            if (s == editing) {
                if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "VALIDER", true, 10) || IsKeyPressed(KEY_ENTER)) { editing = -1; SaveNames(); }
            } else if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "RENOMMER", editing < 0, 10)) editing = s;
            bx -= 90;
            if (confirmDelete == s) {
                if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "CONFIRMER", true, 10)) {
                    remove(SlotPath(s));
                    slots[s].name[0] = 0;
                    SaveNames();
                    confirmDelete = -1;
                    slotsDirty = true;
                }
            } else if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "SUPPRIMER", slots[s].exists && editing < 0, 10)) confirmDelete = s;
        }
        bx -= 90;
        if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "CHARGER", slots[s].exists && !netPlaying && editing < 0, 10)) {
            LoadGame(SlotPath(s));   /* passe a l'ecran de jeu si la lecture reussit */
            return;
        }
        if (s != 1) {
            bx -= 90;
            if (Button((Rectangle){ bx, row.y + 12, 84, 30 }, "SAUVER", canSave && editing < 0, 10)) {
                bool ok = Game_Save(&game, SlotPath(s));
                Toast(ok ? "Partie sauvegardée." : "Échec de la sauvegarde.");
                if (ok && s >= 2 && !slots[s].name[0]) { editing = s; }
                slotsDirty = true;
            }
        }
        y += 62;
    }
    if (editing >= 0) EditName(slots[editing].name, (int)sizeof(slots[editing].name));
    if (slotsDirty && editing >= 0) {   /* garder le nom en cours de saisie */
        char keep[40];
        int e = editing;
        snprintf(keep, sizeof(keep), "%s", slots[e].name);
        RefreshSlots();
        snprintf(slots[e].name, sizeof(slots[e].name), "%s", keep);
    }
    if (toastTime > 0) Text(toast, pr.x + 24, pr.y + pr.height - 44, 10, C_SEL);
    if (Button((Rectangle){ pr.x + pr.width - 224, pr.y + pr.height - 54, 200, 38 }, "RETOUR", editing < 0, 20)
        || (editing < 0 && IsKeyPressed(KEY_ESCAPE)))
        screen = SCR_TITLE;
}
