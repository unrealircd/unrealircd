/************************************************************************
/************************************************************************
 *   IRC - Internet Relay Chat, win32.c
 *   Copyright (C) 1996 Daniel Hazelbaker
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef lint
static char sccsid[] = "@(#)win32.c	2.01 10/21/96 (C) 1996 Daniel Hazelbaker";
#endif


#define APPNAME "wIRCD"
#define wTITLEBAR "UnrealIRCd"

// Windows Header Files:
#include "common.h"
#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <io.h>
#include <fcntl.h>
#include "struct.h"
#include "sys.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "h.h"
#include "version.h"
#include "resource.h"
#include "CioFunc.h"

ID_CVS("$Id$");


BOOL              InitApplication(HINSTANCE);
BOOL              InitInstance(HINSTANCE, int);
LRESULT CALLBACK  FrameWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  Dlg_IRCDCONF(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK  Dlg_IRCDMOTD(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK  Dlg_IRCDRULES(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

BOOL              DisplayString(HWND hWnd, char *InBuf, ...);
void              LoadSetup(void);
void              SaveSetup(void);
int		  SetDebugLevel(HWND hWnd, int NewLevel);

LRESULT CALLBACK  Credits(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  Dreamforge(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  IRCDLicense(HWND, UINT, WPARAM, LPARAM);


extern  void      SocketLoop(void *dummy), s_rehash(), do_dns_async(HANDLE id);
extern  int       localdie(void), InitwIRCD(int argc, char *argv[]);


HINSTANCE   hInst; // current instance
char        szAppName[] = APPNAME; // The name of this application
char        szTitle[]   = wTITLEBAR; // The title bar text
HWND        hwIRCDWnd=NULL, hCio=NULL;
HANDLE      hMainThread = 0;

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

/*
 *  FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)
 *
 *  PURPOSE: Entry point for the application.
 *
 *  COMMENTS:
 *
 *	This function initializes the application and processes the
 *	message loop.
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;
	int argc=1;
	char *s, *argv[20], String[128];

	if (!hPrevInstance)
		if (!InitApplication(hInstance))
			return (FALSE);

	if (!InitInstance(hInstance, nCmdShow))
		return (FALSE);

	argv[0] = "WIRCD.EXE";
	if ( *(s = lpCmdLine) )
	    {
		argv[argc++] = s;
		while ( (s = strchr(s, ' ')) != NULL )
		    {
			while ( *s == ' ' ) *s++ = 0;
			argv[argc++] = s;
		    }
	    }
	argv[argc] = NULL;
	if ( InitwIRCD(argc, argv) != 1 )
		return FALSE;

	wsprintf(String, "UnrealIRCd/32 - %s", me.name);
	SetWindowText(hwIRCDWnd, String);

	SetDebugLevel(hwIRCDWnd, debuglevel);

	hMainThread = (HANDLE)_beginthread(SocketLoop, 0, NULL);
	hAccelTable = LoadAccelerators (hInstance, szAppName);

	LoadSetup();
	atexit(SaveSetup);

	/* Say we are ready to recieve connections */
	wsprintf(String, "%c%c%c%c[info] -=-=[ UnrealIRCd/32 v%s Ready ] - %s\r", 0,0,0,0, VERSIONONLY);
	DisplayString(hCio, String);
	/* Main message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	    {
		if ( !TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		    {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		    }
	    }

	return (msg.wParam);
	lpCmdLine; /* This will prevent 'unused formal parameter' warnings */
}


/*
 *  FUNCTION: InitApplication(HANDLE)
 *
 *  PURPOSE: Initializes window data and registers window class 
 *
 *  COMMENTS:
 *
 *       In this function, we initialize a window class by filling out a data
 *       structure of type WNDCLASS and calling either RegisterClass or 
 *       the internal MyRegisterClass.
 */
BOOL InitApplication(HINSTANCE hInstance)
{
    WNDCLASS  wc;

    // Fill in window class structure.
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = (WNDPROC)FrameWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon (hInstance, APPNAME);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = szAppName;
    wc.lpszClassName = szAppName;

    if ( !RegisterClass(&wc) ) return 0;

    return 1;
}


