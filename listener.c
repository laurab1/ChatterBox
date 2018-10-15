/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file listener.c
 * @brief Implementazione dell'header listener.h
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <message.h>
#include <queue.h>
#include <hashtable.h>
#include <config.h>
#include <ops.h>
#include <user.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stats.h>
#include <time.h>
#include <connections.h>
#include <listener.h>

void* listener(void* arg) {

	/*Apertura del socket e attesa connessione;
	  da qui in poi il main si limita ad aspettare
	  richieste di connessione, per poi passarle ai
	  thread worker*/
	errno=0;
	int listenfd;
	listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(listenfd == -1) {
		execute = 0;
		fprintf(stdout,"Errore in avvio:\ncreazione socket\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	if(strlen(config.UnixPath) > UNIX_PATH_MAX) {
		fprintf(stdout,"Errore in connessione:\npath non valido\n");
		execute = 0;
		exit(EXIT_FAILURE);
	}
	strncpy(serv_addr.sun_path, config.UnixPath, UNIX_PATH_MAX);

	int notused;
	notused = bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
	if(notused == -1) {
		execute = 0;
		perror("Errore in connessione:");
		exit(EXIT_FAILURE);
	}
	notused = listen(listenfd, config.MaxConnections);
	if(notused == -1) {
		execute = 0;
		fprintf(stdout,"Errore in connessione:\nlisten\n");
		exit(EXIT_FAILURE);
	}

	fd_set tmp_set;
	long max_fd, res, connfd, fd;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500;

	FD_ZERO(&read_set);
	FD_SET(listenfd,&read_set);
	max_fd = listenfd;
	while(execute) {
		if(print_stats) {
			FILE* ofp = fopen(config.StatFileName,"a");
			if(ofp == NULL)
				fprintf(stderr,"Impossibile stampare le statistiche\n");
			else {
				if(printStats(ofp) < 0) {
					fprintf(stderr,"Impossibile stampare le statistiche\n");
					set_statistics(0,0,0,0,0,0,1);
				}
				else
					fprintf(stdout,"Statistiche stampate\n");
				fclose(ofp);
			}
		}
		print_stats = 0;
		pthread_mutex_lock(&set_mutex);
		memcpy(&tmp_set,&read_set,sizeof(tmp_set));
		pthread_mutex_unlock(&set_mutex);
		res = select(max_fd+1,&tmp_set,NULL,NULL,&tv);
		if(res < 0 && errno != EINTR) {
			perror("Errore in select");
			execute = 0;
		} else if(res >= 0 && execute) {
			for (fd = 0; fd <= max_fd; fd++) {
        		if (!FD_ISSET(fd, &tmp_set) ) continue;
				
				if(fd == listenfd) {
					connfd = accept(fd,NULL,0);
					if(connfd < 0) {
						perror("Errore in accept:");
						continue;
					}
					else {
						pthread_mutex_lock(&set_mutex);
						FD_SET(connfd,&read_set);
						pthread_mutex_unlock(&set_mutex);
						max_fd = (max_fd<connfd)?connfd:max_fd;
					}
				} else {
					pthread_mutex_lock(&set_mutex);
					FD_CLR(fd,&read_set);
					pthread_mutex_unlock(&set_mutex);
					queue_entry_t* entry = (queue_entry_t*) malloc(sizeof(queue_entry_t));
					if(entry == NULL) {
						perror("Malloc:");
						execute = 0;
						break;
					}
					entry->connfd = fd;
					int push_result;
#ifdef DEBUG
					printf("main Acquisisco lock coda\n");
#endif
					pthread_mutex_lock(&q_mutex);
	
					push_result = push(fdqueue,entry);
					if(push_result == 1) {
						message_hdr_t fail_msg;
						setHeader(&fail_msg,OP_FAIL," ");
						sendHeader(fd,&fail_msg);
						close(fd);
						free(entry);
					}

					pthread_cond_signal(&q_cond);
					pthread_mutex_unlock(&q_mutex);
#ifdef DEBUG
					printf("main Rilascio lock coda\n");
#endif
				}
			}
		}
	}

	return NULL;

}
