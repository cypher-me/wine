/*
 * Win32 miscellaneous functions
 *
 * Copyright 1995 Thomas Sandford (tdgsandf@prds-grn.demon.co.uk)
 */

/* Misc. new functions - they should be moved into appropriate files
at a later date. */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "windows.h"
#include "winnt.h"
#include "winerror.h"
#include "debug.h"

/****************************************************************************
 *		UTRegister (KERNEL32.697)
 */
BOOL32 WINAPI UTRegister(HMODULE32 hModule,
                      LPSTR lpsz16BITDLL,
                      LPSTR lpszInitName,
                      LPSTR lpszProcName,
                      /*UT32PROC*/ LPVOID *ppfn32Thunk,
                      /*FARPROC*/ LPVOID pfnUT32CallBack,
                      LPVOID lpBuff)
{
    fprintf(stderr, "UTRegister(%#x,...): stub!\n",hModule);
    return TRUE;
}

/****************************************************************************
 *		UTUnRegister (KERNEL32.698)
 */
BOOL32 WINAPI UTUnRegister(HMODULE32 hModule)
{
    fprintf(stderr, "UTUnRegister(%#x: stub!\n", hModule);
    return TRUE;
}


/****************************************************************************
 *		QueryPerformanceCounter (KERNEL32.564)
 */
BOOL32 WINAPI QueryPerformanceCounter(LPLARGE_INTEGER counter)
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    counter->LowPart = tv.tv_usec+tv.tv_sec*1000000;
    counter->HighPart = 0;
    return TRUE;
}

HANDLE32 WINAPI FindFirstChangeNotification32A(LPCSTR lpPathName,BOOL32 bWatchSubtree,DWORD dwNotifyFilter) {
	FIXME(file,"(%s,%d,%08lx): stub\n",
	      lpPathName,bWatchSubtree,dwNotifyFilter);
	return 0xcafebabe;
}

BOOL32 WINAPI FindNextChangeNotification(HANDLE32 fcnhandle) {
	FIXME(file,"(%08x): stub!\n",fcnhandle);
	return FALSE;
}

/****************************************************************************
 *		QueryPerformanceFrequency (KERNEL32.565)
 */
BOOL32 WINAPI QueryPerformanceFrequency(LPLARGE_INTEGER frequency)
{
	frequency->LowPart	= 1000000;
	frequency->HighPart	= 0;
	return TRUE;
}

/****************************************************************************
 *		DeviceIoControl (KERNEL32.188)
 */
BOOL32 WINAPI DeviceIoControl(HANDLE32 hDevice, DWORD dwIoControlCode, 
			      LPVOID lpvlnBuffer, DWORD cblnBuffer,
			      LPVOID lpvOutBuffer, DWORD cbOutBuffer,
			      LPDWORD lpcbBytesReturned,
			      LPOVERLAPPED lpoPverlapped)
{

        FIXME(comm, "(...): stub!\n");
	/* FIXME: Set appropriate error */
	return FALSE;

}

/****************************************************************************
 *		FlushInstructionCache (KERNEL32.261)
 */
BOOL32 WINAPI FlushInstructionCache(DWORD x,DWORD y,DWORD z) {
	FIXME(debug,"(0x%08lx,0x%08lx,0x%08lx): stub\n",x,y,z);
	return TRUE;
}

/***********************************************************************
 *           CreateNamedPipeA   (KERNEL32.168)
 */
HANDLE32 WINAPI CreateNamedPipeA (LPCSTR lpName, DWORD dwOpenMode,
				  DWORD dwPipeMode, DWORD nMaxInstances,
				  DWORD nOutBufferSize, DWORD nInBufferSize,
				  DWORD nDafaultTimeOut,
				  LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  FIXME (win32, "CreateNamedPipeA: stub\n");
  /* if (nMaxInstances > PIPE_UNLIMITED_INSTANCES) {
    SetLastError (ERROR_INVALID_PARAMETER);
    return INVALID_HANDLE_VALUE;
  } */

  SetLastError (ERROR_UNKNOWN);
  return INVALID_HANDLE_VALUE32;
}

/***********************************************************************
 *           CreateNamedPipeW   (KERNEL32.169)
 */
HANDLE32 WINAPI CreateNamedPipeW (LPCWSTR lpName, DWORD dwOpenMode,
				  DWORD dwPipeMode, DWORD nMaxInstances,
				  DWORD nOutBufferSize, DWORD nInBufferSize,
				  DWORD nDafaultTimeOut,
				  LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  FIXME (win32, "CreateNamedPipeW: stub\n");

  SetLastError (ERROR_UNKNOWN);
  return INVALID_HANDLE_VALUE32;
}

/***********************************************************************
 *           GetSystemPowerStatus      (KERNEL32.621)
 */
BOOL32 WINAPI GetSystemPowerStatus(LPSYSTEM_POWER_STATUS sps_ptr)
{
    return FALSE;   /* no power management support */
}


/***********************************************************************
 *           SetSystemPowerState      (KERNEL32.630)
 */
BOOL32 WINAPI SetSystemPowerState(BOOL32 suspend_or_hibernate,
                                  BOOL32 force_flag)
{
    /* suspend_or_hibernate flag: w95 does not support
       this feature anyway */

    for ( ;0; )
    {
        if ( force_flag )
        {
        }
        else
        {
        }
    }
    return TRUE;
}

/**************************************************************************
 *              GetNumberFormat32A	(KERNEL32.355)
 */
INT32 WINAPI GetNumberFormat32A(LCID locale, DWORD dwflags,
			       LPCSTR lpvalue,  char *lpFormat,
			       LPSTR lpNumberStr, int cchNumber)
/* NOTE: type of lpFormat should be CONST NUMBERFORMAT */

{
 int n;

 FIXME(file,"%s: stub, no reformating done\n",lpvalue);

 n = strlen(lpvalue);
 if (cchNumber) { 
   strncpy(lpNumberStr,lpvalue,cchNumber);
   if (cchNumber <= n) {
     lpNumberStr[cchNumber-1] = 0;
     n = cchNumber-1;
   }
 }
 return n;
}
