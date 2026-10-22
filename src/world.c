/*
 * BOUCLIER 2050 - donnees statiques du monde
 */
#include "game.h"

#include <stddef.h>

const char *DOMAIN_NAMES[D_COUNT] = {
    "Terre", "Mer", "Air", "Espace", "Cyber", "Informationnel", "Electromagnétique"
};

const char *RESOURCE_NAMES[R_COUNT] = {
    "Budget", "Énergie", "Eau", "Alimentation", "Cohésion", "Influence", "Souveraineté"
};

const char *RESOURCE_DESC[R_COUNT] = {
    "Crédits de l'État (milliards). Sous -60 : défaut de paiement.",
    "Sécurité énergétique : production des provinces + projets + commerce, rapportée à la population.",
    "Sécurité hydrique. Le réchauffement assèche d'abord les provinces du Sud.",
    "Sécurité alimentaire. Une pénurie fait monter la militance des travailleurs.",
    "Cohésion nationale, tirée par la militance de la population. A 0 : effondrement.",
    "Influence internationale, dépensée pour la diplomatie.",
    "Souveraineté. A 0 : la France perd le contrôle de son destin.",
};

const char *STRATA_NAMES[S_COUNT] = { "Travailleurs", "Classes moyennes", "Élites" };
const float STRATA_SHARE[S_COUNT] = { 0.62f, 0.33f, 0.05f };

const TheaterDef THEATERS[TH_COUNT] = {
    { "Frontière Nord",        "Plaine ouverte vers le Benelux. Axe logistique majeur, cible des trafics et des sabotages.", D_TERRE },
    { "Frontière Est",         "Rhin et Lorraine. Zone sensible aux incursions et aux sabotages d'infrastructures.", D_TERRE },
    { "Alpes",                 "Glaciers en recul, nappes convoitées. Passages de montagne difficiles à surveiller.", D_TERRE },
    { "Pyrénées",              "Frontière sud, sous pression migratoire et hydrique avec la désertification ibérique.", D_TERRE },
    { "Manche & Atlantique",   "Câbles sous-marins, éolien offshore, traversées clandestines.", D_MER },
    { "Méditerranée",          "Mer surchauffée, routes migratoires instrumentalisées, harcèlement naval.", D_MER },
    { "Outre-mer & ZEE",       "Deuxième ZEE mondiale : pêche, minerais, territoires exposés aux cyclones.", D_MER },
    { "Espace aérien",         "Police du ciel, défense anti-drones, protection des sites sensibles.", D_AIR },
    { "Orbite",                "Satellites de renseignement et de navigation. Améliore la détection partout.", D_ESPACE },
    { "Cyberespace",           "Réseaux électriques, hôpitaux, industrie : la ligne de front invisible.", D_CYBER },
    { "Champ informationnel",  "Désinformation, deepfakes, ingérences : la bataille des perceptions.", D_INFO },
    { "Spectre EM",            "Brouillage GNSS et guerre électronique. Un spectre perdu affaiblit l'air et la mer.", D_EM },
    { "Territoires extérieurs", "Provinces conquises ou occupées : guérilla, attentats et sabotages. Les divisions stationnées sur place aident à les neutraliser.", D_TERRE },
};

/*                                                      budget energ  eau   alim  cohes infl  souv */
const ThreatDef THREATS[] = {
    { "Trafic transfrontalier",            D_TERRE,  false, { -0.4f, 0,     0,     0,    -0.2f, 0,    -0.3f }, 2, 5, 3 },
    { "Flux migratoire instrumentalisé",   D_TERRE,  true,  {  0,    0,     0,    -0.2f, -0.5f, 0,    -0.2f }, 3, 6, 3 },
    { "Sabotage d'infrastructures",        D_TERRE,  false, { -0.3f,-0.6f,  0,     0,     0,    0,    -0.2f }, 3, 6, 2 },
    { "Milice armée transfrontalière",     D_TERRE,  false, {  0,    0,     0,     0,    -0.3f, 0,    -0.7f }, 4, 7, 2 },
    { "Pillage de la ZEE",                 D_MER,    false, { -0.2f, 0,     0,    -0.6f,  0,    0,    -0.3f }, 2, 5, 3 },
    { "Sabotage de câbles sous-marins",    D_MER,    false, { -0.6f,-0.3f,  0,     0,     0,    0,    -0.2f }, 3, 6, 2 },
    { "Traversées instrumentalisées",      D_MER,    true,  {  0,    0,     0,    -0.2f, -0.5f,-0.2f,  0    }, 3, 6, 3 },
    { "Harcèlement naval",                 D_MER,    false, {  0,   -0.4f,  0,     0,     0,    0,    -0.6f }, 4, 7, 2 },
    { "Essaim de drones",                  D_AIR,    false, {  0,   -0.2f,  0,     0,    -0.4f, 0,    -0.5f }, 3, 7, 2 },
    { "Violation de l'espace aérien",      D_AIR,    false, {  0,    0,     0,     0,     0,   -0.2f, -0.6f }, 2, 5, 2 },
    { "Satellite fureteur / ASAT",         D_ESPACE, false, { -0.3f, 0,     0,     0,     0,    0,    -0.6f }, 3, 7, 3 },
    { "Rançongiciel sur les hôpitaux",     D_CYBER,  false, { -0.3f, 0,     0,     0,    -0.6f, 0,     0    }, 3, 6, 2 },
    { "Attaque du réseau électrique",      D_CYBER,  false, {  0,   -0.8f,  0,     0,    -0.2f, 0,     0    }, 3, 7, 2 },
    { "Espionnage industriel",             D_CYBER,  false, { -0.6f, 0,     0,     0,     0,    0,    -0.2f }, 2, 5, 3 },
    { "Campagne de désinformation",        D_INFO,   false, {  0,    0,     0,     0,    -0.6f,-0.2f,  0    }, 3, 6, 3 },
    { "Deepfakes politiques",              D_INFO,   false, {  0,    0,     0,     0,    -0.5f,-0.4f,  0    }, 3, 6, 2 },
    { "Brouillage GNSS",                   D_EM,     false, { -0.4f, 0,     0,     0,     0,    0,    -0.3f }, 3, 6, 2 },
    { "Accaparement des nappes",           D_TERRE,  false, {  0,    0,    -0.7f,  0,     0,    0,    -0.2f }, 3, 6, 3 },
    { "Guérilla",                          D_TERRE,  false, { -0.2f, 0,     0,     0,    -0.1f, 0,    -0.2f }, 3, 7, 3 },
    { "Attentat contre l'occupant",        D_TERRE,  false, {  0,    0,     0,     0,    -0.3f,-0.1f, -0.1f }, 3, 6, 2 },
    { "Sabotage logistique",               D_TERRE,  false, { -0.4f,-0.2f,  0,     0,     0,    0,     0    }, 2, 5, 2 },
};
const int THREAT_TYPE_COUNT = (int)(sizeof(THREATS) / sizeof(THREATS[0]));

