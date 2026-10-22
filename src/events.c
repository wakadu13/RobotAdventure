/*
 * BOUCLIER 2050 - evenements contextuels, decrits par des donnees.
 *
 * Chaque evenement declare ses conditions (climat, guerre, regime, dette,
 * factions, voisins...) et, pour chacune de ses deux options, une liste
 * d'effets. Le texte des effets affiche au joueur est genere a partir de ces
 * donnees : il ne peut pas diverger de ce qui est applique. Ajouter un
 * evenement revient a ajouter une entree dans EVENTS[].
 */
#include "game_internal.h"
#include "lang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    EC_NONE,
    EC_TEMP_ABOVE,     /* v : dixiemes de degre */
    EC_RES_BELOW,      /* a : ressource */
    EC_RES_ABOVE,
    EC_WAR,            /* la France est en guerre */
    EC_PEACE,
    EC_DEMOCRACY,
    EC_AUTHORITARIAN,
    EC_NATO,
    EC_NOT_NATO,
    EC_EU,
    EC_SEA_ABOVE,      /* v : centimetres */
    EC_DEBT_ABOVE,     /* v : dette en % des recettes annuelles */
    EC_UNEMP_ABOVE,    /* v : % */
    EC_THREATS_ABOVE,
    EC_FAC_HOSTILE,    /* a : faction, v : hostilite */
    EC_ALIVE,          /* a : nation */
    EC_REL_BELOW,      /* a : nation, v : relation avec la France */
    EC_REL_ABOVE,
    EC_STRAT_BELOW,    /* a : ressource strategique, v : stock */
    EC_PROLIFERATION,  /* un programme nucleaire est en cours quelque part */
    EC_PROJECT_DONE,   /* a : projet */
    EC_PROJECT_NONE,
    EC_BOREALE_AT_WAR,
    EC_MILITANCY_ABOVE, /* v : dixiemes */
    EC_WAR_WITH,       /* a : nation */
    EC_TECH_ABOVE,     /* a : domaine technologique, v : niveau */
} EvCond;

typedef enum {
    EE_NONE,
    EE_RES,            /* a : ressource, v */
    EE_MIL,            /* a : strate (-1 : toutes), v : dixiemes */
    EE_FAC,            /* a : faction, v : hostilite */
    EE_FACCAP,         /* a : faction, v : capacite */
    EE_REL,            /* a : nation, v */
    EE_RELGROUP,       /* a : 0 UE, 1 Maghreb et Egypte, 2 OTAN */
    EE_CAP,            /* a : domaine, v */
    EE_IMMIG,          /* v : centiemes de million */
    EE_DEBT,           /* v : milliards */
    EE_STRAT,          /* a : ressource strategique, v */
    EE_THREAT,         /* a : faction, v : nombre de menaces */
    EE_NATO,           /* v : cohesion de l'Alliance */
    EE_CRED,           /* v : credibilite de la dissuasion */
    EE_AGGRO,          /* v : reputation d'agresseur */
    EE_RESEARCH,       /* v : points de recherche militaire */
    EE_MITIG,          /* v : centiemes d'attenuation climatique */
    EE_CON,            /* a : strate (-1 : toutes), v : dixiemes de conscience politique */
    EE_PROSP,          /* v : centiemes de prosperite */
} EvEffect;

typedef struct { unsigned char type; signed char a; short v; } EvTerm;

typedef struct {
    const char *title;
    const char *text;
    unsigned char minTurn;
    unsigned char weight;
    EvTerm cond[2];
    const char *optA;
    EvTerm effA[4];
    const char *optB;
    EvTerm effB[4];
    const char *nextA, *nextB;   /* suite declenchee par l'option (titre de l'evenement), ou NULL */
    unsigned char delay;         /* tours avant la suite */
} EventDef;

#define C(t, a, v) { t, a, v }
#define E(t, a, v) { t, a, v }

