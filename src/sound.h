/*
 * BOUCLIER 2050 - sons synthetises au lancement (aucun fichier audio).
 */
#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

typedef enum {
    SFX_CLICK,      /* bouton */
    SFX_TURN,       /* fin du tour */
    SFX_ALERT,      /* dilemme ou evenement */
    SFX_WAR,        /* declaration de guerre, bataille */
    SFX_GOOD,       /* bonne nouvelle */
    SFX_BAD,        /* mauvaise nouvelle */
    SFX_NUKE,       /* frappe nucleaire */
    SFX_VICTORY,
    SFX_DEFEAT,
    SFX_COUNT
} Sfx;

void Snd_Init(void);
void Snd_Close(void);
void Snd_Play(Sfx s);
void Snd_Update(bool ambient);   /* nappe d'ambiance en boucle */
void Snd_SetEnabled(bool on);
bool Snd_Enabled(void);

#endif
