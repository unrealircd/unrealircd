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

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

#define WIN32_VERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH9
#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#include "version.h"
#include "setup.h"
#include <commctrl.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include "h.h"
#include <richedit.h>
#include <commdlg.h>

#define MIRC_COLORS "{\\colortbl ;\\red255\\green255\\blue255;\\red0\\green0\\blue127;\\red0\\green147\\blue0;\\red255\\green0\\blue0;\\red147\\green0\\blue0;\\red128\\green0\\blue128;\\red255\\green128\\blue0;\\red255\\green255\\blue0;\\red0\\green255\\blue0;\\red0\\green128\\blue128;\\red0\\green255\\blue255;\\red0\\green0\\blue252;\\red255\\green0\\blue255;\\red128\\green128\\blue128;\\red192\\green192\\blue192;\\red0\\green0\\blue0;}"

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
LRESULT CALLBACK StatusDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ConfigErrorDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ColorDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FromVarDLG(HWND, UINT, WPARAM, LPARAM, char *, char **);
LRESULT CALLBACK FromFileDLG(HWND, UINT, WPARAM, LPARAM);

typedef struct {
	int *size;
	char **buffer;
} StreamIO;

extern  void      SocketLoop(void *dummy), rehash(aClient *, aClient *, int);
int CountRTFSize(char *);
void IRCToRTF(char *, char *);
HINSTANCE hInst;
NOTIFYICONDATA SysTray;
void CleanUp(void);
HTREEITEM AddItemToTree(HWND, LPSTR, int, short);
void win_map(aClient *, HWND, short);
extern Link *Servers;
extern ircstats IRCstats;
char *errors = NULL, *RTFBuf = NULL;
extern aMotd *botmotd, *opermotd, *motd, *rules;
void CleanUp(void)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
void CleanUpSegv(int sig)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
HWND hwIRCDWnd=NULL;
HWND hwTreeView;
HWND hWndMod;
HANDLE hMainThread = 0;
UINT WM_TASKBARCREATED;
FARPROC lpfnOldWndProc;
HMENU hContext;
OSVERSIONINFO VerInfo;
char OSName[256];

void TaskBarCreated() {
	HICON hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(ICO_MAIN), IMAGE_ICON,16, 16, 0);
	SysTray.cbSize = sizeof(NOTIFYICONDATA);
	SysTray.hIcon = hIcon;
	SysTray.hWnd = hwIRCDWnd;
	SysTray.uCallbackMessage = WM_USER;
	SysTray.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
	SysTray.uID = 0;
	lstrcpy(SysTray.szTip, WIN32_VERSION);
	Shell_NotifyIcon(NIM_ADD ,&SysTray);
}

LRESULT LinkSubClassFunc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	static HCURSOR hCursor;
	if (!hCursor)
		hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
	if (Message == WM_MOUSEMOVE || WM_LBUTTONUP)
		SetCursor(hCursor);

	return CallWindowProc((WNDPROC)lpfnOldWndProc, hWnd, Message, wParam, lParam);
}