static const EventDef BUILTIN_EVENTS[] = {
    /* ---------------------------------------------------------------- climat */
    { "Dôme de chaleur", "Quarante-huit degrés à Lyon. Les hôpitaux saturent, les centrales nucléaires réduisent leur production faute d'eau froide.",
      4, 3, { C(EC_TEMP_ABOVE, 0, 17) },
      "Plan canicule national", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3), E(EE_RES, R_ENERGIE, -3) },
      "Laisser les collectivités gérer", { E(EE_RES, R_COHESION, -5), E(EE_MIL, S_TRAVAILLEURS, 10) }, NULL, NULL, 0 },
    { "Mégafeux dans les Landes", "La forêt des Landes brûle sur 80 000 hectares. Les Canadair européens sont tous engagés ailleurs.",
      6, 3, { C(EC_TEMP_ABOVE, 0, 17) },
      "Réquisitionner l'armée", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_TERRE, -1), E(EE_RES, R_COHESION, 3) },
      "Acheter une flotte de bombardiers d'eau", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "La Loire à sec", "Pour la première fois, la Loire est franchissable à pied à Orléans. Agriculteurs et centrales se disputent chaque mètre cube.",
      8, 3, { C(EC_TEMP_ABOVE, 0, 18), C(EC_RES_BELOW, R_EAU, 60) },
      "Priorité à l'agriculture", { E(EE_RES, R_ALIM, 4), E(EE_RES, R_ENERGIE, -5) },
      "Priorité à l'énergie", { E(EE_RES, R_ENERGIE, 3), E(EE_RES, R_ALIM, -4), E(EE_MIL, S_TRAVAILLEURS, 5) }, NULL, NULL, 0 },
    { "Tempête méditerranéenne", "Un medicane frappe la côte d'Azur. Ports, routes et voies ferrées sont emportés de Marseille à Menton.",
      6, 2, { C(EC_TEMP_ABOVE, 0, 17) },
      "Reconstruction d'urgence", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_COHESION, 2) },
      "Fonds européen de solidarité", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_SOUVERAINETE, -3), E(EE_RELGROUP, 0, 5) }, NULL, NULL, 0 },
    { "Submersion du littoral vendéen", "Une tempête d'équinoxe, portée par la montée des eaux, rompt les digues. Des villages entiers sont évacués.",
      10, 3, { C(EC_SEA_ABOVE, 0, 15) },
      "Relever les digues", { E(EE_RES, R_BUDGET, -12), E(EE_PROSP, 0, 2) },
      "Repli organisé vers l'intérieur", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_COHESION, -3), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Fonte des glaciers alpins", "Le glacier d'Argentière s'effondre. Les vallées alpines perdent leur réserve d'eau estivale.",
      12, 2, { C(EC_TEMP_ABOVE, 0, 19) },
      "Grands barrages-réservoirs", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_EAU, 5) },
      "Plan de sobriété hydrique", { E(EE_RES, R_EAU, 2), E(EE_MIL, S_MOYENNES, 8) }, NULL, NULL, 0 },
    { "Invasion de criquets", "Des essaims venus du Sahel ravagent les cultures de Provence et d'Occitanie.",
      8, 2, { C(EC_TEMP_ABOVE, 0, 18) },
      "Épandage massif", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_ALIM, -2), E(EE_RES, R_EAU, -2) },
      "Lutte biologique et indemnisation", { E(EE_RES, R_BUDGET, -7), E(EE_RES, R_ALIM, -4), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },
    { "Effondrement de la production de blé", "La récolte de blé chute de 40 %. Les marchés mondiaux s'emballent.",
      10, 3, { C(EC_TEMP_ABOVE, 0, 18), C(EC_RES_BELOW, R_ALIM, 65) },
      "Rationnement et stocks stratégiques", { E(EE_RES, R_ALIM, 5), E(EE_RES, R_COHESION, -4) },
      "Importer coûte que coûte", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_ALIM, 6) }, NULL, NULL, 0 },
    { "Pandémie de dengue", "Le moustique tigre propage la dengue jusqu'à Paris. Les urgences sont débordées.",
      8, 2, { C(EC_TEMP_ABOVE, 0, 17) },
      "Campagne de vaccination", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3) },
      "Démoustication chimique", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_EAU, -3), E(EE_CON, -1, 5) }, NULL, NULL, 0 },
    { "Sommet climatique de Paris+30", "Trente ans après l'accord de Paris, la France accueille un sommet de la dernière chance.",
      12, 2, { C(EC_RES_ABOVE, R_INFLUENCE, 35) },
      "Engagements contraignants", { E(EE_RES, R_BUDGET, -10), E(EE_MITIG, 0, 5), E(EE_RES, R_INFLUENCE, 6) },
      "Déclaration d'intention", { E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Exode des zones invivables", "Le Sud-Est perd des habitants chaque été. Les métropoles du Nord se saturent de déplacés intérieurs.",
      20, 2, { C(EC_TEMP_ABOVE, 0, 21) },
      "Plan de relocalisation", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_COHESION, 4) },
      "Laisser faire le marché", { E(EE_MIL, S_TRAVAILLEURS, 12), E(EE_RES, R_COHESION, -4) }, NULL, NULL, 0 },
    { "Géo-ingénierie solaire", "Un consortium propose d'épandre des aérosols dans la stratosphère pour refroidir l'Europe. Les effets sont imprévisibles.",
      24, 2, { C(EC_TEMP_ABOVE, 0, 21) },
      "Autoriser l'expérience", { E(EE_MITIG, 0, 8), E(EE_RES, R_INFLUENCE, -6), E(EE_RES, R_ALIM, -3) },
      "Interdire", { E(EE_RES, R_INFLUENCE, 3) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- societe */
    { "Marche pour le climat", "Un million de jeunes défilent à Paris. Ils exigent la fin des énergies fossiles d'ici cinq ans.",
      2, 3, { C(EC_DEMOCRACY, 0, 0) },
      "Recevoir les organisateurs", { E(EE_MITIG, 0, 2), E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_ELITES, 5) },
      "Ignorer le mouvement", { E(EE_CON, S_MOYENNES, 6), E(EE_RES, R_COHESION, -3) }, NULL, NULL, 0 },
    { "Émeutes en banlieue", "Une bavure policière embrase les quartiers populaires de plusieurs métropoles.",
      4, 3, { C(EC_MILITANCY_ABOVE, 0, 35) },
      "Apaisement et investissements", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -8) },
      "Couvre-feu et fermeté", { E(EE_RES, R_COHESION, -4), E(EE_CON, S_TRAVAILLEURS, 8) }, NULL, NULL, 0 },
    { "Mouvement des gilets verts", "Les ruraux se soulèvent contre les restrictions d'eau et de carburant.",
      6, 3, { C(EC_RES_BELOW, R_EAU, 55) },
      "Exemptions pour les campagnes", { E(EE_RES, R_EAU, -3), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Tenir bon", { E(EE_MIL, S_TRAVAILLEURS, 10), E(EE_RES, R_EAU, 2) }, NULL, NULL, 0 },
    { "Scandale de corruption au ministère", "Un ministre est accusé d'avoir vendu des contrats d'armement à Hydra.",
      4, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 20) },
      "Démission et enquête", { E(EE_RES, R_COHESION, 2), E(EE_FAC, F_HYDRA, 6) },
      "Étouffer l'affaire", { E(EE_RES, R_COHESION, -5), E(EE_FACCAP, F_HYDRA, 6) }, NULL, NULL, 0 },
    { "Pénurie de médecins", "Les déserts médicaux s'étendent ; les hôpitaux de province ferment des services.",
      4, 2, { C(EC_NONE, 0, 0) },
      "Recruter à l'étranger", { E(EE_IMMIG, 0, 5), E(EE_RES, R_BUDGET, -3), E(EE_RES, R_COHESION, 2) },
      "Former plus et attendre", { E(EE_RES, R_BUDGET, -6), E(EE_MIL, S_MOYENNES, 5) }, NULL, NULL, 0 },
    { "Référendum d'autonomie en Corse", "L'Assemblée de Corse réclame un référendum sur l'autonomie.",
      8, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Accorder le référendum", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_SOUVERAINETE, -2) },
      "Refuser", { E(EE_RES, R_COHESION, -3), E(EE_CON, -1, 3) }, NULL, NULL, 0 },
    { "Grève des routiers", "Le prix du carburant paralyse le transport routier ; les rayons se vident.",
      4, 2, { C(EC_RES_BELOW, R_ENERGIE, 55) },
      "Chèque carburant", { E(EE_RES, R_BUDGET, -7), E(EE_MIL, S_TRAVAILLEURS, -5) },
      "Réquisition des dépôts", { E(EE_RES, R_ALIM, -3), E(EE_MIL, S_TRAVAILLEURS, 8) }, NULL, NULL, 0 },
    { "Chômage de masse", "L'automatisation et la crise climatique font exploser le chômage.",
      6, 3, { C(EC_UNEMP_ABOVE, 0, 12) },
      "Revenu universel expérimental", { E(EE_RES, R_BUDGET, -12), E(EE_MIL, -1, -8) },
      "Grands travaux d'adaptation", { E(EE_RES, R_BUDGET, -8), E(EE_PROSP, 0, 3), E(EE_MIL, S_TRAVAILLEURS, -4) }, NULL, NULL, 0 },
    { "Montée de l'extrême droite", "Un parti nationaliste dépasse 35 % dans les sondages, porté par la crise migratoire.",
      8, 2, { C(EC_DEMOCRACY, 0, 0), C(EC_MILITANCY_ABOVE, 0, 30) },
      "Durcir le discours", { E(EE_RES, R_COHESION, 2), E(EE_RELGROUP, 1, -8), E(EE_RES, R_INFLUENCE, -3) },
      "Défendre l'État de droit", { E(EE_CON, S_MOYENNES, 6), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Exil fiscal des grandes fortunes", "Les grandes fortunes quittent le pays pour la Suisse et les Émirats.",
      6, 2, { C(EC_DEBT_ABOVE, 0, 90) },
      "Taxe de sortie", { E(EE_RES, R_BUDGET, 6), E(EE_MIL, S_ELITES, 12) },
      "Amnistie fiscale", { E(EE_RES, R_BUDGET, 3), E(EE_MIL, S_TRAVAILLEURS, 6) }, NULL, NULL, 0 },
    { "Désinformation sur les vaccins", "Une campagne virale accuse les vaccins d'être une arme de l'État.",
      4, 2, { C(EC_FAC_HOSTILE, F_NEBULA, 25) },
      "Contre-campagne publique", { E(EE_RES, R_BUDGET, -4), E(EE_FAC, F_NEBULA, 5) },
      "Censurer les plateformes", { E(EE_RES, R_SOUVERAINETE, 3), E(EE_RES, R_COHESION, -3), E(EE_CON, -1, 4) }, NULL, NULL, 0 },
    { "Jeux olympiques d'hiver", "Les Alpes françaises accueillent des Jeux d'hiver sur neige artificielle.",
      4, 1, { C(EC_PEACE, 0, 0) },
      "Jeux sobres", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_INFLUENCE, 4) },
      "Jeux grandioses", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_INFLUENCE, 7), E(EE_RES, R_EAU, -3) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- economie */
    { "Attaque des marchés", "Les fonds spéculatifs parient contre la dette française. Les taux s'envolent.",
      6, 3, { C(EC_DEBT_ABOVE, 0, 110) },
      "Plan de rigueur", { E(EE_RES, R_BUDGET, 6), E(EE_MIL, S_TRAVAILLEURS, 10) },
      "Appel à la BCE", { E(EE_RES, R_SOUVERAINETE, -5), E(EE_DEBT, 0, -15) }, NULL, NULL, 0 },
    { "Faillite d'un géant de l'automobile", "Le premier constructeur national est au bord du dépôt de bilan.",
      6, 2, { C(EC_UNEMP_ABOVE, 0, 9) },
      "Nationalisation", { E(EE_RES, R_BUDGET, -10), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_PROSP, 0, 2) },
      "Laisser faire", { E(EE_MIL, S_TRAVAILLEURS, 12), E(EE_PROSP, 0, -3) }, NULL, NULL, 0 },
    { "Pénurie de semi-conducteurs", "Taïwan sous blocus : l'industrie européenne manque de puces.",
      6, 3, { C(EC_STRAT_BELOW, SR_PUCES, 12) },
      "Payer le prix fort", { E(EE_RES, R_BUDGET, -8), E(EE_STRAT, SR_PUCES, 5) },
      "Rationner l'industrie civile", { E(EE_STRAT, SR_PUCES, 3), E(EE_PROSP, 0, -3) }, NULL, NULL, 0 },
    { "Découverte de lithium dans l'Allier", "Un gisement majeur de lithium est confirmé dans le Massif central.",
      4, 2, { C(EC_NONE, 0, 0) },
      "Exploitation rapide", { E(EE_RES, R_BUDGET, 8), E(EE_RES, R_EAU, -4), E(EE_STRAT, SR_TERRES_RARES, 3) },
      "Mine responsable, plus lente", { E(EE_RES, R_BUDGET, 3), E(EE_STRAT, SR_TERRES_RARES, 2), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Offre d'un fonds souverain", "Un fonds du Golfe propose d'entrer au capital de nos ports et aéroports.",
      4, 2, { C(EC_RES_BELOW, R_BUDGET, 30) },
      "Accepter l'investissement", { E(EE_RES, R_BUDGET, 14), E(EE_RES, R_SOUVERAINETE, -6) },
      "Refuser", { E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Krach des cryptomonnaies", "Un krach ruine des millions d'épargnants. Nébula est soupçonné d'avoir manipulé les cours.",
      8, 2, { C(EC_FAC_HOSTILE, F_NEBULA, 20) },
      "Indemniser les petits épargnants", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3) },
      "Traquer les responsables", { E(EE_FAC, F_NEBULA, 8), E(EE_FACCAP, F_NEBULA, -6), E(EE_MIL, S_MOYENNES, 6) }, NULL, NULL, 0 },
    { "Taxe carbone aux frontières", "Bruxelles propose de taxer lourdement les importations carbonées.",
      6, 2, { C(EC_EU, 0, 0) },
      "Soutenir la taxe", { E(EE_MITIG, 0, 3), E(EE_RES, R_BUDGET, 4), E(EE_RELGROUP, 1, -6) },
      "S'y opposer", { E(EE_RELGROUP, 0, -6), E(EE_RES, R_BUDGET, 2) }, NULL, NULL, 0 },
    { "Percée des réacteurs à fusion", "Un prototype européen de fusion dépasse le seuil d'ignition.",
      40, 1, { C(EC_PROJECT_DONE, P_SMR, 0) },
      "Financer l'industrialisation", { E(EE_RES, R_BUDGET, -14), E(EE_RES, R_ENERGIE, 10), E(EE_RES, R_INFLUENCE, 4) },
      "Laisser le privé avancer", { E(EE_RES, R_ENERGIE, 3) }, NULL, NULL, 0 },
    { "Effondrement d'une banque régionale", "Une banque mutualiste exposée aux prêts immobiliers côtiers fait faillite.",
      12, 2, { C(EC_SEA_ABOVE, 0, 12) },
      "Renflouer", { E(EE_DEBT, 0, 15), E(EE_RES, R_COHESION, 2) },
      "Laisser tomber", { E(EE_MIL, S_MOYENNES, 12), E(EE_PROSP, 0, -2) }, NULL, NULL, 0 },
    { "Inflation alimentaire", "Les prix de l'alimentation augmentent de 30 % en un an.",
      6, 3, { C(EC_RES_BELOW, R_ALIM, 55) },
      "Blocage des prix", { E(EE_RES, R_ALIM, -3), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_MIL, S_ELITES, 6) },
      "Chèque alimentaire", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -4) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- defense et guerre hybride */
    { "Rançongiciel contre les hôpitaux", "Des hôpitaux de l'Est sont paralysés par un rançongiciel. Les rançonneurs exigent 200 millions.",
      3, 3, { C(EC_FAC_HOSTILE, F_NEBULA, 20) },
      "Payer la rançon", { E(EE_RES, R_BUDGET, -4), E(EE_FACCAP, F_NEBULA, 6) },
      "Refuser et restaurer", { E(EE_RES, R_COHESION, -3), E(EE_CAP, D_CYBER, 1) }, NULL, NULL, 0 },
    { "Essaim de drones sur une centrale", "Des drones non identifiés survolent la centrale de Gravelines trois nuits de suite.",
      4, 3, { C(EC_FAC_HOSTILE, F_BOREALE, 25) },
      "Déployer des lasers antidrones", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_AIR, 1) },
      "Dénoncer publiquement", { E(EE_RES, R_INFLUENCE, 2), E(EE_FAC, F_BOREALE, 5), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Câble sous-marin sectionné", "Le câble qui relie Marseille à l'Afrique est coupé. Les soupçons se portent sur un navire boréal.",
      6, 2, { C(EC_ALIVE, N_BOREALE, 0) },
      "Escorter les navires câbliers", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_MER, 1) },
      "Saisir le navire suspect", { E(EE_REL, N_BOREALE, -15), E(EE_FAC, F_BOREALE, 8), E(EE_RES, R_SOUVERAINETE, 3) }, NULL, NULL, 0 },
    { "Taupe à la DGSE", "Un officier traitant vendait les noms de nos agents à une puissance étrangère.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Procès public", { E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_COHESION, 2) },
      "L'utiliser pour intoxiquer l'ennemi", { E(EE_FACCAP, F_BOREALE, -5), E(EE_RES, R_BUDGET, -2) }, NULL, NULL, 0 },
    { "Attentat déjoué", "Les services déjouent un attentat contre une gare parisienne. Le réseau est lié à la Ligue des Sables.",
      4, 3, { C(EC_FAC_HOSTILE, F_SABLES, 25) },
      "Frappe contre les commanditaires", { E(EE_FACCAP, F_SABLES, -8), E(EE_FAC, F_SABLES, 8), E(EE_RES, R_BUDGET, -4) },
      "Renforcer la surveillance intérieure", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_CON, -1, 4) }, NULL, NULL, 0 },
    { "Brouillage GPS généralisé", "Le GPS est brouillé dans tout le Nord-Est : avions cloués au sol, agriculture de précision à l'arrêt.",
      8, 2, { C(EC_BOREALE_AT_WAR, 0, 0) },
      "Accélérer Galileo militaire", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_ESPACE, 1), E(EE_CAP, D_EM, 1) },
      "Revenir aux moyens anciens", { E(EE_RES, R_ALIM, -2), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Satellite espion en orbite proche", "Un satellite étranger se colle à notre satellite de communication militaire.",
      8, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 30) },
      "Satellites gardes du corps", { E(EE_RES, R_BUDGET, -10), E(EE_CAP, D_ESPACE, 2) },
      "Protester à l'ONU", { E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Ferme à trolls démantelée", "Une ferme de 2 000 faux comptes est démasquée ; elle visait nos élections locales.",
      4, 2, { C(EC_DEMOCRACY, 0, 0), C(EC_FAC_HOSTILE, F_NEBULA, 15) },
      "Loi sur les plateformes", { E(EE_FACCAP, F_NEBULA, -6), E(EE_RES, R_INFLUENCE, -2) },
      "Éducation aux médias", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_INFO, 1) }, NULL, NULL, 0 },
    { "Mercenaires d'Hydra au Sahel", "Hydra recrute des mercenaires pour protéger ses mines au Sahel et menace nos ressortissants.",
      6, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 30) },
      "Opération d'évacuation", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, 3) },
      "Négocier avec Hydra", { E(EE_FAC, F_HYDRA, -8), E(EE_RES, R_SOUVERAINETE, -3) }, NULL, NULL, 0 },
    { "Volontaires pour le front", "Des milliers de jeunes se présentent aux bureaux de recrutement.",
      2, 2, { C(EC_WAR, 0, 0) },
      "Brigades de volontaires", { E(EE_RES, R_COHESION, 3), E(EE_CAP, D_TERRE, 1) },
      "Les orienter vers la défense civile", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_EAU, 2) }, NULL, NULL, 0 },
    { "Pacifistes devant l'Élysée", "La guerre s'éternise ; des manifestants exigent un cessez-le-feu.",
      4, 3, { C(EC_WAR, 0, 0), C(EC_DEMOCRACY, 0, 0) },
      "Promettre une paix rapide", { E(EE_RES, R_COHESION, 3), E(EE_CRED, 0, -5) },
      "Union sacrée", { E(EE_MIL, S_MOYENNES, 8), E(EE_CRED, 0, 3) }, NULL, NULL, 0 },
    { "Économie de guerre", "L'industrie peine à suivre la consommation d'obus et de missiles.",
      4, 3, { C(EC_WAR, 0, 0) },
      "Réquisitionner les usines", { E(EE_RES, R_BUDGET, -6), E(EE_RESEARCH, 0, 8), E(EE_PROSP, 0, -2) },
      "Acheter à l'étranger", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_SOUVERAINETE, -3) }, NULL, NULL, 0 },
    { "Blessés de guerre", "Les hôpitaux militaires débordent ; les familles demandent des comptes.",
      4, 2, { C(EC_WAR, 0, 0) },
      "Plan de soutien aux blessés", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 4) },
      "Censurer les chiffres", { E(EE_RES, R_COHESION, -2), E(EE_CON, -1, 5) }, NULL, NULL, 0 },
    { "Déserteurs et réfractaires", "Des appelés refusent de partir au front et se réfugient en Suisse.",
      8, 2, { C(EC_WAR, 0, 0), C(EC_AUTHORITARIAN, 0, 0) },
      "Amnistie", { E(EE_RES, R_COHESION, 2), E(EE_CRED, 0, -3) },
      "Cours martiales", { E(EE_MIL, -1, 8), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Essai nucléaire étranger", "Une puissance voisine procède à un essai nucléaire souterrain.",
      8, 3, { C(EC_PROLIFERATION, 0, 0) },
      "Réaffirmer la dissuasion", { E(EE_RES, R_BUDGET, -6), E(EE_CRED, 0, 8) },
      "Initiative de désarmement", { E(EE_RES, R_INFLUENCE, 5), E(EE_CRED, 0, -4) }, NULL, NULL, 0 },
    { "Chantage à l'uranium", "Notre principal fournisseur d'uranium menace de suspendre ses livraisons.",
      6, 2, { C(EC_STRAT_BELOW, SR_URANIUM, 10) },
      "Céder à ses exigences", { E(EE_STRAT, SR_URANIUM, 4), E(EE_RES, R_SOUVERAINETE, -4) },
      "Diversifier les sources", { E(EE_RES, R_BUDGET, -8), E(EE_STRAT, SR_URANIUM, 2) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- international */
    { "Sommet de l'OTAN", "Washington exige que les Européens consacrent 4 % de leur PIB à la défense.",
      4, 2, { C(EC_NATO, 0, 0) },
      "Accepter", { E(EE_RES, R_BUDGET, -10), E(EE_NATO, 0, 8), E(EE_CAP, D_AIR, 1) },
      "Plaider pour l'Europe de la défense", { E(EE_NATO, 0, -5), E(EE_RES, R_SOUVERAINETE, 3), E(EE_RELGROUP, 0, 4) }, NULL, NULL, 0 },
    { "Proposition d'armée européenne", "Berlin propose une armée européenne sous commandement intégré.",
      12, 2, { C(EC_EU, 0, 0), C(EC_ALIVE, N_ALLEMAGNE, 0) },
      "Accepter", { E(EE_RES, R_SOUVERAINETE, -6), E(EE_RELGROUP, 0, 10), E(EE_CAP, D_TERRE, 1) },
      "Garder nos forces nationales", { E(EE_REL, N_ALLEMAGNE, -8), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Crise de l'euro", "L'Italie menace de quitter l'euro. Les marchés paniquent.",
      12, 2, { C(EC_EU, 0, 0), C(EC_ALIVE, N_ITALIE, 0) },
      "Solidarité financière", { E(EE_DEBT, 0, 20), E(EE_REL, N_ITALIE, 15) },
      "Fermeté budgétaire", { E(EE_REL, N_ITALIE, -12), E(EE_RES, R_BUDGET, 2) }, NULL, NULL, 0 },
    { "Ultimatum boréal à la Baltique", "La Fédération Boréale masse ses troupes aux frontières des pays baltes.",
      8, 3, { C(EC_ALIVE, N_BOREALE, 0), C(EC_ALIVE, N_BALTES, 0) },
      "Déployer une brigade en Estonie", { E(EE_RES, R_BUDGET, -6), E(EE_NATO, 0, 6), E(EE_REL, N_BOREALE, -10) },
      "Appeler au dialogue", { E(EE_NATO, 0, -6), E(EE_REL, N_BOREALE, 5) }, NULL, NULL, 0 },
    { "Coup d'État à Alger", "Des généraux prennent le pouvoir à Alger et rompent avec la France.",
      10, 2, { C(EC_ALIVE, N_ALGERIE, 0), C(EC_REL_BELOW, N_ALGERIE, 20) },
      "Reconnaître le nouveau pouvoir", { E(EE_REL, N_ALGERIE, 10), E(EE_RES, R_INFLUENCE, -3) },
      "Sanctions ciblées", { E(EE_REL, N_ALGERIE, -15), E(EE_RES, R_ENERGIE, -3), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Barrage sur le Nil", "L'Égypte menace d'entrer en guerre pour l'eau du Nil et demande l'appui de Paris.",
      12, 2, { C(EC_ALIVE, N_EGYPTE, 0) },
      "Médiation française", { E(EE_RES, R_INFLUENCE, 5), E(EE_RES, R_BUDGET, -3), E(EE_REL, N_EGYPTE, 8) },
      "Ne pas s'en mêler", { E(EE_REL, N_EGYPTE, -5) }, NULL, NULL, 0 },
    { "Réfugiés en Méditerranée", "Une flottille de réfugiés libyens dérive au large de la Sicile.",
      4, 3, { C(EC_ALIVE, N_LIBYE, 0) },
      "Les accueillir", { E(EE_IMMIG, 0, 10), E(EE_RES, R_INFLUENCE, 4), E(EE_MIL, S_MOYENNES, 4) },
      "Financer les garde-côtes libyens", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_INFLUENCE, -4), E(EE_FACCAP, F_SABLES, 4) }, NULL, NULL, 0 },
    { "Accord gazier avec le Maroc", "Le Maroc propose de l'hydrogène vert produit dans le désert.",
      10, 2, { C(EC_ALIVE, N_MAROC, 0), C(EC_REL_ABOVE, N_MAROC, 10) },
      "Signer un partenariat", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ENERGIE, 5), E(EE_REL, N_MAROC, 10) },
      "Préférer la production nationale", { E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Neutralité suisse en question", "La Suisse demande des garanties de sécurité à la France.",
      10, 1, { C(EC_ALIVE, N_SUISSE, 0), C(EC_BOREALE_AT_WAR, 0, 0) },
      "Garantir la Suisse", { E(EE_REL, N_SUISSE, 20), E(EE_CRED, 0, 3), E(EE_AGGRO, 0, -5) },
      "Rester prudents", { E(EE_REL, N_SUISSE, -5) }, NULL, NULL, 0 },
    { "Visite du président turc", "Ankara propose un pacte de sécurité en Méditerranée orientale contre la Grèce.",
      8, 1, { C(EC_ALIVE, N_TURQUIE, 0), C(EC_ALIVE, N_GRECE, 0) },
      "Accepter le pacte", { E(EE_REL, N_TURQUIE, 15), E(EE_REL, N_GRECE, -15) },
      "Soutenir Athènes", { E(EE_REL, N_GRECE, 12), E(EE_REL, N_TURQUIE, -12) }, NULL, NULL, 0 },
    { "Hors de l'OTAN, isolés ?", "Sans l'Alliance, nos partenaires se demandent si la France les défendra.",
      8, 2, { C(EC_NOT_NATO, 0, 0) },
      "Garanties bilatérales", { E(EE_RES, R_BUDGET, -6), E(EE_RELGROUP, 0, 6), E(EE_CRED, 0, 3) },
      "Assumer l'autonomie", { E(EE_RES, R_SOUVERAINETE, 3), E(EE_RES, R_INFLUENCE, -3) }, NULL, NULL, 0 },
    { "Agression française condamnée", "Nos conquêtes inquiètent : les chancelleries évoquent une coalition.",
      4, 3, { C(EC_WAR, 0, 0), C(EC_RES_BELOW, R_INFLUENCE, 40) },
      "Offensive diplomatique", { E(EE_RES, R_BUDGET, -5), E(EE_AGGRO, 0, -10), E(EE_RES, R_INFLUENCE, 2) },
      "Ignorer les critiques", { E(EE_RELGROUP, 0, -6), E(EE_AGGRO, 0, 5) }, NULL, NULL, 0 },
    { "Course aux armements", "Nos voisins réarment massivement ; nos armées accusent un retard technologique.",
      12, 2, { C(EC_PEACE, 0, 0) },
      "Programme d'urgence", { E(EE_RES, R_BUDGET, -10), E(EE_RESEARCH, 0, 12) },
      "Miser sur la diplomatie", { E(EE_RES, R_INFLUENCE, 3), E(EE_AGGRO, 0, -5) }, NULL, NULL, 0 },
    { "Intelligence artificielle militaire", "Nos laboratoires proposent des essaims de drones autonomes, sans humain dans la boucle.",
      20, 2, { C(EC_TECH_ABOVE, T_GE, 2) },
      "Développer", { E(EE_RESEARCH, 0, 15), E(EE_RES, R_INFLUENCE, -4) },
      "Moratoire éthique", { E(EE_RES, R_INFLUENCE, 5), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- chaines d'evenements (suites differees) */
    { "Grève des raffineries", "Les raffineurs bloquent les dépôts pétroliers : l'essence manque dans la moitié du pays.",
      6, 2, { C(EC_RES_BELOW, R_ENERGIE, 65) },
      "Ouvrir des négociations", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ENERGIE, -2) },
      "Réquisitionner les dépôts", { E(EE_RES, R_ENERGIE, 2), E(EE_MIL, S_TRAVAILLEURS, 10) },
      "Accord salarial dans l'énergie", "Sabotage des dépôts", 3 },
    { "Accord salarial dans l'énergie", "Les négociations aboutissent : hausse des salaires contre un plan d'investissement dans les réseaux.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Signer l'accord", { E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, -8), E(EE_RES, R_ENERGIE, 3) },
      "Le renvoyer au Parlement", { E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Sabotage des dépôts", "Des grévistes radicalisés incendient deux dépôts réquisitionnés. Nébula diffuse les images en boucle.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Arrestations", { E(EE_RES, R_ENERGIE, -4), E(EE_CON, S_TRAVAILLEURS, 6) },
      "Amnistie et médiation", { E(EE_RES, R_ENERGIE, -4), E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, -5) }, NULL, NULL, 0 },
    { "Fuite au ministère des Armées", "Des plans de nos systèmes de défense circulent sur des forums. Le Réseau Nébula est soupçonné.",
      6, 2, { C(EC_FAC_HOSTILE, F_NEBULA, 30) },
      "Enquête discrète", { E(EE_RES, R_BUDGET, -3) },
      "Annonce publique", { E(EE_RES, R_COHESION, -2), E(EE_RES, R_INFLUENCE, 2) },
      "Le réseau Nébula démasqué", NULL, 3 },
    { "Le réseau Nébula démasqué", "L'enquête remonte jusqu'aux serveurs du réseau : nos services peuvent frapper.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Démanteler le réseau", { E(EE_FACCAP, F_NEBULA, -12), E(EE_FAC, F_NEBULA, 8) },
      "Le retourner contre ses clients", { E(EE_FACCAP, F_NEBULA, -6), E(EE_FACCAP, F_BOREALE, -5) }, NULL, NULL, 0 },
    { "Sécheresse historique", "Pas une goutte depuis cinq mois. Les nappes phréatiques sont au plus bas depuis un siècle.",
      8, 2, { C(EC_TEMP_ABOVE, 0, 18) },
      "Rationner sévèrement", { E(EE_RES, R_EAU, 2), E(EE_MIL, S_MOYENNES, 8) },
      "Pomper les nappes profondes", { E(EE_RES, R_EAU, 8) },
      "Les nappes se reconstituent", "Les nappes profondes épuisées", 5 },
    { "Les nappes se reconstituent", "Le rationnement a porté ses fruits : les nappes remontent et les rivières reprennent leur cours.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Lever les restrictions", { E(EE_RES, R_EAU, 6), E(EE_MIL, S_MOYENNES, -6) },
      "Maintenir une sobriété durable", { E(EE_RES, R_EAU, 9), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Les nappes profondes épuisées", "Le pompage a vidé les nappes fossiles : des puits s'assèchent dans tout le Sud.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Usines de dessalement d'urgence", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_EAU, -4) },
      "Laisser les cultures sécher", { E(EE_RES, R_EAU, -8), E(EE_RES, R_ALIM, -6) }, NULL, NULL, 0 },
    { "Offre de pacte boréal", "La Fédération Boréale propose un pacte de non-agression et des livraisons de gaz garanties.",
      10, 1, { C(EC_ALIVE, N_BOREALE, 0), C(EC_PEACE, 0, 0) },
      "Signer le pacte", { E(EE_REL, N_BOREALE, 20), E(EE_RES, R_ENERGIE, 5), E(EE_NATO, 0, -8) },
      "Refuser", { E(EE_NATO, 0, 4), E(EE_REL, N_BOREALE, -5) },
      "Le pacte boréal rompu", NULL, 8 },
    { "Le pacte boréal rompu", "Moscou-Boréal suspend les livraisons et accuse la France de duplicité : le pacte n'était qu'un leurre.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Dénoncer la trahison", { E(EE_REL, N_BOREALE, -30), E(EE_RES, R_ENERGIE, -6), E(EE_NATO, 0, 6) },
      "Chercher un compromis", { E(EE_REL, N_BOREALE, -10), E(EE_RES, R_ENERGIE, -3), E(EE_RES, R_SOUVERAINETE, -3) }, NULL, NULL, 0 },
    { "Bouclier cyber européen", "Les Vingt-Sept proposent de mutualiser leur cyberdéfense dans un centre commun.",
      6, 2, { C(EC_EU, 0, 0) },
      "Contribuer largement", { E(EE_RES, R_BUDGET, -8), E(EE_RELGROUP, 0, 4) },
      "Rester seul maître", { E(EE_RES, R_SOUVERAINETE, 2) },
      "Le bouclier cyber opérationnel", NULL, 6 },
    { "Le bouclier cyber opérationnel", "Le centre européen de cyberdéfense est en service : les attaques sont repérées en quelques minutes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Y raccorder nos réseaux", { E(EE_CAP, D_CYBER, 2), E(EE_FACCAP, F_NEBULA, -5) },
      "Garder nos réseaux à part", { E(EE_CAP, D_CYBER, 1), E(EE_RES, R_SOUVERAINETE, 1) }, NULL, NULL, 0 },

    /* ---------------------------------------------------------------- evenements propres a certaines nations */
    { "Référendum en Catalogne", "La Catalogne vote son indépendance ; Madrid demande le soutien de Paris.",
      12, 1, { C(EC_ALIVE, N_ESPAGNE, 0) },
      "Soutenir Madrid", { E(EE_REL, N_ESPAGNE, 15), E(EE_RES, R_INFLUENCE, -2) },
      "Prôner le dialogue", { E(EE_REL, N_ESPAGNE, -8), E(EE_RES, R_INFLUENCE, 3) }, NULL, NULL, 0 },
    { "Le Royaume-Uni frappe à la porte de l'UE", "Londres demande à réintégrer l'Union européenne et sollicite l'appui de la France.",
      16, 1, { C(EC_ALIVE, N_UK, 0), C(EC_EU, 0, 0) },
      "Appuyer le retour", { E(EE_REL, N_UK, 20), E(EE_RELGROUP, 0, 3), E(EE_RES, R_INFLUENCE, 3) },
      "Exiger des conditions", { E(EE_REL, N_UK, -6), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Printemps tunisien", "La jeunesse tunisienne renverse le gouvernement et appelle l'Europe à l'aide.",
      8, 1, { C(EC_ALIVE, N_TUNISIE, 0) },
      "Plan d'aide démocratique", { E(EE_RES, R_BUDGET, -6), E(EE_REL, N_TUNISIE, 20), E(EE_RES, R_INFLUENCE, 3) },
      "Rester neutre", { E(EE_REL, N_TUNISIE, -5), E(EE_FACCAP, F_SABLES, 4) }, NULL, NULL, 0 },
    { "Reconstruction de l'Ukraine", "Kiev lance un grand plan de reconstruction et invite les industriels européens.",
      6, 1, { C(EC_ALIVE, N_UKRAINE, 0), C(EC_PEACE, 0, 0) },
      "Engager nos entreprises", { E(EE_RES, R_BUDGET, 6), E(EE_REL, N_UKRAINE, 10), E(EE_REL, N_BOREALE, -8) },
      "Laisser faire les autres", { E(EE_REL, N_UKRAINE, -4) }, NULL, NULL, 0 },
    { "Bouclier de l'Est", "La Pologne demande des troupes françaises sur la frontière boréale.",
      8, 2, { C(EC_ALIVE, N_POLOGNE, 0), C(EC_BOREALE_AT_WAR, 0, 0) },
      "Envoyer une brigade", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_POLOGNE, 15), E(EE_NATO, 0, 5), E(EE_CAP, D_TERRE, -1) },
      "Envoyer des armes seulement", { E(EE_RES, R_BUDGET, -3), E(EE_REL, N_POLOGNE, 5) }, NULL, NULL, 0 },
    { "Élections allemandes", "Un gouvernement eurosceptique arrive au pouvoir à Berlin et remet en cause le couple franco-allemand.",
      12, 1, { C(EC_ALIVE, N_ALLEMAGNE, 0), C(EC_EU, 0, 0) },
      "Relancer le couple franco-allemand", { E(EE_RES, R_BUDGET, -4), E(EE_REL, N_ALLEMAGNE, 12) },
      "Chercher d'autres partenaires", { E(EE_REL, N_ALLEMAGNE, -8), E(EE_REL, N_ITALIE, 8), E(EE_REL, N_ESPAGNE, 8) }, NULL, NULL, 0 },
};

