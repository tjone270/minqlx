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

import minqlx
import sched
import re

# ====================================================================
#                        REGULAR EXPRESSIONS
# ====================================================================

_re_say = re.compile(r"^say +\"?(?P<msg>.+)\"?$", flags=re.IGNORECASE)
_re_say_team = re.compile(r"^say_team +\"?(?P<msg>.+)\"?$", flags=re.IGNORECASE)
_re_callvote = re.compile(r"^(?:cv|callvote) +(?P<cmd>[^ ]+)(?: \"?(?P<args>.+?)\"?)?$", flags=re.IGNORECASE)
_re_vote = re.compile(r"^vote +(?P<arg>.)", flags=re.IGNORECASE)
_re_team = re.compile(r"^team +(?P<arg>.)", flags=re.IGNORECASE)
_re_vote_ended = re.compile(r"^print \"Vote (?P<result>passed|failed).\n\"$")

# ====================================================================
#                         LOW-LEVEL HANDLERS
#        These are all called by the C code, not within Python.  
# ====================================================================

def handle_rcon(cmd):
    """Console commands that are to be processed as regular pyminqlx
    commands as if the owner executes it. This allows the owner to
    interact with the Python part of minqlx without having to connect.

    """
    minqlx.COMMANDS.handle_input(minqlx.RconDummyPlayer(), cmd, minqlx.CONSOLE_CHANNEL)

def handle_client_command(client_id, cmd):
    """Client commands are commands such as "say", "say_team", "scores",
    "disconnect" and so on. This function parses those and passes it
    on to the event dispatcher.

    :param client_id: The client identifier.
    :type client_id: int
    :param cmd: The command being ran by the client.
    :type cmd: str

    """
    try:
        # Dispatch the "client_command" event before further processing.
        player = minqlx.Player(client_id)
        retval = minqlx.EVENT_DISPATCHERS["client_command"].dispatch(player, cmd)
        if retval == False:
            return False
        elif isinstance(retval, str):
            # Allow plugins to modify the command before passing it on.
            cmd = retval

        res = _re_say.match(cmd)
        if res:
            msg = res.group("msg").replace("\"", "")
            channel = minqlx.CHAT_CHANNEL
            if minqlx.EVENT_DISPATCHERS["chat"].dispatch(player, msg, channel) == False:
                return False
            return cmd
        
        res = _re_say_team.match(cmd)
        if res:
            msg = res.group("msg").replace("\"", "")
            if player.team == "free": # I haven't tried this, but I don't think it's even possible.
                channel = minqlx.FREE_CHAT_CHANNEL
            elif player.team == "red":
                channel = minqlx.RED_TEAM_CHAT_CHANNEL
            elif player.team == "blue":
                channel = minqlx.BLUE_TEAM_CHAT_CHANNEL
            else:
                channel = minqlx.SPECTATOR_CHAT_CHANNEL
            if minqlx.EVENT_DISPATCHERS["chat"].dispatch(player, msg, channel) == False:
                return False
            return cmd

        res = _re_callvote.match(cmd)
        if res and not minqlx.Plugin.is_vote_active():
            vote = res.group("cmd")
            args = res.group("args") if res.group("args") else ""
            if minqlx.EVENT_DISPATCHERS["vote_called"].dispatch(player, vote, args) == False:
                return False
            return cmd

        res = _re_vote.match(cmd)
        if res and minqlx.Plugin.is_vote_active():
            arg = res.group("arg").lower()
            if arg == "y" or arg == "1":
                if minqlx.EVENT_DISPATCHERS["vote"].dispatch(player, True) == False:
                    return False
            elif arg == "n" or arg == "2":
                if minqlx.EVENT_DISPATCHERS["vote"].dispatch(player, False) == False:
                    return False
            return cmd

        res = _re_team.match(cmd)
        if res:
            arg = res.group("arg").lower()
            target_team = ""
            if arg == player.team[0]:
                # Don't trigger if player is joining the same team.
                return cmd
            elif arg == "f":
                target_team = "free"
            elif arg == "r":
                target_team = "red"
            elif arg == "b":
                target_team = "blue"
            elif arg == "s":
                target_team = "spectator"
            elif arg == "a":
                target_team = "any"

            if target_team:
                if minqlx.EVENT_DISPATCHERS["team_switch_attempt"].dispatch(player, player.team, target_team) == False:
                    return False
            return cmd

        return cmd
    except:
        minqlx.log_exception()
        return True

def handle_server_command(client_id, cmd):
    try:
        # Dispatch the "server_command" event before further processing.
        try:
            player = minqlx.Player(client_id) if client_id >= 0 else None
        except minqlx.NonexistentPlayerError:
            return True

        retval = minqlx.EVENT_DISPATCHERS["server_command"].dispatch(player, cmd)
        if retval == False:
            return False
        elif isinstance(retval, str):
            cmd = retval

        res = _re_vote_ended.match(cmd)
        if res:
            if res.group("result") == "passed":
                minqlx.EVENT_DISPATCHERS["vote_ended"].dispatch(True)
            else:
                minqlx.EVENT_DISPATCHERS["vote_ended"].dispatch(False)
        
        return cmd
    except:
        minqlx.log_exception()
        return True

# Executing tasks right before a frame, by the main thread, will often be desirable to avoid
# weird behavior if you were to use threading. This list will act as a task queue.
# Tasks can be added by simply adding the @minqlx.next_frame decorator to functions.
frame_tasks = sched.scheduler()

def handle_frame():
    """This will be called every frame. To allow threads to call stuff from the
    main thread, tasks can be scheduled using the :func:`minqlx.next_frame` decorator
    and have it be executed here.

    """
    
    while True:
        # This will run all tasks that are currently scheduled.
        # If one of the tasks throw an exception, it'll log it
        # and continue execution of the next tasks if any.
        try:
            frame_tasks.run(blocking=False)
            break
        except:
            minqlx.log_exception()
            continue
    try:
        minqlx.EVENT_DISPATCHERS["frame"].dispatch()
    except:
        minqlx.log_exception()
        return True

_zmq_warning_issued = False
_first_game = True

def handle_new_game(is_restart):
    # This is called early in the launch process, so it's a good place to initialize
    # minqlx stuff that needs QLDS to be initialized.
    global _first_game
    if _first_game:
        minqlx.late_init()
        _first_game = False

        # A good place to warn the owner if ZMQ stats are disabled.
        global _zmq_warning_issued
        if not bool(int(minqlx.get_cvar("zmq_stats_enable"))) and not _zmq_warning_issued:
            logger = minqlx.get_logger()
            logger.warning("Some events will not work because ZMQ stats is not enabled. "
                "Launch the server with \"zmq_stats_enable 1\"")
            _zmq_warning_issued = True

    minqlx.set_map_subtitles()

    if not is_restart:
        try:
            minqlx.EVENT_DISPATCHERS["map"].dispatch(
                minqlx.get_cvar("mapname"), 
                minqlx.get_cvar("g_factory"))
        except:
            minqlx.log_exception()
            return True

    try:
        minqlx.EVENT_DISPATCHERS["new_game"].dispatch()
    except:
        minqlx.log_exception()
        return True

def handle_set_configstring(index, value):
    """Called whenever the server tries to set a configstring. Can return
    False to stop the event.

    """
    try:
        res = minqlx.EVENT_DISPATCHERS["set_configstring"].dispatch(index, value)
        if res == False:
            return False
        elif isinstance(res, str):
            value = res

        # GAME STATE CHANGES
        if index == 0:
            old_cs = minqlx.parse_variables(minqlx.get_configstring(index))
            if not old_cs:
                return
            
            new_cs = minqlx.parse_variables(value)
            old_state = old_cs["g_gameState"]
            new_state = new_cs["g_gameState"]
            if old_state != new_state:
                if old_state == "PRE_GAME" and new_state == "IN_PROGRESS":
                    pass
                elif old_state == "PRE_GAME" and new_state == "COUNT_DOWN":
                    minqlx.EVENT_DISPATCHERS["game_countdown"].dispatch()
                elif old_state == "COUNT_DOWN" and new_state == "IN_PROGRESS":
                    pass
                    #minqlx.EVENT_DISPATCHERS["game_start"].dispatch()
                elif old_state == "IN_PROGRESS" and new_state == "PRE_GAME":
                    pass
                elif old_state == "COUNT_DOWN" and new_state == "PRE_GAME":
                    pass
                else:
                    logger = minqlx.get_logger()
                    logger.warning("UNKNOWN GAME STATES: {} - {}".format(old_state, new_state))

        # ROUND COUNTDOWN AND START
        if index == 661:
            cvars = minqlx.parse_variables(value)
            if cvars:
                round_number = int(cvars["round"])
                if round_number and "time" in cvars:
                    if round_number == 1:  # This is the case when the first countdown starts.
                        minqlx.EVENT_DISPATCHERS["round_countdown"].dispatch(round_number)
                        return

                    minqlx.EVENT_DISPATCHERS["round_countdown"].dispatch(round_number)
                    return
                elif round_number:
                    minqlx.EVENT_DISPATCHERS["round_start"].dispatch(round_number)
                    return

        return res
    except:
        minqlx.log_exception()
        return True

def handle_player_connect(client_id, is_bot):
    """This will be called whenever a player tries to connect. If the dispatcher
    returns False, it will not allow the player to connect and instead show them
    a message explaining why. The default message is "You are banned from this
    server.", but it can be set with :func:`minqlx.set_ban_message`.

    :param client_id: The client identifier.
    :type client_id: int
    :param is_bot: Whether or not the player is a bot.
    :type is_bot: bool

    """
    try:
        player = minqlx.Player(client_id)
        return minqlx.EVENT_DISPATCHERS["player_connect"].dispatch(player)
    except:
        minqlx.log_exception()
        return True

def handle_player_loaded(client_id):
    """This will be called whenever a player has connected and finished loading,
    meaning it'll go off a bit later than the usual "X connected" messages.
    This will not trigger on bots.

    :param client_id: The client identifier.
    :type client_id: int

    """
    try:
        player = minqlx.Player(client_id)
        return minqlx.EVENT_DISPATCHERS["player_loaded"].dispatch(player)
    except:
        minqlx.log_exception()
        return True

def handle_player_disconnect(client_id, reason):
    """This will be called whenever a player disconnects.

    :param client_id: The client identifier.
    :type client_id: int

    """
    try:
        player = minqlx.Player(client_id)
        return minqlx.EVENT_DISPATCHERS["player_disconnect"].dispatch(player, reason)
    except:
        minqlx.log_exception()
        return True

def handle_console_print(text):
    """Called whenever the server tries to set a configstring. Can return
    False to stop the event and can be modified along the handler chain.

    """
    try:
        text = text.decode(errors="ignore").rstrip()
        if not text:
            return
        
        # Log console output. Removes the need to have stdout logs in addition to minqlx.log.
        minqlx.get_logger().debug(text)

        res = minqlx.EVENT_DISPATCHERS["console_print"].dispatch(text)
        if res == False:
            return False
        elif isinstance(res, str):
            return res
    except:
        minqlx.log_exception()
        return True


def register_handlers():
    minqlx.register_handler("rcon", handle_rcon)
    minqlx.register_handler("client_command", handle_client_command)
    minqlx.register_handler("server_command", handle_server_command)
    minqlx.register_handler("frame", handle_frame)
    minqlx.register_handler("new_game", handle_new_game)
    minqlx.register_handler("set_configstring", handle_set_configstring)
    minqlx.register_handler("player_connect", handle_player_connect)
    minqlx.register_handler("player_loaded", handle_player_loaded)
    minqlx.register_handler("player_disconnect", handle_player_disconnect)
    minqlx.register_handler("console_print", handle_console_print)
