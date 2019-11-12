#ifndef E3_THREADPOOL_TASKQUEUE_H
#define E3_THREADPOOL_TASKQUEUE_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "CheckSysCall.h"
#include "config.h"

/**
 * @file TaskQueue.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni per craere, gestire ed deallocare la "coda di lavoro" usata dal Threadpool,
 * sottoforma di un array circolare di dimensione finita.
 */

/**
 *  @struct task_node_t
 *  @brief Struct che rappresenta un elemento della coda di lavoro, sottoforma di "file descriptort" appartenente "working set"\n
 *  che contiene tale descrittore e del numero massimo del file descriptor in uso dalla Select. \n
 *  Un worker-thread preleva tali informazioni dalla coda di lavoro, legge la richiesta del client identificato dal suo "file descriptor",\n
 *  la asseconda tramite la chiamata della routine corrispondente e rimette il file descriptor nel "set" (viene tolto dal listener-thread),\n
 *  ed aggionra fd_num.
 */
typedef struct{
    int fd; /**< "file descriptor" che rappresenta un client pronto per un-operazione di I/O*/
    int* fd_num; /**< puntatore al max "file descriptor" della Select*/
    fd_set* set; /**<  puntatore al "working set" dal quale toglie e reinserire, una volta soddisfatta la richiesta, il "file descriptor"*/
} task_node_t;

/**
 *  @struct work_queue_t
 *  @brief Struct che rappresenta la "coda di lavoro" del ThreadPool, sottoforma di un array circolare di dimensione finita.
 */
typedef struct{
    int empty; /**< flag che indica se la coda e' vuota o meno*/
    int first; /**< posizione del primo elemento da estrarre nella coda di lavoro*/
    int insert; /**< posizione del primo elemento da inserire nella coda di lavoro*/
    size_t qtsize; /**< dimensione coda di lavoro*/
    task_node_t* taskQueue; /**< coda di lavoro, sottoforma di array circolare*/
    pthread_mutex_t lockTaskQueue; /**<  mutua esclusione coda di lavoro*/
    pthread_cond_t emptyTaskQueueCond; /**< variabile di condizione coda di lavoro vuota*/
    pthread_cond_t fullTaskQueueCond; /**< variabile di condizione coda di lavoro piena*/
    int shutdown;  /**< flag che indica se il ThreadPool sta per terminare(necessario per il GRACEFUL SHUTDOWN)*/
}work_queue_t;


/**
 * @brief Funzione che alloca ed inizializza la struct che raccoglie la coda di lavoro e le informazioni su di essa usata dal ThreadPool.\n
 * Chiamata solo quando si costruisce il Threadpool.
 *
 * @param qtsize: dimensione della coda di lavoro
 * @return: ritorna il puntatore alla struct creata in caso di successo, NULL in caso di fallimento
 */
work_queue_t* initTaskQueue(size_t qtsize);


/**
 * @brief Funzione che cancella e dealloca una coda di lavoro.\n
 * Chiamata solo quando si distrugge il Threadpool.
 *
 * @param workQueue: coda di lavoro da cancellare
 */
void deleteTaskQueue(work_queue_t* workQueue);

/**
 * @brief Funzione che inserisce un "file descriptor" ed il suo "working set" in fondo alla coda di lavoro.
 *
 * @param workQueue: coda di lavoro in cui inserire task
 * @param fd: "file descriptor" che rappresenta un client pronto per un-operazione di I/O
 * @param set: puntatore al "working set" che contiene il file descriptor
 * @param fd_num: puntatore al max "file descriptort" della Select
 * @return: ritorna 0 se l'inserimento del Task e' andato a buon fine, -1 altrimenti
 */
int pushTask(work_queue_t* workQueue, int fd, fd_set* set, int* fd_num);

/**
 * @brief Funzione che elimina il primo task dalla coda di lavoro, per soddisfarlo.
 *
 * @param workQueue: la coda di lavoro dalla quale eliminare il primo task
 * @param popedTask:  task eliminato in caso di successo, NULL in caso di fallimento
 * @return: ritorna 0 se l'eliminazione del primo Task e' andato a buon fine, -1 altrimenti
 */
int popTask(work_queue_t* workQueue, task_node_t* popedTask);

/**
 * @brief Funzione per stampare la coda di lavoro.
 *
 * @param workQueue: coda di lavoro da stampare
 */
void printTaskQueue(work_queue_t* workQueue);

#endif //TASKQUEUE_H
