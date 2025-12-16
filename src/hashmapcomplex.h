#ifndef HASHMAPCOMPLEX_H
#define HASHMAPCOMPLEX_H

#include "hashmap.h"

#define HASH_TABLE_SIZE 67

// * ============ GŁÓWNA STRUKTURA DANYCH ============
// ? Node
typedef struct hm_c_node {
	char *path; // key
	hashmap_t dests;
	size_t dests_count;
	struct hm_c_node *next;
} *hm_c_node_t;

// ? Struktura przechowująca dane
typedef struct {
	hm_c_node_t nodes[HASH_TABLE_SIZE];
} *hashmap_cmplx_t;

// * ===== Deklaracje funkcji =====
hashmap_cmplx_t hashmap_cmplx_init(void);

void hashmap_cmplx_free(hashmap_cmplx_t *map_c);

/**
 * @brief Dodaje parę ścieżek do struktury danych 1-N.
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param src ścieżka katalogu źródłowego.
 * @param dest ścieżka katalogu docelowego.
 * @param value Wskaźnik do wartości, którą ma przechować.
 * @return (int)
 * - 1: Taka para już istnieje.
 * - 0: W przypadku powodzenia.
 * - -1: Błąd pamięci
 */
int hashmap_cmplx_add(hashmap_cmplx_t map_c, const char *src, const char *dest, const void *value,
					  size_t value_size);

/**
 * @brief Usuwa z mapy parę source-destination.
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param src Ścieżka katalogu źródłowego.
 * @param dest Ścieżka katalogu docelowego.
 * @return (int)
 * - 1: Nie znaleziono.
 * - 0: Znaleziono i usunięto.
 */
int hashmap_cmplx_remove(hashmap_cmplx_t map_c, const char *src, const char *dest);

/**
 * @brief Sprawdza, czy w mapie istnieje para source-destination.
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param src Ścieżka katalogu źródłowego.
 * @param dest Ścieżka katalogu docelowego.
 * @return (int)
 * - 1: Znaleziono.
 * - 0: Nie znaleziono.
 */
int hashmap_cmplx_contains(hashmap_cmplx_t map_c, const char *src, const char *dest);

/**
 * @brief Zwraca value przypisanego do pary src-dest.
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param src ścieżka katalogu źródłowego.
 * @param dest ścieżka katalogu docelowego.
 * @return (void*)
 * - void*: Pobrana wartość.
 * - NULL: Nie znaleziono takiej pary src-dest.
 */
void *hashmap_cmplx_getval(hashmap_cmplx_t map_c, const char *src, const char *dest);

/**
 * @brief Zwraca parę source-destination dla podanego Value
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param value Wartość wobec której będzie szukana para.
 * @param value_size Wielkość zmiennej value.
 * @param src_p Wskaźnik do string'a, który zostanie nadpisany.
 * @param dest_p Wskaźnik do string'a, który zostanie nadpisany.
 * @return (int)
 * - 0: Znaleziono parę.
 * - -1: Nie znaleziono pary.
 */
int hashmap_cmplx_get_pair(hashmap_cmplx_t map_c, const void *value, size_t value_size, char **src_p, char **dest_p);

/**
 * @brief Sprawdza, czy destinaton path jest gdzieś już używany.
 *
 * @param hashmap_cmplx Obiekt HashMap'y.
 * @param dest Ścieżka do katalogu docelowego.
 * @return (int)
 * - 1: Znaleziono gdzieś dest.
 * - 0: Nie znaleziono.
 */
int hashmap_cmplx_is_dest_used(hashmap_cmplx_t map_c, const char *dest);

#endif /// HASHMAPCOMPLEX_H
