/*
 * win/compat/sys/ioctl.h -- minimal <sys/ioctl.h> for MinGW.
 *
 * MinGW-w64 ships no <sys/ioctl.h>, but fern/request.c includes it to find the
 * size of the terminal.  Rather than patch fern, the build puts win/compat
 * ahead of the system include path so this file is found instead.
 */
#ifndef SAC_COMPAT_SYS_IOCTL_H
#define SAC_COMPAT_SYS_IOCTL_H

#ifdef _WIN32

#include <sys/types.h>

/* Same value as the Linux/BSD definition; only the semantics of the fd differ. */
#define TIOCGWINSZ 0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* Returns 0 on success, -1 with errno set otherwise (ENOTTY if fd is not a
 * console).  Only TIOCGWINSZ is implemented. */
int ioctl(int fd, unsigned long request, ...);

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#endif /* _WIN32 */

#endif /* SAC_COMPAT_SYS_IOCTL_H */
