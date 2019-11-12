/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>



/* inserire gli altri include che servono */
#include "stats.h"
#include "config.h"
#include "Parser.h"
#include "CheckSysCall.h"

#include "Ascoltatore.h"


/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 *
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };


static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

int main(int argc, char *argv[]) {

    if(argc != 3){
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("MAIN INIZIATO...\n");

    /***********************************PARSING FILE DI CONFIGURAZIONE************************************************/

    //extern int optind; //INDICE PROSSIMO ELEMENTO argv[] DA "PARSARE"
    //extern char* optarg; //ARRAY DI ARGOMENTI DEI FLAG DI ARGV[]
    //extern int optopt; //FLAG NON RICNONOSCIUTO

    int opt;
    char* argv1 = NULL;

    while((opt = getopt(argc, argv, "f:")) != -1){
        switch(opt){
            case 'f':{
                argv1 = optarg;
                break;
            }
            case '?':
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                exit(EXIT_FAILURE);
            default:
                abort ();
        }
    }

    char path[MAX_CONF_LINE_LENGTH];
    //inizializzo array che conterra' il path assoluto del file di configurazione a vuoto
    path[0] = '\0';
    //ricavo path assoluto del file di configurazione  della cartella corrente
    getcwd(path, MAX_CONF_LINE_LENGTH);

    size_t path_len = strlen(path);

    path[path_len] = '/';

    path[path_len + 1] = '\0';

    size_t arg_len = strlen(argv1);

    strncat(path, argv1, arg_len);


    if (parse_config(path) == -1){
        free_config();
        exit(EXIT_FAILURE);
    }
    //stampo variabili di configurazione
    //print_config();

    /*********************************CREAZIONE THREAD LISTENERE******************************************************/
    //listener-thread ID
    pthread_t ltID;

    //avvio listener-thread
    ptCreate(&ltID, listener, (void*)&chattyStats, "chatty.c");

    //mi fermo ad aspettare terminazione del listener-thread
    ptJoin(ltID, NULL);

    /***********************************TERMINAZIONE******************************************************************/

    //libero risorse allocate con il parsing
    free_config();
    printf("\n");
    printf("MAIN TERMINATO...\n");

    return 0;
}