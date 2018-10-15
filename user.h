#ifndef USER_H
#define USER_H

#include <hashtable.h>
#include <queue.h>
#include <config.h>
#include <message.h>

/**
 * @file   user.h
 * @brief  Header delle strutture utenti e gruppi per l'uso nella hashtable.
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/**
 * @struct  user_s
 * @brief   Struttura utente: contiene dati relativi a un utente connesso
 *
 * @var   nickname         nome di registrazione dell'utente
 * @var   tab_pos          puntatore alla posizione nella tabella hash
 * @var   connfd           il file descriptor di connessione dell'utente
 */
typedef struct user_s {
	char nickname[MAX_NAME_LENGTH+1];
	queue_t* tab_pos;
	long connfd;
} user_t;

/* Lista utenti per l'uso nella group */
typedef user_t**  user_list_t;

/**
 * @struct  group_s
 * @brief   Struttura group: contiene dati relativi a un gruppo
 *
 * @var   groupname        nome del gruppo
 * @var   ulist            lista dei membri del gruppo
 * @var   n_memb           numero di utenti nel gruppo
 */
typedef struct group_s {
	char groupname[MAX_NAME_LENGTH+1];
	user_list_t ulist;
	int n_memb;
} group_t;

/* ------------ Operazioni sugli utenti ------------ */


/**
 * @function                user_init
 * @brief                   crea un nuovo utente
 *
 * @param  nick             il nickname dell'utente
 * @param  connfd           il fd di connessione dell'utente
 *
 * @return il puntatore all'utente, NULL in caso di errore
 */
user_t* user_init(char* nick, long connfd, queue_t* tab_pos);

/**
 * @function                user_send
 * @brief                   invia un messaggio all'utente
 *
 * @param   usr             l'utente destinatario del messaggio
 *
 * @return  1 se il messaggio viene inviato, 0 se viene inserito nella coda dell'utente
 */
int user_send(user_t* usr, message_t* msg);

/**
 * @function                user_delete
 * @brief                   cancella un utente
 *
 * @param   usr             l'utente da cancellare
 *
 */
void user_delete(user_t* usr);

/* ------------ Operazioni sui gruppi ------------ */

/**
 * @function                group_init
 * @brief                   crea un nuovo gruppo
 *
 * @param    groupname      il nome del gruppo da creare
 * @param    usr            il creatore del gruppo, che viene aggiunto automaticamente
 *
 * @return   il puntatore al gruppo, NULL in caso di errore
 */
group_t* group_init(char* groupname, char* nickname);

/**
 * @function                group_add
 * @brief                   aggiunge un utente al gruppo
 *
 * @param    group          il gruppo cui aggiungere l'utente
 * @param    usr            l'utente da aggiungere
 *
 */
void group_add(group_t* group, char* nickname);

/**
 * @function                group_remove
 * @brief                   rimuove un utente dal gruppo
 *
 * @param    group          il gruppo da cui rimuovere l'utente
 * @param    nickname       l'identificatore dell'utente da rimuovere
 *
 */
void group_remove(group_t* group, char* nickname);

/**
 * @function                group_delete
 * @brief                   cancella un gruppo
 *
 * @param    group          il gruppo da cancellare
 *
 */
void group_delete(group_t* group);

#endif