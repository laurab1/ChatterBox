/*Funziona, no leaks*/
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <config.h>

/**
 * @file config.c
 * @brief Implementazione dell'header config.h
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/*-------------------------UTILITÀ-------------------------*/

//restituisce la stringa c convertita in numero se questa è composta di sole cifre, -1 altrimenti
long isNumber(char* str) {

	char* endp;
	long value;
	int base = 10;
	errno = 0; //come da man, val potrebbe essere LONG_MAX O LONG_MIN anche senza errore
	value = strtol(str, &endp, base);

	if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN)) || (errno != 0 && value == 0)) {
		perror("opt value out of range");
		return -1;
	}

	if (*endp != '\0') { //controllo che il valore nel file sia scritto correttamente
		fprintf(stderr, "chars in c\n");
		return -1;
	}

	return value;

}

/**
 * @function readConfig
 * @brief    Legge il valore di configurazione di una determinata opzione 
 *
 * @param    ifp   file di configurazione chatty.conf 
 * @param    opt   l'opzione da configurare
 *
 * @return   0 se la stringa è stata letta correttamente
 *           -1 se opt non compare nel file o non c'è valore per opt
 */
int readConfig(FILE* ifp, char* opt, char** dest) {

	char* s = NULL;
	char* token = NULL;
	char* value = NULL;
	char* saveptr = NULL;
	char buf[MAX_LINE];
	int len = 0;

	while((s = fgets(buf,MAX_LINE,ifp)) != NULL && value == NULL) {
		if(buf[0] == '#' || strstr(buf,opt) == NULL)
		/*salta i commenti e controlla che l'opzione desiderata non sia nella linea*/
			continue;
		for(int i=0; i<2; i++) {
			token = strtok_r(s,"=",&saveptr);
			s = NULL;
			if(token == NULL)
				break;
		}
		if(token == NULL)
			break;

		len = strlen(token); //la dimensione della stringa, compreso '\n'
		value = (char*) malloc(len*sizeof(char));
		assert(value);

		/*ora dobbiamo ripulire la stringa*/
		int j = 0;
		for(int i=0; i<len; i++)
			if(!isspace(token[i])) {
			/*tolgo spazi, tab e newline*/
				value[j] = token[i];
				j++;
			}

		len = j;
		//sono sicura di avere spazio per almeno len+1 caratteri
		//perché, certamente, c'è almeno '\n'
		//termino la stringa con '\0' e rialloco
		value[len] = '\0';
		value = (char*) realloc(value, (len+1)*sizeof(char));
		if(value == NULL) {
			return -1;
		}
		*dest = value;

		rewind(ifp);

		return 0;
	}
	return -1;
}

/**
 * @function setConfig
 * @brief    inizializza le opzioni di configurazione del server secondo i valori
 * contenuti nel file passato come parametro 
 *
 * @param    conf   la struttura contenente le opzioni di configurazione
 * @param    path   il path del file di configurazione
 *
 * @return   0 se le opzioni sono state configurate con successo
 *           -1 altrimenti
 */
int setConfig(Conf_t* conf, char* path) {

	FILE* ifp;
	char* str;

	if((ifp = fopen(path,"r")) == NULL) {
		perror("cannot open config file");
		return -1;
	}

	//setto prima le opzioni che hanno per valore una stringa
	if(readConfig(ifp,"UnixPath",&conf->UnixPath) == -1) {
		fprintf(stdout,"invalid UnixPath option\n");
		return -1;
	}
	if(readConfig(ifp,"DirName",&conf->DirName) == -1) {
		fprintf(stdout,"invalid DirName option\n");
		return -1;
	}
	if(readConfig(ifp,"StatFileName",&conf->StatFileName) == -1) {
		fprintf(stdout,"invalid StatFileName option\n");

	}
	
	//poi mi occupo di quelli che hanno valore int
	if(readConfig(ifp,"MaxConnections",&str) == -1) {
		fprintf(stdout,"invalid MaxConnections option\n");
		return -1;
	}
	if((conf->MaxConnections = isNumber(str)) == -1) {
		fprintf(stdout,"MaxConnections is not a number\n");
		return -1;
	}
	free(str);
	if(readConfig(ifp,"ThreadsInPool",&str) == -1) {
		fprintf(stdout,"invalid ThreadsInPool option\n");
		return -1;
	}
	if((conf->ThreadsInPool = isNumber(str)) <= 0) {
		fprintf(stdout,"ThreadsInPool is not a valid size\n");
		return -1;
	}
	free(str);
	if(readConfig(ifp,"MaxMsgSize",&str) == -1) {
		fprintf(stdout,"invalid MaxMsgSize option\n");
		return -1;
	}
	if((conf->MaxMsgSize = isNumber(str)) == -1) {
		fprintf(stdout,"MaxMsgSize is not a number\n");
		return -1;
	}
	free(str);
	if(readConfig(ifp,"MaxFileSize",&str) == -1) {
		fprintf(stdout,"invalid MaxFileSize option\n");
		return -1;
	}
	if((conf->MaxFileSize = isNumber(str)) == -1) {
		fprintf(stdout,"MaxFileSize is not a number\n");
		return -1;
	}
	free(str);
	if(readConfig(ifp,"MaxHistMsgs",&str) == -1) {
		fprintf(stdout,"invalid MaxHistMsgs option\n");
		return -1;
	}
	if((conf->MaxHistMsgs = isNumber(str)) == -1) {
		fprintf(stdout,"MaxHistMsgs is not a number\n");
		return -1;
	}
	free(str);
	fclose(ifp);

	return 0;
}

/**
 * @function  eraseConfig
 * @brief     resetta le opzioni di configurazione del server
 *
 * @param     conf   la struttura contenente le opzioni di configurazione
 */
void eraseConfig(Conf_t* conf) {

	free(conf->UnixPath);
	free(conf->DirName);
	free(conf->StatFileName);
	conf->MaxConnections = 0;
	conf->ThreadsInPool = 0;
	conf->MaxMsgSize = 0;
	conf->MaxFileSize = 0;
	conf->MaxHistMsgs = 0;

}

