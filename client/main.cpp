#include <iostream>
#include <cstring>
#include <csignal>
#include "../common/message.h"
extern "C" {
#include "../common/ipc/msg_queue.h"
}

void showHelp(char* argv[]);
int createUser();
int publishMessage(int id, char* msg, char* topic);
int subscribeToTopic(int id, char* topic);
int receiveMessage(int id);
int destroyUser(int id);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Cantidad de argumentos incorrecta" << std::endl;
        showHelp(argv);
        return 1;
    }
    std::string cmd(argv[1]);

    if (cmd == "CREATE" || cmd == "c") {
        return createUser();

    } else if (cmd == "PUB" || cmd == "p") {
        if (argc != 5) {
            std::cerr << "PUBlish requiere 3 argumentos" << std::endl;
            return 1;
        }
        return publishMessage(atoi(argv[2]), argv[3], argv[4]); ///TODO: Mejor chequeo de errores con argv[2]

    } else if (cmd == "SUB" || cmd == "s") {
        if (argc != 4) {
            std::cerr << "SUBscribe requiere 2 argumentos" << std::endl;
            return 1;
        }
        return subscribeToTopic(atoi(argv[2]), argv[3]);

    } else if (cmd == "RECV" || cmd == "r") {
        if (argc != 3) {
            std::cerr << "RECeiVe requiere 1 argumento" << std::endl;
            return 1;
        }
        return receiveMessage(atoi(argv[2]));

    } else if (cmd == "DESTROY" || cmd == "d") {
        if (argc != 3) {
            std::cerr << "DESTROY requiere 1 argumento" << std::endl;
            return 1;
        }
        return destroyUser(atoi(argv[2]));

    } else if (cmd == "HELP" || cmd == "h") {
        showHelp(argv);
        return 0;

    } else {
        showHelp(argv);
        return 1;
    }
    /// TODO opcional: autocompletado de terminal para los comandos
}


void showHelp(char* argv[]) {
    std::cout << "Usage: " << argv[0] << " COMMAND" << std::endl
              << "Commands:" << std::endl
              << '\t' << "CREATE / c"  << '\t' << "Crear un usuario; devuelve su id" << std::endl
              << '\t' << "PUB / p"     << '\t' << "Publicar a un topic (en caso de que no exista lo crea)" << std::endl
              << '\t' << "SUB / s"     << '\t' << "Suscribirse a un topic" << std::endl
              << '\t' << "RECV / r"    << '\t' << "Recibir próximo mensaje de cualquier topic" << std::endl
              << '\t' << "DESTROY / d" << '\t' << "Eliminar un usuario" << std::endl
              << '\t' << "HELP / h"    << '\t' << "Mostrar esta ayuda" << std::endl;
}


/** COMANDOS **/

int createUser() {
    struct msg_t m;
    ///m.mtype = ?
    m.type = CREATE_MSG;
    qsend(LOCAL_REQ_Q_ID, &m, sizeof(m));
    ///qrecv(LOCAL_REP_Q_ID, &m, sizeof(m), ???); RESOLVER cómo identificarlo la 1ra vez
    ///Condiciones de error?
    std::cout << "Usuario creado. Tu id es " << m.id << "." << std::endl;
    return 0;
}

int publishMessage(int id, char* msg, char* topic) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = PUB_MSG;
    strncpy(m.msg, msg, MAX_MSG_LENGTH);
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    qsend(LOCAL_REQ_Q_ID, &m, sizeof(m));
    qrecv(LOCAL_REP_Q_ID, &m, sizeof(m), id);
    ///Condiciones de error?
    std::cout << "Mensaje publicado.";
    return 0;
}

int subscribeToTopic(int id, char* topic) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = SUB_MSG;
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    qsend(LOCAL_REQ_Q_ID, &m, sizeof(m));
    qrecv(LOCAL_REP_Q_ID, &m, sizeof(m), id);
    ///Condiciones de error? Ej.: topic no existe
    std::cout << "Suscripto.";
    return 0;
}

int receiveMessage(int id) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = RECV_MSG;
    qsend(LOCAL_REQ_Q_ID, &m, sizeof(m));
    qrecv(LOCAL_REP_Q_ID, &m, sizeof(m), id);
    ///Condiciones de error? Ej.: topic no existe
    std::cout << "Topic: " << m.topic << std::endl;
    std::cout << m.msg << std::endl;
    return 0;
}

int destroyUser(int id) {
    struct msg_t m;
    m.mtype = m.id = id;
    m.type = DESTROY_MSG;
    qsend(LOCAL_REQ_Q_ID, &m, sizeof(m));
    qrecv(LOCAL_REP_Q_ID, &m, sizeof(m), id);
    ///Condiciones de error? Ej.: topic no existe
    std::cout << "Usuario " << m.id << " eliminado." << std::endl;
    return 0;
}
