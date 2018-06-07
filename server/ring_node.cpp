//
// Created by martin on 29/05/18.
//
#include <cstdlib>
#include <cstdint>
#include <sys/socket.h>
#include <zconf.h>
#include <signal.h>
#include <cstring>
#include <sstream>
#include <wait.h>
#include "../common/ringmessage.h"
#include "../common/message.h"
extern "C" {
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/socket.h"
#include "../common/ipc/sig.h"
}

#define SID_SIG_BASE 10
#define RELANZAR_RET 5

///Agregar otra variable que indique que me quiero apagar, distinta a sig_quit?
bool sig_quit = false;
bool new_conn = true;
int sid = 0, sid_sig = -1;
int q_ringsend, q_ringrecv, q_req;

void SIGINT_handler(int signum);
void SIGALRM_handler(int signum);
int ring_receiver(int nafd);
int ring_sender(int nsfd);


int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        log_error("ring_node: Cantidad de argumentos incorrecta. Freno");
        return -1;
    }

    // Agarro parámetros sid y sid_sig
    sid = atoi(argv[1]);
    if (sid < 0/* || sid > MAX_SID*/) {
        log_error("ring_node: server id inválido. Freno");
        return -2;
    } else if (argc >= 3) {
        sid_sig = atoi(argv[2]);
        if (sid_sig < 0/* || sid > MAX_SID*/) {
            log_error("ring_node: server id inválido. Freno");
            return -2;
        }
    }

    register_SIGINT_handler(SIGINT_handler);

    // Colas
    q_ringsend = qget(SERVER_RINGSEND_Q_ID);    // Para el sender del ring
    q_ringrecv = qget(SERVER_RINGRECV_Q_ID);    // Para el receiver del ring
    q_req      = qget(SERVER_REQ_Q_ID);         // Para difundir un mensaje que llegó desde el ring
    if (q_ringsend < 0 || q_ringrecv < 0 || q_req < 0) {
        log_error("ring_node: Error al crear msg queue. Freno");
        exit(-1);
    }

    std::ostringstream oss;
    oss << "ring_node: Comienzo con sid propio " << sid << " y sid_sig " << sid_sig;
    log_info(oss.str().c_str());
    int nfd, nafd, nsfd;

    // Inicio conexión lista para recibir clientes (un nodo "anterior" en el anillo)
    uint16_t puerto_ring = (uint16_t) (PUERTO_NODO_RING + sid);
    nfd = create_server_socket(puerto_ring);
    if (nfd < 0) {
        log_error("ring_node: Error al crear node socket. Freno");
        exit(-1);
    }

    // Lanzo proceso para el sender; siempre hay solo uno que se reconecta con distintos nodos según haga falta
    if (fork() == 0) {
        struct ringmsg_t rm;
        while (!sig_quit) {
            while (sid_sig < 0) {   ///Esto no me encanta...
                // Si no tengo con quién conectarme, esperar un mensaje NEWCONN que me diga
                if (qrecv(q_ringsend, &rm, sizeof(rm), 0) < 0) {
                    log_error("ring_sender: Error con qrecv, esperando un primer NEWCONN. Freno proceso");
                    exit(-1);
                }
                if (rm.type == NEWCONN && strlen(rm.content) > 0) {
                    sid_sig = rm.sid_orig;
                    std::ostringstream oss3;
                    oss3 << "ring_sender: Me conectaré al sid " << sid_sig;
                    log_info(oss3.str().c_str());
                } else {
                    log_info("ring_node: Se intentó distribuir un mensaje pero todavía no estoy conectado");
                }
            }

            int new_sid_sig = 0;
            do {
                std::ostringstream oss4;//
                oss4 << "ring_node: Intento conectarme con nodo de sid " << sid_sig;//
                log_debug(oss4.str().c_str());//
                // Intento conectarme con el siguiente nodo del ring; obtengo el socket file descriptor
                nsfd = create_client_socket(IP_SERVER, (uint16_t) (PUERTO_NODO_RING + sid_sig));
                if (nsfd < 0) {
                    log_error("ring_sender: Error al crear socket cliente para nodo siguiente. No está listo? Freno");
                    kill(getppid(), SIGINT);
                    exit(-1);
                }

                if (rm.sid_orig >= 0) { ///Si recibí de ring_receive un rm, no recibí sid_sig por parámetro
                    // No inicié la conexión, pero debo distribuir el mensaje NEWCONN que recibí
                    if (send(nsfd, &rm, sizeof(rm), 0) < 0) {
                        perror("ring_sender");
                        log_error("ring_sender: Error al distribuir mensaje de NEWCONN al nodo siguiente. Sigo igual");
                    }
                } else if (new_conn) {
                    // Formalizo nueva conexión con nodo siguiente
                    log_debug("ring_sender: Creo y envío NEWCONN a siguiente");
                    struct ringmsg_t rm2;
                    rm2.sid_orig = sid; // Conectarse conmigo
                    rm2.type = NEWCONN;
                    if (send(nsfd, &rm2, sizeof(rm2), 0) < 0) {
                        perror("ring_sender");
                        log_error("ring_sender: Error al enviar mensaje de NEWCONN al nodo siguiente. Sigo igual");
                    }
                }// else: En casos de caída de un nodo, puedo no querer que se envíe un NEWCONN que reconfigure el ring
                new_conn = true; // Vuelvo a valor por defecto; ring_sender() lo cambiará si hace falta

                std::ostringstream oss2;    ///TODO: Limpieza de estos
                oss2 << "ring_sender: Conectado con nodo " << sid_sig;
                log_info(oss2.str().c_str());

                new_sid_sig = ring_sender(nsfd);

                close(nsfd);
                sid_sig = new_sid_sig - SID_SIG_BASE;
                if (sid_sig >= 0) {
                    rm.type = RETCONN;  // Se reconfiguró el anillo (por NEWCONN/ RETCONN / SERVOFF) y con este mensaje lo finalizo
                    rm.sid_orig = sid_sig;
                }
            } while (!sig_quit && sid_sig >= 0 && sid_sig != sid); // Chequea si hace falta reconectarse a otro nodo

            sid_sig = -1;
        } // while (sig_quit)
        exit(0);
    }

    // Espero nuevas conexiones de nodos "anteriores" que intenten cambiar la configuración del anillo
    while (!sig_quit) {
        log_info("ring_node: Espero nuevos nodos anteriores...");
        nafd = accept_client(nfd);
        if (sig_quit) break;
        if (nafd < 0) {
            log_error("ring_node: Error al conectarme a node anterior socket. Sigo");
            continue;
        }
        if (sig_quit) {
            close(nafd);
            break;
        }

        // Lanzo proceso receiver para el nodo que se me conectó
        std::ostringstream oss5;//
        oss5 << "ring_node: Nuevo nodo anterior, (re)lanzo handler receiver (nafd=" << nafd << ")";//
        log_info(oss5.str().c_str());//
//        sleep(2);//
        pid_t p_recv = fork();
        if (p_recv == 0) {
            int r = ring_receiver(nafd);

            while (r == RELANZAR_RET) {
                log_debug("ring_receiver: Relanzo proceso receiver con mismo socket nafd");
                r = ring_receiver(nafd);
            };///Chequear que pueda usar el mismo socket
            close(nafd);
            exit(0);
        }
//        waitpid(p_recv, NULL, 0);///Solo de prueba, espera a que se muera el receiver anterior ///TODO: Borrar?
    }

    // Espero a que terminen todos los procesos hijos
    while ( wait(NULL) > 0 );   ///chan

    close(nfd);
    return 0;
}   ///TODO: Limpiar un poco, distribuir en funciones


