/*
 * win/compat/glob.h -- minimal glob() for MinGW.
 *
 * MinGW-w64 ships no <glob.h>.  src/icm/polezero.c and src/icm/libpz.c use it
 * to expand the "SAC_PZs_<net>_<sta>_<loc>_<chan>_*" file patterns, so the
 * build puts win/compat ahead of the system include path and provides this
 * small implementation on top of FindFirstFile().  Only the subset of the API
 * that SAC uses is supported, which is why the flags are accepted but ignored:
 * FindFirstFile() already does the wildcard matching.
 */
#ifndef SAC_COMPAT_GLOB_H
#define SAC_COMPAT_GLOB_H

#ifdef _WIN32

#include <stddef.h>

typedef struct {
    size_t gl_pathc;    /* number of matched paths */
    char **gl_pathv;    /* NULL-terminated vector of matched paths */
    size_t gl_offs;     /* unused, present for source compatibility */
} glob_t;

/* Flags: accepted for compatibility, all ignored. */
#define GLOB_ERR        (1 << 0)
#define GLOB_MARK       (1 << 1)
#define GLOB_NOSORT     (1 << 2)
#define GLOB_DOOFFS     (1 << 3)
#define GLOB_NOCHECK    (1 << 4)
#define GLOB_APPEND     (1 << 5)
#define GLOB_NOESCAPE   (1 << 6)

/* Return values, matching POSIX. */
#define GLOB_NOSPACE    1
#define GLOB_ABORTED    2
#define GLOB_NOMATCH    3

int  glob(const char *pattern, int flags,
          int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);
void globfree(glob_t *pglob);

#endif /* _WIN32 */

#endif /* SAC_COMPAT_GLOB_H */
