/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/numeric.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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
 *
 *   $Id$
 */

/*
 * Added numerics 600-799 as numeric_replies2[], we ran out
 */

/*
 * Reserve numerics 000-099 for server-client connections where the client
 * is local to the server. If any server is passed a numeric in this range
 * from another server then it is remapped to 100-199.
 */

#define	RPL_WELCOME          001
#define	RPL_YOURHOST         002
#define	RPL_CREATED          003
#define	RPL_MYINFO           004
#define RPL_ISUPPORT	     005

#define RPL_REDIR	     10
#define RPL_YOURID           42

#define RPL_REMOTEISUPPORT 105

/*
 * Errors are in the range from 400-599 currently and are grouped by what
 * commands they come from.
 */
#define ERR_NOSUCHNICK       401
#define ERR_NOSUCHSERVER     402
#define ERR_NOSUCHCHANNEL    403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_TOOMANYCHANNELS  405
#define ERR_WASNOSUCHNICK    406
#define ERR_TOOMANYTARGETS   407
#define ERR_NOSUCHSERVICE    408
#define	ERR_NOORIGIN         409

#define ERR_INVALIDCAPCMD    410

#define ERR_NORECIPIENT      411
#define ERR_NOTEXTTOSEND     412
#define ERR_NOTOPLEVEL       413
#define ERR_WILDTOPLEVEL     414
#define ERR_TOOMANYMATCHES   416

#define ERR_UNKNOWNCOMMAND   421
#define	ERR_NOMOTD           422
#define	ERR_NOADMININFO      423
#define	ERR_FILEERROR        424
#define ERR_NOOPERMOTD	     425
#define ERR_TOOMANYAWAY	     429
#define ERR_NONICKNAMEGIVEN  431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433
#define ERR_NORULES          434
#define ERR_SERVICECONFUSED  435
#define	ERR_NICKCOLLISION    436
#define ERR_BANNICKCHANGE    437
#define ERR_NCHANGETOOFAST   438
#define ERR_TARGETTOOFAST    439
#define ERR_SERVICESDOWN     440

#define ERR_USERNOTINCHANNEL 441
#define ERR_NOTONCHANNEL     442
#define	ERR_USERONCHANNEL    443
#define ERR_NOLOGIN          444
#define	ERR_SUMMONDISABLED   445
#define ERR_USERSDISABLED    446
#define ERR_NONICKCHANGE     447
#define ERR_FORBIDDENCHANNEL 448


#define ERR_NOTREGISTERED    451

#define ERR_HOSTILENAME      455

#define ERR_NOHIDING	     459
#define ERR_NOTFORHALFOPS	 460
#define ERR_NEEDMOREPARAMS   461
#define ERR_ALREADYREGISTRED 462
#define ERR_NOPERMFORHOST    463
#define ERR_PASSWDMISMATCH   464
#define ERR_YOUREBANNEDCREEP 465
#define ERR_YOUWILLBEBANNED  466
#define	ERR_KEYSET           467
#define ERR_ONLYSERVERSCANCHANGE 468
#define ERR_LINKSET	     469
#define ERR_LINKCHANNEL	     470
#define ERR_CHANNELISFULL    471
#define ERR_UNKNOWNMODE      472
#define ERR_INVITEONLYCHAN   473
#define ERR_BANNEDFROMCHAN   474
#define	ERR_BADCHANNELKEY    475
#define	ERR_BADCHANMASK      476
#define ERR_NEEDREGGEDNICK   477
#define ERR_BANLISTFULL      478
#define ERR_LINKFAIL	     479
#define ERR_CANNOTKNOCK		 480

#define ERR_NOPRIVILEGES     481
#define ERR_CHANOPRIVSNEEDED 482
#define	ERR_CANTKILLSERVER   483
#define ERR_ATTACKDENY       484
#define ERR_KILLDENY	     485

#define ERR_NONONREG	     486
#define ERR_NOTFORUSERS	    487

#define ERR_SECUREONLYCHAN   489
#define ERR_NOSWEAR	     490
#define ERR_NOOPERHOST       491
#define ERR_NOCTCP	     492

#define ERR_CHANOWNPRIVNEEDED 499

#define ERR_TOOMANYJOINS     500
#define ERR_UMODEUNKNOWNFLAG 501
#define ERR_USERSDONTMATCH   502

#define ERR_SILELISTFULL     511
#define ERR_TOOMANYWATCH     512
#define ERR_NEEDPONG         513

#define ERR_TOOMANYDCC       514

#define ERR_DISABLED         517
#define ERR_NOINVITE		 518
#define ERR_ADMONLY			 519
#define ERR_OPERONLY		 520
#define ERR_LISTSYNTAX       521

