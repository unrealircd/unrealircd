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
/*
 * Version Unreal3.1.6-Noon
 */
#define UnrealProtocol 		2302
#define PATCH1  		"3"
#define PATCH2  		".1"
#define PATCH3  		".7"
#define PATCH4  		"-Jones"
#define PATCH5  		"[beta1]"
#define PATCH6  		""
#define PATCH7  		""
#define PATCH8  		COMPILEINFO
#ifdef _WIN32
#define PATCH9  		"+win32"
#else
#define PATCH9  		""
#endif
/* release header */
#define Rh BASE_VERSION
#define VERSIONONLY		PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7
#endif /* __versioninclude */
