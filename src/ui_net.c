/*
 * BOUCLIER 2050 - partie en reseau : salon, diffusion des commandes, controle
 * de synchronisation.
 *
 * L'hote dirige la France, chaque client une autre nation (comme en
 * hot-seat). Une commande d'un client est envoyee a l'hote, qui l'applique puis
 * la diffuse a tous (l'emetteur compris) : toutes les machines appliquent les
 * memes commandes dans le meme ordre. A chaque nouveau tour, les clients
 * envoient leur somme de controle a l'hote, qui signale toute divergence.
 */
#include "ui.h"
#include "net.h"

bool netPlaying;
int  localNation = N_FRANCE;
static int  lobbyNation[NET_MAX_PEERS + 1] = { -1, -1, -1, -1 };
static int  wantNation = N_ALLEMAGNE;
static char joinAddr[64] = "127.0.0.1";
static bool editAddr;
static char netStatus[160];
static unsigned sums[HISTORY_LEN + 2];
static int  lastTurn = -1;
static int  desyncTurn = -1;

static void Status(const char *s) { snprintf(netStatus, sizeof(netStatus), "%s", s); }

void NetLeave(void)
{
    Net_Close();
    netPlaying = false;
    localNation = N_FRANCE;
    for (int i = 0; i <= NET_MAX_PEERS; i++) lobbyNation[i] = -1;
    desyncTurn = -1;
}

/* une commande du joueur local : l'hote l'applique et la diffuse ; un client l'envoie a l'hote */
bool NetDo(const Command *cmd)
{
    NetMsg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_CMD;
    Cmd_Serialize(cmd, m.cmd);
    if (!Net_IsHost()) return Net_Send(&m);
    if (!Cmd_Apply(&game, cmd)) return false;
    Replay_Record(&session, cmd);
    Net_Send(&m);
    return true;
}

static void TurnCheck(void)
{
    if (game.turn == lastTurn) return;
    lastTurn = game.turn;
    unsigned s = Game_Checksum(&game);
    if (game.turn < HISTORY_LEN + 2) sums[game.turn] = s;
    if (!Net_IsHost()) {
        NetMsg m;
        memset(&m, 0, sizeof(m));
        m.type = MSG_SUM;
        m.a = (short)game.turn;
        m.u = s;
        Net_Send(&m);
    }
}

static void StartFromHost(unsigned seed, int diff)
{
    Game_Init(&game, seed, diff);
    Replay_Clear(&session);
    session.seed = seed;
    session.difficulty = diff;
    ResetUi();
    netPlaying = true;
    lastTurn = -1;
    handoffFor = -1;
}

void NetUpdate(void)
{
    if (!Net_Active()) return;
    NetMsg in[64];
    int n = Net_Poll(in, 64);
    for (int i = 0; i < n; i++) {
        NetMsg *m = &in[i];
        int p = m->peer;
        switch (m->type) {
        case MSG_HELLO:
            if (!Net_IsHost()) break;
            {
                NetMsg r;
                memset(&r, 0, sizeof(r));
                r.type = MSG_ASSIGN;
                bool taken = m->b <= 0 || m->b >= N_COUNT;
                for (int k = 1; k <= NET_MAX_PEERS; k++) if (k != p && lobbyNation[k] == m->b) taken = true;
                if (m->a != NET_VERSION) { r.a = -1; r.b = 1; }
                else if (netPlaying || taken) { r.a = -1; r.b = 2; }
                else { r.a = m->b; lobbyNation[p] = m->b; }
                Net_SendTo(p, &r);
            }
            break;
        case MSG_ASSIGN:
            if (m->a < 0) {
                Status(m->b == 1 ? "Version différente de celle de l'hôte." : "Nation déjà prise ou partie commencée.");
                Net_Close();
            } else {
                localNation = m->a;
                Status("Connecté. En attente du lancement par l'hôte.");
            }
            break;
        case MSG_START:
            if (Net_IsHost()) break;
            StartFromHost(m->u, m->a);
            /* la camera s'ouvre sur la nation du joueur */
            for (int q = 0; q < PROV_COUNT; q++) if (game.prov[q].owner == localNation) { selProvince = q; break; }
            FocusProvince(selProvince, 2);
            break;
        case MSG_CMD: {
            Command c;
            if (!Cmd_Deserialize(m->cmd, &c)) break;
            if (Net_IsHost()) {
                /* commande d'un client : seulement pour sa propre nation */
                if (c.player != lobbyNation[p]) break;
                if (!Cmd_Apply(&game, &c)) break;
                Replay_Record(&session, &c);
                Net_Send(m);   /* a tous, l'emetteur compris */
            } else {
                Cmd_Apply(&game, &c);
                Replay_Record(&session, &c);
            }
            if (c.type == CMD_DECLARE_WAR) Snd_Play(SFX_WAR);
        } break;
        case MSG_SUM:
            if (Net_IsHost() && m->a >= 0 && m->a < HISTORY_LEN + 2 && m->a <= lastTurn && sums[m->a] != m->u) {
                NetMsg d;
                memset(&d, 0, sizeof(d));
                d.type = MSG_DESYNC;
                d.a = m->a;
                Net_Send(&d);
                desyncTurn = m->a;
            }
            break;
        case MSG_DESYNC:
            desyncTurn = m->a;
            break;
        case MSG_BYE:
            if (Net_IsHost()) {
                int nat = lobbyNation[p];
                lobbyNation[p] = -1;
                if (netPlaying && nat > 0 && game.human[nat]) {
                    Command c = { CMD_DROP_PLAYER, N_FRANCE, (short)nat, 0, 0, 0 };
                    NetDo(&c);
                    Toast("Un joueur a quitté la partie : sa nation revient à l'IA.");
                }
            } else {
                NetLeave();
                Toast("L'hôte a quitté la partie.");
                screen = SCR_TITLE;
                inProgress = false;
            }
            break;
        default: break;
        }
    }
    if (netPlaying) {
        TurnCheck();
        War_UpdateIntelFor(&game, localNation);   /* chacun son brouillard de guerre */
    }
}

