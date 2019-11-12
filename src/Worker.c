
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "ThreadPool.h"

/**
 * @file Woker.c
 * @author Alexandra Bradan
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzioni-routine che un worker-thread deve eseguire, per soddisfare le richieste dei\n
 * clients, conntenute nell'interfaccia ThredPool.h
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

void getPendingMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "getPendingMessages";

    char* username = request->hdr.sender;

    //accedo tramite "fd" alla coda degli utenti connessi e reperisco l'indice del bucket in cui ho memorizzato il
    //nodo dell'utente e di conseguenza la sua historyQueue
    int bucket_index = ht->onlineQueue->connectionQueue[fd].bucket_index; //indice del bucket
    size_t sizeLockItems = ht->sizeLockItems; //dimensione array di locks

    if ((bucket_index < 0) || (bucket_index > (ht->size - 1))) {
        fprintf(stderr, "Errore indice bucket dove ricercare username %s: indice -%d- fuori range in getPendingMessages\n", username, bucket_index);
        exit(EXIT_FAILURE);
    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket
    int lock_index = bucket_index % ((int) sizeLockItems);

    if ((lock_index < 0) || (lock_index > sizeLockItems)) {
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in getPendingMessages\n",
                lock_index, bucket_index);
        exit(EXIT_FAILURE);
    }

    //acquisisco mutua esclusione sul bucke tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ACQUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ricavo la coda dei messaggi dell'utente
    history_queue_t *msgsQueue = getHistoryQueue(&ht->items[bucket_index], username);

    //se l'utente era presente nella lista di trabocco, la coda dei messaggi restituita NON e' nulla
    if (msgsQueue != NULL) {

        //scandisco historyQueue alla ricerca di messaggi pendenti

        //history vuota => non invio messaggi
        if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 1)){
            //non mando nessun messaggio al client, dato che non ne ha
        }
            //ho meno messaggi della dimensione dell'array dei messaggi
        else if((msgsQueue->first != msgsQueue->insert) && (msgsQueue->empty == 0)){
            for(int i = 0; i < msgsQueue->insert; i++){
                if(msgsQueue->messagesQueue[i].send == 0) {

                    char* tmp_buf = NULL;
                    if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in getPendingMessages\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf,  msgsQueue->messagesQueue[i].msg.data.hdr.len);

                    //invio hrd msg + contenuto del msg  all'utente
                    int r = sendResponse(ht, index, pool, fd, msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                   msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);

                    //se l'invio ha avuto successo
                    if(r == 0){

                        if(msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE){
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client
                    }
                    else break;
                }
            }
        }
            //coda dei messaggi e' satura
        else if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 0)){
            //invio messaggi piu' vecchi prima
            for (int i = msgsQueue->first; i < msgsQueue->qhsize; i++) {
                if(msgsQueue->messagesQueue[i].send == 0) {
                    char *tmp_buf = NULL;
                    if ((tmp_buf = (char *) calloc((size_t) (MAXMSGSIZE) + 1, sizeof(char))) == NULL) {
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in getPendingMessages\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf,
                            msgsQueue->messagesQueue[i].msg.data.hdr.len);

                    //invio messaggio all'utente
                    int r = sendResponse(ht, index, pool, fd, msgsQueue->messagesQueue[i].msg.hdr.op,
                                         msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                    msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);


                    //se l'invio ha avuto successo
                    if (r == 0) {

                        if (msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE) {
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client
                    } else break;
                }
            }

            //invio messaggi piu' recenti
            for(int i = 0; i < msgsQueue->insert; i++){
                if(msgsQueue->messagesQueue[i].send == 0) {
                    char *tmp_buf = NULL;
                    if ((tmp_buf = (char *) calloc((size_t) (MAXMSGSIZE) + 1, sizeof(char))) == NULL) {
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in getPendingMessages\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf,
                            msgsQueue->messagesQueue[i].msg.data.hdr.len);

                    //invio hdr msg + contenuto all'utente
                    int r = sendResponse(ht, index, pool, fd, msgsQueue->messagesQueue[i].msg.hdr.op,
                                         msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                    msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);


                    //se l'invio ha avuto successo
                    if (r == 0) {

                        if (msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE) {
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client
                    } else break;
                }
            }
        }
    }

    //rilascio mutua esclusione sul bucket
    lockRelease(&ht->lockItems[lock_index], errMessage);
}

int sendResponseToClient(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, op_t op, char* sender,  message_t* request, unsigned int len, char* buf, int* reinsertInWorkingSet, int freeMmapped){

    char* receiver = request->hdr.sender;

    //Creo il messaggio di responso da inviare al cliet
    message_t response;
    memset(&response, 0, sizeof(message_t));

    //inizializzo hdr della risposta
    setHeader(&(response.hdr), op, sender);

    //invio hdr di risposta al client
    int r = sendHeader(fd, &(response.hdr));

    //client si e' disconesso => disconnetto client
    if(r <= 0){
        //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes dell'hdr di risposta a fd=%d in sendResponse\n", index, r, fd);

         //disconetto utente => lo elimino dalla coda degli utenti connessi
            //disconnectUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
            if(ht->onlineQueue->connectionQueue[fd].empty_struct == 0){
                //disconetto client [NON FACCIO CONTROLLI SU CHI SIA CONNESSO, DATO CHE NON SONO RIUSCITA A LEGGERE NOME CLIENT]
                ht->onlineQueue->connectionQueue[fd].empty_struct = 1;
                ht->onlineQueue->connectionQueue[fd].bucket_index = -1;
                memset(ht->onlineQueue->connectionQueue[fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
                ht->onlineQueue->connectionQueue[fd].p_user_in_overflow_list = NULL;
            }

            //chiudo socket "fd" del client
            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' sendResponse");

            updateStats(&pool->threadsStats[index], 0, 1, 0, 0, 0, 0, 0);

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d in sendResponse\n", index, fd);
        goto end;
    }
    else {

        if (len > 0) { //devo inviare anche data al client

            //inizializzo body della risposta
            setData(&(response.data), receiver, buf, len);

            //invio hdr di risposta al client
            r = sendData(fd, &(response.data));

            //client si e' disconesso => disconnetto client
            if (r <= 0) {
                //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes dell'data di risposta a fd=%d in sendResponse\n", index, r, fd);

                //disconetto utente => lo elimino dalla coda degli utenti connessi
            //disconnectUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
            if(ht->onlineQueue->connectionQueue[fd].empty_struct == 0){
                //disconetto client [NON FACCIO CONTROLLI SU CHI SIA CONNESSO, DATO CHE NON SONO RIUSCITA A LEGGERE NOME CLIENT]
                ht->onlineQueue->connectionQueue[fd].empty_struct = 1;
                ht->onlineQueue->connectionQueue[fd].bucket_index = -1;
                memset(ht->onlineQueue->connectionQueue[fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
                ht->onlineQueue->connectionQueue[fd].p_user_in_overflow_list = NULL;
            }

            //chiudo socket "fd" del client
            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' sendResponse");

            updateStats(&pool->threadsStats[index], 0, 1, 0, 0, 0, 0, 0);

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d in sendResponse\n", index, fd);
                goto end;
            }
        }
    }

    //printf("Worker[%zd] ha inviato  con successo RISPOSTA a fd=%d in sendResponse\n", index, fd);

    //dealloco risorsa allocata
    if(len != 0 && freeMmapped == 1){
        if(strncmp(buf, "\0", strlen(buf)) != 0){
            free(buf);
            buf = NULL;
        }
    }

    //successo invio risposta al client
    return 0;

    end:
    //dealloco risorsa allocata
    if(len != 0 && freeMmapped == 1){
        if(strncmp(buf, "\0", strlen(buf)) != 0){
            free(buf);
            buf = NULL;
        }
    }
    //fallimento invio risposta al client
    return -1;
}


int sendResponse(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, op_t op, char* sender,  message_t* request, unsigned int len, char* buf, int* reinsertInWorkingSet){
    return sendResponseToClient(ht, index, pool, fd, op, sender, request, len, buf, reinsertInWorkingSet, 1);
}


int sendMinusRMessage(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, int* reinsertInWorkingSet){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "sendMInusRMessage";

    message_t phony_request;
    memset(&phony_request, 0, sizeof(message_t));

    setHeader(&phony_request.hdr, MENO_R_OP, ht->onlineQueue->connectionQueue[fd].username);

    //accedo tramite "fd" alla coda degli utenti connessi e reperisco l'indice del bucket in cui ho memorizzato il
    //nodo dell'utente e di conseguenza la sua historyQueue
    int bucket_index = ht->onlineQueue->connectionQueue[fd].bucket_index; //indice del bucket
    size_t sizeLockItems = ht->sizeLockItems; //dimensione array di locks

    if ((bucket_index < 0) || (bucket_index > (ht->size - 1))) {
        fprintf(stderr, "Errore indice bucket dove ricercare username %s: indice -%d- fuori range in sendMinusRMessage\n", \
                        ht->onlineQueue->connectionQueue[fd].username, bucket_index);
        exit(EXIT_FAILURE);

    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket
    int lock_index = bucket_index % ((int) sizeLockItems);

    if ((lock_index < 0) || (lock_index > sizeLockItems)) {
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in sendMinusRMessage\n",
                lock_index, bucket_index);
        exit(EXIT_FAILURE);
    }

    //acquisisco mutua esclusione sul bucke tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ricavo la coda dei messaggi dell'utente
    history_queue_t *msgsQueue = getHistoryQueue(&ht->items[bucket_index], ht->onlineQueue->connectionQueue[fd].username);

    //se l'utente non era presente nella lista di trabocco, la coda dei messaggi restituita e' vuota
    if (msgsQueue == NULL) {
        fprintf(stderr, "Errore history dell' utentente con fd =|%d| non presente nella HashTable in sendMinusRMessage\n", fd);
        exit(EXIT_FAILURE);

    } else {

        // Scorro la History dell'utente e se trovo il msg pendente piu' vecchio (quello inserito per prima e NON ancora inviato)
        //   lo mando al client.

        //history vuota => non invio messaggi
        if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 1)){

            //non mando nessun messaggio al client, dato che non ne ha
            //rilascio mutua esclusione sul bucket
            lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
            return 1; //NON HO ANCORA MSG DA INVIARE
        }
            //ho meno messaggi della dimensione dell'array dei messaggi
        else if((msgsQueue->first != msgsQueue->insert) && (msgsQueue->empty == 0)){
            for(int i = 0; i < msgsQueue->insert; i++){

                //controllo se c'e' un msg pendente
                if(msgsQueue->messagesQueue[i].send == 0){
                    char* tmp_buf = NULL;
                    if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in sendMinusRMessage\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, msgsQueue->messagesQueue[i].msg.data.hdr.len);

                    //invio msg pendente
                    int r = sendResponse(ht, index, pool, fd,  msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender,\
                                   &phony_request, msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);

                    //se l'invio ha avuto successo
                    if(r == 0){

                        if(msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE){
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client

                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return 0;
                    }
                    else{
                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return -1;
                    }
                }
            }
        }
            //coda dei messaggi e' satura
        else if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 0)){
            //invio messaggi piu' vecchi prima

            for (int i = msgsQueue->first; i < msgsQueue->qhsize; i++) {
                //controllo se c'e' un msg pendente
                if(msgsQueue->messagesQueue[i].send == 0){
                    char* tmp_buf = NULL;
                    if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in sendMinusRMessage\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, strlen(msgsQueue->messagesQueue[i].msg.data.buf));


                    //invio msg pendente
                    int r = sendResponse(ht, index, pool, fd,  msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender,\
                                   &phony_request, msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);

                    //se l'invio ha avuto successo
                    if(r == 0){

                        if(msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE){
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client

                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return 0;
                    }
                    else{
                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return -1;
                    }
                }
            }

            //invio messaggi piu' recenti
            for(int i = 0; i < msgsQueue->insert; i++){
                //controllo se c'e' un msg pendente
                if(msgsQueue->messagesQueue[i].send == 0){
                    char* tmp_buf = NULL;
                    if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in sendMinusRMessage\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, strlen(msgsQueue->messagesQueue[i].msg.data.buf));

                    //invio msg pendente
                    int r = sendResponse(ht, index, pool, fd,  msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender,\
                                   &phony_request, msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);

                    //se l'invio ha avuto successo
                    if(r == 0){

                        if(msgsQueue->messagesQueue[i].msg.hdr.op == TXT_MESSAGE){
                            //setto che ho inviato il messaggio
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di msg inviati
                            //decremento # msg NON ancora inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 1, -1, 0, 0, 0);
                        }

                        //se msgsQueue->messagesQueue[i].msg.hdr.op == TXT_FILE aggiornamento statistiche file viene fatto con "getFile" richiesta dal client

                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return 0;
                    }
                    else{
                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
                        return -1;
                    }
                }
            }
        }
    }

    //printf(" history di |%s| non contine msg pendenti\n", ht->onlineQueue->connectionQueue[fd].username);

    //se sono arrivata qui vuol dire che utente non aveva msg pendenti
    //=> devo reinserirlo nella "Coda di ibernazione", ed attendere che qualcuno gli invii un msg
    lockRelease(&ht->lockItems[lock_index], "switch MENO_R_OP\0");
    return 1;
}

int readInterceptAndIgnoreMinusR(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, int* reinsertInWorkingSet){

    message_t msg_to_ignore;
    memset(&msg_to_ignore, 0, sizeof(message_t));

    int r = readServerMsg(fd, &msg_to_ignore);

    if(msg_to_ignore.data.buf != NULL){
        free(msg_to_ignore.data.buf);
        msg_to_ignore.data.buf = NULL;
    }

    if(r < 0){
        //fprintf(stderr, "Worker %zd ha letto |%d| bytes dalla MENO_R_OP del client fd=%d, perche' e' subentrato un errore di connessione\n", index, r, fd);

        //controllo se client e' connesso  e se lo e' lo disconetto [NON CONOSCO SUO NOME CON "MENO_R_OP"]
        if (ht->onlineQueue->connectionQueue[fd].empty_struct == 0) {
            //disconetto client [NON FACCIO CONTROLLI SU CHI SIA CONNESSO, DATO CHE NON SONO RIUSCITA A LEGGERE NOME CLIENT]
            ht->onlineQueue->connectionQueue[fd].empty_struct = 1;
            ht->onlineQueue->connectionQueue[fd].bucket_index = -1;
            memset(ht->onlineQueue->connectionQueue[fd].username, 0,
                   sizeof(char) * (MAX_NAME_LENGTH + 1));
            ht->onlineQueue->connectionQueue[fd].p_user_in_overflow_list = NULL;
        }

        //chiudo socket "fd" del client
        CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

        updateStats(&pool->threadsStats[index], 0, 1, 0, 0, 0, 0, 0);

        //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
        *reinsertInWorkingSet = 0;

       // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d in readInterceptAndIgnoreMinusR\n", index, fd);
        return -1;

    }
    else if(r == 0){
        //fprintf(stderr,
               // "Worker %zd ha letto |%d| bytes dalla MENO_R_OP del client fd=%d, perche' client ha chiuso connessione\n", index, r, fd);

        //controllo se client e' connesso  e se lo e' lo disconetto [NON CONOSCO SUO NOME CON "MENO_R_OP"]
        if (ht->onlineQueue->connectionQueue[fd].empty_struct == 0) {
            //disconetto client [NON FACCIO CONTROLLI SU CHI SIA CONNESSO, DATO CHE NON SONO RIUSCITA A LEGGERE NOME CLIENT]
            ht->onlineQueue->connectionQueue[fd].empty_struct = 1;
            ht->onlineQueue->connectionQueue[fd].bucket_index = -1;
            memset(ht->onlineQueue->connectionQueue[fd].username, 0,
                   sizeof(char) * (MAX_NAME_LENGTH + 1));
            ht->onlineQueue->connectionQueue[fd].p_user_in_overflow_list = NULL;
        }

        //chiudo socket "fd" del client
        CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

        updateStats(&pool->threadsStats[index], 0, 1, 0, 0, 0, 0, 0);

        //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
        *reinsertInWorkingSet = 0;

       // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d in readInterceptAndIgnoreMinusR\n", index, fd);
        return -1;
    }

    //lettura MENO_R_OP e' andata a buon fine
    return 0;
}



void sendResponseAndMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, history_queue_t* msgsQueue, int* reinsertInWorkingSet){

    //buffer che conterra' dimensione della historyQueue castata a char, per poterla inviare al client nel body
    //del messaggio di risposta e consentirgli di sapere quanti messaggi gli verranno inviati
    char *buf = NULL;
    if ((buf = (char *) calloc(2, sizeof(char))) == NULL) {
        perror("buf");
        fprintf(stderr, "Errore allocazione in sendResponseAndMessages\n");
        exit(EXIT_FAILURE);
    }
    unsigned int buf_len = (unsigned int) sizeof(buf);

    //ricavo dimenione (# di elementi della coda dei messaggi)
    int msgsQueue_len = -1;

    //messagesQueue->empty == 1 --> coda vuota
    if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 1)){
        msgsQueue_len = 0;
    }
        //sto riempiendo coda, ma ci sono ancora posizioni libere
    else if((msgsQueue->first != msgsQueue->insert) && (msgsQueue->empty == 0)){
        msgsQueue_len = msgsQueue->insert;
    }
        //non ci sono piu' posizioni libere, quindi ho iniziato a cancellare msgs ed ad aggiure al loro posto nuovi msgs
    else if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 0)){
        msgsQueue_len = (int)msgsQueue->qhsize;
    }

    int r;

    //se l'utente  NON ha ancora ricevuto messaggi, gli invio comunque 1 messaggio con il quale gli comunico che
    //NON ha ancora ricevuti msgs
    if(msgsQueue_len == 0){

        char qhsze = (char)1; //1 => mando un msg al client con il quale gli comunico che NON ha msg
        strncpy(buf, &qhsze, sizeof(char));
        buf_len = (unsigned int) strlen(buf);

        r = sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, buf_len, buf, reinsertInWorkingSet);
    }
    else{

        char qhsze = (char)msgsQueue_len;
        strncpy(buf, &qhsze, sizeof(char));
        buf_len = (unsigned int) strlen(buf);

        r = sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, buf_len, buf, reinsertInWorkingSet);

    }

    if(r == 0){  //invio dimensione(# msgs) della History dell'utente e' andata a buon fine

        /**
         * Ora mi preparo ad inttercettare ed ignorare il primo degli "i", con 1 <= i <= dim. History, MENO_OP_R
         * che il Client mi inviera' facendo <<readMsg>>, dato che si aspettera' che io gli invii gli "i" messaggi
         * della sua History.
         *
         * POSSIEDO MUTUA ESCLUSIONE GRAZIE A "getPreviuosMessages"
         */

        //scandisco historyQueue e mando uno a uno messaggi all'utente
        //history vuota => invio un msg con il quale comunico all'utente che N0N ha ancora messaggi
        if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 1)){

            //leggo, intercetto ed ignoro MENO_R_OP
            r = readInterceptAndIgnoreMinusR(ht, index, pool, fd, reinsertInWorkingSet);

            //controllo che client non  si sia disconesso/connessione e' venuta meno
            if(r != -1){
                char* tmp_buf = NULL;
                if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                    perror("tmp_buf");
                    fprintf(stderr, "Errore allocazione buffer buf_tmp in sendResponseAndMessages\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(tmp_buf, "Non hai ancora rivenuto messaggi\0", strlen("Non hai ancora rivenuto messaggi\0"));

                sendResponse(ht, index, pool, fd, TXT_MESSAGE, "ChattyServer", request, \
                    (unsigned int)strlen("Non hai ancora rivenuto messaggi\0"), tmp_buf, reinsertInWorkingSet);
            }
        }
            //ho meno messaggi della dimensione dell'array dei messaggi
        else if((msgsQueue->first != msgsQueue->insert) && (msgsQueue->empty == 0)){
            for(int i = 0; i < msgsQueue->insert; i++){

                //leggo, intercetto ed ignoro MENO_R_OP
                r = readInterceptAndIgnoreMinusR(ht, index, pool, fd, reinsertInWorkingSet);

                if(r == -1){
                    //client si e'disconesso/connessione e' venuta meno => esco dal ciclo di invio msg History
                    break;
                }

                char* tmp_buf = NULL;
                if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                    perror("tmp_buf");
                    fprintf(stderr, "Errore allocazione buffer buf_tmp in sendResponseAndMessages\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, msgsQueue->messagesQueue[i].msg.data.hdr.len);

                sendResponse(ht, index, pool, fd,  msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                    msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);
            }
        }
            //coda dei messaggi e' satura
        else if((msgsQueue->first == msgsQueue->insert) && (msgsQueue->empty == 0)){
            //invio messaggi piu' vecchi prima

            for (int i = msgsQueue->first; i < msgsQueue->qhsize; i++) {

                //leggo, intercetto ed ignoro MENO_R_OP
                r = readInterceptAndIgnoreMinusR(ht, index, pool, fd, reinsertInWorkingSet);

                if(r == -1){
                    //client si e'disconesso/connessione e' venuta meno => esco dal ciclo di invio msg History
                    break;
                }

                char* tmp_buf = NULL;
                if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                    perror("tmp_buf");
                    fprintf(stderr, "Errore allocazione buffer buf_tmp in sendResponseAndMessages\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, strlen(msgsQueue->messagesQueue[i].msg.data.buf));

               sendResponse(ht, index, pool, fd,  msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                    msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);
            }

            //se invio msg piu' vecchi e' andato a buon fine, provo ad invaire nuovi msg
            if(r == 0){
                //invio messaggi piu' recenti
                for(int i = 0; i < msgsQueue->insert; i++){

                    //leggo, intercetto ed ignoro MENO_R_OP
                    r = readInterceptAndIgnoreMinusR(ht, index, pool, fd, reinsertInWorkingSet);

                    if(r == -1){
                        //client si e'disconesso/connessione e' venuta meno => esco dal ciclo di invio msg History
                        break;
                    }

                    char* tmp_buf = NULL;
                    if((tmp_buf = (char*) calloc((size_t)(MAXMSGSIZE) + 1, sizeof(char))) == NULL){
                        perror("tmp_buf");
                        fprintf(stderr, "Errore allocazione buffer buf_tmp in getPendingMessages\n");
                        exit(EXIT_FAILURE);
                    }
                    strncpy(tmp_buf, msgsQueue->messagesQueue[i].msg.data.buf, strlen(msgsQueue->messagesQueue[i].msg.data.buf));

                    sendResponse(ht, index, pool, fd, msgsQueue->messagesQueue[i].msg.hdr.op, msgsQueue->messagesQueue[i].msg.hdr.sender, request, \
                    msgsQueue->messagesQueue[i].msg.data.hdr.len, tmp_buf, reinsertInWorkingSet);
                }
            }
        }
    }
}


void registerUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){

    char* username = request->hdr.sender;

    //Creo il messaggio di responso da inviare al cliet
    message_t response;
    memset(&response, 0, sizeof(message_t));

    //provo ad inserire utente nella HashTable
    int r = insertUserInHashTable(ht, username);

    if(r == OP_NICK_ALREADY){

        //inizializzo hdr della risposta
        setHeader(&(response.hdr), OP_NICK_ALREADY, "ChattyServer");

        //invio hdr di risposta al client
        int s = sendHeader(fd, &(response.hdr));

        //client si e' disconesso => disconnetto client
        if(s <= 0){
            //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes del hdr di risposta a fd=%d in registerUser\n", index, r, fd);

            //non elimino  utente dalla coda dei connessi, perche' concrettamente  non e' ancora connesso (non c'e'ancora in tale coda)

            //chiudo socket "fd" del client
            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

            //aggiorno le statistiche
            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s| in registerUser\n", index, fd, username);
        }
    }
    else if(r == FUNCTION_FAILURE){

        //inizializzo hdr della risposta
        setHeader(&(response.hdr), OP_FAIL, "ChattyServer");

        //invio hdr di risposta al client
        int s = sendHeader(fd, &(response.hdr));

        //client si e' disconesso => disconnetto client ed aggiorno le statistiche
        if(s <= 0){
            //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes del hdr di risposta a fd=%d in registerUser\n", index, r, fd);

            //non elimino  utente dalla coda dei connessi, perche' concrettamente  non e' ancora connesso (non c'e'ancora in tale coda)

            //chiudo socket "fd" del client
            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

            //printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s|in registerUser\n", index, fd, username);

            //aggiorno le statistiche
            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
    }
    else{ //OP_OK

        //printf("Worker[%zd] ha registarto con successo username |%s| con fd=%d in registerUser\n", index, username, fd);

        //incremento # di utenti registrati
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 1, 0, 0, 0, 0, 0, 0);

        //provo a connettere l'utente appena registrato
        connectUser(ht, index, pool, fd, request, reinsertInWorkingSet);
    }
}

void connectUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd,  message_t* request, int* reinsertInWorkingSet){

    char* username = request->hdr.sender;

    //Creo il messaggio di responso da inviare al cliet
    message_t response;
    memset(&response, 0, sizeof(message_t));

    //provo a inserire utente nella OnlineQueue (indicizzandolo tramite suo "fd")
    int r = pushOnlineUser(ht, fd,  username);

    if(r == OP_NICK_UNKNOWN){

        //OP_NICK_UNKNOWN => utente non registarto (non presente nella HashTable)
        setHeader(&(response.hdr), OP_NICK_UNKNOWN, "ChattyServer");

        //invio hdr di risposta al client
        int s = sendHeader(fd, &(response.hdr));

        //client si e' disconesso => disconnetto client
        if(s <= 0){
            //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes del hdr di risposta a fd=%d in connectUser\n", index, r, fd);

            //non elimino  utente dalla coda dei connessi, perche' concrettamente  non e' ancora connesso (non c'e'ancora in tale coda)

            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s| in connectUser\n", index, fd, username);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
    }
    else if(r == FUNCTION_FAILURE){

        //inizializzo hdr della risposta
        setHeader(&(response.hdr), OP_FAIL, "ChattyServer");

        //invio hdr di risposta al client
        int s = sendHeader(fd, &(response.hdr));

        //client si e' disconesso => disconnetto client
        if(s <= 0){
            //fprintf(stderr, "Worker[%zd] ha inviato |%d| bytes del hdr di risposta a fd=%d in connectUser\n", index, r, fd);

            //non elimino  utente dalla coda dei connessi, perche' concrettamente  non e' ancora connesso (non c'e'ancora in tale coda)

            //chiudo socket "fd" del client
            CLOSECLIENT(&fd, "errore scrittura risposta al client, client si e' disconesso");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s| in connectUser\n", index, fd, username);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
    }
    else{ //OP_OK

        //printf("Worker[%zd] ha connesso con successo username |%s| con fd=%d in connectUser\n", index, username, fd);

        //invio lista degli utenti attualmente connessi all'utente
        getConnectedUsers(ht, index, pool, fd, request, reinsertInWorkingSet);
    }
}

void postMessage(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */
    char* sender = request->hdr.sender;
    char* receiver = request->data.hdr.receiver;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, sender, strlen(sender)) == 0 ) {

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        //getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //controllo che il messaggio che il mittente vuole inviare NON superi la dimensione massima dei messaggi contenuta
        //nel file di configurazione
        if(request->data.hdr.len > MAXMSGSIZE){

            //OP_MSG_TOOLONG
            sendResponse(ht, index, pool, fd, OP_MSG_TOOLONG, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
            goto end;

        }
        else{ //dimensione del msg giusta

            //provo ad inviare, inserendo nella sua historyQueue, il msg al destinatario
            int r = insertMessageInHashTable(ht, sender, receiver, request, TXT_MESSAGE);

            if(r == OP_NICK_UNKNOWN){
                //OP_NICK_UNKNOWN => destinatario sconosciuto (non presente HashTable)
                sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
                goto end;

            }
            else if(r == FUNCTION_FAILURE){
                //OP_FAIL
                sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
                goto end;
            }
            else{
                //OP_OK
                //printf("Worker[%zd] ha inviato con successo msg del mitt |%s| con fd=%d al dest |%s| in postMessage\n", index, sender, fd, request->data.hdr.receiver);

                sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                //incremento # di messaggi NON ancora inviati
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 1, 0, 0, 0);
                goto end;
            }
        }

    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        goto end;
    }

    end:
    if(request->data.buf){
        free(request->data.buf); //devo liberare buf che client mi ha inviato dato che lo ha allocato dinamicamente
        request->data.buf = NULL;
    }
}

