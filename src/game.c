/*
 * BOUCLIER 2050 - logique de simulation
 */
#include "game.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Donnees                                                             */
/* ------------------------------------------------------------------ */

const char *DOMAIN_NAMES[D_COUNT] = {
    "Terre", "Mer", "Air", "Espace", "Cyber", "Informationnel", "Electromagnétique"
};

const char *RESOURCE_NAMES[R_COUNT] = {
    "Budget", "Énergie", "Eau", "Alimentation", "Cohésion", "Influence", "Souveraineté"
};

const char *RESOURCE_DESC[R_COUNT] = {
    "Crédits de défense et de résilience (milliards). Sous -60 : défaut de paiement.",
    "Sécurité d'approvisionnement énergétique. Une pénurie fragilise la cohésion.",
    "Sécurité hydrique. Les sécheresses et les cartels de l'eau la menacent.",
    "Sécurité alimentaire. Climat et pillage de la ZEE la dégradent.",
    "Cohésion nationale. A 0 : effondrement social, partie perdue.",
    "Influence internationale, dépensée pour la diplomatie.",
    "Souveraineté. A 0 : la France perd le contrôle de son destin.",
};

const TheaterDef THEATERS[TH_COUNT] = {
    { "Frontière Nord",        "Plaine ouverte vers le Benelux. Axe logistique majeur, cible des trafics et des sabotages.", D_TERRE },
    { "Frontière Est",         "Rhin et Lorraine. Zone sensible aux incursions et aux sabotages d'infrastructures.", D_TERRE },
    { "Alpes",                 "Glaciers disparus, nappes convoitées. Passages de montagne difficiles à surveiller.", D_TERRE },
    { "Pyrénées",              "Frontière sud, sous pression migratoire et hydrique depuis la désertification ibérique.", D_TERRE },
    { "Manche & Atlantique",   "Câbles sous-marins, éolien offshore, traversées clandestines.", D_MER },
    { "Méditerranée",          "Mer surchauffée, routes migratoires instrumentalisées, harcèlement naval.", D_MER },
    { "Outre-mer & ZEE",       "Deuxième ZEE mondiale : pêche, minerais, territoires exposés aux cyclones.", D_MER },
    { "Espace aérien",         "Police du ciel, défense anti-drones, protection des sites sensibles.", D_AIR },
    { "Orbite",                "Satellites de renseignement, de communication et de navigation. Améliore la détection partout.", D_ESPACE },
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
    { "Fédération Boréale",  "Puissance continentale enrichie par le dégel arctique. Pratique l'intimidation militaire, spatiale et électronique." },
    { "Ligue des Sables",    "Régimes du Sud en effondrement climatique. Instrumentalise les flux de réfugiés pour obtenir des concessions." },
    { "Consortium Hydra",    "Cartel privé de l'eau et des ressources. Corrompt, pille et achète ce qu'il ne peut voler." },
    { "Réseau Nébula",       "Collectif de hackers mercenaires et de fermes à trolls, loué au plus offrant." },
};

const ProjectDef PROJECTS[P_COUNT] = {
    { "Programme SMR",          "Petits réacteurs nucléaires modulaires : +3,5 Énergie/tour.",                      24, 6 },
    { "Éolien & solaire",       "Parcs renouvelables décentralisés : +2 Énergie/tour.",                             12, 3 },
    { "Usines de dessalement",  "Eau douce sur les littoraux : +3 Eau/tour.",                                       14, 4 },
    { "Agro-résilience",        "Cultures adaptées, irrigation de précision : +2,5 Alimentation/tour.",             10, 3 },
    { "Bouclier cyber civil",   "Durcit hôpitaux et réseaux : effets des cyberattaques -35%.",                       9, 2 },
    { "Éducation aux médias",   "Résilience de l'opinion : effets informationnels -35%, +0,5 Cohésion/tour.",        7, 4 },
    { "Constellation souveraine","Satellites nationaux : +15% détection partout, défense orbitale renforcée.",      16, 4 },
    { "Plan Adaptation Climat", "Digues, forêts, réseaux de froid : dégâts des catastrophes -45%.",                 15, 4 },
};

const ActionDef ACTIONS[A_COUNT] = {
    { "Diplomatie",     "Négociations et concessions : réduit l'hostilité (succès selon l'influence).", 1, 0, 6, D_COUNT, 0 },
    { "Sanctions",      "Sanctions économiques : réduit la capacité, augmente l'hostilité.",           1, 3, 0, D_COUNT, 0 },
    { "Cyber-offensive","Frappe cyber sur leurs moyens : forte baisse de capacité. Risque d'attribution. Requiert capacité Cyber 5.", 1, 0, 0, D_CYBER, 5 },
    { "Contre-influence","Opération d'information : baisse hostilité et capacité, +1 Cohésion. Requiert capacité Info 3.", 1, 0, 0, D_INFO, 3 },
    { "Renseignement",  "DGSE/DRM sur cette faction pendant 3 tours : détection et neutralisation accrues.", 1, 2, 0, D_COUNT, 0 },
};

