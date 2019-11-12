#ifndef CHECKSYSCALL_H
#define CHECKSYSCALL_H

#include <sys/select.h>
#include <pthread.h>


/**
 * @file CheckSysCall.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 *
 * @brief Interfaccia che contiene i metodi utili per verificare la buona riuscita o meno di una System Call.
 */

/**
 * @brief Funzione che verfiica la buona inizializzazione di una variabile di mutua esclusione, o meno.
 * @param mtx: variabile di mutua esclusione da iniizializzare
 * @param errMessage: stringa da stampare in caso di errore
 */
void lockInit(pthread_mutex_t* mtx, char* errMessage);

/**
 * @brief Funzione che verifica la buona acquisizione della mutua esclusione, o meno.
 *
 * @param mtx: variabile di mutua esclusione da acquisire
 * @param errMessage: stringa da stampare in caso di errore
 */
void lockAcquire(pthread_mutex_t* mtx, char* errMessage);

/**
 * @brief Funzione che verifica il buon rilascio della mutua esclusione, o meno.
 *
 * @param mtx: variabile di mutua esclusione da rilasciare
 * @param errMessage: stringa da stampare in caso di errore
 */
void lockRelease(pthread_mutex_t* mtx, char* errMessage);

/**
 * @brief Funzione che verifica la buona inizializzazione di una variabile di condizione, o meno.
 * @param mtx: variabile di condizione da iniizializzare
 * @param errMessage: stringa da stampare in caso di errore
 */
void condInit(pthread_cond_t* cond, char* errMessage);

/**
 * @brief Funzione che verifica la buona sospensione su una variabile di condizione, o meno.
 *
 * @param cond: variabile di condizione su cui sospendersi
 * @param mtx: variabile di mutua esclusione da rilasciare
 * @param errMessage: stringa da stampare in caso di errore
 */
void condWait(pthread_cond_t *cond, pthread_mutex_t* mtx, char* errMessage);

/**
 * @brief Funzione che verifica il buon segnalamento di una variabile di condizione, o meno.
 *
 * @param cond: variabile di condizione di cui segnalare cambiamento di stato
 * @param errMessage: stringa da stampare in caso di errore
 */
void condSignal(pthread_cond_t* cond, char* errMessage);

/**
 * @brief Funzione che verifica il buon segnalamento di una variabile di condizione, o meno.
 *
 * @param cond: variabile di condizione di cui segnalare cambiamento di stato a tutti i threads sospesi su di essa
 * @param errMessage: stringa da stampare in caso di errore
 */
void condBroadcast(pthread_cond_t* cond, char* errMessage);

/**
 * @brief Funzione che verifica la buona riuscita della "pthread_create", o meno.
 * @param tID: locazione dove memorizzare l'exit-status di un worker che ha terminato
 * @param errMessage: stringa da stampare in caso di errore
 */
void ptCreate(pthread_t* tID, void *(*function) (void *), void *arg, char* errMessage);

/**
 * @brief Funzione che verifica la buona riuscita della "pthread_join", o meno.
 *
 * @param tID: thread di cui aspettare la terminazione
 * @param errMessage: stringa da stampare in caso di errore
 */
void ptJoin(pthread_t tID, char* errMessage);

/**
 * @param Funzione per verificare il buon esito della System Call @param c
 *
 * @param r: risultato della System Call @param c
 * @param c: System Call da verificare buon esito
 * @param e: stringa da passare a "perror"
 */
void SYSCALL(int* r, int c, char* e );

/**
 * @brief Funchione che aggiorna il massimo descrittore attivo per la "select"
 *
 * @param set: working set
 * @param fd_num: massimo descrittore attivo
 * @return: il nuovo massimo descrittore attivo in caso di successo, -1 in caso di fallimento
 */
int updatemax(fd_set* set, const int* fd_num);

/**
 * @brief Funzione che chiude il client socket file descriptor @param fd di un client che ha inviato l' EOF al server,\n
 * eliminando @param fd dal working-set attivo @param set ed aggiornando il massimo file descriptor attivo per la "select"\n
 * @param fd_num. Dealloca la stringa @param string usata dal server per rispondere al client.
 *
 * @param fd: client socket file descriptor da chiudere
 * @param e: stringa da stampare per spiegare motivazione chiusura @param fd
 */
void CLOSECLIENT(const int* fd, char* e);


#endif //CHECKSYSCALL_H