const FactionDef FACTIONS[F_COUNT] = {
    { "Fédération Boréale",  "Puissance continentale de l'Est. Son appareil de guerre hybride (drones, ASAT, brouillage) suit la force de son armée." },
    { "Ligue des Sables",    "Réseau de juntes sahéliennes en effondrement climatique. Instrumentalise les flux de réfugiés vers l'Europe." },
    { "Consortium Hydra",    "Cartel privé de l'eau et des ressources. Corrompt, pille et achète ce qu'il ne peut voler." },
    { "Réseau Nébula",       "Collectif de hackers mercenaires et de fermes à trolls, loué au plus offrant." },
};

const ProjectDef PROJECTS[P_COUNT] = {
    { "Programme SMR",            "Petits réacteurs modulaires : +12 unités d'énergie par tour.",                 24, 8 },
    { "Éolien & solaire",         "Parcs renouvelables : +7 unités d'énergie par tour.",                          12, 4 },
    { "Usines de dessalement",    "Eau douce sur les littoraux : +9 unités d'eau par tour.",                      14, 5 },
    { "Agro-résilience",          "Cultures adaptées, irrigation de précision : +8 unités d'alimentation.",        10, 4 },
    { "Bouclier cyber civil",     "Durcit hôpitaux et réseaux : effets des cyberattaques -35%.",                   9, 3 },
    { "Éducation aux médias",     "Résilience de l'opinion : effets informationnels -35%, militance -0,03/tour.",  7, 5 },
    { "Constellation souveraine", "Satellites nationaux : +15% détection, défense orbitale, +5% puissance militaire.", 16, 5 },
    { "Plan Adaptation Climat",   "Digues, forêts, réseaux de froid : catastrophes -45%, aridité des provinces -20%.", 15, 6 },
};

const ActionDef ACTIONS[A_COUNT] = {
    { "Diplomatie",      "Négociations secrètes : réduit l'hostilité (succès selon l'influence).", 1, 0, 6, D_COUNT, 0 },
    { "Sanctions",       "Sanctions économiques : réduit la capacité, augmente l'hostilité.",       1, 3, 0, D_COUNT, 0 },
    { "Cyber-offensive", "Frappe cyber sur leurs moyens : forte baisse de capacité. Risque d'attribution. Requiert capacité Cyber 5.", 1, 0, 0, D_CYBER, 5 },
    { "Contre-influence","Opération d'information : baisse hostilité et capacité, +1 Cohésion. Requiert capacité Info 3.", 1, 0, 0, D_INFO, 3 },
    { "Renseignement",   "DGSE/DRM sur cette faction pendant 3 tours : détection et neutralisation accrues.", 1, 2, 0, D_COUNT, 0 },
};

const RegimeDef REGIMES[RG_COUNT] = {
    { "Commune écologique",
      "Démocratie radicale et sobriété partagée. Population apaisée, intégration rapide, mais faible mobilisation et horreur de la guerre.",
      0.85f, 0.06f, 0.00f, 1.2f, 25, 1.0f, 1.6f, 0.03f, 0.80f, 0.90f, { 0.15f, 0.10f, -0.15f }, "Assemblées citoyennes : pas d'élections partisanes, les Écologistes gouvernent. Impôts plafonnés à 35%." },
    { "Démocratie libérale",
      "Le régime de 2030. Équilibré, apprécié des partenaires européens, prudent face à la guerre.",
      1.00f, 0.10f, 0.05f, 0.6f, 12, 0.6f, 1.0f, 0.00f, 1.00f, 1.00f, { 0.00f, 0.05f, 0.05f }, "Élections tous les 4 ans : le courant majoritaire forme le gouvernement et applique son programme." },
    { "Technocratie verte",
      "Gouvernement d'experts et planification climatique. Projets moins chers, recettes accrues, débat public réduit.",
      1.10f, 0.10f, 0.15f, 0.5f, 10, 0.3f, 0.8f, 0.00f, 0.75f, 1.05f, { -0.05f, 0.05f, 0.00f }, "Conseil des experts : pas d'élections, recettes et projets optimisés, la conscience politique monte plus vite." },
    { "Régime autoritaire",
      "Exécutif fort, libertés restreintes. Mobilisation et ordre accrus, isolement diplomatique.",
      1.05f, 0.18f, 0.35f, 0.3f, 4, -0.4f, 0.7f, 0.00f, 1.00f, 1.10f, { -0.05f, -0.10f, 0.10f }, "Élections suspendues. Les libertés sont restreintes : la répression masque le mécontentement sans l'effacer." },
    { "Junte militaire",
      "L'armée gouverne. Puissance militaire maximale, économie et diplomatie sacrifiées.",
      0.90f, 0.28f, 0.50f, 0.2f, 1, -1.0f, 0.5f, -0.02f, 1.10f, 1.25f, { -0.10f, -0.15f, 0.05f }, "L'armée gouverne : recrutement 25% moins cher, organisation des troupes restaurée plus vite." },
    { "État totalitaire",
      "Contrôle total de la société, terreur et propagande. Mobilisation de masse, répression écrasante, paria international.",
      1.15f, 0.40f, 0.80f, 0.1f, 0, -1.6f, 1.3f, -0.05f, 0.90f, 1.30f, { -0.20f, -0.25f, -0.05f }, "Propagande et police politique : cohésion forcée, mais la conscience des populations accumule une pression explosive." },
};