/*
 *   FUNCTION: InitInstance(HANDLE, int)
 *
 *   PURPOSE: Saves instance handle and creates main window 
 *
 *   COMMENTS:
 *
 *        In this function, we save the instance handle in a global variable and
 *        create and display the main program window.
 */
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;
    WSADATA             WSAData;

	
    if ( WSAStartup(MAKEWORD(1, 1), &WSAData) != 0 )
    {
        MessageBox(NULL, "UnrealIRCD/32 Init Error", "Unable to initialize WinSock DLL", MB_OK);
        return FALSE;
    }

	hInst = hInstance; /* Store instance handle in our global variable */
    
    if ( !Cio_Init(hInst) )
    {
        MessageBox(NULL, "UnrealIRCD/32 Init Error", "Couldn't Init CIO Library", MB_OK);
        return FALSE;
    }

	hWnd = CreateWindow(szAppName, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
		NULL, NULL, hInstance, NULL);

	if ( !hWnd )
		return (FALSE);

    ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hwIRCDWnd = hWnd);

	return (TRUE);
}


/*
 *  FUNCTION: FrameWndProc(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for the main window.
 *
 */
LRESULT CALLBACK FrameWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int    wmId, wmEvent;
	char	*xx = NULL;
	
	switch (message)
	    {
	        case WM_CREATE:
			hCio  = Cio_Create(hInst, hWnd, WS_VISIBLE, 0, 0, 300, 200);
			xx = MyMalloc(1024);
			sprintf(xx, "%c%c%c%c[info] -=-=[ UnrealIRCd/32 v%s Loading ]-=-=\r", 0, 0, 0, 0,
				VERSIONONLY);
			DisplayString(hCio, xx);
#ifdef WIN32_SPECIFY
			sprintf(xx, "%c%c%c%c[info] - Ported by %s", 0,0,0,0, WIN32_PORTER);
			DisplayString(hCio, xx);	
			sprintf(xx, "%c%c%c%c[info] - %s", 0,0,0,0, WIN32_URL);
			DisplayString(hCio, xx);
#endif
			MyFree(xx);
			return 0;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			switch (wmId)
			    {
				case IDM_ABOUT:
					DialogBox(hInst, "AboutBox", hWnd, (DLGPROC)About);
					break;
				case IDM_CREDITS:
					DialogBox(hInst, "AboutBox", hWnd, (DLGPROC)Credits);
					break;
				case IDM_DF:
					DialogBox(hInst, "AboutBox", hWnd, (DLGPROC)Dreamforge);
					break;
				case IDM_LICENSE:
					DialogBox(hInst, "AboutBox", hWnd, (DLGPROC)IRCDLicense);
					break;

				case IDM_IRCDCONF:
					DialogBox(hInst, "DLG_IRCDCONF", hWnd, (DLGPROC)Dlg_IRCDCONF);
					break;
				case IDM_IRCDMOTD:
					DialogBox(hInst, "DLG_IRCDMOTD", hWnd, (DLGPROC)Dlg_IRCDMOTD);
					break;
				case IDM_IRCDRULES:
					DialogBox(hInst, "DLG_IRCDRULES", hWnd, (DLGPROC)Dlg_IRCDRULES);
					break;
				case IDM_REHASH:
					s_rehash();
					break;

				case IDM_EXIT:
					if ( MessageBox(hWnd, "Are you sure?",
						"Terminate UnrealIRCD/32",
						MB_ICONQUESTION | MB_YESNO) == IDNO )
						break;
					DestroyWindow(hWnd);
					break;

				case IDM_DBGOFF:
				case IDM_DBGFATAL:
				case IDM_DBGERROR:
				case IDM_DBGNOTICE:
				case IDM_DBGDNS:
				case IDM_DBGINFO:
				case IDM_DBGNUM:
				case IDM_DBGSEND:
				case IDM_DBGDEBUG:
				case IDM_DBGMALLOC:
				case IDM_DBGLIST:
					SetDebugLevel(hWnd, wmId-IDM_DBGFATAL);
					break;

				default:
					return (DefWindowProc(hWnd, message, wParam, lParam));
			    }
			break;

		case WM_CLOSE:
			if ( MessageBox(hWnd, "Are you sure?", "Terminate UnrealIRCd/32",
					MB_ICONQUESTION | MB_YESNO) == IDNO )
				break;
			return (DefWindowProc(hWnd, message, wParam, lParam));

		case WM_DESTROY:
			localdie();   /* Never returns */
			PostQuitMessage(0);
			break;

		case WM_SIZE:
			SetWindowPos(hCio, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam),
				SWP_NOZORDER);
			/* Fallthrough to get the default handling too. */

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
	    }
	return (0);
}