int ring_receiver(int nafd) {
    int sid_ant = -1;
    ssize_t s;
    struct ringmsg_t rm;

    while (!sig_quit) {
        log_debug("ring_receiver: Espero por red próximo mensaje del ring");//
        s = recv(nafd, &rm, sizeof(rm), 0);
        if (s <= 0) {
            if (s == 0 || sig_quit) break; ///Revisar ==0
            perror("ring_receiver");
            log_error("ring_receiver: Error al recibir mensaje del nodo anterior por red. Sigo");
        } else {
            log_debug("ring_receiver: Recibí mensaje por ring:");//
            rm.show();//

            // Descarto mensajes que no tengan un sid_orig válido
            if (rm.sid_orig < 0/* || rm.sid_orig > MAX_SID*/) {
                continue;
            }

            switch (rm.type) {
                case NEWCONN:   // Alguien se busca agregar al anillo
                    if (strlen(rm.content) != 0 && atoi(rm.content) == sid) {
                        // Se terminó de distribuir un NEWCONN de mi nuevo nodo anterior, a través del camino viejo
                        // Me desconecto del viejo nodo anterior
                        sig_quit = true;
                        break;
                    } else if (strlen(rm.content) == 0) {
                        // Se me quiere conectar (directamente) alguien que no estaba en el anillo
                        sid_ant = rm.sid_orig;
                        strcpy(rm.content, std::to_string(sid).c_str());
                        std::ostringstream oss;
                        oss << "ring_receiver: Nuevo nodo anterior es " << sid_ant;
                        log_info(oss.str().c_str());
                    } else if (rm.sid_orig == sid) {
                        // Se terminó de distribuir mi NEWCONN para conectarme al ring; no distribuyo
                        log_info("ring_receiever: Proceso de NEWCONN terminado");
                        break;
                    }
                    log_debug("ring_receiver: Distribuyo NEWCONN:");//
                    rm.show();//
                    qsend(q_ringsend, &rm, sizeof(rm));
                    break;
                case RETCONN:   // Alguien intenta reconectarse
                    if (rm.sid_orig == sid) {
                        // Mi mensaje para reconectarse se distribuyó correctamente.
                    } else if (strlen(rm.content) == 0) {
                        // Nodo se logró reconectar;
                        // notifico a quien me tenga como siguiente que debe pasar por él
                        strncpy(rm.content, std::to_string(sid).c_str(), MAX_MSG_LENGTH);
                        qsend(q_ringsend, &rm, sizeof(rm));
                    } else if (atoi(rm.content) == sid) {sig_quit = true;
                        // Se me conectó el anterior del caído; ///¿no hacer nada?
                    } else {
                        // Envío para que se distribuya
                        qsend(q_ringsend, &rm, sizeof(rm));
                    }
                    break;
                case PUBLISH:
                    if (rm.sid_orig < 0 || rm.sid_orig == sid) {
                        // Es inválido o propio, no actúo ni distribuyo.
                    } else {
                        qsend(q_ringsend, &rm, sizeof(rm));
                        struct msg_t m;
                        m.type = PUB_MSG;
                        m.id = 0;   // Provino del ring
                        strncpy(m.topic, rm.topic, MAX_TOPIC_LENGTH);
                        strncpy(m.msg, rm.content, MAX_MSG_LENGTH);
                        qsend(q_req, &m, sizeof(m));
                    }
                    break;
                case SERVOFF:
                    if (rm.sid_orig == sid) {
                        // Se terminó de distribuir mi mensaje; me apago
                        kill(getppid(), SIGINT);///No estoy seguro que esto también SIGINTee a otros hijos
                        sig_quit = true;
                    } else if (strlen(rm.content) == 0) {
                        // Nodo anterior se apagará; pido que su anterior se reconecte conmigo y cierro este
                        strncpy(rm.content, std::to_string(sid).c_str(), MAX_MSG_LENGTH);
                        qsend(q_ringsend, &rm, sizeof(rm));
                        // Nuevo anterior se debería reconectar con NEWCONN o RETCONN, y ahí morirá este receiver
                    } else {
                        // Envío al sender
                        qsend(q_ringsend, &rm, sizeof(rm));
                    }
                    break;
                case SHUTDWN:
                    // Distribuyo y dejo de escuchar; el sender apagará
                    qsend(q_ringsend, &rm, sizeof(rm));
                    sig_quit = true;
                    break;
                default:
                    log_info("ring_receiver: Pasó 'otro' tipo de mensaje, distribuyo si no propio");
                    if (rm.sid_orig != sid) {
                        qsend(q_ringsend, &rm, sizeof(rm));
                    }
            }
        }
    }

    /// Esto en realidad no sirve
//    if (sig_quit) {
//        // Estado zombie donde distribuye mensajes, por un tiempo o hasta recibir confirmación de apagarse
//        sig_quit = false;
//        register_sig_handler(SIGALRM_handler, SIGALRM);
//        alarm(SERVOFF_TIMER_SEC);
//        while (!sig_quit) {
//            log_debug("ring_receiver: Modo zombie");//
//            s = recv(nafd, &rm, sizeof(rm), 0);
//            if (s <= 0) {
//                if (s == 0 || sig_quit) break;
//                perror("ring_receiver");
//                log_error("ring_receiver: (modo zombie) Error al recibir mensaje del nodo anterior por red. Sigo");
//            } else {
//                log_debug("ring_receiver: (modo zombie) Recibí mensaje por ring:");//
//                rm.show();//
//                // Descarto mensajes que no tengan un sid_orig válido
//                if (rm.sid_orig < 0/* || rm.sid_orig > MAX_SID*/) {
//                    continue;
//                }
//
//                qsend(q_ringsend, &rm, sizeof(rm));
//                if (rm.sid_orig == sid && rm.type == SERVOFF) {
//                    sig_quit = true;
//                }
//            }
//        }

    if (!sig_quit) {    ///Tendría que chequear errno? O algo?
        // En caso de que no esté apagando el server, hubo un error de red
        ///Si cae la conexión y vuelve, tengo q reconectarme o puedo simplemente usarla?
        alarm(RETCONN_TIMER_SEC);    // Chequea en algunos segundos si vuelve   ///TODO: ¿Funciona...?
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(nafd, &readfds);
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        int r = 0;
        while (!sig_quit && r <= 0) {
            r = select(nafd + 1, &readfds, NULL, NULL, &tv);
        }

        if (!sig_quit && r > 0) {
            // Conexión está lista para ser chequeada; envío un mensaje de prueba
            struct ringmsg_t test_rm;
            if (send(nafd, &test_rm, sizeof(test_rm), 0) < 0) {
                log_info("ring_receiver: Conexión con anterior fue cerrada, cierro este receiver");
                return 0;
            }
            // Si anduvo, volvió la conexión y relanzo este receiver
            log_info("ring_receiver: Relanzo esta conexión");
            return RELANZAR_RET;
        } else if (sig_quit && errno == EINTR) {  // Si no volvió a tiempo, intento enviar RETCONN con content=sid_caido
            if (sid_ant >= 0) { // Si tengo sid del caído, envío un RETCONN para el anterior a él
                rm.sid_orig = sid;
                rm.type = RETCONN;
                strncpy(rm.content, std::to_string(sid_ant).c_str(), MAX_MSG_LENGTH);
                qsend(q_ringsend, &rm, sizeof(rm));
            }
            return 0;
        } // Si simplemente tengo que irme, nada
    }
    return 0;
}

