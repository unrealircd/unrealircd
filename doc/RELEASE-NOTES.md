UnrealIRCd 6.0.0-beta1
=======================
This is the first beta for UnrealIRCd 6. It contains all the planned
features for U6 and now we need the help of the public to test these beta's.
Caution: this beta may crash spectacularly, behave weird or in unexpected
ways, so don't run it on production systems!
If you find any issues, please report them at https://bugs.unrealircd.org/.
This way, you help us getting ready for a real stable UnrealIRCd 6 release.

Note that [AddressSanitizer](https://en.wikipedia.org/wiki/AddressSanitizer)
is enabled in these builds, which will cause UnrealIRCd to use a lot more
memory and run more slowly than normal. AddressSanitizer helps us greatly
to catch more bugs during development. However, if this is a problem for
you, then answer --disable-asan to the last question in ./Config about
custom parameters to pass to configure.
Naturally, the eventual stable release won't use AddressSanitizer.

Summary
--------
UnrealIRCd 6 comes with a completely redone logging system (with optional
JSON support), named extended bans, four new IRCv3 features,
geoip support and remote includes support built-in.

Additionally, things are more customizable such as what gets sent to
which snomask. All the +vhoaq channel modes are now modular as well,
handy for admins who don't want or need halfops or +q/+a.
For WHOIS it is now customizable in detail who gets to see what.

Breaking changes
-----------------
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

Enhancements
-------------
* Completely new log system and snomasks overhaul
  * Both logging and snomask sending is done by a single logging function
  * New support for [JSON logging](https://www.unrealircd.org/docs/JSON_logging)
    to disk, instead of the default text format.
    JSON logging adds lot of detail to log messages and consistently
    expands things like 'client' with properties like hostname,
    connected_since, reputation, modes, etc.
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
  * Colors are enabled in snomasks and console logs by default. Later there
    will be an option to turn this off (TODO).
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
  * On TODO list: an option to prevent UnrealIRCd from treating an URL as a
    remote include.
* Invite notification: set `set::normal-user-invite-notification yes;` to make
  chanops receive information about normal users inviting someone to their channel.
  (TODO: Not completely sure about the setting name)
* Websocket: you can add a `listen::options::websocket::forward 1.2.3.4` option
  to make unrealircd accept a `Forwarded` (RFC 7239) header from a reverse proxy
  connecting from `1.2.3.4` (plans to accept legacy `X-Forwarded-For` and a proxy
  password too)

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

Module coders (API changes)
----------------------------

* This section is incomplete and has little details. It will be expanded later.
* Bump module header from unrealircd-5 to unrealircd-6
* Newlog
* ConfigEntry, ConfigFile (c22207c4ca2e6a72024ff9c642863737e2519d33)
* get_channel() is now make_channel() and creates if needed, otherwise use find_channel()
* Extban api breakage
* Message tag api breakage
* ModData MODDATA_SYNC_EARLY
* For adjusting fake lag use add_fake_lag(client, msec)
* Some client/user struct changes: eg client->uplink->name, check log for all..

Server protocol
----------------
* If multiple related `SJOIN` messages are generated for the same channel
  then we now only send the current channel modes (eg ```+sntk key```) in the
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
* Extended bans now have names instead of letters
* TODO: document other stuff?

Mental notes / move these wiki
-------------------------------
These notes are mostly for ourselves:

* Geo ip main configuration (config items may still change!!):
```
  set { geoip {
    check-on-load yes; // check all users already connected and add geoip info to them on module load
  };};
```
  geoip_csv module: always compiled, file locations:
```
  set { geoip-csv {
    ipv4-blocks-file "GeoLite2-Country-Blocks-IPv4.csv"; // don't set for ipv6-only
    ipv6-blocks-file "GeoLite2-Country-Blocks-IPv6.csv"; // don't set for ipv4-only
    countries-file "GeoLite2-Country-Locations-en.csv"; // required
  };};
```
  geoip_maxmind module: compiled when system libmaxminddb is present, file location:
```
  set { geoip-maxmind {
    database "GeoLite2-Country.mmdb";
  };};
```
  geoip_classic module: compiled when `--enable-geoip-classic=yes` added to configure, file locations:
```
  set { geoip-classic {
    ipv4-database "GeoIP.dat"; // don't set for ipv6-only
    ipv6-database "GeoIPv6.dat"; // don't set for ipv4-only
  };};
```
* We now (try to) kill the "old" server when a server links in with the same
  name, handy when the old server is a zombie waiting for ping timeout.
  FIXME: isnt this broken?
FIXME: (wrong) server delinking in case of error may be an issue
* Antirandom no longer has fullstatus-on-load: maybe warn and ignore
  the option rather than failing? Was this in the default conf?