const NationDef NATIONS[N_COUNT] = {
    { "France",              {  70, 120, 220 }, RG_DEMOCRATIE,   true,  100, 0.00f, true, { C_FRANCAISE, C_CORSE, 255 }, { NULL, NULL } },
    { "Royaume-Uni",         { 190,  60,  75 }, RG_DEMOCRATIE,   false,  40, 0.10f, true, { C_BRITANNIQUE, C_ECOSSAISE, 255 }, { "Gén. Hartley", "Gén. Whitmore" } },
    { "Irlande",             {  70, 170,  90 }, RG_DEMOCRATIE,   true,   45, 0.00f, false, { C_IRLANDAISE, 255, 255 }, { "Gén. Byrne", "Gén. Doyle" } },
    { "Benelux",             { 230, 150,  60 }, RG_DEMOCRATIE,   true,   55, 0.00f, false, { C_NEERLANDAISE, C_FRANCAISE, 255 }, { "Gén. Janssens", "Gén. de Vries" } },
    { "Allemagne",           { 125, 125, 125 }, RG_DEMOCRATIE,   true,   50, 0.10f, false, { C_ALLEMANDE, 255, 255 }, { "Gén. Brandt", "Gén. Kessler" } },
    { "Suisse",              { 215,  90,  90 }, RG_DEMOCRATIE,   false,  40, 0.00f, false, { C_ALLEMANDE, C_FRANCAISE, C_ITALIENNE }, { "Gén. Favre", "Gén. Steiner" } },
    { "Autriche",            { 225, 195, 195 }, RG_DEMOCRATIE,   true,   40, 0.05f, false, { C_ALLEMANDE, 255, 255 }, { "Gén. Huber", "Gén. Gruber" } },
    { "Italie",              {  80, 165,  80 }, RG_DEMOCRATIE,   true,   40, 0.10f, false, { C_ITALIENNE, 255, 255 }, { "Gén. Ferrari", "Gén. Lombardi" } },
    { "Espagne",             { 225, 195,  60 }, RG_DEMOCRATIE,   true,   45, 0.10f, false, { C_ESPAGNOLE, 255, 255 }, { "Gén. Navarro", "Gén. Castillo" } },
    { "Portugal",            {  50, 125,  60 }, RG_DEMOCRATIE,   true,   45, 0.00f, false, { C_PORTUGAISE, 255, 255 }, { "Gén. Almeida", "Gén. Sousa" } },
    { "Danemark",            { 185,  95, 110 }, RG_DEMOCRATIE,   true,   45, 0.00f, false, { C_SCANDINAVE, 255, 255 }, { "Gén. Larsen", "Gén. Holm" } },
    { "Scandinavie",         { 100, 150, 205 }, RG_TECHNOCRATIE, true,   40, 0.05f, false, { C_SCANDINAVE, C_FINNOISE, 255 }, { "Gén. Lindqvist", "Gén. Virtanen" } },
    { "Pays baltes",         { 150, 120, 200 }, RG_DEMOCRATIE,   true,   40, 0.05f, false, { C_BALTE, 255, 255 }, { "Gén. Kalnins", "Gén. Tamm" } },
    { "Pologne",             { 220, 120, 145 }, RG_DEMOCRATIE,   true,   30, 0.15f, false, { C_POLONAISE, 255, 255 }, { "Gén. Kowalski", "Gén. Nowak" } },
    { "Europe centrale",     { 165,  95, 135 }, RG_AUTORITAIRE,  true,   15, 0.15f, false, { C_TCHEQUE, C_SLOVAQUE, C_HONGROISE }, { "Gén. Novák", "Gén. Szabó" } },
    { "Balkans",             { 150, 150,  90 }, RG_AUTORITAIRE,  false,  10, 0.20f, false, { C_SLAVE_SUD, C_ALBANAISE, 255 }, { "Gén. Petrović", "Gén. Hoxha" } },
    { "Roumanie",            { 200, 175,  95 }, RG_DEMOCRATIE,   true,   30, 0.10f, false, { C_ROUMAINE, 255, 255 }, { "Gén. Popescu", "Gén. Ionescu" } },
    { "Bulgarie",            {  90, 160, 140 }, RG_DEMOCRATIE,   true,   25, 0.10f, false, { C_BULGARE, 255, 255 }, { "Gén. Dimitrov", "Gén. Ivanov" } },
    { "Grèce",               { 110, 180, 230 }, RG_DEMOCRATIE,   true,   40, 0.10f, false, { C_GRECQUE, 255, 255 }, { "Gén. Papadakis", "Gén. Nikolaou" } },
    { "Turquie",             { 195,  95,  60 }, RG_AUTORITAIRE,  false,   0, 0.35f, false, { C_TURQUE, 255, 255 }, { "Gén. Yılmaz", "Gén. Demir" } },
    { "Ukraine",             { 235, 210,  90 }, RG_DEMOCRATIE,   false,  35, 0.20f, false, { C_UKRAINIENNE, 255, 255 }, { "Gén. Kovalenko", "Gén. Bondarenko" } },
    { "Fédération Boréale",  { 115,  55,  60 }, RG_TOTALITAIRE,  false, -60, 0.60f, true, { C_RUSSE, C_BIELORUSSE, 255 }, { "Gén. Volkov", "Gén. Sokolov" } },
    { "Maroc",               { 175,  75,  60 }, RG_AUTORITAIRE,  false,  10, 0.15f, false, { C_ARABE_MAGHREB, C_AMAZIGHE, 255 }, { "Gén. Alaoui", "Gén. Bennani" } },
    { "Algérie",             {  60, 135,  95 }, RG_AUTORITAIRE,  false, -15, 0.20f, false, { C_ARABE_MAGHREB, C_AMAZIGHE, 255 }, { "Gén. Benali", "Gén. Mansouri" } },
    { "Tunisie",             { 200, 115, 115 }, RG_AUTORITAIRE,  false,  10, 0.10f, false, { C_ARABE_MAGHREB, 255, 255 }, { "Gén. Trabelsi", "Gén. Ben Salah" } },
    { "Libye",               { 120, 105,  75 }, RG_JUNTE,        false, -10, 0.30f, false, { C_ARABE_MAGHREB, 255, 255 }, { "Gén. Al-Mukhtar", "Gén. Haftari" } },
    { "Égypte",              { 210, 180, 125 }, RG_JUNTE,        false,   0, 0.30f, false, { C_ARABE_EGYPTE, 255, 255 }, { "Gén. Farouk", "Gén. Mostafa" } },
};


