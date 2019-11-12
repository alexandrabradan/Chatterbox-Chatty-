#include "UsersQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UsersQueue.h"

/**
 * @file UsersQueue.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzioni per gestire la lista di trabocco di un bucket, contenute nell'iterfaccia UsersQueue.h
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

int searchDuplicateUsersQueue(ht_item* htitem, char* username){
    int result = 0;
    userNode_t* tmp = htitem->usersQueue;

    while(tmp != NULL){
        if(strncmp(tmp->username, username, strlen(username)) == 0){
            result = 1;
            break;
        }
        else{
            tmp = tmp->nextUserNode;
        }
    }

    return result;
}

int pushUsersQueue(ht_item* htitem, char* username){

    if(htitem == NULL){
        fprintf(stderr, "Errore pushUsersQueue: htitem nullo\n");
        return FUNCTION_FAILURE;
    }

    if(username == NULL){
        fprintf(stderr, "Errore pushUsersQueue: username nullo\n");
        return FUNCTION_FAILURE;
    }

    if(searchDuplicateUsersQueue(htitem, username) == 1){
        fprintf(stderr, "Utente %s gia' registrato\n", username);
        return OP_NICK_ALREADY;
    }

    userNode_t* curr = NULL;

    //alloco nuovo nodo che ospitera' il nickname e la historyQueue di un nuovo utente
    if((curr = (userNode_t*) calloc(1, sizeof(userNode_t))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore allocazione nuovo nodo-utente in pushUsersQueue\n");
        free(curr);
        return FUNCTION_FAILURE;
    }

    //alloco spazio per memorizzare il nickname del nuovo utente
    if((curr->username = (char*) calloc((MAX_NAME_LENGTH + 1), sizeof(char))) == NULL){
        perror("calloc");
        fprintf(stderr, "Errore allocazione nuovo username in pushUsersQueue\n");
        free(curr->username);
        free(curr);
        return FUNCTION_FAILURE;
    }

    //copio il nickname del nuovo utente
    strncpy(curr->username, username, strlen(username)); //devo aggiungere \0 ???

    //creo historyQueue
    curr->historyQueue = initHistoryQueue((size_t)MAXHISTMSGS);

    //se ci sono stati errori con l'inizializzazione della historyQueue dealloco tutto ed esco con errore
    if(curr->historyQueue == NULL){
        free(curr->username);
        free(curr);
        return FUNCTION_FAILURE;
    }

    //se la lista di trabocco e' vuota, l'utente che registro e' il primo della lista di trabocco ed anche l'ultimo
    if(htitem->usersQueue == NULL){
        curr->previuosUserNode = NULL;
        htitem->usersQueue = curr;
        htitem->lastUser = curr;
    }
    else{
        htitem->lastUser->nextUserNode = curr;
        curr->previuosUserNode = htitem->lastUser;
        htitem->lastUser = curr;
    }

    curr->nextUserNode = NULL;
    htitem->numUsers = htitem->numUsers + 1;

    return FUNCTION_SUCCESS;
}

int popUsersQueue(ht_item* htitem, char* username){

    if(htitem == NULL){
        fprintf(stderr, "Errore popUsersQueue: htitem nullo\n");
        return FUNCTION_FAILURE;
    }

    if(username == NULL){
        fprintf(stderr, "Errore popUsersQueue: username nullo\n");
        return FUNCTION_FAILURE;
    }

    //non posso eliminare un elemento dalla coda vuota
    if(htitem->usersQueue == NULL){
        fprintf(stderr, "Errore popUsersQueue:  htitem->usersQueue nullo\n");
        return FUNCTION_FAILURE;
    }

    //se la lista di trabocco e' vuota sicuramente non ci sono utenti da eliminare
    if(htitem->usersQueue == NULL){
        return OP_NICK_UNKNOWN;
    }

    //se la lista di trabocco ha un solo nodo e se tale nodo e' l'utente che voglio eliminare, lo elimino
    //altrimenti ritorno OP_NICK_UNKNOWN
    if(htitem->usersQueue->nextUserNode == NULL){
        if((strcmp(htitem->usersQueue->username, username)) == 0){
            userNode_t* tmp = htitem->usersQueue;
            htitem->usersQueue = htitem->usersQueue->nextUserNode;
            //dealloco username allocato dinamicamente
            free(tmp->username);
            //dealloco historyQueue
            deleteHistoryQueue(tmp->historyQueue);
            //dealloco nodo che contiene l'utente nella lista di trabocco
            free(tmp);

            //decremento utenti che collidono sul bucket
            htitem->numUsers = htitem->numUsers - 1;
            return FUNCTION_SUCCESS;
        }
        return OP_NICK_UNKNOWN;
    }

    userNode_t* prev = htitem->usersQueue;
    userNode_t* curr = htitem->usersQueue->nextUserNode;

    if((strcmp(prev->username, username)) == 0){
        curr->previuosUserNode = NULL;
        prev->nextUserNode = NULL;
        //testa della lista di trabocco diventa curr
        htitem->usersQueue = curr;

        //dealloco username allocato dinamicamente
        free(prev->username);
        //dealloco historyQueue
        deleteHistoryQueue(prev->historyQueue);
        //dealloco nodo che contiene l'utente nella lista di trabocco
        free(prev);

        //decremento utenti che collidono sul bucket
        htitem->numUsers = htitem->numUsers - 1;
        return FUNCTION_SUCCESS;
    }

    while(curr != NULL && (strcmp(curr->username, username)) != 0){
        prev = curr;
        curr = curr->nextUserNode;
    }

    if(curr == NULL){
        fprintf(stderr, "Username %s non e' presente nella coda, impossibile eliminarlo\n", username);
        //se utente non e' presente nella lista di trabocco, non faccio nulla
        return OP_NICK_UNKNOWN;
    }

    if((strcmp(curr->username, username)) == 0){
        userNode_t* tmp = curr;
        curr = curr->nextUserNode;
        prev->nextUserNode = curr;
        if(curr == NULL){
            htitem->lastUser = prev;
        }
        else{
            curr->previuosUserNode = prev;
        }

        //dealloco username allocato dinamicamente
        free(tmp->username);
        //dealloco historyQueue dell'utente
        deleteHistoryQueue(tmp->historyQueue);
        //dealloco nodo che contiene l'utente nella lista di trabocco
        free(tmp);
    }

    //decremento utenti che collidono sul bucket
    htitem->numUsers = htitem->numUsers - 1;
    return FUNCTION_SUCCESS;
}

void getUsersQueue(ht_item* htitem, char* username, online_user_t* onlineUser){

    userNode_t* curr = htitem->usersQueue;
    while(curr != NULL){
        if((strncmp(curr->username, username, strlen(username))) == 0){
            onlineUser->empty_struct = 0;
            onlineUser->bucket_index = (int)htitem->index;
            strncpy(onlineUser->username, username, strlen(username));
            onlineUser->p_user_in_overflow_list = curr;
            break;
        }
        else{
            curr = curr->nextUserNode;
        }
    }
}

history_queue_t* getHistoryQueue(ht_item* htitem, char* username){

    userNode_t* curr = htitem->usersQueue;
    while(curr != NULL){
        if((strncmp(curr->username, username, strlen(username))) == 0){
            return curr->historyQueue;
        }
        else{
            curr = curr->nextUserNode;
        }
    }

    //nel caso l'utente non sia presente nella lista di trabocco restituisco una historyQueue nulla
    return NULL;
}

void deleteUsersQueue(ht_item* htitem) {

    while(htitem->usersQueue != NULL){
        userNode_t* curr = htitem->usersQueue;
        htitem->usersQueue = htitem->usersQueue->nextUserNode;
        if(htitem->usersQueue != NULL)
            htitem->usersQueue->previuosUserNode = NULL;
        curr->nextUserNode = NULL;
        //dealloco username allocato dinamicamente
        free(curr->username);
        //dealloco la historyQueue dell'utente
        deleteHistoryQueue(curr->historyQueue);
        //dealloco nodo che contiene l'utente nella lista di trabocco
        free(curr);

        //htime e' allocato con (htsize*sizeof(ht_item)) => lo dealloco nella "deleteHashTable"
    }
}

void printUsersQueue(ht_item* htitem){

    printf("STAMPA LISTA DI TRABOCCO BUCKET[%zd]:\n", htitem->index);
    if(htitem->usersQueue == NULL){
        printf("Lista di trabocco vuota\n");
    }
    else{
        int i = 0;
        userNode_t* tmp = htitem->usersQueue;
        while(tmp != NULL){
            printf("Utente[%d]~%p: %s: => ", i, (void*)&tmp,tmp->username);
            printHistoryQueue(tmp->historyQueue);
            i++;
            tmp = tmp->nextUserNode;
        }
    }

    printf("\n");
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */