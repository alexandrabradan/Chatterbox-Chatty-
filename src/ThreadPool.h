#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sys/types.h>

#include "TaskQueue.h"
#include "connections.h"
#include "stats.h"
#include "HashTable.h"
#include "GlobalVariables.h"

/**
 * @file ThreadPool.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni per creare, gestire e far terminare il ThreadPool di dimensione fissata.
 */

/**
 * @struct hibernation_node_t
 * @brief Struct che rappresenta un elemento della coda di ibernazione, sottoforma di file descriptor oppurtamente modificato (+ FD_SETSIZE=1024)\n
 * per distinguerlo dai "file descriptor" che NON sono in "attesa attiva" (relativi clients che NON hanno fatto -R). \n
 * La "coda di ibernazione", e' un array circolare circolare di dimensione finita, usata dal ThreadPool per gestire \n
 * l'attesa indefinita di uno o piu' messaggi da parte dei clients che fanno l'operazione "MENO_R_OP".\n
 *
 * HOW IT WORKS: il Listener preleva (se ci sono elementi) un file descriptor opportunamente modificato (sommo al fd\n
 * che la Select mi restituisce dall' "accept" di un client FD_SETSIZE) per differenziarlo dagli altri file descrpotor\n
 * della Select e sapere che sta facendo la "MENO_R_OP" e lo inserisce nella coda di lavoro.\n
 * Il worker che si incarica di questo file descriptor modificato,controlla se e' maggiore di FD_SETSIZE e se e' maggiore \n
 * va a verificare direttamente nello switchcase dell'operazione  "MENO_R_OP" se il client, nel mentre della sua ibernazione\n
 * (permanenza nella "coda di ibernazione") ,ha ricevuto un msg.\n
 * Nel caso lo avesse ricevuto glielo invia e lo reinserisce nel "working set", mentre se non ne ha ancora ricevuti, NON\n
 * lo reinserisce nel "working set", ma lo rimette nella "coda di ibernazione" e si ripete la ciclica.\n
 */
typedef struct{
    int fd;  /**<  "file descriptor ibernato"*/
} hibernation_node_t;

/**
 *  @struct hibernation_queue_t
 *  @brief Struct che rappresenta "coda di ibernazione" di un ThreadPool, sottoforma di un array circolare di dimensione\n
 *  finita.\n
 */
typedef struct{
    int empty; /**<  "flag che mi dice se la coda di ibernazione e' vuota o meno"*/
    int first; /**<  posizione del primo elemento da estrarre nella coda di ibernazione*/
    int insert; /**<  posizione del primo elemento da inserire nella coda di ibernazione*/
    size_t qhibsize; /**<  dimensione della coda di ibernazione*/
    hibernation_node_t* hibernateQueue; /**<  coda di ibernazione, sottoforma di array circolare*/
    pthread_mutex_t lockHibernateQueue; /**<  mutua esclusione coda di ibernazione*/
    pthread_cond_t fullHibernateQueueCond; /**< variabile di condizione coda di ibernazione piena*/
}hibernation_queue_t;

/**
 *  @struct threadpool_t
 *  @brief Struct che rappresenta ThreadPool
 */
typedef struct{
    int thread_started; /**< # worker-threads attivi/che hanno inziato a compiere lavoro*/
    size_t threads_in_pool; /**< # di worker-threads nel pool*/
    pthread_t* threads; /**< array che contiene ID degli worker-threads*/
    struct statistics* threadsStats; /**< array che contiene le statistiche di ogni worker-thraed (thread listener va a sommarle quando\n
                                      *  riceve SIGURS1) + numConnections viene aggiornato solo dal ThreadListener + NO MUTUA ESCLUSIONE perche' ogni\n
                                      *  thread va ad accedere solo al suo slot nell'array delle statistiche*/
    work_queue_t* workQueue; /**< coda di lavoro*/
    hibernation_queue_t* hibQueue; /**< coda di ibernazione*/
}threadpool_t;

/**
 * @struct arg_t
 * @brief Struct che raccoglie gli argomenti da passare ad un worker-thread quando lo si crea.
 */
typedef struct{
    threadpool_t* pool; /**< struct che raccoglie il Threadpool, la coda di lavoro e le informazioni ad esso legate*/
    size_t index; /**< indice dell'array degli ID che memorizza l' ID del worker-thread che si vuole creare nell'array, e che serve\n
 * ad ogni worker-thread per accedere all'array delle statistiche ed aggiornarle, in mutua esclusione (aggiorna lo\n
 * slot dell'array delle statistiche indicizzato dallo stesso indice che nell'array degli ID che lo identifica).*/
    ht_hash_table_t* ht; /**< riferimento alla HashTable + coda degli utenti conenessi sulle quali worker-threads devono lavorare*/
}arg_t;


