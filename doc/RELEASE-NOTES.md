UnrealIRCd 6.1.8-git
===============

This is the git version (development version) for future 6.1.8. This is work
in progress and may not always be a stable version.

### Enhancements:
* New [Extended ban](https://www.unrealircd.org/docs/Extended_bans#Group_4:_special)
  to inherit channel bans from another channel:
  * If in channel `#test` you add `+b ~inherit:#main` then anyone banned in
    `#main` will be unable to join `#test`.
  * This only applies for on-join ban checking, not for quiet bans,
    nick-changes, text bans, etc.
  * If the other channel (`#main` in this example) also has `~inherit` bans
    then we do not follow these (no nesting).
  * The maximum number of ~inherit bans in a channel is limited to only
    1 by default, see 
    [set::max-inherit-extended-bans](https://www.unrealircd.org/docs/Set_block#set::max-inherit-extended-bans)
  * This can also be used in `+I`, which entries are counted separately and
    have their own limit.
* JSON-RPC:
  * New call [`log.send`](https://www.unrealircd.org/docs/JSON-RPC:Log#log.send)
    to send a log message / server notice.

### Changes:
* When retrieving cold or hot patches we now do proper GPG/PGP checks.
  Just like we do on `./unrealircd upgrade`
* Update shipped libraries: c-ares to 1.33.1
* Move +/- 1000 lines of code from core to modules (regarding
  throttling, maxperip, vhost, exit_client).

### Fixes:
* In some circumstances users could hang during the handshake when
  their DNS lookup result was cached and using c-ares 1.31.0 or later
  (which was released on June 18 2024 and shipped with UnrealIRCd 6.1.7
  to be used as a fallback for systems which don't have the c-ares
  library installed).
* The [require authentication { }](https://www.unrealircd.org/docs/Require_authentication_block)
  was broken in 6.1.7.*.
* [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC) call `spamfilter.get`
  could not retrieve information about config-based spamfilters.
* The `decode_authenticate_plain()` was reading OOB. This function is not
  used by UnrealIRCd itself but could affect third party modules.
* Crash on invalid server-to-server command regarding `REHASH`
  (This only affected trusted linked servers)

### Developers and protocol:
* The `MD` S2S command now supports `BIGLINES`, allowing synching of 16K
  serialized moddata per entry. We don't plan to use this anytime soon,
  this is mostly so all UnrealIRCd servers support this in a year or
  two. However, if you do plan to serialize big moddata results then be
  sure all UnrealIRCd servers are on 6.1.8 or higher to prevent cut-off.

UnrealIRCd 6.1.7.2
-------------------
UnrealIRCd 6.1.7.2 is a dot release:
* [Central Blocklist](https://www.unrealircd.org/docs/Central_Blocklist):
  Fix issue if CBL server is not reachable (caused nick collisions)
* Stop offering curlinstall script. Most people don't need it anymore as
  without curl we support https remote includes since UnrealIRCd 6.0.0,
  which is usually sufficient. People who need other protocols can install
  the curl library system-wide.

UnrealIRCd 6.1.7.1 is a dot release:
* Add country and ASN support in `WHOWAS`
* Fix an annoying "[BUG] trying to modify fd -2 in fd table" message that
  appeared to IRCOps sometimes. It was harmless and only happened if you
  were using a recent version of the c-ares library (1.31.0 from June 18 2024
  or later, which also is the one we ship with as fallback if the system
  has no c-ares library installed).

See the release notes for 6.1.7 below for a lot more features/changes.

UnrealIRCd 6.1.7
-----------------

This is UnrealIRCd 6.1.7 stable. It comes with ASN support, more flexible
ban user { } and require authentication { } blocks and more.

UnrealIRCd recently turned 25 years! 🎉 See
[UnrealIRCd celebrates its 25th birthday](https://forums.unrealircd.org/viewtopic.php?t=9363).

### Enhancements:
* In the [ban user { }](https://www.unrealircd.org/docs/Ban_user_block)
  and [require authentication { }](https://www.unrealircd.org/docs/Require_authentication_block)
  blocks the `mask` is now a
  [Mask item](https://www.unrealircd.org/docs/Mask_item).
  This means you can use all the power of mask items and security groups and
  multiple matching criteria.
* The GeoIP module now contains information about
  [Autonomous System Numbers](https://www.unrealircd.org/docs/ASN):
   * The asn is shown in the user connecting notice as `[asn: ###]`,
     is shown in `WHOIS` (for IRCOps) and it is expanded in JSON data such as
     [JSON Logging](https://www.unrealircd.org/docs/JSON_logging) and
     [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC) calls like
     `user.list`.
  * Can be used in [Extended server ban](https://www.unrealircd.org/docs/Extended_server_bans):
    `GLINE ~asn:64496 0 This ISP is banned`.
  * Can be used in security groups and [mask items](https://www.unrealircd.org/docs/Mask_item)
    so you can do like:
    ```
    require authentication {
        mask { asn { 64496; 64497; 64498; } }
        reason "Too much abuse from this ISP. You are required to log in with an account using SASL.";
    }
    ```
   * In [Crule](https://www.unrealircd.org/docs/Crule) functions as `match_asn(64496)`
   * Also available in regular extbans/invex, but normally users don't
     know the IP or ASN of other users, unless you use no cloaking or
     change [set::whois-details::asn](https://www.unrealircd.org/docs/Set_block#set::whois-details).
* [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC):
  Similar to oper and operclass, in an
  [rpc-user](https://www.unrealircd.org/docs/Rpc-user_block) you now have
  to specify an rpc-user::rpc-class. The rpc-class is defined in an
  [rpc-class block](https://www.unrealircd.org/docs/Rpc-class_block)
  and configures what JSON methods can be called.  
  There are two default json-rpc classes:
  * `full`: access to all JSON-RPC Methods
  * `read-only`: access to things like *server_ban.list* but not to *server_ban.add*
* [set::spamfilter::except](https://www.unrealircd.org/docs/Set_block#set::spamfilter::except)
  is now a [Mask item](https://www.unrealircd.org/docs/Mask_item) instead of
  only a list of exempted targets. A warning is created to existing users
  along with a suggestion of how to use the new syntax. Technically, this is
  not really new functionality as all this was already possible via
  the [Except ban block](https://www.unrealircd.org/docs/Except_ban_block)
  with type spamfilter, but it is more visible/logical to have this also.
* New option [set::hide-killed-by](https://www.unrealircd.org/docs/Set_block#set::hide-killed-by):
  We normally show the nickname of the oper who did the /KILL in the quit message.
  When set to `yes` the quit message becomes shortened to "Killed (Reason)".
  This can prevent oper harassment.
* [set::restrict-commands](https://www.unrealircd.org/docs/Restrict_commands):
  new option `channel-create` for managing who may create new channels.
* New option [set::tls::certificate-expiry-notification](https://www.unrealircd.org/docs/Set_block#set::tls::certificate-expiry-notification):
  since UnrealIRCd 5.0.8 we warn if a SSL/TLS certificate is (nearly) expired.
  This new option allows turning it off, it is (still) on by default.
* Add the ability to capture the same data as
  [Central Spamreport](https://www.unrealircd.org/docs/Central_spamreport)
  by providing an spamreport::url option.

### Changes:
* IRCOps with the operclass `locop` can now only `REHASH` the local server
  and not remote servers.
* Comment out some more in example.conf by default
* Update shipped libraries: c-ares to 1.31.0, PCRE2 to 10.44,
  Sodium to 1.0.20

### Fixes:
* Crash when removing the `websocket` option on a websocket listener.
* Silence some compiler warnings regarding deprecation of c-ares API in
  src/dns.c.
* Memory leaks of around 1-2KB per rehash

### Developers and protocol:
* We use numeric 569 (RPL_WHOISASN) for displaying ASN info to IRCOps:  
  `:irc.example.net 569 x whoiseduser 64496 :is connecting from AS64496 [Example Corp]`

UnrealIRCd 6.1.6
-----------------

This is mostly a bug fix release but also comes with Crule enhancements.

UnrealIRCd turned 25 a few weeks ago! 🎉 See
[UnrealIRCd celebrates its 25th birthday](https://forums.unrealircd.org/viewtopic.php?t=9363).

### Enhancements:
* [Crule](https://www.unrealircd.org/docs/Crule) functions can now do everything
  that [security group blocks](https://www.unrealircd.org/docs/Security-group_block)
  can do.  
  In practice, this means the following functions were added in this release:
  * `is_tls()` returns true if the client is using SSL/TLS
  * `in_security_group('known-users')` returns true if the user is in the
    specified [security group](https://www.unrealircd.org/docs/Security-group_block).
  * `match_mask('*@*.example.org')` or `match_mask('*.example.org')`
    returns true if client matches mask.
  * `match_ip('192.168.*')` or with CIDR like `match_ip('192.168.0.0/16')`
    returns true if IP address of client matches.
  * `is_identified()` which returns true if the client is identified to a services account.
  * `is_webirc()` which returns true if the client is connected using WEBIRC.
  * `is_websocket()` which returns true if the client is connected using WebSockets.
  * `match_realname('*xyz*')` which returns true if the real name (gecos)
     contains xyz.
  * `match_account('xyz')` which returns true if the services account name is xyz.
  * `match_country('NL')` which returns true if 
    [GeoIP](https://www.unrealircd.org/docs/GeoIP) determined the
    country to be NL.
  * `match_certfp('abc')` which returns true if the 
    [Certificate fingerprint](https://www.unrealircd.org/docs/Certificate_fingerprint)
    is abc.

### Changes:
* For many years `REHASH -all` is the same as `REHASH` so we now reject
  the former.
* The [Crule](https://www.unrealircd.org/docs/Crule) function `inchannel('#xyz')`
  is now called `in_channel('#xyz')` to match the naming style of the other
  functions. The old name will keep working for the entire UnrealIRCd 6 series too.

### Fixes:
* Crash if you first REHASH and have a parse error (failed rehash 1) and then
  REHASH again but have a "late" rehash error, such as a remote include
  failing to load (failed rehash 2).
* Crash on Windows when using
  [Crule](https://www.unrealircd.org/docs/Crule) functions,
  [Central Spamreport](https://www.unrealircd.org/docs/Central_spamreport) or
  [Central Spamfilter](https://www.unrealircd.org/docs/Central_Spamfilter).
* [Conditional config](https://www.unrealircd.org/docs/Defines_and_conditional_config):
  using @if with a variable like `@if $VAR == "something"` always evaluated to false.
* A [`~forward`](https://www.unrealircd.org/docs/Extended_bans#Group_2:_actions)
  ban did not check ban exemptions (+e), always forwarding the user.
* When booting for the first time (without any cached files) the IRCd
  downloads GeoIP.dat. If that fails, e.g. due to lack of internet connectivity,
  we now show a warning and continue booting instead of it being a hard error.
  Note that we already dealt with this properly after the file has been cached
  (so after first download), see "What if your web server is down" in
  [Remote includes](https://www.unrealircd.org/docs/Remote_includes#What_if_your_web_server_is_down).

### Removed:
* The `tls-and-known-users` [security group](https://www.unrealircd.org/docs/Security-group_block)
  was confusing, in the sense that this group consisted of tls-users
  and of known-users (in an OR fashion, not AND).
  Since this group is rarely used it has now been removed altogether.
  If you used it in your configuration then you can still manually
  (re)create the security group with:
  ```
  security-group tls-and-known-users { identified yes; reputation-score 25; tls yes; }
  ```

### Developers and protocol:
* Modules can now provide SASL locally, see
  [Dev:Authentication module](https://www.unrealircd.org/docs/Dev:Authentication_module).

UnrealIRCd 6.1.5
-----------------

This is just a regular release with various enhancements and bug fixes.

### Enhancements:
* You can now use 
  [oper::auto-join](https://www.unrealircd.org/docs/Oper_block#auto-join)
  in an oper block to override the generic
  [set::oper-auto-join](https://www.unrealircd.org/docs/Set_block#set::oper-auto-join)
  setting.
* The `operclass` property is now available in the
  [security-group block](https://www.unrealircd.org/docs/Security-group_block)
  and mask items.  
  Eg: `security-group netadmin { operclass { netadmin; netadmin-with-override; } }`
* Support for IRCv3
  [`draft/no-implicit-names`](https://ircv3.net/specs/extensions/no-implicit-names)
* Improved performance by skipping useless `TAGMSG` spamfilter checks
  (e.g. for typing notifications).
* Improved performance if you have hundreds of non-regex spamfilters.
* Add more [Crule](https://www.unrealircd.org/docs/Crule) functions:
  * `is_away()` returns true if the client is currently away
  * `has_user_mode('x')` returns true if all the user modes are set on the
    client.
  * `has_channel_mode('x')` can be used for spamfilters with a destination
    channel, such as messages: it returns true if all specified channel modes
    are set on the channel.
* Add `example.pt.conf` - (Brazilian) Portuguese example configuration file.

### Changes:
* The config parser now logs a warning if you have a `/*` within a `/*`

### Fixes:
* The whowasdb module caused `WHOWAS` entries to vanish (way too soon)
* If your shell account only allowed very few file descriptors (eg: `ulimit -n`
  returned `150`), then UnrealIRCd would fail to boot. This, because due to
  reserved file descriptors you would have 0 left, or even a negative number.
* Crash when running `SPAMFILTER` as an IRCOp when using UTF8 spamfilters.
* [Set blocks for a security group](https://www.unrealircd.org/docs/Set_block#Set_block_for_a_security_group)
  allow you to set a custom 
  [set::modes-on-connect](https://www.unrealircd.org/docs/Set_block#set::modes-on-connect)
  for a security group. However this setting happened too early, so security
  groups matching account names or 'identified' (when using
  [SASL](https://www.unrealircd.org/docs/SASL)) were not working.
* `+I ~operclass` was not working properly.
* Removed confusing "Central blocklist too slow to respond" message when using
  [soft bans](https://www.unrealircd.org/docs/Soft_ban) or a
  [require authentication block](https://www.unrealircd.org/docs/Require_authentication_block).

UnrealIRCd 6.1.4
-----------------
This release fixes a crash issue with websockets in UnrealIRCd 6.1.0 - 6.1.3.

The full advisory with all details is available at:
https://forums.unrealircd.org/viewtopic.php?t=9340

See that advisory also for a way to hot-patch, to fix the crash issue on *NIX
without restarting (zero downtime). If you choose to go with the hot-patch,
then you don't need to upgrade to 6.1.4.

UnrealIRCd 6.1.4 is for those people who run Windows, or otherwise just feel
like it is a good time to do a full upgrade (with restart). It's naturally
also for new installations.

### Fixes:
* Crash that can be triggered by users when
  [Websockets](https://www.unrealircd.org/docs/WebSocket_support)
  are in use (a listen block with `listen::options::websocket`).
  This was assigned CVE-2023-50784.
* In 6.1.3, [Websockets](https://www.unrealircd.org/docs/WebSocket_support)
  were not working with Chrome and possibly other browsers.
  The fix for this is also included in the hot-patch (for 6.1.3 only).

UnrealIRCd 6.1.3
-----------------

The main focus of this release is adding countermeasures against large
scale spam/drones. We do this by offering a central API which can be used
for accessing Central Blocklist, Central Spamreport and Central Spamfilter.

### Enhancements:
* Central anti-spam services:
  * The services from below require a central-api key, which
    you can [request here](https://www.unrealircd.org/central-api/).
  * [Central Blocklist](https://www.unrealircd.org/docs/Central_Blocklist)
    is an attempt to detect and block spammers. It works similar to DNS
    Blacklists but the central blocklist receives many more details about the
    user that is trying to connect and therefore can make a better decision on
    whether a user is likely a spammer.
  * [Central Spamreport](https://www.unrealircd.org/docs/Central_spamreport)
    allows you to send spam reports (user details, last sent lines) via
    the `SPAMREPORT` command. This information may then be used to improve
    [Central Blocklist](https://www.unrealircd.org/docs/Central_Blocklist)
    and/or [Central Spamfilter](https://www.unrealircd.org/docs/Central_Spamfilter).
  * The [Central Spamfilter](https://www.unrealircd.org/docs/Central_Spamfilter),
    which provides spamfilter { } blocks that are centrally managed, is
    now fetched from a different URL if you have an Central API key set.
    This way, we can later provide spamfilter { } blocks that build on
    central blocklist scoring functionality, and also so we don't have to
    reveal all the central spamfilter blocks to the world.
* New option `auto` for
  [set::hide-ban-reason](https://www.unrealircd.org/docs/Set_block#set::hide-ban-reason),
  which is now the default. This will hide the *LINE reason to other users
  if the *LINE reason contains the IP of the user, for example when it contains
  a DroneBL URL which has `lookup?ip=XXX`. This to protect the privacy of the user.
  Other possible settings are `no` (never hide, the previous default) and
  `yes` to always hide the *LINE reason. In all cases the user affected by
  the server ban can still see the reason and IRCOps too.
* Make [Deny channel](https://www.unrealircd.org/docs/Deny_channel_block)
  support escaped sequences like `channel "#xyz\*";` so you can match
  a literal `*` or `?` via `\*` and `\?`.
* New option
  [listen::options::websocket::allow-origin](https://www.unrealircd.org/docs/Listen_block#options_block_(optional)):
  this allows to restrict websocket connections to a list of websites
  (the sites hosting the HTML/JS page that makes the websocket connection).
  It doesn't *securely* restrict it though, non-browsers will bypass this
  restriction, but it can still be useful to restrict regular webchat users.
* The [Proxy block](https://www.unrealircd.org/docs/Proxy_block) already
  had support for reverse proxying with the `Forwarded` header. Now it
  also properly supports `X-Forwarded-For`. If you previously used a proxy
  block with type `web`, then you now need to choose one of the new types
  explicitly. Note that using a reverse proxy for IRC traffic is rare
  (see the proxy block docs for details), but we offer the option.

### Changes:
* Reserve more file descriptors for internal use. For example, when there
  are 10,000 fd's are available we now reserve 250, and when 2048 are
  available we reserve 32. This so we have more fd's available to handle
  things like log files, do HTTPS callbacks to blacklists, etc.
* Get rid of compiler check for modules vs core, this is mostly an issue
  when you are upgrading a system (eg. Linux distro) and it would previously
  make REHASHing impossible after such an upgrade. Though, if you are doing
  a major distro upgrade you can still be bitten by things like library
  removals such as major openssl upgrades.
* Make `$client.details` in logs follow the ident rules for users in
  the handshake too, so use the `~` prefix if ident lookups are enabled
  and identd fails etc.
* More validation for operclass names (a-zA-Z0-9_-)
* Hits for central-blocklist are now broadcasted globally instead of
  staying on the same server.

### Fixes:
* When using a trusted reverse proxy with the
  [Proxy block](https://www.unrealircd.org/docs/Proxy_block),
  under some circumstances it was possible for end-users to spoof IP's.
* Crash issue when a module is reloaded (not unloaded) and that module
  no longer provides a particular moddata object, e.g. because it was
  renamed or no longer needed. This is rare, but did happen for one
  third party module recently.
* The crash reporter was no longer able to submit reports.
* [Module manager](https://www.unrealircd.org/docs/Module_manager) fixes
* For people running git versions, who did not use 'make clean', 3rd party
  modules were not always automatically recompiled, causing potential
  problems such as crashes.
* Fix memory leak when unloading a module for good and that module provided
  ModData objects for "unknown users" (users still in the handshake).
* Don't ask to generate TLS certificate if one already exists (issue
  introduced in 6.1.2).

### Developers and protocol:
* New hooks: `HOOKTYPE_WATCH_ADD`, `HOOKTYPE_WATCH_DEL`,
  `HOOKTYPE_MONITOR_NOTIFICATION`.
* The hook `HOOKTYPE_IS_HANDSHAKE_FINISHED` is now properly called
  at all places.
* A new [URL API](https://www.unrealircd.org/docs/Dev:URL_API)
  to easily fetch URLs from modules.

UnrealIRCd 6.1.2
-----------------
UnrealIRCd 6.1.2 focuses on adding spamfilter features but also contains
various other new features and some fixes.

The 6.1.2.1 release fixed a spamfilter::rule crash in 6.1.2.  
The 6.1.2.2 release fixed: tkldb accidentally storing central spamfilters,
a crash while booting if you previously used spamfilters with non-UTF8
characters in them, and fix a possible crash with SETNAME when using the
SPAMFILTER 'u' target.  
The 6.1.2.3 release fixed: UTF8 not working in spamfilter { } blocks
and a possible crash on REHASH if you have typos or other errors in the
config file. Also fixing ::exclude-security-group not working and we
now give DNSBL lookups some more time.

### Enhancements:
* We now give tips on (security) best practices depending on settings in your
  configuration file, such as using plaintext oper passwords in the config
  file. It is generally suggested to follow this advice, but you could disable
  such advice via
  [set::best-practices](https://www.unrealircd.org/docs/Set_block#set::best-practices).
* [security-group { } block](https://www.unrealircd.org/docs/Security-group_block)
  and [mask item](https://www.unrealircd.org/docs/Mask_item) enhancements:
  * Add support for `channel "#xyz";` and `channel "@#need_ops_here";`
  * Add support for [Crule](https://www.unrealircd.org/docs/Crule) to allow
    things like `rule "inchannel('@#main')||reputation()>1000";`
* DNS Blacklists are now checked again some time after the user is connected.
  This will kill/ban users who are already online and got blacklisted later
  by for example DroneBL.
  * This is controlled via
    [set::blacklist::recheck-time](https://www.unrealircd.org/docs/Set_block#set::blacklist::recheck-time)
    and can also be set to `never` if you don't want rechecking.
  * To skip checking for specific blacklists, you can set
    [blacklist::recheck](https://www.unrealircd.org/docs/Blacklist_block)
    to `no`.
* The [reputation score](https://www.unrealircd.org/docs/Reputation_score)
  of connected users (actually IP's) is increased every 5 minutes. We still
  do this, but only for users who are at least in one channel that has 3
  or more members. This setting is tweakable via
  [set::reputation::score-bump-timer-minimum-channel-members](https://www.unrealircd.org/docs/Set_block#set::reputation).
  Setting this to 0 means to bump scores also for people who are in no
  channels at all, which was the behavior in previous UnrealIRCd versions.  
  Note: this new feature won't work properly when you have any older UnrealIRCd
  servers on the network (older than 6.1.2), as the older servers will still
  bump scores for everyone, including users in no channels, and this higher
  score will get synced back eventually to all other servers.
* [spamfilter { } block](https://www.unrealircd.org/docs/Spamfilter_block) improvements:
  * Spamfilters now always run, even for users that are exempt via a
    [except ban block](https://www.unrealircd.org/docs/Except_ban_block)
    with `type spamfilter`. However, for exempt users no action is taken
    or logged. This allows us to count normal hits and count hits for except users.
    The idea is that the hits for except users can be a useful measurement
    to detect false positives. These hit counts are exposed in `SPAMFILTER`
    and `STATS spamfilter`.
  * Optional items allowing more complex rules:
    * [spamfilter::rule](https://www.unrealircd.org/docs/Spamfilter_block#Spamfilter_rule):
      with minimal 'if'-like preconditions and functions.
      If this returns false then the spamfilter will not run at all (no hit).
    * spamfilter::except: this is meant as an alternative to 'rule' and
      works like a regular [except item](https://www.unrealircd.org/docs/Mask_item).
      If this matches, then the spamfilter will not run at all (no hit).
  * New target type `raw` (or `R` on IRC) to match a raw command / IRC
    protocol line (except message tags), such as `LIST*`. Naturally one
    needs to be very careful with these since a wrong filter could cause
    all/essential traffic to be rejected.
  * The `action` item now supports multiple actions:
    * A new action `stop` to stop other spamfilters from processing.
    * A new action `set` to
      [set a TAG](https://www.unrealircd.org/docs/Spamfilter_block#Setting_tags)
      on a user, or change the value of one. It also supports changing
      the [reputation score](https://www.unrealircd.org/docs/Reputation_score).
    * A new action `report` to call a spamreport block, see next.
* A new [spamreport { } block](https://www.unrealircd.org/docs/Spamreport_block):
  * This can do a HTTP(S) call to services like DroneBL to report spam hits,
    so they can blacklist the IP address and other users on IRC can benefit.
* Optional [Central Spamfilter](https://www.unrealircd.org/docs/Central_spamfilter):
  This will fetch and refresh spamfilter rules every hour from unrealircd.org.
  * This feature is not enabled by default.
    Use `set { central-spamfilter { enabled yes; } }` to enable.
  * set::central-spamfilter::feed decides which feed to use: `fast` for
    early access to spamfilter rules that are new, and `standard` (the
    default) for rules that have been in fast for a while.
  * set::central-spamfilter::except defines who will never be affected by
    central spamfilters. By default it is: users with a reputation score of
    more than 2016 (7 days online unregged, or 3.5 days as identified user)
    or having a host of *.irccloud.com. Spam matches for users that fall
    in this ::except group are counted as false positives and no action is
    taken or logged.
  * See the [Central Spamfilter](https://www.unrealircd.org/docs/Central_spamfilter)
    article for the disclaimer and all other options you can set.
* [set::spamfilter::utf8](https://www.unrealircd.org/docs/Set_block#set::spamfilter::utf8)
  is now on by default:
  * This means you can safely use UTF8 characters in like `[]` in regex.
  * Case insensitive matches work better. For example, for extended
    Latin, a spamfilter on `ę` then also matches `Ę`.
  * Other PCRE2 features such as [\p](https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC5)
    can then be used. For example the regex `\p{Arabic}` would block all Arabic script.
    See also this [full list of scripts](https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC7).
    Please use this new tool with care. Blocking an entire language or script
    is quite a drastic measure.
  * You can turn it off via: `set { spamfilter { utf8 no; } }`
* Via [set::spamfilter::show-message-content-on-hit](https://www.unrealircd.org/docs/Set_block#set::spamfilter::show-message-content-on-hit)
  you can now configure to hide the message content in spamfilter hit
  messages. Generally it is very useful to see if a spamfilter hit is
  correct or not, so the default is 'always', but it also has privacy
  implications so there is now this option to disable it.
* You can restrict includes to only contain certain blocks, the style is:
  ```
  include "some-file-or-url" { restrict-config { name-of-block; name-of-block2; } }
  ```
* A new `~flood` [extended ban](https://www.unrealircd.org/docs/Extended_bans).
  This mode allows you to exempt users from channel mode `+f` and `+F`.
  It was actually added in a previous version (6.1.0) but never made
  it to the release notes. The syntax is: ~flood:types:mask, where
  *types* are the same letters as used in
  [channel mode +f](https://www.unrealircd.org/docs/Channel_anti-flood_settings#Channel_mode_f).
  Example: `+e ~flood:t:*!*@*.textflood.example.org`

### Changes:
* We now compile the argon2 library shipped with UnrealIRCd by default,
  because it is often two times faster than the OS library. If you don't
  want this, which would be quite rare but for example because you are
  packaging UnrealIRCd as a .deb or .rpm, then you can use
  `--with-system-argon2` as a configure option.
* The argon2 parameters have been lowered a bit, this so the hashing
  speed is acceptable for our purposes.

### Fixes:
* Temporary high CPU usage (99%) under some conditions
* UnrealIRCd has watch away notification since 2008, this is indicated in
  RPL_ISUPPORT via `WATCHOPTS=A` and then the syntax to actually use this
  is `WATCH A +Nick1 +Nick2 etc.`. In UnrealIRCd 6 there was a bug where
  it would not always correctly inform about the away status, that bug
  has now been fixed.
* On 32 bit architectures you can now use more than 32 channel modes.
* [Set block for a security group](https://www.unrealircd.org/docs/Set_block#Set_block_for_a_security_group):
  was not working for the `unknown-users` group.
* A leading slash was silently stripped in config file items, when not in quotes.
* `./unrealircd module upgrade` only showed output for one module upgrade,
  even when multiple modules were upgraded.

### Developers and protocol:
* Changes in numeric 229 (RPL_STATSSPAMF): Now includes hits and hits for
  users that are exempt, two counters inserted right before the last
  argument (the regex).
* Several API changes, like `place_host_ban` to `take_action`

UnrealIRCd 6.1.1.1
-------------------
This 6.1.1.1 version is an update to 6.1.1: a bug and memory leak was fixed
related to maxperip handling if a WEBIRC proxy/gateway was used. To trigger
this bug the WEBIRC proxy would need to use an IPv6 connection to UnrealIRCd
and serve/spoof an IPv4 client, or vice-versa (be on IPv4 and spoof an IPv6).

The original 6.1.1 announcement is below:

UnrealIRCd 6.1.1
-----------------
UnrealIRCd 6.1.1 comes with various bug fixes and performance improvements,
especially for channels with thousands of users.

It also has more options to override settings per security group,
for example if you want to give trusted users or bots more rights or
higher flood rates than regular users. All these options are now
in a single [Special users](https://www.unrealircd.org/docs/Special_users)
article on the wiki.

Other notable features are better connection errors to SSL/TLS users
and a new proxy { } block for websocket reverse proxies.

See the full release notes below. As usual on *NIX you can upgrade easily
with the command: `./unrealircd upgrade`

### Enhancements:
* Two new features that are conditionally on:
  * SSL/TLS users will now correctly receive the error message if they are
    rejected due to throttling (connect-flood) and some other situations.
  * DNS lookups are done before throttling. This allows exempting a hostname
    from both maxperip and connect-flood restrictions.  
    A good example for IRCCloud would be:
    ```
    except ban {
        mask *.irccloud.com;
        type { maxperip; connect-flood; }
    }
    ```
  * Both features are temporarily disabled whenever a 
    [high rate of connection attempts](https://www.unrealircd.org/docs/FAQ#hi-conn-rate)
    is detected, to save CPU and other resources during such an attack.
    The default rate is 1000 per second, so this would be unusual to trigger
    accidentally.
* It is now possible to override some set settings per-security group by
  having a set block with a name, like `set unknown-users { }`
  * You could use this to set more limitations for unknown-users:
    ```
    set unknown-users {
            max-channels-per-user 5;
            static-quit "Quit";
            static-part yes;
    }
    ```
  * Or to set higher values (higher than the normal set block)
    for trusted users:
    ```
    security-group trusted-bots {
            account { BotOne; BotTwo; }
    }
    set trusted-bots {
            max-channels-per-user 25;
    }
    ```
  * Currently the following settings can be used in a set xxx { } block:
    set::auto-join, set::modes-on-connect, set::restrict-usermodes,
    set::max-channels-per-user, set::static-quit, set::static-part.
  * See also [Special users](https://www.unrealircd.org/docs/Special_users)
    in the documentation for applying settings to a security groups.
* New [`proxy { }` block](https://www.unrealircd.org/docs/Proxy_block)
  that can be used for spoofing IP addresses when:
  * Reverse proxying websocket connections (eg. via NGINX, a load
    balancer or other reverse proxy)
  * WEBIRC/CGI:IRC gateways. This will replace the old `webirc { }`
    block in the future, though the old one will still work for now.
* New setting [set::handshake-boot-delay](https://www.unrealircd.org/docs/Set_block#set%3A%3Ahandshake-boot-delay)
  which allows server linking autoconnects to kick in (and incoming
  servers on serversonly ports), before allowing clients in. This
  potentially avoids part of the mess when initially linking on-boot.
  This option is not turned on by default, you have to set it explicitly.
  * This is not a useful feature on hubs, as they don't have clients.
  * It can be useful on client servers, if you `autoconnect` to your hub.
  * If you connect services to a server with clients this can be useful
    as well, especially in single-server setups. You would have to set
    a low `retrywait` in your anope conf (or similar services package)
    of like `5s` instead of the default `60s`.
    Then after an IRCd restart, your services link in before your clients
    and your IRC users have SASL available straight from the start.
* JSON-RPC:
  * New call [`log.list`](https://www.unrealircd.org/docs/JSON-RPC:Log#log.list)
    to fetch past 1000 log entries. This functionality is only loaded if
    you include `rpc.modules.default.conf`, so not wasting any memory on
    servers that are not used for JSON-RPC.

### Changes:
* [set::topic-setter](https://www.unrealircd.org/docs/Set_block#set::topic-setter) and
  [set::ban-setter](https://www.unrealircd.org/docs/Set_block#set::ban-setter)
  are now by default set to `nick-user-host` instead of `nick`, so you can see
  the full nick!user@host of who set the topic/ban/exempt/invex.
* You can no longer (accidentally) load an old `modules.default.conf`.
  People must always use the shipped version of this file as the file VERY
  clearly says in the beginning (see also that file for instructions on
  how to deal with customizations). People run into lots of (strange)
  problems, not only missing nice new functionality, but also Services not
  working because the svslogin module is not loaded, etc.  
  Usually mistakes with an old modules.default.conf are not deliberate,
  like a cp *.conf of an old installation, so this error should be helpful
  for those users (who otherwise tend to bang their head for hours).
* Some small DNS performance improvements:
  * We now 'negatively cache' unresolved hosts for 60 seconds.
  * The maximum number of cached records (positive and negative) was raised
    to 4096.
  * We no longer use "search domains" to avoid silly lookups for like
    `4.3.2.1.dnsbl.dronebl.org.mydomain.org`.
* Data buffer chunks bumped from 512 bytes to ~4K. This results in less write
  calls (lower CPU usage) and more data per TCP/IP packet.
* We now cache sending of lines in `sendto_channel` via a new "LineCache"
  system. It saves CPU on (very) large channels.
* Several other performance improvements such as checking maxperip via
  a hash table and faster invisibility checks for delayjoin.
* Blacklist hits are now logged globally. This means they show up in
  snomask `B`, are logged, and show up in the webpanel "Logs" view.
* The event `REMOTE_CLIENT_JOIN` was mass-triggered when servers were
  syncing. They are now hidden, like `REMOTE_CLIENT_CONNECT`.
* Update shipped libraries: c-ares to 1.19.1

### Fixes:
* Crash on FreeBSD/NetBSD when using JSON-RPC, due to clashing rpc_call
  symbol in their libc library.
* Crash when removing a `listen { }` block for websocket or rpc (or
  changin the port number)
* When using the webpanel, if an IRC client tried to connect with the same
  IP as the webpanel server, it would often receive the error "Too many
  unknown connections". This only affected non-localhost connections.
* The [`require module` block](https://www.unrealircd.org/docs/Require_module_block)
  was only checked of one side of the link, thus partially not working.

### Removed:
* [set::maxbanlength](https://www.unrealircd.org/docs/Set_block#set::maxbanlength)
  has been removed as it was not deemed useful and only confusing
  to users and admins.

### Developers and protocol:
* Server to server lines can now be 16384 bytes in size when
  `PROTOCTL BIGLINES` is set. This will allow us to do things more
  efficiently and possibly raise some other limits in the future.
  This 16k is the size of the complete line, including sender,
  message tags, content and \r\n. Also, in server-to-server traffic
  we now allow 30 parameters (MAXPARA*2).  
  The original input size limits for non-servers remain the same: the
  complete line can be 4k+512, with the non-mtag portion limit set
  at 512 bytes (including \r\n), and MAXPARA is still 15 as well.
* In command handlers, individual `parv[]` elements can be 510 bytes
  max, even if they add up like parv[1] and parv[2] both being 510
  bytes each. If you need more than that, then you need to set the
  flag `CMD_BIGLINES` in `CommandAdd()`, then an individual parameter
  can be near ~16k. This is so, because a lot of the code does not
  expect parameters bigger than 512 bytes (but can still handle
  the total of parameters being greater than 512). The new flag allows
  gradually opting in commands to allow bigger parameters, after
  such code has been checked and modified to handle it.
* In `HOOKTYPE_PRE_CHANMSG` the `mtags` is now a `MessageTag **`,
  so a pointer-to-a-pointer rather than a pointer, to allow stripping
  message tags by modules.

UnrealIRCd 6.1.0
-----------------
This is UnrealIRCd 6.1.0 stable. It is the direct successor to 6.0.7, there
will be no 6.0.8.

This release contains several channel mode `+f` enhancements and introduces a
new channel mode `+F` which works with flood profiles like `+F normal` and
`+F strict`. It is much easier for users than the scary looking mode +f.

UnrealIRCd 6.1.0 also contains lots of JSON-RPC improvements, which is used
by the [UnrealIRCd admin panel](https://www.unrealircd.org/docs/UnrealIRCd_webpanel).
Live streaming of logs has been added and the webpanel now communicates to
UnrealIRCd which web user issued a command (eg: who issued a kill, who
changed a channel mode, ..).

Other improvements are whowasdb (persistent WHOWAS history) and a new guide
on running a Tor Onion service. The release also fixes a crash bug related
to remote includes and fixes multiple memory leaks.

### Enhancements:
* Channel flood protection improvements:
  * New [channel mode `+F`](https://www.unrealircd.org/docs/Channel_anti-flood_settings)
    (uppercase F). This allows the user to choose a "flood profile",
    which (behind the scenes) translates to something similar to an `+f` mode.
    This so end-users can simply choose an `+F` profile without having to learn
    the complex channel mode `+f`.
    * For example `+F normal` effectively results in
      `[7c#C15,30j#R10,10k#K15,40m#M10,8n#N15]:15`
    * Multiple profiles are available and changing them is possible,
      see [the documentation](https://www.unrealircd.org/docs/Channel_anti-flood_settings).
    * Any settings in mode `+f` will override the ones of the `+F` profile.
      To see the effective flood settings, use `MODE #channel F`.
  * You can optionally set a default profile via
    [set::anti-flood::channel::default-profile](https://www.unrealircd.org/docs/Channel_anti-flood_settings#Default_profile).
    This profile is used if the channel is `-F`. If the user does not
    want channel flood protection then they have to use an explicit `+F off`.
  * When channel mode `+f` or `+F` detect that a flood is caused by >75% of
    ["unknown-users"](https://www.unrealircd.org/docs/Security-group_block),
    the server will now set a temporary ban on `~security-group:unknown-users`.
    It will still set `+i` and other modes if the flood keeps on going
    (eg. is caused by known-users).
  * Forced nick changes (eg. by NickServ) are no longer counted in nick flood
    for channel mode `+f`/`+F`.
  * When a server splits on the network, we now temporarily disable +f/+F
    join-flood protection for 75 seconds
    ([set::anti-flood::channel::split-delay](https://www.unrealircd.org/docs/Channel_anti-flood_settings#config)).
    This because a server splitting could mean that server has network problems
    or has died (or restarted), in which case the clients would typically
    reconnect to the remaining other servers, triggering an +f/+F join-flood and
    channels ending up being `+i` and such. That is not good because we want
    +f/+F to be as effortless as possible, with as little false positives as
    possible.
    * If your network has 5+ servers and the user load is spread evenly among
      them, then you could disable this feature by setting the amount of seconds
      to `0`. This because in such a scenario only 1/5th (20%) of the users
      would reconnect and hopefully don't trigger +f/+F join floods.
  * All these features only work properly if all servers are on 6.1.0-rc1 or later.
* New module `whowasdb` (persistent `WHOWAS` history): this saves the WHOWAS
  history on disk periodically and when we terminate, so next server boot
  still has the WHOWAS history. This module is currently not loaded by default.
* New option [listen::spoof-ip](https://www.unrealircd.org/docs/Listen_block#spoof-ip),
  only valid when using UNIX domain sockets (so listen::file).
  This way you can override the IP address that users come online with when
  they use the socket (default was and still is `127.0.0.1`).
* Add a new guide [Running Tor Onion service with UnrealIRCd](https://www.unrealircd.org/docs/Running_Tor_Onion_service_with_UnrealIRCd)
  which uses the new listen::spoof-ip and optionally requires a services account.
* [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC):
  * Logging of JSON-RPC requests (eg. via snomask `+R`) has been improved,
    it now shows:
    * The issuer, such as the user logged in to the admin panel (if known)
    * The parameters of the request
  * The JSON-RPC calls
    [`channel.list`](https://www.unrealircd.org/docs/JSON-RPC:Channel#channel.list),
    [`channel.get`](https://www.unrealircd.org/docs/JSON-RPC:Channel#channel.get),
    [`user.list`](https://www.unrealircd.org/docs/JSON-RPC:User#user.list) and
    [`user.get`](https://www.unrealircd.org/docs/JSON-RPC:User#user.get)
    now support an optional argument `object_detail_level` which specifies how detailed
    the [Channel](https://www.unrealircd.org/docs/JSON-RPC:Channel#Structure_of_a_channel)
    and [User](https://www.unrealircd.org/docs/JSON-RPC:User#Structure_of_a_client_object)
    response object will be. Especially useful if you don't need all the
    details in the list calls.
  * New JSON-RPC methods
    [`log.subscribe`](https://www.unrealircd.org/docs/JSON-RPC:Log#log.subscribe) and
    [`log.unsubscribe`](https://www.unrealircd.org/docs/JSON-RPC:Log#log.unsubscribe)
    to allow real-time streaming of
    [JSON log events](https://www.unrealircd.org/docs/JSON_logging).
  * New JSON-RPC method
    [`rpc.set_issuer`](https://www.unrealircd.org/docs/JSON-RPC:Rpc#rpc.set_issuer)
    to indiciate who is actually issuing the requests. The admin panel uses this
    to communicate who is logged in to the panel so this info can be used in logging.
  * New JSON-RPC methods
    [`rpc.add_timer`](https://www.unrealircd.org/docs/JSON-RPC:Rpc#rpc.add_timer) and
    [`rpc.del_timer`](https://www.unrealircd.org/docs/JSON-RPC:Rpc#rpc.del_timer)
    so you can schedule JSON-RPC calls, like stats.get, to be executed every xyz msec.
  * New JSON-RPC method
    [`whowas.get`](https://www.unrealircd.org/docs/JSON-RPC:Whowas#whowas.get)
    to fetch WHOWAS history.
  * Low ASCII is no longer filtered out in strings in JSON-RPC, only in JSON logging.
* A new message tag `unrealircd.org/issued-by` which is IRCOp-only (and
  used intra-server) to communicate who actually issued a command.
  See [docs](https://www.unrealircd.org/issued-by).

### Changes:
* The RPC modules are enabled by default now. This so remote RPC works
  from other IRC servers for calls like `modules.list`. The default
  configuration does NOT enable the webserver nor does it cause
  listening on any socket for RPC, for that you need to follow the
  [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC) instructions.
* The [blacklist-module](https://www.unrealircd.org/docs/Blacklist-module_directive)
  directive now accepts wildcards, eg `blacklist-module rpc/*;`
* The setting set::modef-boot-delay has been moved to
  [set::anti-flood::channel::boot-delay](https://www.unrealircd.org/docs/Channel_anti-flood_settings#config).
* We now only exempt `127.0.0.1` and `::1` from banning by default
  (hardcoded in the source). Previously we exempted whole `127.*` but
  that gets in the way if you want to allow Tor with a
  [require authentication](https://www.unrealircd.org/docs/Require_authentication_block)
  block or soft-ban. Now you can just tell Tor to bind to `127.0.0.2`
  so its not affected by the default exemption.

### Fixes:
* Crash if there is a parse error in an included file and there are
  other remote included files still being downloaded.
* Memory leak in WHOWAS
* Memory leak when connecting to a TLS server fails
* Workaround a bug in some websocket implementations where the WSOP_PONG
  frame is unmasked (now permitted).

### Developers and protocol:
* The `cmode.free_param` definition changed. It now has an extra argument
  `int soft` and for return value you will normally `return 0` here.
  You can `return 1` if you resist freeing, which is rare and only used by
  `+F` with set::anti-flood::channel::default-profile.
* New `cmode.flood_type_action` which can be used to indicate a channel mode
  can be used from +f/+F as an action. You need to specify for which
  flood type your mode is, eg `cmode.flood_type_action = 'j';` for joinflood.
* JSON-RPC supports
  [UNIX domain sockets](https://www.unrealircd.org/docs/JSON-RPC:Technical_documentation#UNIX_domain_socket)
  for making RPC calls. If this is used, we now split on `\n` (newline)
  so multiple parallel requests can be handled properly.
* Message tag `unrealircd.org/issued-by`, sent to IRCOps only.
  See [docs](https://www.unrealircd.org/issued-by).

UnrealIRCd 6.0.7
-----------------

UnrealIRCd 6.0.7 makes WHOWAS show more information to IRCOps and adds an
experimental spamfilter feature. It also contains other enhancements and
quite a number of bug fixes. One notable change is that on linking of anope
or atheme, every server will now check if they have ulines { } for that
services server, since it's a common mistake to forget this, leading to
desyncs or other weird problems.

### Enhancements:
* [Spamfilter](https://www.unrealircd.org/docs/Spamfilter) can now be made UTF8-aware:
  * This is experimental, to enable: `set { spamfilter { utf8 yes; } }`
  * Case insensitive matches will then work better. For example, for extended
    Latin, a spamfilter on `ę` then also matches `Ę`.
  * Other PCRE2 features such as [\p](https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC5)
    can then be used. For example the regex `\p{Arabic}` would block all Arabic script.
    See also this [full list of scripts](https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC7).
    Please use this new tool with care. Blocking an entire language or script
    is quite a drastic measure.
  * As a consequence of this we require PCRE2 10.36 or newer. If your system
    PCRE2 is older, then the UnrealIRCd-shipped-library version will be compiled
    and `./Config` may take a little longer than usual.
* `WHOWAS` now shows IP address and account information to IRCOps
* Allow services to send a couple of protocol messages in the
  unregistered / SASL stage. These are: `CHGHOST`, `CHGIDENT`
  and `SREPLY`
  * This allows services to set the vhost on a user during SASL,
    so the user receives the vhost straight from the start, before
    all the auto-joining/re-rejoining of channels.
  * Future anope/atheme/etc services will presumably support this.
* [WebSocket](https://www.unrealircd.org/docs/WebSocket_support) status is
  now synced over the network and an extra default
  [security group](https://www.unrealircd.org/docs/Security-group_block)
  `websocket-users` has been added. Similarly there is now
  security-group::websocket and security-group::exclude-websocket item.
  Same for [mask items](https://www.unrealircd.org/docs/Mask_item) such
  as in [set::restrict-commands::command::except](https://www.unrealircd.org/docs/Restrict_commands).
* Support for IRCv3 [Standard Replies](https://ircv3.net/specs/extensions/standard-replies).
  Right now nothing fancy yet, other than us sending `ACCOUNT_REQUIRED_TO_CONNECT`
  from the authprompt module when a user is
  [soft-banned](https://www.unrealircd.org/docs/Soft_ban).
* Add support for sending IRCv3 Standard Replies intra-server, eg
  from services (`SREPLY` server-to-server command)
* Support `NO_COLOR` environment variable, as per [no-color.org](https://no-color.org).

### Changes:
* We now verify that all servers have
  [ulines { }](https://www.unrealircd.org/docs/Ulines_block) for Anope and
  Atheme servers and reject the link if this is not the case.
* The `FLOOD_BLOCKED` log message now shows the target of the flood
  for `target-flood-user` and `target-flood-channel`.
* When an IRCOp sets `+H` to hide ircop status, only the swhois items that
  were added through oper will be hidden (and not the ones added by eg. vhost).
  Previously all were hidden.
* Update shipped libraries: c-ares to 1.19.0, Jansson to 2.14, PCRE2 to 10.42,
  and on Windows LibreSSL to 3.6.2 and cURL to 8.0.1.

### Fixes:
* Crash if a third party module is loaded which allows very large message tags
  (e.g. has no length check)
* Crash if an IRCOp uses
  [`unrealircd.org/json-log`](https://www.unrealircd.org/docs/JSON_logging#Enabling_on_IRC)
  on IRC and during `REHASH` some module sends log output during MOD_INIT
  (eg. with some 3rd party modules)
* Crash when parsing [deny link block](https://www.unrealircd.org/docs/Deny_link_block)
* The [Module manager](https://www.unrealircd.org/docs/Module_manager)
  now works on FreeBSD and similar.
* In `LUSERS` the "unknown connection(s)" count was wrong. This was just a
  harmless counting error with no other effects.
* Silence warnings on Clang 15+ (eg. Ubuntu 23.04)
* Don't download `GeoIP.dat` if you have 
  [`blacklist-module geoip_classic;`](https://www.unrealircd.org/docs/Blacklist-module_directive)
* Channel mode `+S` stripping too much on incorrect color codes.
* Make [`@if module-loaded()`](https://www.unrealircd.org/docs/Defines_and_conditional_config)
  work correctly for modules that are about to be unloaded during REHASH.
* Some missing notices if remotely REHASHing a server, and one duplicate line.
* Check invalid host setting in oper::vhost, just like we already have in vhost::vhost.

UnrealIRCd 6.0.6
-----------------

The main objective of this release is to enhance the new JSON-RPC functionality.
In 6.0.5 we made a start and in 6.0.6 it is expanded a lot, plus some important
bugs were fixed in it. Thanks everyone who has been testing the functionality!

The new [UnrealIRCd Administration Webpanel](https://github.com/unrealircd/unrealircd-webpanel/)
(which uses JSON-RPC) is very much usable now. It allows admins to view the
users/channels/servers lists, view detailed information on users and channels,
manage server bans and spamfilters, all from the browser.

Both the JSON-RPC API and the webpanel are work in progress. They will improve
and expand with more features over time.

If you are already using UnrealIRCd 6.0.5 and you are NOT interested in
JSON-RPC or the webpanel then there is NO reason to upgrade to 6.0.6.

As usual, on *NIX you can easily upgrade with `./unrealircd upgrade`

### Enhancements:
* The [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC) API for
  UnrealIRCd has been expanded a lot. From 12 API methods to 42:
  `stats.get`, `rpc.info`, `user.part`,
  `user.join`, `user.quit`, `user.kill`,
  `user.set_oper`, `user.set_snomask`, `user.set_mode`,
  `user.set_vhost`, `user.set_realname`,
  `user.set_username`, `user.set_nick`, `user.get`,
  `user.list`, `server.module_list`, `server.disconnect`,
  `server.connect`, `server.rehash`, `server.get`,
  `server.list`, `channel.kick`, `channel.set_topic`,
  `channel.set_mode`, `channel.get`, `channel.list`,
  `server_ban.add`, `server_ban.del`, `server_ban.get`,
  `server_ban.list`, `server_ban_exception.add`,
  `server_ban_exception.del`, `server_ban_exception.get`,
  `server_ban_exception.list`, `name_ban.add`,
  `name_ban.del`, `name_ban.get`, `name_ban.list`,
  `spamfilter.add`, `spamfilter.del`, `spamfilter.get`,
  `spamfilter.list`.
  * Server admins can read the [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC)
    documentation on how to get started. For developers, see the
    [Technical documentation](https://www.unrealircd.org/docs/JSON-RPC:Technical_documentation)
    for all info on the different RPC calls and the protocol.
  * Some functionality requires all servers to be on 6.0.6 or later.
  * Some functionality requires all servers to include
    `rpc.modules.default.conf` instead of only the single server that
    the webpanel interfaces with through JSON-RPC.
    When all servers have that file included then the API call
    `server.module_list` can work for remote servers, and the API call
    `server.rehash` for remote servers can return the actual rehash result
    and a full log of the rehash process. It is not used for any other
    API call at the moment, but in the future more API calls may need this
    functionality because it allows us to do things that are otherwise impossible
    or very hard.
  * Known issue: logging of RPC actions needs to be improved. For some API calls,
    like adding of server bans and spamfilters, this already works, but in
    other API calls it is not clearly logged yet "who did what".

### Changes:
* Previously some server protocol commands could only be used by
  services, commands such as `SVSJOIN` and `SVSPART`. We now allow SVS*
  command to be used by any servers, so the JSON-RPC API can use them.
  There's a new option
  [set::limit-svscmds](https://www.unrealircd.org/docs/Set_block#set::limit-svscmds)
  so one can revert back to the original situation, if needed.
* All JSON-RPC calls that don't change anything, such as `user.list`
  are now logged in the `rpc.debug` facility. Any call that changes
  anything like `user.join` or `spamfilter.add` is logged via `rpc.info`.
  This because JSON-RPC calls can be quite noisy and logging the
  read-only calls is generally not so interesting.

### Fixes:
* When using JSON-RPC with UnrealIRCd 6.0.5 it would often crash
* Fix parsing services version (anope) in `EAUTH`.

### Developers and protocol:
* A new `RRPC` server to server command to handle RPC-over-IRC.
  This way the JSON-RPC user, like the admin panel, can interface with
  a remote server. If you are writing an RPC handler, then the remote
  RPC request does not look much different than a local one, so you
  can just process it as usual. See the code for `server.rehash` or
  `server.module_list` for an example (src/modules/rpc/server.c).

UnrealIRCd 6.0.5
-----------------

This release adds experimental JSON-RPC support, a new TLINE command, the
`./unrealircd restart` command has been improved to check for config errors,
logging to files has been improved and there are several other enhancements.

There are also two important changes: 1) servers that use websockets now also
need to load the "webserver" module (so you may need to edit your config
file). 2) we now require TLSv1.2 or higher and a modern cipher for IRC clients.
This should be no problem for clients using any reasonably new SSL/TLS library
(from 2014 or later).

I would also like to take this opportunity to say that we are
[looking for webdevs to create an UnrealIRCd admin panel](https://forums.unrealircd.org/viewtopic.php?t=9257).
The previous attempt at this failed so we are looking for new people.

See the full release notes below for all changes in more detail.

As usual, on *NIX you can easily upgrade with `./unrealircd upgrade`

### Enhancements:
* Internally the websocket module has been split up into 3 modules:
  `websocket_common`, `webserver` and `websocket`. The `websocket_common` one
  is loaded by default via modules.default.conf, the other two are not.  
  **Important:** if you use websockets then you need to load two modules now (instead of only one):
  ```
  loadmodule "websocket";
  loadmodule "webserver";
  ```
* [JSON-RPC](https://www.unrealircd.org/docs/JSON-RPC) API for UnrealIRCd.
  This is work in progress.
* New `TLINE` command to test *LINEs. This can be especially useful for 
  checking how many people match an [extended server ban](https://www.unrealircd.org/docs/Extended_server_bans)
  such as `TLINE ~C:NL`
* The `./unrealircd start` command will now refuse to start if UnrealIRCd
  is already running.
* The `./unrealircd restart` command will validate the configuration file
  (it will call `./unrealircd configtest`). If there is a configuration
  error then the restart will not go through and the current UnrealIRCd
  process is kept running.
* When an IRCOp is outside the channel and does `MODE #channel` they will
  now get to see the mode parameters too. This depends on the `channel:see:mode:remote`
  [operclass permission](https://www.unrealircd.org/docs/Operclass_permissions)
  which all IRCOps have by default if you use the default operclasses.
* [Logging to a file](https://www.unrealircd.org/docs/Log_block) now creates
  a directory structure if needed.
  * You could already use:
    ```
    log { source { !debug; all; } destination { file "ircd.%Y-%m-%d.log"; } }
    ```
  * But now you can also use:
    ```
    log { source { !debug; all; } destination { file "%Y-%m-%d/ircd.log"; } }
    ```
    This is especially useful if you output to multiple log files and then
    want them grouped by date in a directory.
* Add additional variables in
  [blacklist::reason](https://www.unrealircd.org/docs/Blacklist_block):
  * `$blacklist`: name of the blacklist block
  * `$dnsname`: the blacklist::dns::name
  * `$dnsreply`: the DNS reply code
* Resolved technical issue so opers can `REHASH` from
  [Websocket connections](https://www.unrealircd.org/docs/WebSocket_support).
* In the [TLD block](https://www.unrealircd.org/docs/Tld_block) the use
  of `tld::motd` and `tld::rules` is now optional.
* Log which oper actually initiated a server link request (`CONNECT`)

### Changes:
* SSL/TLS: By default we now require TLSv1.2 or later and a modern cipher
  with forward secrecy. Otherwise the connection is refused.
  * Since UnrealIRCd 4.2.2 (March 2019) users see an on-connect notice with
    a warning when they use an outdated TLS protocol or cipher that does not
    meet these requirements.
  * This move also reflects the phase out of versions below TLSv1.2 which
    happened in browsers in 2020/2021.
  * In practice on the client-side this requires at least:
    * OpenSSL 1.0.1 (released in 2012)
    * GnuTLS 3.2.6 (2013)
    * Android 4.4.2 (2013)
    * Or presumably any other SSL/TLS library that is not 9+ years old
  * If you want to revert back to the previous less secure settings, then
    look under ''Previous less secure setting'' in
    [TLS Ciphers and protocols](https://www.unrealircd.org/docs/TLS_Ciphers_and_protocols).
* The code for handling
  [`set::anti-flood::everyone::connect-flood`](https://www.unrealircd.org/docs/Anti-flood_settings#connect-flood)
  is now in its own module `connect-flood`. This module is loaded by default,
  no changes needed in your configuration file.
* Similarly,
  [`set:max-unknown-connections-per-ip`](https://www.unrealircd.org/docs/Set_block#set::max-unknown-connections-per-ip)
  is now handled by the new module `max-unknown-connections-per-ip`. This module is loaded
  by default as well, no changes needed in your configuration file.
* Upgrade shipped PCRE2 to 10.41, curl-ca-bundle to 2022-10-11,
  on Windows LibreSSL to 3.6.1 and cURL to 7.86.0.
* After people do a major upgrade on their Linux distro, UnrealIRCd may
  no longer start due to an `error while loading shared libraries`.
  We now print a more helpful message and link to the new
  [FAQ entry](https://www.unrealircd.org/docs/FAQ#shared-library-error)
  about it.
* When timing out on the [authprompt](https://www.unrealircd.org/docs/Set_block#set::authentication-prompt)
  module, the error (quit message) is now the original (ban) reason for the
  prompt, instead of the generic `Registration timeout`.

### Fixes:
* Crash when linking. This requires a certain sequence of events: first
  a server is linked in successfully, then we need to REHASH, and then a new
  link attempt has to come in with the same server name (for example because
  there is a network issue and the old link has not timed out yet).
  If all that happens, then an UnreaIRCd 6 server may crash, but not always.
* Warning message about moddata creationtime when linking.
* [Snomask `+j`](https://www.unrealircd.org/docs/Snomasks) was not showing
  remote joins, even though it did show remote parts and kicks.
* Leak of 1 file descriptor per /REHASH (the control socket).
* Ban letters showing up twice in 005 EXTBAN=
* Setting [set::authentication-prompt::enabled](https://www.unrealircd.org/docs/Set_block#set::authentication-prompt)
  to `no` was ignored. The default is still `yes`.

### Developers and protocol:
* Add `CALL_CMD_FUNC(cmd_func_name)` for calling commands in the same
  module, see [this commit](https://github.com/unrealircd/unrealircd/commit/dc55c3ec9f19e5ed284e5a786f646d0e6bb60ef9).
  Benefit of this is that it will keep working if we ever change command paramters.
* Add `CALL_NEXT_COMMAND_OVERRIDE()` which can be used instead of
  `CallCommandOverride()`, see also [this commit](https://github.com/unrealircd/unrealircd/commit/4e5598b6cf0986095f757f31a2540b03e4d235dc).
  This too, will keep working if we ever change command parameters.
* During loading and rehash we now set `loop.config_status` to one of
  `CONFIG_STATUS_*` so modules (and core) can see at what step we are
  during configuration file and module processing.
* New RPC API. See the `src/modules/rpc/` directory for examples.
* New function `get_nvplist(NameValuePrioList *list, const char *name)`

UnrealIRCd 6.0.4.2
-------------------
Another small update to 6.0.4.x:

* Two IRCv3 specifications were ratified which we already supported as drafts:
  * Change CAP `draft/extended-monitor` to `extended-monitor`
  * Add message-tag `bot` next to existing (for now) `draft/bot`
* Update Turkish translations

UnrealIRCd 6.0.4.1
-------------------
This is a small update to 6.0.4. It fixes the following issues that were
present in all 6.0.x versions:

* Fix sporadic crash when linking a server (after successful authentication).
  This feels like a compiler bug. It affected only some people with GCC and
  only in some situations. When compiled with clang there was no problem.
  Hopefully we can work around it this way.
* Make /INVITE bypass (nearly) all channel mode restrictions, as it used to
  be in UnrealIRCd 5.x. Both for invites by channel ops and for OperOverride.
  This also fixes a bug where an IRCOp with OperOverride could not bypass +l
  (limit) and other restrictions and would have to resort back to using
  MODE or SAMODE. Only +b and +i could be bypassed via INVITE OperOverride.

(This cherry picks commit 0e6fc07bd9000ecc463577892cf2195a670de4be and
 commit 0d139c6e7c268e31ca8a4c9fc5cb7bfeb4f56831 from 6.0.5-git)

UnrealIRCd 6.0.4
-----------------

If you are already running UnrealIRCd 6 then read below. Otherwise, jump
straight to the [summary about UnrealIRCd 6](#Summary) to learn more
about UnrealIRCd 6.

### Enhancements:
* Show security groups in `WHOIS`
* The [security-group block](https://www.unrealircd.org/docs/Security-group_block)
  has been expanded and the same functionality is now available in
  [mask items](https://www.unrealircd.org/docs/Mask_item) too:
  * This means the existing options like `identified`, `webirc`, `tls` and
    `reputation-score` can be used in `allow::mask` etc.
  * New options (in both security-group and mask) are:
    * `connect-time`: time a user is connected to IRC
    * `security-group`: to check another security group
    * `account`: services account name
    * `country`: country code, as found by GeoIP
    * `realname`: realname (gecos) of the user
    * `certfp`: certificate fingerprint
  * Every option also has an exclude- variant, eg. `exclude-country`.
    If a user matches any `exclude-` option then it is considered not a match.
  * The modules [connthrottle](https://www.unrealircd.org/docs/Connthrottle),
    [restrict-commands](https://www.unrealircd.org/docs/Set_block#set::restrict-commands)
    and [antirandom](https://www.unrealircd.org/docs/Set_block#set::antirandom)
    now use the new `except` sub-block which is a mask item. The old syntax
    (eg <code>set::antirandom::except-webirc</code>) is still accepted by UnrealIRCd
    and converted to the appropriate new setting behind the scenes
    (<code>set::antirandom::except::webirc</code>).
  * The modules [blacklist](https://www.unrealircd.org/docs/Blacklist_block)
    and [antimixedutf8](https://www.unrealircd.org/docs/Set_block#set::antimixedutf8)
    now also support the `except` block (a mask item).
  * Other than that the extended functionality is available in these blocks:
    `allow`, `oper`, `tld`, `vhost`, `deny channel`, `allow channel`.
  * Example of direct use in a ::mask item:
    ```
    /* Spanish MOTD for Spanish speaking countries */
    tld {
        mask { country { ES; AR; BO; CL; CO; CR; DO; EC; SV; GT; HN; MX; NI; PA; PY; PE; PR; UY; VE; } }
        motd "motd.es.txt";
        rules "rules.es.txt";
    }
    ```
  * Example of defining a security group and using it in a mask item later:
    ```
    security-group irccloud {
        mask { ip1; ip2; ip3; ip4; }
    }
    allow {
        mask { security-group irccloud; }
        class clients;
        maxperip 128;
    }
    except ban {
        mask { security-group irccloud; }
        type { blacklist; connect-flood; handshake-data-flood; }
    }
    ```
* Because the mask item is so powerful now, the `password` in the
  [oper block](https://www.unrealircd.org/docs/Oper_block) is optional now.
* We now support oper::auto-login, which means the user will become IRCOp
  automatically if they match the conditions on-connect. This can be used
  in combination with
  [certificate fingerprint](https://www.unrealircd.org/docs/Certificate_fingerprint)
  authentication for example:
  ```
  security-group Syzop { certfp "1234etc."; }
  oper Syzop {
      auto-login yes;
      mask { security-group Syzop; }
      operclass netadmin-with-override;
      class opers;
  }
  except ban {
      mask { security-group Syzop; }
      type all;
  }
  ```
* For [JSON logging](https://www.unrealircd.org/docs/JSON_logging) a number
  of fields were added when a client is expanded:
  * `geoip`: with subitem `country_code` (eg. `NL`)
  * `tls`: with subitems `cipher` and `certfp`
  * Under subitem `users`:
    * `vhost`: if the visible host differs from the realhost then this is
      set (thus for both vhost and cloaked host)
    * `cloakedhost`: this is always set (except for eg. services users), even
      if the user is not cloaked so you can easily search on a cloaked host.
    * `idle_since`: last time the user has spoken (local clients only)
    * `channels`: list of channels (array), with a maximum of 384 chars.
* The JSON logging now also strips ASCII below 32, so color- and
  control codes.
* Support IRCv3 `+draft/channel-context`
* Add `example.es.conf` (Spanish example configuration file)
* The country of users is now communicated in the
  [message-tag](https://www.unrealircd.org/docs/Message_tags)
  `unrealircd.org/geoip` (only to IRCOps).
* Add support for linking servers via UNIX domain sockets
  (`link::outgoing::file`).

### Fixes:
* Crash in `except ban` with `~security-group:xyz`
* Crash if hideserver module was loaded but `LINKS` was not blocked.
* Crash on Windows when using the "Rehash" GUI option.
* Infinite loop if one security-group referred to another.
* Duplicate entries in the `+beI` lists of `+P` channels.
* Regular users were able to -o a service bot (that has umode +S)
* Module manager did not stop on compile error
* [`set::modes-on-join`](https://www.unrealircd.org/docs/Set_block#set::modes-on-join)
  did not work with `+f` + timed bans properly, eg `[3t#b1]:10`
* Several log messages were missing some information.
* Reputation syncing across servers had a small glitch. Fix is mostly
  useful for servers that were not linked to the network for days or weeks.
* In the config file, when not using quotes, a slash at the beginning of a
  variable name or value was silently discarded (eg `file /tmp/xyz;` resulted
  in a file `tmp/xyz`).

### Changes:
* Clarified that UnrealIRCd is licensed as "GPLv2 or later"
* Fix use of variables in
  [`set::reject-message`](https://www.unrealircd.org/docs/Set_block#set::reject-message)
  and in [`blacklist::reason`](https://www.unrealircd.org/docs/Blacklist_block):
  previously short forms of variables were (unintentionally) expanded
  as well, such as `$serv` for `$server`. This is no longer supported, you need
  to use the correct full variable names.

### Developers and protocol:
* The `creationtime` is now communicated of users. Until now this
  information was only known locally (the thing that was communicated
  that came close was "last nick change" but that is not the same).
  This is synced via (early) moddata across servers.
  Module coders can use `get_connected_time()`.
* The `RPL_HOSTHIDDEN` is now sent from `userhost_changed()` so you
  don't explicitly send it yourself anymore.
* The `SVSO` command is back, so services can make people IRCOp again.
  See `HELPOP SVSO` or [the commit](https://github.com/unrealircd/unrealircd/commit/50e5d91c798e7d07ca0c68d9fca302a6b6610786)
  for more information.
* Due to last change the `HOOKTYPE_LOCAL_OPER` parameters were changed.
* Module coders can enhance the
  [JSON logging](https://www.unrealircd.org/docs/JSON_logging)
  expansion items for clients and channels via new hooks like
  `HOOKTYPE_JSON_EXPAND_CLIENT`. This is used by the geoip and tls modules.

UnrealIRCd 6.0.3
-----------------
A number of serious issues were discovered in UnrealIRCd 6. Among these is
an issue which will likely crash the IRCd sooner or later if you /REHASH
with any active clients connected.
We suggest everyone who is running UnrealIRCd 6 to upgrade to 6.0.3.

Fixes:
* Crash in `WATCH` if the IRCd has been rehashed at least once. After doing
  a `REHASH` with active clients it will likely corrupt memory. It may take
  several days until after the rehash for the crash to occur, or even
  weeks/months on smaller networks (accidental triggering, that is).
* A `REHASH` with certain remote includes setups could cause a crash or
  other weird and confusing problems such as complaining about unable
  to open an ipv6-database or missing snomask configuration.
  This only affected some people with remote includes, not all.
* Potential out-of-bounds write in sending code. In practice it seems
  harmless on most servers but this cannot be 100% guaranteed.
* Unlikely triggered log message would log uninitialized stack data to the
  log file or send it to ircops.
* Channel ops could not remove halfops from a user (`-h`).
* After using the `RESTART` command (not recommended) the new IRCd was
  often no longer writing to log files.
* Fix compile problem if you choose to use cURL remote includes but don't
  have cURL on the system and ask UnrealIRCd to compile cURL.

Enhancements:
* The default text log format on disk changed. It now includes the server
  name where the event was generated. Without this, it was sometimes
  difficult to trace problems, since previously it sometimes looked like
  there was a problem on your server when it was actually another server
  on the network.
  * Old log format: `[DATE TIME] subsystem.EVENT_ID loglevel: ........`
  * New log format: `[DATE TIME] servername subsystem.EVENT_ID loglevel: ........`

Changes:
* Any MOTD lines added by services via
  [`SVSMOTD`](https://www.unrealircd.org/docs/MOTD_and_Rules#SVSMOTD)
  are now shown at the end of the MOTD-on-connect (unless using a shortmotd).
  Previously the lines were only shown if you manually ran the `MOTD` command.

Developers and protocol:
* `LIST C<xx` now means: filter on channels that are created less
  than `xx` minutes ago. This is the opposite of what we had earlier.
  `LIST T<xx` is now supported as well (topic changed in last xx minutes),
  it was already advertised in ELIST but support was not enabled previously.

UnrealIRCd 6.0.2
-----------------
UnrealIRCd 6.0.2 comes with several nice feature enhancements along with
some fixes. It also includes a fix for a crash bug that can be triggered
by ordinary users.

Fixes:
* Fix crash that can be triggered by regular users if you have any `deny dcc`
  blocks in the config or any spamfilters with the `d` (DCC) target.
  NOTE: You don't *have* to upgrade to 6.0.2 to fix this, you can also
  hot-patch this issue without restart, see the news announcement.
* Windows: fix crash with IPv6 clients (local or remote) due to GeoIP lookup
* Fix infinite hang on "Loading IRCd configuration" if DNS is not working.
  For example if the 1st DNS server in `/etc/resolv.conf` is down or refusing
  requests.
* Some `MODE` server-to-server commands were missing a timestamp at the end,
  even though this is mandatory for modes coming from a server.
* The [channeldb](https://www.unrealircd.org/docs/Set_block#set::channeldb)
  module now converts letter extbans to named extbans (eg `~a` to `~account`).
  Previously it did not, which caused letter extbans to appear in the banlist.
  Later on, when linking servers, this would cause duplicate entries to appear
  as well, with both the old and new format. The extbans were still effective
  though, so this is mostly a visual +b/+e/+I list issue.
* Some [Extended Server Bans](https://www.unrealircd.org/docs/Extended_server_bans)
  were not working correctly for WEBIRC proxies. In particular, a server ban
  or exempt (ELINE) on `~country:XX` was only checked against the WEBIRC proxy.

Enhancements:
* Support for [logging to a channel](https://www.unrealircd.org/docs/Log_block#Logging_to_a_channel).
  Similar to snomasks but then for channels.
* Command line interface changes:
  * The [CLI tool](https://www.unrealircd.org/docs/Command_Line_Interface) now
    communicates to the running UnrealIRCd process via a UNIX socket to
    send commands and retrieve output.
  * The command `./unrealircd rehash` will now show the rehash output,
    including warnings and errors, and return a proper exit code.
  * The same for `./unrealircd reloadtls`
  * New command `./unrealircd status` to show if UnrealIRCd is running, the
    version, channel and user count, ..
  * The command `./unrealircd genlinkblock` is now
    [documented](https://www.unrealircd.org/docs/Linking_servers_(genlinkblock))
    and is referred to from the
    [Linking servers tutorial](https://www.unrealircd.org/docs/Tutorial:_Linking_servers).
  * On Windows in the `C:\Program Files\UnrealIRCd 6\bin` directory there is
    now an `unrealircdctl.exe` that can be used to do similar things to what
    you can do on *NIX. Supported operations are: `rehash`, `reloadtls`,
    `mkpasswd`, `gencloak` and `spkifp`.
* New option [set::server-notice-show-event](https://www.unrealircd.org/docs/Set_block#set::server-notice-show-event)
  which can be set to `no` to hide the event information (eg `connect.LOCAL_CLIENT_CONNECT`)
  in server notices. This can be overridden per-oper in the
  [Oper block](https://www.unrealircd.org/docs/Oper_block) via oper::server-notice-show-event.
* Support for IRC over UNIX sockets (on the same machine), if you specify a
  file in the [listen block](https://www.unrealircd.org/docs/Listen_block)
  instead of an ip/port. This probably won't be used much, but the option is
  there. Users will show up with a host of `localhost` and IP `127.0.0.1` to
  keep things simple.
* The `MAP` command now shows percentages of users
* Add `WHO` option to search clients by time connected (eg. `WHO <300 t` to
  search for less than 300 seconds)
* Rate limiting of `MODE nick -x` and `-t` via new `vhost-flood` option in
  [set::anti-flood block](https://www.unrealircd.org/docs/Anti-flood_settings).

Changes:
* Update Russian `help.ru.conf`.

Developers and protocol:
* People packaging UnrealIRCd (eg. to an .rpm/.deb):
  * Be sure to pass the new `--with-controlfile` configure option
  * There is now an `unrealircdctl` tool that the `unrealircd` shell script
    uses, it is expected to be in `bindir`.
* `SVSMODE #chan -b nick` will now correctly remove extbans that prevent `nick`
  from joining. This fixes a bug where it would remove too much (for `~time`)
  or not remove extbans (most other extbans, eg `~account`).
  `SVSMODE #chan -b` has also been fixed accordingly (remove all bans
  preventing joins).
  Note that all these commands do not remove bans that do not affect joins,
  such as `~quiet` or `~text`.
* For module coders: setting the `EXTBOPT_CHSVSMODE` flag in `extban.options`
  is no longer useful, the flag is ignored. We now decide based on
  `BANCHK_JOIN` being in `extban.is_banned_events` if the ban should be
  removed or not upon SVS(2)MODE -b.

UnrealIRCd 6.0.1.1
-------------------

Fixes:
* In 6.0.1.1: extended bans were not properly synced between U5 and U6.
  This caused missing extended bans on the U5 side (MODE was working OK,
  this only happened when linking servers)
* Text extbans did not have any effect (`+b ~text:censor:*badword*`)
* Timed bans were not expiring if all servers on the network were on U6
* Channel mode `+f` could place a timed extban with `~t` instead of `~time`
* Crash when unloading any of the vhoaq modules at runtime
* `./unrealircd upgrade` not working on FreeBSD and not with self-compiled cURL
* Some log messages being wrong (`CHGIDENT`, `CHGNAME`)
* Remove confusing high cpu load warning

Enhancements:
* Error on unknown snomask in set::snomask-on-oper and oper::snomask.
* TKL add/remove/expire messages now show `[duration: 60m]` instead of
  the `[expires: ZZZ GMT]` string since that is what people are more
  interested in and is not affected by time zones. The format in all the
  3 notices is also consistent now.

UnrealIRCd 6.0.0
-----------------

Many thanks to k4be for his help during development, other contributors for
their feedback and patches, the people who tested the beta's and release
candidates, translators and everyone else who made this release happen!

Summary
--------
UnrealIRCd 6 comes with a completely redone logging system (with optional
JSON support), named extended bans, four new IRCv3 features,
geoip support and remote includes support built-in.

Additionally, things are more customizable such as what gets sent to
which snomask. All the +vhoaq channel modes are now modular as well,
handy for admins who don't want or need halfops or +q/+a.
For WHOIS it is now customizable in detail who gets to see what.

A summary of the features is available at
[What's new in UnrealIRCd 6](https://www.unrealircd.org/docs/What's_new_in_UnrealIRCd_6).
For complete information, continue reading the release notes below.
The sections below contain all the details.

Upgrading from UnrealIRCd 5
----------------------------
The previous stable series, UnrealIRCd 5, will no longer get any new features.
We still do bug fixes until July 1, 2022. In the 12 months after that, only
security issues will be fixed. Finally, after July 1, 2023,
[all support will stop](https://www.unrealircd.org/docs/UnrealIRCd_5_EOL).

If you want to hold off for a while because you are cautious or if you
depend on 3rd party modules (which may not have been upgraded yet by their
authors) then feel free to wait a while.

If you are upgrading from UnrealIRCd 5 to 6 then you can use your existing
configuration and files. There's no need to start from scratch.
However, you will need to make a few updates, see
[Upgrading from 5.x to 6.x](https://www.unrealircd.org/docs/Upgrading_from_5.x).

Enhancements
-------------
* Completely new log system and snomasks overhaul
  * Both logging and snomask sending is done by a single logging function
  * Support for [JSON logging](https://www.unrealircd.org/docs/JSON_logging)
    to disk, instead of the default text format.
    JSON logging adds lot of detail to log messages and consistently
    expands things like *client* with properties like *hostname*,
    *connected_since*, *reputation*, *modes*, etc.
  * The JSON data is also sent to all IRCOps who request the
    `unrealircd.org/json-log` capability. The data is then sent in
    a message-tag called `unrealircd.org/json-log`. This makes it ideal
    for client scripts and bots to do automated things.
  * A new style log { } block is used to map what log messages should be
    logged to disk, and which ones should be sent to snomasks.
  * The default logging to snomask configuration is in `snomasks.default.conf`
    which everyone should include from unrealircd.conf. That is, unless you
    wish to completely reconfigure which logging goes to which snomasks
    yourself, which is also an option now.
  * See [Snomasks](https://www.unrealircd.org/docs/Snomasks#UnrealIRCd_6)
    on the new snomasks - lots of letters changed!
  * See [FAQ: Converting log { } block](https://www.unrealircd.org/docs/FAQ#old-log-block)
    on how to change your existing log { } blocks for disk logging.
  * We now have a consistent log format and log messages can be multiline.
  * Colors are enabled by default in snomask server notices, these can be disabled via
    [set::server-notice-colors](https://www.unrealircd.org/docs/Set_block#set::server-notice-colors)
    and also in [oper::server-notice-colors](https://www.unrealircd.org/docs/Oper_block)
  * Support for [logging to a channel](https://www.unrealircd.org/docs/Log_block#Logging_to_a_channel).
    Similar to snomasks but then for channels. *Requires UnrealIRCd 6.0.2 or later*
* Almost all channel modes are modularized
  * Only the three list modes (+b/+e/+I) are still in the core
  * The five [level modes](https://www.unrealircd.org/docs/Channel_Modes#Access_levels)
    (+vhoaq) are now also modular. They are all loaded by default but you can
    blacklist one or more if you don't want them. For example to disable halfop:
    `blacklist-module chanmodes/halfop;`
  * Support for compiling without PREFIX_AQ has been removed because
    people often confused it with disabling +a/+q which is something
    different.
* Named extended bans
  * Extbans now no longer show up with single letters but with names.
    For example `+b ~c:#channel` is now `+b ~channel:#channel`.
  * Extbans are automatically converted from the old to the new style,
    both from clients and from/to older UnrealIRCd 5 servers.
    The auto-conversion also works fine with complex extbans such as
    `+b ~t:5:~q:nick!user@host` to `+b ~time:5:~quiet:nick!user@host`.
* New IRCv3 features:
  * [MONITOR](https://ircv3.net/specs/extensions/monitor.html): an
    alternative for `WATCH` to monitor other users ("notify list").
  * draft/extended-monitor: extensions for MONITOR, still in draft.
  * [invite-notify](https://ircv3.net/specs/extensions/invite-notify):
    report channel invites to other chanops (or users) in a machine
    readable way.
  * [setname](https://ircv3.net/specs/extensions/setname.html):
    notify clients about realname (gecos) changes.
* GeoIP lookups are now done by default
  * This shows the country of the user to IRCOps in `WHOIS` and in the
    "user connecting" line.
  * By default the `geoip_classic` module is loaded, for which we
    provide a mirror of database updates at unrealircd.org. This uses
    the classic geolite library that is now shipped with UnrealIRCd
  * Other options are the `geoip_maxmind` and `geoip_csv` modules.
* Configure `WHOIS` output in a very precise way
  * You can now decide which fields (eg modes, geo, certfp, etc) you want
    to expose to who (everyone, self, oper).
  * See [set::whois-details](https://www.unrealircd.org/docs/Set_block#set::whois-details)
    for more details.
* We now ship with 3 cloaking modules and you need to load 1 explicitly
  via `loadmodule`:
  * `cloak_sha256`: the recommended module for anyone starting a *new*
    network. It uses the SHA256 algorithm under the hood.
  * `cloak_md5`: for anyone who is upgrading their network from older
    UnrealIRCd versions. Use this so your cloaked host bans remain the same.
  * `cloak_none`: if you don't want any cloaking, not even as an option
    to your users (rare)
* Remote includes are now supported everywhere in the config file.
  * Support for `https://` fetching is now always available, even
    if you don't compile with libcurl support.
  * Anywhere an URL is encountered on its own, it will be fetched
    automatically. This makes it work not only for includes and motd
    (which was already supported) but also for any other file.
  * To prevent something from being interpreted as a remote include
    URL you can use 'value' instead of "value".
* Invite notification: set `set::normal-user-invite-notification yes;` to make
  chanops receive information about normal users inviting someone to their channel.
  The name of this setting may change in a later version.
* Websocket: you can add a `listen::options::websocket::forward 1.2.3.4` option
  to make unrealircd accept a `Forwarded` (RFC 7239) header from a reverse proxy
  connecting from `1.2.3.4` (plans to accept legacy `X-Forwarded-For` and a proxy
  password too). This feature is currently experimental.

Changes
--------
* TLS cipher and some other information is now visible for remote
  clients as well, also in `[secure: xyz]` connect line.
* Error messages in remote includes use the url instead of a temporary file
* Downgrading from UnrealIRCd 6 is only supported down to 5.2.0 (so not
  lower like 5.0.x). If this is a problem then make a copy of your db files
  (eg: reputation.db).

Removed
--------
* /REHASH -motd and -opermotd are gone, just use /REHASH

Breaking changes
-----------------
See https://www.unrealircd.org/docs/Upgrading_from_5.x, but in short:

You can use the unrealircd.conf from UnrealIRCd 5, but you need to make
a few changes:
* You need to add `include "snomasks.default.conf";`
* You need to load a cloaking module explicitly. Assuming you already
  have a network then add: `loadmodule "cloak_md5";`
* The log block(s) need to be updated, use something like:
  ```
  log {
          source {
              !debug;
              all;
          }
          destination {
              file "ircd.log" { maxsize 100M; }
          }
  }
  ```

Module coders (API changes)
----------------------------
* Be sure to bump the version in the module header from `unrealircd-5` to `unrealircd-6`
* We use a lot more `const char *` now (instead of `char *`). In particular `parv`
  is const now and so are a lot of arguments to hooks. This will mean that in your
  module you have to use more const too. The reason for this change is to indicate
  that certain strings should not be touched, as doing so is dangerous or could
  have had side-effects that were unpredictable.
* Logging has been completely redone. Don't use `ircd_log()`, `sendto_snomask()`,
  `sendto_ops()` and `sendto_realops()` anymore. Instead use `unreal_log()` which
  handles both logging to disk and notifying IRCOps.
* Various struct member names changed, in particular in `ConfigEntry` and `ConfigFile`,
  but also `channel->chname` is `channel->name` now.
* get_channel() is now make_channel() and creates if needed, otherwise use find_channel()
* The Extended Ban API has been changed a lot. We use a `BanContext` struct now
  that we pass around a lot. You also don't need to do `+3` magic anymore on the
  string as it is handled in another layer. When registering the extended ban,
  `.flag` is now `.letter`, and you also need to set a `.name` to a string due
  to named extended bans. Have a look at the built-in extban modules to see
  how to handle the changes.
* ModData now has an option `MODDATA_SYNC_EARLY`. See under *Server protocol*.
* If you want to lag someone up, don't touch `client->since`, but instead use:
  `add_fake_lag(client, msec)`
* Some client/user struct changes, with `client->user->account` (instead of svid)
  and `client->uplink->name` being the most important ones.
* Possibly more, but above is like 90%+ of the changes that you will encounter.

Server protocol
----------------
* When multiple related `SJOIN` messages are generated for the same channel
  then we now only send the current channel modes (eg `+sntk key`) in the
  first SJOIN and not in the other ones as they are unneeded for the
  immediate followup SJOINs, they waste unnecessary bytes and CPU.
  Such messages may be generated when syncing a channel that has dozens
  of users and/or bans/exempts/invexes. Ideally this should not need any
  changes in other software, since we already supported such messages in the
  past and code for handling it exists way back to 3.2.x, but you better
  check to be sure!
* If you send `PROTOCTL NEXTBANS` then you will receive extended bans
  with Named EXTended BANs instead of letters (eg: `+b ~account:xyz`),
  otherwise you receive them with letters (eg: `+b ~a:xyz`).
* Some ModData of users is (also) communicated in the `UID` message while
  syncing using a message tag that only appears in server-to-server traffic,
  `s2s-md/moddataname=value`. Thus, data such as operinfo, tls cipher,
  geoip, certfp, sasl and webirc is communicated at the same time as when
  a remote connection is added.
  This makes it that a "connecting from" server notice can include all this
  information and also so code can make an immediate decission on what to do
  with the user in hooks. ModData modules need to set
  `mreq.sync = MODDATA_SYNC_EARLY;` if they want this.
  Servers of course need to enable `MTAGS` in PROTOCTL to see this.
* The `SLOG` command is used to broadcast logging messages. This is done
  for log::destination remote, as used in doc/conf/snomasks.default.conf,
  for example for link errors, oper ups, flood messages, etc.
  It also includes all JSON data in a message tag when `PROTOCTL MTAGS` is used.
* Bounced modes are gone: these were MODEs that started with a `&` which
  servers were to act on with reversed logic (add becoming remove and
  vice versa) and never to send something back to that server.
  In practice this was almost never used and complicated the code (way)
  too much.

Client protocol
----------------
* Extended bans now have names instead of letters. If a client sends the
  old format with letters (eg `+b ~a:XYZ`) then the server will
  convert it to the new format with names (eg: `+b ~account:XYZ`)
* Support for `MONITOR` and the other IRCv3 features (see *Enhancements*)
