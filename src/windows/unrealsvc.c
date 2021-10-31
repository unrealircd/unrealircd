/************************************************************************
 *   IRC - Internet Relay Chat, windows/unrealsvc.c
 *   Copyright (C) 2002 Dominick Meglio (codemastr)
 *   Copyright (C) 2006-2021 Bram Matthys (Syzop)
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

#include "unrealircd.h"

typedef BOOL (*UCHANGESERVICECONFIG2)(SC_HANDLE, DWORD, LPVOID);
HMODULE hAdvapi;
UCHANGESERVICECONFIG2 uChangeServiceConfig2;

#define IRCD_SERVICE_CONTROL_REHASH 128
void show_usage() {
	fprintf(stderr, "unrealsvc start|stop|rehash|restart|install|uninstall|config <option> <value>");
	fprintf(stderr, "\nValid config options:\nstartup auto|manual\n");
	fprintf(stderr, "crashrestart delay\n");
}

char *show_error(DWORD code) {
	static char buf[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf, 1024, NULL);
	return buf;
}

SC_HANDLE unreal_open_service_manager(void)
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!hSCManager)
	{
		printf("Failed to connect to service manager: %s", show_error(GetLastError()));
		printf("Note that elevated administrator permissions are necessary to execute this command.\n");
		exit(1);
	}
	return hSCManager;
}
int main(int argc, char *argv[]) {
	char *bslash;

	if (argc < 2) {
		show_usage();
		return -1;
	}
	hAdvapi = LoadLibrary("advapi32.dll");
	uChangeServiceConfig2 = (UCHANGESERVICECONFIG2)GetProcAddress(hAdvapi, "ChangeServiceConfig2A");

	if (!strcasecmp(argv[1], "install"))
	{
		SC_HANDLE hService, hSCManager;
		char path[MAX_PATH+1];
		char binpath[MAX_PATH+1];
		hSCManager = unreal_open_service_manager();

		GetModuleFileName(NULL,path,MAX_PATH);
		if ((bslash = strrchr(path, '\\')))
			*bslash = 0;
		
		strcpy(binpath,path);
		strcat(binpath, "\\UnrealIRCd.exe");
		hService = CreateService(hSCManager, "UnrealIRCd", "UnrealIRCd",
				 SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS,
				 SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, binpath,
				 NULL, NULL, NULL, TEXT("NT AUTHORITY\\NetworkService"), "");
		if (hService) 
		{
			SERVICE_DESCRIPTION info;
			printf("UnrealIRCd NT Service successfully installed\n");
			info.lpDescription = "Internet Relay Chat Server. Allows users to chat with eachother via an IRC client.";
			uChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &info);
			CloseServiceHandle(hService);
			printf("\n[!!!] IMPORTANT: By default the network service user cannot write to the \n"
			       "UnrealIRCd 6 folder and this will make UnrealIRCd fail to boot without\n"
				   "writing any meaningful error to the log files.\n"
				   "You have two options:\n"
				   "1) Manually grant FULL permissions to NT AUTHORITY\\NetworkService\n"
				   "   for the UnrealIRCd 6 folder, all its subfolders and files.\n"
				   "OR, easier and recommended:\n"
				   "2) just re-run the UnrealIRCd installer and select 'Install as a service',\n"
				   "   which sets all the necessary permissions automatically.\n");
		} else {
			printf("Failed to install UnrealIRCd NT Service - %s", show_error(GetLastError()));
		}
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!strcasecmp(argv[1], "uninstall"))
	{
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", DELETE); 
		if (DeleteService(hService)) 
			printf("UnrealIRCd NT Service successfully uninstalled\n");
		else
			printf("Failed to uninstall UnrealIRCd NT Service - %s\n", show_error(GetLastError()));
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!strcasecmp(argv[1], "start"))
	{
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_START); 
		if (StartService(hService, 0, NULL))
			printf("UnrealIRCd NT Service successfully started");
		else
			printf("Failed to start UnrealIRCd NT Service - %s", show_error(GetLastError()));
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!strcasecmp(argv[1], "stop"))
	{
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP); 
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
		printf("UnrealIRCd NT Service successfully stopped");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!strcasecmp(argv[1], "restart"))
	{
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP|SERVICE_START); 
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
		if (StartService(hService, 0, NULL)) 
			printf("UnrealIRCd NT Service successfully restarted");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 0;
	}
	else if (!strcasecmp(argv[1], "rehash"))
	{
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_USER_DEFINED_CONTROL); 
		ControlService(hService, IRCD_SERVICE_CONTROL_REHASH, &status);
		printf("UnrealIRCd NT Service successfully rehashed");
	}
	else if (!strcasecmp(argv[1], "config"))
	{
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = unreal_open_service_manager();
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_CHANGE_CONFIG|SERVICE_START);
		if (argc < 3) {
			show_usage();
			return -1;
		}
		if (!strcasecmp(argv[2], "startup")) {
			if (ChangeServiceConfig(hService, SERVICE_NO_CHANGE,
					    !strcasecmp(argv[3], "auto") ? SERVICE_AUTO_START
						: SERVICE_DEMAND_START, SERVICE_NO_CHANGE,
					    NULL, NULL, NULL, NULL, NULL, NULL, NULL)) 
				printf("UnrealIRCd NT Service configuration changed");
			else
				printf("UnrealIRCd NT Service configuration change failed - %s", show_error(GetLastError()));	
		}
		else if (!strcasecmp(argv[2], "crashrestart")) {
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

