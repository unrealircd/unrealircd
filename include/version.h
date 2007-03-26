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

/* Version info follows
 * Please be sure to update ALL fields when changing the version.
 * Also don't forget to bump the protocol version every release.
 */

/** These UNREAL_VERSION_* macros can be used so (3rd party) modules
 * can easily distinguish versions.
 */

/** The generation version number (eg: 3 for Unreal3*) */
#define UNREAL_VERSION_GENERATION   3

/** The major version number (eg: 2 for Unreal3.2*) */
#define UNREAL_VERSION_MAJOR        2

/** The minor version number (eg: 1 for Unreal3.2.1), negative numbers for unstable/alpha/beta */
#define UNREAL_VERSION_MINOR        6

/** Year + week of the year (with Monday as first day of the week).
 * Can be useful if the above 3 versionids are insufficient for you (eg: you want to support CVS).
 * This is updated automatically on the CVS server every Monday. so don't touch it.
 */
#define UNREAL_VERSION_TIME         200713

#define UnrealProtocol 		2309
#define PATCH1  		"3"
#define PATCH2  		".2"
#define PATCH3  		".6"
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
