/*
 * win/compat/ioctl.c -- the implementation behind win/compat/sys/ioctl.h.
 *
 * Only TIOCGWINSZ is supported, which is all that SAC needs (fern/request.c
 * uses it to work out how wide the terminal is).  The console screen buffer
 * size is the closest Windows equivalent.
 */
#ifdef _WIN32

#include <windows.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/ioctl.h>

int
ioctl(int fd, unsigned long request, ...)
{
    va_list ap;

    (void) fd;

    if (request != TIOCGWINSZ) {
        errno = EINVAL;
        return -1;
    }

    {
        CONSOLE_SCREEN_BUFFER_INFO info;

        va_start(ap, request);
        {
            struct winsize *ws = va_arg(ap, struct winsize *);
            va_end(ap);

            if (ws == NULL) {
                errno = EINVAL;
                return -1;
            }

            if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
                errno = ENOTTY;
                return -1;
            }

            ws->ws_col = (unsigned short) (info.srWindow.Right - info.srWindow.Left + 1);
            ws->ws_row = (unsigned short) (info.srWindow.Bottom - info.srWindow.Top + 1);
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
        }
    }

    return 0;
}

#endif /* _WIN32 */
