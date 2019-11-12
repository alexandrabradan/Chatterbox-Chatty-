#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>

#include "ThreadPool.h"

/**
 * @file ThreadPool.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione dell'interfaccia  ThreadPool.h, che raccoglie le funzioni per creare, gestire e far\n
 * terminare un ThreadPool di dimensione fissata.
 */

/**
 * @var arguments: array, globale alle funzioni dell'interfaccia @file ThreadPool.h, che raccoglie gli argomenti da passare
 * agli worker-threads [array viene allocato in "threadpool_create" e deallocato in "threadpool_free"]
 *
*/
arg_t* arguments = NULL;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

void* threadpool_thread(void* arguments){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "threadpool_thread";

    arg_t *args = (arg_t *)arguments;

    while(true){

        //Prelevo un task dalla coda di lavoro
        task_node_t popedTask;
        //azzerro task
        popedTask.fd = -1;
        popedTask.set = NULL; //forse e' meglio FD_ZERO(popedTask.set)
        popedTask.fd_num = NULL;

        popTask(args->pool->workQueue, &popedTask);

        //non ho prelevato nessun task, perche' ThreadPool e' in chiusura e sono terminati => esco dal <<ciclo di lavoro>>
        if(popedTask.fd == -1 || popedTask.set == NULL  || *popedTask.fd_num == -1){
            break;
        }

        message_t request;
        memset(&request, 0, sizeof(message_t));
        int r = 1;

        //se "fd" appartiene ai file descriptors della Select, vuol dire che ho un client NON in "Coda di Ibernazione"
        // (client NON ha fatto MENO_R_OP)
        if(popedTask.fd >= 0 && popedTask.fd < 1024){
            //leggo hdr msg che client mi ha inviato, per sapere quale richiesta desidera e di conseguenza quale routine
            //devo invocare per soddisfarla

           r = readServerMsg(popedTask.fd, &request);
        }
        else { //client e' registrato e si trovava in "Coda di Ibernazione" (ha fatto la MENO_R_OP)

            popedTask.fd = popedTask.fd - MAX_FD_SELECT; //recupero "file descriptor" originario
            setHeader(&request.hdr, MENO_R_OP, args->ht->onlineQueue->connectionQueue[popedTask.fd].username);
        }


        int reinsertInWorkingSet = 1;

        if(r < 0){
            //fprintf(stderr, "Worker %zd ha letto |%d| bytes dalla richiesta del client, perche' e' subentrato un errore di connessione\n", args->index, r);

            //controllo se client e' connesso (potrebbe non esserlo, se per esempio non si e' ancora registarto oppure si
            // e' disconesso ed ora riprova a riconettersi) e se lo e' lo disconetto
            if(args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct == 0){
                args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct = 1;
                args->ht->onlineQueue->connectionQueue[popedTask.fd].bucket_index = -1;
                memset(args->ht->onlineQueue->connectionQueue[popedTask.fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
                args->ht->onlineQueue->connectionQueue[popedTask.fd].p_user_in_overflow_list = NULL;
            }

            CLOSECLIENT(&popedTask.fd, "Client ha chiuso connessione\n");

            updateStats(&args->pool->threadsStats[args->index], 0, 1, 0, 0, 0, 0, 0);

            //printf("WORKER[%zd] HA CHIUSO CLIENT FD = %d in threadpool_thread\n", args->index, popedTask.fd);

            //mi occupo del task successivo, senza reinserire "fd" del client nel "working set"
            //(client dovra' "connettersi" nuovamente al Server e Server "accettarlo" per poter chiedere altri servizi)
            continue;

        }
        else if(r == 0){
            //fprintf(stderr, "Worker %zd ha letto |%d| bytes dalla richiesta del client %d, perche' client ha chiuso connessione\n", args->index, r, popedTask.fd);


            //controllo se client e' connesso (potrebbe non esserlo, se per esempio non si e' ancora registarto oppure si
            // e' disconesso ed ora riprova a riconettersi) e se lo e' lo disconetto
            if(args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct == 0){
                //disconetto client [NON FACCIO CONTROLLI SU CHI SIA CONNESSO, DATO CHE NON SONO RIUSCITA A LEGGERE NOME CLIENT]
                args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct = 1;
                args->ht->onlineQueue->connectionQueue[popedTask.fd].bucket_index = -1;
                memset(args->ht->onlineQueue->connectionQueue[popedTask.fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
                args->ht->onlineQueue->connectionQueue[popedTask.fd].p_user_in_overflow_list = NULL;
            }


            CLOSECLIENT(&popedTask.fd,"Client ha chiuso connessione\n");

            updateStats(&args->pool->threadsStats[args->index], 0, 1, 0, 0, 0, 0, 0);

           // printf("WORKER[%zd] HA CHIUSO CLIENT FD = %d in threadpool_thread\n", args->index, popedTask.fd);

            //mi occupo del task successivo, senza reinserire "fd" del client nel "working set"
            //(client dovra' "connettersi" nuovamente al Server e Server "accettarlo" per poter chiedere altri servizi)
            continue;

        }
        else{ //lettura richiesta client e' andata a buon fine

            //printf("WORKER[%zd] STA PER SODDISFARE RICHIESTA DELL'USERNAME = %s CON  FD=%d CHE FA PARTE DEL SET = %p IL CUI FD_NUM E' %d\n", args->index, request.hdr.sender, popedTask.fd, (void*)&popedTask.set, *popedTask.fd_num);

            //switch sull'operazione che mi richiede il client, per poter innvocare la funzione-routine che la gestisca
            switch(request.hdr.op){
                case REGISTER_OP:{
                    //client richiede registrazione alla chat => connessione + invio lista utenti connessi in automatico
                    registerUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case CONNECT_OP:{
                    //client richiede connessione alla chat => invio lista utenti connessi in automatico
                    connectUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case POSTTXT_OP:{
                    //client richiede di inviare un msg ad un altro utente
                    postMessage(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case POSTTXTALL_OP:{
                    //client richiede di inviare un msg a tutti gli utenti della chat
                    postMessageToAll(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case POSTFILE_OP:{
                    //client richiede di inviare un file ad un altro utente
                    postFile(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case GETFILE_OP:{
                    //client richiede di recuperare il file che un altro utenti gli ha inviato
                    getFile(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case GETPREVMSGS_OP:{
                    //client richiede di recuperare i suoi msg precedenti (quelli della sua historyQueue)
                    getPreviuosMessages(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case USRLIST_OP:{
                    //client richiede la lista dei connesssi (la stessa routine viene usata dal worker-thread per mandare ad
                    // un client appena registrato o/e appena connesso la lista degli utenti attualmente connessi alla chat)
                    getConnectedUsers(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case UNREGISTER_OP:{
                    //client richiede deregistrazione dalla chat
                    unregisterUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case DISCONNECT_OP:{
                    //client richide disconnessione dalla chat
                    disconnectUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                    break;
                }
                case MENO_R_OP:{

                    //client si e' messo in "attesa attiva"
                    //provo a vedere se c'e' un msg pendente per il client ed in caso affermativo glielo invio
                   r = sendMinusRMessage(args->ht, args->index, args->pool, popedTask.fd, &reinsertInWorkingSet);

                    //ho inviato un msg pendete
                    if(r == 0){
                        //lascio reinsertInWorkingSet a 1 per reinerire fd nel "working set"
                    }
                    else if(r == 1){ //non c'erano msg pendenti per il client

                      if(args->pool->workQueue->shutdown == 1){
                      //disconetto utente => lo elimino dalla coda degli utenti connessi
                      //disconnectUser(args->ht, args->index, args->pool, popedTask.fd, &request, &reinsertInWorkingSet);
                          if(args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct == 0){
                              args->ht->onlineQueue->connectionQueue[popedTask.fd].empty_struct = 1;
                              args->ht->onlineQueue->connectionQueue[popedTask.fd].bucket_index = -1;
                              memset(args->ht->onlineQueue->connectionQueue[popedTask.fd].username, 0, sizeof(char)*(MAX_NAME_LENGTH + 1));
                              args->ht->onlineQueue->connectionQueue[popedTask.fd].p_user_in_overflow_list = NULL;
                          }

                      //chiudo socket "fd" del client
                      CLOSECLIENT(&popedTask.fd, "errore scrittura risposta al client, client si e' disconesso");
                      
                      //aggiorno statistiche
                      updateStats(&args->pool->threadsStats[args->index], 0, 1, 0, 0, 0, 0, 0);

                       //faccio sapere al worker-thread che non deve reinserire "fd" in "set"
                      reinsertInWorkingSet = 0;
                      }
                      else{

                         //inserisco client in coda di ibernazione (rittentero' a vedere se c'e' un msg per lui quando
                        // il listener-thread lo estrara' dalla coda di ibernazione e lo mettera' in "coda di lavoro")
                        int hibernateFD =  popedTask.fd + MAX_FD_SELECT;
                        pushHibernationQueue(args->pool->hibQueue, hibernateFD); //FD_SETSIZE = 1024 [max. fd della Select]

                        //segno che worker NON deve reinserire client nel "working set"
                        reinsertInWorkingSet = 0;
                      }
                    }
                    //se r == -1 in "sendResponse" ho disconesso client, chiuso socket e settato reinsertInWorking=0

                    //dealloco buffer richiesta client, se allocato
                    if(request.data.buf != NULL){
                        free(request.data.buf);
                        request.data.buf = NULL;
                    }
                    break;
                }
                default:{

                    //invio msg di errore al client
                    sendResponse(args->ht, args->index, args->pool, popedTask.fd,  OP_FAIL, "ChattyServer", &request, 0, "", &reinsertInWorkingSet);

                    //aggiorno statistiche locali del worker-thread
                    //incremento # di errori
                    //l'incremento viene fatto nelle statistiche locali del thread che sta eseguendo routine (mutua esclusione e' data da cio')
                    updateStats(&args->pool->threadsStats[args->index], 0, 0, 0, 0, 0, 0, 1);

                    //dealloco buffer richiesta client, se allocato
                    if(request.data.buf != NULL){
                        free(request.data.buf);
                        request.data.buf = NULL;
                    }
                }

            }

            /**
             *  AGGIORNAMENTO DELLE STATISTICHE LOCALI DEL WORKER-THRAED E' STATO FATTO NELLE FUNZIONI-ROUTINE.
             *  LO STESSO VALE PER L'INVIO DELL'ESITO DELLE ROUTINE AL CLIENT
             */

            //reinserisco "fd" nel "working set" solo se non sono subentrati errori (come disconessioni da parte del client)
            if( reinsertInWorkingSet == 1){
                //reinserisco "fd" del client nel "set"
                lockAcquire(&mtx_set, errMessage);
                FD_SET(popedTask.fd, popedTask.set); //popedTask.set e' puntatore a fd_set set (no &)

                //aggiorno max # di file descriptor
                if(popedTask.fd > *popedTask.fd_num)
                    *popedTask.fd_num = popedTask.fd;

                lockRelease(&mtx_set, errMessage); //aggiorno in mutua esclusione anche fd_num
            }
        }
    }

    //Esco dal ciclo di lavoro
    //printf("WORKER[%zd] E' USCITO DAL CICLO DI LAVORO\n", args->index);

    //Acquisisco mutua esclusioneprintf
    lockAcquire(&args->pool->workQueue->lockTaskQueue, errMessage);

    //Decremento il numero di worker-threads attivi
    args->pool->thread_started = args->pool->thread_started -1;

    //Rilascio la lock
    lockRelease(&args->pool->workQueue->lockTaskQueue, errMessage);

    //Faccio terminare il worker-thread
    return NULL;
}

threadpool_t* threadpool_create(size_t threads_in_pool, size_t qtsize, ht_hash_table_t* ht){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "threadpool_create";

    threadpool_t* pool = NULL;

    if((pool = (threadpool_t *)calloc(1, sizeof(threadpool_t))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore calloc threadpool in threadpool_create, errno %d\n", errno);
        free(pool);
        exit(EXIT_FAILURE);
    }

    //Inizializzo ThreadPool
    pool->thread_started = 0; //# worker-threads attivi/inziati = 0
    pool->threads_in_pool = threads_in_pool;


    //inizializzo coda di ibernazione
    //MAXCONNECTIONS*BASE_ONLINEQUEUE => stessa dimensione della coda utenti online
    size_t qhibsize = (size_t)MAXCONNECTIONS*BASE_ONLINEQUEUE;

     pool->hibQueue = initHibernationQueue(qhibsize);

     if(pool->hibQueue == NULL){
        free(pool->hibQueue);
        free(pool);
        exit(EXIT_FAILURE);
    }


    //alloco array che memorizza ID degli worker-threads
    if((pool->threads = (pthread_t *)calloc(pool->threads_in_pool, sizeof(pthread_t))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore calloc array threads in threadpool_create, errno %d\n", errno);
        free(pool->threads);
        free(pool);
        exit(EXIT_FAILURE);
    }

    //alloco array che memorizza le statistiche di ogni worker-thread
    if((pool->threadsStats = (struct statistics *)calloc(pool->threads_in_pool, sizeof(struct statistics))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore calloc array statistiche di ogni worker in threadpool_create, errno %d\n", errno);
        free(pool->threads);
        free(pool->threadsStats);
        free(pool);
        exit(EXIT_FAILURE);
    }

    //alloco array che raccoglie gli argomenti da passare agli worker-threads
    if((arguments = (arg_t *)calloc(pool->threads_in_pool, sizeof(arg_t))) == NULL) {
        perror("calloc");
        fprintf(stderr, "Errore calloc array che raccoglie gli argomenti da passre agli worker-threads in threadpool_create, errno %d\n", errno);
        free(pool->threads);
        free(pool->threadsStats);
        free(arguments);
        free(pool);
        exit(EXIT_FAILURE);
    }

    //inizializzo coda di lavoro
    pool->workQueue = initTaskQueue(qtsize);

    if(pool->workQueue == NULL){
        free(pool->threads);
        free(pool->threadsStats);
        free(arguments);
        free(pool);
        exit(EXIT_FAILURE);
    }

    //Creo ed attivo gli worker-threads
    for(int i = 0; i < pool->threads_in_pool; i++){

        //inizializzo array delle statistiche
        pool->threadsStats[i].nusers = 0;
        pool->threadsStats[i].nonline = 0;
        pool->threadsStats[i].ndelivered = 0;
        pool->threadsStats[i].nnotdelivered = 0;
        pool->threadsStats[i].nfiledelivered = 0;
        pool->threadsStats[i].nfilenotdelivered = 0;
        pool->threadsStats[i].nerrors = 0;

        //inizializzo array degli argomenti da passare agli worker-threads
        //ad ogni worker passo:
        //a) ThreadPool dal quale reperire la coda di lavoro
        //b) indice che lo identifica nell'array degli ID e con il quale puo' accedere all'array delle statistiche ed aggiornare le sue
        //c) HashTable dalla quale reperire bucket degli utenti e dei msg + coda degli utenti connessi
        arguments[i].index = (size_t)i;
        arguments[i].pool = pool;
        arguments[i].ht = ht;
        ptCreate(&pool->threads[i], threadpool_thread, (void*)&arguments[i], errMessage);
        pool->thread_started = pool->thread_started + 1;
    }

    return pool;
}

void threadpool_print(threadpool_t* pool){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "threadpool_print";

    printf("\n");
    printf("STAMPA THREADPOOL\n");

    if(pool == NULL){
        printf("Threadpool vuoto\n");
        printf("\n");
    }
    else{
        lockAcquire(&pool->workQueue->lockTaskQueue, errMessage);
        printf("thread_started: %d\n", pool->thread_started);
        for(int i = 0; i < pool->threads_in_pool; i++){
            printf("threads[%d]: %lu ", i, pool->threads[i]); //pthreads[i] == unsigned long
        }
        printf("\n");
        printf("\n");

        printTaskQueue(pool->workQueue);

        lockRelease(&pool->workQueue->lockTaskQueue, errMessage);
    }
}

void threadpool_destroy(threadpool_t *pool){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "threadpool_destroy";

    //Acquisisco mutua esclusione sul ThreadPool
    lockAcquire(&pool->workQueue->lockTaskQueue, errMessage);

    if(pool->workQueue->shutdown != 0){
        fprintf(stderr,"Errore threadpool_destroy (threadpool sta gia' terminando) in threadpool_destroy\n");
        lockRelease(&pool->workQueue->lockTaskQueue, errMessage);
        exit(EXIT_FAILURE);
    }

    //Mi segno che il ThreadPool sta terminando[TERMINAZIONE GRACEFUL] e non devo piu' accettare nuovi task
    pool->workQueue->shutdown = 1;

    //Risveglio tutti gli worker per farli compiere lavoro e fargli terminare
    condBroadcast(&pool->workQueue->emptyTaskQueueCond, errMessage);

    //Rilasciando la mutua esclusione affinche la possano acquisire gli workers
    lockRelease(&pool->workQueue->lockTaskQueue, errMessage);

    //Aspetto che tutti gli worker-threads terminino
    for(int i = 0; i < pool->threads_in_pool; i++) {
        ptJoin(pool->threads[i], NULL);
    }

    //dealocco il ThreadPool se sopra e' andato tutto a buon fine
    threadpool_free(pool);
}

void threadpool_free(threadpool_t* pool){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "threadpool_free";

    lockAcquire(&pool->workQueue->lockTaskQueue, errMessage);

    if(pool->thread_started > 0){
        fprintf(stderr, "Errore in threadpool_free, non posso deallocare un ThreadPoll che ancora in esecuzione worker-threads\n");
        lockRelease(&pool->workQueue->lockTaskQueue, errMessage);
        exit(EXIT_FAILURE);
    }

    //dealloco coda di lavoro
    deleteTaskQueue(pool->workQueue);

    //dealloco array che memorizza gli ID degli worker threads
    free(pool->threads);

    //dealloco array delle statistiche
    free(pool->threadsStats);

    //dealloco array degli argomenti da passare agli worker-threads
    free(arguments);

    //dealloco coda di ibernazione
    deleteHibernationQueue(pool->hibQueue);

    //dealloco THreadPool
    free(pool);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */




