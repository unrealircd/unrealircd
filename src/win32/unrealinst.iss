; UnrealIRCd Win32 Installation Script for My Inno Setup Extensions

[Setup]
AppName=UnrealIRCd
AppVerName=UnrealIRCd3.2-beta11
AppPublisher=UnrealIRCd Team
AppPublisherURL=http://www.unrealircd.com
AppSupportURL=http://www.unrealircd.com
AppUpdatesURL=http://www.unrealircd.com
AppMutex=UnrealMutex
DefaultDirName={pf}\Unreal3.2
DefaultGroupName=UnrealIRCd
AllowNoIcons=yes
LicenseFile=.\gpl.rtf
Compression=bzip/9
MinVersion=4.0.1111,4.0.1381
OutputDir=../../

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "installservice"; Description: "Install &Service"; GroupDescription: "Service support:"; MinVersion: 0,4.0

[Files]
Source: "..\..\wircd.exe"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\WIRCD.pdb"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.CHANGES.NEW"; DestDir: "{app}"; DestName: "CHANGES.NEW.txt";CopyMode: alwaysoverwrite
Source: "..\..\.CONFIG.RANT"; DestDir: "{app}"; DestName: "CONFIG.RANT.txt"; CopyMode: alwaysoverwrite
Source: "..\..\.RELEASE.NOTES"; DestDir: "{app}"; DestName: "RELEASE.NOTES.txt"; CopyMode: alwaysoverwrite
Source: "..\..\.SICI"; DestDir: "{app}"; DestName: "SICI.txt"; CopyMode: alwaysoverwrite
Source: "..\..\badwords.channel.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\badwords.message.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Changes"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Changes.old"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Donation"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: ".\gnu_regex.dll"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\help.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; CopyMode: alwaysoverwrite
Source: "..\..\Unreal.nfo"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\doc\*.*"; DestDir: "{app}\doc"; CopyMode: alwaysoverwrite
Source: "..\..\aliases\*"; DestDir: "{app}\aliases"; CopyMode: alwaysoverwrite
Source: "..\..\networks\*"; DestDir: "{app}\networks"; CopyMode: alwaysoverwrite
Source: "..\..\unreal.exe"; DestDir: "{app}"; CopyMode: alwaysoverwrite; MinVersion: 0,4.0
Source: isxdl.dll; DestDir: {tmp}; CopyMode: dontcopy

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
Name: "{group}\Documentation"; Filename: "{app}\doc\unreal32docs.html"; WorkingDir: "{app}"
Name: "{userdesktop}\UnrealIRCd"; Filename: "{app}\wircd.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\UnrealIRCd"; Filename: "{app}\wircd.exe"; WorkingDir: "{app}"; Tasks: quicklaunchicon

[Run]
Filename: "notepad"; Description: "View example.conf"; Parameters: "{app}\doc\example.conf"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\doc\unreal32docs.html"; Description: "View UnrealIRCd documentation"; Parameters: ""; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Release Notes"; Parameters: "{app}\RELEASE.NOTES.txt"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Changes"; Parameters: "{app}\Changes"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "{app}\unreal.exe"; Parameters: "install"; Flags: runminimized nowait; Tasks: installservice

[UninstallRun]
Filename: "{app}\unreal.exe"; Parameters: "uninstall"; Flags: runminimized; RunOnceID: "DelService"; Tasks: installservice

