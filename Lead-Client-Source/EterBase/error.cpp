#include "StdAfx.h"

#include <stdio.h>
#include <time.h>
#include <winsock.h>
#include <imagehlp.h>

FILE * fException;

/*
static char __msg[4000], __cmsg[4000];
static int __idx;
CLZObject __l;
*/
/*
typedef BOOL
(CALLBACK *PENUMLOADED_MODULES_CALLBACK)(
    __in PCSTR ModuleName,
    __in ULONG ModuleBase,
    __in ULONG ModuleSize,
    __in_opt PVOID UserContext
    );
*/
#if _MSC_VER >= 1400
BOOL CALLBACK EnumerateLoadedModulesProc(PCSTR ModuleName, ULONG ModuleBase, ULONG ModuleSize, PVOID UserContext)
#else
BOOL CALLBACK EnumerateLoadedModulesProc(PSTR ModuleName, ULONG ModuleBase, ULONG ModuleSize, PVOID UserContext)
#endif
{
	DWORD offset = *((DWORD*)UserContext);
	
	if (offset >= ModuleBase && offset <= ModuleBase + ModuleSize)
	{
		fprintf(fException, "%s", ModuleName);
		//__idx += sprintf(__msg+__idx, "%s", ModuleName);
		return FALSE;
	}
	else
		return TRUE;
}

