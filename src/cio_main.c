#include <windows.h>
#include <memory.h>
#include <stdio.h>
#include <crtdbg.h>
#include "Cio.h"
#include "CioFunc.h"
#include "common.h"

CioLine *CioMakeLine(CioLine * Prev, CioLine * Next)
{
	CioLine *Line;

	Line = LocalAlloc(LPTR, sizeof(CioLine));
	_ASSERT(Line);
	if (Prev)
	{
		Prev->Next = Line;
		Line->Prev = Prev;
	}
	if (Next)
	{
		Next->Prev = Line;
		Line->Next = Next;
	}

	return Line;
}


void CioRemoveLine(CioWndInfo * CWI, CioLine * Line)
{
	if (Line->Next)
		Line->Next->Prev = Line->Prev;
	if (Line->Prev)
		Line->Prev->Next = Line->Next;
	else
		CWI->FirstLine = Line->Next;

	if (Line->Data)
		LocalFree(Line->Data);
	LocalFree(Line);
}



//
//   FUNCTION: Cio_Create(HANDLE, HWND, DWORD, int, int, int, int)
//
//   PURPOSE: Creates main CIO window
//
HWND Cio_Create(HINSTANCE hInstance, HWND hParent, DWORD Style, int X, int Y,
    int W, int H)
{
	HWND hWnd;

	if (hParent == NULL)
		return NULL;

	hWnd = CreateWindow(CIOCLASS, NULL, WS_CHILD | WS_VSCROLL | Style,
	    X, Y, W, H, hParent, NULL, hInstance, NULL);

	return hWnd;
}


//
//  FUNCTION: Cio_WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for a CIO window.
//
LRESULT CALLBACK Cio_WndProc(HWND hWnd, UINT message, WPARAM wParam,
    LPARAM lParam)
{
	DWORD Style;

	switch (message)
	{
	  case WM_CREATE:
		  return Cio_WndCreate(hWnd);

	  case WM_RBUTTONDOWN:	// RightClick in windows client area...
		  return 0;

	  case WM_PAINT:
		  return Cio_WndPaint(hWnd);

	  case WM_SIZE:
		  Style = GetWindowLong(GetParent(hWnd), GWL_STYLE);
		  if (Style & WS_MINIMIZE)
			  return 0;
		  return Cio_WndSize(hWnd, lParam);

	  case WM_VSCROLL:
		  return Cio_WndScroll(hWnd, wParam);

	  case WM_DESTROY:
		  return Cio_WndDestroy(hWnd);

	  case WM_SETFONT:
		  return Cio_SetFont(hWnd, wParam);

	  case CIO_ADDSTRING:
		  return Cio_WndAddString(hWnd, wParam, (char *)lParam);

	  default:
		  return (DefWindowProc(hWnd, message, wParam, lParam));
	}
	return (0);
}


BOOL Cio_WndCreate(HWND hWnd)
{
	CioWndInfo *CWI;
	LOGFONT lfFont;
	TEXTMETRIC tm;
	HDC  hdc;

	CWI = LocalAlloc(LPTR, sizeof(CioWndInfo));
	_ASSERT(CWI);
	lfFont.lfHeight = 12;
	lfFont.lfWidth = 0;
	lfFont.lfEscapement = 0;
	lfFont.lfOrientation = 0;
	lfFont.lfWeight = 400;
	lfFont.lfItalic = 0;
	lfFont.lfUnderline = 0;
	lfFont.lfStrikeOut = 0;
	lfFont.lfCharSet = ANSI_CHARSET;
	lfFont.lfOutPrecision = OUT_STRING_PRECIS;
	lfFont.lfClipPrecision = CLIP_STROKE_PRECIS;
	lfFont.lfQuality = DRAFT_QUALITY;
	lfFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	wsprintf(lfFont.lfFaceName, "FixedSys");

	CWI->hFont = CreateFontIndirect(&lfFont);

	hdc = GetDC(hWnd);
	SelectObject(hdc, CWI->hFont);
	GetTextMetrics(hdc, &tm);
	ReleaseDC(hWnd, hdc);

	CWI->XChar = tm.tmAveCharWidth;
	CWI->YChar = tm.tmHeight + tm.tmExternalLeading;

	CWI->FirstLine = CWI->CurLine = CioMakeLine(NULL, NULL);
	CWI->Lines = 0;

	SetWindowLong(hWnd, GWL_USER, (DWORD) CWI);

	return 0;
}


