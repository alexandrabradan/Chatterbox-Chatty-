#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Ascoltatore.h"


#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 * @file Listener.c
 * @file ThreadPool.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzioni dell'inertfaccia  Listener.h , che raccoglie le funzioni utili per l'avvio\n
 * e la terminazione di un thread-listener.
 */


/* Variabile usata per memorizzare il segnale ricevuto
 * La definizione come "volatile sig_atomic_t" permette l'accesso
 * signal safe alla variabile globale
 */
volatile sig_atomic_t sigTerm;  //Per ricevere i segnali di terminazione
volatile sig_atomic_t sigStats;  //Per ricevere il segnale di stampa delle statistiche


//gestore terminazione
void termination_handler(int signal) {
    //scrivo sullo stout,  in modo signal safe,  che è arrivato un nuovo segnale
    write(1, "--Ho catturato un nuovo segnale di terminazione--\n", strlen("--Ho catturato un nuovo segnale di terminazione--\n"));
    //Salvo il segnale in modo signal safe
    sigTerm = signal;
}

//gestore statistiche
void statistics_handler(int signal) {
    //scrivo sullo stout,  in modo signal safe,  che è arrivato un nuovo segnale
    write(1, "-- Ho catturato un nuovo segnale di stampa statistiche--\n", strlen("-- Ho catturato un nuovo segnale di stampa statistiche--\n"));
    //Salvo il segnale in modo signal safe
    sigStats = signal;
}

//registrazione segnali
void signals_registration(){

    //azzerro variabile gestione segnali
    sigTerm = 0;
    //azzero variabile gestione statistiche
    sigStats = 0;

    //struct usata per personalizzare la gestione dei segnali del listener-thread
    struct sigaction sa;
    //azzero il contenuto della struttura sigaction
    memset(&sa, 0, sizeof(sa));
    //installo  gestore dei segnali di terminazione
    sa.sa_handler = termination_handler;

    //registro i segnali per la terminazione del server
    if((sigaction(SIGINT, &sa, NULL) == -1)){
        perror("Errore sigaction: SIGINT");
        exit(EXIT_FAILURE);
    }
    if((sigaction(SIGQUIT, &sa, NULL) == -1)){
        perror("Errore sigaction: SIGQUIT");
        exit(EXIT_FAILURE);
    }
    if((sigaction(SIGTERM, &sa, NULL) == -1)){
        perror("Errore sigaction: SIGTERM");
        exit(EXIT_FAILURE);
    }

    //installo  gestore dei segnali di terminazione
    sa.sa_handler = statistics_handler;
    if((sigaction(SIGUSR1, &sa, NULL) == -1)){
        perror("Errore sigaction: SIGUSR1");
        exit(EXIT_FAILURE);
    }

    //installo  gestore per ignorare SIGPIPE
    sa.sa_handler = SIG_IGN;
    //registro  segnali SIGPIPE da ignorare
    if((sigaction(SIGPIPE, &sa, NULL) == -1)){
        perror( "Errore sigaction: SIGPIPE");
        exit(EXIT_FAILURE);
    }

    //Restart functions if interrupted by handler
    //sa.sa_flags = SA_RESTART;
}

//stampa statistiche sul file all'arrivo del segnale opportuno SIGUSR1
void handle_stats(){
    //printf("--Stampa delle statistiche sul file--\n");

    FILE *file_stats = fopen(STATFILENAME, "a");
    if (file_stats != NULL) {

        //concateno le statistiche nel file delle statistiche
        printStats(file_stats);

        //chiudo file delle statistiche
        fclose(file_stats);
    }
    else{
        perror("Errore nell'apertura del file delle statistiche");
        exit(EXIT_FAILURE);
    }
}


