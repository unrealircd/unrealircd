; UnrealIRCd Win32 Installation Script
; Requires Inno Setup 4.1.6 or later

; Uncomment the line below to package with libcurl support
#define USE_CURL

[Setup]
AppName=UnrealIRCd 4
AppVerName=UnrealIRCd 4.0.2
AppPublisher=UnrealIRCd Team
AppPublisherURL=https://www.unrealircd.org
AppSupportURL=https://www.unrealircd.org
AppUpdatesURL=https://www.unrealircd.org
AppMutex=UnrealMutex,Global\UnrealMutex
DefaultDirName={pf}\UnrealIRCd 4
DefaultGroupName=UnrealIRCd
AllowNoIcons=yes
LicenseFile=src\win32\gplplusssl.rtf
Compression=lzma
SolidCompression=true
MinVersion=5.0
OutputDir=.
SourceDir=../../

; !!! Make sure to update SSL validation (WizardForm.TasksList.Checked[9]) if tasks are added/removed !!!
[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "installservice"; Description: "Install as a &service (not for beginners)"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,4.0
Name: "installservice/startboot"; Description: "S&tart UnrealIRCd when Windows starts"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/startdemand"; Description: "Start UnrealIRCd on &request"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/crashrestart"; Description: "Restart UnrealIRCd if it &crashes"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,5.0;
Name: "makecert"; Description: "&Create certificate"; GroupDescription: "SSL options:";
Name: "enccert"; Description: "&Encrypt certificate"; GroupDescription: "SSL options:"; Flags: unchecked;
Name: "fixperm"; Description: "Make UnrealIRCd folder writable by current user";

[Files]
Source: "UnrealIRCd.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "UnrealIRCd.pdb"; DestDir: "{app}"; Flags: ignoreversion
Source: ".CHANGES.NEW"; DestDir: "{app}"; DestName: "CHANGES.NEW.txt";Flags: ignoreversion
Source: "doc\RELEASE-NOTES"; DestDir: "{app}"; DestName: "RELEASE.NOTES.txt"; Flags: ignoreversion

Source: "doc\conf\*.conf"; DestDir: "{app}\conf"; Flags: ignoreversion
Source: "doc\conf\aliases\*.conf"; DestDir: "{app}\conf\aliases"; Flags: ignoreversion
Source: "doc\conf\help\*.conf"; DestDir: "{app}\conf\help"; Flags: ignoreversion
Source: "doc\conf\examples\*.conf"; DestDir: "{app}\conf\examples"; Flags: ignoreversion

Source: "doc\Donation"; DestDir: "{app}"; DestName: "Donation.txt"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

Source: "doc\*.*"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "doc\technical\*.*"; DestDir: "{app}\doc\technical"; Flags: ignoreversion
Source: "doc\conf\aliases\*"; DestDir: "{app}\conf\aliases"; Flags: ignoreversion

Source: "unrealsvc.exe"; DestDir: "{app}"; Flags: ignoreversion; MinVersion: 0,4.0

Source: "src\win32\makecert.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "src\win32\encpem.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "src\ssl.cnf"; DestDir: "{app}"; Flags: ignoreversion

Source: "src\modules\*.dll"; DestDir: "{app}\modules"; Flags: ignoreversion
Source: "src\modules\chanmodes\*.dll"; DestDir: "{app}\modules\chanmodes"; Flags: ignoreversion
Source: "src\modules\usermodes\*.dll"; DestDir: "{app}\modules\usermodes"; Flags: ignoreversion
Source: "src\modules\snomasks\*.dll"; DestDir: "{app}\modules\snomasks"; Flags: ignoreversion
Source: "src\modules\extbans\*.dll"; DestDir: "{app}\modules\extbans"; Flags: ignoreversion

Source: "c:\dev\tre\win32\release\tre.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\dev\pcre2\build\release\pcre2-8.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\dev\c-ares\msvc90\cares\dll-release\cares.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\openssl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\ssleay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\openssl\bin\libeay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "c:\dev\setacl.exe"; DestDir: "{app}\tmp"; Flags: ignoreversion

#ifdef USE_CURL
; curl with ssl support
Source: "c:\dev\curl-ssl\builds\libcurl-vc-x86-release-dll-sspi\bin\libcurl.dll"; DestDir: "{app}"; Flags: ignoreversion
#endif

[Dirs]
Name: "{app}\tmp"
Name: "{app}\cache"
Name: "{app}\logs"
Name: "{app}\conf"
Name: "{app}\conf\ssl"
Name: "{app}\data"
Name: "{app}\modules\third"

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
         and (not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{BD95A8CD-1D9F-35AD-981A-3E7925026EBB}'))
        ) then
    begin
      MsgBox('UnrealIRCd requires the Microsoft Visual C++ Redistributable for Visual Studio 2012 to be installed.' #13 +
             'After you click OK you will be taken to a download page. There, click Download and choose the vcredist_x86 version. Then download and install it.' #13 +
             'If you are already absolutely sure that you have this package installed then you can skip this step.', mbInformation, MB_OK);
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
	     // This fixes the permissions in the UnrealIRCd folder by granting full access to the user
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

procedure CurPageChanged(CurPage: Integer);
begin
  if (CurPage = wpSelectTasks)then
  begin
     if FileExists(ExpandConstant('{app}\conf\ssl\server.cert.pem')) then
     begin
        WizardForm.TasksList.Checked[9]:=false;
     end
     else
     begin
        WizardForm.TasksList.Checked[9]:=true;
     end
  end
end;

[Icons]
Name: "{group}\UnrealIRCd"; Filename: "{app}\UnrealIRCd.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall UnrealIRCd"; Filename: "{uninstallexe}"; WorkingDir: "{app}"
Name: "{group}\Make Certificate"; Filename: "{app}\makecert.bat"; WorkingDir: "{app}"
Name: "{group}\Encrypt Certificate"; Filename: "{app}\encpem.bat"; WorkingDir: "{app}"
Name: "{group}\Documentation"; Filename: "https://www.unrealircd.org/docs/UnrealIRCd_4_documentation"; WorkingDir: "{app}"
Name: "{userdesktop}\UnrealIRCd"; Filename: "{app}\UnrealIRCd.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\UnrealIRCd"; Filename: "{app}\UnrealIRCd.exe"; WorkingDir: "{app}"; Tasks: quicklaunchicon

[Run]
;Filename: "notepad"; Description: "View example.conf"; Parameters: "{app}\conf\examples\example.conf"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "https://www.unrealircd.org/docs/UnrealIRCd_4_documentation"; Description: "View documentation"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "https://www.unrealircd.org/docs/Installing_%28Windows%29"; Description: "View installation instructions"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Release Notes"; Parameters: "{app}\RELEASE.NOTES.txt"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\unrealsvc.exe"; Parameters: "install"; Flags: runminimized nowait; Tasks: installservice
Filename: "{app}\unrealsvc.exe"; Parameters: "config startup manual"; Flags: runminimized nowait; Tasks: installservice/startdemand
Filename: "{app}\unrealsvc.exe"; Parameters: "config startup auto"; Flags: runminimized nowait; Tasks: installservice/startboot
Filename: "{app}\unrealsvc.exe"; Parameters: "config crashrestart 2"; Flags: runminimized nowait; Tasks: installservice/crashrestart
Filename: "{app}\makecert.bat"; Tasks: makecert; Flags: postinstall;
Filename: "{app}\encpem.bat"; WorkingDir: "{app}"; Tasks: enccert; Flags: postinstall;

[UninstallRun]
Filename: "{app}\unrealsvc.exe"; Parameters: "uninstall"; Flags: runminimized; RunOnceID: "DelService"; Tasks: installservice