LRESULT RESubClassFunc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	POINT p;
	RECT r;
	DWORD start, end;
	char string[500];

	if (Message == WM_GETDLGCODE)
	   return DLGC_WANTALLKEYS;

	
	if (Message == WM_CONTEXTMENU) {
		p.x = GET_X_LPARAM(lParam);
		p.y = GET_Y_LPARAM(lParam);
		if (GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1) {
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
		if (GetWindowLong(hWnd, GWL_STYLE) & ES_READONLY) {
			EnableMenuItem(hContext, IDM_CUT, MF_BYCOMMAND|MF_GRAYED);
			EnableMenuItem(hContext, IDM_DELETE, MF_BYCOMMAND|MF_GRAYED);
		}
		else {
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

/* Somewhat respectable RTF to IRC parser
 * (c) 2001 codemastr
 */
typedef struct colorlist {
	char *color;
	struct colorlist *next, *prev;
} ColorList;

ColorList *TextColors = NULL;
void AddColor(char *color) {
	ColorList *clist;

	clist = MyMalloc(sizeof(ColorList));
	if (!clist)
		return;
	clist->color = strdup(color);
	clist->prev = NULL;
	clist->next = TextColors;
	if (TextColors)
		TextColors->prev = clist;
	TextColors = clist;
}

ColorList *DelNewestColor() {
	ColorList *p = TextColors, *q = TextColors->next;
	MyFree(p->color);

	TextColors = p->next;

	if (p->next)
		p->next->prev = NULL;
	MyFree(p);
	return q;
}

void WipeColors() {
	ColorList *clist, q;

	for (clist = TextColors; clist; clist = clist->next)
	{
			q.next = clist->next;
			MyFree(clist->color);
			MyFree(clist);
			clist = &q;
	}

}
DWORD CALLBACK SplitIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) {
	StreamIO *stream = (StreamIO*)dwCookie;
	if (*stream->size == 0)
		pcb = 0;
	if (cb <= *stream->size) {
		memcpy(pbBuff, *stream->buffer, cb);
		*stream->buffer += cb;
		*stream->size -= cb;
		*pcb = cb;

	}
	else {
		memcpy(pbBuff, *stream->buffer, *stream->size);
		*pcb = *stream->size;
		*stream->size = 0;
	}
	return 0;
}

DWORD CALLBACK BufferIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) {
	char *buf2;
	static long size = 0;
	if (!RTFBuf)
		size = 0;

	buf2 = MyMalloc(size+cb+1);

	if (RTFBuf)
		memcpy(buf2,RTFBuf,size);

	memcpy(buf2+size,pbBuff,cb);

	size += cb;
	if (RTFBuf)
		MyFree(RTFBuf);

	RTFBuf = buf2;

	pcb = &cb;
	return 0;
}

DWORD CALLBACK RTFToIRC(int fd, char *pbBuff, long cb) {
	char *buffer = (char *)malloc(cb);
	int i = 0, j = 0, k = 0, start = 0, end = 0;
	int incolor = 0, bold = 0, uline = 0;
	char cmd[15], value[500], color[25], colorbuf[4];
	char colors[16];
	pbBuff++;
	bzero(buffer, cb);
	for (; *pbBuff; pbBuff++) {
		if (*pbBuff == '\r' || *pbBuff == '\n')
			continue;
		if (*pbBuff == '{' || *pbBuff == '}')
			continue;
		if (*pbBuff == '\\') {
			pbBuff++;
			if (*pbBuff == '\\') {
				buffer[i] = '\\';
				i++;
				continue;
			}
			if (*pbBuff == '{') {
				buffer[i] = '{';
				i++;
				continue;
			}
			if (*pbBuff == '}') {
				buffer[i] = '}';
				i++;
				continue;
			}

			value[0] = cmd[0] = 0;
			for (j = k = start = end = 0;
				*pbBuff && *pbBuff != '\\' && *pbBuff != '\r' && *pbBuff != '\n';
				pbBuff++) {
					if (*pbBuff == '{') {
						start++;
						pbBuff++;
						for (; *pbBuff; pbBuff++) {
							if (*pbBuff == '{')
								start++;
							if (*pbBuff == '}') {
								end++;
								if (start == end) {
									pbBuff++;
									break;
								}
							}
							value[k] = *pbBuff;
							k++;
						}
						break;
					}
				if (*pbBuff == ' ') {
					pbBuff++;
					break;
				}

					cmd[j] = *pbBuff;
					j++;
			}
				cmd[j] = 0;
				value[k] = 0;
				if (!strcmp(cmd, "par")) {
					if (bold) 
						buffer[i++] = '\2';
					if (uline)
						buffer[i++] = '\37';
					if (incolor)
						buffer[i++] = '\3';
					buffer[i++] = '\r';
					buffer[i++] = '\n';
					if (bold)
						buffer[i++] = '\2';
					if (uline)
						buffer[i++] = '\37';
					if (incolor) {
						buffer[i++] = '\3';
						strcat(buffer, TextColors->color);
						i += strlen(TextColors->color);
					}
				}
				else if (!strcmp(cmd, "tab"))
					buffer[i++] = '\t';
				else if (!strcmp(cmd, "b")) {
					bold = 1;
					buffer[i++] = '\2';
				}
				else if (!strcmp(cmd, "b0")) {
					bold = 0;
					buffer[i++] = '\2';
				}

				else if (!strcmp(cmd, "ul")) { 
					uline = 1;
					buffer[i++] = '\37';
				}
				else if (!strcmp(cmd, "ulnone")) {
					uline = 0;
					buffer[i++] = '\37';
				}
				else if (!strcmp(cmd, "colortbl")) {
					int l = 0, m = 0;
					color[0] = 0;
					pbBuff++;
					for (; *pbBuff && *pbBuff != '}'; pbBuff++) {
						if (*pbBuff != ';') {
							color[l] = *pbBuff;
							l++;
						}
						else {
							color[l] = 0;
							l = 0;
							m++;
							if (!strcmp(color, "\\red255\\green255\\blue255"))
								colors[m] = 0;
							else if (!strcmp(color, "\\red0\\green0\\blue0"))
								colors[m] = 1;
							else if (!strcmp(color, "\\red0\\green0\\blue127"))
								colors[m] = 2;
							else if (!strcmp(color, "\\red0\\green147\\blue0"))
								colors[m] = 3;
							else if (!strcmp(color, "\\red255\\green0\\blue0"))
								colors[m] = 4;
							else if (!strcmp(color, "\\red127\\green0\\blue0"))
								colors[m] = 5;
							else if (!strcmp(color, "\\red156\\green0\\blue156"))
								colors[m] = 6;
							else if (!strcmp(color, "\\red252\\green127\\blue0"))
								colors[m] = 7;
							else if (!strcmp(color, "\\red255\\green255\\blue0"))
								colors[m] = 8;
							else if (!strcmp(color, "\\red0\\green252\\blue0"))
								colors[m] = 9;
							else if (!strcmp(color, "\\red0\\green147\\blue147"))
								colors[m] = 10;
							else if (!strcmp(color, "\\red0\\green255\\blue255"))
								colors[m] = 11;
							else if (!strcmp(color, "\\red0\\green0\\blue252"))
								colors[m] = 12;
							else if (!strcmp(color, "\\red255\\green0\\blue255"))
								colors[m] = 13;
							else if (!strcmp(color, "\\red127\\green127\\blue127"))
								colors[m] = 14;
							else if (!strcmp(color, "\\red210\\green210\\blue210")) 
								colors[m] = 15;
						}
					}
					pbBuff++;
				}
				else if (!strcmp(cmd, "f1")) {
					write(fd, buffer, i);
					close(fd);
					return 0;
				}
				else if (!strcmp(cmd, "cf0")) {
					incolor = 0;
					buffer[i++] = '\3';
					DelNewestColor();
				}
				else if (!strncmp(cmd, "cf", 2)) {
					char number[3];
					int num = 0;
					incolor = 1;
					strcpy(number, &cmd[2]);
					num = atoi(number);
					buffer[i++] = '\3';
					sprintf(number, "%d", colors[num]);
					AddColor(number);
					strcat(buffer, number);
					i += strlen(number);
				}
				pbBuff--;
				continue;
		}
		else {
			buffer[i] = *pbBuff;
			i++;
		}
	}
	write(fd, buffer, i);
	close(fd);
	WipeColors();
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	char *s;
	HWND hWnd;
	WSADATA WSAData;
	HICON hIcon;
	InitCommonControls();

	WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");
	atexit(CleanUp);
	if(!LoadLibrary("riched20.dll"))
		LoadLibrary("riched32.dll");
	InitStackTraceLibrary();
	VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&VerInfo);
	strcpy(OSName, "Windows ");
	if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		if (VerInfo.dwMajorVersion == 4) {
			if (VerInfo.dwMinorVersion == 0) {
				strcat(OSName, "95 ");
				if (!strcmp(VerInfo.szCSDVersion," C"))
					strcat(OSName, "OSR2 ");
			}
			else if (VerInfo.dwMinorVersion == 10) {
				strcat(OSName, "98 ");
				if (!strcmp(VerInfo.szCSDVersion, " A"))
					strcat(OSName, "SE ");
			}
			else if (VerInfo.dwMinorVersion == 90)
				strcat(OSName, "Me ");
		}
	}
	else if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		if (VerInfo.dwMajorVersion == 3 && VerInfo.dwMinorVersion == 51)
			strcat(OSName, "NT 3.51 ");
		else if (VerInfo.dwMajorVersion == 4 && VerInfo.dwMinorVersion == 0)
			strcat(OSName, "NT 4.0 ");
		else if (VerInfo.dwMajorVersion == 5) {
			if (VerInfo.dwMinorVersion == 0)
				strcat(OSName, "2000 ");
			else if (VerInfo.dwMinorVersion == 1)
				strcat(OSName, "XP ");
		}
		strcat(OSName, VerInfo.szCSDVersion);
	}
	if (OSName[strlen(OSName)-1] == ' ')
		OSName[strlen(OSName)-1] = 0;

	if (WSAStartup(MAKEWORD(1, 1), &WSAData) != 0)
    	{
        MessageBox(NULL, "Unable to initialize WinSock", "UnrealIRCD Initalization Error", MB_OK);
        return FALSE;
	}
	hInst = hInstance; 
    
	hWnd = CreateDialog(hInstance, "WIRCD", 0, (DLGPROC)MainDLG); 
	hwIRCDWnd = hWnd;
	
	TaskBarCreated();

	if (InitwIRCD(__argc, __argv) != 1)
	{
		MessageBox(NULL, "UnrealIRCd has failed to initialize in InitwIRCD()", "UnrealIRCD Initalization Error" ,MB_OK);
		return FALSE;
	}

	hMainThread = (HANDLE)_beginthread(SocketLoop, 0, NULL);
	while (GetMessage(&msg, NULL, 0, 0))
    {
		if (hWndMod == NULL || !IsDialogMessage(hWndMod, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }
	return FALSE;

}

LRESULT CALLBACK MainDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
static HCURSOR hCursor;
static HMENU hRehash, hAbout, hConfig, hTray;

	char *argv[3];
	aClient *paClient;
	char *msg;
	POINT p;

	if (message == WM_TASKBARCREATED){
		TaskBarCreated();
		return TRUE;
	}
	
	switch (message)
			{
			case WM_INITDIALOG: {
				hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
				hContext = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_CONTEXT)),0);
				/* Rehash popup menu */
				hRehash = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_REHASH)),0);
				/* About popup menu */
				hAbout = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_ABOUT)),0);
				/* Systray popup menu set the items to point to the other menus*/
				hTray = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_SYSTRAY)),0);
				ModifyMenu(hTray, IDM_REHASH, MF_BYCOMMAND|MF_POPUP|MF_STRING, (UINT)hRehash, "&Rehash");
				ModifyMenu(hTray, IDM_CONFIG, MF_BYCOMMAND|MF_POPUP|MF_STRING, (UINT)hConfig, "&Config");
				ModifyMenu(hTray, IDM_ABOUT, MF_BYCOMMAND|MF_POPUP|MF_STRING, (UINT)hAbout, "&About");
				
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
					case WM_RBUTTONDOWN:
						SetForegroundWindow(hDlg);
						break;
					case WM_RBUTTONUP: 
						GetCursorPos(&p);
						TrackPopupMenu(hTray, TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
						/* Kludge for a win bug */
						SendMessage(hDlg, WM_NULL, 0, 0);
						break;
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
				ClientToScreen(hDlg,&p);
				TrackPopupMenu(hRehash,TPM_LEFTALIGN|TPM_LEFTBUTTON,p.x,p.y,0,hDlg,NULL);
				return 0;
			 }
			 else if ((p.x >= 85) && (p.x <= 132) && (p.y >= 178) && (p.y <= 190))  {
				 DialogBox(hInst, "Status", hDlg, (DLGPROC)StatusDLG);
				 return 0;
			 }
			 else if ((p.x >= 140) && (p.x <= 186) && (p.y >= 178) && (p.y <= 190))  {
				unsigned long i = 60000;
				ClientToScreen(hDlg,&p);
				DestroyMenu(hConfig);
				hConfig = CreatePopupMenu();

				AppendMenu(hConfig, MF_STRING, IDM_CONF, CPATH);
				AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);

				if (conf_include) {
					ConfigItem_include *inc;
					for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next) {
						AppendMenu(hConfig, MF_STRING, i++, inc->file);
					}
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
				}

				AppendMenu(hConfig, MF_STRING, IDM_MOTD, MPATH);
				AppendMenu(hConfig, MF_STRING, IDM_OPERMOTD, OPATH);
				AppendMenu(hConfig, MF_STRING, IDM_BOTMOTD, BPATH);
				AppendMenu(hConfig, MF_STRING, IDM_RULES, RPATH);
				
				if (conf_tld) {
					ConfigItem_tld *tlds;
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
					for (tlds = conf_tld; tlds; tlds = (ConfigItem_tld *)tlds->next) {
						if (!tlds->flag.motdptr)
							AppendMenu(hConfig, MF_STRING, i++, tlds->motd_file);
						if (!tlds->flag.rulesptr)
							AppendMenu(hConfig, MF_STRING, i++, tlds->rules_file);
					}
				}
				AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
				AppendMenu(hConfig, MF_STRING, IDM_NEW, "New File");
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
				if (LOWORD(wParam) >= 60000 && HIWORD(wParam) == 0 && !lParam) {
					char path[MAX_PATH];
					GetMenuString(hConfig,LOWORD(wParam), path, MAX_PATH, MF_BYCOMMAND);
					DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
							(LPARAM)path);
					return FALSE;
				}

				switch(LOWORD(wParam)) {

					case IDM_STATUS:
						DialogBox(hInst, "Status", hDlg, (DLGPROC)StatusDLG);
						break;
					case IDM_SHUTDOWN:
						if (MessageBox(hDlg, "Close UnrealIRCd?", "Are you sure?", MB_YESNO|MB_ICONQUESTION) == IDNO)
							return 0;
						else {
							 DestroyWindow(hDlg);
							exit(0);
						}
						break;

					case IDM_RHALL:
						MessageBox(NULL, "Rehashing all files", "Rehashing", MB_OK);
						sendto_realops("Rehashing all files via the console");
						rehash(&me,&me,0);
						opermotd = (aMotd *) read_file(OPATH, &opermotd);
						botmotd = (aMotd *) read_file(BPATH, &botmotd);
						break;
					case IDM_RHCONF:
						MessageBox(NULL, "Rehashing the Config file", "Rehashing", MB_OK);
						sendto_realops("Rehashing the Config file via the console");
						rehash(&me,&me,0);
						break;
					case IDM_RHMOTD: {
						ConfigItem_tld *tlds;
						aMotd *amotd;
						MessageBox(NULL, "Rehashing all MOTD and Rules files", "Rehashing", MB_OK);
						motd = (aMotd *) read_motd(MPATH);
						rules = (aMotd *) read_rules(RPATH);
						for (tlds = conf_tld; tlds;
						    tlds = (ConfigItem_tld *) tlds->next) {
							if (!tlds->flag.motdptr) {
								while (tlds->motd)
								{
									amotd = tlds->motd->next;
									MyFree(tlds->motd->line);
									MyFree(tlds->motd);
									tlds->motd = amotd;
								}
							}
							tlds->motd = read_motd(tlds->motd_file);
							if (!tlds->flag.rulesptr) {
								while (tlds->rules)
								{
									amotd = tlds->rules->next;
									MyFree(tlds->rules->line);
									MyFree(tlds->rules);
									tlds->rules = amotd;
								}
							}
							tlds->rules = read_rules(tlds->rules_file);
						}
						sendto_realops("Rehashing all MOTD and Rules files via the console");
						break;
					}
					case IDM_RHOMOTD:
						MessageBox(NULL, "Rehashing the OperMOTD", "Rehashing", MB_OK);
						opermotd = (aMotd *) read_file(OPATH, &opermotd);
						sendto_realops("Rehashing the OperMOTD via the console");
						break;
					case IDM_RHBMOTD:
						MessageBox(NULL, "Rehashing the BotMOTD", "Rehashing", MB_OK);
						botmotd = (aMotd *) read_file(BPATH, &botmotd);
						sendto_realops("Rehashing the BotMOTD via the console");
						break;
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
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
							(LPARAM)CPATH);
						break;
					case IDM_MOTD:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
							(LPARAM)MPATH);
						break;
					case IDM_OPERMOTD:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
							(LPARAM)OPATH);
						break;
					case IDM_BOTMOTD:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
							(LPARAM)BPATH);
						break;
					case IDM_RULES:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG,
							(LPARAM)RPATH);
						break;
					case IDM_NEW:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, (LPARAM)NULL);
						break;

				}
			}
	}
	return (FALSE);
}

