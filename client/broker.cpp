//
// Created by martin on 28/04/18.
//
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sstream>
#include "../common/constants.h"
#include "../common/message.h"
extern "C" {
#include "../common/ipc/semaphore.h"
#include "../common/ipc/shm.h"
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/socket.h"
#include "../common/ipc/sig.h"
#include "../common/log/log.h"
}

bool sig_quit = false;

int devolverMensajeRecibido(int q_storedmsg, struct msg_t* m, int user_id);
void requester(int* ids_p, int q_req, int q_rep, int q_storedmsg, int sfd);
void replier(int *ids_p, int q_rep, int q_storedmsg, int sfd);
void SIGINT_handler(int signum);

int main(int argc, char* argv[]) {
    /* argc = { ./local-broker [-p <IP server>] [<sid>] } */
    if (argc > 4) {
        log_error("broker: Demasiados argumentos. Freno");
        return -1;
    } else if (argc >= 2 &&
               (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        std::ostringstream oss;
        oss << "Usage: " << argv[0] << " [server id]";
        log_info(oss.str().c_str());
        return 0;
    }
    int sid = 0;
    char ip_serv[16] = IP_SERVER_DEFAULT;
    log_info("broker: Comienzo");

    // Si enviados por argumento, recibo IP y/o id del server al que se conecta;
    // si no hay ring será sid = 0 por default
    if (argc == 2) {
        sid = atoi(argv[1]);
    } else if (argc >= 3) {
        if (strcmp(argv[1], "-p") == 0) {
            strncpy(ip_serv, argv[2], 16);
            sid = atoi(argv[3]);
        } else {
            sid = atoi(argv[1]);
            if (argc >= 4 && strcmp(argv[2], "-p") == 0) {
                strncpy(ip_serv, argv[3], 16);
            }
        }
    }
    if (sid < 0 || sid > MAX_SID) {
        log_error("broker: sid inválido. Freno");
        return -2;
    }
    if (argc >= 2) {
        // Cambio al directorio del server
        std::string folder = "./s" + std::to_string(sid) + "/";
        chdir(folder.c_str());
    }

    register_SIGINT_handler(SIGINT_handler);

    // Conecto con el servidor, obtengo el socket file descriptor
    uint16_t puerto_server = (uint16_t) (PUERTO_SERVER + sid);
    int sfd = create_client_socket(ip_serv, puerto_server);
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
        close(sfd);
        exit(-1);
    }
    // Creo shm para ids globales y próximo id, y sem para acceder
    int ids_sem = creasem(LOCAL_IDS_SEM_ID);
    inisem(ids_sem, 1);
    int ids_shm = creashm(LOCAL_IDS_SHM_ID, LOCAL_MAX_ID * sizeof(int));
    int* ids_p = (int*) mapshm(ids_shm);
    ids_p[0] = LOCAL_FIRST_ID;
    for (int i = 1; i < LOCAL_MAX_ID; ++i) {
        ids_p[i] = 0;
    }

    // Corro procesos
    pid_t p_req;
    p_req = fork();
    if (p_req == 0) {
        requester(ids_p, q_req, q_rep, q_storedmsg, sfd);
        unmapshm(ids_p);
    } else {
        pid_t p_rep = fork();
        if (p_rep == 0) {
            replier(ids_p, q_rep, q_storedmsg, sfd);
            unmapshm(ids_p);

        // Espero a que terminen y cierro los recursos
        } else {
            while (!sig_quit);  //Alternativa que espera a que cierren los hijos: while ( wait(NULL) > 0 );
            close(sfd);
            qdel(q_req);
            qdel(q_rep);
            qdel(q_storedmsg);
            unmapshm(ids_p);
            log_debug("broker: Cerré conexión, colas, y shm; esperaré hijos");//
            // Espero a que terminen todos los procesos hijos
            while (wait(NULL) > 0);
            delshm(ids_shm);
            delsem(ids_sem);
            log_info("broker: TERMINO");
        }
    }
    return 0;
}

