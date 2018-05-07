#ifndef BROKER_MSG_QUEUE_H
#define BROKER_MSG_QUEUE_H

#include <sys/msg.h>

int qcreate(int id);

int qget(int id);

void qsend(int id, const void *msgp, size_t msgsz);

ssize_t qrecv(int id, void *msgp, size_t msgsz, long type);
ssize_t qrecv_nowait(int id, void *msgp, size_t msgsz, long mtype);

int qdel(int id);

#endif //BROKER_MSG_QUEUE_H