LRESULT CALLBACK LicenseDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd License", gnulicense);
}

LRESULT CALLBACK CreditsDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd Credits", unrealcredits);
}

LRESULT CALLBACK DalDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return FromVarDLG(hDlg, message, wParam, lParam, "UnrealIRCd DALnet Credits", dalinfotext);
}

LRESULT CALLBACK FromVarDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam, char *title, char **s) {
	HWND hWnd;
	switch (message) {
		case WM_INITDIALOG: {
			char	String[16384];
			int size;
			char *RTFString;
			StreamIO *stream = malloc(sizeof(StreamIO));
			EDITSTREAM edit;
			SetWindowText(hDlg, title);
			bzero(String, 16384);
			lpfnOldWndProc = (FARPROC)SetWindowLong(GetDlgItem(hDlg, IDC_TEXT), GWL_WNDPROC, (DWORD)RESubClassFunc);
			while (*s) {
				strcat(String, *s++);
				if (*s)
					strcat(String, "\r\n");
			    }
			size = CountRTFSize(String)+1;
			RTFString = malloc(size);
			bzero(RTFString, size);
			IRCToRTF(String,RTFString);
			RTFBuf = RTFString;
			size--;
			stream->size = &size;
			stream->buffer = &RTFBuf;
			edit.dwCookie = (UINT)stream;
			edit.pfnCallback = SplitIt;
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_STREAMIN,
(WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
			free(RTFString);	
			free(stream);
			return (TRUE);
			}

		case WM_COMMAND: {
			hWnd = GetDlgItem(hDlg, IDC_TEXT);
		if (LOWORD(wParam) == IDOK)
				return EndDialog(hDlg, TRUE);
		if (LOWORD(wParam) == IDM_COPY) {
			SendMessage(hWnd, WM_COPY, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_SELECTALL) {
			SendMessage(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PASTE) {
			SendMessage(hWnd, WM_PASTE, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_CUT) {
			SendMessage(hWnd, WM_CUT, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_UNDO) {
			SendMessage(hWnd, EM_UNDO, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_DELETE) {
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


LRESULT CALLBACK HelpDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont;
	static HCURSOR hCursor;
	switch (message) {
		case WM_INITDIALOG:
			hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
			hFont = CreateFont(8,0,0,0,0,0,1,0,ANSI_CHARSET,0,0,PROOF_QUALITY,0,"MS Sans Serif");
			SendMessage(GetDlgItem(hDlg, IDC_EMAIL), WM_SETFONT, (WPARAM)hFont,TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_URL), WM_SETFONT, (WPARAM)hFont,TRUE);
			lpfnOldWndProc = (FARPROC)SetWindowLong(GetDlgItem(hDlg, IDC_EMAIL), GWL_WNDPROC, (DWORD)LinkSubClassFunc);
			SetWindowLong(GetDlgItem(hDlg, IDC_URL), GWL_WNDPROC, (DWORD)LinkSubClassFunc);
			return (TRUE);

		case WM_DRAWITEM: {
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			char text[500];
			COLORREF oldtext;
			RECT focus;
			GetWindowText(lpdis->hwndItem, text, 500);
			if (wParam == IDC_URL || IDC_EMAIL) {
				FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(COLOR_3DFACE));
				oldtext = SetTextColor(lpdis->hDC, RGB(0,0,255));
				DrawText(lpdis->hDC, text, strlen(text), &lpdis->rcItem, DT_CENTER|DT_VCENTER);
				SetTextColor(lpdis->hDC, oldtext);
				if (lpdis->itemState & ODS_FOCUS) {
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
			if (HIWORD(wParam) == BN_DBLCLK) {
				if (LOWORD(wParam) == IDC_URL) 
					ShellExecute(NULL, "open", "http://www.unrealircd.com", NULL, NULL, 
						SW_MAXIMIZE);
				else if (LOWORD(wParam) == IDC_EMAIL)
					ShellExecute(NULL, "open", "mailto:coders@lists.unrealircd.org", NULL, NULL, 
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
	return (FALSE);
}

HWND DrawToolbar(HWND hwndParent, UINT iID) {
	HWND hTool;
	TBADDBITMAP tbBit;
	int newidx;
	TBBUTTON tbButtons[7] = {
		{ STD_FILENEW, IDM_NEW, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_FILESAVE, IDM_SAVE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0},
		{ STD_CUT, IDM_CUT, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_COPY, IDM_COPY, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_PASTE, IDM_PASTE, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0}
	};
		
	TBBUTTON tbAddButtons[3] = {
		{ 0, IDC_BOLD, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0L, 0},
		{ 1, IDC_UNDERLINE, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0L, 0},
		{ 2, IDC_COLOR, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0}
	};
	hTool = CreateToolbarEx(hwndParent, WS_VISIBLE|WS_CHILD|TBSTYLE_FLAT|TBSTYLE_TOOLTIPS, 
				IDC_TOOLBAR, 0, HINST_COMMCTRL, IDB_STD_SMALL_COLOR,
				tbButtons, 7, 0,0,100,30, sizeof(TBBUTTON));
	tbBit.hInst = hInst;
	tbBit.nID = IDB_BITMAP1;
	newidx = SendMessage(hTool, TB_ADDBITMAP, (WPARAM)3, (LPARAM)&tbBit);
	tbAddButtons[0].iBitmap += newidx;
	tbAddButtons[1].iBitmap += newidx;
	tbAddButtons[2].iBitmap += newidx;
	SendMessage(hTool, TB_ADDBUTTONS, (WPARAM)3, (LPARAM)&tbAddButtons);
	return hTool;
}

LRESULT CALLBACK FromFileDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	HWND hWnd;
	static FINDREPLACE find;
	static char *file;
	static HWND hTool, hClip;
	CHARFORMAT2 chars;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			char *buffer = '\0', *string = '\0';
			EDITSTREAM edit;
			StreamIO *stream = malloc(sizeof(StreamIO));
			char szText[256];
			struct stat sb;
			file = (char *)lParam;
			if (file)
				wsprintf(szText, "UnrealIRCd Editor - %s", file);
			else 
				strcpy(szText, "UnrealIRCd Editor - New File");
			SetWindowText(hDlg, szText);
			lpfnOldWndProc = (FARPROC)SetWindowLong(GetDlgItem(hDlg, IDC_TEXT), GWL_WNDPROC, (DWORD)RESubClassFunc);
			hTool = DrawToolbar(hDlg, IDC_TOOLBAR);
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_SETEVENTMASK, 0, (LPARAM)ENM_SELCHANGE);
			chars.cbSize = sizeof(CHARFORMAT2);
			chars.dwMask = CFM_FACE;
			strcpy(chars.szFaceName,"Fixedsys");
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&chars);
			if ((fd = open(file, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = (char *)malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				len = CountRTFSize(buffer)+1;
				string = (char *)malloc(len);
				bzero(string,len);
				IRCToRTF(buffer,string);
				RTFBuf = string;
				len--;
				stream->size = &len;
				stream->buffer = &RTFBuf;
				edit.dwCookie = (UINT)stream;
				edit.pfnCallback = SplitIt;
				SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_STREAMIN,
					(WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
				SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_SETMODIFY,
					(WPARAM)FALSE, 0);

				close(fd);
				RTFBuf = NULL;
				free(buffer);
				free(string);
				free(stream);
				hClip = SetClipboardViewer(hDlg);
				if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_CANPASTE, 0, 0)) 
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(FALSE,0));
			}
			return (TRUE);
			}
		case WM_NOTIFY:
			if (((NMHDR *)lParam)->code == EN_SELCHANGE) {
				HWND hWnd = GetDlgItem(hDlg, IDC_TEXT);
				DWORD start, end;
				chars.cbSize = sizeof(CHARFORMAT2);
				SendMessage(hWnd, EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
				if (chars.dwMask & CFM_BOLD && chars.dwEffects & CFE_BOLD)
					SendMessage(hTool, TB_CHECKBUTTON, (WPARAM)IDC_BOLD, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_CHECKBUTTON, (WPARAM)IDC_BOLD, (LPARAM)MAKELONG(FALSE,0));
				if (chars.dwMask & CFM_UNDERLINE && chars.dwEffects & CFE_UNDERLINE)
					SendMessage(hTool, TB_CHECKBUTTON, (WPARAM)IDC_UNDERLINE, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_CHECKBUTTON, (WPARAM)IDC_UNDERLINE, (LPARAM)MAKELONG(FALSE,0));
				SendMessage(hWnd, EM_GETSEL,(WPARAM)&start, (LPARAM)&end);
				if (start == end) {
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_COPY, (LPARAM)MAKELONG(FALSE,0));
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_CUT, (LPARAM)MAKELONG(FALSE,0));
				}
				else {
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_COPY, (LPARAM)MAKELONG(TRUE,0));
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_CUT, (LPARAM)MAKELONG(TRUE,0));
				}
		
				return (TRUE);
			}
			if (((NMHDR *)lParam)->code == TTN_GETDISPINFO) {
				LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;
				lpttt->hinst = NULL;
				switch (lpttt->hdr.idFrom) {
					case IDM_NEW:
						strcpy(lpttt->szText, "New");
						break;
					case IDM_SAVE:
						strcpy(lpttt->szText, "Save");
						break;
					case IDM_CUT:
						strcpy(lpttt->szText, "Cut");
						break;
					case IDM_COPY:
						strcpy(lpttt->szText, "Copy");
						break;
					case IDM_PASTE:
						strcpy(lpttt->szText, "Paste");
						break;
					case IDC_BOLD:
						strcpy(lpttt->szText, "Bold");
						break;
					case IDC_UNDERLINE:
						strcpy(lpttt->szText, "Underline");
						break;
					case IDC_COLOR:
						strcpy(lpttt->szText, "Text Color");
						break;
				}
				return (TRUE);
			}
				
				return (TRUE);
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_BOLD) {
				hWnd = GetDlgItem(hDlg, IDC_TEXT);
				if (SendMessage(hTool, TB_ISBUTTONCHECKED, (WPARAM)IDC_BOLD, (LPARAM)0) != 0) {
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_BOLD;
					chars.dwEffects = CFE_BOLD;
					chars.wWeight = FW_HEAVY;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				else {
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_BOLD;
					chars.dwEffects = 0;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				return TRUE;
			}
			else if (LOWORD(wParam) == IDC_UNDERLINE) {
				hWnd = GetDlgItem(hDlg, IDC_TEXT);
				if (SendMessage(hTool, TB_ISBUTTONCHECKED, (WPARAM)IDC_UNDERLINE, (LPARAM)0) != 0) {
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_UNDERLINETYPE;
					chars.bUnderlineType = CFU_UNDERLINE;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				else {
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_UNDERLINETYPE;
					chars.bUnderlineType = CFU_UNDERLINENONE;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				return TRUE;
			}
			if (LOWORD(wParam) == IDC_COLOR) 
				DialogBoxParam(hInst, "Color", hDlg, (DLGPROC)ColorDLG, (LPARAM)WM_USER+10);
		hWnd = GetDlgItem(hDlg, IDC_TEXT);
		if (LOWORD(wParam) == IDM_COPY) {
			SendMessage(hWnd, WM_COPY, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_SELECTALL) {
			SendMessage(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PASTE) {
			SendMessage(hWnd, WM_PASTE, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_CUT) {
			SendMessage(hWnd, WM_CUT, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_UNDO) {
			SendMessage(hWnd, EM_UNDO, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_DELETE) {
			SendMessage(hWnd, WM_CLEAR, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_SAVE) {
			int fd;
			EDITSTREAM edit;
			OPENFILENAME lpopen;
			if (!file) {
				char path[MAX_PATH];
				path[0] = '\0';
				bzero(&lpopen, sizeof(OPENFILENAME));
				lpopen.lStructSize = sizeof(OPENFILENAME);
				lpopen.hwndOwner = hDlg;
				lpopen.lpstrFilter = NULL;
				lpopen.lpstrCustomFilter = NULL;
				lpopen.nFilterIndex = 0;
				lpopen.lpstrFile = path;
				lpopen.nMaxFile = MAX_PATH;
				lpopen.lpstrFileTitle = NULL;
				lpopen.lpstrInitialDir = DPATH;
				lpopen.lpstrTitle = NULL;
				lpopen.Flags = (OFN_ENABLESIZING|OFN_NONETWORKBUTTON|
						OFN_OVERWRITEPROMPT);
				if (GetSaveFileName(&lpopen))
					file = path;
				else
					break;
			}
			fd = open(file, _O_TRUNC|_O_CREAT|_O_WRONLY|_O_BINARY,_S_IWRITE);
			edit.dwCookie = 0;
			edit.pfnCallback = BufferIt;
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_STREAMOUT, (WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
			RTFToIRC(fd, RTFBuf, strlen(RTFBuf));
			free(RTFBuf);
			RTFBuf = NULL;
			SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_SETMODIFY, (WPARAM)FALSE, 0);

			return 0;
		}
		if (LOWORD(wParam) == IDM_NEW) {
			char text[1024];
			BOOL newfile = FALSE;
			int ans;
			if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_GETMODIFY, 0, 0) != 0) {
				sprintf(text, "The text in the %s file has changed.\r\n\r\nDo you want to save the changes?", file ? file : "new");
				ans = MessageBox(hDlg, text, "UnrealIRCd", MB_YESNOCANCEL|MB_ICONWARNING);
				if (ans == IDNO)
					newfile = TRUE;
				if (ans == IDCANCEL)
					return TRUE;
				if (ans == IDYES) {
					SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDM_SAVE,0), 0);
					newfile = TRUE;
				}
			}
			else
				newfile = TRUE;
			if (newfile == TRUE) {
				char szText[256];
				file = NULL;
				strcpy(szText, "UnrealIRCd Editor - New File");
				SetWindowText(hDlg, szText);
				SetWindowText(GetDlgItem(hDlg, IDC_TEXT), NULL);
			}

			break;
		}			
			break;
		case WM_USER+10: {
			HWND hWnd = GetDlgItem(hDlg, IDC_TEXT);
			EndDialog((HWND)lParam, TRUE);
			chars.cbSize = sizeof(CHARFORMAT2);
			chars.dwMask = CFM_COLOR;
			chars.crTextColor = (COLORREF)wParam;
			SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
			SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
			SetFocus(hWnd);
			break;
		}
		case WM_CHANGECBCHAIN:
			if ((HWND)wParam == hClip)
				hClip = (HWND)lParam;
			else
				SendMessage(hClip, WM_CHANGECBCHAIN, wParam, lParam);
			break;
		case WM_DRAWCLIPBOARD:
			if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_CANPASTE, 0, 0)) 
				SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(TRUE,0));
			else
				SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(FALSE,0));
			SendMessage(hClip, WM_DRAWCLIPBOARD, wParam, lParam);
			break;
		case WM_CLOSE: {
			char text[256];
			int ans;
			if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_GETMODIFY, 0, 0) != 0) {
				sprintf(text, "The text in the %s file has changed.\r\n\r\nDo you want to save the changes?", file ? file : "new");
				ans = MessageBox(hDlg, text, "UnrealIRCd", MB_YESNOCANCEL|MB_ICONWARNING);
				if (ans == IDNO)
					EndDialog(hDlg, TRUE);
				if (ans == IDCANCEL)
					return TRUE;
				if (ans == IDYES) {
					SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDM_SAVE,0), 0);
					EndDialog(hDlg, TRUE);
				}
			}
			else
				EndDialog(hDlg, TRUE);
		}
		case WM_DESTROY:
			ChangeClipboardChain(hDlg, hClip);
			break;
		}

	return (FALSE);
}

LRESULT CALLBACK StatusDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_INITDIALOG: {
				hwTreeView = GetDlgItem(hDlg, IDC_TREE);
				win_map(&me, hwTreeView, 0);
				SetDlgItemInt(hDlg, IDC_CLIENTS, IRCstats.clients, FALSE);
				SetDlgItemInt(hDlg, IDC_SERVERS, IRCstats.servers, FALSE);
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
				SetTimer(hDlg, 1, 5000, NULL);
				return (TRUE);
			}
		case WM_CLOSE:
			EndDialog(hDlg, TRUE);
			break;
		case WM_TIMER:
				TreeView_DeleteAllItems(hwTreeView);
				win_map(&me, hwTreeView, 1);
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
				SetTimer(hDlg, 1, 5000, NULL);
				return (TRUE);
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(hDlg, TRUE);
			break;

		}
	return (FALSE);
}

