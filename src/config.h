/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH                  32 //lunghezza nickname

/* aggiungere altre define qui */

#define FUNCTION_SUCCESS            0

#define FUNCTION_FAILURE         -1

#define MAX_ERRMESSAGE_LENGTH            256//lunghezza msg di errore

#define MAX_CONF_LINE_LENGTH            1024 //lunghezza massima linea di chatty.conf

#define BASE_HASHTABLE        32 //dimensione di base della HashTable a cui motiplico MAXCONNECTIONS

#define BASE_ONLINEQUEUE        32 //dimensione di base della onlineQueue a cui motiplico MAXCONNECTIONS

#define HT_PRIME_1 151 //numero primo > 128 (rappresentazione numerica stringhe ASCII)
#define HT_PRIME_2 163 //altro numero primo > 128 (rappresentazione numerica stringhe ASCII)

#define MAX_FD_SELECT 1024  //FD_SETSIZE= massimo file deescriptor assegnato dalla Select

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
