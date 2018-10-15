#ifndef QUEUE_H
#define QUEUE_H

/**
 * @file  queue.h
 * @brief Header della struttura dati coda (a dimensione limitata).
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/**
 *  @struct queue_s
 *  @brief  coda condivisa
 *
 *  @var    els               array di elementi della coda
 *  @var    hd                testa della coda
 *  @var    dim               numero di elementi nella coda
 *  @var    max_size          numero massimo di elementi ammesso
 */
typedef struct queue_s {
	void** els;
	int hd;
	int dim;
	int max_size;
} queue_t;

/* ------ funzioni di utilità ------- */

/**
 * @function queue_init
 * @brief    crea una coda vuota
 *
 * @example  nuova coda di 10 elementi
 *
 *  queue_t* q;
 *  q = queue_init(10);
 *
 * @param    nels              numero di elementi ammessi dalla coda
 *
 * @return   il puntatore alla coda, NULL in caso di errore
 */
queue_t* queue_init(int nels);

/**
 * @function is_empty
 * @brief    verifica se la coda sia vuota
 *
 * @param    q                 la coda da verificare
 *
 * @return   1 se la coda è vuota, 0 altrimenti
 */
int is_empty(queue_t* q);

/**
 * @function push
 * @brief    inserisce un elemento in coda
 *
 * @example  inserisce un (puntatore a) intero nella coda
 *
 *  int x = 8;
 *  push(q,&x);
 *
 * @param    q                  la coda in cui inserire l'elemento
 * @param    el 		l'elemento da inserire
 *
 * @return   0 se l'inserimento avviene con successo, 1 in caso di coda piena
 */
int push(queue_t* q, void* el);

/**
 * @function pop
 * @brief    rimuove un elemento dalla coda
 *
 * @example  estrae un elemento da una coda di interi
 *
 *  int* x;
 *  x = pop(q);
 *
 * @param    q                  la coda da cui estrarre un elemento
 *
 * @return   l'elemento estratto
 */
void* pop(queue_t* q);

/**
 * @function queue_delete
 * @brief    distrugge la coda
 *
 * @param    q                  la coda da distruggere
 */
void queue_delete(queue_t* q);

#endif