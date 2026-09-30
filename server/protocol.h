#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_MSG 1024
#define MAX_FIELD 256
#define MAX_PAYLOAD 512

/* Tipos de mensaje del protocolo SRMP (ver docs/fase1.md) */
#define MSG_REG_NODO        "REG_NODO"
#define MSG_ESTADO          "ESTADO"
#define MSG_EVENTO          "EVENTO"
#define MSG_LOGIN           "LOGIN"
#define MSG_CONSULTA_ACTUAL "CONSULTA_ACTUAL"
#define MSG_CONSULTA_HIST   "CONSULTA_HIST"
#define MSG_ACK             "ACK"
#define MSG_RESP_ESTADO     "RESP_ESTADO"
#define MSG_RESP_HIST       "RESP_HIST"
#define MSG_ERROR           "ERROR"

/* Formato: TIPO|ID_ORIGEN|ID_MENSAJE|TIMESTAMP|LONGITUD_PAYLOAD|PAYLOAD */
typedef struct {
    char tipo[MAX_FIELD];
    char id_origen[MAX_FIELD];
    char id_mensaje[MAX_FIELD];
    char timestamp[MAX_FIELD];
    int  longitud;
    char payload[MAX_PAYLOAD];
} SrmpMsg;

#endif
