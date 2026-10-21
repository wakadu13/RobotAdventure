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
- L'interface fait déjà passer par des commandes la fin de tour, le recrutement, les ordres, les
  déclarations de guerre et les choix des dilemmes.
- `make test` vérifie l'aller-retour de sérialisation et qu'un rejeu donne la même somme de contrôle.

## Reste à faire

1. **Faire passer toutes les actions de l'interface par `Do()`** (diplomatie, budget, opérations,
   traités...) pour que le rejeu soit complet.
2. **Généraliser le joueur** : la simulation suppose que la France est le seul camp humain
   (`N_FRANCE` dans la défense hybride, le budget, les décrets, les dilemmes...). Il faut une notion
   de « nations jouées » et déplacer l'état propre au joueur (posture M2MC, budget, projets,
   doctrine...) dans une structure par joueur.
3. **Tours simultanés** : chaque joueur envoie ses commandes du tour, l'hôte les ordonne
   (par numéro de joueur), puis tous appliquent `CMD_END_TURN` ensemble.
4. **Réseau** : raylib ne fournit pas de couche réseau. Une bibliothèque légère comme ENet
   (UDP fiable, C, licence MIT) convient ; l'hôte fait office de relais.
5. **Déterminisme flottant entre plateformes** : la simulation utilise des `float`. Pour jouer entre
   Windows, Linux, macOS et Android, il faudra compiler avec des options strictes
   (`-ffp-contract=off`, pas de `-ffast-math`) et vérifier la somme de contrôle à chaque tour ;
   en cas d'écart, l'hôte renvoie l'état complet (`Game_Save`).
6. **Interface** : salon, choix des nations, discussion, minuteur de tour.
