; UnrealIRCd Win32 Installation Script for My Inno Setup Extensions
; Requires ISX 3.0.4 to work

; #define USE_SSL
; Uncomment the above line to package an SSL build
#define USE_ZIP
; Uncomment the above line to package with ZIP support


[Setup]
AppName=UnrealIRCd
AppVerName=UnrealIRCd3.2-beta18
AppPublisher=UnrealIRCd Team
AppPublisherURL=http://www.unrealircd.com
AppSupportURL=http://www.unrealircd.com
AppUpdatesURL=http://www.unrealircd.com
AppMutex=UnrealMutex,Global\UnrealMutex
DefaultDirName={pf}\Unreal3.2
DefaultGroupName=UnrealIRCd
AllowNoIcons=yes
#ifndef USE_SSL
LicenseFile=.\gpl.rtf
#else
LicenseFile=.\gplplusssl.rtf
#endif
Compression=bzip/9
MinVersion=4.0.1111,4.0.1381
OutputDir=../../

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "installservice"; Description: "Install as a &service (not for beginners)"; GroupDescription: "Service support:"; MinVersion: 0,4.0
Name: "installservice/startboot"; Description: "S&tart UnrealIRCd when Windows starts"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive
Name: "installservice/startdemand"; Description: "Start UnrealIRCd on &request"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive
Name: "installservice/crashrestart"; Description: "Restart UnrealIRCd if it &crashes"; GroupDescription: "Service support:"; MinVersion: 0,5.0;
#ifdef USE_SSL
Name: "makecert"; Description: "&Create certificate"; GroupDescription: "SSL options:";
Name: "enccert"; Description: "&Encrypt certificate"; GroupDescription: "SSL options:"; Flags: unchecked;
#endif

[Files]
Source: "..\..\wircd.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\WIRCD.pdb"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\.CHANGES.NEW"; DestDir: "{app}"; DestName: "CHANGES.NEW.txt";Flags: ignoreversion
Source: "..\..\.CONFIG.RANT"; DestDir: "{app}"; DestName: "CONFIG.RANT.txt"; Flags: ignoreversion
Source: "..\..\.RELEASE.NOTES"; DestDir: "{app}"; DestName: "RELEASE.NOTES.txt"; Flags: ignoreversion
Source: "..\..\.SICI"; DestDir: "{app}"; DestName: "SICI.txt"; Flags: ignoreversion
Source: "..\..\badwords.channel.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\badwords.message.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\Changes"; DestDir: "{app}"; DestName: "Changes.txt"; Flags: ignoreversion
Source: "..\..\Changes.old"; DestDir: "{app}"; DestName: "Changes.old.txt"; Flags: ignoreversion
Source: "..\..\Donation"; DestDir: "{app}"; DestName: "Donation.txt"; Flags: ignoreversion
Source: "..\..\help.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\..\Unreal.nfo"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\doc\*.*"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "..\..\aliases\*"; DestDir: "{app}\aliases"; Flags: ignoreversion
Source: "..\..\networks\*"; DestDir: "{app}\networks"; Flags: ignoreversion
Source: "..\..\unreal.exe"; DestDir: "{app}"; Flags: ignoreversion; MinVersion: 0,4.0
Source: "tre.dll"; DestDir: "{app}"; Flags: ignoreversion
#ifdef USE_SSL
Source: "c:\openssl\bin\openssl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\ssleay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\libeay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\makecert.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\encpem.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\ssl.cnf"; DestDir: "{app}"; Flags: ignoreversion
#endif
#ifdef USE_ZIP
Source: "c:\dev\zlib\dll32\zlib.dll"; DestDir: "{app}"; Flags: ignoreversion
#endif
Source: isxdl.dll; DestDir: {tmp}; Flags: dontcopy

[UninstallDelete]
Type: files; Name: "{app}\DbgHelp.Dll"

