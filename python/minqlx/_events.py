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
import re

_re_vote = re.compile(r"^(?P<cmd>[^ ]+)(?: \"?(?P<args>.+?)\"?)?$")

# ====================================================================
#                               EVENTS
# ====================================================================

class EventDispatcher:
    """The base event dispatcher. Each event should inherit this and provides a way
    to hook into events by registering an event handler.

    """
    no_debug = ("frame", "set_configstring", "stats", "server_command", "death", "kill", "command", "console_print")

    def __init__(self):
        self.name = type(self).name
        self.plugins = {}
    
    def dispatch(self, *args, **kwargs):
        """Calls all the handlers that have been registered when hooking this event.
        The recommended way to use this for events that inherit this class is to
        override the method with explicit arguments (as opposed to the this one's)
        and call this method by using ``super().dispatch()``.

        Handlers have several options for return values that can affect the flow:
            - minqlx.RET_NONE or None -- Continue execution normally.
            - minqlx.RET_STOP -- Stop any further handlers from being called.
            - minqlx.RET_STOP_EVENT -- Let handlers process it, but stop the event
                at the engine-level.
            - minqlx.RET_STOP_ALL -- Stop handlers **and** the event.
            - Any other value -- Passed on to :func:`self.handle_return`, which will
                by default simply send a warning to the logger about an unknown value
                being returned. Can be overridden so that events can have their own
                special return values.

        :param args: Any arguments.
        :param kwargs: Any keyword arguments.
        
        """
        # Allow subclasses of this to edit the arguments without having
        # to reimplement this method. Whenever an unknown return value
        # is returned, we pass it on to handle_return.
        self.args = args
        self.kwargs = kwargs
        logger = minqlx.get_logger()
        # Log the events as they come in.
        if self.name not in self.no_debug:
            dbgstr = "{}{}".format(self.name, args)
            if len(dbgstr) > 100:
                dbgstr = dbgstr[0:99] + ")"
            logger.debug(dbgstr)

        plugins = self.plugins.copy()
        self.return_value = True
        for i in range(5):
            for plugin in plugins:
                for handler in plugins[plugin][i]:
                    try:
                        res = handler(*self.args, **self.kwargs)
                        if res == minqlx.RET_NONE or res == None:
                            continue
                        elif res == minqlx.RET_STOP:
                            return True
                        elif res == minqlx.RET_STOP_EVENT:
                            self.return_value = False
                        elif res == minqlx.RET_STOP_ALL:
                            return False
                        else: # Got an unknown return value.
                            return_handler = self.handle_return(handler, res)
                            if return_handler != None:
                                return return_handler
                    except:
                        minqlx.log_exception(plugin)
                        continue

        return self.return_value

    def handle_return(self, handler, value):
        """Handle an unknown return value. If this returns anything but None,
        the it will stop execution of the event and pass the return value on
        to the C-level handlers. This method can be useful to override,
        because of the fact that you can edit the arguments that will be
        passed on to any handler after whatever handler returned *value*
        by editing *self.args*, *self.kwargs*. Furthermore, *self.return_value*
        is the return value that will be sent to the C-level handler if the
        event isn't stopped later along the road.
        """
        logger = minqlx.get_logger()
        logger.warning("Handler '{}' returned unknown value '{}' for event '{}'"
            .format(handler.__name__, value, self.name))
    
    def add_hook(self, plugin, handler, priority=minqlx.PRI_NORMAL):
        """Hook the event, making the handler get called with relevant arguments
        whenever the event is takes place.

        :param plugin: The plugin that's hooking the event.
        :type plugin: minqlx.Plugin
        :param handler: The handler to be called when the event takes place.
        :type handler: callable
        :param priority: The priority of the hook. Determines the order the handlers are called in.
        :type priority: minqlx.PRI_LOWEST, minqlx.PRI_LOW, minqlx.PRI_NORMAL, minqlx.PRI_HIGH or minqlx.PRI_HIGHEST
        :raises: ValueError
        
        """
        if not (priority >= minqlx.PRI_HIGHEST and priority <= minqlx.PRI_LOWEST):
            raise ValueError("'{}' is an invalid priority level.".format(priority))
        
        if plugin not in self.plugins:
            # Initialize tuple.
            self.plugins[plugin] = ([], [], [], [], []) # 5 priority levels.
        else:
            # Check if we've already registered this handler.
            for i in range(len(self.plugins[plugin])):
                for hook in self.plugins[plugin][i]:
                    if handler == hook:
                        raise ValueError("The event has already been hooked with the same handler and priority.")
        
        self.plugins[plugin][priority].append(handler)
        
    def remove_hook(self, plugin, handler, priority=minqlx.PRI_NORMAL):
        """Removes a previously hooked event.
        
        :param plugin: The plugin that hooked the event.
        :type plugin: minqlx.Plugin
        :param handler: The handler used when hooked.
        :type handler: callable
        :param priority: The priority of the hook when hooked.
        :type priority: minqlx.PRI_LOWEST, minqlx.PRI_LOW, minqlx.PRI_NORMAL, minqlx.PRI_HIGH or minqlx.PRI_HIGHEST
        :raises: ValueError

        """
        for hook in self.plugins[plugin][priority]:
            if handler == hook:
                self.plugins[plugin][priority].remove(handler)
                return
        
        raise ValueError("The event has not been hooked with the handler provided")

