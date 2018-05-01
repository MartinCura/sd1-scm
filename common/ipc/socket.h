#ifndef SOCKET_H
#define SOCKET_H

int create_client_socket(const char* server_ip, int server_port);
int create_server_socket(int client_port);
int accept_client(int server_socket);

#endif //SOCKET_H
