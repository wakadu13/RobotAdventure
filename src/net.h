/*
 * BOUCLIER 2050 - reseau : TCP non bloquant, trames de 24 octets.
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
#define NET_VERSION   2
#define NET_MAX_PEERS 3
#define NET_FRAME     24

typedef enum {
    MSG_HELLO,     /* client -> hote : a = version, b = nation souhaitee */
    MSG_ASSIGN,    /* hote -> client : a = nation attribuee (-1 : refuse), b = raison */
    MSG_START,     /* hote -> tous : u = graine, a = difficulte, b = scenario */
    MSG_CMD,       /* commande serialisee (12 octets) */
    MSG_SUM,       /* a = tour, u = somme de controle */
    MSG_DESYNC,    /* hote -> tous : a = tour de la desynchronisation */
    MSG_BYE,
    MSG_STATE,     /* hote -> client(s) : morceau de l'etat de la partie, u = position, a = longueur (12 au plus) */
    MSG_STATE_END, /* hote -> client(s) : u = taille totale ; l'etat recu remplace la partie */
    MSG_CHAT,      /* discussion : a = numero du morceau, b = nation de l'auteur, 12 octets de texte (0 final) */
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

/* transfert de l'etat d'une partie (fichier de sauvegarde) */
bool        Net_SendFile(int peer, const char *path);   /* peer < 0 : a tous */
bool        Net_ReceiveState(const NetMsg *m, const char *path);   /* vrai quand le fichier complet est ecrit */

/* discussion : texte decoupe en morceaux de 12 octets */
void        Net_SendChat(int peer, int nation, const char *text);   /* peer < 0 : a tous (client : a l'hote) */
bool        Net_ReceiveChat(const NetMsg *m, char *out, int cap);   /* vrai quand un message est complet */

#endif
