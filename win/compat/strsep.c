/*
 * win/compat/strsep.c -- strsep() for platforms that lack it.
 *
 * MinGW-w64 provides neither a declaration nor an implementation of strsep(),
 * which fern's sources use to split strings.  Supplying it here keeps fern
 * free of Windows #ifdefs.
 *
 * This is the 4.4BSD implementation, with the usual strsep() semantics: the
 * token is NUL-terminated in place and *stringp is advanced past the
 * delimiter.
 */
#ifdef _WIN32

#include <string.h>

char *
strsep(char **stringp, const char *delim)
{
    char *s;
    char *token;

    if (stringp == NULL || (s = *stringp) == NULL) {
        return NULL;
    }

    token = s;
    while (*s != '\0') {
        const char *d;

        for (d = delim; *d != '\0'; d++) {
            if (*s == *d) {
                *s = '\0';
                *stringp = s + 1;
                return token;
            }
        }
        s++;
    }

    *stringp = NULL;
    return token;
}

#endif /* _WIN32 */