/*
 *  FUNCTION: About(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "About" dialog box
 * 		This version allows greater flexibility over the contents of the 'About' box,
 * 		by pulling out values from the 'Version' resource.
 *
 *  MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 */
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_INITDIALOG:
#define Ccat strcat(String, String2)
		    {
			char	String[16384], String2[16384], **s = infotext;
			sprintf(String, "%s\n%s", version, creation);
			SetDlgItemText(hDlg, IDC_VERSION, String);
			String[0] = 0; String2[0] = 0;
			sprintf(String2, "-=-=-=-=-=-==-==- %s -=-=-==-==-=-=-=-=-=-\r\n", ircnetwork); Ccat;
			sprintf(String2, "|Web Page:      | http://www.%s\r\n", netdomain); Ccat;
			sprintf(String2, "|FTP Archive:   | ftp://ftp.%s\r\n", netdomain); Ccat;
			sprintf(String2, "|Help channel:  | %s\r\n", helpchan); Ccat;
			sprintf(String2, "|=-=-=-=-=-==-==|-=-=-=-=-=-=-==-==-=-=-=-=-=-=-=\r\n"); Ccat;
			sprintf(String2, "|IRCd version:  | %s\r\n", IRCDTOTALVERSION); Ccat;			
			sprintf(String2, "| Developers:   | Stskeeps <stskeeps@tspre.org>\r\n"); Ccat;
			sprintf(String2, "|               | codemastr <codemastr@tspre.org>\r\n"); Ccat; 
#if defined(_WIN32) && defined(WIN32_SPECIFY)
			sprintf(String2, "| Win32 Porter: | %s\r\n", WIN32_PORTER); Ccat;
			sprintf(String2, "|     >>URL:    | %s\r\n", WIN32_URL); Ccat;
#endif
			sprintf(String2, "|Credits:       | Type /Credits\r\n"); Ccat;
			sprintf(String2, "|DALnet Credits:| Type /DALinfo\r\n"); Ccat;
			sprintf(String2, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\r\n"); Ccat;
			sprintf(String2, "| Unreal IRCd can be downloaded at http://unreal.tspre.org\r\n"); Ccat;	
			sprintf(String2, "| This notice may not be removed from the IRCd package\r\n"); Ccat;
			sprintf(String2, "| It will be a violation of copyright. This program must always stay free of charge\r\n"); Ccat;
			sprintf(String2, "| being sold commercially or privately\r\n"); Ccat;
			sprintf(String2, "| Only charge may be for the transport medium like on CD-ROM, floppy\r\n"); Ccat;
			sprintf(String2, "| or other kinds (-Stskeeps'1999)\r\n"); Ccat;
			sprintf(String2, "--------------------------------------------\r\n"); Ccat;
			SetDlgItemText(hDlg, IDC_INFOTEXT, String);
#undef Ccat
			ShowWindow (hDlg, SW_SHOW);
			return (TRUE);
		    }

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			    {
				EndDialog(hDlg, TRUE);
				return (TRUE);
			    }
			break;
	    }
    return FALSE;
}

/*
 *  FUNCTION: Credits(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "Credits" dialog box
 * 		This version allows greater flexibility over the contents of the 'Credits' box,
 * 		by pulling out values from the 'Version' resource.
 *
 *  MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 */
