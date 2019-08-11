/************************************************************************
 *   IRC - Internet Relay Chat, win32/win32.h
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

typedef struct {
	int *size;
	unsigned char **buffer;
} StreamIO;

typedef struct colorlist {
	struct colorlist *next;
	unsigned char *color;
} IRCColor;

extern UINT WM_FINDMSGSTRING;
extern unsigned char *RTFBuf;
extern HINSTANCE hInst;

extern FARPROC lpfnOldWndProc;
extern LRESULT RESubClassFunc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
extern DWORD CALLBACK SplitIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);
extern DWORD CALLBACK BufferIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);
extern DWORD CALLBACK RTFToIRC(int fd, unsigned char *pbBuff, long cb);

#define OSVER_SIZE 256

