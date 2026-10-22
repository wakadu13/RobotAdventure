/*
 * BOUCLIER 2050 - test du jeu en reseau sur la boucle locale (POSIX).
 *
 * Deux processus : l'hote dirige la France, le client l'Allemagne. Le protocole
 * est celui du jeu (HELLO, START, commandes appliquees par l'hote puis
 * diffusees) ; apres 12 tours, les deux sommes de controle doivent etre
 * identiques.
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

static int RunHost(void)
{
    if (!Net_Host(PORT)) { printf("hote : %s\n", Net_Error()); return 1; }
    int client = -1, guard = 0;
    unsigned seed = 4242;
    while (g.turn < TURNS && guard++ < 200000) {
        NetMsg in[32];
        int n = Net_Poll(in, 32);
        for (int i = 0; i < n; i++) {
            NetMsg *m = &in[i];
            if (m->type == MSG_HELLO) {
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
                if (Cmd_Deserialize(m->cmd, &c) && c.player == N_ALLEMAGNE && Cmd_Apply(&g, &c)) Net_Send(m);
            }
        }
        if (client >= 0 && g.activePlayer == N_FRANCE && g.state == GAME_RUNNING) {
            if (g.pendingDilemma >= 0) HostDo((Command){ CMD_DILEMMA, N_FRANCE, 0, 0, 0, 0 });
            HostDo((Command){ CMD_END_TURN, N_FRANCE, 0, 0, 0, 0 });
        }
        Pause();
    }
    /* attendre la somme de controle du client */
    for (int k = 0; k < 20000; k++) {
        NetMsg in[8];
        int n = Net_Poll(in, 8);
        for (int i = 0; i < n; i++)
            if (in[i].type == MSG_SUM) {
                unsigned mine = Game_Checksum(&g);
                printf("tour %d : hote %08x, client %08x -> %s\n", g.turn, mine, in[i].u, mine == in[i].u ? "identiques" : "DIVERGENCE");
                Net_Close();
                return mine == in[i].u && g.turn >= TURNS ? 0 : 1;
            }
        Pause();
    }
    printf("hote : pas de somme de controle recue (tour %d)\n", g.turn);
    return 1;
}

static int RunClient(void)
{
    bool ok = false;
    for (int k = 0; k < 200 && !(ok = Net_Join("127.0.0.1", PORT)); k++) Pause();
    if (!ok) { printf("client : %s\n", Net_Error()); return 1; }
    NetMsg h = { 0 };
    h.type = MSG_HELLO;
    h.a = NET_VERSION;
    h.b = N_ALLEMAGNE;
    Net_Send(&h);
    bool started = false, sentPhase = false;
    for (int guard = 0; guard < 200000; guard++) {
        NetMsg in[32];
        int n = Net_Poll(in, 32);
        for (int i = 0; i < n; i++) {
            NetMsg *m = &in[i];
            if (m->type == MSG_START) { Game_Init(&g, m->u, m->a); started = true; }
            else if (m->type == MSG_CMD) {
                Command c;
                if (Cmd_Deserialize(m->cmd, &c)) Cmd_Apply(&g, &c);
                sentPhase = false;
            } else if (m->type == MSG_BYE) return 1;
        }
        if (started && g.activePlayer == N_ALLEMAGNE && !sentPhase) {
            /* un recrutement de temps en temps, puis fin de phase */
            if (g.turn % 3 == 0) SendCmd((Command){ CMD_RECRUIT, N_ALLEMAGNE, U_INF, PV_DE_NRW, 0, 0 });
            SendCmd((Command){ CMD_END_TURN, N_ALLEMAGNE, 0, 0, 0, 0 });
            sentPhase = true;
        }
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
    pid_t pid = fork();
    if (pid == 0) return RunClient();
    int r = RunHost();
    int st = 0;
    waitpid(pid, &st, 0);
    bool ok = r == 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
    printf("%s\n", ok ? "TEST RESEAU OK" : "TEST RESEAU EN ECHEC");
    return ok ? 0 : 1;
}