BOOL Cio_WndPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	CioWndInfo *CWI;
	HDC  hdc;
	HFONT hOldFont;
	CioLine *Line;
	RECT rect;
	int  nCol, nEndCol, nMaxCol, nRow, nEndRow, nBegCol, nCount, i = 0;
	int  nVertPos, nHorzPos, nOff, nBegOff, NewColorAt = 0;
	BYTE Red, Green, Blue, NewColor = 0, UnderLine = 0;
	HPEN hpen;


	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);
	hdc = BeginPaint(hWnd, &ps);

	hOldFont = SelectObject(hdc, CWI->hFont);
	rect = ps.rcPaint;
	nCol = max(0, rect.left / CWI->XChar);
	nEndCol = rect.right / CWI->XChar;
	nEndCol = min(CWI->Width, nEndCol + 1);
	nEndRow = max(0, rect.top / CWI->YChar);
	nRow = rect.bottom / CWI->YChar;
	nRow = min(CWI->Height, nRow + 1);

	nBegOff = nCol;
	nBegCol = nCol;

	/*
	 * Find the first line to use, and work up from there.
	 */
	Line = CWI->FirstLine;
	while (Line && i < CWI->Scroll)
	{
		if (Line->Data)
			i++;
		Line = Line->Next;
	}
	i = 0;
	while (Line && i < (CWI->Height - nRow))
	{
		if (Line->Data)
			i++;
		Line = Line->Next;
	}

	for (; nRow >= nEndRow; nRow--)
	{
		UnderLine = 0;
		while (Line && !Line->Data)
			Line = Line->Next;
		if (!Line)
			continue;

		nMaxCol = Line->Len;
		nOff = nBegOff;
		nCol = 0;
		nCount = 0;
		for (i = 0; i < Line->Len; i++)
		{
			if (Line->Data[i] == 0)
			{
				Red = Line->Data[i + 1];
				Green = Line->Data[i + 2];
				Blue = Line->Data[i + 3];
				SetTextColor(hdc, RGB(Red, Green, Blue));
				SetBkColor(hdc, RGB(255, 255, 255));
				i += 3;
				nCol += 4;
				continue;
			}
			if (nCount >= nBegCol)
				break;
			nCol++;
			nCount++;
		}
		if (i >= Line->Len)
			continue;

		for (; nCol <= nMaxCol; nCol++)
		{
			nCount = nMaxCol - nCol;
			for (i = 0; i < (nMaxCol - nCol); i++)
			{
				if (Line->Data[nCol + i] == 0x1F)
				{
					nCount = i;
					if (UnderLine == 0)
					{
						UnderLine = 1;
						break;
					}
					if (UnderLine == 1)
					{
						UnderLine = 2;
						break;
					}
					break;
				}
				if (Line->Data[nCol + i] == 0x0F)
				{
					nCount = i;
					UnderLine = 0;
					break;
				}
				if (Line->Data[nCol + i] == 0)
				{
					NewColor = 1;
					NewColorAt = nCol + i;
					nCount = i;
					i += 3;
					break;
				}
			}

			if (nCount)
			{
				nVertPos = nRow * CWI->YChar - CWI->YChar +
				    CWI->YJunk;
				nHorzPos = nOff * CWI->XChar;
				rect.top = nVertPos;
				rect.bottom = nVertPos + CWI->YChar;
				rect.left = nHorzPos;
				rect.right = nHorzPos + (CWI->XChar * nCount);
				SetBkMode(hdc, OPAQUE);

				ExtTextOut(hdc, nHorzPos, nVertPos, ETO_OPAQUE,
				    &rect, (LPSTR) Line->Data + nCol,
				    nCount, NULL);
				nOff += nCount;

				if (UnderLine == 2)
				{
					hpen = CreatePen(PS_SOLID, 0,
					    RGB(Red, Green, Blue));
					SelectObject(hdc, hpen);
					MoveToEx(hdc, (int)rect.left,
					    (int)rect.bottom - 1,
					    (LPPOINT) NULL);
					LineTo(hdc, (int)rect.right,
					    (int)rect.bottom - 1);
					DeleteObject(hpen);
					UnderLine = 0;
				}
			}
			if (UnderLine == 1)
				UnderLine = 2;

			nCol += i;
			if (NewColor)
			{
				Red = Line->Data[NewColorAt + 1];
				Green = Line->Data[NewColorAt + 2];
				Blue = Line->Data[NewColorAt + 3];
				SetTextColor(hdc, RGB(Red, Green, Blue));
				SetBkColor(hdc, RGB(255, 255, 255));
				NewColor = 0;
			}
		}
		Line = Line->Next;
	}

	SelectObject(hdc, hOldFont);
	EndPaint(hWnd, &ps);
	return 0;
}


BOOL Cio_WndDestroy(HWND hWnd)
{
	CioWndInfo *CWI;

	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);
	while (CWI->FirstLine)
		CioRemoveLine(CWI, CWI->FirstLine);

	DeleteObject(CWI->hFont);
	LocalFree(CWI);

	return 0;
}


int  Cio_WndAddString(HWND hWnd, int Len, char *Buffer)
{
	CioWndInfo *CWI;
	CioLine *Line;
	int  TLen = 0, DLen = 0, Loop = 0, LastSpace = 0, Scrolled = 0, Pad = 0;
	BYTE Red = 0, Grn = 0, Blu = 0;


	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);

	if (CWI->ScrollMe == TRUE)
	{
		Cio_Scroll(hWnd, CWI, 1);
		Scrolled++;
		CWI->ScrollMe = FALSE;
	}
	if (Buffer[Len - 1] == '\r')
	{
		CWI->ScrollMe = TRUE;
		Len -= 1;
	}

	while (Len)
	{
		/*
		 * Check the current line to see if we have room to tack
		 * on our string to the end of.  If not, put as much as
		 * possible on, then scroll up a new line.
		 */
		TLen = DLen = 0;
		Line = CWI->FirstLine;
		while (TLen < Line->Len)
		{
			if (Line->Data[TLen] == 0)
			{
				TLen += 4;
				continue;
			}
			TLen += 1;
			DLen += 1;
		}
		Loop = TLen = 0;
		while (TLen < Len)
		{
			if (Buffer[TLen] == 0)
			{
				TLen += 4;
				continue;
			}
			TLen += 1;
			Loop += 1;
		}
		if (DLen + Loop > CWI->Width)
		{
			Cio_Scroll(hWnd, CWI, 1);
			Scrolled++;
			Loop = TLen = 0;
			do
			{
				if (Buffer[TLen] == 0)
				{
					Red = Buffer[TLen + 1];
					Grn = Buffer[TLen + 2];
					Blu = Buffer[TLen + 3];
					TLen += 4;
					continue;
				}
				if (TLen >= Len || DLen + Loop >= CWI->Width)
					break;
				if (Buffer[TLen] == ' ')
					LastSpace = TLen;
				TLen += 1;
				Loop += 1;
			}
			while (TLen < Len && DLen + Loop < CWI->Width);

			if (TLen != Len && Buffer[TLen] != ' ' &&
			    LastSpace && (LastSpace - TLen) < 16)
				TLen = LastSpace;

			if (Line->Data != NULL)
			{
				Line->Data = LocalReAlloc(Line->Data,
				    Line->Len + TLen + Pad, LMEM_MOVEABLE);
				_ASSERT(Line->Data);
				memset(Line->Data, ' ', Pad);
				memmove(Line->Data + Line->Len + Pad, Buffer,
				    TLen);
				Line->Len += TLen + Pad;
			}
			else
			{
				if (Buffer[0] == 0)
				{
					Line->Data =
					    LocalAlloc(LPTR, TLen + Pad);
					_ASSERT(Line->Data);
					memset(Line->Data, ' ', Pad);
					memmove(Line->Data + Pad, Buffer, TLen);
					Line->Len = TLen + Pad;
				}
				else
				{
					Line->Data =
					    LocalAlloc(LPTR, TLen + 4 + Pad);
					_ASSERT(Line->Data);
					memset(Line->Data + 4, ' ', Pad);
					memmove(Line->Data + 4 + Pad, Buffer,
					    TLen);
					Line->Len = TLen + 4 + Pad;
					Line->Data[0] = 0;
					Line->Data[1] = Red;
					Line->Data[2] = Grn;
					Line->Data[3] = Blu;
				}
			}

			if (TLen == LastSpace)
				TLen++;
			Pad = 3;
			Len -= TLen;
			memmove(Buffer, Buffer + TLen, Len);
			if (Buffer[0] == '\r' && Len == 1)
				Len = 0;
			continue;
		}		/* if ( DLen+Loop > CWI->Width ) */

		Line = CWI->FirstLine;
		if (Line->Data != NULL)
		{
			Line->Data =
			    LocalReAlloc(Line->Data, Line->Len + TLen + Pad,
			    LMEM_MOVEABLE);
			_ASSERT(Line->Data);
			memset(Line->Data, ' ', Pad);
			memmove(Line->Data + Line->Len + Pad, Buffer, Len);
			Line->Len += Len + Pad;
		}
		else
		{
			if (Buffer[0] == 0)
			{
				Line->Data = LocalAlloc(LPTR, Len + Pad);
				_ASSERT(Line->Data);
				memset(Line->Data, ' ', Pad);
				memmove(Line->Data + Pad, Buffer, Len);
				Line->Len = Len + Pad;
			}
			else
			{
				Line->Data = LocalAlloc(LPTR, Len + 4 + Pad);
				_ASSERT(Line->Data);
				memset(Line->Data + 4, ' ', Pad);
				memmove(Line->Data + 4 + Pad, Buffer, Len);
				Line->Len = Len + 4 + Pad;
				Line->Data[0] = 0;
				Line->Data[1] = Red;
				Line->Data[2] = Grn;
				Line->Data[3] = Blu;
			}
		}

		break;
	}			/* while (Len) */

	if (!CWI->Scroll)
	{
		RECT rc;

		rc.left = rc.top = 0;
		rc.right = CWI->Width * CWI->XChar;
		rc.bottom = CWI->Height * CWI->YChar + CWI->YJunk;
		ScrollWindow(hWnd, 0, -(Scrolled * CWI->YChar), &rc, NULL);
		rc.top = (CWI->Height - Scrolled) * CWI->YChar + CWI->YJunk;
		if (rc.top < 0)
			rc.top = 0;
		return Scrolled;
	}
	return 0;
}


BOOL Cio_WndSize(HWND hWnd, LPARAM lParam)
{
	CioWndInfo *CWI;
	SCROLLINFO si;

	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);

	CWI->Scroll = 0;
	CWI->Width = LOWORD(lParam) / CWI->XChar;
	CWI->Height = HIWORD(lParam) / CWI->YChar;
	CWI->YJunk = HIWORD(lParam) - (CWI->Height * CWI->YChar);

	InvalidateRect(hWnd, NULL, TRUE);

	/* Set the Scroll Bar information. */

	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	si.nMin = 1;
	si.nMax = CWI->Lines;
	si.nPage = CWI->Height - 1;
	si.nPos = si.nMax;
	SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

	return 0;
}


BOOL Cio_WndScroll(HWND hWnd, WPARAM wParam)
{
	CioWndInfo *CWI;
	SCROLLINFO si;
	int  Amnt = 0, nPos = 0;

	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	GetScrollInfo(hWnd, SB_VERT, &si);

	si.nMax -= si.nPage;
	si.nMax += si.nMin;
	nPos = si.nPos;

	switch (LOWORD(wParam))
	{
	  case SB_PAGEUP:
		  Amnt = -(int)si.nPage;
		  break;

	  case SB_PAGEDOWN:
		  Amnt = si.nPage;
		  break;

	  case SB_LINEUP:
		  Amnt = -(int)1;
		  break;

	  case SB_LINEDOWN:
		  Amnt = 1;
		  break;

	  case SB_THUMBTRACK:
		  break;
	}

	if (Amnt)
	{
		nPos += Amnt;

		if (nPos < si.nMin)
		{
			Amnt -= nPos - si.nMin;
			nPos = si.nMin;
		}

		if (nPos > si.nMax)
		{
			Amnt -= nPos - si.nMax;
			nPos = si.nMax;
		}

		if (Amnt)
		{
			SetScrollPos(hWnd, SB_VERT, nPos, TRUE);
			CWI->Scroll = si.nMax - nPos;
			InvalidateRect(hWnd, NULL, TRUE);
		}
	}

	return 0;
}


