#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "OnlineQueue.h"


/**
 * @file OnlineQueue.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione dell'interfaccia OnlineQueue.h, che raccoglie  le funzioni di utilita' per la coda degli\n
 * utenti connessi, indicizzata tramite i "file descriptort" assegnati dalla "Select" agli users connessi e di dimensione finita.\n
 * Siccome la "Select" assegna ad ogni client un "file descriptor" univoco, le operazioni fatte su tale coda sono\n
 * effettuate senza mutua esclusione esplicita (la mutua esclusione e' data appunto dall'accesso univoco tramite\n
 * "file descriptor").
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS


online_queue_t* initOnlineQueue(size_t qosize){

    online_queue_t* onlineQueue = NULL;
    if((onlineQueue = (online_queue_t*) calloc(1, sizeof(online_queue_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare struct che raccoglie coda degli utenti connessi in initOnlineQueue\n");
        free(onlineQueue);
        exit(EXIT_FAILURE);
    }

    onlineQueue->qosize = qosize;

    onlineQueue->connectionQueue = NULL;
    if((onlineQueue->connectionQueue = (online_user_t*) calloc(onlineQueue->qosize, sizeof(online_user_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare coda degli utenti connessi in initOnlineQueue\n");
        free(onlineQueue->connectionQueue);
        free(onlineQueue);
        exit(EXIT_FAILURE);
    }

    //iniziallizzo tutti i nodi della coda degli utenti connessi
    for(int i = 0; i < onlineQueue->qosize; i++){
        onlineQueue->connectionQueue[i].empty_struct = 1; //flag nodo vuoto "attivo"
        onlineQueue->connectionQueue[i].bucket_index = -1; //nodo non contiene ancora nessun indice di bucket di un utente connesso
        onlineQueue->connectionQueue[i].p_user_in_overflow_list = NULL; //nodo non possiede ancora riferimento ad un utente connesso

        if((onlineQueue->connectionQueue[i].username = (char*) calloc((MAX_NAME_LENGTH + 1), sizeof(char))) == NULL){
            perror("calloc username in initOnlineQueue");
            fprintf(stderr, "Errore allocazione dell'username %d in initOnlineQueue\n", i);
            free(onlineQueue->connectionQueue);
            free(onlineQueue);
            return NULL;
        }

    }

    return onlineQueue;
}

void deleteOnlineQueue(online_queue_t* onlineQueue){

    //LA MUTUA ESCLUSIONE LA POSSIEDO DA "deleteThreadpool"

    //dealloco username utenti connessi
    for(int i = 0; i < onlineQueue->qosize; i++){
        if(onlineQueue->connectionQueue[i].username != NULL)
            free(onlineQueue->connectionQueue[i].username);
    }

    //dealloco coda degli utenti connessi
    if(onlineQueue->connectionQueue != NULL)
        free(onlineQueue->connectionQueue);

    //dealloco struct coda di lavoro
    if(onlineQueue != NULL)
        free(onlineQueue);
}

int popOnlineUser(online_queue_t* onlineQueue, int fd, char* username){

    if(fd < 0 || fd >= onlineQueue->qosize){
        fprintf(stderr, "Errore file descriptor -%d- non valido in popOnlineUser\n", fd);
        return FUNCTION_FAILURE;
    }

    //non posso "disconnettere" un utente il cui slot, indicizzato con il suo "fd", in onlineQueue e' gia' "disconesso"
    // => slot gia' vuoto (se lo slot e' vuoto vuol dire che l'utente e' gia' stato disconesso da qualcuno in precedenza)
    if(onlineQueue->connectionQueue[fd].empty_struct == 1){
        fprintf(stderr, "Errore file descriptor -%d- presenta un nodo vuoto in popOnlineUser\n", fd);
        return FUNCTION_FAILURE;
    }

    //verifico,che l'utente connesso in onlineQueue->connectionQueue[fd], sia proprio l'utente che voglio eliminare io
    if(strncmp(onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0){

        //azzerro la struct indicizzata dal "file descriptort", per "disconettere" utente associato al "file descriptor"
        //memset(&(onlineQueue->connectionQueue[fd]), 0, sizeof(online_user_t));
        onlineQueue->connectionQueue[fd].empty_struct = 1;
        onlineQueue->connectionQueue[fd].bucket_index = -1;
        memset(onlineQueue->connectionQueue[fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
        onlineQueue->connectionQueue[fd].p_user_in_overflow_list = NULL;

        return FUNCTION_SUCCESS;
    }
    else return FUNCTION_FAILURE;
}

void printOnlineQueue(online_queue_t* onlineQueue){

    printf("\n");
    printf("STAMPA ONLINEQUEUE:\n");
    if(onlineQueue == NULL){
        printf("La coda di lavoro e' vuota\n");
    }
    else {
        for(int i = 0; i < onlineQueue->qosize; i++){
            printf("FD[%d] = emty_struct %d bucket_index %d username %s p_user_in_overflow_list %p\n", i, \
            onlineQueue->connectionQueue[i].empty_struct, onlineQueue->connectionQueue[i].bucket_index, \
            onlineQueue->connectionQueue[i].username, (void*)&onlineQueue->connectionQueue[i].p_user_in_overflow_list);
        }
    }

    printf("\n");

}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */