/************************************************************************
 *   IRC - Internet Relay Chat, win32/service.c
 *   Copyright (C) 2002 Dominick Meglio (codemastr)
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
#include <winsvc.h>
#include "struct.h"
#include "h.h"
#include "proto.h"
#include "version.h"
#include <string.h>

SERVICE_STATUS IRCDStatus; 
SERVICE_STATUS_HANDLE IRCDStatusHandle; 
#define IRCD_SERVICE_CONTROL_REHASH 128

BOOL IsService = FALSE;

extern OSVERSIONINFO VerInfo;
#define WIN32_VERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4
				
VOID WINAPI IRCDCtrlHandler(DWORD opcode) {
	DWORD status;
	int i;
	aClient *acptr;
	if (opcode == SERVICE_CONTROL_STOP) {
		IRCDStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus);
		for (i = 0; i <= LastSlot; i++) {
			if (!(acptr = local[i]))
				continue;
			if (IsClient(acptr))
				sendto_one(acptr,
			    	":%s %s %s :Server Terminating.",
			   	 me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name);
			else if (IsServer(acptr))
				sendto_one(acptr, ":%s ERROR :Terminated", me.name);
		}
		unload_all_modules();
		for (i = LastSlot; i >= 0; i--)
			if ((acptr = local[i]) && DBufLength(&acptr->sendQ) > 0)
				(void)send_queued(acptr);
		IRCDStatus.dwWin32ExitCode = 0; 
		IRCDStatus.dwCurrentState = SERVICE_STOPPED;
		IRCDStatus.dwCheckPoint = 0;
		IRCDStatus.dwWaitHint = 0;  
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus);
	}
	else if (opcode == IRCD_SERVICE_CONTROL_REHASH) {
		rehash(&me,&me,0);
	}

  SetServiceStatus(IRCDStatusHandle, &IRCDStatus);
} 

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
	WSADATA WSAData;
	DWORD error = 0;
	char path[MAX_PATH], *folder;
	IsService = TRUE;
	IRCDStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	IRCDStatus.dwCurrentState = SERVICE_START_PENDING;
	IRCDStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;
	IRCDStatus.dwWin32ExitCode = NO_ERROR;
	IRCDStatus.dwServiceSpecificExitCode = 0;
	IRCDStatus.dwCheckPoint = 0;
	IRCDStatus.dwWaitHint = 0;  
	GetModuleFileName(NULL,path,MAX_PATH);
	folder = strrchr(path, '\\');
	*folder = 0;
	chdir(path);
	IRCDStatusHandle = RegisterServiceCtrlHandler("unreal", IRCDCtrlHandler);  
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
			else if (VerInfo.dwMinorVersion == 2)
				strcat(OSName, "Server 2003 ");
		}
		strcat(OSName, VerInfo.szCSDVersion);
	}
	if (OSName[strlen(OSName)-1] == ' ')
		OSName[strlen(OSName)-1] = 0;
	InitDebug();
	if ((error = WSAStartup(MAKEWORD(1, 1), &WSAData)) != 0) {
		IRCDStatus.dwCurrentState = SERVICE_STOPPED;
		IRCDStatus.dwCheckPoint = 0;
		IRCDStatus.dwWaitHint = 0;
		IRCDStatus.dwWin32ExitCode = error;
		IRCDStatus.dwServiceSpecificExitCode = error;
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus); 
		return;
	}
	if ((error = InitwIRCD(dwArgc, lpszArgv)) != 1) {
		IRCDStatus.dwCurrentState = SERVICE_STOPPED;
		IRCDStatus.dwCheckPoint = 0;
		IRCDStatus.dwWaitHint = 0;
		IRCDStatus.dwWin32ExitCode = error;
		IRCDStatus.dwServiceSpecificExitCode = error;
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus); 
		return;
	}	
	IRCDStatus.dwCurrentState = SERVICE_RUNNING;
	IRCDStatus.dwCheckPoint = 0;
	IRCDStatus.dwWaitHint = 0;  
	SetServiceStatus(IRCDStatusHandle, &IRCDStatus);

	SocketLoop(0);
	return;
}
