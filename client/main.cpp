//
// Created by martin on 27/04/18.
//
#include <iostream>
#include <cstring>
#include <csignal>
#include "../common/message.h"
extern "C" {
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/semaphore.h"
#include "../common/ipc/shm.h"
}

int q_req, q_rep;

void showHelp(char* argv[]);
int createUser();
int publishMessage(int id, char* msg, char* topic);
int subscribeToTopic(int id, char* topic);
int receiveMessage(int id);
int destroyUser(int id);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Cantidad de argumentos incorrecta" << std::endl << std::endl;
        showHelp(argv);
        return 1;
    }
    std::string cmd(argv[1]);

    // Obtengo colas para comunicarme con el broker local
    q_req = qget(LOCAL_REQ_Q_ID);
    q_rep = qget(LOCAL_REP_Q_ID);
    if (q_req < 0 || q_rep < 0) {
        log_error("client: Error al gettear msg queue. Freno");
        exit(-1);
    }

    if (cmd == "CREATE" || cmd == "c") {
        return createUser();

    } else if (cmd == "PUB" || cmd == "p") {
        if (argc != 5) {
            std::cerr << "PUBlish requiere 3 argumentos" << std::endl;
            return 1;
        } else if (atoi(argv[2]) <= 0) {
            std::cerr << "id debe ser un número positivo" << std::endl;
            return 2;
        }
        return publishMessage(atoi(argv[2]), argv[3], argv[4]);

    } else if (cmd == "SUB" || cmd == "s") {
        if (argc != 4) {
            std::cerr << "SUBscribe requiere 2 argumentos" << std::endl;
            return 1;
        } else if (atoi(argv[2]) <= 0) {
            std::cerr << "id debe ser un número positivo" << std::endl;
            return 2;
        }
        return subscribeToTopic(atoi(argv[2]), argv[3]);

    } else if (cmd == "RECV" || cmd == "r") {
        if (argc != 3) {
            std::cerr << "RECeiVe requiere 1 argumento" << std::endl;
            return 1;
        } else if (atoi(argv[2]) <= 0) {
            std::cerr << "id debe ser un número positivo" << std::endl;
            return 2;
        }
        return receiveMessage(atoi(argv[2]));

    } else if (cmd == "DESTROY" || cmd == "d") {
        if (argc != 3) {
            std::cerr << "DESTROY requiere 1 argumento" << std::endl;
            return 1;
        } else if (atoi(argv[2]) <= 0) {
            std::cerr << "id debe ser un número positivo" << std::endl;
            return 2;
        }
        return destroyUser(atoi(argv[2]));

    } else if (cmd == "HELP" || cmd == "h") {
        showHelp(argv);
        return 0;

    } else {
        std::cout << "Comando no reconocido" << std::endl << std::endl;
        showHelp(argv);
        return 1;
    }
    ///TODO: Errores de comunicación no son detectados
}


void showHelp(char* argv[]) {
    std::cout << "Usage: " << argv[0] << " COMMAND" << std::endl
              << "Commands:" << std::endl
              << '\t' << "CREATE  / c" << "\t\t\t" << "Crear un usuario; devuelve su id" << std::endl
              << '\t' << "PUB     / p (id, msg, topic)" << '\t' << "Publicar a un topic (en caso de que no exista lo crea)" << std::endl
              << '\t' << "SUB     / s (id, topic)" << "\t\t" << "Suscribirse a un topic" << std::endl
              << '\t' << "RECV    / r (id)" << "\t\t" << "Recibir próximo mensaje de cualquier topic" << std::endl
              << '\t' << "DESTROY / d (id)" << "\t\t" << "Eliminar un usuario" << std::endl
              << '\t' << "HELP    / h" << "\t\t\t" << "Mostrar esta ayuda" << std::endl;
}

bool errorCheck(struct msg_t m) {
    if (m.id > 0) {
        return false;
    }// else if (!strcmp(m.msg, "")) {
    std::cout << "Error: " << m.msg;
    //}
    return true;
}

int generarNuevoId() {
    int nuevo_id;
    int ids_sem = getsem(LOCAL_IDS_SEM_ID);
    int ids_shm = getshm(LOCAL_IDS_SHM_ID);
    int* ids_p = (int*) mapshm(ids_shm);
    // Obtengo nuevo id
    p(ids_sem); {
        nuevo_id = ids_p[0]++;
    } v(ids_sem);
    unmapshm(ids_p);
    return nuevo_id;
}


/** COMANDOS **/

int createUser() {
    struct msg_t m;
    m.id = generarNuevoId();
    if (m.id >= LOCAL_MAX_ID) {
        log_warn("client: Superé cantidad máxima de ids");
        m.id = -1;
        strcpy(m.msg, "Cantidad permitida de usuarios locales superada");
    } else {
        m.mtype = 1;
        m.type = CREATE_MSG;
        qsend(q_req, &m, sizeof(m));
        qrecv(q_rep, &m, sizeof(m), m.id);
    }
    if (errorCheck(m)) return -1;
    std::cout << "Usuario creado. Su id es " << m.id << "." << std::endl;
    return 0;
}

int publishMessage(int id, char* msg, char* topic) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = PUB_MSG;
    strncpy(m.msg, msg, MAX_MSG_LENGTH);
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    qsend(q_req, &m, sizeof(m));
    qrecv(q_rep, &m, sizeof(m), id);
    if (errorCheck(m)) return -1;
    std::cout << "Mensaje publicado." << std::endl;
    return 0;
}

int subscribeToTopic(int id, char* topic) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = SUB_MSG;
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    qsend(q_req, &m, sizeof(m));
    qrecv(q_rep, &m, sizeof(m), id);
    if (errorCheck(m)) return -1;
    std::cout << "Suscripto." << std::endl;
    return 0;
}

int receiveMessage(int id) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = RECV_MSG;
    qsend(q_req, &m, sizeof(m));
    qrecv(q_rep, &m, sizeof(m), id);
    if (errorCheck(m)) return -1;
    std::cout << "----------" << std::endl;
    std::cout << "Topic: " << m.topic << std::endl;
    std::cout << m.msg << std::endl;
    std::cout << "----------" << std::endl;
    return 0;
}

int destroyUser(int id) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = DESTROY_MSG;
    qsend(q_req, &m, sizeof(m));
    qrecv(q_rep, &m, sizeof(m), id);
    if (errorCheck(m)) return -1;
    std::cout << "Usuario " << m.id << " eliminado." << std::endl;
    return 0;
}