/**
 * @brief Routine/Task da far eseguire ad un worker-thread. La funzione si occupa di aggiornare la struct threadpool
 * e la struct threadpool_task_t
 *
 * @param threadpool: ThreadPool
 */
void* threadpool_thread(void* args);

/**
 * @brief Funzione che crea ed inizializza un Threadpool avente un numero fisso di Workers e la relativa coda di lavoro,
 * di dimensione finita.
 *
 * @param threads_in_pool: # di threads nel ThreadPool
 * @param qtsize: dimensione della coda di lavoro
 * @param ht: riferimento alla HashTable + coda degli utenti conenessi sulle quali worker-threads devono lavorare
 * @return ritorna il ThreadPool creato in caso di successo, NULL in caso di fallimento
 */
threadpool_t* threadpool_create(size_t threads_in_pool, size_t qtsize, ht_hash_table_t* ht);

/**
 * @brief Funzione che stampa un Threadpool
 *
 * @param pool: ThreadPool da stampare
 */
void threadpool_print(threadpool_t* pool);

/**
 * @brief Funzione che fa terminare il ThreadPool, con "GRACEFUL SHUT DOWN" => thread non accetta piu'
 * nuovi task e porta a termine quelli che ancora deve compiere.
 *
 * @param pool: il ThreadPool da distruggere
 */
void threadpool_destroy(threadpool_t* pool);

/**
 * @brief Funzione che dealloca il ThreadPool e la relativa coda di lavoro.
 *
 * @param pool: ThreadPool da deallocare
 */
void threadpool_free(threadpool_t* pool);


/*************************************WORKER ROUTINES*****************************************************************/

 //L'AGGIORNAMENTO DELLE STATISTICHE LOCALI DI OGNI WORKER-THREAD, IL MESSAGGIO DA INVIARE, COME ESITO, AL CLIENT  E\n
 //L'EVENTUALE DEALLOCAZIONE DEL BUFFER CHE CONTIENE IL BODY DELLA RISPOSTA AL CLIENT VENGONO FATTI  NELLA FUNZIONE-ROUTINE
 //CHE SODDISFA LA RELATIVA RICHIESTA.




/**
 * @brief Funzione che invia i messaggi pendenti (quelli inviati da altri utenti, inseriti nella historyQueue e non\n
 * anocra recapitati) ad un utente.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void getPendingMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che setta l'invio del messaggio di risposta al client, in seguito all'esito della routine per\n
 * soddisfare una sua richiesta.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param op: esito della routine
 * @param sender: mittente del msg
 * @param request: richiesta del client da soddisfare
 * @param len: lunghezza del body del messaggio di risposta (se presente)
 * @param text: contenuto del body del messaggio di risposta (se presente)
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"
 * o meno @param fd, una volta che ho soddisfatto la richiesta.
 * @param freeMmapped: flag che indica se liberare buffer messaggio di risposta o meno (chiamato cosi per discernere buffer statico\n
 * resitutito dalla mappatura di un file di memoria che Server manda come risposta al client e che non deve essere liberato)\n
 * @return ritorna 0 nel caso in cui sia stato possibile inviare il messaggio di risposta al client, 1 quando non sono\n
 * stati trovati messaggi pendenti da invuare al client, -1 se il client si e' disconesso
 */
int sendResponseToClient(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd,  op_t op, char* sender, message_t* request, unsigned int len, char* text, int* reinsertInWorkingSet, int freeMmapped);

/**
 * @brief Funzione che si antepone alll'invio del messaggio di risposta al client @param fd, in seguito all'esito della routine per\n
 * soddisfare una sua richiesta. Cio' viene fatto per discernere se il buffer di risposta e' statico (come nel caso della mappatura\n
 * di un file nella memoria virtuale del processo) oppure dinamico, e di conseguenza deve essere liberato.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param op: esito della routine
 * @param sender: mittente del msg
 * @param request: richiesta del client da soddisfare
 * @param len: lunghezza del body del messaggio di risposta (se presente)
 * @param text: contenuto del body del messaggio di risposta (se presente)
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"
 * o meno @param fd, una volta che ho soddisfatto la richiesta.
 * @return ritorna 0 nel caso in cui sia stato possibile inviare il messaggio di risposta al client, 1 quando non sono\n
 * stati trovati messaggi pendenti da invuare al client, -1 se il client si e' disconesso
 */
int sendResponse(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, op_t op, char* sender,  message_t* request, unsigned int len, char* buf, int* reinsertInWorkingSet);

