; UnrealIRCd Windows Installation Script
; Requires Inno Setup 4.1.6 or later

; Uncomment the line below to package with libcurl support
#define USE_CURL

[Setup]
AppName=UnrealIRCd 6
AppVerName=UnrealIRCd 6.1.3-rc1
AppPublisher=UnrealIRCd Team
AppPublisherURL=https://www.unrealircd.org
AppSupportURL=https://www.unrealircd.org
AppUpdatesURL=https://www.unrealircd.org
AppMutex=UnrealMutex,Global\UnrealMutex
DefaultDirName={pf}\UnrealIRCd 6
DefaultGroupName=UnrealIRCd 6
AllowNoIcons=yes
LicenseFile=src\windows\gplplusssl.rtf
Compression=lzma
SolidCompression=true
MinVersion=6.1
OutputDir=.
SourceDir=../../
UninstallDisplayIcon={app}\bin\UnrealIRCd.exe
UninstallFilesDir={app}\bin\uninstaller
DisableWelcomePage=no
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
;These are set only on release:
;SignedUninstaller=yes
;SignTool=signtool

; !!! Make sure to update TLS validation (WizardForm.TasksList.Checked[9]) if tasks are added/removed !!!
[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "installservice"; Description: "Install as a &service (not for beginners)"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,4.0
Name: "installservice/startboot"; Description: "S&tart UnrealIRCd when Windows starts"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/startdemand"; Description: "Start UnrealIRCd on &request"; GroupDescription: "Service support:"; MinVersion: 0,4.0; Flags: exclusive unchecked
Name: "installservice/crashrestart"; Description: "Restart UnrealIRCd if it &crashes"; GroupDescription: "Service support:"; Flags: unchecked; MinVersion: 0,5.0;
Name: "makecert"; Description: "&Create certificate"; GroupDescription: "TLS options:";
Name: "fixperm"; Description: "Make UnrealIRCd folder writable by current user";

[Files]
; UnrealIRCd binaries
Source: "UnrealIRCd.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "UnrealIRCd.pdb"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "unrealircdctl.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "unrealsvc.exe";  DestDir: "{app}\bin"; Flags: ignoreversion signonce

; TLS certificate generation helpers
Source: "src\windows\makecert.bat"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "extras\tls.cnf"; DestDir: "{app}\bin"; Flags: ignoreversion

; UnrealIRCd modules
Source: "src\modules\*.dll"; DestDir: "{app}\modules"; Flags: ignoreversion signonce
Source: "src\modules\chanmodes\*.dll"; DestDir: "{app}\modules\chanmodes"; Flags: ignoreversion signonce
Source: "src\modules\usermodes\*.dll"; DestDir: "{app}\modules\usermodes"; Flags: ignoreversion signonce
Source: "src\modules\extbans\*.dll"; DestDir: "{app}\modules\extbans"; Flags: ignoreversion signonce
Source: "src\modules\rpc\*.dll"; DestDir: "{app}\modules\rpc"; Flags: ignoreversion signonce
Source: "src\modules\third\*.dll"; DestDir: "{app}\modules\third"; Flags: ignoreversion skipifsourcedoesntexist signonce

; Libraries
Source: "c:\dev\unrealircd-6-libs\pcre2\bin\pcre*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\argon2\vs2015\build\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\libsodium\bin\x64\Release\v142\dynamic\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\jansson\bin\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\c-ares\msvc\cares\dll-release\cares.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\libressl\bin\openssl.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\libressl\bin\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\GeoIP\libGeoIP\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "c:\dev\unrealircd-6-libs\setacl.exe"; DestDir: "{app}\tmp"; Flags: ignoreversion signonce
#ifdef USE_CURL
Source: "c:\dev\unrealircd-6-libs\curl\builds\libcurl-vc-x64-release-dll-ssl-dll-cares-dll-ipv6-obj-lib\libcurl.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
#endif
Source: "doc\conf\tls\curl-ca-bundle.crt"; DestDir: "{app}\conf\tls"; Flags: ignoreversion

; Config files
Source: "doc\conf\*.default.conf"; DestDir: "{app}\conf"; Flags: ignoreversion
Source: "doc\conf\*.optional.conf"; DestDir: "{app}\conf"; Flags: ignoreversion
Source: "doc\conf\spamfilter.conf"; DestDir: "{app}\conf"; Flags: onlyifdoesntexist
Source: "doc\conf\badwords.conf"; DestDir: "{app}\conf"; Flags: onlyifdoesntexist
Source: "doc\conf\dccallow.conf"; DestDir: "{app}\conf"; Flags: onlyifdoesntexist
Source: "doc\conf\aliases\*.conf"; DestDir: "{app}\conf\aliases"; Flags: ignoreversion
Source: "doc\conf\help\*.conf"; DestDir: "{app}\conf\help"; Flags: ignoreversion
Source: "doc\conf\examples\*.conf"; DestDir: "{app}\conf\examples"; Flags: ignoreversion

; Documentation etc.
Source: "doc\Donation"; DestDir: "{app}\doc"; DestName: "Donation.txt"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}\doc"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "doc\*.*"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "doc\technical\*.*"; DestDir: "{app}\doc\technical"; Flags: ignoreversion

