## Nouveautés de la 1.2.0

- **Plus de 360 événements** (contre une soixantaine), traduits dans les quatre langues, dont une cinquantaine de chaînes à suites différées : climat et outre-mer, société, économie et ressources stratégiques, défense et international, migrations en Méditerranée, avenir 2045-2060.
- **Événements inspirés de la Red Team Défense** : nation pirate, sphères immersives et guerre cognitive, hyperforteresses, rationnement énergétique et guerre du vivant, ruée vers l'espace, armée civile à implants de compétences.
- **Deux scénarios** : « La guerre des Balkans (2035) » et « L'effondrement du Sahel (2050) ».
- **Espagnol et allemand**, en plus du français et de l'anglais.
- **Multijoueur en réseau** (TCP, jusqu'à quatre joueurs) : l'hôte dirige la France, chaque joueur une nation ; contrôle de synchronisation à chaque tour. Traités entre joueurs, impôts et investissements des commandants.
- **Mobile** : interface agrandie (boutons A- et A+, x1,5 par défaut) avec défilement au doigt ; mode de carte, ORDRE et zoom de la carte agrandis.
- **Courbes de la partie** (touche G, clic sur le score ou écran de fin) : score, population, température, inflation, cohésion, popularité, lobbies, dette, budget.
- **Modes de carte COMMERCE et DÉGÂTS**.
- **OPTIONS** : mode daltonien, infobulles agrandies, raccourcis clavier modifiables. **PARTIES** : six emplacements de sauvegarde nommés en plus de la sauvegarde rapide et automatique.
- **Mods** : événements supplémentaires dans `mods/evenements.txt` (voir docs/MODS.md).

## Nouveautés de la 1.1.0

- **Grande interface pour téléphones** : une partie de l'écran à la fois, agrandie, avec une barre de navigation (F2 ou menu).
- **Multijoueur hot-seat** : la France et jusqu'à trois nations dirigées par d'autres joueurs sur le même appareil, phases successives, écran de passage, brouillard de guerre propre à chacun.
- **Version anglaise** complète (menu « Langue »).
- **Plus de 60 événements contextuels**, déclenchés selon le climat, la guerre, le régime, la dette, les factions...
- **Scénario « La crise de 2040 »** : +2 °C, invasion boréale des pays baltes, Libye effondrée, dette record.
- **Son** synthétisé par le jeu (effets et ambiance), activable dans le menu.
- **Simulation** : réserve de main-d'œuvre et vétérance des unités ; missions aériennes (supériorité, bombardement stratégique) ; routes commerciales géographiques ; inflation ; ministres, lobbies et popularité ; propositions d'alliance et d'accord de l'IA, chantage nucléaire en temps de paix ; campagnes de l'IA sur plusieurs fronts ; fin de partie rééquilibrée.
- **Technique** : sauvegardes versionnées champ par champ (les sauvegardes de la 1.0.0 ne sont plus lisibles), toutes les actions passent par des commandes rejouables, code découpé par thème, tests d'interface et intégration continue à chaque push.

## Paquets

- **Windows** : décompressez le zip et lancez `bouclier2050.exe` (SmartScreen : « Informations complémentaires » puis « Exécuter quand même »).
- **Linux** : `tar xzf` puis `./bouclier2050/bouclier2050` (X11, OpenGL 3.3).
- **macOS Apple Silicon** : décompressez, puis clic droit sur « Bouclier 2050 » > Ouvrir (application non notarisée).
- **Android** (arm64, Android 10+) : installez l'APK en autorisant les sources inconnues. Si l'APK est signé avec une nouvelle clé, désinstallez d'abord la version précédente.
