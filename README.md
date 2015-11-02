minqlx
======
minqlx is a modification to the Quake Live Dedicated Server that Quake Live's dedicated server with
extra functionality and allows scripting of server behavior through an embedded Python
interpreter.

**NOTE:** This is still in a very early development stage. Bugs **will** happen.

The mod has been tested on Debian 7 and 8, and Ubuntu 14.04. At the moment it only supports the x64 
build of the server, but x86 will definitely be added. In fact, at some point it was x86 only,
but I added x64 support and decided to keep only one of them up to date because the
frequency of updates during the beta was a bit too much to keep up with, since key
structures were changing all the time. Once the frequency drops, I will do it.

Installation
============
These instructions are for Debian 7 or 8 (use the latter if you can choose). For Ubuntu,
it's pretty much the same except you add Ubuntu 15's package repository temporarily,
since it has Python 3.5 in it. You're on your own for the time being on other distros,
but feel free to add instructions to the [wiki](https://github.com/MinoMino/minqlx/wiki)
if you want to help out.

- Install Python 3.5. At the time of writing, in Debian 7 or 8 you can install it by adding the `sid` repository to apt. This can be done by adding the line `deb http://ftp.debian.org/debian sid main` to
`/etc/apt/sources.list`. We also use Redis and git for our standard plugins, so we install that as well:

```
sudo apt-get update
sudo apt-get -y install python3.5 python3.5-dev
sudo apt-get -y install redis-server git
```

- You should remove `sid` by commenting out or removing the line you added to `sources.list`
earlier and then do `sudo apt-get update` again to make sure you don't install any unstable
packages unintentionally later.

- Download one of the tarballs in [releases](https://github.com/MinoMino/minqlx/releases) and extract
its contents into `steamcmd/steamapps/common/qlds`, or whatever other directory you might have put the
files of your server in.

- Clone the plugins repository and get/build Python dependencies. Assuming you're in
the directory with all the server files (and where you extracted the above files) do:

```
git clone https://github.com/MinoMino/minqlx-plugins.git
wget https://raw.github.com/pypa/pip/master/contrib/get-pip.py
sudo python3.5 get-pip.py
rm get-pip.py
sudo apt-get -y install build-essential
sudo python3.5 -m easy_install pyzmq hiredis
sudo python3.5 -m pip install -r minqlx-plugins/requirements.txt
```

**NOTE**: During the pip and easy_install steps, you might get a lot of warnings. You can safely
ignore them.

- Redis should work right off the bat, but you might want to edit the config and make
it use UNIX sockets instead for the sake of speed. minqlx is configured through cvars,
just like you would configure the QLDS. This means it can be done either with a server.cfg
or by passing the cvars as command line arguments with `+set`. All the cvars have default
values, except for `qlx_owner`, which is your SteamID64 (there are converters out there, just google it).
Make sure you set that, otherwise you won't be able to execute any admin commands,
since it won't know you are the owner of it.

- You're almost there. Now simply edit the scripts you use to launch the server, but
make it point to `run_server_x64_minqlx.sh` instead of `run_server_x64.sh`.

Configuration
=============
minqlx is configured using cvars, like you would configure the server. All minqlx cvars
should be prefixed with `qlx_`. The following cvars are the core cvars. For plugin configuration
see the [plugins repository](https://github.com/MinoMino/minqlx-plugins).

- `qlx_owner`: The SteamID64 of the server owner. This is should be set, otherwise minqlx
can't tell who the owner is and will refuse to execute admin commands.
- `qlx_plugins`: A comma-separated list of plugins that should be loaded at launch.
  - Default: `plugin_manager, essentials, motd, permission, ban, clan`.
- `qlx_pluginsPath`: The path (either relative or absolute) to the directory with the plugins.
  - Default: `minqlx-plugins`
- `qlx_database`: The default database to use. You should not change this unless you know what you're doing.
  - Default: `Redis`
- `qlx_commandPrefix`: The prefix used before command names in order to execute them.
  - Default: `!`
- `qlx_redisAddress`: The address to the Redis database. Can be a path if `qlx_redisUnixSocket` is `"1"`.
  - Default: `127.0.0.1`
- `qlx_redisDatabase`: The Redis database number.
  - Default: `0`
- `qlx_redisUnixSocket`: A boolean that determines whether or not `qlx_redisAddress` is a path to a UNIX socket.
  - Default: `0`
- `qlx_redisPassword`: The password to the Redis server, if any.
  - Default: None

Compiling
=========
**NOTE**: This is *not* required if you are using binaries.

It's just a makefile for now. No autoconf or anything, so you might need to edit the file in some cases.
It assumes you have GCC and that `python3.5-config` is Python 3.5's python-config. On Debian, install
`python3.5-dev` and it should compile right off the bat assuming you have all the build tools.

To compile, just do a `make` and you should get a `minqlx.so` and a `minqlx.zip` in the `bin` directory.
The `bin` directory also has launch scripts, so you can simply copy the contents of the `bin` directory
into the QLDS folder and use those scripts to launch it. If you do not want to use this with
Python, you can compile it with `make nopy` and you should get a `minqlx_nopy.so` instead.

Contribute
==========
If you'd like to contribute with code, you can fork this or the plugin repository and create pull requests for changes. If you found a bug, please open an issue here on Github and include the relevant part from either the
server's console output or from `minqlx.log` which is in your `fs_homepath`, preferably the latter as it is
more verbose. Note that `minqlx.log` does get truncated between runs.

Both when compiling and when using binaries, the core module is in a zip file. If you want to modify
the code, simply unzip the contents of it in the same directory and then delete the zip file. minqlx will
continue to function in the same manner, but using the code that is now in the `minqlx` directory.

Donations would also be greatly appreciated. It helps me with motivation, QL server costs, and to fund
the stupid amount of tea I drink. You can do so with [PayPal](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=mino%40minomino%2eorg&lc=US&item_name=Mino&item_number=minqlbot&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donate_SM%2egif%3aNonHosted) or with Bitcoins to `1MinoB3DxijyXSLgzA6JYGKmM3Jj6Gw2wW`.
