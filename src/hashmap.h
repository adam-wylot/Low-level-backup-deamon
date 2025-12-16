#ifndef HASHMAP_H
#define HASHMAP_H

#include <sys/types.h>

#define HASH_TABLE_SIZE 67

typedef struct destNode {
	char *path; // key
	void *value; // value
	struct destNode *next;
} *hm_node_t;

// ? Struktura przechowująca dane
typedef struct {
	hm_node_t nodes[HASH_TABLE_SIZE];
} *hashmap_t;


size_t hash_djb2(const char *str);

hm_node_t hm_node_init(const char *key, const void *value, size_t value_size);

void hm_node_free(hm_node_t *node);


hashmap_t hashmap_init(void);

void hashmap_free(hashmap_t *map);

/**
 * @brief Sprawdza, czy wpis dla podanego klucza istnieje.
 *
 * @param map Obiekt HashMap'y.
 * @param key Klucz.
 * @return (int)
 * - 1: Znaleziono.
 * - 0: Nie znaleziono.
 */
int hashmap_contains(hashmap_t map, const char *key);

/**
 * @brief Dodaje wpis key - value.
 *
 * @param key Klucz.
 * @param value Wartość.
 * @return (int)
 * - 1: Element z takim kluczem już istnieje.
 * - 0: Pomyślnie dodano.
 * - -1: Błąd krytyczny.
 */
int hashmap_add(hashmap_t map, const char *key, const void *value, size_t value_size);

/**
 * @brief Usuwa wpis dla danego klucza.
 *
 * @param map Obiekt HashMap'y.
 * @param key Klucz węzła do usunięcia.
 * @return (int)
 * - 1: Nie znaleziono.
 * - 0: Pomyślnie usunięto.
 */
int hashmap_remove(hashmap_t map, const char *key);

/**
 * @brief Zwraca value dla podanego klucza.
 *
 * @param map Obiekt HashMap'y.
 * @param key Ścieżka, dla której ma znaleźć value.
 * @return (void*)
 * - NULL: Nie znaleziono.
 * - w.p.p.: value.
 */
void *hashmap_getval(hashmap_t map, const char *key);

/**
 * @brief Zwraca ścieżke (key) dla podanego value.
 *
 * @param map Obiekt HashMap'y.
 * @param value Wartość, dla której będzie szukana ścieżka (key).
 * @param value_size Rozmiar zmiennej value.
 * @return (char*)
 * - NULL: Nie znaleziono.
 * - w.p.p.: Ścieżka (key).
 */
char *hashmap_get_path(hashmap_t map, const void *value, size_t value_size);

#endif /// HASHMAP_H
