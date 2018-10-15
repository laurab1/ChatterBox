/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
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

/* inserire gli altri include che servono */

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };

/* ------------ Variabili globali ------------ */
Conf_t config; //struttura contenente le configurazioni
queue_t* fdqueue; //coda condivisa dei file descriptor
hashtable_t* usrtbl; //tabella utenti
hashtable_t* grptbl; //tabella gruppi
volatile sig_atomic_t execute = 1; //settata a 0 quando è necessario far terminare chatty
volatile sig_atomic_t print_stats = 0; //settata a 1 quando devono essere stampate le statistiche
user_list_t conn_list; //lista utenti online
fd_set read_set; //set per la select

/* ------------ Mutua esclusione ------------*/
pthread_mutex_t ut_mutexes[32]; //mutex sulla user table
pthread_mutex_t gt_mutexes[32]; //mutex sulla group table
pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex sulla coda condivisa
pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER; //cond sulla coda condivisa
pthread_mutex_t set_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex sul set per la select
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex sulla struttura per le statistiche
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex sulla lista degli utenti connessi
pthread_mutex_t* fdmutexes; //mutex sui file descriptor: verranno inizializzati con MaxConnections

/* ------------ Funzioni di utilità ------------ */

/* Hashing funtions
 * Well known hash function: Fowler/Noll/Vo - 32 bit version
 */
static inline unsigned int fnv_hash_function( void *key, int len ) {
	char *p = (char*) key;
	unsigned int h = 2166136261u;
	int i;
	for ( i = 0; i < len; i++ )
		h = ( h * 16777619 ) ^ p[i];
	return h;
}

/* Funzioni hash per l'hashing di stringhe */
static inline unsigned int ustring_hash_function( void *key ) {
	char* str = (char*) key;
	int len = strlen(str);
	unsigned int hashval = fnv_hash_function( key, len );
	return hashval;
}

static inline int ustring_key_compare( void *key1, void *key2  ) {
	return (!strcmp((char*)key1,(char*)key2));
}

/* Stampa a video le istruzioni per l'esecuzione di chatty */
static void usage(const char *progname) {
	fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
	fprintf(stderr, "  %s -f conffile\n", progname);
}

/* ------------ Gestione delle richieste ------------ */
/* Lavoro affidato al threadpool:
   legge un fd dalla coda e valuta la richiesta.
   Se l'utente non è registrato o arriva richiesta di registrazione da un nickname esistente,
   il thread invalida il fd e lo elimina dalla coda;
   diversamente, gestisce una richiesta e rimette in coda il fd. */
void* worker(void *arg) {
#ifdef DEBUG
	printf("Ciao\nIo sono il worker\n");
#endif
	queue_entry_t* entry;
	long connfd;
	int err = 0;
	while(execute) {
		message_t msg;
		memset(&msg,0,sizeof(msg));
		//estrazione dalla coda: da gestire in mutua esclusione
		//se la coda è vuota o ci sono altri thread attivi aspetta
#ifdef DEBUG
		printf("Acquisisco lock coda\n");
#endif
		pthread_mutex_lock(&q_mutex);
		while(is_empty(fdqueue)) {
			if(execute == 0) {
				pthread_mutex_unlock(&q_mutex);
#ifdef DEBUG
				printf("Rilascio lock coda\n");
#endif
				return NULL;
			}
			pthread_cond_wait(&q_cond,&q_mutex);
		}

		entry = pop(fdqueue);

		pthread_cond_signal(&q_cond); //sveglio eventuali thread in attesa
		pthread_mutex_unlock(&q_mutex);
#ifdef DEBUG
		printf("Rilascio lock coda\n");
#endif

		connfd = entry->connfd;
		free(entry);

		err = 0;
		errno = 0;
		//inizio la gestione della richiesta
		pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
		err = readMsg(connfd,&msg);
		pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		if(err < 0) {
			perror("In lettura messaggio");
			set_statistics(0,0,0,0,0,0,1);
			user_disconnect(connfd);
			continue;
		}
		//se err vale 0, il client ha chiuso la connessione
		if(!err) {
#ifdef DEBUG
			printf("Disconnetto\n");
			printf("connfd %ld\n",connfd);
#endif
			user_disconnect(connfd);
			set_statistics(0,-1,0,0,0,0,0);
			continue;
		}

		//if(msg.hdr.sender == NULL || strlen(msg.hdr.sender) == 0 )
		//	continue;

		int result = execute_reply(msg,connfd);
		if(result) {
			user_disconnect(connfd);
			set_statistics(0,-1,0,0,0,0,0);
		} else {
			//reinserisco il connfd nel set per la select;
			pthread_mutex_lock(&set_mutex);
			FD_SET(connfd,&read_set);
			pthread_mutex_unlock(&set_mutex);
		}
#ifdef DEBUG
		printf("Ricomincio\n");
#endif
	}

	return NULL;

}

