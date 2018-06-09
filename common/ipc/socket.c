#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <ifaddrs.h>
#include <string.h>

#include "socket.h"
#include "../log/log.h"

int create_client_socket(const char* server_ip, uint16_t server_port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_error("Error creating client socket.");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    struct hostent* server = gethostbyname(server_ip) ;
    if (!server) {
        log_error("Error calling gethostbyname.");
        return -1;
    }
    bcopy(server->h_addr, &(server_addr.sin_addr.s_addr), (size_t) server->h_length);

    if ( connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) ) {
        log_error("Error calling connect.");
        return -1;
    }

    return socket_fd;
}

int create_server_socket(uint16_t client_port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_error("Error creating server socket.");
        return -1;
    }

    // Reutilizar dirección aunque esté en el estado TIME_WAIT
    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuse, sizeof(reuse)) < 0) {
        log_error("setsockopt(SO_REUSEADDR) failed");
    }

    struct sockaddr_in myaddr;
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(client_port);
    myaddr.sin_addr.s_addr = INADDR_ANY;

    socklen_t myaddr_size = sizeof(myaddr);

    if (bind(socket_fd, (struct sockaddr*) &myaddr, myaddr_size) < 0) {
        log_error("Error calling bind.");
        return -1;
    }

    if (listen(socket_fd, 10) < 0) {
        log_error("Error calling listen.");
        return -1;
    }

    return socket_fd;
}

int accept_client(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    int client_fd = accept(server_socket, (struct sockaddr*) &client_addr, &client_addr_size);
    if (client_fd < 0 && errno != EINTR) {
        log_error("Error accepting client.");
    }

    return client_fd;
}

/* Obtiene dirección IP propia, wlan# si está, si no eth#
 * Si no encuentra, no modifica ip_addr y devuelve -1 */
int obtener_ip_propia(char *ip_addr) {
    struct ifaddrs *ifaddr, *ifa;
    int n, s = 0;
    char e_host[NI_MAXHOST] = "", w_host[NI_MAXHOST] = "";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (strncmp(ifa->ifa_name, "eth", 3) == 0) {
                s = getnameinfo(ifa->ifa_addr,
                                sizeof(struct sockaddr_in),
                                e_host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);
            } else if (strncmp(ifa->ifa_name, "wlan", 4) == 0) {
                s = getnameinfo(ifa->ifa_addr,
                                sizeof(struct sockaddr_in),
                                w_host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);
            }
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                return -1;
            }
        }
    }
    freeifaddrs(ifaddr);

    if (strlen(w_host) > 0) {
        strcpy(ip_addr, w_host);
    } else if (strlen(e_host) > 0) {
        strcpy(ip_addr, e_host);
    } else {
        return -1;
    }
    return 0;
}