/* l'hote lance la partie : graine commune, puis scenario et joueurs diffuses en commandes */
void NetHostStart(void)
{
    unsigned seed = (unsigned)time(NULL);
    NetMsg s;
    memset(&s, 0, sizeof(s));
    s.type = MSG_START;
    s.u = seed;
    s.a = (short)newDifficulty;
    s.b = (short)newScenario;
    Net_Send(&s);
    StartFromHost(seed, newDifficulty);
    localNation = N_FRANCE;
    if (newScenario) { Command c = { CMD_SCENARIO, N_FRANCE, (short)newScenario, 0, 0, 0 }; NetDo(&c); }
    for (int k = 1; k <= NET_MAX_PEERS; k++)
        if (lobbyNation[k] > 0) { Command c = { CMD_ADD_PLAYER, N_FRANCE, (short)lobbyNation[k], 0, 0, 0 }; NetDo(&c); }
    tutorial = -1;
}

/* un client rejoint l'hote en demandant une nation */
bool NetJoin(const char *addr, int nation)
{
    if (!Net_Join(addr, NET_PORT)) { Status(Net_Error()); return false; }
    NetMsg h;
    memset(&h, 0, sizeof(h));
    h.type = MSG_HELLO;
    h.a = NET_VERSION;
    h.b = (short)nation;
    Net_Send(&h);
    Status("Connexion établie, en attente de l'hôte...");
    return true;
}

/* bandeau en jeu : phase en cours, desynchronisation */
void NetBanner(void)
{
    if (!netPlaying) return;
    char buf[160];
    if (desyncTurn >= 0) snprintf(buf, sizeof(buf), Tr("DÉSYNCHRONISATION au tour %d : les parties divergent, relancez."), desyncTurn);
    else if (game.activePlayer == localNation) snprintf(buf, sizeof(buf), "%s", Tr("Réseau : à vous de jouer."));
    else snprintf(buf, sizeof(buf), Tr("Réseau : phase de %s, en attente..."),
                  game.activePlayer == N_FRANCE ? Tr("la France") : Tr(NATIONS[game.activePlayer].name));
    int w = TextW(buf, 10);
    Rectangle r = { MAP_X + MAP_VW / 2 - w / 2 - 8, MAP_Y + 20, (float)w + 16, 16 };
    DrawRectangleRec(r, Fade((Color){ 10, 14, 20, 255 }, 0.9f));
    DrawRectangleLinesEx(r, 1, desyncTurn >= 0 ? C_BAD : game.activePlayer == localNation ? C_GOOD : C_WARN);
    Text(buf, r.x + 8, r.y + 3, 10, desyncTurn >= 0 ? C_BAD : C_TEXT);
}

