#ifndef GLOBALVARIABLES_H
#define GLOBALVARIABLES_H

#include "pthread.h"

/**
 * @file GlobalVariables.h
 * @author Alexandra Bradan
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le variabile globali usate dal listener-thead e dagli worker-threads.
 */

/**
 * @var mtx_set
 * @bried Variabile di mutua esclusione, globale, che permette di modificare il "working set" dei "file descriptor" della
 * Select, sia al listener-thread che agli worker-threads.
 */
pthread_mutex_t mtx_set;

/**
 * @var numConnectedUsers
 * @brief Varibile, globale, che tiene traccia del numero di utenti connessi. NON e' protteta da mutua esclusione, perche'
 * l'unico thread a modificarla e' il listener-thread, mentre gli worker-threads la leggono.
 */
int numConnectedUsers;

/**
 * @var mtx_file
 * @bried Variabile di mutua esclusione, globale, che permettere di accedere in mutua esclusione ad un file.
 */
pthread_mutex_t mtx_file;

#endif //GLOBALVARIABLES_H
