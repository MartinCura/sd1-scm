//
// Created by martin on 30/04/18.
//
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sstream>
#include <cstring>
#include "../common/constants.h"
#include "../common/message.h"
#include "../common/ipc/resources.h"
#include "../common/ringmessage.h"
extern "C" {
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/socket.h"
#include "../common/ipc/shm.h"
#include "../common/ipc/semaphore.h"
#include "../common/ipc/sig.h"
}

//std::string procn = "server";
bool sig_quit = false;
bool partOfRing = false;
int q_ringrecv = -1, q_ringsend = -1;

void requestHandler(int cfd, int q_req, int q_rep);
void replyHandler(int cfd, int q_rep);
void SIGINT_handler(int signum);
void crearEstructuraDb();

int main(int argc, char* argv[]) {
    /* argv = { ./scm-server [<sid>] [<sid-siguiente>] [<IP-siguiente>] } */
    if (argc > 4) {
        log_error("server: Cantidad de argumentos incorrecta. Freno");
        return -1;
    }
    if ( argc >= 2 &&
         (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) ) {
        std::ostringstream oss;
        oss << "Usage: " << argv[0] << " [id] [id del siguiente] [IP del siguiente]" << std::endl
            << "Usar sin argumentos para correr fuera de un ring";
        log_info(oss.str().c_str());
        return 0;
    }
    // Si llegaron, agarro id asignado y id al que me conectaré, y cambio a la carpeta correspondiente
    char *s_sid = NULL, *s_sid_sig = NULL, *ip_sig = NULL;
    if (argc >= 2) {
        partOfRing = true;
        s_sid = argv[1];
        if (strlen(s_sid) > 0 && (atoi(s_sid) < 0 || atoi(s_sid) >= MAX_SID)) {
            log_error("server: sid inválido. Freno");
            return -1;
        }
        if (argc >= 3) {
            s_sid_sig = argv[2];
            if (argc >= 4) {
                ip_sig = argv[3];
            }
        }
        std::string folder = "./s" + std::string(s_sid) + "/";
        std::string s = "mkdir -p " + folder;
        system(s.c_str());
        chdir(folder.c_str());
        s = "cp ../scm-client .";
        system(s.c_str());
    }
    if (argc >= 2)
        log_info("server: COMIENZO, con sid " + atoi(argv[1]));
    else
        log_info("server: COMIENZO");

    register_SIGINT_handler(SIGINT_handler);
    crearEstructuraDb();

    // Creo colas internas
    int q_req, q_rep;
    q_req = qcreate(SERVER_REQ_Q_ID);
    q_rep = qcreate(SERVER_REP_Q_ID);
    if (partOfRing) {
        q_ringrecv = qcreate(SERVER_RINGRECV_Q_ID);
        q_ringsend = qcreate(SERVER_RINGSEND_Q_ID);
    }
    if ( (q_req < 0 || q_rep < 0) &&
         (partOfRing && (q_ringrecv < 0 || q_ringsend < 0)) ) {
        log_error("server: Error al crear msg queue. Freno");
        exit(-1);
    }

    // Creo shm para próximo id y sem para acceder
    int next_id_sem = creasem(SERVER_NEXT_ID_SEM_ID);
    inisem(next_id_sem, 1);
    int next_id_shm = creashm(SERVER_NEXT_ID_SHM_ID, sizeof(int));
    int *next_id_p = (int *) mapshm(next_id_shm);
    *next_id_p = SERVER_FIRST_ID;

    // Creo el nodo que me conecta al ring
    if (partOfRing) {
        if (fork() == 0) {
            execl("../ring-node", "./ring-node", s_sid, s_sid_sig, ip_sig, (char *) NULL);
            exit(0);
        }
    }

    // Inicio conexión lista para recibir clientes (brokers locales)
    int sid = 0;
    if (partOfRing)
        sid = atoi(s_sid);
    uint16_t puerto_brokers = (uint16_t) (PUERTO_SERVER + (partOfRing ? sid : 0));
    int sfd = create_server_socket(puerto_brokers);
    if (sfd < 0) {
        log_error("ring_node: Error al crear server socket. Freno");
        exit(-1);
    }

    // Lanzo workers
    for (int i = 0; i < CANT_SERVER_WORKERS; ++i) {
        if (fork() == 0) {
            if (partOfRing) {
                execl("../server-worker", "./server-worker", std::string(s_sid).c_str(), (char *) NULL);
            } else {
                execl("../server-worker", "./server-worker", (char *) NULL);
            }
            exit(0);
        }
    }

    int cfd;
    while (!sig_quit) {
        // Espero brokers locales que se conecten; obtengo el socket apropiado
        cfd = accept_client(sfd);
        if (cfd >= 0) {
            // Corro procesos handlers para el nuevo broker local conectado
            log_info("server: Nuevo cliente, lanzo handlers");
            if (fork() == 0) {
                requestHandler(cfd, q_req, q_rep);
                close(cfd);
                exit(0);
            }
        }
    }

    // Limpieza
    close(sfd);
    qdel(q_req);
    qdel(q_rep);
    if (partOfRing) {
        // Envío mensaje de apagar servidor
        struct ringmsg_t rm;
        rm.sid_orig = UNFILLED_SID;
        rm.type = SERVOFF;
        qsend(q_ringsend, &rm, sizeof(rm));
    }
    // Espero a que terminen todos los procesos hijos
    while ( wait(NULL) > 0 );
    if (partOfRing) {
        qdel(q_ringrecv);
        qdel(q_ringsend);
    }
    unmapshm(next_id_p);
    delshm(next_id_shm);
    delsem(next_id_sem);
    // Borro todos los archivos
    std::stringstream c1, c2;
    c1 << "exec rm -rf " << SERVER_DB_SUBS_DIR   << "* &> /dev/null";
    c2 << "exec rm -rf " << SERVER_DB_TOPICS_DIR << "* &> /dev/null";
    system(c1.str().c_str());
    system(c2.str().c_str());
    log_info("server: TERMINO");
    return 0;
}


