/************************************************************************
 *   IRC - Internet Relay Chat, Win32GUI.c
 *   Copyright (C) 2000 David Flynn (DrBin)
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



#define APPNAME "wIRCD"
#define wTITLEBAR "UnrealIRCd"
#define OEMRESOURCE
//#define _WIN32_IE 0x0500


/* debug stuff*/
#define WINDEBUGLEVEL_0 0x01
#define WINDEBUGLEVEL_1 0x02
#define WINDEBUGLEVEL_2 0x04
#define WINDEBUGLEVEL_3 0x08
#define WINDEBUGLEVEL_FLUSH 0x10
#define WINDEBUG_FORCE 0x20
#define WINNOTIFY_0 0x0100
#define WINNOTIFY_1 0x0200
#define WINNOTIFY_2 0x0400
#define WINNOTIFY_3 0x0800
/*end*/
//#define ircnetwork "a network"
//#define netdomain "netdomain"
//#define helpchan "helpchan"
//#define IRCDTOTALVERSION "3"
#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

#include <windows.h>
#include "resource.h"
#include "version.h"
#include "setup.h"
#include <commctrl.h>

/* These came from ircd.c*/
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "userload.h"
//#include "services.h"
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include "h.h"
#include "Win32New.h"
#include <richedit.h>

