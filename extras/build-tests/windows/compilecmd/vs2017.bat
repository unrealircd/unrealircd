rem Build command for Visual Studio 2017

nmake -f makefile.win32 ^
LIBRESSL_INC_DIR="c:\projectsNREALIRCD-DEPS\libressl\include" ^
LIBRESSL_LIB_DIR="c:\projectsNREALIRCD-DEPS\libressl\lib" ^
SSLLIB="crypto-43.lib ssl-45.lib" ^
USE_REMOTEINC=1 ^
LIBCURL_INC_DIR="c:\projectsNREALIRCD-DEPS\curl-ssl\include" ^
LIBCURL_LIB_DIR="c:\projectsNREALIRCD-DEPS\curl-ssl\builds\libcurl-vc-x86-release-dll-ssl-dll-ipv6-sspi-obj-lib" ^
CARES_LIB_DIR="c:\projectsNREALIRCD-DEPS\c-ares\msvc\cares\dll-release" ^
CARES_INC_DIR="c:\projectsNREALIRCD-DEPS\c-ares" ^
CARESLIB="cares.lib" ^
TRE_LIB_DIR="c:\projectsNREALIRCD-DEPS\tre\win32\release" ^
TRE_INC_DIR="c:\projectsNREALIRCD-DEPS\tre" ^
TRELIB="tre.lib" ^
PCRE2_INC_DIR="c:\projectsNREALIRCD-DEPS\pcre2\include" ^
PCRE2_LIB_DIR="c:\projectsNREALIRCD-DEPS\pcre2\lib" ^
PCRE2LIB="pcre2-8.lib" %*
