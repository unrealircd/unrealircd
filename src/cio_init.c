
#include <windows.h>
#include "Cio.h"
#include "CioFunc.h"
#include "common.h"
ID_CVS("$Id$");

//
//  FUNCTION: Cio_Init(void)
//
//  PURPOSE: Initializes window data and registers window class and other stuff
//
BOOL Cio_Init(HINSTANCE hInstance)
{
	WNDCLASS wc;

	// Fill in window class structure with parameters that describe
	// the main window.
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC) Cio_WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 4;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = CIOCLASS;

	if (!RegisterClass(&wc))
		return FALSE;

	return TRUE;
}