const DilemmaDef DILEMMAS[] = {
    { "Offre du Consortium Hydra",
      "Hydra propose de racheter la gestion de l'eau du Sud-Est. L'offre est généreuse, mais l'eau deviendrait un levier entre ses mains.",
      "Accepter la vente", "+15 Budget, +5 Eau, -8 Souveraineté, Hydra -10 hostilité",
      "Refuser", "Hydra +10 hostilité" },
    { "Exode climatique sahélien",
      "Des centaines de milliers de déplacés convergent vers les côtes méditerranéennes après l'effondrement des récoltes du Sahel.",
      "Corridor humanitaire européen", "+8 Influence, -6 Budget, +0,3 M d'habitants à intégrer",
      "Sous-traiter le contrôle aux juntes", "-4 Budget, -6 Influence, Ligue -10 hostilité mais +10 capacité" },
    { "Blackout en pleine canicule",
      "Le réseau électrique cède sous la demande de climatisation. Des millions de foyers sont dans le noir.",
      "Rationnement national", "+6 Énergie, militance des classes moyennes en hausse",
      "Importer à prix d'or", "-10 Budget, +2 Énergie" },
    { "Le lanceur d'alerte",
      "Un fonctionnaire révèle un programme de surveillance de masse. Nébula amplifie le scandale.",
      "Transparence et commission d'enquête", "+4 Cohésion, -3 Souveraineté",
      "Faire taire les fuites", "-6 Cohésion, Nébula +10 hostilité" },
    { "Alliance spatiale européenne",
      "Les partenaires européens proposent de mutualiser surveillance orbitale et lanceurs.",
      "Rejoindre l'alliance", "-8 Budget, +2 capacité Espace, +5 Influence",
      "Préserver l'autonomie", "+3 Souveraineté" },
    { "Grève générale contre la vie chère",
      "L'inflation alimentaire déclenche une grève générale. Le pays est paralysé.",
      "Concessions sociales", "-8 Budget, militance des travailleurs -2",
      "Briser la grève", "Militance des travailleurs +2, +2 Budget" },
    { "Coup d'État au Sahel",
      "Une junte alliée à la Ligue des Sables prend le pouvoir et menace les ressortissants français.",
      "Intervention ciblée", "-10 Budget, Ligue -12 capacité, +15 hostilité, -3 Influence",
      "Non-ingérence", "+2 Influence, Ligue +8 capacité" },
    { "Accord gazier boréal",
      "La Fédération Boréale propose un contrat gazier à bas prix sur dix ans. Bruxelles désapprouve.",
      "Signer l'accord", "+6 unités d'énergie/tour, Boréale +30 relation, -5 Souveraineté, -5 Influence",
      "Refuser", "+2 Souveraineté" },
    { "IA de défense autonome",
      "Les armées proposent de confier la coordination tactique à une IA. Efficace, mais controversée.",
      "Déployer l'IA", "Défense +12% partout, -4 Cohésion, -3 Influence",
      "Refuser par principe", "+2 Cohésion" },
    { "Cyclone sur les Antilles",
      "Un cyclone d'une violence inédite ravage les Antilles. Les sinistrés attendent la réponse de la métropole.",
      "Plan de reconstruction", "-12 Budget, +5 Cohésion, +2 Influence",
      "Aide minimale", "-6 Cohésion, -3 Influence" },
    { "Cyber-mercenaires à vendre",
      "D'anciens membres de Nébula proposent leurs services et leurs outils offensifs.",
      "Les recruter", "-6 Budget, +3 capacité Cyber, -4 Influence",
      "Les livrer à la justice", "Nébula +8 hostilité, -6 capacité" },
    { "Sommet climat de Paris",
      "La France accueille un sommet de la dernière chance sur le climat.",
      "Engagement ambitieux", "-10 Budget, +10 Influence, réchauffement et catastrophes ralentis",
      "Statu quo", "+1 Budget, -4 Influence" },
    { "La tentation du sabre",
      "Un quarteron de généraux, excédé par l'impuissance politique, propose de « remettre de l'ordre ».",
      "Laisser l'armée prendre le pouvoir", "Régime : Junte militaire. -8 Cohésion, quitte l'UE",
      "Faire arrêter les conjurés", "+3 Cohésion, -1 division (purges)" },
    { "Le mouvement des communes",
      "Des assemblées citoyennes occupent les mairies et réclament une démocratie radicale et sobre.",
      "Adopter leur programme", "Régime : Commune écologique. Les élites s'insurgent",
      "Dissoudre les assemblées", "Militance des classes moyennes +2, -4 Cohésion" },
    { "Nationaliser l'eau",
      "Les sécheresses font flamber le prix de l'eau. La gauche comme l'extrême droite réclament sa nationalisation.",
      "Nationaliser", "-10 Budget, +6 Souveraineté, +4 Eau, Hydra +15 hostilité",
      "Laisser faire le marché", "Militance des travailleurs +1" },
    { "Exode vers le Nord",
      "Une canicule à 52°C au Maghreb jette des millions de personnes sur les routes. Des navires attendent au large.",
      "Ouvrir les ports", "+1 M d'habitants à intégrer, +8 Influence, -4 Cohésion",
      "Bloquer les navires", "-8 Influence, Ligue +10 hostilité, flux instrumentalisés" },
};
const int DILEMMA_COUNT = (int)(sizeof(DILEMMAS) / sizeof(DILEMMAS[0]));

const DilemmaDef DYN_DILEMMAS[6] = {
    { "Article 42.7 : un allié est attaqué",
      "Un État membre de l'Union européenne est envahi et invoque la clause d'assistance mutuelle. Les capitales regardent Paris.",
      "Entrer en guerre à ses côtés", "Guerre contre l'agresseur, +5 Influence, relation de l'allié +30",
      "Soutien matériel uniquement", "-6 Budget, -4 Influence" },
    { "Offre de paix",
      "L'ennemi propose de cesser les hostilités. Les conditions du traité, fixées selon le score de guerre, sont détaillées ci-dessus.",
      "Signer le traité", "Cessions et réparations appliquées, trêve de 4 ans",
      "Poursuivre la guerre", "La guerre continue" },
    { "Les autoritaires gagnent les élections",
      "Portés par la peur et les pénuries, les partisans d'un pouvoir fort remportent le scrutin et exigent les pleins pouvoirs.",
      "Leur confier le pouvoir", "Régime : autoritaire, sans coût de transition",
      "Annuler le scrutin", "-10 Cohésion, conscience politique en hausse" },
    { "Chantage nucléaire",
      "La Fédération Boréale menace d'employer l'arme nucléaire si nous ne cessons pas les hostilités. Bluffe-t-elle ?",
      "Céder et signer la paix", "Paix immédiate, -8 Cohésion, -10 crédibilité",
      "Tenir bon", "Risque de frappe si notre dissuasion est peu crédible" },
    { "Crise de la dette",
      "Les marchés refusent de nous prêter. L'Union européenne et le FMI proposent un plan de sauvetage, en échange d'une austérité sévère.",
      "Accepter le plan de sauvetage", "Dette -30%, dépenses publiques bridées 12 tours, -15 Souveraineté, -8 Cohésion",
      "Faire défaut", "La France cesse de payer : défaite" },
    { "Conseil de sécurité : un allié mis en cause",
      "Une résolution de l'ONU condamne un de nos alliés et prévoit des sanctions internationales. La France peut opposer son veto.",
      "Opposer notre veto", "Résolution bloquée, -6 Influence, relation de l'allié +15",
      "Laisser voter", "Sanctions contre notre allié, relation -20" },
};

const char *MIGRATION_DOCTRINES[3] = { "Accueil organisé", "Contrôle équilibré", "Fermeture stricte" };
const char *MIGRATION_DESC[3] = {
    "90% des migrants admis et répartis dans les provinces : main d'oeuvre et croissance, mais population à intégrer. +1 Influence/tour, relations du Sud en hausse.",
    "50% des migrants admis. Les refoulés alimentent les flux clandestins aux frontières.",
    "15% admis. Neutralisation des flux facilitée (x1,4), surcoût 1,5/tour, -1,5 Influence/tour, relations du Sud en baisse.",
};
const float MIGRATION_ACCEPT[3] = { 0.9f, 0.5f, 0.15f };

const char *VET_RANKS[4] = { "Recrues", "Aguerris", "Vétérans", "Élite" };

const char *SCENARIO_NAMES[2] = { "Janvier 2030", "La crise de 2040" };
const char *SCENARIO_DESC[2] = {
    "+1,5°C. Le monde bascule, mais la France a encore des marges : vingt ans pour se préparer.",
    "+2°C, la mer a monté de 45 cm. La Fédération Boréale envahit les pays baltes, la Libye s'est effondrée, "
    "l'Algérie se dote de la bombe. Trésor vide, dette record, pays fracturé : vingt ans à tenir.",
};

const char *ALERT_LEVELS[3] = { "Vigilance", "Alerte renforcée", "Urgence attentat" };
const char *ALERT_DESC[3] = {
    "Posture normale. Aucun surcoût.",
    "Défense +15% partout. -3 Budget/tour, -0,4 Cohésion/tour (lassitude).",
    "Défense +30% partout. -7 Budget/tour, -1,2 Cohésion/tour.",
};

/* ------------------------------------------------------------------ */
/* Population, politique, economie, armees                             */
/* ------------------------------------------------------------------ */

const char *IDEOLOGY_NAMES[I_COUNT] = {
    "Écologistes", "Socialistes", "Libéraux", "Conservateurs", "Souverainistes", "Autoritaires"
};

const char *IDEOLOGY_DESC[I_COUNT] = {
    "Sobriété et adaptation. Au pouvoir : projets -15%, réchauffement ralenti, élites mécontentes.",
    "Justice sociale. Au pouvoir : travailleurs apaisés, recettes -5%.",
    "Marché et libertés. Au pouvoir : prospérité accrue, élites satisfaites, travailleurs moins.",
    "Ordre et traditions. Au pouvoir : militance en baisse, intégration ralentie.",
    "Frontières et indépendance. Au pouvoir : souveraineté en hausse, influence en baisse.",
    "Pouvoir fort. Vainqueurs d'une élection, ils exigent un changement de régime.",
};

/* adhesion de chaque courant a chaque regime */
const float IDEOLOGY_AFFINITY[RG_COUNT][I_COUNT] = {
    /*            Eco   Soc   Lib   Cons  Souv  Aut */
    /* Utopie */ { 1.0f, 0.7f, 0.2f, 0.0f, 0.0f, 0.0f },
    /* Democ. */ { 0.6f, 0.8f, 1.0f, 0.8f, 0.4f, 0.0f },
    /* Techno */ { 0.9f, 0.4f, 0.7f, 0.4f, 0.2f, 0.1f },
    /* Autor. */ { 0.1f, 0.2f, 0.2f, 0.7f, 1.0f, 0.8f },
    /* Junte  */ { 0.0f, 0.1f, 0.1f, 0.5f, 0.7f, 1.0f },
    /* Totali */ { 0.0f, 0.1f, 0.0f, 0.3f, 0.6f, 1.0f },
};

const char *GOOD_NAMES[G_COUNT] = { "Alimentation", "Eau", "Énergie", "Minerais", "Biens industriels" };
const float GOOD_BASE_PRICE[G_COUNT] = { 0.08f, 0.0f, 0.10f, 0.06f, 0.15f };

const char *SPENDING_NAMES[SP_COUNT] = { "Éducation", "Administration", "Protection sociale" };
const char *SPENDING_DESC[SP_COUNT] = {
    "Fait monter l'alphabétisation : prospérité et ascension sociale, mais aussi conscience politique.",
    "Efficacité fiscale et maintien de l'ordre : recettes accrues, révoltes plus vite réprimées.",
    "Pensions, chômage, logement : baisse la militance des travailleurs et des classes moyennes.",
};

const char *DIFFICULTY_NAMES[DIFF_COUNT] = { "Facile", "Normale", "Difficile" };

const char *BUDGET_LINE_NAMES[BL_COUNT] = {
    "Impôts et réquisitions", "Commerce extérieur", "Droits de douane", "Fonds européens", "Réparations, réquisitions",
    "Défense M2MC", "Armée", "Dépenses publiques", "Recherche militaire", "Dissuasion", "Importations", "Réparations versées",
    "Intérêts de la dette", "Adaptation côtière",
};

const char *SCORE_PART_NAMES[SC_COUNT] = {
    "Survie", "Population", "Prospérité", "Territoire", "Stabilité",
    "Résilience", "Puissance", "Réputation", "Pénalités",
};

const char *VICTORY_NAMES[VIC_COUNT] = {
    "Aucune", "Survie", "Transition réussie", "Hégémonie", "Utopie réalisée", "Ordre nouveau"
};
const char *VICTORY_DESC[VIC_COUNT] = {
    "",
    "La France a traversé la tempête.",
    "Tous les projets de résilience sont achevés et les ressources sont assurées. Bonus +25%.",
    "La France contrôle plus du quart de la population de la carte. Bonus +25%.",
    "Une commune écologique stable et prospère en 2060. Bonus +30%.",
    "Un régime de fer règne sur un vaste territoire pacifié. Bonus +15%.",
};

const char *NUKE_POSTURES[3] = { "Minimale", "Crédible", "Renforcée" };

/*  nom, abrege, description, milieu, attaque, defense, antiaerien, naval, cout, entretien, biens, formation,
    vitesse, portee, technologie, niveau requis, poids dans le plafond */
const UnitDef UNIT_TYPES[U_TYPE_COUNT] = {
    { "Infanterie", "INF", "Le socle de l'armée : bon marché, solide en défense.",
      DOM_LAND, 2.0f, 4.0f, 0.5f, 0, 3, 0.25f, 0.4f, 2, 1, 0, T_INF, 0, 1.0f },
    { "Mécanisée", "MEC", "Infanterie sur véhicules blindés : polyvalente, avance de 2 provinces.",
      DOM_LAND, 4.0f, 4.5f, 1.0f, 0, 5, 0.40f, 0.8f, 3, 2, 0, T_BLINDES, 0, 1.0f },
    { "Blindée (MGCS)", "BLI", "Chars de nouvelle génération : percée, 2 provinces par tour, gênés par la montagne.",
      DOM_LAND, 7.0f, 3.5f, 0.5f, 0, 8, 0.60f, 1.2f, 4, 2, 0, T_BLINDES, 0, 1.0f },
    { "Infanterie de marine", "MAR", "Débarque sans pénalité par les liaisons maritimes.",
      DOM_LAND, 3.5f, 3.5f, 0.5f, 0, 5, 0.40f, 0.6f, 3, 1, 0, T_MARINE, 0, 1.0f },
    { "Troupes de montagne", "MNT", "Chasseurs alpins : +30% en montagne, aucune pénalité de terrain.",
      DOM_LAND, 3.0f, 4.5f, 0.5f, 0, 4, 0.30f, 0.5f, 3, 1, 0, T_INF, 1, 1.0f },
    { "Feux longue portée", "ART", "Lance-roquettes et canons : appuient les batailles des provinces voisines.",
      DOM_LAND, 3.0f, 2.0f, 0.0f, 0, 5, 0.35f, 0.8f, 3, 1, 1, T_FEUX, 0, 1.0f },
    { "Essaims de drones", "DRN", "Drones kamikazes et de reconnaissance : frappes bon marché, vulnérables à la DCA et au brouillage.",
      DOM_LAND, 5.0f, 1.5f, 0.0f, 0, 3, 0.20f, 0.9f, 2, 2, 0, T_AIR, 1, 0.5f },
    { "Robots de combat", "ROB", "Plateformes terrestres autonomes : très résistantes, sans soldats exposés.",
      DOM_LAND, 5.0f, 6.0f, 1.0f, 0, 9, 0.50f, 1.5f, 4, 2, 0, T_INF, 2, 0.25f },
    { "Forces spéciales", "FS", "Raids et sabotages : désorganisent les défenseurs avant l'assaut.",
      DOM_LAND, 3.0f, 2.0f, 0.0f, 0, 4, 0.30f, 0.3f, 3, 1, 0, T_INF, 1, 0.5f },
    { "Défense sol-air", "DCA", "Batteries SAMP/T : abattent avions et drones ennemis sur place et alentour.",
      DOM_LAND, 0.5f, 2.0f, 6.0f, 0, 5, 0.35f, 0.7f, 3, 1, 0, T_GE, 1, 0.5f },
    { "Missiles hypersoniques", "HYP", "Frappes dans la profondeur jusqu'à 2 provinces : brisent l'organisation adverse.",
      DOM_LAND, 6.0f, 0.5f, 0.0f, 0, 10, 0.60f, 1.0f, 4, 1, 2, T_FEUX, 2, 0.5f },
    { "Chasseurs (SCAF)", "CHA", "Supériorité aérienne et appui des batailles à 2 provinces.",
      DOM_AIR, 5.0f, 3.0f, 3.0f, 0, 10, 0.60f, 1.5f, 4, 4, 2, T_AIR, 0, 0.5f },
    { "Drones MALE", "MAL", "Surveillance et frappes de précision à 2 provinces.",
      DOM_AIR, 3.0f, 1.0f, 0.0f, 0, 4, 0.30f, 0.6f, 2, 4, 2, T_AIR, 1, 0.25f },
    { "Frégates (FDI)", "FRE", "Escorte, défense aérienne et blocus des côtes ennemies.",
      DOM_SEA, 0.0f, 0.0f, 3.0f, 4, 8, 0.40f, 1.0f, 4, 4, 0, T_MARINE, 0, 0.5f },
    { "Sous-marins (SNA)", "SNA", "Chasseurs de flottes et de convois : redoutables au blocus.",
      DOM_SEA, 0.0f, 0.0f, 0.0f, 7, 12, 0.50f, 1.2f, 5, 4, 0, T_MARINE, 1, 0.5f },
    { "Porte-avions", "PA", "Base aérienne flottante : appui aérien sur toutes les côtes à portée.",
      DOM_SEA, 6.0f, 0.0f, 4.0f, 3, 25, 1.50f, 2.5f, 8, 4, 0, T_MARINE, 2, 1.0f },
    { "Drones navals", "USV", "Embarcations kamikazes : bon marché, harcèlent les flottes et les ports.",
      DOM_SEA, 0.0f, 0.0f, 0.0f, 3, 3, 0.20f, 0.6f, 2, 4, 0, T_MARINE, 1, 0.25f },
};

