//
// Created by martin on 28/04/18.
//
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <sys/wait.h>
#include "../common/constants.h"
#include "../common/message.h"
extern "C" {
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/socket.h"
#include "../common/log/log.h"
}

int devolverMensajeRecibido(int q_storedmsg, struct msg_t* m, int user_id);
void requester(int q_req, int q_rep, int q_storedmsg, int sfd);
void replier(int q_rep, int q_storedmsg, int sfd);

int main(int argc, char* argv[]) {
    // Conecto con el servidor, obtengo el socket file descriptor
    int sfd = create_client_socket(IP_SERVER, PUERTO_SERVER);
    if (sfd < 0) {
        log_error("broker: Error al crear socket cliente. Freno");
        exit(-1);
    }
    // Creo las colas
    int q_req = qcreate(LOCAL_REQ_Q_ID);
    int q_rep = qcreate(LOCAL_REP_Q_ID);
    int q_storedmsg = qcreate(STORED_MESSAGES_ID);
    if (q_req < 0 || q_rep < 0 || q_storedmsg < 0) {
        log_error("broker: Error al crear msg queue. Freno");
        exit(-1);
    }
    // Creo map para ids locales-globales
    std::map<std::string, int> mapDeIds; /// TODO: Agregar en memoria compartida

    // Corro procesos
    if (fork() == 0) {
        requester(q_req, q_rep, q_storedmsg, sfd);
    } else if (fork() == 0) {
        replier(q_rep, q_storedmsg, sfd);

    // Espero a que terminen y cierro los recursos
    } else {
        while ( wait(NULL) > 0 );
        close(sfd);
        qdel(q_req);
        qdel(q_rep);
        qdel(q_storedmsg);
    }
    return 0;
    ///TODO: Handler de SIGINT que cierre conexión y queues
    ///TODO: Me falta chequear fork()<0 (fail). En todos lados.
    ///TODO: handler de reqs de recv que responda enviando a q_rep el primer anuncio de dicho id? O lo dejo así?
    ///Comenzar user id desde algo distinto de 1, como 101
}

void requester(int q_req, int q_rep, int q_storedmsg, int sfd) { // No sé si me encanta esta solución
    struct msg_t m;

    while (true) {
        log_debug("broker-requester: Espero próximo mensaje en q_req");//
        if (qrecv(q_req, &m, sizeof(m), 0) < 0) {
            log_warn("broker-requester: Error al recibir un mensaje de q_req. Sigo intentando");
        } else if (m.type == RECV_MSG) {
            if (devolverMensajeRecibido(q_storedmsg, &m, m.id)) { ///O lo hago de otra manera? Señal al replier?
                m.mtype = m.id;
                qsend(q_rep, &m, sizeof(m));
            }
        } else {
            ///m.id = TODO: Cambiar por mappeo global, y a falta de él y si es CREATE_MSG por 0?
            // Envío mensaje al servidor por red
            if (send(sfd, &m, sizeof(m), 0) < 0) {
                log_error("broker-requester: Error al enviar mensaje al servidor");
            }
        }
    }
}

void replier(int q_rep, int q_storedmsg, int sfd) {
    struct msg_t m;

    while (true) {
        // Recibo mensaje del servidor por red
        log_debug("broker-replier: Espero próximo mensaje por red del servidor");//
        if (recv(sfd, &m, sizeof(m), 0) < 0) {
            log_error("broker-replier: Error al recibir mensaje del servidor");
        } else {
            m.show();//
            // Para que cada usuario reciba solo sus mensajes
            m.mtype = m.id;///TODO: Cambiar por id local

            switch (m.type) {
                case CREATE_MSG:    // Guarda id global para mappeo y devuelve id local al usuario
                    ///TODO: Guardar en map y cambiar id en m
                case SUB_MSG:
                case PUB_MSG:       // Reenvían el retorno al usuario
                    qsend(q_rep, &m, sizeof(m));
                    break;
                case RECV_MSG:      // Almaceno mensaje
                    qsend(q_storedmsg, &m, sizeof(m));
                    break;
                case DESTROY_MSG:   // Borra del mappeo y reenvía retorno al usuario
                    ///TODO: Borrar del mappeo
                    qsend(q_rep, &m, sizeof(m));
                    break;
                default:
                    log_error("Tipo de mensaje no esperado. Freno");
                    exit(-1);
            }
        }
    }
}


int devolverMensajeRecibido(int q_storedmsg, struct msg_t* m, int user_id) {
    if (qrecv_nowait(q_storedmsg, &m, sizeof(m), user_id) >= 0) {
        log_debug("broker-requester: Tengo mensaje recibido para devolverle al cliente:");//
        m->show();//
        return 0;
    } else if (errno == ENOMSG) {
        // No hay nuevos mensajes
        log_debug("broker-requester: No hay mensajes recibidos para devolverle al cliente:");//
        strcpy(m->topic, "");
        strcpy(m->msg, "");
        return 0;
    } else {
        log_warn("broker-requester: Error al pedir un mensaje de q_stored_msg. Sigo");
        return -1;
    }
}
