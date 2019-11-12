#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#include "CheckSysCall.h"
#include "GlobalVariables.h"



#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 * @file CheckSysCall.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzioni dell'iterfaccia CheckSysCall.h
 */


void lockInit(pthread_mutex_t* mtx, char* errMessage){
    int result;
    if((result = pthread_mutex_init(mtx, NULL)) != 0){
        perror("pthread_mutex_init");
        fprintf(stderr, "Errore pthread_mutex_init in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void lockAcquire(pthread_mutex_t* mtx, char* errMessage){
    int result;
    if((result = pthread_mutex_lock(mtx)) != 0){
        perror("pthread_mutex_lock");
        fprintf(stderr, "Errore pthread_mutex_lock in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void lockRelease(pthread_mutex_t* mtx, char* errMessage){
    int result;
    if((result = pthread_mutex_unlock(mtx)) != 0){
        perror("pthread_mutex_unlock");
        fprintf(stderr, "Errore pthread_mutex_unlock in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void condInit(pthread_cond_t* cond, char* errMessage){
    int result;
    if((result = pthread_cond_init(cond, NULL)) != 0){
        perror("pthread_cond_init");
        fprintf(stderr, "Errore pthread_cond_init in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void condWait(pthread_cond_t *cond, pthread_mutex_t* mtx, char* errMessage){
    int result;
    if((result = pthread_cond_wait(cond, mtx)) != 0){
        perror("pthread_mutex_wait");
        fprintf(stderr, "Errore pthread_mutex_wait in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void condSignal(pthread_cond_t* cond, char* errMessage){
    int result;
    if((result = pthread_cond_signal(cond)) != 0){
        perror("pthread_cond_signal");
        fprintf(stderr, "Errore pthread_cond_signal in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void condBroadcast(pthread_cond_t* cond, char* errMessage){
    int result;
    if((result = pthread_cond_broadcast(cond)) != 0){
        perror("pthread_cond_broadcast");
        fprintf(stderr, "Errore pthread_cond_broadcast in %s: result %d\n", errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void  ptCreate(pthread_t* tID, void *(*function) (void *), void *arg, char* errMessage){
    int result;
    if((result = pthread_create(&(*tID), NULL, function, arg) ) != 0){
        printf("result %d\n", result);
        perror("pthread_join");
        fprintf(stderr, "Errore pthread_join sul thread %lu in %s: result %d\n", *tID,  errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void ptJoin(pthread_t tID, char* errMessage){
    int result;
    if((result = pthread_join(tID, NULL)) != 0){
        perror("pthread_join");
        fprintf(stderr, "Errore pthread_join sul thread %lu in %s: result %d\n", tID,  errMessage, result);
        exit(EXIT_FAILURE);
    }
}

void SYSCALL(int* r, int c, char* e ){
    if((*r = c) == -1){
        perror(e);
        int error = errno;
        fprintf(stderr, "%s errno: %d\n", e, error);
        //exit(errno);
        exit(EXIT_FAILURE);
    }
}

int updatemax(fd_set* set, const int* fd_num) {
    for(int i = ((*fd_num)-1); i >= 0; i--) {
        if (FD_ISSET(i, set))
            return i;
    }

    return -1;
}

void CLOSECLIENT(const int* fd, char* e){

    if(close(*fd) == -1){
        perror("close fd");
        fprintf(stderr, "Errore chiusura del socket fd=%d: %s\n", *fd, e);
        exit(EXIT_FAILURE);
    }

}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */



