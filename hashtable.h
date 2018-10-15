#ifndef HASHT_H
#define HASHT_H

/**
 * @file  hashtable.h
 * @brief Header della struttura dati hashtable.
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/**
 * @struct  hash_entry_s
 * @brief   entry della hashtable
 *
 * @var     key             la chiave della entry
 * @var     data            dato contenuto nella entry
 * @var     next            puntatore all'elemento successivo nel bucket
 */
typedef struct hash_entry_s {
	void* key;
	void* data;
	struct hash_entry_s* next;
} hash_entry_t;

/**
 *  @struct hash_bucket_s
 *  @brief  lista di trabocco della hashtable
 *
 *  @var    head           puntatore alla testa della lista
 *  @var    len            lunghezza del bucket
 */
typedef struct hash_bucket_s {
	hash_entry_t* head;
	int len;
} hash_bucket_t;

/**
 *  @struct hashtable_s
 *  @brief  hashtable ridimensionabile
 *
 *  @var    buckets         buckets della tabella
 *  @var    hash_fun        la funzione di hashing
 *  @var    hash_compare    funzione compare per l'hashing
 *  @var    nbuckets        numero di buckets della tabella
 *  @var    nentries        numero di elementi nella tabella
 */
typedef struct hashtable_s {
	hash_bucket_t** buckets;
	unsigned int (*hash_fun)(void*);
	int (*hash_compare)(void*, void*);
	int nbuckets;
	int nentries;
} hashtable_t;

/* ------ funzioni di utilità ------- */

/**
 * @function hashtable_init
 * @brief    crea una hashtable vuota
 *
 * @example  nuova hashtable contenente 5 buckets
 *
 *  hashtable_t* ht;
 *  ht = hashtable_init(5);
 *
 * @param    buckets        numero di buckets iniziale
 *
 * @return   il puntatore alla hashtable, NULL in caso di errore
 */
hashtable_t* hashtable_init(int buckets,unsigned int (*hash_fun)(void*), int (*hash_key_compare)(void*, void*));

/**
 * @function hash_insert
 * @brief    inserisce un elemento nella hashtable
 *
 * @example  inserisce una stringa nella coda
 *
 *  char* key = "key";
 *  char* data = "data";
 *  insert(ht,key,data);
 *
 * @param    ht              l'hashtable in cui inserire
 * @param    key             la chiave della entry
 * @param    data            dati della entry
 *
 * @return   0 se l'inserimento avviene con successo, -1 in caso di errore
 */
int hash_insert(hashtable_t* ht, void* key, void* data);

/**
 * @function hash_remove
 * @brief    rimuove un elemento dalla tabella
 *
 * @param    ht              la tabella da cui rimuovere l'elemento
 * @param    key             la chiave dell'elemento da rimuovere
 *
 */
void hash_remove(hashtable_t* ht, void* key);

/**
 * @function hash_get
 * @brief    restituisce l'elemento di chiave key
 *
 * @param    ht                  la hashtable in cui cercare l'elemento
 * @param    key                 la chiave dell'elemento da cercare
 *
 * @return   il puntatore all'elemento cercato, NULL se la tabella non contiene tale elemento
 */
void* hash_find(hashtable_t* ht, void* key);

/**
 * @function hash_delete
 * @brief    distrugge la tabella hash, liberando la memoria occupata
 *
 * @param    ht                  la tabella da distruggere
 *
 */
void hash_delete(hashtable_t* ht);

#endif