/* Gestione della richiesta: execute_reply gestisce l'operazione
   richiesta dal client e restituisce 0 in caso di successo, 1 in
   caso di fallimento */
int execute_reply(message_t msg, long connfd) {

	char nick[MAX_NAME_LENGTH];
	op_t op = msg.hdr.op;
	strcpy(nick,msg.hdr.sender);
	queue_t* hst_msgs = hash_find(usrtbl,nick);
	group_t* group = hash_find(grptbl,msg.data.hdr.receiver);
	//controlla che il nickname sia registrato;
	//se non lo è, invalida il fd e rigetta tutte le richieste successive
	if(check_register(hst_msgs,nick,connfd,op))
		return 1;
	if(msg.data.hdr.receiver != NULL && op == CREATEGROUP_OP)
		if(check_group(msg.data.hdr.receiver,nick,connfd)) {
			return 1;
		}

	switch(op) {
		case REGISTER_OP: { //funziona
			//se l'utente si è appena registrato, lo inserisco nella usrtbl
			hst_msgs = register_user(nick);
			set_statistics(1,0,0,0,0,0,0);
		}
		case CONNECT_OP: { //funziona
			pthread_mutex_lock(&list_mutex);
			for(int i=0; i<config.MaxConnections; i++)
				if(conn_list[i] == NULL) { //appena trovo una slot libera inserisco
					conn_list[i] = user_init(nick,connfd,hst_msgs);
					break;
				}
			pthread_mutex_unlock(&list_mutex);
			set_statistics(0,1,0,0,0,0,0);
		}
		case USRLIST_OP: { //funziona
			int count = 0;
			pthread_mutex_lock(&list_mutex);
			//conto gli utenti online dalla lista per evitare inconsistenze
			for(int i=0; i<config.MaxConnections; i++) {
				if(conn_list[i] != NULL)
					count++;
			}
#ifdef DEBUG
			printf("numero utenti online %d\n",count);
#endif
			char list[(MAX_NAME_LENGTH+1)*count];
			int j=0;
			for(int i=0; i<config.MaxConnections; i++)
				if(conn_list[i] != NULL) {
					strncpy(list+((MAX_NAME_LENGTH+1)*j),conn_list[i]->nickname,MAX_NAME_LENGTH+1);
					j++;
			}
			pthread_mutex_unlock(&list_mutex);
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			setData(&reply.data,nick,list,(MAX_NAME_LENGTH+1)*count);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&(reply.hdr));
			sendData(connfd,&(reply.data));
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		case POSTTXT_OP: { //pare che funzioni
			if(msg.data.hdr.len > config.MaxMsgSize) {
				fail_send(nick,OP_MSG_TOOLONG,connfd);
				set_statistics(0,0,0,0,0,0,1);
				free(msg.data.buf);
				return 1;
			}
			char receiver[MAX_NAME_LENGTH];
			strcpy(receiver,msg.data.hdr.receiver);
			group_t* aux;
			if(group == NULL) { //devo spedire a un solo utente
				aux = group_init("tempgroup",receiver);
			} else
				aux = group;
			int found = 0;
			int j = 0;
			//controllo che l'utente faccia parte del gruppo
			if(group != NULL)
				while(j < group->n_memb && !found) {
					if(!strcmp(nick,group->ulist[j]->nickname))
						found = 1;
					j++;
				}
			if(group != NULL && !found) {
				fail_send(nick,OP_NICK_UNKNOWN,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			for(int j=0; j<aux->n_memb; j++) {
				message_t* new_msg = (message_t*) malloc(sizeof(message_t));
				if(new_msg == NULL) {
					fprintf(stderr,"Impossibile allocare messaggio\n");
					fail_send(nick,OP_FAIL,connfd);
					set_statistics(0,0,0,0,0,0,1);
					free(msg.data.buf);
					return 1;
				}
				memcpy(&new_msg->hdr,&msg.hdr,sizeof(msg.hdr));
				memcpy(&new_msg->data.hdr,&msg.data.hdr,sizeof(msg.data.hdr));
				new_msg->data.buf = (char*) malloc(msg.data.hdr.len*sizeof(char));
				if(new_msg->data.buf == NULL) {
					fprintf(stderr,"Impossibile allocare messaggio\n");
					fail_send(nick,OP_FAIL,connfd);
					set_statistics(0,0,0,0,0,0,1);
					free(msg.data.buf);
					return 1;
				}
				strcpy(new_msg->data.buf,msg.data.buf);
				//cerco tra gli utenti online
				queue_t* tab_pos = NULL;
				pthread_mutex_lock(&list_mutex);
				int delivered = 0;
				for(int i=0; i<config.MaxConnections; i++)
					if(conn_list[i] != NULL && !strcmp(conn_list[i]->nickname,aux->ulist[j]->nickname)) {
						new_msg->hdr.op = TXT_MESSAGE;
						pthread_mutex_lock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
						sendHeader(conn_list[i]->connfd,&new_msg->hdr);
						sendData(conn_list[i]->connfd,&new_msg->data);
						pthread_mutex_unlock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
						delivered = 1;
						break;
					}
				pthread_mutex_unlock(&list_mutex);
				if(delivered)
					set_statistics(0,0,1,0,0,0,0);
				//devo aggiungere alla history, no matter what
				int key = ustring_hash_function(aux->ulist[j]->nickname);
				key = key%TAB_DIM;
				pthread_mutex_lock(&ut_mutexes[key/32]);
				tab_pos = hash_find(usrtbl,aux->ulist[j]->nickname);
				if(tab_pos == NULL)
					if(group == NULL) {
						pthread_mutex_unlock(&ut_mutexes[key/32]);
						fail_send(nick,OP_NICK_UNKNOWN,connfd);
						set_statistics(0,0,0,0,0,0,1);
						return 1;
					} else
						group_remove(group,aux->ulist[j]->nickname);
				else {
					if(tab_pos->dim == tab_pos->max_size) {
						message_t* tmp = pop(tab_pos);
						free(tmp);
					}
					push(tab_pos,new_msg);
					pthread_mutex_unlock(&ut_mutexes[key/32]);
					if(!delivered)
						set_statistics(0,0,0,1,0,0,0);
				}
			}
			if(group == NULL) {
				free(aux->ulist[0]);
				free(aux->ulist);
				free(aux);
			}
			free(msg.data.buf);
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		case POSTTXTALL_OP: {
			if(msg.data.hdr.len > config.MaxMsgSize) {
				fail_send(nick,OP_MSG_TOOLONG,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			pthread_mutex_lock(&list_mutex);
			int delivered = 0;
			for(int i=0; i<config.MaxConnections; i++)
				if(conn_list[i] != NULL && strcmp(conn_list[i]->nickname,nick)) {
					msg.hdr.op = TXT_MESSAGE;
					pthread_mutex_lock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
					sendHeader(conn_list[i]->connfd,&msg.hdr);
					sendData(conn_list[i]->connfd,&msg.data);
					pthread_mutex_unlock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
					delivered++;
				}
			pthread_mutex_unlock(&list_mutex);
			set_statistics(0,0,delivered,0,0,0,0);
			//ora posso aggiungere a tutte le history
			int not_delivered = 0;
			for(int j=0; j<32; j++) {
				pthread_mutex_lock(&ut_mutexes[j]);
				for(int i=32*j; i<32*(j+1); i++) {
					hash_entry_t* curr = usrtbl->buckets[i]->head;
					while(curr != NULL) {
						message_t* new_msg = (message_t*) malloc(sizeof(message_t));
						if(new_msg == NULL) {
							fprintf(stderr,"Impossibile allocare messaggio\n");
							pthread_mutex_unlock(&ut_mutexes[j]);
							fail_send(nick,OP_FAIL,connfd);
							set_statistics(0,0,0,0,0,0,1);
							free(msg.data.buf);
							return 1;
						}
						memcpy(&new_msg->hdr,&msg.hdr,sizeof(msg.hdr));
						memcpy(&new_msg->data.hdr,&msg.data.hdr,sizeof(msg.data.hdr));
						new_msg->hdr.op = TXT_MESSAGE;
						new_msg->data.buf = (char*) malloc(msg.data.hdr.len*sizeof(char));
						if(new_msg == NULL) {
							fprintf(stderr,"Impossibile allocare messaggio\n");
							pthread_mutex_unlock(&ut_mutexes[j]);
							fail_send(nick,OP_FAIL,connfd);
							set_statistics(0,0,0,0,0,0,1);
							free(msg.data.buf);
							return 1;
						}
						strcpy(new_msg->data.buf,msg.data.buf);
						queue_t* tab_pos = (queue_t*) curr->data;
						if(tab_pos->dim == tab_pos->max_size) {
							message_t* tmp = pop(tab_pos);
							free(tmp);
						}
						push(tab_pos,new_msg);
						not_delivered++;
						curr = curr->next;
					}
				}
				pthread_mutex_unlock(&ut_mutexes[j]);
			}
			free(msg.data.buf);
			//invio l'ack
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
			not_delivered -= delivered;
			set_statistics(0,0,0,not_delivered,0,0,0);
		}
		break;
		case POSTFILE_OP: {
			//copio il nome e libero il buffer di msg
			int name_size = msg.data.hdr.len;
			char file_name[name_size+1];
			memset(file_name,'\0',name_size);
			strcpy(file_name,msg.data.buf);
			free(msg.data.buf);
			//preparo il buffer per la ricezione del file
			message_data_t file;
			if(readData(connfd,&file) < 0) {
				perror("In lettura dati");
				fail_send(nick,OP_FAIL,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			//faccio un controllo preventivo per evitare di caricare il file
			//nel caso in cui l'utente a cui voglio inviarlo non esista
			queue_t* tab_pos = hash_find(usrtbl,msg.data.hdr.receiver);
			if(group == NULL && check_register(tab_pos,nick,connfd,op)) {
				free(file.buf);
				return 1;
			}
			int file_size = file.hdr.len;
			//MaxFileSize è espresso in kbytes
			if(file_size > config.MaxFileSize*1024) {
				fail_send(nick,OP_MSG_TOOLONG,connfd);
				free(file.buf);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			//gestione della richiesta
			FILE* opf;
			int len = strlen(config.DirName) + strlen(file_name) + 2;
			//path definitivo del file memorizzato in chatty
			char path[len];
			memset(path,'\0',len);
			strcpy(path,config.DirName);
			if(file_name[0] == '.')
				strcat(path,file_name+1);
			else {
				strcat(path,"/");
				strcat(path,file_name);
			}
			//ora posso scrivere sul file
			opf = fopen(path,"wb");
			if(opf == NULL) {
				perror("Aprendo il file in scrittura");
				fail_send(nick,OP_FAIL,connfd);
				set_statistics(0,0,0,0,0,0,1);
				free(file.buf);
				return 1;
			}
			len = 0;
			len = fwrite(file.buf,sizeof(char),file_size,opf);
			if(len < file_size) {
				perror("Scrivendo il file");
				fail_send(nick,OP_FAIL,connfd);
				set_statistics(0,0,0,0,0,0,1);
				free(file.buf);
				fclose(opf);
				return 1;
			}
			fclose(opf);
			//a questo punto, ho ricevuto il file correttamente:
			//provo a vedere se il destinatario sia connesso
			char receiver[MAX_NAME_LENGTH+1];
			memset(receiver,'\0',MAX_NAME_LENGTH+1);
			strcpy(receiver,msg.data.hdr.receiver);

			group = hash_find(grptbl,receiver);
			group_t* aux;
			if(group == NULL) { //devo spedire a un solo utente
				aux = group_init("tempgroup",receiver);
			} else
				aux = group;
			int found = 0;
			int j = 0;
			//controllo che l'utente faccia parte del gruppo
			if(group != NULL)
				while(j < group->n_memb && !found) {
					if(!strcmp(nick,group->ulist[j]->nickname))
						found = 1;
					j++;
				}
			if(group != NULL && !found) {
				fail_send(nick,OP_NICK_UNKNOWN,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			for(int j=0; j<aux->n_memb; j++) {			
				pthread_mutex_lock(&list_mutex);
				int delivered = 0;
				for(int i=0; i<config.MaxConnections; i++)
					if(conn_list[i] != NULL && !strcmp(conn_list[i]->nickname,aux->ulist[j]->nickname)) {
						message_t notify;
						setHeader(&notify.hdr,FILE_MESSAGE,nick);
						setData(&notify.data,aux->ulist[j]->nickname,file_name,name_size);
						pthread_mutex_lock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
						sendRequest(conn_list[i]->connfd,&notify);
						pthread_mutex_unlock(&fdmutexes[conn_list[i]->connfd%config.MaxConnections]);
						delivered = 1;
					}
				pthread_mutex_unlock(&list_mutex);
				if(delivered)
					set_statistics(0,0,0,0,1,0,0);
				else {
					//aggiungo la notifica alla history
					//controllo di sicurezza
					int key = ustring_hash_function(aux->ulist[j]->nickname);
					key = key%TAB_DIM;
					pthread_mutex_lock(&ut_mutexes[key/32]);
					tab_pos = hash_find(usrtbl,aux->ulist[j]->nickname);
					if(tab_pos == NULL)
						if(group == NULL) {
							pthread_mutex_unlock(&ut_mutexes[key/32]);
							fail_send(nick,OP_NICK_UNKNOWN,connfd);
							set_statistics(0,0,0,0,0,0,1);
							return 1;
						} else
							group_remove(group,aux->ulist[j]->nickname);
					else {
						char* f_name = (char*) calloc((name_size+1)*sizeof(char),'\0');
						strcpy(f_name,file_name);
						message_t* notify = (message_t*) malloc(sizeof(message_t));
						if(notify == NULL) {
							fprintf(stderr,"Impossibile allocare messaggio\n");
							pthread_mutex_unlock(&ut_mutexes[j]);
							fail_send(nick,OP_FAIL,connfd);
							set_statistics(0,0,0,0,0,0,1);
							free(msg.data.buf);
						}
						setHeader(&notify->hdr,FILE_MESSAGE,nick);
						setData(&notify->data,receiver,f_name,name_size);
						if(tab_pos->dim == tab_pos->max_size) {
							message_t* tmp = pop(tab_pos);
							free(tmp);
						}
						push(tab_pos,notify);
						pthread_mutex_unlock(&ut_mutexes[key/32]);
						set_statistics(0,0,0,0,0,1,0);
					}
				}
			}
			if(group == NULL) {
				free(aux->ulist[0]);
				free(aux->ulist);
				free(aux);
			}

			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
			free(file.buf);
		}
		break;
		case GETFILE_OP: {
			//copio il nome del file e lo concateno al path contenuto in config
			int name_size = msg.data.hdr.len;
			char file_name[name_size+1];
			strcpy(file_name,msg.data.buf);
			free(msg.data.buf);
			int len = strlen(config.DirName) + name_size + 2;
			char path[len];
			strcpy(path,config.DirName);
			strcat(path,"/");
			strcat(path,file_name);
			//apro il file e faccio i controlli
			int ipf = open(path,O_RDONLY);
			if(ipf < 0) {
				perror("Aprendo il file");
				fail_send(nick,OP_NO_SUCH_FILE,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			struct stat file_stat;
			int err = stat(path,&file_stat);
			//il controllo che il file sia regolare viene fatto dal client
			if(err == -1) {
				fprintf(stderr,"In file stat\n");
				fail_send(nick,OP_NO_SUCH_FILE,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			//buffer per mappare il file in memoria
			char* m_file;
			size_t file_size = file_stat.st_size;
			m_file = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, ipf, 0);
			if(m_file == MAP_FAILED) {
				perror("Mappando il file in memoria");
				fail_send(nick,OP_NO_SUCH_FILE,connfd);
				set_statistics(0,0,0,0,0,0,1);
				return 1;
			}
			close(ipf);
			//spedisco
			message_t file_msg;
			setHeader(&file_msg.hdr,OP_OK,nick);
			setData(&file_msg.data,nick,m_file,file_size);
			int key = ustring_hash_function(nick);
			key = key%TAB_DIM;
			pthread_mutex_lock(&ut_mutexes[key/32]);
			sendRequest(connfd,&file_msg);
			pthread_mutex_unlock(&ut_mutexes[key/32]);
			set_statistics(0,0,0,0,1,-1,0);
		}
		break;
		case GETPREVMSGS_OP: {
			hst_msgs = hash_find(usrtbl,nick);
			size_t* n_msgs = (size_t*) malloc(sizeof(size_t));
			*n_msgs = hst_msgs->dim;
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			setData(&reply.data," ",(char*) n_msgs,sizeof(size_t));
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			sendData(connfd,&reply.data);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
			int key = ustring_hash_function(nick);
			key = key%TAB_DIM;
			pthread_mutex_lock(&ut_mutexes[key/32]);
			for(int i=0; i<*n_msgs; i++) {
				message_t* new_msg = pop(hst_msgs);
				pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
				sendRequest(connfd,new_msg);
				pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
				push(hst_msgs,new_msg);	
			}
			pthread_mutex_unlock(&ut_mutexes[key/32]);
			set_statistics(0,0,*n_msgs,0,0,0,0);
			free(n_msgs);
		}
		break;
		case UNREGISTER_OP: {
			char removing[MAX_NAME_LENGTH];
			strcpy(removing,msg.hdr.sender);
			int key = ustring_hash_function(removing);
			key = key%TAB_DIM;
			pthread_mutex_lock(&ut_mutexes[key/32]);
			hst_msgs = hash_find(usrtbl,removing);
			while(!is_empty(hst_msgs)) {
				message_t* msg = pop(hst_msgs);
				free(msg->data.buf);
				free(msg);
			}
			queue_delete(hst_msgs);
				hash_remove(usrtbl,removing);
			pthread_mutex_unlock(&ut_mutexes[key/32]);
			set_statistics((-1),0,0,0,0,0,0);	
			//invio l'ack
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		case CREATEGROUP_OP: {
			char groupname[MAX_NAME_LENGTH+1];
			strcpy(groupname,msg.data.hdr.receiver);
			group = group_init(groupname,nick);
			int key = ustring_hash_function(groupname);
			key = key%TAB_DIM;
			pthread_mutex_lock(&gt_mutexes[key/32]);
			hash_insert(grptbl,groupname,group);
			pthread_mutex_unlock(&gt_mutexes[key/32]);
			//invio l'ack
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		case ADDGROUP_OP: {
			char groupname[MAX_NAME_LENGTH+1];
			strcpy(groupname,msg.data.hdr.receiver);
			int key = ustring_hash_function(groupname);
			key = key%TAB_DIM;
			pthread_mutex_lock(&gt_mutexes[key/32]);
			group_t* group = hash_find(grptbl,groupname);
			for(int i=0; i<group->n_memb; i++)
				if(!strcmp(nick,group->ulist[i]->nickname)) {
					pthread_mutex_unlock(&gt_mutexes[key/32]);
					fail_send(nick,OP_NICK_ALREADY,connfd);
					set_statistics(0,0,0,0,0,0,1);
					return 1;
				}
			group_add(group,nick);
			pthread_mutex_unlock(&gt_mutexes[key/32]);
			//invio l'ack
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		case DELGROUP_OP: {
			char groupname[MAX_NAME_LENGTH+1];
			strcpy(groupname,msg.data.hdr.receiver);
			int key = ustring_hash_function(groupname);
			key = key%TAB_DIM;
			pthread_mutex_lock(&gt_mutexes[key/32]);
			group_t* group = hash_find(grptbl,groupname);
			for(int i=0; i<group->n_memb; i++)
				if(!strcmp(group->ulist[i]->nickname,nick)) {
					free(group->ulist[i]);
					break;
				}
			pthread_mutex_unlock(&gt_mutexes[key/32]);
			//invio l'ack
			message_t reply;
			memset(&reply,0,sizeof(reply));
			setHeader(&reply.hdr,OP_OK,nick);
			pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
			sendHeader(connfd,&reply.hdr);
			pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);
		}
		break;
		default:
		break;
		//non ho bisogno di specificare operazioni di gestione di richieste non riconosciute
		//sono già gestite lato client
	}
	return 0;
}

/* Set_statistics aggiorna le statistiche con i valori passati
   come parametro, sommandoli ai valori presenti in chattyStats */
void set_statistics(long nus, long nol, long ndel, long nndel, long nfdel, long nfndel, long nerrs) {

	pthread_mutex_lock(&stats_mutex);
	chattyStats.nusers += nus;
	chattyStats.nonline += nol;
	chattyStats.ndelivered += ndel;
	chattyStats.nnotdelivered += nndel;
	chattyStats.nfiledelivered += nfdel;
	chattyStats.nfilenotdelivered += nfndel;
	chattyStats.nerrors += nerrs;
	pthread_mutex_unlock(&stats_mutex);

}

/* Check register controlla se un utente che fa richiesta di connessione
   sia effettivamente registrato al servizio o che un utente non stia cercando
   di registrarsi con un nome già esistente */
int check_register(queue_t* hst_msgs, char* nick, long connfd, int op) {

	if(hst_msgs == NULL && op != REGISTER_OP) {
#ifdef DEBUG
		fprintf(stdout,"Nickname non esistente\n");
#endif
		fail_send(nick,OP_NICK_UNKNOWN,connfd);
		return 1;
	}
	//se arriva una richiesta di registrazione di un nickname
	//esistente, invalida il fd e rigetta le richieste successive
	if(hst_msgs != NULL && op == REGISTER_OP) {
#ifdef DEBUG			
		fprintf(stdout,"Utente già registrato\n");
#endif					
		fail_send(nick,OP_NICK_ALREADY,connfd);
		return 1;
	}

	return 0;

}

/* Check group controlla se un gruppo esista o meno e che
   un gruppo che si sta tentando di creare non abbia lo stesso nome di
   un altro gruppo o di un utente */
int check_group(char* receiver, char* nick, long connfd) {

	if(hash_find(usrtbl,receiver) != NULL && hash_find(grptbl,receiver) != NULL) {
#ifdef DEBUG			
		fprintf(stdout,"Nome già registrato\n");
#endif					
		fail_send(nick,OP_NICK_ALREADY,connfd);
		set_statistics(0,0,0,0,0,0,1);
		return 1;
	}

	return 0;

}

/* Disconnessione utente: quando viene letto 0 su connfd
   l'utente collegato ad esso viene rimosso dalla lista degli
   utenti online e la connessione viene chiusa */
void user_disconnect(long connfd) {

	pthread_mutex_lock(&list_mutex);
	for(int i=0; i<config.MaxConnections; i++)
		if(conn_list[i] != NULL && conn_list[i]->connfd == connfd) {
			free(conn_list[i]);
			conn_list[i] = NULL;
			break;
		}
	pthread_mutex_unlock(&list_mutex);
	pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
	close(connfd);
	pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);

}

/* Registrazione di un nuovo utente:
   viene creata una nuova history messages a nome nick
   e inserita nella tabella */
queue_t* register_user(char* nick) {

	queue_t* hst_msgs = queue_init(config.MaxHistMsgs);
	int key = ustring_hash_function(nick);
	key = key%TAB_DIM;
	pthread_mutex_lock(&ut_mutexes[key/32]);
	hash_insert(usrtbl,nick,hst_msgs);
	pthread_mutex_unlock(&ut_mutexes[key/32]);

	return hst_msgs;

}

/* Invio messaggio di errore: fail_send invia un messaggio di
   fallimento all'utente nick e chiude la connessione connfd */
void fail_send(char* nick, op_t op, long connfd) {

	message_t msg;
	memset(&msg,0,sizeof(msg));
	setHeader(&msg.hdr,op,nick);
	pthread_mutex_lock(&fdmutexes[connfd%config.MaxConnections]);
	sendHeader(connfd,&msg.hdr);
	pthread_mutex_unlock(&fdmutexes[connfd%config.MaxConnections]);

}

/* ------------ Cleanup e Signal Handler ------------ */

/* Funzione di cleanup: pulisce i campi data di usrtbl e grptbl,
   per poi deallocare tutte le strutture dati */
void cleanup() {

	hash_entry_t* curr;
	for(int i=0; i<TAB_DIM; i++) {
		curr = usrtbl->buckets[i]->head;
		while(curr != NULL) {
			while(!is_empty(curr->data)) {
				message_t* msg = pop(curr->data);
				free(msg->data.buf);
				free(msg);
			}
			queue_delete(curr->data);
			curr = curr->next;
		}
	}
	hash_delete(usrtbl);
	hash_delete(grptbl);
	queue_delete(fdqueue);
	unlink(config.UnixPath);
	for(int i=0; i<config.MaxConnections; i++)
		if(conn_list[i] != NULL)
			free(conn_list[i]);
	free(conn_list);
	eraseConfig(&config);
	free(fdmutexes);

}

/* Funzione chiamata alla configurazione di chatty:
   registra i segnali SIGINT, SIGTERM, SIGQUIT, SIGPIPE
   e SIGUSR1 */
int signals_handling() {

	/* Inizializzazione delle strutture */
	struct sigaction h_term;
	struct sigaction h_pipe;
	struct sigaction h_usr1;

	memset(&h_term,0,sizeof(h_term));
	memset(&h_pipe,0,sizeof(h_pipe));
	memset(&h_usr1,0,sizeof(h_usr1));

	/* Assegnazione degli handler */
	h_term.sa_handler = term_handler;
	h_pipe.sa_handler = SIG_IGN;
	h_usr1.sa_handler = stat_handler;

	/* Registrazione segnali */
	int err = 0;
	err = sigaction(SIGINT, &h_term, NULL);
  	if (err == -1) {
    	perror("Registrazione segnali");
    	return -1;
  	}
	err = sigaction(SIGQUIT, &h_term, NULL);
  	if (err == -1) {
  	  perror("Registrazione segnali");
  	  return -1;
  	}
	err = sigaction(SIGTERM, &h_term, NULL);
  	if (err == -1) {
  	  perror("Registrazione segnali");
  	  return -1;
  	}
	err = sigaction(SIGUSR1, &h_usr1, NULL);
  	if (err == -1) {
  	  perror("Registrazione segnali");
  	  return -1;
  	}
	err = sigaction(SIGPIPE, &h_pipe, NULL);
  	if (err == -1) {
  	  perror("Registrazione segnali");
  	  return -1;
  	}
#ifdef DEBUG
	printf("Segnali registrati\n");
#endif
	return 0;
}

/* Gli handler mi servono solo a settare le variabili volatile:
   le funzioni per la gestione di interruzione e stampa verranno
   chiamate di conseguenza */
/* Handler per la terminazione di chatty: setta a 0 execute */
void term_handler() {
	execute = 0;
	pthread_cond_broadcast(&q_cond); //da controllare se signal safe
}

/* Handler per la terminazione di chatty: setta a 1 print_stats */
void stat_handler() {
	print_stats = 1;
}

/* ------------ Main Server ------------ */
int main(int argc, char *argv[]) {

	fprintf(stdout,"Server chatty in avvio...\n");

	/* Avvio del server, configurazione e registrazione dei segnali */
	if(argc < 3 || strcmp("-f",argv[1]) != 0) {
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	char* path = argv[2];
	if(setConfig(&config,path) == -1) {
		fprintf(stderr,"errore nell'apertura del file di configurazione\n");
		exit(EXIT_FAILURE);
	}
	if(strlen(config.UnixPath) > UNIX_PATH_MAX)
		exit(EXIT_FAILURE);
#ifdef DEBUG
	fprintf(stdout,"%s\n",config.UnixPath);
	fprintf(stdout,"%d\n",config.MaxConnections);
	fprintf(stdout,"%d\n",config.ThreadsInPool);
	fprintf(stdout,"%d\n",config.MaxMsgSize);
	fprintf(stdout,"%d\n",config.MaxFileSize);
	fprintf(stdout,"%d\n",config.MaxHistMsgs);
	fprintf(stdout,"%s\n",config.DirName);
	fprintf(stdout,"%s\n",config.StatFileName);
	fprintf(stdout,"Configurazione del server completata\n");
#endif
	
	if(signals_handling() == -1) {
		fprintf(stderr, "Errore nella configurazione del server\n");
		eraseConfig(&config);
		exit(EXIT_FAILURE);
	}
	/* Configurazione del server terminata */	

	/* Avvio del threadpool e creazione delle strutture dati */
	//coda file descriptors
	fdqueue = queue_init(config.MaxConnections);
	//il thpool è un semplice array di thread
	pthread_t thpool[config.ThreadsInPool];
	//listener
	pthread_t lis;
	//tabella utenti
	usrtbl = hashtable_init(TAB_DIM, ustring_hash_function, ustring_key_compare);
	//tabella gruppi
	grptbl = hashtable_init(TAB_DIM, ustring_hash_function, ustring_key_compare);
	//lista utenti online
	conn_list = (user_list_t) malloc(config.MaxConnections*sizeof(user_t*));

	if(fdqueue == NULL || usrtbl == NULL || grptbl == NULL || conn_list == NULL) {
		fprintf(stderr,"Errore in avvio\n");
		exit(EXIT_FAILURE);
	}

	for(int i=0; i<config.MaxConnections; i++)
		conn_list[i] = NULL;

	for(int i=0; i<32; i++)
		if(pthread_mutex_init(&ut_mutexes[i],NULL)!=0 || pthread_mutex_init(&gt_mutexes[i],NULL) != 0) {
			queue_delete(fdqueue);
			hash_delete(usrtbl);
			hash_delete(grptbl);
			free(conn_list);
			exit(EXIT_FAILURE);
		}

	fdmutexes = (pthread_mutex_t*) malloc(config.MaxConnections*sizeof(pthread_mutex_t));
	if(fdmutexes == NULL) {
		queue_delete(fdqueue);
		hash_delete(usrtbl);
		hash_delete(grptbl);
		free(conn_list);
		exit(EXIT_FAILURE);
	}

	int err = 0;
	for(int i=0; i<config.MaxConnections; i++) {
		err = pthread_mutex_init(&fdmutexes[i],NULL);
		if(err != 0) {
			queue_delete(fdqueue);
			hash_delete(usrtbl);
			hash_delete(grptbl);
			free(fdmutexes);
			free(conn_list);
			exit(EXIT_FAILURE);
		}
	}

	for(int i=0; i<config.ThreadsInPool; i++)
		if((pthread_create(&thpool[i],NULL,&worker,NULL))!=0) {
			fprintf(stdout,"error creating producer\n");
			queue_delete(fdqueue);
			hash_delete(usrtbl);
			hash_delete(grptbl);
			free(fdmutexes);
			free(conn_list);
			exit(EXIT_FAILURE);
		}

#ifdef DEBUG
	fprintf(stdout,"Creazione threadpool, queue e hashtable completata\n");
#endif
	/*Avvio strutture terminato*/

	/*Avvio il listener*/
	if((pthread_create(&lis,NULL,&listener,NULL))!=0) {
		fprintf(stdout,"error creating listener\n");
		queue_delete(fdqueue);
		hash_delete(usrtbl);
		hash_delete(grptbl);
		free(fdmutexes);
		free(conn_list);
		exit(EXIT_FAILURE);
	}

	//attendo gli worker
	//non controllo l'errore: devo comunque pulire
	for(int i=0; i<config.ThreadsInPool; i++) {
		pthread_join(thpool[i],NULL);
	}

	//attendo il listener
	pthread_join(lis,NULL);

	cleanup();

	exit(EXIT_SUCCESS);

}
