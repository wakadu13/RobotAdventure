/*
 * BOUCLIER 2050 - donnees statiques du monde
 */
#include "game.h"

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
      0.85f, 0.06f, 0.00f, 1.2f, 25, 1.0f, 1.6f, 0.03f, 0.80f, 0.90f, { 0.15f, 0.10f, -0.15f } },
    { "Démocratie libérale",
      "Le régime de 2030. Équilibré, apprécié des partenaires européens, prudent face à la guerre.",
      1.00f, 0.10f, 0.05f, 0.6f, 12, 0.6f, 1.0f, 0.00f, 1.00f, 1.00f, { 0.00f, 0.05f, 0.05f } },
    { "Technocratie verte",
      "Gouvernement d'experts et planification climatique. Projets moins chers, recettes accrues, débat public réduit.",
      1.10f, 0.10f, 0.15f, 0.5f, 10, 0.3f, 0.8f, 0.00f, 0.75f, 1.05f, { -0.05f, 0.05f, 0.00f } },
    { "Régime autoritaire",
      "Exécutif fort, libertés restreintes. Mobilisation et ordre accrus, isolement diplomatique.",
      1.05f, 0.18f, 0.35f, 0.3f, 4, -0.4f, 0.7f, 0.00f, 1.00f, 1.10f, { -0.05f, -0.10f, 0.10f } },
    { "Junte militaire",
      "L'armée gouverne. Puissance militaire maximale, économie et diplomatie sacrifiées.",
      0.90f, 0.28f, 0.50f, 0.2f, 1, -1.0f, 0.5f, -0.02f, 1.10f, 1.25f, { -0.10f, -0.15f, 0.05f } },
    { "État totalitaire",
      "Contrôle total de la société, terreur et propagande. Mobilisation de masse, répression écrasante, paria international.",
      1.15f, 0.40f, 0.80f, 0.1f, 0, -1.6f, 1.3f, -0.05f, 0.90f, 1.30f, { -0.20f, -0.25f, -0.05f } },
};

const NationDef NATIONS[N_COUNT] = {
    { "France",              {  70, 120, 220 }, RG_DEMOCRATIE,   true,  100, 0.00f },
    { "Royaume-Uni",         { 190,  60,  75 }, RG_DEMOCRATIE,   false,  40, 0.10f },
    { "Irlande",             {  70, 170,  90 }, RG_DEMOCRATIE,   true,   45, 0.00f },
    { "Benelux",             { 230, 150,  60 }, RG_DEMOCRATIE,   true,   55, 0.00f },
    { "Allemagne",           { 125, 125, 125 }, RG_DEMOCRATIE,   true,   50, 0.10f },
    { "Suisse",              { 215,  90,  90 }, RG_DEMOCRATIE,   false,  40, 0.00f },
    { "Autriche",            { 225, 195, 195 }, RG_DEMOCRATIE,   true,   40, 0.05f },
    { "Italie",              {  80, 165,  80 }, RG_DEMOCRATIE,   true,   40, 0.10f },
    { "Espagne",             { 225, 195,  60 }, RG_DEMOCRATIE,   true,   45, 0.10f },
    { "Portugal",            {  50, 125,  60 }, RG_DEMOCRATIE,   true,   45, 0.00f },
    { "Danemark",            { 185,  95, 110 }, RG_DEMOCRATIE,   true,   45, 0.00f },
    { "Scandinavie",         { 100, 150, 205 }, RG_TECHNOCRATIE, true,   40, 0.05f },
    { "Pays baltes",         { 150, 120, 200 }, RG_DEMOCRATIE,   true,   40, 0.05f },
    { "Pologne",             { 220, 120, 145 }, RG_DEMOCRATIE,   true,   30, 0.15f },
    { "Europe centrale",     { 165,  95, 135 }, RG_AUTORITAIRE,  true,   15, 0.15f },
    { "Balkans",             { 150, 150,  90 }, RG_AUTORITAIRE,  false,  10, 0.20f },
    { "Roumanie",            { 200, 175,  95 }, RG_DEMOCRATIE,   true,   30, 0.10f },
    { "Bulgarie",            {  90, 160, 140 }, RG_DEMOCRATIE,   true,   25, 0.10f },
    { "Grèce",               { 110, 180, 230 }, RG_DEMOCRATIE,   true,   40, 0.10f },
    { "Turquie",             { 195,  95,  60 }, RG_AUTORITAIRE,  false,   0, 0.35f },
    { "Ukraine",             { 235, 210,  90 }, RG_DEMOCRATIE,   false,  35, 0.20f },
    { "Fédération Boréale",  { 115,  55,  60 }, RG_TOTALITAIRE,  false, -60, 0.60f },
    { "Maroc",               { 175,  75,  60 }, RG_AUTORITAIRE,  false,  10, 0.15f },
    { "Algérie",             {  60, 135,  95 }, RG_AUTORITAIRE,  false, -15, 0.20f },
    { "Tunisie",             { 200, 115, 115 }, RG_AUTORITAIRE,  false,  10, 0.10f },
    { "Libye",               { 120, 105,  75 }, RG_JUNTE,        false, -10, 0.30f },
    { "Égypte",              { 210, 180, 125 }, RG_JUNTE,        false,   0, 0.30f },
};