void requester(int* ids_p, int q_req, int q_rep, int q_storedmsg, int sfd) { // No sé si me encanta esta solución
    int ids_sem = getsem(LOCAL_IDS_SEM_ID);
    struct msg_t m;
    int mapped_id;

    while (!sig_quit) {
        log_debug("broker-requester: Espero próximo mensaje en q_req...");//
        if (qrecv(q_req, &m, sizeof(m), 0) < 0) {
            if (errno == EINVAL || errno == EIDRM || sig_quit) break;
            log_warn("broker-requester: Error al recibir un mensaje de q_req. Sigo intentando");
        } else {
            log_debug("broker-requester: Recibí mensaje por cola:");//
            m.show();//

            if (m.type != CREATE_MSG) { // Estos no tendrían id mappeado todavía
                if (m.id < 0 || m.id >= LOCAL_MAX_ID) {
                    log_error("broker-requester: id inválido, notifico");
                    strncpy(m.msg, "Id inválido", MAX_MSG_LENGTH);
                    qsend(q_rep, &m, sizeof(m));
                    continue;
                }
                p(ids_sem); {
                    // Cambio id local por global; a falta de él, 0 (si no existe) o -1 (si fue borrado)
                    mapped_id = ids_p[m.id];
                } v(ids_sem);
                if (mapped_id <= 0) {
                    log_info("broker-requester: id no existe o fue destruido, notifico");
                    m.id = -1 * m.id;
                    if (mapped_id == 0)
                        strncpy(m.msg, "Id no existe", MAX_MSG_LENGTH);
                    else if (mapped_id == -1)
                        strncpy(m.msg, "Id fue destruido", MAX_MSG_LENGTH);
                    qsend(q_rep, &m, sizeof(m));
                    continue;
                }
                if (m.type != RECV_MSG) {   // En el caso de RECV_MSG solo revisé que existe
                    m.id = mapped_id;
                }
            }

            if (m.type == RECV_MSG) {
                if (devolverMensajeRecibido(q_storedmsg, &m, m.id) == 0) {
                    m.show();//
                    qsend(q_rep, &m, sizeof(m));
                }
            } else {
                // Envío mensaje al servidor por red
                if (send(sfd, &m, sizeof(m), 0) < 0) {
                    perror("broker-requester");
                    log_error("broker-requester: Error al enviar mensaje al servidor");
                }
            }
        }
    }
    log_debug("broker-requester: Termino");//
}

void replier(int *ids_p, int q_rep, int q_storedmsg, int sfd) {
    int ids_sem = getsem(LOCAL_IDS_SEM_ID);
    struct msg_t m;

    while (!sig_quit) {
        // Recibo mensaje del servidor por red
        log_debug("broker-replier: Espero próximo mensaje por red del servidor...");//
        ssize_t r = recv(sfd, &m, sizeof(m), 0);
        if (r <= 0) {
            if (r == 0 || sig_quit) break;
            perror("broker-replier");
            log_error("broker-replier: Error al recibir mensaje del servidor");
        } else {
            log_debug("broker-replier: Recibí por red un mensaje:");//
            m.show();//

            // Recambio id global por id local
            // También lo meto en mtype para que cada usuario reciba solo sus mensajes
            if (m.type != CREATE_MSG) {
                p(ids_sem); {
                    int signo = (m.id < 0) ? -1 : 1;    // Mantengo señal de error
                    int i, ultimo_id = ids_p[0] - LOCAL_FIRST_ID;
                    for (i = LOCAL_FIRST_ID; i <= ultimo_id; ++i) {
                        if (abs(m.id) == ids_p[i]) {
                            m.mtype = i;
                            m.id = signo * i;
                            break;
                        }
                    }
                    if (i > ultimo_id) {
                        log_error("broker-replier: Id local no encontrado a partir del global. Ignoro reply para anon");
                        continue;
                    }
                } v(ids_sem);
            }

            switch (m.type) {
                case CREATE_MSG:    // Guarda id global para mappeo y devuelve id local al usuario
                    m.mtype = abs(m.id);
                    if (m.id >= 0) {    // Solo si fue exitoso
                        p(ids_sem); {
                            ids_p[m.id] = atoi(m.msg);  // ids_p[0] se incrementa en el client
                        }
                        v(ids_sem);
                        strcpy(m.msg, std::to_string(m.id).c_str());
                    }
                    qsend(q_rep, &m, sizeof(m));
                    break;
                case SUB_MSG:
                case PUB_MSG:       // Reenvían retorno al usuario
                    qsend(q_rep, &m, sizeof(m));
                    break;
                case RECV_MSG:      // Almacena mensaje
                    qsend(q_storedmsg, &m, sizeof(m));
                    break;
                case DESTROY_MSG:   // Borra del mappeo y reenvía retorno al usuario
                    p(ids_sem); {
                        ids_p[m.id] = -1;
                    } v(ids_sem);
                    qsend(q_rep, &m, sizeof(m));
                    break;
                default:
                    log_error("broker-replier: Tipo de mensaje no esperado. Freno");
                    exit(-1);
            }
        }
    }
    // Si yo termino, ya sea por detectar que cayó el server o lo que sea, me aseguro de que muera toda mi familia
    kill(getppid(), SIGINT);
    log_debug("broker-replier: Termino");//
}


int devolverMensajeRecibido(int q_storedmsg, struct msg_t* m, int user_id) {
    log_debug("broker-requester: Busco msg en q_storedmsg para devolver");
    if (qrecv_nowait(q_storedmsg, m, sizeof(*m), user_id) >= 0) {
        log_debug("broker-requester: Tengo mensaje recibido para devolverle al cliente:");//
        m->show();//
        return 0;
    } else if (errno == ENOMSG) {
        // No hay nuevos mensajes
        log_debug("broker-requester: No hay mensajes recibidos para devolverle al cliente");//
        m->id = 0;
        strcpy(m->topic, "");
        strcpy(m->msg, "¡No hay mensajes nuevos!");
        return 0;
    } else {
        log_warn("broker-requester: Error al pedir un mensaje de q_stored_msg. Sigo");
        return -1;
    }
}


void SIGINT_handler(int signum) {
    if (signum != SIGINT) {
        log_error("broker: Atrapé señal distinta de SIGINT: " + signum);
    } else {
        log_info("broker: SIGINT");
        sig_quit = true;
    }
}