LRESULT CALLBACK ColorDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HBRUSH hBrushWhite, hBrushBlack, hBrushDarkBlue, hBrushDarkGreen, hBrushRed,
		hBrushDarkRed, hBrushPurple, hBrushOrange, hBrushYellow, hBrushGreen, hBrushVDarkGreen,
		hBrushLightBlue, hBrushBlue, hBrushPink, hBrushDarkGray, hBrushGray;
	static UINT ResultMsg=0;
	switch (message) {
	case WM_INITDIALOG:
		hBrushWhite = CreateSolidBrush(RGB(255,255,255));
		hBrushBlack = CreateSolidBrush(RGB(0,0,0));
		hBrushDarkBlue = CreateSolidBrush(RGB(0,0,127));
		hBrushDarkGreen = CreateSolidBrush(RGB(0,147,0));
		hBrushRed = CreateSolidBrush(RGB(255,0,0));
		hBrushDarkRed = CreateSolidBrush(RGB(127,0,0));
		hBrushPurple = CreateSolidBrush(RGB(156,0,156));
		hBrushOrange = CreateSolidBrush(RGB(252,127,0));
		hBrushYellow = CreateSolidBrush(RGB(255,255,0));
		hBrushGreen = CreateSolidBrush(RGB(0,252,0));
		hBrushVDarkGreen = CreateSolidBrush(RGB(0,147,147));
		hBrushLightBlue = CreateSolidBrush(RGB(0,255,255));
		hBrushBlue = CreateSolidBrush(RGB(0,0,252));
		hBrushPink = CreateSolidBrush(RGB(255,0,255));
		hBrushDarkGray = CreateSolidBrush(RGB(127,127,127));
		hBrushGray = CreateSolidBrush(RGB(210,210,210));
		ResultMsg = (UINT)lParam;
		SetFocus(NULL);
		return (TRUE);
	case WM_DRAWITEM: {
		LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
		if (wParam == IDC_WHITE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushWhite);
			}
		if (wParam == IDC_BLACK) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushBlack);
		}
		if (wParam == IDC_DARKBLUE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkBlue);
		}
		if (wParam == IDC_DARKGREEN) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkGreen);
		}
		if (wParam == IDC_RED) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushRed);
		}
		if (wParam == IDC_DARKRED) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkRed);
		}
		if (wParam == IDC_PURPLE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushPurple);
		}
		if (wParam == IDC_ORANGE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushOrange);
		}
		if (wParam == IDC_YELLOW) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushYellow);
		}
		if (wParam == IDC_GREEN) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushGreen);
		}
		if (wParam == IDC_VDARKGREEN) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushVDarkGreen);
		}
		if (wParam == IDC_LIGHTBLUE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushLightBlue);
		}
		if (wParam == IDC_BLUE) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushBlue);
		}
		if (wParam == IDC_PINK) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushPink);
		}
		if (wParam == IDC_DARKGRAY) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkGray);
		}
		if (wParam == IDC_GRAY) {
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrushGray);
		}
		DrawEdge(lpdis->hDC, &lpdis->rcItem, EDGE_SUNKEN, BF_RECT);
		return TRUE;
		}
	case WM_COMMAND: {
		COLORREF clrref;
		if (LOWORD(wParam) == IDC_WHITE) 
			clrref = RGB(255,255,255);
		else if (LOWORD(wParam) == IDC_BLACK)
			clrref = RGB(0,0,0);
		else if (LOWORD(wParam) == IDC_DARKBLUE)
			clrref = RGB(0,0,127);
		else if (LOWORD(wParam) == IDC_DARKGREEN)
			clrref = RGB(0,147,0);
		else if (LOWORD(wParam) == IDC_RED)
			clrref = RGB(255,0,0);
		else if (LOWORD(wParam) == IDC_DARKRED)
			clrref = RGB(127,0,0);
		else if (LOWORD(wParam) == IDC_PURPLE)
			clrref = RGB(156,0,156);
		else if (LOWORD(wParam) == IDC_ORANGE)
			clrref = RGB(252,127,0);
		else if (LOWORD(wParam) == IDC_YELLOW)
			clrref = RGB(255,255,0);
		else if (LOWORD(wParam) == IDC_GREEN)
			clrref = RGB(0,252,0);
		else if (LOWORD(wParam) == IDC_VDARKGREEN)
			clrref = RGB(0,147,147);
		else if (LOWORD(wParam) == IDC_LIGHTBLUE)
			clrref = RGB(0,255,255);
		else if (LOWORD(wParam) == IDC_BLUE)
			clrref = RGB(0,0,252);
		else if (LOWORD(wParam) == IDC_PINK)
			clrref = RGB(255,0,255);
		else if (LOWORD(wParam) == IDC_DARKGRAY)
			clrref = RGB(127,127,127);
		else if (LOWORD(wParam) == IDC_GRAY)
			clrref = RGB(210,210,210);
		SendMessage(GetParent(hDlg), ResultMsg, (WPARAM)clrref, (LPARAM)hDlg);
	}

		break;
	case WM_CLOSE:
		EndDialog(hDlg, TRUE);
	case WM_DESTROY:
		DeleteObject(hBrushWhite);
		DeleteObject(hBrushBlack);
		DeleteObject(hBrushDarkBlue);
		DeleteObject(hBrushDarkGreen);
		DeleteObject(hBrushRed);
		DeleteObject(hBrushDarkRed);
		DeleteObject(hBrushPurple);
		DeleteObject(hBrushOrange);
		DeleteObject(hBrushYellow);
		DeleteObject(hBrushGreen);
		DeleteObject(hBrushVDarkGreen);
		DeleteObject(hBrushLightBlue);
		DeleteObject(hBrushBlue);
		DeleteObject(hBrushPink);
		DeleteObject(hBrushDarkGray);
		DeleteObject(hBrushGray);
		break;
	}

	return (FALSE);
}


/* find how big a buffer expansion we need for RTF transformation */
int CountRTFSize(char *buffer) {
	int size = 0;
	short bold = 0, uline = 0, reverse = 0;
	char *buf = buffer;

	for (; *buf; buf++, size++) {
		if (*buf == '{' || *buf == '}' || *buf == '\\') {
			size++;
			continue;
		}
		if (*buf == '\r') {
			buf++;
			if (*buf == '\n')
				size += 5;
		}
		if (*buf == '\2') {
			if (bold) 
				size += 3;
			else  
				size += 2;
			bold = ~bold;
		}
		if (*buf == '\3') {
			char color[3];
			int number;
			size += 3;
			if (!isdigit(*(buf+1)))
				size += 12;
			if (isdigit(*(buf+1))) {
				color[0] = *(++buf);
				color[1] = 0;
				if (isdigit(*(buf+1)))
					color[1] = *(++buf);
				color[2] = 0;
				number = atoi(color);
				if (number > 15 && number != 99)
					number %= 16;
				if (number > 9)
					size += 2;
				else
					size++;
			}
		}

		if (*buf == '\37') {
			if (uline)
				size += 7;
			else
				size += 3;
			uline = ~uline;
		}
		if (*buf == '\17') {
			if (uline)
				size += 7;
			if (bold)
				size += 3;
			uline = bold = 0;
		}
	}
	return (size+494);
}

