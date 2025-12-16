#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ? Dodatkowe
size_t hash_djb2(const char *str) {
	size_t hash = 5381;
	int c;
	while ((c = *str++)) {
		hash = (hash << 5) + hash + (unsigned char) c; // hash * 33 + c
	}
	return hash;
}


// ? --------------- Node ---------------
hm_node_t hm_node_init(const char *key, const void *value, size_t value_size) {
	hm_node_t node = (hm_node_t) malloc(sizeof *node);
	if (!node) return NULL;

	node->path = strdup(key);
	if (!node->path) {
		free(node);
		return NULL;
	}

	node->value = malloc(value_size);
	if (!node->value) {
		free(node->path);
		free(node);
		return NULL;
	}

	memcpy(node->value, value, value_size);
	node->next = NULL;

	return node;
}

void hm_node_free(hm_node_t *node) {
	if (node == NULL || *node == NULL) {
		return;
	}

	free((*node)->path);
	free((*node)->value);
	free(*node);
	*node = NULL;
}


// ? --------------- Tablica ---------------
hashmap_t hashmap_init(void) {
	hashmap_t hm = (hashmap_t) calloc(1, sizeof *hm);
	if (hm == NULL) {
		return NULL;
	}

	return hm;
}

void hashmap_free(hashmap_t *hashmap) {
	if (hashmap == NULL || *hashmap == NULL) {
		return;
	}

	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_node_t current = (*hashmap)->nodes[i];
		while (current != NULL) {
			hm_node_t next = current->next;
			hm_node_free(&current);
			current = next;
		}
	}

	free(*hashmap);
	*hashmap = NULL;
}

int hashmap_contains(hashmap_t hashmap, const char *key) {
	size_t index = hash_djb2(key) % HASH_TABLE_SIZE;
	hm_node_t current = hashmap->nodes[index];

	while (current != NULL) {
		if (strcmp(current->path, key) == 0) {
			return 1;
		}
		current = current->next;
	}

	return 0;
}

int hashmap_add(hashmap_t hashmap, const char *key, const void *value, size_t value_size) {
	if (hashmap_contains(hashmap, key)) {
		// ! Powtarzający się element
		return 1;
	}

	size_t index = hash_djb2(key) % HASH_TABLE_SIZE;

	hm_node_t new_node = hm_node_init(key, value, value_size);
	if (new_node == NULL) {
		// ! Błąd pamięci
		return -1;
	}

	new_node->next = hashmap->nodes[index];
	hashmap->nodes[index] = new_node;

	return 0;
}

int hashmap_remove(hashmap_t hashmap, const char *key) {
	size_t index = hash_djb2(key) % HASH_TABLE_SIZE;
	hm_node_t current = hashmap->nodes[index];
	hm_node_t prev = NULL;

	while (current != NULL) {
		if (strcmp(current->path, key) == 0) {
			if (prev == NULL) {
				// pierwszy element
				hashmap->nodes[index] = current->next;
			} else {
				// inny element
				prev->next = current->next;
			}

			hm_node_free(&current);
			return 0;
		}

		prev = current;
		current = current->next;
	}

	// ! Brak elementu do usunięcia
	return 1;
}

void *hashmap_getval(hashmap_t hashmap, const char *key) {
	size_t index = hash_djb2(key) % HASH_TABLE_SIZE;
	hm_node_t current = hashmap->nodes[index];

	while (current != NULL) {
		if (strcmp(current->path, key) == 0) {
			// znaleziono
			return current->value;
		}
		current = current->next;
	}

	return NULL;
}

char *hashmap_get_path(hashmap_t map, const void *value, size_t value_size) {
	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_node_t node = map->nodes[i];

		while (node != NULL) {
			if (node->value != NULL) {
				if (memcmp(node->value, value, value_size) == 0) {
					return node->path;
				}
			}
			node = node->next;
		}
	}

	return NULL;
}
