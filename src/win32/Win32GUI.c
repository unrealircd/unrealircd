/************************************************************************
 *   IRC - Internet Relay Chat, Win32GUI.c
 *   Copyright (C) 2000-2003 David Flynn (DrBin) & Dominick Meglio (codemastr)
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

#define WIN32_VERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4
#include "resource.h"
#include "version.h"
#include "setup.h"
#ifdef INET6
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
#include <windowsx.h>
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

#define MIRC_COLORS "{\\colortbl;\\red255\\green255\\blue255;\\red0\\green0\\blue127;\\red0\\green147\\blue0;\\red255\\green0\\blue0;\\red127\\green0\\blue0;\\red156\\green0\\blue156;\\red252\\green127\\blue0;\\red255\\green255\\blue0;\\red0\\green252\\blue0;\\red0\\green147\\blue147;\\red0\\green255\\blue255;\\red0\\green0\\blue252;\\red255\\green0\\blue255;\\red127\\green127\\blue127;\\red210\\green210\\blue210;\\red0\\green0\\blue0;}"

/* Lazy macro */
#define ShowDialog(handle, inst, template, parent, proc) {\
	if (!IsWindow(handle)) { \
		handle = CreateDialog(inst, template, parent, (DLGPROC)proc); ShowWindow(handle, SW_SHOW); \
	}\
	else\
		SetForegroundWindow(handle);\
}
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
LRESULT CALLBACK FromVarDLG(HWND, UINT, WPARAM, LPARAM, unsigned char *, unsigned char **);
LRESULT CALLBACK FromFileReadDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FromFileDLG(HWND, UINT, WPARAM, LPARAM);

typedef struct {
	int *size;
	unsigned char **buffer;
} StreamIO;

extern  void      SocketLoop(void *dummy);
int CountRTFSize(unsigned char *);
void IRCToRTF(unsigned char *, unsigned char *);
HINSTANCE hInst;
NOTIFYICONDATA SysTray;
void CleanUp(void);
HTREEITEM AddItemToTree(HWND, LPSTR, int, short);
void win_map(aClient *, HWND, short);
extern Link *Servers;
extern ircstats IRCstats;
unsigned char *errors = NULL, *RTFBuf = NULL;
extern aMotd *botmotd, *opermotd, *motd, *rules;
extern VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
extern BOOL IsService;
void CleanUp(void)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
void CleanUpSegv(int sig)
{
	Shell_NotifyIcon(NIM_DELETE ,&SysTray);
}
HWND hStatusWnd;
HWND hwIRCDWnd=NULL;
HWND hwTreeView;
HWND hWndMod;
HANDLE hMainThread = 0;
UINT WM_TASKBARCREATED, WM_FINDMSGSTRING;
FARPROC lpfnOldWndProc;
HMENU hContext;
OSVERSIONINFO VerInfo;
char OSName[256];
HWND hFind;
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
	unsigned char string[500];

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
DWORD CALLBACK SplitIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) {
	StreamIO *stream = (StreamIO*)dwCookie;
	if (*stream->size == 0)
	{
		pcb = 0;
		*stream->buffer = 0;
	}
	else if (cb <= *stream->size) {
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
	unsigned char *buf2;
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

#define iseol(x) ((x) == '\r' || (x) == '\n')

typedef struct colorlist {
	struct colorlist *next;
	unsigned char *color;
} IRCColor;

IRCColor *TextColors = NULL;
IRCColor *BgColors = NULL;

void ColorPush(unsigned char *color, IRCColor **stack)
{
	IRCColor *t = MyMallocEx(sizeof(IRCColor));
	t->color = strdup(color);
	t->next = *stack;
	(*stack) = t;
}

void ColorPop(IRCColor **stack)
{
	IRCColor *p = *stack;
	if (!(*stack))
		return;
	MyFree(p->color);
	
	*stack = p->next;
	MyFree(p);
}

void ColorEmpty(IRCColor **stack)
{
	IRCColor *t, *next;
	for (t = *stack; t; t = next)
	{
		next = t->next;
		MyFree(t->color);
		MyFree(t);
	}
}

DWORD CALLBACK RTFToIRC(int fd, unsigned char *pbBuff, long cb) 
{
	unsigned char *buffer = malloc(cb*2);
	int colors[17], bold = 0, uline = 0, incolor = 0, inbg = 0;
	int lastwascf = 0, lastwascf0 = 0;
	int i = 0;
	TextColors = BgColors = NULL;
	bzero(buffer, cb);

	for (; *pbBuff; pbBuff++)
	{
		if (iseol(*pbBuff) || *pbBuff == '{' || *pbBuff == '}')
			continue;
		else if (*pbBuff == '\\')
		{
			/* RTF control sequence */
			pbBuff++;
			if (*pbBuff == '\\' || *pbBuff == '{' || *pbBuff == '}')
				buffer[i++] = *pbBuff;
			else if (*pbBuff == '\'')
			{
				/* Extended ASCII character */
				unsigned char ltr, ultr[3];
				ultr[0] = *(++pbBuff);
				ultr[1] = *(++pbBuff);
				ultr[2] = 0;
				ltr = strtoul(ultr,NULL,16);
				buffer[i++] = ltr;
			}
			else
			{
				int j;
				char cmd[128];
				/* Capture the control sequence */
				for (j = 0; *pbBuff && *pbBuff != '\\' && !isspace(*pbBuff) &&
					!iseol(*pbBuff); pbBuff++)
				{
					cmd[j++] = *pbBuff;
				}
				if (*pbBuff != ' ')
					pbBuff--;
				cmd[j] = 0;
				if (!strcmp(cmd, "fonttbl{"))
				{
					/* Eat the parameter */
					while (*pbBuff && *pbBuff != '}')
						pbBuff++;
					lastwascf = lastwascf0 = 0;
				}
				if (!strcmp(cmd, "colortbl"))
				{
					char color[128];
					int k = 0, m = 1;
					/* Capture the color table */
					while (*pbBuff && !isalnum(*pbBuff))
						pbBuff++;
					for (; *pbBuff && *pbBuff != '}'; pbBuff++)
					{
						if (*pbBuff == ';')
						{
							color[k]=0;
							if (!strcmp(color, "\\red255\\green255\\blue255"))
								colors[m++] = 0;
							else if (!strcmp(color, "\\red0\\green0\\blue0"))
								colors[m++] = 1;
							else if (!strcmp(color, "\\red0\\green0\\blue127"))
								colors[m++] = 2;
							else if (!strcmp(color, "\\red0\\green147\\blue0"))
								colors[m++] = 3;
							else if (!strcmp(color, "\\red255\\green0\\blue0"))
								colors[m++] = 4;
							else if (!strcmp(color, "\\red127\\green0\\blue0"))
								colors[m++] = 5;
							else if (!strcmp(color, "\\red156\\green0\\blue156"))
								colors[m++] = 6;
							else if (!strcmp(color, "\\red252\\green127\\blue0"))
								colors[m++] = 7;
							else if (!strcmp(color, "\\red255\\green255\\blue0"))
								colors[m++] = 8;
							else if (!strcmp(color, "\\red0\\green252\\blue0"))
								colors[m++] = 9;
							else if (!strcmp(color, "\\red0\\green147\\blue147"))
								colors[m++] = 10;
							else if (!strcmp(color, "\\red0\\green255\\blue255"))
								colors[m++] = 11;
							else if (!strcmp(color, "\\red0\\green0\\blue252"))
								colors[m++] = 12;
							else if (!strcmp(color, "\\red255\\green0\\blue255"))
								colors[m++] = 13;
							else if (!strcmp(color, "\\red127\\green127\\blue127"))
								colors[m++] = 14;
							else if (!strcmp(color, "\\red210\\green210\\blue210")) 
								colors[m++] = 15;
							k=0;
						}
						else
							color[k++] = *pbBuff;
					}
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "tab"))
				{
					buffer[i++] = '\t';
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "par"))
				{
					if (bold || uline || incolor || inbg)
						buffer[i++] = '\17';
					buffer[i++] = '\r';
					buffer[i++] = '\n';
					if (!*(pbBuff+3) || *(pbBuff+3) != '}')
					{
					if (bold)
						buffer[i++] = '\2';
					if (uline)
						buffer[i++] = '\37';
					if (incolor)
					{
						buffer[i++] = '\3';
						strcat(buffer, TextColors->color);
						i += strlen(TextColors->color);
						if (inbg)
						{
							buffer[i++] = ',';
							strcat(buffer, BgColors->color);
							i += strlen(BgColors->color);
						}
					}
					else if (inbg) {
						buffer[i++] = '\3';
						buffer[i++] = '0';
						buffer[i++] = '1';
						buffer[i++] = ',';
						strcat(buffer, BgColors->color);
						i += strlen(BgColors->color);
					}
}
				}
				else if (!strcmp(cmd, "b"))
				{
					bold = 1;
					buffer[i++] = '\2';
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "b0"))
				{
					bold = 0;
					buffer[i++] = '\2';
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "ul"))
				{
					uline = 1;
					buffer[i++] = '\37';
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "ulnone"))
				{
					uline = 0;
					buffer[i++] = '\37';
					lastwascf = lastwascf0 = 0;
				}
				else if (!strcmp(cmd, "cf0"))
				{
					lastwascf0 = 1;
					lastwascf = 0;
				}
				else if (!strcmp(cmd, "highlight0"))
				{
					inbg = 0;
					ColorPop(&BgColors);
					buffer[i++] = '\3';
					if (lastwascf0)
					{
						incolor = 0;
						ColorPop(&TextColors);
						lastwascf0 = 0;
					}
					else if (incolor)
					{
						strcat(buffer, TextColors->color);
						i += strlen(TextColors->color);
						buffer[i++] = ',';
						buffer[i++] = '0';
						buffer[i++] = '0';
					}
					lastwascf = lastwascf0 = 0;
				}
				else if (!strncmp(cmd, "cf", 2))
				{
					unsigned char number[3];
					int num;
					incolor = 1;
					strcpy(number, &cmd[2]);
					num = atoi(number);
					buffer[i++] = '\3';
					if (colors[num] < 10)
						sprintf(number, "0%d", colors[num]);
					else
						sprintf(number, "%d", colors[num]);
					ColorPush(number, &TextColors);
					strcat(buffer,number);
					i += strlen(number);
					lastwascf = 1;
					lastwascf0 = 0;
				}
				else if (!strncmp(cmd, "highlight", 9))
				{
					int num;
					unsigned char number[3];
					inbg = 1;
					num = atoi(&cmd[9]);
					if (colors[num] < 10)
						sprintf(number, "0%d", colors[num]);
					else
						sprintf(number, "%d", colors[num]);
					if (incolor && !lastwascf)
					{
						buffer[i++] = '\3';
						strcat(buffer, TextColors->color);
						i += strlen(TextColors->color);
					}
					else if (!incolor)
					{
						buffer[i++] = '\3';
						buffer[i++] = '0';
						buffer[i++] = '1';
					}
					buffer[i++] = ',';
					strcat(buffer, number);
					i += strlen(number);
					ColorPush(number, &BgColors);
					lastwascf = lastwascf0 = 0;
				}
				else
					lastwascf = lastwascf0 = 0;

				if (lastwascf0 && incolor)
				{
					incolor = 0;
					ColorPop(&TextColors);
					buffer[i++] = '\3';
				}
			}
		}
		else
		{
			lastwascf = lastwascf0 = 0;
			buffer[i++] = *pbBuff;
		}
				
	}
	write(fd, buffer, i);
	close(fd);
	ColorEmpty(&TextColors);
	ColorEmpty(&BgColors);
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	unsigned char *s;
	HWND hWnd;
	WSADATA WSAData;
	HICON hIcon;
	SERVICE_TABLE_ENTRY DispatchTable[] = {
		{ "UnrealIRCd", ServiceMain },
		{ 0, 0 }
	};
	DWORD need;
	
	VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&VerInfo);
	if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		SC_HANDLE hService, hSCManager = OpenSCManager(NULL, NULL, GENERIC_EXECUTE);
		if ((hService = OpenService(hSCManager, "UnrealIRCd", GENERIC_EXECUTE))) {
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
		}
	}
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
			else if (VerInfo.dwMinorVersion == 2)
				strcat(OSName, "Server 2003 ");
		}
		strcat(OSName, VerInfo.szCSDVersion);
	}
	if (OSName[strlen(OSName)-1] == ' ')
		OSName[strlen(OSName)-1] = 0;
	InitCommonControls();
	WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");
	WM_FINDMSGSTRING = RegisterWindowMessage(FINDMSGSTRING);
	atexit(CleanUp);
	if(!LoadLibrary("riched20.dll"))
		LoadLibrary("riched32.dll");
	InitDebug();

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
	ShowWindow(hWnd, SW_SHOW);
	hMainThread = (HANDLE)_beginthread(SocketLoop, 0, NULL);
	while (GetMessage(&msg, NULL, 0, 0))
    {
		if (!IsWindow(hStatusWnd) || !IsDialogMessage(hStatusWnd, &msg)) {
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
	aClient *paClient;
	unsigned char *msg;
	POINT p;

	if (message == WM_TASKBARCREATED){
		TaskBarCreated();
		return TRUE;
	}
	
	switch (message)
			{
			case WM_INITDIALOG: {
				ShowWindow(hDlg, SW_HIDE);
				hCursor = LoadCursor(hInst, MAKEINTRESOURCE(CUR_HAND));
				hContext = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_CONTEXT)),0);
				/* Rehash popup menu */
				hRehash = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_REHASH)),0);
				/* About popup menu */
				hAbout = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_ABOUT)),0);
				/* Systray popup menu set the items to point to the other menus*/
				hTray = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(MENU_SYSTRAY)),0);
				ModifyMenu(hTray, IDM_REHASH, MF_BYCOMMAND|MF_POPUP|MF_STRING, (UINT)hRehash, "&Rehash");
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
				if (MessageBox(hDlg, "Close UnrealIRCd?", "Are you sure?", MB_YESNO|MB_ICONQUESTION) == IDNO)
					return 0;
				else {
					DestroyWindow(hDlg);
					exit(0);
				}
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
					case WM_RBUTTONUP: {
						unsigned long i = 60000;
						GetCursorPos(&p);
						DestroyMenu(hConfig);
						hConfig = CreatePopupMenu();
						DestroyMenu(hLogs);
						hLogs = CreatePopupMenu();
						AppendMenu(hConfig, MF_STRING, IDM_CONF, CPATH);
						if (conf_log) {
							ConfigItem_log *logs;
							AppendMenu(hConfig, MF_POPUP|MF_STRING, (UINT)hLogs, "Logs");
							for (logs = conf_log; logs; logs = (ConfigItem_log *)logs->next) {
								AppendMenu(hLogs, MF_STRING, i++, logs->file);
							}
						}
						AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
						if (conf_include) {
							ConfigItem_include *inc;
							for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next) {
								AppendMenu(hConfig, MF_STRING, i++, inc->file);
							}
							AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
						}

						AppendMenu(hConfig, MF_STRING, IDM_MOTD, MPATH);
						AppendMenu(hConfig, MF_STRING, IDM_SMOTD, SMPATH);
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
								if (tlds->smotd_file)
									AppendMenu(hConfig, MF_STRING, i++, tlds->smotd_file);
							}
						}
						AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
						AppendMenu(hConfig, MF_STRING, IDM_NEW, "New File");
						ModifyMenu(hTray, IDM_CONFIG, MF_BYCOMMAND|MF_POPUP|MF_STRING, (UINT)hConfig, "&Config");
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
				ShowDialog(hStatusWnd, hInst, "Status", hDlg, StatusDLG);
				return 0;
			 }
			 else if ((p.x >= 140) && (p.x <= 186) && (p.y >= 178) && (p.y <= 190))  {
				unsigned long i = 60000;
				ClientToScreen(hDlg,&p);
				DestroyMenu(hConfig);
				hConfig = CreatePopupMenu();
				DestroyMenu(hLogs);
				hLogs = CreatePopupMenu();

				AppendMenu(hConfig, MF_STRING, IDM_CONF, CPATH);
				if (conf_log) {
					ConfigItem_log *logs;
					AppendMenu(hConfig, MF_POPUP|MF_STRING, (UINT)hLogs, "Logs");
					for (logs = conf_log; logs; logs = (ConfigItem_log *)logs->next) {
						AppendMenu(hLogs, MF_STRING, i++, logs->file);
					}
				}
				AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);

				if (conf_include) {
					ConfigItem_include *inc;
					for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next) {
						AppendMenu(hConfig, MF_STRING, i++, inc->file);
					}
					AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
				}

				AppendMenu(hConfig, MF_STRING, IDM_MOTD, MPATH);
				AppendMenu(hConfig, MF_STRING, IDM_SMOTD, SMPATH);
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
						if (tlds->smotd_file)
							AppendMenu(hConfig, MF_STRING, i++, tlds->smotd_file);
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
					unsigned char path[MAX_PATH];
					if (GetMenuString(hLogs, LOWORD(wParam), path, MAX_PATH, MF_BYCOMMAND))
						DialogBoxParam(hInst, "FromVar", hDlg,
(DLGPROC)FromFileReadDLG, (LPARAM)path);
					
					else {
						GetMenuString(hConfig,LOWORD(wParam), path, MAX_PATH, MF_BYCOMMAND);
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
								(LPARAM)path);
					}
					return FALSE;
				}

				switch(LOWORD(wParam)) {

					case IDM_STATUS:
						ShowDialog(hStatusWnd, hInst, "Status", hDlg, StatusDLG);
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
						rehash_motdrules();
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
					case IDM_SMOTD:
						DialogBoxParam(hInst, "FromFile", hDlg, (DLGPROC)FromFileDLG, 
							(LPARAM)SMPATH);
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

LRESULT CALLBACK FromVarDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam,
unsigned char *title, unsigned char **s) {
	HWND hWnd;
	switch (message) {
		case WM_INITDIALOG: {
			unsigned char	String[16384];
			int size;
			unsigned char *RTFString;
			StreamIO *stream = malloc(sizeof(StreamIO));
			EDITSTREAM edit;
			SetWindowText(hDlg, title);
			bzero(String, 16384);
			lpfnOldWndProc = (FARPROC)SetWindowLong(GetDlgItem(hDlg, IDC_TEXT),
GWL_WNDPROC, (DWORD)RESubClassFunc);
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

LRESULT CALLBACK FromFileReadDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	HWND hWnd;
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			unsigned char *buffer = '\0', *string = '\0';
			EDITSTREAM edit;
			StreamIO *stream = malloc(sizeof(StreamIO));
			unsigned char szText[256];
			struct stat sb;
			HWND hWnd = GetDlgItem(hDlg, IDC_TEXT), hTip;
			wsprintf(szText, "UnrealIRCd Viewer - %s", (unsigned char *)lParam);
			SetWindowText(hDlg, szText);
			lpfnOldWndProc = (FARPROC)SetWindowLong(hWnd, GWL_WNDPROC, (DWORD)RESubClassFunc);
			if ((fd = open((unsigned char *)lParam, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size); 
				buffer[len] = 0;
				len = CountRTFSize(buffer)+1;
				string = malloc(len);
				bzero(string,len);
				IRCToRTF(buffer,string);
				RTFBuf = string;
				len--;
				stream->size = &len;
				stream->buffer = &RTFBuf;
				edit.dwCookie = (UINT)stream;
				edit.pfnCallback = SplitIt;
				SendMessage(hWnd, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
				SendMessage(hWnd, EM_STREAMIN, (WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
				close(fd);
				RTFBuf = NULL;
				free(buffer);
				free(string);
				free(stream);
			}
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
			unsigned char text[500];
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
	TBBUTTON tbButtons[10] = {
		{ STD_FILENEW, IDM_NEW, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_FILESAVE, IDM_SAVE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0},
		{ STD_CUT, IDM_CUT, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_COPY, IDM_COPY, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_PASTE, IDM_PASTE, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0},
		{ STD_UNDO, IDM_UNDO, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_REDOW, IDM_REDO, 0, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0}
	};
		
	TBBUTTON tbAddButtons[7] = {
		{ 0, IDC_BOLD, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0L, 0},
		{ 1, IDC_UNDERLINE, TBSTATE_ENABLED, TBSTYLE_CHECK, {0}, 0L, 0},
		{ 2, IDC_COLOR, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 3, IDC_BGCOLOR, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0L, 0},
		{ 4, IDC_GOTO, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0},
		{ STD_FIND, IDC_FIND, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0}
	};
	hTool = CreateToolbarEx(hwndParent, WS_VISIBLE|WS_CHILD|TBSTYLE_FLAT|TBSTYLE_TOOLTIPS, 
				IDC_TOOLBAR, 0, HINST_COMMCTRL, IDB_STD_SMALL_COLOR,
				tbButtons, 10, 0,0,100,30, sizeof(TBBUTTON));
	tbBit.hInst = hInst;
	tbBit.nID = IDB_BITMAP1;
	newidx = SendMessage(hTool, TB_ADDBITMAP, (WPARAM)5, (LPARAM)&tbBit);
	tbAddButtons[0].iBitmap += newidx;
	tbAddButtons[1].iBitmap += newidx;
	tbAddButtons[2].iBitmap += newidx;
	tbAddButtons[3].iBitmap += newidx;
	tbAddButtons[5].iBitmap += newidx;
	SendMessage(hTool, TB_ADDBUTTONS, (WPARAM)7, (LPARAM)&tbAddButtons);
	return hTool;
}

HWND DrawStatusbar(HWND hwndParent, UINT iID) {
	HWND hStatus, hTip;
	TOOLINFO ti;
	RECT clrect;
	hStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE|SBT_TOOLTIPS, NULL, hwndParent, iID);
	hTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
 		WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP, 0, 0, 0, 0, hwndParent, NULL, hInst, NULL);
	GetClientRect(hStatus, &clrect);
	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = hStatus;
	ti.uId = 1;
	ti.hinst = hInst;
	ti.rect = clrect;
	ti.lpszText = "Go To";
	SendMessage(hTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
	return hStatus;
	
}


LRESULT CALLBACK GotoDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_COMMAND) {
		if (LOWORD(wParam) == IDCANCEL)
			EndDialog(hDlg, TRUE);
		if (LOWORD(wParam) == IDOK) {
			HWND hWnd = GetDlgItem(GetParent(hDlg),IDC_TEXT);
			int line = GetDlgItemInt(hDlg, IDC_GOTO, NULL, FALSE);
			int pos = SendMessage(hWnd, EM_LINEINDEX, (WPARAM)--line, 0);
			SendMessage(hWnd, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
			SendMessage(hWnd, EM_SCROLLCARET, 0, 0);
			EndDialog(hDlg, TRUE);
		}
	}
	return FALSE;
}

LRESULT CALLBACK FromFileDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	HWND hWnd;
	static FINDREPLACE find;
	static char findbuf[256];
	static unsigned char *file;
	static HWND hTool, hClip, hStatus;
	static RECT rOld;
	CHARFORMAT2 chars;

	if (message == WM_FINDMSGSTRING)
	{
		FINDREPLACE *fr = (FINDREPLACE *)lParam;

		if (fr->Flags & FR_FINDNEXT)
		{
			HWND hRich = GetDlgItem(hDlg, IDC_TEXT);
			DWORD flags=0;
			FINDTEXTEX ft;
			CHARRANGE chrg;

			if (fr->Flags & FR_DOWN)
				flags |= FR_DOWN;
			if (fr->Flags & FR_MATCHCASE)
				flags |= FR_MATCHCASE;
			if (fr->Flags & FR_WHOLEWORD)
				flags |= FR_WHOLEWORD;
			ft.lpstrText = fr->lpstrFindWhat;
			SendMessage(hRich, EM_EXGETSEL, 0, (LPARAM)&chrg);
			if (flags & FR_DOWN)
			{
				ft.chrg.cpMin = chrg.cpMax;
				ft.chrg.cpMax = -1;
			}
			else
			{
				ft.chrg.cpMin = chrg.cpMin;
				ft.chrg.cpMax = -1;
			}
			if (SendMessage(hRich, EM_FINDTEXTEX, flags, (LPARAM)&ft) == -1)
			{
				MessageBox(NULL, "Unreal has finished searching the document",
					"Find", MB_ICONINFORMATION|MB_OK);
			}
			else
			{
				SendMessage(hRich, EM_EXSETSEL, 0, (LPARAM)&(ft.chrgText));
				SendMessage(hRich, EM_SCROLLCARET, 0, 0);
				SetFocus(hRich);
			}
		}
		return TRUE;
	}
	switch (message) {
		case WM_INITDIALOG: {
			int fd,len;
			unsigned char *buffer = '\0', *string = '\0';
			EDITSTREAM edit;
			StreamIO *stream = malloc(sizeof(StreamIO));
			unsigned char szText[256];
			struct stat sb;
			HWND hWnd = GetDlgItem(hDlg, IDC_TEXT), hTip;
			file = (unsigned char *)lParam;
			if (file)
				wsprintf(szText, "UnrealIRCd Editor - %s", file);
			else 
				strcpy(szText, "UnrealIRCd Editor - New File");
			SetWindowText(hDlg, szText);
			lpfnOldWndProc = (FARPROC)SetWindowLong(hWnd, GWL_WNDPROC, (DWORD)RESubClassFunc);
			hTool = DrawToolbar(hDlg, IDC_TOOLBAR);
			hStatus = DrawStatusbar(hDlg, IDC_STATUS);
			SendMessage(hWnd, EM_SETEVENTMASK, 0, (LPARAM)ENM_SELCHANGE);
			chars.cbSize = sizeof(CHARFORMAT2);
			chars.dwMask = CFM_FACE;
			strcpy(chars.szFaceName,"Fixedsys");
			SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&chars);
			if ((fd = open(file, _O_RDONLY|_O_BINARY)) != -1) {
				fstat(fd,&sb);
				/* Only allocate the amount we need */
				buffer = malloc(sb.st_size+1);
				buffer[0] = 0;
				len = read(fd, buffer, sb.st_size);
				buffer[len] = 0;
				len = CountRTFSize(buffer)+1;
				string = malloc(len);
				bzero(string,len);
				IRCToRTF(buffer,string);
				RTFBuf = string;
				len--;
				stream->size = &len;
				stream->buffer = &RTFBuf;
				edit.dwCookie = (UINT)stream;
				edit.pfnCallback = SplitIt;
				SendMessage(hWnd, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
				SendMessage(hWnd, EM_STREAMIN, (WPARAM)SF_RTF|SFF_PLAINRTF, (LPARAM)&edit);
				SendMessage(hWnd, EM_SETMODIFY, (WPARAM)FALSE, 0);
				SendMessage(hWnd, EM_EMPTYUNDOBUFFER, 0, 0);
				close(fd);
				RTFBuf = NULL;
				free(buffer);
				free(string);
				free(stream);
				hClip = SetClipboardViewer(hDlg);
				if (SendMessage(hWnd, EM_CANPASTE, 0, 0)) 
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_PASTE, (LPARAM)MAKELONG(FALSE,0));
				SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_UNDO, (LPARAM)MAKELONG(FALSE,0));
				wsprintf(szText, "Line: 1");
				SetWindowText(hStatus, szText);
			}
			return (TRUE);
			}
		case WM_WINDOWPOSCHANGING:
		{
			GetClientRect(hDlg, &rOld);
			return FALSE;
		}
		case WM_SIZE:
		{
			DWORD new_width, new_height;
			HWND hRich;
			RECT rOldRich;
			DWORD old_width, old_height;
			DWORD old_rich_width, old_rich_height;
			if (hDlg == hFind)
				return FALSE;
			new_width =  LOWORD(lParam);
			new_height = HIWORD(lParam);
			hRich  = GetDlgItem(hDlg, IDC_TEXT);
			SendMessage(hStatus, WM_SIZE, 0, 0);
			SendMessage(hTool, TB_AUTOSIZE, 0, 0);
			old_width = rOld.right-rOld.left;
			old_height = rOld.bottom-rOld.top;
			new_width = new_width - old_width;
			new_height = new_height - old_height;
			GetWindowRect(hRich, &rOldRich);
			old_rich_width = rOldRich.right-rOldRich.left;
			old_rich_height = rOldRich.bottom-rOldRich.top;
			SetWindowPos(hRich, NULL, 0, 0, old_rich_width+new_width, 
				old_rich_height+new_height,
				SWP_NOMOVE|SWP_NOREPOSITION|SWP_NOZORDER);
			bzero(&rOld, sizeof(RECT));
			return TRUE;
		}
		case WM_NOTIFY:
			switch (((NMHDR *)lParam)->code) {
				case EN_SELCHANGE: {
				HWND hWnd = GetDlgItem(hDlg, IDC_TEXT);
				DWORD start, end, currline;
				static DWORD prevline = 0;
				unsigned char buffer[512];
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
				if (SendMessage(hWnd, EM_CANUNDO, 0, 0)) 
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_UNDO, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_UNDO, (LPARAM)MAKELONG(FALSE,0));
				if (SendMessage(hWnd, EM_CANREDO, 0, 0)) 
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_REDO, (LPARAM)MAKELONG(TRUE,0));
				else
					SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_REDO, (LPARAM)MAKELONG(FALSE,0));
				currline = SendMessage(hWnd, EM_LINEFROMCHAR, (WPARAM)-1, 0);
				currline++;
				if (currline != prevline) {
					wsprintf(buffer, "Line: %d", currline);
					SetWindowText(hStatus, buffer);
					prevline = currline;
				}
				}
				return (TRUE);
			
			case TTN_GETDISPINFO: {
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
					case IDM_UNDO:
						strcpy(lpttt->szText, "Undo");
						break;
					case IDM_REDO:
						strcpy(lpttt->szText, "Redo");
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
					case IDC_BGCOLOR:
						strcpy(lpttt->szText, "Background Color");
						break;
					case IDC_GOTO:
						strcpy(lpttt->szText, "Goto");
						break;
					case IDC_FIND:
						strcpy(lpttt->szText, "Find");
						break;
				}
				return (TRUE);
			}
			case NM_DBLCLK:
				DialogBox(hInst, "GOTO", hDlg, (DLGPROC)GotoDLG);
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
			{
				DialogBoxParam(hInst, "Color", hDlg, (DLGPROC)ColorDLG, (LPARAM)WM_USER+10);
				return 0;
			}
			if (LOWORD(wParam) == IDC_BGCOLOR)
			{
				DialogBoxParam(hInst, "Color", hDlg, (DLGPROC)ColorDLG, (LPARAM)WM_USER+11);
				return 0;
			}
			if (LOWORD(wParam) == IDC_GOTO)
			{
				DialogBox(hInst, "GOTO", hDlg, (DLGPROC)GotoDLG);
				return 0;
			}
			if (LOWORD(wParam) == IDC_FIND)
			{
				static FINDREPLACE fr;
				bzero(&fr, sizeof(FINDREPLACE));
				fr.lStructSize = sizeof(FINDREPLACE);
				fr.hwndOwner = hDlg;
				fr.lpstrFindWhat = findbuf;
				fr.wFindWhatLen = 255;
				hFind = FindText(&fr);
				return 0;
			}
				
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
		if (LOWORD(wParam) == IDM_REDO) {
			SendMessage(hWnd, EM_REDO, 0, 0);
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
				unsigned char path[MAX_PATH];
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
			unsigned char text[1024];
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
				unsigned char szText[256];
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
		case WM_USER+11: {
			HWND hWnd = GetDlgItem(hDlg, IDC_TEXT);
			EndDialog((HWND)lParam, TRUE);
			chars.cbSize = sizeof(CHARFORMAT2);
			chars.dwMask = CFM_BACKCOLOR;
			chars.crBackColor = (COLORREF)wParam;
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
			unsigned char text[256];
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
			break;
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
			DestroyWindow(hDlg);
			return TRUE;
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
			if (LOWORD(wParam) == IDOK) {
				DestroyWindow(hDlg);
				return TRUE;
			}
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
int CountRTFSize(unsigned char *buffer) {
	int size = 0;
	char bold = 0, uline = 0, incolor = 0, inbg = 0, reverse = 0;
	char *buf = buffer;

	for (; *buf; buf++) 
	{
		if (*buf == '{' || *buf == '}' || *buf == '\\')
			size++;
		else if (*buf == '\r')
		{
			if (*(buf+1) && *(buf+1) == '\n')
			{
				buf++;
				if (bold)
					size += 3;
				if (uline)
					size += 7;
				if (incolor && !reverse)
					size += 4;
				if (inbg && !reverse)
					size += 11;
				if (reverse)
					size += 15;
				if (bold || uline || incolor || inbg || reverse)
					size++;
				bold = uline = incolor = inbg = reverse = 0;
				size +=6;
				continue;
			}
		}
		else if (*buf == '\2')
		{
			if (bold)
				size += 4;
			else
				size += 3;
			bold = !bold;
			continue;
		}
		else if (*buf == '\3' && reverse)
		{
			if (*(buf+1) && isdigit(*(buf+1)))
			{
				++buf;
				if (*(buf+1) && isdigit(*(buf+1)))
					++buf;
				if (*(buf+1) && *(buf+1) == ',')
				{
					if (*(buf+2) && isdigit(*(buf+2)))
					{
						buf+=2;
						if (*(buf+1) && isdigit(*(buf+1)))
							++buf;
					}
				}
			}
			continue;
		}
		else if (*buf == '\3' && !reverse)
		{
			size += 3;
			if (*(buf+1) && !isdigit(*(buf+1)))
			{
				incolor = 0;
				size++;
				if (inbg)
				{
					inbg = 0;
					size += 11;
				}
			}
			else if (*(buf+1))
			{
				unsigned char color[3];
				int number;
				color[0] = *(++buf);
				color[1] = 0;
				if (*(buf+1) && isdigit(*(buf+1)))
					color[1] = *(++buf);
				color[2] = 0;
				number = atoi(color);
				if (number == 99 || number == 1) 
					size += 2;
				else if (number == 0) 
					size++;
				else  {
					number %= 16;
					_itoa(number, color, 10);
					size += strlen(color);
				}
				color[2] = 0;
				number = atoi(color);
				if (*(buf+1) && *(buf+1) == ',')
				{
					if (*(buf+2) && isdigit(*(buf+2)))
					{
						size += 10;
						buf++;
						color[0] = *(++buf);
						color[1] = 0;
						if (*(buf+1) && isdigit(*(buf+1)))
							color[1] = *(++buf);
						color[2] = 0;
						number = atoi(color);
						if (number == 1)
							size += 2;
						else if (number == 0 || number == 99)
							size++;
						else
						{
							number %= 16;
							_itoa(number, color, 10);
							size += strlen(color);
						}
						inbg = 1;
					}
				}
				incolor = 1;
			}
			size++;
			continue;
		}
		else if (*buf == '\17')
		{
			if (bold)
				size += 3;
			if (uline)
				size += 7;
			if (incolor && !reverse)
				size += 4;
			if (inbg && !reverse)
				size += 11;
			if (reverse)
				size += 15;
			if (bold || uline || incolor || inbg || reverse)
				size++;
			bold = uline = incolor = inbg = reverse = 0;
			continue;
		}
		else if (*buf == '\26')
		{
			if (reverse)
				size += 16;
			else
				size += 17;
			reverse = !reverse;
			continue;
		}
		else if (*buf == '\37')
		{
			if (uline)
				size += 8;
			else
				size += 4;
			uline = !uline;
			continue;
		}
		size++;
	}			
	size+=strlen("{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fprq1\\"
		"fcharset0 Fixedsys;}}\r\n"
		MIRC_COLORS
		"\\viewkind4\\uc1\\pard\\lang1033\\f0\\fs20")+1;
	return (size);
}

void IRCToRTF(unsigned char *buffer, unsigned char *string) {
	unsigned char *tmp;
	int i = 0;
	short bold = 0, uline = 0, incolor = 0, inbg = 0, reverse = 0;
	sprintf(string, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fprq1\\"
		"fcharset0 Fixedsys;}}\r\n"
		MIRC_COLORS
		"\\viewkind4\\uc1\\pard\\lang1033\\f0\\fs20");
	i = strlen(string);
	for (tmp = buffer; *tmp; tmp++)
	{
		if (*tmp == '{')
		{
			strcat(string, "\\{");
			i+=2;
			continue;
		}
		else if (*tmp == '}')
		{
			strcat(string, "\\}");
			i+=2;
			continue;
		}
		else if (*tmp == '\\')
		{
			strcat(string, "\\\\");
			i+=2;
			continue;
		}
		else if (*tmp == '\r')
		{
			if (*(tmp+1) && *(tmp+1) == '\n')
			{
				tmp++;
				if (bold)
				{
					strcat(string, "\\b0 ");
					i+=3;
				}
				if (uline)
				{
					strcat(string, "\\ulnone");
					i+=7;
				}
				if (incolor && !reverse)
				{
					strcat(string, "\\cf0");
					i+=4;
				}
				if (inbg && !reverse)
				{
					strcat(string, "\\highlight0");
					i +=11;
				}
				if (reverse) {
					strcat(string, "\\cf0\\highlight0");
					i += 15;
				}
				if (bold || uline || incolor || inbg || reverse)
					string[i++] = ' ';
				bold = uline = incolor = inbg = reverse = 0;
				strcat(string, "\\par\r\n");
				i +=6;
			}
			else
				string[i++]='\r';
			continue;
		}
		else if (*tmp == '\2')
		{
			if (bold)
			{
				strcat(string, "\\b0 ");
				i+=4;
			}
			else
			{
				strcat(string, "\\b ");
				i+=3;
			}
			bold = !bold;
			continue;
		}
		else if (*tmp == '\3' && reverse)
		{
			if (*(tmp+1) && isdigit(*(tmp+1)))
			{
				++tmp;
				if (*(tmp+1) && isdigit(*(tmp+1)))
					++tmp;
				if (*(tmp+1) && *(tmp+1) == ',')
				{
					if (*(tmp+2) && isdigit(*(tmp+2)))
					{
						tmp+=2;
						if (*(tmp+1) && isdigit(*(tmp+1)))
							++tmp;
					}
				}
			}
			continue;
		}
		else if (*tmp == '\3' && !reverse)
		{
			strcat(string, "\\cf");
			i += 3;
			if (*(tmp+1) && !isdigit(*(tmp+1)))
			{
				incolor = 0;
				string[i++] = '0';
				if (inbg)
				{
					inbg = 0;
					strcat(string, "\\highlight0");
					i += 11;
				}
			}
			else if (*(tmp+1))
			{
				unsigned char color[3];
				int number;
				color[0] = *(++tmp);
				color[1] = 0;
				if (*(tmp+1) && isdigit(*(tmp+1)))
					color[1] = *(++tmp);
				color[2] = 0;
				number = atoi(color);
				if (number == 99 || number == 1)
				{
					strcat(string, "16"); 
					i += 2;
				}
				else if (number == 0) 
				{
					strcat(string, "1");
					i++;
				}
				else
				{
					number %= 16;
					_itoa(number, color, 10);
					strcat(string, color);
					i += strlen(color);
				}
				if (*(tmp+1) && *(tmp+1) == ',')
				{
					if (*(tmp+2) && isdigit(*(tmp+2)))
					{
						strcat(string, "\\highlight");
						i += 10;
						tmp++;
						color[0] = *(++tmp);
						color[1] = 0;
						if (*(tmp+1) && isdigit(*(tmp+1)))
							color[1] = *(++tmp);
						color[2] = 0;
						number = atoi(color);
						if (number == 1)
						{
							strcat(string, "16");
							i += 2;
						}
						else if (number == 0 || number == 99)
							string[i++] = '1';
						else
						{
							number %= 16;
							_itoa(number, color, 10);
							strcat(string,color);
							i += strlen(color);
						}
						inbg = 1;
					}
				}
				incolor=1;
			}
			string[i++] = ' ';
			continue;
		}
		else if (*tmp == '\17') {
			if (uline) {
				strcat(string, "\\ulnone");
				i += 7;
			}
			if (bold) {
				strcat(string, "\\b0");
				i += 3;
			}
			if (incolor && !reverse) {
				strcat(string, "\\cf0");
				i += 4;
			}
			if (inbg && !reverse)
			{
				strcat(string, "\\highlight0");
				i += 11;
			}
			if (reverse) {
				strcat(string, "\\cf0\\highlight0");
				i += 15;
			}
			if (uline || bold || incolor || inbg || reverse)
				string[i++] = ' ';
			uline = bold = incolor = inbg = reverse = 0;
			continue;
		}
		else if (*tmp == '\26')
		{
			if (reverse)
			{
				strcat(string, "\\cf0\\highlight0 ");
				i += 16;
			}
			else
			{
				strcat(string, "\\cf1\\highlight16 ");
				i += 17;
			}
			reverse = !reverse;
			continue;
		}

		else if (*tmp == '\37') {
			if (uline) {
				strcat(string, "\\ulnone ");
				i += 8;
			}
			else {
				strcat(string, "\\ul ");
				i += 4;
			}
			uline = !uline;
			continue;
		}
		string[i++] = *tmp;
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
void win_log(unsigned char *format, ...) {
        va_list ap;
        unsigned char buf[2048];
		unsigned char *buf2;
        va_start(ap, format);
        ircvsprintf(buf, format, ap);
	if (!IsService) {
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
	}
	else {
		FILE *fd = fopen("service.log", "a");
		fprintf(fd, "%s\n", buf);
		fclose(fd);
	}
        va_end(ap);
}

void win_error() {
	if (errors && !IsService)
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
