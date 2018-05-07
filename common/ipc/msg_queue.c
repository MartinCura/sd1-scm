#include <errno.h>
#include "msg_queue.h"
#include "resources.h"
#include "../log/log.h"

// Crea cola de mensajes inexistente
int qcreate(int id) {
    key_t clave = ftok(DIRECTORY, id);
    if (clave < 0) {
        log_error("Error on ftok while creating message queue");
        return -1;
    }
    /* da error si ya existe */
    int msg_id = msgget(clave,  IPC_CREAT | IPC_EXCL | 0660);
    if (msg_id < 0) {
        log_error("Error on msgget while creating message queue");
    }
    return msg_id;
}

// Obtiene cola de mensajes existente
int qget(int id) {
    key_t clave;
    clave = ftok(DIRECTORY, id);
    if (clave < 0) {
        log_error("Error on ftok while getting message queue");
        return -1;
    }
    int msg_id = msgget(clave, 0660);
    if (msg_id < 0) {
        log_error("Error on msgget while getting message queue");
    }
    return msg_id;
}

// Enviar un mensaje por la cola de mensajes
void qsend(int id, const void *msgp, size_t msgsz) {
    if (msgsnd(id,msgp,msgsz-sizeof(long),0) == -1) {
        log_error("Error sending message over queue");
    }
}

// Recibir un mensaje con tipo type por la cola de mensajes
ssize_t qrecv(int id, void *msgp, size_t msgsz, long mtype) {
    ssize_t res = msgrcv(id, msgp, msgsz-sizeof(long), mtype, 0);
    if (res < 0 && errno != EINTR) {
        log_error("Error receiving message over queue");
    }
    return res;
}

// Recibir un mensaje con tipo type por la cola de mensajes, o salir inmediatamente si no hay
ssize_t qrecv_nowait(int id, void *msgp, size_t msgsz, long mtype) {
    ssize_t res = msgrcv(id, msgp, msgsz-sizeof(long), mtype, IPC_NOWAIT);
    if (res < 0 && errno != EINTR && errno != ENOMSG) {
        log_error("Error receiving message over queue");
    }
    return res;
}

// Eliminar la cola de id id
int qdel(int id) {
    int res = msgctl(id, IPC_RMID, NULL);
    if (res < 0) {
        log_error("Error deleting message queue");
    }
    return res;
}
