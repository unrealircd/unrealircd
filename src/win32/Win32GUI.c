/************************************************************************
 *   IRC - Internet Relay Chat, Win32GUI.c
 *   Copyright (C) 2000-2001 David Flynn (DrBin) & Dominick Meglio (codemastr)
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

#define OEMRESOURCE
//#define _WIN32_IE 0x0500

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

#define WIN32_VERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH9
#include <windows.h>
#include "resource.h"
#include "version.h"
#include "setup.h"
#include <commctrl.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "userload.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include "h.h"

/* Comments:
 * 
 * DrBin did a great job with the original GUI, but he has been gone a long time
 * in his absense it was decided it would be best to continue windows development.
 * The new code is based on his so it will be pretty much similar in features, my
 * main goal is to make it more stable. A lot of what I know about GUI coding 
 * I learned from DrBin so thanks to him for teaching me :) -- codemastr
 */

LRESULT CALLBACK MainDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LicenseDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CreditsDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DalDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HelpDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ConfigDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MotdDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OperMotdDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BotMotdDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RulesDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK StatusDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ConfigErrorDLG(HWND, UINT, WPARAM, LPARAM);
extern  void      SocketLoop(void *dummy), s_rehash();
static char *StripCodes(char *);
HINSTANCE hInst;
NOTIFYICONDATA SysTray;
void CleanUp(void);
HTREEITEM AddItemToTree(HWND, LPSTR, int);
void win_map(aClient *, HWND);
extern Link *Servers;
extern ircstats IRCstats;
char *errors;
void CleanUp(void)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
void CleanUpSegv(int sig)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
HWND hwIRCDWnd=NULL/* hwnd=NULL*/;
HWND hwTreeView;
HANDLE hMainThread = 0;
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	int argc=0;
	char *s, *argv[20];
	HWND hWnd;
    WSADATA WSAData;
	HICON hIcon;

	atexit(CleanUp);
	InitStackTraceLibrary();
    if (WSAStartup(MAKEWORD(1, 1), &WSAData) != 0)
    {
        MessageBox(NULL, "UnrealIRCD/32 Initalization Error", "Unable to initialize WinSock", MB_OK);
        return FALSE;
    }
	hInst = hInstance; 
    
	hWnd = CreateDialog(hInstance, "WIRCD", 0, (DLGPROC)MainDLG); 
	hwIRCDWnd = hWnd;
	
	hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,16, 16, 0);
	SysTray.cbSize = sizeof(NOTIFYICONDATA);
	SysTray.hIcon = hIcon;
	SysTray.hWnd = hwIRCDWnd;
	SysTray.uCallbackMessage = WM_USER;
	SysTray.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE; 
	SysTray.uID = 0;
	lstrcpy(SysTray.szTip, WIN32_VERSION);
	Shell_NotifyIcon(NIM_ADD ,&SysTray);
	if ((s =  GetCommandLine()))
		argv[argc++] = s;
	    
	argv[argc] = NULL;

	if (InitwIRCD(argc, argv) != 1)
	{
		MessageBox(NULL,"UnrealIRCd has failed to initialize in InitwIRCD()","Error:",MB_OK);
		return FALSE;
	}

	hMainThread = (HANDLE) _beginthread(SocketLoop, 0, NULL);
	while (GetMessage(&msg, NULL, 0, 0))
    {
		TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
	return FALSE;

}

