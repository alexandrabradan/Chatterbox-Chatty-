#include <glob.h>

#if !defined(USERS_QUEUE_H)
#define USERS_QUEUE_H

#include "HistoryQueue.h"

/**
 * @file UsersQueue.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che contiene le funzioni di utilita' per gestire la lista di trabocco degli utenti il cui username\n
 * collide su uno stesso bucket della HashTable.
 */

/**
 * @struct userNode_t
 * @brief Nodo della lista di trabocco degli utenti che collidono sullo stesso bucket della HashTable.\n
 */

typedef struct userNode{
    char* username; /**< nickname dell'utente [CHIAVE HASH-TABLE]*/
    struct userNode* nextUserNode; /**<  puntatore al successivo utente nella lista di trabocco*/
    struct userNode* previuosUserNode; /**< puntatore al precedente utente nella lista di trabocco*/
    history_queue_t* historyQueue; /**< struct che contiene la coda dei messaggi dell'utente [VALORE HASH-TABLE] ed informazioni legate ad essa*/
}userNode_t;


/**
 * @struct online_user_t
 * @brief Struct che rappresenta un utente connesso nella coda degli utenti connessi.
 */
typedef struct{
    int empty_struct;  /**< flag che mi dice se la struct e' vuota (liberata con  memset), oppure meno*/
    int bucket_index;  /**< indice del bucket in cui si trova l'utente (HASHING NOME UTENTE)*/
    char* username;  /**< nickname dell'utente connesso*/
    userNode_t* p_user_in_overflow_list;  /**<  puntatore al nodo che contiene l'utente, nella lista di trabocco del bucket[@var bucket_index]*/
}online_user_t;


/**
 * @struct ht_item
 * @brief Bucket di una hash-table, che contiene il riferimento ad una lista di trabocco nella quale si risolvono le\n
 * collisioni relative a questo bucket.
 */
typedef struct {
    size_t index; /**<  indice del bucket nella HashTable*/
    userNode_t* usersQueue; /**< lista di trabocco nella quale memorizzare gli username che collidono sullo stesso bucket*/
    size_t numUsers; /**< # di utenti che collidono sullo stesso bucket*/
    userNode_t* lastUser; /**< punatatore all'ultimo utente che collide nella lista di trabocco*/
} ht_item;

/**
 * @brief Funzione che restituisce 1 se nella lista di trabocco relativa un utente e' gia presente, 0 altrimenti.
 *
 * @param htitem: bucket di cui devo cercare utente duplicato nella lista di trabocco
 * @param username: nickname di cui devo cercare duplicato nella lista di trabocco relativa
 * @return ritorna 1 se l'utente e' gia' presente nella lista di trabocco del bucket, 0 altrimenti
 */
int searchDuplicateUsersQueue(ht_item* htitem, char* username);

/**
 * @brief Funzione che inserisce un utente in coda alla lista di trabocco del bucket relativo.
 *
 * @param htitem: bucket di cui devo inserrie utente lista di trabocco
 * @param username: utente da inserire in fondo alla lista di trabocco
 * @return ritorna 0 in caso di successo, OP_NICK_ALREADY=26 in caso nickname sia gia' presente nella HashTable, -1 in caso di errore
 */
int pushUsersQueue(ht_item* htitem, char* username);

/**
 * @brief Funzione che elimina un utente dalla lista di trabocco relativa, se tale elemento e' presente,\n
 * altrimeneti non fa nulla.
 *
 * @param htitem: bucket di cui devo eliminare l'utente dalla lista di trabocco
 * @param username: utente da cercare nella lista di trabocco ed eliminare
 * @return  ritorna 0 se l'utente era presente nella lista di trabocco ed e' stato eliminato, OP_NICK_UNKNOWN=27 se
 * l'utente non e' stato trovato nella lista di trabocco e pertanto non e' stato possibile eliminarlo , oppure -1 se ci sono stati \n
 * errori
 */
int popUsersQueue(ht_item* htitem, char* username);

/**
 * @brief Funzione che ricerca un utente nella lista di trabocco relativa, restituendo la struct online_user_t contenente\n
 * informazioni sulla localizzazione di tale utente nella lista di trabocco. \n
 * Tale funzione viene chiamata solo per inizializzare la coda degli utenti connessi.
 *
 * @param htitem: bucket di cui devo cercare utente nella lista di trabocco
 * @param username: username da cercare nella lista di trabocco
 * @param onlineUser: struct dove memorizzare le informazioni sulla localizzazione dell'utente,\n
 * nella lista di trabocco se l'utente e' presente (se l'utente non era presente nella lista di trabocco, la struct
 * non viene inizializzata ed e' compito del chiamate segnalare l'errore)
 */
void getUsersQueue(ht_item* htitem, char* username, online_user_t* onlineUser);

/**
 * @brief Funzione che restituisce la struct che contiene la coda dei messaggi di un utente ed informazioni\n
 * su di essa, in caso l'utente sia presente nella lista di trabocco del bucket, restituisce NULL in caso\n
 * contrario.
 *
 * @param htitem: bucket di cui devo cercare utente nella lista di trabocco
 * @param username: utente da cercare e di cui restituire la lista di trabocco
 * @return ritorna la struct che contiene la coda dei messaggi dell'utente ed informazioni su di essa\n
 * in caso si riesca a trovare l'utente, NULL altrimenti
 */
history_queue_t* getHistoryQueue(ht_item* htitem, char* username);

/**
 * @brief Funzione che cancella e dealloca la lista di trabocco di un bucket della HashTable./n
 * Chiamata solo all'atto di deallocazione della HashTable./n
 *
 * @param htitem: bucket di cui devo creare lista di trabocco
 */
void deleteUsersQueue(ht_item* htitem);

/**
 * @brief Funzione per stampare la lista di trabocco di un bucket.
 *
 * @param htitem: bucket di cui devo stampare lista di trabocco
 */
void printUsersQueue(ht_item* htitem);

#endif
