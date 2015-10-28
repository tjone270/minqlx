minqlx
======
minqlx is a modification to the Quake Live Dedicated Server that Quake Live's dedicated server with
extra functionality and allows scripting of server behavior through an embedded Python
interpreter.

**NOTE:** This is still in a very early development stage. Bugs **will** happen.

The mod has been tested on Debian 7 and 8. At the moment it only supports the x64 build
of the server, but x86 will definitely be added. In fact, at some point it was x86 only,
but I added x64 support and decided to keep only one of them up to date because the
frequency of updates during the beta was a bit too much to keep up with, since key
structures were changing all the time.

Installation
============
These instructions are for Debian 7 or 8 (use the latter if you can choose). I imagine Ubuntu
is very similar. You're on your own for the time being on other distros, but feel free to add
instructions to the [wiki](https://github.com/MinoMino/minqlx/wiki) if you want to help out.

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
git clone https://github.com/MinoMino/minqlx-plugins.git minqlx/plugins
wget https://raw.github.com/pypa/pip/master/contrib/get-pip.py
sudo python3.5 get-pip.py
rm get-pip.py
sudo apt-get -y install build-essential
sudo python3.5 -m easy_install pyzmq hiredis
sudo python3.5 -m pip install -r minqlx/plugins/requirements.txt
```

- You now need to configure both the config in `qlds/minqlx/config.cfg` and Redis.
I recommend you use Unix sockets with Redis, like the default config suggests. Also
make sure you add your SteamID64 (there are converters out there, just google it)
to the `Owner` field in the config, otherwise you won't be able to execute any
admin commands, since it won't know you are the owner of it.

Compiling
=========
It's just a makefile for now. No autoconf or anything, so you might need to edit the file in some cases.
It assumes you have GCC and that `python3.5-config` is Python 3.5's python-config. On Debian, install
`python3.5-dev` and it should compile right off the bat assuming you have all the build tools.

To compile, just do a `make` and you should get a `minqlx.so`. If you do not want to use this without
Python, you can compile it with `make nopy` and you should get a `minqlx_nopy.so`.

Contribute
==========
If you'd like to contribute with code, you can fork this or the plugin repository and create pull requests for changes. If you found a bug, please open an issue here on Github and include the relevant part from either the
server's console output or from `steamapps/common/qlds/pyminqlx.log`, preferably the latter as it is
more verbose.

Donations would also be greatly appreciated. It helps me with motivation, QL server costs, and to fund
the stupid amount of tea I drink. You can do so with [PayPal](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=mino%40minomino%2eorg&lc=US&item_name=Mino&item_number=minqlbot&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donate_SM%2egif%3aNonHosted) or with Bitcoins to `1MinoB3DxijyXSLgzA6JYGKmM3Jj6Gw2wW`.
