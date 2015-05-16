## About UnrealIRCd
UnrealIRCd is an Open Source IRC Server, serving thousands of networks since 1999. 
It runs on Linux, OS X and Windows and is currently the most widely deployed IRCd
with a market share of over 50%. UnrealIRCd is a highly advanced IRCd with a strong
focus on modularity, an advanced and highly configurable configuration file.
Key features include SSL, cloaking, its advanced anti-flood and anti-spam systems,
swear filtering and module support. We are also particularly proud on our extensive
online documentation. 

## Pick the right branch!
The **3.2.x (stable)** series have proven to be very reliable over the past 10(!) years.
If you want to run a stable IRCd then download UnrealIRCd 3.2 from www.unrealircd.com,
or use the 'master' branch on GitHub (NOTE: this is not the default branch!).

Otherwise, if you want to check out the latest technology and features, don't mind
crashing once in a while and want to help us test, then try out **3.4.x (alpha!)**.
On GitHub use the 'unreal34' branch (the default).

If you are just interested in checking out UnrealIRCd for the first time then we
recommend you to try 3.2.x and not 3.4.x. UnrealIRCd 3.2.x is full of features as
well and is much more stable. Also the documentation for 3.4.x is not up to date
which may hinder you getting started.

## How to get started
Please consult our excelent online documentation at http://www.unrealircd.com/unreal32docs.html
when setting up the IRCd!

### Step 1: Installation
#### Windows
Simply download the UnrealIRCd Windows version from www.unrealircd.com

Compiling UnrealIRCd for Windows yourself is not straightforward and thus not recommended.

#### *BSD/Linux/OS X
First you must compile the IRCd:

* Run `./Config`
* Run `make`
* If you chose to install the IRCd at a different location than the defaults, then also run `make install`

### Step 2: Configuration
#### Create a configuration file
If you are new, then you need to create your own configuration file:
copy `doc/example.conf` to your main UnrealIRCd directory and call it `unrealircd.conf`.
Then open it in an editor and carefully modify it using the documentation and FAQ (see below) as a guide.

### Step 3: Booting
#### Windows
* Simply run `wircd.exe`

#### *BSD/Linux/OS X
* Run `./unreal start`

Note that after booting, the errors are usually logged to `ircd.log`
so check that file if you have any problems.

## Documentation & FAQ
You can find the **documentation** online at: http://www.unrealircd.com/unreal32docs.html

We also have a good **FAQ**: http://www.unrealircd.com/faq

## Website, support, and other links ##
* http://www.unrealircd.com - Our main website
* http://forums.unrealircd.com - Support
* http://bugs.unrealircd.com - Bug tracker
* http://modules.unrealircd.com - 3rd party modules
