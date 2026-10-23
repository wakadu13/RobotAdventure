/*
 * BOUCLIER 2050 - succes : objectifs debloques au fil des parties, conserves dans un fichier.
 */
#include "ui.h"

typedef struct { const char *name, *desc; } Achievement;

static const Achievement ACH[ACH_COUNT] = {
    { "Ils ont tenu", "Terminer une partie par une victoire." },
    { "Sous le feu", "Gagner en difficulté Difficile." },
    { "Transition", "Remporter la victoire de la transition." },
    { "Hégémonie", "Remporter la victoire de l'hégémonie." },
    { "Utopie", "Remporter la victoire de l'utopie." },
    { "L'ordre règne", "Remporter la victoire de l'ordre." },
    { "La crise surmontée", "Gagner le scénario « La crise de 2040 »." },
    { "Paix dans les Balkans", "Gagner le scénario « La guerre des Balkans (2035) »." },
    { "Au-delà du Sahel", "Gagner le scénario « L'effondrement du Sahel (2050) »." },
    { "Pas un pouce", "Gagner sans perdre une seule région." },
    { "Main tendue", "Gagner sans avoir livré une seule bataille." },
    { "Démocratie jusqu'au bout", "Gagner en étant toujours une démocratie libérale." },
    { "Sans la bombe", "Gagner sans employer d'arme nucléaire, en ayant connu au moins une guerre." },
    { "Bouclier hybride", "Neutraliser 40 menaces hybrides dans une même partie." },
    { "Conquérant", "Conquérir 5 régions dans une même partie." },
    { "Chroniqueur", "Vivre 40 événements dans une même partie." },
    { "Climat contenu", "Finir la partie sous +2,4 °C." },
    { "Grand stratège", "Atteindre un score de 2500." },
};

bool achUnlocked[ACH_COUNT];
static bool achLoaded;

/* lang:ignore-begin */
static const char *AchFile(void) { return "bouclier2050_succes.txt"; }

static void AchLoad(void)
{
    achLoaded = true;
    if (uiTestDir) return;   /* tests d'interface : rien de persistant */
    FILE *f = fopen(AchFile(), "r");
    if (!f) return;
    int id;
    while (fscanf(f, "%d", &id) == 1) if (id >= 0 && id < ACH_COUNT) achUnlocked[id] = true;
    fclose(f);
}

static void AchSave(void)
{
    if (uiTestDir) return;
    FILE *f = fopen(AchFile(), "w");
    if (!f) return;
    for (int i = 0; i < ACH_COUNT; i++) if (achUnlocked[i]) fprintf(f, "%d\n", i);
    fclose(f);
}
/* lang:ignore-end */

static void Unlock(int id)
{
    if (achUnlocked[id]) return;
    achUnlocked[id] = true;
    AchSave();
    char buf[160];
    snprintf(buf, sizeof(buf), Tr("Succès débloqué : %s"), Tr(ACH[id].name));
    Toast(buf);
    Snd_Play(SFX_GOOD);
}

int AchCount(void)
{
    if (!achLoaded) AchLoad();
    int n = 0;
    for (int i = 0; i < ACH_COUNT; i++) n += achUnlocked[i];
    return n;
}

/* apres chaque tour de la France et en fin de partie ; le multijoueur compte aussi */
void AchCheck(void)
{
    if (!achLoaded) AchLoad();
    if (!inProgress && game.state == GAME_RUNNING) return;
    if (netPlaying && localNation != N_FRANCE) return;   /* les succes suivent la France */
    int events = 0;
    for (int k = 0; k < EVENT_COUNT && k < EVENT_MAX; k++) events += game.eventUsed[k];
    if (game.statNeutralized >= 40) Unlock(ACH_HYBRID);
    if (game.statConquered >= 5) Unlock(ACH_CONQUEROR);
    if (events >= 40) Unlock(ACH_CHRONICLER);
    if (game.state == GAME_RUNNING) return;
    int score = Game_Score(&game, NULL);
    if (score >= 2500) Unlock(ACH_SCORE);
    if (game.state != GAME_WON) return;
    Unlock(ACH_WIN);
    if (game.difficulty == DIFF_DIFFICILE) Unlock(ACH_HARD);
    switch (Game_Victory(&game)) {
    case VIC_TRANSITION: Unlock(ACH_TRANSITION); break;
    case VIC_HEGEMONIE:  Unlock(ACH_HEGEMONY); break;
    case VIC_UTOPIE:     Unlock(ACH_UTOPIA); break;
    case VIC_ORDRE:      Unlock(ACH_ORDER); break;
    default: break;
    }
    if (game.scenario == 1) Unlock(ACH_SC2040);
    if (game.scenario == 2) Unlock(ACH_BALKANS);
    if (game.scenario == 3) Unlock(ACH_SAHEL);
    if (game.statLost == 0) Unlock(ACH_NO_LOSS);
    if (game.statBattlesWon + game.statBattlesLost == 0) Unlock(ACH_PEACE);
    if (!game.leftDemocracy) Unlock(ACH_DEMOCRACY);
    if (game.nukesUsed + game.tacticalUsed == 0 && game.everAtWar) Unlock(ACH_NO_NUKE);
    if (game.temp < 2.4f) Unlock(ACH_CLIMATE);
}

void DrawAchievements(void)
{
    if (!achLoaded) AchLoad();
    DrawBackdrop();
    Rectangle pr = { 140, 30, 1000, 660 };
    Panel(pr, C_PANEL);
    DrawRectangle((int)pr.x, (int)pr.y, (int)pr.width, 4, C_ACCENT);
    Text("SUCCÈS", pr.x + 24, pr.y + 18, 20, C_TEXT);
    char buf[96];
    snprintf(buf, sizeof(buf), Tr("%d sur %d débloqués"), AchCount(), ACH_COUNT);
    Text(buf, pr.x + 140, pr.y + 24, 10, C_SEL);
    for (int i = 0; i < ACH_COUNT; i++) {
        int col = i % 2, row = i / 2;
        Rectangle r = { pr.x + 20 + col * 484, pr.y + 54 + row * 62, 476, 56 };
        bool on = achUnlocked[i];
        Panel(r, on ? (Color){ 30, 48, 40, 255 } : C_BG);
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, on ? C_GOOD : C_BORDER);
        Text(ACH[i].name, r.x + 14, r.y + 8, 20, on ? C_TEXT : C_DIM);
        Text(ACH[i].desc, r.x + 14, r.y + 34, 10, on ? C_TEXT : C_DIM);
        if (on) Text("OK", r.x + r.width - 34, r.y + 8, 20, C_GOOD);
    }
    if (Button((Rectangle){ pr.x + pr.width - 224, pr.y + pr.height - 50, 200, 38 }, "RETOUR", true, 20) || IsKeyPressed(KEY_ESCAPE))
        screen = SCR_TITLE;
    if (toastTime > 0) Text(toast, pr.x + 24, pr.y + pr.height - 36, 10, C_SEL);
}