LRESULT CALLBACK MainDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
static HCURSOR hCursor;
static HMENU hAbout, hConfig;
	int wmId, wmEvent;
	
	switch (message)
			{
			case WM_INITDIALOG: {
				hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
				hAbout = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_ABOUT)),0);
				hConfig = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_CONFIG)),0);
				SetWindowText(hDlg, WIN32_VERSION);
				SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_SMALL, 
					(LPARAM)(HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,16, 16, 0));
				SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_BIG, 
					(LPARAM)(HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,32, 32, 0));
					return (TRUE);
			}
			case WM_SIZE: {
				if (wParam & SIZE_MINIMIZED) {
					ShowWindow(hDlg,SW_HIDE);
				}
				return 0;
			}
			case WM_CLOSE: {
					DestroyWindow(hDlg);
					exit(0);
			}
			case WM_USER: {
				switch(LOWORD(lParam)) {
					case WM_LBUTTONDBLCLK:
						ShowWindow(hDlg, SW_SHOW);
						ShowWindow(hDlg,SW_RESTORE);
						SetForegroundWindow(hDlg);
					}
				return 0;
			}
			case WM_DESTROY:
					  return 0;
			case WM_MOUSEMOVE: {
				 POINT p;
				 p.x = LOWORD(lParam);
				 p.y = HIWORD(lParam);

				 if ((p.x >= 24) && (p.x <= 78) && (p.y >= 178) && (p.y <= 190)) 
					 SetCursor(hCursor);
				 else if ((p.x >= 85) && (p.x <= 132) && (p.y >= 178) && (p.y <= 190)) 
					 SetCursor(hCursor);
				 else if ((p.x >= 140) && (p.x <= 186) && (p.y >= 178) && (p.y <= 190)) 
				     SetCursor(hCursor);
				 else if ((p.x >= 194) && (p.x <= 237) && (p.y >= 178) && (p.y <= 190)) 
					 SetCursor(hCursor);
				 else if ((p.x >= 245) && (p.x <= 311) && (p.y >= 178) && (p.y <= 190)) 
					 SetCursor(hCursor);
				 return 0;
			}
			case WM_LBUTTONDOWN: {
			 POINT p;
	         p.x = LOWORD(lParam);
		     p.y = HIWORD(lParam);
			 if ((p.x >= 24) && (p.x <= 78) && (p.y >= 178) && (p.y <= 190))
             {
				 MessageBox(hDlg, "Rehashing the wIRCd", "Rehashing", MB_OK);
         		 rehash();
				 return 0;
			 }
			 else if ((p.x >= 85) && (p.x <= 132) && (p.y >= 178) && (p.y <= 190))  {
				 DialogBox(hInst, "Status", hDlg, (DLGPROC)StatusDLG);
				 return 0;
			 }
			 else if ((p.x >= 140) && (p.x <= 186) && (p.y >= 178) && (p.y <= 190))  {
				ClientToScreen(hDlg,&p);
				TrackPopupMenu(hConfig,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
				return 0;
			 }
			 else if ((p.x >= 194) && (p.x <= 237) && (p.y >= 178) && (p.y <= 190)) {
				ClientToScreen(hDlg,&p);
				TrackPopupMenu(hAbout,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
				return 0;
			 }

			 else if ((p.x >= 245) && (p.x <= 311) && (p.y >= 178) && (p.y <= 190)) {
				 if (MessageBox(hDlg, "Close UnrealIRCd?", "Are you sure?", MB_YESNO|MB_ICONQUESTION) == IDNO)
					 return 0;
				 else {
					 DestroyWindow(hDlg);
					 exit(0);
				 }
			 }

		}
			case WM_COMMAND: {
				switch(LOWORD(wParam)) {
					case IDM_LICENSE: 
						DialogBox(hInst, "FromVar", hDlg, (DLGPROC)LicenseDLG);
						break;
					case IDM_CREDITS:
						DialogBox(hInst, "FromVar", hDlg, (DLGPROC)CreditsDLG);
						break;
					case IDM_DAL:
						DialogBox(hInst, "FromVar", hDlg, (DLGPROC)DalDLG);
						break;
					case IDM_HELP:
						DialogBox(hInst, "Help", hDlg, (DLGPROC)HelpDLG);
						break;
					case IDM_CONF:
						DialogBox(hInst, "FromFile", hDlg, (DLGPROC)ConfigDLG);
						break;
					case IDM_MOTD:
						DialogBox(hInst, "FromFile", hDlg, (DLGPROC)MotdDLG);
						break;
					case IDM_OPERMOTD:
						DialogBox(hInst, "FromFile", hDlg, (DLGPROC)OperMotdDLG);
						break;
					case IDM_BOTMOTD:
						DialogBox(hInst, "FromFile", hDlg, (DLGPROC)BotMotdDLG);
						break;
					case IDM_RULES:
						DialogBox(hInst, "FromFile", hDlg, (DLGPROC)RulesDLG);

				}
			}
	}
	return (FALSE);
}

