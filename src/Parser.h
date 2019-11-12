#ifndef PARSER_H
#define PARSER_H

#include "config.h"

/**
 * @file Parser.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccogie le funzioni per il parsing del file di configurazione e le variabili risultanti.
 */

/**
 * @globalvar UNIXPATH:  path utilizzato per la creazione del socket AF_UNIX
 */
char* UNIXPATH;
/**
 * @globalvar MAXCONNECTIONS:  numero massimo di connessioni concorrenti gestite dal server
 */
int MAXCONNECTIONS;
/**
 * @globalvar THREADSINPOOL:  numero di thread nel Threadpool
 */
int THREADSINPOOL;
/**
 * @globalvar MAXMSGSIZE:  dimensione massima di un messaggio
 */
int MAXMSGSIZE;
/**
 * @globalvar MAXFILESIZE:  dimensione massima di un file accettato dal server (kilobytes)
 */
int MAXFILESIZE;
/**
 * @globalvar MAXHISTMSGS:  numero massimo di messaggi nella History di un client
 */
int MAXHISTMSGS;
/**
 * @globalvar DIRNAME:  directory dove memorizzare i files da inviare agli utenti
 */
char* DIRNAME;
/**
 * @globalvar STATFILENAME:   file nel quale verranno scritte le statistiche
 */
char* STATFILENAME;

/**
 * @function file_exist
 * @brief Funzione che controlla se il file @param filename esiste o meno.
 *
 * @param filename: file di cui controllare esistenza
 * @return: 1 se il file @param filename eiste, 0 altrimenti
 */
int file_exist (char *filename);

/**
 * @function clean
 * @brief Funzione che ripulisce la stringa @param str dai caratteri di controllo, spazi e simbolo '='.
 *
 * @param str: stringa da ripulire
 * @return: la stringa ripulita in caso di successo, NULL altrimenti
 */
char* clean(char* str);

/**
 * @brief Funzione che effettua il parsing del file di configurazione.
 *
 * @param filename: file di configurazione da parsare
 * @return: ritorna 1 se il parsing e' andato a buon fine, -1 altrimenti
 */
int parse_config(char* filename);

/**
 * @brief Funzione che stampa le variabili globali parsate a partire dal file di configurazione.
 */
void print_config();

/**
 * @function free_config
 * @brief Funzione che dealloca le variabili di configurazione allocate dinamicamente
 */
void free_config();
#endif //PARSER_H
