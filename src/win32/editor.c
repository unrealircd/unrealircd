/************************************************************************
 *   IRC - Internet Relay Chat, win32/editor.c
 *   Copyright (C) 2004 Dominick Meglio (codemastr)
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

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include "resource.h"
#include "setup.h"
#include "win32.h"
#include "sys.h"

LRESULT CALLBACK GotoDLG(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ColorDLG(HWND, UINT, WPARAM, LPARAM);

HWND hFind;

/* Draws the statusbar for the editor
 * Parameters:
 *  hInstance  - The instance to create the statusbar in
 *  hwndParent - The parent of the statusbar
 *  iId        - The message value used to send messages to the parent
 * Returns:
 *  The handle to the statusbar
 */
HWND DrawStatusbar(HINSTANCE hInstance, HWND hwndParent, UINT iId)
{
	HWND hStatus, hTip;
	TOOLINFO ti;
	RECT clrect;
	hStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE|SBT_TOOLTIPS, NULL, hwndParent, iId);
	hTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
 		WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP, 0, 0, 0, 0, hwndParent, NULL, hInstance, NULL);
	GetClientRect(hStatus, &clrect);
	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = hStatus;
	ti.uId = 1;
	ti.hinst = hInstance;
	ti.rect = clrect;
	ti.lpszText = "Go To";
	SendMessage(hTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
	return hStatus;
}

/* Draws the toolbar for the editor
 * Parameters:
 *  hInstance  - The instance to create the toolbar in
 *  hwndParent - The parent of the toolbar
 * Returns:
 *  The handle to the toolbar
 */
HWND DrawToolbar(HINSTANCE hInstance, HWND hwndParent) 
{
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
				tbButtons, 10, 0, 0, 100, 30, sizeof(TBBUTTON));
	tbBit.hInst = hInstance;
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

/* Dialog procedure for the color selection dialog
 * Parameters:
 *  hDlg    - The dialog handle
 *  message - The message received
 *  wParam  - The first message parameter
 *  lParam  - The second message parameter
 * Returns:
 *  TRUE if the message was processed, FALSE otherwise
 */
LRESULT CALLBACK ColorDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HBRUSH hBrushWhite, hBrushBlack, hBrushDarkBlue, hBrushDarkGreen, hBrushRed,
		hBrushDarkRed, hBrushPurple, hBrushOrange, hBrushYellow, hBrushGreen, hBrushVDarkGreen,
		hBrushLightBlue, hBrushBlue, hBrushPink, hBrushDarkGray, hBrushGray;
	static UINT ResultMsg = 0;

	switch (message) 
	{
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
			return TRUE;
		case WM_DRAWITEM: 
		{
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			if (wParam == IDC_WHITE) 
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushWhite);
			if (wParam == IDC_BLACK)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushBlack);
			if (wParam == IDC_DARKBLUE) 
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkBlue);
			if (wParam == IDC_DARKGREEN) 
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkGreen);
			if (wParam == IDC_RED)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushRed);
			if (wParam == IDC_DARKRED)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkRed);
			if (wParam == IDC_PURPLE)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushPurple);
			if (wParam == IDC_ORANGE)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushOrange);
			if (wParam == IDC_YELLOW)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushYellow);
			if (wParam == IDC_GREEN)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushGreen);
			if (wParam == IDC_VDARKGREEN)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushVDarkGreen);
			if (wParam == IDC_LIGHTBLUE)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushLightBlue);
			if (wParam == IDC_BLUE)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushBlue);
			if (wParam == IDC_PINK)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushPink);
			if (wParam == IDC_DARKGRAY)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushDarkGray);
			if (wParam == IDC_GRAY)
				FillRect(lpdis->hDC, &lpdis->rcItem, hBrushGray);
			DrawEdge(lpdis->hDC, &lpdis->rcItem, EDGE_SUNKEN, BF_RECT);
			return TRUE;
		}
		case WM_COMMAND: 
		{
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
			break;
		}
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

	return FALSE;
}

/* Dialog procedure for the goto dialog
 * Parameters:
 *  hDlg    - The dialog handle
 *  message - The message received
 *  wParam  - The first message parameter
 *  lParam  - The second message parameter
 * Returns:
 *  TRUE if the message was processed, FALSE otherwise
 */
LRESULT CALLBACK GotoDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	if (message == WM_COMMAND) 
	{
		if (LOWORD(wParam) == IDCANCEL)
			EndDialog(hDlg, TRUE);
		else if (LOWORD(wParam) == IDOK) 
		{
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

LRESULT CALLBACK FromFileDLG(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
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
				MessageBox(NULL, "Unreal has finished searching the document",
					"Find", MB_ICONINFORMATION|MB_OK);
			else
			{
				SendMessage(hRich, EM_EXSETSEL, 0, (LPARAM)&(ft.chrgText));
				SendMessage(hRich, EM_SCROLLCARET, 0, 0);
				SetFocus(hRich);
			}
		}
		return TRUE;
	}
	switch (message) 
	{
		case WM_INITDIALOG: 
		{
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
			hTool = DrawToolbar(hInst, hDlg);
			hStatus = DrawStatusbar(hInst, hDlg, IDC_STATUS);
			SendMessage(hWnd, EM_SETEVENTMASK, 0, (LPARAM)ENM_SELCHANGE);
			chars.cbSize = sizeof(CHARFORMAT2);
			chars.dwMask = CFM_FACE;
			strcpy(chars.szFaceName,"Fixedsys");
			SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&chars);
			if ((fd = open(file, _O_RDONLY|_O_BINARY)) != -1) 
			{
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
			return TRUE;
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
			switch (((NMHDR *)lParam)->code) 
			{
				case EN_SELCHANGE: 
				{
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
					if (start == end) 
					{
						SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_COPY, (LPARAM)MAKELONG(FALSE,0));
						SendMessage(hTool, TB_ENABLEBUTTON, (WPARAM)IDM_CUT, (LPARAM)MAKELONG(FALSE,0));
					}
					else 
					{
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
					if (currline != prevline) 
					{
						wsprintf(buffer, "Line: %d", currline);
						SetWindowText(hStatus, buffer);
						prevline = currline;
					}
				return TRUE;
			}
			case TTN_GETDISPINFO: 
			{
				LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;
				lpttt->hinst = NULL;
				switch (lpttt->hdr.idFrom) 
				{
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
				return TRUE;
			}
			case NM_DBLCLK:
				DialogBox(hInst, "GOTO", hDlg, (DLGPROC)GotoDLG);
				return (TRUE);
		}
				
				return (TRUE);
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_BOLD) 
			{
				hWnd = GetDlgItem(hDlg, IDC_TEXT);
				if (SendMessage(hTool, TB_ISBUTTONCHECKED, (WPARAM)IDC_BOLD, (LPARAM)0) != 0) 
				{
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_BOLD;
					chars.dwEffects = CFE_BOLD;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				else 
				{
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_BOLD;
					chars.dwEffects = 0;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				return TRUE;
			}
			else if (LOWORD(wParam) == IDC_UNDERLINE) 
			{
				hWnd = GetDlgItem(hDlg, IDC_TEXT);
				if (SendMessage(hTool, TB_ISBUTTONCHECKED, (WPARAM)IDC_UNDERLINE, (LPARAM)0) != 0) 
				{
					chars.cbSize = sizeof(CHARFORMAT2);
					chars.dwMask = CFM_UNDERLINETYPE;
					chars.bUnderlineType = CFU_UNDERLINE;
					SendMessage(hWnd, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&chars);
					SendMessage(hWnd, EM_HIDESELECTION, 0, 0);
					SetFocus(hWnd);
				}
				else 
				{
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
			if (LOWORD(wParam) == IDM_REDO) 
			{
				SendMessage(hWnd, EM_REDO, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_DELETE) 
			{
				SendMessage(hWnd, WM_CLEAR, 0, 0);
				return 0;
			}
			if (LOWORD(wParam) == IDM_SAVE) 
			{
				int fd;
				EDITSTREAM edit;
				OPENFILENAME lpopen;
				if (!file) 
				{
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
			if (LOWORD(wParam) == IDM_NEW) 
			{
				unsigned char text[1024];
				BOOL newfile = FALSE;
				int ans;
				if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_GETMODIFY, 0, 0) != 0) 
				{
					sprintf(text, "The text in the %s file has changed.\r\n\r\nDo you want to save the changes?", file ? file : "new");
					ans = MessageBox(hDlg, text, "UnrealIRCd", MB_YESNOCANCEL|MB_ICONWARNING);
					if (ans == IDNO)
						newfile = TRUE;
					if (ans == IDCANCEL)
						return TRUE;
					if (ans == IDYES) 
					{
						SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDM_SAVE,0), 0);
						newfile = TRUE;
					}
				}
				else
					newfile = TRUE;
				if (newfile == TRUE) 
				{
					unsigned char szText[256];
					file = NULL;
					strcpy(szText, "UnrealIRCd Editor - New File");
					SetWindowText(hDlg, szText);
					SetWindowText(GetDlgItem(hDlg, IDC_TEXT), NULL);
				}
				break;
			}
			break;
		case WM_USER+10: 
		{
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
		case WM_USER+11: 
		{
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
		case WM_CLOSE: 
		{
			unsigned char text[256];
			int ans;
			if (SendMessage(GetDlgItem(hDlg, IDC_TEXT), EM_GETMODIFY, 0, 0) != 0) 
			{
				sprintf(text, "The text in the %s file has changed.\r\n\r\nDo you want to save the changes?", file ? file : "new");
				ans = MessageBox(hDlg, text, "UnrealIRCd", MB_YESNOCANCEL|MB_ICONWARNING);
				if (ans == IDNO)
					EndDialog(hDlg, TRUE);
				if (ans == IDCANCEL)
					return TRUE;
				if (ans == IDYES) 
				{
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

	return FALSE;
}