LONG __stdcall EterExceptionFilter(_EXCEPTION_POINTERS* pExceptionInfo)
{
	HANDLE		hProcess	= GetCurrentProcess();
	HANDLE		hThread		= GetCurrentThread();
	
	fopen_s(&fException, "ErrorLog.txt", "wt");
	if (fException)
	{
		char module_name[256];
		time_t module_time;
		
		HMODULE hModule = GetModuleHandle(NULL);
		
		GetModuleFileName(hModule, module_name, sizeof(module_name));
		module_time = (time_t)GetTimestampForLoadedLibrary(hModule);
		
		char module_time_str[26];
		ctime_s(module_time_str, sizeof(module_time_str), &module_time);

		fprintf(fException, "Module Name: %s\n", module_name);
		fprintf(fException, "Time Stamp: 0x%08x - %s\n", (unsigned int)module_time, module_time_str);
		fprintf(fException, "\n");
		fprintf(fException, "Exception Type: 0x%08x\n", pExceptionInfo->ExceptionRecord->ExceptionCode);
		fprintf(fException, "\n");

		/*
		{
			__idx+=sprintf(__msg+__idx,"Module Name: %s\n", module_name);
			__idx+=sprintf(__msg+__idx, "Time Stamp: 0x%08x - %s\n", module_time, ctime(&module_time));
			__idx+=sprintf(__msg+__idx, "\n");
			__idx+=sprintf(__msg+__idx, "Exception Type: 0x%08x\n", pExceptionInfo->ExceptionRecord->ExceptionCode);
			__idx+=sprintf(__msg+__idx, "\n");
		}
		*/
		
		CONTEXT&	context		= *pExceptionInfo->ContextRecord;
		
#ifdef _WIN64
		fprintf(fException, "rax: 0x%016llx\trbx: 0x%016llx\n", context.Rax, context.Rbx);
		fprintf(fException, "rcx: 0x%016llx\trdx: 0x%016llx\n", context.Rcx, context.Rdx);
		fprintf(fException, "rsi: 0x%016llx\trdi: 0x%016llx\n", context.Rsi, context.Rdi);
		fprintf(fException, "rbp: 0x%016llx\trsp: 0x%016llx\n", context.Rbp, context.Rsp);
#else
		fprintf(fException, "eax: 0x%08x\tebx: 0x%08x\n", context.Eax, context.Ebx);
		fprintf(fException, "ecx: 0x%08x\tedx: 0x%08x\n", context.Ecx, context.Edx);
		fprintf(fException, "esi: 0x%08x\tedi: 0x%08x\n", context.Esi, context.Edi);
		fprintf(fException, "ebp: 0x%08x\tesp: 0x%08x\n", context.Ebp, context.Esp);
#endif
		fprintf(fException, "\n");
		/*
		{
			__idx+=sprintf(__msg+__idx, "eax: 0x%08x\tebx: 0x%08x\n", context.Eax, context.Ebx);
			__idx+=sprintf(__msg+__idx, "ecx: 0x%08x\tedx: 0x%08x\n", context.Ecx, context.Edx);
			__idx+=sprintf(__msg+__idx, "esi: 0x%08x\tedi: 0x%08x\n", context.Esi, context.Edi);
			__idx+=sprintf(__msg+__idx, "ebp: 0x%08x\tesp: 0x%08x\n", context.Ebp, context.Esp);
			__idx+=sprintf(__msg+__idx, "\n");
		}
		*/
		
		STACKFRAME64 stackFrame = {0,};
#ifdef _WIN64
		const DWORD machineType		= IMAGE_FILE_MACHINE_AMD64;
		stackFrame.AddrPC.Offset	= context.Rip;
		stackFrame.AddrStack.Offset	= context.Rsp;
		stackFrame.AddrFrame.Offset	= context.Rbp;
#else
		const DWORD machineType		= IMAGE_FILE_MACHINE_I386;
		stackFrame.AddrPC.Offset	= context.Eip;
		stackFrame.AddrStack.Offset	= context.Esp;
		stackFrame.AddrFrame.Offset	= context.Ebp;
#endif
		stackFrame.AddrPC.Mode		= AddrModeFlat;
		stackFrame.AddrStack.Mode	= AddrModeFlat;
		stackFrame.AddrFrame.Mode	= AddrModeFlat;

		for (int i=0; i < 512 && stackFrame.AddrPC.Offset; ++i)
		{
			if (StackWalk64(machineType, hProcess, hThread, &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL) != FALSE)
			{
				fprintf(fException, "0x%p\t", (void*) stackFrame.AddrPC.Offset);
				//__idx+=sprintf(__msg+__idx, "0x%08x\t", stackFrame.AddrPC.Offset);
				EnumerateLoadedModules(hProcess, (PENUMLOADED_MODULES_CALLBACK) EnumerateLoadedModulesProc, &stackFrame.AddrPC.Offset);
				fprintf(fException, "\n");

				//__idx+=sprintf(__msg+__idx,  "\n");
			}
			else
			{
				break;
			}
		} 
		
		fprintf(fException, "\n");
		//__idx+=sprintf(__msg+__idx, "\n");
		
		
/*
		BYTE* stack = (BYTE*)(context.Esp);
		fprintf(fException, "stack %08x - %08x\n", context.Esp, context.Esp+1024);
		//__idx+=sprintf(__msg+__idx, "stack %08x - %08x\n", context.Esp, context.Esp+1024);
		
		for(i=0; i<16; ++i)
		{
			fprintf(fException, "%08X : ", context.Esp+i*16);
			//__idx+=sprintf(__msg+__idx, "%08X : ", context.Esp+i*16);
			for(int j=0; j<16; ++j)
			{
				fprintf(fException, "%02X ", stack[i*16+j]);
				//__idx+=sprintf(__msg+__idx, "%02X ", stack[i*16+j]);
			}
			fprintf(fException, "\n");
			//__idx+=sprintf(__msg+__idx, "\n");
		}
		fprintf(fException, "\n");
		//__idx+=sprintf(__msg+__idx, "\n");
*/

		fflush(fException);

		fclose(fException);
		fException = NULL;
		
		//WinExec()
		/*
 * CreateProcess("cmd.exe",NULL,NULL,NULL,FALSE, CREATE_NEW_PROCESS_GROUP|DETACHED_PROCESS,NULL,NULL,NULL,NULL); MessageBox(NULL,"A fatal problem occurred while running the game.\nQuit the game and leave an error log.\nDo you want to send the error log to the server?","An error occurred!",MB_YESNO);
 */

		/*
		__l.BeginCompressInBuffer(__msg,__idx,__cmsg);
		if (__l.Compress())
		{
			//fprintf(fException,"Compress printing\n");
			// send this to server
			SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			
			ioctlsocket(s,FIONBIO,0);

			if (s==INVALID_SOCKET)
			{
				//fprintf(fException,"INVALID %X\n",WSAGetLastError());
			}
			
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_port = htons(19294);
			sa.sin_addr.s_addr = inet_addr("147.46.127.42");
			if (connect(s,(sockaddr*)&sa,sizeof(sa)))
			{
				//fprintf(fException,"%X\n",WSAGetLastError());
			}

			int total = 0;
			int ret=0;
			while(1)
			{
				//ret = send(s,(char*)__msg+total,__idx-total,0);
				ret = send(s,(char*)__l.GetBuffer()+total,__l.GetSize()-total,0);
				//fprintf(fException,"send %d\n",ret);
				if (ret<0)
				{
					//fprintf(fException,"%X\n",WSAGetLastError());
					break;
				}
				total+=ret;
				if (total>=__idx)
				//if (total>=__l.GetSize())
					break;
			}
			//__l.GetBuffer();
			Sleep(500);
			closesocket(s);
		}*/

		WinExec("errorlog.exe",SW_SHOW);
		
		
	}
	
	return EXCEPTION_EXECUTE_HANDLER;
}

void SetEterExceptionHandler()
{
	SetUnhandledExceptionFilter(EterExceptionFilter);
}