void Cio_Scroll(HWND hWnd, CioWndInfo * CWI, int Scroll)
{
	int  i = 0, KillAmt = 0, StopAt = 0;
	SCROLLINFO si;
	CioLine *Line, *Next;

	if (CWI->Lines + Scroll > 500 && !CWI->Scroll)
	{
		Line = CWI->FirstLine;
		KillAmt = max((CWI->Lines + Scroll) - 500, 20);
		for (i = 0; Line && i < (CWI->Lines - KillAmt); i++)
			Line = Line->Next;

		while (Line)
		{
			Next = Line->Next;
			CioRemoveLine(CWI, Line);
			Line = Next;
		}
		CWI->Lines -= KillAmt;
	}

	for (i = 0; i < Scroll; i++)
		CWI->FirstLine = CioMakeLine(NULL, CWI->FirstLine);

	CWI->Lines += Scroll;
	if (CWI->Scroll)
		CWI->Scroll += Scroll;

	/* Set the Scroll Bar information. */
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	GetScrollInfo(hWnd, SB_VERT, &si);

	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE | SIF_DISABLENOSCROLL;
	si.nMin = 1;
	si.nMax = CWI->Lines;
	si.nPage = CWI->Height - 1;
	si.nPos = CWI->Lines - si.nPage - CWI->Scroll;
	SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}


BOOL Cio_PrintF(HWND hWnd, char *InBuf, ...)
{
	CioWndInfo *CWI;
	va_list argptr;
	char *Buffer = NULL, *Ptr = NULL;
	DWORD Len = 0, TLen, Off;
	int  Scrolled = 0;
	RECT rc;


	if ((Buffer = LocalAlloc(LPTR, 16384)) == NULL)
		return FALSE;

	va_start(argptr, InBuf);
	Len = vsprintf(Buffer, InBuf, argptr);
	va_end(argptr);
	if (Len == 0)
	{
		LocalFree(Buffer);
		return FALSE;
	}

	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);
	Ptr = memchr(Buffer, 0, TLen = Len);
	while (Ptr != NULL)
	{
		Off = Ptr - Buffer;

		CWI->FR = Buffer[Off + 1];
		CWI->FG = Buffer[Off + 2];
		CWI->FB = Buffer[Off + 3];

		Off += 4;
		TLen = Len - Off;
		if (TLen == 0)
			break;
		Ptr = memchr(Buffer + Off, 0, TLen);
	}
	if (Buffer[0] != 0)
	{
		memmove(Buffer + 4, Buffer, Len);
		Len += 4;
		Buffer[0] = 0;
		Buffer[1] = CWI->FR;
		Buffer[2] = CWI->FG;
		Buffer[3] = CWI->FB;
	}

	rc.left = 0;
	rc.right = CWI->Width * CWI->XChar;
	rc.top = rc.bottom = CWI->Height * CWI->YChar;

	do
	{
		Ptr = memchr(Buffer, '\r', Len);
		if (Ptr)
			TLen = (Ptr - Buffer) + 1;
		else
			TLen = Len;
		if (Buffer[0] != '\r' || CWI->ScrollMe == FALSE)
		{
			Scrolled = Cio_WndAddString(hWnd, TLen, Buffer);
			rc.top -= Scrolled * CWI->YChar;
		}
		Len -= TLen;
		memmove(Buffer, Buffer + TLen, Len);
	}
	while (Len);

	if (rc.top < rc.bottom)
	{
		if (rc.top < 0)
			rc.top = 0;
		InvalidateRect(hWnd, &rc, TRUE);
		UpdateWindow(hWnd);
	}
	LocalFree(Buffer);
	return TRUE;
}


