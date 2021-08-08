UnrealIRCd 6
=============
This is UnrealIRCd 6's latest git, bleeding edge. Do not use on production servers.

* Logs & snomasks overhaul
* JSON logging: both on disk and to ircops
* MONITOR
* Lots of code cleanups / API breakage
* We now (try to) kill the "old" server when a server links in with the same
  name, handy when the old server is a zombie waiting for ping timeout.
