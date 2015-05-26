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

#define ERR_UNKNOWNCOMMAND   421
#define	ERR_NOMOTD           422
#define	ERR_NOADMININFO      423
#define	ERR_FILEERROR        424
#define ERR_NOOPERMOTD	     425
#ifdef NO_FLOOD_AWAY
#define ERR_TOOMANYAWAY	     429
#endif
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
#define ERR_HTMDISABLED		 488
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
#define RPL_LOCALUSERS       265
#define RPL_GLOBALUSERS      266

#define RPL_SILELIST         271
#define RPL_ENDOFSILELIST    272
#define RPL_STATSDLINE       275

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

#define ERR_MLOCKRESTRICTED	742

#define ERR_CANNOTDOCOMMAND 972
#define ERR_CANNOTCHANGECHANMODE 974

#define ERR_STARTTLS            691

#define RPL_LOGGEDIN            900
#define RPL_LOGGEDOUT           901
#define ERR_NICKLOCKED          902

#define RPL_SASLSUCCESS         903
#define ERR_SASLFAIL            904
#define ERR_SASLTOOLONG         905
#define ERR_SASLABORTED         906
#define ERR_SASLALREADY         907

#define ERR_NUMERICERR       999