/* end */
int	SetDebugLevel(HWND hWnd, int NewLevel);
void SetupPopups(HWND hDlg);
HWND CreateATreeView(HWND hwndParent/*, LPSTR lpszFileName*/,RECT rcClient); 
HTREEITEM AddItemToTree(HWND hWnd, LPSTR lpszItem, int nLevel);
void win_map(cptr, server, mask, prompt_length, length,hwTreeView);
LRESULT CALLBACK  WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK  MainDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK wStatusDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Dlg_IRCDRULES(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Dlg_IRCDMOTD(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Dlg_IRCDCONF(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Credits(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Dreamforge(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK IRCDLicense(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern  void      SocketLoop(void *dummy), s_rehash(), do_dns_async(HANDLE id);
void	windebug(level, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
extern struct /*current_load_struct */current_load_data;
LRESULT CALLBACK GraphCtlProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
void NiceQuit(void);
extern int max_client_count;
HINSTANCE hInst;
HWND hStatsWnd,hgraphwnd;
int windebuglevel = 0 ;
//| WINDEBUGLEVEL_0 | WINDEBUGLEVEL_1 | WINDEBUGLEVEL_2 | WINDEBUGLEVEL_3 | WINDEBUGLEVEL_FLUSH ;
int usernumhistory[530], usernumpointer;
char String[2048];
#include "version.h"
FILE *debugfile;
aConfiguration iConf;
char *version, *creation;
char shortversion[] = BASE_VERSION PATCH1;
char        szAppName[] = APPNAME; // The name of this application
char        szTitle[]   = wTITLEBAR; // The title bar text
HWND        hwIRCDWnd=NULL/* hwnd=NULL*/;
INFRICHLINE AllLines;
	NOTIFYICONDATA SysTray;
HANDLE      hMainThread = 0;
HMENU	ConfPopup, AboutPopup, DebugPopup;
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;
	int argc=0, yy;
	char *s, *argv[20], String[128];
	HWND hWnd;
    WSADATA WSAData;
	char meep[MAX_PATH];

	if ((debugfile = fopen("debugout2.log","ac"))==NULL)
		MessageBox(NULL, "UnrealIRCD/32 Initalization Error", "Unable to create debugout.log",MB_OK);
	windebug(WINDEBUG_FORCE,"Initializing Unreal wIRCd");
	windebug(WINDEBUG_FORCE,"%s compiled on %s %s",version,__DATE__,__TIME__);
	windebug(WINDEBUG_FORCE,"%s Last modifed %s",__FILE__,__TIMESTAMP__);


	/* Create a new instance */
    if ( WSAStartup(MAKEWORD(1, 1), &WSAData) != 0 )
    {
        MessageBox(NULL, "UnrealIRCD/32 Initalization Error", "Unable to initialize WinSock", MB_OK);
        return FALSE;
    }
	/* Store instance handle in our global variable */
	hInst = hInstance; 
    
	hWnd = CreateDialog(hInstance, "wIRCD", 0, MainDLG); 
	if ( !hWnd )
		return (FALSE);
	hwIRCDWnd = hWnd;

	{

	HICON hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(sysicondisabled), IMAGE_ICON,16, 16, 0);
	SysTray.cbSize = sizeof (NOTIFYICONDATA);
					 
	SysTray.hIcon  = hIcon;
	SysTray.hWnd   = hwIRCDWnd;
	lstrcpyn(SysTray.szTip, shortversion, sizeof(shortversion));;
	SysTray.uCallbackMessage = WM_USER;
	SysTray.uFlags =  NIF_ICON | NIF_TIP | NIF_MESSAGE; 
	SysTray.uID    = 0;
	
	Shell_NotifyIcon(NIM_ADD ,&SysTray);
	if (hIcon)
		{
		DestroyIcon(hIcon);
		}
	}

	/*
	**  We have initialised, start doing something usefull !
	*/
	/* Parse the command line */
	/*argv[0] = "WIRCD.EXE";*/
	if ( (s = /*lpCmdLine*/ GetCommandLine()) )
	    {
		argv[argc++] = s;
		/*while ( (s = strchr(s, ' ')) != NULL )
		    {*/
	//		while ( *s == ' ' ) *s++ = 0;
	//		argv[argc++] = s;
		   // }
	    }
	argv[argc] = NULL;

	if ( InitwIRCD(argc, argv) != 1 )
		{
		MessageBox(NULL,"Unreal IRCd for Windows has failed to initialise in InitwIRCD()","Error:",MB_OK);
		return FALSE;
		}

	/*Make sure we have the common controlls loaded ...
	 *And the Rich edit controll */
	InitCommonControls();
	LoadLibrary("RichEd20.Dll");
	{

	/* Setup WNDCLASS for graphy control*/
	WNDCLASS wc;
	memset(&wc,0,sizeof(WNDCLASS));

	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = GraphCtlProc;
	wc.cbWndExtra = 4;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "Graph";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);

    if (!RegisterClass (&wc))
      {
        MessageBox (NULL,"Error Initializing RegisterClass() for class Graphy",
                    (LPCTSTR) "UnrealIRCD/32 Initalization Error", MB_OK | MB_ICONEXCLAMATION);
      }
	}

	wsprintf(String, "UnrealIRCd/32 - %s", me.name);
	SetWindowText(hwIRCDWnd, String);
	SetDebugLevel(hwIRCDWnd, debuglevel);

	hMainThread = (HANDLE) _beginthread(SocketLoop, 0, NULL);
	hAccelTable = LoadAccelerators (hInstance, szAppName);
	{
		HICON hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(sysicon), IMAGE_ICON,16, 16, 0);
	SysTray.hIcon = hIcon;
	Shell_NotifyIcon(NIM_MODIFY ,&SysTray);
	if (hIcon)
		{
		DestroyIcon(hIcon);
		}
	}

	atexit(NiceQuit);

	/* Main message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	    {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
	    }

	windebug(WINDEBUG_FORCE,"Unreal Terminating ....");
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
	fclose(debugfile);

	return msg.wParam;
}

void NiceQuit(void)
{
	windebug(WINDEBUG_FORCE,"Unreal Terminating ....");
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
	fclose(debugfile);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_CREATE :
			return 0;

/*		case WM_USER:
			windebug(WINDEBUGLEVEL_2,"entering WM_USER");
			switch(lParam)
				{
				case WM_LBUTTONDOWN:
					/* We want to first ... get a pasword if required, then show the window*/
/*					if (hwIRCDWnd)
					{
						ShowWindow (hwIRCDWnd, SW_SHOW);
						ShowWindow (hwIRCDWnd,SW_RESTORE);
						SetForegroundWindow(hwIRCDWnd);
					}else{
						hwIRCDWnd = CreateDialog(hInst, "wIRCD", 0, MainDLG);
						ShowWindow (hwIRCDWnd, SW_SHOW);
					}
					break;
				}
			return 1;*/
	    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


LRESULT CALLBACK MainDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
int wmId, wmEvent;	
static HMENU hMenu, hSystemSubMenu, hHelpSubMenu;
switch (message)
	    {
		case WM_INITDIALOG:
			{
			/* ToDo :: ReWrite whole menu routine .. to actually work properly ... and use MF_(UN)CHECKED !
			**    -- David Flynn 15-02-2000 **/
			/* Done ! Now uses windebuglevel  -- David Flynn 16-02-2000 **/

			hMenu       = LoadMenu(hInst, "Menu_PopUp");
		    hSystemSubMenu = GetSubMenu(hMenu, 2);
		    hHelpSubMenu = GetSubMenu(hMenu, 3);
	        SetMenuDefaultItem(hSystemSubMenu,IDM_IRCDCONF, MF_BYCOMMAND);
			SetTimer (hDlg, UPDATE_TIMER, UPDATE_INTERVAL, NULL);
			}
			return 1;

		case WM_SIZE :
			windebug(WINDEBUGLEVEL_2,"recieved WM_SIZE");
			if (wParam & SIZE_MINIMIZED)
				{
					ShowWindow(hDlg,SW_HIDE);
					return 0;
				}

		case WM_USER:
			windebug(WINDEBUGLEVEL_2,"entering WM_USER");
			switch(lParam)
				{
				case WM_LBUTTONDOWN:
					/* We want to first ... get a pasword if required, then show the window*/
				/*	if (hwIRCDWnd)
					{*/
						ShowWindow (hDlg, SW_SHOW);
						ShowWindow (hDlg,SW_RESTORE);
						SetForegroundWindow(hDlg);
				//	}else{
				//		hwIRCDWnd = CreateDialog(hInst, "wIRCD", 0, MainDLG);
				//		ShowWindow (hwIRCDWnd, SW_SHOW);
				//	}
					break;
				}
			return 1;

		case WM_TIMER :
			switch(wParam)
				{
				case UPDATE_TIMER:
					usernumhistory[usernumpointer] = lu_clu;
					usernumpointer++;
					usernumpointer %= 370;
					if (hStatsWnd != NULL)
						PostMessage(hgraphwnd,(WM_USER +1), 0,0);
					return 1;
				}
			break;

		case WM_COMMAND:
			
			wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

			switch(LOWORD(wParam))
			    {
				case MM_WINDEBUGLEVEL_0 :
					/*Standard Level Debug ... not much stuff*/
					windebug(WINDEBUGLEVEL_1,"Recieved MM_WINDEBUGLEVEL_0, windebuglevel = %X", windebuglevel);
					if ((windebuglevel & WINDEBUGLEVEL_0) != 0)
						{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_0) != 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_0,MF_UNCHECKED);
						windebuglevel &= ~WINDEBUGLEVEL_0;
						}else{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_0) = 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_0,MF_CHECKED);
						windebuglevel |= WINDEBUGLEVEL_0;
						}
					windebug(WINDEBUGLEVEL_1,"finished MM_WINDEBUGLEVEL_0, windebuglevel = %X", windebuglevel);
					return 0;
				case MM_WINDEBUGLEVEL_1 :
					/*Higher Level ... Look at when functions are called*/
					windebug(WINDEBUGLEVEL_1,"Recieved MM_WINDEBUGLEVEL_1, windebuglevel = %X", windebuglevel);
					if ((windebuglevel & WINDEBUGLEVEL_1) != 0)
						{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_1) != 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_1,MF_UNCHECKED);
						windebuglevel &= ~WINDEBUGLEVEL_1;
						}else{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_1) = 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_1,MF_CHECKED);
						windebuglevel |= WINDEBUGLEVEL_1;
						}
					windebug(WINDEBUGLEVEL_1,"finished MM_WINDEBUGLEVEL_1, windebuglevel = %X", windebuglevel);
					return 0;
				case MM_WINDEBUGLEVEL_2 :
					/*Higher Still ... Look at what happens in functions*/
					windebug(WINDEBUGLEVEL_1,"Recieved MM_WINDEBUGLEVEL_2, windebuglevel = %X", windebuglevel);
					if ((windebuglevel & WINDEBUGLEVEL_2) != 0)
						{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_2) != 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_2,MF_UNCHECKED);
						windebuglevel &= ~WINDEBUGLEVEL_2;
						}else{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_2) = 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_2,MF_CHECKED);
						windebuglevel |= WINDEBUGLEVEL_2;
						}
					windebug(WINDEBUGLEVEL_1,"finished MM_WINDEBUGLEVEL_2, windebuglevel = %X", windebuglevel);
					return 0;
				case MM_WINDEBUGLEVEL_FLUSH :
					/*Make fflush happen on windebug()*/
					windebug(WINDEBUGLEVEL_1,"Recieved MM_WINDEBUGLEVEL_FLUSH, windebuglevel = %X", windebuglevel);
					if ((windebuglevel & WINDEBUGLEVEL_FLUSH) != 0)
						{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_FLUSH) != 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_FLUSH,MF_UNCHECKED);
						windebuglevel &= ~WINDEBUGLEVEL_FLUSH;
						}else{
						windebug(WINDEBUGLEVEL_1,"(windebuglevel & WINDEBUGLEVEL_FLUSH) = 0");
						CheckMenuItem(hSystemSubMenu,MM_WINDEBUGLEVEL_FLUSH,MF_CHECKED);
						windebuglevel |= WINDEBUGLEVEL_FLUSH;
						}
					windebug(WINDEBUGLEVEL_1,"finished MM_WINDEBUGLEVEL_FLUSH, windebuglevel = %X", windebuglevel);
					return 0;
				case IDM_ABOUT:
					DialogBox(hInst, "AboutBox", hDlg, (DLGPROC)About);
				return 0;
				case IDM_CREDITS:
					DialogBox(hInst, "AboutBox", hDlg, (DLGPROC)Credits);
				return 0;
				case IDM_DF:
					DialogBox(hInst, "AboutBox", hDlg, (DLGPROC)Dreamforge);
				return 0;
				case IDM_LICENSE:
					DialogBox(hInst, "AboutBox", hDlg, (DLGPROC)IRCDLicense);
				return 0;
				case IDM_IRCDCONF:
					DialogBox(hInst, "Dlg_IRCDCONF", hDlg, (DLGPROC)Dlg_IRCDCONF);
				return 0;
				case IDM_IRCDMOTD:
					DialogBox(hInst, "DLG_IRCDMOTD", hDlg, (DLGPROC)Dlg_IRCDMOTD);
				return 0;
				case IDM_IRCDRULES:
					DialogBox(hInst, "DLG_IRCDRULES", hDlg, (DLGPROC)Dlg_IRCDRULES);
				return 0;
				case IDM_REHASH:
					MessageBox(hDlg,"Server Rehashing ....","Unreal wIRCd3",MB_OK | MB_APPLMODAL);
					s_rehash();
				return 0;

				case IDM_EXIT:
					if ( MessageBox(hDlg, "Are you sure?",
						"Terminate UnrealIRCD/32",
						MB_ICONQUESTION | MB_YESNO) == IDNO )
						return 0;
					DestroyWindow(hDlg);
					return 0;
			    }
			return 0;

		case WM_RBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK:
         // emulate default menu item for double click
         	DialogBox(hInst, "Dlg_IRCDCONF", hDlg, (DLGPROC)Dlg_IRCDCONF);
         break;

      case WM_LBUTTONDOWN: {
         POINT p;
	             p.x = LOWORD(lParam);
		     p.y = HIWORD(lParam);
		     /* Config popup */
		     if ((p.x >= 149) && (p.x <= 198)
		      && (p.y >= 173) && (p.y <= 186))
                     {

         	     	ClientToScreen(hDlg,&p);
			TrackPopupMenu(hSystemSubMenu,
				  TPM_LEFTALIGN|TPM_LEFTBUTTON,
           			 p.x,p.y,0,hDlg,NULL);

			return 0;
		      }
		     /* about popup */
		     if ((p.x >= 206) && (p.x <= 252)
		      && (p.y >= 173) && (p.y <= 186))
                     {
         	     	ClientToScreen(hDlg,&p);
			TrackPopupMenu(hHelpSubMenu,
				  TPM_LEFTALIGN|TPM_LEFTBUTTON,
           			 p.x,p.y,0,hDlg,NULL);
			 return 0;
		      }
                      /* rehash button */
		     if ((p.x >= 31) && (p.x <= 81)
		      && (p.y >= 173) && (p.y <= 186))
                     {
		      	PostMessage(hDlg, WM_COMMAND, IDM_REHASH, 0); 
			 return 0;
		      }
                      /* quit button */
		     if ((p.x >= 264) && (p.x <= 328)
		      && (p.y >= 173) && (p.y <= 186))
                     {
			 PostMessage(hDlg, WM_COMMAND, IDM_EXIT, 0);
			 return 0;
		      }
		      /* status button */
		     if ((p.x >= 93) && (p.x <= 138)
		      && (p.y >= 173) && (p.y <= 186))
                     {
			if (hStatsWnd == NULL){
			 DialogBox(hInst, /*"WIRCDSTATUS"*/"DLG_STATS", NULL, (DLGPROC)wStatusDLG);
			 return 0;
				}else{
				if (SetForegroundWindow(hStatsWnd)==0){
					windebug(WINDEBUG_FORCE,"Error at BringWindowToTop(), Err=%d",GetLastError());
					}
				return 0;
				}
				 }
		                           
		      break;
		}
		case WM_CLOSE:
		 PostMessage(hDlg, WM_COMMAND, IDM_EXIT, 0);
			
			return 0;
			/* Fall through to destroy the window and then send to WM_QUIT to the msg que*/

		case WM_DESTROY:
		    KillTimer(hDlg, UPDATE_TIMER);
			localdie();

							/* Never returns *//* i hope it does ... */
							/* Ok ... It doesnt ... That _NEEDS_ Fixing !!!!!*/
							/* Fixed -- i hope */
			PostQuitMessage(0);
			return 0;

		case WM_QUIT:
			if ( MessageBox(hDlg, "WM_QUIT Are you sure?", "Terminate UnrealIRCd/32",
					MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL) == IDNO )
				return 0;

			return 0;

	    }
	DefWindowProc(hDlg, message, wParam, lParam);
    return 0;
}



LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	    {
		case WM_INITDIALOG:
#define Ccat strcat(String, String2)
		    {
			char	String[16384], String2[16384], **s = infotext;
			wsprintf(String, "%s\n%s", version, creation);
			SetDlgItemText(hDlg, IDC_VERSION, String);
			String[0] = 0; String2[0] = 0;
			wsprintf(String2, "-=-=-=-=-=-==-==- %s -=-=-==-==-=-=-=-=-=-\r\n", ircnetwork); Ccat;
			wsprintf(String2, "|Web Page:      | http://www.%s\r\n", netdomain); Ccat;
			wsprintf(String2, "|FTP Archive:   | ftp://ftp.%s\r\n", netdomain); Ccat;
			wsprintf(String2, "|Help channel:  | %s\r\n", helpchan); Ccat;
			wsprintf(String2, "|=-=-=-=-=-==-==|-=-=-=-=-=-=-==-==-=-=-=-=-=-=-=\r\n"); Ccat;
			wsprintf(String2, "|IRCd version:  | %s\r\n", IRCDTOTALVERSION); Ccat;			
			wsprintf(String2, "| Programmer:   | Stskeeps <stskeeps@tspre.org>\r\n"); Ccat;
			wsprintf(String2, "| Win32 Coders: | DrBin <drbin@tspre.org>\r\n"); Ccat;
			wsprintf(String2, "|               | codemastr <codemastr@tspre.org>\r\n"); Ccat; 
			wsprintf(String2, "|               | {X} <x@tspre.org>\r\n"); Ccat; 
#if defined(_WIN32) && defined(WIN32_SPECIFY)
			wsprintf(String2, "| Win32 Porter: | %s\r\n", WIN32_PORTER); Ccat;
			wsprintf(String2, "|     >>URL:    | %s\r\n", WIN32_URL); Ccat;
#endif
			wsprintf(String2, "|Credits:       | Type /Credits\r\n"); Ccat;
			wsprintf(String2, "|DALnet Credits:| Type /DALinfo\r\n"); Ccat;
			wsprintf(String2, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\r\n"); Ccat;
			wsprintf(String2, "| Unreal IRCd can be downloaded at http://unreal.tspre.org\r\n"); Ccat;	
			wsprintf(String2, "| This notice may not be removed from the IRCd package\r\n"); Ccat;
			wsprintf(String2, "| It will be a violation of copyright. This program must always\r\n"); Ccat;
			wsprintf(String2, "| stay free of charge being sold commercially or privately\r\n"); Ccat;
			wsprintf(String2, "| Only charge may be for the transport medium like on CD-ROM, floppy\r\n"); Ccat;
			wsprintf(String2, "| or other kinds (-Stskeeps'1999)\r\n"); Ccat;
			wsprintf(String2, "--------------------------------------------\r\n"); Ccat;
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

			wsprintf(String, "%s\n%s", version, creation);
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

			wsprintf(String, "%s\n%s", version, creation);
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

			wsprintf(String, "%s\n%s", version, creation);
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

/*
 *  FUNCTION: wStatusDLG(HWND, unsigned, WORD, LONG)
 *
 *  PURPOSE:  Processes messages for "DLG_IRCDCONF" dialog box
 *
 */
HWND hreditwnd;
LRESULT CALLBACK wStatusDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
static HWND hwTreeView/*hreditwnd*/;
POINT p;
char string[1024];
const int i=0;
LPNMTREEVIEW lpnmtv;
static HWND hgraph;

			RECT TV;
	switch (message)
    {
        case WM_INITDIALOG:
            {
CHARFORMAT2 FontFormat;
WINSTATS  pWSTATS /*= (pWINSTATS) LocalAlloc (LPTR, sizeof(pWINSTATS))*/;
			windebug(WINDEBUGLEVEL_1,"wStatusDLG recieved WM_INITDIALOG");
			hStatsWnd = hDlg;
			TV.left=14;
			TV.right=140;
			TV.top=14;
			TV.bottom=240;
			SetLastError(0);
			if ((hreditwnd=CreateWindowEx(WS_EX_CLIENTEDGE,RICHEDIT_CLASS ,"Unreal wIRCd 3", WS_VISIBLE | WS_CHILD |
				 ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY ,
							0, 245, 535, 185, hDlg, NULL, hInst, NULL))==NULL)
							MessageBox(NULL,"garhle","grargle", MB_OK);
			if (GetLastError()!=0)
				windebug(WINDEBUG_FORCE,"error %d",GetLastError());

			FontFormat.cbSize = sizeof(CHARFORMAT2);
			FontFormat.yHeight = 8 * 20 /* We measure it in twips (1/20th of a point !)*/;
			FontFormat.crTextColor = RGB(0,150,0);
			lstrcpyn(FontFormat.szFaceName, "Lucida Console", sizeof("Lucida Console"));;;
			FontFormat.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_PROTECTED;

			SendMessage(hreditwnd,EM_SETCHARFORMAT,SCF_ALL,&FontFormat);



			hwTreeView = CreateATreeView(hDlg,TV);
	        win_map(NULL, &me, "*", 0, 60,hwTreeView);
			windebug(WINDEBUGLEVEL_2,"hDlg = %d",hDlg);
			windebug(WINDEBUGLEVEL_2,"win_map completed");
		//	for (i=0;i<TreeView_GetCount(hwTreeView);i++)
		//		{
		//		windebug(WINDEBUGLEVEL_2,"Allocating pWSTATS for i=%d of max %d",i,TreeView_GetCount(hwTreeView));
				{
				
				windebug(WINDEBUGLEVEL_2,"Allocated pWSTATS");
				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.connections");
				pWSTATS.connections=current_load_data.conn_count;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.connections=%d",pWSTATS.connections);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.CurrGlobUsers");
				pWSTATS.CurrGlobUsers=lu_cglobalu;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.CurrGlobUsers=%d",pWSTATS.CurrGlobUsers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.CurrLoclUsers");
				pWSTATS.CurrLoclUsers=lu_clu;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.CurrLoclUsers=%d",pWSTATS.CurrLoclUsers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.MaxGlobUsers");
				pWSTATS.MaxGlobUsers=max_client_count;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.MaxGlobUsers=%d",pWSTATS.MaxGlobUsers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.NumUsers");
				pWSTATS.NumUsers=lu_noninv;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.NumUsers=%d",pWSTATS.NumUsers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.Invisible");
				pWSTATS.Invisible=lu_inv;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.Invisible=%d",pWSTATS.Invisible);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.Servers");
				pWSTATS.Servers=TreeView_GetCount(hwTreeView);
				windebug(WINDEBUGLEVEL_2,"pWSTATS.Servers=%d",pWSTATS.Servers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.LocalClients");
				pWSTATS.LocalClients=lu_mlu;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.LocalClients=%d",pWSTATS.LocalClients);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.LocalServers");
				pWSTATS.LocalServers=lu_lserv;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.LocalServers=%d",pWSTATS.LocalServers);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.NumIRCops");
				pWSTATS.NumIRCops=lu_oper;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.NumIRCops=%d",pWSTATS.NumIRCops);

				windebug(WINDEBUGLEVEL_2,"assigning pWSTATS.chans");
				pWSTATS.chans=lu_channel;
				windebug(WINDEBUGLEVEL_2,"pWSTATS.chans=%d",pWSTATS.chans);
				wsprintf(string, "%d",pWSTATS.CurrLoclUsers);
				SetDlgItemText(hDlg,EDIT_CLOCAL,string);
				wsprintf(string, "%d",pWSTATS.MaxLoclUsers);
				SetDlgItemText(hDlg,EDIT_CLOCALMAX,string);
				wsprintf(string, "%d",pWSTATS.Invisible);
				SetDlgItemText(hDlg,EDIT_INV,string);
				wsprintf(string, "%d",pWSTATS.NumUsers);
				SetDlgItemText(hDlg,EDIT_NONINV,string);
				wsprintf(string, "%d",pWSTATS.Servers);
				SetDlgItemText(hDlg,EDIT_IRCSERVERS,string);
				wsprintf(string, "%d",pWSTATS.LocalClients);
				SetDlgItemText(hDlg,EDIT_MYUSERS,string);
				wsprintf(string, "%d",pWSTATS.LocalServers);
				SetDlgItemText(hDlg,EDIT_MYSERVERS,string);
				wsprintf(string, "%d",pWSTATS.CurrGlobUsers);
				SetDlgItemText(hDlg,EDIT_GLOBAL,string);
				wsprintf(string, "%d",pWSTATS.MaxLoclUsers);
				SetDlgItemText(hDlg,EDIT_GLOBALMAX,string);
				wsprintf(string, "%d",pWSTATS.NumIRCops);
				SetDlgItemText(hDlg,EDIT_IRCOPS,string);
				wsprintf(string, "%d",pWSTATS.chans);
				SetDlgItemText(hDlg,EDIT_CHANNELS,string);
				wsprintf(string, "%d",MAXCLIENTS);
				SetDlgItemText(hDlg,EDIT_LOCALMAXPOS,string);

 SetLastError(0);
	//			SetWindowLong (hDlg,i, (LONG) pWSTATS);
	  windebug(WINDEBUGLEVEL_2,"Saved (pWSTATS) into (hDlg) offset = i = %d, Last error =%u",i,GetLastError());
