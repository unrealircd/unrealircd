/************************************************************************
 *   IRC - Internet Relay Chat, aln.c
 *   (C) 2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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

#ifndef STANDALONE
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include "version.h"
#endif

#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#ifndef STANDALONE
#include "h.h"

ID_CVS("$Id$");
ID_Copyright("(C) Carsten Munk 2000");
#endif

static char *aln_chars[] = {
	/* 0-9 */ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	/* 10-19 */ "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
	/* 20-29 */ "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
	/* 30-35 */ "U", "V", "W", "X", "Y", "Z",
	/* 36-45 */ "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
	/* 46-55 */ "k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
	/* 56-61 */ "u", "v", "w", "x", "y", "z",
	/* 62-71 */ "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9",
	/* 73-82 */ "AA", "AB", "AC", "AD", "AE", "AF", "AG", "AH", "AI", "AJ",
	/* 83-92 */ "AK", "AL", "AM", "AN", "AO", "AP", "AQ", "AR", "AS", "AT",
	/* 93-98 */ "AU", "AV", "AW", "AX", "AY", "AZ",
	/* 99-108 */ "Aa", "Ab", "Ac", "Ad", "Ae", "Af", "Ag", "Ah", "Ai", "Aj",
	/* 109-118 */ "Ak", "Al", "Am", "An", "Ao", "Ap", "Aq", "Ar", "As",
	"At",
	/* 119-124 */ "Au", "Av", "Aw", "Ax", "Ay", "Az",
	/* 125-134 */ "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8",
	"B9",
	/* 135-144 */ "BA", "BB", "BC", "BD", "BE", "BF", "BG", "BH", "BI",
	"BJ",
	/* 145-154 */ "BK", "BL", "BM", "BN", "BO", "BP", "BQ", "BR", "BS",
	"BT",
	/* 155-160 */ "BU", "BV", "BW", "BX", "BY", "BZ",
	/* 161-170 */ "Ba", "Bb", "Bc", "Bd", "Be", "Bf", "Bg", "Bh", "Bi",
	"Bj",
	/* 171-180 */ "Bk", "Bl", "Bm", "Bn", "Bo", "Bp", "Bq", "Br", "Bs",
	"Bt",
	/* 181-186 */ "Bu", "Bv", "Bw", "Bx", "By", "Bz",
	/* 187-196 */ "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8",
	"C9",
	/* 196-205 */ "CA", "CB", "CC", "CD", "CE", "CF", "CG", "CH", "CI",
	"CJ",
	/* 206-215 */ "CK", "CL", "CM", "CN", "CO", "CP", "CQ", "CR", "CS",
	"CT",
	/* 216-227 */ "CU", "CV", "CW", "CX", "CY", "CZ",
	/* 228-237 */ "Ca", "Cb", "Cc", "Cd", "Ce", "Cf", "Cg", "Ch", "Ci",
	"Cj",
	/* 238-247 */ "Ck", "Cl", "Cm", "Cn", "Co", "Cp", "Cq", "Cr", "Cs",
	"Ct",
	/* 248-253 */ "Cu", "Cv", "Cw", "Cx", "Cy", "Cz",
	/* 254-257 */ "D0", "D1", "D2", "D3", "D4", "D5", "D6",
	/* */ "D7", "D8", "D9", "DA", "DB", "DC", "DD",
	/* EOT */ NULL
};

int  a_row = 62;
int  b_row = 124;
int  c_row = 186;
int  d_row = 248;

char *convert2aln(int i)
{
	return (aln_chars[i]);
}

int  convertfromaln(char *s)
{
	int  i;
	if (strlen(s) == 1)
	{
		for (i = 0; i < a_row; i++)
			if (!strcmp(s, aln_chars[i]))
				return (i);
		/* no matches? we return 0 */
		return 0;
	}
	else
	{
		switch (*s)
		{
		  case 'A':
			  i = a_row;
			  break;
		  case 'B':
			  i = b_row;
			  break;
		  case 'C':
			  i = c_row;
			  break;
		  case 'D':
			  i = d_row;
			  break;
		  default:
			  i = 0;
		}
		for (; i < 257; i++)
			if (!strcmp(s, aln_chars[i]))
				return (i);

		/* no matches? we return 0 */
		return 0;
	}
}

#ifdef STANDALONE
main()
{
	int  i;

	for (i = 0; i <= 100; i++)
		printf("(%i = %s)\n", i, aln_chars[i]);

	printf("62 = %s\n", convert2aln(62));
	printf("256 = %s\n", convert2aln(256));
	printf("C0 = %i\n", convertfromaln("C0"));
	printf("D = %i\n", convertfromaln("D"));
}
#endif
