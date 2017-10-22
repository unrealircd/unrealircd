rem Build script for appveyor

rem Visual Studio 2012
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" x86
rem Installing tools
cinst unrar -y
cinst wget -y
cinst innosetup -y
wget https://www.unrealircd.org/files/dev/win/dlltool.exe

rem Installing UnrealIRCd dependencies
cd \projects
mkdir unrealircd-deps
cd unrealircd-deps
wget https://www.unrealircd.org/files/dev/win/SetACL.exe
wget https://www.unrealircd.org/files/dev/win/libs/unrealircd-libraries-4.0.8.rar
unrar x unrealircd-libraries-4.0.8.rar

cd \projects\unrealircd

rem Patch to support Visual Studio 2012
wget https://www.unrealircd.org/files/dev/win/rollback409410.rar
unrar x rollback409410.rar
patch -p1 -R <rollback409.makefile.patch
patch -p1 -R <rollback409.unrealinst.patch

rem Now the actual build
rem NOTE: if you update the nmake command, be sure to update the other one as well
nmake -f makefile.win32 USE_SSL=1 OPENSSL_INC_DIR="c:\projects\unrealircd-deps\libressl\include" OPENSSL_LIB_DIR="c:\projects\unrealircd-deps\libressl\x86" USE_REMOTEINC=1 LIBCURL_INC_DIR="c:\projects\unrealircd-deps\curl-ssl\include" LIBCURL_LIB_DIR="c:\projects\unrealircd-deps\curl-ssl\builds\libcurl-vc-x86-release-dll-ssl-dll-ipv6-sspi-obj-lib" CARES_LIB_DIR="c:\projects\unrealircd-deps\c-ares\msvc110\cares\dll-release" CARES_INC_DIR="c:\projects\unrealircd-deps\c-ares" CARESLIB="cares.lib" TRE_LIB_DIR="c:\projects\unrealircd-deps\tre\win32\release" TRE_INC_DIR="c:\projects\unrealircd-deps\tre" TRELIB="tre.lib" PCRE2_INC_DIR="c:\projects\unrealircd-deps\pcre2\build" PCRE2_LIB_DIR="c:\projects\unrealircd-deps\pcre2\build\release" PCRE2LIB="pcre2-8.lib" || exit 0

rem The above command will fail, due to missing symbol file
rem However the symbol file can only be generated after the above command
rem So... we create the symbolfile...
nmake -f makefile.win32 SYMBOLFILE

rem And we re-run the exact same command:
nmake -f makefile.win32 USE_SSL=1 OPENSSL_INC_DIR="c:\projects\unrealircd-deps\libressl\include" OPENSSL_LIB_DIR="c:\projects\unrealircd-deps\libressl\x86" USE_REMOTEINC=1 LIBCURL_INC_DIR="c:\projects\unrealircd-deps\curl-ssl\include" LIBCURL_LIB_DIR="c:\projects\unrealircd-deps\curl-ssl\builds\libcurl-vc-x86-release-dll-ssl-dll-ipv6-sspi-obj-lib" CARES_LIB_DIR="c:\projects\unrealircd-deps\c-ares\msvc110\cares\dll-release" CARES_INC_DIR="c:\projects\unrealircd-deps\c-ares" CARESLIB="cares.lib" TRE_LIB_DIR="c:\projects\unrealircd-deps\tre\win32\release" TRE_INC_DIR="c:\projects\unrealircd-deps\tre" TRELIB="tre.lib" PCRE2_INC_DIR="c:\projects\unrealircd-deps\pcre2\build" PCRE2_LIB_DIR="c:\projects\unrealircd-deps\pcre2\build\release" PCRE2LIB="pcre2-8.lib"

rem Convert c:\dev to c:\projects\unrealircd-deps
rem TODO: should use environment variable in innosetup script?
sed -i "s/c:\\\\dev/c:\\\\projects\\\\unrealircd-deps/gi" src\win32\unrealinst.iss

rem Build installer file
"c:\Program Files (x86)\Inno Setup 5\Compil32.exe" /cc src\win32\unrealinst.iss

rem Show some proof
ren mysetup.exe unrealircd-dev-build.exe
dir unrealircd-dev-build.exe
sha256sum unrealircd-dev-build.exe

rem Upload artifact
appveyor PushArtifact unrealircd-dev-build.exe
