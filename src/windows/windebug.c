/************************************************************************
 *   IRC - Internet Relay Chat, windows/windebug.c
 *   Copyright (C) 2002-2004 Dominick Meglio (codemastr)
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
#include <dbghelp.h>

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

extern OSVERSIONINFO VerInfo;
extern char OSName[256];
extern char backupbuf[8192];
extern char *buildid;
extern char *extraflags;

/* crappy, but safe :p */
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
										CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
										CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
										CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
										);


/* Runs a stack trace 
 * Parameters:
 *  e - The exception information
 * Returns:
 *  The stack trace with function and line number information
 */
__inline char *StackTrace(EXCEPTION_POINTERS *e) 
{
	static char buffer[5000];
	char curmodule[256];
	DWORD symOptions;
	DWORD64 dwDisp;
	DWORD dwDisp32;
	int frame;
	HANDLE hProcess = GetCurrentProcess();
	IMAGEHLP_SYMBOL64 *pSym = safe_alloc(sizeof(IMAGEHLP_SYMBOL64)+500);
	IMAGEHLP_LINE64 pLine;
	IMAGEHLP_MODULE64 pMod;
	STACKFRAME64 Stack;
	CONTEXT context;

	memcpy(&context, e->ContextRecord, sizeof(CONTEXT));

	/* Load the stack information */
	memset(&Stack, 0, sizeof(Stack));
	Stack.AddrPC.Offset = e->ContextRecord->Rip;
	Stack.AddrPC.Mode = AddrModeFlat;
	Stack.AddrFrame.Offset = e->ContextRecord->Rbp;
	Stack.AddrFrame.Mode = AddrModeFlat;
	Stack.AddrStack.Offset = e->ContextRecord->Rsp;
	Stack.AddrStack.Mode = AddrModeFlat;
	hProcess = GetCurrentProcess();

	/* Initialize symbol retrieval system */
	SymInitialize(hProcess, NULL, TRUE);
	SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME);
	pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	pSym->MaxNameLength = 500;

	/* Retrieve the first module name */
	memset(&pMod, 0, sizeof(pMod));
	pMod.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	SymGetModuleInfo64(hProcess, Stack.AddrPC.Offset, &pMod);
	strcpy(curmodule, pMod.ModuleName);
	sprintf(buffer, "\tModule: %s\n", pMod.ModuleName);

	/* Walk through the stack */
	for (frame = 0; ; frame++) 
	{
		char buf[500];
		if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(),
			&Stack, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
			break;

		memset(&pMod, 0, sizeof(pMod));
		pMod.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
		SymGetModuleInfo64(hProcess, Stack.AddrPC.Offset, &pMod);
		if (strcmp(curmodule, pMod.ModuleName)) 
		{
			strcpy(curmodule, pMod.ModuleName);
			sprintf(buf, "\tModule: %s\n", pMod.ModuleName);
			strcat(buffer, buf);
		}

		memset(&pLine, 0, sizeof(pLine));
		pLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		SymGetLineFromAddr64(hProcess, Stack.AddrPC.Offset, &dwDisp32, &pLine);
		SymGetSymFromAddr64(hProcess, Stack.AddrPC.Offset, &dwDisp, pSym);
		sprintf(buf, "\t\t#%d %s:%d: %s\n", frame, pLine.FileName, pLine.LineNumber, 
		        pSym->Name);
		strcat(buffer, buf);
	}
	strcat(buffer, "End of Stack trace\n");
	return buffer;

}

/* Retrieves the values of several registers
 * Parameters:
 *  context - The CPU context
 * Returns:
 *  The values of the registers as a string.
 */
__inline char *GetRegisters(CONTEXT *context) 
{
	static char buffer[1024];

	sprintf(buffer,
		"\tRAX=%p"
		"\tRBX=%p"
		"\tRCX=%p"
		"\tRDX=%p\n"
		"\tRSI=%p"
		"\tRDI=%p"
		"\tRBP=%p"
		"\tRSP=%p\n"
		"\tR8=%p"
		"\tR9=%p"
		"\tR10=%p"
		"\tR11=%p\n"
		"\tR12=%p"
		"\tR13=%p"
		"\tR14=%p"
		"\tR15=%p\n"
		"\tRIP=%p\n",
		(void *)context->Rax,
		(void *)context->Rbx,
		(void *)context->Rcx,
		(void *)context->Rdx,
		(void *)context->Rsi,
		(void *)context->Rdi,
		(void *)context->Rbp,
		(void *)context->Rsp,
		(void *)context->R8,
		(void *)context->R9,
		(void *)context->R10,
		(void *)context->R11,
		(void *)context->R12,
		(void *)context->R13,
		(void *)context->R14,
		(void *)context->R15,
		(void *)context->Rip);

	return buffer;
}

/* Convert the exception code to a human readable string
 * Parameters:
 *  code - The exception code to convert
 * Returns:
 *  The exception code represented as a string
 */
__inline char *GetException(DWORD code) 
{
	switch (code) 
	{
		case EXCEPTION_ACCESS_VIOLATION:
			return "Access Violation";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			return "Array Bounds Exceeded";
		case EXCEPTION_BREAKPOINT:
			return "Breakpoint";
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			return "Datatype Misalignment";
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			return "Floating Point Denormal Operand";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			return "Floating Point Division By Zero";
		case EXCEPTION_FLT_INEXACT_RESULT:
			return "Floating Point Inexact Result";
		case EXCEPTION_FLT_INVALID_OPERATION:
			return "Floating Point Invalid Operation";
		case EXCEPTION_FLT_OVERFLOW:
			return "Floating Point Overflow";
		case EXCEPTION_FLT_STACK_CHECK:
			return "Floating Point Stack Overflow";
		case EXCEPTION_FLT_UNDERFLOW:
			return "Floating Point Underflow";
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			return "Illegal Instruction";
		case EXCEPTION_IN_PAGE_ERROR:
			return "In Page Error";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			return "Integer Division By Zero";
		case EXCEPTION_INT_OVERFLOW:
			return "Integer Overflow";
		case EXCEPTION_INVALID_DISPOSITION:
			return "Invalid Disposition";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			return "Noncontinuable Exception";
		case EXCEPTION_PRIV_INSTRUCTION:
			return "Unallowed Instruction";
		case EXCEPTION_SINGLE_STEP:
			return "Single Step";
		case EXCEPTION_STACK_OVERFLOW:
			return "Stack Overflow";
		default:
			return "Unknown Exception";
	}
}