// Creo estructura de carpetas y un archivo vacío para que usen las variables IPC
void crearEstructuraDb() {
    std::string s;
    s = "mkdir -p " + std::string(SERVER_DB_SUBS_DIR);
    system(s.c_str());
    s = "rm -rf " + std::string(SERVER_DB_SUBS_DIR) + "*";
    system(s.c_str());
    s = "mkdir -p " + std::string(SERVER_DB_TOPICS_DIR);
    system(s.c_str());
    s = "rm -rf " + std::string(SERVER_DB_TOPICS_DIR) + "*";
    system(s.c_str());
    s = "echo \"\" > " + std::string(IPC_DIRECTORY);
    system(s.c_str());
}

void requestHandler(int cfd, int q_req, int q_rep) {
//    procn = "server-requestHandler";
    int p_replyH = fork();
    if (p_replyH == 0) {
        replyHandler(cfd, q_rep);
        exit(0);
    }
    ssize_t r;
    struct msg_t m;

    while (!sig_quit) {
        // Recibo mensaje del cliente por red
        log_debug("server-requestHandler: Espero por red próximo mensaje del cliente...");//
        r = recv(cfd, &m, sizeof(m), 0);
        if (r <= 0) {
            if (r == 0 || sig_quit) break;
            perror("server-requestHandler");
            log_error("server-requestHandler: Error al recibir mensaje del cliente por red. Sigo");
        } else {
            log_debug("server-requestHandler: Recibí mensaje de un broker local por red:");//
            m.show();//
            m.mtype = cfd;  // Referencia de dónde lo recibí
            // Mando mensaje a algún worker por cola interna
            qsend(q_req, &m, sizeof(m));
        }
    }
    kill(p_replyH, SIGINT);
    log_debug("server-requestHandler: Termino");//
}

void replyHandler(int cfd, int q_rep) {
//    procn = "server-replyHandler";
    struct msg_t m;

    while (!sig_quit) {
        log_debug("server-replyHandler: Espero próximo mensaje en q_rep...");//
        if (qrecv(q_rep, &m, sizeof(m), cfd) < 0) {
            if (sig_quit) break;
            log_warn("server-replyHandler: Error al recibir un mensaje de q_rep. Sigo intentando");
        } else {
            log_debug("server-replyHandler: Recibí por cola reply de un worker:");//
            m.show();//
            m.mtype = 1; // Oculto server-side info
            // Reenvío mensaje al cliente por red
            if (send(cfd, &m, sizeof(m), 0) < 0) {
                perror("server-replyHandler");
                log_error("server-replyHandler: Error al enviar mensaje al broker local");
            }
        }
    }
    log_debug("server-replyHandler: Termino");//
}

void SIGINT_handler(int signum) {
    if (signum != SIGINT) {
        log_error("server: Atrapé señal distinta de SIGINT: " + signum);
    } else {
        log_info("server: SIGINT");
        sig_quit = true;
    }
}
