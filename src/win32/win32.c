/************************************************************************
 *   IRC - Internet Relay Chat, win32/win32.c
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

/* Retrieves the OS name as a string
 * Parameters:
 *  VerInfo - The version information from GetVersionEx
 *  OSName  - The buffer to write the OS name to
 */
void GetOSName(OSVERSIONINFO VerInfo, char *OSName)
{
	int len;
	strcpy(OSName, "Windows ");
	if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) 
	{
		if (VerInfo.dwMajorVersion == 4) 
		{
			if (VerInfo.dwMinorVersion == 0) 
			{
				strcat(OSName, "95 ");
				if (!strcmp(VerInfo.szCSDVersion, " C"))
					strcat(OSName, "OSR2 ");
			}
			else if (VerInfo.dwMinorVersion == 10) 
			{
				strcat(OSName, "98 ");
				if (!strcmp(VerInfo.szCSDVersion, " A"))
					strcat(OSName, "SE ");
			}
			else if (VerInfo.dwMinorVersion == 90)
				strcat(OSName, "Me ");
		}
	}
	else if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) 
	{
		if (VerInfo.dwMajorVersion == 3 && VerInfo.dwMinorVersion == 51)
			strcat(OSName, "NT 3.51 ");
		else if (VerInfo.dwMajorVersion == 4 && VerInfo.dwMinorVersion == 0)
			strcat(OSName, "NT 4.0 ");
		else if (VerInfo.dwMajorVersion == 5) 
		{
			if (VerInfo.dwMinorVersion == 0)
				strcat(OSName, "2000 ");
			else if (VerInfo.dwMinorVersion == 1)
				strcat(OSName, "XP ");
			else if (VerInfo.dwMinorVersion == 2)
				strcat(OSName, "Server 2003 ");
		}
		strcat(OSName, VerInfo.szCSDVersion);
	}

	len = strlen(OSName)-1;
	if (OSName[len] == ' ')
		OSName[len] = 0;	
}
