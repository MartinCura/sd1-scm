#ifndef BROKER_SIG_H
#define BROKER_SIG_H

#include <signal.h>

void register_handler(void (*handler)(int));

#endif //BROKER_SIG_H