void StartCrashReporter(void)
{
	char fname[MAX_PATH], fnamewarg[MAX_PATH+32];
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	
	GetModuleFileName(GetModuleHandle(NULL), fname, MAX_PATH);
	
	snprintf(fnamewarg, sizeof(fnamewarg), "\"%s\" %s", fname, "-R");
	CreateProcess(fname, fnamewarg, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
}

void StartUnrealAgain(void)
{
	char fname[MAX_PATH], fnamewarg[MAX_PATH+32];
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	
	GetModuleFileName(GetModuleHandle(NULL), fname, MAX_PATH);
	
	snprintf(fnamewarg, sizeof(fnamewarg), "\"%s\"", fname);
	CreateProcess(fname, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
}

/* Callback for the exception handler
 * Parameters:
 *  e - The exception information
 * Returns:
 *  EXCEPTION_EXECUTE_HANDLER to terminate the process
 * Side Effects:
 *  unrealircd.PID.core is created
 *  If not running in service mode, a message box is displayed, 
 *   else output is written to service.log
 */
LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS *e) 
{
	MEMORYSTATUSEX memStats;
	char file[512], text[1024], minidumpf[512];
	FILE *fd;
	time_t timet = time(NULL);
#ifndef NOMINIDUMP
	HANDLE hDump;
	HMODULE hDll = NULL;
#endif

	sprintf(file, "unrealircd.%d.core", getpid());
	fd = fopen(file, "w");
	GlobalMemoryStatusEx(&memStats);
	fprintf(fd, "Generated at %s\nOS: %s\n%s[%s%s%s] (%s) on %s\n"
		    "-----------------\nMemory Information:\n"
		    "\tPhysical: (Available:%lluMB/Total:%lluMB)\n"
		    "\tVirtual: (Available:%lluMB/Total:%lluMB)\n"
		    "-----------------\nException:\n\t%s\n-----------------\n"
		    "Backup Buffer:\n\t%s\n-----------------\nRegisters:\n"
		    "%s-----------------\nStack Trace:\n%s",
		     asctime(gmtime(&timet)), OSName,
			 IRCDTOTALVERSION,
		     serveropts, extraflags ? extraflags : "", tainted ? "3" : "",
		     buildid, me.name, memStats.ullAvailPhys/1048576, memStats.ullTotalPhys/1048576,
		     memStats.ullAvailVirtual/1048576, memStats.ullTotalVirtual/1048576,
		     GetException(e->ExceptionRecord->ExceptionCode), backupbuf,
		     GetRegisters(e->ContextRecord), StackTrace(e));

	sprintf(text, "UnrealIRCd has encountered a fatal error. Debugging information has been dumped to %s.", file);
	fclose(fd);

#ifndef NOMINIDUMP
	hDll = LoadLibrary("DBGHELP.DLL");
	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)GetProcAddress(hDll, "MiniDumpWriteDump");
		if (pDump)
		{
			MINIDUMP_EXCEPTION_INFORMATION ExInfo;
			sprintf(minidumpf, "unrealircd.%d.mdmp", getpid());
			hDump = CreateFile(minidumpf, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hDump != INVALID_HANDLE_VALUE)
			{
				ExInfo.ThreadId = GetCurrentThreadId();
				ExInfo.ExceptionPointers = e;
				ExInfo.ClientPointers = 0;

				if (pDump(GetCurrentProcess(), GetCurrentProcessId(), hDump, MiniDumpWithIndirectlyReferencedMemory, &ExInfo, NULL, NULL))
				{
					sprintf(text, "UnrealIRCd has encountered a fatal error. Debugging information has been dumped to %s and %s.", file, minidumpf);
				}
				CloseHandle(hDump);
			}
			sprintf(minidumpf, "unrealircd.%d.full.mdmp", getpid());
			hDump = CreateFile(minidumpf, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hDump != INVALID_HANDLE_VALUE)
			{
				ExInfo.ThreadId = GetCurrentThreadId();
				ExInfo.ExceptionPointers = e;
				ExInfo.ClientPointers = 0;

				pDump(GetCurrentProcess(), GetCurrentProcessId(), hDump, MiniDumpWithPrivateReadWriteMemory, &ExInfo, NULL, NULL);
				CloseHandle(hDump);
			}
		}
	}
#endif
	
	if (!IsService)
	{
		MessageBox(NULL, text, "Fatal Error", MB_OK);
		StartCrashReporter();
	}
	else 
	{
		FILE *fd = fopen("logs\\service.log", "a");

		if (fd)
		{
			fprintf(fd, "UnrealIRCd has encountered a fatal error. Debugging information "
					"has been dumped to unrealircd.%d.core, please file a bug and upload "
					"this file to https://bugs.unrealircd.org/.", getpid());
			fclose(fd);
		}
	}
	CleanUp();
	return EXCEPTION_EXECUTE_HANDLER;
}

void GotSigAbort(int signal)
{
	/* I just want to call ExceptionFilter() but it requires an argument which we don't have...
	 * So just crash, which is rather silly but produces at least a crash report.
	 * Feel free to improve this!
	 */
	char *crash = NULL;
	*crash = 'X';
}

/* Initializes the exception handler */
void InitDebug(void) 
{
	SetUnhandledExceptionFilter(&ExceptionFilter);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	signal(SIGABRT, GotSigAbort);
}


