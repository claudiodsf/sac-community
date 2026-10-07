
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

#include "sac_resource.h"
#include "WinSacView.h"
#include "config.h"
#include "mach.h"       /* MCMSG */

extern "C" {
    #include "debug.h"
    /* C Code */
    char*    getline_stdin();
    void     main_command(char *kmsg, int n);
    void     win_init();
    void     SacViewDraw(SacView *view);
    char *   ImageToBounds(SacImage *im, SacRect bounds);
    unsigned int sleep(unsigned int seconds);
    void     sac_command_line_copyright(int argc, char **argv);

    /* C code declared in inc/co.h */
    void     zgimsg(int argc, char **argv, char *mess, int messlen);
    /* C code defined in src/main/sac.c; declared there and in src/osx/stubs.c */
    void     execute_command_line(char *kmsg, int len);

    SacView *SacViewWindowsGetByHandle(SacViewWindows *wins, HWND handle);
    SacViewWindows *SacViewWindowsInit();

    /* C++ code, defined in src/win/win_view.cpp together with the window and
     * drawing runtime: the C device in windows_sac.c calls these, so they
     * cannot live in the file that holds the program's entry point. */
    void      SacWindowShow  (SacView *view);
    void      SacWindowAdd   (int id);
    int       use_tty        ();

    /* Also defined in win_view.cpp.  The entry point below sets the thread ids
     * up and registers the window class whose name is Application. */
    extern SacViewWindows *wins;
    extern char Application[];
    extern DWORD main_thread;
    extern DWORD ThreadId;
}

#define MAX_CONSOLE_LINES 500

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

int
show_prompt() {
  if(use_tty()) {
    fprintf(stdout, "SAC> ");
  }
  return TRUE;	
}

DWORD WINAPI ConsoleIO(LPVOID lpArg) {
  char *line;
  while(show_prompt() && (line = getline_stdin())) {
      main_command(line, strlen(line));
      free(line);
      line = NULL;
  }
  return 0;
}

/* SacWindowAdd(), SacWindowDefaultGeometry(), SacWindow(), SacWindowShow()
 * and the sac_draw_*() helpers live in src/win/win_view.cpp now, so that the
 * Win32 device can be linked without the program's entry point - the t/ unit
 * tests reach the device through inigdm() and have to do exactly that. */

#ifdef GUI_APP
int WINAPI
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR pCmdLine,
        int nCmdShow)  {
    
    WNDCLASS wc;
    /* HWND hwnd; */
    MSG msg;

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    // Initialize GDI+.
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL,IDI_WINLOGO);
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOWFRAME;
    /* No menu bar.  WNDCLASS is not zero-initialised on this path, so this has
     * to be set explicitly rather than left out. */
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = Application;
    
    if (!RegisterClass(&wc))
        return 0;
    
    {
        /* Create Console */
        AllocConsole();    
        freopen("CONIN$", "r", stdin); 
        freopen("CONOUT$", "w", stdout); 
        freopen("CONOUT$", "w", stderr); 
    }
#else
/* MinGW's <stdlib.h> defines __argc and __argv as function-like macros, which
 * would mangle the parameter names below into bogus function pointer types.
 * MSVC exposes them as globals instead, hence the guard. */
#ifdef __MINGW32__
#undef __argc
#undef __argv
#endif
int
main(int __argc, char **__argv) {
  MSG msg;

  /* The console entry point needs the same one-time GDI+ initialisation and
   * plot window class registration that the WinMain() path performs; without
   * it SacWindow()'s CreateWindow() has no class to create a window from. */
  {
    WNDCLASS wc;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOWFRAME;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = Application;

    if (!RegisterClass(&wc)) {
      fprintf(stderr, "sac: could not register the plot window class\n");
    }
  }
#endif

#ifdef _MSC_VER
  /* MSVC-only CRT tweak: print exponents with at least two digits.  The
   * symbol no longer exists in the modern Universal CRT. */
  _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
  sac_command_line_copyright(__argc, __argv);

  main_thread = GetCurrentThreadId();
    /* Initialize the Window List */
    wins = SacViewWindowsInit();

    /* Initialize SAC */
    win_init();

    /* Run a macro or command given on the command line, the way the X11 and
     * macOS entry points do with zgimsg() and execute_command_line().  Only
     * arguments after the options are a macro: the flags (--stdout,
     * --history-off, ...) are consumed above, and passing them on would make
     * SAC try to run "macro --stdout ..." and fail to find that file.
     * src/main/sac.c does the same by advancing argv past optind. */
    {
        int i = 1;

        while (i < __argc && __argv[i][0] == '-' && __argv[i][1] != '\0') {
            i++;
        }
        if (i < __argc) {
            char kmsg[MCMSG + 1];

            memset(&(kmsg[0]), ' ', MCMSG);
            kmsg[0] = '\0';
            kmsg[MCMSG] = '\0';

            zgimsg(__argc - (i - 1), __argv + (i - 1), kmsg, MCMSG + 1);
            execute_command_line(kmsg, MCMSG + 1);
        }
    }

    /* Put Console on its own Thread */
    CreateThread(NULL, 0, ConsoleIO, NULL, 0, &ThreadId);

    /* Loop over Events */
    while (GetMessage(&msg,NULL,0,0) > 0) {
        if(msg.message == SAC_WINDOW_CREATE) {
            SacWindowAdd(msg.wParam);
        } else if(msg.message == SAC_WINDOW_SHOW) {
            SacWindowShow((SacView *)msg.wParam);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#ifdef GUI_APP
    GdiplusShutdown(gdiplusToken);
#endif

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    SacView *view;
    switch (msg) {
    case WM_PAINT:
        view = SacViewWindowsGetByHandle(wins, hwnd);
        if(view) {
            SacViewDraw(view);
        } else {
            fprintf(stderr, "View undefined\n");
        }
        break;
    case WM_CHAR:
        /* A key typed in the plot window while a picking command is waiting.
         * Record it and wake the command thread blocked in win_cursor(). */
        view = SacViewWindowsGetByHandle(wins, hwnd);
        if(view && view->cursor_active) {
            view->key_char = (char) wparam;
            view->key_ready = 1;
            SetEvent(view->key_event);
            return 0;
        }
        break;
    case SAC_WINDOW_FOCUS:
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        break;
    case WM_CLOSE:
        /* Closing a plot window must not end the SAC session.  Hide the window
         * instead; the next frame re-opens it (see win_begin_frame).  If a
         * picking command is waiting for a key, release it so it returns to
         * the command level instead of waiting on an invisible window. */
        view = SacViewWindowsGetByHandle(wins, hwnd);
        if(view && view->cursor_active) {
            view->key_char = 'Q';
            view->key_ready = 1;
            SetEvent(view->key_event);
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        /* No PostQuitMessage()/TerminateThread() here: destroying one plot
         * window must not terminate SAC.  Just release a waiting cursor. */
        DEBUG("WINDOW DESTORY\n");
        view = SacViewWindowsGetByHandle(wins, hwnd);
        if(view && view->cursor_active) {
            view->key_char = 'Q';
            view->key_ready = 1;
            SetEvent(view->key_event);
        }
        break;
        
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
} 


