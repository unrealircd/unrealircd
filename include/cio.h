// $Id$
#include <windows.h>


#define CIOCLASS    "CioClass"

#ifndef CIO
#define CIO

typedef struct tag_CioLine {
	BYTE *Data;
	WORD Len;
	struct tag_CioLine *Prev, *Next;
} CioLine;

typedef struct tag_CioWndInfo {
	CioLine *FirstLine, *CurLine;
	int  Lines, Scroll;
	int  Width, Height, XChar, YChar, YJunk, ScrollMe;
	HFONT hFont;
	BYTE FR, FG, FB;
} CioWndInfo;

#endif
