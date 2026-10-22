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

- **Réseau** (`src/net.c`, `src/ui_net.c`) : TCP non bloquant, trames fixes de 16 octets, topologie en
  étoile. L'hôte (la France) applique les commandes des clients puis les diffuse à tous, émetteur compris :
  l'ordre est donc identique partout. Phases successives comme en hot-seat ; chaque client calcule son
  brouillard de guerre (`War_UpdateIntelFor`), recalculé pour la France avant la résolution du tour. Sommes
  de contrôle comparées par l'hôte à chaque tour ; `CMD_DROP_PLAYER` rend à l'IA la nation d'un joueur
  déconnecté. Simulation sans contraction FMA et sans fonction transcendante : calculs identiques sur toutes
  les plateformes. Testé par `make net-test` (deux processus sur la boucle locale) en CI.

## Reste à faire

1. **Généraliser l'État joueur** : les commandants ne gèrent ni le budget, ni la défense hybride, ni
   les décrets, propres à la France (`N_FRANCE` dans ces systèmes). Pour offrir à chaque joueur
   l'ensemble du jeu, il faudra déplacer cet état (posture M2MC, budget, projets, doctrine...) dans
   une structure par joueur.
2. **Reprise après désynchronisation** : l'hôte pourrait renvoyer l'état complet (`Game_Save`) au lieu
   d'arrêter la partie ; de même, permettre à un joueur de rejoindre une partie en cours.
3. **Traversée des NAT** : aujourd'hui, l'hôte doit ouvrir le port 27050 pour jouer par Internet.
4. **Discussion et minuteur de phase** dans le salon et en jeu.