const DilemmaDef DILEMMAS[] = {
    { "Offre du Consortium Hydra",
      "Hydra propose de racheter la gestion de l'eau du Sud-Est. L'offre est généreuse, mais l'eau deviendrait un levier entre ses mains.",
      "Accepter la vente", "+15 Budget, +5 Eau, -8 Souveraineté, Hydra -10 hostilité",
      "Refuser", "Hydra +10 hostilité" },
    { "Exode climatique sahélien",
      "200 000 déplacés convergent vers les côtes méditerranéennes après l'effondrement des récoltes du Sahel.",
      "Corridor humanitaire européen", "+8 Influence, -6 Budget, -4 Alimentation",
      "Sous-traiter le contrôle à la Ligue", "-4 Budget, -6 Influence, Ligue -10 hostilité mais +10 capacité" },
    { "Blackout en pleine canicule",
      "Le réseau électrique cède sous la demande de climatisation. Des millions de foyers sont dans le noir.",
      "Rationnement national", "+6 Énergie, -6 Cohésion",
      "Importer à prix d'or", "-10 Budget, +2 Énergie" },
    { "Le lanceur d'alerte",
      "Un fonctionnaire révèle un programme de surveillance de masse. Nébula amplifie le scandale.",
      "Transparence et commission d'enquête", "+4 Cohésion, -3 Souveraineté",
      "Censurer les fuites", "-6 Cohésion, Nébula +10 hostilité" },
    { "Alliance spatiale européenne",
      "Les partenaires européens proposent de mutualiser surveillance orbitale et lanceurs.",
      "Rejoindre l'alliance", "-8 Budget, +2 capacité Espace, +5 Influence",
      "Préserver l'autonomie", "+3 Souveraineté" },
    { "Grève générale contre la vie chère",
      "L'inflation alimentaire déclenche une grève générale. Le pays est paralysé.",
      "Concessions sociales", "-8 Budget, +8 Cohésion",
      "Fermeté", "-7 Cohésion, +2 Budget" },
    { "Coup d'État au Sahel",
      "Une junte alliée à la Ligue des Sables prend le pouvoir et menace les ressortissants français.",
      "Intervention ciblée", "-10 Budget, Ligue -12 capacité, +15 hostilité, -3 Influence",
      "Non-ingérence", "+2 Influence, Ligue +8 capacité" },
    { "Accord gazier boréal",
      "La Fédération Boréale propose un contrat gazier à bas prix sur dix ans. Bruxelles désapprouve.",
      "Signer l'accord", "+1,5 Énergie/tour, Boréale -15 hostilité, -5 Souveraineté, -5 Influence",
      "Refuser", "+2 Souveraineté" },
    { "IA de défense autonome",
      "Les armées proposent de confier la coordination tactique à une IA. Efficace, mais controversée.",
      "Déployer l'IA", "Défense +12% partout, -4 Cohésion, -3 Influence",
      "Refuser par principe éthique", "+2 Cohésion" },
    { "Cyclone sur les Antilles",
      "Un cyclone de catégorie 6 ravage les Antilles. Les sinistrés attendent la réponse de la métropole.",
      "Plan de reconstruction", "-12 Budget, +5 Cohésion, +2 Influence",
      "Aide minimale", "-6 Cohésion, -3 Influence" },
    { "Cyber-mercenaires à vendre",
      "D'anciens membres de Nébula proposent leurs services et leurs outils offensifs.",
      "Les recruter", "-6 Budget, +3 capacité Cyber, -4 Influence",
      "Les livrer à la justice", "Nébula +8 hostilité, -6 capacité" },
    { "Sommet climat de Paris 2050",
      "La France accueille un sommet de la dernière chance sur le climat.",
      "Engagement ambitieux", "-10 Budget, +10 Influence, catastrophes moins fréquentes",
      "Statu quo", "+1 Budget, -4 Influence" },
};
const int DILEMMA_COUNT = (int)(sizeof(DILEMMAS) / sizeof(DILEMMAS[0]));

const char *MIGRATION_DOCTRINES[3] = { "Accueil organisé", "Contrôle équilibré", "Fermeture stricte" };
const char *MIGRATION_DESC[3] = {
    "Les flux sont pris en charge : effets sur la cohésion divisés par deux, mais coût budgétaire et alimentaire. +1 Influence/tour.",
    "Doctrine par défaut : contrôle aux frontières et accueil sélectif.",
    "Neutralisation des flux facilitée (x1,4), surcoût de 1,5/tour, -1,5 Influence/tour et risque d'incidents humanitaires.",
};
const char *ALERT_LEVELS[3] = { "Vigilance", "Alerte renforcée", "Urgence attentat" };
const char *ALERT_DESC[3] = {
    "Posture normale. Aucun surcoût.",
    "Défense +15% partout. -3 Budget/tour, -0,4 Cohésion/tour (lassitude).",
    "Défense +30% partout. -7 Budget/tour, -1,2 Cohésion/tour.",
};

