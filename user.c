#include <user.h>
#include <stdio.h>
#include <stdlib.h>
#include <connections.h>
#include <queue.h>

/**
 * @file   user.c
 * @brief  Implementazione dell'header user.h.
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/* ------------ Operazioni sugli utenti ------------*/

/**
 * @function user_init
 * @brief                     crea un nuovo utente
 *
 * @param    nick             il nickname dell'utente
 * @param    connfd           il fd di connessione dell'utente
 *
 * @return il puntatore all'utente, NULL in caso di errore
 */
user_t* user_init(char* nick, long connfd, queue_t* tab_pos) {
	user_t* usr = (user_t*) malloc(sizeof(user_t));
	if(usr == NULL) {
		return NULL;
	}

	strcpy(usr->nickname,nick);
	usr->tab_pos = tab_pos;
	usr->connfd = connfd;

	return usr;
}

/**
 * @function user_delete
 * @brief    cancella un utente
 *
 * @param    usr           l'utente da cancellare
 *
 */
void user_delete(user_t* usr) {

	if(usr != NULL) {
		free(usr);
	}

}

/* ------------ Operazioni sui gruppi ------------ */

/**
 * @function group_init
 * @brief    crea un nuovo gruppo
 *
 * @param    groupname      il nome del gruppo da creare
 * @param    nickname       il creatore del gruppo, che viene aggiunto automaticamente
 *
 * @return   il puntatore al gruppo, NULL in caso di errore
 */
group_t* group_init(char* groupname, char* nickname) {

	group_t* group = (group_t*) malloc(sizeof(group_t));
	if(group == NULL) {
		fprintf(stderr,"Impossibile creare gruppo\n");
		return NULL;
	}
	group->ulist = (user_list_t) malloc(sizeof(user_t*));
	strcpy(group->groupname,groupname);
	group->ulist[0] = user_init(nickname,0,NULL);
	if(group->ulist == NULL) {
		fprintf(stderr, "Impossibile creare la lista utenti\n");
		free(group);
		return NULL;
	}
	group->n_memb = 1;

	return group;
}

/**
 * @function group_add
 * @brief    aggiunge un utente al gruppo
 *
 * @param    group          il gruppo cui aggiungere l'utente
 * @param    nick           l'utente da aggiungere
 *
 */
void group_add(group_t* group, char* nick) {

	group->n_memb++;
	//user_list_t tmp = NULL;
	group->ulist = (user_list_t) realloc(group->ulist,(group->n_memb)*sizeof(user_t*));
	if(group->ulist == NULL) {
		fprintf(stderr, "Impossibile aggiungere utente al gruppo\n");
		group->n_memb--;
	}
	group->ulist[(group->n_memb)-1] = user_init(nick,0,NULL);

}

/**
 * @function group_remove
 * @brief    rimuove un utente dal gruppo
 *
 * @param    group          il gruppo da cui rimuovere l'utente
 * @param    nickname       l'identificatore dell'utente da rimuovere
 *
 */
void group_remove(group_t* group, char* nickname) {

	user_list_t tmp = (user_list_t) malloc((group->n_memb)*sizeof(user_t*));
	if(tmp == NULL) {
		fprintf(stderr,"Impossibile rimuovere utente dal gruppo\n");
		return;
	}
			
	for(int i=0; i<group->n_memb; i++) {
		tmp[i] = group->ulist[i];
	}		
	group->n_memb--;
	group->ulist = (user_list_t) realloc(group->ulist,(group->n_memb)*sizeof(user_t*));
	if(group->ulist == NULL) {
		fprintf(stderr,"Impossibile rimuovere utente dal gruppo\n");
		group->n_memb++;
		group->ulist = tmp;
		return;
	}

	for(int i=0; i<group->n_memb; i++)
		if(!strcmp(nickname,tmp[i]->nickname))
			group->ulist[i] = tmp[i];

}

/**
 * @function group_delete
 * @brief    cancella un gruppo
 *
 * @param    group          il gruppo da cancellare
 *
 */
void group_delete(group_t* group) {

	if(group != NULL) {
		free(group->ulist);
		free(group);
	}

}

