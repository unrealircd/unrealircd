## About UnrealIRCd
UnrealIRCd is an Open Source IRC Server, serving thousands of networks since 1999. 
It runs on Linux, OS X and Windows and is currently the most widely deployed IRCd
with a market share of over 50%. UnrealIRCd is a highly advanced IRCd with a strong
focus on modularity, an advanced and highly configurable configuration file.
Key features include SSL, cloaking, its advanced anti-flood and anti-spam systems,
swear filtering and module support. We are also particularly proud on our extensive
online documentation. 

## How to get started
Please consult our excellent online documentation at https://www.unrealircd.org/docs/
when setting up the IRCd!

### Step 1: Installation
#### Windows
Simply download the UnrealIRCd Windows version from www.unrealircd.org

Alternatively you can compile UnrealIRCd for Windows yourself. However this is not straightforward and thus not recommended.

#### *BSD/Linux/OS X
First you must compile the IRCd:

* Run `./Config`
* Run `make`
* Run `make install`
* Now change to the directory where you installed UnrealIRCd, e.g. `cd /home/xxxx/unrealircd`

### Step 2: Configuration
Configuration files are stored in the conf/ folder by default (eg: /home/xxxx/unrealircd/conf)

#### Create a configuration file
If you are new, then you need to create your own configuration file:
Copy conf/examples/example.conf to conf/ and call it unrealircd.conf.
Then open it in an editor and carefully modify it using the documentation and FAQ as a guide (see below).

### Step 3: Booting

#### Linux/*BSD/OS X
Run `./unrealircd start` in the directory where you installed UnrealIRCd.

#### Windows
Start -> All Programs -> UnrealIRCd -> UnrealIRCd

## Documentation & FAQ
You can find the **documentation** online at: http://www.unrealircd.org/docs/

We also have a good **FAQ**: http://www.unrealircd.org/docs/FAQ

## Website, support, and other links ##
* https://www.unrealircd.org - Our main website
* https://forums.unrealircd.org - Support
* https://bugs.unrealircd.org - Bug tracker
