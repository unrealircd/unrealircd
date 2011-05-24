/************************************************************************
 *   IRC - Internet Relay Chat, win32/rtf.c
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

#include "sys.h"
#include <windows.h>
#include "win32.h"
#include "common.h"
#include "struct.h"
#include "h.h"

unsigned char *RTFBuf;

#define MIRC_COLORS "{\\colortbl;\\red255\\green255\\blue255;\\red0\\green0\\blue127;\\red0\\green147\\blue0;\\red255\\green0\\blue0;\\red127\\green0\\blue0;\\red156\\green0\\blue156;\\red252\\green127\\blue0;\\red255\\green255\\blue0;\\red0\\green252\\blue0;\\red0\\green147\\blue147;\\red0\\green255\\blue255;\\red0\\green0\\blue252;\\red255\\green0\\blue255;\\red127\\green127\\blue127;\\red210\\green210\\blue210;\\red0\\green0\\blue0;}"

/* Splits the file up for the EM_STREAMIN message
 * Parameters:
 *  dwCookie - The file information to split
 *  pbBuff   - The output buffer
 *  cb       - The size of pbBuff
 *  pcb      - The total bytes written to bpBuff
 * Returns:
 *  Returns 0 to indicate success
 */
DWORD CALLBACK SplitIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) 
{
	StreamIO *stream = (StreamIO*)dwCookie;
	if (*stream->size == 0)
	{
		pcb = 0;
		*stream->buffer = 0;
	}
	else if (cb <= *stream->size) 
	{
		memcpy(pbBuff, *stream->buffer, cb);
		*stream->buffer += cb;
		*stream->size -= cb;
		*pcb = cb;

	}
	else 
	{
		memcpy(pbBuff, *stream->buffer, *stream->size);
		*pcb = *stream->size;
		*stream->size = 0;
	}
	return 0;
}

/* Reassembles the RTF buffer from EM_STREAMOUT
 * Parameters:
 *  dwCookie - Unused
 *  pbBuff   - The input buffer
 *  cb       - The length of the input buffer
 *  pcb      - The total bytes read from pbBuff
 * Returns:
 *  0 to indicate success
 * Side Effects:
 *  RTFBuf contains the assembled RTF buffer
 */
DWORD CALLBACK BufferIt(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) 
{
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

/* Pushes a color onto the stack
 * Parameters:
 *  color - The color to add to the stack
 *  stack - The stack to add the color to
 */
void ColorPush(unsigned char *color, IRCColor **stack)
{
	IRCColor *t = MyMallocEx(sizeof(IRCColor));
	t->color = strdup(color);
	t->next = *stack;
	(*stack) = t;
}

/* Pops a color off of the stack
 * Parameters:
 *  stack - The stack to pop from
 */
void ColorPop(IRCColor **stack)
{
	IRCColor *p = *stack;
	if (!(*stack))
		return;
	MyFree(p->color);
	
	*stack = p->next;
	MyFree(p);
}

/* Completely empties the color stack
 * Parameters:
 *  stack - The stack to empty
 */
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

#define iseol(x) ((x) == '\r' || (x) == '\n')

/* Converts a string in RTF format to IRC codes
 * Parameters:
 *  fd     - The file descriptor to write to
 *  pbBuff - The buffer containing the RTF text
 *  cb     - The length of the RTF text
 */
DWORD CALLBACK RTFToIRC(int fd, unsigned char *pbBuff, long cb) 
{
	unsigned char *buffer = malloc(cb*2);
	int colors[17], bold = 0, uline = 0, incolor = 0, inbg = 0;
	int lastwascf = 0, lastwascf0 = 0;
	int i = 0;

	IRCColor *TextColors = NULL;
	IRCColor *BgColors = NULL;
	
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
						else if (inbg) 
						{
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

/* Determines the size of the buffer needed to convert IRC codes to RTF
 * Parameters:
 *  buffer - The input buffer with IRC codes
 * Returns:
 *  The lenght of the buffer needed to store the RTF translation
 */
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
		else if (*buf == '\n')
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
			size +=6;
			continue;	
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
	size += strlen("{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fprq1\\"
		"fcharset0 Fixedsys;}}\r\n"
		MIRC_COLORS
		"\\viewkind4\\uc1\\pard\\lang1033\\f0\\fs20")+1;
	return (size);
}

/* Converts a string containing IRC codes to RTF
 * Parameters:
 *  buffer - The input buffer containing IRC codes
 *  string - The output buffer in RTF
 */
void IRCToRTF(unsigned char *buffer, unsigned char *string) 
{
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
		else if (*tmp == '\n')
		{
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
