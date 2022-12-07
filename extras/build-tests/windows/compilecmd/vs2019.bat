rem Build command for Visual Studio 2019

rem This used to start with:
rem nmake -f makefile.windows ^
rem But nowadays we use JOM for parallel builds:
jom /j32 -f makefile.windows ^
LIBRESSL_INC_DIR="c:\projects\unrealircd-6-libs\libressl\include" ^
LIBRESSL_LIB_DIR="c:\projects\unrealircd-6-libs\libressl\lib" ^
SSLLIB="crypto-50.lib ssl-53.lib" ^
USE_REMOTEINC=1 ^
LIBCURL_INC_DIR="c:\projects\unrealircd-6-libs\curl\include" ^
LIBCURL_LIB_DIR="c:\projects\unrealircd-6-libs\curl\builds\libcurl-vc-x64-release-dll-ssl-dll-cares-dll-ipv6-obj-lib" ^
CARES_LIB_DIR="c:\projects\unrealircd-6-libs\c-ares\msvc\cares\dll-release" ^
CARES_INC_DIR="c:\projects\unrealircd-6-libs\c-ares\include" ^
CARESLIB="cares.lib" ^
PCRE2_INC_DIR="c:\projects\unrealircd-6-libs\pcre2\include" ^
PCRE2_LIB_DIR="c:\projects\unrealircd-6-libs\pcre2\lib" ^
PCRE2LIB="pcre2-8.lib" ^
ARGON2_LIB_DIR="c:\projects\unrealircd-6-libs\argon2\vs2015\build" ^
ARGON2_INC_DIR="c:\projects\unrealircd-6-libs\argon2\include" ^
ARGON2LIB="Argon2RefDll.lib" ^
SODIUM_LIB_DIR="c:\projects\unrealircd-6-libs\libsodium\bin\x64\Release\v142\dynamic" ^
SODIUM_INC_DIR="c:\projects\unrealircd-6-libs\libsodium\src\libsodium\include" ^
SODIUMLIB="libsodium.lib" ^
JANSSON_LIB_DIR="c:\projects\unrealircd-6-libs\jansson\lib" ^
JANSSON_INC_DIR="c:\projects\unrealircd-6-libs\jansson\include" ^
JANSSONLIB="jansson.lib" ^
GEOIPCLASSIC_LIB_DIR="c:\projects\unrealircd-6-libs\GeoIP\libGeoIP" ^
GEOIPCLASSIC_INC_DIR="c:\projects\unrealircd-6-libs\GeoIP\libGeoIP" ^
GEOIPCLASSICLIB="GeoIP.lib" %*