//LocalFree (LocalHandle ((LPVOID) pWSTATS));

				//store them in a safe place that we can find later
				
				}
			//	}
			//	{


			if ((hgraph=CreateWindowEx(WS_EX_CLIENTEDGE,"Graph","m",WS_VISIBLE | WS_CHILD, 166, 70, 364, 70, hDlg, NULL, hInst, NULL))==NULL)
			       {
				   MessageBox (NULL,
                    "Error Creating Graph Control \n CreateWindow (\"Graph\", \"\",WS_VISIBLE , 335, 25, 100, 400, hDlg, NULL, hInst, NULL)","Error CreatingWindow",
                    MB_OK | MB_ICONEXCLAMATION);
				   }
				   windebug(WINDEBUGLEVEL_2,"CreateWindowEx==Success");
				   	UpdateWindow(hgraph);
					windebug(WINDEBUGLEVEL_1,"UpdateWindow");
			//}
			/*	{
				pWINSTATS pWSTATS = (pWINSTATS) GetWindowLong (hDlg, 0);
				if (pWSTATS == NULL)
					{
					windebug(WINDEBUGLEVEL_1,"Error Getting pWSTATS from GetWindowLong (hDlg,0) -> LastError = %u",GetLastError());
					}else{

						}
				}*/
		    }
			return (TRUE);

		case WM_DESTROY:
		//	MessageBox(NULL,"destroying ....","WM_DESTROY",MB_OK);
		//	windebug(WINDEBUG_FORCE,"hreditwnd = %d , TreeViewCount = %d",hreditwnd,TreeView_GetCount(hwTreeView));
//			for (i=0;i<TreeView_GetCount(hwTreeView);i++)
			{
	//			pWINSTATS pWSTATS = (pWINSTATS) GetWindowLong (hDlg, i);
	//			LocalFree (LocalHandle ((LPVOID) pWSTATS));
			}
			DestroyWindow(hreditwnd);
			return (TRUE);
			
		case WM_NOTIFY: 
			switch (((LPNMHDR) lParam)->code) {
				case TVN_SELCHANGED :
					     lpnmtv = ((LPNMTREEVIEW) lParam);
						 
				wsprintf(string, "%d",lpnmtv->itemNew.hItem);
				SetDlgItemText(hDlg,EDIT_CHANNELS,string);
						 
				break; 

				// Handle other notifications here. 

				}
			break; 

	    case WM_RBUTTONDOWN: {
   		     p.x = LOWORD(lParam);
   		     p.y = HIWORD(lParam);
		     wsprintf(String, "Clicked at (%li, %li)", p.x, p.y);
		     SetWindowText(hDlg, String);
			}
			return TRUE;
		case WM_COMMAND:
			if ( LOWORD(wParam) == IDOK )
            {
  
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

HTREEITEM AddItemToTree (HWND hWnd, LPSTR lpszItem, int nLevel)
{
	
    TVITEM tvi; 
    TVINSERTSTRUCT tvins; 
    static HTREEITEM hPrev = (HTREEITEM) TVI_FIRST; 
    static HTREEITEM hPrevRootItem = NULL; 
    static HTREEITEM hPrevLev2Item = NULL; 
    HTREEITEM hti; 
 
    tvi.mask = TVIF_TEXT  | TVIF_PARAM; 
 
    // Set the text of the item. 
    tvi.pszText = lpszItem; 
    tvi.cchTextMax = lstrlen(lpszItem); 
 
    // Assume the item is not a parent item, so give it a 
    // document image. 
//    tvi.iImage = g_nDocument; 
//    tvi.iSelectedImage = g_nDocument; 
 
    // Save the heading level in the item's application-defined 
    // data area. 
    tvi.lParam = (LPARAM) nLevel; 
 
    tvins.item = tvi; 
    tvins.hInsertAfter = hPrev; 
 
    // Set the parent item based on the specified level. 
    if (nLevel == 1) 
        tvins.hParent = TVI_ROOT; 
    else if (nLevel == 2) 
        tvins.hParent = hPrevRootItem; 
    else 
        tvins.hParent = hPrevLev2Item; 
 
    // Add the item to the tree view control. 
    hPrev = (HTREEITEM) SendMessage(hWnd, TVM_INSERTITEM, 0, 
         (LPARAM) (LPTVINSERTSTRUCT) &tvins); 
 
    // Save the handle to the item. 
    if (nLevel == 1) 
        hPrevRootItem = hPrev; 
    else if (nLevel == 2) {
        hPrevLev2Item = hPrev; 
		TreeView_EnsureVisible(hWnd,hPrev);
		}
    // The new item is a child item. Give the parent item a 
    // closed folder bitmap to indicate it now has child items. 
    if (nLevel > 1) { 
        hti = TreeView_GetParent(hWnd, hPrev); 
        tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE; 
        tvi.hItem = hti; 
//        tvi.iImage = g_nClosed; 
//        tvi.iSelectedImage = g_nClosed; 
        TreeView_SetItem(hWnd, &tvi); 
    } 
 
    return hPrev; 
} 


// CreateATreeView - creates a tree view control. 
// Returns the handle to the new control if successful,
// or NULL otherwise. 
// hwndParent - handle to the control's parent window. 
// lpszFileName - name of the file to parse for tree view items.

HWND CreateATreeView(HWND hwndParent/*, LPSTR lpszFileName*/,RECT rcClient) 
{ 
   // RECT rcClient;  // dimensions of client area 
    HWND hwndTV;    // handle to tree view control 
 
    // Ensure that the common control DLL is loaded. 
    InitCommonControls(); 
 
    // Get the dimensions of the parent window's client area, and create 
    // the tree view control. 
    //GetClientRect(hwndParent, &rcClient); 
    hwndTV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, "Tree View", 
        WS_VISIBLE | WS_CHILD /*| WS_BORDER | */| TVS_HASBUTTONS /*| TVS_DISABLEDRAGDROP 
		| TVS_SHOWSELALWAYS */ | TVS_HASLINES, 
        0, 0, rcClient.right, rcClient.bottom, 
        hwndParent, NULL/*(HMENU) ID_TREEVIEW*/, hInst, NULL); 
 
                     
    // Initialize the image list, and add items to the control. 
    // InitTreeViewImageLists and InitTreeViewItems are application- 
    // defined functions. 
 /*   if (!InitTreeViewImageLists(hwndTV) || 
            !InitTreeViewItems(hwndTV, lpszFileName)) { 
        DestroyWindow(hwndTV); 
        return FALSE; 
    }*/ 
    return hwndTV;
} 

