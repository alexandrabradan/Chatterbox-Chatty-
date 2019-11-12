#include <stdio.h>
#include <stdlib.h>

#include "HashTable.h"

/**
 * @file HashTable.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief  Implementazione delle funzioni dell'iterfaccia HashTable.h
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

int ht_hash(const char* s,  int a,  int m){
    long hash = 0;
    const int len_s = (int)strlen(s);

    for (int i = 0; i < len_s; i++) {
        hash = hash + (long) pow(a, (len_s - (i+1))) * s[i]; //s[i] = rappresentazione numerica del carattere s[i]
        hash = hash % m;
    }

    return (int)hash;
}

int getHash(ht_hash_table_t* ht, char* username){
    const int hash_a = ht_hash(username, HT_PRIME_1, ((int)ht->size));
    const int hash_b = ht_hash(username, HT_PRIME_2, ((int)ht->size));

    return (hash_a + (1 * (hash_b + 1))) % ((int)ht->size);
}

ht_hash_table_t* initHashTable(size_t size, size_t sizeLockItems){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "initHashTable";

    ht_hash_table_t* ht = NULL;
    if((ht = (ht_hash_table_t*) calloc(1, sizeof(ht_hash_table_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore nell'allocare la struct che rappresenta HashTable in initHashTable\n");
        free(ht);
        return NULL;
    }

    //setto variabili della struct che raccoglie la HashTable a NULL
    ht->size = size;
    ht->items = NULL;
    ht->sizeLockItems = sizeLockItems;
    ht->lockItems = NULL;
    ht->onlineQueue = NULL;

    //alloco array di locks per accedere in mutua esclusione alla HashTable, di dimensione MAXCONNECTIONS
    if((ht->lockItems = calloc(ht->sizeLockItems, sizeof(pthread_mutex_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore allocazione array di locks in initHashTable\n");
        free(ht->lockItems);
        free(ht);
        return NULL;
    }

    //inizializzo locks array di locks
    for(int i = 0; i < sizeLockItems; i++){
        lockInit(&ht->lockItems[i], errMessage);
    }

    //alloco buckets della HashTable
    if((ht->items = calloc(ht->size, sizeof(ht_item))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore allocazione array di buckets della HashTable in initHashTable\n");
        free(ht->lockItems);
        free(ht->items);
        free(ht);
        return NULL;
    }

    //setto buckets della HashTable a nulli
    for(int i = 0; i < ht->size; i++){
        ht->items[i].index = (size_t)i; //setto indice del bucket
        ht->items[i].usersQueue = NULL; //bucket[i] ha la coda di trabocco nulla
        ht->items[i].numUsers = 0; //bucket[i] ha 0 utenti che collidono
        ht->items[i].lastUser = NULL; //bucket[i] ha riferimento nullo all'ultimo utente della coda di trabocco
    }

    //alloco struct che contiene coda degli utenti connessi ed informazioni relative ad essa
    //dimensione della coda degli utenti connessi e' data da MAXCONNECTIONS*BASE_ONLINEQUEUE
    int sizeOnlineQueue = ((int)sizeLockItems)* BASE_ONLINEQUEUE;
    ht->onlineQueue = initOnlineQueue((size_t)sizeOnlineQueue);

    //controllo che allocazione struct coda utenti connessi sia andata a buon fine
    if(ht->onlineQueue == NULL){
        free(ht->lockItems);
        free(ht->items);
        free(ht->onlineQueue);
        free(ht);
        return NULL;
    }

    return ht;
}

int insertUserInHashTable(ht_hash_table_t *ht, char *username){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "insertUserInHashTable";

    if(ht == NULL || ht->lockItems == NULL || ht->items == NULL){
        fprintf(stderr, "Errore inserimento username -%s- in insertUserInHashTable, ht o strutture dati vuote\n", username);
        return FUNCTION_FAILURE;
    }

    if(username == NULL){
        fprintf(stderr, "Errore inserimento in insertUserInHashTable, username e' vuoto\n");
        return FUNCTION_FAILURE;
    }

    //calcolo indice del bucket in cui inserire l'utente, tramite la funzione di hashing
    int index = getHash(ht, username);

    if((index < 0) || (index > (ht->size - 1))){
        fprintf(stderr, "Errore calcolo indice bucket dove inserire username %s: indice -%d- fuori range\n", username, index);
        return FUNCTION_FAILURE;
    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket appena trovato
    int lock_index = index % ((int)ht->sizeLockItems); //mod dimensione array di locks

    if((lock_index < 0) || (lock_index > ht->sizeLockItems)){
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in insertUserInHashTable\n", lock_index, index);
        return FUNCTION_FAILURE;
    }

    //acquisisco mutua esclusione sul bucket tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ho acquisito la mutua esclusione se sono arrivata qui

    //inserisco utente in fondo alla lista di trabocco del bucket indicizzato dall'indice appena trovato
    int r = pushUsersQueue(&ht->items[index], username);
    if(r == FUNCTION_FAILURE){
       // fprintf(stderr, "Errore inserimento username -%s- in insertUserInHashTable, dovuto a pushUsersQueue \n", username);
        //rilascio la mutua esclusione
        lockRelease(&ht->lockItems[lock_index], errMessage);
        return FUNCTION_FAILURE;
    }
    else if(r == OP_NICK_ALREADY){
       // fprintf(stderr, "Errore inserimento username -%s- in insertUserInHashTable, nickname gia' in uso \n", username);
        //rilascio la mutua esclusione
        lockRelease(&ht->lockItems[lock_index], errMessage);
        return OP_NICK_ALREADY;
    }

    //rilascio la mutua esclusione
    lockRelease(&ht->lockItems[lock_index], errMessage);
    return FUNCTION_SUCCESS;
}

int insertMessageInHashTable(ht_hash_table_t *ht, char* sender, char* receiver, message_t* request, op_t op){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "insertMessageInHashTable";

    if(ht == NULL || ht->lockItems == NULL || ht->items == NULL){
        fprintf(stderr, "Errore inserimento username -%s- in insertMessageInHashTable, ht o strutture dati vuote\n", sender);
        return FUNCTION_FAILURE;
    }

    if(sender == NULL){
        fprintf(stderr, "Errore inserimento in insertMessageInHashTable, username e' vuoto\n");
        return FUNCTION_FAILURE;
    }

    if(receiver == NULL){
        fprintf(stderr, "Errore inserimento in insertMessageInHashTable, receiver e' vuoto\n");
        return FUNCTION_FAILURE;
    }

    if(request == NULL){
        fprintf(stderr, "Errore inserimento in insertMessageInHashTable, richiesta e' vuota\n");
        return FUNCTION_FAILURE;
    }

    //calcolo indice del bucket in cui si trova destinatario e di conseguenza la historyQueue in cui inserire il msg
    int index = getHash(ht, receiver);

    if((index < 0) || (index > (ht->size - 1))){
        fprintf(stderr, "Errore calcolo indice bucket dove inserire username %s: indice -%d- fuori range in insertMessageInHashTable\n", receiver, index);
        return FUNCTION_FAILURE;
    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket appena trovato
    int lock_index = index % ((int)ht->sizeLockItems); //mod dimensione array di locks

    if((lock_index < 0) || (lock_index > ht->sizeLockItems)){
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in insertMessageInHashTable\n", lock_index, index);

        return FUNCTION_FAILURE;
    }

    //acquisisco mutua esclusione sul bucket tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ho acquisito la mutua esclusione se sono arrivata qui

    //ricavo la coda dei messaggi del destinatario
    history_queue_t *msgsQueue = getHistoryQueue(&ht->items[index], receiver);

    //se l'utente non era presente nella lista di trabocco, la coda dei messaggi restituita e' vuota
    if (msgsQueue == NULL) {
        //rilascio mutua esclusione sul bucket
        lockRelease(&ht->lockItems[lock_index], errMessage);
        return OP_NICK_UNKNOWN;

    } else {

        //provo ad inviare, inserendo nella sua historyQueue, il msg al destinatario
        pushMessage(msgsQueue, op, sender, receiver, request->data.buf, request->data.hdr.len);
    }

    //rilascio la mutua esclusione
    lockRelease(&ht->lockItems[lock_index], errMessage);
    return FUNCTION_SUCCESS;
}

int removeUserInHashTable(ht_hash_table_t *ht, char *username){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "removeUserInHashTable";

    if(ht == NULL || ht->lockItems == NULL || ht->items == NULL){
        fprintf(stderr, "Errore cancellazione username -%s- in removeUserInHashTable, ht o strutture dati vuote\n", username);
        return FUNCTION_FAILURE;
    }

    if(username == NULL){
        fprintf(stderr, "Errore cencellazione in removeUserInHashTable, username e' vuoto\n");
        return FUNCTION_FAILURE;
    }

    //calcolo indice del bucket da cui eliminare l'utente, tramite la funzione di hashing
    int index = getHash(ht, username);

    if((index < 0) || (index > (ht->size - 1))){
        fprintf(stderr, "Errore calcolo indice bucket dove eliminare username %s-: indice -%d- fuori range\n", username, index);
        return FUNCTION_FAILURE;
    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket appena trovato
    int lock_index = index % ((int)ht->sizeLockItems);; //mod dimensione array di locks

    if((lock_index < 0) || (lock_index > ht->sizeLockItems)){
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in removeUserInHashTable\n", lock_index, index);
        return FUNCTION_FAILURE;
    }

    //acquisisco mutua esclusione sul bucke tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ho acquisito la mutua esclusione se sono arrivata qui

    //cerco utente nella lista di trabocco del bucket e se vi e' presente lo cancello
    int r = popUsersQueue(&ht->items[index], username);
    if(r == FUNCTION_FAILURE){
       // fprintf(stderr, "Errore cancellazione username -%s- in removeUserInHashTable, dovuto a popUsersQueue \n", username);
        //rilascio la mutua esclusione
        lockRelease(&ht->lockItems[lock_index], errMessage);

        return FUNCTION_FAILURE;
    } else if(r == OP_NICK_UNKNOWN){
        //fprintf(stderr, "Errore cancellazione username -%s- in removeUserInHashTable, nickname non presente \n", username);
        //rilascio la mutua esclusione
        lockRelease(&ht->lockItems[lock_index], errMessage);

        return OP_NICK_UNKNOWN;
    }

    //rilascio la mutua esclusione
    lockRelease(&ht->lockItems[lock_index], errMessage);

    return FUNCTION_SUCCESS;
}

int searchUserInHashTable(ht_hash_table_t *ht, int fd, char *username){
    char errMessage[MAX_ERRMESSAGE_LENGTH] = "searchUserInHashTable";

    if(ht == NULL || ht->lockItems == NULL || ht->items == NULL){
        fprintf(stderr, "Errore ricerca username -%s- in searchUserInHashTable, ht o strutture dati vuote\n", username);
        return FUNCTION_FAILURE;
    }

    if(username == NULL){
        fprintf(stderr, "Errore ricerca in searchUserInHashTable, username e' vuoto\n");
        return FUNCTION_FAILURE;
    }

    if(fd < 0 || fd >= ht->onlineQueue->qosize){
        fprintf(stderr, "Errore file descriptor -%d- non valido in searchUserInHashTable\n", fd);
        return FUNCTION_FAILURE;
    }

    //calcolo indice del bucket in cui ricercare l'utente, tramite la funzione di hashing
    int index = getHash(ht, username);

    if((index < 0) || (index > (ht->size - 1))){
        fprintf(stderr, "Errore calcolo indice bucket dove ricercare username %s: indice -%d- fuori range\n", username, index);
        return FUNCTION_FAILURE;
    }

    //ricavo l'indice nell'array di locks per poter acquisire mutua esclusione sul bucket appena trovato
    int lock_index = index % ((int)ht->sizeLockItems); //mod dimensione array di locks

    if((lock_index < 0) || (lock_index > ht->sizeLockItems)){
        fprintf(stderr, "Errore calcolo indice array di lock -%d- per acquisire mutua esclusione sul bucket[%d] in searchUserInHashTable\n", lock_index, index);
        return FUNCTION_FAILURE;
    }

    //acquisisco mutua esclusione sul bucket tramite la lock indicizzata da "lock_index" nell'array di locks
    //NEL CASO UN ALTRO THREAD ABBIA GIA' ASCUISITO LA LOCK SU "lock_index" MI SOSPENDO FINO A QUANDO NON LA RILASCIA
    //AD OGNI LISTA DI TRABOCCO DI OGNI BUCKET PUO' ACCEDERE 1 SOLO THREAD ALLA VOLTA
    lockAcquire(&ht->lockItems[lock_index], errMessage);

    //ho acquisito la mutua esclusione se sono arrivata qui

    //scorro la lista di trabocco del bucket[index] alla ricerca dell'utente e nel caso lo trovassi, inizializzo
    //la struct ht->onlineQueue->connectionQueue[fd]
    getUsersQueue(&ht->items[index], username, &ht->onlineQueue->connectionQueue[fd]);

    //rilascio la mutua esclusione
    lockRelease(&ht->lockItems[lock_index], errMessage);

    //se la struct ht->onlineQueue->connectionQueue[fd] NON e' stata inizializzata vuol dire che utente NON e'
    //presente nella lista di trabocco
    if(ht->onlineQueue->connectionQueue[fd].empty_struct == 1 && ht->onlineQueue->connectionQueue[fd].bucket_index == -1\
    && ht->onlineQueue->connectionQueue[fd].p_user_in_overflow_list == NULL ){
       // fprintf(stderr, "Errore in searchUserInHashTable: username -%s- NON presente nella HashTable\n", username);
        return OP_NICK_UNKNOWN;
    }
    else return FUNCTION_SUCCESS;
}

int pushOnlineUser(ht_hash_table_t* ht, int fd, char* username){

    if(fd < 0 || fd >= ht->onlineQueue->qosize){
        fprintf(stderr, "Errore file descriptor -%d- non valido in pushOnlineUser\n", fd);
        return FUNCTION_FAILURE;
    }

    //se c'e' gia' un utente connesso, non posso connetterne un altro associato allo stesso "file descriptor"
    //non dovrebbe succedere questa situazione dato che la "Select" assegna "file descriptor" univoci, ma controllo lo stesso
    if(ht->onlineQueue->connectionQueue[fd].empty_struct == 0){
        //fprintf(stderr, "Errore file descriptor -%d- gia' in uso in pushOnlineUser da parte dell'utente %s\n", fd, ht->onlineQueue->connectionQueue[fd].username);
        return FUNCTION_FAILURE;
    }

    //se sono qui vuol dire che non c'e' un altro utente connesso, che ha associato lo stesso "file descriptor"

    //ricerco utente nella HashTable, per verificare che sia registrato e in caso affermativo memorizzo le
    //informazioni utili per la sua localizzazione nella HashTable, nel nodo relativo nella coda dei connessi
    //tramite la chiamata di "searchUserInHashTable"

    int r = searchUserInHashTable(ht, fd, username);
    if(r == FUNCTION_FAILURE) {
        //fprintf(stderr, "Errore connessionde di -%s- in  pushOnlineUser, dovuto a searchUserInHashTable\n", username);
        return FUNCTION_FAILURE;
    }
    else if(r == OP_NICK_UNKNOWN){
       // fprintf(stderr, "Errore in pushOnlineUser: username -%s- NON presente nella HashTable\n", username);
        return OP_NICK_UNKNOWN;
    }
    return FUNCTION_SUCCESS;
}

void deleteHashTable(ht_hash_table_t* ht){

    //non ho mutua esclusione, perche' deallocazione della HashTable viene fatta solo all'atto di chiusura del server Chatty
    //e contiene variabili che non vengono modificate nel corso dell'esecuzione

    //dealloco array di locks che consentono mutua esclusione ai buckets della  HashTable
    if(ht->lockItems != NULL){
        free(ht->lockItems);
    }


    //dealloco buckets della HashTable, andando per ogni bucket a deallocare:
    //1. username degli utenti della lista di trabocco, allocati dinamicamente
    //2. historyQueues di ogni utente
    //3. la lista di trabocco stessa
    for(int i = 0; i < ht->size; i++){
        deleteUsersQueue(&ht->items[i]);
    }

    //dealloco array associativo di buckets che rappresenta la HashTable
    free(ht->items);

    //dealloco coda degli utenti connessi
    deleteOnlineQueue(ht->onlineQueue);

    //dealloco struct che raccoglie HashTable ed informazioni legate ad essa
    free(ht);
}

void printHashTable(ht_hash_table_t* ht){
    printf("\n");
    printf("STAMPA HASH-TABLE:\n");
    if(ht == NULL){
        printf("HashTable vuota\n");
    }
    else{
        printf("STAMPA ARRAY DI LOCKS");
        for(int i = 0; i < ht->sizeLockItems; i++){
            printf("ThreadLOck[%d]=%d\n", i, ht->lockItems[i].__data.__lock);
        }
        for(int i = 0; i < ht->size; i++){
            printUsersQueue(&ht->items[i]);
        }

        printOnlineQueue(ht->onlineQueue);
    }
    printf("\n");
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
