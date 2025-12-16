#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "hashmapcomplex.h"
#include "hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// * =============== SOURCE ===============
// ? --------------- Node ---------------
hm_c_node_t hm_c_node_init(const char *path) {
	hm_c_node_t node = (hm_c_node_t) malloc(sizeof *node);
	if (node == NULL) {
		return NULL;
	}

	node->dests = hashmap_init();
	if (node->dests == NULL) {
		free(node);
		return NULL;
	}

	size_t len = strlen(path);
	node->path = (char *) malloc((len + 1) * sizeof(char));
	if (node->path == NULL) {
		hashmap_free(&node->dests);
		free(node);
		return NULL;
	}
	memcpy(node->path, path, len + 1);

	node->dests_count = 0;
	node->next = NULL;

	return node;
}

void hm_c_node_free(hm_c_node_t *node) {
	if (node == NULL || *node == NULL) {
		return;
	}

	hashmap_free(&(*node)->dests);
	free((*node)->path);
	free(*node);
	*node = NULL;
}


// ? --------------- Tablica ---------------
hashmap_cmplx_t hashmap_cmplx_init(void) {
	hashmap_cmplx_t ht = (hashmap_cmplx_t) calloc(1, sizeof *ht);
	if (ht == NULL) {
		return NULL;
	}

	return ht;
}

void hashmap_cmplx_free(hashmap_cmplx_t *map_c) {
	if (map_c == NULL || *map_c == NULL) {
		return;
	}

	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_c_node_t current = (*map_c)->nodes[i];
		while (current != NULL) {
			hm_c_node_t next = current->next;
			hm_c_node_free(&current);
			current = next;
		}
	}

	free(*map_c);
	*map_c = NULL;
}

int hashmap_cmplx_contains(hashmap_cmplx_t map_c, const char *src, const char *dest) {
	size_t index = hash_djb2(src) % HASH_TABLE_SIZE;
	hm_c_node_t current = map_c->nodes[index];

	while (current != NULL) {
		if (strcmp(current->path, src) == 0) {
			// znaleziono source — przystępuję do szukania dest
			return hashmap_contains(current->dests, dest);
		}
		current = current->next;
	}

	return 0;
}

int hashmap_cmplx_add(hashmap_cmplx_t map_c, const char *src, const char *dest, const void *value,
					  size_t value_size) {
	if (hashmap_cmplx_contains(map_c, src, dest)) {
		// ! Powtarzająca się para
		return 1;
	}

	if (hashmap_cmplx_is_dest_used(map_c, dest)) {
		// ! Powtarzający się element dest
		return 1;
	}

	size_t index = hash_djb2(src) % HASH_TABLE_SIZE;

	// sprawdzenie, czy już nie ma
	hm_c_node_t current = map_c->nodes[index];

	while (current != NULL) {
		if (strcmp(current->path, src) == 0) {
			// ? znaleziono source
			if (hashmap_add(current->dests, dest, value, value_size) == -1) {
				// ! Błąd pamięci
				return -1;
			}

			++current->dests_count;
			return 0;
		}
		current = current->next;
	}

	// ? nie znaleziono source — inicjalizacja nowego elementu
	hm_c_node_t new_node = hm_c_node_init(src);
	if (new_node == NULL) {
		// ! Błąd pamięci
		return -1;
	}

	if (hashmap_add(new_node->dests, dest, value, value_size) == -1) {
		// ! Błąd pamięci
		hm_c_node_free(&new_node);
		return -1;
	}
	++new_node->dests_count;

	new_node->next = map_c->nodes[index];
	map_c->nodes[index] = new_node;

	return 0;
}

int hashmap_cmplx_remove(hashmap_cmplx_t map_c, const char *src, const char *dest) {
	size_t index = hash_djb2(src) % HASH_TABLE_SIZE;
	hm_c_node_t current = map_c->nodes[index];
	hm_c_node_t prev = NULL;

	while (current != NULL) {
		if (strcmp(current->path, src) == 0) {
			if (hashmap_remove(current->dests, dest) == 1) {
				// ! nie znaleziono dest
				return 1;
			}
			--current->dests_count;

			if (current->dests_count == 0) {
				// nie ma więcej elementów — usuwam
				if (prev == NULL) {
					// pierwszy element
					map_c->nodes[index] = current->next;
				} else {
					// inny element
					prev->next = current->next;
				}

				hm_c_node_free(&current);
			}

			return 0;
		}

		prev = current;
		current = current->next;
	}

	// ! Brak elementu do usunięcia
	return 1;
}

void *hashmap_cmplx_getval(hashmap_cmplx_t map_c, const char *src, const char *dest) {
	size_t index_src = hash_djb2(src) % HASH_TABLE_SIZE;
	hm_c_node_t current_src = map_c->nodes[index_src];

	while (current_src != NULL) {
		if (strcmp(current_src->path, src) == 0) {
			// znaleziono source — przystępuję do szukania dest
			return hashmap_getval(current_src->dests, dest);
		}
		current_src = current_src->next;
	}

	// ! Nie znaleziono
	return NULL;
}

int hashmap_cmplx_get_pair(hashmap_cmplx_t map, const void *value, size_t value_size, char **src_p, char **dest_p) {
	if (map == NULL || value == NULL) {
		return -1;
	}

	// iteracja po wszystkich source
	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_c_node_t src_node = map->nodes[i];

		while (src_node != NULL) {
			char *dest_path = hashmap_get_path(src_node->dests, value, value_size);

			if (dest_path != NULL) {
				// Znaleziono pasującą wartość
				if (src_p != NULL) {
					*src_p = src_node->path;
				}
				if (dest_p != NULL) {
					*dest_p = dest_path;
				}
				return 0;
			}

			src_node = src_node->next;
		}
	}

	// ! Nie znaleziono pary dla podanego value
	return -1;
}

int hashmap_cmplx_is_dest_used(hashmap_cmplx_t map_c, const char *dest) {
	if (map_c == NULL || dest == NULL) {
		return 0;
	}

	// ? Iteracja po wszystkich źródłach
	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_c_node_t current_src = map_c->nodes[i];

		while (current_src != NULL) {
			// dla każdego źródła sprawdzamy, czy w jego mapie istnieje podany dest
			if (hashmap_contains(current_src->dests, dest)) {
				return 1;
			}
			current_src = current_src->next;
		}
	}

	return 0;
}
