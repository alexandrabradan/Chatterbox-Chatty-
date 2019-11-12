#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>
#include "config.h"
#include <math.h>

#include "OnlineQueue.h"
#include "UsersQueue.h"
#include "CheckSysCall.h"


/**
 * @file HashTable.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che contiene le funzioni per allocare, gestire e deallocare la HashTable,  rappresentata come un\n
 * array associativo di dimensione finita (la dimensione e' in funzione del numero di connessioni estrappolato dal\n
 * file di configurazione moltiplicato per una potenza del due per facilitare la mutua esclusione dall'array di locks), \n
 * che risolve le collisioni con il metodo del "SEPARATE CHAINING".\n
 */

/**
 * @struct ht_hash_table_t
 * @brief Struct che raccoglie la HashTable degli utenti registrati alla chat e le informazzioni sulla HashTable.\n
 */
typedef struct ht_hash_table{
    size_t size; /**<  dimensione (# buckets) della HashTable*/
    ht_item* items; /**<  "array associtivo che rappresenta la HashTable*/
    size_t sizeLockItems; /**<  dimensione (# entries) array di locks che consente accesso in mutua esclusione a porzioni della Hashtable*/
    pthread_mutex_t* lockItems; /**<  array di lock per poter usare in mutua esclusione buckets della HashTable.*/
    online_queue_t* onlineQueue; /**<  struct che contiene coda degli utenti connessi ed informazioni ad essa associate*/
} ht_hash_table_t;



/**
 * @brief Funzione che alloca una HashTable di dimensione finita.\n
 * Funzione chiamata solo all'atto di creazione del server <<Chatty>>.
 *
 * @return: ritorna la HashTable creata in caso di successo,  NULL in caso di fallimento
 */
ht_hash_table_t* initHashTable(size_t size, size_t sizeLockItems);

/**
 * @brief Funzione hash che calcola l'indice  di un elemento da inserire nella hash-table.\n
 * Richiamata dalla funzione "ht_get_hash".
 *
 * @param s: elemento che si vuole inserire nella hash
 * @param a: numero primo, piu' grande di 128 (rapprensentazione numerica dell'alfabeto ASCII)
 * @param m: # max di buckets, ossia la dimensione massima della hash-table
 * @return un numero >=0 in caso di successo che rappresenta l'indice del bucket in cui mettere elemento, -1 in caso di fallimento
 */
int ht_hash(const char* s,  int a,  int m);

/**
 * @brief Funzione che calcola l'indice del bucket in cui inserire il nuovo utente, risolvendo eventuali\n
 * collisioni con le liste di trabocco.
 *
 * @param username: nickname di cui bisogna fare hashing
 * @return: l'hashing della nickname in caso di successo[>=0], -1 altrimenti
 */
int getHash(ht_hash_table_t* ht, char* username);

/**
 * @brief Funzione che inserisce un nuovo utente nella HashTable, andando ad inserirlo nel bucket\n
 * indicizzato con l'hashing del suo nickname, in fondo alla lista di trabocco di tale bucket.
 *
 * @param ht: hash-table in cui inserire l'utente
 * @param username: nickname dell'utente da inserire nella HashTable
 * @return ritorna 0 in caso di successo, OP_NICK_ALREADY=26 in caso il nickname di @param username sia gia' in uso,
 * -1 in caso di fallimento
 */
int insertUserInHashTable(ht_hash_table_t *ht, char *username);

/**
 * @brief Funzione che inserisce il messaggio di richiesta (si chiama cosi perche si cerca di soddisfare la richiesta\n
 * di un  client che vuole inviare un messaggio/file ad un altro client) nella historyQueue del destinatario\n
 * (CIO' E' FATTO PER SIMULARE L'INVIO DI UN MSG/FILE AD UN UTENTE, l'invio effettivo verra' fatto quando il\n
 * destinatario  lo richiedera' un esplicitamente al Server).
 *
 * @param ht: hash-table in cui cercare destinatario e relativa historyQueue
 * @param sender: mittente del messaggio/file
 * @param receiver: destinatario del messaggio/file
 * @param request: messaggio/file da inserire nella historyQueue del destinatario
 * @param op: hdr del messaggio (TXT_MESSAGE) o del file (FILE_MESSAGE) da memorizzare nella historyQueue del destinatario
 * @return: ritorna 0 in caso di successo, OP_NICK_UNKNOWN=27 in caso il nickname del destinatario\n
 * non esista (non sia memorizzato nella HashTable), -1 in caso di fallimento
 */
