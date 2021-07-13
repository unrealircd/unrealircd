/** Standard include for all UnrealIRCd modules.
 * This should normally provide all of UnrealIRCd's functionality
 * (that is publicly exposed anyway).
 */
#include "config.h"
<<<<<<< HEAD
=======
#include <assert.h>
>>>>>>> unreal52
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
<<<<<<< HEAD
=======
#include "mempool.h"
>>>>>>> unreal52
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
<<<<<<< HEAD
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif
=======
#include <string.h>
#ifdef GLOBH
 #include <glob.h>
#endif
#ifdef _WIN32
 #include <io.h>
 #include <sys/timeb.h>
 #undef GLOBH
#else
 #include <sys/resource.h>
 #include <utime.h>
 #include <dirent.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include "h.h"
#include "url.h"
#include "version.h"
#ifdef USE_LIBCURL
 #include <curl/curl.h>
#endif
#include <argon2.h>
>>>>>>> unreal52
