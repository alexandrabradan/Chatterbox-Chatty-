#ifndef ONLINEQUEUE_H
#define ONLINEQUEUE_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "UsersQueue.h"

/**
 * @file OnlineQueue.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni per craere, gestire ed deallocare la coda degli utenti connessi,\n
 * sottoforma di un array di dimensione finita e memorizzato nella HashTable che raccoglie gli utenti della chat.\n
 * L'indicizzazione dell'array dei connessi viene fatta tramite il "file descriptor" assegnato ad ogni client connesso\n
 * dalla "Select". Siccome la "Select" assegna ad ogni client un "file descriptort" univoco, e' proprio quest'ultimo\n
 * ad indicizzare la coda dei connessi ed a dare l'accesso all'array in  mutua esclsuione.\n
 *\n
 *
 * 
 */
/**
 *  @struct online_queue_t
 *  @brief Struct che rappresenta la coda degli utenti connessi, sottoforma di un array circolare di dimensione\n
 *  finita.
 */
typedef struct{
    size_t qosize; /**< dimensione della coda degli utenti connessi*/
    online_user_t* connectionQueue; /**< coda degli utenti connessi, sottoforma di array  di dimensione finita*/
}online_queue_t;


/**
 * @brief Funzione che alloca ed inizializza la struct che raccogli i dati della coda degli utenti connessi,\n
 * sottoforma di coda  di dimensione finita.\n
 * Chiamata solo quando si costruisce la HashTable che raccoglie gli utenti della chat.
 *
 * @param qosize: dimensione della coda degli utenti connessi
 * @return il puntatore alla struct creata in caso di successo, NULL in caso di fallimento
 */
online_queue_t* initOnlineQueue(size_t qosize);


/**
 * @brief Funzione che cancella e dealloca la coda degli utenti connessi.\n
 * Chiamata solo quando si cancella la HashTable che raccoglie gli utenti della chat.
 *
 * @param onlineQueue: coda degli utenti connessi da cancellare
 */
void deleteOnlineQueue(online_queue_t* onlineQueue);

/**
 * @brief Funzione che elimina l'utente in posizione @param fd nella coda degli utenti connessi, perche' quest'ultimo\n
 * si e' disconesso.
 *
 * @param historyQueue: la coda degli utenti dalla quale eliminare l'utente in posizione fd
 * @param fd:  "file descriptor" che indicizza utente da eliminare nella coda degli utenti connessi
 * @param username: username dell'utente da disconettere
 * @return: ritorna 0 se la "disconessione" dell'utente e' andata a buon fine, -1 altrimenti
 */
int  popOnlineUser(online_queue_t* onlineQueue, int fd, char* username);

/**
 * @brief Funzione per stampare la coda degli utenti connessi.
 *
 * @param onlineQueue: coda degli utenti connessi da stampare
 */
void printOnlineQueue(online_queue_t* onlineQueue);

#endif //ONLINEQUEUE_H