/*
 * Based on the New /MAP format [dump_map()] -Potvin
 * Now used to create list of servers for server list tree view -- David Flynn
 */
void win_map(cptr, server, mask, prompt_length, length, hwTreeView)
aClient *cptr, *server;
char *mask;
register int prompt_length;
int length;
HWND hwTreeView;
{
        static char prompt[64];
        register char *p = &prompt[prompt_length];
        register int cnt = 0, local = 0;
        aClient *acptr;


                for (acptr = client; acptr; acptr = acptr->next)
                {
                    if (IsPerson(acptr))
                    {
                                ++cnt;                       /* == */
                                if (!strcmp(acptr->user->server, server->name)) ++local;
                    }
                }

            //    sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, prompt, length, server->name,
			//		local, (local*100)/cnt );
				AddItemToTree (hwTreeView,server->name,1+prompt_length);
                cnt = 0;
            
/*        if (prompt_length > 0)
        {
                p[-1] = ' ';
                if (p[-2] == '`') p[-2] = ' ';
        }*/

//        if (prompt_length > 60) return;

        strcpy(p, "|-");


        for (acptr = client; acptr ; acptr = acptr->next)
        {
                if ( !IsServer (acptr) || strcmp(acptr->serv->up, server->name)) continue;

                if ( match(mask, acptr->name) )
                        acptr->flags &= ~FLAGS_MAP;
                else
                {
                        acptr->flags |= FLAGS_MAP;
                        cnt++;
                }
        }

        for (acptr = client; acptr ; acptr = acptr->next)
        {
                if ( ! (acptr->flags & FLAGS_MAP) ||          /* != */
                     !IsServer (acptr) || strcmp(acptr->serv->up, server->name)) continue;
                if (--cnt == 0) *p = '`';
                win_map (cptr, acptr, mask, prompt_length+1, length-2,hwTreeView);
        }

        if (prompt_length > 0) p[-1] = '-';
}

/****************************************************************
**********    Graphy !!!! (C) David Flynn 2000 *****************/

