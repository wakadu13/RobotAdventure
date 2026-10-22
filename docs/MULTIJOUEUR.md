# Vers le multijoueur

Ce document décrit ce qui est déjà en place pour un futur mode multijoueur et ce qu'il reste à faire.

## Principe retenu : le lockstep déterministe

La simulation (`src/*.c` hors `main.c`) n'a aucune dépendance graphique ni réseau et elle est
**déterministe** : tout le hasard vient d'un générateur pseudo-aléatoire stocké dans l'état de jeu
(`Game.rng`). Deux machines qui partent de la même graine et appliquent les mêmes actions dans le
même ordre obtiennent exactement le même état.

Plutôt que d'échanger l'état complet (plusieurs centaines de kilo-octets), les clients n'échangent
donc que les **commandes** des joueurs (12 octets chacune). Chacun les applique localement et
compare régulièrement une **somme de contrôle** pour détecter une désynchronisation.

## Déjà en place

- `src/command.c` et `CmdType`/`Command` dans `src/game.h` : une commande de taille fixe par action
  (fin de tour, recrutement, ordre, déclaration de guerre, diplomatie, opération, posture, budget,
  fortification, dépôt, groupes d'armées, missions navales, dilemmes, dette, ONU...).
- `Cmd_Apply()` applique une commande ; `Cmd_Serialize()` / `Cmd_Deserialize()` la convertissent
  en 12 octets petit-boutistes, indépendants du compilateur et de la plateforme.
- `Game_Checksum()` résume l'état (graine, ressources, provinces, unités, relations).
- `Replay` enregistre les commandes d'une partie ; `Replay_Run()` la rejoue ; `Replay_Save()`
  l'écrit sur disque (touche `F6` dans le jeu).
- **Toutes** les actions de l'interface passent par `Do()` (dissuasion, traités, généraux, budget,
  politique, OTAN...) ; `Cmd_Apply()` valide les bornes de chaque argument avant de l'appliquer.
- **Hot-seat** : `Game.human[]` désigne les nations jouées, `Game.activePlayer` le joueur dont c'est la
  phase. Une commande n'est acceptée que de ce joueur. `CMD_END_TURN` passe au commandant suivant
  (`War_NextPlayer()`), puis le dernier déclenche la résolution du trimestre. Les commandants
  (`Cmdr_*()` dans `src/war.c`) disposent de l'armée et de la diplomatie militaire de leur nation ;
  l'IA militaire de ces nations est désactivée. Le brouillard de guerre (`War_UpdateIntelFor()`)
  est calculé pour le joueur actif.
- `make test` vérifie l'aller-retour de sérialisation, qu'un rejeu donne la même somme de contrôle
  et l'enchaînement des phases du hot-seat.

## Reste à faire

1. **Généraliser l'État joueur** : les commandants ne gèrent ni le budget, ni la défense hybride, ni
   les décrets, propres à la France (`N_FRANCE` dans ces systèmes). Pour offrir à chaque joueur
   l'ensemble du jeu, il faudra déplacer cet état (posture M2MC, budget, projets, doctrine...) dans
   une structure par joueur.
2. **Tours simultanés** en réseau : chaque joueur envoie ses commandes du tour, l'hôte les ordonne
   (par numéro de joueur), puis tous appliquent les fins de phase dans le même ordre.
3. **Réseau** : raylib ne fournit pas de couche réseau. Une bibliothèque légère comme ENet
   (UDP fiable, C, licence MIT) convient ; l'hôte fait office de relais.
4. **Déterminisme flottant entre plateformes** : la simulation utilise des `float`. Pour jouer entre
   Windows, Linux, macOS et Android, il faudra compiler avec des options strictes
   (`-ffp-contract=off`, pas de `-ffast-math`) et vérifier la somme de contrôle à chaque tour ;
   en cas d'écart, l'hôte renvoie l'état complet (`Game_Save`).
5. **Interface réseau** : salon, discussion, minuteur de tour.