/**
 * @brief Funzione che ricerca il messaggio pendente piu' veccchio (quello inserito per primo e NON ancora inviato)\n
 * e nel lo trovi, lo invia a @param fd, altrimenti non fa nulla.
 *
 * N.B. TALE FUNZIONE VIENE UTILIZZATA PER LA GESTIONE DELL'OPERAZIONE "MENO_R_OP" PER INVIARE GLI "n" MESSAGGI CHE\n
 * IL CLIENT SI ASPETTA.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"
 * o meno @param fd, una volta che ho soddisfatto la richiesta
 * @return ritorna 0 nel caso in cui sia prensente nella History dell'utente @param fd un messaggio pendente e sia stato\n
 * possibile inviarlo, -1 se NON c'e' nessun messaggio pendente oppure NON e' stato possibile inviare il messaggio al client.
 */
int sendMinusRMessage(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, int* reinsertInWorkingSet);

/**
 * @brief Funzione che legge la <<sendRequest>> con l'operazione MENO_R_OP che il Client invia al Client quando fa la\n
 * <<readMsg>>.\n
 *\n
 * N.B. CLIENT RICHIEDE "readMsg" IN 2 OCCASIONI:\n
 * 1. '-R' => client  riceve un messaggio da un nickname. In base al valore di n il comportamento e' diverso, se:\n
 *      a) n > 0 : aspetta di ricevere 'n' messaggi e poi passa al comando successivo (se c'e')\n
 *      b)n <= 0: aspetta di ricevere uno o piu' messaggi indefinitamente (eventuali comandi che  seguono non verranno eseguiti)\n
 *\n
 * 2. '-p' => client richiede la history e riceve per ogni messaggio della sua history dal Server, hdr con OP_TXT_MESSAGE\n
 * e data contentente il buf del contenuto del messaggio.\n
 *
 * Ne segue che questo "escamotage" per conoscere quando il cliet fa MENO_R_OP, deve essere ignorato dall'operazione\n
 * <<getPreviousMessages>> e tale funzione fa appunto cio', ossia ignorare i "messaggi MENO_R_OP" che il client invia\n
 * mettendosi in attesa dell'invio dei messaggi della sua History da parte del Server.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * @return ritorna 0 se la lettura del messsaggio MENO_R_OP che il client ha inviato tramite la <<readMsg>> e' andato a\n
 * buon fine, -1 altrimenti.
 */
int readInterceptAndIgnoreMinusR(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, int* reinsertInWorkingSet);

/**
 * @brief Funzione che va ad inviare i messaggi della historyQueue.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descritor del client al quale bisogna scrivere esito routine
 * @param request: richiesta del client da soddisfare
 * @param msgsQueue: struct che contiene la coda dei messaggi dell'utente @param username e le informazione che la riguardano
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente,, una volta che ho soddisfatto la richiesta.
 */
void sendResponseAndMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, history_queue_t* msgsQueue, int* reinsertInWorkingSet);

/**
 * @brief Funzione che registra un utente alla chat, andando ad inserirlo nella HashTable nel caso in\n
 * cui non sia gia' presente.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # utenti registrati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"
 * o meno il file descriptor dell'utente,, una volta che ho soddisfatto la richiesta.
 */
void registerUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che connete un utente alla chat, andando ad inserirlo nella coda degli utenti connessi,/n
 * in caso ci sia ancora spazio per nuovi utenti ed in caso l'utente sia registrato alla chat.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # utenti connessi\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void connectUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd,  message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che inserisce il messaggio che un utente vuole inviare ad un destinatario, nella\n
 * historyQueue di quest'ultimo, in caso tale destinatario sia un utente registarto alla chat. L'invio effettivo\n
 * di tale messaggio avvera' tramite l'operazione "MENO_R_OP".\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # messagi NON ancora inviati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void postMessage(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che inserisce il messaggio che un utente vuole inviare a tutti gli utenti della chat, nella\n
 * loro historyQueue. Per fare cio',l'utente acquisisce la mutua esclusione su un bucket della HashTable e\n
 * scorrendo la lista di trabocco, inserisce nella HistoryQueue di ogni utente che collide su quel bucket il messaggio.\n
 * L'invio effettivo di tale messaggio avvera' tramite l'operazione "MENO_R_OP".\n
 * ricihiedera' un servizio al Server.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # messagi NON ancora inviati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void postMessageToAll(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che inserisce il file che un utente vuole inviare ad un destinatario, nella\n
 * historyQueue di quest'ultimo, in caso tale destinatario sia un utente registarto alla chat. L'invio effettivo\n
 * di tale messaggio avvera' tramite l'operazione "MENO_R_OP".\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # file NON ancora inviati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void postFile(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che restituisce la dimensione del contenuto del file @param filename, attingendo tale informazione\n
 * dalla struct "stat" che contiene le informazioni sul file (all'atto della chiamata e' gia' stata verificata l'esistenza del file).
 *
 * @param filename: nome del file di cui bisogna restituire la dimensione
 * @return ritorna la dimensione del file
 */
