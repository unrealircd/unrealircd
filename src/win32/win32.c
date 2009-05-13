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
#include <tchar.h>
#include <strsafe.h>

#include "win32.h"

#pragma comment(lib, "User32.lib")

// Newer product types than what is currently defined in
//   Visual Studio 2005
#ifndef PRODUCT_ULTIMATE
#define PRODUCT_ULTIMATE                        0x00000001
#endif
#ifndef PRODUCT_HOME_BASIC
#define PRODUCT_HOME_BASIC                      0x00000002
#endif
#ifndef PRODUCT_HOME_PREMIUM
#define PRODUCT_HOME_PREMIUM                    0x00000003
#endif
#ifndef PRODUCT_ENTERPRISE
#define PRODUCT_ENTERPRISE                      0x00000004
#endif
#ifndef PRODUCT_HOME_BASIC_N
#define PRODUCT_HOME_BASIC_N                    0x00000005
#endif
#ifndef PRODUCT_BUSINESS
#define PRODUCT_BUSINESS                        0x00000006
#endif
#ifndef PRODUCT_STANDARD_SERVER
#define PRODUCT_STANDARD_SERVER                 0x00000007
#endif
#ifndef PRODUCT_DATACENTER_SERVER
#define PRODUCT_DATACENTER_SERVER               0x00000008
#endif
#ifndef PRODUCT_SMALLBUSINESS_SERVER
#define PRODUCT_SMALLBUSINESS_SERVER            0x00000009
#endif
#ifndef PRODUCT_ENTERPRISE_SERVER
#define PRODUCT_ENTERPRISE_SERVER               0x0000000A
#endif
#ifndef PRODUCT_STARTER
#define PRODUCT_STARTER                         0x0000000B
#endif
#ifndef PRODUCT_DATACENTER_SERVER_CORE
#define PRODUCT_DATACENTER_SERVER_CORE          0x0000000C
#endif
#ifndef PRODUCT_STANDARD_SERVER_CORE
#define PRODUCT_STANDARD_SERVER_CORE            0x0000000D
#endif
#ifndef PRODUCT_ENTERPRISE_SERVER_CORE
#define PRODUCT_ENTERPRISE_SERVER_CORE          0x0000000E
#endif
#ifndef PRODUCT_ENTERPRISE_SERVER_IA64
#define PRODUCT_ENTERPRISE_SERVER_IA64          0x0000000F
#endif
#ifndef PRODUCT_BUSINESS_N
#define PRODUCT_BUSINESS_N                      0x00000010
#endif
#ifndef PRODUCT_WEB_SERVER
#define PRODUCT_WEB_SERVER                      0x00000011
#endif
#ifndef PRODUCT_CLUSTER_SERVER
#define PRODUCT_CLUSTER_SERVER                  0x00000012
#endif
#ifndef PRODUCT_HOME_SERVER
#define PRODUCT_HOME_SERVER                     0x00000013
#endif
#ifndef PRODUCT_STORAGE_EXPRESS_SERVER
#define PRODUCT_STORAGE_EXPRESS_SERVER          0x00000014
#endif
#ifndef PRODUCT_STORAGE_STANDARD_SERVER
#define PRODUCT_STORAGE_STANDARD_SERVER         0x00000015
#endif
#ifndef PRODUCT_STORAGE_WORKGROUP_SERVER
#define PRODUCT_STORAGE_WORKGROUP_SERVER        0x00000016
#endif
#ifndef PRODUCT_STORAGE_ENTERPRISE_SERVER
#define PRODUCT_STORAGE_ENTERPRISE_SERVER       0x00000017
#endif
#ifndef PRODUCT_SERVER_FOR_SMALLBUSINESS
#define PRODUCT_SERVER_FOR_SMALLBUSINESS        0x00000018
#endif
#ifndef PRODUCT_SMALLBUSINESS_SERVER_PREMIUM
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM    0x00000019
#endif

// Newer system metrics values
#ifndef SM_SERVERR2
#define SM_SERVERR2 89   
#endif

#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER                     0x00008000
#endif

#ifndef VER_SUITE_STORAGE_SERVER
#define VER_SUITE_STORAGE_SERVER                0x00002000
#endif

#ifndef VER_SUITE_COMPUTE_SERVER
#define VER_SUITE_COMPUTE_SERVER                0x00004000
#endif

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);



/* Retrieves the OS name as a string
 * Parameters:
 * pszOS  - The buffer to write the OS name to (size at least OSVER_SIZE)
 */
