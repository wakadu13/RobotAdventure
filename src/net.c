/*
 * BOUCLIER 2050 - reseau : sockets TCP non bloquants (POSIX et Winsock).
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L   /* getaddrinfo malgre -std=c99 */
#endif
#include "net.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define CLOSESOCK closesocket
#define WOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define CLOSESOCK close
#define WOULDBLOCK (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
#endif

typedef struct {
    sock_t s;
    unsigned char in[NET_FRAME * 64];
    int inLen;
} Conn;

static bool  active, host, started;
static sock_t listener = BAD_SOCK;
static Conn  peers[NET_MAX_PEERS + 1];   /* hote : 1..3 clients ; client : 0 = l'hote */
static char  error[128];

const char *Net_Error(void) { return error; }
bool Net_Active(void) { return active; }
bool Net_IsHost(void) { return active && host; }

static void SetNonBlocking(sock_t s)
{
#if defined(_WIN32)
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
}

static bool Startup(void)
{
    if (started) return true;
#if defined(_WIN32)
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { snprintf(error, sizeof(error), "Winsock indisponible"); return false; }
#endif
    for (int i = 0; i <= NET_MAX_PEERS; i++) peers[i].s = BAD_SOCK;
    started = true;
    return true;
}

bool Net_Host(int port)
{
    Net_Close();
    if (!Startup()) return false;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == BAD_SOCK) { snprintf(error, sizeof(error), "Impossible de créer le socket"); return false; }
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((unsigned short)port);
    if (bind(listener, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(listener, 4) != 0) {
        snprintf(error, sizeof(error), "Port %d indisponible", port);
        CLOSESOCK(listener);
        listener = BAD_SOCK;
        return false;
    }
    SetNonBlocking(listener);
    active = host = true;
    error[0] = 0;
    return true;
}

bool Net_Join(const char *name, int port)
{
    Net_Close();
    if (!Startup()) return false;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char p[16];
    snprintf(p, sizeof(p), "%d", port);
    if (getaddrinfo(name, p, &hints, &res) != 0 || !res) { snprintf(error, sizeof(error), "Adresse inconnue : %s", name); return false; }
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    /* connexion bloquante (courte) puis passage en non bloquant */
    int r = connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    if (r != 0) { snprintf(error, sizeof(error), "Connexion refusée par %s", name); CLOSESOCK(s); return false; }
    SetNonBlocking(s);
    peers[0].s = s;
    peers[0].inLen = 0;
    active = true;
    host = false;
    error[0] = 0;
    return true;
}

void Net_Close(void)
{
    if (!started) return;
    NetMsg bye;
    memset(&bye, 0, sizeof(bye));
    bye.type = MSG_BYE;
    if (active) Net_Send(&bye);
    for (int i = 0; i <= NET_MAX_PEERS; i++)
        if (peers[i].s != BAD_SOCK) { CLOSESOCK(peers[i].s); peers[i].s = BAD_SOCK; }
    if (listener != BAD_SOCK) { CLOSESOCK(listener); listener = BAD_SOCK; }
    active = host = false;
}

int Net_PeerCount(void)
{
    int c = 0;
    for (int i = 1; i <= NET_MAX_PEERS; i++) if (peers[i].s != BAD_SOCK) c++;
    return c;
}

bool Net_PeerConnected(int peer) { return peer >= 0 && peer <= NET_MAX_PEERS && peers[peer].s != BAD_SOCK; }

/* trame : type, (emetteur), a, b, u puis 12 octets de donnees (commande, etat, discussion), petit-boutiste */
static void Encode(const NetMsg *m, unsigned char o[NET_FRAME])
{
    memset(o, 0, NET_FRAME);
    o[0] = m->type;
    o[1] = (unsigned char)m->peer;
    o[2] = (unsigned char)(m->a & 0xFF); o[3] = (unsigned char)((m->a >> 8) & 0xFF);
    o[4] = (unsigned char)(m->b & 0xFF); o[5] = (unsigned char)((m->b >> 8) & 0xFF);
    for (int k = 0; k < 4; k++) o[6 + k] = (unsigned char)((m->u >> (8 * k)) & 0xFF);
    memcpy(o + 10, m->cmd, 12);
}

static void Decode(const unsigned char i[NET_FRAME], NetMsg *m)
{
    memset(m, 0, sizeof(*m));
    m->type = i[0];
    m->peer = (signed char)i[1];
    m->a = (short)(i[2] | (i[3] << 8));
    m->b = (short)(i[4] | (i[5] << 8));
    for (int k = 0; k < 4; k++) m->u |= (unsigned)i[6 + k] << (8 * k);
    memcpy(m->cmd, i + 10, 12);
}

static bool SendRaw(int peer, const NetMsg *m)
{
    if (peers[peer].s == BAD_SOCK) return false;
    unsigned char buf[NET_FRAME];
    Encode(m, buf);
    int sent = 0;
    long spins = 0;
    while (sent < NET_FRAME) {
        int r = (int)send(peers[peer].s, (const char *)buf + sent, NET_FRAME - sent, 0);
        if (r > 0) { sent += r; continue; }
        if (r < 0 && WOULDBLOCK && ++spins < 1000000) continue;   /* tampon plein : on insiste (trames minuscules) */
        CLOSESOCK(peers[peer].s);
        peers[peer].s = BAD_SOCK;
        return false;
    }
    return true;
}

