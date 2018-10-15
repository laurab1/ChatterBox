/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 *        e funzioni per la configurazione del server
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

#if !defined(CONFIG_H_)

#define CONFIG_H_

#define MAX_NAME_LENGTH                  32
#define MAX_LINE                         1024
#define TAB_DIM				 1024

#include <stdio.h>

/**
 *  @struct config
 *  @brief configurazioni del server 
 *
 */
typedef struct config {
	char* UnixPath;			//path utilizzato per la creazione del socket AF_UNIX
	int MaxConnections;             //numero massimo di connessioni pendenti
	int ThreadsInPool;              //numero di thread nel pool
	int MaxMsgSize;                 //dimensione massima di un messaggio testuale (numero di caratteri)
	int MaxFileSize;                //dimensione massima di un file accettato dal server (kilobytes)
	int MaxHistMsgs;                //numero massimo di messaggi che il server 'ricorda' per ogni client
	char* DirName;                  //directory dove memorizzare i files da inviare agli utenti
	char* StatFileName;             //file nel quale verranno scritte le statistiche del server
} Conf_t;

/**
 * @function  readConfig
 * @brief     Legge il valore di configurazione di una determinata opzione 
 *
 * @param     ifp   file di configurazione chatty.conf 
 * @param     opt   l'opzione da configurare
 *
 * @return    la stringa contenente il valore di configurazione
 *            NULL se opt non compare nel file o non c'è valore per opt
 */
int readConfig(FILE* ifp, char* opt, char** dest);

/**
 * @function  setConfig
 * @brief     inizializza le opzioni di configurazione del server secondo i valori
 *            contenuti nel file passato come parametro 
 *
 * @param     conf   la struttura contenente le opzioni di configurazione
 * @param     path   il path del file di configurazione
 *
 * @return    0 se le opzioni sono state configurate con successo
 *            -1 altrimenti
 */
int setConfig(Conf_t* conf, char* path);

/**
 * @function  eraseConfig
 * @brief     resetta le opzioni di configurazione del server
 *
 * @param     conf la struttura contenente le opzioni di configurazione
 */
void eraseConfig(Conf_t* conf);

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
