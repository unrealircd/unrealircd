/************************************************************************
 *   IRC - Internet Relay Chat, windows/gui.c
 *   Copyright (C) 2000-2004 David Flynn (DrBin) & Dominick Meglio (codemastr)
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

#define WIN32_VERSION BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH5

#include "unrealircd.h"
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <Strsafe.h>
#include "resource.h"
#include "win.h"

#define TOOLBAR_START 82
#define TOOLBAR_STOP (TOOLBAR_START+20)

__inline void ShowDialog(HWND *handle, HINSTANCE inst, char *template, HWND parent, 
			 DLGPROC proc)
{
	if (!IsWindow(*handle)) 
	{
		*handle = CreateDialog(inst, template, parent, (DLGPROC)proc); 
		ShowWindow(*handle, SW_SHOW);
	}
	else
		SetForegroundWindow(*handle);
}

/* Comments:
 * 
 * DrBin did a great job with the original GUI, but he has been gone a long time.
 * In his absense, it was decided it would be best to continue windows development.
 * The new code is based on his so it will be pretty much similar in features, my
 * main goal is to make it more stable. A lot of what I know about GUI coding 
 * I learned from DrBin so thanks to him for teaching me :) -- codemastr
 */

LRESULT CALLBACK MainDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LicenseDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK InfoDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CreditsDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HelpDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK StatusDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ConfigErrorDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FromVarDLG(HWND, UINT, WPARAM, LPARAM, unsigned char *, unsigned char **);
LRESULT CALLBACK FromFileReadDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FromFileDLG(HWND, UINT, WPARAM, LPARAM);

HBRUSH MainDlgBackground;

extern void SocketLoop(void *dummy);
HINSTANCE hInst;
NOTIFYICONDATA SysTray;
HTREEITEM AddItemToTree(HWND, LPSTR, int, short);
void win_map(Client *, HWND, short);
extern Link *Servers;
unsigned char *errors = NULL;
extern VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
void CleanUp(void)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
HWND hStatusWnd;
HWND hwIRCDWnd=NULL;
HWND hwTreeView;
HWND hWndMod;
UINT WM_TASKBARCREATED, WM_FINDMSGSTRING;
FARPROC lpfnOldWndProc;
HMENU hContext;
char OSName[OSVER_SIZE];

void TaskBarCreated() 
{
	HICON hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,16, 16, 0);
	SysTray.cbSize = sizeof(NOTIFYICONDATA);
	SysTray.hIcon = hIcon;
	SysTray.hWnd = hwIRCDWnd;
	SysTray.uCallbackMessage = WM_USER;
	SysTray.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
	SysTray.uID = 0;
	strcpy(SysTray.szTip, WIN32_VERSION);
	Shell_NotifyIcon(NIM_ADD ,&SysTray);
}

LRESULT LinkSubClassFunc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) 
{
	static HCURSOR hCursor;
	if (!hCursor)
		hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
	if (Message == WM_MOUSEMOVE || Message == WM_LBUTTONDOWN)
		SetCursor(hCursor);

	return CallWindowProc((WNDPROC)lpfnOldWndProc, hWnd, Message, wParam, lParam);
}