const TechDef TECHS[T_COUNT] = {
    { "Infanterie & robotique", "Infanterie, montagne, forces spéciales, robots (niveau 2). +6% par niveau." },
    { "Blindés",                "Mécanisées et chars. +6% par niveau." },
    { "Aviation & drones",      "Chasseurs, drones MALE et essaims (niveau 1). +6% par niveau." },
    { "Marine",                 "Frégates, SNA et drones navals (niveau 1), porte-avions (niveau 2). +6% par niveau." },
    { "Feux & missiles",        "Artillerie longue portée, missiles hypersoniques (niveau 2). +6% par niveau." },
    { "Guerre électronique",    "DCA (niveau 1), brouillage des drones et des avions ennemis : -5% d'efficacité adverse par niveau." },
    { "Logistique",             "Récupération de l'organisation +3% par niveau, attrition réduite." },
    { "Nucléaire",              "Crédibilité +4 par niveau, armes tactiques au niveau 3." },
};

const char *GENERAL_TRAITS[GT_COUNT] = { "Offensif", "Défensif", "Logisticien", "Arme blindée", "Montagnard" };

const char *FRENCH_GENERALS[20] = {
    "Gén. Lefèvre", "Gén. Morel", "Gén. Garnier", "Gén. Fontaine", "Gén. Rousseau", "Gén. Vincent",
    "Gén. Mercier", "Gén. Blanchard", "Gén. Guérin", "Gén. Boyer", "Gén. Lambert", "Gén. Faure",
    "Gén. Chevalier", "Gén. Moreau", "Gén. Girard", "Gén. Bonnet", "Gén. Dupuis", "Gén. Leroy",
    "Gén. Marchand", "Gén. Perrin",
};

const char *RELIGION_NAMES[REL_COUNT] = { "Catholiques", "Protestants", "Orthodoxes", "Musulmans", "Sans religion" };

const char *CULTURE_POLICY_NAMES[CP_COUNT] = { "Assimilation", "Pluralisme", "Répression des minorités" };
const char *CULTURE_POLICY_DESC[CP_COUNT] = {
    "Intégration rapide à la culture française. Les minorités s'agitent un peu plus.",
    "Toutes les cultures sont reconnues : militance des minorités très réduite, assimilation lente, souverainistes renforcés.",
    "Régimes autoritaires seulement : militance des minorités écrasée, mais conscience en hausse, exil et réprobation internationale.",
};

/* nation-patrie de chaque culture (irredentisme), -1 si aucune */
const int CULTURE_HOMELAND[CULT_COUNT] = {
    N_FRANCE, -1, N_UK, -1, N_IRLANDE, N_BENELUX, N_ALLEMAGNE, N_ITALIE, N_ESPAGNE, -1, -1, N_PORTUGAL,
    N_SCANDINAVIE, N_SCANDINAVIE, N_BALTES, N_POLOGNE, N_EUROPE_C, N_EUROPE_C, N_EUROPE_C, N_BALKANS, N_BALKANS,
    N_ROUMANIE, N_BULGARIE, N_GRECE, N_TURQUIE, -1, N_UKRAINE, N_BOREALE, N_BOREALE, N_ALGERIE, -1, N_EGYPTE,
};

const DecreeDef DECREES[RG_COUNT][2] = {
    { { "Budget participatif", "Les assemblées décident des dépenses : militance -1,5 partout.", 8, 4 },
      { "Sobriété partagée", "Besoins en énergie et en biens -25% pendant 8 tours, prospérité en légère baisse.", 16, 0 } },
    { { "Référendum", "Consulter le peuple : +6 Cohésion si le régime est soutenu, -6 sinon.", 16, 1 },
      { "Plan de relance", "Investissement public : prospérité +5% dans les provinces françaises.", 12, 10 } },
    { { "Plan quinquennal", "Planification industrielle : +1 usine dans les 3 provinces les plus peuplées.", 20, 12 },
      { "Gouvernance algorithmique", "Recettes +10% pendant 8 tours, conscience politique en hausse.", 16, 3 } },
    { { "Censure", "Supprime les campagnes informationnelles en cours, Nébula -10 capacité, conscience -1, Influence -5.", 8, 0 },
      { "État d'urgence", "Met fin aux révoltes, militance -2, Cohésion -3.", 12, 0 } },
    { { "Mobilisation générale", "Plafond de divisions x1,5 pendant 12 tours, 3 divisions d'infanterie levées, prospérité -10%.", 20, 0 },
      { "Loi martiale", "Écrase les révoltes, militance -3, conscience +1.", 10, 0 } },
    { { "Police politique", "Conscience -2 et militance -2 partout, population -0,2%.", 8, 0 },
      { "Propagande totale", "Pendant 12 tours : cohésion soutenue, courants autoritaires renforcés. Influence -6.", 16, 2 } },
};


/* ------------------------------------------------------------------ */
/* Operations ciblees (milieux cyber, informationnel, espace, EM)      */
/* ------------------------------------------------------------------ */
const OpDef OPS[OP_COUNT] = {
    { "Frappe sur le réseau électrique",
      "Cyberattaque contre le réseau : -40% d'énergie pendant 3 tours, usines et armée ralenties.",
      D_CYBER, 4, 3, 0, false, 0.35f, 3 },
    { "Vol de technologies",
      "Intrusion dans les laboratoires de défense : accélère notre recherche, surtout si la cible est en avance.",
      D_CYBER, 3, 2, 0, false, 0.25f, 0 },
    { "Paralysie logistique",
      "Attaque des systèmes de commandement et de ravitaillement : toutes ses unités perdent de l'organisation.",
      D_CYBER, 5, 3, 0, true, 0.2f, 0 },
    { "Ingérence électorale",
      "Campagne d'influence : ses courants proches de notre régime progressent ; élections anticipées en démocratie.",
      D_INFO, 3, 2, 8, false, 0.4f, 0 },
    { "Déstabilisation",
      "Attise les colères sociales et les minorités : militance en forte hausse, risque de révolution.",
      D_INFO, 4, 3, 6, false, 0.45f, 0 },
    { "Reconnaissance satellitaire",
      "Surveillance de ses forces pendant 6 tours : +15% à nos attaques et +10% à nos défenses face à elle, "
      "programme nucléaire suivi.",
      D_ESPACE, 2, 1, 0, false, 0.05f, 6 },
    { "Brouillage électromagnétique",
      "Brouillage GNSS et des liaisons de données : son aviation et ses drones perdent 30 à 40% d'efficacité pendant 3 tours.",
      D_EM, 3, 2, 0, true, 0.3f, 3 },
    { "Tir antisatellite",
      "Détruit ses satellites militaires pendant 10 tours (qualité de l'armée -10%). Les débris menacent aussi nos "
      "satellites et le monde condamne.",
      D_ESPACE, 5, 4, 0, true, 1.0f, 10 },
};