#define BUILTIN_COUNT ((int)(sizeof(BUILTIN_EVENTS) / sizeof(BUILTIN_EVENTS[0])))

/* table de travail : evenements integres, puis ceux des mods */
static EventDef EVENTS[EVENT_MAX];
int EVENT_COUNT;
#define EVENT_COUNT_REAL EVENT_COUNT

void Ev_Init(void)
{
    if (EVENT_COUNT) return;
    for (int k = 0; k < BUILTIN_COUNT && k < EVENT_MAX; k++) EVENTS[k] = BUILTIN_EVENTS[k];
    EVENT_COUNT = BUILTIN_COUNT < EVENT_MAX ? BUILTIN_COUNT : EVENT_MAX;
}

static int FindEvent(const char *title)
{
    for (int k = 0; k < EVENT_COUNT; k++) if (strcmp(EVENTS[k].title, title) == 0) return k;
    return -1;
}

static float FrenchMilitancy(const Game *g)
{
    float m = 0, pop = 0;
    for (int p = 0; p < PROV_COUNT; p++) {
        const Province *pr = &g->prov[p];
        if (pr->owner != N_FRANCE) continue;
        m += Soc_ProvinceMilitancy(pr) * pr->pop;
        pop += pr->pop;
    }
    return pop > 0 ? m / pop : 0;
}

