/*
**
** version.h
** UnrealIRCd
** $Id$
*/
#ifndef __versioninclude
#define __versioninclude 1

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

/** Unreal Protocol version */
#define UnrealProtocol 		2400

/** These UNREAL_VERSION_* macros will be used so (3rd party) modules
 * can easily distinguish versions.
 */

/** The generation version number (eg: 3 for Unreal3*) */
#define UNREAL_VERSION_GENERATION   3
/** The major version number (eg: 3 for Unreal3.3*) */
#define UNREAL_VERSION_MAJOR        3
/** The minor version number (eg: 1 for Unreal3.3.1), we'll use negative numbers for unstable/alpha/beta */
#define UNREAL_VERSION_MINOR        -100

#define PATCH1  		"3"
#define PATCH2  		".3"
#define PATCH3  		"-unstable"
#define PATCH4  		""
#define PATCH5  		""
#define PATCH6  		""
#define PATCH7  		""
#define PATCH8  		COMPILEINFO
#define PATCH9  		""
/* release header */
#define Rh BASE_VERSION
#define VERSIONONLY		PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7
#endif /* __versioninclude */
