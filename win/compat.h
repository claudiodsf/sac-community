/*
 * win/compat.h -- Windows compatibility shim for third-party sources.
 *
 * The CMake build force-includes this header into every translation unit
 * (see CMakeLists.txt).  It exists so that the vendored libraries in this
 * repository -- evalresp (including its bundled mxml), libmseed, sacio's
 * time64 and linenoise -- do not have to carry Windows-specific #ifdefs of
 * their own, which would be lost the next time they are updated from
 * upstream.
 *
 * Keep this file small and limited to shims for vendored code.  Fixes for
 * SAC's own sources belong in those sources.
 */
#ifndef SAC_WIN_COMPAT_H
#define SAC_WIN_COMPAT_H

#ifdef _WIN32

/* ---------------------------------------------------------------------------
 * sacio/time64.h
 *
 * That header hard-codes the POSIX capabilities it was generated with:
 * HAS_GMTIME_R, HAS_LOCALTIME_R, HAS_TM_TM_GMTOFF and HAS_TM_TM_ZONE.  None of
 * them hold on Windows, where struct tm has no tm_gmtoff/tm_zone members and
 * the CRT provides no gmtime_r/localtime_r, so time64.c would not compile.
 *
 * The header is vendored, so rather than patch it we include it here and then
 * undefine the capabilities Windows lacks.  time64.h has its own include
 * guard, so the later #include inside time64.c becomes a no-op and these
 * undefinitions stick.  time64.c then falls back to its bundled
 * fake_gmtime_r()/fake_localtime_r() implementations.
 *
 * INT_64_T is defined as int64_t, hence <stdint.h> first.
 * ------------------------------------------------------------------------ */
#include <stdint.h>
#include <inttypes.h>   /* PRId64, used by time64.h's TM64_ASCTIME_FORMAT */

/* MinGW's PRId64 expands to __PRI64_PREFIX, which is not defined when the
 * header is compiled as C++, so the macro exists but expands to garbage that
 * time64.h cannot concatenate into a format string.  Supply a literal instead
 * (int64_t is `long long` on Windows) so that time64.h's own #ifndef guard
 * leaves it alone. */
#undef PRId64
#define PRId64 "lld"

#include "time64.h"

/* struct tm has no tm_gmtoff/tm_zone on Windows, and unlike the missing
 * functions those members cannot be shimmed, so disable them and let
 * time64.c skip the code that touches them. */
#undef HAS_TM_TM_GMTOFF
#undef HAS_TM_TM_ZONE

/* The CRT has no gmtime_r/localtime_r, but time64.c calls them directly as
 * well as through the LOCALTIME_R()/GMTIME_R() macros, so provide them here
 * (implementations in win/compat/time_r.c) rather than undefining
 * HAS_GMTIME_R/HAS_LOCALTIME_R. */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);

/* ---------------------------------------------------------------------------
 * evalresp/libsrc/mxml/mxml-file.c
 *
 * mxml writes and reads through read()/write()/close(), and only includes
 * <unistd.h> when WIN32 is *not* defined.  MinGW defines WIN32, so the header
 * is skipped -- but MinGW declares those functions in <io.h>, which mxml never
 * includes either.  MSVC only warned about the implicit declarations; Clang
 * rejects them.
 * ------------------------------------------------------------------------ */
#include <io.h>

#include <unistd.h>
#include <stdio.h>      /* FILE, for the bison trace declarations below */

/*
 * The declarations below are for functions whose definitions live in SAC's own
 * sources rather than here, so they must have C linkage when this header is
 * force-included into a C++ translation unit (src/win/win.cpp).
 */
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * strsep()
 *
 * MinGW-w64 provides neither a declaration nor an implementation, and fern's
 * sources use it to split strings.  Implementation in win/compat/strsep.c.
 * ------------------------------------------------------------------------ */
char *strsep(char **stringp, const char *delim);

/* ---------------------------------------------------------------------------
 * basename() / dirname()
 *
 * POSIX declares these in <libgen.h>, but callers such as src/dfm/xw.c do not
 * include it.  The implementations live in src/dfm/readfl.c and are compiled
 * when MISSING_FUNC_BASENAME/MISSING_FUNC_DIRNAME are defined (see
 * win/config.h.in), so only the declarations are needed here.
 * ------------------------------------------------------------------------ */
char *basename(const char *path);
char *dirname(const char *path);

/* ---------------------------------------------------------------------------
 * Other functions the CRT lacks, whose implementations SAC provides in
 * src/string/strings.c.  Declared here because not every caller includes
 * inc/string_utils.h, where the same declarations are guarded by the
 * MISSING_FUNC_* macros.
 * ------------------------------------------------------------------------ */
size_t strlcpy(char *dst, const char *src, size_t size);
char  *rindex(const char *s, int c);
char  *index(const char *s, int c);

/* ---------------------------------------------------------------------------
 * Bison trace helpers
 *
 * src/eval/expr_parse.c is committed generated code that calls ParseTrace()
 * and ParseNoOpTrace() unconditionally.  Bison emits them only when tracing
 * is enabled, so provide no-op versions (win/compat/bison_trace.c) instead of
 * regenerating or patching the parser.
 * ------------------------------------------------------------------------ */
void ParseTrace(FILE *stream, const char *fmt, ...);
void ParseNoOpTrace(FILE *stream, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* SAC_WIN_COMPAT_H */
