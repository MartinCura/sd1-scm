#ifndef BROKER_SHM_H
#define BROKER_SHM_H

int   creashm(int key, size_t size);
int   getshm(int id);
void* mapshm(int id);
int   unmapshm(void* addr);
int   delshm(int id);

#endif //BROKER_SHM_H
