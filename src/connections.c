#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "connections.h"


/**
 * @file connections.c
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief  Funzione che implementa le funzioni dell'iterfaccia  connections.h
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    ssize_t r;
    char *bufptr = (char*)buf;

    while(left>0) {
        if ((r=read(fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue; //richiamo read() nel caso sopraggiunga un SEGNALE
            return -1;
        }
        if (r == 0) return 0;   // gestione chiusura socket ( EOF )
        left    -= r;
        bufptr  += r;
    }
    return n;  /* return >= 0 */
}

int writen(int fd, void *buf, size_t n) {
    size_t left = n;
    ssize_t r;
    char *bufptr = (char*)buf;

    while(left>0) {
        if ((r=write(fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue; //richiamo write() se soppraggiunge un SEGNALE
            return -1;
        }
        if (r == 0) return 0; //errore
        left    -= r;
        bufptr  += r;
    }
    return 1;
}

/********************************************CLIENT SIDE**************************************************************/

int openConnection(char* path, unsigned int ntimes, unsigned int secs){

    //controllo argomenti
    if(path == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore path nullo, passato come argomento in openConnection\n");
        return -1;
    }

    if(strlen(path)>UNIX_PATH_MAX){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore lunghezza path passato come argomento in openConnection, path_len=%zd mentre deve essere <=%d", strlen(path), UNIX_PATH_MAX);
        return -1;
    }

    //Controllo che il numero di tentativi di retry non superi il numero massimo stabilito
    if(ntimes>MAX_RETRIES)
        ntimes=MAX_RETRIES;
    //Controllo che il tempo di intervallo tra due tentativi non superi il tempo massimo
    if(secs > MAX_SLEEPING)
        secs=MAX_SLEEPING;

    int csfd; //client socket file descriptor

    //inizializzo indirizzo AF_UNIX
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, path, strlen(path) + 1);

    //creo client socket file descriptor
    if((csfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        perror("client-socket");
        int error = errno;
        fprintf(stderr, "%s errno: %d\n", "client-socket", error);
        //exit(errno);
        exit(EXIT_FAILURE);
    }

    //provo a connettere client all'indirzzo del server per massimo "ntimes"
    int tentivo = 0;
    while(tentivo < ntimes){
        if((connect(csfd, (struct sockaddr*)&sa, sizeof(sa))) == -1){
            if(errno == ENOENT) { //server e' occupato (no such file or directory , such as no such server address)
                sleep(secs); //timeout

                tentivo++;
            }
            else return -1;
        }
        else return csfd;
    }

    //se dopo "ntimes" non mi sono ancora connesso, ritorno -1 (fallimento)
    return -1;
}

int sendHeader(long fd, message_hdr_t *hdr){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in sendHeader\n", fd);
        return -1;
    }

    if(hdr == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore hdr nullo in sendHeader\n");
        return -1;
    }

    //printf("SENDHDR sender %s  op %d \n", hdr->sender, hdr->op);

    //scrivo sul socket hdr messaggio da inviare
    ssize_t byteswrite = 0;


    byteswrite = writen((int)fd, hdr, sizeof(message_hdr_t));

    //byteswrite <= 0 se c'e' stato un errore
    if(byteswrite <= 0)
        return (int)byteswrite;
    else return 1;
}

int sendData(long fd, message_data_t *msg){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in sendData\n", fd);
        return -1;
    }

    if(msg == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore msg nullo in sendData\n");
        return -1;
    }

   // printf("SONO IN SENDDATA\n");
    //printf("SENDDATA buf %s   len_buf %d\n", msg->buf, msg->hdr.len);
    //printf("SENDDATA receiver %s len %d receiver %s\n", msg->hdr.receiver, msg->hdr.len, msg->buf);

    ssize_t byteswrite = 0;

    //scrivo header data, per far sapere quanto e' lungo il body del msg
    byteswrite = writen((int)fd, &msg->hdr, sizeof(message_data_hdr_t));

    //byteswrite <= 0 se c'e' stato un errore
    if(byteswrite <= 0)
        return (int)byteswrite;

    //scrivo byte per byte contenuto del body del msg
    ssize_t byte = 0;
    while(byte < msg->hdr.len){
        byteswrite = write((int)fd, (msg->buf) + byte, 1);  // 1 = scrivo byte per byte
        if(byteswrite <= 0)
            return (int)byteswrite;
        else byte = byte + byteswrite; //siccome scrivo byte per byte => bytesread = 1 ad ogni iterazione
    }

    //se sono arrivata qui, ho scritto tutti i bytes del body del msg => restituisco successo
    return 1;
}

int sendRequest(long fd, message_t *msg){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in sendRequest\n", fd);
        return -1;
    }

    if(msg == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore msg nullo in sendRequest\n");
        return -1;
    }

    int bytesread = 0;
    bytesread = sendHeader(fd, &msg->hdr); //sendHeader restituisce int
    if(bytesread <= 0)
        return bytesread;
    bytesread = sendData(fd, &msg->data);  //sendData restituisce int
    if(bytesread <= 0)
        return bytesread;

    return 1;
}

