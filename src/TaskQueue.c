#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "TaskQueue.h"


/**
 * @file TaskQueue.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione dell'interfaccia TaskQueue.h, che raccoglie  le funzioni di utilita' per gestire la coda di lavoro,\n
 * sottoforma di array circolare di dimensione finita, usata dal ThreadPool.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

work_queue_t* initTaskQueue(size_t qtsize){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "initTaskQueue";

    work_queue_t* workQueue = NULL;
    if((workQueue = (work_queue_t*) calloc(1, sizeof(work_queue_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare coda di lavoro in %s\n", errMessage);
        free(workQueue);
        exit(EXIT_FAILURE);
    }

    workQueue->qtsize = qtsize;

    workQueue->taskQueue = NULL;
    if((workQueue->taskQueue = (task_node_t*) calloc(workQueue->qtsize, sizeof(task_node_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare coda di lavoro in %s\n", errMessage);
        free(workQueue->taskQueue);
        free(workQueue);
        exit(EXIT_FAILURE);
    }

    workQueue->empty = 1;
    workQueue->first = 0;
    workQueue->insert = 0;
    lockInit(&workQueue->lockTaskQueue, errMessage);
    condInit(&workQueue->emptyTaskQueueCond, errMessage);
    condInit(&workQueue->fullTaskQueueCond, errMessage);
    workQueue->shutdown = 0;

    return workQueue;
}

void deleteTaskQueue(work_queue_t* workQueue){

    //ho gia'mutua esclusione tramite threadpool_free

    //dealloco coda di lavoro
    free(workQueue->taskQueue);

    //dealloco struct coda di lavoro
    free(workQueue);
}

int pushTask(work_queue_t* workQueue, int fd, fd_set* set, int* fd_num){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "pushTask";

    lockAcquire(&workQueue->lockTaskQueue, errMessage);

    //Non posso accettare nuovi task nel caso Threadpool sia in chiusura [GRACEFUL SHUTDOWN]
    if(workQueue->shutdown == 1) {
        fprintf(stderr, "Errore Threadpool in chiusura in threadpool_add, non posso accettare nuovo: task da parte di  %d\n", fd);
        lockRelease(&workQueue->lockTaskQueue, errMessage);
        return FUNCTION_FAILURE;
    }

    while((workQueue->insert == workQueue->first) && (workQueue->empty == 0))
        condWait(&workQueue->fullTaskQueueCond, &workQueue->lockTaskQueue, errMessage);

    //insrisco il task nella coda di lavoro
    workQueue->taskQueue[workQueue->insert].fd = fd;
    workQueue->taskQueue[workQueue->insert].set = set;
    workQueue->taskQueue[workQueue->insert].fd_num = fd_num;

    //incremento posizione di inserimento dell'array di lavoro circolare
    workQueue->insert = (workQueue->insert + 1) % ((int)workQueue->qtsize);
    if(workQueue->empty == 1)
        workQueue->empty = 0;

    //segnalo che ho inserito un elemento nella coda di lavoro
    condSignal(&workQueue->emptyTaskQueueCond, errMessage);
    lockRelease(&workQueue->lockTaskQueue, errMessage);

    return FUNCTION_SUCCESS;
}

int  popTask(work_queue_t* workQueue, task_node_t* popedTask){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "popTask";

    lockAcquire(&workQueue->lockTaskQueue, errMessage);


    while((workQueue->empty == 1) && (workQueue->shutdown == 0)){
        condWait(&workQueue->emptyTaskQueueCond, &workQueue->lockTaskQueue, errMessage);
    }

    if(workQueue->empty == 1){
        //non setto "popedTask" => per worker-thread "popedTask rimane vuoto" e cosi sa che non deve svolgere lavoro
        //ed esce dal ciclo di lavoro e termina
        lockRelease(&workQueue->lockTaskQueue, errMessage);
    }
    else{

        //copio valori del task da eliminare  in "popedTask", per passarli al worker-thread e poter eliminare task
        //dalla coda di lavoro
        (*popedTask).fd = workQueue->taskQueue[workQueue->first].fd;
        (*popedTask).set = workQueue->taskQueue[workQueue->first].set;
        (*popedTask).fd_num =  workQueue->taskQueue[workQueue->first].fd_num;

        //cancello task prelevato
        memset(&(workQueue->taskQueue[workQueue->first]), 0, sizeof(task_node_t));

        //incremento indice di estarzione nell'array di lavoro circolare
        workQueue->first = (workQueue->first + 1) % ((int)workQueue->qtsize);

        //se avevo un solo task nella coda di lavoro, eliminandolo, la coda e' vuota
        if(workQueue->first == workQueue->insert)
            workQueue->empty = 1;

        //segnalo che ho eleminato un elemento dalla coda di lavoro
        condSignal(&workQueue->fullTaskQueueCond, errMessage);
        lockRelease(&workQueue->lockTaskQueue, errMessage);
    }

    return FUNCTION_SUCCESS;

}

void printTaskQueue(work_queue_t* workQueue){ //NON PROTETTA DA LOCK PERCHE' CHIAMATA SOLO DA THREADPOOL_PRINT
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "printTaskQueue";

    lockAcquire(&workQueue->lockTaskQueue, errMessage);

    if(workQueue->empty){
        printf("La coda di lavoro e' vuota\n");
    }
    else {
        for(int i = 0; i < workQueue->qtsize; i++){
            printf("Task[%d] %p = fd %d set %p fd_num %d\n", i, (void*)&workQueue->taskQueue[i], workQueue->taskQueue[i].fd,\
            (void*)&workQueue->taskQueue[i].set, *workQueue->taskQueue[i].fd_num); //& == indirizzo struct
        }
    }
    printf("\n");
    lockRelease(&workQueue->lockTaskQueue, errMessage);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */