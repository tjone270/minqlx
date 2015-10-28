# minqlx - Extends Quake Live's dedicated server with extra functionality and scripting.
# Copyright (C) 2015 Mino <mino@minomino.org>

# This file is part of minqlx.

# minqlx is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# minqlx is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with minqlx. If not, see <http://www.gnu.org/licenses/>.


# Since this isn't the actual module, we define it here and export
# it later so that it can be accessed with minqlx.__doc__ by Sphinx.

import minqlx
import minqlx.database
import configparser
import collections
import threading
import traceback
import importlib
import datetime
import os.path
import logging
import sys
import os

# Team number -> string
TEAMS = dict(enumerate(("free", "red", "blue", "spectator")))

# Game type number -> string
GAMETYPES = dict(enumerate(("Free for All", "Duel", "Race", "Team Deathmatch", "Clan Arena",
    "Capture the Flag", "Overload", "Harvester", "Freeze Tag", "Domination", "Attack and Defend", "Red Rover")))

# Game type number -> short string
GAMETYPES_SHORT = dict(enumerate(("ffa", "duel", "race", "tdm", "ca", "ctf", "ob", "har", "ft", "dom", "ad", "rr")))

# ====================================================================
#                               HELPERS
# ====================================================================

def parse_variables(varstr, ordered=False):
    """
    Parses strings of key-value pairs delimited by "\\" and puts
    them into a dictionary.

    :param varstr: The string with variables.
    :type varstr: str
    :param ordered: Whether it should use :class:`collections.OrderedDict` or not.
    :type ordered: bool
    :returns: dict -- A dictionary with the variables added as key-value pairs.
    """
    if ordered:
        res = collections.OrderedDict()
    else:
        res = {}
    if not varstr.strip():
        return res
    
    vars = varstr.lstrip("\\").split("\\")
    try:
        for i in range(0, len(vars), 2):
            res[vars[i]] = vars[i + 1]
    except:
        raise ValueError("Uneven number of keys and values: {}".format(varstr))
    
    return res

main_logger = None

def get_logger(plugin=None):
    """
    Provides a logger that should be used by your plugin for debugging, info
    and error reporting. It will automatically output to both the server console
    as well as to a file.

    :param plugin: The plugin that is using the logger.
    :type plugin: minqlx.Plugin
    :returns: logging.Logger -- The logger in question.
    """
    if plugin:
        return logging.getLogger("minqlx." + str(plugin))
    else:
        return logging.getLogger("minqlx")

def _configure_logger():
    logger = logging.getLogger("minqlx")
    logger.setLevel(logging.DEBUG)
    
    # File
    file_fmt = logging.Formatter("(%(asctime)s) [%(levelname)s @ %(name)s.%(funcName)s] %(message)s", "%H:%M:%S")
    file_handler = logging.FileHandler("minqlx/pyminqlx.log", mode="w", encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(file_fmt)
    logger.addHandler(file_handler)
    logger.info("File logger initialized!")

    # Console
    console_fmt = logging.Formatter("[%(name)s.%(funcName)s] %(levelname)s: %(message)s", "%H:%M:%S")
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG if minqlx.DEBUG else logging.INFO)
    console_handler.setFormatter(console_fmt)
    logger.addHandler(console_handler)

def log_exception(plugin=None):
    """
    Logs an exception using :func:`get_logger`. Call this in an except block.

    :param plugin: The plugin that is using the logger.
    :type plugin: minqlx.Plugin
    """
    # TODO: Remove plugin arg and make it automatic.
    logger = get_logger(plugin)
    e = traceback.format_exc().rstrip("\n")
    for line in e.split("\n"):
        logger.error(line)

def handle_exception(exc_type, exc_value, exc_traceback):
    """A handler for unhandled exceptions."""
    # TODO: If exception was raised within a plugin, detect it and pass to log_exception()
    logger = get_logger(None)
    e = "".join(traceback.format_exception(exc_type, exc_value, exc_traceback)).rstrip("\n")
    for line in e.split("\n"):
        logger.error(line)

_init_time = datetime.datetime.now()

def uptime():
    """Returns a :class:`datetime.timedelta` instance of the time since initialized."""
    return datetime.datetime.now() - _init_time

# This is initialized in load_config. It's the SteamID64 of the owner.
# It's used to bypass any permission checks so that the owner can do
# stuff without having to initialize the database first.
_owner = "-1"

def owner():
    """Returns the SteamID64 of the owner. This is set in the config."""
    return _owner

# Can be overriden in the config by setting "CommandPrefix" under the Core section.
_command_prefix = "!"

def command_prefix():
    """Returns the command prefix."""
    return _command_prefix

_stats = None

def stats_listener():
    """Returns the :class:`minqlx.StatsListener` instance used to listen for stats."""
    return _stats

# ====================================================================
#                              DECORATORS
# ====================================================================

def next_frame(func):
    def f(*args, **kwargs):
        minqlx.frame_tasks.enter(0, 0, func, args, kwargs)
    
    return f

def delay(time):
    """Delay a function call a certain amount of time.

    .. note::
        It cannot guarantee you that it will be called right as the timer
        expires, but unless some plugin is for some reason blocking, then
        you can expect it to be called practically as soon as it expires.

    :param func: The function to be called.
    :type func: callable
    :param time: The number of seconds before the function should be called.
    :type time: int

    """
    def wrap(func):
        def f(*args, **kwargs):
            minqlx.frame_tasks.enter(time, 0, func, args, kwargs)
        return f
    return wrap

_thread_count = 0
_thread_name = "minqlxthread"

def thread(func, force=False):
    """Starts a thread with the function passed as its target. If a function decorated
    with this is called within a function also decorated, it will **not** create a second
    thread unless told to do so with the *force* keyword.

    :param func: The function to be ran in a thread.
    :type func: callable
    :param force: Force it to create a new thread even if already in one created by this decorator.
    :type force: bool
    :returns: threading.Thread

    """
    def f(*args, **kwargs):
        if not force and threading.current_thread().name.endswith(_thread_name):
            func(*args, **kwargs)
        else:
            global _thread_count
            name = func.__name__ + "-{}-{}".format(str(_thread_count), _thread_name)
            t = threading.Thread(target=func, name=name, args=args, kwargs=kwargs, daemon=True)
            t.start()
            _thread_count += 1

            return t
    
    return f

# ====================================================================
#                       CONFIG AND PLUGIN LOADING
# ====================================================================

# We need to keep track of module instances for use with importlib.reload.
_modules = {}

class PluginLoadError(Exception):
    pass

class PluginUnloadError(Exception):
    pass

def load_config():
    config = get_config()
    config_file = "minqlx/config.cfg"
    if os.path.isfile(config_file):
        config.read(config_file)
        config["DEFAULT"] = { 
                                "PluginsFolder" : "minqlx/plugins",
                                "CommandPrefix" : "!"
                            }

        # Set default database if present.
        if "Database" in config["Core"]:
            db = config["Core"]["Database"]
            if db.lower() == "redis":
                minqlx.Plugin.database = minqlx.database.Redis

        # Set owner.
        # TODO: Convert regular SteamID format to 64-bit format automatically.
        if "Owner" in config["Core"]:
            try:
                global _owner
                _owner = int(config["Core"]["Owner"])
            except ValueError:
                logger = minqlx.get_logger()
                logger.error("Failed to parse the Owner Steam ID. Make sure it's in SteamID64 format.")
        else:
            logger = minqlx.get_logger()
            logger.warning("Owner not set in the config. Consider setting it.")

        sys.path.append(os.path.dirname(config["Core"]["PluginsFolder"]))
        global _command_prefix
        _command_prefix = config["Core"]["CommandPrefix"].strip()
        return config
    else:
        raise(RuntimeError("Config file '{}' not found.".format(os.path.abspath(config_file))))

def reload_config():
    load_config()
    load_preset_plugins() # Will not double-load plugins.

_config = None

def get_config():
    global _config
    if not _config:
        _config = configparser.ConfigParser()

    return _config

def load_preset_plugins():
    config = get_config()
    if os.path.isdir(config["Core"]["PluginsFolder"]):
        # Filter out already loaded plugins. This allows us to safely call this function after reloading the config.
        plugins = [p.strip() for p in config["Core"]["Plugins"].split(",") if "plugins." + p not in sys.modules]
        for plugin in plugins:
            load_plugin(plugin.strip())
    else:
        raise(PluginLoadError("Cannot find the plugins directory '{}'."
            .format(os.path.abspath(config["Core"]["PluginsFolder"]))))

def load_plugin(plugin):
    logger = get_logger(None)
    logger.info("Loading plugin '{}'...".format(plugin))
    plugins = minqlx.Plugin._loaded_plugins
    conf = get_config()

    if not os.path.isfile(os.path.join(conf["Core"]["PluginsFolder"], plugin + ".py")):
        raise PluginLoadError("No such plugin exists.")
    elif plugin in plugins:
        return reload_plugin(plugin)
    try:
        module = importlib.import_module("plugins." + plugin)
        if not hasattr(module, plugin):
            raise(PluginLoadError("The plugin needs to have a class with the exact name as the file, minus the .py."))
        
        global _modules
        plugin_class = getattr(module, plugin)
        _modules[plugin] = module
        if issubclass(plugin_class, minqlx.Plugin):
            plugins[plugin] = plugin_class()
        else:
            raise(PluginLoadError("Attempted to load a plugin that is not a subclass of 'minqlx.Plugin'."))
    except:
        log_exception(plugin)
        raise

def unload_plugin(plugin):
    logger = get_logger(None)
    logger.info("Unloading plugin '{}'...".format(plugin))
    plugins = minqlx.Plugin._loaded_plugins
    if plugin in plugins:
        try:
            minqlx.EVENT_DISPATCHERS["unload"].dispatch(plugin)

            # Unhook its hooks.
            for hook in plugins[plugin].hooks:
                plugins[plugin].remove_hook(*hook)

            # Unregister commands.
            for cmd in plugins[plugin].commands:
                plugins[plugin].remove_command(cmd.name, cmd.handler)
                
            del plugins[plugin]
        except:
            log_exception(plugin)
            raise
    else:
        raise(PluginUnloadError("Attempted to unload a plugin that is not loaded."))

def reload_plugin(plugin):
    try:
        unload_plugin(plugin)
    except PluginUnloadError:
        pass

    try:
        global _modules
        if plugin in _modules: # Unloaded previously?
            importlib.reload(_modules[plugin])
        load_plugin(plugin)
    except:
        log_exception(plugin)
        raise


# ====================================================================
#                                 MAIN
# ====================================================================

def initialize():
    _configure_logger()
    logger = get_logger()
    # Set our own exception handler so that we can log them if unhandled.
    sys.excepthook = handle_exception

    logger.info("Loading config...")
    load_config()
    logger.info("Loading preset plugins...")
    load_preset_plugins()
    logger.info("Registering handlers...")
    minqlx.register_handlers()

    # Needs to be called after server initialization, so a simple
    # next_frame will make sure that's the case.
    @minqlx.next_frame
    def start_stats_listener():
        if bool(int(minqlx.get_cvar("zmq_stats_enable"))):
            global _stats
            _stats = minqlx.StatsListener()
            logger.info("Stats listener started on {}.".format(_stats.address))
            # Start polling. Not blocking due to decorator magic. Aw yeah.
            _stats.keep_receiving()
    start_stats_listener()

    logger.info("We're good to go!")