size_t getFileSize(char* filename);

/**
 * @brief Funzione che invia il file che un utente  vuole repererire dalla sua historyQueue , se tale file\n
 * e' preente nella sua historyQueue (ossia gli e' stato inviato da un altro utente).\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # file inviati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void getFile(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che restituisce la history dei messaggi di un utente, inviando uno ad uno i messaggi.\n
 * La funzione fa ausilio della funzione "sendResponseAndMessages" per l'invio uno ad uno dei messaggi.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente una volta che ho soddisfatto la richiesta.
 */
void getPreviuosMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che restituisce i nicknames degli utenti connessi ad un utente, inviandogli tali nomi un array.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o menoil file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void getConnectedUsers(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che deregistra un utente dalla chat, andando ad eliminarlo dalla HashTable nel caso in\n
 * cui sia presente.\n
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # utenti registrati\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.\n
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void unregisterUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet);

/**
 * @brief Funzione che disconnete un utente dalla chat, andando a rimuoverlo dalla coda degli utenti connessi.
 *\n
 * N.B. L'AGGIORNAMENTO DELLE "STATISTICHE LOCALI" DEL WORKER-THREAD CHE INVOCA TALE ROUTINE, OSSIA:\n
 * a) # utenti connessi\n
 * b) # errori\n
 * VIENE FATTA QUI.\n
 *\n
 * N.N.B L'INVIO DELL'ESITO DELL'OPERAZIONE AL CLIENT VIENE FATTA QUI.
 *
 * @param ht: HashTable
 * @param index: indice con il quale aggiornare le statistiche locali del thread nell'array delle statistiche
 * @param pool: ThreadPool
 * @param fd: file descriptor del client, sul quale inviargli esito della routine
 * @param request: richiesta del client da soddisfare
 * @param reinsertInWorkingSet: flag che se settato ad 1 consente al worker-thread di sapere se reinserire nel "working set"\n
 * o meno il file descriptor dell'utente, una volta che ho soddisfatto la richiesta.
 */
void disconnectUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd,  message_t* request, int* reinsertInWorkingSet);

/*************************************HIBERNATION FUNCTIONS***********************************************************/

/**
 * @brief Funzione che alloca ed inizializza la struct che raccogli i dati della coda di ibernazione utilizzata di un\n
 * Threadpool, sottoforma di coda circolare di dimensione finita, per memorizzare i "file descriptors" che hanno fatto\n
 * l'operazione "MENO_R_OP" o non hanno ancora ricevuto il numero sufficiente di messaggi per sbloccarsi e pertanto sono\n
 * in  "attesa indefinita."\n
 * Chiamata solo quando si costruisce il Threadpool.\n
 *
 * @param qtsize: dimensione della coda di lavoro
 * @return il puntatore alla struct creata in caso di successo, NULL in caso di fallimento
 */
hibernation_queue_t* initHibernationQueue(size_t qhibsize);


/**
 * @brief Funzione che cancella e dealloca la coda di ibernazione.\n
 * Chiamata solo quando si distrugge il Threadpool.
 *
 * @param hibernationQueue: coda di ibernazione da cancellare
 */
void deleteHibernationQueue(hibernation_queue_t* hibernationQueue);

/**
 * @brief Funzione che inserisce un "file descriptor" in fondo alla coda di ibernazione.
 *
 * @param hibernationQueue: coda di ibernazione in cui inserire "file descriptor"
 * @param fd: "file descriptor" da "ibernare"
 * @return ritorna 0 se l'inserimento del file descriptor e' andato a buon fine, -1 altrimenti
 */
int pushHibernationQueue(hibernation_queue_t* hibernationQueue, int fd);

/**
 * @brief Funzione che elimina il primo "fd ibernato" dalla coda di ibernazione.
 *
 * @param hibernationQueue: la coda di ibernazione dalla quale eliminare il primo "file descriptor ibernato"
 * @return ritorna il "file descriptor" piu' anziato (il primo inserito) nella coda di ibernazione (>0), -1 altrimenti
 */
int popHibernationQueue(hibernation_queue_t* hibernationQueue);

/**
 * @brief Funzione per stampare la coda di ibernazione.
 *
 * @param hibernationQueue: coda di ibernazione da stampare
 */
void printHibernationQueue(hibernation_queue_t* hibernationQueue);


#endif //THREADPOOL_H


