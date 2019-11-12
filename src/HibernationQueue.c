#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ThreadPool.h"


/**
 * @file HibernationQueue.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzione per allocare, gestire e deallocare la coda di ibernazione, contenute nell'interfaccia\n
 * ThreadPool.h.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

hibernation_queue_t* initHibernationQueue(size_t qhibsize) {
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "initHibernationQueue";

    hibernation_queue_t *hibernationQueue = NULL;
    if ((hibernationQueue = (hibernation_queue_t *) calloc(1, sizeof(hibernation_queue_t))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare struct della coda di ibernazione in %s\n", errMessage);
        free(hibernationQueue);
        exit(EXIT_FAILURE);
    }

    hibernationQueue->qhibsize = qhibsize;

    hibernationQueue->hibernateQueue = NULL;
    if ((hibernationQueue->hibernateQueue = (hibernation_node_t *) calloc(hibernationQueue->qhibsize,
                                                                          sizeof(hibernation_node_t))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare coda di ibernazione in %s\n", errMessage);
        free(hibernationQueue->hibernateQueue);
        free(hibernationQueue);
        exit(EXIT_FAILURE);
    }

    hibernationQueue->empty = 1;
    hibernationQueue->first = 0;
    hibernationQueue->insert = 0;
    lockInit(&hibernationQueue->lockHibernateQueue, errMessage);
    condInit(&hibernationQueue->fullHibernateQueueCond, errMessage);

    return hibernationQueue;
}

void deleteHibernationQueue(hibernation_queue_t* hibernationQueue){

    //ho gia'mutua esclusione tramite threadpool_free

    //dealloco coda di ibernazione
    free(hibernationQueue->hibernateQueue);

    //dealloco struct coda di ibernazione
    free(hibernationQueue);
}

int pushHibernationQueue(hibernation_queue_t* hibernationQueue, int fd){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "pushHibernationQueue";

    lockAcquire(&hibernationQueue->lockHibernateQueue, errMessage);

    while((hibernationQueue->insert == hibernationQueue->first) && (hibernationQueue->empty == 0))
        condWait(&hibernationQueue->fullHibernateQueueCond, &hibernationQueue->lockHibernateQueue, errMessage);

    //insrisco il fd nella coda di lavoro
    hibernationQueue->hibernateQueue[hibernationQueue->insert].fd = fd;

    //incremento posizione di inserimento dell'array di ibernazione circolare
    hibernationQueue->insert = (hibernationQueue->insert + 1) % ((int)hibernationQueue->qhibsize);
    if(hibernationQueue->empty == 1)
        hibernationQueue->empty = 0;

    //segnalo che ho inserito un elemento nella coda di lavoro
    lockRelease(&hibernationQueue->lockHibernateQueue, errMessage);

    return FUNCTION_SUCCESS;
}

int popHibernationQueue(hibernation_queue_t* hibernationQueue){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "popHibernationQueue";

    lockAcquire(&hibernationQueue->lockHibernateQueue, errMessage);

    if(hibernationQueue->empty == 1){
        lockRelease(&hibernationQueue->lockHibernateQueue, errMessage);
        return -1; //listener-thread puo' andare avanti senza dover inserire un "fd ibernato" nella coda di lavoro
    }

    //memorizzo fd da restituire
    int fd = hibernationQueue->hibernateQueue[hibernationQueue->first].fd;

    //cancello task prelevato
    memset(&(hibernationQueue->hibernateQueue[hibernationQueue->first]), 0, sizeof(hibernation_node_t));

    //incremento indice di estarzione nell'array di lavoro circolare
    hibernationQueue->first = (hibernationQueue->first + 1) % ((int)hibernationQueue->qhibsize);

    //se avevo un solo task nella coda di lavoro, eliminandolo, la coda e' vuota
    if(hibernationQueue->first == hibernationQueue->insert)
        hibernationQueue->empty = 1;

    //segnalo che ho eleminato un elemento dalla coda di lavoro
    condSignal(&hibernationQueue->fullHibernateQueueCond, errMessage);
    lockRelease(&hibernationQueue->lockHibernateQueue, errMessage);

    return fd;
}

void printHibernationQueue(hibernation_queue_t* hibernationQueue){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "printHibernationQueue";

    lockAcquire(&hibernationQueue->lockHibernateQueue, errMessage);

    if(hibernationQueue->empty){
        printf("La coda di ibernazione e' vuota\n");
    }
    else {
        for(int i = 0; i < hibernationQueue->qhibsize; i++){
            printf("Ibernated FD[%d] %p = %d \n", i, (void*)&hibernationQueue->hibernateQueue[i], hibernationQueue->hibernateQueue[i].fd);//& == indirizzo struct
        }
    }
    printf("\n");
    lockRelease(&hibernationQueue->lockHibernateQueue, errMessage);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