int ring_sender(int nsfd) {
    struct ringmsg_t rm;

    while (!sig_quit) {
        log_debug("ring_sender: Espero próximo mensaje en q_ringsend...");
        if (qrecv(q_ringsend, &rm, sizeof(rm), 0) < 0) {
            if (sig_quit) break;
            log_warn("ring_sender: Error al recibir un mensaje de q_ringsend. Sigo intentando");
        } else {
            log_debug("ring_sender: Recibí por cola msj ring:");//
            rm.show();//

            switch (rm.type) {
                case NEWCONN:
                    if (strlen(rm.content) == 0) {
                        //??? No debería poder ser, cierto?
                    } else if (atoi(rm.content) == sid_sig) {
                        if (rm.sid_orig == sid) {
                            log_error("ring_sender: Recibí un mensaje NEWCONN con mi sid. Error. Ignoro");
                            break;
                        }
                        // Debo desconectarme de sid_sig y conectarme al que está en sid_orig; primero distribuyo
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                        ///Cómo esperar a que el nodo siguiente actual termine de recibir mensajes?
                        sleep(1);//
                        return SID_SIG_BASE + rm.sid_orig;
                    }
                    log_debug("ring_sender: Envío NEWCONN a red:");//
                    rm.show();//
                    if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                        perror("ring_sender");
                    break;
                case RETCONN:
                    if (rm.sid_orig == sid_sig) {
                        // Se logró reconectar el nodo siguiente.
                        ///Tal vez tengo que hacer algo acá para reestablecer la conexión.
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    } else if (atoi(rm.content) == sid_sig) {
                        // Cayó el siguiente; tiro esta conexión y me conecto con uno posterior
                        new_conn = false;   // Evitar que el nuevo sender envíe un NEWCONN
                        return SID_SIG_BASE + rm.sid_orig;
                    } else {
                        // Simplemente distribuyo
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    }
                    break;
                case PUBLISH:
                    if (rm.sid_orig == UNFILLED_SID) {
                        rm.sid_orig = sid;  // Lleno el sid propio
                    }
                    if (rm.sid_orig != sid_sig) {   // Si lo originó el siguiente, no distribuyo
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    }
                    break;
                case SERVOFF:
                    if (rm.sid_orig == UNFILLED_SID) {
                        // Me quiero apagar
                        log_info("ring_sender: Me quiero apagar, envío SERVOFF");
                        rm.sid_orig = sid;    // Lleno el sid propio
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                        sig_quit = true;    // Entro a un modo zombie que espera ser apagado
                    } else if (rm.sid_orig == sid) {
                        // Me quiero apagar
                        // No debería haber llegado, me apago igual
                        sig_quit = true;
                    } else if (rm.sid_orig == sid_sig) {
                        // Se apaga el siguiente; distribuyo msj y me conectaré en su lugar con el que esté en content
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                        ///Puedo llegar a necesitar un timer para que el otro sepa apagarse?
                        sleep(1);//
                        return SID_SIG_BASE + atoi(rm.content);
                    } else {
                        // Simplemente distribuyo
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    }
                    break;
                case SHUTDWN:
                    if (rm.sid_orig == UNFILLED_SID) {
                        rm.sid_orig = sid;  // Lleno el sid propio
                    }
                    if (rm.sid_orig != sid_sig) {   // Si lo originó el siguiente, no distribuyo
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    }
                    ///¿Flush timer?
                    sig_quit = true;
                    break;
                default:
                    log_info("ring_receiver: Pasó 'otro' tipo de mensaje, distribuyo si no propio");
                    if (rm.sid_orig != sid) {
                        if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                            perror("ring_sender");
                    }
            }
        }
    }

    // Estado zombie donde distribuye mensajes, por un tiempo o hasta recibir confirmación de apagarse
    log_debug("ring_sender: Entro en modo zombie");
    sig_quit = false;
    register_sig_handler(SIGALRM_handler, SIGALRM);
    alarm(SERVOFF_TIMER_SEC);
    while (!sig_quit) {
        log_debug("ring_sender: (zombie mode) Espero próximo mensaje en q_ringsend...");
        if (qrecv(q_ringsend, &rm, sizeof(rm), 0) < 0) {
            if (sig_quit) break;
            log_warn("ring_sender: (modo zombie) Error al recibir un mensaje de q_ringsend. Freno");
            break;
        } else {
            log_debug("ring_sender: (modo zombie) Recibí por cola msj ring:");//
            rm.show();//

            if (rm.sid_orig != sid) {
                if (send(nsfd, &rm, sizeof(rm), 0) < 0)
                    perror("ring_sender");
            } else if (rm.type == SERVOFF) {
                kill(getppid(), SIGINT);    // Mato al ring node
                sig_quit = true;
            }
        }
    }
    return 0;
}


void SIGINT_handler(int signum) {
    if (signum != SIGINT) {
        log_error("ring_node: Atrapé señal distinta de SIGINT: " + signum);
    } else {
        log_info("ring_node: SIGINT");
        sig_quit = true;
    }
}

void SIGALRM_handler(int signum) {
    if (signum != SIGALRM) {
        log_warn("ring_node: Freno con señal distinta de SIGALRM: " + signum);
    } else {
        log_info("ring_node: SIGALRM");
    }
    sig_quit = true;
}
