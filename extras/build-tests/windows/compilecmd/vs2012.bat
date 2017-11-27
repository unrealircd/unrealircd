rem Build command for Visual Studio 2012

rem This version needs a patch of the makefile.win32
IF EXIST rollback409410.rar GOTO nopatch
rem Patch to support Visual Studio 2012
wget https://www.unrealircd.org/files/dev/win/rollback409410.rar
unrar x rollback409410.rar
patch -p1 -R <rollback409.makefile.patch
patch -p1 -R <rollback409.unrealinst.patch
:nopatch


nmake -f makefile.win32 ^
USE_SSL=1 ^
OPENSSL_INC_DIR="c:\projects\unrealircd-deps\libressl\include" ^
OPENSSL_LIB_DIR="c:\projects\unrealircd-deps\libressl\x86" ^
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
