UnrealIRCd 6.1.2-git
=================
This is the git version (development version) for future 6.1.2. This is work
in progress and may not be a stable version.

### Enhancements:
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
  * You can turn it off via: `set { spamfilter { utf8 yes; } }`
* The module [antimixedutf8](https://www.unrealircd.org/docs/Set_block#set::antimixedutf8)
  now recognizes more code block transitions and should do a better job at
  catching mixed UTF8 spam.

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
