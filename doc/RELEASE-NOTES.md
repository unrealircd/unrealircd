UnrealIRCd 6
=============
This is UnrealIRCd 6's latest git, bleeding edge. Do not use on production servers.

* Newlog:
  * Explain basic idea
  * New log format on disk
  * Messages can be multiline (+)
  * New log { } block format (TODO: auto-convert old style?)
  * JSON logging (optional)
  * Snomasks work different now
  * Colored logs, explain where/when used and how to turn off
  * CAP unrealircd.org/json-log
* Named extbans
* Geo ip: tell how it works and where it is available/used/shown
  main configuration:
```
  set { geoip {
    check-on-load yes; // check all users already connected and add geoip info to them on module load
    whois-for-anyone no; // display country info in whois: yes for all users, no for opers only
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
* Remote includes support:
  * Use an URL anywhere that you use a file
  * Support for https:// is always available, even without curl
* IRCv3: MONITOR, invite-notify, setname, draft/metadata, draft/extended-monitor
* Lots of code cleanups / API breakage
* We now (try to) kill the "old" server when a server links in with the same
  name, handy when the old server is a zombie waiting for ping timeout.
* Error messages in remote includes use the url instead of temp file
* Something with cloaking
* Downgrading is only supported down to 5.2.0, not lower, otherwise
  make a copy of your reputation db etc.
* Antirandom no longer has fullstatus-on-load: maybe warn and ignore
  the option rather than failing? Was this in the default conf?
* /REHASH -motd and -opermotd are gone, just use /REHASH
* Invite: set `set::normal-user-invite-notification yes;` to make chanops
  receive information about normal users inviting someone to their channel.
  (Not completely sure about the setting name)
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
