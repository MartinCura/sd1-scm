//
// Created by martin on 27/04/18.
//

#ifndef SD1_SCM_MESSAGE_H
#define SD1_SCM_MESSAGE_H

#include "constants.h"

// Decisión editorial: el tipo de msg enviado por el server es el mismo al que responde.
// Ej.: Se recibe un PUB_MSG por haber enviado el mismo; server envía RECV para que el cliente lo pueda obtener.
#define CREATE_MSG  1
#define SUB_MSG     2
#define PUB_MSG     3
#define RECV_MSG    4
#define DESTROY_MSG 5

typedef struct msg_t {
    long mtype = 1;
    int  type;
    int  id;
    char topic[MAX_TOPIC_LENGTH] = "";
    char msg  [MAX_MSG_LENGTH] = "";

    void show() {
        std::cout << "msg> type=" << type
                       << ", id=" << id
                    << ", topic=" << topic
                  << ", content=" << msg << std::endl;
    }
} msg_t;

#endif //SD1_SCM_MESSAGE_H