/* faction -> types de menaces privilegies */
static const int FACTION_THREATS[F_COUNT][8] = {
    {  3,  5,  7,  8,  9, 10, 12, 16 },   /* Boreale */
    {  1,  6,  0,  2,  4, 15,  1,  6 },   /* Sables */
    {  0,  4, 13, 17,  5, 11, 17,  4 },   /* Hydra */
    { 14, 15, 11, 12, 13, 14, 16, 15 },   /* Nebula */
};

/* poids geographiques par faction et theatre */
static const int FACTION_GEO[F_COUNT][TH_COUNT] = {
    /* N  E  A  P  At Me OM Ci Or Cy In Sp */
    {  3, 4, 1, 0, 3, 1, 1, 1, 1, 1, 1, 1 },
    {  1, 1, 2, 3, 1, 4, 1, 1, 1, 1, 1, 1 },
    {  2, 1, 3, 3, 2, 2, 4, 1, 1, 1, 1, 1 },
    {  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

/* ------------------------------------------------------------------ */
/* Utilitaires                                                         */
/* ------------------------------------------------------------------ */

static unsigned Rand(Game *g)
{
    unsigned x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x ? x : 0x9E3779B9u;
    return g->rng;
}
static float Randf(Game *g) { return (Rand(g) & 0xFFFFFF) / 16777216.0f; }
static int RandRange(Game *g, int lo, int hi) { return lo + (int)(Rand(g) % (unsigned)(hi - lo + 1)); }
static float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void SpawnThreatFor(Game *g, int f);

void Game_Log(Game *g, LogKind kind, const char *fmt, ...)
{
    if (g->logCount == MAX_LOG) {
        memmove(&g->log[0], &g->log[1], sizeof(LogEntry) * (MAX_LOG - 1));
        g->logCount--;
    }
    LogEntry *e = &g->log[g->logCount++];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, LOG_LEN, fmt, ap);
    va_end(ap);
    e->turn = g->turn;
    e->kind = kind;
}

void Game_DateString(int turn, char *buf, int len)
{
    snprintf(buf, len, "T%d %d", turn % 4 + 1, START_YEAR + turn / 4);
}

static void AddRes(Game *g, Resource r, float v)
{
    g->res[r] += v;
    if (r != R_BUDGET) g->res[r] = Clampf(g->res[r], 0, 100);
}

static void AddFac(Game *g, int f, float hostility, float capability)
{
    g->fac[f].hostility  = Clampf(g->fac[f].hostility + hostility, 5, 100);
    g->fac[f].capability = Clampf(g->fac[f].capability + capability, 5, 100);
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

void Game_Init(Game *g, unsigned seed)
{
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 12345u;
    g->temp = 2.1f;

    g->res[R_BUDGET]       = 30;
    g->res[R_ENERGIE]      = 62;
    g->res[R_EAU]          = 58;
    g->res[R_ALIM]         = 66;
    g->res[R_COHESION]     = 58;
    g->res[R_INFLUENCE]    = 50;
    g->res[R_SOUVERAINETE] = 72;

    static const int cap[D_COUNT] = { 10, 8, 4, 2, 3, 2, 2 };
    memcpy(g->capacity, cap, sizeof(cap));

    static const int post[TH_COUNT] = { 3, 3, 2, 2, 3, 3, 2, 3, 1, 2, 1, 1 };
    memcpy(g->posture, post, sizeof(post));

    g->fac[F_BOREALE] = (Faction){ 55, 65, 0, false };
    g->fac[F_SABLES]  = (Faction){ 45, 45, 0, false };
    g->fac[F_HYDRA]   = (Faction){ 40, 55, 0, false };
    g->fac[F_NEBULA]  = (Faction){ 50, 45, 0, false };

    g->migrationDoctrine = 1;
    g->alertLevel = 0;
    g->commandPoints = BASE_CP;
    g->pendingDilemma = -1;
    g->state = GAME_RUNNING;

    Game_Log(g, EV_EVENT, "Janvier 2050. +2,1°C. Le Conseil de défense vous confie le Bouclier national.");
    Game_Log(g, EV_INFO, "Répartissez vos forces, protégez les ressources et tenez jusqu'en 2060.");

    /* menaces initiales */
    for (int i = 0; i < 4; i++) {
        SpawnThreatFor(g, i);
    }
}

/* ------------------------------------------------------------------ */
/* Forces                                                              */
/* ------------------------------------------------------------------ */

int Game_PostureUsed(const Game *g, Domain d)
{
    int n = 0;
    for (int t = 0; t < TH_COUNT; t++)
        if (THEATERS[t].domain == d) n += g->posture[t];
    return n;
}

bool Game_ChangePosture(Game *g, int theater, int delta)
{
    Domain d = THEATERS[theater].domain;
    int nv = g->posture[theater] + delta;
    if (nv < 0) return false;
    if (delta > 0 && Game_PostureUsed(g, d) + delta > g->capacity[d]) return false;
    g->posture[theater] = nv;
    return true;
}

float Game_InvestCost(const Game *g, Domain d)
{
    static const float base[D_COUNT] = { 5, 7, 7, 9, 5, 4, 5 };
    return base[d] + g->capacity[d] * 0.4f;
}

bool Game_InvestCapacity(Game *g, Domain d)
{
    if (g->pendingTurns[d] > 0) return false;
    float c = Game_InvestCost(g, d);
    if (g->res[R_BUDGET] < c) return false;
    g->res[R_BUDGET] -= c;
    g->pendingCap[d] = 2;
    g->pendingTurns[d] = 2;
    Game_Log(g, EV_INFO, "Montée en puissance %s lancée (+2 capacité dans 2 tours).", DOMAIN_NAMES[d]);
    return true;
}

static int ActiveThreatsOfDomain(const Game *g, Domain d)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++)
        if (g->threats[i].active && THREATS[g->threats[i].type].domain == d) n++;
    return n;
}