LRESULT CALLBACK GraphCtlProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
		 HDC mem;
		 PAINTSTRUCT ps;
		 RECT r;

	switch (msg) {
    case WM_CREATE:
		{
		RECT temprect;
		HDC            hdc;
        LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
        PGRAPHINFO  pSCI = (PGRAPHINFO) LocalAlloc (LPTR, sizeof(GRAPHINFO));
        if (!pSCI)
			{
			MessageBox (NULL,
                    "It Seems that you have a potential problem ... \n This function dont want to do a LocallAlloc() ... Report this at once !!!!",
                    (LPCTSTR) "GRAPHCNT.DLL",
                    MB_OK | MB_ICONEXCLAMATION);
			return -1;
			}
		hgraphwnd=hwnd;
		/*
		** I am initialising the "constants" now ... as they could be a root to my problems
		*/
		pSCI->Grid        = 0x00008000;
		pSCI->BackColor   =	0x00000000;
		pSCI->GridColor   =	0x00008000;
		pSCI->CPUColor    = 0x00000000;

		GetClientRect(hwnd,&temprect);
		pSCI->WindowSize.cx = temprect.right;
		pSCI->WindowSize.cy = temprect.bottom;

	    windebug(WINDEBUGLEVEL_2,"memset(pSCI->cpuhistory, -1, 2048)");
		memset(pSCI->cpuhistory, -1, 2048);
		windebug(WINDEBUGLEVEL_2,"memset completed");
		windebug(WINDEBUGLEVEL_2,"hwnd = %d",hwnd);

		/*
		** Alloc the compatible DC for this control.
		*/

		pSCI->DCBack = GetDC (hwnd);
		windebug(WINDEBUGLEVEL_2,"got hdc");

		pSCI->DCDblBuff = CreateCompatibleDC (pSCI->DCBack);

		if (pSCI->DCDblBuff == NULL)
			{
			windebug(WINDEBUG_FORCE,"Damn Bloody Errors ... pSCI->DCDblBuff == NULL ... GAH !");
			}else{
			windebug(WINDEBUGLEVEL_2,"[((pSCI->DCDblBuff (=%d) = CreateCompatibleDC (hdc)) != NULL)] ~> GOOD!!!",pSCI->DCDblBuff);
			}
		/*
		** Baa ... What is going on here, i ask myself ... referencing things before i have initialised them ..
		**   -- fixed !!!!
		*/

		pSCI->BMDblBuff = CreateCompatibleBitmap(pSCI->DCBack, pSCI->WindowSize.cx, pSCI->WindowSize.cy);

		if ((pSCI->BMDblBuff == NULL))
			{
			windebug(WINDEBUG_FORCE,"According to my calculations ... pSCI->BMDblBuff = NULL --Ooops");
			}else{
			windebug(WINDEBUGLEVEL_2,"Created Compatable Bitmap (pSCI->BMDblBuff) from (pSCI->DCBack)");
			}	  

		pSCI->OldDblBuff = (HBITMAP)SelectObject(pSCI->DCDblBuff, pSCI->BMDblBuff);
		windebug(WINDEBUGLEVEL_2,"Selected (pSCI->BMDblBuff) into (pSCI->DCDblBuff) -- old value saved to (pSCI->OldDblBuff)");

		ReleaseDC (hwnd, pSCI->DCBack);
		windebug(WINDEBUGLEVEL_2,"Released (hdc)");



		SetLastError(0);
		  
		SetWindowLong (hwnd, GWL_GRAPHDATA, (LONG) pSCI);
		  
//	  windebug(WINDEBUGLEVEL_2,"Saved (pSCI) into (hwnd) offset = GWL_GRAPHDATA, Last error =%u",GetLastError());
//      SetTimer (hwnd, GRAPH_EVENT, UPDATE_INTERVAL, NULL);
//	  windebug(WINDEBUGLEVEL_2,"SetTimer");

         return 1;
		}
	case WM_ERASEBKGND:
		{
		windebug(WINDEBUGLEVEL_2,"recieved WM_ERASEBKGND");
		}
   		 return 1;
	case WM_PAINT:
		windebug(WINDEBUGLEVEL_2,"recieved WM_PAINT");
		SetLastError(0);
			{
		PGRAPHINFO pSCI = (PGRAPHINFO) GetWindowLong (hwnd, GWL_GRAPHDATA);
		if (pSCI==NULL)
			{
			windebug(WINDEBUGLEVEL_2,"Error from GetWindowLong ~> %u",GetLastError);
			return FALSE;
			}
		windebug(WINDEBUGLEVEL_2,"got pSCI from GetWindowLong()");
		windebug(WINDEBUGLEVEL_2,"Testing pSCI .... pSCI->BMDblBuff=%d",pSCI->BMDblBuff);
		 GetClientRect(hwnd, &r);
		 windebug(WINDEBUGLEVEL_2,"got GetClientRect()=&r= r.bottom->%d, r.left->%d, r.right->%d, r.top->%d",r.bottom ,r.left,r.right,r.top);

		 DrawMonitor(pSCI->DCDblBuff, r,hwnd);
		 windebug(WINDEBUGLEVEL_2,"DrawMonitor returned");
		 mem = BeginPaint(hwnd, &ps);
		 BitBlt(mem, 0, 0, pSCI->WindowSize.cx, pSCI->WindowSize.cy, pSCI->DCDblBuff, 0, 0, SRCCOPY);
		 EndPaint(hwnd, &ps);
         DeleteDC(mem);
		 return 0;
		}

	case (WM_USER + 1):
			GetClientRect(hwnd, &r);
			InvalidateRect(hwnd, &r, FALSE);

	case WM_TIMER:
	  windebug(WINDEBUGLEVEL_2,"recieved WM_TIMER");
      switch (wParam)
      {
        case GRAPH_EVENT:
        {
			windebug(WINDEBUGLEVEL_2,"  -> Specifically GRAPH_EVENT");
			{PGRAPHINFO pSCI = (PGRAPHINFO) GetWindowLong (hwnd, GWL_GRAPHDATA);
			if (pSCI == NULL)
				windebug(WINDEBUGLEVEL_2,"Failed retreval of pSCI");
			pSCI->cpuhistory[pSCI->cpupointer] = (rand()%100)/1.5+20/**data*/;
			pSCI->cpupointer++;
			pSCI->cpupointer %= pSCI->width;
		SetWindowLong (hwnd, GWL_GRAPHDATA, (LONG) pSCI);
			// Invalidate client. This causes a WM_PAINT message to be sent to our window
			GetClientRect(hwnd, &r);
			InvalidateRect(hwnd, &r, FALSE);
          break;
			}
        }
      }

      break;


    case WM_DESTROY:
        {

		PGRAPHINFO pSCI = (PGRAPHINFO) GetWindowLong (hwnd, GWL_GRAPHDATA);
//	    KillTimer(hwnd, GRAPH_EVENT);
		hStatsWnd = NULL;
		//New DoubleBuffer Code
		//SelectObject(DCBack, OldBack);

	    if(pSCI->DCBack)
		   DeleteDC(pSCI->DCBack);
    	if(pSCI->BMBack)
	       DeleteObject(pSCI->BMBack);
     	//Free our DoubleBuffer Handles

		if(pSCI->DCDblBuff) {
			SelectObject(pSCI->DCDblBuff, pSCI->OldDblBuff);
			DeleteDC(pSCI->DCDblBuff);
		}

		pSCI->DCDblBuff = NULL;

		if(pSCI->BMDblBuff) {
			DeleteObject(pSCI->BMDblBuff);
		}

		pSCI->BMDblBuff = NULL;


		      LocalFree (LocalHandle ((LPVOID) pSCI));
        }
		break;

	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}

	return 0;
}