int GetOSName(char *pszOS)
{
   OSVERSIONINFOEX osvi;
   SYSTEM_INFO si;
   PGNSI pGNSI;
   PGPI pGPI;
   BOOL bOsVersionInfoEx;
   DWORD dwType;

   ZeroMemory(&si, sizeof(SYSTEM_INFO));
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
      return -1;

   // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

   pGNSI = (PGNSI) GetProcAddress(
      GetModuleHandle(TEXT("kernel32.dll")), 
      "GetNativeSystemInfo");
   if(NULL != pGNSI)
      pGNSI(&si);
   else GetSystemInfo(&si);

   if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && 
        osvi.dwMajorVersion > 4 )
   {
      StringCchCopy(pszOS, OSVER_SIZE, TEXT("Microsoft "));

      // Test for the specific product.

      if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 )
      {
         if( osvi.wProductType == VER_NT_WORKSTATION )
             StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows 7 "));
         else StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows Server 2008 R2 " ));
      }
      
      if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 )
      {
         if( osvi.wProductType == VER_NT_WORKSTATION )
             StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows Vista "));
         else StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows Server 2008 " ));

         pGPI = (PGPI) GetProcAddress(
            GetModuleHandle(TEXT("kernel32.dll")), 
            "GetProductInfo");

         pGPI( 6, 0, 0, 0, &dwType);

         switch( dwType )
         {
            case PRODUCT_ULTIMATE:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Ultimate Edition" ));
               break;
            case PRODUCT_HOME_PREMIUM:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Home Premium Edition" ));
               break;
            case PRODUCT_HOME_BASIC:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Home Basic Edition" ));
               break;
            case PRODUCT_ENTERPRISE:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Enterprise Edition" ));
               break;
            case PRODUCT_BUSINESS:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Business Edition" ));
               break;
            case PRODUCT_STARTER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Starter Edition" ));
               break;
            case PRODUCT_CLUSTER_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Cluster Server Edition" ));
               break;
            case PRODUCT_DATACENTER_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Datacenter Edition" ));
               break;
            case PRODUCT_DATACENTER_SERVER_CORE:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Datacenter Edition (core installation)" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Enterprise Edition" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Enterprise Edition (core installation)" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Enterprise Edition for Itanium-based Systems" ));
               break;
            case PRODUCT_SMALLBUSINESS_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Small Business Server" ));
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Small Business Server Premium Edition" ));
               break;
            case PRODUCT_STANDARD_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Standard Edition" ));
               break;
            case PRODUCT_STANDARD_SERVER_CORE:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Standard Edition (core installation)" ));
               break;
            case PRODUCT_WEB_SERVER:
               StringCchCat(pszOS, OSVER_SIZE, TEXT("Web Server Edition" ));
               break;
         }
         if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            StringCchCat(pszOS, OSVER_SIZE, TEXT( ", 64-bit" ));
         else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
            StringCchCat(pszOS, OSVER_SIZE, TEXT(", 32-bit"));
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
      {
         if( GetSystemMetrics(SM_SERVERR2) )
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Windows Server 2003 R2, "));
         else if ( osvi.wSuiteMask==VER_SUITE_STORAGE_SERVER )
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Windows Storage Server 2003"));
         else if ( osvi.wSuiteMask==VER_SUITE_WH_SERVER )
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Windows Home Server"));
         else if( osvi.wProductType == VER_NT_WORKSTATION &&
                  si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
         {
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Windows XP Professional x64 Edition"));
         }
         else StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows Server 2003, "));

         // Test for the server type.
         if ( osvi.wProductType != VER_NT_WORKSTATION )
         {
            if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Datacenter Edition for Itanium-based Systems" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Enterprise Edition for Itanium-based Systems" ));
            }

            else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Datacenter x64 Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Enterprise x64 Edition" ));
                else StringCchCat(pszOS, OSVER_SIZE, TEXT( "Standard x64 Edition" ));
            }

            else
            {
                if ( osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Compute Cluster Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Datacenter Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Enterprise Edition" ));
                else if ( osvi.wSuiteMask & VER_SUITE_BLADE )
                   StringCchCat(pszOS, OSVER_SIZE, TEXT( "Web Edition" ));
                else StringCchCat(pszOS, OSVER_SIZE, TEXT( "Standard Edition" ));
            }
         }
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
      {
         StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows XP "));
         if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Home Edition" ));
         else StringCchCat(pszOS, OSVER_SIZE, TEXT( "Professional" ));
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
      {
         StringCchCat(pszOS, OSVER_SIZE, TEXT("Windows 2000 "));

         if ( osvi.wProductType == VER_NT_WORKSTATION )
         {
            StringCchCat(pszOS, OSVER_SIZE, TEXT( "Professional" ));
         }
         else 
         {
            if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
              StringCchCat(pszOS, OSVER_SIZE, TEXT( "Datacenter Server" ));
            else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
               StringCchCat(pszOS, OSVER_SIZE, TEXT( "Advanced Server" ));
            else StringCchCat(pszOS, OSVER_SIZE, TEXT( "Server" ));
         }
      }

       // Include service pack (if any) and build number.

      if( _tcslen(osvi.szCSDVersion) > 0 )
      {
          StringCchCat(pszOS, OSVER_SIZE, TEXT(" ") );
          StringCchCat(pszOS, OSVER_SIZE, osvi.szCSDVersion);
      }

      {
          TCHAR buf[80];

          StringCchPrintf( buf, 80, TEXT(" (build %d)"), osvi.dwBuildNumber);
          StringCchCat(pszOS, OSVER_SIZE, buf);
      }

      return 0; 
   }
   else
   {  
      return -1;
   }
}