float Game_DefensePower(const Game *g, int theater)
{
    float p = g->posture[theater] * 1.5f;
    float mult = 1.0f + 0.15f * g->alertLevel;
    if (g->aiDefense) mult += 0.12f;
    Domain d = THEATERS[theater].domain;
    if (d == D_AIR || d == D_MER) {
        /* un spectre electromagnetique conteste degrade l'air et la mer */
        int em = ActiveThreatsOfDomain(g, D_EM);
        float pen = 0.15f * em;
        if (pen > 0.45f) pen = 0.45f;
        mult -= pen;
    }
    if (theater == TH_ORBITE && g->proj[P_CONSTELL].state == 2) p += 3;
    return p * mult;
}

float Game_NeutralizeChance(const Game *g, const Threat *t)
{
    float def = Game_DefensePower(g, t->theater);
    if (def <= 0) return 0;
    float p = def / (def + t->strength * 1.2f);
    if (g->fac[t->faction].intelTurns > 0) p *= 1.2f;
    if (THREATS[t->type].migration && g->migrationDoctrine == 2) p *= 1.4f;
    if (!t->detected) p *= 0.35f;
    return Clampf(p, 0, 0.95f);
}

static float DetectChance(const Game *g, const Threat *t)
{
    float p = 0.45f + 0.05f * g->posture[TH_ORBITE] + 0.03f * g->posture[t->theater];
    if (g->proj[P_CONSTELL].state == 2) p += 0.15f;
    if (g->fac[t->faction].intelTurns > 0) p += 0.35f;
    return Clampf(p, 0, 0.98f);
}

int Game_ThreatCount(const Game *g, int theater)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++)
        if (g->threats[i].active && g->threats[i].theater == theater) n++;
    return n;
}

int Game_ActiveThreats(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_THREATS; i++) if (g->threats[i].active) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Actions strategiques                                                */
/* ------------------------------------------------------------------ */

const char *Game_ActionBlocked(const Game *g, int f, int a)
{
    const ActionDef *ad = &ACTIONS[a];
    if (g->state != GAME_RUNNING) return "Partie terminée";
    if (g->commandPoints < ad->cp) return "Plus de points de commandement";
    if (g->fac[f].actedThisTurn) return "Déjà une action ce tour sur cette faction";
    if (g->res[R_BUDGET] < ad->budget) return "Budget insuffisant";
    if (g->res[R_INFLUENCE] < ad->influence) return "Influence insuffisante";
    if (ad->needDomain != D_COUNT && g->capacity[ad->needDomain] < ad->needCapacity)
        return ad->needDomain == D_CYBER ? "Capacité Cyber insuffisante (5)" : "Capacité Info insuffisante (3)";
    return NULL;
}

