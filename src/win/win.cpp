
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
    SacView *SacViewInit();
    void     SacViewDraw(SacView *view);
    void     win_set_current(SacView *view);
    char *   ImageToBounds(SacImage *im, SacRect bounds);
    unsigned int sleep(unsigned int seconds);
    void     sac_command_line_copyright(int argc, char **argv);

    /* C code declared in inc/co.h */
    void     zgimsg(int argc, char **argv, char *mess, int messlen);
    /* C code defined in src/main/sac.c; declared there and in src/osx/stubs.c */
    void     execute_command_line(char *kmsg, int len);

    void     SacViewWindowsAdd(SacViewWindows *wins, SacView *view);
    SacView *SacViewWindowsGetByHandle(SacViewWindows *wins, HWND handle);
    SacViewWindows *SacViewWindowsInit();

    /* C++ Code */
    void      sac_draw_line (HDC hdc, 
                             float x1, float y1, float x2, float y2,
                             float red, float green, float blue, 
                             int width);
    void      sac_draw_image (HDC hdc, int x, int y, int w, int h, char *data);
    void      SacWindowShow  (SacView *view);
    SacView * SacWindow      (int id);
    void      SacWindowAdd   (int id);
    int       use_tty        ();
    SacViewWindows *wins = NULL;
}

#define MAX_CONSOLE_LINES 500

#define SAC_WINDOW_CREATE (WM_APP + 1)
#define SAC_WINDOW_SHOW   (WM_APP + 2)

char Application[] = "SAC";
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
DWORD ThreadId;

//SacView *view;

DWORD main_thread;

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

void
SacWindowAdd(int id) {
    DEBUG("\n");
    if(GetCurrentThreadId() == main_thread) {
        SacView *view;
        DEBUG("main thread\n");
        view = SacWindow( id );
        SacViewWindowsAdd(wins, view);
        ShowWindow(view->window_handle, SW_SHOWNORMAL);
        UpdateWindow(view->window_handle);
        PostThreadMessage(ThreadId, SAC_WINDOW_CREATE, id, 0);
    } else {
      MSG msg;
      DEBUG("post message: CREATE WINDOW\n");
      if(!PostThreadMessage(main_thread, SAC_WINDOW_CREATE, id,0)) {
            fprintf(stderr, "Error attempting to create window on GUI Thread\n");
      }
      GetMessage(&msg, NULL, 0,0);
      if(msg.message != SAC_WINDOW_CREATE) {
        fprintf(stderr, "Error creating window\n");
        exit(-1);
      }
      DEBUG("post message: CREATE WINDOW: DONE\n");
    }
}

SacView * 
SacWindow(int id) {
    SacView *view;
    HWND hwnd;
    TCHAR title[100];
    int n;

    memset(&title[0], 0, 100);

#ifdef UNICODE
    n = swprintf(&title[0], 100, "Sac Plot Window: %d", id);
#else
    n = snprintf(&title[0], 100, "Sac Plot Window: %d", id);
#endif

    hwnd = CreateWindow(Application, 
                        title,
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 
                        400, 300,
                        NULL,NULL,
                        NULL,//hInstance,
                        NULL);
    
    if (!hwnd) {
        fprintf(stderr, "Error creating window\n");
        exit(-1);
        return 0;
    }
    view = SacViewInit();
    view->window_handle = hwnd;
    view->id            = id;
    win_set_current(view);

    return view;
}

void
SacWindowShow(SacView *view) {
    if(!view) {
        return;
    }
    if(GetCurrentThreadId() == main_thread) {
        //ShowWindow(view->window_handle, SW_SHOW);
        SetWindowPos(view->window_handle, HWND_TOP, 0,0, 0,0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
    } else {
        if(!PostThreadMessage(main_thread, SAC_WINDOW_SHOW, (WPARAM)view, 0)) {
            fprintf(stderr, "Error attempting to show_window on GUI Thread\n");
        }
    }
}

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
     * macOS entry points do with zgimsg() and execute_command_line().  Without
     * this, arguments are silently ignored on Windows. */
    if (__argc > 1) {
        char kmsg[MCMSG + 1];

        memset(&(kmsg[0]), ' ', MCMSG);
        kmsg[0] = '\0';
        kmsg[MCMSG] = '\0';

        zgimsg(__argc, __argv, kmsg, MCMSG + 1);
        execute_command_line(kmsg, MCMSG + 1);
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

void
sac_draw_line(HDC hdc, 
              float x1, float y1, 
              float x2, float y2, 
              float red, float green, float blue, 
              int width) {
    Graphics graphics(hdc);
    Pen      pen(Color(255,red,green,blue), width);
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.DrawLine(&pen, x1,y1, x2,y2);
}

void
sac_draw_image(HDC hdc, int x, int y, int w, int h, char *data) {
    Graphics graphics(hdc);
    Bitmap   bitmap(w,h, w*4, PixelFormat32bppARGB, (BYTE *) data);
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.DrawImage(&bitmap, x, y);
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
    case WM_DESTROY:
        DEBUG("WINDOW DESTORY\n");
        PostQuitMessage(0);
        TerminateThread(&ThreadId, 0);
        break;
        
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
} 