void* listener(void* stats){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "listener";

    printf("\nLISTENER STARTED...\n");

    //casto argomento al tipo specifico
    struct statistics* chattyStats = (struct statistics*) stats;

    /**************************************INIZIALIZZO VARIABILI GLOBALI***********************************************/
    lockInit(&mtx_set, "listener");
    numConnectedUsers = 0;
    lockInit(&mtx_file, "listener");

    /*****************************************GESTIONE SEGNALI********************************************************/
    //registrazione dei segnali
    signals_registration();

    printf("SIGNALS REGISTRATION DONE...\n");

    /********************************CREAZIONE SOCKET FILE DESCRIPTOR**************************************************/

    //cancello il socket file se esiste gia'
    unlink(UNIXPATH);

    int sfd; //socket file descriptor
    int csfd; //clinet socket file descriptor dell'accept
    int fd; //indice select
    int fd_num = 0; //# massimo file descriptor attivi

    fd_set set; //insieme file descriptor [master_set]
    fd_set rdset; //insieme file descriptors attivi [working_set]

    //apro socket file descriptor
    SYSCALL(&sfd, socket(AF_UNIX, SOCK_STREAM, 0), "server-socket");

    //mantengo il num # di connessioni in fd_num
    if (sfd > fd_num)
        fd_num = sfd;

    //inizializzo indirizzo AF_UNIX
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, UNIXPATH, strlen(UNIXPATH) + 1);

    int esito;
    //assegno un indirizzo AF_UNIX al server
    SYSCALL(&esito, bind(sfd, (struct sockaddr *) &sa, sizeof(sa)), "bind");

    //setto server per accettare connessioni
    SYSCALL(&esito, listen(sfd, SOMAXCONN), "listen");

    //azzerro set file descriptor
    FD_ZERO(&set);

    //aggiugo "sfd" al set di file descriptor
    FD_SET(sfd, &set);

    //azzerro set file descriptor attivi
    FD_ZERO(&rdset);


    printf("SERVER SOCKET OPENED....\n");

    /*************************************CREAZIONE HASHTABLE**********************************************************/
    //alloco ed inizializzo HashTable, che contiene anche la coda degli utenti connessi (inizializzo anche questa)

    ht_hash_table_t* ht = NULL;

    size_t size = BASE_HASHTABLE*((size_t)MAXCONNECTIONS);

    ht = initHashTable(size, (size_t)MAXCONNECTIONS); //dim. ht e dim. array di lock

    if(ht == NULL){
        fprintf(stderr, "Errore allocazione HashTable\n");
        return NULL; //faccio terminare thread-listener
    }

    printf("HASHTABLE CREATED...\n");

    /*************************************CREAZIONE THREADPOOL*********************************************************/
    //alloco ed inizializzo ThreadPool, attivando anche gli worker-threads

    threadpool_t* pool = NULL;
    //pool = threadpool_create(MAX_THREAD, MAX_CONN, &ht);
    pool = threadpool_create((size_t)THREADSINPOOL, (size_t)MAXCONNECTIONS, ht);

    if(pool == NULL){
        fprintf(stderr, "Errore allocazione threadpool\n");
        deleteHashTable(ht);
        return NULL; //faccio terminare thread-listener
    }

    printf("THREADPOOL CREATED....\n");

    /************************************CICLO DI ASCOLTO RICHIESTE****************************************************/

    printf("LISTENER IS WAITING CONNECTIONS....\n");

    while(!((sigTerm == SIGINT) || (sigTerm == SIGQUIT) || (sigTerm == SIGTERM))){

        //timer per la select
        struct timeval tsel;
        tsel.tv_sec = 0; //0 secondi
        //tsel.tv_usec = 10000; //1000 microsecondi
        tsel.tv_usec = 500; //10000 microsecondi

        //gestisco  la stampa delle statistiche se arriva il segnale corrispondente
        if(sigStats == SIGUSR1) {

            int onlineUsers = 0;
            int disconnectedUsesrs = 0;

            //mi calcolo # utenti connessi, sottraendo agli utenti globali connessi dal listener gli utenti locali disconessi dagli workers
            for(int i = 0; i < pool->threads_in_pool; i++){
                disconnectedUsesrs = disconnectedUsesrs + (int)pool->threadsStats[i].nonline; //# utenti disconessi dagli workers

                updateStats(chattyStats, (int)pool->threadsStats[i].nusers, 0, (int)pool->threadsStats[i].ndelivered, \
                (int)pool->threadsStats[i].nnotdelivered, (int)pool->threadsStats[i].nfiledelivered, (int)pool->threadsStats[i].nfilenotdelivered, \
                (int)pool->threadsStats[i].nerrors);
            }
            onlineUsers = numConnectedUsers - disconnectedUsesrs; //# utenti connessi dal Listener - # utenti disconessi dagli workers

            //assegno # utenti connessi risultanti
            chattyStats->nonline = (unsigned)onlineUsers;

            //stampa su file delle statistiche
            handle_stats();

            //azzero la variabile che controlla le statistiche
            sigStats = 0;
        }

        lockAcquire(&mtx_set, errMessage);
        rdset = set; //riinizializzo ogni volta rdset a set perche' select lo modifica
        lockRelease(&mtx_set, errMessage);

        //seleziono file descriptor pronti per operazioni I/O
        esito = select(fd_num + 1, &rdset, NULL, NULL, &tsel); //fd_num + 1 => voglio # descrittori attivi (non indice massimo)

        //se la  SELECT ha fallito per:
        //a) per interruzione (segnale SIGINT, SIGQUIT o SIGTERM)
        //b) richiesta stampa statistiche (segnale SIGUSR1)
        //i relativi handlers se ne occupano
        //altrimenti, rieseguo ciclo e ritento la SELECT
        if(esito < 0)
            continue;

        //SELECT ok se sono arrivata qui

        //itero sui file descriptor pronti
        for (fd = 0; fd <= fd_num; fd++) {

            //se file descriptor appartiene al "rdset"
            if (FD_ISSET(fd, &rdset)) {
                //fd del server => deve accettare connessioni
                if (fd == sfd) {
                    SYSCALL(&csfd, accept(sfd, (struct sockaddr *) NULL, NULL), "accept");
                   
                    //controllo il numero di connessioni e nel caso si sia raggiunto il tetto massimo
                    //rifiuto la connessione del nuovo client

                    //flag che mi dice se devo accettare nuova connessione, o mandare messaggio di errore al rispettivo
                    //client
                    int refuse = 0;

                    int onlineUsers = 0;
                    int disconnectedUsesrs = 0;

                    //mi calcolo # utenti connessi, sottraendo agli utenti globali connessi dal listener gli utenti locali disconessi dagli workers
                    for(int i = 0; i < pool->threads_in_pool; i++){
                        disconnectedUsesrs = disconnectedUsesrs + (int)pool->threadsStats[i].nonline; //# utenti disconessi dagli workers
                    }
                    onlineUsers = numConnectedUsers - disconnectedUsesrs; //# utenti connessi dal Listener - # utenti disconessi dagli workers*/

                    //Devo rifiutare la connessione perché ho superato il tetto massimo
                    if(onlineUsers > MAXCONNECTIONS)
                        refuse = 1;

                    if(refuse == 1){
                       // printf("SERVER REFUSED CLIENT WITH FD = %d\n", csfd);

                        //invio messaggio di errore al client

                        message_hdr_t response;
                        setHeader(&response, OP_FAIL, "ServerChatty");
                        //Invio il messaggio di rifiuto della connessione
                        sendHeader(fd, &response);

                        //Incrementiamo il numero di errori nelle Statistiche
                        /**
                         * N.B. mutua esclusione e' data dal fatto che fino a quando non sopraggiunge SIGURS1, nessuno
                          aggiorna le statistiche globali (lo fa solo il listener), ma ogni worker-thread aggiorna
                          solo le sue statistiche locali
                         */
                        updateStats(chattyStats, 0,0,0,0,0,0,1);

                        //chiudo connessione con il client
                        CLOSECLIENT(&fd, "Server ha raggiunto MAXCONNECTIONS\0");
                    }
                    else{ //non ho ancora raggiunto # max connessioni => accetto client

                        lockAcquire(&mtx_set, errMessage);
                        //inserisco "csfd" nel set dei file descriptor attivi
                        FD_SET(csfd, &set);

                        //aggiorno num # di connessioni in fd_num
                        if (csfd > fd_num)
                            fd_num = csfd;
                        lockRelease(&mtx_set, errMessage); //aggiorno anche fd_num in mutua esclusione cosi

                        //incremento # utenti connessi dal listener
                        numConnectedUsers++;

                        //printf("LISTENER ACCEPTED CLIENT %d\n", csfd);
                    }

                } else { //socket di un client, gia' connesso, e' pronto per operazione di I/O

                    lockAcquire(&mtx_set, errMessage);
                    //tolgo "fd" dal "working set" => lo reinserisce working-thread
                    FD_CLR(fd, &set);

                    //aggiorno num # di connessioni in fd_num, se "fd" era il massimo
                    if(fd == fd_num)
                        fd_num = updatemax(&set, &fd_num);

                    lockRelease(&mtx_set, errMessage);

                   // printf("LISTENER ACCEPTED NEW REQUEST BY CLIENT: %d\n", fd);

                    //inserisco "fd" e "set" in testa alla coda di lavoro
                    pushTask(pool->workQueue, fd, &set, &fd_num);
                    }
                }
            }
        /**
        * provo a vedere se ci sono "fd ibernati" in "coda di ibernazione" e ne prelevo uno in caso affermativo
        * da reinserire nella coda di lavoro e permettere al worker-thread che lo estrarra' di verificare
        * se nel tempo della sua "ibernazione"/attesa indeffinita gli e' arrivato un messaggio
        */
        int hibernateFD = popHibernationQueue(pool->hibQueue);

        //se c'e' almeno un "fd ibernato" da inseire nella coda di lavoro, avra' "fd" diverso da -1
        //(-1 si ha quando la coda di "ibernazione e' vuota")
        if(hibernateFD != -1){
            //inserisco "fd" e "set" in testa alla coda di lavoro
            pushTask(pool->workQueue, hibernateFD, &set, &fd_num);
        }
    }

    //server e' stato interrotto da segnale SIGINT, SIGQUIT o SIGTERM

    printf("\nLISTENER IS PREPARING TO SHUTDOWN (graceful shutdown)...\n\n");
   

    /***************************************DEALLOCAZIONI**************************************************************/

    //inerisco tutti gli "fd ibernati" nella "coda di lavoro" prima di chiudere il ThreadPool (e non potervi piu' inserire nulla)
    int hibernateFD = 0;
    while(hibernateFD != -1){
         hibernateFD = popHibernationQueue(pool->hibQueue);

        if(hibernateFD != -1){
            //inserisco "fd" e "set" in testa alla coda di lavoro
            pushTask(pool->workQueue, hibernateFD, &set, &fd_num);
        }
    }

    //termino e deallocco ThreadPool, con "GRACEFUL SHUTDOWN"
    threadpool_destroy(pool);
    printf("\nTHREADPOOL DESTROYED\n");

    //dealloco HashTable e coda degli utenti connessi che essa contiene
    deleteHashTable(ht);
    printf("HASHTABLE DESTROYED\n");

    //Chiudo il socket
    unlink(UNIXPATH);
    printf("SERVER SOCKET CLOSED\n");

    //faccio terminare listener-thread
    return NULL;
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
