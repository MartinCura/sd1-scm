#include <iostream>
#include <string>
#include <cstring>
#include "../common/message.h"

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
    std::string c = argv[1];

    if (argv[1] == "CREATE" || argv[1] == "c") {
        return createUser();
    } else if (argv[1] == "PUB" || argv[1] == "c") {
        if (argc != 5) {
            std::cerr << "PUBlish requiere 3 argumentos" << std::endl;
            return 1;
        }
        return publishMessage(strtol(argv[2],NULL,10), argv[3], argv[4]);
    } else if (argv[1] == "SUB" || argv[1] == "c") {
        if (argc != 4) {
            std::cerr << "SUBscribe requiere 2 argumentos" << std::endl;
            return 1;
        }
        return subscribeToTopic(strtol(argv[2],NULL,10), argv[3]);
    } else if (argv[1] == "RECV" || argv[1] == "c") {
        if (argc != 3) {
            std::cerr << "RECieVe requiere 1 argumento" << std::endl;
            return 1;
        }
        return receiveMessage(strtol(argv[2],NULL,10));
    } else if (argv[1] == "DESTROY" || argv[1] == "c") {
        if (argc != 3) {
            std::cerr << "DESTROY requiere 1 argumento" << std::endl;
            return 1;
        }
        return destroyUser(strtol(argv[2],NULL,10));
    } else if (argv[1] == "HELP" || argv[1] == "c") {
        showHelp(argv);
        return 0;
    } else {
        showHelp(argv);
        return 1;
    }
    //return 0;
}

void showHelp(char* argv[]) {
    std::cout << "Usage: " << argv[0] << " COMMAND" << std::endl
              << "COMMANDS:" << std::endl
              << '\t' << "CREATE / c" << '\t' << "..." << std::endl
              << '\t' << "PUB / p" << '\t' << "..." << std::endl
              << '\t' << "SUB / s" << '\t' << "..." << std::endl
              << '\t' << "RECV / r" << '\t' << "..." << std::endl
              << '\t' << "DESTROY / d" << '\t' << "..." << std::endl
              << '\t' << "HELP / h" << '\t' << "Mostrar esta ayuda" << std::endl;
}

int addMsgToRequestQ(msg_t m) {
    //... Acceder a cola correcta, enviar mensaje
}

msg_t receiveMsgFromReplyQ(int id) {
    //... Acceder a cola correcta con mtype correcto, esperar mensaje
}

int createUser() {
    struct msg_t m;
    ///m.mtype = ?
    m.type = CREATE_MSG;
    addMsgToRequestQ(m);
    msg_t r = receiveMsgFromReplyQ(0);   // Cómo identifica?
    // TODO: Mostrar resultados
}

int publishMessage(int id, char* msg, char* topic) {
    struct msg_t m;
    ///m.mtype = ?
    m.type = PUB_MSG;
    m.id = id;
    strncpy(m.msg, msg, MAX_MSG_LENGTH);
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    addMsgToRequestQ(m);
    msg_t r = receiveMsgFromReplyQ(id);
    // TODO: Mostrar resultados
}

int subscribeToTopic(int id, char* topic) {
    struct msg_t m;
    ///m.mtype = ?
    m.type = SUB_MSG;
    m.id = id;
    strncpy(m.topic, topic, MAX_TOPIC_LENGTH);
    addMsgToRequestQ(m);
    msg_t r = receiveMsgFromReplyQ(id);
    // TODO: Mostrar resultados
}

int receiveMessage(int id) {
    struct msg_t m;
    ///m.mtype = ?
    m.type = RECV_MSG;
    m.id = id;
    addMsgToRequestQ(m);
    msg_t r = receiveMsgFromReplyQ(id);
    // TODO: Mostrar resultados
}

int destroyUser(int id) {
    struct msg_t m;
    ///m.mtype = ?
    m.type = DESTROY_MSG;
    m.id = id;
    addMsgToRequestQ(m);
    msg_t r = receiveMsgFromReplyQ(id);
    // TODO: Mostrar resultados
}
