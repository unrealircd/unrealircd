[![Mastodon Follow](https://img.shields.io/mastodon/follow/110769722108208212?domain=https%3A%2F%2Ffosstodon.org&style=social&label=Follow)](https://fosstodon.org/@unrealircd)
[![Twitter Follow](https://img.shields.io/twitter/follow/Unreal_IRCd.svg?style=social&label=Follow)](https://twitter.com/Unreal_IRCd)
[![Linux CI](https://github.com/unrealircd/unrealircd/actions/workflows/linux-ci.yml/badge.svg)](https://github.com/unrealircd/unrealircd/actions/workflows/linux-ci.yml)

## About ObbyIRCd
ObbyIRCd is a fork of [UnrealIRCd](https://www.unrealircd.org/) — an Open Source IRC Server,
serving thousands of networks since 1999. It runs on Linux and OS X.
ObbyIRCd is built on UnrealIRCd's highly advanced, modular, and secure foundation with a strong
focus on modularity and security. It uses an advanced and highly configurable configuration file.
Key features include: full IRCv3 support, SSL/TLS, cloaking, JSON-RPC, advanced anti-flood and
anti-spam systems, GeoIP, remote includes, and lots of
[other features](https://www.unrealircd.org/docs/About_UnrealIRCd).

## Versions
* ObbyIRCd is based on UnrealIRCd 6, the *stable* series since December 2021.
* Upstream UnrealIRCd release information:
  [UnrealIRCd releases](https://www.unrealircd.org/docs/UnrealIRCd_releases) on the wiki

## How to get started
Follow the installation guide on the wiki. See:
* [Installing from source for *NIX](https://www.unrealircd.org/docs/Installing_from_source)
* [Installation instructions for Windows](https://www.unrealircd.org/docs/Installing_(Windows))

## Documentation and Support
You can find all **documentation** online at: https://www.unrealircd.org/docs/

We also have a good **FAQ**: https://www.unrealircd.org/docs/FAQ

If you are in need of support, you can pop up on [**#unreal-support** on `irc.unrealircd.org`](ircs://irc.unrealircd.org:6697/unreal-support)
or ask your question on the [forums](https://forums.unrealircd.org).

## Supported systems
We try to **support** all major *NIX systems: all Linux distros but also NetBSD, OpenBSD and macOS,
provided the OS version was released within the past ~5 years.

We use a private BuildBot instance to test each commit. The **tested** systems are (others are
likely to work too):
* Linux: Debian (10, 11, 12, 13), Ubuntu (18.04, 20.04, 22.04, 24.04, 26.04)
* FreeBSD: 15
* Windows: Visual Studio 2019

UnrealIRCd is architecture-agnostic. Most of the BuildBot workers run on x64 but we
also have some on x86 and arm64 to ensure these work as well.

## Other links ##
* https://www.unrealircd.org - Main website
* https://bugs.unrealircd.org - Bug tracker
* https://fosstodon.org/@unrealircd - Mastodon
* https://twitter.com/Unreal_IRCd - Twitter
