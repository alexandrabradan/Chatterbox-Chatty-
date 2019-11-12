#ifndef HISTORYQUEUE_H
#define HISTORYQUEUE_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "message.h"
#include "Parser.h"

/**
 * @file HistoryQueue.h
 *  @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni per craere, gestire ed deallocare la coda dei messagi di un utente,\n
 * sottoforma di un array circolare di dimensione finita e memorizzata nella HashTable che raccoglie l'utente relativo.\n
 *\n
 */

/**
 * @struct msgInQueue_t
 * @brief Funzione che rapprensenta un messaggio memorizzato nella historyQueue di un utente
 */
typedef struct{
    int send;  /**< flag che indica se il messaggio e' stato inviato o meno*/
    message_t msg;  /**< messaggio che e' stato recapitato all'autente*/
}msgInQueue_t;

/**
 *  @struct history_queue_t
 *  @brief Struct che rappresenta la coda dei messagi di un utente, sottoforma di un array circolare di dimensione\n
 *  finita.
 */
typedef struct{
    int empty;  /**< falg che mi dice se la history e' vuota o meno*/
    int first;  /**<  posizione del primo elemento da estrarre nella coda di dei messaggi dell'utente*/
    int insert;  /**< posizione del primo elemento da inserire nella coda dei messaggi dell'utente*/
    size_t qhsize;  /**< dimensione della coda dei messaggi dell'utente*/
    msgInQueue_t* messagesQueue;  /**< coda dei messaggi dell'utente, sottoforma di array circolare*/
}history_queue_t;


/**
 * @brief Funzione che alloca ed inizializza una struct che raccogli i dati di una coda di messaggi relativa ad un utente,\n
 * sottoforma di coda circolare di dimensione finita.\n
 * Chiamata solo quando si costruisce la HashTable che raccoglie gli utenti della chat.
 *
 * @param qhsize: dimensione della coda di messaggi di un utente
 * @return il puntatore alla struct creata in caso di successo, NULL in caso di fallimento
 */
history_queue_t* initHistoryQueue(size_t qhsize);

/**
 * @brief Funzione che inserisce un messaggio in fondo alla coda di messaggi di un utente.
 *
 * @param workQueue: coda di messaggi in cui inserire messaggio
 * @var msg: messaggio da inserire  nella coda di messaggi
 */
void pushMessage(history_queue_t* historyQueue, op_t op, char *sender, char *rcv, const char *buf, unsigned int len);

/**
 * @brief Funzione che elimina il primo messaggio dalla coda di messaggi di un utente.
 *
 * @param historyQueue: la coda di messaggi dalla quale eliminare il primo messaggio
 */
void popMesssage(history_queue_t* historyQueue);

/**
 * @brief Funzione che cancella e dealloca una coda di messaggi relativa ad un utente.\n
 * Chiamata solo quando si cancella la HashTable che raccoglie gli utenti della chat, oppure quando un utente si deregistra.
 *
 * @param historyQueue: coda di messaggi di un utente da cancellare
 */
void deleteHistoryQueue(history_queue_t* historyQueue);

/**
 * @brief Funzione per stampare una coda di messaggi di un utente.
 *
 * @param historyQueue: coda di messaggi da stampare
 */
void printHistoryQueue(history_queue_t* historyQueue);

#endif //HISTORYQUEUE_H