void postMessageToAll(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet) {
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "postMessageToAll";
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */

    //acquisisco mutua esclusione su ogni bucket della HashTable => mutua esclusione su un bucket mi consente di
    //modificare tutti i nodi della lista di trabocco di tale bucket => con la mutua esclusione su un bucket vado ad
    //inserire il messaggio che l'utente vuole mandare, a tutti gli utenti che collidono sul bucket nella loro
    //historyQueue

    char* sender = request->hdr.sender;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, sender, strlen(sender)) == 0 ) {

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        //getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //controllo che il messaggio che il mittente vuole inviare NON superi la dimensione massima dei messaggi contenuta
        //nel file di configurazione
        if(request->data.hdr.len > MAXMSGSIZE){

            //OP_MSG_TOOLONG
            sendResponse(ht, index, pool, fd, OP_MSG_TOOLONG, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

        }
        else{  //dimensione del msg giusta

            //itero sui buckets della HashTable
            for (int i = 0; i < ht->size; i++) {
                size_t sizeLockItems = ht->sizeLockItems; //dimensione array di locks

                //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket[i]
                int lock_index = i % ((int) sizeLockItems);

                if ((lock_index < 0) || (lock_index > sizeLockItems)) {
                    fprintf(stderr,
                            "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in postMessage\n",
                            lock_index, i);

                    //OP_FAIL
                    sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                    //incremento # di errori
                    //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                    updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
                    goto end;
                }

                //acquisisco mutua esclusione sul bucke tramite la lock indicizzata da "lock_index" nell'array di locks
                //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
                //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
                lockAcquire(&ht->lockItems[lock_index], errMessage);

                //scorro la lista di trabocco del bucket[i] e inserisco nella coda dei messaggi di ogni utente  il messaggio
                //destinato a tutti
                userNode_t *curr = ht->items[i].usersQueue;
                while (curr != NULL) {

                    //provo ad inviare, inserendo nella sua historyQueue, il msg a tutti gli utenti della lista di trabocco del bucket[i]
                    pushMessage(curr->historyQueue, TXT_MESSAGE, sender, curr->username, request->data.buf,
                                request->data.hdr.len);

                    //incremento # di messaggi NON ancora inviati
                    //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                    updateStats(&pool->threadsStats[index], 0, 0, 0, 1, 0, 0, 0);

                    //continuo a scorre la lista di trabocco
                    curr = curr->nextUserNode;
                }
                //rilascio mutua esclusione sul bucket
                lockRelease(&ht->lockItems[lock_index], errMessage);
            }

            //printf("Worker[%zd] ha inviato con successo il msg dell'username  |%s| con fd=%d a tutti in postMessageToAll\n", index, sender, fd);

            //dopo aver iterato su tutti i buckets e sulle relative liste di trabocco, mando all'utente OP_OK
            sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);
            goto end;
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        goto end;
    }

    end:
    if( request->data.buf != NULL){
        free(request->data.buf); //devo liberare buf che client mi ha inviato dato che lo ha allocato dinamicamente
        request->data.buf = NULL;
    }
}