void IRCToRTF(char *buffer, char *string) {
	char *tmp = buffer;
	int i = 0;
	short bold = 0, uline = 0;
	sprintf(string, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fprq1\\"
		"fcharset0 Fixedsys;}}\r\n"
		MIRC_COLORS
		"\\viewkind4\\uc1\\pard\\lang1033\\f0\\fs20");
	i = 487;
	for (tmp; *tmp; tmp++, i++) {
		if (*tmp == '{') {
			strcat(string, "\\{");
			i++;
			continue;
		}
		if (*tmp == '}') {
			strcat(string, "\\}");
			i++;
			continue;
		}
		if (*tmp == '\\') {
			strcat(string, "\\\\");
			i++;
			continue;
		}
		if (*tmp == '\r') {
			tmp++;
			if (*tmp == '\n') {
				strcat(string, "\\par\r\n");
				i += 5;
				continue;
			}
		}
		if (*tmp == '\2') {
			if (bold) {
				strcat(string, "\\b0 ");
				i += 3;
			}
			else {
				strcat(string, "\\b ");
				i += 2;
			}
			bold = ~bold;
			continue;
		}
		if (*tmp == '\3') {
			char color[3];
			int number;
			strcat(string, "\\cf");
			i += 3;
			if (!isdigit(*(tmp+1))) {
				strcat(string, "0\\highlight0");
				i += 12;
			}
			else {
				color[0] = *(++tmp);
				color[1] = 0;
				if (isdigit(*(tmp+1)))
					color[1] = *(++tmp);
				color[2] = 0;
				number = atoi(color);
				if (number == 99 || number == 1) {
					strcat(string, "16");
					i += 2;
				}
				else if (number == 0) {
					string[i] = '1';
					i++;
				}
				else {
					number %= 16;
					_itoa(number, color, 10);
					strcat(string, color);
					i += strlen(color);
				}
			}
			string[i] = ' ';
			continue;
		}
		if (*tmp == '\37') {
			if (uline) {
				strcat(string, "\\ulnone ");
				i += 7;
			}
			else {
				strcat(string, "\\ul ");
				i += 3;
			}
			uline = ~uline;
			continue;
		}
		if (*tmp == '\17') {
			if (uline) {
				strcat(string, "\\ulnone ");
				i += 7;
			}
			if (bold) {
				strcat(string, "\\b0 ");
				i += 3;
			}
			uline = bold = 0;
		}
		string[i] = *tmp;
	}
	strcat(string, "}");
	return;
}

/* This was made by DrBin but I cleaned it up a bunch to make it work better */

HTREEITEM AddItemToTree(HWND hWnd, LPSTR lpszItem, int nLevel, short remap)
{
    TVITEM tvi; 
    TVINSERTSTRUCT tvins; 
    static HTREEITEM hPrev = (HTREEITEM)TVI_FIRST; 
	static HTREEITEM hPrevLev[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    HTREEITEM hti; 
	if (remap) {
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
void win_map(aClient *server, HWND hwTreeView, short remap)
{
        aClient *acptr;
		Link *lp;
		AddItemToTree(hwTreeView,server->name,server->hopcount+1, remap);
		for (lp = Servers; lp; lp = lp->next)
        {
                acptr = lp->value.cptr;
                if (acptr->srvptr != server)
                        continue;
                win_map(acptr, hwTreeView, 0);
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
