UnrealIRCd 5.0.4-dev Release Notes
===============================

This is mostly a bug fix release, with few new features.

Fixes:
* Channeldb was not properly restoring all channel modes, such as +P.
* UnrealIRCd was giving up too easily on ident lookups.
* Crash when unloading a module with moddata.
* Crash if an authenticated server sends wrong information (rare).
* Removing a TEMPSHUN did not work if the user was on another server.
* SAJOIN to 0 (part all channels) resulted in a desync when used on remote users.
* Forced nick change from services was not showing up if the user
  was not in any channels.

Enhancements:
* New option [set::hide-idle-time::policy](https://www.unrealircd.org/docs/Set_block#set%3A%3Ahide-idle-time)
  by which you can change usermode +I (hide idle time in WHOIS) from
  oper-only to settable by users. More options will follow in a future
  release.
* In WHOIS you can now see if a user is currently (temp)shunned.
  This only works for locally connected users for technical reasons,
  so use ```/WHOIS Nick Nick``` to see it for remote users.

Changes:
* The oper notices and logging with regards to server linking have changed
  a little. They are more consistent and log more now.
* When an IRCOp tries to oper up from an insecure connection we will now
  mention the https://www.unrealircd.org/docs/FAQ#oper-requires-tls page.
  This message is customizable through
  [set::plaintext-policy::oper-message](https://www.unrealircd.org/docs/Set_block#set::plaintext-policy).

Are you upgrading from UnrealIRCd 4.x to UnrealIRCd 5? If so,
then check out the *UnrealIRCd 5* release notes [further down](#unrealircd-5). At the
very least, check out [Upgrading from 4.x](https://www.unrealircd.org/docs/Upgrading_from_4.x).

UnrealIRCd 5.0.3.1
-------------------
This fixes a crash issue after REHASH in 5.0.3.

UnrealIRCd 5.0.3
-----------------
Fixes:
* Fix serious flood issue in labeled-response implementation.
* An IRCOp SQUIT'ing a far remote server may cause a broken link topology
* In channels that are +D (delayed join), PARTs were not shown correctly to
  channel operators.

Enhancements:
* A new HISTORY command for history playback (```HISTORY #channel number-of-lines```)
  which allows you to fetch more lines than the on-join history playback.
  Of course, taking into account the set limits in the +H channel mode.
  This command is one of the [two interfaces](https://www.unrealircd.org/docs/Channel_history#Ways_to_retrieve_history)
  to [Channel history](https://www.unrealircd.org/docs/Channel_history).
* Two new [message tags](https://www.unrealircd.org/docs/Message_tags),
  ```unrealircd.org/userip``` and ```unrealircd.org/userhost```
  which communicate the user@ip and real user@host to IRCOps.

Changes:
* Drop the draft/ prefix now that the IRCv3
  [labeled-response](https://ircv3.net/specs/extensions/labeled-response.html)
  specification is out of draft.
* The operclass permission ```immune:target-limit``` is now called
  ```immune:max-concurrent-conversations```, since it bypasses
  [set::anti-flood::max-concurrent-conversations](https://www.unrealircd.org/docs/Set_block#set::anti-flood::max-concurrent-conversations).
  For 99% of the users this change is not important, but it may be
  if you use highly customized [operclass blocks](https://www.unrealircd.org/docs/Operclass_block)

Are you upgrading from UnrealIRCd 4.x to UnrealIRCd 5? If so,
then check out the *UnrealIRCd 5* release notes [further down](#unrealircd-5). At the
very least, check out [Upgrading from 4.x](https://www.unrealircd.org/docs/Upgrading_from_4.x).

UnrealIRCd 5.0.2
-----------------

Fixes:
* Halfop users are not synced correctly, resulting in missing users across links.
* [Channel history](https://www.unrealircd.org/docs/Channel_history) used
incorrect time internally, resulting in messages expiring too soon.
The syntax is now really ```/MODE #chan +H lines:time-in-minutes```.
To make clear that the time is in minutes, an 'm' will be added
automatically by the server (eg ```+H 15:1440m```).
* Documentation: to exempt someone from gline via /ELINE you have to use type 'G', not 'g'.
  Similarly, to exempt from spamfilter, use type 'F' and not 'f'.
* Exempting IPs from throttling via [except throttle](https://www.unrealircd.org/docs/Except_throttle_block) was not working.
* Unable to customize [set::tls::outdated-protocols](https://www.unrealircd.org/docs/Set_block#set::ssl::outdated-protocols)
  and [set::tls::outdated-ciphers](https://www.unrealircd.org/docs/Set_block#set::ssl::outdated-ciphers).
* Specifying multiple channels did not work in [set::auto-join](https://www.unrealircd.org/docs/Set_block#set::auto-join),
  [set::oper-auto-join](https://www.unrealircd.org/docs/Set_block#set::oper-auto-join) and
  [tld::channel](https://www.unrealircd.org/docs/Tld_block).

Enhancements:
* [Extended server bans](https://www.unrealircd.org/docs/Extended_server_bans) in *LINE and /ELINE allow
  you to ban or exempt users on criteria other than host/IP. These use a
  similar syntax to extended bans. Currently supported are ~a, ~S and ~r. Examples:
  * ```/ELINE ~a:TrustedAccount kG 0 This user can bypass kline/gline when using SASL```
  * ```/ELINE ~S:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef kGF 0 Trusted user with this certificate fingerprint```
  * ```/GLINE ~r:*some*stupid*real*name*```
  * These can also be used in the configuration file, eg: ```except ban { mask ~S:11223344etc; type all; };```
* New options that may not be used much, but can be useful on specific networks:
  * The IRCd may add automatic bans, for example due to a blacklist hit,
    a spamfilter hit, or because of antirandom or antimixedutf8. The new
    option [set::automatic-ban-target](https://www.unrealircd.org/docs/Set_block#set::automatic-ban-target) specifies on *what* the ban should
    be placed. The default is *ip*. Other options are: userip, host, userhost, account, certfp.
  * Similarly, an oper may type ```/GLINE nickname```. The new option
    [set::manual-ban-target](https://www.unrealircd.org/docs/Set_block#set::manual-ban-target) specifies on what the ban should be placed.
    By default this is *host* (fallback to *ip*).
* New options to exempt webirc users: [set::connthrottle::webirc-bypass](https://www.unrealircd.org/docs/Connthrottle),
  [set::restrict-commands::name-of-command::exempt-webirc](https://www.unrealircd.org/docs/Set_block#set::restrict-commands).

UnrealIRCd 5.0.1
-----------------

Fixes:
* IRCd may hang in rare circumstances
* Windows: fix repeated "ERROR renaming 'data/reputation.db.tmp'" warnings
* Antirandom and blacklist did not deal properly with 'warn' actions
* [Authprompt](https://www.unrealircd.org/docs/Authentication#How_it_looks_like)
  did not always work properly
* Line numbers were incorrect in config file warnings/errors when using @if or @define

Enhancements:
* New /ELINE exception type 'm' to bypass allow::maxperip.
  Or in the configuration file: ```except ban { mask 203.0.113.0/24; type maxperip; };```
* IRCOps can override MLOCK restrictions when services are down,
  if they have the channel:override:mlock operclass permission,
  such as opers which use the operclass 'netadmin-with-override'.

Other:
* Gottem and k4be have [uploaded their 3rd party modules](https://modules.unrealircd.org/)
  to unrealircd-contrib so *NIX users can now easily install them using the new
  [Module manager](https://www.unrealircd.org/docs/Module_manager)

UnrealIRCd 5
-------------
After more than 6 months of hard work, UnrealIRCd 5 is now our new "stable" branch.
In particular I would like to thank Gottem and 'i' for their source code
contributions and PeGaSuS and westor for testing releases.

When we transitioned from 3.2.x to 4.0.0 there were 175,000 lines of source code
added/removed during 3 years of development. This time it was 120,000 lines in
only 6 months, a major effort!

**If you are upgrading from 4.x to 5.x, then it would be wise to read
[Upgrading from 4.x](https://www.unrealircd.org/docs/Upgrading_from_4.x).
In any case, be sure to upgrade your services package first! (if you use any)**

UnrealIRCd 5 is compatible with the following services:
* [anope](https://www.anope.org/) (version 2.0.7 or higher) -
  with the "unreal4" protocol module
* [atheme](https://atheme.github.io/atheme.html) (version 7.2.9 or higher) -
  with the "unreal4" protocol module

Summary
--------
The most visible change to end-users is channel history. A lot of IRCv3 features were added.
Various modules from Gottem have been integrated and enhanced.
We now have a 3rd party module manager so you can install modules with 1 simple command.
Channel settings of ```+P``` channels and *LINES are saved in a database and
restored on startup (via 'channeldb' and 'tkldb' respectively).
Channel mode ```+L``` has a slight change of meaning, the existing floodprot
mode (```+f```) has a new type to prevent repeated messages and a new drop action.
A few extended bans have been added as well (```~f``` and ```~p```).
IRCOps now have the ability to add ban exceptions via the ```/ELINE``` command.
Advanced admins can use more dynamic configuration options where you can
define variables and use them later in the configuration file.
Finally, there have been speed improvements, we use better defaults and
have added more countermeasures and options against spambots.
Under the hood *a significant amount* of the source code was changed and cleaned up.

Read below for the full list of enhancements, changes and removals (and information for developers too).

Enhancements
-------------
* Support for IRCv3 server generated [message tags](https://ircv3.net/specs/extensions/message-tags), which allows us to communicate
  additional information in protocol messages such as in JOIN and PRIVMSG.
  Currently implemented and permitted message tags are:
  * [account](https://ircv3.net/specs/extensions/account-tag-3.2): communicate the services account that a user uses
  * [msgid](https://ircv3.net/specs/extensions/message-ids): assign an unique message id to each message
  * [time](https://ircv3.net/specs/extensions/server-time-3.2): assign a time label to each message
  The last two are mainly for history playback.
* Support for IRCv3 [echo-message](https://ircv3.net/specs/extensions/echo-message-3.2), which helps clients, among other things,
  to see if the message you sent was altered in any way, eg: censored,
  stripped from color, etc.
* Support for IRCv3 [draft/labeled-response-0.2](https://ircv3.net/specs/extensions/labeled-response), which helps clients to
  correlate commands and responses.
* Support for IRCv3 [BATCH](https://ircv3.net/specs/extensions/batch-3.2), needed for some other features.
* Recording and playback of [channel history](https://www.unrealircd.org/docs/Channel_history) when channel mode +H is set.
  The syntax is: ```MODE #chan +H max-lines-to-record:max-time-to-record-in-minutes```.

  For example: ```MODE #chan +H 50:1440``` means the last 50 messages will be stored and no
  message will be stored longer than 1440 minutes (1 day).

  The channel history is then played back when joining such a channel,
  but with two things to keep in mind:
  1) The client must support the 'server-time' CAP ('time' message tag),
     otherwise history is not shown. Any modern IRC client supports this.
  2) Only a maximum of 15 lines are played back on-join by default

  The reason for the maximum 15 lines on-join playback is that this can
  be quite annoying if you rejoin repeatedly and as to not flood the users
  screen too much (unwanted). In the future we will support a mechanism
  for clients to "fetch" history - rather than sending it on-join - so
  they can fetch more than the 15 lines, up to the number of lines and
  time configured in the +H channel mode.

  You can configure the exact number of lines that are played back and
  all the limits that apply to +H via [set::history::channel](https://www.unrealircd.org/docs/Set_block#set::history).
* For saving and retrieving history we currently have the following options:
  * *history_backend_mem*: channel history is stored in memory.
    This is very fast but also means history is lost on restart.
  * *history_backend_null*: don't store channel history at all.
    This can be useful to load on servers with no users on it, such as a
    hub server, where storing history is unnecessary.

  As you can see there is currently no 'disk' backend. However, in the
  future more options may be added. Also note that 3rd party modules
  can add history backends as well.
* Support for ban exceptions via the new ```/ELINE``` command. This allows you
  to add exceptions for regular bans (KLINE/GLINE/ZLINE/etc), but also
  for connection throttling and blacklist checking.
  For more information, just type ```/ELINE ``` in your IRC client as an IRCOp.
* [Websocket](https://www.unrealircd.org/docs/WebSocket_support) support now includes type 'text'
  in addition to 'binary', which should work with [KiwiIRC](https://kiwiirc.com/)'s nextclient.

  Also, websockets are no longer active on all ports by default. You have to explicitly
  enable the websocket option in the listen block and also specify type *text* or *binary*,
  eg: ```listen { ip *; port 6667; options { websocket { type text; } } }```

  Also note that websockets require nick names and channels to consist of UTF8
  characters only, due to
  [WebSocket being incompatible with non-UTF8](https://www.unrealircd.org/docs/WebSocket_support#Problems_with_websockets_and_non-UTF8)
* There's now a [Module manager](https://www.unrealircd.org/docs/Module_manager)
  which allows you to install and upgrade 3rd party modules in an easy way:
  * ```./unrealircd module list``` - to list all available 3rd party modules
  * ```./unrealircd module install third/something``` - to install the specified module.
* You can now test for configuration errors without actually starting the
  IRC server. This is ideal if you are upgrading UnrealIRCd to a newer
  version: simply run ```./unrealircd configtest``` to make sure it passes
  the configuration test, and then you can safely restart the server for
  the upgrade (in this example case).
* Channel mode +L now kicks in for any rejected join, so not just for +l but
  also for +b, +i, +O, +z, +R and +k. If, for example, the channel is
  +L #insecure and also +z then, when an insecure user ties to join, they
  will be redirected to #insecure.
* New extended ban ~f to forward users to the specified channel if the ban
  matches. Example: ```MODE #chan +b ~f:#badisp:*!*@*.isp.org```
* Channel mode +f now has a 'd' action: drop message. This will send an
  error message to the user and not show the message in the channel but
  otherwise do nothing (no kick or ban).
  For example: ```MODE #chan +f [5t#d]:15``` will limit sending a maximum of
  5 messages per 15 seconds per-user and drop any messages sent above that limit.
* Channel mode +f now has 'r' floodtype to prevent repeated lines. This will
  compare the current message to the last message and the one before that
  the user sent to the channel. If it's a repeat then the user can be
  kicked (the default action), the message can be dropped ('d') or the
  user can be banned ('b'). Example: ```MODE #chan +f [1r#d]:15```
  If you want to permit 1 repeated line but not 2 then use: ```+f [2r#d]:15```
* New module **tkldb** (loaded by default): all *LINES and spamfilters are now
  saved across reboots. No need for services for that anymore.
* New module **channeldb** (loaded by default): saves and restores all channel
  settings including topic, modes, bans etc. of +P (persistent) channels.
* New module [restrict-commands](https://www.unrealircd.org/docs/Set_block#set::restrict-commands), which allows you to restrict any IRC
  command based on criteria such as "how long is this user connected",
  "is this user registered (has a services account)" etc.
  The example.conf now ships with configuration to disable LIST the
  first 60 seconds and disable INVITE the first 120 seconds.
  If you are having spambot problems then tweaking this configuration
  may be helpful to you.
* New option [set::require-module](https://www.unrealircd.org/docs/Set_block#set::require-module), which allows you to require certain
  modules on other UnrealIRCd 5 servers, otherwise the link is rejected.
* New option [set::min-nick-length](https://www.unrealircd.org/docs/Set_block#set::min-nick-length) to set a minimum nick length.
* New module rmtkl (loaded by default): this allows you to remove TKL's
  such as GLINEs easily via the /RMTKL command.
* The [reputation and connthrottle](https://www.unrealircd.org/docs/Connthrottle) modules are now loaded by default.
  Just as a reminder, what these do is classifying your users in "known
  users (known IP's)" and "unknown IP's" for IP's that have not been
  seen before (or only for a short amount of time). Then, when there
  is a connection flood, unknown/new IP addresses are throttled at
  20 connections per minute, while known users are always allowed in.
* Add support for [defines and conditional configuration](https://www.unrealircd.org/docs/Defines_and_conditional_config) via @define and @if.
  This is mostly for power users, in particular users who share the same
  configuration file across several servers.
* New extban ~p to hide the part/quit message in PART and QUIT.
  For example: ```MODE #chan +b ~p:*!*@*.nl```
* You will now see a warning when a server is not responding even
  before they time out. How long to wait for a PONG reply upon PING
  can be changed via [set::ping-warning](https://www.unrealircd.org/docs/Set_block#set::ping-warning) and defaults to 15 seconds.
  If you see the warning frequently then your connection is flakey.
* Add new setting [set::broadcast-channel-messages](https://www.unrealircd.org/docs/Set_block#set::broadcast-channel-messages) which defines when
  channel messages are sent across server links. The default setting
  is *auto* which is the correct setting for pretty much everyone.
* Add new option [set::part-instead-of-quit-on-comment-change](https://www.unrealircd.org/docs/Set_block#set::part-instead-of-quit-on-comment-change):
  when a QUIT message is changed due to channel restrictions, such as
  stripping color or censoring a word, we normally change the QUIT
  message. This has an effect on ALL channels, not just the one that
  imposed the restrictions. While we feel that is the best tradeoff,
  there is now also this new option (off by default) that will change
  the QUIT into a PART in such a case, so the other channels that
  do not have the restrictions (eg: are -S and -G) can still see the
  original QUIT message.
* New module [webredir](https://www.unrealircd.org/docs/Set_block#set::webredir::url). Quite some people run their IRCd on port 443 or 80
  so their users can avoid firewall restrictions in place. In such a case,
  with this module, you can now send a HTTP redirect in case some user
  enters your IRC server name in their browser. Eg https://irc.example.org/
  can be made to redirect to https://www.example.org/
* We now protect against misbehaving SASL servers and will time out
  SASL sessions after
  [set::sasl-timeout](https://www.unrealircd.org/docs/Set_block#set::sasl-timeout),
  which is 15 seconds by default.

Changed
--------
* Channel mode +L can now be set by chanops (+o and higher) instead of only
  by +q (channel owner)
* Channel names must now be valid UTF8 by default.
  We actually have 3 possible settings of [set::allowed-channelchars](https://www.unrealircd.org/docs/Set_block#set::allowed-channelchars):
  * **utf8**:  Channel must be valid UTF8, this is the new default
  * **ascii**: A very strict setting, for example in use at freenode,
     the channel name may not contain high ascii or UTF8
  * **any**:   A very loose setting, which allows almost all characters
     in the channel name. This was the OLD default, up to and
     including UnrealIRCd 4. It is no longer recommended.

  For most networks this new default setting of utf8 will be fine, since
  by far most IRC clients use UTF8 for many years already.
  If you have a network that has a significant portion of chatters
  that are on old non-UTF8 clients that use a specific character set
  then you may want to use ```set { allowed-nickchars any; }```
  Some Russian and Ukrainian networks are known to need this.
* The "except tkl" block is now called [except ban](https://www.unrealircd.org/docs/Except_ban_block#UnrealIRCd_5). If no type
  is specified in an except ban { } block then we exempt the entry
  from kline, gline, zline, gzline and shun.
* We no longer use a blacklist for stats (set::oper-only-stats).
  We use a whitelist now instead: [set::allow-user-starts](https://www.unrealircd.org/docs/Set_block#set::allow-user-stats).
  Most users can just remove their old set::oper-only-stats line,
  since the new default set::allow-user-starts setting is fine.
* Windows: we now require a 64-bit version, Windows 7 or later.
  The new program path is: C:\Program Files\UnrealIRCd 5
  and the binaries have been moved to a new subdirectory: bin\
* Modules lost their m_ prefix, so for example m_map is now just map.
  Also the modules in cap/ are now directly in modules.
* More modules that were previously PERM (permanent) can now be unloaded
  and reloaded on the fly. This allows more "hotfixing" without restart
  in case of a bug and also more control for admins at runtime.
  Only <5 modules out of 173 are permanent now.
* User mode +T now blocks channel CTCPs as well.
* User mode +q (unkickable) could previously be set by any IRCOp.
  This has been changed to require the self:unkickablemode operclass
  permission. This is included in the *-with-override operclasses
  (eg: netadmin-with-operoverride).
* [set::modes-on-join](https://www.unrealircd.org/docs/Set_block#set::modes-on-join) is now ```+nt``` by default.
* The [authprompt](https://www.unrealircd.org/docs/Authentication#How_it_looks_like) module is now loaded by default. This means that if
  you do a soft kline on someone (eg: ```KLINE %*@*.badisp```) then the user
  has a chance to [authenticate](https://www.unrealircd.org/docs/Authentication#How_it_looks_like) to services, even without SASL, and
  bypass the ban if (s)he is authenticated.
* The WHOX module is now used by default. Previously it was optional.
  WHOX enhances the "WHO" output, providing additional information to
  IRC clients such as the services account that someone is using.
  It is also more universal than standard WHO. Unfortunately this also
  means the WHO syntax changed to something less logical.
* At many places the term *SSL* has been changed to *SSL/TLS* or *TLS*.
  Configuration items (eg: set::ssl to set::tls) have been renamed
  as well and so have directories (eg: conf/ssl to conf/tls).
  The old configuration names still work and currently does NOT raise
  any warning. Also, when upgrading an existing installation on *NIX,
  the conf/tls directory will be symlinked to conf/ssl as to not break
  any Let's Encrypt certificate scripts.
* It is now mandatory to have at least one open SSL/TLS port, otherwise
  UnrealIRCd will refuse to boot. Previously this was a warning.
* IRCOps now need to use SSL/TLS in order to oper up, as the
  [set::plaintext-policy::oper](https://www.unrealircd.org/docs/Set_block#set::plaintext-policy) default setting is now 'deny'.
  Similarly, [set::outdated-tls-policy::oper](https://www.unrealircd.org/docs/Set_block#set::outdated-tls-policy) is now also 'deny'.
  You can change this, if you want, but it is not recommended.
* [set::outdated-tls-policy::server](https://www.unrealircd.org/docs/Set_block#set::outdated-tls-policy) is now 'deny' as well, since all
  servers should use reasonable SSL/TLS protocols and ciphers.
* The default generated certificated has been changed from RSA 4096 bits
  to Elliptic Curve Cryptography "384r1". This provides the same amount
  of security but at higher speed. This only affects the default self-
  signed certificate. You can still use RSA certificates just fine.
* If you do use an RSA certificate, we now require it to be at least
  2048 bits otherwise UnrealIRCd will refuse to boot.
* When matching [allow { } blocks](https://www.unrealircd.org/docs/Allow_block), we now always continue with the next
  block (if any) if the password did not match or no password was
  specified. In other words, allow::options::nopasscont is now the
  default and we behave as if there was a ::wrongpasscont too.
* All snomasks are now oper-only. Previously some were not, which
  was confusing and could lead to information leaks.
  Also removed weird set::snomask-on-connect accordingly.
* The IRCd now uses hash tables that are resilient against hash table
  attacks. Also, the hash tables have increased in size to speed things
  up when looking up nick names etc.
* Server options in VERSION (eg: Fhin6OoEMR3) are no longer shown to
  normal users. They don't mean much nowadays anyway.
* ```./Config``` now asks fewer questions and configure runs faster since
  many unnecessary checks have been removed (compatibility with very
  old compilers / systems).
* We now default to system libs (eg: ```--with-system-pcre2``` is assumed)
* Spamfilter should catch some more spam evasion techniques.
* All /DCCDENY and deny dcc { } parsing and checking is now moved to
  the 'dccdeny' module.
* Windows: If you choose to run UnrealIRCd as a service then it now
  runs under the low-privilege NetworkService account rather than
  the high-privilege LocalSystem account.

Minor issues fixed
-------------------
* Specifying a custom OpenSSL/LibreSSL path should work now

Removed
--------
* Support for old server protocols has been removed.
  This means UnrealIRCd 5.x cannot link to 3.2.x. It also means you need
  to use reasonably new services. Generally, if your services can link to
  4.x then they should be able to link to 5.x as well. More information
  about this change and why it was done
  [can be found here](https://www.unrealircd.org/docs/FAQ#old-server-protocol).
* Connecting with a server password will no longer send that password
  to NickServ. Use [SASL](https://www.unrealircd.org/docs/SASL) instead!
* Extended ban ~R (registered nick): this was the old method to match
  registered users. Everyone should use ~a (services account) instead.
* The old TRE **posix** regex method has been removed because the TRE
  library is no longer maintained for over a decade and contains many
  bugs. (It was already deprecated in UnrealIRCd 4.2.3).
  Use type **regex** instead, which uses the modern PCRE2 regex engine.
* Timesync support has been removed. Use your OS time synchronization
  instead. (Note that Timesync was already disabled by default in 2018)
* Changing time offsets via ```TSCTL OFFSET``` and ```TSCTL SVSTIME``` are no longer
  supported. Use your OS time synchronization (NTP!). Adjustments via
  TSCTL are simply not accurate enough.
* The *nopost* module was removed since it no longer serves any useful
  purpose. UnrealIRCd already protects against these kind of attacks
  via ping cookies ([set::ping-cookie](https://www.unrealircd.org/docs/Set_block#set::ping-cookie), enabled by default).

Deprecated
-----------
* The set::official-channels block is now deprecated. This provided a
  mechanism to pre-configure channels that would have 0 members and
  would appear in /LIST with those settings, but once you joined all
  those settings would be gone. Rather confusing.

  Since UnrealIRCd 4.x we have permanent channels (+P) and since 5.x
  we store these permanent channels in a database so all settings are
  saved every few minutes and across restarts.

  Since permanent channels (+P) are much better, the official-channels
  support will be removed in a later version. There's no reason to
  use official-channels anymore.

Developers
-----------
* The module header is now as follows:

      ModuleHeader MOD_HEADER
        = {
              "nameofmodule",
              "5.0",
              "Some description", 
              "Name of Author",
              "unrealircd-5",
          };
  There's a new author field, the version must start with a digit,
  and also the name of the module must match the loadmodule name.
  So for example third/funmod must also be named third/funmod.
* The ```MOD_TEST```, ```MOD_INIT```, ```MOD_LOAD``` and ```MOD_UNLOAD``` functions no longer
  take a name argument. So: ```MOD_INIT(mymod)``` is now ```MOD_INIT()```
* We now use our own BuildBot infrastructure, so Travis-CI and AppVeyor
  have been removed.
* We now use a new test framework.
* ```Auth_Check()``` now returns ```1``` for allow and ```0``` on deny (!!)
* New function ```new_message()``` which should be called when a new message
  is sent, or at least for all channel events. It adds (or inherits)
  message tags like 'account', 'msgid', 'time', etc.
* Many send functions now take an extra MessageTag *mtags parameter,
  including but not limited to: sendto_one() and sendto_server().
* Command functions (CMD_FUNC) have an extra ```MessageTag *mtags```,
  on the other hand the ```cptr``` parameter has been removed.
* Command functions no longer return ```int``` but are ```void```,
  the same is true for  ```exit_client()```. ```FLUSH_BUFFER``` has been removed too.
  All this is a consequence of removing this (limited) signaling
  of client exits. From now on, if you call ```exit_client()``` it will free
  a lot of the client data and exit the user (close socket, send [s]quit),
  but it will **not free 'sptr' itself**, so you can simply check if some
  upstream function killed the client by checking ```IsDead(sptr)```.
  This is highly recommended after running ```do_cmd()``` or calling other
  functions that could kill a client. In which case you should return
  rather than continue doing anything with ```sptr```.
  Ultimately, in the main loop, the client will be freed (normally in less than 1 second).
* New single unified ```sendto_channel()``` and ```sendto_local_common_channels()```
  functions that are used by all the channel commands.
* Numerics should now be sent using ```sendnumeric()```. There's also
  a format string version ```sendnumericfmt()``` in case you need it,
  in which case you need to pass the numeric format string yourself.
  In such a case, don't forget the colon character, like ":%s", where needed.
* The parameters in several hooks have changed. Many now have an
  extra ```MessageTag *mtags``` parameter. Sometimes there are other changes
  as well, for example ```HOOKTYPE_CHANMSG``` now has 4 extra parameters.
* You can call do_cmd() with NULL mtags. Usually this is the correct way.
* If you used ```HOOKTYPE_PRE_USERMSG``` to block a message then you
  should now use ```HOOKTYPE_CAN_SEND_TO_USER```. Similarly, the hook
  ```HOOKTYPE_CAN_SEND``` which deals with channels is now called
  ```HOOKTYPE_CAN_SEND_TO_CHANNEL```. Some other remarks:
  * You CANNOT use HOOKTYPE_PRE_USERMSG anymore.
  * The hooks require you to set an error message if you return HOOK_DENY.
  * You should not send an error message yourself from these hooks.
    In other words: do not use sendnumeric(). This is done by the
    hook caller, based on the error message you return.
  * Thanks to this, all rejecting of user messages now use generic
    numeric 531 and all rejecting of channel messages use numeric 404.
    See also under *Client protocol* later in this document.
* If you use CommandOverrideAddEx() to specify a priority value (rare)
  then be aware that in 5.0.1 we now use the 4.0.x behavior again to
  match the same style of priorities in hooks: overrides with the
  lowest priority are run first.
* If you ever send a timestamp in a printf-like function, such as
  in ```sendto_server()```, then be sure to use ```%lld``` and cast the timestamp
  to *long long* so that it is compatible with both *NIX and Windows.
  Example: ```sendnotice(sptr, "Timestamp is %lld", (long long)ts);```
* ```EventAdd()``` changed the order of parameters and expects every_msec now
  which specifies the time in milliseconds rather than seconds. This
  allows for additional precision, or at least multiple calls per second.
  The minimum allowed every_msec value is 100 at this time.
  The prototype is now: ```EventAdd(Module *module, char *name,
  vFP event, void *data, long every_msec, int count);```
* New ```HOOKTYPE_IS_HANDSHAKE_FINISHED```. If a module returns ```0``` there, then
  the ```register_user()``` function will not be called and the user will
  not come online (yet). This is used by CAP and some other stuff.
  Can be useful if your module needs to "hold" a user in the registration
  phase.
* The function ```is_module_loaded()``` now takes a relative path like
  "usermodes/noctcp" because with just "ctcp" one could not see the
  difference between usermodes/noctcp and chanmodes/noctcp.
* ```CHFL_CHANPROT``` is now ```CHFL_CHANADMIN```, ```is_chanprot()``` is now ```is_chanadmin()```
* All hash tables now use [SipHash](https://en.wikipedia.org/wiki/SipHash), which is a hash function that is
  resilient against hash table attacks. If you, as a module dev, too
  use any hash tables anywhere (note: this is quite rare), then you
  are recommended to use our functions, see the functions siphash()
  and siphash_nocase() in src/hash.c.
* The random generator has been updated to use [ChaCha](https://en.wikipedia.org/wiki/Salsa20#ChaCha20_adoption) (more modern).
* You can now save pointers and integers etc. across rehashes by using
  ```LoadPersistentPointer()``` and ```SavePersistentPointer()```. For an example,
  see ```src/modules/chanmodes/floodprot.c``` how this can be used.
  Note that there can be no struct or type changes between rehashes.
* New ModData types: ```MODDATA_LOCALVAR``` and ```MODDA_GLOBALVAR```. These are
  settings or things that are locally or globally identified by the
  variable name only and not attached to any user/channel.
* Various files have been renamed. As previously mentioned, the m_
  prefix was dropped in ```src/modules/m_*.c```. Similarly the s_ prefix
  was dropped in ```src/s_*.c``` since it no longer had meaning. Also some
  files have been deleted and integrated elsewhere or renamed to
  have a name that better reflects their true meaning.
  Related to this change is that all command functions are now called
  ```cmd_name``` rather than ```m_name```.
* ```HOOKTYPE_CHECK_INIT``` and ```HOOKTYPE_PRE_LOCAL_CONNECT```
  have their return value changed. You should now return ```HOOK_*```, such
  as ```HOOK_DENY``` or ```HOOK_CONTINUE```.

Server protocol
----------------
* UnrealIRCd 5 now assumes you support the following PROTOCTL options:
  ```NOQUIT EAUTH SID NICKv2 SJOIN SJ3 NICKIP TKLEXT2```.
  If you fail to use ```SID``` or ```EAUTH``` then you will receive an
  error. For the other options, support is *assumed*, no warning or
  error is shown when you lack support. These are options that most,
  if not all, services support since UnrealIRCd 4.x so it shouldn't be
  a problem. More information [here](https://www.unrealircd.org/docs/FAQ#old-server-protocol)
* ```PROTOCTL MTAGS``` indicates that the server is capable of handling
  message tags and that the server can cope with 4K lines. (Note that
  the ordinary non-message-tag part is still limited to 512 bytes).
* Pseudo ID support in SASL was removed. We now use real UID's.
  This breaks services who rely on the old pseudo ID format.

Client protocol
----------------
* Support for message tags and other IRCv3 features. See the IRCv3
  specifications for more details.
* When a message is blocked, for whatever reason, we now use a generic
  numeric response: ```:server 531 yourname targetname :reason``` for the block
  This replaces all the various NOTICEs, ```ERR_NOCTCP```, ```ERR_NONONREG```, etc.
  with just one single numeric.
  The only other numerics that you may still encounter when PM'ing are
  ```ERR_NOSUCHNICK```, ```ERR_TOOMANYTARGETS``` and ```ERR_TARGETTOOFAST```, which are
  generic errors to any command involving targets. And ```ERR_SERVICESDOWN```.
  Note that channel messages already had a generic numeric for signaling
  blocked messages for a very long time, ```ERR_CANNOTSENDTOCHAN```.
* The 271 response to the SILENCE command is now:
  ```:server 271 yournick listentry!*@*```
  Previously the nick name appeared twice, which was a mistake.
* The 470 numeric, which is sent on /JOIN #channel redirect to #redirect
  now uses the following format:
  ```:server 470 yournick #channel #redirect :[Link] Cannot join channel...etc..```
* Clients are recommended to implement and enable the
  [server-time](https://ircv3.net/specs/extensions/server-time-3.2)
  extension by default. When enabled, channel history is played back
  on-join (if any) when the channel has channel mode +H.
  Otherwise your users will not see channel history.