void postFile(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */

    char* sender = request->hdr.sender;
    char* receiver = request->data.hdr.receiver;
    char* fileName = request->data.buf;


    // Primo msg del client mi di serve per sapere nome del destinatario, nome del file e lunghezza nome del file che il mittente vuole
    // inviare al destinatario. Il nome del file mi server per aprire il file e concatenarci, a poco a poco, il
    // contenuto che il mittente mi inviera' con il secondo data msg. Il secondo data request2 contiene la dimensione del file
    //ed il contenuto del file


    // II msg del client con lunghezza contenuto del file e contenuto del file
    message_t request2;
    memset(&request2, 0, sizeof(message_t));

    char* path = NULL;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, sender, strlen(sender)) == 0 ) {

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        // getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);


        int r = readData(fd, &request2.data);

        if(r < 0){
            //fprintf(stderr, "Ho ha letto |%d| bytes dalla richiesta del client %d, perche' e' subentrato un errore in postFile\n", r, fd);

            //disconetto client
            disconnectUser(ht, index, pool, fd, request, reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

            //chiudo client
            CLOSECLIENT(&fd, "errore lettura richiesta da parte server");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

            //printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s| in postFile\n", index, fd, sender);
            goto end;
        }
        else if(r == 0){
            //fprintf(stderr, "Ho letto |%d| bytes dalla richiesta del client %d, perche' client ha chiuso connessione in postFile\n", r, fd);

            //disconetto client
            disconnectUser(ht, index, pool, fd, request, reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

            //chiudo client
            CLOSECLIENT(&fd, "errore lettura richiesta da parte server");

            //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
            *reinsertInWorkingSet = 0;

            //printf("WORKER[%zd] HA CHIUSO CLIENT FD=%d CON USERNAME |%s| in PostFile\n", index, fd, sender);
            goto end;
        }

        //ricavo dimensione del file in kilobytes
        int dimFile =  (request2.data.hdr.len)/1024;

        //controllo che il file che il mittente vuole inviare NON superi la dimensione massima dei file contenuta
        //nel file di configurazione
        if(dimFile > MAXFILESIZE){
            //OP_MSG_TOOLONG
            sendResponse(ht, index, pool, fd, OP_MSG_TOOLONG, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
            goto end;
        }
        else{ //dimensione giusta

            // printf("FILENAME: %s\n", fileName);

            //ripulisco file da '/', se lo possiede, dato che il file che il client mi puo' inviare puo' avere
            // "./" oppure no
            char * newFileName = NULL;
            int    ch = '/';

            newFileName = strrchr( fileName, ch );
            //se non era presente '/' nel nome del file, allora il nome del file andava bene e lo riassegno
            if(newFileName == NULL)
                newFileName = fileName;

            //printf("SERVER STA PER RICEVERE IL CONTENUTO DEL FILE |%s| DAL CLIENT FD=%d IN POSTFILE\n", newFileName, fd);

            //lunghezza nome del file
            size_t fileName_size = strlen(newFileName);

            //lunghezza directory dove memorizzare i file
            size_t dir_size = strlen(DIRNAME);

            //Creo il path del file nella directory prevista nel file di configurazione
            if((path = calloc(dir_size + fileName_size + 2, sizeof(char))) == NULL){ // +2 perche; aggiungo / alla fine nome dir
                perror("calloc");
                fprintf(stderr, "Errore allocazione path in postFile\n");
                free(path);
                exit(EXIT_FAILURE);
            }

            //costruisco il percorso
            if(newFileName[0] == '/'){
                strncpy(path, DIRNAME, dir_size);
                strncat(path, newFileName, fileName_size);
            }
            else{
                strncpy(path, DIRNAME, dir_size);
                strncat(path, "/", 1);
                strncat(path, newFileName, fileName_size);
            }

            FILE *f;

            // printf("DIRNAME ##%s##\n", DIRNAME);

            //se non esiste la directory DirName, la creo con permessi universali
            if (mkdir(DIRNAME, 0777) == -1 && errno != EEXIST){
                //OP_FAIL
                perror("mkpath");
                fprintf(stderr, "Errore mkpath in postFile\n");

                //OP_FAIL
                sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
                goto end;
            }

            //printf("STO PER APRIRE IL FILE CON PATH IN POSTFILE: %s\n", path);


            lockAcquire(&mtx_file, "PostFile");

            //appro file in modalita' scrittura binaria [questo lo faccio perche' file potrebbe avere un contenuto grande]
            if((f = fopen(path, "wb")) == NULL){
                free(path);
                exit(EXIT_FAILURE);
            }

            // printf("HO APERTO CON SUCCESSO IL FILE CON PATH IN POST: %s\n", path);

            //ricavo il file descriptor del FILE* f
            int fileDescriptor = fileno(f);

            //printf("[POSTFILE di %s] fileDescriptor %d fileSize %zd file %s\n", request->hdr.sender, fileDescriptor, request2.data.hdr.len, path);

            //scrivo contenuto del file sul file, byte a byte [contenuto potrebbe essere molto grande]
            writen(fileDescriptor, request2.data.buf, request2.data.hdr.len);


            //chiudo file
            //fclose => ritorna 0 in caso di successo. EOF in caso di errore
            if(fclose(f) != 0){
                perror("fclose");
                fprintf(stderr, "Errore chiususra file |%s|\n", path);
                exit(EXIT_FAILURE);
            }
            f = NULL;

            lockRelease(&mtx_file, "PostFile");

            //provo ad inviare, inserendo nella sua historyQueue, il file al destinatario
            r = insertMessageInHashTable(ht, sender, receiver, request, FILE_MESSAGE); //request, perche' inserisco nome e lunghezza del nome del file nella History

            if(r == OP_NICK_UNKNOWN){
                // printf("UTENTE NON TROVATO IN POST\n");
                //OP_NICK_UNKNOWN => destinatario sconosciuto (non presente HashTable)
                sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

                //lockRelease(&mtx_file, "postFile");
                goto end;

            }
            else if(r == FUNCTION_FAILURE){
                // printf("ERRORE POST\n");
                //OP_FAIL
                sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

                //lockRelease(&mtx_file, "postFile");
                goto end;
            }
            else{
                //OP_OK
                // printf("INVIO OP_OK DELLA BUONA RIUSCITA DELLOPERAZIONE IN POSTFILE\n");
                // printf("Worker[%zd] ha inviato con successo il file |%s| del mitt |%s| con fd=%d al dest |%s| in postFile\n", index, request->data.buf, sender, fd, receiver);

                sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, 0, "\0", reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

                //incremento # di file NON ancora inviati
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 1, 0);

                //lockRelease(&mtx_file, "postFile");
                goto end;
            }
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet); //request perche' devo reperire nome client a cui inviare risposta

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        goto end;
    }

    end:
    //libero risorse allocate
    if(path != NULL){
        free(path);
        path = NULL;
    }

    //libero buffers che utente ha allocato quando mi ha inviato i suoi 2 messaggi
    if(request->data.buf != NULL){
        free(request->data.buf);
        request->data.buf = NULL;
    }
    if(request2.data.buf != NULL){
        free(request2.data.buf);
        request2.data.buf = NULL;
    }

}

size_t getFileSize(char* filename){
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

void getFile(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "getFile";
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */

    char* username = request->hdr.sender;

    char* path = NULL;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0) {

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        // getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //accedo tramite "fd" alla coda degli utenti connessi e reperisco l'indice del bucket in cui ho memorizzato il
        //nodo dell'utente e di conseguenza la sua historyQueue
        int bucket_index = ht->onlineQueue->connectionQueue[fd].bucket_index; //indice del bucket
        size_t sizeLockItems = ht->sizeLockItems; //dimensione array di locks

        if ((bucket_index < 0) || (bucket_index > (ht->size - 1))) {
            fprintf(stderr, "Errore indice bucket dove ricercare username %s: indice -%d- fuori range in getPreviuosMessages\n", username, bucket_index);
            exit(EXIT_FAILURE);
        }

        //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket
        int lock_index = bucket_index % ((int) sizeLockItems);;

        if ((lock_index < 0) || (lock_index > sizeLockItems)) {
            fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in getPreviuosMessages\n",
                    lock_index, bucket_index);
            exit(EXIT_FAILURE);
        }

        //acquisisco mutua esclusione sul bucket tramite la lock indicizzata da "lock_index" nell'array di locks
        //NEL CASO UN ALTRO THREAD ABBIA GIA' ACQUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
        //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
        lockAcquire(&ht->lockItems[lock_index], errMessage);

        //ricavo la coda dei messaggi dell'utente
        history_queue_t *msgsQueue = getHistoryQueue(&ht->items[bucket_index], username);

        //se l'utente non era presente nella lista di trabocco, la coda dei messaggi restituita e' vuota
        if (msgsQueue == NULL) {
            fprintf(stderr, "Utente |%s| non presente nella HashTable in getFile\n", request->hdr.sender);
            exit(EXIT_FAILURE);

        } else {

            //scandisco historyQueue alla ricerca del file che utente vuole reperire, per verificare che effettivamenete gli
            //sia stato inviato
            for(int i = 0; i < msgsQueue->qhsize; i++){

                //controllo che nella historyQueue ci sia un FILE_MESSAGGE con lo stesso nome del file che utente mi sta richiedendo
                if(strncmp(msgsQueue->messagesQueue[i].msg.data.buf, request->data.buf, request->data.hdr.len) == 0 && \
                msgsQueue->messagesQueue[i].msg.hdr.op == FILE_MESSAGE){

                    //lunghezza directory dove è memorizzato il file
                    size_t dir_size = strlen(DIRNAME);

                    //ripulisco file da '/', se lo possiede, dato che il file che ho memorizzato nella history dell'utente
                    // puo' avere "./" oppure no
                    char * newFileName = NULL;
                    int    ch = '/';

                    newFileName = strrchr( request->data.buf, ch );
                    // printf("new file prima %s\n", newFileName);
                    //se non era presente '/' nel nome del file, allora il nome del file andava bene e lo riassegno
                    if(newFileName == NULL)
                        newFileName = request->data.buf;
                    //printf("new file dopo %s\n", newFileName);

                    // printf("SERVER VUOLE INVIARE FILE |%s| AL CLIENT FD=%d\n", newFileName, fd);

                    //lunghezza del nome del file
                    size_t fileName_size = strlen(newFileName);

                    //creo il path del file nella directory prevista nel file di configurazione
                    if((path = calloc(dir_size + fileName_size + 2, sizeof(char))) == NULL){ // +2 perche' concateno al nome della directory un '/'
                        perror("calloc");
                        fprintf(stderr, "Errore allocazione path in getFile\n");
                        free(path);
                        exit(EXIT_FAILURE);
                    }

                    //costruisco il percorso
                    if(newFileName[0] == '/'){
                        strncpy(path, DIRNAME, dir_size);
                        strncat(path, newFileName, fileName_size);
                    }
                    else{
                        strncpy(path, DIRNAME, dir_size);
                        strncat(path, "/", 1);
                        strncat(path, newFileName, fileName_size);
                    }

                    lockAcquire(&mtx_file, "GetFile");

                    //printf("\nSTO PER APRIRE IL FILE CON PATH: %s\n\n", path);

                    //reperisco la dimensione del contenuto del file
                    size_t fileSize = getFileSize(path);

                    //printf("DIMENSIONE DEL CONTENTUO DEL FILE E' |||%zd|||\n", fileSize);

                    //apro il file, in sola lettura
                    int fileDescriptor = open(path, O_RDONLY, 0);

                    //apertura file andata a buon fine
                    if(fileDescriptor != -1){

                        //printf("\nAPERTURA ANDATA A BUON FINE\n\n");

                        /**
                         * MAPPO CONTENTUO DEL FILE CHE HO MEMORIZZATO SU DISCO NELLO SPAZIO VIRTUALE DEL PREOCESSO,
                         * PER POTERLO LEGGERE E SPEDIRE COME RISPOSTA AL CLIENT:
                         *
                         *  void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
                         *
                         * addr = NULL: Kernel sceglie a partire da quale indirizzo di memoria creare il "mapping"
                         * lenth = fileSize: dimensione del contenuto del file da mappare
                         * prot = PROT_READ: "mapping" accessibile in sola lettura
                         * flags = MAP_PRIVATE: "mapping" privato => aggiornamenti al "mapping" NON sono visibile agli altri processi
                         * [crea una  <<private copy-on-write mapping>>]
                         * fd = fileDescriptor: file descriptor del file da mappare
                         * offset = 0: posizione del file descriptor a partire dalla quale inziare "mapping" (intervallo mapping[0-fileSize])
                         */
                        //buffer risposta
                        // printf("[GETFILE di %s]fileDescriptor %d fileSize %zd file %s\n", request->hdr.sender, fileDescriptor, fileSize, path);
                        char* mmapedData = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE , fileDescriptor, 0);

                        //mappatura ha fallito
                        if(mmapedData == MAP_FAILED){
                            perror("mmaap");
                            fprintf(stderr, "Errore mappatura file |%s| da disco a memoria\n", path);
                            exit(EXIT_FAILURE);
                        }

                        //se sono arrivata qui, la mappatura del file e' andata a buon fine

                        // invio contenuto del file al client
                        int r = sendResponseToClient(ht, index, pool, fd, OP_OK, msgsQueue->messagesQueue[i].msg.hdr.sender, \
                        request, (unsigned int)fileSize, mmapedData, reinsertInWorkingSet, 0);

                        //chiudo file descriptor file
                        if(close(fileDescriptor) == -1){
                            perror("close");
                            fprintf(stderr, "Errore chiususra file |%s| in getFile\n", path);
                            exit(EXIT_FAILURE);
                        }

                        lockRelease(&mtx_file, "GetFile");

                        //invio risposta al client ha avuto successo
                        if(r == 0){
                            // printf("Worker[%zd] ha inviato con successo file |%s| all'username |%s| con fd=%d in getFile\n", index, request->data.buf, username, fd);

                            //setto che ho inviato il file all'utente
                            msgsQueue->messagesQueue[i].send = 1;

                            //incremento # di file  inviati
                            //decremento # di file NON inviati
                            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 1, -1, 0);

                            //rilascio mutua esclusione sul bucket
                            lockRelease(&ht->lockItems[lock_index], errMessage);
                            goto end;
                        }
                        else{ //utente si e' disconesso/problemi con connessione

                            //rilascio mutua esclusione sul bucket
                            lockRelease(&ht->lockItems[lock_index], errMessage);
                            goto end;
                        }
                    }
                    else{ //errore apertura file

                        //OP_FAIL
                        sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                        //incremento # di errori
                        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

                        //rilascio mutua esclusione sul bucket
                        lockRelease(&ht->lockItems[lock_index], errMessage);
                        goto end;

                    }
                }
            }

            //se sono arrivata qui vuol dire che il file non e' presente nella historyQueue dell'utente
            //sendResponse(ht, index, pool, fd, OP_NO_SUCH_FILE, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);
            sendResponse(ht, index, pool, fd, OP_NO_SUCH_FILE, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

            //rilascio mutua esclusione sul bucket
            lockRelease(&ht->lockItems[lock_index], errMessage);
            goto end;
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        //sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);
        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        goto end;
    }

    end:
    //libero risorse allocate
    if(path != NULL){
        free(path);
        path = NULL;
    }

    if(request->data.buf != NULL){
        free(request->data.buf); //devo liberare buf che client mi ha inviato dato che lo ha allocato dinamicamente
        request->data.buf = NULL;
    }
}