static bool CondOk(const Game *g, const EvTerm *c)
{
    const Nation *fr = &g->nat[N_FRANCE];
    int a = c->a, v = c->v;
    switch (c->type) {
    case EC_NONE:            return true;
    case EC_TEMP_ABOVE:      return g->temp * 10 > v;
    case EC_RES_BELOW:       return g->res[a] < v;
    case EC_RES_ABOVE:       return g->res[a] > v;
    case EC_WAR:             return War_AtWarAny(g, N_FRANCE);
    case EC_PEACE:           return !War_AtWarAny(g, N_FRANCE);
    case EC_DEMOCRACY:       return IsDemocratic(fr->regime);
    case EC_AUTHORITARIAN:   return fr->regime >= RG_AUTORITAIRE;
    case EC_NATO:            return fr->nato;
    case EC_NOT_NATO:        return !fr->nato;
    case EC_EU:              return fr->eu;
    case EC_SEA_ABOVE:       return g->seaLevel * 100 > v;
    case EC_DEBT_ABOVE:      return Eco_DebtRatio(g) * 100 > v;
    case EC_UNEMP_ABOVE:     return Eco_NationUnemployment(g, N_FRANCE) * 100 > v;
    case EC_THREATS_ABOVE:   return Game_ActiveThreats(g) > v;
    case EC_FAC_HOSTILE:     return g->fac[a].hostility > v;
    case EC_ALIVE:           return g->nat[a].alive;
    case EC_REL_BELOW:       return g->nat[a].alive && g->nat[a].relation < v;
    case EC_REL_ABOVE:       return g->nat[a].alive && g->nat[a].relation > v;
    case EC_STRAT_BELOW:     return fr->strat[a] < v;
    case EC_PROLIFERATION:   for (int n = 1; n < N_COUNT; n++) if (g->nat[n].alive && g->nat[n].nukeStatus == 1) return true;
                             return false;
    case EC_PROJECT_DONE:    return g->proj[a].state == 2;
    case EC_PROJECT_NONE:    return g->proj[a].state == 0;
    case EC_BOREALE_AT_WAR:  return g->nat[N_BOREALE].alive && War_AtWarAny(g, N_BOREALE);
    case EC_MILITANCY_ABOVE: return FrenchMilitancy(g) * 10 > v;
    case EC_WAR_WITH:        return War_AtWar(g, N_FRANCE, a);
    case EC_TECH_ABOVE:      return fr->tech[a] >= v;
    default:                 return false;
    }
}