[Code]
function isxdl_Download(hWnd: Integer; URL, Filename: PChar): Integer;
external 'isxdl_Download@files:isxdl.dll stdcall';
function isxdl_SetOption(Option, Value: PChar): Integer;
external 'isxdl_SetOption@files:isxdl.dll stdcall';
const url = 'http://www.unrealircd.com/downloads/DbgHelp.Dll';
function NextButtonClick(CurPage: Integer): Boolean;
var
dbghelp,tmp,output: String;
m: String;
hWnd,answer: Integer;
begin
  dbghelp := ExpandConstant('{sys}\DbgHelp.Dll');
  output := ExpandConstant('{app}\DbgHelp.Dll');
  GetVersionNumbersString(dbghelp,m);
  if ((CurPage = wpReady) AND NOT FileExists(output)) then begin
    if (NOT FileExists(dbghelp)) then
      m := StringOfChar('0',1);
    if (StrToInt(m[1]) < 5) then begin
     answer := MsgBox('DbgHelp.dll version 5.0 or higher is required to install Unreal, do you wish to install it now?', mbConfirmation, MB_YESNO);
     if answer = IDYES then begin
      tmp := ExpandConstant('{tmp}\dbghelp.dll');
      isxdl_SetOption('title', 'Downloading DbgHelp.dll');
      hWnd := StrToInt(ExpandConstant('{wizardhwnd}'));
      if isxdl_Download(hWnd, url, tmp) = 0 then
         MsgBox('Download and installation of DbgHelp.Dll failed, the file must be manually installed. The file can be downloaded at http://www.unrealircd.com/downloads/DbgHelp.Dll', mbInformation, MB_OK);
     end else
       MsgBox('In order for Unreal to properly function you must manually install this dll. The dll can be downloaded from http://www.unrealircd.com/downloads/DbgHelp.Dll', mbInformation, MB_OK);
    end;
  end;
  Result := true;
end;

procedure DeInitializeSetup();
var
input,output: String;
begin
  input := ExpandConstant('{tmp}\dbghelp.dll');
  output := ExpandConstant('{app}\dbghelp.dll');
  FileCopy(input, output, true);
end;

[Icons]
Name: "{group}\UnrealIRCd"; Filename: "{app}\wircd.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall UnrealIRCd"; Filename: "{uninstallexe}"; WorkingDir: "{app}"
#ifdef USE_SSL
Name: "{group}\Make Certificate"; Filename: "{app}\makecert.bat"; WorkingDir: "{app}"
Name: "{group}\Encrypt Certificate"; Filename: "{app}\encpem.bat"; WorkingDir: "{app}"
#endif
Name: "{group}\Documentation"; Filename: "{app}\doc\unreal32docs.html"; WorkingDir: "{app}"
Name: "{userdesktop}\UnrealIRCd"; Filename: "{app}\wircd.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\UnrealIRCd"; Filename: "{app}\wircd.exe"; WorkingDir: "{app}"; Tasks: quicklaunchicon

[Run]
Filename: "notepad"; Description: "View example.conf"; Parameters: "{app}\doc\example.conf"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\doc\unreal32docs.html"; Description: "View UnrealIRCd documentation"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Release Notes"; Parameters: "{app}\RELEASE.NOTES.txt"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Changes"; Parameters: "{app}\Changes.txt"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\unreal.exe"; Parameters: "install"; Flags: runminimized nowait; Tasks: installservice
Filename: "{app}\unreal.exe"; Parameters: "config startup manual"; Flags: runminimized nowait; Tasks: installservice/startdemand
Filename: "{app}\unreal.exe"; Parameters: "config startup auto"; Flags: runminimized nowait; Tasks: installservice/startboot
Filename: "{app}\unreal.exe"; Parameters: "config crashrestart 2"; Flags: runminimized nowait; Tasks: installservice/crashrestart
#ifdef USE_SSL
Filename: "{app}\makecert.bat"; Tasks: makecert
Filename: "{app}\encpem.bat"; WorkingDir: "{app}"; Tasks: enccert
#endif

[UninstallRun]
Filename: "{app}\unreal.exe"; Parameters: "uninstall"; Flags: runminimized; RunOnceID: "DelService"; Tasks: installservice

