#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <message.h>
#include <connections.h>

/**
 * @file  connection.c
 * @brief implementazione di connections.h
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

//Utilità

//readn attende di ricevere tutti i byte richiesti
//restituisce 0<=size<n se la connessione viene chiusa
//il numero di byte chiesti (e letti) altrimenti
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    errno = 0;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // gestione chiusura socket
        left    -= r;
	bufptr  += r;
    }
    return size;
}

//writen attende di consegnare tutti i byte richiesti
//restituisce -1 in caso di errore e setta errno
//0 se termina i byte da leggere, -1 se non vengono letti tutti i byte
static inline int writen(long fd, void *buf, size_t size) {

    size_t nleft = size;
    size_t written;
    char *bufptr = (char*)buf;
    errno = 0;
    while(nleft>0) {
	if ((written=write((int)fd ,bufptr,nleft)) == -1) {
	    if (errno == EINTR)
		continue;
	    return -1;
	}
	if (written == 0) return 0;  
        nleft    -= written;
	bufptr  += written;
    }
    return size;
}

/**
 * @function openConnection
 * @brief    Apre una connessione AF_UNIX verso il server 
 *
 * @param    path     Path del socket AF_UNIX 
 * @param    ntimes   numero massimo di tentativi di retry
 * @param    secs     tempo di attesa tra due retry consecutive
 *
 * @return   il descrittore associato alla connessione in caso di successo
 *           -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs) {

	long csfd;
	struct sockaddr_un sa;
	strncpy(sa.sun_path,path,UNIX_PATH_MAX);
	secs = (secs<MAX_SLEEPING)?secs:MAX_SLEEPING;
	ntimes = (ntimes<MAX_RETRIES)?ntimes:MAX_RETRIES;
	sa.sun_family = AF_UNIX;
	if((csfd = socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
		perror("on socket creation");
		return -1;
	} //csfd è il fd del socket
	for(int i=0;i<ntimes;i++) {
		if(connect(csfd,(struct sockaddr*) &sa,sizeof(sa)) == -1) {
			perror("connection");
			if(errno == ENOENT) //il socket non esiste
				sleep(secs);
			else return -1;
		 } else return csfd;
	}			
	return csfd;
}

// -------- server side ----- 
/*
 * @function   readHeader
 * @brief      Legge l'header del messaggio
 *
 * @param      fd     descrittore della connessione
 * @param      hdr    puntatore all'header del messaggio da ricevere
 *
 * @return     <=0 se c'e' stato un errore 
 *             (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readHeader(long connfd, message_hdr_t *hdr) {

	int nread = readn(connfd,hdr,sizeof(message_hdr_t));
	
	return nread;

}
/**
 * @function  readData
 * @brief     Legge il body del messaggio
 *
 * @param     fd     descrittore della connessione
 * @param     data   puntatore al body del messaggio
 *
 * @return    <=0 se c'e' stato un errore
 *            (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readData(long fd, message_data_t *data) {

	int nread = readn(fd,&data->hdr,sizeof(message_data_hdr_t));
	size_t size = data->hdr.len;
	if(size>0) {
		data->buf = (char*) malloc(size*sizeof(char));
		int res = readn(fd,data->buf,size);
		nread += res;
	} else
		data->buf = NULL;

	return nread;

}

/**
 * @function   readMsg
 * @brief      Legge l'intero messaggio
 *
 * @param      fd     descrittore della connessione
 * @param      data   puntatore al messaggio
 *
 * @return     <=0 se c'e' stato un errore
 *             (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readMsg(long fd, message_t *msg) {

	int nread = readHeader(fd,&(msg->hdr));

	int res = readData(fd,&(msg->data));

	return nread + res;

}

/* da completare da parte dello studente con altri metodi di interfaccia */


// ------- client side ------
/**
 * @function   sendRequest
 * @brief      Invia un messaggio di richiesta al server 
 *
 * @param      fd     descrittore della connessione
 * @param      msg    puntatore al messaggio da inviare
 *
 * @return     <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg) {

	int nwrite = sendHeader(fd,&(msg->hdr));

	int res = sendData(fd,&(msg->data));

	return nwrite + res;

}

/**
 * @function   sendData
 * @brief      Invia il body del messaggio al server
 *
 * @param      fd     descrittore della connessione
 * @param      msg    puntatore al messaggio da inviare
 *
 * @return     <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg) {

	int nwrite = writen(fd,&(msg->hdr),sizeof(message_data_hdr_t));

	size_t size = msg->hdr.len;
	if(size>0) {
		nwrite+= writen(fd,msg->buf,size);
	} else
		msg->buf = NULL;

	return nwrite;

}

/**
 * @function   sendReply
 * @brief      Invia un messaggio di risposta al client
 *
 * @param      fd     descrittore della connessione
 * @param      msg    puntatore al messaggio da inviare
 *
 * @return     <=0 se c'e' stato un errore
 */
int sendHeader(long fd, message_hdr_t *msg) {

	int nwrite = writen(fd,msg,sizeof(message_hdr_t));

	return nwrite;

}