LRESULT RESubClassFunc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) 
{
	POINT p;
	RECT r;
	DWORD start, end;
	unsigned char string[500];

	if (Message == WM_GETDLGCODE)
		return DLGC_WANTALLKEYS;

	
	if (Message == WM_CONTEXTMENU) 
	{
		p.x = GET_X_LPARAM(lParam);
		p.y = GET_Y_LPARAM(lParam);
		if (GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1) 
		{
			GetClientRect(hWnd, &r);
			p.x = (int)((r.left + r.right)/2);
			p.y = (int)((r.top + r.bottom)/2);
			ClientToScreen(hWnd,&p);
		}
		if (!SendMessage(hWnd, EM_CANUNDO, 0, 0)) 
			EnableMenuItem(hContext, IDM_UNDO, MF_BYCOMMAND|MF_GRAYED);
		else
			EnableMenuItem(hContext, IDM_UNDO, MF_BYCOMMAND|MF_ENABLED);
		if (!SendMessage(hWnd, EM_CANPASTE, 0, 0)) 
			EnableMenuItem(hContext, IDM_PASTE, MF_BYCOMMAND|MF_GRAYED);
		else
			EnableMenuItem(hContext, IDM_PASTE, MF_BYCOMMAND|MF_ENABLED);
		if (GetWindowLong(hWnd, GWL_STYLE) & ES_READONLY) 
		{
			EnableMenuItem(hContext, IDM_CUT, MF_BYCOMMAND|MF_GRAYED);
			EnableMenuItem(hContext, IDM_DELETE, MF_BYCOMMAND|MF_GRAYED);
		}
		else 
		{
			EnableMenuItem(hContext, IDM_CUT, MF_BYCOMMAND|MF_ENABLED);
			EnableMenuItem(hContext, IDM_DELETE, MF_BYCOMMAND|MF_ENABLED);
		}
		SendMessage(hWnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
		if (start == end) 
			EnableMenuItem(hContext, IDM_COPY, MF_BYCOMMAND|MF_GRAYED);
		else
			EnableMenuItem(hContext, IDM_COPY, MF_BYCOMMAND|MF_ENABLED);
		TrackPopupMenu(hContext,TPM_LEFTALIGN|TPM_RIGHTBUTTON,p.x,p.y,0,GetParent(hWnd),NULL);
		return 0;
	}

	return CallWindowProc((WNDPROC)lpfnOldWndProc, hWnd, Message, wParam, lParam);
}

int DoCloseUnreal(HWND hWnd)
{
	unreal_log(ULOG_INFO, "main", "UNREALIRCD_STOP", NULL,
	           "Terminating server (process termination requested or GUI window closed)");
	loop.terminating = 1;
	unload_all_modules();
	DestroyWindow(hWnd);
	TerminateProcess(GetCurrentProcess(), 0);
	exit(0); /* in case previous fails (possible?) */
}

int AskCloseUnreal(HWND hWnd)
{
	if (MessageBox(hWnd, "Close UnrealIRCd?", "Are you sure?", MB_YESNO|MB_ICONQUESTION) == IDNO)
		 return 0;
	DoCloseUnreal(hWnd);
	exit(0);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	unsigned char *s;
	HWND hWnd;
	HICON hIcon;
	SC_HANDLE hService, hSCManager;
	SERVICE_TABLE_ENTRY DispatchTable[] = 
	{
		{ "UnrealIRCd", ServiceMain },
		{ 0, 0 }
	};
	DWORD need;
	
	/* Go one level up, since we are currently in the bin\ subdir
	 * and we want to be in (f.e.) "C:\Program Files\UnrealIRCd 6"
	 */
	chdir("..");

	GetOSName(OSName);

	/* Check if we are running as a service... */
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if ((hService = OpenService(hSCManager, "UnrealIRCd", SC_MANAGER_CONNECT)))
	{
		int save_err = 0;
		StartServiceCtrlDispatcher(DispatchTable);
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			SERVICE_STATUS status;
			/* Restart handling, it's ugly but it's as
			 * pretty as it is gonna get :)
			 */
			if (__argc == 2 && !strcmp(__argv[1], "restartsvc"))
			{
				QueryServiceStatus(hService, &status);
				if (status.dwCurrentState != SERVICE_STOPPED)
				{
					ControlService(hService,
						SERVICE_CONTROL_STOP, &status);
					while (status.dwCurrentState == SERVICE_STOP_PENDING)
					{
						QueryServiceStatus(hService, &status);
						if (status.dwCurrentState != SERVICE_STOPPED)
							Sleep(1000);
					}
				}
			}
			if (!StartService(hService, 0, NULL))
				save_err = GetLastError();
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		if (save_err != ERROR_SERVICE_DISABLED)
			exit(0);
	} else {
		CloseServiceHandle(hSCManager);
	}
	InitCommonControls();
	WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");
	WM_FINDMSGSTRING = RegisterWindowMessage(FINDMSGSTRING);
	atexit(CleanUp);
	if (!LoadLibrary("riched20.dll"))
		LoadLibrary("riched32.dll");
	InitDebug();
	init_winsock();
	hInst = hInstance; 

	MainDlgBackground = CreateSolidBrush(RGB(75, 134, 238)); /* Background of main dialog */

	hWnd = CreateDialog(hInstance, "UnrealIRCd", 0, (DLGPROC)MainDLG); 
	hwIRCDWnd = hWnd;
	
	TaskBarCreated();

	if (InitUnrealIRCd(__argc, __argv) != 1)
	{
		MessageBox(NULL, "UnrealIRCd has failed to initialize in InitUnrealIRCd()", "UnrealIRCD Initalization Error" ,MB_OK);
		return FALSE;
	}
	ShowWindow(hWnd, SW_SHOW);
	_beginthread(SocketLoop, 0, NULL);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsWindow(hStatusWnd) || !IsDialogMessage(hStatusWnd, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return FALSE;

}

LRESULT CALLBACK MainDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HCURSOR hCursor;
	static HMENU hRehash, hAbout, hConfig, hTray, hLogs;

	unsigned char *argv[3];
	Client *pClient;
	unsigned char *msg;
	POINT p;

	if (message == WM_TASKBARCREATED)
	{
		TaskBarCreated();
		return TRUE;
	}
	
	switch (message)
	{
		case WM_INITDIALOG: 
		{
			ShowWindow(hDlg, SW_HIDE);
			hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
			hContext = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_CONTEXT)),0);
			/* Rehash popup menu */
			hRehash = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_REHASH)),0);
			/* About popup menu */
			hAbout = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_ABOUT)),0);
			/* Systray popup menu set the items to point to the other menus*/
			hTray = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_SYSTRAY)),0);
			ModifyMenu(hTray, IDM_REHASH, MF_BYCOMMAND|MF_POPUP|MF_STRING, HandleToUlong(hRehash), "&Rehash");
			ModifyMenu(hTray, IDM_ABOUT, MF_BYCOMMAND|MF_POPUP|MF_STRING, HandleToUlong(hAbout), "&About");
			
			SetWindowText(hDlg, WIN32_VERSION);
			SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_SMALL, 
				(LPARAM)(HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,16, 16, 0));
			SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_BIG, 
				(LPARAM)(HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,32, 32, 0));
			return TRUE;
		}
		case WM_CTLCOLORDLG:
			return (LONG)HandleToLong(MainDlgBackground);
		case WM_SIZE: 
		{
			if (wParam & SIZE_MINIMIZED)
				ShowWindow(hDlg,SW_HIDE);
			return 0;
		}
		case WM_CLOSE: 
			return DoCloseUnreal(hDlg);
		case WM_USER: 
		{
			switch(LOWORD(lParam)) 
			{
				case WM_LBUTTONDBLCLK:
					ShowWindow(hDlg, SW_SHOW);
					ShowWindow(hDlg,SW_RESTORE);
					SetForegroundWindow(hDlg);
				case WM_RBUTTONDOWN:
					SetForegroundWindow(hDlg);
					break;
				case WM_RBUTTONUP: 
				{
					unsigned long i = 60000;
					MENUITEMINFO mii;
					GetCursorPos(&p);
					DestroyMenu(hConfig);
					hConfig = CreatePopupMenu();
					DestroyMenu(hLogs);
					hLogs = CreatePopupMenu();
					AppendMenu(hConfig, MF_STRING, IDM_CONF, CPATH);
#if 0
					if (conf_log) 
					{
						ConfigItem_log *logs;
						AppendMenu(hConfig, MF_POPUP|MF_STRING, HandleToUlong(hLogs), "Logs");
						for (logs = conf_log; logs; logs = logs->next)
						{
							AppendMenu(hLogs, MF_STRING, i++, logs->file);
						}
					}
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
#endif
					if (conf_files)
					{
						AppendMenu(hConfig, MF_STRING, IDM_MOTD, conf_files->motd_file);
						AppendMenu(hConfig, MF_STRING, IDM_SMOTD, conf_files->smotd_file);
						AppendMenu(hConfig, MF_STRING, IDM_OPERMOTD, conf_files->opermotd_file);
						AppendMenu(hConfig, MF_STRING, IDM_BOTMOTD, conf_files->botmotd_file);
						AppendMenu(hConfig, MF_STRING, IDM_RULES, conf_files->rules_file);
					}
						
					if (conf_tld) 
					{
						ConfigItem_tld *tlds;
						AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
						for (tlds = conf_tld; tlds; tlds = tlds->next)
						{
							if (!tlds->flag.motdptr)
								AppendMenu(hConfig, MF_STRING, i++, tlds->motd_file);
							if (!tlds->flag.ruleclient)
								AppendMenu(hConfig, MF_STRING, i++, tlds->rules_file);
							if (tlds->smotd_file)
								AppendMenu(hConfig, MF_STRING, i++, tlds->smotd_file);
						}
					}
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
					AppendMenu(hConfig, MF_STRING, IDM_NEW, "New File");
					mii.cbSize = sizeof(MENUITEMINFO);
					mii.fMask = MIIM_SUBMENU;
					mii.hSubMenu = hConfig;
					SetMenuItemInfo(hTray, IDM_CONFIG, MF_BYCOMMAND, &mii);
					TrackPopupMenu(hTray, TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
					/* Kludge for a win bug */
					SendMessage(hDlg, WM_NULL, 0, 0);
					break;
				}
			}
			return 0;
		}
		case WM_DESTROY:
			return 0;
		case WM_MOUSEMOVE: 
		{
			POINT p;
			p.x = LOWORD(lParam);
			p.y = HIWORD(lParam);
			if ((p.x >= 93) && (p.x <= 150) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				SetCursor(hCursor);
			else if ((p.x >= 160) && (p.x <= 208) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				SetCursor(hCursor);
			else if ((p.x >= 219) && (p.x <= 267) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				SetCursor(hCursor);
			else if ((p.x >= 279) && (p.x <= 325) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				SetCursor(hCursor);
			else if ((p.x >= 336) && (p.x <= 411) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				SetCursor(hCursor);
			return 0;
		}
		case WM_LBUTTONDOWN: 
		{
			POINT p;
			p.x = LOWORD(lParam);
			p.y = HIWORD(lParam);
			if ((p.x >= 93) && (p.x <= 150) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP))
			{
				ClientToScreen(hDlg,&p);
				TrackPopupMenu(hRehash,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
				return 0;
			}
			else if ((p.x >= 160) && (p.x <= 208) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP))
			{
				ShowDialog(&hStatusWnd, hInst, "Status", hDlg, StatusDLG);
				return 0;
			}
			else if ((p.x >= 219) && (p.x <= 267) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP))
			{
				unsigned long i = 60000;
				ClientToScreen(hDlg,&p);
				DestroyMenu(hConfig);
				hConfig = CreatePopupMenu();
				DestroyMenu(hLogs);
				hLogs = CreatePopupMenu();

				AppendMenu(hConfig, MF_STRING, IDM_CONF, CPATH);
#if 0
				if (conf_log) 
				{
					ConfigItem_log *logs;
					AppendMenu(hConfig, MF_POPUP|MF_STRING, HandleToUlong(hLogs), "Logs");
					for (logs = conf_log; logs; logs = logs->next)
					{
						AppendMenu(hLogs, MF_STRING, i++, logs->file);
					}
				}
				AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
#endif
				if (conf_files)
				{
					AppendMenu(hConfig, MF_STRING, IDM_MOTD, conf_files->motd_file);
					AppendMenu(hConfig, MF_STRING, IDM_SMOTD, conf_files->smotd_file);
					AppendMenu(hConfig, MF_STRING, IDM_OPERMOTD, conf_files->opermotd_file);
					AppendMenu(hConfig, MF_STRING, IDM_BOTMOTD, conf_files->botmotd_file);
					AppendMenu(hConfig, MF_STRING, IDM_RULES, conf_files->rules_file);
				}
				
				if (conf_tld) 
				{
					ConfigItem_tld *tlds;
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
					for (tlds = conf_tld; tlds; tlds = tlds->next)
					{
						if (!tlds->flag.motdptr)
							AppendMenu(hConfig, MF_STRING, i++, tlds->motd_file);
						if (!tlds->flag.ruleclient)
							AppendMenu(hConfig, MF_STRING, i++, tlds->rules_file);
						if (tlds->smotd_file)
							AppendMenu(hConfig, MF_STRING, i++, tlds->smotd_file);
					}
				}
				AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
				AppendMenu(hConfig, MF_STRING, IDM_NEW, "New File");
				TrackPopupMenu(hConfig,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);

				return 0;
			}
			else if ((p.x >= 279) && (p.x <= 325) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
			{
				ClientToScreen(hDlg,&p);
				TrackPopupMenu(hAbout,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
				return 0;
			}
			else if ((p.x >= 336) && (p.x <= 411) && (p.y >= TOOLBAR_START) && (p.y <= TOOLBAR_STOP)) 
				return AskCloseUnreal(hDlg);
		}
		case WM_SYSCOMMAND:
		{
			if (wParam == SC_CLOSE)
			{
				AskCloseUnreal(hDlg);
				return 1;
			}
			break;
		}
		case WM_COMMAND: 
		{
			if (LOWORD(wParam) >= 60000 && HIWORD(wParam) == 0 && !lParam) 
			{
				unsigned char path[MAX_PATH];
				if (GetMenuString(hLogs, LOWORD(wParam), path, MAX_PATH, MF_BYCOMMAND))
					DialogBoxParam(hInst, "FromVar", hDlg, (DLGPROC)FromFileReadDLG, (LPARAM)path);
				
				else 
				{
					GetMenuString(hConfig,LOWORD(wParam), path, MAX_PATH, MF_BYCOMMAND);
					if (!url_is_valid(path))
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, (LPARAM)path);
				}
				return FALSE;
			}

			if (!loop.booted)
			{
				MessageBox(NULL, "UnrealIRCd not booted due to configuration errors. "
				                 "Check other window for error details. Then close that window, "
				                 "fix the errors and start UnrealIRCd again.",
				                 "UnrealIRCd not started",
				                 MB_OK);
				return FALSE;
			}
			switch(LOWORD(wParam)) 
			{
				case IDM_STATUS:
					ShowDialog(&hStatusWnd, hInst, "Status", hDlg,StatusDLG);
					break;
				case IDM_SHUTDOWN:
					return AskCloseUnreal(hDlg);
				case IDM_RHALL:
					MessageBox(NULL, "Rehashing all files", "Rehashing", MB_OK);
					dorehash = 1;
					break;
				case IDM_LICENSE: 
					DialogBox(hInst, "FromVar", hDlg, (DLGPROC)LicenseDLG);
					break;
				case IDM_INFO:
					DialogBox(hInst, "FromVar", hDlg, (DLGPROC)InfoDLG);
					break;
				case IDM_CREDITS:
					DialogBox(hInst, "FromVar", hDlg, (DLGPROC)CreditsDLG);
					break;
				case IDM_HELP:
					DialogBox(hInst, "Help", hDlg, (DLGPROC)HelpDLG);
					break;
				case IDM_CONF:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
						(LPARAM)CPATH);
					break;
				case IDM_MOTD:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
						(LPARAM)conf_files->motd_file);
					break;
				case IDM_SMOTD:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
						(LPARAM)conf_files->smotd_file);
					break;
				case IDM_OPERMOTD:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
						(LPARAM)conf_files->opermotd_file);
					break;
				case IDM_BOTMOTD:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
						(LPARAM)conf_files->botmotd_file);
					break;
				case IDM_RULES:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
						(LPARAM)conf_files->rules_file);
					break;
				case IDM_NEW:
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, (LPARAM)NULL);
					break;
			}
		}
	}
	return FALSE;
}