bool Game_FactionAction(Game *g, int f, int a)
{
    if (Game_ActionBlocked(g, f, a)) return false;
    const ActionDef *ad = &ACTIONS[a];
    g->commandPoints -= ad->cp;
    g->res[R_BUDGET] -= ad->budget;
    g->res[R_INFLUENCE] -= ad->influence;
    g->fac[f].actedThisTurn = true;
    const char *fn = FACTIONS[f].name;

    switch (a) {
    case A_DIPLO: {
        float chance = 0.45f + g->res[R_INFLUENCE] / 150.0f;
        if (Randf(g) < chance) {
            AddFac(g, f, -(8 + g->res[R_INFLUENCE] * 0.1f), 0);
            Game_Log(g, EV_GOOD, "Diplomatie : désescalade obtenue avec %s.", fn);
        } else {
            AddFac(g, f, -2, 0);
            Game_Log(g, EV_BAD, "Diplomatie : %s rejette nos propositions.", fn);
        }
    } break;
    case A_SANCTION:
        AddFac(g, f, 5, -6);
        Game_Log(g, EV_INFO, "Sanctions contre %s : leurs moyens s'érodent, la rancoeur grandit.", fn);
        break;
    case A_CYBEROFF:
        AddFac(g, f, 6, -10);
        if (Randf(g) < 0.25f) {
            AddFac(g, f, 6, 0);
            AddRes(g, R_INFLUENCE, -6);
            Game_Log(g, EV_BAD, "Cyber-offensive contre %s attribuée publiquement à la France !", fn);
        } else {
            Game_Log(g, EV_GOOD, "Cyber-offensive réussie : infrastructures de %s neutralisées.", fn);
        }
        break;
    case A_INFLUENCE:
        AddFac(g, f, -4, -4);
        AddRes(g, R_COHESION, 1);
        Game_Log(g, EV_GOOD, "Contre-influence : les récits de %s perdent du terrain.", fn);
        break;
    case A_RENSEIGNEMENT:
        g->fac[f].intelTurns = 3;
        for (int i = 0; i < MAX_THREATS; i++)
            if (g->threats[i].active && g->threats[i].faction == f) g->threats[i].detected = true;
        Game_Log(g, EV_GOOD, "Renseignement : les réseaux de %s sont cartographiés.", fn);
        break;
    }
    return true;
}

bool Game_StartProject(Game *g, int p)
{
    if (g->proj[p].state != 0 || g->res[R_BUDGET] < PROJECTS[p].cost) return false;
    g->res[R_BUDGET] -= PROJECTS[p].cost;
    g->proj[p].state = 1;
    g->proj[p].turnsLeft = PROJECTS[p].turns;
    Game_Log(g, EV_INFO, "Lancement du projet : %s (%d tours).", PROJECTS[p].name, PROJECTS[p].turns);
    return true;
}

void Game_SetMigrationDoctrine(Game *g, int d)
{
    if (d == g->migrationDoctrine) return;
    g->migrationDoctrine = d;
    AddRes(g, R_COHESION, -2);   /* tout changement de doctrine divise */
    Game_Log(g, EV_INFO, "Nouvelle doctrine migratoire : %s.", MIGRATION_DOCTRINES[d]);
}

void Game_SetAlertLevel(Game *g, int a)
{
    if (a == g->alertLevel) return;
    g->alertLevel = a;
    Game_Log(g, EV_INFO, "Niveau d'alerte : %s.", ALERT_LEVELS[a]);
}

/* ------------------------------------------------------------------ */
/* Dilemmes                                                            */
/* ------------------------------------------------------------------ */

void Game_ResolveDilemma(Game *g, int c)
{
    int d = g->pendingDilemma;
    if (d < 0) return;
    g->pendingDilemma = -1;
    bool A = (c == 0);
    switch (d) {
    case 0: if (A) { AddRes(g, R_BUDGET, 15); AddRes(g, R_EAU, 5); AddRes(g, R_SOUVERAINETE, -8); AddFac(g, F_HYDRA, -10, 0); }
            else   { AddFac(g, F_HYDRA, 10, 0); } break;
    case 1: if (A) { AddRes(g, R_INFLUENCE, 8); AddRes(g, R_BUDGET, -6); AddRes(g, R_ALIM, -4); }
            else   { AddRes(g, R_BUDGET, -4); AddRes(g, R_INFLUENCE, -6); AddFac(g, F_SABLES, -10, 10); } break;
    case 2: if (A) { AddRes(g, R_ENERGIE, 6); AddRes(g, R_COHESION, -6); }
            else   { AddRes(g, R_BUDGET, -10); AddRes(g, R_ENERGIE, 2); } break;
    case 3: if (A) { AddRes(g, R_COHESION, 4); AddRes(g, R_SOUVERAINETE, -3); }
            else   { AddRes(g, R_COHESION, -6); AddFac(g, F_NEBULA, 10, 0); } break;
    case 4: if (A) { AddRes(g, R_BUDGET, -8); g->capacity[D_ESPACE] += 2; AddRes(g, R_INFLUENCE, 5); }
            else   { AddRes(g, R_SOUVERAINETE, 3); } break;
    case 5: if (A) { AddRes(g, R_BUDGET, -8); AddRes(g, R_COHESION, 8); }
            else   { AddRes(g, R_COHESION, -7); AddRes(g, R_BUDGET, 2); } break;
    case 6: if (A) { AddRes(g, R_BUDGET, -10); AddFac(g, F_SABLES, 15, -12); AddRes(g, R_INFLUENCE, -3); }
            else   { AddRes(g, R_INFLUENCE, 2); AddFac(g, F_SABLES, 0, 8); } break;
    case 7: if (A) { g->importBonus += 1.5f; AddFac(g, F_BOREALE, -15, 0); AddRes(g, R_SOUVERAINETE, -5); AddRes(g, R_INFLUENCE, -5); }
            else   { AddRes(g, R_SOUVERAINETE, 2); } break;
    case 8: if (A) { g->aiDefense = true; AddRes(g, R_COHESION, -4); AddRes(g, R_INFLUENCE, -3); }
            else   { AddRes(g, R_COHESION, 2); } break;
    case 9: if (A) { AddRes(g, R_BUDGET, -12); AddRes(g, R_COHESION, 5); AddRes(g, R_INFLUENCE, 2); }
            else   { AddRes(g, R_COHESION, -6); AddRes(g, R_INFLUENCE, -3); } break;
    case 10: if (A) { AddRes(g, R_BUDGET, -6); g->capacity[D_CYBER] += 3; AddRes(g, R_INFLUENCE, -4); }
             else   { AddFac(g, F_NEBULA, 8, -6); } break;
    case 11: if (A) { AddRes(g, R_BUDGET, -10); AddRes(g, R_INFLUENCE, 10); g->climateMitigation += 0.08f; }
             else   { AddRes(g, R_BUDGET, 1); AddRes(g, R_INFLUENCE, -4); } break;
    }
    Game_Log(g, EV_EVENT, "%s : %s.", DILEMMAS[d].title, A ? DILEMMAS[d].optA : DILEMMAS[d].optB);
}