static bool Eligible(const Game *g, int k)
{
    const EventDef *e = &EVENTS[k];
    if (g->eventUsed[k] || g->turn < e->minTurn || e->weight == 0) return false;   /* poids nul : une suite, jamais tiree */
    for (int i = 0; i < 2; i++) if (!CondOk(g, &e->cond[i])) return false;
    return true;
}

static void AddRelGroup(Game *g, int group, float v)
{
    for (int n = 1; n < N_COUNT; n++) {
        const Nation *na = &g->nat[n];
        if (!na->alive) continue;
        bool in = group == 0 ? na->eu : group == 1 ? (n >= N_MAROC && n <= N_EGYPTE) : na->nato;
        if (in) AddRelation(g, n, v);
    }
}

static void ApplyEffect(Game *g, const EvTerm *e)
{
    int a = e->a;
    float v = e->v;
    switch (e->type) {
    case EE_RES:      AddRes(g, (Resource)a, v); break;
    case EE_MIL:      AddMilitancy(g, a, v / 10); break;
    case EE_FAC:      AddFac(g, a, v, 0); break;
    case EE_FACCAP:   AddFac(g, a, 0, v); break;
    case EE_REL:      if (g->nat[a].alive) AddRelation(g, a, v); break;
    case EE_RELGROUP: AddRelGroup(g, a, v); break;
    case EE_CAP:      g->capacity[a] = g->capacity[a] + (int)v < 0 ? 0 : g->capacity[a] + (int)v; break;
    case EE_IMMIG:    AddImmigrants(g, v / 100); break;
    case EE_DEBT:     g->debt = g->debt + v < 0 ? 0 : g->debt + v; break;
    case EE_STRAT:    g->nat[N_FRANCE].strat[a] = Clampf(g->nat[N_FRANCE].strat[a] + v, 0, 60); break;
    case EE_THREAT:   for (int i = 0; i < (int)v; i++) SpawnThreatFor(g, a); break;
    case EE_NATO:     g->natoCohesion = Clampf(g->natoCohesion + v, 0, 100); break;
    case EE_CRED:     g->credibility = Clampf(g->credibility + v, 0, 100); break;
    case EE_AGGRO:    g->aggressionMemory = Clampf(g->aggressionMemory + v, 0, 100); break;
    case EE_RESEARCH: g->researchProgress += v; break;
    case EE_MITIG:    g->climateMitigation += v / 100; break;
    case EE_CON:
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == N_FRANCE)
                for (int s = 0; s < S_COUNT; s++)
                    if (a < 0 || s == a) g->prov[p].con[s] = Clampf(g->prov[p].con[s] + v / 10, 0, 10);
        break;
    case EE_PROSP:
        for (int p = 0; p < PROV_COUNT; p++)
            if (g->prov[p].owner == N_FRANCE) g->prov[p].prosperity = Clampf(g->prov[p].prosperity + v / 100, 0.4f, 2.5f);
        break;
    default: break;
    }
}