LRESULT CALLBACK LicenseDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
static HFONT hFont;
	switch (message) {
		case WM_INITDIALOG: {
			char	String[16384], **s = gnulicense;
			hFont = CreateFont(8,0,0,0,0,0,0,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"fixedsys");
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_SETFONT, (WPARAM)hFont,TRUE);
			SetWindowText(hDlg, "UnrealIRCd License");
			String[0] = 0;
			while (*s)
			    {
				strcat(String, StripCodes(*s++));
				if (*s)
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_TEXT, String);

			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			DeleteObject(hFont);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK CreditsDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
static HFONT hFont;
	switch (message) {
		case WM_INITDIALOG: {
			char	String[16384], **s = unrealcredits;
			hFont = CreateFont(8,0,0,0,0,0,0,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"fixedsys");
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_SETFONT, (WPARAM)hFont,TRUE);
			SetWindowText(hDlg, "UnrealIRCd Credits");
			String[0] = 0;
			while (*s)
			    {
				strcat(String, StripCodes(*s++));
				if (*s)
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_TEXT, String);

			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			DeleteObject(hFont);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK DalDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
static HFONT hFont;
	switch (message) {
		case WM_INITDIALOG: {
			char	String[16384], **s = dalinfotext;
			hFont = CreateFont(8,0,0,0,0,0,0,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"fixedsys");
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_SETFONT, (WPARAM)hFont,TRUE);
			SetWindowText(hDlg, "UnrealIRCd DALnet Credits");
			String[0] = 0;
			while (*s)
			    {
				strcat(String, StripCodes(*s++));
				if (*s)
					strcat(String, "\r\n");
			    }
			SetDlgItemText(hDlg, IDC_TEXT, String);

			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			DeleteObject(hFont);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK HelpDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HBRUSH hBrush;
	static HFONT hFont;
	static HCURSOR hCursor;
	switch (message) {
		case WM_INITDIALOG:
			hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
			hFont = CreateFont(8,0,0,0,0,0,1,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"MS Sans Serif");
			SendMessage(GetDlgItem(hDlg, IDC_EMAIL), WM_SETFONT, (WPARAM)hFont,TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_URL), WM_SETFONT, (WPARAM)hFont,TRUE);
			hBrush = GetSysColorBrush(COLOR_3DFACE);
			return (TRUE);

		case WM_CTLCOLORSTATIC: 
			if ((GetDlgItem(hDlg, IDC_URL) == (HWND)lParam) 
				|| (GetDlgItem(hDlg, IDC_EMAIL) == (HWND)lParam)) {
			SetBkColor((HDC)wParam, GetSysColor(COLOR_3DFACE));
			SetTextColor((HDC)wParam, RGB(0,0,255));
			return (DWORD)hBrush;
			}
			break;
		/* Ugly code to make it change the cursor over a link */
		case WM_MOUSEMOVE: {
			POINT p, p2, p3;
			RECT r;
			p.x = LOWORD(lParam);
			p.y = HIWORD(lParam);
			GetClientRect(GetDlgItem(hDlg, IDC_URL),&r);
			p2.x = r.left;
			p2.y = r.top;
			p3.x = r.right;
			p3.y = r.bottom;
			MapWindowPoints(GetDlgItem(hDlg, IDC_URL),hDlg, &p2, 1);
			MapWindowPoints(GetDlgItem(hDlg, IDC_URL),hDlg, &p3, 1);

			if ((p.x >= p2.x) && (p.x <= p3.x) && (p.y <= p3.y) && (p.y >= p2.y)) {
				SetCursor(hCursor);
				return 0;
			}
			GetClientRect(GetDlgItem(hDlg, IDC_EMAIL),&r);
			p2.x = r.left;
			p2.y = r.top;
			p3.x = r.right;
			p3.y = r.bottom;
			MapWindowPoints(GetDlgItem(hDlg, IDC_EMAIL),hDlg, &p2, 1);
			MapWindowPoints(GetDlgItem(hDlg, IDC_EMAIL),hDlg, &p3, 1);

			if ((p.x >= p2.x) && (p.x <= p3.x) && (p.y <= p3.y) && (p.y >= p2.y)) {
				SetCursor(hCursor);
				return 0;
			}

			return 0;
		}
		/* Ugly code to simulate a link by opening the browser/email program */
		case WM_LBUTTONDOWN: {
			POINT p, p2, p3;
			RECT r;
			p.x = LOWORD(lParam);
			p.y = HIWORD(lParam);
			GetClientRect(GetDlgItem(hDlg, IDC_URL),&r);
			p2.x = r.left;
			p2.y = r.top;
			p3.x = r.right;
			p3.y = r.bottom;
			MapWindowPoints(GetDlgItem(hDlg, IDC_URL),hDlg, &p2, 1);
			MapWindowPoints(GetDlgItem(hDlg, IDC_URL),hDlg, &p3, 1);

			if ((p.x >= p2.x) && (p.x <= p3.x) && (p.y <= p3.y) && (p.y >= p2.y)) {
				ShellExecute(NULL, "open", "http://www.unrealircd.com", NULL, NULL,
					SW_MAXIMIZE);
				EndDialog(hDlg, TRUE);
				return 0;
			}
			GetClientRect(GetDlgItem(hDlg, IDC_EMAIL),&r);
			p2.x = r.left;
			p2.y = r.top;
			p3.x = r.right;
			p3.y = r.bottom;
			MapWindowPoints(GetDlgItem(hDlg, IDC_EMAIL),hDlg, &p2, 1);
			MapWindowPoints(GetDlgItem(hDlg, IDC_EMAIL),hDlg, &p3, 1);

			if ((p.x >= p2.x) && (p.x <= p3.x) && (p.y <= p3.y) && (p.y >= p2.y)) {
				ShellExecute(NULL, "open", "mailto:unreal-dev@lists.sourceforge.net", NULL,
					NULL, SW_MAXIMIZE);
				EndDialog(hDlg, TRUE);
				return 0;
			}

			return 0;
		}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_DESTROY:
			DeleteObject(hBrush);
			DeleteObject(hFont);
			break;

		}
	return (FALSE);
}

LRESULT CALLBACK ConfigDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0';
			struct stat sb;
			SetWindowText(hDlg, "UnrealIRCd Configuration File");
			if ((fd = open(CPATH, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				SetDlgItemText(hDlg, IDC_TEXT, buffer);
				close(fd);
				free(buffer);
			}
			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDOK) {
				int fd, len;
				char *buffer = '\0', string[21];
				len = SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_GETTEXTLENGTH, 0, 0);
				len++;
				/* Again, only allocate the amount we need */
				buffer = (char *)malloc(len);
				GetDlgItemText(hDlg, IDC_TEXT, buffer, len);
				fd = open(CPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY);
				write(fd, buffer, len);
				close(fd);
				free(buffer);
				EndDialog(hDlg, TRUE);
			}

			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK MotdDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static struct stat sb;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0';
			SetWindowText(hDlg, "UnrealIRCd MOTD File");
			if ((fd = open(MPATH, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				SetDlgItemText(hDlg, IDC_TEXT, buffer);
				close(fd);
				free(buffer);
			}
			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDOK) {
				int fd, len;
				char *buffer = '\0', string[21];
				len = SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_GETTEXTLENGTH, 0, 0);
				len++;
				/* Again, only allocate the amount we need */
				buffer = (char *)malloc(len);
				GetDlgItemText(hDlg, IDC_TEXT, buffer, len);
				fd = open(MPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY);
				write(fd, buffer, len);
				close(fd);
				free(buffer);
				EndDialog(hDlg, TRUE);
			}

			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK OperMotdDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static struct stat sb;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0';
			SetWindowText(hDlg, "UnrealIRCd OperMOTD File");
			if ((fd = open(OPATH, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				SetDlgItemText(hDlg, IDC_TEXT, buffer);
				close(fd);
				free(buffer);
			}
			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDOK) {
				int fd, len;
				char *buffer = '\0', string[21];
				len = SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_GETTEXTLENGTH, 0, 0);
				len++;
				/* Again, only allocate the amount we need */
				buffer = (char *)malloc(len);
				GetDlgItemText(hDlg, IDC_TEXT, buffer, len);
				fd = open(OPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY);
				write(fd, buffer, len);
				close(fd);
				free(buffer);
				EndDialog(hDlg, TRUE);
			}

			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK BotMotdDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static struct stat sb;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0';
			SetWindowText(hDlg, "UnrealIRCd BotMOTD File");
			if ((fd = open(BPATH, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				SetDlgItemText(hDlg, IDC_TEXT, buffer);
				close(fd);
				free(buffer);
			}
			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDOK) {
				int fd, len;
				char *buffer = '\0', string[21];
				len = SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_GETTEXTLENGTH, 0, 0);
				len++;
				/* Again, only allocate the amount we need */
				buffer = (char *)malloc(len);
				GetDlgItemText(hDlg, IDC_TEXT, buffer, len);
				fd = open(BPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY);
				write(fd, buffer, len);
				close(fd);
				free(buffer);
				EndDialog(hDlg, TRUE);
			}

			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK RulesDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static struct stat sb;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0';
			SetWindowText(hDlg, "UnrealIRCd Rules File");
			if ((fd = open(RPATH, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				SetDlgItemText(hDlg, IDC_TEXT, buffer);
				close(fd);
				free(buffer);
			}
			return (TRUE);
			}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				EndDialog(hDlg, TRUE);
			if (LOWORD(wParam) == IDOK) {
				int fd, len;
				char *buffer = '\0', string[21];
				len = SendMessage(GetDlgItem(hDlg, IDC_TEXT), WM_GETTEXTLENGTH, 0, 0);
				len++;
				/* Again, only allocate the amount we need */
				buffer = (char *)malloc(len);
				GetDlgItemText(hDlg, IDC_TEXT, buffer, len);
				fd = open(RPATH, _O_TRUNC|_O_CREAT|_O_RDWR|_O_BINARY);
				write(fd, buffer, len);
				close(fd);
				free(buffer);
				EndDialog(hDlg, TRUE);
			}

			break;
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		}
	return (FALSE);
}

