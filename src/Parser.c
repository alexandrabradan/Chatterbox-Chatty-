#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include <sys/stat.h>

#include "Parser.h"

/**
 * @file Parser.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Implementazione delle funzioni per il parsing del file di configurazione, contenute nell'interfaccia\n
 * Parser.h
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

int file_exist (char *filename){
    struct stat   buffer;
    if(stat (filename, &buffer) == -1)
        return 0;
    else return 1;
}
char* clean(char* str){
    int i=0; //index
    int j=0; //writeback index
    char tmp;

    while((tmp=str[i]) != '\0'){
        if(!iscntrl(tmp) && !isspace(tmp) && tmp!='='){
            str[j]=tmp;
            j++;
        }
        i++;
    }

    str[j]='\0';

    return str;
}

int parse_config(char* filename){
    FILE* f;
    char line[MAX_CONF_LINE_LENGTH];

    //controllo che file esista
    if(file_exist(filename) == 0){
        fprintf(stderr, "Errore, il file %s non esiste\n", filename);
        return -1;
    }

    //apro il file di configurazione
    if((f=fopen(filename,"r")) == NULL){
        fprintf(stderr, "Impossibile aprire il file di configurazione %s\n", filename);
        goto err;
    }

    int contatore_variabili_di_config = 0;
    //leggo riga per riga del file di configurazione
    while((fgets(line, MAX_CONF_LINE_LENGTH, f)) != NULL){

        //ignoro linee vuote e linee-commento
        if(line[0]!='\n' && line[0]!='#'){
            const char delim[2] = "="; //delimitatore tra chiave e valore
            char* key = strtok(line, delim);
            char* value = strtok(NULL, delim);

            //ripulisco la chiave ed il valore dai caratteri "in eccesso"
            if(key != NULL && value != NULL){
                key = clean(key);
                value = clean(value);
                // printf("clean key : #%s#\n", key);
                // printf("clean value : #%s#\n", value);
            }
                // interrompo ricerca delle variabili di configurazione
            else if(contatore_variabili_di_config == 8)
                break;

            size_t val_size = strlen(value) + 1; //"\0" aggiunto con "clean" ??????

            if (strcmp(key, "UnixPath") == 0) {
                if((UNIXPATH = (char *) calloc(val_size, sizeof(char))) == NULL){
                    perror("calloc");
                    fprintf(stderr, "Errore calloc UNIXPATH in parse_config\n");
                    free(UNIXPATH);
                    goto err;
                }
                strncpy(UNIXPATH, value, val_size);
                contatore_variabili_di_config++;
            }
            else if ( strcmp(key, "DirName") == 0) {
                if(( DIRNAME = (char *)calloc(val_size, sizeof(char))) == NULL){
                    perror("calloc");
                    fprintf(stderr, "Errore calloc DIRNAME in parse_config\n");
                    free(UNIXPATH);
                    free(DIRNAME);
                    goto err;
                }
                strncpy(DIRNAME, value, val_size);
                contatore_variabili_di_config++;
            }
            else if ( strcmp(key, "StatFileName") == 0) {
                if((  STATFILENAME = (char *)calloc(val_size, sizeof(char))) == NULL){
                    perror("calloc");
                    fprintf(stderr, "Errore calloc STATFILENAME in parse_config\n");
                    free(UNIXPATH);
                    free(DIRNAME);
                    free(STATFILENAME);
                    goto err;
                }
                strncpy(STATFILENAME, value, val_size);
                contatore_variabili_di_config++;
            }
            else if (strcmp(key, "MaxConnections") == 0) {
                MAXCONNECTIONS = (int)strtol(value, NULL, 10);
                contatore_variabili_di_config++;
            }
            else if (strcmp(key, "ThreadsInPool") == 0) {
                THREADSINPOOL = (int)strtol(value, NULL, 10);
                contatore_variabili_di_config++;
            }
            else if (strcmp(key, "MaxMsgSize") == 0) {
                MAXMSGSIZE = (int)strtol(value, NULL, 10);
                contatore_variabili_di_config++;
            }
            else if (strcmp(key, "MaxFileSize") == 0) {
                MAXFILESIZE =(int)strtol(value, NULL, 10);
                contatore_variabili_di_config++;
            }
            else if (strcmp(key, "MaxHistMsgs") == 0) {
                MAXHISTMSGS = (int)strtol(value, NULL, 10);
                contatore_variabili_di_config++;
            }
        }
    }

    fclose(f);
    return 0;

    err:
    fclose(f);
    return -1;
}

void print_config(){
    printf("\n");
    printf("Stampa variabili di configurazione:\n");
    printf("UNIXPATH = %s\n", UNIXPATH);
    printf("DIRNAME = %s\n", DIRNAME);
    printf("STATFILENAME = %s\n", STATFILENAME);
    printf("MAXCONNECTIONS = %d\n", MAXCONNECTIONS);
    printf("THREADSINPOOL = %d\n", THREADSINPOOL);
    printf("MAXMSGSIZE = %d\n", MAXMSGSIZE);
    printf("MAXFILESIZE = %d\n", MAXFILESIZE);
    printf("MAXHISTMSGS = %d\n", MAXHISTMSGS);
    printf("\n");
}

void free_config(){
    free(UNIXPATH);
    free(DIRNAME);
    free(STATFILENAME);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