/* salon : heberger ou rejoindre, choix de la nation */
void DrawNetLobby(void)
{
    DrawBackdrop();
    Rectangle r = { 180, 40, 920, 640 };
    Panel(r, C_PANEL);
    Text("PARTIE EN RÉSEAU", r.x + 20, r.y + 16, 20, C_ACCENT);
    TextWrap("L'hôte dirige la France ; jusqu'à trois joueurs le rejoignent, chacun avec une nation (armée, guerre, paix, "
             "alliances), comme en hot-seat. Chacun joue sa phase à son tour sur sa propre machine. Réseau local ou Internet "
             "(port TCP 27050 ouvert chez l'hôte). Tous les joueurs doivent avoir la même version du jeu.",
             r.x + 20, r.y + 44, r.width - 40, 10, C_DIM);
    char buf[200];
    float y = r.y + 96;
    if (!Net_Active()) {
        if (Button((Rectangle){ r.x + 20, y, 260, 36 }, "HÉBERGER", true, 20)) {
            if (Net_Host(NET_PORT)) Status("Partie hébergée : en attente de joueurs sur le port 27050.");
            else Status(Net_Error());
        }
        y += 52;
        Text("Rejoindre l'adresse :", r.x + 20, y + 8, 10, C_TEXT);
        Rectangle ar = { r.x + 180, y, 240, 28 };
        DrawRectangleRec(ar, editAddr ? C_PANEL2 : C_BG);
        DrawRectangleLinesEx(ar, 1, editAddr ? C_ACCENT : C_BORDER);
        Text(joinAddr, ar.x + 8, ar.y + 9, 10, C_TEXT);
        if (Hover(ar) && clicked) { editAddr = true; clicked = false; }
        if (editAddr) {
            int ch;
            while ((ch = GetCharPressed()) > 0) {
                int l = (int)strlen(joinAddr);
                if (ch >= 32 && ch < 127 && l < (int)sizeof(joinAddr) - 1) { joinAddr[l] = (char)ch; joinAddr[l + 1] = 0; }
            }
            if (IsKeyPressed(KEY_BACKSPACE) && joinAddr[0]) joinAddr[strlen(joinAddr) - 1] = 0;
            if (IsKeyPressed(KEY_ENTER)) editAddr = false;
        }
        if (Button((Rectangle){ r.x + 430, y, 160, 28 }, "REJOINDRE", true, 10)) {
            editAddr = false;
            NetJoin(joinAddr, wantNation);
        }
        y += 40;
        Text("Nation souhaitée (pour rejoindre) :", r.x + 20, y, 10, C_TEXT);
        y += 16;
        int col = 0, row = 0;
        for (int k = 1; k < N_COUNT; k++) {
            Rectangle b = { r.x + 20 + col * 222, y + row * 24, 214, 20 };
            if (Choice(b, Tr(NATIONS[k].name), wantNation == k)) wantNation = k;
            if (++col == 4) { col = 0; row++; }
        }
    } else if (Net_IsHost()) {
        Text("Joueurs connectés :", r.x + 20, y, 10, C_TEXT);
        y += 18;
        int ready = 0;
        for (int k = 1; k <= NET_MAX_PEERS; k++) {
            if (!Net_PeerConnected(k)) { lobbyNation[k] = -1; continue; }
            snprintf(buf, sizeof(buf), Tr("Joueur %d : %s"), k, lobbyNation[k] > 0 ? Tr(NATIONS[lobbyNation[k]].name) : Tr("en cours de connexion"));
            Text(buf, r.x + 30, y, 10, C_GOOD);
            y += 16;
            if (lobbyNation[k] > 0) ready++;
        }
        if (!ready) { Text("Aucun joueur pour l'instant.", r.x + 30, y, 10, C_DIM); y += 16; }
        snprintf(buf, sizeof(buf), Tr("Difficulté : %s - scénario : %s (réglages du menu)"), Tr(DIFFICULTY_NAMES[newDifficulty]),
                 Tr(SCENARIO_NAMES[newScenario]));
        Text(buf, r.x + 20, y + 10, 10, C_DIM);
        if (Button((Rectangle){ r.x + r.width - 300, r.y + r.height - 60, 280, 40 }, "LANCER LA PARTIE", ready > 0, 20)) NetHostStart();
    } else {
        snprintf(buf, sizeof(buf), Tr("Connecté à %s. Votre nation : %s."), joinAddr,
                 localNation > 0 ? Tr(NATIONS[localNation].name) : Tr("en attente de confirmation"));
        Text(buf, r.x + 20, y, 10, C_GOOD);
    }
    if (netStatus[0]) Text(netStatus, r.x + 20, r.y + r.height - 84, 10, C_SEL);
    if (Button((Rectangle){ r.x + 20, r.y + r.height - 60, 200, 40 }, "RETOUR", true, 20)) { NetLeave(); screen = SCR_TITLE; netStatus[0] = 0; }
}