[Dirs]
Name: "{app}\tmp"
Name: "{app}\bin"
Name: "{app}\cache"
Name: "{app}\logs"
Name: "{app}\conf"
Name: "{app}\conf\tls"
Name: "{app}\data"
Name: "{app}\modules\third"

[Code]
var
	uninstaller: String;
	ErrorCode: Integer;

//*********************************************************************************
// This is where all starts.
//*********************************************************************************
function InitializeSetup(): Boolean;
var
	major: Cardinal;
begin
	Result := true;

	if Not RegQueryDWordValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Major', major) then
	begin
		MsgBox('UnrealIRCd requires the Microsoft Visual C++ Redistributable for Visual Studio 2019 to be installed.' #13 +
		       'After you click OK you will be taken to a download page from Microsoft:' #13 +
		       '1) Scroll down to the section "Visual Studio 2015, 2017 and 2019"' #13 +
		       '2) Click on the x64 "vc_redist.x64.exe" to download the 64 bit installer' #13 +
		       '3) Run the installer.' #13 + #13 +
		       'If you are already absolutely sure that you have this package installed then you can skip this step.', mbInformation, MB_OK);
		ShellExec('open', 'https://support.microsoft.com/help/2977003/the-latest-supported-visual-c-downloads', '', '', SW_SHOWNORMAL,ewNoWait,ErrorCode);
		MsgBox('Your browser was launched. After you have installed the Microsoft Visual C++ Redistributable for Visual Studio 2019 (vc_redist.x64.exe), click OK below to continue the UnrealIRCd installer', mbInformation, MB_OK);
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
			s := '-on "'+d+'" -ot file -actn ace -ace "n:'+GetUserNameString()+';p:full;m:set"';
			Exec(d+'\tmp\setacl.exe', s, d, SW_HIDE, ewWaitUntilTerminated, Res);
		end
		else
		begin
			MsgBox('You have chosen to not have the installer automatically set write access. Please ensure that the user running the IRCd can write to '+d+', otherwise the IRCd will fail to load.',mbConfirmation, MB_OK);
		end;
		if IsTaskSelected('installservice') then
		begin
			// Similar to above, but this adds full access to NetworkService,
			// otherwise it cannot copy modules, cannot write to logs, etc etc.
			s := '-on "'+d+'" -ot file -actn ace -ace "n:NetworkService;p:full;m:set"';
			Exec(d+'\tmp\setacl.exe', s, d, SW_HIDE, ewWaitUntilTerminated, Res);
		end;
	end;
end;

//*********************************************************************************
// Checks if TLS cert file exists
//*********************************************************************************

procedure CurPageChanged(CurPage: Integer);
begin
	if (CurPage = wpSelectTasks) then
	begin
		if FileExists(ExpandConstant('{app}\conf\tls\server.cert.pem')) then
		begin
			WizardForm.TasksList.Checked[9]:=false;
		end
		else
		begin
			WizardForm.TasksList.Checked[9]:=true;
		end;
	end;
end;

[Icons]
Name: "{group}\UnrealIRCd"; Filename: "{app}\bin\UnrealIRCd.exe"; WorkingDir: "{app}\bin"
Name: "{group}\Uninstall UnrealIRCd"; Filename: "{uninstallexe}"; WorkingDir: "{app}\bin"
Name: "{group}\Make Certificate"; Filename: "{app}\bin\makecert.bat"; WorkingDir: "{app}\bin"
Name: "{group}\Documentation"; Filename: "https://www.unrealircd.org/docs/"; WorkingDir: "{app}\bin"
Name: "{userdesktop}\UnrealIRCd"; Filename: "{app}\bin\UnrealIRCd.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\UnrealIRCd"; Filename: "{app}\bin\UnrealIRCd.exe"; WorkingDir: "{app}\bin"; Tasks: quicklaunchicon

[Run]
Filename: "https://www.unrealircd.org/docs/"; Description: "View documentation"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "https://www.unrealircd.org/docs/Installing_%28Windows%29"; Description: "View installation instructions"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\bin\unrealsvc.exe"; Parameters: "install"; Flags: runminimized nowait; Tasks: installservice
Filename: "{app}\bin\unrealsvc.exe"; Parameters: "config startup manual"; Flags: runminimized nowait; Tasks: installservice/startdemand
Filename: "{app}\bin\unrealsvc.exe"; Parameters: "config startup auto"; Flags: runminimized nowait; Tasks: installservice/startboot
Filename: "{app}\bin\unrealsvc.exe"; Parameters: "config crashrestart 2"; Flags: runminimized nowait; Tasks: installservice/crashrestart
Filename: "{app}\bin\makecert.bat"; Tasks: makecert; Flags: postinstall;

[UninstallRun]
Filename: "{app}\bin\unrealsvc.exe"; Parameters: "uninstall"; Flags: runminimized; RunOnceID: "DelService"; Tasks: installservice
