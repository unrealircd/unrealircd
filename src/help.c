/************************************************************************
 *   IRC - Internet Relay Chat, ircd/help.c
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
 * Updated to use UnrealIRCd commands -Techie/Stskeeps
 *
*/
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"

ID_CVS("$Id$");
ID_Copyright("DALnet & Techie");
ID_Notes("6.20 7/5/99");

void xx(sptr, str, num)
aClient *sptr;
char *str;
int num;
{
	if (sptr == NULL) {
		printf("%s\n", str);
		return;
	}
		sendto_one(sptr, ":%s %i %s :%s", me.name, num, sptr->name, str);
}

#define HDR(str) xx(sptr, str, 290)
#define SND(str) xx(sptr, str, 291)
#define FTR(str) xx(sptr, str, 292)
#define HLP(str) xx(sptr, str, 293)

/*
 * This is _not_ the final help.c, we're just testing the functionality...
 * Return 0 to forward request to helpops... -Donwulff
 */
extern struct Message msgtab[];
int parse_help (sptr, name, help)
aClient	*sptr;
char	*name;
char	*help;
{

   int i;  
//	sendto_one(sptr, ":%s NOTICE %s :%s", me.name, sptr->name, help);

  if(BadPtr(help)) {
HDR(" ***** UnrealIRCd Help System *****");
SND("  You need to specify your question after the help-command");
SND("  If the IRC Server is unable to satisfy your help-request");
SND("  it will be forwarded to appropriate people for handling.");
SND("  Preceed your question with ! to automatically forward it to");
SND("  qualified helpers, or ? to never forward it.");
SND("  Else type /HELPOP CMDS to get some help on the IRCd");
SND("  /HELPOP ABOUT gets some info about UnrealIRCd");
  }
   else if (!myncmp(help, "CMDS", 8)) {
   	  HDR(" *** UnrealIRCd Commands *** ");
   	  SND(" Currently help is available on these commands ");
	  SND(" Use /HelpOp <command name> to get info about the command");
   	  SND(" WATCH      HELPOP    LIST");
   	  SND(" PRIVMSG    KNOCK     LICENSE");
   	  SND(" SETNAME    MODE      STATSERV");
   	  SND(" CREDITS    DALINFO");
   	  SND(" ----------------------------");
   	  SND(" OPERCMDS   - IRCop Commands");
   	  SND(" UMODES     - Usermodes");
   	  SND(" CHMODES    - Channelmodes");
	  SND(" OFLAGS     - O:Line flags");
	  SND(" -----------oOo------------");
   }
  else if (!myncmp(help, "ABOUT", 8)) {
  	HDR(" *** About UnrealIRCd ***");
  	SND("I started making UnrealIRCd about 3-4 months ago.");
  	SND("First it was called mpx2.0b13.soundforge - as I was");
  	SND("inspired of the 'forge' word. I quickly changed name");
  	SND("after I realized the IRCd had more potential for other");
  	SND("IRC nets. Unreal is based off df4.6.5 and some of");
  	SND("First lemme");
  	SND("introduce myself. My nick is Techie/Stskeeps. I hang");
  	SND("out at Global-IRC.net, DALnet, DragonWings.org and so on");
  	SND("--");
	SND("Unreal is a hybrid of Dreamforge (as I said) mixed with some");
	SND("Twilight IRCd, TerraIRCd, TS4 (channel mode +h & +e) features");
	SND("(IMHO TwilightIRCd is one of the best dreamforge hybrids I've seen!)");
	SND("Unreal is not a rip-off of other IRCds - I added a lot of features myself!");
	SND("I really cannot mention some major features as I think a IRCd is a");
	SND("IRCd when it has got useful commands- and people actually say mine has!");
	SND("--");
	SND("Anyways I dunt demand donations or anything. I just do coding for fun");
	SND("I treat coding like playing with toys. It makes me happy(or is it just");
	SND("caffeine?;). An addy to send donations is at the Donation file in the IRCd dir");
	SND("I would be more happy if someone e-mailed me with ideas");
	SND("to the IRCd.. The whole IRCd is GNU so if you want to rip off any of my ideas");
	SND("You are generally welcome:) Just remember to do what's said in the Changes file!");
	SND("-- So.. Enjoy this IRCd:)");
	SND("   -- Carsten Munk / Techie .. =)");	
  }
   else if (!myncmp(help, "WATCH", 8)) {
      HDR(" *** WATCH Command ***");
	SND("Watch is a new notify-type system in UnrealIRCd which is both faster");
	SND("and uses less network resources than any old-style notify");
	SND("system. You may add entries to your Watch list with the command");
	SND("/watch +nick1 [+nick2 +nick3 ..., and the server will send");
	SND("you a message when any nickname in your watch list logs on or off.");
	SND("Use /watch -nick to remove a nickname from the watch list, and");
	SND("just /watch to view your watch list.");
	SND("The watch list DOES NOT REMAIN BETWEEN SESSIONS - you (or your");
	SND("script or client) must add the nicknames to your watch list every");
	SND("time you connect to an IRC server. /Watch was made in DreamForge IRCd");
	SND("which UnrealIRCd in ground is based off");
   }
   else if (!myncmp(help, "HELPOP", 8)) {
   	HDR(" *** HELPOP Command ***");
   	SND("HelpOp is a new system of getting IRC Server help. You type either");
   	SND("/HelpOp ? <help system topic>  or /HelpOp ! <question>");
   	SND("The ï?ï in /HelpOp means query the help system and if you get no");
   	SND("response you can choose '!' to send it to the Help Operators online");
   	SND("------------oOo--------------");
   }
   else if (!myncmp(help, "LIST", 8)) {
	    HDR(" *** LIST Command ***");
		SND("New extended /list command options are supported.  To use these");
		SND("features, you will likely need to prefix the LIST command with");
		SND("/quote to avoid your client interpreting the command.");
		SND("");
		SND("Usage: /quote LIST options");
		SND("");
		SND("If you don't include any options, the default is to send you the");
		SND("entire unfiltered list of channels. Below are the options you can");
		SND("use, and what channels LIST will return when you use them.");
		SND(">number  List channels with more than <number> people.");
		SND("<number  List channels with less than <number> people.");
		SND("C>number List channels created between now and <number> minutes ago.");
		SND("C<number List channels created earlier than <number> minutes ago.");
		SND("T>number List channels whose topics are older than <number> minutes");
		SND("         (Ie., they have not changed in the last <number> minutes.");
		SND("T<number List channels whose topics are newer than <number> minutes.");
		SND("*mask*   List channels that match *mask*");
		SND("!*mask*  List channels that do not match *mask*");
		SND("LIST defaults to sending a list of channels with 2 or more members,");
		SND("so use the >0 option to get the full channel listing.");  
    }
	else if (!myncmp(help, "PRIVMSG", 8)) {
		HDR("*** PRIVMSG Command ***");
		SND("PRIVMSG and NOTICE, which are used internally by the client for");
		SND("/msg and /notice, in UnrealIRCd support two additional formats:");
		SND("/msg @#channel <text> will send the text to channel-ops on the");
		SND("given channel only. /msg @+#channel <text> will send the text");
		SND("to both ops and voiced users on the channel. While some clients");
		SND("may support these as-is, on others (such as ircII), it's necessary");
		SND("to use/quote privmsg @#channel <text> instead. It's perhaps a");
		SND("good idea to add the/alias omsg /quote privmsg @$0 $1 into");
		SND("your script (.ircrc) file in that case.");
	}
   else if (!myncmp(help, "KNOCK", 8)) {
		HDR("**** KNOCK Command ****");
		SND("/Knock is a new UnrealIRCd command which enables you to");
		SND("'knock' on a channel if it is +i and these criteria is met");
		SND("- Channel is not +K (No knocks)");
		SND("- Channel is not +I (No invites!)");
		SND("- You're not banned!");
		SND("- And you are not already there:)");
		SND("Syntax:");
		SND(" /Knock #Channel :Reason");
	}

   else if (!myncmp(help, "LICENSE", 8)) {
   		HDR("*** LICENSE Command ***");
   		SND("This command shows the GNU License");
   		SND("Which is hard-coded into the IRCd");
		SND("Syntax: /License [optional server]");
   }
   else if (!myncmp(help, "SETNAME", 8)) {
   	    HDR("*** SetName Command ***");
   	    SND("/SetName is a new feature in UnrealIRCd");
   	    SND("Which allows users to change their 'Real name'");
   	    SND(" (GECOS) directly online at IRC without reconnecting");
   	    SND("Syntax:");
   	    SND(" /SetName :New Real Name");
   }
   else if (!myncmp(help, "MODE", 8)) {
   		HDR("*** MODE Command ***");
   		SND("This is basically the /mode command as it has always");
   		SND("been on IRC. Thou in Channel mode basis it has got an");
   		SND("Extra feature (/mode #Channel ^ ) which reports channel");
   		SND("modes represented in a bitstring (may be handy, maybe not)");
   		SND("UnrealIRCd has got some new channel / usermodes I think you");
   		SND("wish to take a look at");
   		SND("Channel Modes Help: /HelpOp CHMODES");
   		SND("User modes help: /HelpOp UMODES");
   }
   else if (!myncmp(help, "STATSERV", 8)) {
   		HDR("*** STATSERV Command ***");
   		SND("This is a alias for the /msg StatServ command,");
   		SND("But is more secure. If the IRC network doesn't have StatServ");
   		SND("It will report it is down.");
   		SND("Syntax:");
   		SND("/StatServ <command>");
   }
   else if (!myncmp(help, "CREDITS", 8)) {
   		HDR("*** /Credits Help ***");
   		SND("This command will list the credits I've created");
   		SND("to thank the people who has helped me with making");
   		SND("UnrealIRCd. Anyone who I've forgotten all my kind");
   		SND("thoughts go to -- Techie'99");
   		SND("Syntax:");
   		SND(" /Credits [optional server]");
   }
   else if (!myncmp(help, "DALINFO", 8)) {
   		HDR("*** /DALINFO Help ***");
   		SND("This command will list the credits that the");
   		SND("Dreamforge IRCd team/the IRCd developers");
   		SND("from the start when IRCd got developed");
   		SND("Syntax:");
   		SND(" /DALInfo [optional server]");
   }
   else if (!myncmp(help, "OPERCMDS", 8)) {
   		HDR("*** IRCop Commands Help ***");
   		SND("This section is the IRCOp's only commands");
   		SND("area:) - These topics are available:");
   		SND("Note: This doesnt include Dreamforge commands");
   		SND("-------------oOo---------------");
   		SND("SETHOST    SETIDENT     SDESC");
   		SND("ADCHAT     NACHAT       TECHAT");
   		SND("GLINE      REMGLINE     STATS");
   		SND("MKPASSWD   SNOTES       SNOTE");
   		SND("ADDLINE    LAG          RPING");
		SND("ADDMOTD    ADDOMOTD     OPERMOTD");
		SND("CHGHOST");
		SND("-------------oOo---------------");
   }	
   else if (!myncmp(help, "ADDMOTD", 8)) {
   		HDR("*** ADDMOTD Command Help ***");
   		SND("This will add the text you specify to the MOTD");
   		SND("(the general motd - T:lines doesnt count ..)");
   		SND("Server Admin & Co-Admin only");
   		SND("-");
   		SND("Syntax: /ADDMOTD :text");
   }
   else if (!myncmp(help, "ADDOMOTD", 8)) {
   		HDR("*** ADDOMOTD Command Help ***");
   		SND("This will add the text you specify to the Operator MOTD");
   		SND("Server Admin & Co-Admin only");
   		SND("-");
   		SND("Syntax: /ADDOMOTD :text");
   }
   else if (!myncmp(help, "CHGHOST", 8)) 
   {
   	HDR("*** CHGHOST Command help ***");
   	SND("This command makes you able to change other people's virtual hostname");
   	SND("- IRCop only.");
   	SND("Syntax: /CHGHOST <nick> <newhost>");
   }

   else if (!myncmp(help, "OPERMOTD", 8)) {
   		HDR("*** OPERMOTD Command Help ***");
		HDR("This is a IRCop only command - shows the IRCd Operator MOTD");
		HDR("Syntax: /OperMotd");
   }
   else if (!myncmp(help, "SETHOST", 8)) {
   		HDR("*** SETHOST Command Help ***");
   		SND("This command is so you can change your");
   		SND("Virtual host (hiddenhost) to everything you want to");
   		SND("Except special characters;). Syntax:");
   		SND("/SetHost <new hostname>)");
   		SND("Example:");
   		SND(" /Sethost ircaddicts.org");
   }
   else if (!myncmp(help, "SETIDENT", 8)) {
   		HDR("*** SETIDENT Command Help ***");
   		SND("With this command you can change your");
   		SND("ident (username).");
   		SND("Syntax:");
   		SND("/SetIdent <new ident>");
		SND("Example:");
		SND(" /SetIdent root");
   }
   else if (!myncmp(help, "SDESC", 8)) {
   		HDR("*** SDesc Command help ***");
   		SND("NOTE: This is a Server Admin/Co Admin only command");
   		SND("With this command you can change your Server Info Line");
   		SND("Without having to squit and reconnect.");
   		SND("Syntax: /SDesc :New description");
   		SND("Example: /SDesc :If you belong to me..");		
   }
   else if (!myncmp(help, "ADCHAT", 8)) {
   		HDR("*** AdChat Command Help ***");
   		SND("This command sends to all Admins online (IsAdmin)");
   		SND("Only for Admins. This is a ChatOps style command");
   		SND("Syntax: /AdChat :<text>");
		SND("Example: /AdChat :Hey guys!");
   }
   else if (!myncmp(help, "NACHAT", 8)) {
   		HDR("*** NAChat Command Help ***");
   		SND("This command sends to all NetAdmins & TechAdmins online");
   		SND("Only for Net/Techadmins. This is a ChatOps style command");
   		SND("Syntax: /NAChat :<text>");
		SND("Example: /NAChat :Hey guys!");
   }

 		  else if (!myncmp(help, "STATS", 8)) {
   		HDR("*** Stats Command Help ***");
   		SND("UnrealIRCd has got a extension called /Stats G");
   		SND("Which will list the current G:Lines");
   		SND("Syntax: /Stats G");
   }
   else if (!myncmp(help, "TECHAT", 8)) {
   		HDR("*** TEChat Command Help ***");
   		SND("This command sends to all TechAdmins online");
   		SND("Only for Net/Techadmins. This is a ChatOps style command");
   		SND("Syntax: /TEChat :<text>");
		SND("Example: /TEChat :Hey guys!");
   }
   else if (!myncmp(help, "REMGLINE", 8)) {
   		HDR("*** RemGline Command Help");
   		SND("This command can remove G:Lines");
   		SND("Syntax:");
   		SND(" /RemGline <user@host mask>");
   		SND("Example:");
   		SND(" /RemGline *@*.flirt.org");
   }
   else if (!myncmp(help, "GLINE", 8)) {
   		HDR("*** G:line command Help ***");
   		SND("This command provides timed G:Lines. If you match");
   		SND("a G:Line you cannot connect to ANY server at the");
   		SND("IRC network");
   		SND("Syntax:");
   		SND(" /GLINE <user@host mask> <seconds to be banned> :<reason>");
   		SND("Example:");
   		SND(" /GLINE *@*.dal.net 900 :Spammers");
   		SND("  this will ban all users matching *@*.dal.net for 15 minutes");
   		SND("  with reason 'Spammers'");
   }
   else if (!myncmp(help, "MKPASSWD", 8)) {
   		HDR("*** MkPasswd Command help ***");
   		SND("This command will encrypt the string it has been given");
   		SND("So u can add it directly to the ircd.conf if you use");
   		SND("Encrypted passwords. /MKPassWd is disabled in UnrealIRCd/32");
		SND("Syntax : /MkPasswd :string to be encrypted");
   }
   else if (!myncmp(help, "SNOTE", 8)) {
   		HDR("*** SNOTE Command Help ***");
   		SND("This will store the parameter of the command to a file");
   		SND("Which then can be read by using /SNOTES LIST");
		SND("Syntax:  /SNOTE :<message>");
   }
   else if (!myncmp(help, "SNOTES", 8)) {
   		HDR("*** SNOTES Command Help ***");
   		SND("This command is made to view notes");
   		SND("Written to the SNOTE file by using /SNOTE");
   		SND("Syntax: /SNOTES LIST");
   		SND("  or /SNOTES <number>");
  }
  else if (!myncmp(help, "ADDLINE",8)) {
  		HDR("*** ADDLINE Command Help ***");
  		SND("This command can be used to add lines to the ircd.conf file");
  		SND("Only for Server Admins");
  		SND("Syntax: /AddLine :<line>");
  }
  else if (!myncmp(help, "LAG", 8)) {
  		HDR("*** LAG Command Help ***");
  		SND("This command is like a sonar/traceroute for IRC servers");
  		SND("You type in /lag server1.irc.net and it will");
  		SND("reply from every server it passes with time and so on");
  		SND("Useful for looking where lag is and optional TS future/past travels");
		SND("Syntax: /LAG <servername>");
  }
  else if (!myncmp(help, "RPING", 8)) {
  		HDR("*** RPING Command Help ***");
  		SND("This will calculate the milliseconds (lag) between servers");
  		SND("Syntax: /RPING <servermask>");	
  }
  else if (!myncmp(help, "TSCTL", 8)) {
  		HDR("*** TSCTL Command Help ***");
  		SND("This is a highly advanced command");
  		SND("Syntax:");
  		SND(" /TSCTL OFFSET +|- <time> - Adjust internal IRC clock");
  		SND(" /TSCTL TIME - Will give TS report");
  }
  else if (!myncmp(help, "SAJOIN", 8)) {
  		HDR("*** SAJOIN Command help **");
  		SND("Makes <nick> join channel <channel>");
  		SND("Services Admin only..");
  		SND("Syntax: /SAJOIN nick channel");
  }
   else if(!myncmp(help, "UMODES", 8)) {
   		HDR("*** UnrealIRCd Usermodes ***");
		SND("o = Global IRCop");
		SND("O = Local IRCop");
		SND("i = Invisible (Not shown in /who searches)");
		SND("w = Can listen to wallop messages");
		SND("g = Can read & send to globops, and locops");
		SND("h = Available for help");
		SND("s = Can listen to server notices");
		SND("k = See's all the /KILL's which were executed");
		SND("S = For services only. (Protects them)");
		SND("a = Is a services admin");
		SND("A = Is a server admin");
		SND("N = Is a network admin");
		SND("T = Is a tech admin");
		SND("C = Is a co admin");
		SND("c = See's all connects/disconnects on local server");
		SND("f = Listen to flood alerts from server");
		SND("r = Identifies the nick as being registered");
		SND("x = Gives the user hidden hostname");
		SND("e = Can listen to server messages sent to +e users");
		SND("b = Can read & send to chatops");
		SND("W = (IRCops only) Lets you see when people does a /whois on you");
		SND("q = (Services Admins only) Gets you unable to be");
		SND("     kicked unless by U:Lines");
		SND("B = (users) Marks you being a Bot");
		SND("F = (netadmins&techadmins) Lets you recieve far connect notices + local notices");
		SND("I = (netadmins&techadmins) Invisible Join/Part. Makes you being hidden at channels");
		SND("1 = (ircops only) Marks you being a Coder");
		HDR("---------------------oOo-------------------");		
   }
   else if(!myncmp(help, "CHMODES", 8)) {
		HDR("*** UnrealIRCd Channel Modes ***");
	    SND("p = Private channel");
	    SND("s = Secret channel");
	    SND("i = Invite-only allowed");
	    SND("m = Moderated channel, noone can speak except users with mode +voh");
	    SND("n = No messages from outside channel");
	    SND("t = Only channel operators may set the topic");
	    SND("r = Channel is registered");
	    SND("R = Requires a registered nickname to join the channel");
	    SND("x = No ANSI color can be sent to the channel");
	    SND("q = Channel owner (The big cheese)");
	    SND("Q = No kicks able in channel unless by U:Lines");
	    SND("O = IRCop only channel (setable by Opers)");
	    SND("A = Server Admin | Network Admin | Tech Admin only channel (same as above)");
	    SND("K = /Knock is not allowed");
	    SND("I = /Invite is not allowed");
	    SND("S = Strip all incoming colours away");
	    SND("l <number of max users> = Channel may hold at most <number> of users");
	    SND("b <nick!user@host>      = Bans the nick!user@host from the channel");
	    SND("k <key>                 = Needs the channel key to join the channel");
	    SND("o <nickname>            = Gives operator status to the user");
	    SND("v <nickname>            = Gives voice to the user (May talk if chan is +m)");
	    SND("L <chan2>               = If +l is full, the next user will auto-join <chan2>");
	    SND("a <nickname>            = Gives protection to the user (No kick/drop)");
		SND("e <exception ban>       = Exception ban - If someone matches it");
		SND("                          they can join even if some else ban matches!");
		SND("h <nickname>            = Gives halfop status to the user");	    
		HDR("You can get additional explanation on modes:");
		SND(" Q h");
		SND("With /HELPOP mode-<x> where <x> is Q f.x. like mode-Q");
   }
   else if (!myncmp(help, "mode-Q", 8)) {
   		HDR("*** Channel mode +Q ***");
   		SND("This is the 'peace' mode. Noone can kick eachother");
   		SND("except by U:Lines. Bans can be placed thou.");
   }
   else if (!myncmp(help, "MODE-h", 8)) {
  		HDR("*** Channel halfops (+h) *** ");
  		SND("If you are marked as halfop (% in /names) you can do:");
  		SND(" - Set topic");
  		SND(" - Kick non-ops");
  		SND(" - Set modes +vmntibe");
   }
   else if(!myncmp(help, "OFLAGS", 8)) {
		HDR("*** UnrealIRCd O:Line flags ***");
		SND("r = Access to /rehash server");
		SND("R = Access to /restart server");
		SND("D = Access to /die server");
		SND("h = Oper can send /help ops - gets +h on oper up");
		SND("g = Oper can send /globops");
		SND("w = Oper can send /wallops");
		SND("l = Oper can send /locops");
		SND("c = Access to do local /squits and /connects");
		SND("Y = Access to do remote /squits and /connects");
		SND("k = Access to do local /kills");
		SND("K = Access to do global /kills");
		SND("b = Oper can /kline users from server");
		SND("B = Oper can /unkline users from server");
		SND("n = Oper can send local server notices(/notice $servername message)");
		SND("N = Oper can send global notices(/notice $*.network.net message)");
		SND("u = Oper can set /umode +c");
		SND("f = Oper can set /umode +f");
		SND("o = Local oper, flags included: rhgwlckbBnuf");
		SND("O = Global oper, flags included: oRDCKN");
		SND("A = Gets +A on oper up. Is server admin");
		SND("a = Gets +a on oper up. Is services admin");
		SND("N = Gets +N on oper up. Is network admin");
		SND("T = Gets +T on oper up. Is tech admin");
		SND("C = Gets +C on oper up. Is co admin");
		SND("z = Can add /zlines");
		SND("H = Gets +x on oper up.");
		SND("W = Gets +W on oper up.");	   
		SND("^ = Allows to use umode +I"); 
		SND("----------oOo-----------");
   }
   else if(!myncmp(help, "COMMANDS", 8)) {
		HDR("***** Full Command List *****");
		SND("Full command list, in the format: command token");
		/* Send the command list (with tokens)
	 	* -Wizzu
	 	*/
		for (i = 0; msgtab[i].cmd; i++)
		/* The following command is (almost) the same as the SND
		 * macro, but includes variables.  -Wizzu
		 */
			if (sptr != NULL)
				sendto_one(sptr,":%s 291 %s :%s %s",
					me.name, name, msgtab[i].cmd, msgtab[i].token);
			else
				printf("%s %s\n",
					msgtab[i].cmd, msgtab[i].token);

		if (sptr != NULL)
			sendto_one(sptr, ":%s 291 %s :End of command list - %i commands shown",
				me.name, name, (i - 1));
		else
			printf("End of command list - %i commands shown\n",(i -1));

	} 

   else { /* Flood back the user ;) */
    HLP("If you need help on services, try...");
    HLP("  /helpop nickserv - for help on registering nicknames.");
    HLP("  /helpop chanserv - for help on registering channels.");
    HLP("  /helpop memoserv - for help on sending short messages.");
    HLP("If you are using ircII, use /quote helpop instead of /helpops.");
	sendto_one(sptr, ":%s %i %s : ***** Go to %s if you have any further questions *****", me.name, 292, sptr->name, helpchan);
    return 0;
  }
  sendto_one(sptr, ":%s %i %s : ***** Go to %s if you have any further questions *****", me.name, 292, sptr->name, helpchan);

  return 1;
}