class EventDispatcherManager:
    """Holds all the event dispatchers and provides a way to access the dispatcher
    instances by accessing it like a dictionary using the event name as a key.
    Only one dispatcher can be used per event.

    """
    def __init__(self):
        self._dispatchers = {}

    def __getitem__(self, key):
        return self._dispatchers[key]

    def __contains__(self, key):
        return key in self._dispatchers

    def add_dispatcher(self, dispatcher):
        if dispatcher.name in self:
            raise ValueError("Event name already taken.")
        elif not issubclass(dispatcher, EventDispatcher):
            raise ValueError("Cannot add an event dispatcher not based on EventDispatcher.")

        self._dispatchers[dispatcher.name] = dispatcher()

    def remove_dispatcher(self, dispatcher):
        if dispatcher.name not in self:
            raise ValueError("Event name not found.")

        del self._dispatchers[dispatcher.name]

    def remove_dispatcher_by_name(self, event_name):
        if event_name not in self:
            raise ValueError("Event name not found.")

        del self._dispatchers[event_name]

# ====================================================================
#                          EVENT DISPATCHERS
# ====================================================================

class ConsolePrintDispatcher(EventDispatcher):
    name = "console_print"
    
    def dispatch(self, text):
        return super().dispatch(text)

    def handle_return(self, handler, value):
        """If a string was returned, continue execution, but we edit the
        string that's being printed along the chain of handlers.

        """
        if isinstance(value, str):
            self.args = (value,)
            self.return_value = value
        else:
            return super().handle_return(handler, value)

class CommandDispatcher(EventDispatcher):
    """Event that goes off when a command is executed. This can be used
    to for instance keep a log of all the commands admins have used.

    """
    name = "command"
    
    def dispatch(self, caller, command, args):
        super().dispatch(caller, command, args)

class ClientCommandDispatcher(EventDispatcher):
    """Event that triggers with any client command. This overlaps with
    other events, such as "chat".

    """
    name = "client_command"
    
    def dispatch(self, player, cmd):
        ret = super().dispatch(player, cmd)
        if ret == False:
            return False

        ret = minqlx.COMMANDS.handle_input(player, cmd, minqlx.ClientCommandChannel(player))
        if ret == False:
            return False

        return self.return_value

    def handle_return(self, handler, value):
        """If a string was returned, continue execution, but we edit the
        command that's being executed along the chain of handlers.

        """
        if isinstance(value, str):
            player, cmd = self.args
            self.args = (player, value)
            self.return_value = value
        else:
            return super().handle_return(handler, value)

class ServerCommandDispatcher(EventDispatcher):
    """Event that triggers with any server command sent by the server.
    Does not go off when sending a server command with
    :func:`minqlx.send_server_command`.

    """
    name = "server_command"
    
    def dispatch(self, player, cmd):
        return super().dispatch(player, cmd)

    def handle_return(self, handler, value):
        """If a string was returned, continue execution, but we edit the
        command that's being sent along the chain of handlers.

        """
        if isinstance(value, str):
            player, cmd = self.args
            self.args = (player, value)
            self.return_value = value
        else:
            return super().handle_return(handler, value)

