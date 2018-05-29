//
// Created by martin on 29/05/18.
//
#include <cstdlib>
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
#include "../common/constants.h"

int q_ringsend, q_ringrecv;
int sid = 0, sid_sig = -1;

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        log_error("ring_node: Cantidad de argumentos incorrecta. Freno");
        return -1;
    }

    sid = atoi(argv[1]);
    if (argc >= 3) {
        sid_sig = atoi(argv[2]);
    }

    q_ringsend = qget(SERVER_RINGSEND_Q_ID);
    q_ringrecv = qget(SERVER_RINGRECV_Q_ID);
    if (q_ringsend < 0 || q_ringrecv < 0) {
        log_error("ring_node: Error al crear msg queue. Freno");
        exit(-1);
    }

    ///...

    ///Copiar receptor de conexiones del server, el cual inicia ring_receiver
    ///También ring_sender

    return 0;
}
