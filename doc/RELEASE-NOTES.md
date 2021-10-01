UnrealIRCd 6
=============
This is UnrealIRCd 6's latest git, bleeding edge. Do not use on production servers!

Summary
--------
TODO

Enhancements
-------------
* Completely new log system and snomasks overhaul
  * Both logging and snomask sending is done by a single logging function
  * A new style log { } block is used to map what log messages should be
    logged to disk, and which ones should be sent to snomasks.
  * The default logging to snomask configuration is in ```snomasks.default.conf```
    which everyone should include from unrealircd.conf. That is, unless you
    wish to completely reconfigure which logging goes to which snomasks
    yourself, which is also an option now.
  * See ... on the new snomasks - lots of letters changed!
  * See ... on how to change your existing log { } blocks for disk logging
  * New support for JSON logging to disk, instead of the default text format.
    JSON logging adds lot of detail to log messages and consistently
    expands things like 'client' with properties like hostname,
    connected_since, reputation, modes, etc.
  * The JSON data is also sent to all IRCOps who request the
    ```unrealircd.org/json-log``` capability. The data is then sent in
    a message-tag called ```unrealircd.org/json-log```. This makes it ideal
    for client scripts and bots to do automated things.
  * TODO: Explain log format, and multiline
  * TODO: Explain colored logs, where/when and how to turn off
* GeoIP lookups are now done by default
  * This shows the country of the user to IRCOps in ```WHOIS``` and in the
    "user connecting" line.
  * By default the ```geoip_classic``` module is loaded, for which we
    provide a mirror of database updates at unrealircd.org. This uses
    the classic geolite library that is now shipped with UnrealIRCd
  * Other options are the ```geoip_maxmind``` and ```geoip_csv``` modules.
* Named extbans
  * Extbans now no longer show up with single letters but with names.
    For example ```+b ~c:#channel``` is now ```+b ~channel:#channel```.
  * Extbans are automatically converted from the old to the new style,
    both from clients and from/to older UnrealIRCd 5 servers.
    The auto-conversion also works fine with complex extbans such as
    ```+b ~t:5:~q:nick!user@host``` to ```+b ~time:5:~quiet:nick!user@host```.
* Configure ```WHOIS``` output in a very precise way
  * You can now decide which fields (eg modes, geo, certfp, etc) you want
    to expose to who (everyone, self, oper).
  * See ... for the default configuration and more details
* We now ship with 3 cloaking modules and you need to load 1 explicitly
  via ```loadmodule```:
  * ```cloak_sha256```: the recommended module for anyone starting a new
    network. It uses the SHA256 algorithm under the hood.
  * ```cloak_md5```: for anyone who is upgrading their network from older
    UnrealIRCd versions. Use this so your cloaked host bans don't break.
  * ```cloak_none```: if you don't want any cloaking, not even as an option
    to your users (rare)
* Remote includes are now supported everywhere in the config file.
  * Support for ```https://``` fetching is now always available, even
    if you don't compile with libcurl support.
  * Anywhere an URL is encountered on its own, it will be fetched
    automatically. This makes it work not only for includes and motd
    (which is already supported) but also for any other file.
  * If you need to prevent UnrealIRCd from treating an URL as a
    remote include then use ..... TODO......
* New IRCv3 features:
  * [MONITOR](https://ircv3.net/specs/extensions/monitor.html)
  * [invite-notify](https://ircv3.net/specs/extensions/invite-notify)
  * [setname](https://ircv3.net/specs/extensions/setname.html)
  * draft/metadata --- ONLY if we decide to ship with this
  * draft/extended-monitor ?
* Almost all channel modes are modularized
  * Only the three list modes (+b/+e/+I) are still in the core
  * The five rank modes +vhoaq are now also modular. They are loaded by
    default but you can blacklist one or more if you don't want them.
  * For example to disable halfop: ```blacklist-module chanmodes/halfop;```
  * Support for compiling without PREFIX_AQ has been removed because
    people often confused it with disabling +a/+q which is something
    different.
  * TODO: replace part of the above with a simple docs reference, eg
    a new page called Channel ranks or whatever, or part of the
    Channel modes page.

Mental notes / move these wiki:

* Geo ip: tell how it works and where it is available/used/shown
  main configuration:
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
  None of these modules are loaded by default
* TLS cipher and some other information is now visible for remote
  clients as well, also in [secure: xyz] connect line.
* Lots of code cleanups / API breakage
* We now (try to) kill the "old" server when a server links in with the same
  name, handy when the old server is a zombie waiting for ping timeout.
  FIXME: isnt this broken?
* Error messages in remote includes use the url instead of temp file
* Downgrading is only supported down to 5.2.0, not lower, otherwise
  make a copy of your reputation db etc.
* Antirandom no longer has fullstatus-on-load: maybe warn and ignore
  the option rather than failing? Was this in the default conf?
* /REHASH -motd and -opermotd are gone, just use /REHASH
* Invite: set `set::normal-user-invite-notification yes;` to make chanops
  receive information about normal users inviting someone to their channel.
  (TODO: Not completely sure about the setting name)
* Websocket: you can add a `listen::options::websocket::forward 1.2.3.4` option
  to make unrealircd accept a `Forwarded` (RFC 7239) header from a reverse proxy
  connecting from `1.2.3.4` (plans to accept legacy `X-Forwarded-For` and a proxy
  password too)

API:
* Bump from unrealircd-5 to unrealircd-6
* Where do I start...
* Newlog
* ConfigEntry, ConfigFile (c22207c4ca2e6a72024ff9c642863737e2519d33)
* get_channel() is now make_channel() and creates if needed, otherwise use find_channel()
* Extban api breakage
* Message tag api breakage
* ModData MODDATA_SYNC_EARLY
* For adjusting fake lag use add_fake_lag(client, msec)
* Some client/user struct changes: eg client->uplink->name, check log for all..

Protocol:
* SJOIN followups
* NEXTBANS
* Bounced modes are gone
* SLOG

FIXME: (wrong) server delinking in case of error may be an issue
