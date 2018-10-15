/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file   listener.h
 * @brief  File contenente la dichiarazione della funzione che gestisce
 *         l'accettazione di nuove connessioni da parte di Chatty
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

#if !defined(LISTENER_H_)

#define LISTENER_H_

#include <global.h>

/**
 * @function listener    thread che si occupa dell'ascolto delle connessioni entranti
 */
void* listener(void* arg);

#endif