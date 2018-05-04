#include "sig.h"

void register_handler(void (*handler)(int)) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    ///Agregar algún otro?
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, 0);
}
///TODO: Agregar sig_handler a [ main, broker, server, worker ]
:"@