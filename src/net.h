/*
 * BOUCLIER 2050 - reseau : TCP non bloquant, trames de 16 octets.
 *
 * Topologie en etoile : l'hote (la France) relaie les commandes des clients
 * (les commandants) a tous les autres. Chacun joue sa phase a son tour ; les
 * commandes diffusees sont appliquees dans le meme ordre partout (lockstep) et
 * une somme de controle est comparee a chaque tour. N'inclut pas raylib (les
 * en-tetes de Winsock entrent en conflit avec les siens).
 */
#ifndef NET_H
#define NET_H

#include <stdbool.h>

#define NET_PORT      27050
#define NET_VERSION   1
#define NET_MAX_PEERS 3
#define NET_FRAME     16

typedef enum {
    MSG_HELLO,     /* client -> hote : a = version, b = nation souhaitee */
    MSG_ASSIGN,    /* hote -> client : a = nation attribuee (-1 : refuse), b = raison */
    MSG_START,     /* hote -> tous : u = graine, a = difficulte, b = scenario */
    MSG_CMD,       /* commande serialisee (12 octets) */
    MSG_SUM,       /* a = tour, u = somme de controle */
    MSG_DESYNC,    /* hote -> tous : a = tour de la desynchronisation */
    MSG_BYE,
} NetMsgType;

typedef struct {
    unsigned char type;
    signed char peer;              /* emetteur (0 : hote, 1..3 : clients) ; rempli a la reception */
    short a, b;
    unsigned u;
    unsigned char cmd[12];         /* MSG_CMD */
} NetMsg;

bool        Net_Host(int port);
bool        Net_Join(const char *host, int port);
void        Net_Close(void);
bool        Net_Active(void);
bool        Net_IsHost(void);
int         Net_PeerCount(void);               /* clients connectes (hote) */
bool        Net_PeerConnected(int peer);
int         Net_Poll(NetMsg *out, int max);    /* accepte les connexions et lit les trames recues */
bool        Net_Send(const NetMsg *m);         /* client : vers l'hote ; hote : vers tous les clients */
bool        Net_SendTo(int peer, const NetMsg *m);
bool        Net_Relay(const NetMsg *m, int except);   /* hote : vers tous les clients sauf un */
const char *Net_Error(void);

#endif
