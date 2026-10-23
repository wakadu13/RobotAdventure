# Créer des événements (mods)

Le jeu charge au démarrage le fichier `mods/evenements.txt`, placé à côté de l'exécutable (sur Android :
dans le dossier privé de l'application). Chaque bloc `[evenement]` ajoute un événement aux 80 du jeu.
Un exemple commenté se trouve dans [exemple_evenements.txt](exemple_evenements.txt).

En réseau, tous les joueurs doivent avoir exactement le même fichier, sinon les parties divergent
(la somme de contrôle le signale).

## Clés

| Clé | Rôle |
|---|---|
| `titre`, `texte` | titre et description (le titre identifie aussi l'événement pour les suites) |
| `tour_min` | premier tour possible (0 = janvier 2030, 4 par an) |
| `poids` | fréquence relative ; `0` pour une suite qui ne survient jamais au hasard |
| `condition` | jusqu'à deux conditions, toutes requises |
| `option_a`, `option_b` | libellés des deux choix |
| `effet_a`, `effet_b` | jusqu'à quatre effets par option |
| `suite_a`, `suite_b`, `delai` | titre de l'événement déclenché par l'option, après `delai` tours |

## Conditions

`aucune`, `temp_sup D` (dixièmes de degré : 18 = +1,8°C), `ressource_inf R V`, `ressource_sup R V`,
`guerre`, `paix`, `democratie`, `autoritaire`, `otan`, `hors_otan`, `ue`, `mer_sup CM`, `dette_sup PCT`,
`chomage_sup PCT`, `menaces_sup N`, `faction_hostile F V`, `nation_vivante N`, `relation_inf N V`,
`relation_sup N V`, `strategique_inf S V`, `proliferation`, `projet_fini P`, `projet_absent P`,
`boreale_en_guerre`, `militance_sup DIXIEMES`, `guerre_contre N`, `tech_sup T NIVEAU`,
`scenario NUMERO` (0 : janvier 2030, 1 : crise de 2040, 2 : Balkans 2035, 3 : Sahel 2050).

## Effets

`ressource R V`, `militance STRATE DIXIEMES`, `conscience STRATE DIXIEMES`, `hostilite F V`,
`capacite_faction F V`, `menaces F NOMBRE`, `relation N V`, `relation_groupe G V`, `capacite DOMAINE V`,
`immigres CENTIEMES_DE_MILLION`, `dette MD`, `strategique S V`, `otan V`, `credibilite V`, `agression V`,
`recherche V`, `attenuation CENTIEMES`, `prosperite POURCENT`.

## Mots reconnus

- Ressources `R` : budget, energie, eau, alimentation, cohesion, influence, souverainete.
- Strates : travailleurs, moyennes, elites, toutes.
- Factions `F` : boreale, sables, hydra, nebula.
- Nations `N` : france, uk, irlande, benelux, allemagne, suisse, autriche, italie, espagne, portugal,
  danemark, scandinavie, baltes, pologne, europe_centrale, balkans, roumanie, bulgarie, grece, turquie,
  ukraine, boreale, maroc, algerie, tunisie, libye, egypte.
- Groupes `G` : ue, maghreb, otan. Domaines : terre, mer, air, espace, cyber, info, em.
- Ressources stratégiques `S` : terres_rares, uranium, puces.
- Projets `P` : smr, enr, dessal, agro, cyberciv, medias, constell, adapt.
- Technologies `T` : inf, blindes, air, marine, feux, ge, log, nuc.

Le texte des effets affiché au joueur est généré à partir de ces données. Les textes d'un mod ne sont pas
traduits : écrivez-les dans la langue de vos joueurs.
