//
// Created by martin on 01/05/18.
//
#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <csignal>
#include "../common/constants.h"
#include "../common/message.h"
extern "C" {
#include "../common/log/log.h"
#include "../common/ipc/msg_queue.h"
#include "../common/ipc/semaphore.h"
#include "../common/ipc/shm.h"
#include "../common/ipc/sig.h"
}

bool sig_quit = false;

void SIGINT_handler(int signum) {
    if (signum != SIGINT) {
        log_error("worker: Atrapé señal distinta de SIGINT: " + signum);
    } else {
        log_info("worker: SIGINT");
        sig_quit = true;
    }
}

std::string getTopicFn(std::string topic) {
    return std::string(SERVER_DB_TOPICS_DIR) + topic + SERVER_DB_FILE_EXT;
}

std::string getSubFn(int id) {
    return std::string(SERVER_DB_SUBS_DIR) + std::to_string(id) + SERVER_DB_FILE_EXT;
}

void setError(struct msg_t *m, const char *s) {
    m->id = -1 * m->id;
    strcpy(m->msg, s);
}


int main(int argc, char* argv[]) {
    register_handler(SIGINT_handler);

    // Obtiene colas
    int q_req = qget(SERVER_REQ_Q_ID);
    int q_rep = qget(SERVER_REP_Q_ID);
    if (q_req < 0 || q_rep < 0) {
        log_error("worker: Error al crear msg queue. Freno");
        exit(-1);
    }
    // Mappeo id local-global; shm con id siguiente y sem para acceder a ella
    std::map<int,int> ids;      // Concurrencia: Eventualmente mover a shm. Alternativa: hacer workers con threads
    int next_id_sem = getsem(SERVER_NEXT_ID_SEM_ID);
    int next_id_shm = getshm(SERVER_NEXT_ID_SHM_ID);
    int* next_id_p = (int*) mapshm(next_id_shm);

    while (!sig_quit) {
        struct msg_t m;
        // Obtengo próximo mensaje que necesite ser procesado
        if (qrecv(q_req, &m, sizeof(m), 0) < 0) {
            if (sig_quit) break;
            log_error("worker: Error al recibir mensaje por cola. Sigo");
            continue;
        }
        log_debug("worker: Recibí mensaje por cola:");//
        m.show();//

        switch (m.type) {
            case CREATE_MSG:    // Registro nuevo id y lo entrego en el msg
                {
                    int nuevo_id;
                    p(next_id_sem); {
                        nuevo_id = (*next_id_p)++;
                    } v(next_id_sem);
                    strcpy( m.msg, std::to_string(nuevo_id).c_str() );
                    if (!ids.insert(std::pair<int, int>(nuevo_id, m.mtype)).second) {
                        // Id ya estaba registrado???
                        log_error("worker: id ya registrado, cosa imposible. Freno");
                        exit(-1);
                    }
                }
                break;

            case SUB_MSG:     // Si id existe, en fs agrega sub al topic y topic al sub. Si topic no existía lo crea
                {
                    if (ids.find(m.id) != ids.end()) {
                        ///Validar input?
                        // Concurrencia: Eventualmente lockear archivos
                        int s = 0;
                        std::fstream s_fs, t_fs;
                        t_fs.open(getTopicFn(m.topic), std::fstream::in | std::fstream::out | std::fstream::app);
                        if (!t_fs.is_open()) {
                            log_debug(getTopicFn(m.topic).c_str());//
                        }

                        while (t_fs >> s && s != m.id) {}
                        if (s == m.id) {
                            setError(&m, "Id ya suscrito al topic");
                        } else {
                            // No lo encontré, lo suscribo
                            t_fs.clear();
                            t_fs << m.id << std::endl;  // Agrego id del sub al topic
                            s_fs.open(getSubFn(m.id), std::fstream::out | std::fstream::app);
                            log_debug(getSubFn(m.id).c_str());//
                            s_fs << m.topic << std::endl;   // Agrego topic al sub
                            s_fs.close();
                        }
                        t_fs.close();
                    } else {
                        log_info("worker: sub tiene id inexistente");
                        setError(&m, "Id no existe");
                    }
                }
                break;

            case PUB_MSG:     // Si id existe, envía msg a cada suscriptor. Si topic no existía lo crea
                {
                    if (ids.find(m.id) != ids.end()) {
                        // Preparo mensaje de difusión
                        struct msg_t m_dif;
                        m_dif.type = RECV_MSG;
                        strncpy(m_dif.topic, m.topic, MAX_TOPIC_LENGTH);
                        strncpy(m_dif.msg, m.msg, MAX_MSG_LENGTH);

                        int id_s;
                        std::ifstream t_ifs;
                        t_ifs.open(getTopicFn(m.topic), std::ifstream::in | std::ifstream::app);
                        // Difusión: Envío un mensaje por cada suscriptor
                        while (t_ifs >> id_s) {
                            std::map<int, int>::iterator it_rcp = ids.find(id_s);
                            if (it_rcp != ids.end()) {
                                m_dif.id = id_s;
                                m_dif.mtype = it_rcp->second;   // cfd para este id
                                qsend(q_rep, &m_dif, sizeof(m_dif));
                            } else {
                                log_error("worker: sub encontrado en topic pero inexistente en map de cfd. Sigo");
                            }
                        }
                        t_ifs.close();
                    } else {
                        log_info("worker: pub tiene id inexistente");
                        setError(&m, "Id inexistente, no se publicó");
                    }
                }
                break;

            case RECV_MSG:
                log_error("worker: Me llegó un RECV_MSG, no debería pasar");
                continue;

            case DESTROY_MSG:   // Si id existe, lo desuscribo de cada topic y borro
                {
                    std::map<int, int>::iterator id_it = ids.find(m.id);
                    if (id_it != ids.end()) {
                        int s;
                        std::string t;
                        std::ifstream s_ifs, t_ifs;
                        // Recorro archivo del sub agarrando sus topics
                        std::string s_fn = getSubFn(m.id);
                        s_ifs.open(s_fn, std::ifstream::in);
                        while (std::getline(s_ifs, t)) {
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
                        setError(&m, "Id inexistente");
                    }
                }
                break;

            default:
                log_error("worker: Tipo de mensaje no esperado. Freno");
                exit(-1);
        }

        // Envío mensaje respuesta
        qsend(q_rep, &m, sizeof(m));
        log_debug("worker: Devolví por cola el mensaje respuesta");//
    }

    unmapshm(next_id_p);
    return 0;
}