LRESULT CALLBACK Credits(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_INITDIALOG:
		    {
			char	String[16384], **s = unrealcredits;

			sprintf(String, "%s\n%s", version, creation);
			SetDlgItemText(hDlg, IDC_VERSION, String);
			String[0] = 0;
			while ( *s )
			    {
				strcat(String, *s++);
				if ( *s )
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_INFOTEXT, String);


			ShowWindow (hDlg, SW_SHOW);
			return (TRUE);
		    }

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			    {
				EndDialog(hDlg, TRUE);
				return (TRUE);
			    }
			break;
	    }
    return FALSE;
}

/*
 *  FUNCTION: Dreamforge(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "Dreamforge" dialog box
 * 		This version allows greater flexibility over the contents of the 'Dreamforge' box,
 * 		by pulling out values from the 'Version' resource.
 *
 *  MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 */
LRESULT CALLBACK Dreamforge(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_INITDIALOG:
		    {
			char	String[16384], **s = dalinfotext;

			sprintf(String, "%s\n%s", version, creation);
			SetDlgItemText(hDlg, IDC_VERSION, String);
			String[0] = 0;
			while ( *s )
			    {
				strcat(String, *s++);
				if ( *s )
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_INFOTEXT, String);


			ShowWindow (hDlg, SW_SHOW);
			return (TRUE);
		    }

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			    {
				EndDialog(hDlg, TRUE);
				return (TRUE);
			    }
			break;
	    }
    return FALSE;
}

/*
 *  FUNCTION: IRCDLicense(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "IRCDLicense" dialog box
 * 		This version allows greater flexibility over the contents of the 'IRCDLicense' box,
 * 		by pulling out values from the 'Version' resource.
 *
 *  MESSAGES:
 *
 *	WM_INITDIALOG - initialize dialog box
 *	WM_COMMAND    - Input received
 *
 */
LRESULT CALLBACK IRCDLicense(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_INITDIALOG:
		    {
			char	String[16384], **s = gnulicense;

			sprintf(String, "%s\n%s", version, creation);
			SetDlgItemText(hDlg, IDC_VERSION, String);
			String[0] = 0;
			while ( *s )
			    {
				strcat(String, *s++);
				if ( *s )
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_INFOTEXT, String);


			ShowWindow (hDlg, SW_SHOW);
			return (TRUE);
		    }

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			    {
				EndDialog(hDlg, TRUE);
				return (TRUE);
			    }
			break;
	    }
    return FALSE;
}


/*
 *  FUNCTION: Dlg_IrcdConf(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "DLG_IRCDCONF" dialog box
 *
 */
LRESULT CALLBACK Dlg_IRCDCONF(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
    {
        case WM_INITDIALOG:
            {
                char  *Buffer = MyMalloc(65535);   /* Should be big enough */
                int   fd, Len;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCd/32 Setup", MB_OK);
                    EndDialog(hDlg, FALSE);
					return FALSE;
                }
                /* Open the ircd.conf file */
                fd = open(CONFIGFILE, _O_RDONLY | _O_BINARY);
                if ( fd == -1 )
		    {
			MessageBox(hDlg, "Error: Could not open configuration file",
				   "UnrealIRCd/32 Setup", MB_OK);
			MyFree(Buffer);
			EndDialog(hDlg, FALSE);
			return FALSE;
		    }

                Buffer[0] = 0;          /* Incase read() fails */
                Len = read(fd, Buffer, 65535);
                Buffer[Len] = 0;
                /* Set the text for the edit control to what was in the file */
                SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_SETTEXT, 0,
                    (LPARAM)(LPCTSTR)Buffer);

                close(fd);
                MyFree(Buffer);
            }
			return (TRUE);

		case WM_COMMAND:
			if ( LOWORD(wParam) == IDOK )
            {
                char  *Buffer = MyMalloc(65535);   /* Should be big enough */
                DWORD Len;
                int   fd;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCD/32 Setup", MB_OK);
                    return TRUE;
                }
                /* Open the ircd.conf file */
                fd = open(CONFIGFILE, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY,
			S_IREAD|S_IWRITE);
                if ( fd == -1 )
                {
                    MessageBox(hDlg, "Error: Could not open configuration file",
                        "UnrealIRCD/32 Setup", MB_OK);
                    MyFree(Buffer);
                    return TRUE;
                }

                /* Get the text from the edit control and save it to disk. */
                Len = SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_GETTEXT, 65535,
                    (LPARAM)(LPCTSTR)Buffer);
                write(fd, Buffer, Len);

                close(fd);
                MyFree(Buffer);

				EndDialog(hDlg, TRUE);
				return TRUE;
			}
            if ( LOWORD(wParam) == IDCANCEL )
            {
                EndDialog(hDlg, FALSE);
                return TRUE;
            }
			break;
	}

    return FALSE;
}

