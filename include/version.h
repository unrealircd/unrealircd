/*
**
** version.h
** UnrealIRCd
** $Id$
*/
#ifndef __versioninclude
#define __versioninclude 1

#include "relinfo.h"
/* 
 * Mark of settings
 */
#ifdef DEBUGMODE
#define DEBUGMODESET "+(debug)"
#else
#define DEBUGMODESET ""
#endif
/**/
#ifdef DEBUG
#define DEBUGSET "(Debug)"
#else
#define DEBUGSET ""
#endif 
/**/
#define COMPILEINFO DEBUGMODESET DEBUGSET
#ifndef WEBTV
#define XTRAINFO ""
#else
#define XTRAINFO "(WebTV)"
#endif
/*
 * Version Unreal3.0
 */
#define UnrealProtocol 		2300
#define PATCH1  		"3"
#define PATCH2  		".0"
#define PATCH3  		"-Morrigan"
#define PATCH4  		"(fix)"
#define PATCH5  		""
#define PATCH6  		""
#define PATCH7  		""
#define PATCH8  		COMPILEINFO
#define PATCH9  		XTRAINFO

#ifndef _WIN32
#define BASE_VERSION "Unreal"
#else
#define BASE_VERSION "UnrealIRCd/32 v"
#endif

#define VERSIONONLY		PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7

#endif /* __versioninclude */ 
