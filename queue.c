#include <stdlib.h>
#include <queue.h>
#include <stdio.h>

/**
 * @file  queue.c
 * @brief Implementazione dell'header queue.h (a dimensione limitata).
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/* ------ implementazione interfaccia ------- */

/**
 * @function queue_init
 * @brief crea una coda vuota
 *
 * @param nels numero di elementi ammessi dalla coda
 *
 * @return NULL in caso di errore, il puntatore alla coda altrimenti
 */
queue_t* queue_init(int nels) {

	queue_t* queue = (queue_t*) malloc(sizeof(queue_t));
	if(queue == NULL) {
		fprintf(stderr,"Errore: allocazione coda\n");
		return NULL;

	}
	if((queue->els = (void**) malloc(nels*sizeof(void*))) == NULL) {
		fprintf(stderr,"Errore: allocazione els\n");
		free(queue);
		return NULL;
	}

	queue->max_size = nels;
	queue->hd = queue->dim = 0;

	return queue;

}

/**
 * @function isEmpty
 * @brief verifica se la coda sia vuota
 *
 * @param q la coda da verificare
 *
 * @return 1 se la coda è vuota, 0 altrimenti
 */
int is_empty(queue_t* q) {

	return (q->dim == 0);

}

/**
 * @function push
 * @brief inserisce un elemento in coda
 *
 * @param q la coda da verificare
 * @param el l'elemento da inserire
 *
 * @return 0 se l'inserimento avviene con successo, 1 in caso di coda piena
 */
int push(queue_t* q, void* el) {


	if(q->dim == q->max_size) {
		return 1;
	}

	q->els[(q->hd+q->dim)%q->max_size] = el;
	q->dim++;

	return 0;

}

/**
 * @function pop
 * @brief rimuove un elemento dalla coda
 *
 * @param q la coda da cui estrarre un elemento
 *
 * @return l'elemento estratto, NULL se la coda è vuota
 */
void* pop(queue_t* q) {

	void* el = NULL;

	el = (q->els[q->hd]);
	q->hd = (q->hd+1)%(q->max_size);
	(q->dim)--;

	return el;

}

/**
 * @function destroyQueue
 * @brief distrugge la coda
 *
 * @param q la coda da distruggere
 */
void queue_delete(queue_t* q) {

	if(q == NULL)
		return;

	for(int i=q->hd; i<q->dim; i++)
		if(q->els[i] != NULL)
			free(q->els[i]);
	free(q->els);
	free(q);

}