#define ERR_CANTSENDTOUSER	531
/*
 * Numberic replies from server commands.
 * These are currently in the range 200-399.
 */
#define	RPL_NONE             300
#define RPL_AWAY             301
#define RPL_USERHOST         302
#define RPL_ISON             303
#define RPL_TEXT             304
#define	RPL_UNAWAY           305
#define	RPL_NOWAWAY          306
#define RPL_WHOISREGNICK     307
#define RPL_RULESSTART       308
#define RPL_ENDOFRULES       309
#define RPL_WHOISHELPOP      310	/* -Donwulff */

#define RPL_WHOISUSER        311
#define RPL_WHOISSERVER      312
#define RPL_WHOISOPERATOR    313

#define RPL_WHOWASUSER       314
/* rpl_endofwho below (315) */
#define	RPL_ENDOFWHOWAS      369

#define RPL_WHOISCHANOP      316	/* redundant and not needed but reserved */
#define RPL_WHOISIDLE        317

#define RPL_ENDOFWHOIS       318
#define RPL_WHOISCHANNELS    319
#define RPL_WHOISSPECIAL     320
#define RPL_LISTSTART        321
#define RPL_LIST             322
#define RPL_LISTEND          323
#define RPL_CHANNELMODEIS    324
#define RPL_CREATIONTIME     329

#define RPL_WHOISLOGGEDIN    330	/* ircu/charybdis-family --nenolod */

#define RPL_NOTOPIC          331
#define RPL_TOPIC            332
#define RPL_TOPICWHOTIME     333

#define RPL_INVITELIST       336
#define RPL_ENDOFINVITELIST  337

#define RPL_LISTSYNTAX       334
#define RPL_WHOISBOT	     335
#define RPL_USERIP	     340
#define RPL_INVITING         341
#define	RPL_SUMMONING        342
#define RPL_WHOISCOUNTRY     344

#define RPL_VERSION          351

#define RPL_WHOREPLY         352
#define RPL_ENDOFWHO         315
#define RPL_NAMREPLY         353
#define RPL_ENDOFNAMES       366
#define RPL_INVEXLIST	     346
#define RPL_ENDOFINVEXLIST   347

#define RPL_EXLIST	     348
#define RPL_ENDOFEXLIST      349
#define RPL_KILLDONE         361
#define	RPL_CLOSING          362
#define RPL_CLOSEEND         363
#define RPL_LINKS            364
#define RPL_ENDOFLINKS       365
/* rpl_endofnames above (366) */
#define RPL_BANLIST          367
#define RPL_ENDOFBANLIST     368
/* rpl_endofwhowas above (369) */

#define	RPL_INFO             371
#define	RPL_MOTD             372
#define	RPL_INFOSTART        373
#define	RPL_ENDOFINFO        374
#define	RPL_MOTDSTART        375
#define	RPL_ENDOFMOTD        376

#define RPL_WHOISHOST        378
#define RPL_WHOISMODES       379
#define RPL_YOUREOPER        381
#define RPL_REHASHING        382
#define RPL_YOURESERVICE     383
#define RPL_MYPORTIS         384
#define RPL_NOTOPERANYMORE   385
#define RPL_QLIST			 386
#define	RPL_ENDOFQLIST		 387
#define	RPL_ALIST			 388
#define	RPL_ENDOFALIST		 389

#define RPL_TIME             391
#define	RPL_USERSSTART       392
#define	RPL_USERS            393
#define	RPL_ENDOFUSERS       394
#define	RPL_NOUSERS          395
#define RPL_HOSTHIDDEN       396

#define RPL_TRACELINK        200
#define RPL_TRACECONNECTING  201
#define RPL_TRACEHANDSHAKE   202
#define RPL_TRACEUNKNOWN     203

#define RPL_TRACEOPERATOR    204
#define RPL_TRACEUSER        205
#define RPL_TRACESERVER      206
#define RPL_TRACESERVICE     207
#define RPL_TRACENEWTYPE     208
#define RPL_TRACECLASS       209

#define RPL_STATSHELP	     210
#define RPL_STATSLINKINFO    211
#define RPL_STATSCOMMANDS    212
#define RPL_STATSCLINE       213
#define RPL_STATSOLDNLINE    214

#define RPL_STATSILINE       215
#define RPL_STATSKLINE       216
#define RPL_STATSQLINE       217
#define RPL_STATSYLINE       218
#define RPL_ENDOFSTATS       219
#define RPL_STATSBLINE	     220