BOOL Cio_Puts(HWND hWnd, char *InBuf, DWORD Len)
{
	CioWndInfo *CWI;
	char *Buffer = NULL, *Ptr = NULL;
	DWORD TLen, Off;
	int  Scrolled = 0;
	RECT rc;


	if (!Len)
		return FALSE;

	if ((Buffer = LocalAlloc(LPTR, 16384)) == NULL)
		return FALSE;
	memmove(Buffer, InBuf, Len);

	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);
	Ptr = memchr(Buffer, 0, TLen = Len);
	while (Ptr != NULL)
	{
		Off = Ptr - Buffer;

		CWI->FR = Buffer[Off + 1];
		CWI->FG = Buffer[Off + 2];
		CWI->FB = Buffer[Off + 3];

		Off += 4;
		TLen = Len - Off;
		if (TLen == 0)
			break;
		Ptr = memchr(Buffer + Off, 0, TLen);
	}
	if (Buffer[0] != 0)
	{
		memmove(Buffer + 4, Buffer, Len);
		Len += 4;
		Buffer[0] = 0;
		Buffer[1] = CWI->FR;
		Buffer[2] = CWI->FG;
		Buffer[3] = CWI->FB;
	}

	rc.left = 0;
	rc.right = CWI->Width * CWI->XChar;
	rc.top = rc.bottom = CWI->Height * CWI->YChar;

	do
	{
		Ptr = memchr(Buffer, '\r', Len);
		if (Ptr)
			TLen = (Ptr - Buffer) + 1;
		else
			TLen = Len;
		if (Buffer[0] != '\r' || CWI->ScrollMe == FALSE)
		{
			Scrolled = Cio_WndAddString(hWnd, TLen, Buffer);
			rc.top -= Scrolled * CWI->YChar;
		}
		Len -= TLen;
		memmove(Buffer, Buffer + TLen, Len);
	}
	while (Len);

	if (rc.top < rc.bottom)
	{
		if (rc.top < 0)
			rc.top = 0;
		InvalidateRect(hWnd, &rc, TRUE);
		UpdateWindow(hWnd);
	}
	LocalFree(Buffer);
	return TRUE;
}


BOOL Cio_SetFont(HWND hWnd, WPARAM wParam)
{
	CioWndInfo *CWI;
	TEXTMETRIC tm;
	HFONT hFont = (HFONT) wParam;
	HDC  hdc;
	RECT rc;


	CWI = (CioWndInfo *) GetWindowLong(hWnd, GWL_USER);

	if (CWI->hFont)
		DeleteObject(CWI->hFont);
	CWI->hFont = hFont;
	hdc = GetDC(hWnd);
	SelectObject(hdc, CWI->hFont);
	GetTextMetrics(hdc, &tm);
	ReleaseDC(hWnd, hdc);

	CWI->XChar = tm.tmAveCharWidth;
	CWI->YChar = tm.tmHeight + tm.tmExternalLeading;

	/*
	 * Send resize message so it will pick up the new font size.
	 */
	GetClientRect(hWnd, &rc);
	SendMessage(hWnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left,
	    rc.bottom - rc.top));
	InvalidateRect(hWnd, NULL, TRUE);

	return TRUE;
}