int insertMessageInHashTable(ht_hash_table_t *ht, char* sender, char* receiver, message_t* request, op_t op);

/**
 * @brief Funzione che dato un utente, lo ricerca nella HashTable e se e' presente lo cancella,\n
 * non fa nulla in caso contrario.
 *
 * @param ht: HashTable in cui cercare l'utente cancellarlo
 * @return  ritorna 0 se l'utente era presente nella lista di trabocco ed e' stato eliminato, OP_NICK_UNKNOWN=27 in caso\n
 * non fosse presente nella lista di trabocco  oppure - 1 se ci sono stati errori
 */
int removeUserInHashTable(ht_hash_table_t *ht, char *username);

/**
   @brief Funzione che ricerca un utente nella lista di trabocco relativa, inizializzando lo slot che contine l'utente\n
 * nella lista degli utenti connessi. contenente. Cio' serve per efficientizzare le operazioni fatte sulla onlineQueue.\n
 * Tale funzione, infatti, viene chiamata solo per inizializzare la coda degli utenti connessi.\n
 * Nel caso in cui l'utente non sia presente nella HashTable @param onlineUser rimane vuoto (e' compito del chiamante gestire errore).
 *
 * @param ht: HashTable in cui cercare utente e inizializzare, nel caso esso vi sia presente, le informazioni utili per\n
 * localizzarlo nella lista di trabocco del bucket relativo.
 * @param fd: file descriptort dell'utente connesso, con cui indicizzare coda degli utenti connessi
 * @param username: chiave da cercare nella HashTable e se presente con cui inizializzare  onlineUser
 * @return: ritorna 0 se l'utente era presente nella HashTable ed e' stato possibile inizialare onlinQueue[fd],\n
 * OP_NICK_UNKNOWN=27 in caso non fosse presente nella HashTable, oppure -1 se ci sono stati errori
 */
int searchUserInHashTable(ht_hash_table_t *ht, int fd, char *username);

/**
 * @brief Funzione che inserisce un utente che lo richiede nella posizione "fd" ("file descriptort" assegnato dalla\n
 * "Select" all'user) della coda degli utenti connessi, inizializzando il nodo relativo a "fd" nella coda degli utenti\n
 * connessi con le informazioni utili per localizzare @param username nella HashTable (cio' viene fatto per efficientizzare\n
 * le operazioni che client @param fd richiedera') in caso l'username sia presente nella HashTable, non facendo cio'\n
 * altrimenti.\n
 *\n
 * LA FUNZIONE FA AUSILIO DELLA FUNZIONE "searchUserInHashTable"\n
 *
 * @param ht: HashTable nella quale ricercare utente da connettere
 * @param fd: file descriptort dell'utente connesso, con cui indicizzare coda degli utenti connessi
 * @param username: nickname dell'utente da inserire nella coda degli utenti connessi
 * @return: ritorna 0 se la connessione dell'utente  fd e' andata, perche' esso era presente nella HashTable ed e'\n
 * stato possibile inizialare onlinQueue[fd],  OP_NICK_UNKNOWN=27 in caso non fosse presente nella HashTable oppure\n
 * -1 se ci sono stati errori.
 */
int pushOnlineUser(ht_hash_table_t* ht, int fd, char* username);

/**
 * @brief Funzione che dealloca una HashTable\n
 * Funzione chiamata solo all'atto di distruzione del server <<Chatty>>.
 *
 * @param ht: HashTable da deallocare
 */
void deleteHashTable(ht_hash_table_t* ht);

/**
 * @biref Funzione che stampa una HashTable.
 *
 * @param ht: HashTable da stampare
 */
void printHashTable(ht_hash_table_t* ht);


#endif //HASHTABLE_HASHTABLE_H
