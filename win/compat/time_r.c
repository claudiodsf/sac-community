/*
 * win/compat/time_r.c -- gmtime_r()/localtime_r() for Windows.
 *
 * The Microsoft CRT provides only the non-reentrant gmtime()/localtime(), and
 * MinGW does not add the _r variants.  sacio/time64.c calls both directly, so
 * provide them here instead of patching the vendored file.
 *
 * These wrap the non-reentrant CRT functions.  SAC only calls them from its
 * single-threaded start-up and formatting paths, so the lack of true
 * thread-safety is not a concern here.
 */
#ifdef _WIN32

#include <time.h>

struct tm *
gmtime_r(const time_t *timep, struct tm *result)
{
    struct tm *tmp;

    if (timep == NULL || result == NULL) {
        return NULL;
    }

    tmp = gmtime(timep);
    if (tmp == NULL) {
        return NULL;
    }

    *result = *tmp;
    return result;
}

struct tm *
localtime_r(const time_t *timep, struct tm *result)
{
    struct tm *tmp;

    if (timep == NULL || result == NULL) {
        return NULL;
    }

    tmp = localtime(timep);
    if (tmp == NULL) {
        return NULL;
    }

    *result = *tmp;
    return result;
}

#endif /* _WIN32 */
