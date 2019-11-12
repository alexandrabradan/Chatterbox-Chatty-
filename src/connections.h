#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3

#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <stdio.h>
#include <sys/param.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/select.h>

#include "message.h"
#include "Parser.h"

/**
 * @file  connection.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 *
 * @brief Interfaccia  delle funzioni che implementano il protocollo  tra i clients ed il server.
 */

/**
 * @brief Apre una connessione AF_UNIX verso il server.
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

/**
 * @brief Legge l'intero messaggio di risposta del server, discriminando se il client sta facendo "-R" (attesa indefinita).
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg);

// -------- server side -----
/**
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readHeader(long fd, message_hdr_t *hdr);
/**
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data);

/**
 * @brief Legge l'intero messaggio che un client invia per richiedere un servizio al server.
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readServerMsg(long fd, message_t* msg);

/* da completare da parte dello studente con altri metodi di interfaccia */


// ------- client side ------
/**
 * @brief Invia un messaggio di richiesta al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg);

/**
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg);


/* da completare da parte dello studente con eventuali altri metodi di interfaccia */

/**
 * @brief Invia un header di messaggio ad un descrittore
 *
 * @param fd    Il descrittore a cui inviare l'header
 * @param hdr   L'header da inviare
 *
 * @return <=0 se c'e' stato un errore
 *            >0 se l'operazione ha avuto successo
 */
int sendHeader(long fd, message_hdr_t *hdr);

/**
 * @brief Funzione che legge @param n bytes dal socket file descriptor @param fd e ne memorizza il contenuto in @param buf.
 * Si comporta come una read, ma riavvia la read in caso di interruzioni della lettura.
 * Se ci sono meno bytes di quelli richiesti, avviene una lettura parziale.
 *
 * @param fd: socket file descriptor da cui leggere
 * @param buf: stringa in cui memorizzare contenuto letto
 * @param n: # di bytes da leggere dal @param fd
 * @return: i bytes letti da @param fd, oppure 0 in caso di EOF oppure < 0  in caso di errore
 */
ssize_t readn(int fd, void *buf, size_t n);

/**
 * @brief Funzione che scrive @param n bytes sul socket file descriptor @param fd, prelevando il contenuto da scrivere
 * da @param buf.  Si comporta come una write, ma riavvia la write in caso di interruzioni della scrittura.
 * Se non basta lo spazio nel buffer può avvenire una scrittura parziale.
 *
 * @param fd: socket file descriptor su cui scrivere
 * @param buf: stringa dal quale prelevare contenuto da scrivere
 * @param n: # di bytes da scrivere sul  @param fd
 * @return: i bytes scritti sul @param fd [ >0], oppure un numero <= 0 in caso di errore
 */
int writen(int fd, void *buf, size_t n);

#endif /* CONNECTIONS_H_ */