void getPreviuosMessages(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet) {
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "getPreviuosMessages";

    char* username = request->hdr.sender;

    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */

    //per mandare ad un utente i suoi messaggi, accedo in mutua esclusione alla sua historyQueue,
    //gli invio la dimensione (# messaggi) della sua historyQueue come body del primo messaggio di risposta,
    //con hdr OP_OK, e poi gli invio uno ad uno i messaggi come body e hdr TXT_MESSAGE, poi rilascio mutua esclusione

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0){

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        //getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //accedo tramite "fd" alla coda degli utenti connessi e reperisco l'indice del bucket in cui ho memorizzato il
        //nodo dell'utente e di conseguenza la sua historyQueue
        int bucket_index = ht->onlineQueue->connectionQueue[fd].bucket_index; //indice del bucket
        size_t sizeLockItems = ht->sizeLockItems; //dimensione array di locks

        if ((bucket_index < 0) || (bucket_index > (ht->size - 1))) {
            fprintf(stderr, "Errore indice bucket dove ricercare username %s: indice -%d- fuori range in getPreviuosMessages\n", username, bucket_index);

            //OP_FAIL
            sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
        else{ //bucket_index ok

            //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket
            int lock_index = bucket_index % ((int) sizeLockItems);

            if ((lock_index < 0) || (lock_index > sizeLockItems)) {
                fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in getPreviuosMessages\n",
                        lock_index, bucket_index);

                //OP_FAIL
                sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                //incremento # di errori
                //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
            }
            else{ //lock_index ok

                //acquisisco mutua esclusione sul bucke tramite la lock indicizzata da "lock_index" nell'array di locks
                //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
                //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
                lockAcquire(&ht->lockItems[lock_index], errMessage);

                //ricavo la coda dei messaggi dell'utente
                history_queue_t *msgsQueue = getHistoryQueue(&ht->items[bucket_index], username);

                //se l'utente non era presente nella lista di trabocco, la coda dei messaggi restituita e' vuota
                if (msgsQueue == NULL) {
                    //OP_FAIL
                    sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

                    //incremento # di errori
                    //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                    updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

                    //rilascio mutua esclusione sul bucket
                    lockRelease(&ht->lockItems[lock_index], errMessage);

                } else {

                    /**
                     * invio dimensione della historyQueue al client [QUESTO CONSENTE AL CLIENT DI SAPERE QUANTI MSG GLI INVIERO']
                     * e i messaggi della sua historyQueue
                     */
                    sendResponseAndMessages(ht, index, pool, fd, request, msgsQueue, reinsertInWorkingSet);

                    //rilascio mutua esclusione sul bucket
                    lockRelease(&ht->lockItems[lock_index], errMessage);
                }
            }
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
    }
}

void getConnectedUsers(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    /**
     * NON POSSO AVERE CONSISTENZA SULLA CODA DEI CONNESSI, ECCO PERCHE' NON E' PROTTETA DA MUTUA ESCLUSIONE
     */

    char* username = request->hdr.sender;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0){

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
       // getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //alloco array di usernames da inizializzare con i nomi degli utenti connessi
        char* onlineUsers = NULL;

        //nuova posizione in onlineUsers in cui scrivere
        int newPos=0;
        //# utenti conessi che ho inserito fino ad ora in onlineUsers
        int counter = 0;

        //dimensione buffer username utenti online
        int onlineUsers_len = 0;
        int disconnectedUsesrs = 0;

        //mi calcolo # utenti connessi, sommando gli utenti locali connessi degli workers
        for(int i = 0; i < pool->threads_in_pool; i++){
            disconnectedUsesrs = disconnectedUsesrs + (int)pool->threadsStats[i].nonline; //# utenti disconessi dagli workers
        }
        onlineUsers_len = numConnectedUsers - disconnectedUsesrs; //# utenti connessi dal Listener - # utenti disconessi dagli workers

        onlineUsers_len = (MAX_NAME_LENGTH + 1)*onlineUsers_len;

        //(MAX_NAME_LENGTH + 1)*((size_t)numConnections) => (dimensione di un username)*(# utenti connessi)
        if((onlineUsers = (char*) calloc((size_t)onlineUsers_len, sizeof(char))) == NULL){
            perror("calloc getConnectedUsers");
            fprintf(stderr, "Errore allocazione buffer che conterra' i nomi degli utenti connessi\n");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < ht->onlineQueue->qosize; i++){
            if(ht->onlineQueue->connectionQueue[i].empty_struct == 0){

                //Salvo la nuova posizione di inserzione nel contenitore di memorizzazione
                newPos = (MAX_NAME_LENGTH + 1) * counter;

                //Aggiorno il numero di client copiati
                counter++;

                //copio nome utente connesso in onlineUsers
                strncpy(onlineUsers + newPos, ht->onlineQueue->connectionQueue[i].username, \
            strlen(ht->onlineQueue->connectionQueue[i].username));
            }
        }

        //printf("Worker[%zd] ha inviato con successo OnlineQueue all'username |%s| con fd=%d in getConnectedUsers\n", index, username, fd);

        //invio OP_OK e buffer che contiene nomi degli utenti connessi al client
        sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, (unsigned int)onlineUsers_len, onlineUsers, reinsertInWorkingSet);
        //DEALLOCAZIONE DELLA RISORSA ALLOCATA VIENE FATTA IN "sendResponse"
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
    }
}


void unregisterUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */
     char* username = request->hdr.sender;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0){

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
        //getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //provo ad cancellare utente dalla HashTable
        int r = removeUserInHashTable(ht, username);

        if(r == OP_NICK_UNKNOWN){
            sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);

        }
        else if(r == FUNCTION_FAILURE){
            sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
        else{

           // printf("Worker[%zd] ha deregistrato con successo username |%s| con fd=%d in unregisterUser\n", index, username, fd);

            //decremento # di utenti registrati
            //il decremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], -1, 0, 0, 0, 0, 0, 0);

            //disconetto utente => lo elimino dalla coda degli utenti connessi
            disconnectUser(ht, index, pool, fd, request, reinsertInWorkingSet);
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
    }
}

void disconnectUser(ht_hash_table_t* ht, size_t index, threadpool_t* pool, int fd, message_t* request, int* reinsertInWorkingSet){
    /**
     * NON CONTROLLO SE UTENTE E' CONNESSO, PERCHE' SE SONO ARRIAVATA QUI SARA' CONNESSO PER FORZA
     */

    char* username = request->hdr.sender;

    //controllo che utente connesso sia lo stesso di quello che mi richiede servizio
    if(strncmp(ht->onlineQueue->connectionQueue[fd].username, username, strlen(username)) == 0){

        //se ci sono messaggi pendenti (non ancora inviati all'utente da parte di altri utenti) gliegli invio
       // getPendingMessages(ht, index, pool, fd, request, reinsertInWorkingSet);

        //provo a cancellare utente dalla OnlineQueue (indicizzandolo tramite suo "fd")
        int r = popOnlineUser(ht->onlineQueue, fd, username);

        if(r == FUNCTION_FAILURE){
            //invio hdr di errore al client
            sendResponse(ht, index, pool, fd, OP_FAIL, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //incremento # di errori
            //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
            updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
        }
        else{ //OP_OK

           // printf("Worker[%zd] ha disconesso con successo username |%s| con fd=%d in disconnectUser\n", index, username, fd);

            //invio hdr di successo al client
            r = sendResponse(ht, index, pool, fd, OP_OK, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

            //invio risposta ha avuto successo
            if(r == 0){
                //chiudo socket "fd" del client
                CLOSECLIENT(&fd, "disconnectUser");

                //incremento # di utenti disconnessi
                //l' incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                updateStats(&pool->threadsStats[index], 0, 1, 0, 0, 0, 0, 0);

                //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
                *reinsertInWorkingSet = 0;
            }
        }
    }
    else{ //utente connesso non e' lo stesso dell'utente che mi richiede servizio

        sendResponse(ht, index, pool, fd, OP_NICK_UNKNOWN, "ChattyServer", request, 0, "\0", reinsertInWorkingSet);

        //incremento # di errori
        //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
        updateStats(&pool->threadsStats[index], 0, 0, 0, 0, 0, 0, 1);
    }
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */