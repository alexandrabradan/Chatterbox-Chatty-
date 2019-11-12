#ifndef LISTERNER_H
#define LISTERNER_H

//#include "connections.h"
#include "ThreadPool.h"
//#include "GlobalVariables.h"
//#include "stats.h"
//#include "Parser.h"

/**
 * @file Listener.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni utili per l'avvio e la terminazione del thread-listener.
 */


/**
 * @brief Signal Handler che personalizza la gestione dei segnali di terminaione del thread-listener (SIGINT, SIGQUIT, SIGTERM),\n
 * andando a settare la variabile sigTerm con il segnale rilevato, la quale fa uscire dal ciclo di attesa del listener-thread e\n
 * predispone la chiusura e la deallocazione(GRACEFUL SHUTDOWN) del TheadPool, della  HashTable e della coda degli utenti connessi ivi contenuta.\n
 *
 * @param signal: segnale catturato
 */
void termination_handler(int signal);

/**
 * @brief Signal Handler che personalizza la gestione del segnale di stampa su file delle statistiche del thread-listener\n
 * (SIGURS1),  andando a settare la variabile sigStats con il segnale rilevato.\n
 * Siccome ogni worker-thread mantiene le proprie statistiche locali, prima della stampa su file il listener-thread deve sommare\n
 * le statistiche locali di ogni worker-thread per poter ottenere le sttaistiche globali della Chat.\n
 *\n
 * N.B. IL LISTENER-THREAD E' L'UNICO A MANTENERE IL CONTEGGIO DEGLI UTENTI CONNESSI, NELLA VARIABILE GLOBALE, numConnectedUsers,\n
 * (numConnectedUsers++, dopo "accept").\n
 * GLI WORKER-THREADS, INVECE, SI OCCUPANO DI SOMMANO GLI UTENTI DISCONNESSI NELLE LORO STATISTICHE LOCALI AL POSTO DEGLI UTENTI CONNESSI \n
 * (pool->threadsStats[index].nonline++) E DI CONSEGUENZA PER SCOPRIRE QUANTI UTENTI SONO CONNESSI IN UN DATO MOMENTO, BISOGNA SOTTRARE \n
 * ALLA VARIABILE GLOBALE numConnectedUsers LA SOMMA DEFLI UTENTI DISCONESSI DAGLI WORKERTHREADS.
 *
 * @param signal: segnale catturato
 */
void statistics_handler(int signal);

/**
 * @brif Funzione che va a registrare i gestori per la terminazione del listener-thread per i segnali di terminazione SIGINT / SIGQUIT / SIGTERM\n
 * e per la stampa delle statistiche per il segnale SIGURS1.
 */
void signals_registration();


/**
 * @brief Funzione che stampa le statistiche globali sul file specificato nel file di configurazione,  all'arrivo dell'opportuno segnale SIGUSR1.
 */
void handle_stats();

/**
 * @bried Routine che il thread-listener esegue, per gestire la comunicazioni tra i clints ed il Server Chatty, tramite l'utilizzo del costrutto "Select".
 *
 * @param stats: struttura dove memorizzare le statistiche del Server <<Chatty>>
 */
void* listener(void* stats);

#endif //LISTERNER_H
