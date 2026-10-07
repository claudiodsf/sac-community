/*
 * win/compat/sys/select.h -- <sys/select.h> for MinGW.
 *
 * MinGW-w64 has no <sys/select.h>; select() and its fd_set live in
 * <winsock2.h>.  src/co/select.c includes the POSIX header, so provide it
 * here rather than patching SAC's source.  <winsock2.h> must be included
 * before <windows.h>, which is why this header is deliberately tiny.
 */
#ifndef SAC_COMPAT_SYS_SELECT_H
#define SAC_COMPAT_SYS_SELECT_H

#ifdef _WIN32

#include <winsock2.h>

#endif /* _WIN32 */

#endif /* SAC_COMPAT_SYS_SELECT_H */
