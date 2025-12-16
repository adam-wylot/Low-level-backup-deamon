#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <sys/stat.h>
#include <ftw.h>

/**
 * @brief Sprawdza, czy folder istnieje.
 *
 * @param path ścieżka do katalogu.
 * @return (int)
 * - 1: Katalog istnieje.
 * - 0: Katalog NIE istnieje.
 * - -1: Nieokreślony błąd.
 * - -2: Brak uprawnień.
 * - -3: Podana ścieżka nie prowadzi do katalogu.
 */
int dir_exists(const char *path);

/**
 * @brief Rekursywnie tworzy brakujące katalogi w podanej ścieżce.
 *
 * @param path ścieżka.
 * @param mode uprawnienia do tworzonego folderu.
 * @return (int)
 * - 0: W przypadku powodzenia.
 * - -1: Nieokreślony błąd.
 * - -2: Brak uprawnień.
 */
int mkdir_recursive(const char *path, mode_t mode);

/**
 * @brief Sprawdza, czy podany katalog jest pusty.
 *
 * @param path Ścieżka do katalogu.
 * @return (int)
 * - 1: Jeśli katalog jest pusty.
 * - 0: Katalog nie jest pusty.
 * - -1: Nieokreślony błąd (errno).
 * - -2: Brak uprawnień.
 */
int is_dir_empty(const char *path);

/**
 * @brief Sprawdza, czy drugi podany katalog leży w pierwszym.
 *
 * @param parent_path Ścieżka do katalogu podejżanego rodzica.
 * @param path Ścieżka do podejżanego podkatalogu
 * @return (int)
 * - 1: Jeśt path leży w parent_path.
 * - 0: w.p.p.
 */
int is_subdirectory(const char *parent_path, const char *path);


/**
 * @brief Kopiuje plik z ścieżki src do ścieżki dest.
 *
 * @param src Ścieżka do pliku do przekopiowania.
 * @param dest Ścieżka docelowa.
 * @return (int)
 * - 0: Sukces.
 * - -1: Nieokreślony błąd.
 */
int copy_file(const char *src, const char *dest);

int remove_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);

#endif /// FILEHANDLER_H