/* texte d'un effet, genere depuis les donnees */
static int EffectText(const EvTerm *e, char *buf, int len)
{
    int a = e->a, v = e->v;
    static const char *GROUPS[3] = { "l'Union européenne", "le Maghreb et l'Égypte", "l'OTAN" };
    switch (e->type) {
    case EE_RES:      return snprintf(buf, len, "%+d %s", v, Tr(RESOURCE_NAMES[a]));
    case EE_MIL:      return a < 0 ? snprintf(buf, len, Tr("militance générale %+.1f"), v / 10.0)
                                   : snprintf(buf, len, Tr("militance (%s) %+.1f"), Tr(STRATA_NAMES[a]), v / 10.0);
    case EE_FAC:      return snprintf(buf, len, Tr("%s : hostilité %+d"), Tr(FACTIONS[a].name), v);
    case EE_FACCAP:   return snprintf(buf, len, Tr("%s : capacité %+d"), Tr(FACTIONS[a].name), v);
    case EE_REL:      return snprintf(buf, len, Tr("relations avec %s %+d"), Tr(NATIONS[a].name), v);
    case EE_RELGROUP: return snprintf(buf, len, Tr("relations avec %s %+d"), Tr(GROUPS[a]), v);
    case EE_CAP:      return snprintf(buf, len, Tr("capacité %s %+d"), Tr(DOMAIN_NAMES[a]), v);
    case EE_IMMIG:    return snprintf(buf, len, Tr("+%d 000 habitants à intégrer"), v * 10);
    case EE_DEBT:     return snprintf(buf, len, Tr("dette %+d Md"), v);
    case EE_STRAT:    return snprintf(buf, len, "%s %+d", Tr(STRATS[a].name), v);
    case EE_THREAT:   return snprintf(buf, len, Tr("%d menace(s) de %s"), v, Tr(FACTIONS[a].name));
    case EE_NATO:     return snprintf(buf, len, Tr("cohésion de l'OTAN %+d"), v);
    case EE_CRED:     return snprintf(buf, len, Tr("crédibilité de la dissuasion %+d"), v);
    case EE_AGGRO:    return snprintf(buf, len, Tr("réputation d'agresseur %+d"), v);
    case EE_RESEARCH: return snprintf(buf, len, Tr("recherche militaire %+d"), v);
    case EE_MITIG:    return snprintf(buf, len, Tr("atténuation du réchauffement %+d"), v);
    case EE_CON:      return a < 0 ? snprintf(buf, len, Tr("conscience politique %+.1f"), v / 10.0)
                                   : snprintf(buf, len, Tr("conscience politique (%s) %+.1f"), Tr(STRATA_NAMES[a]), v / 10.0);
    case EE_PROSP:    return snprintf(buf, len, Tr("prospérité %+d%%"), v);
    default:          return 0;
    }
}

