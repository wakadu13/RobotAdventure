/*
 * BOUCLIER 2050 - test du jeu en reseau sur la boucle locale (POSIX).
 *
 * Trois processus : l'hote dirige la France, un client l'Allemagne des le
 * depart, un second client l'Italie, qui arrive au tour 5 et recoit l'etat de la
 * partie (fichier de sauvegarde decoupe en trames). Le protocole est celui du
 * jeu (HELLO, START, commandes appliquees par l'hote puis diffusees, message
 * de discussion relaye) ; apres 12 tours, les trois sommes de controle doivent
 * etre identiques.
 */
#define _POSIX_C_SOURCE 200112L
#include "../src/game.h"
#include "../src/net.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT  27151
#define TURNS 12

static Game g;

static void Pause(void) { struct timespec t = { 0, 2000000 }; nanosleep(&t, NULL); }

static void SendCmd(Command c)
{
    NetMsg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_CMD;
    Cmd_Serialize(&c, m.cmd);
    Net_Send(&m);
}

/* l'hote applique puis diffuse */
static void HostDo(Command c)
{
    if (Cmd_Apply(&g, &c)) SendCmd(c);
}

#define STATE_FILE "build/net_test_etat.sav"

static int RunHost(void)
{
    if (!Net_Host(PORT)) { printf("hote : %s\n", Net_Error()); return 1; }
    int client = -1, guard = 0, late = -1;
    bool chatOk = false;
    unsigned seed = 4242;
    while (g.turn < TURNS && guard++ < 200000) {
        NetMsg in[32];
        int n = Net_Poll(in, 32);
        for (int i = 0; i < n; i++) {
            NetMsg *m = &in[i];
            if (m->type == MSG_HELLO && client >= 0) {
                /* arrivee en cours de partie : etat complet, puis prise de commandement */
                late = m->peer;
                NetMsg r = { 0 };
                r.type = MSG_ASSIGN;
                r.a = m->b;
                Net_SendTo(late, &r);
                if (!Game_Save(&g, STATE_FILE) || !Net_SendFile(late, STATE_FILE)) printf("hote : envoi de l'etat impossible\n");
                HostDo((Command){ CMD_JOIN_PLAYER, N_FRANCE, N_ITALIE, 0, 0, 0 });
                printf("hote : l'Italie arrive au tour %d\n", g.turn);
            } else if (m->type == MSG_CHAT) {
                char text[200];
                Net_Relay(m, m->peer);
                if (Net_ReceiveChat(m, text, sizeof(text))) chatOk = strcmp(text, "Bonjour, l'hôte !") == 0;
            } else if (m->type == MSG_HELLO) {
                client = m->peer;
                NetMsg r = { 0 };
                r.type = MSG_ASSIGN;
                r.a = m->b;
                Net_SendTo(client, &r);
                NetMsg s = { 0 };
                s.type = MSG_START;
                s.u = seed;
                s.a = DIFF_NORMAL;
                Net_Send(&s);
                Game_Init(&g, seed, DIFF_NORMAL);
                HostDo((Command){ CMD_ADD_PLAYER, N_FRANCE, N_ALLEMAGNE, 0, 0, 0 });
            } else if (m->type == MSG_CMD) {
                Command c;
                if (Cmd_Deserialize(m->cmd, &c) && (c.player == N_ALLEMAGNE || c.player == N_ITALIE) && Cmd_Apply(&g, &c)) Net_Send(m);
            }
        }
        if (client >= 0 && g.activePlayer == N_FRANCE && g.state == GAME_RUNNING) {
            if (g.pendingDilemma >= 0) HostDo((Command){ CMD_DILEMMA, N_FRANCE, 0, 0, 0, 0 });
            HostDo((Command){ CMD_END_TURN, N_FRANCE, 0, 0, 0, 0 });
        }
        Pause();
    }
    /* attendre les sommes de controle des deux clients */
    int got = 0, same = 0;
    unsigned mine = Game_Checksum(&g);
    for (int k = 0; k < 20000 && got < 2; k++) {
        NetMsg in[8];
        int n = Net_Poll(in, 8);
        for (int i = 0; i < n; i++)
            if (in[i].type == MSG_SUM) {
                printf("tour %d : hote %08x, client %d %08x -> %s\n", g.turn, mine, in[i].peer, in[i].u, mine == in[i].u ? "identiques" : "DIVERGENCE");
                got++;
                same += mine == in[i].u;
            }
        Pause();
    }
    printf("discussion relayee : %s, arrivee en cours de partie : %s\n", chatOk ? "oui" : "NON", late >= 0 ? "oui" : "NON");
    Net_Close();
    return got == 2 && same == 2 && chatOk && late >= 0 && g.turn >= TURNS ? 0 : 1;
}

