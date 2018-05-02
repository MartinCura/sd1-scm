//
// Created by martin on 01/05/18.
//

#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include "../common/constants.h"
#include "../common/message.h"

extern "C" {
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
}

std::string getTopicFn(std::string topic) {
    return std::string(SERVER_DB_TOPICS_DIR) + topic + SERVER_DB_FILE_EXT;
}

std::string getSubFn(int id) {
    return std::string(SERVER_DB_SUBS_DIR) + std::to_string(id) + SERVER_DB_FILE_EXT;
}

int main(int argc, char* argv[]) {
    // Obtiene colas
    int q_req = qget(SERVER_REQ_Q_ID);
    int q_rep = qget(SERVER_REP_Q_ID);
    if (q_req < 0 || q_rep < 0) {
        log_error("worker: Error al crear msg queue. Freno");
        exit(-1);
    }
    std::map<int,int> ids;
    int next_id = SERVER_FIRST_ID;  ///TODO: Compartir next_id con otros workers mediante shm

    while (true) {
        struct msg_t m;
        // Obtengo próximo mensaje que necesite ser procesado
        if (qrecv(q_req, &m, sizeof(m), 0)) {
            log_error("worker: Error al recibir mensaje por cola. Sigo");
        } else {
            log_debug("worker: Recibí mensaje por cola:");//
            m.show();//
            ///Agarrar algo?
            ////m.mtype, m.type, m.id, m.topic, m.msg

            switch (m.type) {   ///TODO: Limpiar diviendo en funciones?
                case CREATE_MSG:    // Entrego y registro nuevo id
                    {
                        m.id = next_id++;
                        //std::pair<std::map<int,int>::iterator,bool> ret;//
                        if (!ids.insert(std::pair<int, int>(m.id, m.mtype)).second) {
                            // Id ya estaba registrado???
                            log_error("worker: id previamente registrado, cosa imposible? Freno");
                            exit(-1);
                        }
                    }
                    break;

                case SUB_MSG:       // Si id existe, en fs agrega sub al topic y topic al sub. Si topic no existía lo crea
                    {
                        if (ids.find(m.id) != ids.end()) {
                            ///Validar input?
                            ///TODO: lock?
                            std::fstream s_fs, t_fs;
                            t_fs.open(getTopicFn(m.topic), std::fstream::in | std::fstream::out | std::fstream::app);

                            int s = 0;
                            while (t_fs >> s && s != m.id) {}
                            if (s == m.id) {
                                // Id ya estaba suscripto al topic
                                ///Qué hago? Algo? Aviso?

                            } else {
                                // No lo encontré, lo suscribo
                                t_fs.clear();
                                t_fs << m.id << std::endl;  // Agrego id al topic
                                s_fs.open(getSubFn(m.id), std::fstream::out | std::fstream::app);
                                s_fs << m.topic << std::endl;   // Agrego topic al sub
                                s_fs.close();
                            }
                            t_fs.close();
                            ///TODO: unlock?
                        } else {
                            log_info("worker: sub tiene id inexistente");
                            ///TODO: Tengo q avisar del error de alguna manera
                        }
                    }
                    break;

                case PUB_MSG:
                    {
                        if (ids.find(m.id) != ids.end()) {
                            // Preparo mensaje de difusión
                            struct msg_t m_dif;
                            m_dif.type = RECV_MSG;
                            strncpy(m_dif.topic, m.topic, MAX_TOPIC_LENGTH);
                            strncpy(m_dif.msg, m.msg, MAX_MSG_LENGTH);

                            int s;
                            std::ifstream t_ifs;
                            t_ifs.open(getTopicFn(m.topic), std::ifstream::in);
                            // Envío un mensaje por cada suscriptor
                            while (t_ifs >> s) {
                                std::map<int, int>::iterator it_rcp = ids.find(s);
                                if (it_rcp != ids.end()) {
                                    m_dif.id = s;
                                    m_dif.mtype = it_rcp->second;   // cfd para este id
                                    qsend(q_rep, &m_dif, sizeof(m_dif));
                                } else {
                                    log_error("worker: sub encontrado en topic pero inexistente en map. Sigo");
                                }
                            }
                            t_ifs.close();
                        } else {
                            log_info("worker: pub tiene id inexistente; ignoro?");
                            ///TODO: Tengo q avisar del error de alguna manera
                        }
                    }
                    break;

                case RECV_MSG:
                    log_error("worker: Me llegó un RECV_MSG, no debería pasar");
                    continue; ///??? No debería pasar

                case DESTROY_MSG:   // Si id existe, lo desuscribo de cada topic y borro
                    {
                        std::map<int, int>::iterator id_it = ids.find(m.id);
                        if (id_it != ids.end()) {
                            // Recorro archivo de sub agarrando sus topics
                            std::ifstream s_ifs, t_ifs;
                            std::string s_fn = getSubFn(m.id);
                            s_ifs.open(s_fn, std::ifstream::in);
                            std::string t;
                            int s;
                            while (s_ifs >> t) {    ///TODO: REEMPLAZAR ESTOS DEL TOPIC POR GETLINE
                                // Copio archivo del topic ignorando al sub que elimino
                                std::string t_fn = getTopicFn(t);
                                std::string aux_fn = t_fn + ".aux";
                                std::ofstream aux_ofs;
                                t_ifs.open(t_fn, std::ifstream::in);
                                aux_ofs.open(aux_fn, std::ofstream::out | std::ofstream::trunc);
                                while (t_ifs >> s) {
                                    if (s != m.id) {
                                        aux_ofs << s << std::endl;
                                    }
                                }
                                t_ifs.close();
                                aux_ofs.close();
                                // Borro archivo topic y renombro auxiliar como original
                                remove(t_fn.c_str());
                                rename(aux_fn.c_str(), t_fn.c_str());
                            }
                            s_ifs.close();
                            // Borro archivo del sub destruido
                            remove(s_fn.c_str());
                            // Borro del mapa
                            ids.erase(id_it);
                        } else {
                            log_info("worker: destroy tiene id inexistente");
                            ///TODO: Tengo q avisar del error de alguna manera
                        }
                    }
                    break;

                default:
                    log_error("worker: Tipo de mensaje no esperado. Freno");
                    exit(-1);
            }

            // Envío mensaje respuesta
            qsend(q_rep, &m, sizeof(m));
            log_debug("worker: Devolví por cola el mensaje respuesta");
        }
    }
}
