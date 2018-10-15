#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hashtable.h>

/**
 * @file  hashtable.c
 * @brief Implementazione dell'header hashtable.h.
 * @author Laura Bussi 521927
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore
 */

/**
 * @function hash_entry_init
 * @brief    inizializza un elemento della tabella hash
 *
 * @return   l'elemento inizializzato, NULL in caso di errore
 */
hash_entry_t* hash_entry_init(void* key, void* data) {

	hash_entry_t* entry = (hash_entry_t*) malloc(sizeof(hash_entry_t));
	if(entry == NULL) {
		fprintf(stderr,"errore nell'allocazione della entry\n");
		return NULL;
	}

	entry->key = malloc(33*sizeof(char));
	strcpy(entry->key,key);
	entry->data = data;
	entry->next = NULL;

	return entry;

}

/**
 * @function hash_entry_delete
 * @brief    libera la memoria occupata dalla entry
 *
 * @param    entry          l'elemento da liberare
 *
 */
void hash_entry_delete(hash_entry_t* entry) {

	if(entry != NULL) {
		free(entry->key);
		free(entry);
	}

}

/**
 * @function hash_bucket_init
 * @brief    inizializza un bucket della tabella
 *
 * @return   il puntatore al bucket, NULL in caso di errore
 */
hash_bucket_t* hash_bucket_init() {

	hash_bucket_t* bucket = (hash_bucket_t*) malloc(sizeof(hash_bucket_t));
	if(bucket == NULL) {
		fprintf(stdout,"errore nell'allocazione del bucket\n");
		return NULL;
	}

	bucket->head = NULL;
	bucket->len = 0;

	return bucket;

}

/**
 * @function hash_bucket_delete
 * @brief    libera la memoria occupata dal bucket
 *
 * @param    bucket        il bucket da distruggere
 *
 */
void hash_bucket_delete(hash_bucket_t* bucket) {

	if(bucket == NULL)
		return;

	hash_entry_t* curr = bucket->head;
	hash_entry_t* prev = NULL;
	while(curr != NULL) {
		prev = curr;
		curr = curr->next;
		hash_entry_delete(prev);
	}

	free(bucket);
}

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
hashtable_t* hashtable_init(int buckets,unsigned int (*hash_fun)(void*),int (*hash_compare)(void*, void*)) {

	hashtable_t* ht = (hashtable_t*) malloc(sizeof(hashtable_t));
	if(ht == NULL) {
		fprintf(stderr,"errore nell'allocazione della tabella hash\n");
		return NULL;
	}
	ht->buckets = (hash_bucket_t**) malloc(buckets*sizeof(hash_bucket_t*));
	if(ht->buckets == NULL) {
		fprintf(stderr,"errore nell'allocazione dei buckets\n");
		free(ht);
		return NULL;
	}
	for(int i=0; i<buckets; i++) {
		ht->buckets[i] = hash_bucket_init();
		if(ht->buckets[i] == NULL) {
			free(ht->buckets);
			free(ht);
			return NULL;
		}
	}

	ht->nbuckets = buckets;
	ht->nentries = 0;
	ht->hash_fun = hash_fun;
	ht->hash_compare = hash_compare;

	return ht;

}

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
 * @return   0 se l'inserimento ha successo, -1 in caso di errore
 */
int hash_insert(hashtable_t* ht, void* key, void* data) {

	hash_entry_t* curr = NULL;
	hash_entry_t* prev = NULL;
	unsigned int hash_val = 0;

	hash_val = (* ht->hash_fun)(key) % ht->nbuckets;


	if(ht->buckets[hash_val]->head == NULL)
		ht->buckets[hash_val]->head = hash_entry_init(key,data);
	else {
		curr = ht->buckets[hash_val]->head;
		while(curr != NULL) {
			if(ht->hash_compare(curr->key, key)) {
       	   			return -1;
			}
			prev = curr;
			curr = curr->next;
		}

		curr = hash_entry_init(key,data);
		prev->next = curr;
	}
	ht->nentries++;

	return 0;

}

/**
 * @function hash_remove
 * @brief    rimuove un elemento dalla tabella
 *
 * @param    ht              la tabella da cui rimuovere l'elemento
 * @param    key             la chiave dell'elemento da rimuovere
 *
 */
void hash_remove(hashtable_t* ht, void* key) {

	unsigned int hash_val = (* ht->hash_fun)(key) % ht->nbuckets;

	int found = 0;
	hash_entry_t* tmp = NULL;
	hash_entry_t* prev = NULL;
	hash_entry_t* curr = ht->buckets[hash_val]->head;
	while(curr != NULL) {
		if(ht->hash_compare(curr->key, key)) {
			found = 1;
			if(prev == NULL) {
				tmp = ht->buckets[hash_val]->head;
				ht->buckets[hash_val]->head = ht->buckets[hash_val]->head->next;
			} else {
				tmp = curr;
           			prev->next = curr->next;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	if(found) {
		hash_entry_delete(tmp);
		ht->nentries--;
	}

}

/**
 * @function hash_find
 * @brief    restituisce l'elemento di chiave key
 *
 * @param    ht                  la hashtable in cui cercare l'elemento
 * @param    key                 la chiave dell'elemento da cercare
 *
 * @return   il puntatore all'elemento cercato, NULL se la tabella non contiene tale elemento
 */
void* hash_find(hashtable_t* ht, void* key) {

	hash_entry_t* curr = NULL;
	unsigned int hash_val = 0;
	hash_val = (* ht->hash_fun)(key) % ht->nbuckets;
	curr = ht->buckets[hash_val]->head;
	while(curr != NULL) {
		if(ht->hash_compare(curr->key, key)) {
			return curr->data;
		}
		curr = curr->next;
	}

	return NULL;

}

/**
 * @function hash_delete
 * @brief    distrugge la tabella hash, liberando la memoria occupata
 *
 * @param    ht                  la tabella da distruggere
 *
 */
void hash_delete(hashtable_t* ht) {

	for(int i=0; i<ht->nbuckets; i++)
		hash_bucket_delete(ht->buckets[i]);

	free(ht->buckets);
	free(ht);

}
