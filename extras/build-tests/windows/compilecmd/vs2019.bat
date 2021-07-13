rem Build command for Visual Studio 2019

nmake -f makefile.windows ^
LIBRESSL_INC_DIR="c:\projects\unrealircd-5-libs\libressl\include" ^
LIBRESSL_LIB_DIR="c:\projects\unrealircd-5-libs\libressl\lib" ^
SSLLIB="crypto-46.lib ssl-48.lib" ^
USE_REMOTEINC=1 ^
LIBCURL_INC_DIR="c:\projects\unrealircd-5-libs\curl\include" ^
LIBCURL_LIB_DIR="c:\projects\unrealircd-5-libs\curl\builds\libcurl-vc-x64-release-dll-ssl-dll-cares-dll-ipv6-obj-lib" ^
CARES_LIB_DIR="c:\projects\unrealircd-5-libs\c-ares\msvc\cares\dll-release" ^
CARES_INC_DIR="c:\projects\unrealircd-5-libs\c-ares\include" ^
CARESLIB="cares.lib" ^
PCRE2_INC_DIR="c:\projects\unrealircd-5-libs\pcre2\include" ^
PCRE2_LIB_DIR="c:\projects\unrealircd-5-libs\pcre2\lib" ^
PCRE2LIB="pcre2-8.lib" ^
ARGON2_LIB_DIR="c:\projects\unrealircd-5-libs\argon2\vs2015\build" ^
ARGON2_INC_DIR="c:\projects\unrealircd-5-libs\argon2\include" ^
ARGON2LIB="Argon2RefDll.lib" ^
SODIUM_LIB_DIR="c:\projects\unrealircd-5-libs\libsodium\bin\x64\Release\v142\dynamic" ^
SODIUM_INC_DIR="c:\projects\unrealircd-5-libs\libsodium\src\libsodium\include" ^
SODIUMLIB="libsodium.lib" %*
