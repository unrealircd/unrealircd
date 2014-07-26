; UnrealIRCd Win32 Installation Script for My Inno Setup Extensions
; Requires Inno Setup 4.1.6 or later

;#define USE_SSL
; Uncomment the above line to package an SSL build
#define USE_ZIP
; Uncomment the above line to package with ZIP support
#define USE_CURL
; Uncomment the above line to package with libcurl support


[Setup]
AppName=UnrealIRCd
AppVerName=UnrealIRCd3.2.10.4
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
Compression=lzma
SolidCompression=true
MinVersion=5.0
OutputDir=../../

; !!! Make sure to update SSL validation (WizardForm.TasksList.Checked[9]) if tasks are added/removed !!!
[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "installservice"; Description: "Install as a &service (not for beginners)"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,4.0
Name: "installservice/startboot"; Description: "S&tart UnrealIRCd when Windows starts"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/startdemand"; Description: "Start UnrealIRCd on &request"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/crashrestart"; Description: "Restart UnrealIRCd if it &crashes"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,5.0;
#ifdef USE_SSL
Name: "makecert"; Description: "&Create certificate"; GroupDescription: "SSL options:";
Name: "enccert"; Description: "&Encrypt certificate"; GroupDescription: "SSL options:"; Flags: unchecked;
#endif
Name: "fixperm"; Description: "Make Unreal folder writable by current user";

[Files]
Source: "..\..\wircd.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\WIRCD.pdb"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\.CHANGES.NEW"; DestDir: "{app}"; DestName: "CHANGES.NEW.txt";Flags: ignoreversion
Source: "..\..\.CONFIG.RANT"; DestDir: "{app}"; DestName: "CONFIG.RANT.txt"; Flags: ignoreversion
Source: "..\..\.RELEASE.NOTES"; DestDir: "{app}"; DestName: "RELEASE.NOTES.txt"; Flags: ignoreversion
Source: "..\..\.SICI"; DestDir: "{app}"; DestName: "SICI.txt"; Flags: ignoreversion
Source: "..\..\badwords.channel.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\badwords.message.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\badwords.quit.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\spamfilter.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\dccallow.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\Changes"; DestDir: "{app}"; DestName: "Changes.txt"; Flags: ignoreversion
Source: "..\..\Changes.old"; DestDir: "{app}"; DestName: "Changes.old.txt"; Flags: ignoreversion
Source: "..\..\Donation"; DestDir: "{app}"; DestName: "Donation.txt"; Flags: ignoreversion
Source: "..\..\help.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\..\Unreal.nfo"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\doc\*.*"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "..\..\doc\technical\*.*"; DestDir: "{app}\doc\technical"; Flags: ignoreversion
Source: "..\..\aliases\*"; DestDir: "{app}\aliases"; Flags: ignoreversion
Source: "..\..\unreal.exe"; DestDir: "{app}"; Flags: ignoreversion; MinVersion: 0,4.0
Source: "..\modules\*.dll"; DestDir: "{app}\modules"; Flags: ignoreversion
Source: "c:\dev\tre\win32\release\tre.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\c-ares\msvc90\cares\dll-release\cares.dll"; DestDir: "{app}"; Flags: ignoreversion
#ifdef USE_SSL
Source: "c:\openssl\bin\openssl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\ssleay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\libeay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\dev\setacl.exe"; DestDir: "{app}\tmp"; Flags: ignoreversion
Source: ".\makecert.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\encpem.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\ssl.cnf"; DestDir: "{app}"; Flags: ignoreversion
#endif
#ifdef USE_ZIP
Source: "c:\dev\zlib\zlibwapi.dll"; DestDir: "{app}"; Flags: ignoreversion
#endif
#ifdef USE_SSL
#ifdef USE_CURL
; curl with ssl support
Source: "C:\dev\curl-ssl\builds\libcurl-vc-x86-release-dll-sspi-spnego\bin\libcurl.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\curl-ca-bundle.crt"; DestDir: "{app}"; Flags: ignoreversion
#endif
#else
#ifdef USE_CURL
; curl without ssl support
Source: "C:\dev\curl\builds\libcurl-vc-x86-release-dll-sspi-spnego\bin\libcurl.dll"; DestDir: "{app}"; Flags: ignoreversion
#endif
#endif
;Source: "..\..\..\dbghelp.dll"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{app}\tmp"

[UninstallDelete]
Type: files; Name: "{app}\DbgHelp.Dll"

[Code]
var
  uninstaller: String;
  ErrorCode: Integer;

//*********************************************************************************
// This is where all starts.
//*********************************************************************************
function InitializeSetup(): Boolean;

begin

	Result := true;
    if ((not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{2F73A7B2-E50E-39A6-9ABC-EF89E4C62E36}'))
         and (not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{E824E81C-80A4-3DFF-B5F9-4842A9FF5F7F}'))
         and (not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{E7D4E834-93EB-351F-B8FB-82CDAE623003}'))
         and (not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{3D6AD258-61EA-35F5-812C-B7A02152996E}'))
        ) then
    begin
      MsgBox('UnrealIRCd requires the Microsoft Visual C++ Redistributable for Visual Studio 2012 to be installed.' #13 +
             'After you click OK you will be taken to a download page. There, choose Download -> choose the vcredist_x86 version (last of 3 choices). Then download and install it.', mbInformation, MB_OK);
      ShellExec('open', 'http://www.microsoft.com/en-us/download/details.aspx?id=30679', '', '', SW_SHOWNORMAL,ewNoWait,ErrorCode);
      MsgBox('Click OK once you have installed the Microsoft Visual C++ Redistributable for Visual Studio 2012 (vcredist_x86) to continue the UnrealIRCd installer', mbInformation, MB_OK);

		end;
end;

function NextButtonClick(CurPage: Integer): Boolean;

var
  hWnd: Integer;
  ResultCode: Integer;
  ResultXP: boolean;
  Result2003: boolean;
  Res: Integer;
begin

  Result := true;
  ResultXP := true;
  Result2003 := true;

  // Prevent the user from selecting both 'Install as service' and 'Encrypt SSL certificate'
  if CurPage = wpSelectTasks then
  begin
    if IsTaskSelected('enccert') and IsTaskSelected('installservice') then
    begin
      MsgBox('When running UnrealIRCd as a service there is no way to enter the password for an encrypted SSL certificate, therefore you cannot combine the two. Please deselect one of the options.', mbError, MB_OK);
      Result := False
    end
  end;

end;

procedure CurStepChanged(CurStep: TSetupStep);

var
  hWnd: Integer;
  ResultCode: Integer;
  ResultXP: boolean;
  Result2003: boolean;
  Res: Integer;
  s: String;
  d: String;
begin
if CurStep = ssPostInstall then
	begin
     d := ExpandConstant('{app}');
	   if IsTaskSelected('fixperm') then
	   begin
	     // This fixes the permissions in the Unreal3.2 folder by granting full access to the user
	     // running the install.
	     s := '-on "'+d+'" -ot file -actn ace -ace "n:'+GetUserNameString()+';p:full;m:set';
	     Exec(d+'\tmp\setacl.exe', s, d, SW_HIDE, ewWaitUntilTerminated, Res);
	   end
	   else
	   begin
	     MsgBox('You have chosen to not have the installer automatically set write access. Please ensure that the user running the IRCd can write to '+d+', otherwise the IRCd will fail to load.',mbConfirmation, MB_OK);
	   end
  end;
end;

//*********************************************************************************
// Checks if ssl cert file exists
//*********************************************************************************

#ifdef USE_SSL
procedure CurPageChanged(CurPage: Integer);
begin
  if (CurPage = wpSelectTasks)then
  begin
     if FileExists(ExpandConstant('{app}\server.cert.pem')) then
     begin
        WizardForm.TasksList.Checked[9]:=false;
     end
     else
     begin
        WizardForm.TasksList.Checked[9]:=true;
     end
  end
end;
#endif

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
Filename: "{app}\makecert.bat"; Tasks: makecert; Flags: postinstall;
Filename: "{app}\encpem.bat"; WorkingDir: "{app}"; Tasks: enccert; Flags: postinstall;
#endif

[UninstallRun]
Filename: "{app}\unreal.exe"; Parameters: "uninstall"; Flags: runminimized; RunOnceID: "DelService"; Tasks: installservice