int readMsg(long fd, message_t *msg){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in readMsg\n", fd);
        return -1;
    }

    if(msg == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore msg nullo in readMsg\n");
        return -1;
    }

    /**
     * N.B. CLIENT RICHIEDE "readMsg" IN 2 OCCASIONI:
     * 1. '-R' => client  riceve un messaggio da un nickname. In base al valore di n il comportamento e' diverso, se:
     *      a) n > 0 : aspetta di ricevere 'n' messaggi e poi passa al comando successivo (se c'e')
     *      b)n <= 0: aspetta di ricevere uno o piu' messaggi indefinitamente (eventuali comandi che  seguono non verranno eseguiti)
     *
     * 2. '-p' => client richiede la history e riceve per ogni messaggio della sua history dal Server, hdr con OP_TXT_MESSAGE
     * e data contentente il buf del contenuto del messaggio.
     */

    //printf("SONO IN READMSG\n");

    message_t* msg_meno_r = NULL;
    if((msg_meno_r = (message_t*) calloc(1, sizeof(message_t))) == NULL){
        perror("msg_meno_r");
        fprintf(stderr, "Errore allocazione msg_meno_r in readMsg\n");
        free(msg_meno_r);
        exit(EXIT_FAILURE);
    }

    char* buf = NULL;
    if((buf = (char*) calloc((MAX_CONF_LINE_LENGTH), sizeof(char))) == NULL){
        perror("buf");
        fprintf(stderr, "Errore allocazione buf in readMsg\n");
        free(msg_meno_r);
        exit(EXIT_FAILURE);
    }
   // strncpy(buf, "\0", strlen(buf));
    unsigned int buf_len = (unsigned int) MAX_CONF_LINE_LENGTH;



    //mando hdr con MENO_R_OP al server
    setHeader(&msg_meno_r->hdr, MENO_R_OP, "Server\0");
    //mando un data vuoto dato che il server legge richiesta con "readServerMsg"
    setData(&msg_meno_r->data, "Server\0", buf, buf_len);

   // printf("Client con fd=%d sta per mandare MENO_R_OP\n");

    //invio msg_meno_r
    int r = sendRequest(fd, msg_meno_r);

    if(r <= 0){
        //dealloco risorsa allocata
        if(msg_meno_r != NULL){
            free(msg_meno_r);
            msg_meno_r = NULL;
        }

        //dealloco risorsa allocata
        if(buf != NULL){
            free(buf);
            buf = NULL;
        }
        return r;
    }

    //printf("Client con fd=%d sta ha mandato MENO_R_OP\n");


    int bytesread = 0;
    bytesread = readHeader(fd, &msg->hdr); //readHeader restituisce int
    if(bytesread <= 0)
        return bytesread;
    bytesread = readData(fd, &msg->data);  //readData restituisce int
    if(bytesread <= 0)
        return bytesread;

    //dealloco risorsa allocata
    if(msg_meno_r != NULL){
        free(msg_meno_r);
        msg_meno_r = NULL;
    }

    //dealloco risorsa allocata
    if(buf != NULL){
        free(buf);
        buf = NULL;
    }

    return 1;
}



/********************************************SERVER SIDE**************************************************************/

int readHeader(long fd, message_hdr_t *hdr){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in readHeader\n", fd);
        return -1;
    }

    if(hdr == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore hdr nullo in readHeader\n");
        return -1;
    }

    ssize_t bytesread;
    //leggo header messaggio
    bytesread = readn((int)fd, hdr, sizeof(message_hdr_t));

    //printf("READDHDR sender %s  op %d \n", hdr->sender, hdr->op);


    //se byetesread = 0 => connesione chiusa
    //se btesread < 0 => errore lettura header => devo settare errno
    if(bytesread <= 0)
        return (int)bytesread;
    else return 1;
}

int readData(long fd, message_data_t *data){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in readData\n", fd);
        return -1;
    }

    if(data == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore data nullo in readData\n");
        return -1;
    }

    ssize_t bytesread;
    //leggo header data, per sapere quanto e' lungo il body del msg
    bytesread = readn((int)fd, data, sizeof(message_data_hdr_t));

    //se byetesread = 0 => connesione chiusa
    //se btesread < 0 => errore lettura header => devo settare errno
    if(bytesread <= 0)
        return (int)bytesread;

    //se la dimensione del buffer dei dati e' nulla, non sto ad allocarlo
    if(data->hdr.len == 0){
        data->buf = NULL;

    }
    else{
        //alloco il buffer dei dati
        data->buf = NULL;
        if((data->buf = (char*) calloc(data->hdr.len, sizeof(char))) == NULL){ //DEVO DEALLOCARLO ALLA RICEZIONE
            perror("calloc data->buf");
            fprintf(stderr, "Errore calloc data->buf in readData\n");
            free(data->buf);
            return -1;
        }

        //leggo byte per byte contenuto del buffer dati
        ssize_t byte = 0;
        while(byte < data->hdr.len){
            bytesread = read((int)fd, (data->buf) + byte, 1); // 1 = leggo byte per byte
            if(bytesread <= 0)
                return (int)bytesread;
            else byte = byte + bytesread; //siccome leggo byte per byte => bytesread = 1 ad ogni iterazione
        }
    }

   // printf("READDATA receiver %s buf %s   len_buf %d\n", data->hdr.receiver, data->buf, data->hdr.len);


    //se sono arrivata qui, ho letto tutti i bytes del body del msg => restituisco successo
    return 1;
}

int readServerMsg(long fd, message_t *msg){

    //controllo parametri
    if(fd < 0 ){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore file descriptor -%lu- in readServerMsg\n", fd);
        return -1;
    }

    if(msg == NULL){
        errno = EINVAL; //invalid argument
        fprintf(stderr, "Errore msg nullo in readServerMsg\n");
        return -1;
    }

    //printf("SONO IN READSERVERMSG\n");

    int bytesread = 0;
    bytesread = readHeader(fd, &msg->hdr); //readHeader restituisce int
    if(bytesread <= 0)
        return bytesread;
    bytesread = readData(fd, &msg->data);  //readData restituisce int
    if(bytesread <= 0)
        return bytesread;

   // printf("SONO USCITA DA READSERVERMSG\n");
    return 1;
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

