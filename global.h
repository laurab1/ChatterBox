/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file global.h
 * @brief File contenente le variabili globali e i prototipi delle
 *        funzioni utilizzate in chatty
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

#if !defined(GLOBAL_H_)

#define GLOBAL_H_

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

/*
 * struttura queue_entry: memorizza il fd di un utente connesso
 */
typedef struct queue_entry_s {
	int connfd;
} queue_entry_t;

/* ------------ Variabili globali ------------ */
extern Conf_t config; //struttura contenente le configurazioni
extern queue_t* fdqueue; //coda condivisa dei file descriptor
extern hashtable_t* usrtbl; //tabella utenti
extern hashtable_t* grptbl; //tabella gruppi
extern volatile sig_atomic_t execute; //settata a 0 quando è necessario far terminare chatty
extern volatile sig_atomic_t print_stats; //settata a 1 quando devono essere stampate le statistiche
extern user_list_t conn_list; //lista utenti online
extern fd_set read_set; //set per la select

/* ------------ Mutua esclusione ------------*/
extern pthread_mutex_t ut_mutexes[32]; //mutex sulla user table
extern pthread_mutex_t gt_mutexes[32]; //mutex sulla group table
extern pthread_mutex_t q_mutex; //mutex sulla coda condivisa
extern pthread_cond_t q_cond; //cond sulla coda condivisa
extern pthread_mutex_t set_mutex; //mutex sul set per la select
extern pthread_mutex_t stats_mutex; //mutex sulla struttura per le statistiche
extern pthread_mutex_t list_mutex; //mutex sulla lista degli utenti connessi
extern pthread_mutex_t* fdmutexes; //mutex sui file descriptor: verranno inizializzati con MaxConnections

/* ------------ Prototipi ------------ */

//funzione per la gestione delle richieste
int execute_reply(message_t msg, long connfd);
//controlla se un utente sia registrato
int check_register(queue_t* hst_msgs, char* nick, long connfd, int op);
//verifica l'esistenza e la partecipazione di un utente ad un gruppo
int check_group(char* receiver, char* nick, long connfd);
//pulizia delle strutture
void cleanup();
//registrazione segnali
int signals_handling();
//gestione segnali di terminazione
void term_handler();
//gestione sigusr1 per la stampa delle statistiche
void stat_handler();
//invio messaggi di errore
void fail_send(char* nick, op_t op, long connfd);
//invio ack generico
void ack_send(char* nick, char* buf, long connfd);
//registrazione utente
queue_t* register_user(char* nick);
//disconnessione utente
void user_disconnect(long connfd);
//aggiornamento statistiche
void set_statistics(long nus, long nol, long ndel, long nndel, long nfdel, long nfndel, long nerrs);

#endif