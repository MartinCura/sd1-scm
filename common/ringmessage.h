//
// Created by martin on 29/05/18.
//

#ifndef SD1_SCM_RINGMESSAGE_H
#define SD1_SCM_RINGMESSAGE_H

#include <iostream>
#include "constants.h"

// Type
#define NEWCONN 1   // Nueva conexión de nodo que quiere integrarse al ring
#define RETCONN 2   // Falló algo y busca reconectarse
#define PUBLISH 3   // Distribución de mensaje por el ring
#define SERVOFF 4   // Se desconectará y apagará tras volver a recibirlo o timeout
#define SHUTDWN 5   // Apagar todos los servers

#define UNFILLED_SID -2
#define CLOSE_RING_GAP -3

typedef struct ringmsg_t {
    long mtype = 1;
    int  type;                      // Tipo de ringmsg, de la lista anterior
    int  sid_orig = -2;             // Originador del mensaje
    char topic[MAX_TOPIC_LENGTH] = "";
    char content[MAX_MSG_LENGTH] = "";

    void show() {
        std::cout << "rmsg> type=" << type
                  << ", id_orig=" << sid_orig
                  << ", topic=" << topic
                  << ", content=" << content << std::endl;
    }
} ringmsg_t;

#endif //SD1_SCM_RINGMESSAGE_H