LRESULT CALLBACK StatusDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_INITDIALOG: {
				hwTreeView = GetDlgItem(hDlg, IDC_TREE);
				win_map(&me, hwTreeView);
				SetDlgItemInt(hDlg, IDC_CLIENTS, IRCstats.clients, FALSE);
				SetDlgItemInt(hDlg, IDC_SERVERS, IRCstats.servers, FALSE);
				SetDlgItemInt(hDlg, IDC_INVISO, IRCstats.invisible, FALSE);
				SetDlgItemInt(hDlg, IDC_INVISO, IRCstats.invisible, FALSE);
				SetDlgItemInt(hDlg, IDC_UNKNOWN, IRCstats.unknown, FALSE);
				SetDlgItemInt(hDlg, IDC_OPERS, IRCstats.operators, FALSE);
				SetDlgItemInt(hDlg, IDC_CHANNELS, IRCstats.channels, FALSE);
				if (IRCstats.clients > IRCstats.global_max)
					IRCstats.global_max = IRCstats.clients;
				if (IRCstats.me_clients > IRCstats.me_max)
						IRCstats.me_max = IRCstats.me_clients;
				SetDlgItemInt(hDlg, IDC_MAXCLIENTS, IRCstats.global_max, FALSE);
				SetDlgItemInt(hDlg, IDC_LCLIENTS, IRCstats.me_clients, FALSE);
				SetDlgItemInt(hDlg, IDC_LSERVERS, IRCstats.me_servers, FALSE);
				SetDlgItemInt(hDlg, IDC_LMAXCLIENTS, IRCstats.me_max, FALSE);
				return (TRUE);
			}
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;

		}
	return (FALSE);
}


