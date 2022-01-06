UnrealIRCd 6.0.2-git
=================
*THIS IS WORK IN PROGRESS*

You are looking at the git version of UnrealIRCd, bleeding edge.

If you are already running UnrealIRCd 6 then read below on the new
features in 6.0.2. Otherwise, jump straight to the
[summary about UnrealIRCd 6](#Summary) to learn more about UnrealIRCd 6.

Fixes:
* Windows: fix crash with IPv6 clients (local or remote) due to GeoIP lookup
* Fix infinite hang on "Loading IRCd configuration" if DNS is not working.
  For example if the 1st DNS server in `/etc/resolv.conf` is down or refusing
  requests.
* Some `MODE` server-to-server commands were missing a timestamp at the end,
  even though this is mandatory for modes coming from a server.

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
  listen::file instead of ip/port. This probably won't be used much, but
  the option is there. Users will show up with a host of `localhost`
  and IP `127.0.0.1` to keep things simple.
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