#define RPL_UMODEIS          221
#define RPL_SQLINE_NICK      222
#define RPL_STATSGLINE		 223
#define RPL_STATSTLINE		 224
#define RPL_STATSELINE	     225
#define RPL_STATSNLINE	     226
#define RPL_STATSVLINE	     227
#define RPL_STATSBANVER	     228
#define RPL_STATSSPAMF       229
#define RPL_STATSEXCEPTTKL   230
#define RPL_SERVICEINFO      231
#define RPL_RULES            232
#define	RPL_SERVICE          233
#define RPL_SERVLIST         234
#define RPL_SERVLISTEND      235

#define	RPL_STATSLLINE       241
#define	RPL_STATSUPTIME      242
#define	RPL_STATSOLINE       243
#define	RPL_STATSHLINE       244
#define	RPL_STATSSLINE       245
#define RPL_STATSXLINE	     247
#define RPL_STATSULINE       248
#define	RPL_STATSDEBUG	     249
#define RPL_STATSCONN        250

#define	RPL_LUSERCLIENT      251
#define RPL_LUSEROP          252
#define	RPL_LUSERUNKNOWN     253
#define	RPL_LUSERCHANNELS    254
#define	RPL_LUSERME          255
#define	RPL_ADMINME          256
#define	RPL_ADMINLOC1        257
#define	RPL_ADMINLOC2        258
#define	RPL_ADMINEMAIL       259

#define	RPL_TRACELOG         261
#define RPL_TRYAGAIN         263
#define RPL_LOCALUSERS       265
#define RPL_GLOBALUSERS      266

#define RPL_SILELIST         271
#define RPL_ENDOFSILELIST    272
#define RPL_STATSDLINE       275
#define RPL_WHOISCERTFP      276

#define RPL_HELPHDR	     290
#define RPL_HELPOP	     291
#define RPL_HELPTLR	     292
#define RPL_HELPHLP	     293
#define RPL_HELPFWD	     294
#define RPL_HELPIGN	     295


/*
 * New /MAP format.
 */
#define RPL_MAP              006
#define RPL_MAPMORE          610
#define RPL_MAPEND           007


#define ERR_WHOSYNTAX 522
#define ERR_WHOLIMEXCEED 523
#define ERR_OPERSPVERIFY 524

#define RPL_SNOMASK	     8

/*
 * Numberic replies from server commands.
 * These are also in the range 600-799.
 */

#define RPL_REAWAY           597
#define RPL_GONEAWAY         598
#define RPL_NOTAWAY          599
#define RPL_LOGON            600
#define RPL_LOGOFF           601
#define RPL_WATCHOFF         602
#define RPL_WATCHSTAT        603
#define RPL_NOWON            604
#define RPL_NOWOFF           605
#define RPL_WATCHLIST        606
#define RPL_ENDOFWATCHLIST   607
#define RPL_CLEARWATCH       608
#define RPL_NOWISAWAY        609

#define RPL_DCCSTATUS        617
#define RPL_DCCLIST          618
#define RPL_ENDOFDCCLIST     619
#define RPL_DCCINFO          620

#define RPL_DUMPING			 640
#define RPL_DUMPRPL			 641
#define RPL_EODUMP           642

#define RPL_SPAMCMDFWD       659

#define RPL_STARTTLS         670

#define RPL_WHOISSECURE      671

#define RPL_WHOISKEYVALUE    760
#define RPL_KEYVALUE         761
#define RPL_METADATAEND      762
#define ERR_METADATALIMIT    764
#define ERR_TARGETINVALID    765
#define ERR_NOMATCHINGKEY    766
#define ERR_KEYINVALID       767
#define ERR_KEYNOTSET        768
#define ERR_KEYNOPERMISSION  769
#define RPL_METADATASUBOK    770
#define RPL_METADATAUNSUBOK  771
#define RPL_METADATASUBS     772
#define ERR_METADATATOOMANYSUBS 773
#define ERR_METADATASYNCLATER 774
#define ERR_METADATARATELIMIT 775
#define ERR_METADATAINVALIDSUBCOMMAND 776

#define RPL_MONONLINE		730
#define RPL_MONOFFLINE		731
#define RPL_MONLIST			732
#define RPL_ENDOFMONLIST	733
#define ERR_MONLISTFULL		734

#define ERR_MLOCKRESTRICTED	742

#define ERR_CANNOTDOCOMMAND 972
#define ERR_CANNOTCHANGECHANMODE 974

#define ERR_STARTTLS            691

#define ERR_INVALIDMODEPARAM	696

#define RPL_LOGGEDIN            900
#define RPL_LOGGEDOUT           901
#define ERR_NICKLOCKED          902

