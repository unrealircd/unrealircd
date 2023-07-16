echo on

rem Temporarily hardcoded:
set TARGET=Visual Studio 2019
set SHORTNAME=vs2019

rem Initialize Visual Studio variables
if "%TARGET%" == "Visual Studio 2017" call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
if "%TARGET%" == "Visual Studio 2019" call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

rem Installing tools
rem only for appveyor:
rem cinst unrar -y
rem cinst unzip -y
rem cinst innosetup -y

rem Installing UnrealIRCd dependencies
cd \projects
mkdir unrealircd-6-libs
cd unrealircd-6-libs
curl -fsS -o unrealircd-libraries-6-devel.zip https://www.unrealircd.org/files/dev/win/libs/unrealircd-libraries-6-devel.zip
unzip unrealircd-libraries-6-devel.zip
copy dlltool.exe \users\user\worker\unreal6-w10\build /y

rem for appveyor, use: cd \projects\unrealircd
cd \users\user\worker\unreal6-w10\build

rem Install 'unrealircd-tests'
cd ..
rd /q/s unrealircd-tests
git clone -q --branch unreal60 https://github.com/unrealircd/unrealircd-tests.git unrealircd-tests
if %ERRORLEVEL% NEQ 0 EXIT /B 1
cd build

rem Now the actual build
rem - First this, otherwise JOM will fail
IF NOT EXIST src\version.c nmake -f Makefile.windows CONF
rem - Then build most of UnrealIRCd.exe etc
call extras\build-tests\windows\compilecmd\%SHORTNAME%.bat UNREALSVC.EXE UnrealIRCd.exe unrealircdctl.exe
rem - It will fail due to missing symbolfile, which we create here..
rem   it needs to run with SLOW=1 because JOM doesn't understand things otherwise..
SET SLOW=1
call extras\build-tests\windows\compilecmd\%SHORTNAME%.bat SYMBOLFILE
SET SLOW=0
rem - Then we finalize building UnrealIRCd.exe: should be no error
call extras\build-tests\windows\compilecmd\%SHORTNAME%.bat UNREALSVC.EXE UnrealIRCd.exe unrealircdctl.exe
if %ERRORLEVEL% NEQ 0 EXIT /B 1
rem - Build all the modules (DLL files): should be no error
call extras\build-tests\windows\compilecmd\%SHORTNAME%.bat MODULES
if %ERRORLEVEL% NEQ 0 EXIT /B 1

rem Compile dependencies for unrealircd-tests -- this doesn't belong here though..
copy ..\unrealircd-tests\serverconfig\unrealircd\modules\fakereputation.c src\modules\third /Y
call extras\build-tests\windows\compilecmd\%SHORTNAME%.bat CUSTOMMODULE MODULEFILE=fakereputation
if %ERRORLEVEL% NEQ 0 EXIT /B 1

rem Convert c:\dev to c:\projects\unrealircd-6-libs
rem TODO: should use environment variable in innosetup script?
sed -i "s/c:\\dev\\unrealircd-6-libs/c:\\projects\\unrealircd-6-libs/gi" src\windows\unrealinst.iss

rem Build installer file
"c:\Program Files (x86)\Inno Setup 5\iscc.exe" /Q- src\windows\unrealinst.iss
if %ERRORLEVEL% NEQ 0 EXIT /B 1

rem Show some proof
ren mysetup.exe unrealircd-dev-build.exe
dir unrealircd-dev-build.exe
sha256sum unrealircd-dev-build.exe

rem Kill any old instances, just to be sure
taskkill -im unrealircd.exe -f
sleep 2
rem Just a safety measure so we don't end up testing
rem some old version...
del "C:\Program Files\UnrealIRCd 6\bin\unrealircd.exe"

echo Running installer...
start /WAIT unrealircd-dev-build.exe /VERYSILENT /LOG=setup.log
if %ERRORLEVEL% NEQ 0 goto installerfailed

rem Upload artifact
rem appveyor PushArtifact unrealircd-dev-build.exe
rem if %ERRORLEVEL% NEQ 0 EXIT /B 1

cd ..\unrealircd-tests
dir

rem All tests except db:
"C:\Program Files\Git\bin\bash.exe" ./runwin
if %ERRORLEVEL% NEQ 0 EXIT /B 1

rem Test unencrypted db's:
"C:\Program Files\Git\bin\bash.exe" ./runwin -boot tests/db/writing/*
if %ERRORLEVEL% NEQ 0 EXIT /B 1
"C:\Program Files\Git\bin\bash.exe" ./runwin -keepdbs -boot tests/db/reading/*
if %ERRORLEVEL% NEQ 0 EXIT /B 1

rem Test encrypted db's:
"C:\Program Files\Git\bin\bash.exe" ./runwin -include db_crypted.conf -boot tests/db/writing/*
if %ERRORLEVEL% NEQ 0 EXIT /B 1
"C:\Program Files\Git\bin\bash.exe" ./runwin -include db_crypted.conf -keepdbs -boot tests/db/reading/*
if %ERRORLEVEL% NEQ 0 EXIT /B 1

goto end



:installerfailed
type setup.log
echo INSTALLATION FAILED
EXIT /B 1

:end