/* ------------------------------------------------------------------ */
/* Ressources strategiques                                             */
/* ------------------------------------------------------------------ */
const StratDef STRATS[SR_COUNT] = {
    { "Terres rares",      "t", "Aimants des moteurs, des drones, des radars et des éoliennes.", 1.5f },
    { "Uranium",           "t", "Combustible des réacteurs (SMR), des sous-marins et de la dissuasion.", 2.0f },
    { "Semi-conducteurs",  "lots", "Puces de l'électronique militaire, de la recherche et de l'industrie.", 1.2f },
};

/* gisements et fonderies (unites par tour) */
const DepositDef DEPOSITS[] = {
    { PV_SE_N, SR_TERRES_RARES, 1.5f }, { PV_NO_S, SR_TERRES_RARES, 0.7f }, { PV_FI, SR_TERRES_RARES, 0.4f },
    { PV_UA_C, SR_TERRES_RARES, 0.8f }, { PV_TR_ANK, SR_TERRES_RARES, 1.0f }, { PV_RU_NORD, SR_TERRES_RARES, 1.6f },
    { PV_RU_VOL, SR_URANIUM, 1.2f }, { PV_UA_C, SR_URANIUM, 1.0f }, { PV_CZ_W, SR_URANIUM, 0.4f },
    { PV_FR_NAQ, SR_URANIUM, 0.8f }, { PV_MA_C, SR_URANIUM, 0.6f }, { PV_DZ_S, SR_URANIUM, 0.6f },
    { PV_RO_TRA, SR_URANIUM, 0.3f },
    { PV_NL_S, SR_PUCES, 1.5f }, { PV_DE_SAX, SR_PUCES, 1.2f }, { PV_FR_ARA, SR_PUCES, 0.8f },
    { PV_IE, SR_PUCES, 0.8f }, { PV_IT_LOM, SR_PUCES, 0.4f }, { PV_BE_FL, SR_PUCES, 0.3f },
};
const int DEPOSIT_COUNT = (int)(sizeof(DEPOSITS) / sizeof(DEPOSITS[0]));

/* consommation au recrutement : terres rares, uranium, semi-conducteurs */
const unsigned char UNIT_STRAT[U_TYPE_COUNT][SR_COUNT] = {
    [U_BLINDE] = { 1, 0, 0 }, [U_ARTILLERIE] = { 0, 0, 1 }, [U_DRONES] = { 1, 0, 1 },
    [U_ROBOTS] = { 2, 0, 2 }, [U_DCA] = { 1, 0, 1 }, [U_HYPER] = { 2, 0, 1 },
    [U_CHASSE] = { 2, 0, 2 }, [U_MALE] = { 1, 0, 1 },
    [U_FREGATE] = { 1, 0, 1 }, [U_SNA] = { 1, 2, 1 }, [U_PA] = { 2, 3, 2 }, [U_USV] = { 0, 0, 1 },
};

/* exposition des cotes basses a la montee des eaux (part des terres menacees a +1 m) */
const float PROVINCE_SEA_EXPOSURE[PROV_COUNT] = {
    [PV_NL_N] = 0.9f, [PV_NL_S] = 0.55f, [PV_BE_FL] = 0.45f, [PV_DE_NDS] = 0.3f, [PV_DE_SH] = 0.35f,
    [PV_DE_MV] = 0.12f, [PV_DK] = 0.25f, [PV_UK_LDN] = 0.25f, [PV_UK_MID] = 0.15f, [PV_IT_VEN] = 0.4f,
    [PV_IT_EMR] = 0.25f, [PV_EG_ALX] = 0.6f, [PV_EG_CAI] = 0.25f, [PV_FR_OCC] = 0.12f, [PV_FR_PAC] = 0.08f,
    [PV_FR_PDL] = 0.15f, [PV_FR_NAQ] = 0.12f, [PV_FR_HDF] = 0.12f, [PV_FR_NOR] = 0.08f, [PV_PL_POM] = 0.15f,
    [PV_RO_MOL] = 0.1f, [PV_UA_OD] = 0.1f, [PV_TN_N] = 0.08f, [PV_LY_TRI] = 0.05f, [PV_ES_VAL] = 0.08f,
    [PV_ES_CAT] = 0.05f, [PV_LV] = 0.05f, [PV_EE] = 0.05f, [PV_LT] = 0.05f, [PV_GR_C] = 0.04f, [PV_TR_IST] = 0.03f,
};

const char *GROUP_MODE_NAMES[GM_COUNT] = { "Libre", "Tenir le front", "Offensive" };
const char *NAVAL_MISSION_NAMES[NM_COUNT] = { "Patrouille", "Frappe à terre", "Guerre au commerce", "Escorte" };
const char *NAVAL_MISSION_DESC[NM_COUNT] = {
    "Contrôle des eaux côtières : combat naval et blocus des côtes ennemies voisines.",
    "Missiles de croisière et aviation embarquée sur une côte ennemie à 2 régions : désorganise ses défenseurs.",
    "Chasse aux navires marchands ennemis : ses importations et son trésor fondent (sous-marins et drones navals).",
    "Protège nos convois contre la guerre au commerce adverse.",
};
const char *RATING_NAMES[RATING_COUNT] = { "AAA", "AA", "A", "BBB", "BB", "B", "CCC" };
/* taux d'interet annuels selon la notation */
const float RATING_RATE[RATING_COUNT] = { 0.015f, 0.02f, 0.03f, 0.045f, 0.065f, 0.09f, 0.14f };
