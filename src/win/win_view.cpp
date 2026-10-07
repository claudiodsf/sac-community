/**
 * @file   win_view.cpp
 *
 * @brief  Plot windows, and the GDI+ drawing SAC does in them
 *
 * The C device (src/win/windows_sac.c and src/win/WinSacView.c) drives these
 * helpers.  They used to live in win.cpp together with the program's entry
 * point, which meant the device could only be linked into sac.exe: the t/ unit
 * tests reach it through inigdm(), and would not link because of it.
 *
 * Keep the program entry point, the window class registration, the message
 * loop and the window procedure in win.cpp; keep the window and drawing
 * runtime here, where anything that links the device can reach it.
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "WinSacView.h"
#include "config.h"

extern "C" {
    #include "debug.h"

    /* C code, src/win/WinSacView.c */
    SacView *SacViewInit();
    void     SacViewDraw(SacView *view);
    void     win_set_current(SacView *view);
    void     SacViewWindowsAdd(SacViewWindows *wins, SacView *view);
    SacViewWindows *SacViewWindowsInit();

    /* Defined below; declared here so that the C device links to them. */
    void      SacWindowAdd   (int id);
    void      SacWindowShow  (SacView *view);
    SacView * SacWindow      (int id);
    void      sac_draw_line  (HDC hdc,
                              float x1, float y1, float x2, float y2,
                              float red, float green, float blue, int width);
    void      sac_draw_polyline(HDC hdc, POINT *pts, int n,
                              float red, float green, float blue, int width);
    void      sac_draw_image (HDC hdc, int x, int y, int w, int h, char *data);

    /* The plot window list.  Declared with C linkage because that is how the
     * device refers to it (src/win/windows_sac.c: "extern SacViewWindows *wins"). */
    SacViewWindows *wins = NULL;
}

/* The entry point in win.cpp runs the GUI on main_thread and the console on
 * ThreadId; both are used here to decide whether a window operation can be
 * done directly or has to be posted to the GUI thread. */
DWORD main_thread;
DWORD ThreadId;

/* Name of the plot window class, registered by the entry point in win.cpp. */
char Application[] = "SAC";

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

/* Pick a default size and position for a new plot window: three quarters of the
 * usable screen, so that plots are readable without being resized first, but
 * never larger than the work area (which excludes the taskbar). */
static void
SacWindowDefaultGeometry(int id, int *x, int *y, int *width, int *height) {
    RECT work;
    int work_w, work_h;

    /* Fall back to the primary screen if the work area is unavailable. */
    work.left = 0;
    work.top = 0;
    work.right = GetSystemMetrics(SM_CXSCREEN);
    work.bottom = GetSystemMetrics(SM_CYSCREEN);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);

    work_w = work.right - work.left;
    work_h = work.bottom - work.top;

    *width = (int) (work_w * 0.75);
    *height = (int) (work_h * 0.75);
    if (*width < 640) {
        *width = (work_w < 640) ? work_w : 640;
    }
    if (*height < 480) {
        *height = (work_h < 480) ? work_h : 480;
    }

    /* Centre the window, offsetting each extra window so they do not stack. */
    *x = work.left + (work_w - *width) / 2 + (id - 1) * 24;
    *y = work.top + (work_h - *height) / 2 + (id - 1) * 24;
}

SacView *
SacWindow(int id) {
    SacView *view;
    HWND hwnd;
    TCHAR title[100];
    int x, y, width, height;
    int n;

    memset(&title[0], 0, 100);

#ifdef UNICODE
    n = swprintf(&title[0], 100, "Sac Plot Window: %d", id);
#else
    n = snprintf(&title[0], 100, "Sac Plot Window: %d", id);
#endif
    (void) n;

    SacWindowDefaultGeometry(id, &x, &y, &width, &height);

    hwnd = CreateWindow(Application,
                        title,
                        WS_OVERLAPPEDWINDOW,
                        x, y,
                        width, height,
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

/* Draw a whole polyline with a single call.  Drawing it one segment at a time
 * (what SacViewDraw used to do) builds a Graphics object and a Pen, and sets
 * the smoothing mode, for every single point: SAC plots traces of up to a
 * million points, so sm/bandpass and sm/sss spent minutes or hours in here.
 *
 * Antialiasing is skipped for the long traces: it is expensive over hundreds
 * of thousands of segments, and the X11 backend draws them aliased anyway. */
void
sac_draw_polyline(HDC hdc, POINT *pts, int n,
                  float red, float green, float blue, int width) {
    if (n < 2) {
        return;
    }
    Graphics graphics(hdc);
    Pen      pen(Color(255,red,green,blue), width);
    graphics.SetSmoothingMode(n > 20000 ? SmoothingModeNone
                                        : SmoothingModeHighQuality);
    /* GDI+ Point and Win32 POINT are both a pair of 32-bit integers. */
    graphics.DrawLines(&pen, (const Point *) pts, n);
}

void
sac_draw_image(HDC hdc, int x, int y, int w, int h, char *data) {
    Graphics graphics(hdc);
    Bitmap   bitmap(w,h, w*4, PixelFormat32bppARGB, (BYTE *) data);
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.DrawImage(&bitmap, x, y);
}