static int RunClient(int nation, int joinTurn)
{
    if (joinTurn) { for (int k = 0; k < 300000; k++) { if (access("build/net_test_tour5", F_OK) == 0) break; Pause(); } }
    bool ok = false;
    for (int k = 0; k < 200 && !(ok = Net_Join("127.0.0.1", PORT)); k++) Pause();
    if (!ok) { printf("client : %s\n", Net_Error()); return 1; }
    NetMsg h = { 0 };
    h.type = MSG_HELLO;
    h.a = NET_VERSION;
    h.b = (short)nation;
    Net_Send(&h);
    bool started = false, sentPhase = false;
    for (int guard = 0; guard < 200000; guard++) {
        NetMsg in[32];
        int n = Net_Poll(in, 32);
        for (int i = 0; i < n; i++) {
            NetMsg *m = &in[i];
            if (m->type == MSG_START) { Game_Init(&g, m->u, m->a); started = true; }
            else if ((m->type == MSG_STATE || m->type == MSG_STATE_END) && Net_ReceiveState(m, "build/net_test_recu.sav")) {
                started = Game_Load(&g, "build/net_test_recu.sav");
                printf("client %d : etat recu au tour %d (%s)\n", nation, g.turn, started ? "lu" : "ILLISIBLE");
            } else if (m->type == MSG_CHAT) {
                char text[200];
                if (Net_ReceiveChat(m, text, sizeof(text))) printf("client %d : message recu : %s\n", nation, text);
            }
            else if (m->type == MSG_CMD) {
                Command c;
                if (Cmd_Deserialize(m->cmd, &c)) Cmd_Apply(&g, &c);
                sentPhase = false;
            } else if (m->type == MSG_BYE) return 1;
        }
        if (started && g.activePlayer == nation && !sentPhase) {
            /* un recrutement de temps en temps, puis fin de phase */
            if (g.turn % 3 == 0 && nation == N_ALLEMAGNE) SendCmd((Command){ CMD_RECRUIT, N_ALLEMAGNE, U_INF, PV_DE_NRW, 0, 0 });
            if (g.turn == 3 && nation == N_ALLEMAGNE) Net_SendChat(-1, nation, "Bonjour, l'hôte !");
            SendCmd((Command){ CMD_END_TURN, (short)nation, 0, 0, 0, 0 });
            sentPhase = true;
        }
        if (started && nation == N_ALLEMAGNE && g.turn >= 5) { FILE *f = fopen("build/net_test_tour5", "w"); if (f) fclose(f); }
        if (started && g.turn >= TURNS) {
            NetMsg s = { 0 };
            s.type = MSG_SUM;
            s.a = (short)g.turn;
            s.u = Game_Checksum(&g);
            Net_Send(&s);
            for (int k = 0; k < 200; k++) Pause();
            Net_Close();
            return 0;
        }
        Pause();
    }
    return 1;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    remove("build/net_test_tour5");
    pid_t pid = fork();
    if (pid == 0) return RunClient(N_ALLEMAGNE, 0);
    pid_t pid2 = fork();
    if (pid2 == 0) return RunClient(N_ITALIE, 5);
    int r = RunHost();
    int st = 0, st2 = 0;
    waitpid(pid, &st, 0);
    waitpid(pid2, &st2, 0);
    remove("build/net_test_tour5");
    bool ok = r == 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0 && WIFEXITED(st2) && WEXITSTATUS(st2) == 0;
    printf("%s\n", ok ? "TEST RESEAU OK" : "TEST RESEAU EN ECHEC");
    return ok ? 0 : 1;
}
