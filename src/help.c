/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/help.c
 *   Copyright (C) 1996 DALnet
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Updated with the latest UnrealIRCd Commands ,
 * as of Unreal 3.1.1-DarkShades ..
 *
 * Last Modified : 28/11/2000
 * Version : v2.0
 *                       - hAtbLaDe
 *
*/

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "proto.h"

ID_Copyright("DALnet , Techie & hAtbLaDe");
ID_Notes("6.20 7/5/99");

void xx(sptr, str, num)
	aClient *sptr;
	char *str;
	int  num;
{
	if (sptr == NULL)
	{
		printf("%s\n", str);
		return;
	}
	sendto_one(sptr, ":%s %i %s :%s", me.name, num, sptr->name, str);
}

#define HDR(str) xx(sptr, str, 290)
#define SND(str) xx(sptr, str, 291)
#define FTR(str) xx(sptr, str, 292)
#define HLP(str) xx(sptr, str, 293)

extern struct Message msgtab[];
int  parse_help(sptr, name, help)
	aClient *sptr;
	char *name;
	char *help;
{
	int  i;

	if (BadPtr(help))
	{
      SND(" -");
      HDR("        ***** UnrealIRCd Help System *****");
      SND(" -");
      SND(" Specify your Question after the /HELPOP command.");
      SND(" If the request cannot be satisfied by the Server,");
      SND(" it will be forwarded to the appropriate Help Operators");
      SND(" A prefix of ! will send the Question directly to Help Ops");
      SND(" and a prefix of ? will force it to be queried only within");
      SND(" the UnrealIRCd Help System , without forwarding it");
      SND(" -");
      SND(" /HELPOP USERCMDS - To get the list of User Commands");
      SND(" /HELPOP OPERCMDS - To get the list of Oper Commands");
      SND(" /HELPOP SVSCMDS  - Commands sent via U:Lined Server (Services)");
      SND(" /HELPOP UMODES   - To get the list of User Modes");
      SND(" /HELPOP CHMODES  - To get the list of Channel Modes");
      SND(" /HELPOP OFLAGS   - To get the list of O:Line flags");
      SND(" /HELPOP COMMANDS - To get the whole list of commands with Tokens");
      SND(" /HELPOP ABOUT    - Some more info on the Help System");
      SND(" -");
      SND(" -------------------------oOo--------------------------");
      SND(" -");
	}
	else if (!myncmp(help, "USERCMDS", 8))
   {
      SND(" -");
      HDR("          *** User Commands List *** ");
      SND(" -");
      SND(" Currently the following User commands are available.");
      SND(" Use /HELPOP <command name> to get info about the command.");
      SND(" -");
      SND(" -------------------oOo--*----------------");
      SND(" NICK    WHOIS   WHO     WHOWAS   NAMES");
      SND(" ISON    JOIN    PART    MOTD     RULES");
      SND(" LUSERS  MAP     QUIT    PING     VERSION");
      SND(" STATS   LINKS   ADMIN   USERHOST TOPIC");
      SND(" INVITE  KICK    AWAY    WATCH    LIST");
      SND(" PRIVMSG NOTICE  KNOCK   SETNAME  VHOST");
      SND(" MODE    CREDITS DALINFO LICENSE  TIME");
      SND(" BOTMOTD SILENCE PONG    IDENTIFY");
      SND(" -------------------oOo-------------------");
      SND(" -");
	}
	else if (!myncmp(help, "OPERCMDS", 8))
	{
      SND(" -");
      HDR("        *** IRC Operator Commands List ***");
      SND(" -");
   	  SND(" This section gives the IRC Operator only commands.");
      SND(" Use /HELPOP <command name> to get info about that command");
      SND(" -");
	  SND(" -----------------oOo----------------");
      SND(" OPER     WALLOPS GLOBOPS  CHATOPS LOCOPS");
      SND(" ADCHAT   NACHAT  GZLINE   KILL    KLINE");
      SND(" UNKLINE  ZLINE   UNZLINE  GLINE   SHUN");
      SND(" AKILL    RAKILL  REHASH   RESTART DIE");
      SND(" LAG      SETHOST SETIDENT CHGHOST CHGIDENT");
      SND(" CHGNAME  SQUIT   CONNECT  DCCDENY UNDCCDENY");
      SND(" SAJOIN   SAPART  SAMODE   RPING   TRACE");
      SND(" OPERMOTD ADDMOTD ADDOMOTD SDESC   ADDLINE");
      SND(" MKPASSWD TSCTL   HTM");
	  SND(" -----------------oOo----------------");
      SND(" -");
	}
   else if (!myncmp(help, "SVSCMDS", 8))
   {
     SND(" -");
     HDR("   *** U:Lined Server Command List ***");
     SND(" -");
     SND(" This section gives the commands that can be");
     SND(" sent via a U:Lined Server such as Services.");
     SND(" The command is typically sent as -");
     SND(" /MSG OPERSERV RAW :services <command>");
     SND(" Use /HELPOP <command name> to get info about that command");
     SND(" -");
     SND(" ----------oOo-----------");
     SND(" SVSNICK  SVSMODE  SVSKILL");
     SND(" SVSNOOP  SVSJOIN  SVSPART");
     SND(" SVSO     SWHOIS   SQLINE");
     SND(" UNSQLINE SVS2MODE SVSFLINE");
     SND(" SVSMOTD  SVSTIME");
     SND(" -----------oOo-----------");
     SND(" -");
   }
	else if (!myncmp(help, "UMODES", 8))
	{
     SND(" -");
     HDR(" *** UnrealIRCd Usermodes ***");
     SND(" -");
     SND(" o = Global IRC Operator");
     SND(" O = Local IRC Operator");
     SND(" i = Invisible (Not shown in /WHO searches)");
     SND(" w = Can listen to Wallop messages");
     SND(" g = Can read & send to GlobOps, and LocOps");
     SND(" h = Available for Help (Help Operator)");
     SND(" s = Can listen to Server notices");
     SND(" k = See's all the /KILL's which were executed");
     SND(" S = For Services only. (Protects them)");
     SND(" a = Is a Services Administrator");
     SND(" A = Is a Server Administrator");
     SND(" N = Is a Network Administrator");
     SND(" C = Is a Co Administrator");
     SND(" c = See's all Connects/Disconnects on local server");
     SND(" f = Listen to Flood Alerts from server");
     SND(" r = Identifies the nick as being Registered");
     SND(" x = Gives the user Hidden Hostname");
     SND(" e = Can listen to Server messages sent to +e users (Eyes)");
     SND(" b = Can read & send to ChatOps");
     SND(" W = Lets you see when people do a /WHOIS on you (IRC Operators only)");
     SND(" q = Only U:lines can kick you (Services Admins only)");
     SND(" B = Marks you as being a Bot");
     SND(" F = Lets you recieve Far and Local connect notices)");
     SND(" I = Invisible Join/Part. Makes you being hidden at channels");
     SND(" H = Hide IRCop status in /WHO and /WHOIS. (IRC Operators only)");
     SND(" d = Makes it so you can not recieve channel PRIVMSGs (Deaf)");
     SND(" v = Receive infected DCC send rejection notices");
     SND(" t = Says that you are using a /VHOST");
     SND(" G = Filters out all Bad words in your messages with <censored>.");
     SND(" z = Marks the client as being on a Secure Connection (SSL)");
     SND(" j = \"Junk\" mode. Displays misc information + nick changes");
     SND(" ---------------------oOo-------------------");
     SND(" -");
	}
	else if (!myncmp(help, "CHMODES", 8))
	{
        SND(" -");
		HDR(" *** UnrealIRCd Channel Modes ***");
        SND(" -");
		SND(" p = Private channel");
		SND(" s = Secret channel");
		SND(" i = Invite-only allowed");
		SND(" m = Moderated channel, Only users with mode +voh can speak.");
		SND(" n = No messages from outside channel");
		SND(" t = Only Channel Operators may set the topic");
		SND(" r = Channel is Registered");
		SND(" R = Requires a Registered nickname to join the channel");
		SND(" c = Blocks messages with ANSI colour (ColourBlock).");
		SND(" q = Channel owner");
		SND(" Q = No kicks able in channel unless by U:Lines");
		SND(" O = IRC Operator only channel (Settable by IRCops)");
		SND(" A = Server/Net Admin only channel (Settable by Admins)");
		SND(" K = /KNOCK is not allowed");
		SND(" V = /INVITE is not allowed");
		SND(" S = Strip all incoming colours away");
		SND(" l <number of max users> = Channel may hold at most <number> of users");
		SND(" b <nick!user@host>      = Bans the nick!user@host from the channel");
		SND(" k <key>                 = Needs the Channel Key to join the channel");
		SND(" o <nickname>            = Gives Operator status to the user");
		SND(" v <nickname>            = Gives Voice to the user (May talk if chan is +m)");
		SND(" L <chan2>               = If +l is full, the next user will auto-join <chan2>");
		SND(" a <nickname>            = Gives protection to the user (No kick/drop)");
		SND(" e <nick!user@host>      = Exception ban - If someone matches it");
		SND("                           they can join even if a ban matches them");
		SND(" h <nickname>            = Gives HalfOp status to the user");
		SND(" f [*]<lines>:<seconds>  = Flood protection, if the * is given a user will");
		SND("                           be kick banned when they send <lines> in <seconds>");
		SND("                           if * is not given they are just kicked and not banned");
		SND(" H = No +I users may join (Settable by Admins)");
        SND(" N = No Nickname changes are permitted in the channel.");
        SND(" ^ = Reports Channel modes in bitstring. Only exists in");
        SND("     Development Versions i.e #define DEVELOP");
		SND(" G = Makes the channel G Rated. Any badwords are replaced");
        SND("     with <censored> in channel messages (badwords.channel.conf).");
		SND(" u = \"Auditorium\". Makes /NAMES and /WHO #channel only show Operators.");
        SND(" C = No CTCPs allowed in the channel.");
        SND(" z = Only Clients on a Secure Connection (SSL) can join.");
		SND(" ---------------------oOo-------------------");
        SND(" -");
	}
	else if (!myncmp(help, "OFLAGS", 8))
	{
        SND(" -");
		HDR(" *** UnrealIRCd O:Line flags ***");
        SND(" -");
		SND(" r = Access to /REHASH server");
		SND(" R = Access to /RESTART server");
		SND(" D = Access to /DIE server");
		SND(" h = Oper can send /HELPOPS - gets +h on oper up");
		SND(" g = Oper can send /GLOBOPS");
		SND(" w = Oper can send /WALLOPS");
		SND(" l = Oper can send /LOCOPS");
		SND(" c = Access to do local /SQUITs and /CONNECTs");
		SND(" Y = Access to do remote /SQUITs and /CONNECTs");
		SND(" k = Access to do local /KILLs");
		SND(" K = Access to do global /KILLs");
		SND(" b = Oper can /KLINE users from server");
		SND(" B = Oper can /UNKLINE users from server");
		SND(" n = Oper can send Local Server Notices (/NOTICE $servername message)");
		SND(" u = Oper can set usermode +c");
		SND(" f = Oper can set usermode +f");
		SND(" o = Local Operator, flags included: rhgwlckbBnuf");
		SND(" O = Global Operator, flags included: oRDK");
		SND(" A = Gets +A on oper up. Is Server Administrator");
		SND(" a = Gets +a on oper up. Is Services Administrator");
		SND(" N = Gets +N on oper up. Is Network Administrator");
		SND(" C = Gets +C on oper up. Is Co Administrator");
		SND(" z = Can add Z:Lines");
		SND(" H = Gets +x on oper up.");
		SND(" W = Gets +W on oper up.");
		SND(" ^ = Allows to use usermode +I");
		SND(" * = Flags AaNCTzSHW^");
		SND(" ----------oOo-----------");
        SND(" -");
	}
	else if (!myncmp(help, "ABOUT", 8))
	{
      SND(" -");
      HDR("          *** ABOUT UnrealIRCD Help System ***");
      SND(" -");
	  SND(" The UnrealIRCd Help System. Originally by Techie/Stskeeps.");
      SND(" Modified to include new Modes and Commands by hAtbLaDe.");
      SND(" Type /INFO for some info about the IRCd.");
      SND(" -");
	  SND(" ----------------------------oOo--------------------------");
      SND(" -");
	}

   /* All that follows is for the User Commands
                        - hAtbLaDe */

   else if (!myncmp(help, "NICK", 8))
   {
      SND(" -");
      HDR(" *** NICK Command ***");
      SND(" -");
      SND(" Changes your \"Online Identity\" on a server.");
      SND(" All those in the channel you are in will be");
      SND(" alerted of your nickname change.");
      SND(" -");
      SND(" Syntax:  NICK <new nickname>");
      SND(" Example: NICK hAtbLaDe1");
      SND(" -");
   }
   else if (!myncmp(help, "WHOIS", 8))
   {
      SND(" -");
      HDR(" *** WHOIS Command ***");
      SND(" -");
      SND(" Shows information about the user in question,");
      SND(" such as their \"Name\", channels they are");
      SND(" currently in, their hostmask, etc.");
      SND(" -");
      SND(" Syntax:  WHOIS <user>");
      SND(" Example: WHOIS hAtbLaDe");
      SND(" -");
   }
   else if (!myncmp(help, "WHO", 8))
   {
      SND(" -");
      HDR(" *** WHO Command ***");
      SND(" -");
      SND(" Searches User Information (-i users only) for supplied information. IRCops are able to");
      SND(" search +i users & can use masks only. When used on a channel, it will give a list of");
      SND(" all the non-invisible users on that channel.");
      SND(" -");
      SND(" Syntax:  WHO <mask>");
      SND("          WHO <nickname>");
      SND("          WHO <#channel>");   
      SND("          WHO 0 o (Lists all IRC Operators currently online and not +i)");
	  SND(" Example: WHO *.aol.com");
      SND("          WHO *Bot*");
      SND("          WHO #UnrealIRCd");
      SND(" -");
   }
   else if (!myncmp(help, "WHOWAS", 8))
   {
      SND(" -");
      HDR(" *** WHOWAS Command ***");
      SND(" -");
      SND(" Retrieves previous WHOIS information for users");
      SND(" no longer connected to the server.");
      SND(" -");
      SND(" Syntax:  WHOWAS <nickname>");
	  SND("          WHOWAS <nickname> <max number of replies>");
      SND(" Example: WHOWAS hAtbLaDe");
      SND(" -");
   }
   else if (!myncmp(help, "NAMES", 8))
   {
      SND(" -");
      HDR(" *** NAMES Command ***");
      SND(" -");
      SND(" Provides a list of users on the specified channel.");
      SND(" -");
      SND("Syntax:  NAMES <channel>");
      SND("Example: NAMES #Support");
      SND(" -");
   }
   else if (!myncmp(help, "ISON", 8))
   {
      SND(" -");
      HDR(" *** ISON Command ***");
      SND(" -");
      SND(" Used to determine of a certain user or users are");
      SND(" currently on the IRC server based upon their nickname.");
      SND(" -");
      SND(" Syntax:  ISON <user> <user2> <user3> <user4>");
      SND(" Example: ISON hAtbLaDe Stskeeps OperServ AOLBot");
      SND(" -");
   }
   else if (!myncmp(help, "JOIN", 8))
   {
      SND(" -");
      HDR(" *** JOIN Command ***");
      SND(" -");
      SND(" Used to enter one or more channels on an IRC server.");
      SND(" All occupants of the channel will be notified of your arrival.");
      SND(" JOIN with 0 as a parameter makes you Part all channels.");
	  SND(" -");
      SND(" Syntax:  JOIN <chan>,<chan2>,<chan3>");
      SND("          JOIN 0 (Parts all channels)");
      SND(" Example: JOIN #Support");
      SND("          JOIN #Lobby,#UnrealIRCd");
      SND(" -");
   }
   else if (!myncmp(help, "PART", 8))
   {
      SND(" -");
      HDR(" *** PART Command ***");
      SND(" -");
      SND(" Used to part (or leave) a channel you currently occupy.");
      SND(" All those in the channel will be notified of your departure.");
      SND(" -");
      SND(" Syntax:  PART <chan>,<chan2>,<chan3>,<chan4>");
      SND(" Example: PART #Support");
      SND("          PART #Lobby,#UnrealIRCd");
      SND(" -");
   }
   else if (!myncmp(help, "MOTD", 8))
   {
      SND(" -");
      HDR(" *** MOTD Command ***");
      SND(" -");
      SND(" Displays the Message Of The Day.");
      SND(" -");
      SND(" Syntax: MOTD");
      SND("         MOTD <server>");
      SND(" -");
   }
   else if (!myncmp(help, "BOTMOTD", 8))
   {
      SND(" -");
      SND(" *** BOTMOTD Command ***");
      SND(" -");
      SND(" Displays the IRCd Bot Message Of The Day");
      SND(" -");
      SND(" Syntax : BOTMOTD");
      SND("          BOTMOTD <server>");
      SND(" -");
   }
   else if (!myncmp(help, "RULES", 8))
   {
      SND(" -");
      HDR(" *** RULES Command ***");
      SND(" -");
      SND(" Reads the Network Rules (ircd.rules) file and sends");
      SND(" the contents to the user.");
      SND(" -");
      SND(" Syntax: RULES");
      SND("         RULES <server>");
	  SND(" -");
   }
   else if (!myncmp(help, "LUSERS", 8))
   {
      SND(" -");
      HDR(" *** LUSERS Command ***");
      SND(" -");
      SND(" Provides Local and Global user information");
      SND(" (Such as Current and Maximum user count).");
      SND(" -");
      SND(" Syntax: LUSERS");
      SND(" -");
   }
   else if (!myncmp(help, "MAP", 8))
   {
      SND(" -");
      HDR(" *** MAP Command ***");
      SND(" -");
      SND(" Provides a graphical \"Network Map\" of the IRC network.");
      SND(" Mainly used for routing purposes.");
      SND(" -");
      SND(" Syntax: MAP");
      SND(" -");
   }
   else if (!myncmp(help, "QUIT", 8))
   {
      SND(" -");
      HDR(" *** QUIT Command ***");
      SND(" -");
      SND(" Disconnects you from the IRC server. Those in the");
      SND(" channels you occupy will be notified of your departure.");
      SND(" If you do not specify a reason, your nickname becomes the reason.");
      SND(" -");
      SND(" Syntax:  QUIT <reason>");
      SND(" Example: QUIT Leaving!");
      SND(" -");
   }
   else if (!myncmp(help, "PING", 8))
   {
      SND(" -");
      HDR(" *** PING Command ***");
      SND(" -");
      SND(" The PING command is used to test the presence of an active client or");
      SND(" server at the other end of the connection.  Servers send a PING");
      SND(" message at regular intervals if no other activity detected coming");
      SND(" from a connection.  If a connection fails to respond to a PING");
      SND(" message within a set amount of time, that connection is closed.A");
      SND(" PING message MAY be sent even if the connection is active.");
      SND(" Note that this is different from a CTCP PING command..");
      SND(" -");
      SND(" Syntax:  PING <server> <server2>");
      SND(" Example: PING irc.fyremoon.net");
      SND("          PING hAtbLaDe");
      SND("          PING hAtbLaDe irc2.dynam.ac");
      SND(" -");
   }
   else if (!myncmp(help, "PONG", 8))
   {
      SND(" -");
      HDR(" *** PONG Command ***");
      SND(" -");
      SND(" PONG message is a reply to PING message.  If parameter <server2> is");
      SND(" given, this message will be forwarded to given target.  The <server>");
      SND(" parameter is the name of the entity who has responded to PING message");
      SND(" and generated this message.");
      SND(" -");
      SND(" Syntax:  PONG <server> <server2>"); 
      SND(" Example: PONG irc.fyremoon.net irc2.dynam.ac"); 
      SND("          (PONG message from irc.fyremoon.net to irc2.dynam.ac)");
      SND(" -");
   }
   else if (!myncmp(help, "VERSION", 8))
   {
      SND(" -");
      HDR(" *** VERSION Command ***");
      SND(" -");
      SND(" Provides Version information of the IRCd software in usage.");
      SND(" -");
      SND(" Syntax: VERSION");
      SND("         VERSION <server>");
	  SND(" -");
   }
   else if (!myncmp(help, "STATS", 8))
   {
      SND(" -");
      HDR(" *** STATS Command ***");
      SND(" -");
      SND(" Provides certain Statistical information about the server");
      SND(" -");
      SND(" Syntax:  STATS <flags>");
      SND(" Example: STATS u");
      SND(" -");
      SND(" ### Stats Flags ###");
      SND(" -");
      SND(" k = Lists all the current K:Lines, Z:Lines (Banned hosts/IP) & E:Lines (K:Line exceptions)");
      SND(" g = Lists all the current G:Lines (Banned hosts) & Shuns");
      SND(" E = Lists all the current E:Lines (K:Line Exceptions)");
      SND(" f = Lists all the current F:lines (Filename masks on DCCDENY)");
      SND(" O = Lists all the current O:Lines (IRC Operator Lines)");
      SND(" Q = Lists all the current Q:Lines (Forbidden Nicks)");
      SND(" C = Lists all the current C/N:Lines (Servers to connect or accept connects from)");
      SND(" H = Lists all the current H:Lines (Hub Lines) & L:Lines (Leaf Lines)");
      SND(" n = Lists all the current n:Lines (GECOS Deny)");
      SND(" V = Lists all the current VHost lines");
      SND(" T = Lists all the current T:Lines (Specific MOTD/Rules Lines)");
      SND(" Y = Lists all the current Y:Lines (Connection classes)");
      SND(" U = Lists all the current U:Lines (Usually Services)");
      SND(" v = Lists all the current V:Lines (Version Deny)");
      SND(" D = Lists all the current D:Lines (Disallow Lines-Oper & Server Orig Connects)");
      SND(" d = Lists all the current d:Lines (Disallow Lines-Autoconnects)");
      SND(" e = Lists all the current e:Lines (Proxy scan exempt IPs)");
      SND(" I = Lists all the current I:Lines (Client auth Lines)");
      SND(" F = Lists all the current F:Lines (DCCDENY Lines)");
      SND(" r = Lists all Channel Restrict lines");
      SND(" N = Lists the Network Configuration report");
      SND(" S = Gives the Dynamic Configuration report");
      SND(" W = Gives the current Server Load");
      SND(" q = Lists all the SQLINEed Nicks");
      SND(" u = Server Uptime");
      SND(" m = Gives the Server command list");
      SND(" z = Gives Misc Server Information");
      SND(" s = Returns the scache and NS numbers");
      SND(" t = Returns Misc Info");
      SND(" L = Information about current server connections");
      SND(" -");
   }
   else if (!myncmp(help, "LINKS", 8))
   {
      SND(" -");
      HDR(" *** LINKS Command ***");
      SND(" -");
      SND(" Lists all of the servers currently linked to the network.");
      SND(" -");
      SND(" Syntax: LINKS");
      SND(" -");
   }
   else if (!myncmp(help, "ADMIN", 8))
   {
      SND(" -");
      HDR(" *** ADMIN Command ***");
      SND(" -");
      SND(" Provides Administrative information regarding the server.");
      SND(" -");
      SND(" Syntax: ADMIN <server>");
      SND(" -");
   }
   else if (!myncmp(help, "USERHOST", 8))
   {
      SND(" -");
      HDR(" *** USERHOST Command ***");
      SND(" -");
      SND(" Returns the userhost of the user in question.");
      SND(" Usually used by scripts or bots.");
      SND(" -");
      SND(" Syntax:  USERHOST <nickname>");
      SND(" Example: USERHOST hAtbLaDe");
      SND(" -");
   }
   else if (!myncmp(help, "TOPIC", 8))
   {
      SND(" -");
      HDR(" *** TOPIC Command ***");
      SND(" -");
      SND(" Sets/Changes the topic of the channel in question,");
      SND(" or just display the current Topic.");
      SND(" -");
      SND(" Syntax:  TOPIC <channel> (Displays the current topic)");
      SND("          TOPIC <channel> <topic> (Changes topic)");
      SND(" Example: TOPIC #Operhelp");
      SND("          TOPIC #Lobby Welcome to #Lobby!!");
      SND(" -");
   }
   else if (!myncmp(help, "INVITE", 8))
   {
      SND(" -");
      HDR(" *** INVITE Command ***");
      SND(" -");
      SND(" Sends a user an Invitation to join a particular channel.");
      SND(" You must be an Operator on the channel in order to");
      SND(" invite a user into it.");
      SND(" -");
      SND(" Syntax:  INVITE <user> <channel>");
      SND(" Example: INVITE hAtbLaDe #Support");
      SND(" -");
   }
   else if (!myncmp(help, "KICK", 8))
   {
      SND(" -");
      HDR(" *** KICK Command ***");
      SND(" -");
      SND(" Removes a user from a channel. Can only be used by Operators");
      SND(" or Half-Ops. If no reason is specified, your nickname becomes the reason.");
      SND(" -");
      SND(" Syntax:  KICK <channel>[,<channel2>..] <user>[,<user2>..] <reason>");
      SND(" Example: KICK #Lobby foobar Lamer..");
	  SND("          KICK #Lobby,#OperHelp Lamer23,Luser12 Lamers!");
      SND(" -");
   }
   else if (!myncmp(help, "AWAY", 8))
   {
      SND(" -");
      HDR(" *** AWAY Command ***");
      SND(" -");
      SND(" Sets your online status to \"Away\".");
      SND(" -");
      SND(" Syntax:  AWAY <reason> (Sets you Away with the reason given)");
      SND("          AWAY          (Un-Sets you as Away)");
      SND(" Example: AWAY Lunch time!");
      SND(" -");
   }
   else if (!myncmp(help, "WATCH", 8))
   {
      SND(" -");
      HDR(" *** WATCH Command ***");
      SND(" -");
      SND(" Watch is a new notify-type system in UnrealIRCd which is both faster");
      SND(" and uses less network resources than any old-style notify");
      SND(" system. The server will send you a message when any nickname");
      SND(" in your watch list logs on or off.");
      SND(" The watch list DOES NOT REMAIN BETWEEN SESSIONS - You (or your");
      SND(" script or client) must add the nicknames to your watch list every");
      SND(" time you connect to an IRC server.");
      SND(" -");
      SND(" Syntax: WATCH +nick1 +nick2 +nick3  (Add nicknames)");
      SND("         WATCH -nick                 (Delete nicknames)");
      SND("         WATCH                       (View the watchlist)");
      SND(" -");
   }
   else if (!myncmp(help, "LIST", 8))
   {
      SND(" -");
      HDR(" *** LIST Command ***");
      SND(" -");
      SND(" Provides a complete listing of all channels on the network.");
      SND(" If a search string is specified, it will only show those");
      SND(" matching the search string.");
      SND(" -");
      SND(" Syntax:  LIST <search string>");
      SND(" Example: LIST");
      SND("          LIST *ircd*");
      SND(" -");
      SND(" New extended /LIST command options are supported. To use these");
      SND(" features, you will likely need to prefix the LIST command with");
      SND(" /QUOTE to avoid your client interpreting the command.");
      SND(" -");
      SND(" Usage: /QUOTE LIST options");
      SND(" -");
      SND(" If you don't include any options, the default is to send you the");
      SND(" entire unfiltered list of channels. Below are the options you can");
      SND(" use, and what channels LIST will return when you use them.");
      SND(" -");
      SND(" >number  List channels with more than <number> people.");
      SND(" <number  List channels with less than <number> people.");
      SND(" C>number List channels created between now and <number> minutes ago.");
      SND(" C<number List channels created earlier than <number> minutes ago.");
      SND(" T>number List channels whose topics are older than <number> minutes");
      SND("          (Ie., they have not changed in the last <number> minutes.");
      SND(" T<number List channels whose topics are newer than <number> minutes.");
      SND(" *mask*   List channels that match *mask*");
      SND(" !*mask*  List channels that do not match *mask*");
      SND(" -");
      SND(" NOTE : C & T parameters do not exist Unreal 3.1.1-Darkshades onwards.");
      SND(" LIST defaults to sending a list of channels with 2 or more members,");
      SND(" so use the >0 option to get the full channel listing.");
      SND(" -");
   }
   else if ((!myncmp(help, "PRIVMSG", 8))||(!myncmp(help, "NOTICE", 8)))
   {
      SND(" -");
      HDR(" *** PRIVMSG/NOTICE Command ***");
      SND(" -");
      SND(" PRIVMSG and NOTICE, which are used internally by the client for");
      SND(" /MSG and /NOTICE, in UnrealIRCd support two additional formats:");
      SND(" /MSG @#channel <text> will send the text to Channel-ops on the");
      SND(" given channel only. /MSG @+#channel <text> will send the text");
      SND(" to both ops and voiced users on the channel. While some clients");
      SND(" may support these as-is, on others (such as ircII), it's necessary");
      SND(" to use /QUOTE PRIVMSG @#channel <text> instead. You can also use");
	  SND(" % to signify HalfOps on the channel.");
      SND(" -");
      SND(" Syntax:  MSG <nick>,<nick2>,<nick3>,<nick4> :<text>");
      SND(" Example: PRIVMSG hAtbLaDe :Hello.");
      SND("          PRIVMSG hAtbLaDe,Somefella,Lamer :Hello everyone!");
      SND(" -");
      SND(" The format for the NOTICE command is the same as above.");
      SND(" -");
   }
   else if (!myncmp(help, "KNOCK", 8))
   {
      SND(" -");
      HDR(" **** KNOCK Command ****");
      SND(" -");
      SND(" For channels which are invite only, you can \"Knock\" on the");
      SND(" channel to request an invite.");
      SND(" -");
      SND(" The following criteria must be met :");
      SND("     - Channel is not +K (No knocks)");
      SND("     - Channel is not +I (No invites!)");
      SND("     - You're not banned!");
      SND("     - And you are not already there");
      SND(" -");
      SND(" Syntax:  KNOCK <channel> <message>");
      SND(" Example: KNOCK #secret_chan I'm an op, let me in!");
      SND(" -");
   }
   else if (!myncmp(help, "SETNAME", 8))
   {
      SND(" -");
      HDR(" *** SETNAME Command ***");
      SND(" -");
      SND(" Allows users to change their \'Real name\' (GECOS)");
      SND(" directly online at IRC without reconnecting");
   	SND(" -");
      SND(" Syntax: SETNAME <New Real Name>");
      SND(" -");
   }
   else if (!myncmp(help, "VHOST", 8))
   {
      SND(" -");
      HDR(" *** VHOST Command ***");
      SND(" -");
      SND(" Hides your real hostname with a virtual hostname");
      SND(" provided by the IRC server , using SETHOST.");
      SND(" -");
      SND(" Synatx:  VHOST <login> <password>");
      SND(" Example: VHOST openbsd ilovecypto");
      SND(" -");
   }
   else if (!myncmp(help, "MODE", 8))
   {
      SND(" -");
      HDR(" *** MODE Command ***");
      SND(" -");
      SND(" Sets a mode on a Channel or User.");
      SND(" UnrealIRCd has got some new Channel & User Modes.");
      SND(" Use /HELPOP CHMODES or /HELPOP UMODES to see a list of Modes");
      SND(" -");
      SND(" Syntax:  MODE <channel/user> <mode>");
      SND(" Example: MODE #Support +tn");
      SND("          MODE #Support +ootn hAtbLaDe XYZ");
      SND(" -");
   }
   else if (!myncmp(help, "CREDITS", 8))
   {
      SND(" -");
	   HDR(" *** CREDITS Command ***");
      SND(" -");
  	   SND(" This command will list the Credits to all the people who");
      SND(" helped create UnrealIRCd.");
      SND(" -");
	   SND(" Syntax: CREDITS");
       SND("         CREDITS <server>");
      SND(" -");
   }
   else if (!myncmp(help, "DALINFO", 8))
   {
      SND(" -");
   	HDR(" *** DALINFO Command ***");
      SND(" -");
   	SND(" This command will list the Credits that the Dreamforge");
      SND(" IRCd team/the IRCd developers from the start when IRCd got developed");
   	SND(" -");
	   SND(" Syntax: DALINFO");
       SND("         DALINFO <server>");
      SND(" -");
   }
   else if (!myncmp(help, "LICENSE", 8))
   {
      SND(" -");
	   HDR(" *** LICENSE Command ***");
      SND(" -");
 	   SND(" This command shows the GNU License Which is hard-coded into the IRCd.");
      SND(" -");
	   SND(" Syntax: LICENSE");
       SND("         LICENSE <server>");
      SND(" -");
   }
   else if (!myncmp(help, "TIME", 8))
   {
     SND(" -");
     HDR(" *** TIME Command ***");
     SND(" -");
     SND(" Lists the current Server Date and Time.");
     SND(" -");
     SND(" Syntax : TIME");
	 SND("          TIME <server>");
     SND(" -");
   }
   else if (!myncmp(help, "SILENCE", 8))
   {
     SND(" -");
     HDR(" *** SILENCE Command ***");
     SND(" -");
     SND(" Ignores messages from a user or list of users at the Server itself.");
     SND(" -");
     SND(" Syntax: SILENCE +nickname (Adds a nickname to SILENCE list)");
     SND("         SILENCE -nickname (Removes a nickname from the SILENCE list)");
     SND("         SILENCE           (Lists the current SILENCE list)");
     SND(" -");
   }
   else if (!myncmp(help, "IDENTIFY", 8))
   {
     SND(" -");
     HDR(" *** IDENTIFY Command ***");
     SND(" -");
     SND(" An alias to allow you to identify to NickServ or ChanServ with your password.");
     SND(" If it cannot find NickServ or ChanServ , it will report services as down.");
     SND(" -");
     SND(" Syntax: IDENTIFY <password>  (Identify to NickServ)");
     SND("         IDENTIFY #<channel> <password> (Identify to ChanServ as Founder of #channel)");
     SND(" -");
   }
   else if ((!myncmp(help, "NICKSERV", 8))||(!myncmp(help, "NS", 8)))
   {
     SND(" -");
     SND(" *** NICKSERV/NS Command ***");
     SND(" -");
     SND(" Sends a message to NickServ. This is an alias for /MSG NickServ ");
     SND(" If NickServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: NICKSERV <text>");
     SND("         NS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "CHANSERV", 8))||(!myncmp(help, "CS", 8)))
   {
     SND(" -");
     SND(" *** CHANSERV/CS Command ***");
     SND(" -");
     SND(" Sends a message to ChanServ. This is an alias for /MSG ChanServ ");
     SND(" If ChanServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: CHANSERV <text>");
     SND("         CS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "MEMOSERV", 8))||(!myncmp(help, "MS", 8)))
   {
     SND(" -");
     SND(" *** MEMOSERV/MS Command ***");
     SND(" -");
     SND(" Sends a message to MemoServ. This is an alias for /MSG MemoServ ");
     SND(" If MemoServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: MEMOSERV <text>");
     SND("         MS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "OPERSERV", 8))||(!myncmp(help, "OS", 8)))
   {
     SND(" -");
     SND(" *** OPERSERV/OS Command ***");
     SND(" -");
     SND(" Sends a message to OperServ. This is an alias for /MSG OperServ ");
     SND(" If OperServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: OPERSERV <text>");
     SND("         OS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "HELPSERV", 8))||(!myncmp(help, "HS", 8)))
   {
     SND(" -");
     SND(" *** HELPSERV/HS Command ***");
     SND(" -");
     SND(" Sends a message to HelpServ. This is an alias for /MSG HelpServ ");
     SND(" If HelpServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: HELPSERV <text>");
     SND("         HS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "BOTSERV", 8))||(!myncmp(help, "BS", 8)))
   {
     SND(" -");
     SND(" *** BOTSERV/BS Command ***");
     SND(" -");
     SND(" Sends a message to BotServ. This is an alias for /MSG BotServ ");
     SND(" If BotServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: BOTSERV <text>");
     SND("         BS <text>");
     SND(" -");
   }
   else if ((!myncmp(help, "INFOSERV", 8))||(!myncmp(help, "IS", 8)))
   {
     SND(" -");
     SND(" *** INFOSERV/IS Command ***");
     SND(" -");
     SND(" Sends a message to InfoServ. This is an alias for /MSG InfoServ ");
     SND(" If InfoServ cannot be found , it will report services as down.");
     SND(" -");
     SND(" Syntax: INFOSERV <text>");
     SND("         IS <text>");
     SND(" -");
   }


   /* On to the IRC Operator commands
                     - hAtbLaDe */

   else if (!myncmp(help, "OPER", 8))
   {
      SND(" -");
      HDR(" *** OPER Command ***");
      SND(" -");
      SND(" Attempts to give a user IRC Operator status.");
      SND(" -");
      SND(" Syntax:  OPER <uid> <password>");
      SND(" Example: OPER hAtbLaDe foobar234");
      SND(" -");
   }
   else if (!myncmp(help, "WALLOPS", 8))
   {
      SND(" -");
      HDR(" *** WALLOPS Command ***");
      SND(" -");
      SND(" Sends a \"Message\" to all those with the umode +w.");
      SND(" Only IRCops can send Wallops, while anyone can view them.");
      SND(" -");
      SND(" Syntax: WALLOPS <message>");
      SND(" -");
   }
   else if (!myncmp(help, "GLOBOPS", 8))
   {
      SND(" -");
      HDR(" *** GLOBOPS Command ***");
      SND(" -");
      SND(" Sends a global \"Message\" to all IRCops. Only viewable by IRCops");
      SND(" (unlike WALLOPS, which can be viewed by normal users).");
      SND(" -");
      SND(" Syntax:  GLOBOPS <message>");
      SND(" Example: GLOBOPS Lets get em clones ..");
      SND(" -");
   }
   else if (!myncmp(help, "CHATOPS", 8))
   {
      SND(" -");
      HDR(" *** CHATOPS Command ***");
      SND(" -");
      SND(" GLOBOPS is usually reserved for important network information.");
      SND(" Therefore, for Oper Chat, CHATOPS was invented. IRCops with");
      SND(" the +c flag enabled will be able to send/receive CHATOPS messages.");
      SND(" -");
      SND(" Syntax:  CHATOPS <message>");
      SND(" Example: CHATOPS How's everyone doing today?");
      SND(" -");
   }
   else if (!myncmp(help, "LOCOPS", 8))
   {
      SND(" -");
      HDR(" *** LOCOPS Command ***");
      SND(" -");
      SND(" Similar to GLOBOPS, except only received by those IRCops local to your server.");
      SND(" -");
      SND(" Syntax:  LOCOPS <message>");
      SND(" Example: LOCOPS Gonna k:line that luser ...");
      SND(" -");
   }
   else if (!myncmp(help, "ADCHAT", 8))
   {
      SND(" -");
      HDR(" *** ADCHAT Command ***");
      SND(" -");
      SND(" This command sends to all Admins online");
      SND(" Only for Admins. This is a ChatOps style command");
      SND(" -");
      SND(" Syntax:  ADCHAT <text>");
      SND(" Example: ADCHAT Hey guys!");
      SND(" -");
   }
   else if (!myncmp(help, "NACHAT", 8))
   {
      SND(" -");
      HDR(" *** NACHAT Command ***");
      SND(" -");
      SND(" This command sends to all NetAdmins online");
      SND(" Only for Net Admins. This is a ChatOps style command");
      SND(" -");
      SND(" Syntax:  NACHAT <text>");
      SND(" Example: NACHAT Hey guys!");
      SND(" -");
   }
   else if (!myncmp(help, "KILL", 8))
   {
      SND(" -");
      HDR(" *** KILL Command ***");
      SND(" -");
      SND(" Forcefully Disconnects a user from an IRC Server.");
      SND(" IRC Operator only command.");
	  SND(" -");
      SND(" Syntax:  KILL <user> <reason>");
      SND(" Example: KILL Clone5 Cloning is not allowed");
      SND(" -");
   }
   else if (!myncmp(help, "KLINE", 8))
   {
      SND(" -");
      HDR(" *** KLINE Command ***");
      SND(" -");
      SND(" \"Bans\" a hostmask from connection to the IRC server.");
      SND(" The user can however connect to other servers on the network !!");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  KLINE <hostmask> <reason>");
      SND(" Example: KLINE *@*.aol.com Abuse");
      SND(" -");
   }
   else if (!myncmp(help, "UNKLINE", 8))
   {
      SND(" -");
      HDR(" *** UNKLINE Command ***");
      SND(" -");
      SND(" Removes a k:line from the server.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  UNKLINE <hostmask>");
      SND(" Example: UNKLINE *@*.aol.com");
      SND(" -");
   }
   else if (!myncmp(help, "ZLINE", 8))
   {
      SND(" -");
      HDR(" *** ZLINE Command ***");
      SND(" -");
      SND(" Disables all access to the IRC server from a specified IP.");
      SND(" The IP can however connect to other servers on the network !!");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  ZLINE <ip> :<Reason>");
      SND(" Example: ZLINE 127.0.0.1 :Localhost");
      SND(" -");
   }
   else if (!myncmp(help, "UNZLINE", 8))
   {
      SND(" -");
      HDR(" *** UNZLINE Command ***");
      SND(" -");
      SND(" Removes a currently active z:Line.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  UNZLINE <ip>");
      SND(" Example: UNZLINE 127.0.0.1");
      SND(" -");
   }
   else if (!myncmp(help, "GZLINE", 8))
   {
      SND(" -");
      HDR(" *** ^BGZLINE command^B ***");
      SND(" -");
      SND(" This command provides timed Z:Lines. If you match a Z:Line you cannot");
      SND(" connect to ANY server on the IRC network");
      SND(" A time of 0 in the ZLINE makes it permanent (Never Expires).");
      SND(" You may also specify the time in the format 1d10h15m30s.");  
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  GZLINE <user@host mask> <seconds to be banned> :<reason>");
      SND("          (Adds a Z:line for user@host)");
      SND("          GZLINE -<user@host mask> (Removes a Z:line for user@host)");
      SND(" Example: GZLINE *@24.247.1.* 900 :Spammers  (Adds a 15 min Z:line)");
      SND("          GZLINE *@24.247.1.* 1d5h :Spammers (Adds a 29 hour Z:line)");
      SND(" -");
   }
   else if (!myncmp(help, "GLINE", 8))
   {
      SND(" -");
      HDR(" *** GLINE command ***");
      SND(" -");
      SND(" This command provides timed G:Lines. If you match a G:Line you cannot");
	  SND(" connect to ANY server on the IRC network");
      SND(" A time of 0 in the GLINE makes it permanent (Never Expires).");
      SND(" In Unreal 3.1.1 you may also specify the time in the format 1d10h15m30s.");  
      SND(" IRC Operator only command.");
	  SND(" -");
      SND(" Syntax:  GLINE <user@host mask> <seconds to be banned> :<reason>");
      SND("          (Adds a G:line for user@host)");
      SND("          GLINE -<user@host mask> (Removes a G:line for user@host)");
      SND(" Example: GLINE *@*.dal.net 900 :Spammers  (Adds a 15 min G:line)");
      SND("          GLINE *@*.dal.net 1d5h :Spammers (Adds a 29 hour G:line)");
      SND(" -");
   }
   else if (!myncmp(help, "SHUN", 8))
   {
      SND(" -");
      HDR(" *** SHUN Command ***");
      SND(" -");
      SND(" Prevents a user from executing ANY command except ADMIN");
      SND(" and respond to Server Pings.");
      SND(" A time of 0 in the SHUN makes it permanent (Never Expires).");
      SND(" In Unreal 3.1.1 you may also specify the time in the format 1d10h15m30s.");  
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  SHUN <nickname> <time> :<Reason>  (Shun the nickname for time in seconds)");
      SND("          SHUN +<user@host> <time> :<Reason>(Shun the user@host for time in seconds)");
      SND("          SHUN -<user@host>                 (Removes the SHUN for user@host)");
      SND("          SHUN                              (View the current SHUN list)");
      SND(" -");
      SND(" Example: SHUN +foobar@aol.com 600 :Spamming");
      SND("          (Shuns foobar@aol.com for 10 mins for Spamming)");
      SND("          SHUN +foobar@aol.com 1d6h :Spamming (Adds a 30 hour SHUN)");
      SND(" -");
   }
   else if (!myncmp(help, "AKILL", 8))
   {
      SND(" -");
      HDR(" *** AKILL Command ***");
      SND(" -");
      SND(" Adds an Autokill for the specific host mask. This prevents");
      SND(" any user from that hostmask from connecting to the network.");
      SND(" Services Admin Command");
	  SND(" -");
      SND(" Syntax:  AKILL <user@host> :<Reason>");
      SND(" Example: AKILL foo@aol.com :Spammers!");
      SND(" -");
   }
   else if (!myncmp(help, "RAKILL", 8))
   {
      SND(" -");
      HDR(" *** RAKILL Command ***");
      SND(" -");
      SND(" Removes an AKILL set by an IRC Operator or Services.");
      SND(" Services Admin Command");
	  SND(" -");
      SND(" Syntax: RAKILL <user@host>");
      SND(" -");
   }
   else if (!myncmp(help, "REHASH", 8))
   {
      SND(" -");
      HDR(" *** REHASH Command ***");
      SND(" -");
      SND(" Prompts the server to reread the configuration files.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: REHASH <servername> -<flags>");
      SND("         REHASH -<flags>");
      SND(" -");
      SND(" If servername and flags are not specified this rehashes the");
      SND(" ircd.conf , removing any temporary k:lines.");
      SND(" If servername is specified, this is used to rehash config files on servername");
      SND(" Only NetAdmins may specify a server name");
      SND(" -");
      SND(" The flags are used to rehash other config files, valid flags are:");
      SND("       -dccdeny   - Rehashes dccdeny.conf");
      SND("       -dynconf   - Rehashes UnrealIRCd Config and Network file");
      SND("       -restrict  - Rehashes chrestrict.conf");
      SND("       -vhost     - Rehashes vhost.conf");
      SND("       -motd      - Rehashes all MOTD files and RULES files (including T:lines)");
      SND("       -opermotd  - Rehashes the OPERMOTD");
      SND("       -botmotd   - Rehashes the BOTMOTD");
      SND("       -garbage   - Force garbage collection");
      SND("       -badwords  - Rehashes the badwords config.");
      SND(" -");
   }
   else if (!myncmp(help, "RESTART", 8))
   {
      SND(" -");
      HDR(" *** RESTART Command ***");
      SND(" -");
      SND(" Kills and Restarts the IRC daemon, disconnecting all users");
      SND(" currently on that server.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: RESTART");
      SND("         RESTART <password>");
      SND("         RESTART <server> <password>");
      SND(" -");
   }
   else if (!myncmp(help, "DIE", 8))
   {
      SND(" -");
      HDR(" *** DIE Command ***");
      SND(" -");
      SND(" Kills the IRC daemon, disconnecting all users currently on that server.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: DIE");
      SND("         DIE <password>");
      SND(" -");
   }
   else if (!myncmp(help, "LAG", 8))
   {
      SND(" -");
      HDR(" *** LAG Command ***");
      SND(" -");
      SND(" This command is like a Traceroute for IRC servers");
      SND(" You type in /LAG irc.fyremoon.net and it will");
      SND(" reply from every server it passes with time and so on");
      SND(" Useful for looking where lag is and optional TS future/past travels");
      SND(" -");
      SND(" Syntax: LAG <server>");
      SND(" -");
   }
   else if (!myncmp(help, "SETHOST", 8))
   {
      SND(" -");
      HDR(" *** SETHOST Command ***");
      SND(" -");
      SND(" This command is so you can change your Virtual host (Vhost)");
      SND(" to anything you want, except special characters.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  SETHOST <new hostname>");
      SND(" Example: SETHOST the.domain.of.the.coders");
      SND(" -");
   }
   else if (!myncmp(help, "SETIDENT", 8))
   {
      SND(" -");
      HDR(" *** SETIDENT Command ***");
      SND(" -");
      SND(" With this command you can change your Ident (Username).");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  SETIDENT <new ident>");
      SND(" Example: SETIDENT root");
      SND(" -");
   }
   else if (!myncmp(help, "CHGHOST", 8))
   {
      SND(" -");
      HDR(" *** CHGHOST Command ***");
      SND(" -");
      SND(" Changes the hostname of a user currently on the IRC network.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  CHGHOST <nick> <host>");
      SND(" Example: CHGHOST hAtbLaDe root.me.com");
      SND(" -");
   }
   else if (!myncmp(help, "CHGIDENT", 8))
   {
      SND(" -");
      HDR(" *** CHGIDENT Command ***");
      SND(" -");
      SND(" Changes the Ident of a user currently on the IRC network.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  CHGIDENT <nick> <ident>");
      SND(" Example: CHGIDENT hAtbLaDe FreeBSD");
      SND(" -");
   }
   else if (!myncmp(help, "CHGNAME", 8))
   {
      SND(" -");
      HDR(" *** CHGNAME Command ***");
      SND(" -");
      SND(" Changes the \"IRC Name\" (or \"Real Name\") of a user currently on the IRC network.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  CHGNAME <nick> <name>");
      SND(" Example: CHGNAME hAtbLaDe Gotta new name :)");
      SND(" -");
   }
   else if (!myncmp(help, "SQUIT", 8))
   {
      SND(" -");
      HDR(" *** SQUIT Command ***");
      SND(" -");
      SND(" Disconnects an IRC Server from the network");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  SQUIT <server>");
      SND(" Example: SQUIT leaf.*");
      SND(" -");
   }
   else if (!myncmp(help, "CONNECT", 8))
   {
      SND(" -");
      HDR(" *** CONNECT Command ***");
      SND(" -");
      SND(" Links another IRC server to the one you are currently on.");
      SND(" Remote connections are also possible.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax:  CONNECT <server>");
      SND("          CONNECT <hub> <port> <leaf>");
      SND(" Example: CONNECT leaf.*");
      SND("          CONNECT hub.* 6667 leaf.*");
      SND(" -");
   }
   else if (!myncmp(help, "DCCDENY", 8))
   {
      SND(" -");
      HDR(" *** DCCDENY Command ***");
      SND(" -");
      SND(" Adds a DCC Deny for that Filename mask. This means that any");
      SND(" DCC sends of Files matching that Filename mask will be rejected.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: DCCDENY <filename mask> <reason>");
      SND(" -");
   }
   else if (!myncmp(help, "UNDCCDENY", 8))
   {
      SND(" -");
      HDR(" *** UNDCCDENY Command ***");
      SND(" -");
      SND(" If the EXACT file you type is found it is removed, else it uses wildcards to search");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: UNDCCDENY <filename mask>");
      SND(" -");
   }
   else if (!myncmp(help, "SAJOIN", 8))
   {
      SND(" -");
      HDR(" *** SAJOIN Command ***");
      SND(" -");
      SND(" Forces a user to join a channel. Can only be used by a Services Admin.");
      SND(" -");
      SND(" Syntax:  SAJOIN <nick> <channel>,[<channel2>..]");
      SND(" Example: SAJOIN hAtbLaDe #OperHelp");
	  SND("          SAJOIN hAtbLaDe #Support,#IRCHelp");
      SND(" -");
   }
   else if (!myncmp(help, "SAPART", 8))
   {
      SND(" -");
      HDR(" *** SAPART Command ***");
      SND(" -");
      SND(" Forces a user to leave a channel. Can only be used by a Services Admin.");
      SND(" -");
      SND(" Syntax:  SAPART <nick> <channel>,[<channel2>..]");
      SND(" Example: SAPART hAtbLaDe #OperHelp");
	  SND("          SAPART hAtbLaDe #Support,#IRCHelp");
      SND(" -");
   }
   else if (!myncmp(help, "SAMODE", 8))
   {
      SND(" -");
      HDR(" *** SAMODE Command ***");
      SND(" -");
      SND(" Allows a Services Administrator to change the mode on a channel,");
      SND(" without having Operator status.");
      SND(" -");
      SND(" Syntax:  SAMODE <channel> <mode>");
      SND(" Example: SAMODE #Support +m");
      SND(" -");
   }
   else if (!myncmp(help, "RPING", 8))
   {
      SND(" -");
      HDR(" *** RPING Command ***");
      SND(" -");
      SND(" This will calculate the Lag (In milliseconds) between servers");
      SND(" -");
      SND(" Syntax: RPING <servermask>");
      SND(" -");
   }
   else if (!myncmp(help, "TRACE", 8))
   {
      SND(" -");
      HDR(" *** TRACE Command ***");
      SND(" -");
      SND(" TRACE is useful to know what servers are connected to");
      SND(" what. Sometimes TRACE can be confusing, especially if you are using");
      SND(" it for the first time.");
      SND(" -");
      SND(" Syntax:  TRACE <servername>");
      SND(" Example: TRACE irc.fyremoon.net");
      SND(" -");
   }
   else if (!myncmp(help, "OPERMOTD", 8))
   {
      SND(" -");
      HDR(" *** OPERMOTD Command ***");
      SND(" -");
      SND(" Shows the IRCd Operator MOTD");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: OPERMOTD");
      SND(" -");
   }
   else if (!myncmp(help, "ADDMOTD", 8))
   {
      SND(" -");
      HDR(" *** ADDMOTD Command ***");
      SND(" -");
      SND(" This will add the text you specify to the MOTD");
      SND(" (the general MOTD - T:lines dont count ..)");
      SND(" Server Admin & Co-Admin only");
      SND(" -");
      SND(" Syntax: ADDMOTD :text");
      SND(" -");
   }
   else if (!myncmp(help, "ADDOMOTD", 8))
   {
      SND(" -");
      HDR(" *** ADDOMOTD Command ***");
      SND(" -");
      SND(" This will add the text you specify to the Operator MOTD");
      SND(" Server Admin & Co-Admin only");
      SND(" -");
      SND(" Syntax: ADDOMOTD :text");
      SND(" -");
   }
   else if (!myncmp(help, "SDESC", 8))
   {
      SND(" -");
      HDR(" *** SDESC Command ***");
      SND(" -");
      SND(" With this command you can change your Server Info Line");
      SND(" Without having to squit and reconnect.");
      SND(" This is a Server Admin/Co Admin only command");
      SND(" -");
      SND(" Syntax:  SDESC <New description>");
      SND(" Example: SDESC Fly High, Fly Free");
      SND(" -");
   }
   else if (!myncmp(help, "ADDLINE", 8))
   {
      SND(" -");
      HDR(" *** ADDLINE Command ***");
      SND(" -");
      SND(" This command can be used to add lines to the ircd.conf file");
      SND(" Only for Server Admins");
      SND(" -");
      SND(" Syntax: ADDLINE <line>");
      SND(" -");
   }
   else if (!myncmp(help, "MKPASSWD", 8))
   {
      SND(" -");
      HDR(" *** MKPASSWD Command ***");
      SND(" -");
      SND(" This command will Encrypt the string it has been given");
      SND(" So you can add it directly to the ircd.conf if you use");
      SND(" Encrypted passwords. Disabled in UnrealIRCd/32");
      SND(" -");
      SND(" Syntax: MKPASSWD <string to be encrypted>");
      SND(" -");
   }
   else if (!myncmp(help, "TSCTL", 8))
   {
      SND(" -");
      HDR(" *** TSCTL Command ***");
      SND(" -");
      SND(" This is a highly advanced command used to Adjust the");
      SND(" Internal IRC clock.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: TSCTL OFFSET +|- <time> (Adjust internal IRC clock)");
      SND("         TSCTL TIME              (Will give TS report)");
      SND("         TSCTL ALLTIME   (Shows the TS report of all server)");
      SND("         TSCTL SVSTIME <timestamp> (Sets the Time on all Servers)");
	  SND(" -");
   }
   else if (!myncmp(help, "HTM", 8))
   {
      SND(" -");
      HDR(" *** HTM Command ***");
      SND(" -");
      SND(" Switches the server In & Out of High Traffic Mode");
      SND(" It is activated when the server is receiving extremely high amounts of information.");
      SND(" IRC Operator only command.");
      SND(" -");
      SND(" Syntax: HTM [option]");
      SND(" -");
      SND(" If no option is specified it just displays the current HTM state");
      SND(" If an option is specified it does a more specific task, valid options are:");
      SND(" -");
      SND(" ON         - Force HTM to activate");
      SND(" OFF        - Force HTM to deactivate");
      SND(" NOISY      - Make HTM announce when it is entering/leaving HTM");
      SND(" QUIET      - Stop HTM from announcing when it is entering/leaving HTM");
      SND(" TO <value> - Tell HTM at what incoming rate to activate HTM");
      SND(" -");
   }
   else if (!myncmp(help, "REMGLINE", 8))  /* Obsolete .. */
   {
      SND(" -");
      HDR(" *** REMGLINE Command ***");
      SND(" -");
      SND(" This command is now obsolete. Use GLINE -<user@host> instead.");
      SND(" -");
   }

   /* Commands Sent via U:Lined Servers
                       - hAtbLaDe */

   else if (!myncmp(help, "SVSNICK", 8))
   {
      SND(" -");
      HDR(" *** SVSNICK Command ***");
      SND(" -");
      SND(" Changes the nickname of the user in question.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSNICK <nickname> <new nickname> <timestamp>");
      SND(" Example: SVSNICK hAtbLaDe Foobar 963086432");
      SND(" -");
   }
   else if (!myncmp(help, "SVSMODE", 8))
   {
      SND(" -");
      HDR(" *** SVSMODE Command ***");
      SND(" -");
      SND(" Changes the mode of the User in question.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSMODE <nickname> <usermode>");
      SND(" Example: SVSMODE hAtbLaDe +i");
      SND(" -");
   }
   else if (!myncmp(help, "SVSKILL", 8))
   {
      SND(" -");
      HDR(" *** SVSKILL Command ***");
      SND(" -");
      SND(" Forcefully disconnects a user from the network.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSKILL <user> :<reason>");
      SND(" Example: SVSKILL Lamer21 :Goodbye");
      SND(" -");
   }
   else if (!myncmp(help, "SVSNOOP", 8))
   {
      SND(" -");
      HDR(" *** SVSNOOP Command ***");
      SND(" -");
      SND(" Enabled or disables whether Global IRCop functions exist on the server in question or not.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSNOOP <server> <+/->");
      SND(" Example: SVSNOOP leaf.* -");
      SND(" -");
   }
   else if (!myncmp(help, "SVSJOIN", 8))
   {
      SND(" -");
      HDR(" *** SVSJOIN Command ***");
      SND(" -");
      SND(" Forces a user to join a channel.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSJOIN <nick> <channel>[,<channel2>..]");
      SND(" Example: SVSJOIN hAtbLaDe #jail");
	  SND("          SVSJOIN hAtbLaDe #jail,#zoo");
      SND(" -");
   }
   else if (!myncmp(help, "SVSPART", 8))
   {
      SND(" -");
      HDR(" *** SVSPART Command ***");
      SND(" -");
      SND(" Forces a user to leave a channel.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSPART <nick> <channel>[,<channel2>..]");
      SND(" Example: SVSPART hAtbLaDe #Hanson");
	  SND("          SVSPART hAtbLaDe #Hanson,#AOL");
      SND(" -");
   }
   else if (!myncmp(help, "SVSO", 8))
   {
      SND(" -");
      HDR(" *** SVSO Command ***");
      SND(" -");
      SND(" Gives nick Operflags like the ones in O:lines.");
      SND(" Remember to set SVSMODE +o and alike.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSO <nick> <+operflags> (Adds the Operflags)");
      SND("          SVSO <nick> -            (Removes all O:Line flags)");
      SND(" Example: SVSO SomeNick +bBkK");
      SND(" -");
   }
   else if (!myncmp(help, "SWHOIS", 8))
   {
      SND(" -");
      HDR(" *** SWHOIS Command ***");
      SND(" -");
      SND(" Changes the WHOIS message of the Nickname.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SWHOIS <nick> :<message> (Sets the SWHOIS)");
      SND("          SWHOIS <nick> :          (Resets the SWHOIS)");
      SND(" Example: SWHOIS SomeNick :is a lamer");
      SND(" -");
   }
   else if (!myncmp(help, "SQLINE", 8))
   {
      SND(" -");
      HDR(" *** SQLINE Command ***");
      SND(" -");
      SND(" Bans a Nickname or a certain Nickname mask from the Server.");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SQLINE <nickmask> :<Reason>");
      SND(" Example: SQLINE *Bot* :No bots");
      SND(" -");
   }
   else if (!myncmp(help, "UNSQLINE", 8))
   {
      SND(" -");
      HDR(" *** UNSQLINE Command ***");
      SND(" -");
      SND(" Un-Bans a Nickname or Nickname mask");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Synax:   UNSQLINE <nickmask>");
      SND(" Example: UNSQLINE *Bot*");
      SND(" -");
   }
   else if (!myncmp(help, "SVS2MODE", 8))
   {
      SND(" -");
      HDR(" *** SVS2MODE Command ***");
      SND(" -");
      SND(" Changes the Usermode of a nickname");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  :services.somenet.com SVS2MODE <nickname> +<mode>");
      SND(" Example: :services.roxnet.org SVS2MODE hAtbLaDe +h");
      SND(" -");
   }
   else if (!myncmp(help, "SVSFLINE", 8))
   {
      SND(" -");
      HDR(" *** SVSFLINE Command ***");
      SND(" -");
      SND(" Adds the given Filename mask to DCCDENY");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax: :server SVSFLINE + file :reason (Add the filename)");
      SND("         :server SVSFLINE - file (Deletes the filename)");
      SND("         :server SVSFLINE *      (Wipes the DCCDENY list)");
      SND(" -");
   }
   else if (!myncmp(help, "SVSMOTD", 8))
   {
      SND(" -");
      HDR(" *** SVSMOTD Command ***");
      SND(" -");
      SND(" Changes the Services Message Of The Day");
      SND(" Must be sent through an U:Lined server.");
      SND(" -");
      SND(" Syntax:  SVSMOTD # :<text> (Adds to Services MOTD)");
      SND("          SVSMOTD !         (Deletes the MOTD)");
      SND("          SVSMOTD ! :<text> (Deletes and Adds text)");
      SND(" Example: SVSMOTD # :Services MOTD");
      SND(" -");
   }

   /* Send the whole damn Command list :)
                       - hAtbLaDe */

	else if (!myncmp(help, "COMMANDS", 8))
	{
		HDR("***** Full Command List *****");
		SND("Full command list, in the format: Command Token");
		/* Send the command list (with tokens)
		   * -Wizzu
		 */
		for (i = 0; msgtab[i].cmd; i++)
			/* The following command is (almost) the same as the SND
			 * macro, but includes variables.  -Wizzu
			 */
			if (sptr != NULL)
				sendto_one(sptr, ":%s 291 %s :%s %s",
				    me.name, name, msgtab[i].cmd,
				    msgtab[i].token);
			else
				printf("%s %s\n", msgtab[i].cmd,
				    msgtab[i].token);

		if (sptr != NULL)
			sendto_one(sptr,
			    ":%s 291 %s :End of Command list - %i commands shown",
			    me.name, name, (i - 1));
		else
			printf("End of Command list - %i commands shown\n",
			    (i - 1));

	}

	else          /* When no argument is specified .. - hAtbLaDe */
	{
      HLP(" -");
	  HLP(" For help with the Services :");
      HLP(" -");
	  HLP("   /MSG NickServ Help - Help on Registering Nicknames.");
	  HLP("   /MSG ChanServ Help - Help on Registering Channels.");
	  HLP("   /MSG MemoServ Help - Help on sending short Messages.");
      HLP(" -");
	  HLP(" If you are using ircII, use /QUOTE HELPOP instead of /HELPOP,");
      HLP(" to prevent your client from trying to interpret the command.");
      HLP(" -");
		sendto_one(sptr,":%s %i %s : ***** Go to %s if you have any further questions *****",
		    me.name, 292, sptr->name, helpchan);
		return 0;
	}
/*	if (sptr)
		sendto_one(sptr,"***** Go to %s if you have any further questions *****",
	    me.name, 292, sptr->name, helpchan);
*/
	return 1;
}
