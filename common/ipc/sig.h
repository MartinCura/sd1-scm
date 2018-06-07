#ifndef BROKER_SIG_H
#define BROKER_SIG_H

#include <signal.h>

void register_SIGINT_handler(void (*handler)(int));
void register_sig_handler(void (*handler)(int), int signum);

#endif //BROKER_SIG_H
