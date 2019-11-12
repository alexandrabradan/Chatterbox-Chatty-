#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "HistoryQueue.h"

/**
 * @file HistoryQueue.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione dell'interfaccia  HistoryQueue.h, che raccoglie  le funzioni di utilita' per una coda di\n
 * messaggi di un utente, indicizzata e di dimensione finita.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

history_queue_t* initHistoryQueue(size_t qhsize){

    //alloco la struct che raccoglie la coda di messaggi di un utente e le informazioni legate ad essa
    history_queue_t* historyQueue = NULL;
    if((historyQueue = (history_queue_t*) calloc(1, sizeof(history_queue_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare struct che raccoglie coda di messaggi di un utente \n");
        free(historyQueue);
        return NULL;
    }

    historyQueue->qhsize = qhsize;

    //alloco coda dei messaggi
    historyQueue->messagesQueue = NULL;
    if((historyQueue->messagesQueue = (msgInQueue_t*) calloc(historyQueue->qhsize, sizeof(msgInQueue_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare coda di messaggi\n");
        free(historyQueue->messagesQueue);
        free(historyQueue);
        return NULL;
    }

    //setto flag di msg NON ancora inviato
    for(int i = 0; i < historyQueue->qhsize; i++){
        historyQueue->messagesQueue[i].send = 0;
    }

    historyQueue->empty = 1; //coda dei messaggi e' vuota
    historyQueue->first = 0; //primo elemento sara' inserito in prima posizione
    historyQueue->insert = 0; //primo elemento sara' estratto dalla prima posizione

    return historyQueue;
}

void pushMessage(history_queue_t* historyQueue, op_t op, char *sender, char *rcv, const char *buf, unsigned int len){

    //la coda di messaggi e' piena => elimino msg piu' vecchio (indicizzato con "first")
    if((historyQueue->insert == historyQueue->first) && (historyQueue->empty == 0)){
        popMesssage(historyQueue);
    }

    //alloco msg da inserire in msg data, come puntatore
    char* buffer = NULL;
    if((buffer = (char*) calloc ((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
        perror("buffer");
        fprintf(stderr, "Errore allocazione buffer in pushMessage\n");
        exit(EXIT_FAILURE);
    }

    //copio contentuo di "buf" nel "buffer" allocato, dato che il primo verra' deallocato dal worker-thread successivamente
    strncpy(buffer, buf, len);

    //inserisco msg nella coda di messaggi
    setHeader(&historyQueue->messagesQueue[historyQueue->insert].msg.hdr, op, sender);
    setData(&historyQueue->messagesQueue[historyQueue->insert].msg.data, rcv, buffer, len);
    /**
     * setto messaggio come <<NON INVIATO>> => destinatario lo leggera' quando richiedera' un nuovo servizio
     * oppure quando lo richiedera' esplicitamente  tramite GETPREVMSGS_OP
     */
    historyQueue->messagesQueue[historyQueue->insert].send = 0;

    //aggiorno indice prossima posizione di inserimento
    historyQueue->insert = (historyQueue->insert + 1) % ((int)historyQueue->qhsize);

    //se la coda era vuota, dopo push, non e' piu' vuota
    if(historyQueue->empty == 1)
        historyQueue->empty = 0;
}

void popMesssage(history_queue_t* historyQueue){

    //elimino messaggio piu' vecchio (primo inserito rispetto agli altri) solo se coda NON e' vuota
    if(historyQueue->empty == 0){

        /**
         * TO DO: resettare msg in historyQueueu perche' potrebbe creare problemi
         */
        //azzerro la struct del messaggio piu' vecchio , per "eliminarlo"
        memset(&(historyQueue->messagesQueue[historyQueue->first]), 0, sizeof(message_t));


        //aggiorno indice prossimo elemento da estrarre
        historyQueue->first = (historyQueue->first + 1) % ((int)historyQueue->qhsize);

        //se avevo solo un messaggio, dopo la pop la coda e' vuota
        if(historyQueue->first == historyQueue->insert)
            historyQueue->empty = 1;
    }
}

void deleteHistoryQueue(history_queue_t* historyQueue){

    //dealloco buf msg, perche' allocati dinamicamente
    for(int i = 0; i < historyQueue->qhsize; i++){
        if(historyQueue->messagesQueue[i].msg.data.buf != NULL){
            free(historyQueue->messagesQueue[i].msg.data.buf);
            historyQueue->messagesQueue[i].msg.data.buf = NULL;
        }

    }

    //dealloco coda di messaggi
    if(historyQueue->messagesQueue != NULL){
        free(historyQueue->messagesQueue);
        historyQueue->messagesQueue = NULL;
    }

    //dealloco struct coda di messaggi
    if(historyQueue != NULL){
        free(historyQueue);
        historyQueue = NULL;
    }
}

void printHistoryQueue(history_queue_t* historyQueue){

    printf("STAMPA CODA MESSAGGI: \n");
    if(historyQueue->empty){
        printf("La coda di messaggi e' vuota\n");
    }
    else {
        for(int i = 0; i < historyQueue->qhsize; i++){
            printf("Messaggio[%d] = op %d sender %s  receiver %s len %d buf %s inviato -%d-\n", i, historyQueue->messagesQueue[i].msg.hdr.op, //
                   historyQueue->messagesQueue[i].msg.hdr.sender, historyQueue->messagesQueue[i].msg.data.hdr.receiver, historyQueue->messagesQueue[i].msg.data.hdr.len, //
                   historyQueue->messagesQueue[i].msg.data.buf , historyQueue->messagesQueue[i].send);
        }
    }

    printf("\n");

}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

