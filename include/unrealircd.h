/** Standard include for all UnrealIRCd modules.
 * This should normally provide all of UnrealIRCd's functionality
 * (that is publicly exposed anyway).
 */
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "inet.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <sys/timeb.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "badwords.h"
#ifdef _WIN32
#include "version.h"
#endif
