/************************************************************************
 *   IRC - Internet Relay Chat, win32/unreal.c
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
#include <string.h>
#include <stdio.h>
static 	OSVERSIONINFO VerInfo;
typedef BOOL (*UCHANGESERVICECONFIG2)(SC_HANDLE, DWORD, LPVOID);
HMODULE hAdvapi;
UCHANGESERVICECONFIG2 uChangeServiceConfig2;

#define IRCD_SERVICE_CONTROL_REHASH 128
void show_usage() {
	fprintf(stderr, "unreal start|stop|rehash|restart|install|uninstall|config <option> <value>");
	fprintf(stderr, "\nValid config options:\nstartup auto|manual\n");
	if (VerInfo.dwMajorVersion == 5) 
		fprintf(stderr, "crashrestart delay\n");
}

char *show_error(DWORD code) {
	static char buf[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf, 1024, NULL);
	return buf;
}


int main(int argc, char *argv[]) {
	char *bslash;
	VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&VerInfo);
	if (argc < 2) {
		show_usage();
		return -1;
	}
	hAdvapi = LoadLibrary("advapi32.dll");
	uChangeServiceConfig2 = (UCHANGESERVICECONFIG2)GetProcAddress(hAdvapi, "ChangeServiceConfig2A");
	if (!stricmp(argv[1], "install")) {
		SC_HANDLE hService, hSCManager;
		char path[MAX_PATH+1];
		char binpath[MAX_PATH+1];
		hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

		if (!hSCManager) {
			exit(0);
		}
		GetModuleFileName(NULL,path,MAX_PATH);
		if ((bslash = strrchr(path, '\\')))
			*bslash = 0;
		
		strcpy(binpath,path);
		strcat(binpath, "\\wircd.exe");
		hService = CreateService(hSCManager, "UnrealIRCd", "UnrealIRCd",
				 SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
				 SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, binpath,
				 NULL, NULL, NULL, NULL, NULL); 
		if (hService) 
		{
			printf("UnrealIRCd NT Service successfully installed");
			if (VerInfo.dwMajorVersion >= 5) {
				SERVICE_DESCRIPTION info;
				info.lpDescription = "Internet Relay Chat Server. Allows users to chat with eachother via an IRC client.";
				uChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &info);
			}
			CloseServiceHandle(hService);
		} else
			printf("Failed to install UnrealIRCd NT Service - %s", show_error(GetLastError()));
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!stricmp(argv[1], "uninstall")) {
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", DELETE); 
		if (DeleteService(hService)) 
			printf("UnrealIRCd NT Service successfully uninstalled");
		else
			printf("Failed to uninstall UnrealIRCd NT Service - %s", show_error(GetLastError()));
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!stricmp(argv[1], "start")) {
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_START); 
		if (StartService(hService, 0, NULL)) 
			printf("UnrealIRCd NT Service successfully started");
		else
			printf("Failed to start UnrealIRCd NT Service - %s", show_error(GetLastError()));
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!stricmp(argv[1], "stop")) {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP); 
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
		printf("UnrealIRCd NT Service successfully stopped");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!stricmp(argv[1], "restart")) {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP|SERVICE_START); 
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
		if (StartService(hService, 0, NULL)) 
			printf("UnrealIRCd NT Service successfully restarted");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!stricmp(argv[1], "rehash")) {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_USER_DEFINED_CONTROL); 
		ControlService(hService, IRCD_SERVICE_CONTROL_REHASH, &status);
		printf("UnrealIRCd NT Service successfully rehashed");
	}
	else if (!stricmp(argv[1], "config")) {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd",
						 SERVICE_CHANGE_CONFIG|SERVICE_START); 
		if (argc < 3) {
			show_usage();
			return -1;
		}
		if (!stricmp(argv[2], "startup")) {
			if (ChangeServiceConfig(hService, SERVICE_NO_CHANGE,
					    !stricmp(argv[3], "auto") ? SERVICE_AUTO_START
						: SERVICE_DEMAND_START, SERVICE_NO_CHANGE,
					    NULL, NULL, NULL, NULL, NULL, NULL, NULL)) 
				printf("UnrealIRCd NT Service configuration changed");
			else
				printf("UnrealIRCd NT Service configuration change failed - %s", show_error(GetLastError()));	
		}
		else if (!stricmp(argv[2], "crashrestart") && VerInfo.dwMajorVersion == 5) {
			SERVICE_FAILURE_ACTIONS hFailActions;
			SC_ACTION hAction;
			memset(&hFailActions, 0, sizeof(hFailActions));
			if (argc >= 4) {
				hFailActions.dwResetPeriod = 30;
				hFailActions.cActions = 1;
				hAction.Type = SC_ACTION_RESTART;
				hAction.Delay = atoi(argv[3])*60000;
				hFailActions.lpsaActions = &hAction;
				if (uChangeServiceConfig2(hService, SERVICE_CONFIG_FAILURE_ACTIONS, 	
						     &hFailActions))
					printf("UnrealIRCd NT Service configuration changed");
				else
					printf("UnrealIRCd NT Service configuration change failed - %s", show_error(GetLastError()));	
			}
			else {
				hFailActions.dwResetPeriod = 0;
				hFailActions.cActions = 0;
				hAction.Type = SC_ACTION_NONE;
				hFailActions.lpsaActions = &hAction;
				if (uChangeServiceConfig2(hService, SERVICE_CONFIG_FAILURE_ACTIONS,
						     &hFailActions)) 
					printf("UnrealIRCd NT Service configuration changed");
				else
					printf("UnrealIRCd NT Service configuration change failed - %s", show_error(GetLastError()));	

				
			}
		}
		else {
			show_usage();
			return -1;
		}	
	}
	else {
		show_usage();
		return -1;
	}

		
			
}