static void EffectsText(const EvTerm *eff, char *buf, int len)
{
    int l = 0;
    buf[0] = 0;
    for (int i = 0; i < 4 && eff[i].type != EE_NONE && l < len - 2; i++) {
        if (i) l += snprintf(buf + l, len - l, ", ");
        if (l < len) l += EffectText(&eff[i], buf + l, len - l);
    }
    if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 'a' + 'A');
}

const DilemmaDef *Ev_Dilemma(int k)
{
    static DilemmaDef d;
    static char effA[200], effB[200];
    if (k < 0 || k >= EVENT_COUNT_REAL) return NULL;
    const EventDef *e = &EVENTS[k];
    EffectsText(e->effA, effA, sizeof(effA));
    EffectsText(e->effB, effB, sizeof(effB));
    d.title = e->title;
    d.text = e->text;
    d.optA = e->optA;
    d.effA = effA;
    d.optB = e->optB;
    d.effB = effB;
    return &d;
}

void Ev_Apply(Game *g, int k, int choice)
{
    if (k < 0 || k >= EVENT_COUNT_REAL) return;
    const EvTerm *eff = choice == 0 ? EVENTS[k].effA : EVENTS[k].effB;
    for (int i = 0; i < 4 && eff[i].type != EE_NONE; i++) ApplyEffect(g, &eff[i]);
    /* suite differee */
    const char *next = choice == 0 ? EVENTS[k].nextA : EVENTS[k].nextB;
    int f = next ? FindEvent(next) : -1;
    if (f >= 0)
        for (int q = 0; q < EVENT_QUEUE; q++)
            if (g->eventQueue[q] == 0) {
                g->eventQueue[q] = (short)(f + 1);
                g->eventDue[q] = (short)(g->turn + (EVENTS[k].delay ? EVENTS[k].delay : 2));
                break;
            }
}

/* tirage d'un evenement parmi ceux dont les conditions sont reunies, selon leur poids */
/* suites arrivees a echeance : elles surviennent d'office */
bool Ev_TriggerDue(Game *g)
{
    if (g->pendingDilemma >= 0) return false;
    for (int q = 0; q < EVENT_QUEUE; q++) {
        int f = g->eventQueue[q] - 1;
        if (f < 0 || g->turn < g->eventDue[q]) continue;
        g->eventQueue[q] = 0;
        if (f >= EVENT_COUNT) continue;
        g->eventUsed[f] = true;
        g->pendingDilemma = DLM_EVENT + f;
        return true;
    }
    return false;
}

bool Ev_Trigger(Game *g)
{
    int total = 0;
    for (int k = 0; k < EVENT_COUNT_REAL && k < EVENT_MAX; k++) if (Eligible(g, k)) total += EVENTS[k].weight;
    if (total == 0) return false;
    int r = (int)(Rand(g) % (unsigned)total);
    for (int k = 0; k < EVENT_COUNT_REAL && k < EVENT_MAX; k++) {
        if (!Eligible(g, k)) continue;
        r -= EVENTS[k].weight;
        if (r < 0) {
            g->eventUsed[k] = true;
            g->pendingDilemma = DLM_EVENT + k;
            return true;
        }
    }
    return false;
}

bool Ev_Eligible(const Game *g, int k) { return k >= 0 && k < EVENT_COUNT_REAL && Eligible(g, k); }

/* ------------------------------------------------------------------ */
/* Mods : evenements decrits dans un fichier texte                     */
/* ------------------------------------------------------------------ */
/*
 * Format (voir docs/MODS.md) : un bloc par evenement, lignes « cle = valeur ».
 *
 *     [evenement]
 *     titre = Crue de la Seine
 *     texte = La Seine deborde a Paris.
 *     tour_min = 8
 *     poids = 2
 *     condition = temp_sup 17
 *     option_a = Evacuer
 *     effet_a = ressource budget -6
 *     option_b = Attendre
 *     effet_b = ressource cohesion -4
 */
/* lang:ignore-begin */
enum { ARG_NONE, ARG_RES, ARG_STRATA, ARG_FAC, ARG_NATION, ARG_GROUP, ARG_DOMAIN, ARG_STRAT, ARG_PROJ, ARG_TECH };
typedef struct { const char *name; unsigned char type; unsigned char arg; bool value; } ModKey;