LRESULT CALLBACK LicenseDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd License", gnulicense);
}

LRESULT CALLBACK InfoDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd Team", unrealinfo);
}

LRESULT CALLBACK CreditsDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd Credits", unrealcredits);
}

LRESULT CALLBACK FromVarDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam,
                            unsigned char *title, unsigned char **s) 
{
	HWND hWnd;
	switch (message) 
	{
		case WM_INITDIALOG: 
		{
#if 0
			unsigned char	String[16384];
			int size;
			unsigned char *RTFString;
			StreamIO *stream = safe_alloc(sizeof(StreamIO));
			EDITSTREAM edit;
			SetWindowText(hDlg, title);
			memset(String, 0, sizeof(String));
			lpfnOldWndProc = (FARPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_TEXT), GWLP_WNDPROC, (LONG_PTR)RESubClassFunc);
			while (*s) 
			{
				strcat(String, *s++);
				if (*s)
					strcat(String, "\r\n");
			}
			size = CountRTFSize(String)+1;
			RTFString = safe_alloc(size);
			IRCToRTF(String,RTFString);
			RTFBuf = RTFString;
			size--;
			stream->size = &size;
			stream->buffer = &RTFBuf;
			edit.dwCookie = HandleToUlong(stream);
			edit.pfnCallback = SplitIt;
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_STREAMIN, (WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
			safe_free(RTFString);
			safe_free(stream);
			return TRUE;
#else
			return FALSE;
#endif
		}

		case WM_COMMAND: 
		{
			hWnd = GetDlgItem(hDlg, IDC_TEXT);
			if (LOWORD(wParam) == IDOK)
				return EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDM_COPY) 
			{
				SendMessage(hWnd, WM_COPY, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_SELECTALL) 
			{
				SendMessage(hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			if (LOWORD(wParam) == IDM_PASTE) 
			{
				SendMessage(hWnd, WM_PASTE, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_CUT) 
			{
				SendMessage(hWnd, WM_CUT, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_UNDO) 
			{
				SendMessage(hWnd, EM_UNDO, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_DELETE) 
			{
				SendMessage(hWnd, WM_CLEAR, 0, 0);
				return 0;
			}
			break;
		}
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			break;
	}
	return (FALSE);
}

LRESULT CALLBACK FromFileReadDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	HWND hWnd;
	switch (message) 
	{
		case WM_INITDIALOG: 
		{
			int fd,len;
			unsigned char *buffer = '\0', *string = '\0';
			EDITSTREAM edit;
			StreamIO *stream = safe_alloc(sizeof(StreamIO));
			unsigned char szText[256];
			struct stat sb;
			HWND hWnd = GetDlgItem(hDlg, IDC_TEXT), hTip;
			StringCbPrintf(szText, sizeof(szText), "UnrealIRCd Viewer - %s", (unsigned char *)lParam);
			SetWindowText(hDlg, szText);
			lpfnOldWndProc = (FARPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RESubClassFunc);
			if ((fd = open((unsigned char *)lParam, _O_RDONLY|_O_BINARY)) != -1) 
			{
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = safe_alloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size); 
				buffer[len] = 0;
				len = CountRTFSize(buffer)+1;
				string = safe_alloc(len);
				IRCToRTF(buffer,string);
				RTFBuf = string;
				len--;
				stream->size = &len;
				stream->buffer = &RTFBuf;
				edit.dwCookie = (DWORD_PTR)stream;
				edit.pfnCallback = SplitIt;
				SendMessage(hWnd, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
				SendMessage(hWnd, EM_STREAMIN, (WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
				close(fd);
				RTFBuf = NULL;
				safe_free(buffer);
				safe_free(string);
				safe_free(stream);
			}
			return TRUE;
		}
		case WM_COMMAND: 
		{
			hWnd = GetDlgItem(hDlg, IDC_TEXT);
			if (LOWORD(wParam) == IDOK)
				return EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDM_COPY) 
			{
				SendMessage(hWnd, WM_COPY, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_SELECTALL) 
			{
				SendMessage(hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			if (LOWORD(wParam) == IDM_PASTE) 
			{
				SendMessage(hWnd, WM_PASTE, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_CUT) 
			{
				SendMessage(hWnd, WM_CUT, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_UNDO) 
			{
				SendMessage(hWnd, EM_UNDO, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_DELETE) 
			{
				SendMessage(hWnd, WM_CLEAR, 0, 0);
				return 0;
			}
			break;
		}
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			break;
	}
	return FALSE;
}

LRESULT CALLBACK HelpDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	static HFONT hFont;
	static HCURSOR hCursor;
	switch (message) 
	{
		case WM_INITDIALOG:
			hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
			hFont = CreateFont(8,0,0,0,0,0,1,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"MS Sans Serif");
			SendMessage(GetDlgItem(hDlg, IDC_EMAIL), WM_SETFONT, (WPARAM)hFont,TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_URL), WM_SETFONT, (WPARAM)hFont,TRUE);
			lpfnOldWndProc = (FARPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_EMAIL), GWLP_WNDPROC, (LONG_PTR)LinkSubClassFunc);
			SetWindowLongPtr(GetDlgItem(hDlg, IDC_URL), GWLP_WNDPROC, (LONG_PTR)LinkSubClassFunc);
			return TRUE;

		case WM_DRAWITEM: 
		{
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			unsigned char text[500];
			COLORREF oldtext;
			RECT focus;
			GetWindowText(lpdis->hwndItem, text, 500);
			if (wParam == IDC_URL || IDC_EMAIL) 
			{
				FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(COLOR_3DFACE));
				oldtext = SetTextColor(lpdis->hDC, RGB(0,0,255));
				DrawText(lpdis->hDC, text, strlen(text), &lpdis->rcItem, DT_CENTER|DT_VCENTER);
				SetTextColor(lpdis->hDC, oldtext);
				if (lpdis->itemState & ODS_FOCUS) 
				{
					CopyRect(&focus, &lpdis->rcItem);
					focus.left += 2;
					focus.right -= 2;
					focus.top += 1;
					focus.bottom -= 1;
					DrawFocusRect(lpdis->hDC, &focus);
				}
				return TRUE;
			}
		}	
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			if (HIWORD(wParam) == BN_DBLCLK) 
			{
				if (LOWORD(wParam) == IDC_URL) 
					ShellExecute(NULL, "open", "https://www.unrealircd.org", NULL, NULL, 
						SW_MAXIMIZE);
				else if (LOWORD(wParam) == IDC_EMAIL)
					ShellExecute(NULL, "open", "mailto:unreal-users@lists.sourceforge.net", NULL, NULL, 
						SW_MAXIMIZE);
				EndDialog(hDlg, TRUE);
				return 0;
			}
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			DeleteObject(hFont);
			break;

	}
	return FALSE;
}








LRESULT CALLBACK StatusDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	switch (message) 
	{
		case WM_INITDIALOG: 
		{
			hwTreeView = GetDlgItem(hDlg, IDC_TREE);
			win_map(&me, hwTreeView, 0);
			SetDlgItemInt(hDlg, IDC_CLIENTS, irccounts.clients, FALSE);
			SetDlgItemInt(hDlg, IDC_SERVERS, irccounts.servers, FALSE);
			SetDlgItemInt(hDlg, IDC_INVISO, irccounts.invisible, FALSE);
			SetDlgItemInt(hDlg, IDC_UNKNOWN, irccounts.unknown, FALSE);
			SetDlgItemInt(hDlg, IDC_OPERS, irccounts.operators, FALSE);
			SetDlgItemInt(hDlg, IDC_CHANNELS, irccounts.channels, FALSE);
			if (irccounts.clients > irccounts.global_max)
				irccounts.global_max = irccounts.clients;
			if (irccounts.me_clients > irccounts.me_max)
					irccounts.me_max = irccounts.me_clients;
			SetDlgItemInt(hDlg, IDC_MAXCLIENTS, irccounts.global_max, FALSE);
			SetDlgItemInt(hDlg, IDC_LCLIENTS, irccounts.me_clients, FALSE);
			SetDlgItemInt(hDlg, IDC_LSERVERS, irccounts.me_servers, FALSE);
			SetDlgItemInt(hDlg, IDC_LMAXCLIENTS, irccounts.me_max, FALSE);
			SetTimer(hDlg, 1, 5000, NULL);
			return TRUE;
		}
		case WM_CLOSE:
			DestroyWindow(hDlg);
			return TRUE;
		case WM_TIMER:
			TreeView_DeleteAllItems(hwTreeView);
			win_map(&me, hwTreeView, 1);
			SetDlgItemInt(hDlg, IDC_CLIENTS, irccounts.clients, FALSE);
			SetDlgItemInt(hDlg, IDC_SERVERS, irccounts.servers, FALSE);
			SetDlgItemInt(hDlg, IDC_INVISO, irccounts.invisible, FALSE);
			SetDlgItemInt(hDlg, IDC_INVISO, irccounts.invisible, FALSE);
			SetDlgItemInt(hDlg, IDC_UNKNOWN, irccounts.unknown, FALSE);
			SetDlgItemInt(hDlg, IDC_OPERS, irccounts.operators, FALSE);
			SetDlgItemInt(hDlg, IDC_CHANNELS, irccounts.channels, FALSE);
			if (irccounts.clients > irccounts.global_max)
				irccounts.global_max = irccounts.clients;
			if (irccounts.me_clients > irccounts.me_max)
					irccounts.me_max = irccounts.me_clients;
			SetDlgItemInt(hDlg, IDC_MAXCLIENTS, irccounts.global_max, FALSE);
			SetDlgItemInt(hDlg, IDC_LCLIENTS, irccounts.me_clients, FALSE);
			SetDlgItemInt(hDlg, IDC_LSERVERS, irccounts.me_servers, FALSE);
			SetDlgItemInt(hDlg, IDC_LMAXCLIENTS, irccounts.me_max, FALSE);
			SetTimer(hDlg, 1, 5000, NULL);
			return TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) 
			{
				DestroyWindow(hDlg);
				return TRUE;
			}
			break;

	}
	return FALSE;
}

/* This was made by DrBin but I cleaned it up a bunch to make it work better */

HTREEITEM AddItemToTree(HWND hWnd, LPSTR lpszItem, int nLevel, short remap)
{
	TVITEM tvi; 
	TVINSERTSTRUCT tvins; 
	static HTREEITEM hPrev = (HTREEITEM)TVI_FIRST; 
	static HTREEITEM hPrevLev[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	HTREEITEM hti; 

	if (remap) 
	{
		hPrev = (HTREEITEM)TVI_FIRST;
		memset(hPrevLev, 0, sizeof(HTREEITEM)*10);
	}
		
	tvi.mask = TVIF_TEXT|TVIF_PARAM; 
	tvi.pszText = lpszItem; 
	tvi.cchTextMax = lstrlen(lpszItem); 
	tvi.lParam = (LPARAM)nLevel; 
	tvins.item = tvi; 
	tvins.hInsertAfter = hPrev; 
	if (nLevel == 1) 
		tvins.hParent = TVI_ROOT; 
	else 
		tvins.hParent = hPrevLev[nLevel-1];
	hPrev = (HTREEITEM)SendMessage(hWnd, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT) &tvins); 
	hPrevLev[nLevel] = hPrev;
	TreeView_EnsureVisible(hWnd,hPrev);
	if (nLevel > 1) 
	{ 
		hti = TreeView_GetParent(hWnd, hPrev); 
		tvi.mask = TVIF_IMAGE|TVIF_SELECTEDIMAGE; 
		tvi.hItem = hti; 
		TreeView_SetItem(hWnd, &tvi); 
	} 
	return hPrev; 
}

/*
 * Now used to create list of servers for server list tree view -- David Flynn
 * Recoded by codemastr to be faster.
 * I removed the Potvin credit because it no longer uses any original code and I don't
 * even think Potvin actually made the original code
 */
void win_map(Client *server, HWND hwTreeView, short remap)
{
/*
	Client *acptr;
	Link *lp;

	AddItemToTree(hwTreeView,server->name,server->hopcount+1, remap);

	for (lp = Servers; lp; lp = lp->next)
        {
                acptr = lp->value.client;
                if (acptr->uplink != server)
                        continue;
                win_map(acptr, hwTreeView, 0);
        }
FIXME
*/
}

/* ugly stuff, but hey it works -- codemastr */
void win_log(FORMAT_STRING(const char *format), ...)
{
	va_list ap;
	unsigned char buf[2048];
	FILE *fd;

	va_start(ap, format);

	ircvsnprintf(buf, sizeof(buf), format, ap);
	stripcrlf(buf);

	if (!IsService) 
	{
		strcat(buf, "\r\n");
		if (errors) 
		{
			char *tbuf = safe_alloc(strlen(errors) + strlen(buf) + 1);
			strcpy(tbuf, errors);
			strcat(tbuf, buf);
			safe_free(errors);
			errors = tbuf;
		}
		else 
		{
			safe_strdup(errors, buf);
		}
	}
	else 
	{
		FILE *fd = fopen("logs\\service.log", "a");
		if (fd)
		{
			char timebuf[256];
			snprintf(timebuf, sizeof(timebuf), "[%s]", myctime(time(NULL)));
			fprintf(fd, "%s - %s\n", timebuf, buf);
			fclose(fd);
		}
#ifdef _DEBUG
		else
		{
			OutputDebugString(buf);
		}
#endif
	}
	va_end(ap);
}

void win_error() 
{
	if (errors && !IsService)
		DialogBox(hInst, "ConfigError", hwIRCDWnd, (DLGPROC)ConfigErrorDLG);
}

LRESULT CALLBACK ConfigErrorDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	switch (message) 
	{
		case WM_INITDIALOG:
			MessageBeep(MB_ICONEXCLAMATION);
			SetDlgItemText(hDlg, IDC_CONFIGERROR, errors);
			safe_free(errors);
			errors = NULL;
			return (TRUE);
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			break;

		}
	return (FALSE);
}
