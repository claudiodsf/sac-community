/*
 * win/compat/glob.c -- the implementation behind win/compat/glob.h.
 */
#ifdef _WIN32

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include <glob.h>

/* Number of entries the vector grows by. */
#define GLOB_CHUNK 16

/*
 * Split a pattern into its directory part (including the trailing separator,
 * if any) and its final component.  FindFirstFile() matches the final
 * component, and the directory part is prepended to each hit.
 */
static void
split_pattern(const char *pattern, char **dir_out, const char **file_out)
{
    const char *sep = NULL;
    const char *p;
    size_t dir_len;

    for (p = pattern; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            sep = p;
        }
    }

    if (sep == NULL) {
        *dir_out = NULL;
        *file_out = pattern;
        return;
    }

    dir_len = (size_t) (sep - pattern) + 1;
    *dir_out = (char *) malloc(dir_len + 1);
    if (*dir_out == NULL) {
        *file_out = pattern;
        return;
    }
    memcpy(*dir_out, pattern, dir_len);
    (*dir_out)[dir_len] = '\0';
    *file_out = sep + 1;
}

int
glob(const char *pattern, int flags,
     int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char *dir;
    const char *file;
    char **vec;
    size_t cap;
    size_t n;

    (void) flags;

    if (pglob == NULL) {
        return GLOB_NOSPACE;
    }

    pglob->gl_pathc = 0;
    pglob->gl_pathv = NULL;
    pglob->gl_offs = 0;

    if (pattern == NULL || *pattern == '\0') {
        return GLOB_NOMATCH;
    }

    dir = NULL;
    split_pattern(pattern, &dir, &file);

    vec = (char **) malloc(GLOB_CHUNK * sizeof(char *));
    if (vec == NULL) {
        free(dir);
        return GLOB_NOSPACE;
    }
    cap = GLOB_CHUNK;
    n = 0;

    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        free(vec);
        free(dir);
        if (errfunc != NULL) {
            errfunc(pattern, (int) GetLastError());
        }
        return GLOB_NOMATCH;
    }

    do {
        char *full;
        size_t len;

        /* FindFirstFile() never returns these, but be explicit. */
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }

        len = (dir ? strlen(dir) : 0) + strlen(fd.cFileName) + 1;
        full = (char *) malloc(len);
        if (full == NULL) {
            break;
        }
        if (dir != NULL) {
            strcpy(full, dir);
            strcat(full, fd.cFileName);
        } else {
            strcpy(full, fd.cFileName);
        }

        if (n + 2 > cap) {
            char **grown = (char **) realloc(vec, (cap + GLOB_CHUNK) * sizeof(char *));
            if (grown == NULL) {
                free(full);
                break;
            }
            vec = grown;
            cap += GLOB_CHUNK;
        }
        vec[n++] = full;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    free(dir);

    if (n == 0) {
        free(vec);
        return GLOB_NOMATCH;
    }

    vec[n] = NULL;
    pglob->gl_pathc = n;
    pglob->gl_pathv = vec;
    return 0;
}

void
globfree(glob_t *pglob)
{
    size_t i;

    if (pglob == NULL || pglob->gl_pathv == NULL) {
        return;
    }

    for (i = 0; i < pglob->gl_pathc; i++) {
        free(pglob->gl_pathv[i]);
    }
    free(pglob->gl_pathv);

    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}

#endif /* _WIN32 */
