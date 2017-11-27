rem Build command for Visual Studio 2017

nmake -f makefile.win32 ^
LIBRESSL_INC_DIR="c:\projects\unrealircd-deps\libressl\include" ^
LIBRESSL_LIB_DIR="c:\projects\unrealircd-deps\libressl\x86" ^
SSLLIB="libcrypto-41.lib libssl-43.lib libtls-15.lib" ^
USE_REMOTEINC=1 ^
LIBCURL_INC_DIR="c:\projects\unrealircd-deps\curl-ssl\include" ^
LIBCURL_LIB_DIR="c:\projects\unrealircd-deps\curl-ssl\builds\libcurl-vc-x86-release-dll-ssl-dll-ipv6-sspi-obj-lib" ^
CARES_LIB_DIR="c:\projects\unrealircd-deps\c-ares\msvc110\cares\dll-release" ^
CARES_INC_DIR="c:\projects\unrealircd-deps\c-ares" ^
CARESLIB="cares.lib" ^
TRE_LIB_DIR="c:\projects\unrealircd-deps\tre\win32\release" ^
TRE_INC_DIR="c:\projects\unrealircd-deps\tre" ^
TRELIB="tre.lib" ^
PCRE2_INC_DIR="c:\projects\unrealircd-deps\pcre2\build" ^
PCRE2_LIB_DIR="c:\projects\unrealircd-deps\pcre2\build\release" ^
PCRE2LIB="pcre2-8.lib" %*