/*
 *  FUNCTION: Dlg_Dlg_IRCdMotd(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "DLG_IRCDCONF" dialog box
 *
 */
LRESULT CALLBACK Dlg_IRCDMOTD(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
    {
        case WM_INITDIALOG:
            {
                char  *Buffer = MyMalloc(65535*2);   /* Should be big enough */
                int   fd, Len;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCd/32 Setup", MB_OK);
                    EndDialog(hDlg, FALSE);
					return FALSE;
                }
                /* Open the ircd.motd file */
                fd = open(MPATH, _O_RDONLY | _O_BINARY);
                if ( fd == -1 )
		    {
			MessageBox(hDlg, "Error: Could not open MOTD file",
				   "UnrealIRCd/32 Setup", MB_OK);
			MyFree(Buffer);
			EndDialog(hDlg, FALSE);
			return FALSE;
		    }

                Buffer[0] = 0;          /* Incase read() fails */
                Len = read(fd, Buffer, 65535);
                Buffer[Len] = 0;
                /* Set the text for the edit control to what was in the file */
                SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_SETTEXT, 0,
                    (LPARAM)(LPCTSTR)Buffer);

                close(fd);
                MyFree(Buffer);
            }
			return (TRUE);

		case WM_COMMAND:
			if ( LOWORD(wParam) == IDOK )
            {
                char  *Buffer = MyMalloc(65535);   /* Should be big enough */
                DWORD Len;
                int   fd;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCD/32 Setup", MB_OK);
                    return TRUE;
                }
                /* Open the ircd.motd file */
                fd = open(MPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY,
			S_IREAD|S_IWRITE);
                if ( fd == -1 )
                {
                    MessageBox(hDlg, "Error: Could not open motd file",
                        "UnrealIRCD/32 Setup", MB_OK);
                    MyFree(Buffer);
                    return TRUE;
                }

                /* Get the text from the edit control and save it to disk. */
                Len = SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_GETTEXT, 65535,
                    (LPARAM)(LPCTSTR)Buffer);
                write(fd, Buffer, Len);

                close(fd);
                MyFree(Buffer);

				EndDialog(hDlg, TRUE);
				return TRUE;
			}
            if ( LOWORD(wParam) == IDCANCEL )
            {
                EndDialog(hDlg, FALSE);
                return TRUE;
            }
			break;
	}

    return FALSE;
}

/*
 *  FUNCTION: Dlg_IRCdRules(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "DLG_IRCDCONF" dialog box
 *
 */
