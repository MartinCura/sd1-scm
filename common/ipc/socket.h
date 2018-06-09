#ifndef SOCKET_H
#define SOCKET_H

int create_client_socket(const char *server_ip, uint16_t server_port);
int create_server_socket(uint16_t client_port);
int accept_client(int server_socket);
int obtener_ip_propia(char *ip_addr);

#endif //SOCKET_H