void DrawMonitor(HDC hdc, RECT r,HWND hwnd)
{
	{
	windebug(WINDEBUGLEVEL_2,"Entering DrawMonitor"); 
	}
	{
		PGRAPHINFO pSCI = (PGRAPHINFO) GetWindowLong (hwnd, GWL_GRAPHDATA);
 


 int i, j;
 HBRUSH brush;
 HPEN pen, oldpen, dotpen;
 HBITMAP graphmask;
 HDC image, mask;
 int height;
int BorderTop = 0, BorderBottom = 0, BorderLeft= 0,BorderRight=0;
windebug(WINDEBUGLEVEL_2,"Got pSCI from hwnd (=%d)",hwnd);
windebug(WINDEBUGLEVEL_2,"initialised vars");
 // Set border
 r.top += BorderTop;
 r.bottom -= BorderBottom;
 r.left += BorderLeft;
 r.right -= BorderRight;
windebug(WINDEBUGLEVEL_2,"initialised vars -- 1");
 pSCI->width  = (r.right - r.left); // Width of graph
 height = (r.bottom - r.top); // Height of graph
windebug(WINDEBUGLEVEL_2,"initialised vars -- 2");
windebug(WINDEBUGLEVEL_2,"pSCI->Background=%d,pSCI->BMBack=%d,pSCI->BMDblBuff=%d,pSCI->BackColor=%d,pSCI->GridColor=%d,pSCI->WindowSize.cx=%d,pSCI->WindowSize.cy=%d,pSCI->cpupointer=%d",pSCI->Background,pSCI->BMBack,pSCI->BMDblBuff,pSCI->BackColor,pSCI->GridColor,pSCI->WindowSize.cx,pSCI->WindowSize.cy,pSCI->cpupointer);

	// Draw Background
 /*if (Background)
   {
    HDC src = CreateCompatibleDC(NULL);
    SelectObject(src, Background);
    BitBlt(hdc, 0, 0, WindowSize.cx, WindowSize.cy, src, 0, 0, SRCCOPY);
	DeleteObject(src);
   }
 else*/
   {
    brush = CreateSolidBrush(pSCI->BackColor);
    FillRect(hdc, &r, brush);
    DeleteObject(brush);
   }

	// Draw Grid
  if (pSCI->Grid) {
		pen = CreatePen(PS_SOLID, 1, pSCI->GridColor);
		oldpen = SelectObject(hdc, pen);

		for (i=1; i<((r.bottom - r.top)%10 + 1); i++) {
			MoveToEx(hdc, r.left, r.top +/* ((r.bottom - r.top) / 10)*/ 10 * i, NULL);
			LineTo(hdc, r.right/*-1*/, r.top + /*((r.bottom - r.top) / 10)*/ 10 * i);
			windebug(WINDEBUGLEVEL_2,"i =%d, pen = %d , oldpen = %d , pSCI->Grid = %d", i,pen,oldpen,pSCI->Grid);
		}

		for (i=1; i<((r.right-r.left)/10+/*pSCI->cpupointer*/usernumpointer); i++) {
			MoveToEx(hdc, r.left + ((/*(r.right-r.left) / */ 10)*i-(/*pSCI->cpupointer*/usernumpointer)), r.top, NULL);
			LineTo(hdc, r.left + ((/*(r.right-r.left)/ */ 10)*i-(/*pSCI->cpupointer*/usernumpointer)), r.bottom/*-1*/);
		}

		SelectObject(hdc, oldpen);
		DeleteObject(pen);
	}
windebug(WINDEBUGLEVEL_2,"Drawn Grid");

/* mask = CreateCompatibleDC(NULL);

 graphmask = CreateCompatibleBitmap(mask, pSCI->WindowSize.cx, pSCI->WindowSize.cy);
 SelectObject(mask, graphmask);

 brush = CreateSolidBrush(0x0FFFFFF);
 FillRect(mask, &r, brush);
 DeleteObject(brush);

 pen = CreatePen(PS_SOLID, 1, 0x0FFFFF);
 SelectObject(mask, pen);
 oldpen = SelectObject(mask, pen);*/


 j = (/*pSCI->cpupointer*/ usernumpointer + pSCI->width) % pSCI->width;
 if (usernumhistory/*pSCI->cpuhistory*/[j-1]!= -1){ 
 //MoveToEx(mask, r.left, r.bottom-(height * pSCI->cpuhistory[j]/100), NULL);

 for (i=0; i<pSCI->width; i++)
   {
    j++;
	j %= pSCI->width;
//	MoveToEx(mask, r.left+i, r.bottom-(height * pSCI->cpuhistory[j]/100), NULL);
  //  LineTo(mask, r.left+i, r.bottom);
//windebug(WINDEBUGLEVEL_2,"j = %d pSCI->width= %d , i= %d , pSCI->cpuhistory[j]=%d pSCI->cpuhistory[j+1]= %d pSCI->cpuhistory[j-1]= %d",j,pSCI->width,i,pSCI->cpuhistory[j],pSCI->cpuhistory[j+1],pSCI->cpuhistory[j-1]);
			if (i==0)
				MoveToEx(hdc, r.left-1, r.bottom-(pSCI->width * usernumhistory/*pSCI->cpuhistory*/[j]/ MAXCLIENTS /*(1+lu_mlu+(lu_mlu*(75/100)))*/), NULL);
			dotpen = CreatePen(PS_SOLID, 0, 0x0000FF00);
		    oldpen = SelectObject(hdc, dotpen);
			LineTo(hdc, r.left+i, r.bottom-(pSCI->width * usernumhistory/*pSCI->cpuhistory*/[j]/ MAXCLIENTS/*(1+lu_mlu+(lu_mlu*(75/100)))*/));
            SelectObject(hdc, oldpen);
		    DeleteObject(dotpen);

   }
	 }
// SelectObject(mask, oldpen);
// DeleteObject(pen);

// image = CreateCompatibleDC(NULL);
// SelectObject(image, CPUMap);

// BitBlt(hdc, BorderLeft, BorderTop,pSCI->width, height, image, BorderLeft, BorderTop, SRCINVERT);
 //BitBlt(hdc, BorderLeft, BorderTop,width, height, mask,  BorderLeft, BorderTop, SRCAND);
// BitBlt(hdc, BorderLeft, BorderTop,pSCI->width, height, image, BorderLeft, BorderTop, SRCINVERT);

 DeleteObject(graphmask);

// DeleteObject(CPUMap);

 DeleteDC(mask);
// DeleteDC(image);
 SetWindowLong (hwnd, GWL_GRAPHDATA, (LONG) pSCI);
}

}


/***************************************************************
*****************   Debug Code Added 14-02 *********************
*****************   Modified 16-02         ********************/

void	windebug(level, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
int	level;
char	*form, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
{
	static	char	windebugbuf[1024];
	static	char	windebugbuf2[1024];
	char tmpbuf[128];
	char tmpbuf2[128];
/*
0x0000001 = General Debug
0x0000002 = Graphy Specific Debug
0x0000004 = [undefined]
0x0000008 = [undefined]
0x0000010 = [undefined]
*/
	if (((windebuglevel & level) != 0)||(level & WINDEBUG_FORCE))
		{
			int	err = WSAGetLastError();
			(void)sprintf(windebugbuf, form,
				p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			_strdate( tmpbuf );
			_strtime( tmpbuf2 );
			(void)sprintf(windebugbuf2, "%s %s::%s", tmpbuf,tmpbuf2,windebugbuf );


		strcat(windebugbuf2, "\n");
		fprintf(debugfile,windebugbuf2);
		if (((windebuglevel & WINDEBUGLEVEL_FLUSH) != 0)||(level & WINDEBUG_FORCE))
			fflush(debugfile);
		WSASetLastError(err);
	  }
if (0)
	{
SETTEXTEX TextEx;
TextEx.codepage = CP_ACP;
SendMessage(hreditwnd,EM_SETTEXTEX,&TextEx,form);
	}
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

/**************************************************************/
int GUI_TextWaiting(char *szInBuf,int iLength)
{
return 0;
}