class FrameEventDispatcher(EventDispatcher):
    """Event that triggers every frame if the config has FrameEvent to True.
    Cannot be cancelled.

    """
    name = "frame"
    
    def dispatch(self):
        return super().dispatch()

class SetConfigstringDispatcher(EventDispatcher):
    """Event that triggers when the server tries to set a configstring. You can
    stop this event and use :func:`minqlx.set_configstring` to tamper with a
    configstring as it's being set.

    """
    name = "set_configstring"
    
    def dispatch(self, index, value):
        return super().dispatch(index, value)

    def handle_return(self, handler, value):
        """If a string was returned, continue execution, but we edit the
        configstring to the returned string. This allows multiple handlers
        to edit the configstring along the way before it's actually
        set by the QL engine.

        """
        if isinstance(value, str):
            index, old_value = self.args
            self.args = (index, value)
            self.return_value = value
        else:
            return super().handle_return(handler, value)

class ChatEventDispatcher(EventDispatcher):
    """Event that triggers with the "say" command. If the handler cancels it,
    the message will will also be cancelled.

    """
    name = "chat"
    
    def dispatch(self, player, msg, channel):
        ret = super().dispatch(player, msg, channel)
        if not ret: # Stop event if told to.
            return False
        
        return minqlx.COMMANDS.handle_input(player, msg, channel)

class UnloadDispatcher(EventDispatcher):
    """Event that triggers whenever a plugin is unloaded. Cannot be cancelled."""
    name = "unload"
    
    def dispatch(self, plugin):
        super().dispatch(plugin)

class PlayerConnectDispatcher(EventDispatcher):
    """Event that triggers whenever a player tries to connect. If the event
    is not stopped, it will let the player connect as usual. If it is stopped
    it will either display a generic ban message, or the message set with
    :func:`minqlx.set_ban_message`.

    """
    name = "player_connect"
    
    def dispatch(self, player):
        return super().dispatch(player)

    def handle_return(self, handler, value):
        """If a string was returned, stop execution of event, disallow
        the player from connecting, and display the returned string as
        a message to the player trying to connect.

        """
        if isinstance(value, str):
            return value
        else:
            return super().handle_return(handler, value)

class PlayerLoadedDispatcher(EventDispatcher):
    """Event that triggers whenever a player connects AND finishes loading.
    This means it'll trigger later than the "X connected" messages in-game.
    If the handler cancels it, the player will be kicked.

    """
    name = "player_loaded"
    
    def dispatch(self, player):
        return super().dispatch(player)

class PlayerDisonnectDispatcher(EventDispatcher):
    """Event that triggers whenever a player disconnects. Cannot be cancelled."""
    name = "player_disconnect"
    
    def dispatch(self, player, reason):
        return super().dispatch(player, reason)

class StatsDispatcher(EventDispatcher):
    """Event that triggers whenever the server sends stats over ZMQ."""
    name = "stats"
    
    def dispatch(self, stats):
        return super().dispatch(stats)

class VoteCalledDispatcher(EventDispatcher):
    name = "vote_called"

    def dispatch(self, player, vote, args):
        return super().dispatch(player, vote, args)

class VoteEndedDispatcher(EventDispatcher):
    name = "vote_ended"

    def dispatch(self, passed):
        super().dispatch(passed)

    def cancel(self):
        # Check if there's a current vote in the first place.
        cs = minqlx.get_configstring(9)
        if not cs:
            return

        res = _re_vote.match(cs)
        vote = res.group("cmd")
        args = res.group("args") if res.group("args") else ""
        votes = (int(minqlx.get_configstring(10)), int(minqlx.get_configstring(11)))
        # Return None if the vote's cancelled (like if the round starts before vote's over).
        super().trigger(votes, vote, args, None)

class VoteDispatcher(EventDispatcher):
    name = "vote"

    def dispatch(self, player, yes):
        return super().dispatch(yes)

class GameCountdownDispatcher(EventDispatcher):
    name = "game_countdown"
    
    def dispatch(self):
        return super().dispatch()

class GameStartDispatcher(EventDispatcher):
    name = "game_start"
    
    def dispatch(self, data):
        return super().dispatch(data)

class GameEndDispatcher(EventDispatcher):
    name = "game_end"
    
    def dispatch(self, data):
        return super().dispatch(data)

