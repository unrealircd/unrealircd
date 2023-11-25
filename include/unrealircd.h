/** Standard include for all UnrealIRCd modules.
 * This should normally provide all of UnrealIRCd's functionality
 * (that is publicly exposed anyway).
 */
#include "config.h"
#include <assert.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "mempool.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
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
#include "dns.h"
#include "version.h"
#ifdef USE_LIBCURL
 #include <curl/curl.h>
#endif
#include <argon2.h>