static const ModKey MOD_CONDS[] = {
    { "aucune", EC_NONE, ARG_NONE, false }, { "temp_sup", EC_TEMP_ABOVE, ARG_NONE, true },
    { "ressource_inf", EC_RES_BELOW, ARG_RES, true }, { "ressource_sup", EC_RES_ABOVE, ARG_RES, true },
    { "guerre", EC_WAR, ARG_NONE, false }, { "paix", EC_PEACE, ARG_NONE, false },
    { "democratie", EC_DEMOCRACY, ARG_NONE, false }, { "autoritaire", EC_AUTHORITARIAN, ARG_NONE, false },
    { "otan", EC_NATO, ARG_NONE, false }, { "hors_otan", EC_NOT_NATO, ARG_NONE, false }, { "ue", EC_EU, ARG_NONE, false },
    { "mer_sup", EC_SEA_ABOVE, ARG_NONE, true }, { "dette_sup", EC_DEBT_ABOVE, ARG_NONE, true },
    { "chomage_sup", EC_UNEMP_ABOVE, ARG_NONE, true }, { "menaces_sup", EC_THREATS_ABOVE, ARG_NONE, true },
    { "faction_hostile", EC_FAC_HOSTILE, ARG_FAC, true }, { "nation_vivante", EC_ALIVE, ARG_NATION, false },
    { "relation_inf", EC_REL_BELOW, ARG_NATION, true }, { "relation_sup", EC_REL_ABOVE, ARG_NATION, true },
    { "strategique_inf", EC_STRAT_BELOW, ARG_STRAT, true }, { "proliferation", EC_PROLIFERATION, ARG_NONE, false },
    { "projet_fini", EC_PROJECT_DONE, ARG_PROJ, false }, { "projet_absent", EC_PROJECT_NONE, ARG_PROJ, false },
    { "boreale_en_guerre", EC_BOREALE_AT_WAR, ARG_NONE, false }, { "militance_sup", EC_MILITANCY_ABOVE, ARG_NONE, true },
    { "guerre_contre", EC_WAR_WITH, ARG_NATION, false }, { "tech_sup", EC_TECH_ABOVE, ARG_TECH, true },
};
static const ModKey MOD_EFFECTS[] = {
    { "ressource", EE_RES, ARG_RES, true }, { "militance", EE_MIL, ARG_STRATA, true }, { "hostilite", EE_FAC, ARG_FAC, true },
    { "capacite_faction", EE_FACCAP, ARG_FAC, true }, { "relation", EE_REL, ARG_NATION, true },
    { "relation_groupe", EE_RELGROUP, ARG_GROUP, true }, { "capacite", EE_CAP, ARG_DOMAIN, true },
    { "immigres", EE_IMMIG, ARG_NONE, true }, { "dette", EE_DEBT, ARG_NONE, true }, { "strategique", EE_STRAT, ARG_STRAT, true },
    { "menaces", EE_THREAT, ARG_FAC, true }, { "otan", EE_NATO, ARG_NONE, true }, { "credibilite", EE_CRED, ARG_NONE, true },
    { "agression", EE_AGGRO, ARG_NONE, true }, { "recherche", EE_RESEARCH, ARG_NONE, true },
    { "attenuation", EE_MITIG, ARG_NONE, true }, { "conscience", EE_CON, ARG_STRATA, true }, { "prosperite", EE_PROSP, ARG_NONE, true },
};
static const char *ARG_WORDS[][N_COUNT + 1] = {
    [ARG_RES]    = { "budget", "energie", "eau", "alimentation", "cohesion", "influence", "souverainete", NULL },
    [ARG_STRATA] = { "travailleurs", "moyennes", "elites", NULL },
    [ARG_FAC]    = { "boreale", "sables", "hydra", "nebula", NULL },
    [ARG_NATION] = { "france", "uk", "irlande", "benelux", "allemagne", "suisse", "autriche", "italie", "espagne", "portugal",
                     "danemark", "scandinavie", "baltes", "pologne", "europe_centrale", "balkans", "roumanie", "bulgarie", "grece",
                     "turquie", "ukraine", "boreale", "maroc", "algerie", "tunisie", "libye", "egypte", NULL },
    [ARG_GROUP]  = { "ue", "maghreb", "otan", NULL },
    [ARG_DOMAIN] = { "terre", "mer", "air", "espace", "cyber", "info", "em", NULL },
    [ARG_STRAT]  = { "terres_rares", "uranium", "puces", NULL },
    [ARG_PROJ]   = { "smr", "enr", "dessal", "agro", "cyberciv", "medias", "constell", "adapt", NULL },
    [ARG_TECH]   = { "inf", "blindes", "air", "marine", "feux", "ge", "log", "nuc", NULL },
};


static char *Dup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *Trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static int WordIndex(int kind, const char *w)
{
    if (kind == ARG_STRATA && strcmp(w, "toutes") == 0) return -1;
    for (int i = 0; ARG_WORDS[kind][i]; i++) if (strcmp(ARG_WORDS[kind][i], w) == 0) return i;
    return -2;
}

/* lang:ignore-end */

/* « nom [argument] [valeur] » -> terme ; false si la ligne est invalide */
static bool ParseTerm(const ModKey *keys, int count, char *line, EvTerm *out)
{
    char *tok[3] = { NULL, NULL, NULL };
    int n = 0;
    for (char *t = strtok(line, " \t"); t && n < 3; t = strtok(NULL, " \t")) tok[n++] = t;
    if (n == 0) return false;
    for (int k = 0; k < count; k++) {
        if (strcmp(keys[k].name, tok[0]) != 0) continue;
        int i = 1;
        out->type = keys[k].type;
        out->a = 0;
        out->v = 0;
        if (keys[k].arg != ARG_NONE) {
            if (i >= n) return false;
            int a = WordIndex(keys[k].arg, tok[i++]);
            if (a < -1) return false;
            out->a = (signed char)a;
        }
        if (keys[k].value) {
            if (i >= n) return false;
            out->v = (short)atoi(tok[i]);
        }
        return true;
    }
    return false;
}

/* lang:ignore-begin */
int Ev_LoadMods(const char *path)
{
    Ev_Init();
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    int added = 0, nc = 0, na = 0, nb = 0;
    EventDef *e = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *s = Trim(line);
        if (!*s || *s == '#') continue;
        if (strcmp(s, "[evenement]") == 0) {
            if (e && e->title && e->optA && e->optB) { EVENT_COUNT++; added++; }
            e = NULL;
            if (EVENT_COUNT >= EVENT_MAX) break;
            e = &EVENTS[EVENT_COUNT];
            memset(e, 0, sizeof(*e));
            e->weight = 2;
            nc = na = nb = 0;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!e || !eq) continue;
        *eq = 0;
        char *key = Trim(s), *val = Trim(eq + 1);
        if (strcmp(key, "titre") == 0) e->title = Dup(val);
        else if (strcmp(key, "texte") == 0) e->text = Dup(val);
        else if (strcmp(key, "tour_min") == 0) e->minTurn = (unsigned char)atoi(val);
        else if (strcmp(key, "poids") == 0) e->weight = (unsigned char)atoi(val);
        else if (strcmp(key, "option_a") == 0) e->optA = Dup(val);
        else if (strcmp(key, "option_b") == 0) e->optB = Dup(val);
        else if (strcmp(key, "suite_a") == 0) e->nextA = Dup(val);
        else if (strcmp(key, "suite_b") == 0) e->nextB = Dup(val);
        else if (strcmp(key, "delai") == 0) e->delay = (unsigned char)atoi(val);
        else if (strcmp(key, "condition") == 0 && nc < 2) { if (ParseTerm(MOD_CONDS, (int)(sizeof(MOD_CONDS) / sizeof(MOD_CONDS[0])), val, &e->cond[nc])) nc++; }
        else if (strcmp(key, "effet_a") == 0 && na < 4) { if (ParseTerm(MOD_EFFECTS, (int)(sizeof(MOD_EFFECTS) / sizeof(MOD_EFFECTS[0])), val, &e->effA[na])) na++; }
        else if (strcmp(key, "effet_b") == 0 && nb < 4) { if (ParseTerm(MOD_EFFECTS, (int)(sizeof(MOD_EFFECTS) / sizeof(MOD_EFFECTS[0])), val, &e->effB[nb])) nb++; }
    }
    if (e && e->title && e->optA && e->optB && EVENT_COUNT < EVENT_MAX) { EVENT_COUNT++; added++; }
    fclose(f);
    for (int k = EVENT_COUNT - added; k < EVENT_COUNT; k++) if (!EVENTS[k].text) EVENTS[k].text = "";
    return added;
}
/* lang:ignore-end */
