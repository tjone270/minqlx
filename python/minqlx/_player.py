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

def _player(client_id):
    """A wrapper for minqlx.Player to make the output more usable."""
    info = minqlx.player_info(client_id)
    if info == None:
        return None
    
    d = {}
    for key in info:
        if key == "configstring":
            d.update(minqlx.parse_variables(info["configstring"]))
        elif key == "userinfo":
            userinfo = minqlx.parse_variables(info["userinfo"])
            if "name" in userinfo:
                del userinfo["name"]
            d.update(userinfo)
        else:
            d[key] = info[key]

    return d

def _players():
    """A wrapper for minqlx.Players to make the output more usable."""
    ret = []
    for player in minqlx.players_info():
        d = {}
        for key in player:
            if key == "configstring":
                d.update(minqlx.parse_variables(player["configstring"]))
            elif key == "userinfo":
                d.update(minqlx.parse_variables(player["userinfo"]))
            else:
                d[key] = player[key]
        ret.append(d)
    return ret

class NonexistentPlayerError(Exception):
    """An exception that is raised when a player that disconnected is being used
    as if the player were still present.

    """
    pass

class Player():
    """A class that represents a player on the server. As opposed to minqlbot,
    attributes are all the values from when the class was instantiated. This
    means for instance if a player is on the blue team when you check, but
    then moves to red, it will still be blue when you check a second time.
    To update it, use :meth:`~.Player.update`. Note that if you update it
    and the player has disconnected, it will raise a
    :exc:`minqlx.NonexistentPlayerError` exception.

    At the same time, that also means that if you access an attribute a lot, you should
    probably assign it to a temporary variable first.

    """
    def __init__(self, client_id, cvars_func=_player, player_dict=None):
        self._cvars_func = cvars_func
        self._valid = True
        # player_dict used for more efficient Plugin.players().
        if player_dict:
            self._id = player_dict["client_id"]
            self._cvars = player_dict
        else:
            self._id = client_id
            self._cvars = cvars_func(client_id)
            if not self._cvars:
                self._invalidate("Tried to initialize a Player instance of nonexistant player {}."
                    .format(client_id))

        self._steam_id = self._cvars["steam_id"]
        try:
            self._name = self._cvars["name"]
        except KeyError:
            self._name = self._cvars["n"]

    def __repr__(self):
        if not self._valid:
            return "{}(INVALID:'{}':{})".format(self.__class__.__name__,
                self.clean_name, self.steam_id)

        return "{}({}:'{}':{})".format(self.__class__.__name__, self._id,
            self.clean_name, self.steam_id)

    def __str__(self):
        return self.name

    def __contains__(self, key):
        return key in self.cvars

    def __getitem__(self, key):
        return self.cvars[key]

    def __eq__(self, other):
        if isinstance(other, type(self)):
            return self.steam_id == other.steam_id
        else:
            return self.steam_id == other

    def __ne__(self, other):
        return not self.__eq__(other)

    def update(self):
        """Update the player information with the latest data. If the player
        disconnected it will raise an exception and invalidates a player.
        The player's name and Steam ID can still be accessed after being 
        invalidated, but anything else will make it throw an exception too.

        :raises: minqlx.NonexistentPlayerError

        """
        self._cvars = self._cvars_func(self._id)

        if not self._cvars or self.steam_id != self._steam_id:
            self._invalidate()

        self._name = self._cvars["name"]

    def _invalidate(self, e="The player does not exist anymore. Did the player disconnect?"):
        self._valid = False
        raise NonexistentPlayerError(e)

    @property
    def cvars(self):
        if not self._valid:
            self._invalidate()

        return self._cvars.copy()

    @property
    def steam_id(self):
        return self._steam_id
    
    @property
    def id(self):
        return self._id

    @property
    def ip(self):
        return self["ip"]

    @property
    def clan(self):
        """The clan tag. Not actually supported by QL, but it used to be and
        fortunately the scoreboard still properly displays it if we manually
        set the configstring to use clan tags."""
        try:
            return self["cn"]
        except KeyError:
            return ""
    
    @property
    def name(self):
        return self._name + "^7"

    @name.setter
    def name(self, value):
        info = minqlx.parse_variables(minqlx.get_userinfo(self.id), ordered=True)
        info["name"] = value
        new_info = "\\".join(["{}\\{}".format(key, info[key]) for key in info])
        minqlx.client_command(self.id, "userinfo \"{}\"".format(new_info))
        self._name = value

    @property
    def clean_name(self):
        """Removes color tags from the name."""
        return re.sub(r"\^[0-9]", "", self.name)

    @property
    def qport(self):
        return int(self["qport"])
    
    @property
    def team(self):
        return minqlx.TEAMS[int(self["t"])]
    
    @property
    def colors(self):
        # Float because they can occasionally be floats for some reason.
        return float(self["color1"]), float(self["color2"])
    
    @property
    def model(self):
        return self["model"]

    @property
    def headmodel(self):
        return self["headmodel"]

    @property
    def state(self):
        """A string describing the connection state of a player.

        Possible values:
        - *connected* -- The player connected, but is currently loading the game.
        - *primed* -- The player was sent the necessary information to play, but has yet to send commands.
        - *active* -- The player finished loading and is actively sending commands to the server.

        In other words, if you need to make sure a player is in-game, check if ``player.state == "active"``.

        """
        return self["state"]

    @property
    def country(self):
        return self["country"]

    @property
    def valid(self):
        try:
            self["name"]
            return True
        except NonexistentPlayerError:
            return False

    def tell(self, msg):
        return minqlx.Plugin.tell(msg, self)

    def kick(self, reason=""):
        return minqlx.Plugin.kick(self, reason)

    def ban(self):
        return minqlx.Plugin.ban(self)

    def tempban(self):
        return minqlx.Plugin.tempban(self)

    def addadmin(self):
        return minqlx.Plugin.addadmin(self)

    def addmod(self):
        return minqlx.Plugin.addmod(self)

    def demote(self):
        return minqlx.Plugin.demote(self)

    def mute(self):
        return minqlx.Plugin.mute(self)

    def unmute(self):
        return minqlx.Plugin.unmute(self)

    def put(self, team):
        return minqlx.Plugin.put(self, team)

    def addscore(self, score):
        return minqlx.Plugin.addscore(self, score)

    def switch(self, other_player):
        return minqlx.Plugin.switch(self, other_player)

    def slap(self, damage=0):
        return minqlx.Plugin.slap(self, damage)

    def slay(self):
        return minqlx.Plugin.slay(self)

    @classmethod
    def all_players(cls):
        return [cls(pd["client_id"], player_dict=pd) for pd in _players()]