bool Net_SendTo(int peer, const NetMsg *m) { return active && SendRaw(peer, m); }

bool Net_Send(const NetMsg *m)
{
    if (!active) return false;
    if (!host) return SendRaw(0, m);
    bool ok = true;
    for (int i = 1; i <= NET_MAX_PEERS; i++) if (peers[i].s != BAD_SOCK) ok &= SendRaw(i, m);
    return ok;
}

bool Net_Relay(const NetMsg *m, int except)
{
    bool ok = true;
    for (int i = 1; i <= NET_MAX_PEERS; i++) if (i != except && peers[i].s != BAD_SOCK) ok &= SendRaw(i, m);
    return ok;
}

int Net_Poll(NetMsg *out, int max)
{
    if (!active) return 0;
    if (host && listener != BAD_SOCK) {
        sock_t s = accept(listener, NULL, NULL);
        if (s != BAD_SOCK) {
            int slot = -1;
            for (int i = 1; i <= NET_MAX_PEERS && slot < 0; i++) if (peers[i].s == BAD_SOCK) slot = i;
            if (slot < 0) CLOSESOCK(s);
            else { SetNonBlocking(s); peers[slot].s = s; peers[slot].inLen = 0; }
        }
    }
    int n = 0;
    for (int i = 0; i <= NET_MAX_PEERS && n < max; i++) {
        Conn *c = &peers[i];
        if (c->s == BAD_SOCK) continue;
        for (;;) {
            int room = (int)sizeof(c->in) - c->inLen;
            if (room <= 0) break;
            int r = (int)recv(c->s, (char *)c->in + c->inLen, room, 0);
            if (r > 0) { c->inLen += r; continue; }
            if (r < 0 && WOULDBLOCK) break;
            /* connexion fermee */
            CLOSESOCK(c->s);
            c->s = BAD_SOCK;
            if (n < max) { memset(&out[n], 0, sizeof(NetMsg)); out[n].type = MSG_BYE; out[n].peer = (signed char)i; n++; }
            break;
        }
        int off = 0;
        while (c->inLen - off >= NET_FRAME && n < max) {
            Decode(c->in + off, &out[n]);
            out[n].peer = (signed char)(host ? i : 0);
            off += NET_FRAME;
            n++;
        }
        memmove(c->in, c->in + off, (size_t)(c->inLen - off));
        c->inLen -= off;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Etat de la partie et discussion                                     */
/* ------------------------------------------------------------------ */
#include <stdlib.h>

#define STATE_MAX (4 << 20)

static bool SendMsg(int peer, const NetMsg *m) { return peer < 0 ? Net_Send(m) : Net_SendTo(peer, m); }

bool Net_SendFile(int peer, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    NetMsg m;
    unsigned off = 0;
    bool ok = true;
    for (;;) {
        memset(&m, 0, sizeof(m));
        m.type = MSG_STATE;
        size_t n = fread(m.cmd, 1, sizeof(m.cmd), f);
        if (n == 0) break;
        m.u = off;
        m.a = (short)n;
        ok &= SendMsg(peer, &m);
        off += (unsigned)n;
    }
    fclose(f);
    memset(&m, 0, sizeof(m));
    m.type = MSG_STATE_END;
    m.u = off;
    return SendMsg(peer, &m) && ok;
}

static unsigned char *stateBuf;
static unsigned stateLen;

bool Net_ReceiveState(const NetMsg *m, const char *path)
{
    if (!stateBuf) stateBuf = malloc(STATE_MAX);
    if (!stateBuf) return false;
    if (m->type == MSG_STATE) {
        if (m->a <= 0 || m->a > 12 || m->u + (unsigned)m->a > STATE_MAX) return false;
        memcpy(stateBuf + m->u, m->cmd, (size_t)m->a);
        if (m->u + (unsigned)m->a > stateLen) stateLen = m->u + (unsigned)m->a;
        return false;
    }
    if (m->type != MSG_STATE_END) return false;
    bool ok = m->u == stateLen && stateLen > 0;
    stateLen = 0;
    if (!ok) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    ok = fwrite(stateBuf, 1, m->u, f) == m->u;
    fclose(f);
    return ok;
}

void Net_SendChat(int peer, int nation, const char *text)
{
    int len = (int)strlen(text);
    for (int k = 0; k * 12 <= len && k < 16; k++) {   /* le dernier morceau contient le 0 final */
        NetMsg m;
        memset(&m, 0, sizeof(m));
        m.type = MSG_CHAT;
        m.a = (short)k;
        m.b = (short)nation;
        int n = len + 1 - k * 12;
        memcpy(m.cmd, text + k * 12, (size_t)(n > 12 ? 12 : n));
        SendMsg(peer, &m);
    }
}

bool Net_ReceiveChat(const NetMsg *m, char *out, int cap)
{
    static char buf[NET_MAX_PEERS + 1][16 * 12 + 1];
    if (m->type != MSG_CHAT || m->a < 0 || m->a >= 16 || m->peer < 0 || m->peer > NET_MAX_PEERS) return false;
    char *b = buf[(int)m->peer];
    memcpy(b + m->a * 12, m->cmd, 12);
    if (!memchr(m->cmd, 0, 12)) return false;
    b[16 * 12] = 0;
    snprintf(out, (size_t)cap, "%s", b);
    return true;
}