LRESULT CALLBACK Dlg_IRCDRULES(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
    {
        case WM_INITDIALOG:
            {
                char  *Buffer = MyMalloc(65535*2);   /* Should be big enough */
                int   fd, Len;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCd/32 Setup", MB_OK);
                    EndDialog(hDlg, FALSE);
					return FALSE;
                }
                /* Open the ircd.rules file */
                fd = open(RPATH, _O_RDONLY | _O_BINARY);
                if ( fd == -1 )
		    {
			MessageBox(hDlg, "Error: Could not open rules file",
				   "UnrealIRCd/32 Setup", MB_OK);
			MyFree(Buffer);
			EndDialog(hDlg, FALSE);
			return FALSE;
		    }

                Buffer[0] = 0;          /* Incase read() fails */
                Len = read(fd, Buffer, 65535);
                Buffer[Len] = 0;
                /* Set the text for the edit control to what was in the file */
                SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_SETTEXT, 0,
                    (LPARAM)(LPCTSTR)Buffer);

                close(fd);
                MyFree(Buffer);
            }
			return (TRUE);

		case WM_COMMAND:
			if ( LOWORD(wParam) == IDOK )
            {
                char  *Buffer = MyMalloc(65535);   /* Should be big enough */
                DWORD Len;
                int   fd;

                if ( !Buffer )
                {
                    MessageBox(hDlg, "Error: Could not allocate temporary buffer",
                        "UnrealIRCD/32 Setup", MB_OK);
                    return TRUE;
                }
                /* Open the ircd.rules file */
                fd = open(RPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY,
			S_IREAD|S_IWRITE);
                if ( fd == -1 )
                {
                    MessageBox(hDlg, "Error: Could not open rules file",
                        "UnrealIRCD/32 Setup", MB_OK);
                    MyFree(Buffer);
                    return TRUE;
                }

                /* Get the text from the edit control and save it to disk. */
                Len = SendDlgItemMessage(hDlg, IDC_IRCDCONF, WM_GETTEXT, 65535,
                    (LPARAM)(LPCTSTR)Buffer);
                write(fd, Buffer, Len);

                close(fd);
                MyFree(Buffer);

				EndDialog(hDlg, TRUE);
				return TRUE;
			}
            if ( LOWORD(wParam) == IDCANCEL )
            {
                EndDialog(hDlg, FALSE);
                return TRUE;
            }
			break;
	}

    return FALSE;
}


int  DisplayString(HWND hWnd, char *InBuf, ...)
{
    CioWndInfo  *CWI;
    va_list  argptr;
    char    *Buffer=NULL, *Ptr=NULL;
    DWORD    Len=0, TLen=0, Off=0, i=0;
    BYTE     Red=0, Green=0, Blue=0;
    BOOL     Bold = FALSE;

    if ( (Buffer = LocalAlloc(LPTR, 16384)) == NULL ) return FALSE;

    va_start(argptr, InBuf);
    Len = vsprintf(Buffer, InBuf, argptr);
    va_end(argptr);
    if ( Len == 0 )
    {
        LocalFree(Buffer);
        return FALSE;
    }

    CWI = (CioWndInfo *)GetWindowLong(hWnd, GWL_USER);
    for ( i = 0; i < Len; i++ )
    {
        if ( Buffer[i] == 0 )
        {
            i+=3;
            continue;
        }
        if ( Buffer[i] == 0x02 )
        {
            if ( !Bold )
            {
                Buffer[i] = 0;
                Cio_Puts(hWnd, Buffer+Off, i-Off);
                Red = CWI->FR;
                Green = CWI->FG;
                Blue = CWI->FB;
			
                Off=i+1;
                Cio_PrintF(hWnd, "%c%c%c%c", 0, 255, 32, 32);
                Bold = 1;
                continue;
            }
            if ( Bold )
            {
                Buffer[i] = 0;
                Cio_Puts(hWnd, Buffer+Off, i-Off);
                Off=i+1;
                Cio_PrintF(hWnd, "%c%c%c%c", 0, Red, Green, Blue);
                Bold = 0;
                continue;
            }
        }
    }
    Cio_Puts(hWnd, Buffer+Off, Len-Off);

    LocalFree(Buffer);
    return TRUE;
}


void   LoadSetup(void)
{
}

void   SaveSetup(void)
{
}


int	SetDebugLevel(HWND hWnd, int NewLevel)
{
	HMENU	hMenu = GetMenu(hWnd);

	if ( !hMenu || !(hMenu = GetSubMenu(hMenu, 1)) ||
	     !(hMenu = GetSubMenu(hMenu, 4)) )
		return -1;

	CheckMenuItem(hMenu, IDM_DBGFATAL+debuglevel,
		MF_BYCOMMAND | MF_UNCHECKED);
	debuglevel = NewLevel;
	CheckMenuItem(hMenu,IDM_DBGFATAL+debuglevel,
		MF_BYCOMMAND | MF_CHECKED);

	return debuglevel;
}


 