/* Modified StripColors that also strips bold reverse and underline. Makes sure we dont
 * Display any codes in the edit boxes -- codemastr
 */

static char *StripCodes(char *buffer)
{
	static char tmp[512], out[512];
	int  i = 0, j = 0, hascomma = 0;

	bzero((char *)out, sizeof(out));
	memcpy(tmp, buffer, 512);

	while (tmp[i])
	{
		/* Strip other codes stuff */
		while (tmp[i] == '\002' || tmp[i] == '\031' || tmp[i] == '\022' || tmp[i] == '\015' || tmp[i] == '\037') {
			i++;
		}

		/* Color code found, so parse */
		if (tmp[i] == '\003')
		{
			hascomma = 0;

			/* Increase it by 1 so we can see if it is valid */
			i++;
			/* It's fake, so continue */
			if (!isdigit(tmp[i]))
				continue;

			while (isdigit(tmp[i]) || (isdigit(tmp[i - 1])
			    && tmp[i] == ',' && isdigit(tmp[i + 1])
			    && hascomma == 0))
			{
				if (tmp[i] == ',' && hascomma == 1)
					break;

				if (tmp[i] == ',' && hascomma == 0)
					hascomma = 1;
				i++;
			}
			continue;
		}
		out[j] = tmp[i];
		i++;
		j++;
	}
	return out;

}

/* This was made by DrBin but I cleaned it up a bunch to make it work better */

HTREEITEM AddItemToTree(HWND hWnd, LPSTR lpszItem, int nLevel)
{
    TVITEM tvi; 
    TVINSERTSTRUCT tvins; 
    static HTREEITEM hPrev = (HTREEITEM)TVI_FIRST; 
	static HTREEITEM hPrevLev[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    HTREEITEM hti; 
 
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
    if (nLevel > 1) { 
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
void win_map(aClient *server, HWND hwTreeView)
{
        static char prompt[64];
        aClient *acptr;
		Link *lp;
		AddItemToTree(hwTreeView,server->name,server->hopcount+1);
		for (lp = Servers; lp; lp = lp->next)
        {
                acptr = lp->value.cptr;
                if (acptr->srvptr != server)
                        continue;
                win_map(acptr, hwTreeView);
        }
}

/* ugly stuff, but hey it works -- codemastr */
void win_log(char *format, ...) {
        va_list ap;
        char buf[2048];
		char *buf2;
        va_start(ap, format);
        ircvsprintf(buf, format, ap);
	strcat(buf, "\r\n");
	if (errors) {
		buf2 = MyMalloc(strlen(errors)+strlen(buf)+1);
		sprintf(buf2, "%s%s",errors,buf);
		MyFree(errors);
		errors = NULL;
	}
	else {
		buf2 = MyMalloc(strlen(buf)+1);
		sprintf(buf2, "%s",buf);
	}
	errors = buf2;
        va_end(ap);
}

void win_error() {
	if (errors)
		DialogBox(hInst, "ConfigError", hwIRCDWnd, (DLGPROC)ConfigErrorDLG);
}

LRESULT CALLBACK ConfigErrorDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG:
			MessageBeep(MB_ICONEXCLAMATION);
			SetDlgItemText(hDlg, IDC_CONFIGERROR, errors);
			MyFree(errors);
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