/* ------------------------------------------------------------------ */
/* Menaces                                                             */
/* ------------------------------------------------------------------ */

static void SpawnThreatFor(Game *g, int f)
{
    int slot = -1;
    for (int i = 0; i < MAX_THREATS; i++) if (!g->threats[i].active) { slot = i; break; }
    if (slot < 0) return;

    int type = FACTION_THREATS[f][Rand(g) % 8];
    Domain d = THREATS[type].domain;

    int total = 0;
    for (int t = 0; t < TH_COUNT; t++) if (THEATERS[t].domain == d) total += FACTION_GEO[f][t];
    int theater = -1;
    if (total > 0) {
        int r = (int)(Rand(g) % (unsigned)total);
        for (int t = 0; t < TH_COUNT; t++) {
            if (THEATERS[t].domain != d) continue;
            r -= FACTION_GEO[f][t];
            if (r < 0) { theater = t; break; }
        }
    }
    if (theater < 0) {
        for (int t = 0; t < TH_COUNT; t++) if (THEATERS[t].domain == d) { theater = t; break; }
    }

    Threat *th = &g->threats[slot];
    th->active = true;
    th->type = type;
    th->theater = theater;
    th->faction = f;
    th->strength = RandRange(g, THREATS[type].minStr, THREATS[type].maxStr) + (int)(g->fac[f].capability / 40.0f);
    if (g->fac[f].intelTurns > 0 && th->strength > 2) th->strength--;
    th->turnsLeft = THREATS[type].duration;
    th->detected = g->fac[f].intelTurns > 0 || Randf(g) < 0.3f;
    th->fresh = true;
}

