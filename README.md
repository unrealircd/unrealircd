[![Twitter Follow](https://img.shields.io/twitter/follow/Unreal_IRCd.svg?style=social&label=Follow)](https://twitter.com/Unreal_IRCd)

## About UnrealIRCd
UnrealIRCd is an Open Source IRC Server, serving thousands of networks since 1999. 
It runs on Linux, OS X and Windows and is currently the most widely deployed IRCd
with a market share of 42%. UnrealIRCd is a highly advanced IRCd with a strong
focus on modularity, an advanced and highly configurable configuration file.
Key features include SSL/TLS, cloaking, its advanced anti-flood and anti-spam systems,
swear filtering and module support. We are also particularly proud on our extensive
online documentation. 

## Versions
* UnrealIRCd 6 is the *stable* series since December 2021. All new features go in there.
* UnrealIRCd 5 is the *oldstable* series. It will receive bug fixes until
  July 1, 2022 plus another 12 months of security fixes.
* For full details of release scheduling and EOL dates, see
  [UnrealIRCd releases](https://www.unrealircd.org/docs/UnrealIRCd_releases) on the wiki

## How to get started
### Use the wiki!
**IMPORTANT:** We recommend you follow our installation guide on the wiki instead of the
steps in this README. The wiki has more detailed information and is more easy to navigate.
* [Installing from source for *NIX](https://www.unrealircd.org/docs/Installing_from_source)
* [Installating instructions for Windows](https://www.unrealircd.org/docs/Installing_(Windows))

Please consult the online documentation at https://www.unrealircd.org/docs/ when setting up the IRCd!

### Step 1: Installation
#### Windows
Simply download the UnrealIRCd Windows version from www.unrealircd.org

Alternatively you can compile UnrealIRCd for Windows yourself. However this is not straightforward and thus not recommended.

#### *BSD/Linux/macOS
Do the following steps under a separate account for running UnrealIRCd,
[do NOT compile or run as root](https://www.unrealircd.org/docs/Do_not_run_as_root).

### Step 1: Compile the IRCd

* Run `./Config`
* Run `make`
* Run `make install`
* Now change to the directory where you installed UnrealIRCd, e.g. `cd /home/xxxx/unrealircd`

### Step 2: Configuration
Configuration files are stored in the `conf/` folder by default (eg: `/home/xxxx/unrealircd/conf`)

#### Create a configuration file
If you are new, then you need to create your own configuration file:
Copy `conf/examples/example.conf` to `conf/` and call it `unrealircd.conf`.
Then open it in an editor and carefully modify it using the documentation and FAQ as a guide (see below).

### Step 3: Booting

#### Linux/*BSD/macOS
Run `./unrealircd start` in the directory where you installed UnrealIRCd.

#### Windows
Start -> All Programs -> UnrealIRCd -> UnrealIRCd

## Documentation & FAQ
You can find the **documentation** online at: https://www.unrealircd.org/docs/

We also have a good **FAQ**: https://www.unrealircd.org/docs/FAQ

## Website, support, and other links ##
* https://www.unrealircd.org - Our main website
* https://forums.unrealircd.org - Support
* https://bugs.unrealircd.org - Bug tracker
* ircs://irc.unrealircd.org:6697/unreal-support - IRC support