/* nation, pop (M), croissance %/tour, aridite, aliment, eau, energie, richesse */
const ProvinceDef PROVINCES[PROV_COUNT] = {
    { N_FRANCE,      18.0f,  0.02f, 0.20f, 16.0f, 15.0f, 20.0f, 0.45f },  /* Ile-de-France & Nord */
    { N_FRANCE,       9.0f,  0.03f, 0.15f, 16.0f, 11.0f,  6.0f, 0.38f },  /* Bretagne & Normandie */
    { N_FRANCE,       9.0f,  0.00f, 0.20f, 11.0f, 10.0f, 12.0f, 0.40f },  /* Grand Est */
    { N_FRANCE,       9.0f,  0.03f, 0.40f, 13.0f, 10.0f,  8.0f, 0.38f },  /* Aquitaine */
    { N_FRANCE,      10.0f,  0.03f, 0.35f,  8.0f, 14.0f, 16.0f, 0.44f },  /* Rhone-Alpes */
    { N_FRANCE,      12.6f,  0.04f, 0.55f, 10.0f,  8.0f,  8.0f, 0.38f },  /* Occitanie & Provence */
    { N_FRANCE,       0.4f,  0.02f, 0.60f,  0.3f,  0.5f,  0.2f, 0.30f },  /* Corse */
    { N_UK,          61.6f,  0.03f, 0.15f, 45.0f, 60.0f, 50.0f, 0.44f },
    { N_UK,           7.4f,  0.00f, 0.05f,  8.0f, 12.0f, 12.0f, 0.40f },
    { N_IRLANDE,      5.3f,  0.05f, 0.05f,  8.0f,  7.0f,  3.0f, 0.55f },
    { N_BENELUX,     18.0f,  0.02f, 0.10f, 20.0f, 18.0f, 15.0f, 0.50f },
    { N_BENELUX,     12.4f,  0.02f, 0.15f,  9.0f, 11.0f,  9.0f, 0.47f },
    { N_ALLEMAGNE,   24.0f, -0.02f, 0.18f, 16.0f, 24.0f, 20.0f, 0.48f },
    { N_ALLEMAGNE,   20.0f, -0.02f, 0.12f, 22.0f, 20.0f, 22.0f, 0.47f },
    { N_ALLEMAGNE,   25.0f,  0.00f, 0.20f, 22.0f, 26.0f, 18.0f, 0.50f },
    { N_ALLEMAGNE,   14.0f, -0.05f, 0.25f, 13.0f, 12.0f, 14.0f, 0.40f },
    { N_SUISSE,       9.2f,  0.03f, 0.15f,  5.0f, 14.0f,  9.0f, 0.70f },
    { N_AUTRICHE,     9.2f,  0.02f, 0.25f,  8.0f, 12.0f,  8.0f, 0.48f },
    { N_ITALIE,      27.0f, -0.03f, 0.35f, 25.0f, 28.0f, 18.0f, 0.42f },
    { N_ITALIE,      12.0f, -0.04f, 0.50f, 10.0f, 10.0f,  7.0f, 0.36f },
    { N_ITALIE,      12.0f, -0.06f, 0.70f, 11.0f,  8.0f,  6.0f, 0.24f },
    { N_ITALIE,       4.6f, -0.06f, 0.80f,  4.0f,  3.0f,  3.0f, 0.22f },
    { N_ITALIE,       1.5f, -0.05f, 0.70f,  1.5f,  1.4f,  1.5f, 0.26f },
    { N_ESPAGNE,     13.0f,  0.02f, 0.60f, 11.0f,  9.0f, 12.0f, 0.38f },
    { N_ESPAGNE,     14.0f,  0.02f, 0.65f, 13.0f, 11.0f, 10.0f, 0.38f },
    { N_ESPAGNE,     12.0f,  0.01f, 0.85f, 14.0f,  7.0f, 12.0f, 0.26f },
    { N_ESPAGNE,      8.0f, -0.03f, 0.30f,  8.0f, 11.0f,  7.0f, 0.30f },
    { N_PORTUGAL,    10.2f, -0.03f, 0.70f,  7.0f,  8.0f,  8.0f, 0.27f },
    { N_DANEMARK,     6.0f,  0.02f, 0.08f, 12.0f,  6.0f,  7.0f, 0.55f },
    { N_SCANDINAVIE,  5.6f,  0.04f, 0.03f,  4.0f, 14.0f, 22.0f, 0.70f },
    { N_SCANDINAVIE, 10.7f,  0.04f, 0.05f, 10.0f, 16.0f, 14.0f, 0.52f },
    { N_SCANDINAVIE,  5.6f,  0.00f, 0.03f,  5.0f,  9.0f,  6.0f, 0.48f },
    { N_BALTES,       5.8f, -0.08f, 0.10f,  7.0f,  7.0f,  4.0f, 0.26f },
    { N_POLOGNE,     17.0f, -0.04f, 0.25f, 21.0f, 15.0f, 14.0f, 0.27f },
    { N_POLOGNE,     20.0f, -0.04f, 0.30f, 22.0f, 17.0f, 12.0f, 0.24f },
    { N_EUROPE_C,    10.6f, -0.02f, 0.30f,  9.0f,  9.0f, 10.0f, 0.30f },
    { N_EUROPE_C,    15.0f, -0.05f, 0.45f, 18.0f, 13.0f,  9.0f, 0.24f },
    { N_BALKANS,      5.8f, -0.06f, 0.40f,  5.0f,  8.0f,  5.0f, 0.22f },
    { N_BALKANS,     12.0f, -0.08f, 0.50f, 13.0f, 11.0f,  8.0f, 0.12f },
    { N_BALKANS,      4.5f, -0.05f, 0.60f,  4.0f,  5.0f,  3.0f, 0.10f },
    { N_ROUMANIE,    21.5f, -0.07f, 0.45f, 26.0f, 20.0f, 16.0f, 0.17f },
    { N_BULGARIE,     6.2f, -0.10f, 0.50f,  7.0f,  6.0f,  6.0f, 0.14f },
    { N_GRECE,       10.0f, -0.06f, 0.75f,  9.0f,  7.0f,  7.0f, 0.22f },
    { N_TURQUIE,     22.0f,  0.08f, 0.50f, 17.0f, 15.0f, 12.0f, 0.16f },
    { N_TURQUIE,     18.0f,  0.08f, 0.65f, 18.0f, 13.0f, 10.0f, 0.14f },
    { N_TURQUIE,     28.0f,  0.10f, 0.70f, 25.0f, 20.0f, 16.0f, 0.11f },
    { N_TURQUIE,     21.0f,  0.15f, 0.65f, 15.0f, 20.0f, 14.0f, 0.08f },
    { N_UKRAINE,     12.0f, -0.12f, 0.35f, 16.0f, 12.0f,  6.0f, 0.06f },
    { N_UKRAINE,     11.0f, -0.12f, 0.45f, 22.0f, 12.0f, 12.0f, 0.06f },
    { N_UKRAINE,      7.0f, -0.15f, 0.55f, 10.0f,  6.0f, 10.0f, 0.05f },
    { N_UKRAINE,      6.0f, -0.12f, 0.65f, 12.0f,  4.0f,  5.0f, 0.05f },
    { N_BOREALE,      9.0f, -0.08f, 0.20f, 11.0f, 10.0f,  5.0f, 0.09f },
    { N_BOREALE,     30.0f, -0.05f, 0.20f, 18.0f, 28.0f, 40.0f, 0.15f },
    { N_BOREALE,      8.0f, -0.10f, 0.08f,  4.0f, 14.0f, 20.0f, 0.12f },
    { N_BOREALE,     25.0f, -0.03f, 0.60f, 34.0f, 18.0f, 30.0f, 0.10f },
    { N_BOREALE,     18.0f, -0.08f, 0.45f, 22.0f, 18.0f, 28.0f, 0.10f },
    { N_MAROC,       26.0f,  0.15f, 0.80f, 18.0f, 13.0f,  8.0f, 0.05f },
    { N_MAROC,       13.0f,  0.18f, 0.95f,  7.0f,  5.0f,  6.0f, 0.04f },
    { N_ALGERIE,     26.0f,  0.18f, 0.80f, 13.0f, 11.0f, 20.0f, 0.06f },
    { N_ALGERIE,     13.0f,  0.18f, 0.85f,  6.0f,  5.0f, 10.0f, 0.05f },
    { N_ALGERIE,     10.0f,  0.20f, 1.00f,  2.0f,  4.0f, 40.0f, 0.07f },
    { N_TUNISIE,     12.8f,  0.10f, 0.85f,  9.0f,  7.0f,  6.0f, 0.06f },
    { N_LIBYE,        5.0f,  0.15f, 1.00f,  2.0f,  3.0f, 22.0f, 0.10f },
    { N_LIBYE,        2.5f,  0.15f, 1.00f,  1.0f,  2.0f, 18.0f, 0.10f },
    { N_EGYPTE,      90.0f,  0.30f, 0.95f, 65.0f, 70.0f, 50.0f, 0.04f },
    { N_EGYPTE,      30.0f,  0.35f, 1.00f, 12.0f, 15.0f, 15.0f, 0.03f },
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

const DilemmaDef DYN_DILEMMAS[2] = {
    { "Article 42.7 : un allié est attaqué",
      "Un État membre de l'Union européenne est envahi et invoque la clause d'assistance mutuelle. Les capitales regardent Paris.",
      "Entrer en guerre à ses côtés", "Guerre contre l'agresseur, +5 Influence, relation de l'allié +30",
      "Soutien matériel uniquement", "-6 Budget, -4 Influence" },
    { "Offre de paix",
      "Épuisé, l'ennemi propose de cesser les hostilités. Les territoires conquis resteraient sous contrôle français.",
      "Signer la paix", "Fin de la guerre, trêve de 4 ans",
      "Poursuivre la guerre", "La guerre continue" },
};

const char *MIGRATION_DOCTRINES[3] = { "Accueil organisé", "Contrôle équilibré", "Fermeture stricte" };
const char *MIGRATION_DESC[3] = {
    "90% des migrants admis et répartis dans les provinces : main d'oeuvre et croissance, mais population à intégrer. +1 Influence/tour, relations du Sud en hausse.",
    "50% des migrants admis. Les refoulés alimentent les flux clandestins aux frontières.",
    "15% admis. Neutralisation des flux facilitée (x1,4), surcoût 1,5/tour, -1,5 Influence/tour, relations du Sud en baisse.",
};
const float MIGRATION_ACCEPT[3] = { 0.9f, 0.5f, 0.15f };

const char *ALERT_LEVELS[3] = { "Vigilance", "Alerte renforcée", "Urgence attentat" };
const char *ALERT_DESC[3] = {
    "Posture normale. Aucun surcoût.",
    "Défense +15% partout. -3 Budget/tour, -0,4 Cohésion/tour (lassitude).",
    "Défense +30% partout. -7 Budget/tour, -1,2 Cohésion/tour.",
};