#define RPL_SASLSUCCESS         903
#define ERR_SASLFAIL            904
#define ERR_SASLTOOLONG         905
#define ERR_SASLABORTED         906
#define ERR_SASLALREADY         907
#define RPL_SASLMECHS           908

#define ERR_NUMERICERR       999

/* Numeric texts */

#define STR_RPL_WELCOME		":Welcome to the %s IRC Network %s!%s@%s"
#define STR_RPL_YOURHOST		":Your host is %s, running version %s"
#define STR_RPL_CREATED		":This server was created %s"
#define STR_RPL_MYINFO		"%s %s %s %s"
#define STR_RPL_ISUPPORT		"%s :are supported by this server"
#define STR_RPL_MAP		":%s%-*s(%ld) %s"
#define STR_RPL_MAPEND		":End of /MAP"
#define STR_RPL_SNOMASK		"%s :Server notice mask"
#define STR_RPL_REDIR		"%s %d :Please use this Server/Port instead"
#define STR_RPL_YOURID		"%s :your unique ID"
#define STR_RPL_REMOTEISUPPORT		"%s :are supported by this server"
#define STR_RPL_TRACELINK		"Link %s%s %s %s"
#define STR_RPL_TRACECONNECTING		"Attempt %s %s"
#define STR_RPL_TRACEHANDSHAKE		"Handshaking %s %s"
#define STR_RPL_TRACEUNKNOWN		"???? %s %s"
#define STR_RPL_TRACEOPERATOR		"Operator %s %s [%s] %ld"
#define STR_RPL_TRACEUSER		"User %s %s [%s] %ld"
#define STR_RPL_TRACESERVER		"Server %s %dS %dC %s %s!%s@%s %ld"
#define STR_RPL_TRACESERVICE		"Service %s %s"
#define STR_RPL_TRACENEWTYPE		"%s 0 %s"
#define STR_RPL_TRACECLASS		"Class %s %d"
#define STR_RPL_STATSHELP		":%s"
#define STR_RPL_STATSCOMMANDS		"%s %u %lu"
#define STR_RPL_STATSCLINE		"%c %s * %s %d %d %s"
#define STR_RPL_STATSOLDNLINE		"%c %s * %s %d %d %s"
#define STR_RPL_STATSILINE		"I %s %s %d %d %s %s %d"
#define STR_RPL_STATSKLINE		"%s %s %s"
#define STR_RPL_STATSQLINE		"%c %s %ld %ld %s :%s"
#define STR_RPL_STATSYLINE		"Y %s %d %d %d %d %d"
#define STR_RPL_ENDOFSTATS		"%c :End of /STATS report"
#define STR_RPL_STATSBLINE		"%c %s %s %s %d %d"
#define STR_RPL_UMODEIS		"%s"
#define STR_RPL_SQLINE_NICK		"%s :%s"
#define STR_RPL_STATSGLINE		"%c %s %li %li %s :%s"
#define STR_RPL_STATSTLINE		"T %s %s %s"
#define STR_RPL_STATSELINE		/* 225    STR_RPL_STATSELINE (we use 230 instead) */ NULL
#define STR_RPL_STATSNLINE		"n %s %s"
#define STR_RPL_STATSVLINE		"v %s %s %s"
#define STR_RPL_STATSBANVER		"%s %s"
#define STR_RPL_STATSSPAMF		"%c %s %s %s %li %li %li %s %s :%s"
#define STR_RPL_STATSEXCEPTTKL		"%s %s %li %li %s :%s"
#define STR_RPL_RULES		":- %s"
#define STR_RPL_STATSLLINE		"%c %s * %s %d %d"
#define STR_RPL_STATSUPTIME		":Server Up %ld days, %ld:%02ld:%02ld"
#define STR_RPL_STATSOLINE		"%c %s * %s %s %s"
#define STR_RPL_STATSHLINE		"%c %s * %s %d %d"
#define STR_RPL_STATSSLINE		"%c %s * %s %d %d"
#define STR_RPL_STATSXLINE		"X %s %d"
#define STR_RPL_STATSULINE		"U %s"
#define STR_RPL_STATSDEBUG		":%s"
#define STR_RPL_STATSCONN		":Highest connection count: %d (%d clients)"
#define STR_RPL_LUSERCLIENT		":There are %d users and %d invisible on %d servers"
#define STR_RPL_LUSEROP		"%d :operator(s) online"
#define STR_RPL_LUSERUNKNOWN		"%d :unknown connection(s)"
#define STR_RPL_LUSERCHANNELS		"%d :channels formed"
#define STR_RPL_LUSERME		":I have %d clients and %d servers"
#define STR_RPL_ADMINME		":Administrative info about %s"
#define STR_RPL_ADMINLOC1		":%s"
#define STR_RPL_ADMINLOC2		":%s"
#define STR_RPL_ADMINEMAIL		":%s"
#define STR_RPL_TRACELOG		"File %s %d"
#define STR_RPL_TRYAGAIN		"%s :Flooding detected. Please wait a while and try again."
#define STR_RPL_LOCALUSERS		"%d %d :Current local users %d, max %d"
#define STR_RPL_GLOBALUSERS		"%d %d :Current global users %d, max %d"
#define STR_RPL_SILELIST		"%s"
#define STR_RPL_ENDOFSILELIST		":End of Silence List"
#define STR_RPL_STATSDLINE		"%c %s %s"
#define STR_RPL_WHOISCERTFP		"%s :has client certificate fingerprint %s"
#define STR_RPL_HELPFWD		":Your help-request has been forwarded to Help Operators"
#define STR_RPL_HELPIGN		":Your address has been ignored from forwarding"
#define STR_RPL_AWAY		"%s :%s"
#define STR_RPL_USERHOST		":%s %s %s %s %s"
#define STR_RPL_ISON		":"
#define STR_RPL_UNAWAY		":You are no longer marked as being away"
#define STR_RPL_NOWAWAY		":You have been marked as being away"
#define STR_RPL_WHOISREGNICK		"%s :is identified for this nick"
#define STR_RPL_RULESSTART		":- %s Server Rules - "
#define STR_RPL_ENDOFRULES		":End of RULES command."
#define STR_RPL_WHOISHELPOP		"%s :is available for help."
#define STR_RPL_WHOISUSER		"%s %s %s * :%s"
#define STR_RPL_WHOISSERVER		"%s %s :%s"
#define STR_RPL_WHOISOPERATOR		"%s :is %s"
#define STR_RPL_WHOWASUSER		"%s %s %s * :%s"
#define STR_RPL_ENDOFWHO		"%s :End of /WHO list."
#define STR_RPL_WHOISIDLE		"%s %ld %ld :seconds idle, signon time"
#define STR_RPL_ENDOFWHOIS		"%s :End of /WHOIS list."
#define STR_RPL_WHOISCHANNELS		"%s :%s"
#define STR_RPL_WHOISSPECIAL		"%s :%s"
#define STR_RPL_LISTSTART		"Channel :Users  Name"
#define STR_RPL_LIST		"%s %d :%s %s"
#define STR_RPL_LISTEND		":End of /LIST"
#define STR_RPL_CHANNELMODEIS		"%s %s %s"
#define STR_RPL_CREATIONTIME		"%s %lu"
#define STR_RPL_WHOISLOGGEDIN		"%s %s :is logged in as"
#define STR_RPL_NOTOPIC		"%s :No topic is set."
#define STR_RPL_TOPIC		"%s :%s"
#define STR_RPL_TOPICWHOTIME		"%s %s %lu"
#define STR_RPL_LISTSYNTAX		":%s"
#define STR_RPL_WHOISBOT		"%s :is a \2Bot\2 on %s"
#define STR_RPL_INVITELIST		":%s"
#define STR_RPL_ENDOFINVITELIST		":End of /INVITE list."
#define STR_RPL_USERIP		":%s %s %s %s %s"
#define STR_RPL_INVITING		"%s %s"
#define STR_RPL_SUMMONING		"%s :User summoned to irc"
#define STR_RPL_WHOISCOUNTRY		"%s %s :is connecting from %s"
#define STR_RPL_INVEXLIST		"%s %s %s %lu"
#define STR_RPL_ENDOFINVEXLIST		"%s :End of Channel Invite List"
#define STR_RPL_EXLIST		"%s %s %s %lu"
#define STR_RPL_ENDOFEXLIST		"%s :End of Channel Exception List"
#define STR_RPL_VERSION		"%s.%s %s :%s%s%s [%s=%d]"
#define STR_RPL_WHOREPLY		"%s %s %s %s %s %s :%d %s"
#define STR_RPL_NAMREPLY		"%s"
#define STR_RPL_CLOSING		"%s :Closed. Status = %d"
#define STR_RPL_CLOSEEND		"%d: Connections Closed"
#define STR_RPL_LINKS		"%s %s :%d %s"
#define STR_RPL_ENDOFLINKS		"%s :End of /LINKS list."
#define STR_RPL_ENDOFNAMES		"%s :End of /NAMES list."
#define STR_RPL_BANLIST		"%s %s %s %lu"
#define STR_RPL_ENDOFBANLIST		"%s :End of Channel Ban List"
#define STR_RPL_ENDOFWHOWAS		"%s :End of WHOWAS"
#define STR_RPL_INFO		":%s"
#define STR_RPL_MOTD		":- %s"
#define STR_RPL_INFOSTART		":Server INFO"
#define STR_RPL_ENDOFINFO		":End of /INFO list."
#define STR_RPL_MOTDSTART		":- %s Message of the Day - "
#define STR_RPL_ENDOFMOTD		":End of /MOTD command."
#define STR_RPL_WHOISHOST		"%s :is connecting from %s@%s %s"
#define STR_RPL_WHOISMODES		"%s :is using modes %s %s"
#define STR_RPL_YOUREOPER		":You are now an IRC Operator"
#define STR_RPL_REHASHING		"%s :Rehashing"
#define STR_RPL_MYPORTIS		"%d :Port to local server is\r\n"
#define STR_RPL_QLIST		"%s %s"
#define STR_RPL_ENDOFQLIST		"%s :End of Channel Owner List"
#define STR_RPL_ALIST		"%s %s"
#define STR_RPL_ENDOFALIST		"%s :End of Protected User List"
#define STR_RPL_TIME		"%s :%s"
#define STR_RPL_HOSTHIDDEN		"%s :is now your displayed host"
#define STR_ERR_NOSUCHNICK		"%s :No such nick/channel"
#define STR_ERR_NOSUCHSERVER		"%s :No such server"
#define STR_ERR_NOSUCHCHANNEL		"%s :No such channel"
#define STR_ERR_CANNOTSENDTOCHAN		"%s :%s (%s)"
#define STR_ERR_TOOMANYCHANNELS		"%s :You have joined too many channels"
#define STR_ERR_WASNOSUCHNICK		"%s :There was no such nickname"
#define STR_ERR_TOOMANYTARGETS		"%s :Too many targets. The maximum is %d for %s."
#define STR_ERR_NOORIGIN		":No origin specified"
#define STR_ERR_INVALIDCAPCMD		"%s :Invalid CAP subcommand"
#define STR_ERR_NORECIPIENT		":No recipient given (%s)"
#define STR_ERR_NOTEXTTOSEND		":No text to send"
#define STR_ERR_NOTOPLEVEL		"%s :No toplevel domain specified"
#define STR_ERR_WILDTOPLEVEL		"%s :Wildcard in toplevel Domain"
#define STR_ERR_TOOMANYMATCHES		"%s :%s"
#define STR_ERR_UNKNOWNCOMMAND		"%s :Unknown command"
#define STR_ERR_NOMOTD		":MOTD File is missing"
#define STR_ERR_NOADMININFO		"%s :No administrative info available"
#define STR_ERR_FILEERROR		":File error doing %s on %s"
#define STR_ERR_NOOPERMOTD		":OPERMOTD File is missing"
#define STR_ERR_TOOMANYAWAY		":Too Many aways - Flood Protection activated"
#define STR_ERR_NONICKNAMEGIVEN		":No nickname given"
#define STR_ERR_ERRONEUSNICKNAME		"%s :Nickname is unavailable: %s"
#define STR_ERR_NICKNAMEINUSE		"%s :Nickname is already in use."
#define STR_ERR_NORULES		":RULES File is missing"
#define STR_ERR_NICKCOLLISION		"%s :Nickname collision KILL"
#define STR_ERR_BANNICKCHANGE		"%s :Cannot change nickname while banned on channel"
#define STR_ERR_NCHANGETOOFAST		"%s :Nick change too fast. Please try again later."
#define STR_ERR_TARGETTOOFAST		"%s :Message target change too fast. Please wait %ld seconds"
#define STR_ERR_SERVICESDOWN		"%s :Services are currently down. Please try again later."
#define STR_ERR_USERNOTINCHANNEL		"%s %s :They aren't on that channel"
#define STR_ERR_NOTONCHANNEL		"%s :You're not on that channel"
#define STR_ERR_USERONCHANNEL		"%s %s :is already on channel"
#define STR_ERR_NOLOGIN		"%s :User not logged in"
#define STR_ERR_SUMMONDISABLED		":SUMMON has been disabled"
#define STR_ERR_USERSDISABLED		":USERS has been disabled"
#define STR_ERR_NONICKCHANGE		":Can not change nickname while on %s (+N)"
#define STR_ERR_FORBIDDENCHANNEL		"%s :Cannot join channel: %s"
#define STR_ERR_NOTREGISTERED		":You have not registered"
#define STR_ERR_HOSTILENAME		":Your username %s contained the invalid character(s) %s and has been changed to %s. Please use only the characters 0-9 a-z A-Z _ - or . in your username. Your username is the part before the @ in your email address."
#define STR_ERR_NOHIDING		"%s :Cannot join channel (+H)"
#define STR_ERR_NOTFORHALFOPS		":Halfops cannot set mode %c"
#define STR_ERR_NEEDMOREPARAMS		"%s :Not enough parameters"
#define STR_ERR_ALREADYREGISTRED		":You may not reregister"
#define STR_ERR_NOPERMFORHOST		":Your host isn't among the privileged"
#define STR_ERR_PASSWDMISMATCH		":Password Incorrect"
#define STR_ERR_YOUREBANNEDCREEP		":%s"
#define STR_ERR_KEYSET		"%s :Channel key already set"
#define STR_ERR_ONLYSERVERSCANCHANGE		"%s :Only servers can change that mode"
#define STR_ERR_LINKSET		"%s :Channel link already set"
#define STR_ERR_LINKCHANNEL		"%s %s :[Link] %s has become full, so you are automatically being transferred to the linked channel %s"
#define STR_ERR_CHANNELISFULL		"%s :Cannot join channel (+l)"
#define STR_ERR_UNKNOWNMODE		"%c :is unknown mode char to me"
#define STR_ERR_INVITEONLYCHAN		"%s :Cannot join channel (+i)"
#define STR_ERR_BANNEDFROMCHAN		"%s :Cannot join channel (+b)"
#define STR_ERR_BADCHANNELKEY		"%s :Cannot join channel (+k)"
#define STR_ERR_BADCHANMASK		"%s :Bad Channel Mask"
#define STR_ERR_NEEDREGGEDNICK		"%s :You need a registered nick to join that channel."
#define STR_ERR_BANLISTFULL		"%s %s :Channel ban/ignore list is full"
#define STR_ERR_LINKFAIL		"%s :Sorry, the channel has an invalid channel link set."
#define STR_ERR_CANNOTKNOCK		":Cannot knock on %s (%s)"
#define STR_ERR_NOPRIVILEGES		":Permission Denied- You do not have the correct IRC operator privileges"
#define STR_ERR_CHANOPRIVSNEEDED		"%s :You're not channel operator"
#define STR_ERR_CANTKILLSERVER		":You cant kill a server!"
#define STR_ERR_ATTACKDENY		"%s :Cannot kick protected user %s."
#define STR_ERR_KILLDENY		":Cannot kill protected user %s."
#define STR_ERR_NONONREG		":You must identify to a registered nick to private message %s"
#define STR_ERR_NOTFORUSERS		":%s is a server only command"
#define STR_ERR_SECUREONLYCHAN		"%s :Cannot join channel (Secure connection is required)"
#define STR_ERR_NOSWEAR		":%s does not accept private messages containing swearing."
#define STR_ERR_NOOPERHOST		":No O-lines for your host"
#define STR_ERR_NOCTCP		":%s does not accept CTCPs"
#define STR_ERR_CHANOWNPRIVNEEDED		"%s :You're not a channel owner"
#define STR_ERR_TOOMANYJOINS		"%s :Too many join requests. Please wait a while and try again."
#define STR_ERR_UMODEUNKNOWNFLAG		":Unknown MODE flag"
#define STR_ERR_USERSDONTMATCH		":Cant change mode for other users"
#define STR_ERR_SILELISTFULL		"%s :Your silence list is full"
#define STR_ERR_TOOMANYWATCH		"%s :Maximum size for WATCH-list is 128 entries"
#define STR_ERR_NEEDPONG		":To connect, type /QUOTE PONG %lX"
#define STR_ERR_TOOMANYDCC		"%s :Your dcc allow list is full. Maximum size is %d entries"
#define STR_ERR_DISABLED		"%s :%s" /* ircu */
#define STR_ERR_NOINVITE		":Cannot invite (+V) at channel %s"
#define STR_ERR_ADMONLY			":Cannot join channel %s (Admin only)"
#define STR_ERR_OPERONLY		":Cannot join channel %s (IRCops only)"
#define STR_ERR_LISTSYNTAX		":Bad list syntax, type /quote list ? or /raw list ?"
#define STR_ERR_WHOSYNTAX		":/WHO Syntax incorrect, use /who ? for help"
#define STR_ERR_WHOLIMEXCEED		":Error, /who limit of %d exceeded. Please narrow your search down and try again"
#define STR_ERR_OPERSPVERIFY		":Trying to join +s or +p channel as an oper. Please invite yourself first."
#define STR_ERR_CANTSENDTOUSER		"%s :%s"
#define STR_RPL_REAWAY		"%s %s %s %d :%s"
#define STR_RPL_GONEAWAY		"%s %s %s %d :%s"
#define STR_RPL_NOTAWAY		"%s %s %s %d :is no longer away"
#define STR_RPL_LOGON		"%s %s %s %d :logged online"
#define STR_RPL_LOGOFF		"%s %s %s %d :logged offline"
#define STR_RPL_WATCHOFF		"%s %s %s %d :stopped watching"
#define STR_RPL_WATCHSTAT		":You have %d and are on %d WATCH entries"
#define STR_RPL_NOWON		"%s %s %s %ld :is online"
#define STR_RPL_NOWOFF		"%s %s %s %ld :is offline"
#define STR_RPL_WATCHLIST		":%s"
#define STR_RPL_ENDOFWATCHLIST		":End of WATCH %c"
#define STR_RPL_CLEARWATCH		":Your WATCH list is now empty"
#define STR_RPL_NOWISAWAY		"%s %s %s %ld :is away"
#define STR_RPL_MAPMORE		":%s%-*s --> *more*"
#define STR_RPL_DCCSTATUS		":%s has been %s your DCC allow list"
#define STR_RPL_DCCLIST		":%s"
#define STR_RPL_ENDOFDCCLIST		":End of DCCALLOW %s"
#define STR_RPL_DCCINFO		":%s"
#define STR_RPL_SPAMCMDFWD		"%s :Command processed, but a copy has been sent to ircops for evaluation (anti-spam) purposes. [%s]"
#define STR_RPL_STARTTLS		":STARTTLS successful, go ahead with TLS handshake" /* kineircd */
#define STR_RPL_WHOISSECURE		"%s :%s" /* our variation on the kineircd numeric */
#define STR_ERR_STARTTLS		":%s"
#define STR_ERR_INVALIDMODEPARAM		"%s %c %s :%s"
#define STR_RPL_MONONLINE		":%s!%s@%s"
#define STR_RPL_MONOFFLINE		":%s"
#define STR_RPL_MONLIST		":%s"
#define STR_RPL_ENDOFMONLIST		":End of MONITOR list"
#define STR_ERR_MONLISTFULL		"%d %s :Monitor list is full."
#define STR_ERR_MLOCKRESTRICTED		"%s %c %s :MODE cannot be set due to channel having an active MLOCK restriction policy"
#define STR_RPL_WHOISKEYVALUE		"%s %s %s :%s"
#define STR_RPL_KEYVALUE		"%s %s %s :%s"
#define STR_RPL_METADATAEND		":end of metadata"
#define STR_ERR_METADATALIMIT		"%s :metadata limit reached"
#define STR_ERR_TARGETINVALID		"%s :invalid metadata target"
#define STR_ERR_NOMATCHINGKEY		"%s %s :no matching key"
#define STR_ERR_KEYINVALID		":%s"
#define STR_ERR_KEYNOTSET		"%s %s :key not set"
#define STR_ERR_KEYNOPERMISSION		"%s %s :permission denied"
#define STR_RPL_METADATASUBOK		":%s"
#define STR_RPL_METADATAUNSUBOK		":%s"
#define STR_RPL_METADATASUBS		":%s"
#define STR_ERR_METADATATOOMANYSUBS		"%s"
#define STR_ERR_METADATASYNCLATER		"%s %s"
#define STR_ERR_METADATARATELIMIT		"%s %s %s :%s"
#define STR_ERR_METADATAINVALIDSUBCOMMAND		"%s :invalid metadata subcommand"
#define STR_RPL_LOGGEDIN		"%s!%s@%s %s :You are now logged in as %s."
#define STR_RPL_LOGGEDOUT		"%s!%s@%s :You are now logged out."
#define STR_ERR_NICKLOCKED		":You must use a nick assigned to you."
#define STR_RPL_SASLSUCCESS		":SASL authentication successful"
#define STR_ERR_SASLFAIL		":SASL authentication failed"
#define STR_ERR_SASLTOOLONG		":SASL message too long"
#define STR_ERR_SASLABORTED		":SASL authentication aborted"
#define STR_ERR_SASLALREADY		":You have already completed SASL authentication"
#define STR_RPL_SASLMECHS		"%s :are available SASL mechanisms"
#define STR_ERR_CANNOTDOCOMMAND		"%s :%s"
#define STR_ERR_CANNOTCHANGECHANMODE		"%c :%s"
#define STR_ERR_NUMERICERR		"Numeric error!"
