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
    EC_SCENARIO,       /* v : scenario de depart (0 : 2030, 1 : 2040, 2 : Balkans 2035, 3 : Sahel 2050) */
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
      20, 2, { C(EC_TECH_ABOVE, T_GE, 1) },
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
    /* ---------------------------------------------------------------- climat, environnement, santé, outre-mer */
    { "Cyclone sur la Guadeloupe", "Un cyclone de catégorie 5 dévaste la Basse-Terre. L'eau courante est coupée pour 200 000 habitants, les bananeraies sont rasées.",
      2, 3, { C(EC_TEMP_ABOVE, 0, 17) },
      "Pont aérien militaire", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_AIR, -1), E(EE_RES, R_COHESION, 3) },
      "Fonds d'assurance mutualisé", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, -3), E(EE_PROSP, 0, -1) }, "Reconstruction aux normes cycloniques", "Colère dans les Antilles", 3 },
    { "Reconstruction aux normes cycloniques", "Six mois après le cyclone, l'État doit choisir : reconstruire vite ou imposer des normes paracycloniques coûteuses.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Imposer des normes strictes", { E(EE_RES, R_BUDGET, -10), E(EE_PROSP, 0, 2), E(EE_MITIG, 0, 1) },
      "Reconstruire à l'identique", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 2), E(EE_PROSP, 0, -2) }, NULL, NULL, 0 },
    { "Colère dans les Antilles", "Les sinistrés attendent toujours leurs indemnités. Barrages routiers à Pointe-à-Pitre ; des collectifs dénoncent un abandon colonial.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Indemnisation d'urgence", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Envoyer des gendarmes mobiles", { E(EE_RES, R_COHESION, -3), E(EE_CON, S_TRAVAILLEURS, 4), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Orpaillage illégal en Guyane", "Des chercheurs d'or armés liés au Cartel Hydra empoisonnent au mercure le haut Maroni. Les villages amérindiens sont menacés.",
      6, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 15) },
      "Opération militaire en forêt", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -6), E(EE_FAC, F_HYDRA, 6), E(EE_CAP, D_TERRE, -1) },
      "Légaliser une filière d'or encadrée", { E(EE_RES, R_BUDGET, 4), E(EE_FACCAP, F_HYDRA, 4), E(EE_RES, R_EAU, -2) }, "Représailles sur le Maroni", NULL, 3 },
    { "Représailles sur le Maroni", "Une patrouille de légionnaires tombe dans une embuscade. Les orpailleurs se replient au Suriname et promettent de revenir.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Présence militaire permanente", { E(EE_RES, R_BUDGET, -8), E(EE_FACCAP, F_HYDRA, -5) },
      "Coopération avec le Suriname", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, 2), E(EE_FACCAP, F_HYDRA, -2), E(EE_RES, R_SOUVERAINETE, -2) }, NULL, NULL, 0 },
    { "Robinets à sec à Mayotte", "L'eau ne coule plus qu'un jour sur trois à Mayotte. Les files d'attente aux citernes s'allongent et les médecins redoutent une épidémie.",
      3, 3, { C(EC_RES_BELOW, R_EAU, 60) },
      "Usine de dessalement d'urgence", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_EAU, 2), E(EE_RES, R_COHESION, 2) },
      "Navires-citernes de la Marine", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_MER, -1), E(EE_RES, R_COHESION, 1) }, NULL, "Choléra à Mamoudzou", 2 },
    { "Choléra à Mamoudzou", "Les livraisons d'eau n'ont pas suffi : un foyer de choléra se déclare dans les bidonvilles de Mamoudzou.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Campagne sanitaire massive", { E(EE_RES, R_BUDGET, -7), E(EE_RES, R_COHESION, 2) },
      "Mettre l'île en quarantaine", { E(EE_RES, R_BUDGET, -2), E(EE_RES, R_COHESION, -3), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Nickel calédonien et lagon pollué", "Les mines de nickel de Nouvelle-Calédonie déversent latérite et métaux lourds dans le lagon classé. Les tribus kanak bloquent les sites.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Suspendre les mines fautives", { E(EE_RES, R_BUDGET, -6), E(EE_PROSP, 0, -2), E(EE_RES, R_COHESION, 3) },
      "Maintenir la production", { E(EE_RES, R_BUDGET, 5), E(EE_RES, R_EAU, -2), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_RES, R_INFLUENCE, -2) }, NULL, "Blocage des mines de la Grande Terre", 3 },
    { "Blocage des mines de la Grande Terre", "Les barrages se durcissent ; une usine métallurgique est occupée. Des drapeaux indépendantistes flottent sur les engins.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Partager les revenus miniers", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_COHESION, 3), E(EE_RES, R_SOUVERAINETE, -2) },
      "Évacuer les sites par la force", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_RES, R_COHESION, -5), E(EE_CON, S_TRAVAILLEURS, 4) }, NULL, NULL, 0 },
    { "Les assureurs jettent l'éponge", "Les grands assureurs refusent désormais de couvrir les logements en zone inondable ou argileuse. Trois millions de foyers sont sans garantie.",
      14, 3, { C(EC_TEMP_ABOVE, 0, 20) },
      "Régime public des catastrophes", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_COHESION, 3), E(EE_DEBT, 0, 5) },
      "Laisser le marché tarifer le risque", { E(EE_MIL, S_MOYENNES, 6), E(EE_PROSP, 0, 1), E(EE_RES, R_COHESION, -3) }, "Le fonds catastrophes à sec", "Villes fantômes sur la côte", 5 },
    { "Le fonds catastrophes à sec", "Trois sinistres majeurs en un an ont vidé le fonds public. Bercy propose de doubler la surprime sur tous les contrats.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Doubler la surprime", { E(EE_RES, R_BUDGET, 5), E(EE_MIL, S_MOYENNES, 4), E(EE_RES, R_COHESION, -2) },
      "Emprunter pour renflouer", { E(EE_DEBT, 0, 15), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },
    { "Villes fantômes sur la côte", "Sans assurance, les maisons du littoral ne valent plus rien. Des quartiers entiers se vident et plusieurs communes font faillite.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Racheter les biens menacés", { E(EE_RES, R_BUDGET, -10), E(EE_MITIG, 0, 1), E(EE_RES, R_COHESION, 2) },
      "Placer les communes sous tutelle", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_COHESION, -3), E(EE_MIL, S_MOYENNES, 4) }, NULL, NULL, 0 },
    { "Réserves d'eau géantes contestées", "Les irrigants du Poitou veulent creuser vingt bassines géantes remplies en pompant les nappes l'hiver. Les opposants promettent d'occuper les chantiers.",
      4, 2, { C(EC_RES_BELOW, R_EAU, 65) },
      "Autoriser les réserves", { E(EE_RES, R_ALIM, 4), E(EE_RES, R_EAU, -3), E(EE_MIL, S_MOYENNES, 4) },
      "Moratoire et sobriété agricole", { E(EE_RES, R_EAU, 3), E(EE_RES, R_ALIM, -3), E(EE_MIL, S_TRAVAILLEURS, 4) }, "Affrontements sur un chantier de bassine", NULL, 2 },
    { "Affrontements sur un chantier de bassine", "Trente mille manifestants assiègent un chantier ; des centaines de blessés. Les images font le tour de l'Europe.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Poursuivre les travaux sous escorte", { E(EE_RES, R_ALIM, 2), E(EE_RES, R_COHESION, -4), E(EE_CON, S_MOYENNES, 3) },
      "Suspendre et ouvrir une concertation", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_ALIM, -3), E(EE_MIL, S_ELITES, 3) }, NULL, NULL, 0 },
    { "Polluants éternels au sud de Lyon", "Les « polluants éternels » d'une usine chimique contaminent l'eau potable de 200 communes. L'industriel menace de délocaliser.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Fermeture et dépollution", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_EAU, 3), E(EE_PROSP, 0, -2), E(EE_RES, R_COHESION, 2) },
      "Mise en conformité progressive", { E(EE_RES, R_EAU, -2), E(EE_PROSP, 0, 1), E(EE_CON, S_MOYENNES, 3) }, "Le chimiste part en Asie", NULL, 3 },
    { "Le chimiste part en Asie", "Le groupe chimique ferme trois sites et transfère sa production. Deux mille emplois disparaissent dans la vallée de la chimie.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Reprise publique des sites", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_RES, R_SOUVERAINETE, 2) },
      "Reconversion vers la chimie verte", { E(EE_RES, R_BUDGET, -5), E(EE_PROSP, 0, 1), E(EE_MIL, S_TRAVAILLEURS, 4) }, NULL, NULL, 0 },
    { "L'Espagne veut l'eau du Rhône", "Madrid propose un aqueduc géant pour détourner une part du Rhône vers la Catalogne asséchée, en échange d'électricité solaire.",
      36, 2, { C(EC_TEMP_ABOVE, 0, 23), C(EC_ALIVE, N_ESPAGNE, 0) },
      "Accepter l'aqueduc", { E(EE_RES, R_EAU, -4), E(EE_RES, R_ENERGIE, 4), E(EE_REL, N_ESPAGNE, 10) },
      "Refuser au nom de nos besoins", { E(EE_REL, N_ESPAGNE, -10), E(EE_RES, R_EAU, 1), E(EE_RES, R_SOUVERAINETE, 2) }, "L'aqueduc du Rhône en service", NULL, 6 },
    { "L'aqueduc du Rhône en service", "L'aqueduc fonctionne, mais l'été suivant le débit du Rhône s'effondre. Les riziculteurs de Camargue exigent de fermer les vannes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Fermer les vannes l'été", { E(EE_RES, R_EAU, 3), E(EE_REL, N_ESPAGNE, -8), E(EE_RES, R_ENERGIE, -2) },
      "Honorer le contrat", { E(EE_RES, R_EAU, -3), E(EE_RES, R_ALIM, -2), E(EE_REL, N_ESPAGNE, 6), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Chlordécone : l'État condamné", "La justice reconnaît la responsabilité de l'État dans l'empoisonnement des sols martiniquais par un pesticide. Les victimes réclament réparation.",
      4, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Indemniser toutes les victimes", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_COHESION, 4), E(EE_RES, R_INFLUENCE, 2) },
      "Faire appel du jugement", { E(EE_RES, R_BUDGET, -2), E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 3) }, NULL, NULL, 0 },
    { "Marée de sargasses", "Des bancs géants de sargasses s'échouent en Martinique et dégagent un gaz toxique. Écoles fermées, touristes en fuite.",
      6, 2, { C(EC_TEMP_ABOVE, 0, 18) },
      "Barrages flottants et collecte", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 2) },
      "Valoriser les algues en biomasse", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_ENERGIE, 2), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Déluge sur les hauts de La Réunion", "Deux mètres de pluie en quatre jours sur les hauts de La Réunion. Les ravines emportent routes et ponts, des cirques entiers sont isolés.",
      10, 2, { C(EC_TEMP_ABOVE, 0, 19) },
      "Reconstruire routes et ponts", { E(EE_RES, R_BUDGET, -12), E(EE_PROSP, 0, 2) },
      "Téléphériques et relogement", { E(EE_RES, R_BUDGET, -8), E(EE_MITIG, 0, 1), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Le corail polynésien blanchit", "Quatre-vingts pour cent des récifs de Polynésie ont blanchi en un été. La pêche lagonaire et le tourisme s'effondrent.",
      20, 2, { C(EC_TEMP_ABOVE, 0, 21) },
      "Sanctuaire marin intégral", { E(EE_RES, R_BUDGET, -5), E(EE_MITIG, 0, 1), E(EE_RES, R_INFLUENCE, 3), E(EE_RES, R_ALIM, -2) },
      "Soutenir les pêcheurs", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ALIM, 2), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Atolls des Tuamotu inhabitables", "La mer envahit les premiers atolls polynésiens. Quinze mille habitants doivent être relogés à Tahiti ou en métropole.",
      50, 2, { C(EC_SEA_ABOVE, 0, 60) },
      "Relogement en métropole", { E(EE_IMMIG, 0, 1), E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 2) },
      "Îles flottantes artificielles", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_INFLUENCE, 4), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "La forêt guyanaise en feu", "La sécheresse amazonienne gagne la Guyane : des incendies dévorent la forêt primaire. La fumée menace les lancements à Kourou.",
      60, 2, { C(EC_TEMP_ABOVE, 0, 28) },
      "Moyens aériens de métropole", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_AIR, -1), E(EE_MITIG, 0, 1) },
      "Protéger seulement Kourou", { E(EE_RES, R_BUDGET, -3), E(EE_MITIG, 0, -2), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Paris à 50 degrés", "Pour la première fois, Paris dépasse 50 degrés. Le métro est arrêté, des personnes âgées meurent seules sous les toits.",
      40, 2, { C(EC_TEMP_ABOVE, 0, 25) },
      "Îlots de fraîcheur et réquisitions", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_ENERGIE, -3), E(EE_RES, R_COHESION, 3) },
      "Évacuer les fragiles en province", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_COHESION, -2), E(EE_MIL, S_MOYENNES, 4) }, NULL, NULL, 0 },
    { "Le Rhône trop chaud pour les réacteurs", "Le Rhône dépasse 28 degrés : les centrales nucléaires de la vallée doivent baisser la puissance ou rejeter de l'eau brûlante.",
      24, 3, { C(EC_TEMP_ABOVE, 0, 21) },
      "Dérogation thermique", { E(EE_RES, R_ENERGIE, 3), E(EE_RES, R_EAU, -3), E(EE_RES, R_ALIM, -2) },
      "Réduire la production", { E(EE_RES, R_ENERGIE, -5), E(EE_RES, R_BUDGET, -3), E(EE_RES, R_ALIM, 1), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Paludisme autochtone en Camargue", "Chaleur et moustiques anophèles : des cas de paludisme contracté sur place apparaissent en Camargue et dans l'Hérault.",
      46, 2, { C(EC_TEMP_ABOVE, 0, 26) },
      "Assécher une partie des marais", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ALIM, 1), E(EE_MITIG, 0, -1), E(EE_RES, R_COHESION, 1) },
      "Plan sanitaire et moustiquaires", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },
    { "Effondrement des pollinisateurs", "Les scientifiques mesurent une chute des trois quarts des insectes pollinisateurs. Vergers et champs de colza produisent de moins en moins.",
      16, 2, { C(EC_TEMP_ABOVE, 0, 19) },
      "Interdire les pesticides restants", { E(EE_RES, R_ALIM, -3), E(EE_MITIG, 0, 1), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_COHESION, 1) },
      "Pollinisation par microdrones", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_ALIM, 2), E(EE_RES, R_SOUVERAINETE, 1) }, NULL, NULL, 0 },
    { "Crue centennale de la Seine", "La Seine atteint huit mètres et demi à Paris. Le métro est noyé, des quartiers entiers de la capitale privés d'électricité pour un mois.",
      18, 2, { C(EC_TEMP_ABOVE, 0, 20) },
      "Mobiliser l'armée", { E(EE_RES, R_BUDGET, -10), E(EE_CAP, D_TERRE, -1), E(EE_RES, R_COHESION, 3) },
      "Déplacer des ministères en région", { E(EE_RES, R_BUDGET, -6), E(EE_PROSP, 0, 2), E(EE_MIL, S_ELITES, 4) }, NULL, NULL, 0 },
    { "Le port du Havre menacé", "La montée des eaux et les surcotes rendent les terminaux du Havre inutilisables une semaine par mois. Les armateurs lorgnent Anvers.",
      30, 2, { C(EC_SEA_ABOVE, 0, 45) },
      "Rehausser le port de deux mètres", { E(EE_RES, R_BUDGET, -14), E(EE_PROSP, 0, 3) },
      "Transférer le trafic vers Rouen", { E(EE_RES, R_BUDGET, -5), E(EE_PROSP, 0, -2), E(EE_REL, N_BENELUX, 5) }, NULL, NULL, 0 },
    { "Le Maroc à court de pluie", "Trois années sans pluie ruinent l'agriculture marocaine. Rabat demande du blé français et des ingénieurs en irrigation.",
      28, 2, { C(EC_TEMP_ABOVE, 0, 22), C(EC_ALIVE, N_MAROC, 0) },
      "Livrer blé et ingénieurs", { E(EE_RES, R_ALIM, -4), E(EE_REL, N_MAROC, 12), E(EE_RES, R_INFLUENCE, 3) },
      "Proposer des prêts au prix du marché", { E(EE_RES, R_BUDGET, 3), E(EE_REL, N_MAROC, -6) }, NULL, NULL, 0 },
    { "Engrais azotés introuvables", "La Fédération Boréale coupe ses exportations d'ammoniac. Le prix des engrais triple à l'approche des semis.",
      6, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 30) },
      "Basculer vers l'agriculture biologique", { E(EE_RES, R_ALIM, -4), E(EE_MITIG, 0, 1), E(EE_RES, R_SOUVERAINETE, 2) },
      "Acheter au marché noir", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ALIM, 2), E(EE_FACCAP, F_HYDRA, 4) }, NULL, NULL, 0 },
    { "Potagers obligatoires", "Le régime impose la culture vivrière de toutes les pelouses publiques et privées. Les contrevenants paieront de lourdes amendes.",
      10, 2, { C(EC_AUTHORITARIAN, 0, 0), C(EC_RES_BELOW, R_ALIM, 60) },
      "Appliquer avec rigueur", { E(EE_RES, R_ALIM, 4), E(EE_MIL, S_MOYENNES, 5), E(EE_CON, S_MOYENNES, 3) },
      "Encourager sans contraindre", { E(EE_RES, R_ALIM, 1), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Convention citoyenne sur l'adaptation", "Cent cinquante citoyens tirés au sort remettent cent mesures pour adapter le pays au climat. Ils exigent une reprise sans filtre.",
      20, 2, { C(EC_DEMOCRACY, 0, 0), C(EC_TEMP_ABOVE, 0, 20) },
      "Reprendre les mesures sans filtre", { E(EE_RES, R_BUDGET, -8), E(EE_MITIG, 0, 2), E(EE_RES, R_COHESION, 3), E(EE_MIL, S_ELITES, 5) },
      "Retenir quelques mesures", { E(EE_MITIG, 0, 1), E(EE_RES, R_COHESION, -2), E(EE_CON, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Quartiers résilients livrés", "Grâce au plan d'adaptation, les premiers quartiers résilients sont livrés : toits végétalisés, réseaux de froid, rues inondables. D'autres villes réclament leur part.",
      20, 2, { C(EC_PROJECT_DONE, P_ADAPT, 0) },
      "Étendre à cinquante villes", { E(EE_RES, R_BUDGET, -10), E(EE_PROSP, 0, 3), E(EE_RES, R_COHESION, 2) },
      "Exporter le savoir-faire", { E(EE_RES, R_BUDGET, 5), E(EE_RES, R_INFLUENCE, 3), E(EE_MIL, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Saumure brûlante en Méditerranée", "Les usines de dessalement rejettent une saumure chaude qui tue les herbiers de posidonie. Pêcheurs et écologistes manifestent à Toulon.",
      16, 2, { C(EC_PROJECT_DONE, P_DESSAL, 0) },
      "Diluer les rejets au large", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 2) },
      "Ignorer les protestations", { E(EE_RES, R_EAU, 2), E(EE_MIL, S_MOYENNES, 4), E(EE_RES, R_ALIM, -1) }, NULL, NULL, 0 },
    { "Sols épuisés dans la Beauce", "Sans plan agroécologique, les sols de la Beauce ne retiennent plus l'eau. Les rendements de blé reculent pour la cinquième année.",
      28, 2, { C(EC_PROJECT_NONE, P_AGRO, 0), C(EC_TEMP_ABOVE, 0, 22) },
      "Couverts végétaux obligatoires", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ALIM, 2), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Engrais et irrigation intensifs", { E(EE_RES, R_ALIM, 3), E(EE_RES, R_EAU, -4), E(EE_MITIG, 0, -1) }, NULL, NULL, 0 },
    { "Nouméa face à la mer", "Les quartiers bas de Nouméa sont inondés à chaque grande marée. Le Congrès calédonien demande un plan de digues à la métropole.",
      44, 2, { C(EC_SEA_ABOVE, 0, 50) },
      "Financer les digues", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_SOUVERAINETE, 2), E(EE_RES, R_COHESION, 2) },
      "Laisser la collectivité décider", { E(EE_RES, R_BUDGET, -2), E(EE_RES, R_SOUVERAINETE, -3), E(EE_RES, R_INFLUENCE, 1) }, NULL, NULL, 0 },
    { "Canicule marine en Méditerranée", "L'eau atteint 31 degrés au large de Marseille. Les gorgones et les mérous meurent en masse, les pêcheurs côtiers sont ruinés.",
      34, 2, { C(EC_TEMP_ABOVE, 0, 24) },
      "Aires marines protégées élargies", { E(EE_RES, R_BUDGET, -5), E(EE_MITIG, 0, 1), E(EE_RES, R_ALIM, -2), E(EE_RES, R_INFLUENCE, 2) },
      "Aquaculture en mer profonde", { E(EE_RES, R_BUDGET, -7), E(EE_RES, R_ALIM, 3), E(EE_RES, R_EAU, -1) }, NULL, NULL, 0 },
    { "Le Nord sous la mer ?", "Les wateringues de Flandre ne suffisent plus : la plaine entre Dunkerque et Calais, sous le niveau de la mer, est inondée tout l'hiver.",
      56, 2, { C(EC_SEA_ABOVE, 0, 70) },
      "Pompes géantes et polders", { E(EE_RES, R_BUDGET, -14), E(EE_RES, R_ENERGIE, -2), E(EE_RES, R_COHESION, 2) },
      "Rendre la plaine à la mer", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ALIM, -3), E(EE_RES, R_COHESION, -3), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    /* ---------------------------------------------------------------- societe et politique interieure */
    { "Réforme des retraites à 67 ans", "Le Conseil d'orientation des retraites prévoit un déficit record en 2035. Le gouvernement veut reculer l'âge légal de deux ans.",
      12, 2, { C(EC_DEMOCRACY, 0, 0), C(EC_DEBT_ABOVE, 0, 80) },
      "Passer la réforme en force", { E(EE_RES, R_BUDGET, 7), E(EE_MIL, S_TRAVAILLEURS, 8), E(EE_CON, S_TRAVAILLEURS, 3) },
      "Ouvrir une conférence sociale", { E(EE_RES, R_BUDGET, -4), E(EE_DEBT, 0, 10), E(EE_RES, R_COHESION, 2) },
      "Grève générale reconductible", "Pacte entre les générations", 3 },
    { "Grève générale reconductible", "Trains, ports et écoles sont à l'arrêt depuis trois semaines. Les syndicats exigent le retrait pur et simple du texte.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Retirer la réforme", { E(EE_RES, R_BUDGET, -6), E(EE_MIL, S_TRAVAILLEURS, -10), E(EE_MIL, S_ELITES, 4) },
      "Tenir malgré la paralysie", { E(EE_PROSP, 0, -3), E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 3) },
      NULL, NULL, 0 },
    { "Pacte entre les générations", "La conférence sociale propose un compromis : départ progressif, mais une cotisation exceptionnelle sur les hauts patrimoines.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Adopter le compromis", { E(EE_RES, R_BUDGET, 4), E(EE_MIL, S_ELITES, 6), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Épargner les patrimoines", { E(EE_RES, R_BUDGET, -3), E(EE_MIL, S_MOYENNES, 4), E(EE_CON, S_MOYENNES, 3) },
      NULL, NULL, 0 },
    { "Entrepôts sans humains", "Un géant de la logistique remplace 40 000 manutentionnaires par des robots autonomes. Les bassins d'emploi du Nord sont sonnés.",
      20, 3, { C(EC_UNEMP_ABOVE, 0, 9) },
      "Taxer chaque robot installé", { E(EE_RES, R_BUDGET, 5), E(EE_PROSP, 0, -2), E(EE_MIL, S_ELITES, 5) },
      "Laisser l'automatisation suivre", { E(EE_PROSP, 0, 3), E(EE_MIL, S_TRAVAILLEURS, 6) },
      "Les robots partent en Pologne", "Les briseurs de robots", 4 },
    { "Les robots partent en Pologne", "Pour fuir la taxe robot, le logisticien transfère ses plateformes automatisées de l'autre côté du Rhin et en Pologne.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Plaider une taxe robot européenne", { E(EE_RES, R_INFLUENCE, -3), E(EE_RELGROUP, 0, -4), E(EE_RES, R_BUDGET, 3) },
      "Supprimer la taxe", { E(EE_PROSP, 0, 2), E(EE_RES, R_BUDGET, -5), E(EE_CON, S_TRAVAILLEURS, 3) },
      NULL, NULL, 0 },
    { "Les briseurs de robots", "Des ex-salariés sabotent des robots de livraison à Lille et Roubaix. Le mouvement se revendique des luddites du XIXe siècle.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Fonds de reconversion massif", { E(EE_RES, R_BUDGET, -10), E(EE_MIL, S_TRAVAILLEURS, -8), E(EE_PROSP, 0, 1) },
      "Poursuites pénales", { E(EE_RES, R_COHESION, -3), E(EE_CON, S_TRAVAILLEURS, 4), E(EE_MIL, S_ELITES, -5) },
      NULL, NULL, 0 },
    { "Journalistes sur écoute", "Un hebdomadaire révèle que la police a placé sur écoute une dizaine de journalistes enquêtant sur des marchés publics.",
      8, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Commission d'enquête parlementaire", { E(EE_RES, R_COHESION, -2), E(EE_CON, S_MOYENNES, 3), E(EE_RES, R_INFLUENCE, 2) },
      "Invoquer le secret défense", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_RES, R_COHESION, -4), E(EE_MIL, S_MOYENNES, 4) },
      "Le rapport accablant", "Les enregistrements fuitent", 3 },
    { "Le rapport accablant", "La commission établit que le ministre de l'Intérieur savait. L'opposition réclame sa démission et une loi sur les sources.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Démission et loi sur les sources", { E(EE_RES, R_COHESION, 4), E(EE_CAP, D_INFO, -1), E(EE_MIL, S_MOYENNES, -6) },
      "Soutenir le ministre", { E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 3), E(EE_MIL, S_ELITES, -5) },
      NULL, NULL, 0 },
    { "Les enregistrements fuitent", "Des heures d'écoutes circulent en ligne, retouchées par endroits. Le Réseau Nébula amplifie l'affaire sur toutes les plateformes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Tout publier et s'excuser", { E(EE_RES, R_COHESION, -2), E(EE_FAC, F_NEBULA, 4), E(EE_CON, -1, 3) },
      "Dénoncer une manipulation", { E(EE_RES, R_COHESION, -5), E(EE_FACCAP, F_NEBULA, 4), E(EE_MIL, S_MOYENNES, 5) },
      NULL, NULL, 0 },
    { "Un million de logements vides", "Alors que 300 000 personnes dorment dehors, un collectif occupe des tours de bureaux vides à La Défense et exige leur réquisition.",
      6, 3, { C(EC_DEMOCRACY, 0, 0), C(EC_RES_BELOW, R_COHESION, 60) },
      "Réquisitionner les bureaux vides", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 4), E(EE_MIL, S_ELITES, 5) },
      "Évacuer les tours", { E(EE_RES, R_COHESION, -3), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_CON, S_TRAVAILLEURS, 3) },
      "La fronde des propriétaires", NULL, 3 },
    { "La fronde des propriétaires", "Les foncières attaquent les réquisitions devant le Conseil constitutionnel et gèlent leurs investissements en France.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Indemniser généreusement", { E(EE_RES, R_BUDGET, -9), E(EE_MIL, S_ELITES, -8) },
      "Tenir bon, loi sur le logement", { E(EE_PROSP, 0, -2), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_CON, S_ELITES, 3) },
      NULL, NULL, 0 },
    { "Candidature à la Coupe du monde 2046", "La fédération de football veut organiser le Mondial 2046 avec des matchs en soirée pour éviter les canicules.",
      28, 1, { C(EC_PEACE, 0, 0) },
      "Porter la candidature", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, 4), E(EE_RES, R_COHESION, 2) },
      "Refuser : priorité au climat", { E(EE_MITIG, 0, 1), E(EE_MIL, S_MOYENNES, 3) },
      "Les stades ne sont pas prêts", NULL, 5 },
    { "Les stades ne sont pas prêts", "Pénurie de béton bas carbone et grèves sur les chantiers : trois stades ont un an de retard à dix-huit mois du coup d'envoi.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rallonge budgétaire", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_INFLUENCE, 2) },
      "Réduire à huit stades", { E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_BUDGET, -3), E(EE_MIL, S_MOYENNES, 3) },
      "Finale au Stade de France", NULL, 4 },
    { "Finale au Stade de France", "Les Bleus sont en finale à domicile. Le pays entier est devant les écrans ; la sécurité redoute un attentat.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Fan-zones géantes dans les villes", { E(EE_RES, R_COHESION, 5), E(EE_THREAT, F_SABLES, 1), E(EE_MIL, -1, -5) },
      "Huis clos sécuritaire", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_BUDGET, -4), E(EE_CON, S_MOYENNES, 3) },
      NULL, NULL, 0 },
    { "La Guyane se soulève", "Vie chère, orpaillage illégal et coupures d'eau : des barrages paralysent Cayenne et la route du centre spatial.",
      10, 2, { C(EC_MILITANCY_ABOVE, 0, 25) },
      "Envoyer une mission de dialogue", { E(EE_RES, R_BUDGET, -5), E(EE_MIL, S_TRAVAILLEURS, -5) },
      "Envoyer les gendarmes mobiles", { E(EE_CAP, D_TERRE, -1), E(EE_RES, R_SOUVERAINETE, 2), E(EE_CON, S_TRAVAILLEURS, 3) },
      "Statut d'autonomie pour la Guyane", "Blocus de Kourou", 3 },
    { "Statut d'autonomie pour la Guyane", "Les élus guyanais réclament un statut d'autonomie avec pouvoir normatif sur les mines et le foncier.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Accorder l'autonomie", { E(EE_RES, R_SOUVERAINETE, -3), E(EE_RES, R_COHESION, 3), E(EE_FACCAP, F_HYDRA, 3) },
      "Plan d'investissement à la place", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -4) },
      NULL, NULL, 0 },
    { "Blocus de Kourou", "Les manifestants encerclent le centre spatial : un lancement de satellite militaire est reporté sine die.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Négocier et payer", { E(EE_RES, R_BUDGET, -7), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_RES, R_SOUVERAINETE, -2) },
      "Dégager le site par la force", { E(EE_CAP, D_ESPACE, 1), E(EE_RES, R_COHESION, -5), E(EE_CON, -1, 3) },
      NULL, NULL, 0 },
    { "Tuberculose résistante à Marseille", "Une souche de tuberculose résistante à tous les antibiotiques se propage dans les quartiers nord. Les hôpitaux saturent.",
      14, 2, { C(EC_TEMP_ABOVE, 0, 17) },
      "Dépistage obligatoire", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 2), E(EE_CON, S_TRAVAILLEURS, 3) },
      "Dépistage volontaire ciblé", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_COHESION, -3), E(EE_PROSP, 0, -1) },
      "Fronde contre le dépistage", NULL, 3 },
    { "Fronde contre le dépistage", "Des milliers de personnes refusent les tests, dénonçant une stigmatisation des quartiers populaires et des migrants.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Médiateurs de santé de quartier", { E(EE_RES, R_BUDGET, -5), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Sanctions pour les réfractaires", { E(EE_RES, R_COHESION, -3), E(EE_MIL, S_TRAVAILLEURS, 6), E(EE_MIL, S_MOYENNES, -4) },
      NULL, NULL, 0 },
    { "L'algorithme d'orientation déraille", "Une IA d'affectation universitaire a recalé 80 000 bacheliers sur des critères opaques. Les lycéens défilent par milliers.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Revenir à un tirage au sort", { E(EE_RES, R_COHESION, 2), E(EE_MIL, S_MOYENNES, 4), E(EE_PROSP, 0, -1) },
      "Ouvrir 50 000 places en urgence", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_MOYENNES, -6), E(EE_PROSP, 0, 1) },
      NULL, NULL, 0 },
    { "L'uniforme à l'école", "Après une série de rixes entre collèges, le ministre veut imposer l'uniforme et le salut au drapeau chaque lundi.",
      4, 2, { C(EC_RES_BELOW, R_COHESION, 55) },
      "Généraliser l'uniforme", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 3), E(EE_MIL, S_MOYENNES, 3) },
      "Recruter des éducateurs", { E(EE_RES, R_BUDGET, -7), E(EE_MIL, S_TRAVAILLEURS, -4) },
      NULL, NULL, 0 },
    { "Financement étranger d'une grande mosquée", "Un émirat du Golfe propose de financer entièrement une grande mosquée à Lyon. La loi de 1905 interdit toute aide publique.",
      6, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Accepter sous contrôle", { E(EE_RES, R_INFLUENCE, 3), E(EE_RES, R_SOUVERAINETE, -3), E(EE_MIL, S_MOYENNES, 4) },
      "Créer une fondation nationale", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_SOUVERAINETE, 2), E(EE_RELGROUP, 1, -4) },
      NULL, NULL, 0 },
    { "La secte des Derniers Jours", "Une secte apocalyptique climatique recrute des milliers de jeunes et achète des fermes fortifiées dans les Cévennes.",
      24, 2, { C(EC_TEMP_ABOVE, 0, 19) },
      "Dissoudre la secte", { E(EE_RES, R_COHESION, 2), E(EE_CON, S_MOYENNES, 3), E(EE_CAP, D_TERRE, -1) },
      "Surveiller sans interdire", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_COHESION, -3), E(EE_MIL, S_MOYENNES, 3) },
      NULL, NULL, 0 },
    { "Un milliardaire rachète l'audiovisuel", "Un industriel de l'armement rachète deux chaînes d'info et une radio nationale. Les rédactions entrent en grève.",
      6, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Loi anti-concentration", { E(EE_MIL, S_ELITES, 6), E(EE_RES, R_COHESION, 2), E(EE_CON, S_MOYENNES, 3) },
      "Laisser faire le marché des médias", { E(EE_CAP, D_INFO, 1), E(EE_RES, R_COHESION, -3), E(EE_MIL, S_MOYENNES, 4) },
      NULL, NULL, 0 },
    { "Le présentateur synthétique", "Le journal de 20 heures le plus regardé est désormais présenté par un avatar d'IA. Le public adore, les journalistes s'alarment.",
      26, 2, { C(EC_NONE, 0, 0) },
      "Imposer un label « humain »", { E(EE_RES, R_BUDGET, -3), E(EE_FAC, F_NEBULA, 3), E(EE_MIL, S_ELITES, 3) },
      "Accompagner l'innovation", { E(EE_PROSP, 0, 1), E(EE_FACCAP, F_NEBULA, 3), E(EE_MIL, S_MOYENNES, 3) },
      NULL, NULL, 0 },
    { "Reconnaissance faciale dans le métro", "La préfecture veut généraliser la reconnaissance faciale en temps réel dans les transports franciliens.",
      10, 3, { C(EC_THREATS_ABOVE, 0, 2) },
      "Généraliser le dispositif", { E(EE_RES, R_BUDGET, -5), E(EE_THREAT, F_SABLES, -1), E(EE_CON, S_MOYENNES, 3) },
      "L'interdire au nom des libertés", { E(EE_RES, R_COHESION, 2), E(EE_FACCAP, F_SABLES, 3), E(EE_MIL, S_ELITES, -4) },
      NULL, NULL, 0 },
    { "Prisons à 160 pour cent", "Les maisons d'arrêt dépassent 160 pour cent d'occupation. Une mutinerie à Fresnes fait deux morts parmi les surveillants.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Construire 15 000 places", { E(EE_RES, R_BUDGET, -12), E(EE_FACCAP, F_HYDRA, -3) },
      "Peines alternatives massives", { E(EE_RES, R_BUDGET, -3), E(EE_FACCAP, F_HYDRA, 3), E(EE_MIL, S_MOYENNES, 4) },
      NULL, NULL, 0 },
    { "Drones de police municipale", "Nice et Perpignan équipent leurs polices municipales de drones armés de gaz lacrymogène pour les nuits d'émeutes.",
      16, 2, { C(EC_MILITANCY_ABOVE, 0, 30) },
      "Autoriser par décret", { E(EE_MIL, S_TRAVAILLEURS, -5), E(EE_CON, S_TRAVAILLEURS, 4), E(EE_RES, R_COHESION, -2) },
      "Interdire les drones armés", { E(EE_MIL, S_MOYENNES, 4), E(EE_RES, R_COHESION, 1) },
      NULL, NULL, 0 },
    { "Pétition pour un référendum d'initiative", "Quatre millions de signatures réclament un référendum d'initiative citoyenne inscrit dans la Constitution.",
      10, 2, { C(EC_DEMOCRACY, 0, 0), C(EC_MILITANCY_ABOVE, 0, 25) },
      "Réviser la Constitution", { E(EE_MIL, -1, -6), E(EE_CON, -1, 3), E(EE_MIL, S_ELITES, 4) },
      "Grand débat sans suite", { E(EE_RES, R_BUDGET, -3), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_COHESION, -2) },
      NULL, NULL, 0 },
    { "Écoles immersives en breton", "Le Conseil d'État censure l'enseignement immersif en breton. À Quimper et Rennes, 50 000 personnes manifestent.",
      4, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Loi sur les langues régionales", { E(EE_RES, R_COHESION, 3), E(EE_RES, R_SOUVERAINETE, -2), E(EE_MIL, S_ELITES, 3) },
      "Respecter la décision", { E(EE_RES, R_COHESION, -3), E(EE_MIL, S_MOYENNES, 4) },
      NULL, NULL, 0 },
    { "Mayotte au point de rupture", "Les arrivées de kwassa-kwassa doublent après une sécheresse aux Comores. Les dispensaires et les écoles de Mayotte débordent.",
      12, 2, { C(EC_TEMP_ABOVE, 0, 17) },
      "Pont aérien vers la métropole", { E(EE_RES, R_BUDGET, -6), E(EE_IMMIG, 0, 2), E(EE_MIL, S_MOYENNES, 3) },
      "Blocus naval de l'archipel", { E(EE_CAP, D_MER, -1), E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_SOUVERAINETE, 2) },
      NULL, NULL, 0 },
    { "Service civique obligatoire", "Le gouvernement propose six mois de service civique obligatoire à 18 ans : incendies, digues, aide aux aînés.",
      8, 2, { C(EC_RES_BELOW, R_COHESION, 60) },
      "L'instaurer", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 4), E(EE_MIL, S_MOYENNES, 3) },
      "Volontariat rémunéré", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 1) },
      NULL, NULL, 0 },
    { "Tour de France sous 45 degrés", "Trois coureurs s'effondrent dans l'ascension du Ventoux par 45 degrés. Les sponsors menacent de se retirer.",
      12, 2, { C(EC_TEMP_ABOVE, 0, 18) },
      "Déplacer le Tour en septembre", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, -2), E(EE_MITIG, 0, 1) },
      "Maintenir la tradition de juillet", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_INFLUENCE, -3), E(EE_CON, S_MOYENNES, 3) },
      NULL, NULL, 0 },
    { "Les intermittents bloquent Avignon", "Face à l'IA générative qui écrit, joue et compose, les intermittents du spectacle annulent le festival d'Avignon.",
      18, 2, { C(EC_UNEMP_ABOVE, 0, 8) },
      "Quota d'oeuvres humaines", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_INFLUENCE, 3), E(EE_PROSP, 0, -1) },
      "Refuser tout protectionnisme", { E(EE_PROSP, 0, 1), E(EE_RES, R_INFLUENCE, -4), E(EE_MIL, S_MOYENNES, 4) },
      NULL, NULL, 0 },
    { "Contrat d'intégration renforcé", "Le gouvernement veut conditionner les titres de séjour à 600 heures de français et à un examen d'éducation civique.",
      8, 3, { C(EC_NONE, 0, 0) },
      "Cours gratuits et examen", { E(EE_RES, R_BUDGET, -7), E(EE_RES, R_COHESION, 3) },
      "Examen sans cours financés", { E(EE_RES, R_BUDGET, 2), E(EE_RELGROUP, 1, -5), E(EE_MIL, S_TRAVAILLEURS, 3) },
      NULL, NULL, 0 },
    { "Étudiants à la rue", "À la rentrée, 60 000 étudiants n'ont pas de logement. Certains dorment dans les amphithéâtres de Lyon et Bordeaux.",
      4, 2, { C(EC_NONE, 0, 0) },
      "Résidences modulaires d'urgence", { E(EE_RES, R_BUDGET, -7), E(EE_MIL, S_MOYENNES, -5) },
      "Encadrer les loyers étudiants", { E(EE_MIL, S_ELITES, 4), E(EE_PROSP, 0, -1), E(EE_RES, R_COHESION, 2) },
      NULL, NULL, 0 },
    { "Juge artificiel pour les petits litiges", "Pour résorber trois ans de retard, la Chancellerie veut confier les litiges de moins de 5 000 euros à une IA judiciaire.",
      22, 2, { C(EC_NONE, 0, 0) },
      "Déployer l'IA judiciaire", { E(EE_RES, R_BUDGET, 3), E(EE_CON, S_MOYENNES, 3), E(EE_FACCAP, F_NEBULA, 3) },
      "Recruter 2 000 magistrats", { E(EE_RES, R_BUDGET, -9), E(EE_RES, R_COHESION, 2) },
      NULL, NULL, 0 },
    { "Universités sous tutelle", "Le régime nomme des recteurs-commissaires pour surveiller cours et colloques. Des chercheurs s'exilent à Genève et Montréal.",
      4, 3, { C(EC_AUTHORITARIAN, 0, 0) },
      "Maintenir la surveillance", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_PROSP, 0, -2), E(EE_CON, S_MOYENNES, 3) },
      "Rendre l'autonomie aux campus", { E(EE_RESEARCH, 0, 8), E(EE_MIL, S_MOYENNES, -5), E(EE_MIL, S_ELITES, 3) },
      NULL, NULL, 0 },
    { "Les dissidents de Genève", "Des opposants exilés à Genève diffusent chaque soir un journal clandestin capté par des millions de Français.",
      6, 2, { C(EC_AUTHORITARIAN, 0, 0) },
      "Brouiller les émissions", { E(EE_CAP, D_INFO, 1), E(EE_REL, N_SUISSE, -8), E(EE_CON, -1, 3) },
      "Amnistie et retour des exilés", { E(EE_MIL, -1, -6), E(EE_MIL, S_ELITES, 5), E(EE_RES, R_INFLUENCE, 3) },
      NULL, NULL, 0 },
    { "Les dons occultes du fonds boréal", "Des documents révèlent qu'un fonds lié à la Fédération Boréale a financé la campagne de trois parlementaires de la majorité.",
      8, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 20) },
      "Exclusion et levée d'immunité", { E(EE_RES, R_COHESION, 2), E(EE_FAC, F_BOREALE, 5), E(EE_MIL, S_ELITES, 3) },
      "Minimiser l'affaire", { E(EE_RES, R_COHESION, -5), E(EE_FACCAP, F_BOREALE, 4), E(EE_CON, -1, 3) },
      NULL, NULL, 0 },
    /* ---------------------------------------------------------------- economie, energie, industrie, finance */
    { "Note souveraine sous surveillance", "Une grande agence de notation place la dette française sous surveillance négative. Les taux d'emprunt grimpent déjà.",
      20, 2, { C(EC_DEBT_ABOVE, 0, 80) },
      "Présenter un plan de redressement", { E(EE_RES, R_BUDGET, 5), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_RES, R_COHESION, -2) },
      "Dénoncer l'arbitraire des agences", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_MIL, S_MOYENNES, -4), E(EE_DEBT, 0, 10) }, "Les marchés saluent le plan", "Dette dégradée en catégorie spéculative", 3 },
    { "Les marchés saluent le plan", "Les taux reculent et l'agence relève la perspective. Les syndicats exigent une contrepartie sociale.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Redistribuer une partie des gains", { E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Accélérer le désendettement", { E(EE_DEBT, 0, -10), E(EE_MIL, S_TRAVAILLEURS, 4) }, NULL, NULL, 0 },
    { "Dette dégradée en catégorie spéculative", "La France perd sa note d'investissement. Les fonds de pension vendent nos obligations en masse.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Grand emprunt auprès des Français", { E(EE_RES, R_BUDGET, 6), E(EE_PROSP, 0, -2), E(EE_CON, S_MOYENNES, 3) },
      "Mécanisme européen de stabilité", { E(EE_DEBT, 0, -15), E(EE_RES, R_SOUVERAINETE, -5), E(EE_RELGROUP, 0, 3) }, NULL, NULL, 0 },
    { "Vallée de l'hydrogène en Lorraine", "Un consortium propose de reconvertir les friches sidérurgiques lorraines en pôle d'électrolyseurs, contre une garantie publique.",
      16, 2, { C(EC_NONE, 0, 0) },
      "Garantie publique totale", { E(EE_RES, R_BUDGET, -12), E(EE_PROSP, 0, 2), E(EE_MIL, S_TRAVAILLEURS, -4) },
      "Partenariat avec l'Allemagne", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_ALLEMAGNE, 8), E(EE_RES, R_SOUVERAINETE, -2) }, "Les électrolyseurs lorrains tournent", "L'hydrogène lorrain part en Allemagne", 5 },
    { "Les électrolyseurs lorrains tournent", "L'hydrogène vert alimente aciéries et poids lourds, mais l'usine absorbe une part notable de l'électricité nationale.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Priorité à l'acier vert", { E(EE_RES, R_ENERGIE, -4), E(EE_PROSP, 0, 3), E(EE_MITIG, 0, 2) },
      "Brider la production", { E(EE_RES, R_ENERGIE, 2), E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, 3) }, NULL, NULL, 0 },
    { "L'hydrogène lorrain part en Allemagne", "Les contrats de long terme réservent l'essentiel de la production aux aciéries de la Sarre et de la Ruhr.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Renégocier les quotas", { E(EE_REL, N_ALLEMAGNE, -8), E(EE_RES, R_ENERGIE, 3) },
      "Honorer les contrats", { E(EE_REL, N_ALLEMAGNE, 6), E(EE_RES, R_BUDGET, 4), E(EE_RES, R_SOUVERAINETE, -2) }, NULL, NULL, 0 },
    { "Alerte de délestage hivernal", "Vague de froid et vent nul : le gestionnaire du réseau prévient que la pointe de consommation dépassera la production dès lundi.",
      8, 3, { C(EC_RES_BELOW, R_ENERGIE, 60) },
      "Coupures tournantes planifiées", { E(EE_RES, R_ENERGIE, 3), E(EE_RES, R_COHESION, -3), E(EE_PROSP, 0, -2) },
      "Importer à tout prix", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_ENERGIE, 2) }, "Hiver tenu sans black-out", "Black-out du Grand Ouest", 2 },
    { "Hiver tenu sans black-out", "Les délestages ont évité l'effondrement. Les industriels réclament des compensations pour leurs arrêts de production.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Indemniser les industriels", { E(EE_RES, R_BUDGET, -6), E(EE_PROSP, 0, 2) },
      "Créer un marché d'effacement", { E(EE_RES, R_ENERGIE, 3), E(EE_MIL, S_ELITES, 4) }, NULL, NULL, 0 },
    { "Black-out du Grand Ouest", "Les interconnexions saturées ont lâché : huit millions de foyers privés de courant pendant deux jours, en plein gel.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Plan Marshall des réseaux", { E(EE_RES, R_BUDGET, -14), E(EE_RES, R_ENERGIE, 5), E(EE_RES, R_COHESION, 2) },
      "Sanctionner le gestionnaire", { E(EE_RES, R_COHESION, -4), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_RES, R_BUDGET, 4) }, NULL, NULL, 0 },
    { "Embargo sur les aimants permanents", "Le premier producteur mondial suspend ses exportations d'aimants aux terres rares. Éoliennes et moteurs électriques sont à l'arrêt.",
      10, 2, { C(EC_STRAT_BELOW, SR_TERRES_RARES, 12) },
      "Usine de recyclage à Lacq", { E(EE_RES, R_BUDGET, -10), E(EE_STRAT, SR_TERRES_RARES, 2), E(EE_PROSP, 0, 1) },
      "Concession minière en Afrique", { E(EE_RES, R_BUDGET, -6), E(EE_STRAT, SR_TERRES_RARES, 5), E(EE_RES, R_INFLUENCE, -3) }, "Premiers aimants recyclés", "Concession minière contestée", 4 },
    { "Premiers aimants recyclés", "Le site de Lacq sort ses premières tonnes d'aimants. Un constructeur automobile veut acheter toute la production.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Vendre au constructeur", { E(EE_RES, R_BUDGET, 6), E(EE_PROSP, 0, 2) },
      "Constituer un stock stratégique", { E(EE_STRAT, SR_TERRES_RARES, 4), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Concession minière contestée", "Des villageois accusent la mine de polluer le fleuve. Les images font le tour des réseaux sociaux africains.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Financer la dépollution", { E(EE_RES, R_BUDGET, -7), E(EE_RES, R_INFLUENCE, 3) },
      "Sécuriser le site avec des privés", { E(EE_STRAT, SR_TERRES_RARES, 3), E(EE_RES, R_INFLUENCE, -5), E(EE_FAC, F_SABLES, 5) }, NULL, NULL, 0 },
    { "Fissures dans les circuits primaires", "Des inspections révèlent des fissures sur les tuyauteries de plusieurs réacteurs. L'autorité de sûreté exige des contrôles.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Arrêter tous les réacteurs suspects", { E(EE_RES, R_ENERGIE, -8), E(EE_RES, R_BUDGET, -6) },
      "Contrôles réacteur par réacteur", { E(EE_RES, R_ENERGIE, -2), E(EE_RES, R_COHESION, -2) }, "Le parc nucléaire redémarre", "Incident sur un réacteur rhodanien", 4 },
    { "Le parc nucléaire redémarre", "Les réparations achevées, les réacteurs reviennent sur le réseau. L'exploitant propose de prolonger leur vie jusqu'à quatre-vingts ans.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Prolonger les réacteurs", { E(EE_RES, R_ENERGIE, 6), E(EE_RES, R_BUDGET, -5), E(EE_CON, -1, 3) },
      "Programmer leur remplacement", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_ENERGIE, 2), E(EE_PROSP, 0, 3), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Incident sur un réacteur rhodanien", "Une fissure non détectée provoque une fuite dans l'enceinte d'un réacteur de la vallée du Rhône. Aucun rejet, mais la peur gagne.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Transparence totale", { E(EE_RES, R_ENERGIE, -4), E(EE_RES, R_COHESION, 2), E(EE_RES, R_INFLUENCE, 2) },
      "Minimiser l'incident", { E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 3), E(EE_RES, R_ENERGIE, 2) }, NULL, NULL, 0 },
    { "Ruée sur une néobanque", "Une néobanque très populaire chez les jeunes gèle les retraits après des pertes immobilières. Trois millions de comptes bloqués.",
      10, 2, { C(EC_NONE, 0, 0) },
      "Garantir tous les dépôts", { E(EE_DEBT, 0, 15), E(EE_RES, R_COHESION, 3) },
      "Imposer le renflouement interne", { E(EE_MIL, S_MOYENNES, 6), E(EE_PROSP, 0, -2) }, "La néobanque rachetée pour un euro", "Panique dans les banques en ligne", 2 },
    { "La néobanque rachetée pour un euro", "Un géant bancaire propose de reprendre la néobanque pour un euro symbolique, si l'État efface ses dettes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Accepter la reprise", { E(EE_DEBT, 0, 8), E(EE_PROSP, 0, 2) },
      "Nationaliser la néobanque", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_SOUVERAINETE, 2), E(EE_MIL, S_ELITES, 4) }, NULL, NULL, 0 },
    { "Panique dans les banques en ligne", "La contagion gagne : files d'attente devant les agences, retraits massifs dans tout le secteur bancaire numérique.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Plafonner les retraits", { E(EE_RES, R_COHESION, -4), E(EE_PROSP, 0, -2), E(EE_RES, R_BUDGET, 3) },
      "Injection massive de liquidités", { E(EE_DEBT, 0, 15), E(EE_MIL, S_MOYENNES, -6) }, NULL, NULL, 0 },
    { "Pays pilote de l'euro numérique", "La BCE propose à la France d'être le pays pilote de l'euro numérique, pour contrer les stablecoins adossés au dollar.",
      12, 2, { C(EC_EU, 0, 0) },
      "Être le pays pilote", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_SOUVERAINETE, 3), E(EE_RELGROUP, 0, 4) },
      "Laisser d'autres essuyer les plâtres", { E(EE_RES, R_SOUVERAINETE, -2), E(EE_RES, R_BUDGET, 2) }, "L'euro numérique attaqué", NULL, 4 },
    { "L'euro numérique attaqué", "Le Réseau Nébula inonde les portefeuilles de faux jetons et paralyse les paiements pendant tout un week-end.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rembourser et suspendre", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 2), E(EE_RES, R_SOUVERAINETE, -2) },
      "Riposte cyber contre Nébula", { E(EE_RES, R_BUDGET, -5), E(EE_FAC, F_NEBULA, 6), E(EE_FACCAP, F_NEBULA, -6) }, NULL, NULL, 0 },
    { "Fonderie de puces près de Grenoble", "Un fondeur asiatique propose une usine de gravure avancée en Isère, contre des subventions record et l'eau du Drac.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Subventionner la fonderie", { E(EE_RES, R_BUDGET, -12), E(EE_STRAT, SR_PUCES, 4), E(EE_RES, R_EAU, -3) },
      "Fonderie européenne commune", { E(EE_RES, R_BUDGET, -6), E(EE_STRAT, SR_PUCES, 2), E(EE_RELGROUP, 0, 4) }, NULL, NULL, 0 },
    { "Agrandir l'usine de La Hague", "Les stocks d'uranium fondent. Les ingénieurs proposent d'agrandir l'usine de retraitement pour recycler le combustible usé.",
      8, 2, { C(EC_STRAT_BELOW, SR_URANIUM, 10) },
      "Lancer l'agrandissement", { E(EE_RES, R_BUDGET, -10), E(EE_STRAT, SR_URANIUM, 4), E(EE_CON, -1, 3) },
      "Achats au comptant en Asie centrale", { E(EE_RES, R_BUDGET, -6), E(EE_STRAT, SR_URANIUM, 2), E(EE_RES, R_SOUVERAINETE, -2) }, NULL, NULL, 0 },
    { "Électricité à prix négatif", "Soleil et vent saturent le réseau : les producteurs paient pour écouler leurs kilowattheures et les petits exploitants font faillite.",
      20, 2, { C(EC_PROJECT_DONE, P_ENR, 0) },
      "Batteries géantes publiques", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_ENERGIE, 5) },
      "Geler les nouvelles installations", { E(EE_MITIG, 0, -2), E(EE_RES, R_BUDGET, 3), E(EE_MIL, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Centres de données en Bourgogne", "Les géants du numérique veulent bâtir des centres de données en Bourgogne. Ils boiraient l'eau et l'électricité d'une ville moyenne.",
      10, 2, { C(EC_NONE, 0, 0) },
      "Dérouler le tapis rouge", { E(EE_RES, R_BUDGET, 6), E(EE_RES, R_ENERGIE, -4), E(EE_RES, R_EAU, -2) },
      "Moratoire sur les centres", { E(EE_PROSP, 0, -2), E(EE_RES, R_ENERGIE, 2), E(EE_RES, R_SOUVERAINETE, 1) }, NULL, NULL, 0 },
    { "Dernier haut fourneau du Nord", "Le sidérurgiste annonce la fermeture du dernier haut fourneau de Dunkerque : quatre mille emplois et l'acier des blindés en jeu.",
      6, 2, { C(EC_UNEMP_ABOVE, 0, 8) },
      "Prise de participation publique", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_SOUVERAINETE, 3), E(EE_MIL, S_TRAVAILLEURS, -6) },
      "Importer l'acier", { E(EE_MIL, S_TRAVAILLEURS, 6), E(EE_PROSP, 0, -2), E(EE_RES, R_BUDGET, 3) }, NULL, NULL, 0 },
    { "Cocaïne dans les conteneurs du Havre", "Le Cartel Hydra a corrompu des dockers : des tonnes de cocaïne transitent par le premier port français.",
      4, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 15) },
      "Scanner chaque conteneur", { E(EE_RES, R_BUDGET, -5), E(EE_PROSP, 0, -3), E(EE_FACCAP, F_HYDRA, -8), E(EE_FAC, F_HYDRA, 6) },
      "Démanteler en douceur", { E(EE_RES, R_BUDGET, -2), E(EE_FACCAP, F_HYDRA, -3), E(EE_RES, R_COHESION, -3) }, NULL, NULL, 0 },
    { "Droits de douane sur le luxe", "Les États-Unis taxent à cent pour cent cognac, champagne et maroquinerie, en représailles à nos normes agricoles.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Riposte commerciale européenne", { E(EE_RELGROUP, 2, -6), E(EE_RELGROUP, 0, 4), E(EE_PROSP, 0, -2) },
      "Assouplir nos normes agricoles", { E(EE_RES, R_SOUVERAINETE, -3), E(EE_PROSP, 0, 2), E(EE_MIL, S_TRAVAILLEURS, 3) }, NULL, NULL, 0 },
    { "Stations de ski sans neige", "Faute de neige, une vingtaine de stations alpines déposent le bilan. Les vallées perdent leur principale source d'emplois.",
      12, 2, { C(EC_TEMP_ABOVE, 0, 18) },
      "Reconversion quatre saisons", { E(EE_RES, R_BUDGET, -8), E(EE_PROSP, 0, 1), E(EE_MITIG, 0, 1) },
      "Canons à neige subventionnés", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_EAU, -4), E(EE_RES, R_ENERGIE, -2), E(EE_MIL, S_TRAVAILLEURS, -4) }, NULL, NULL, 0 },
    { "La Riviera désertée en été", "Cinquante degrés à Nice : les touristes fuient la Méditerranée pour la Baltique. Hôtels et restaurants licencient en masse.",
      20, 2, { C(EC_TEMP_ABOVE, 0, 20) },
      "Miser sur le tourisme d'hiver", { E(EE_RES, R_BUDGET, -6), E(EE_PROSP, 0, 2) },
      "Indemniser les saisonniers", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Élevages géants en Bretagne", "Un groupe agro-industriel veut bâtir des élevages de cent mille porcs en Bretagne pour nourrir le pays à bas prix.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Autoriser les élevages", { E(EE_RES, R_ALIM, 5), E(EE_RES, R_EAU, -3), E(EE_CON, S_MOYENNES, 3) },
      "Soutenir l'élevage paysan", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ALIM, 1), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },
    { "Fin du fret ferroviaire", "L'opérateur ferroviaire veut fermer ses dernières lignes de fret : les camions récupéreraient deux millions de tonnes par an.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Subventionner le fret ferroviaire", { E(EE_RES, R_BUDGET, -8), E(EE_MITIG, 0, 2), E(EE_RES, R_ENERGIE, 1) },
      "Autoroutes à camions électriques", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ENERGIE, -3), E(EE_PROSP, 0, 2) }, NULL, NULL, 0 },
    { "Génération sans emploi", "Un jeune sur trois est sans emploi. Les diplômés partent par dizaines de milliers vers le Canada et l'Australie.",
      10, 2, { C(EC_UNEMP_ABOVE, 0, 11) },
      "Service civique climatique payé", { E(EE_RES, R_BUDGET, -8), E(EE_MIL, S_TRAVAILLEURS, -5), E(EE_MITIG, 0, 1) },
      "Exonérer l'embauche des jeunes", { E(EE_RES, R_BUDGET, -5), E(EE_PROSP, 0, 2), E(EE_MIL, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Robots dans les entrepôts", "La logistique s'automatise : cent mille préparateurs de commandes menacés d'ici deux ans par des robots autonomes.",
      16, 2, { C(EC_NONE, 0, 0) },
      "Taxer les robots", { E(EE_RES, R_BUDGET, 5), E(EE_PROSP, 0, -2), E(EE_MIL, S_ELITES, 4) },
      "Laisser l'automatisation", { E(EE_PROSP, 0, 3), E(EE_MIL, S_TRAVAILLEURS, 6) }, NULL, NULL, 0 },
    { "Grand emprunt pour l'adaptation", "Les caisses sont vides et les digues cèdent. Le Trésor propose un emprunt sur cinquante ans fléché vers l'adaptation climatique.",
      16, 2, { C(EC_DEBT_ABOVE, 0, 100) },
      "Lancer l'emprunt", { E(EE_DEBT, 0, 20), E(EE_PROSP, 0, 2), E(EE_RES, R_COHESION, 2) },
      "Couper dans les dépenses", { E(EE_RES, R_BUDGET, 5), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_PROSP, 0, -1) }, NULL, NULL, 0 },
    { "Profits envolés vers les paradis fiscaux", "Les multinationales du numérique contournent l'impôt minimal mondial. Le manque à gagner atteint dix milliards d'euros.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Taxe unilatérale sur le chiffre", { E(EE_RES, R_BUDGET, 6), E(EE_RELGROUP, 2, -4), E(EE_PROSP, 0, -1) },
      "Attendre un accord européen", { E(EE_RES, R_BUDGET, -3), E(EE_RELGROUP, 0, 3) }, NULL, NULL, 0 },
    { "Minage de cryptomonnaies aux barrages", "Des mineurs de cryptomonnaies rachètent la production de petits barrages du Massif central. Hydra y blanchirait des fonds.",
      10, 2, { C(EC_NONE, 0, 0) },
      "Interdire le minage", { E(EE_RES, R_ENERGIE, 3), E(EE_FACCAP, F_HYDRA, -4), E(EE_RES, R_BUDGET, -2) },
      "Taxer et réglementer", { E(EE_RES, R_BUDGET, 5), E(EE_RES, R_ENERGIE, -3), E(EE_FACCAP, F_HYDRA, 3) }, NULL, NULL, 0 },
    { "Dérive du chantier de fusion", "Le réacteur expérimental de fusion de Cadarache accuse dix ans de retard et des coûts triplés. Les partenaires hésitent.",
      24, 1, { C(EC_NONE, 0, 0) },
      "Remettre au pot", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_INFLUENCE, 4), E(EE_PROSP, 0, 1) },
      "Geler notre contribution", { E(EE_RES, R_BUDGET, 4), E(EE_RES, R_INFLUENCE, -4), E(EE_RELGROUP, 0, -3) }, NULL, NULL, 0 },
    { "Câble asiatique à Marseille", "Un consortium étranger veut faire atterrir à Marseille un câble de données Asie-Europe, en gérant lui-même la station.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Accepter le câble", { E(EE_RES, R_BUDGET, 5), E(EE_PROSP, 0, 2), E(EE_RES, R_SOUVERAINETE, -4) },
      "Imposer un opérateur français", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_SOUVERAINETE, 3), E(EE_CAP, D_CYBER, 1) }, NULL, NULL, 0 },
    { "Usines d'engrais à l'arrêt", "Les usines d'engrais européennes ferment faute de gaz bon marché. Les céréaliers craignent une récolte divisée par deux.",
      6, 2, { C(EC_RES_BELOW, R_ENERGIE, 65) },
      "Importer des engrais du Maroc", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ALIM, 3), E(EE_REL, N_MAROC, 6) },
      "Engrais verts à l'hydrogène", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_ALIM, 2), E(EE_RES, R_ENERGIE, -2), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "L'or de la Guyane", "Le Cartel Hydra contrôle des centaines de sites d'orpaillage. Le mercure empoisonne les fleuves et les populations.",
      8, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 20) },
      "Opération militaire en forêt", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -6), E(EE_FAC, F_HYDRA, 6) },
      "Créer une filière d'or légal", { E(EE_RES, R_BUDGET, 3), E(EE_FACCAP, F_HYDRA, 2), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    /* ---------------------------------------------------------------- defense, guerre hybride et international (lot defense) */
    { "Collision en orbite basse", "Un satellite d'observation français est pulvérisé par des débris. Le nuage menace toute notre constellation militaire.",
      16, 2, { C(EC_NONE, 0, 0) },
      "Financer un remorqueur orbital", { E(EE_RES, R_BUDGET, -10), E(EE_RESEARCH, 0, 6) },
      "Manoeuvrer et attendre", { E(EE_CAP, D_ESPACE, -1), E(EE_RES, R_BUDGET, -2) }, "Mission de nettoyage orbital", "Syndrome de Kessler redouté", 4 },
    { "Mission de nettoyage orbital", "Le remorqueur français capture ses premiers débris. Plusieurs agences étrangères proposent de louer ses services.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Vendre le service aux alliés", { E(EE_RES, R_BUDGET, 5), E(EE_RES, R_INFLUENCE, 3), E(EE_CAP, D_ESPACE, -1) },
      "Le réserver à nos satellites", { E(EE_CAP, D_ESPACE, 2), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Syndrome de Kessler redouté", "Les collisions s'enchaînent en orbite basse. Nos communications militaires et la météo agricole se dégradent.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Relais par ballons stratosphériques", { E(EE_RES, R_BUDGET, -7), E(EE_CAP, D_EM, 1) },
      "Dépendre des satellites alliés", { E(EE_RES, R_SOUVERAINETE, -4), E(EE_NATO, 0, 3), E(EE_RES, R_ALIM, -2) }, NULL, NULL, 0 },
    { "Périscope dans le golfe de Gascogne", "Un sous-marin boréal est repéré près de la base de l'île Longue, où mouillent nos sous-marins nucléaires.",
      8, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 20) },
      "Traque avec frégates et avions", { E(EE_RES, R_BUDGET, -6), E(EE_FAC, F_BOREALE, 6), E(EE_CRED, 0, 3) },
      "Le laisser repartir discrètement", { E(EE_CRED, 0, -4), E(EE_FACCAP, F_BOREALE, 4) }, "Le sous-marin pris au piège", NULL, 2 },
    { "Le sous-marin pris au piège", "Le sous-marin boréal, avarié, fait surface dans nos eaux territoriales. Son équipage demande assistance.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Saisir le bâtiment et ses secrets", { E(EE_RESEARCH, 0, 10), E(EE_REL, N_BOREALE, -15), E(EE_FAC, F_BOREALE, 8) },
      "Le remorquer et le rendre", { E(EE_REL, N_BOREALE, 8), E(EE_RES, R_INFLUENCE, 3), E(EE_MIL, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "L'offre de la compagnie Égide", "Une société militaire privée propose de sécuriser nos convois au Sahel pour moitié moins cher que l'armée.",
      6, 2, { C(EC_FAC_HOSTILE, F_SABLES, 20) },
      "Signer le contrat", { E(EE_RES, R_BUDGET, 4), E(EE_FACCAP, F_SABLES, -4), E(EE_RES, R_INFLUENCE, -2) },
      "Garder des soldats réguliers", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_TERRE, -1), E(EE_RES, R_INFLUENCE, 2) }, "Bavure des mercenaires d'Égide", NULL, 4 },
    { "Bavure des mercenaires d'Égide", "Des contractuels d'Égide ouvrent le feu sur un marché nigérien. Les images font le tour de l'Afrique.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rompre le contrat et indemniser", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, 2), E(EE_FACCAP, F_SABLES, 3) },
      "Couvrir la société", { E(EE_RES, R_INFLUENCE, -6), E(EE_FAC, F_SABLES, 8), E(EE_RELGROUP, 1, -5) }, NULL, NULL, 0 },
    { "Faux discours du chef d'état-major", "Une vidéo truquée montre notre chef d'état-major annoncer un coup de force. Elle est vue vingt millions de fois en une nuit.",
      6, 3, { C(EC_FAC_HOSTILE, F_NEBULA, 20) },
      "Traçage technique de la vidéo", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_INFO, 1) },
      "Démenti solennel à la télévision", { E(EE_RES, R_COHESION, -2), E(EE_CON, -1, 3) }, "Les auteurs du trucage identifiés", "Défiance envers l'armée", 3 },
    { "Les auteurs du trucage identifiés", "Les signatures numériques mènent à un studio de Nébula payé par des intermédiaires boréaux.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Expulser des diplomates boréaux", { E(EE_REL, N_BOREALE, -12), E(EE_FAC, F_BOREALE, 6), E(EE_RES, R_COHESION, 3) },
      "Pirater le studio en retour", { E(EE_FACCAP, F_NEBULA, -8), E(EE_FAC, F_NEBULA, 6), E(EE_AGGRO, 0, 3) }, NULL, NULL, 0 },
    { "Défiance envers l'armée", "Malgré le démenti, un sondage révèle qu'un Français sur trois croit à un projet de putsch. Les recrutements chutent.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Journées portes ouvertes des armées", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 3) },
      "Commission d'enquête parlementaire", { E(EE_CON, -1, 3), E(EE_CAP, D_TERRE, -1), E(EE_FACCAP, F_NEBULA, -3) }, NULL, NULL, 0 },
    { "Centrifugeuses dans le désert", "Nos satellites repèrent un site d'enrichissement d'uranium caché sous une ferme solaire en Afrique du Nord.",
      20, 2, { C(EC_PROLIFERATION, 0, 0) },
      "Préparer une frappe préventive", { E(EE_RES, R_BUDGET, -6), E(EE_CRED, 0, 4), E(EE_AGGRO, 0, 5) },
      "Saisir l'AIEA et l'ONU", { E(EE_RES, R_INFLUENCE, 4), E(EE_CRED, 0, -3) }, "Raid sur le site d'enrichissement", "Inspecteurs de l'ONU sur place", 3 },
    { "Raid sur le site d'enrichissement", "Nos Rafale sont prêts à décoller. Les centrifugeuses peuvent être détruites cette nuit, au prix d'une crise régionale.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Frapper", { E(EE_CRED, 0, 8), E(EE_RELGROUP, 1, -8), E(EE_AGGRO, 0, 8), E(EE_FAC, F_SABLES, 6) },
      "Annuler au dernier moment", { E(EE_CRED, 0, -5), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Inspecteurs de l'ONU sur place", "Les inspecteurs onusiens obtiennent l'accès au site, mais demandent à la France de financer la mission et des compensations.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Payer la mission et l'aide civile", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_INFLUENCE, 5), E(EE_RELGROUP, 1, 5) },
      "Laisser l'ONU se débrouiller", { E(EE_RES, R_INFLUENCE, -3), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
    { "Ingénieure approchée à Toulouse", "Une ingénieure de l'aéronautique signale qu'un « chasseur de têtes » lui offre une fortune pour des plans de missile.",
      4, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 15) },
      "Monter une opération de contre-espionnage", { E(EE_RES, R_BUDGET, -3) },
      "Arrêter le recruteur sur-le-champ", { E(EE_FAC, F_BOREALE, 4), E(EE_RES, R_COHESION, 2) }, "Le réseau d'espions remonté", NULL, 3 },
    { "Le réseau d'espions remonté", "L'agent recruteur mène nos services à onze sources implantées dans l'industrie de défense.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Tout démanteler", { E(EE_FACCAP, F_BOREALE, -8), E(EE_FAC, F_BOREALE, 6), E(EE_RES, R_SOUVERAINETE, 2) },
      "Leur livrer de faux plans", { E(EE_FACCAP, F_BOREALE, -4), E(EE_RESEARCH, 0, 5), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Drones artisanaux du Califat", "Au Sahel, le Califat des Sables arme des drones agricoles avec des grenades. Nos bases avancées sont visées.",
      6, 2, { C(EC_FAC_HOSTILE, F_SABLES, 25) },
      "Brouilleurs et canons antidrones", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_EM, 1) },
      "Évacuer les bases avancées", { E(EE_FACCAP, F_SABLES, 5), E(EE_RES, R_INFLUENCE, -3) }, NULL, "Drones sur un port français", 4 },
    { "Drones sur un port français", "Fort de son succès au Sahel, le Califat lance trois drones piégés contre un méthanier dans le port de Fos.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Bulle antidrones sur les ports", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_AIR, 1), E(EE_RES, R_COHESION, 2) },
      "Fermer le terminal méthanier", { E(EE_RES, R_ENERGIE, -5), E(EE_THREAT, F_SABLES, 1) }, NULL, NULL, 0 },
    { "Black-out provoqué à Lyon", "Un logiciel malveillant ouvre les disjoncteurs de trois postes électriques : un million de Lyonnais privés de courant en plein hiver.",
      6, 3, { C(EC_FAC_HOSTILE, F_NEBULA, 25) },
      "Isoler le réseau d'Internet", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_CYBER, 1), E(EE_PROSP, 0, -2) },
      "Riposte cyber contre les serveurs", { E(EE_FACCAP, F_NEBULA, -6), E(EE_FAC, F_NEBULA, 6), E(EE_RES, R_ENERGIE, -3) }, NULL, NULL, 0 },
    { "Tir antisatellite boréal", "La Fédération Boréale détruit l'un de ses vieux satellites avec un missile. Le message adressé à l'Europe est limpide.",
      12, 2, { C(EC_ALIVE, N_BOREALE, 0), C(EC_FAC_HOSTILE, F_BOREALE, 25) },
      "Lancer notre programme antisatellite", { E(EE_RES, R_BUDGET, -10), E(EE_CAP, D_ESPACE, 1), E(EE_AGGRO, 0, 3) },
      "Traité d'interdiction à l'ONU", { E(EE_RES, R_INFLUENCE, 5), E(EE_FACCAP, F_BOREALE, 3) }, NULL, NULL, 0 },
    { "Mines dérivantes dans la Manche", "Des mines marines larguées par des chalutiers boréaux coulent un caboteur. Le trafic transmanche est suspendu.",
      0, 3, { C(EC_WAR_WITH, N_BOREALE, 0) },
      "Mobiliser tous nos chasseurs de mines", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_MER, -1), E(EE_RES, R_COHESION, 2) },
      "Confier le déminage à la Royal Navy", { E(EE_REL, N_UK, 6), E(EE_RES, R_SOUVERAINETE, -3), E(EE_PROSP, 0, -2) }, NULL, NULL, 0 },
    { "Cheminots piratés en pleine guerre", "Des hackers boréaux prennent la main sur la signalisation ferroviaire : nos trains de munitions sont bloqués en Lorraine.",
      0, 2, { C(EC_WAR_WITH, N_BOREALE, 0) },
      "Retour à la signalisation manuelle", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_TERRE, -1) },
      "Mobiliser les réservistes cyber", { E(EE_CAP, D_CYBER, 1), E(EE_MIL, S_MOYENNES, 3), E(EE_PROSP, 0, -2) }, NULL, NULL, 0 },
    { "Otages français à Gao", "Cinq humanitaires français sont enlevés par le Califat des Sables, qui réclame la libération de ses chefs.",
      4, 2, { C(EC_FAC_HOSTILE, F_SABLES, 20) },
      "Raid des forces spéciales", { E(EE_RES, R_BUDGET, -5), E(EE_FAC, F_SABLES, 8), E(EE_FACCAP, F_SABLES, -4) },
      "Négocier dans l'ombre", { E(EE_RES, R_COHESION, 3), E(EE_FACCAP, F_SABLES, 6), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
    { "Fusils d'assaut dans des conteneurs", "Les douaniers du Havre découvrent quatre cents fusils d'assaut cachés dans des machines à laver. Hydra fournit les cités et le Califat.",
      4, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 20) },
      "Scanner tous les conteneurs", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -6), E(EE_PROSP, 0, -1) },
      "Infiltrer la filière", { E(EE_RES, R_BUDGET, -2), E(EE_FACCAP, F_SABLES, -3), E(EE_THREAT, F_HYDRA, 1) }, NULL, NULL, 0 },
    { "Missiles hypersoniques à la frontière", "La Fédération Boréale déploie des missiles hypersoniques capables d'atteindre Paris en sept minutes.",
      16, 2, { C(EC_ALIVE, N_BOREALE, 0), C(EC_REL_BELOW, N_BOREALE, 0) },
      "Moderniser la composante aérienne", { E(EE_RES, R_BUDGET, -12), E(EE_CRED, 0, 8), E(EE_RESEARCH, 0, 5) },
      "Proposer des pourparlers", { E(EE_REL, N_BOREALE, 8), E(EE_CRED, 0, -4), E(EE_NATO, 0, -3) }, NULL, NULL, 0 },
    { "Débat sur le parapluie européen", "Plusieurs capitales demandent que la dissuasion française couvre explicitement toute l'Union.",
      14, 2, { C(EC_EU, 0, 0), C(EC_FAC_HOSTILE, F_BOREALE, 20) },
      "Étendre la dissuasion à l'Europe", { E(EE_RELGROUP, 0, 8), E(EE_CRED, 0, -3), E(EE_RES, R_BUDGET, -5) },
      "Dissuasion strictement nationale", { E(EE_CRED, 0, 4), E(EE_RELGROUP, 0, -5), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Manoeuvres Rempart du Nord", "L'OTAN organise son plus grand exercice depuis la guerre froide et demande deux brigades françaises.",
      6, 2, { C(EC_NATO, 0, 0) },
      "Envoyer les deux brigades", { E(EE_RES, R_BUDGET, -6), E(EE_NATO, 0, 6), E(EE_CAP, D_TERRE, 1) },
      "Un simple détachement symbolique", { E(EE_NATO, 0, -4), E(EE_RES, R_BUDGET, -1), E(EE_REL, N_BOREALE, 4) }, NULL, NULL, 0 },
    { "Article 5 invoqué par un allié", "Un allié de l'Est, frappé par des missiles boréaux, invoque l'article 5. Washington attend de voir ce que fera Paris.",
      10, 2, { C(EC_NATO, 0, 0), C(EC_BOREALE_AT_WAR, 0, 0) },
      "Escadrilles de Rafale sur place", { E(EE_RES, R_BUDGET, -8), E(EE_NATO, 0, 10), E(EE_CAP, D_AIR, -1), E(EE_FAC, F_BOREALE, 8) },
      "Aide humanitaire et sanctions", { E(EE_NATO, 0, -8), E(EE_CRED, 0, -4), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Emprunt européen pour les obus", "Bruxelles propose un grand emprunt commun pour doubler la production européenne de munitions.",
      8, 2, { C(EC_EU, 0, 0) },
      "Soutenir l'emprunt commun", { E(EE_DEBT, 0, 10), E(EE_RESEARCH, 0, 8), E(EE_RELGROUP, 0, 5) },
      "Exiger la préférence française", { E(EE_RES, R_SOUVERAINETE, 3), E(EE_RELGROUP, 0, -4), E(EE_PROSP, 0, 2) }, NULL, NULL, 0 },
    { "Casques bleus pour le Sahel", "Le Conseil de sécurité vote une mission de paix au Sahel et demande à la France d'en prendre le commandement.",
      10, 2, { C(EC_FAC_HOSTILE, F_SABLES, 15) },
      "Prendre le commandement", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_INFLUENCE, 6), E(EE_FACCAP, F_SABLES, -5) },
      "Fournir seulement la logistique", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, 1), E(EE_FACCAP, F_SABLES, 2) }, NULL, NULL, 0 },
    { "Chantage migratoire d'Ankara", "Fâchée avec Paris, la Turquie laisse partir des centaines de milliers de migrants vers les îles grecques.",
      10, 2, { C(EC_ALIVE, N_TURQUIE, 0), C(EC_REL_BELOW, N_TURQUIE, 0) },
      "Payer Ankara pour fermer la route", { E(EE_RES, R_BUDGET, -8), E(EE_REL, N_TURQUIE, 10), E(EE_RES, R_SOUVERAINETE, -2) },
      "Envoyer la marine aider Athènes", { E(EE_REL, N_GRECE, 12), E(EE_REL, N_TURQUIE, -10), E(EE_IMMIG, 0, 2) }, NULL, NULL, 0 },
    { "Frégates pour la mer Égée", "Athènes, inquiète des incursions turques, veut acheter quatre frégates françaises à crédit.",
      8, 2, { C(EC_ALIVE, N_GRECE, 0), C(EC_ALIVE, N_TURQUIE, 0) },
      "Vendre à crédit", { E(EE_REL, N_GRECE, 15), E(EE_DEBT, 0, 5), E(EE_REL, N_TURQUIE, -8) },
      "Exiger un paiement comptant", { E(EE_RES, R_BUDGET, 3), E(EE_REL, N_GRECE, -6) }, NULL, NULL, 0 },
    { "Émeutes à Mitrovica", "Les violences reprennent dans les Balkans ; la force de paix européenne manque de soldats et appelle Paris.",
      6, 2, { C(EC_ALIVE, N_BALKANS, 0) },
      "Envoyer un bataillon", { E(EE_RES, R_BUDGET, -4), E(EE_REL, N_BALKANS, 10), E(EE_RELGROUP, 0, 3), E(EE_CAP, D_TERRE, -1) },
      "Laisser les voisins gérer", { E(EE_REL, N_BALKANS, -5), E(EE_FACCAP, F_HYDRA, 4) }, NULL, NULL, 0 },
    { "Simulations nucléaires partagées", "Londres propose de fusionner nos programmes de simulation des têtes nucléaires pour diviser les coûts.",
      12, 1, { C(EC_ALIVE, N_UK, 0), C(EC_REL_ABOVE, N_UK, 20) },
      "Accepter la mutualisation", { E(EE_RES, R_BUDGET, 5), E(EE_REL, N_UK, 12), E(EE_CRED, 0, 3), E(EE_RES, R_SOUVERAINETE, -4) },
      "Garder nos secrets", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_RES, R_BUDGET, -3), E(EE_REL, N_UK, -4) }, NULL, NULL, 0 },
    { "Patrouilles communes avec Rome", "L'Italie propose des patrouilles navales communes contre les passeurs entre la Libye et Lampedusa.",
      6, 2, { C(EC_ALIVE, N_ITALIE, 0), C(EC_ALIVE, N_LIBYE, 0) },
      "Engager nos navires", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_ITALIE, 10), E(EE_FACCAP, F_HYDRA, -4) },
      "Décliner poliment", { E(EE_REL, N_ITALIE, -6), E(EE_FACCAP, F_HYDRA, 3) }, NULL, NULL, 0 },
    { "Ceuta sous pression", "Des milliers de migrants forcent la frontière de Ceuta ; Madrid accuse Rabat et demande le soutien de Paris.",
      8, 2, { C(EC_ALIVE, N_ESPAGNE, 0), C(EC_ALIVE, N_MAROC, 0) },
      "Soutenir Madrid", { E(EE_REL, N_ESPAGNE, 12), E(EE_REL, N_MAROC, -10), E(EE_RELGROUP, 0, 2) },
      "Proposer une médiation", { E(EE_REL, N_MAROC, 6), E(EE_REL, N_ESPAGNE, -6), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Varsovie veut sa propre bombe", "Face à la menace boréale, la Pologne annonce étudier l'arme nucléaire, sauf si Paris étend sa garantie.",
      16, 1, { C(EC_ALIVE, N_POLOGNE, 0), C(EC_FAC_HOSTILE, F_BOREALE, 30) },
      "Garantie nucléaire à la Pologne", { E(EE_REL, N_POLOGNE, 15), E(EE_CRED, 0, -3), E(EE_FAC, F_BOREALE, 6) },
      "Laisser Varsovie choisir", { E(EE_REL, N_POLOGNE, -6), E(EE_NATO, 0, -4), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Drones ukrainiens à vendre", "Kiev, rodé par des années de guerre, offre de coproduire en France ses drones d'attaque à longue portée.",
      6, 2, { C(EC_ALIVE, N_UKRAINE, 0) },
      "Usine franco-ukrainienne", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_AIR, 1), E(EE_REL, N_UKRAINE, 10), E(EE_REL, N_BOREALE, -8) },
      "Privilégier nos industriels", { E(EE_RES, R_SOUVERAINETE, 2), E(EE_RESEARCH, 0, 4), E(EE_REL, N_UKRAINE, -5) }, NULL, NULL, 0 },
    { "Contrat d'armement avec Le Caire", "L'Égypte veut trente avions de combat français. Des ONG dénoncent leur usage contre des civils.",
      6, 2, { C(EC_ALIVE, N_EGYPTE, 0), C(EC_REL_ABOVE, N_EGYPTE, 0) },
      "Signer le contrat", { E(EE_RES, R_BUDGET, 8), E(EE_REL, N_EGYPTE, 12), E(EE_RES, R_INFLUENCE, -4) },
      "Conditionner aux droits humains", { E(EE_REL, N_EGYPTE, -10), E(EE_RES, R_INFLUENCE, 3), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Escarmouches au Sahara", "Des tirs d'artillerie éclatent entre forces marocaines et algériennes. Les deux capitales exigent le soutien de Paris.",
      10, 2, { C(EC_ALIVE, N_MAROC, 0), C(EC_ALIVE, N_ALGERIE, 0) },
      "Pencher vers Rabat", { E(EE_REL, N_MAROC, 12), E(EE_REL, N_ALGERIE, -15), E(EE_RES, R_ENERGIE, -3) },
      "Neutralité et médiation", { E(EE_RES, R_INFLUENCE, 3), E(EE_REL, N_MAROC, -4), E(EE_REL, N_ALGERIE, -4) }, NULL, NULL, 0 },
    { "Le char franco-allemand en panne", "Berlin menace de quitter le programme de char commun et d'acheter un modèle étranger moins cher.",
      10, 2, { C(EC_ALIVE, N_ALLEMAGNE, 0) },
      "Céder sur le partage industriel", { E(EE_REL, N_ALLEMAGNE, 10), E(EE_PROSP, 0, -2), E(EE_CAP, D_TERRE, 1) },
      "Poursuivre seuls", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_SOUVERAINETE, 3), E(EE_REL, N_ALLEMAGNE, -8) }, NULL, NULL, 0 },
    /* ---------------------------------------------------------------- futur 2045-2060 : ruptures technologiques, espace, effondrements */
    { "Première intelligence artificielle générale", "Un laboratoire franco-allemand annonce un système capable de raisonner dans tous les domaines. États-majors et marchés s'affolent.",
      70, 2, { C(EC_TECH_ABOVE, T_GE, 2) },
      "Nationaliser le laboratoire", { E(EE_RES, R_BUDGET, -12), E(EE_RESEARCH, 0, 15), E(EE_RELGROUP, 0, -5) },
      "Moratoire international", { E(EE_RES, R_INFLUENCE, 5), E(EE_RESEARCH, 0, -5), E(EE_PROSP, 0, -2) }, "L'IA générale au service de l'État", "Fuite des poids de l'IA", 4 },
    { "L'IA générale au service de l'État", "Deux ans plus tard, l'IA planifie réseaux électriques et logistique militaire. Les syndicats dénoncent une technocratie sans visage.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Étendre à l'administration", { E(EE_PROSP, 0, 3), E(EE_MIL, S_TRAVAILLEURS, 6), E(EE_RES, R_BUDGET, 5) },
      "La cantonner à la défense", { E(EE_RESEARCH, 0, 10), E(EE_CAP, D_CYBER, 1), E(EE_RES, R_BUDGET, -4) }, NULL, NULL, 0 },
    { "Fuite des poids de l'IA", "Pendant le moratoire, une copie du modèle circule sur le darknet. Le Réseau Nébula s'en sert déjà pour ses campagnes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Traque mondiale des copies", { E(EE_RES, R_BUDGET, -8), E(EE_FACCAP, F_NEBULA, -8), E(EE_FAC, F_NEBULA, 6) },
      "Lever le moratoire en urgence", { E(EE_RESEARCH, 0, 10), E(EE_RES, R_INFLUENCE, -6), E(EE_FACCAP, F_NEBULA, 4) }, NULL, NULL, 0 },
    { "Base lunaire européenne", "L'Europe cherche un pays pilote pour sa première base habitée au pôle sud lunaire. Facture annoncée : 40 milliards d'euros.",
      65, 2, { C(EC_PROJECT_DONE, P_CONSTELL, 0) },
      "Prendre la tête du projet", { E(EE_RES, R_BUDGET, -15), E(EE_RES, R_INFLUENCE, 6), E(EE_CAP, D_ESPACE, 1) },
      "Participation minoritaire", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_INFLUENCE, 2), E(EE_RELGROUP, 0, 4) }, "Accident sur la base Clavius", "Les autres plantent leur drapeau", 5 },
    { "Accident sur la base Clavius", "Une dépressurisation tue deux astronautes français. La base doit être évacuée ou réparée sous trois mois.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Mission de sauvetage et réparation", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_INFLUENCE, 4), E(EE_RES, R_COHESION, 2) },
      "Évacuer et suspendre", { E(EE_RES, R_INFLUENCE, -5), E(EE_CAP, D_ESPACE, -1), E(EE_RES, R_BUDGET, 4) }, NULL, NULL, 0 },
    { "Les autres plantent leur drapeau", "Un consortium asiatique revendique les cratères riches en glace du pôle sud lunaire. L'Europe arrive trop tard.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Contester devant l'ONU", { E(EE_RES, R_INFLUENCE, 3), E(EE_RES, R_BUDGET, -3), E(EE_RELGROUP, 0, -3) },
      "Acheter un accès à leur base", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_SOUVERAINETE, -3), E(EE_CAP, D_ESPACE, 1) }, NULL, NULL, 0 },
    { "Ville flottante au large de Marseille", "Un promoteur propose une cité flottante de 50 000 habitants pour reloger les sinistrés du littoral. Elle échapperait en partie au droit commun.",
      75, 2, { C(EC_SEA_ABOVE, 0, 30) },
      "Autoriser la zone franche", { E(EE_RES, R_BUDGET, 4), E(EE_PROSP, 0, 2), E(EE_RES, R_SOUVERAINETE, -4) },
      "Cité publique sous droit français", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_COHESION, 3), E(EE_MIL, S_MOYENNES, -5) }, "Sécession de la cité flottante", NULL, 5 },
    { "Sécession de la cité flottante", "La direction de la cité flottante la déclare « territoire neutre » et refuse l'impôt. Le Cartel Hydra y blanchit déjà des fonds.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Envoyer la Marine", { E(EE_CAP, D_MER, -1), E(EE_RES, R_SOUVERAINETE, 5), E(EE_FACCAP, F_HYDRA, -5), E(EE_AGGRO, 0, 3) },
      "Négocier un statut spécial", { E(EE_RES, R_SOUVERAINETE, -4), E(EE_RES, R_BUDGET, 3), E(EE_FACCAP, F_HYDRA, 5) }, NULL, NULL, 0 },
    { "Le culte de l'Effondrement", "La secte des Derniers Jours recrute des milliers de jeunes qui brûlent leurs papiers et attendent la fin dans des communes autarciques.",
      70, 2, { C(EC_TEMP_ABOVE, 0, 23) },
      "Dissoudre le mouvement", { E(EE_RES, R_COHESION, -2), E(EE_CON, S_MOYENNES, 3), E(EE_RES, R_SOUVERAINETE, 2) },
      "Tolérer et surveiller", { E(EE_RES, R_BUDGET, -3), E(EE_MIL, S_MOYENNES, 3) }, "Le culte entre en clandestinité", "Les communes de la fin du monde", 4 },
    { "Le culte entre en clandestinité", "Dissous, le culte se réfugie dans les Cévennes et sabote des pylônes pour « hâter la fin ». Trois départements privés de courant.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Opération de gendarmerie", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_ENERGIE, -2), E(EE_CON, -1, 3) },
      "Programme de sortie de secte", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_ENERGIE, -4), E(EE_RES, R_COHESION, 3) }, NULL, NULL, 0 },
    { "Les communes de la fin du monde", "Les communes du culte nourrissent désormais des villages entiers grâce à une agriculture de survie. Leur modèle séduit.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Les intégrer au plan alimentaire", { E(EE_RES, R_ALIM, 4), E(EE_MITIG, 0, 1), E(EE_RES, R_COHESION, -3) },
      "Leur imposer la loi commune", { E(EE_RES, R_COHESION, 2), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_ALIM, -2) }, NULL, NULL, 0 },
    { "Embryons génétiquement édités", "Des cliniques proposent d'éditer les embryons contre le diabète et la chaleur extrême. Le Comité d'éthique est divisé.",
      65, 2, { C(EC_NONE, 0, 0) },
      "Autoriser sous contrôle public", { E(EE_RES, R_BUDGET, -6), E(EE_PROSP, 0, 2), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Interdiction totale", { E(EE_RES, R_INFLUENCE, 2), E(EE_PROSP, 0, -1) }, "La génération éditée", "Tourisme génétique", 6 },
    { "La génération éditée", "Les premiers enfants édités entrent à l'école. Seules les familles aisées y ont eu accès : on parle d'une caste biologique.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Gratuité pour tous", { E(EE_RES, R_BUDGET, -10), E(EE_MIL, S_TRAVAILLEURS, -8), E(EE_PROSP, 0, 2) },
      "Plafonner les modifications", { E(EE_RES, R_COHESION, -2), E(EE_MIL, S_ELITES, 4), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Tourisme génétique", "Des milliers de couples font éditer leurs embryons dans des cliniques offshore tenues par le Cartel Hydra.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Poursuivre les parents", { E(EE_RES, R_COHESION, -4), E(EE_FACCAP, F_HYDRA, -3), E(EE_CON, S_MOYENNES, 3) },
      "Légaliser a minima", { E(EE_FACCAP, F_HYDRA, -5), E(EE_RES, R_INFLUENCE, -3), E(EE_MIL, S_ELITES, -4) }, NULL, NULL, 0 },
    { "Capture d'un astéroïde métallique", "Une start-up toulousaine veut détourner un astéroïde riche en nickel et en platine vers l'orbite lunaire. Il lui faut un garant public.",
      80, 1, { C(EC_TECH_ABOVE, T_AIR, 3) },
      "Garantie de l'État", { E(EE_DEBT, 0, 15), E(EE_CAP, D_ESPACE, 1), E(EE_RES, R_INFLUENCE, 3) },
      "Laisser les investisseurs privés", { E(EE_RES, R_SOUVERAINETE, -3), E(EE_PROSP, 0, 1) }, "L'astéroïde en orbite lunaire", NULL, 5 },
    { "L'astéroïde en orbite lunaire", "L'astéroïde est stabilisé autour de la Lune. Les premières tonnes de métaux rares peuvent redescendre, au prix de risques d'impact.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rapatrier en masse", { E(EE_STRAT, SR_TERRES_RARES, 5), E(EE_RES, R_BUDGET, 5), E(EE_RES, R_INFLUENCE, -4) },
      "Extraction prudente et lente", { E(EE_STRAT, SR_TERRES_RARES, 2), E(EE_RES, R_BUDGET, -4), E(EE_RES, R_INFLUENCE, 2) }, "Fragment tombé dans l'Atlantique", NULL, 3 },
    { "Fragment tombé dans l'Atlantique", "Une capsule de minerai rate sa rentrée et s'abîme près des Açores, soulevant une petite vague. Lisbonne exige des comptes.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Indemniser le Portugal", { E(EE_RES, R_BUDGET, -8), E(EE_REL, N_PORTUGAL, 8) },
      "Plaider l'aléa technique", { E(EE_REL, N_PORTUGAL, -12), E(EE_RES, R_INFLUENCE, -3) }, NULL, NULL, 0 },
    { "Exode climatique planétaire", "L'ONU estime à 400 millions les déplacés climatiques et demande à l'Europe d'accueillir sa part selon une clé contraignante.",
      80, 2, { C(EC_TEMP_ABOVE, 0, 25) },
      "Accepter la clé de répartition", { E(EE_IMMIG, 0, 5), E(EE_RES, R_INFLUENCE, 6), E(EE_RES, R_COHESION, -3) },
      "Refuser et financer le Sud", { E(EE_RES, R_BUDGET, -12), E(EE_RES, R_INFLUENCE, -2), E(EE_RELGROUP, 1, 4) }, "Camps de transit géants", NULL, 3 },
    { "Camps de transit géants", "Les centres d'accueil saturent : 200 000 personnes vivent sous des tentes climatisées à l'énergie solaire.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Villes nouvelles d'accueil", { E(EE_RES, R_BUDGET, -12), E(EE_PROSP, 0, 2), E(EE_RES, R_COHESION, 2) },
      "Répartition forcée dans les communes", { E(EE_RES, R_COHESION, -3), E(EE_MIL, S_MOYENNES, 5), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Le chiffrement quantique tombe", "Un ordinateur quantique boréal casserait les chiffrements bancaires et militaires actuels. Tout le réseau de l'État doit migrer.",
      68, 2, { C(EC_TECH_ABOVE, T_GE, 1) },
      "Migration post-quantique d'urgence", { E(EE_RES, R_BUDGET, -12), E(EE_CAP, D_CYBER, 1), E(EE_RES, R_SOUVERAINETE, 2) },
      "Acheter la solution américaine", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_SOUVERAINETE, -4), E(EE_NATO, 0, 4) }, NULL, NULL, 0 },
    { "Viande cellulaire à bas prix", "Les steaks de culture coûtent désormais moins cher que la viande d'élevage. Les éleveurs bloquent les autoroutes.",
      60, 2, { C(EC_NONE, 0, 0) },
      "Subventionner la transition", { E(EE_RES, R_BUDGET, -8), E(EE_MITIG, 0, 3), E(EE_RES, R_ALIM, 3), E(EE_MIL, S_TRAVAILLEURS, 3) },
      "Protéger l'élevage", { E(EE_MIL, S_TRAVAILLEURS, -5), E(EE_RES, R_EAU, -3), E(EE_PROSP, 0, -1) }, NULL, NULL, 0 },
    { "Robots de combat autonomes", "L'industrie propose des quadrupèdes armés capables de tenir une ligne de front sans soldat. Ils tireraient sans validation humaine.",
      72, 2, { C(EC_TECH_ABOVE, T_INF, 2) },
      "Les déployer", { E(EE_RES, R_BUDGET, -10), E(EE_CAP, D_TERRE, 1), E(EE_RES, R_INFLUENCE, -5), E(EE_AGGRO, 0, 4) },
      "Garder l'humain dans la boucle", { E(EE_RES, R_INFLUENCE, 3), E(EE_RESEARCH, 0, 6), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
    { "Implants cognitifs au travail", "Des employeurs exigent des implants neuronaux de productivité. Ceux qui refusent sont relégués aux tâches subalternes.",
      75, 2, { C(EC_NONE, 0, 0) },
      "Interdire l'implant obligatoire", { E(EE_MIL, S_TRAVAILLEURS, -6), E(EE_PROSP, 0, -2), E(EE_MIL, S_ELITES, 3) },
      "Laisser le marché décider", { E(EE_PROSP, 0, 3), E(EE_MIL, S_TRAVAILLEURS, 6), E(EE_CON, S_TRAVAILLEURS, 3) }, NULL, NULL, 0 },
    { "Hôtel en orbite basse", "Un groupe hôtelier veut lancer depuis Kourou des séjours orbitaux à deux millions d'euros. Le carbone émis indigne les écologistes.",
      62, 1, { C(EC_NONE, 0, 0) },
      "Soutenir le projet", { E(EE_RES, R_BUDGET, 4), E(EE_RES, R_INFLUENCE, 3), E(EE_MIL, S_MOYENNES, 4), E(EE_MITIG, 0, -1) },
      "Taxer les vols de loisir", { E(EE_RES, R_BUDGET, 2), E(EE_MIL, S_ELITES, 4), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Montpellier sous dôme", "Après 52 degrés l'été dernier, la métropole veut se couvrir d'un dôme climatisé. Les communes voisines, exclues, crient à l'apartheid thermique.",
      85, 2, { C(EC_TEMP_ABOVE, 0, 26) },
      "Financer le dôme", { E(EE_RES, R_BUDGET, -14), E(EE_PROSP, 0, 3), E(EE_RES, R_ENERGIE, -4), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Refuges climatiques pour tous", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3), E(EE_PROSP, 0, -2) }, NULL, NULL, 0 },
    { "Rationnement calorique mondial", "Trois récoltes mondiales perdues d'affilée : l'ONU instaure des quotas de céréales par habitant. La France devrait exporter ses réserves.",
      90, 2, { C(EC_TEMP_ABOVE, 0, 27) },
      "Respecter les quotas", { E(EE_RES, R_ALIM, -6), E(EE_RES, R_INFLUENCE, 8), E(EE_RELGROUP, 1, 6) },
      "Garder nos stocks", { E(EE_RES, R_ALIM, 3), E(EE_RES, R_INFLUENCE, -8), E(EE_RELGROUP, 1, -8) }, NULL, NULL, 0 },
    { "Conseil planétaire du climat", "Une coalition propose une autorité mondiale capable d'imposer des plafonds d'émission et de sanctionner les États récalcitrants.",
      88, 1, { C(EC_TEMP_ABOVE, 0, 24) },
      "Céder cette souveraineté", { E(EE_RES, R_SOUVERAINETE, -6), E(EE_MITIG, 0, 6), E(EE_RES, R_INFLUENCE, 5) },
      "Défendre la souveraineté nationale", { E(EE_RES, R_SOUVERAINETE, 3), E(EE_RES, R_INFLUENCE, -4), E(EE_MIL, S_MOYENNES, -4) }, NULL, NULL, 0 },
    { "Panne en cascade du commerce mondial", "Ports inondés, canaux à sec, assureurs en faillite : le commerce maritime mondial chute d'un tiers en un trimestre.",
      100, 2, { C(EC_TEMP_ABOVE, 0, 30) },
      "Autarcie organisée", { E(EE_PROSP, 0, -3), E(EE_RES, R_SOUVERAINETE, 4), E(EE_RES, R_ALIM, 2) },
      "Convois escortés par la Marine", { E(EE_RES, R_BUDGET, -10), E(EE_CAP, D_MER, -1), E(EE_PROSP, 0, 1), E(EE_STRAT, SR_PUCES, 3) }, NULL, NULL, 0 },
    { "Les néo-luddites", "Des chômeurs remplacés par l'IA détruisent des centres de données à la masse. Ils réclament la « fin des machines pensantes ».",
      78, 2, { C(EC_UNEMP_ABOVE, 0, 10) },
      "Taxer les robots pour eux", { E(EE_RES, R_BUDGET, 5), E(EE_PROSP, 0, -2), E(EE_MIL, S_TRAVAILLEURS, -8), E(EE_MIL, S_ELITES, 4) },
      "Protéger les infrastructures", { E(EE_RES, R_BUDGET, -5), E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_PROSP, 0, 1) }, NULL, NULL, 0 },
    { "Thérapie contre le vieillissement", "Un traitement allonge de quinze ans l'espérance de vie en bonne santé. Il coûte 300 000 euros par patient.",
      82, 1, { C(EC_NONE, 0, 0) },
      "Remboursement universel", { E(EE_RES, R_BUDGET, -15), E(EE_RES, R_COHESION, 4), E(EE_PROSP, 0, 2) },
      "Réservé au privé", { E(EE_MIL, S_TRAVAILLEURS, 5), E(EE_MIL, S_ELITES, -5), E(EE_RES, R_BUDGET, 3) }, NULL, NULL, 0 },
    { "Usines géantes de captage du CO2", "Des aspirateurs à carbone géants retireraient 50 millions de tonnes par an, mais dévoreraient un dixième de notre électricité.",
      70, 2, { C(EC_TEMP_ABOVE, 0, 22) },
      "Construire la flotte", { E(EE_RES, R_BUDGET, -12), E(EE_MITIG, 0, 6), E(EE_RES, R_ENERGIE, -5) },
      "Planter des forêts à la place", { E(EE_RES, R_BUDGET, -4), E(EE_MITIG, 0, 2), E(EE_RES, R_EAU, -3) }, NULL, NULL, 0 },
    { "Syndrome de Kessler", "Une collision en chaîne pulvérise des centaines de satellites. L'orbite basse devient un champ de débris qui menace notre constellation.",
      85, 1, { C(EC_PROJECT_DONE, P_CONSTELL, 0) },
      "Satellites nettoyeurs", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_INFLUENCE, 4), E(EE_CAP, D_ESPACE, 1) },
      "Remonter en orbite haute", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_ESPACE, -1), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Forêts génétiquement modifiées", "Des chênes édités pour résister à 50 degrés et à la sécheresse sont prêts. Leur dissémination serait irréversible.",
      66, 2, { C(EC_TEMP_ABOVE, 0, 22) },
      "Replanter massivement", { E(EE_RES, R_BUDGET, -6), E(EE_MITIG, 0, 3), E(EE_RES, R_INFLUENCE, -3), E(EE_CON, S_MOYENNES, 3) },
      "Laisser la forêt s'adapter", { E(EE_RES, R_EAU, -2), E(EE_PROSP, 0, -1), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Nation insulaire engloutie", "Un archipel du Pacifique bientôt submergé demande à devenir territoire associé de la France pour offrir une patrie à ses 100 000 habitants.",
      80, 2, { C(EC_SEA_ABOVE, 0, 35) },
      "Accorder l'association", { E(EE_IMMIG, 0, 4), E(EE_RES, R_INFLUENCE, 6), E(EE_RES, R_BUDGET, -6) },
      "Aide humanitaire seule", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_COHESION, 1) }, NULL, NULL, 0 },
    { "Virus de synthèse dans un garage", "Un biohacker lyonnais a recréé un virus éteint avec une imprimante à ADN de bureau. Les plans sont en ligne.",
      74, 2, { C(EC_NONE, 0, 0) },
      "Contrôler toutes les imprimantes ADN", { E(EE_RES, R_BUDGET, -6), E(EE_RESEARCH, 0, -4), E(EE_RES, R_SOUVERAINETE, 2), E(EE_PROSP, 0, -1) },
      "Vaccins préventifs à grande échelle", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_COHESION, 2), E(EE_RESEARCH, 0, 5) }, NULL, NULL, 0 },
    { "Jumeau numérique du Président", "Un avatar parfait du chef de l'État annonce en direct la mobilisation générale. Des millions de citoyens y croient pendant six heures.",
      76, 2, { C(EC_FAC_HOSTILE, F_NEBULA, 20) },
      "Identité numérique certifiée", { E(EE_RES, R_BUDGET, -8), E(EE_FACCAP, F_NEBULA, -6), E(EE_CON, -1, 3) },
      "Riposte offensive contre Nébula", { E(EE_FAC, F_NEBULA, 8), E(EE_FACCAP, F_NEBULA, -8), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Supraconducteur à température ambiante", "Un laboratoire grenoblois confirme un supraconducteur stable à 20 degrés. Brevet national ou science ouverte ?",
      84, 1, { C(EC_TECH_ABOVE, T_LOG, 2) },
      "Brevet et industrie nationale", { E(EE_PROSP, 0, 3), E(EE_RES, R_ENERGIE, 3), E(EE_RES, R_INFLUENCE, -4) },
      "Publier pour l'humanité", { E(EE_RES, R_INFLUENCE, 8), E(EE_MITIG, 0, 3), E(EE_RES, R_SOUVERAINETE, -3) }, NULL, NULL, 0 },
    { "Démocratie algorithmique", "Un mouvement veut remplacer le Parlement par une IA arbitrant entre les préférences de tous. Il frôle les 30 pour cent.",
      86, 1, { C(EC_DEMOCRACY, 0, 0) },
      "Assemblée citoyenne assistée", { E(EE_RES, R_COHESION, 3), E(EE_RES, R_BUDGET, -5), E(EE_MIL, S_ELITES, 5) },
      "Défendre la démocratie représentative", { E(EE_CON, -1, 3), E(EE_MIL, S_MOYENNES, 4), E(EE_RES, R_SOUVERAINETE, 1) }, NULL, NULL, 0 },
    { "Remorquer un iceberg", "Faute d'usines de dessalement, des ingénieurs proposent de remorquer un iceberg antarctique jusqu'à Toulon. Six mois de voyage.",
      90, 2, { C(EC_PROJECT_NONE, P_DESSAL, 0), C(EC_TEMP_ABOVE, 0, 24) },
      "Tenter le remorquage", { E(EE_RES, R_BUDGET, -9), E(EE_RES, R_EAU, 6), E(EE_RES, R_INFLUENCE, 2) },
      "Rationnement draconien", { E(EE_RES, R_EAU, 3), E(EE_MIL, -1, 5), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Réacteurs modulaires sur barges", "Nos petits réacteurs sont demandés sur des barges pour alimenter les villes côtières du Sud. Une manne, mais un risque de prolifération.",
      72, 1, { C(EC_PROJECT_DONE, P_SMR, 0) },
      "Exporter la flotte", { E(EE_RES, R_BUDGET, 8), E(EE_RELGROUP, 1, 5), E(EE_STRAT, SR_URANIUM, -3), E(EE_CRED, 0, -2) },
      "Garder la technologie", { E(EE_RES, R_ENERGIE, 3), E(EE_RES, R_SOUVERAINETE, 2), E(EE_RES, R_INFLUENCE, -3) }, NULL, NULL, 0 },
    /* ---------------------------------------------------------------- migrations, Méditerranée, Afrique et humanitaire */
    { "Naufrage au large de Lampedusa", "Un chalutier chargé de 600 exilés a chaviré à 30 milles de Lampedusa : 412 morts. L'Europe se tourne vers Paris.",
      4, 3, { C(EC_NONE, 0, 0) },
      "Lancer une mission navale européenne", { E(EE_RES, R_BUDGET, -8), E(EE_IMMIG, 0, 3), E(EE_RES, R_INFLUENCE, 4), E(EE_RELGROUP, 0, 3) },
      "Fermer les ports aux navires d'ONG", { E(EE_RES, R_INFLUENCE, -4), E(EE_RES, R_COHESION, 2), E(EE_CON, S_MOYENNES, 3), E(EE_FACCAP, F_HYDRA, 3) }, "Mare Nostrum II sous le feu", NULL, 4 },
    { "Mare Nostrum II sous le feu", "Les passeurs du Cartel Hydra lancent des canots surchargés en comptant sur la mission : les arrivées ont doublé en un trimestre.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Maintenir la mission", { E(EE_IMMIG, 0, 4), E(EE_RES, R_INFLUENCE, 3), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Détruire les embarcations des passeurs", { E(EE_FAC, F_HYDRA, 6), E(EE_FACCAP, F_HYDRA, -5), E(EE_RES, R_BUDGET, -4), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Accord migratoire avec Rabat", "Le Maroc propose de reprendre ses ressortissants expulsés et de surveiller le détroit en échange de 50 000 visas de travail par an.",
      6, 2, { C(EC_ALIVE, N_MAROC, 0) },
      "Signer : visas contre réadmissions", { E(EE_REL, N_MAROC, 12), E(EE_IMMIG, 0, 2), E(EE_MIL, S_TRAVAILLEURS, 3), E(EE_RES, R_COHESION, 2) },
      "Refuser le marchandage", { E(EE_REL, N_MAROC, -8), E(EE_IMMIG, 0, 2), E(EE_RES, R_SOUVERAINETE, 2) }, "Rabat réclame son dû", NULL, 6 },
    { "Rabat réclame son dû", "Le Maroc exige des visas étudiants supplémentaires et un appui diplomatique sur ses frontières sahariennes pour continuer à appliquer l'accord.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Tenir nos engagements", { E(EE_REL, N_MAROC, 10), E(EE_IMMIG, 0, 2), E(EE_RELGROUP, 1, -3), E(EE_RES, R_INFLUENCE, -2) },
      "Renégocier à la baisse", { E(EE_REL, N_MAROC, -12), E(EE_IMMIG, 0, 3), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Garde-frontières tunisiens payés par Bruxelles", "Tunis propose de bloquer les départs de Sfax si l'Europe finance 3 000 garde-frontières et des vedettes rapides.",
      6, 2, { C(EC_ALIVE, N_TUNISIE, 0) },
      "Financer le dispositif", { E(EE_RES, R_BUDGET, -6), E(EE_REL, N_TUNISIE, 8), E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_COHESION, 2) },
      "Conditionner aux droits humains", { E(EE_REL, N_TUNISIE, -6), E(EE_RES, R_INFLUENCE, 3), E(EE_IMMIG, 0, 2) }, NULL, NULL, 0 },
    { "Pacte du Nil pour la main-d'oeuvre", "Le Caire propose d'envoyer 80 000 travailleurs agricoles égyptiens pour sauver les récoltes françaises frappées par la pénurie de bras.",
      10, 2, { C(EC_ALIVE, N_EGYPTE, 0) },
      "Ouvrir des visas saisonniers", { E(EE_IMMIG, 0, 2), E(EE_RES, R_ALIM, 3), E(EE_REL, N_EGYPTE, 10), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Investir dans l'agriculture égyptienne", { E(EE_RES, R_BUDGET, -8), E(EE_REL, N_EGYPTE, 6), E(EE_RELGROUP, 1, 4) }, NULL, NULL, 0 },
    { "Dix mille exilés sur le littoral calaisien", "Les campements s'étendent de Calais à Dunkerque ; Londres exige que Paris empêche toute traversée de la Manche.",
      4, 2, { C(EC_ALIVE, N_UK, 0) },
      "Ouvrir des centres d'accueil dignes", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, 2), E(EE_MIL, S_TRAVAILLEURS, 3), E(EE_REL, N_UK, -4) },
      "Évacuations et clôtures", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, -3), E(EE_CON, -1, 3), E(EE_REL, N_UK, 6) }, NULL, "Noyades dans la Manche", 3 },
    { "Noyades dans la Manche", "Chassés des plages, les exilés tentent la traversée par grand vent : 40 morts en une nuit. Londres accuse Paris de laxisme.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Accord de partage avec Londres", { E(EE_REL, N_UK, 10), E(EE_IMMIG, 0, 1), E(EE_RES, R_SOUVERAINETE, -2), E(EE_RES, R_INFLUENCE, 2) },
      "Traquer les réseaux de passeurs", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -4), E(EE_FAC, F_HYDRA, 4), E(EE_REL, N_UK, -6) }, NULL, NULL, 0 },
    { "Mayotte submergée par les arrivées", "Fuyant la famine des Grands Lacs, 40 000 personnes ont débarqué à Mayotte en six mois ; l'eau et les écoles manquent.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Pont aérien vers la métropole", { E(EE_IMMIG, 0, 2), E(EE_RES, R_BUDGET, -5), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_INFLUENCE, 2) },
      "Reconduites massives aux frontières", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, -4), E(EE_CON, S_MOYENNES, 3), E(EE_RES, R_COHESION, 2) }, NULL, "Mayotte s'embrase", 3 },
    { "Mayotte s'embrase", "Barrages routiers, coupures d'eau et affrontements entre collectifs : l'île est paralysée depuis trois semaines.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Envoyer la gendarmerie mobile", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_TERRE, -1), E(EE_RES, R_COHESION, 2), E(EE_CON, -1, 3) },
      "Plan d'urgence eau et logement", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_EAU, -2), E(EE_MIL, -1, -5) }, NULL, NULL, 0 },
    { "Famine au Sahel central", "Trois récoltes perdues d'affilée : douze millions de Sahéliens sont menacés de famine et les colonnes de déplacés montent vers le nord.",
      12, 3, { C(EC_TEMP_ABOVE, 0, 18) },
      "Pont aérien humanitaire", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_ALIM, -3), E(EE_RES, R_INFLUENCE, 5) },
      "S'en remettre à l'ONU", { E(EE_RES, R_INFLUENCE, -3), E(EE_FACCAP, F_SABLES, 6), E(EE_IMMIG, 0, 2) }, "Le pont aérien de Niamey", NULL, 3 },
    { "Le pont aérien de Niamey", "Les avions français nourrissent deux millions de personnes, mais le Califat des Sables menace de frapper l'aéroport.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Déployer une force de protection", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_AIR, -1), E(EE_FAC, F_SABLES, 6), E(EE_FACCAP, F_SABLES, -5) },
      "Passer le relais aux ONG", { E(EE_RES, R_INFLUENCE, -2), E(EE_FACCAP, F_SABLES, 3), E(EE_RES, R_BUDGET, 3) }, "Les récoltes de l'espoir au Sahel", NULL, 5 },
    { "Les récoltes de l'espoir au Sahel", "Grâce aux semences résistantes acheminées par le pont aérien, le Sahel connaît sa première bonne récolte en dix ans.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Programme agricole commun", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ALIM, 3), E(EE_RES, R_INFLUENCE, 4), E(EE_FACCAP, F_SABLES, -4) },
      "Se retirer sur un succès", { E(EE_RES, R_INFLUENCE, 2), E(EE_RES, R_BUDGET, 3), E(EE_FACCAP, F_SABLES, 3) }, NULL, NULL, 0 },
    { "Le Califat recrute dans les camps", "Les recruteurs du Califat des Sables offrent solde et vivres aux jeunes réfugiés des camps du lac Tchad.",
      8, 2, { C(EC_FAC_HOSTILE, F_SABLES, 15) },
      "Écoles et emplois dans les camps", { E(EE_RES, R_BUDGET, -7), E(EE_FACCAP, F_SABLES, -6), E(EE_RES, R_INFLUENCE, 2) },
      "Frappes ciblées sur les recruteurs", { E(EE_FAC, F_SABLES, 8), E(EE_FACCAP, F_SABLES, -4), E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_BUDGET, -3) }, NULL, NULL, 0 },
    { "Un État sahélien s'effondre", "La capitale est tombée aux mains des milices : un million de déplacés marchent vers le nord et les côtes libyennes.",
      14, 2, { C(EC_NONE, 0, 0) },
      "Évacuer et accueillir les civils", { E(EE_IMMIG, 0, 2), E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, 3), E(EE_RES, R_INFLUENCE, 3) },
      "Soutenir une junte de transition", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_INFLUENCE, -3), E(EE_FACCAP, F_SABLES, -4), E(EE_CON, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Les réfugiés de l'eau andalous", "L'Andalousie n'a plus d'eau courante trois mois par an : 300 000 Espagnols demandent à s'installer dans le Sud-Ouest.",
      30, 2, { C(EC_TEMP_ABOVE, 0, 20), C(EC_ALIVE, N_ESPAGNE, 0) },
      "Libre installation européenne", { E(EE_IMMIG, 0, 5), E(EE_REL, N_ESPAGNE, 12), E(EE_RES, R_EAU, -3), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Envoyer de l'eau plutôt qu'accueillir", { E(EE_RES, R_EAU, -5), E(EE_RES, R_BUDGET, -5), E(EE_REL, N_ESPAGNE, 6) }, "Les Andalous du Roussillon", NULL, 5 },
    { "Les Andalous du Roussillon", "Les arrivants espagnols relancent maraîchage et commerces, mais les loyers flambent à Perpignan et Narbonne.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Plan logement régional", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_COHESION, 3), E(EE_PROSP, 0, 2) },
      "Quotas d'installation par commune", { E(EE_RES, R_COHESION, -2), E(EE_MIL, S_MOYENNES, 4), E(EE_REL, N_ESPAGNE, -6), E(EE_RES, R_ALIM, 2) }, NULL, NULL, 0 },
    { "L'Italie du Sud se vide", "Assoiffées, la Sicile et les Pouilles perdent un habitant sur dix ; Rome réclame une répartition européenne des déplacés.",
      32, 2, { C(EC_TEMP_ABOVE, 0, 20), C(EC_ALIVE, N_ITALIE, 0) },
      "Accepter un quota de déplacés", { E(EE_IMMIG, 0, 3), E(EE_REL, N_ITALIE, 12), E(EE_RELGROUP, 0, 4), E(EE_MIL, S_TRAVAILLEURS, 3) },
      "Proposer un fonds hydraulique", { E(EE_RES, R_BUDGET, -8), E(EE_REL, N_ITALIE, 6), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Évacuation des Cyclades", "Sans eau ni touristes, les Cyclades sont évacuées ; Athènes réclame des navires et des places d'accueil à ses partenaires.",
      34, 2, { C(EC_TEMP_ABOVE, 0, 20), C(EC_ALIVE, N_GRECE, 0) },
      "Envoyer la Marine et accueillir", { E(EE_CAP, D_MER, -1), E(EE_IMMIG, 0, 2), E(EE_REL, N_GRECE, 12), E(EE_RES, R_INFLUENCE, 3) },
      "Des navires-citernes seulement", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_EAU, -2), E(EE_REL, N_GRECE, 5) }, NULL, NULL, 0 },
    { "Dessalement solaire au Cap Bon", "Un consortium franco-tunisien propose une usine géante de dessalement solaire pour fixer les populations du littoral.",
      16, 2, { C(EC_ALIVE, N_TUNISIE, 0) },
      "Financer l'usine", { E(EE_RES, R_BUDGET, -10), E(EE_REL, N_TUNISIE, 12), E(EE_RELGROUP, 1, 4) },
      "Exiger des contrats pour nos firmes", { E(EE_RES, R_BUDGET, 3), E(EE_REL, N_TUNISIE, -6), E(EE_RES, R_INFLUENCE, -2), E(EE_IMMIG, 0, 1) }, "Le Cap Bon reverdit", NULL, 6 },
    { "Le Cap Bon reverdit", "L'usine abreuve deux millions de personnes ; les départs vers l'Europe chutent et Tunis propose d'en bâtir une seconde.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Doubler la mise", { E(EE_RES, R_BUDGET, -8), E(EE_REL, N_TUNISIE, 10), E(EE_RES, R_INFLUENCE, 3), E(EE_FACCAP, F_HYDRA, -3) },
      "Céder l'exploitation à Tunis", { E(EE_RES, R_BUDGET, 4), E(EE_REL, N_TUNISIE, 4), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Taxer les transferts de la diaspora", "Une proposition de loi veut taxer les 15 milliards d'euros envoyés chaque année par la diaspora vers le Maghreb et l'Afrique.",
      8, 2, { C(EC_NONE, 0, 0) },
      "Taxer les transferts", { E(EE_RES, R_BUDGET, 5), E(EE_RELGROUP, 1, -6), E(EE_MIL, S_TRAVAILLEURS, 5) },
      "Les défiscaliser pour le codéveloppement", { E(EE_RES, R_BUDGET, -4), E(EE_RELGROUP, 1, 5), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "Cargos fantômes d'Hydra en Corse", "Des cargos sans pavillon affrétés par le Cartel Hydra débarquent de nuit des centaines d'exilés sur les plages corses.",
      8, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 15) },
      "Abordages par la Marine", { E(EE_RES, R_BUDGET, -4), E(EE_FAC, F_HYDRA, 7), E(EE_FACCAP, F_HYDRA, -6), E(EE_CAP, D_MER, -1) },
      "Remonter la filière financière", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -3), E(EE_IMMIG, 0, 2) }, NULL, "Les banquiers d'Hydra démasqués", 4 },
    { "Les banquiers d'Hydra démasqués", "L'enquête révèle que des banques maltaises et chypriotes blanchissent les profits des passeurs du Cartel Hydra.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Geler leurs avoirs", { E(EE_FACCAP, F_HYDRA, -8), E(EE_FAC, F_HYDRA, 6), E(EE_RELGROUP, 0, -3), E(EE_RES, R_BUDGET, 4) },
      "Coopération discrète avec La Valette", { E(EE_FACCAP, F_HYDRA, -3), E(EE_RELGROUP, 0, 3), E(EE_RES, R_INFLUENCE, 2) }, NULL, NULL, 0 },
    { "L'application des passeurs", "Le Cartel Hydra vend des places sur des canots via une appli chiffrée, avec avis clients et remboursement en cas d'échec.",
      10, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 10) },
      "Infiltrer l'application", { E(EE_RES, R_BUDGET, -5), E(EE_FACCAP, F_HYDRA, -6), E(EE_FAC, F_HYDRA, 5) },
      "La faire retirer des plateformes", { E(EE_RES, R_INFLUENCE, 2), E(EE_FACCAP, F_HYDRA, -2), E(EE_FACCAP, F_NEBULA, 3) }, NULL, NULL, 0 },
    { "Couloir humanitaire depuis la Libye", "Des milliers d'Africains croupissent dans des centres de détention libyens ; le HCR propose un couloir aérien vers l'Europe.",
      8, 2, { C(EC_ALIVE, N_LIBYE, 0) },
      "Ouvrir le couloir", { E(EE_IMMIG, 0, 2), E(EE_RES, R_INFLUENCE, 5), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_BUDGET, -3) },
      "Financer les centres sur place", { E(EE_RES, R_BUDGET, -5), E(EE_RES, R_INFLUENCE, -4), E(EE_FACCAP, F_HYDRA, 4), E(EE_REL, N_LIBYE, 6) }, NULL, NULL, 0 },
    { "Un barrage pour le fleuve Sénégal", "Dakar et Nouakchott sollicitent la France pour un barrage d'irrigation capable de nourrir trois millions de paysans.",
      14, 2, { C(EC_NONE, 0, 0) },
      "Financer le barrage", { E(EE_RES, R_BUDGET, -10), E(EE_RES, R_INFLUENCE, 4), E(EE_FACCAP, F_SABLES, -4), E(EE_MITIG, 0, 1) },
      "Laisser un fonds étranger s'en charger", { E(EE_RES, R_INFLUENCE, -4), E(EE_RES, R_BUDGET, 2), E(EE_IMMIG, 0, 1) }, "Le chantier du barrage attaqué", NULL, 5 },
    { "Le chantier du barrage attaqué", "Le Califat des Sables fait sauter une partie du chantier sénégalais et menace d'enlever les ingénieurs français.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Protéger le chantier militairement", { E(EE_RES, R_BUDGET, -5), E(EE_CAP, D_TERRE, -1), E(EE_FAC, F_SABLES, 6), E(EE_RES, R_INFLUENCE, 3) },
      "Rapatrier les ingénieurs", { E(EE_RES, R_INFLUENCE, -5), E(EE_FACCAP, F_SABLES, 4), E(EE_RES, R_BUDGET, 3) }, NULL, NULL, 0 },
    { "Pêcheurs sétois devant le juge", "Des pêcheurs de Sète qui ont recueilli 80 naufragés sont poursuivis pour aide à l'entrée irrégulière ; le port est en grève.",
      6, 2, { C(EC_NONE, 0, 0) },
      "Loi de protection des sauveteurs", { E(EE_RES, R_INFLUENCE, 3), E(EE_RES, R_COHESION, 2), E(EE_IMMIG, 0, 1), E(EE_MIL, S_ELITES, 3) },
      "Laisser la justice suivre son cours", { E(EE_RES, R_INFLUENCE, -3), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_RES, R_SOUVERAINETE, 2) }, NULL, NULL, 0 },
    { "Réadmissions contre mémoire", "Alger accepte de reprendre ses ressortissants en situation irrégulière si Paris présente des excuses officielles pour la colonisation.",
      12, 2, { C(EC_ALIVE, N_ALGERIE, 0) },
      "Présenter des excuses officielles", { E(EE_REL, N_ALGERIE, 15), E(EE_RELGROUP, 1, 4), E(EE_MIL, S_MOYENNES, 4), E(EE_RES, R_COHESION, -2) },
      "Refuser le marchandage mémoriel", { E(EE_REL, N_ALGERIE, -10), E(EE_RES, R_COHESION, 2), E(EE_RES, R_ENERGIE, -2), E(EE_IMMIG, 0, 1) }, NULL, NULL, 0 },
    { "Un hub européen de l'asile au Caire", "Le Caire propose d'examiner les demandes d'asile destinées à l'Europe dans des camps égyptiens, contre six milliards d'euros.",
      14, 2, { C(EC_ALIVE, N_EGYPTE, 0) },
      "Externaliser l'asile au Caire", { E(EE_RES, R_BUDGET, -6), E(EE_REL, N_EGYPTE, 12), E(EE_RES, R_INFLUENCE, -5), E(EE_RES, R_COHESION, 3) },
      "Garder l'examen en Europe", { E(EE_IMMIG, 0, 2), E(EE_RES, R_INFLUENCE, 2), E(EE_REL, N_EGYPTE, -6) }, "Scandale dans les camps du Caire", NULL, 4 },
    { "Scandale dans les camps du Caire", "Des vidéos montrent des mauvais traitements dans le hub financé par l'Europe ; des ONG portent plainte contre la France.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Suspendre les versements", { E(EE_REL, N_EGYPTE, -12), E(EE_RES, R_BUDGET, 4), E(EE_RES, R_INFLUENCE, 3), E(EE_IMMIG, 0, 2) },
      "Imposer des inspecteurs européens", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, -2), E(EE_REL, N_EGYPTE, 3), E(EE_CON, S_MOYENNES, 3) }, NULL, NULL, 0 },
    { "Le Califat taxe les convois humanitaires", "Les convois d'ONG au Sahel versent un péage au Califat des Sables pour passer ; Paris doit dire s'il tolère la pratique.",
      10, 2, { C(EC_FAC_HOSTILE, F_SABLES, 10) },
      "Interdire tout paiement", { E(EE_FAC, F_SABLES, 5), E(EE_FACCAP, F_SABLES, -4), E(EE_RES, R_INFLUENCE, -3), E(EE_IMMIG, 0, 2) },
      "Fermer les yeux pour nourrir", { E(EE_FACCAP, F_SABLES, 5), E(EE_RES, R_INFLUENCE, 2), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    { "Lampedusa bloque son port", "9 000 arrivants pour 6 000 habitants : les Lampédusains bloquent le port et Rome demande à Paris d'en prendre la moitié.",
      8, 2, { C(EC_ALIVE, N_ITALIE, 0) },
      "Accueillir la moitié", { E(EE_IMMIG, 0, 2), E(EE_REL, N_ITALIE, 10), E(EE_MIL, S_TRAVAILLEURS, 4) },
      "Envoyer des fonds et des tentes", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_ITALIE, -4), E(EE_RELGROUP, 0, -2), E(EE_RES, R_COHESION, 2) }, NULL, NULL, 0 },
    { "Le visa climatique africain", "L'Union africaine propose un « visa climatique » permettant aux déplacés du Sahel de travailler cinq ans en Europe.",
      40, 2, { C(EC_TEMP_ABOVE, 0, 20) },
      "Signer le visa climatique", { E(EE_IMMIG, 0, 4), E(EE_RES, R_INFLUENCE, 5), E(EE_PROSP, 0, 2), E(EE_MIL, S_TRAVAILLEURS, 5) },
      "Proposer un fonds d'adaptation", { E(EE_RES, R_BUDGET, -8), E(EE_RES, R_INFLUENCE, 2), E(EE_MITIG, 0, 1) }, NULL, NULL, 0 },
    { "Saisonniers bloqués à Tanger", "En représailles à une décision de justice, le Maroc retient 40 000 saisonniers agricoles : les fraises pourrissent en Provence.",
      12, 2, { C(EC_ALIVE, N_MAROC, 0) },
      "Céder sur le dossier judiciaire", { E(EE_REL, N_MAROC, 8), E(EE_RES, R_ALIM, 3), E(EE_RES, R_SOUVERAINETE, -3) },
      "Mécaniser les récoltes", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_ALIM, -3), E(EE_REL, N_MAROC, -5), E(EE_PROSP, 0, 1) }, NULL, NULL, 0 },
    { "Garde-côtes libyens vendus à Hydra", "Des garde-côtes libyens équipés par l'Europe revendent les exilés interceptés aux réseaux du Cartel Hydra.",
      10, 2, { C(EC_ALIVE, N_LIBYE, 0), C(EC_FAC_HOSTILE, F_HYDRA, 10) },
      "Couper l'aide aux garde-côtes", { E(EE_REL, N_LIBYE, -8), E(EE_IMMIG, 0, 2), E(EE_FACCAP, F_HYDRA, -4), E(EE_RES, R_INFLUENCE, 2) },
      "Former et encadrer sur place", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_LIBYE, 4), E(EE_FACCAP, F_HYDRA, -1), E(EE_CAP, D_MER, -1) }, NULL, NULL, 0 },
    { "Retour des diasporas vers le Maghreb", "Le Maghreb, dopé par l'hydrogène vert, attire des dizaines de milliers d'ingénieurs franco-maghrébins : un exode inversé.",
      36, 2, { C(EC_NONE, 0, 0) },
      "Créer des passerelles binationales", { E(EE_RELGROUP, 1, 6), E(EE_RES, R_ENERGIE, 3), E(EE_PROSP, 0, -2), E(EE_RES, R_BUDGET, -3) },
      "Primes pour les retenir en France", { E(EE_RES, R_BUDGET, -7), E(EE_PROSP, 0, 2), E(EE_MIL, S_TRAVAILLEURS, 3) }, NULL, NULL, 0 },
    { "Bidonvilles de déplacés sur la Durance", "Entre migrants africains et Espagnols assoiffés, des campements de 20 000 personnes bordent la Durance sans eau potable.",
      28, 2, { C(EC_TEMP_ABOVE, 0, 19) },
      "Raccorder les camps au réseau", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_EAU, -3), E(EE_RES, R_COHESION, 2) },
      "Démanteler et disperser", { E(EE_RES, R_BUDGET, -2), E(EE_CON, -1, 3), E(EE_MIL, S_TRAVAILLEURS, 4), E(EE_FACCAP, F_HYDRA, 3) }, NULL, NULL, 0 },
    { "Un navire d'ONG sous pavillon français", "Une ONG de sauvetage, privée de pavillon par Malte, demande à naviguer sous drapeau tricolore pour patrouiller au large de la Libye.",
      5, 2, { C(EC_NONE, 0, 0) },
      "Accorder le pavillon", { E(EE_RES, R_INFLUENCE, 4), E(EE_IMMIG, 0, 2), E(EE_FACCAP, F_HYDRA, -2), E(EE_MIL, S_MOYENNES, 3) },
      "Refuser poliment", { E(EE_RES, R_INFLUENCE, -3), E(EE_RES, R_COHESION, 1), E(EE_CON, S_TRAVAILLEURS, 3) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : nation pirate et barbaresques modernes */
    { "Rançon pour un ferry de Bastia", "Des pirates arraisonnent un ferry au large du cap Corse et exigent 80 millions en cryptomonnaie pour libérer ses 900 passagers.",
      8, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 15) },
      "Payer discrètement la rançon", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, 4), E(EE_RES, R_COHESION, 2) },
      "Donner l'assaut en mer", { E(EE_RES, R_BUDGET, -3), E(EE_CAP, D_MER, -1), E(EE_FAC, F_HYDRA, 3) }, NULL, "Commandos à bord du ferry", 2 },
    { "Commandos à bord du ferry", "L'assaut libère le navire, mais onze passagers sont blessés. Les images des nageurs de combat font le tour du monde.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Célébrer les commandos", { E(EE_RES, R_INFLUENCE, 3), E(EE_FACCAP, F_HYDRA, -3), E(EE_RES, R_COHESION, -2) },
      "Enquête et indemnisation des blessés", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_COHESION, 2), E(EE_FACCAP, F_HYDRA, -2) }, NULL, NULL, 0 },
    { "Une nation pirate proclamée en mer", "Flottilles, plateformes et serveurs offshore se fédèrent en « État libre des Marées » : sans territoire, il a un drapeau, une monnaie et des raids.",
      18, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 25) },
      "Former une coalition navale", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_MER, -1), E(EE_RELGROUP, 0, 5), E(EE_FACCAP, F_HYDRA, -5) },
      "Ignorer cette fiction juridique", { E(EE_FACCAP, F_HYDRA, 5), E(EE_PROSP, 0, -2), E(EE_RES, R_INFLUENCE, -2) }, "Les Marées dispersent leur flotte", NULL, 4 },
    { "Les Marées dispersent leur flotte", "Plutôt que d'affronter la coalition, la nation pirate éclate en cent flottilles qui se replient vers l'Atlantique et le golfe de Guinée.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Les traquer jusqu'en Atlantique", { E(EE_RES, R_BUDGET, -6), E(EE_FACCAP, F_HYDRA, -6), E(EE_CAP, D_MER, -1) },
      "Dissoudre la coalition", { E(EE_RES, R_BUDGET, 4), E(EE_FACCAP, F_HYDRA, 3), E(EE_RELGROUP, 0, -3) }, NULL, NULL, 0 },
    { "Paradis numérique sur une plateforme gazière", "Une plateforme désaffectée au large de l'Irlande héberge banques, serveurs et passeports de la nation pirate. Elle blanchit déjà les rançons.",
      30, 2, { C(EC_NONE, 0, 0) },
      "Arraisonner la plateforme", { E(EE_AGGRO, 0, 3), E(EE_FACCAP, F_HYDRA, -6), E(EE_REL, N_IRLANDE, -6), E(EE_CAP, D_MER, -1) },
      "Couper ses accès financiers", { E(EE_RES, R_BUDGET, -4), E(EE_FACCAP, F_HYDRA, -2), E(EE_FAC, F_HYDRA, 3) }, NULL, NULL, 0 },
    { "Péage pirate au canal de Sicile", "Des vedettes rapides imposent un « droit de passage » aux cargos entre la Tunisie et la Sicile. Les assureurs triplent leurs primes.",
      40, 2, { C(EC_FAC_HOSTILE, F_HYDRA, 20) },
      "Patrouilles conjointes avec Rome", { E(EE_RES, R_BUDGET, -5), E(EE_REL, N_ITALIE, 8), E(EE_FACCAP, F_HYDRA, -4) },
      "Laisser les armateurs payer", { E(EE_PROSP, 0, -2), E(EE_FACCAP, F_HYDRA, 4), E(EE_RES, R_ALIM, -2) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : sphères immersives et guerre cognitive */
    { "Les sphères immersives séduisent le pays", "Un Français sur trois vit l'essentiel de son temps dans une sphère immersive taillée à ses affinités : ses infos, ses amis, sa réalité.",
      14, 2, { C(EC_NONE, 0, 0) },
      "Imposer un tronc commun d'information", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_INFO, 1), E(EE_CON, -1, 3) },
      "Laisser prospérer le secteur", { E(EE_PROSP, 0, 2), E(EE_RES, R_COHESION, -4) }, "Les sphères contournent le tronc commun", NULL, 4 },
    { "Les sphères contournent le tronc commun", "Les opérateurs relèguent le tronc commun obligatoire dans un couloir virtuel que personne ne visite. La loi est respectée à la lettre.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Sanctionner les opérateurs", { E(EE_RES, R_BUDGET, 4), E(EE_PROSP, 0, -2), E(EE_MIL, S_MOYENNES, 6) },
      "Coproduire des contenus avec eux", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 2), E(EE_RES, R_SOUVERAINETE, -2) }, NULL, NULL, 0 },
    { "Guerre cognitive entre sphères", "Deux sphères rivales s'accusent de crimes inventés. Nébula alimente les deux camps ; des agressions bien réelles suivent dans trois villes.",
      32, 2, { C(EC_FAC_HOSTILE, F_NEBULA, 20) },
      "Fermer les deux sphères", { E(EE_RES, R_COHESION, -3), E(EE_RES, R_SOUVERAINETE, 2), E(EE_CON, -1, 4), E(EE_FACCAP, F_NEBULA, -4) },
      "Médiateurs dans chaque sphère", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_INFO, 1), E(EE_FAC, F_NEBULA, 3) }, NULL, NULL, 0 },
    { "Contamination dans la ville inondée", "Une semaine après la crue, une fièvre hémorragique éclate à Valmorne. Les analyses révèlent une souche cultivée en laboratoire.",
      44, 2, { C(EC_NONE, 0, 0) },
      "L'armée évacue la ville", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_TERRE, -1), E(EE_RES, R_COHESION, 2) },
      "Cordon sanitaire et soins sur place", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, -4), E(EE_FACCAP, F_SABLES, 3), E(EE_RES, R_ALIM, -2) }, "Valmorne refuse l'évacuation", NULL, 2 },
    { "Valmorne refuse l'évacuation", "Persuadés par leur sphère que l'armée a elle-même lâché le virus, des habitants barricadent les rues contre les convois militaires.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Évacuer par la force", { E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 5), E(EE_RES, R_BUDGET, -2) },
      "Envoyer médecins et élus locaux", { E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, 3), E(EE_CAP, D_TERRE, -1) }, "L'armée doit regagner la confiance", NULL, 4 },
    { "L'armée doit regagner la confiance", "L'évacuation forcée a sauvé des vies, mais la moitié des Français doutent désormais de leurs soldats, selon un sondage.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Commission d'enquête publique", { E(EE_RES, R_COHESION, 3), E(EE_CRED, 0, -2), E(EE_RES, R_BUDGET, -2) },
      "Campagne de communication militaire", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_INFO, 1), E(EE_CON, -1, 3) }, NULL, NULL, 0 },
    { "Le socle commun de réalité se dissout", "Les sondeurs renoncent : les Français ne s'accordent plus ni sur la date des dernières élections, ni sur le score du dernier match des Bleus.",
      60, 2, { C(EC_DEMOCRACY, 0, 0) },
      "Fondation publique des faits partagés", { E(EE_RES, R_BUDGET, -6), E(EE_RES, R_COHESION, 3), E(EE_CAP, D_INFO, 1) },
      "Accepter une société d'archipels", { E(EE_RES, R_COHESION, -5), E(EE_PROSP, 0, 2), E(EE_MIL, S_ELITES, -4) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : hyperforteresses et junte en Méditerranée orientale */
    { "Bouclier contre les armes hypervéloces", "L'état-major propose le programme Vigie-Dôme : radars, intercepteurs et satellites d'alerte pour faire du pays une hyperforteresse d'ici 2045.",
      26, 2, { C(EC_NONE, 0, 0) },
      "Financer un bouclier national", { E(EE_RES, R_BUDGET, -11), E(EE_CAP, D_AIR, 1), E(EE_CRED, 0, 3), E(EE_DEBT, 0, 10) },
      "Se placer sous le bouclier allié", { E(EE_RES, R_BUDGET, -4), E(EE_NATO, 0, 4), E(EE_RES, R_SOUVERAINETE, -4) }, "Vigie-Dôme face à sa première salve", NULL, 5 },
    { "Vigie-Dôme face à sa première salve", "Trois planeurs hypervéloces d'essai frôlent notre espace aérien. Vigie-Dôme en détecte deux ; le troisième passe inaperçu jusqu'à la mer.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rendre l'échec public et corriger", { E(EE_RES, R_BUDGET, -4), E(EE_RESEARCH, 0, 8), E(EE_RES, R_INFLUENCE, -2), E(EE_RES, R_COHESION, -2) },
      "Classer l'incident secret-défense", { E(EE_CRED, 0, 2), E(EE_CAP, D_AIR, -1), E(EE_RES, R_INFLUENCE, 1) }, NULL, NULL, 0 },
    { "Junte nationaliste à Ankara", "Un comité d'officiers prend le pouvoir à Ankara, promet de restaurer la grandeur impériale et déploie ses propres boucliers face à la Grèce.",
      34, 2, { C(EC_ALIVE, N_TURQUIE, 0) },
      "Sanctions et soutien à Athènes", { E(EE_REL, N_TURQUIE, -12), E(EE_REL, N_GRECE, 10), E(EE_NATO, 0, -4), E(EE_RES, R_BUDGET, -2) },
      "Dialogue pragmatique avec la junte", { E(EE_REL, N_TURQUIE, 6), E(EE_RELGROUP, 0, -5), E(EE_RES, R_INFLUENCE, -2) }, NULL, NULL, 0 },
    { "Front gelé derrière les boucliers", "En mer Égée, deux réseaux de boucliers se font face : aucun missile ne passe, aucun camp ne recule. Le conflit est figé depuis dix-huit mois.",
      46, 2, { C(EC_ALIVE, N_GRECE, 0) },
      "Maintenir nos frégates sur zone", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_MER, -1), E(EE_REL, N_GRECE, 8), E(EE_NATO, 0, 3) },
      "Rapatrier la flotte", { E(EE_RES, R_BUDGET, 4), E(EE_CRED, 0, -3), E(EE_REL, N_GRECE, -8) }, NULL, NULL, 0 },
    { "Saboteur dans une station radar", "Un technicien sous-traitant est surpris en train d'installer un module espion dans une station de détection du bouclier, près de Mont-de-Marsan.",
      40, 2, { C(EC_NONE, 0, 0) },
      "Purger les sous-traitants", { E(EE_RES, R_BUDGET, -4), E(EE_PROSP, 0, -1), E(EE_CAP, D_CYBER, 1) },
      "Le laisser faire pour remonter la filière", { E(EE_FACCAP, F_BOREALE, -4), E(EE_RES, R_SOUVERAINETE, -1) }, NULL, "Le réseau de détection aveuglé", 3 },
    { "Le réseau de détection aveuglé", "La filière est remontée, mais un second module déclenche une cyberattaque : un quart de nos radars reste aveugle pendant six heures.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Frapper les commanditaires en ligne", { E(EE_FAC, F_BOREALE, 4), E(EE_FACCAP, F_BOREALE, -5), E(EE_AGGRO, 0, 2) },
      "Reconstruire et durcir le réseau", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_CYBER, 1), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
    { "Engin nucléaire rudimentaire en Orient", "Le renseignement confirme qu'un régime de Méditerranée orientale a assemblé un engin nucléaire rudimentaire, transportable par camion ou par cargo.",
      64, 2, { C(EC_PROLIFERATION, 0, 0) },
      "Frappe préventive sur l'arsenal", { E(EE_AGGRO, 0, 5), E(EE_CRED, 0, 4), E(EE_CAP, D_AIR, -1), E(EE_NATO, 0, -4) },
      "Garantie nucléaire aux voisins", { E(EE_CRED, 0, 3), E(EE_RES, R_BUDGET, -4), E(EE_REL, N_GRECE, 6), E(EE_RES, R_COHESION, -2) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : après la nuit carbonique, énergie et guerre du vivant */
    { "Quotas d'énergie pour les armées", "Après des hivers de rationnement, le gouvernement impose aux armées une consommation divisée par trois. Les chasseurs restent au sol.",
      38, 2, { C(EC_RES_BELOW, R_ENERGIE, 50) },
      "Accepter le plafond", { E(EE_RES, R_ENERGIE, 4), E(EE_CAP, D_AIR, -1), E(EE_RESEARCH, 0, 5) },
      "Exempter la défense", { E(EE_RES, R_ENERGIE, -4), E(EE_RES, R_COHESION, -3), E(EE_MIL, S_TRAVAILLEURS, 6) }, NULL, NULL, 0 },
    { "Dirigeables solaires et drones à voile", "La DGA propose une armée sobre : dirigeables solaires de surveillance, drones à voile, véhicules à assistance électrique pour l'infanterie.",
      50, 2, { C(EC_TECH_ABOVE, T_LOG, 1) },
      "Lancer le programme Zéphyr", { E(EE_RES, R_BUDGET, -6), E(EE_RESEARCH, 0, 8), E(EE_RES, R_ENERGIE, 2) },
      "Garder les moteurs thermiques", { E(EE_RES, R_ENERGIE, -3), E(EE_MITIG, 0, -1), E(EE_RES, R_BUDGET, 3) }, "Les dirigeables Zéphyr en patrouille", NULL, 4 },
    { "Les dirigeables Zéphyr en patrouille", "Les premiers dirigeables survolent le golfe de Gascogne pour une fraction de l'énergie d'un avion de patrouille. Mais ils sont lents et vulnérables.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Généraliser à la surveillance maritime", { E(EE_CAP, D_MER, 1), E(EE_RES, R_ENERGIE, 2), E(EE_RES, R_BUDGET, -4) },
      "Les réserver aux Outre-mer", { E(EE_RES, R_BUDGET, -2), E(EE_CAP, D_EM, 1), E(EE_RES, R_INFLUENCE, 1) }, NULL, NULL, 0 },
    { "Biologistes enlevés sur l'Oyapock", "Une mission d'inventaire du vivant est attaquée sur le haut Oyapock par des rebelles armés. Six chercheurs sont emmenés dans la forêt.",
      28, 2, { C(EC_NONE, 0, 0) },
      "Opération de sauvetage en jungle", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_TERRE, -1), E(EE_FACCAP, F_HYDRA, -3), E(EE_RES, R_COHESION, 2) },
      "Négocier par des intermédiaires", { E(EE_RES, R_BUDGET, -3), E(EE_FACCAP, F_HYDRA, 3) }, NULL, "Les ravisseurs veulent les souches", 3 },
    { "Les ravisseurs veulent les souches", "Les ravisseurs ne veulent pas d'argent : ils exigent les souches microbiennes rares collectées par la mission, convoitées par des laboratoires clandestins.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Livrer les souches", { E(EE_RES, R_COHESION, 2), E(EE_FACCAP, F_HYDRA, 4), E(EE_RES, R_INFLUENCE, -3) },
      "Refuser et donner l'assaut", { E(EE_CAP, D_TERRE, -1), E(EE_RES, R_COHESION, -3), E(EE_FACCAP, F_HYDRA, -4) }, NULL, NULL, 0 },
    { "Rouille de synthèse sur les blés", "Une rouille du blé au génome visiblement retouché ravage la Champagne et la Picardie. Elle résiste à tous les fongicides homologués.",
      56, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 30) },
      "Brûler les parcelles infectées", { E(EE_RES, R_ALIM, -5), E(EE_RES, R_BUDGET, -4), E(EE_MIL, S_TRAVAILLEURS, 6) },
      "Importer des semences résistantes", { E(EE_RES, R_ALIM, -2), E(EE_RES, R_SOUVERAINETE, -3), E(EE_RES, R_BUDGET, -4) }, NULL, NULL, 0 },
    { "Tourbières du Congo incendiées", "Une milice armée par la Fédération incendie méthodiquement les tourbières du bassin du Congo, réserve mondiale de carbone. L'Union africaine appelle à l'aide.",
      80, 1, { C(EC_NONE, 0, 0) },
      "Envoyer des forces de protection", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_TERRE, -1), E(EE_MITIG, 0, 2), E(EE_FAC, F_BOREALE, 3) },
      "Financer les gardes forestiers locaux", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_INFLUENCE, 2), E(EE_MITIG, 0, -2) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : la ruée vers l'espace */
    { "Consortium lunaire public-privé", "Trois industriels et une agence européenne invitent la France dans le consortium Séléné-Mines, qui vise les gisements du pôle Sud lunaire.",
      48, 2, { C(EC_NONE, 0, 0) },
      "Entrer au capital", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_ESPACE, 1), E(EE_STRAT, SR_TERRES_RARES, 2) },
      "Rester entre agences publiques", { E(EE_RES, R_BUDGET, -3), E(EE_RES, R_SOUVERAINETE, 2), E(EE_PROSP, 0, -1) }, "Séléné-Mines réclame une escorte", NULL, 5 },
    { "Séléné-Mines réclame une escorte", "Un rival a placé un satellite inspecteur au-dessus du site lunaire. Les actionnaires de Séléné-Mines exigent une protection militaire en orbite.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Déployer un satellite gardien", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_ESPACE, 1), E(EE_AGGRO, 0, 2), E(EE_FAC, F_BOREALE, 3) },
      "Laisser le privé se défendre", { E(EE_RES, R_SOUVERAINETE, -3), E(EE_PROSP, 0, -1), E(EE_RES, R_BUDGET, 3) }, NULL, NULL, 0 },
    { "Laser aveuglant contre un satellite", "Un satellite d'observation français est aveuglé depuis le sol pendant quarante minutes. Aucun débris, aucun mort : est-ce déjà la guerre ?",
      36, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 25) },
      "Riposte symétrique en orbite", { E(EE_AGGRO, 0, 3), E(EE_FAC, F_BOREALE, 3), E(EE_CRED, 0, 3) },
      "Porter l'affaire devant l'ONU", { E(EE_RES, R_INFLUENCE, 3), E(EE_CRED, 0, -2) }, NULL, "Second satellite aveuglé", 3 },
    { "Second satellite aveuglé", "Encouragé par l'absence de riposte, l'agresseur aveugle cette fois un satellite de télécommunications militaires pendant toute une nuit.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Définir publiquement un seuil d'agression", { E(EE_CRED, 0, 4), E(EE_NATO, 0, 3), E(EE_FAC, F_BOREALE, 3) },
      "Durcir nos satellites", { E(EE_RES, R_BUDGET, -8), E(EE_CAP, D_ESPACE, 1), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
    { "Robots prospecteurs sous pavillon français", "Une start-up minière veut immatriculer en France ses robots d'astéroïdes, menacés d'être « réquisitionnés » en orbite par un concurrent.",
      72, 2, { C(EC_NONE, 0, 0) },
      "Accorder le pavillon", { E(EE_PROSP, 0, 2), E(EE_STRAT, SR_TERRES_RARES, 2), E(EE_FAC, F_BOREALE, 3) },
      "Refuser par prudence", { E(EE_RES, R_INFLUENCE, -2), E(EE_PROSP, 0, -1) }, NULL, NULL, 0 },
    { "Le Califat met des satellites en orbite", "Grâce à des lanceurs bon marché vendus sur catalogue, le Califat des Sables place trois nanosatellites d'imagerie au-dessus du Sahel.",
      86, 1, { C(EC_FAC_HOSTILE, F_SABLES, 20) },
      "Les neutraliser par cyberattaque", { E(EE_FACCAP, F_SABLES, -4), E(EE_AGGRO, 0, 2), E(EE_FAC, F_SABLES, 3) },
      "Contrôle mondial des ventes de lanceurs", { E(EE_RES, R_INFLUENCE, 2), E(EE_RES, R_BUDGET, -3), E(EE_FACCAP, F_SABLES, 2) }, NULL, NULL, 0 },
    /* inspiré de la Red Team Défense : implants de compétences et armée civile symbiotique */
    { "Compétences injectées par implant", "Un implant neuronal permet de télécharger en une nuit le pilotage d'un drone ou le déminage. L'état-major veut en équiper les réservistes.",
      54, 2, { C(EC_TECH_ABOVE, T_INF, 2) },
      "Équiper les réservistes", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_TERRE, 1), E(EE_RESEARCH, 0, 5), E(EE_CON, -1, 3) },
      "Interdire l'implant aux armées", { E(EE_RES, R_COHESION, 2), E(EE_RES, R_INFLUENCE, 2), E(EE_CRED, 0, -2) }, "Réservistes implantés piratés", NULL, 4 },
    { "Réservistes implantés piratés", "Une mise à jour frauduleuse efface le module de déminage de deux mille réservistes et le remplace par des messages de Nébula.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Rappeler tous les implants", { E(EE_CAP, D_TERRE, -1), E(EE_RES, R_BUDGET, -4), E(EE_RES, R_COHESION, -2) },
      "Mises à jour souveraines chiffrées", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_CYBER, 1), E(EE_FAC, F_NEBULA, 3) }, NULL, NULL, 0 },
    { "Une armée sans uniformes", "Chez l'adversaire, des millions de civils implantés se relaient en soldats d'un jour, coordonnés par un réseau nommé le Mycélium. Ni chef, ni caserne.",
      74, 2, { C(EC_FAC_HOSTILE, F_BOREALE, 30) },
      "Cartographier le réseau", { E(EE_RES, R_BUDGET, -4), E(EE_CAP, D_INFO, 1), E(EE_RESEARCH, 0, 6) },
      "Durcir nos frontières", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_TERRE, 1), E(EE_RELGROUP, 0, -3) }, "Le Mycélium se recompose", NULL, 4 },
    { "Le Mycélium se recompose", "Nos cyberattaques coupent trois noeuds du réseau ; en une heure, le Mycélium se reconfigure. Il recrute aussi des sympathisants en France.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Surveiller tous les implants", { E(EE_CAP, D_CYBER, 1), E(EE_RES, R_COHESION, -4), E(EE_CON, -1, 5), E(EE_RES, R_SOUVERAINETE, 2) },
      "Cibler seulement les recruteurs", { E(EE_RES, R_BUDGET, -4), E(EE_FACCAP, F_BOREALE, -3), E(EE_FAC, F_BOREALE, 3) }, "Défense populaire contre le Mycélium", NULL, 4 },
    { "Défense populaire contre le Mycélium", "Pour opposer un collectif à un collectif, l'état-major propose une garde territoriale de volontaires reliés par une application de défense.",
      0, 0, { C(EC_NONE, 0, 0) },
      "Créer la garde citoyenne", { E(EE_RES, R_BUDGET, -6), E(EE_CAP, D_TERRE, 1), E(EE_RES, R_COHESION, 3), E(EE_CON, -1, 3) },
      "Garder une armée professionnelle", { E(EE_CRED, 0, 2), E(EE_RES, R_COHESION, -2), E(EE_RES, R_BUDGET, -2) }, NULL, NULL, 0 },
    { "Les essaims de quartier", "Sur le modèle du Mycélium, des collectifs de quartier implantés s'organisent pour la sécurité, l'entraide et parfois la vengeance.",
      96, 1, { C(EC_DEMOCRACY, 0, 0) },
      "Les intégrer à la réserve", { E(EE_RES, R_COHESION, 2), E(EE_CAP, D_TERRE, 1), E(EE_RES, R_SOUVERAINETE, -2), E(EE_MIL, S_MOYENNES, 4) },
      "Les dissoudre", { E(EE_RES, R_COHESION, -3), E(EE_CON, -1, 4), E(EE_RES, R_BUDGET, -2) }, NULL, NULL, 0 },
    { "Fichage des porteurs d'implants", "Le ministre de l'Intérieur veut ficher chaque porteur d'implant comme combattant potentiel du Mycélium. Douze millions de Français sont concernés.",
      96, 2, { C(EC_AUTHORITARIAN, 0, 0) },
      "Ficher les porteurs d'implants", { E(EE_RES, R_SOUVERAINETE, 3), E(EE_RES, R_COHESION, -5), E(EE_MIL, S_MOYENNES, 8) },
      "Renoncer au fichage", { E(EE_FACCAP, F_BOREALE, 3), E(EE_RES, R_COHESION, 2), E(EE_CRED, 0, -2) }, NULL, NULL, 0 },
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
    case EC_SCENARIO:        return g->scenario == v;
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
    { "guerre_contre", EC_WAR_WITH, ARG_NATION, false }, { "tech_sup", EC_TECH_ABOVE, ARG_TECH, true }, { "scenario", EC_SCENARIO, ARG_NONE, true },
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
