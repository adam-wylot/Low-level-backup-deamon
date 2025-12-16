#ifndef BACKUPER_H
#define BACKUPER_H

/**
 * @brief Próbuje zainicjalizować tworzenie backup'u.
 *
 * @param src ścieżka do folderu źródłowego.
 * @param dest ścieżka do folderu docelowego backup'u.
 * @return (int)
 * - 0: W przypadku udanej inicjalizacji.
 * - -1: Nieokreślony błąd.
 * - -2: Brak uprawnień.
 * - -3: Podane ścieżki nie były katalogami.
 * - -4: Katalog źródłowy nie istnieje.
 * - -5: Katalog docelowy nie jest pusty.
 * - -6: Próba utworzenia backup'u w katalogu źródłowym.
 */
int make_backup(char *src, char *dest, int watch_only);

/**
 * @brief Wykonuje procedurę przywracania danych (blokująco).
 * 1. Usuwa z src pliki nieistniejące w dest.
 * 2. Kopiuje z dest do src pliki nowsze lub brakujące.
 */
int perform_restore(char *src, char *dest);

#endif /// BACKUPER_H
