; UnrealIRCd Win32 Installation Script for My Inno Setup Extensions

[Setup]
AppName=UnrealIRCd
AppVerName=UnrealIRCd3.2-beta10
AppPublisher=UnrealIRCd Team
AppPublisherURL=http://www.unrealircd.com
AppSupportURL=http://www.unrealircd.com
AppUpdatesURL=http://www.unrealircd.com
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

[Files]
Source: "..\..\wircd.exe"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\WIRCD.pdb"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.CHANGES.NEW"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.CONFIG.RANT"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.NEW_CONFIG"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.RELEASE.NOTES"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.SICI"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\.UPDATE"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\badwords.channel.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\badwords.message.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Changes"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Changes.old"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Donation"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: ".\gnu_regex.dll"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\help.conf"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\LICENSE"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: ".\debug\StackTrace.dll"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\Unreal.nfo"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "..\..\doc\*.*"; DestDir: "{app}\doc"; CopyMode: alwaysoverwrite
Source: "..\..\aliases\*"; DestDir: "{app}\aliases"; CopyMode: alwaysoverwrite
Source: "..\..\networks\*"; DestDir: "{app}\networks"; CopyMode: alwaysoverwrite
Source: "..\..\unreal.exe"; DestDir: "{app}"; CopyMode: alwaysoverwrite; MinVersion: 0,4.0
[Icons]
Name: "{group}\UnrealIRCd"; Filename: "{app}\wircd.exe"
Name: "{group}\Uninstall UnrealIRCd"; Filename: "{uninstallexe}"
Name: "{userdesktop}\UnrealIRCd"; Filename: "{app}\wircd.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\UnrealIRCd"; Filename: "{app}\wircd.exe"; Tasks: quicklaunchicon

[Run]
Filename: "notepad"; Description: "View example.conf"; Parameters: "{app}\doc\example.conf"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View conf.doc"; Parameters: "{app}\doc\conf.doc"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Release Notes"; Parameters: "{app}\.RELEASE.NOTES"; Flags: postinstall skipifsilent shellexec runmaximized
Filename: "notepad"; Description: "View Changes"; Parameters: "{app}\Changes"; Flags: postinstall skipifsilent shellexec runmaximized
