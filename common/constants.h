//
// Created by martin on 28/04/18.
//

#ifndef SD1_SCM_CONSTANTS_H
#define SD1_SCM_CONSTANTS_H

// Colas
#define LOCAL_REP_Q_ID 1
#define LOCAL_REQ_Q_ID 2
#define STORED_MESSAGES_ID 3
#define SERVER_REQ_Q_ID 4
#define SERVER_REP_Q_ID 5
#define SERVER_RINGSEND_Q_ID 6
#define SERVER_RINGRECV_Q_ID 7

// Semáforos
#define LOCAL_IDS_SEM_ID 1
#define SERVER_NEXT_ID_SEM_ID 2

// Memoria compartida
#define LOCAL_IDS_SHM_ID 1
#define SERVER_NEXT_ID_SHM_ID 2

// Constantes
#define MAX_TOPIC_LENGTH 30
#define MAX_MSG_LENGTH   280
#define IP_SERVER "127.0.0.1"
#define PUERTO_NODO_RING 8000   // Puerto como nodo del ring
#define PUERTO_SERVER    8100   // Puerto para los brokers
#define CANT_SERVER_WORKERS 1   ///Para que pueda ser mayor a 1, falta concurrencia en worker
#define LOCAL_FIRST_ID  1   // 101 ? Menor es mejor
#define LOCAL_MAX_ID    1000
#define SERVER_FIRST_ID 1001
#define SERVOFF_TIMER_SEC 2
#define RETCONN_TIMER_SEC 8

// Directorios
//#define SERVER_DB_ROOT_DIR    "./scm-db/"
#define SERVER_DB_TOPICS_DIR  "./scm-db/topics/"
#define SERVER_DB_SUBS_DIR    "./scm-db/subs/"
#define SERVER_DB_FILE_EXT    ".txt"

#endif //SD1_SCM_CONSTANTS_H