class RoundCountdownDispatcher(EventDispatcher):
    name = "round_countdown"
    
    def dispatch(self, round_number):
        return super().dispatch(round_number)

class RoundStartDispatcher(EventDispatcher):
    name = "round_start"
    
    def dispatch(self, round_number):
        return super().dispatch(round_number)

class RoundEndDispatcher(EventDispatcher):
    name = "round_end"
    
    def dispatch(self, data):
        return super().dispatch(data)

class TeamSwitchDispatcher(EventDispatcher):
    """For when a player switches teams. If cancelled,
    simply put the player back in the old team.

    If possible, consider using team_switch_attempt for a cleaner
    solution if you need to cancel the event."""
    name = "team_switch"
    
    def dispatch(self, player, old_team, new_team):
        return super().dispatch(player, old_team, new_team)

class TeamSwitchAttemptDispatcher(EventDispatcher):
    """For when a player attempts to join a team. Prevents the player from doing it when cancelled.

    When players click the Join Match button, it sends "team a" (with the "a" being "any",
    presumably), meaning the new_team argument can also be "any" in addition to all the
    other teams.

    """
    name = "team_switch_attempt"
    
    def dispatch(self, player, old_team, new_team):
        return super().dispatch(player, old_team, new_team)

class MapDispatcher(EventDispatcher):
    name = "map"
    
    def dispatch(self, mapname, factory):
        return super().dispatch(mapname, factory)

class NewGameDispatcher(EventDispatcher):
    name = "new_game"
    
    def dispatch(self):
        return super().dispatch()

class KillDispatcher(EventDispatcher):
    name = "kill"
    
    def dispatch(self, victim, killer, data):
        return super().dispatch(victim, killer, data)

class DeathDispatcher(EventDispatcher):
    name = "death"
    
    def dispatch(self, victim, killer, data):
        return super().dispatch(victim, killer, data)

EVENT_DISPATCHERS = EventDispatcherManager()
EVENT_DISPATCHERS.add_dispatcher(ConsolePrintDispatcher)
EVENT_DISPATCHERS.add_dispatcher(CommandDispatcher)
EVENT_DISPATCHERS.add_dispatcher(ClientCommandDispatcher)
EVENT_DISPATCHERS.add_dispatcher(ServerCommandDispatcher)
EVENT_DISPATCHERS.add_dispatcher(FrameEventDispatcher)
EVENT_DISPATCHERS.add_dispatcher(SetConfigstringDispatcher)
EVENT_DISPATCHERS.add_dispatcher(ChatEventDispatcher)
EVENT_DISPATCHERS.add_dispatcher(UnloadDispatcher)
EVENT_DISPATCHERS.add_dispatcher(PlayerConnectDispatcher)
EVENT_DISPATCHERS.add_dispatcher(PlayerLoadedDispatcher)
EVENT_DISPATCHERS.add_dispatcher(PlayerDisonnectDispatcher)
EVENT_DISPATCHERS.add_dispatcher(StatsDispatcher)
EVENT_DISPATCHERS.add_dispatcher(VoteCalledDispatcher)
EVENT_DISPATCHERS.add_dispatcher(VoteEndedDispatcher)
EVENT_DISPATCHERS.add_dispatcher(VoteDispatcher)
EVENT_DISPATCHERS.add_dispatcher(GameCountdownDispatcher)
EVENT_DISPATCHERS.add_dispatcher(GameStartDispatcher)
EVENT_DISPATCHERS.add_dispatcher(GameEndDispatcher)
EVENT_DISPATCHERS.add_dispatcher(RoundCountdownDispatcher)
EVENT_DISPATCHERS.add_dispatcher(RoundStartDispatcher)
EVENT_DISPATCHERS.add_dispatcher(RoundEndDispatcher)
EVENT_DISPATCHERS.add_dispatcher(TeamSwitchDispatcher)
EVENT_DISPATCHERS.add_dispatcher(TeamSwitchAttemptDispatcher)
EVENT_DISPATCHERS.add_dispatcher(MapDispatcher)
EVENT_DISPATCHERS.add_dispatcher(NewGameDispatcher)
EVENT_DISPATCHERS.add_dispatcher(KillDispatcher)
EVENT_DISPATCHERS.add_dispatcher(DeathDispatcher)
