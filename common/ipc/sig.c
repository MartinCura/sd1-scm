#include "sig.h"

void register_SIGINT_handler(void (*handler)(int)) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, 0);
}

void register_sig_handler(void (*handler)(int), int signum) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, signum);
    sa.sa_handler = handler;
    sigaction(signum, &sa, 0);
}