static void ResolveThreats(Game *g)
{
    for (int t = 0; t < TH_COUNT; t++) g->tension[t] = 0;

    for (int i = 0; i < MAX_THREATS; i++) {
        Threat *t = &g->threats[i];
        if (!t->active) continue;
        t->fresh = false;
        const ThreatDef *td = &THREATS[t->type];

        if (!t->detected && Randf(g) < DetectChance(g, t)) t->detected = true;

        if (Randf(g) < Game_NeutralizeChance(g, t)) {
            t->active = false;
            g->statNeutralized++;
            AddRes(g, R_SOUVERAINETE, 0.4f);
            Game_Log(g, EV_GOOD, "%s neutralisé(e) - %s.", td->name, THEATERS[t->theater].name);
            continue;
        }

        float mod = 1.0f;
        if (td->domain == D_CYBER && g->proj[P_CYBERCIV].state == 2) mod *= 0.65f;
        if (td->domain == D_INFO  && g->proj[P_MEDIAS].state == 2)   mod *= 0.65f;

        float dmg = 0;
        for (int r = 0; r < R_COUNT; r++) {
            float e = td->effects[r] * t->strength * mod * THREAT_SCALE;
            if (td->migration && r == R_COHESION && g->migrationDoctrine == 0) e *= 0.5f;
            AddRes(g, (Resource)r, e);
            dmg -= e;
        }
        if (td->migration) {
            if (g->migrationDoctrine == 0) {
                AddRes(g, R_BUDGET, -0.35f * t->strength);
                AddRes(g, R_ALIM, -0.15f * t->strength);
            } else if (g->migrationDoctrine == 2 && Randf(g) < 0.15f) {
                AddRes(g, R_INFLUENCE, -4);
                Game_Log(g, EV_BAD, "Drame humanitaire à la frontière (%s) : indignation internationale.",
                         THEATERS[t->theater].name);
            }
        }
        g->tension[t->theater] += dmg;
        g->statSuffered++;

        if (--t->turnsLeft <= 0) {
            t->active = false;
            Game_Log(g, EV_BAD, "%s (%s) a frappé sans être stoppé(e).", td->name, THEATERS[t->theater].name);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Climat                                                              */
/* ------------------------------------------------------------------ */

static void ClimateTurn(Game *g)
{
    g->temp += 0.03f;
    float stress = g->temp - 2.0f;

    /* derive climatique, partiellement compensee par l'adaptation spontanee
       de l'economie (rappel vers 55) */
    AddRes(g, R_ENERGIE, -(0.6f + stress * 0.9f) + g->importBonus + 0.04f * (55 - g->res[R_ENERGIE]));
    AddRes(g, R_EAU,     -(0.8f + stress * 1.3f) + 0.04f * (55 - g->res[R_EAU]));
    AddRes(g, R_ALIM,    -(0.6f + stress * 1.0f) + 0.04f * (55 - g->res[R_ALIM]));

    if (g->proj[P_SMR].state == 2)    AddRes(g, R_ENERGIE, 3.5f);
    if (g->proj[P_ENR].state == 2)    AddRes(g, R_ENERGIE, 2.0f);
    if (g->proj[P_DESSAL].state == 2) AddRes(g, R_EAU, 3.0f);
    if (g->proj[P_AGRO].state == 2)   AddRes(g, R_ALIM, 2.5f);
    if (g->proj[P_MEDIAS].state == 2) AddRes(g, R_COHESION, 0.5f);

    float chance = 0.30f + stress * 0.35f - g->climateMitigation;
    if (Randf(g) < chance) {
        float k = g->proj[P_ADAPT].state == 2 ? 0.55f : 1.0f;
        g->statDisasters++;
        switch (Rand(g) % 5) {
        case 0:
            AddRes(g, R_ENERGIE, -7 * k); AddRes(g, R_EAU, -7 * k); AddRes(g, R_COHESION, -3 * k);
            Game_Log(g, EV_CLIMATE, "Canicule historique : 48°C à Lyon, réseaux à saturation.");
            break;
        case 1:
            AddRes(g, R_EAU, -10 * k); AddRes(g, R_ALIM, -8 * k);
            Game_Log(g, EV_CLIMATE, "Sécheresse : la Loire est à sec, récoltes perdues.");
            break;
        case 2:
            AddRes(g, R_BUDGET, -6 * k); AddRes(g, R_COHESION, -2 * k);
            Game_Log(g, EV_CLIMATE, "Inondations majeures dans le bassin du Rhône.");
            break;
        case 3:
            AddRes(g, R_BUDGET, -4 * k); AddRes(g, R_ALIM, -3 * k); AddRes(g, R_COHESION, -3 * k);
            Game_Log(g, EV_CLIMATE, "Mégafeux dans les Landes et en Provence.");
            break;
        default:
            AddRes(g, R_ENERGIE, -5 * k); AddRes(g, R_BUDGET, -4 * k);
            Game_Log(g, EV_CLIMATE, "Tempête atlantique : littoral dévasté, réseau coupé.");
            break;
        }
        /* chaque catastrophe aggrave la crise mondiale */
        AddFac(g, F_SABLES, 1.5f, 1.0f);
        AddFac(g, F_HYDRA, 1.0f, 0.5f);
    }
}

/* ------------------------------------------------------------------ */
/* Budget                                                              */
/* ------------------------------------------------------------------ */

float Game_Income(const Game *g)
{
    return 24.0f + 0.08f * g->res[R_ENERGIE] + 0.06f * g->res[R_COHESION];
}

float Game_Upkeep(const Game *g)
{
    float u = 0;
    for (int t = 0; t < TH_COUNT; t++) u += g->posture[t] * 0.6f;
    for (int d = 0; d < D_COUNT; d++) u += g->capacity[d] * 0.15f;
    static const float alert[3] = { 0, 3, 7 };
    u += alert[g->alertLevel];
    if (g->migrationDoctrine == 2) u += 1.5f;
    return u;
}

/* ------------------------------------------------------------------ */
/* Tour                                                                */
/* ------------------------------------------------------------------ */

void Game_EndTurn(Game *g)
{
    if (g->state != GAME_RUNNING || g->pendingDilemma >= 0) return;

    float before[R_COUNT];
    memcpy(before, g->res, sizeof(before));

    /* 1. economie */
    g->res[R_BUDGET] += Game_Income(g) - Game_Upkeep(g);
    if (g->res[R_BUDGET] < 0) {
        g->res[R_BUDGET] *= 1.03f;     /* interets de la dette */
        AddRes(g, R_COHESION, -1);
    }
    if (g->migrationDoctrine == 0) AddRes(g, R_INFLUENCE, 1.0f);
    if (g->migrationDoctrine == 2) AddRes(g, R_INFLUENCE, -1.5f);
    if (g->alertLevel == 1) AddRes(g, R_COHESION, -0.4f);
    if (g->alertLevel == 2) AddRes(g, R_COHESION, -1.2f);

    /* 2. projets et montees en puissance */
    for (int p = 0; p < P_COUNT; p++) {
        if (g->proj[p].state == 1 && --g->proj[p].turnsLeft <= 0) {
            g->proj[p].state = 2;
            Game_Log(g, EV_GOOD, "Projet opérationnel : %s.", PROJECTS[p].name);
        }
    }
    for (int d = 0; d < D_COUNT; d++) {
        if (g->pendingTurns[d] > 0 && --g->pendingTurns[d] == 0) {
            g->capacity[d] += g->pendingCap[d];
            g->pendingCap[d] = 0;
            Game_Log(g, EV_GOOD, "Nouvelles capacités %s disponibles.", DOMAIN_NAMES[d]);
        }
    }

    /* 3. menaces */
    ResolveThreats(g);

    /* 4. climat */
    ClimateTurn(g);

    /* 5. penuries */
    for (int r = R_ENERGIE; r <= R_ALIM; r++) {
        if (g->res[r] < 15) {
            AddRes(g, R_COHESION, -3.5f);
            AddRes(g, R_SOUVERAINETE, -1.5f);
        } else if (g->res[r] < 35) {
            AddRes(g, R_COHESION, -1.2f);
        }
    }
    /* resilience naturelle de la societe et des institutions */
    AddRes(g, R_COHESION, 0.04f * (60 - g->res[R_COHESION]));
    if (Game_ActiveThreats(g) < 6) AddRes(g, R_SOUVERAINETE, 0.5f);
    AddRes(g, R_INFLUENCE, 0.5f);

    /* 6. factions */
    float stress = g->temp - 2.0f;
    for (int f = 0; f < F_COUNT; f++) {
        AddFac(g, f, 0.3f + Randf(g) * 2.0f - 1.0f, 0.4f);
        if (g->fac[f].intelTurns > 0) g->fac[f].intelTurns--;
        g->fac[f].actedThisTurn = false;
    }
    AddFac(g, F_SABLES, 0.3f * stress, 0.6f * stress);

    /* 7. nouvelles menaces */
    float escalation = 1.0f + 0.6f * g->turn / (float)TOTAL_TURNS;
    for (int f = 0; f < F_COUNT; f++) {
        float h = g->fac[f].hostility / 100.0f, c = g->fac[f].capability / 100.0f;
        float expected = (0.2f + 1.5f * h * c) * escalation;
        int n = (int)expected;
        if (Randf(g) < expected - n) n++;
        for (int k = 0; k < n; k++) SpawnThreatFor(g, f);
    }

    g->commandPoints = BASE_CP;
    g->turn++;

    for (int r = 0; r < R_COUNT; r++) g->lastDelta[r] = g->res[r] - before[r];

    /* 8. fin de partie */
    if (g->res[R_COHESION] < 1)          g->state = GAME_LOST_COHESION;
    else if (g->res[R_SOUVERAINETE] < 1) g->state = GAME_LOST_SOUVERAINETE;
    else if (g->res[R_BUDGET] < -60)      g->state = GAME_LOST_BUDGET;
    else if (g->turn >= TOTAL_TURNS)      g->state = GAME_WON;

    if (g->state != GAME_RUNNING) return;

    char date[32];
    Game_DateString(g->turn, date, sizeof(date));
    Game_Log(g, EV_INFO, "--- %s : %d menaces actives, +%.2f°C ---", date, Game_ActiveThreats(g), g->temp);

    /* 9. dilemme */
    if (g->turn >= 2 && Randf(g) < 0.3f) {
        int avail[16], n = 0;
        for (int d = 0; d < DILEMMA_COUNT; d++) if (!g->dilemmaUsed[d]) avail[n++] = d;
        if (n > 0) {
            int d = avail[Rand(g) % (unsigned)n];
            g->dilemmaUsed[d] = true;
            g->pendingDilemma = d;
        }
    }
}

int Game_Score(const Game *g)
{
    float s = g->res[R_COHESION] * 2 + g->res[R_SOUVERAINETE] * 2 + g->res[R_INFLUENCE]
            + g->res[R_ENERGIE] + g->res[R_EAU] + g->res[R_ALIM]
            + g->res[R_BUDGET] * 0.5f + g->statNeutralized * 3 + g->turn * 10;
    return s < 0 ? 0 : (int)s;
}
