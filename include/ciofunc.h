// $Id$
#include "Cio.h"

#define GWL_USER        0
#define CIO_ADDSTRING   WM_USER
#define CIO_CLEAR       WM_USER+1

// Cio_Init.c
BOOL Cio_Init(HINSTANCE hInstance);

// Cio_Main.c
LRESULT CALLBACK Cio_WndProc(HWND, UINT, WPARAM, LPARAM);
HWND Cio_Create(HINSTANCE hInstance, HWND hParent, DWORD Style, int X, int Y, int W, int H);
BOOL Cio_WndCreate(HWND hWnd);
BOOL Cio_WndPaint(HWND hWnd);
BOOL Cio_WndDestroy(HWND hWnd);
BOOL Cio_WndAddString(HWND hWnd, int Len, char *Buffer);
BOOL Cio_WndSize(HWND hWnd, LPARAM lParam);
void Cio_Scroll(HWND hWnd, CioWndInfo *CWI, int Scroll);
BOOL Cio_PrintF(HWND hWnd, char *InBuf, ...);
