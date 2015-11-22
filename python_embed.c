#include <Python.h>
#include <structmember.h>
#include <structseq.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "pyminqlx.h"
#include "quake_common.h"
#include "patterns.h"
#include "common.h"

PyObject* client_command_handler = NULL;
PyObject* server_command_handler = NULL;
PyObject* client_connect_handler = NULL;
PyObject* client_loaded_handler = NULL;
PyObject* client_disconnect_handler = NULL;
PyObject* frame_handler = NULL;
PyObject* custom_command_handler = NULL;
PyObject* new_game_handler = NULL;
PyObject* set_configstring_handler = NULL;
PyObject* rcon_handler = NULL;
PyObject* console_print_handler = NULL;

static PyThreadState* mainstate;
static int initialized = 0;

/*
 * If we don't do this, we'll simply get NULL from both PyRun_File*()
 * and PyRun_String*() if the module has an error in it. It's ugly as
 * fuck, but other than doing this, I have no idea how to extract the
 * traceback. The documentation or Google doesn't help much either.
*/
static const char loader[] = "import traceback\n" \
    "try:\n" \
    "  import sys\n" \
	"  sys.path.append('" CORE_MODULE "')\n" \
    "  sys.path.append('.')\n" \
	"  import minqlx\n" \
	"  minqlx.initialize()\n" \
    "  ret = True\n" \
    "except Exception as e:\n" \
    "  e = traceback.format_exc().rstrip('\\n')\n" \
    "  for line in e.split('\\n'): print(line)\n" \
    "  ret = False\n";

/*
 * The number of handlers was getting large, so instead of a bunch of
 * else ifs in register_handler, I'm using a struct to hold name-handler
 * pairs and iterate over them instead.
 */
static handler_t handlers[] = {
		{"client_command", 		&client_command_handler},
		{"server_command", 		&server_command_handler},
		{"frame", 				&frame_handler},
		{"player_connect", 		&client_connect_handler},
		{"player_loaded", 		&client_loaded_handler},
		{"player_disconnect", 	&client_disconnect_handler},
		{"custom_command", 		&custom_command_handler},
		{"new_game",			&new_game_handler},
		{"set_configstring", 	&set_configstring_handler},
        {"rcon",                &rcon_handler},
        {"console_print",       &console_print_handler},
		{NULL, NULL}
};

/*
 * ================================================================
 *                      Struct Sequences
 * ================================================================
*/

// Players
static PyTypeObject player_info_type = {0};

static PyStructSequence_Field player_info_fields[] = {
    {"client_id", "The player's client ID."},
    {"name", "The player's name."},
    {"connection_state", "The player's connection state."},
    {"userinfo", "The player's userinfo."},
    {"steam_id", "The player's 64-bit representation of the Steam ID."},
    {"team", "The player's team."},
    {"privileges", "The player's privileges."},
    {NULL}
};

static PyStructSequence_Desc player_info_desc = {
    "PlayerInfo",
    "Information about a player, such as Steam ID, name, client ID, and whatnot.",
    player_info_fields,
    7
};

// Player state
static PyTypeObject player_state_type = {0};

static PyStructSequence_Field player_state_fields[] = {
    {"is_alive", "Whether the player's alive or not."},
    {"position", "The player's position."},
    {"velocity", "The player's velocity."},
    {"health", "The player's health."},
    {"armor", "The player's armor."},
    {"noclip", "Whether the player has noclip or not."},
    {"weapon", "The weapon the player is currently using."},
    {"weapons", "The player's weapons."},
    {"ammo", "The player's weapon ammo."},
    {NULL}
};

static PyStructSequence_Desc player_state_desc = {
    "PlayerState",
    "Information about a player's state in the game.",
    player_state_fields,
    9
};

// Stats
static PyTypeObject player_stats_type = {0};

static PyStructSequence_Field player_stats_fields[] = {
    {"score", "The player's primary score."},
    {"kills", "The player's number of kills."},
    {"deaths", "The player's number of deaths."},
    {"damage_dealt", "The player's total damage dealt."},
    {"damage_taken", "The player's total damage taken."},
    {NULL}
};

static PyStructSequence_Desc player_stats_desc = {
    "PlayerStats",
    "A player's score and some basic stats.",
    player_stats_fields,
    5
};

// Vectors
static PyTypeObject vector3_type = {0};

static PyStructSequence_Field vector3_fields[] = {
    {"x", NULL},
    {"y", NULL},
    {"z", NULL},
    {NULL}
};

static PyStructSequence_Desc vector3_desc = {
    "Vector3",
    "A three-dimensional vector.",
    vector3_fields,
    3
};

// Weapons
static PyTypeObject weapons_type = {0};

static PyStructSequence_Field weapons_fields[] = {
    {"g", NULL}, {"mg", NULL}, {"sg", NULL},
    {"gl", NULL}, {"rl", NULL}, {"lg", NULL},
    {"rg", NULL}, {"pg", NULL}, {"bfg", NULL},
    {"gh", NULL}, {"ng", NULL}, {"pl", NULL},
    {"cg", NULL}, {"hmg", NULL}, {"hands", NULL},
    {NULL}
};

static PyStructSequence_Desc weapons_desc = {
    "Weapons",
    "A struct sequence containing all the weapons in the game.",
    weapons_fields,
    15
};

/*
 * ================================================================
 *                    player_info/players_info
 * ================================================================
*/

static PyObject* makePlayerTuple(int client_id) {
    PyObject *name, *team, *priv;
    PyObject* cid = PyLong_FromLongLong(client_id);

    if (g_entities[client_id].client != NULL) {
        name = PyUnicode_DecodeUTF8(g_entities[client_id].client->pers.netname,
            strlen(g_entities[client_id].client->pers.netname), "ignore");

        if (g_entities[client_id].client->pers.connected == CON_DISCONNECTED)
            team = PyLong_FromLongLong(TEAM_SPECTATOR); // Set team to spectator if not yet connected.
        else
            team = PyLong_FromLongLong(g_entities[client_id].client->sess.sessionTeam);

        priv = PyLong_FromLongLong(g_entities[client_id].client->sess.privileges);
    }
    else {
        name = PyUnicode_FromString("");
        team = PyLong_FromLongLong(TEAM_SPECTATOR);
        priv = PyLong_FromLongLong(-1);
    }

    PyObject* state = PyLong_FromLongLong(svs->clients[client_id].state);
    PyObject* userinfo = PyUnicode_DecodeUTF8(svs->clients[client_id].userinfo, strlen(svs->clients[client_id].userinfo), "ignore");
    PyObject* steam_id = PyLong_FromLongLong(svs->clients[client_id].steam_id);
    
    PyObject* info = PyStructSequence_New(&player_info_type);
    PyStructSequence_SetItem(info, 0, cid);
    PyStructSequence_SetItem(info, 1, name);
    PyStructSequence_SetItem(info, 2, state);
    PyStructSequence_SetItem(info, 3, userinfo);
    PyStructSequence_SetItem(info, 4, steam_id);
    PyStructSequence_SetItem(info, 5, team);
    PyStructSequence_SetItem(info, 6, priv);

    return info;
}

static PyObject* PyMinqlx_PlayerInfo(PyObject* self, PyObject* args) {
    int i;
    if (!PyArg_ParseTuple(args, "i:player", &i))
        return NULL;
    
    if (i < 0 || i >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
        
    }
    else if (allow_free_client != i && svs->clients[i].state == CS_FREE) {
        #ifndef NDEBUG
        DebugPrint("WARNING: PyMinqlx_PlayerInfo called for CS_FREE client %d.\n", i);
        #endif
        Py_RETURN_NONE;
    }

    return makePlayerTuple(i);
}

static PyObject* PyMinqlx_PlayersInfo(PyObject* self, PyObject* args) {
	PyObject* ret = PyList_New(sv_maxclients->integer);

	for (int i = 0; i < sv_maxclients->integer; i++) {
		if (svs->clients[i].state == CS_FREE) {
			if (PyList_SetItem(ret, i, Py_None) == -1)
                        return NULL;
            Py_INCREF(Py_None);
            continue;
		}

		if (PyList_SetItem(ret, i, makePlayerTuple(i)) == -1)
			return NULL;
	}

    return ret;
}

/*
 * ================================================================
 *                          get_userinfo
 * ================================================================
*/

static PyObject* PyMinqlx_GetUserinfo(PyObject* self, PyObject* args) {
    int i;
    if (!PyArg_ParseTuple(args, "i:get_userinfo", &i))
        return NULL;

    if (i < 0 || i >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;

    }
    else if (allow_free_client != i && svs->clients[i].state == CS_FREE)
        Py_RETURN_NONE;

    return PyUnicode_DecodeUTF8(svs->clients[i].userinfo, strlen(svs->clients[i].userinfo), "ignore");
}

/*
 * ================================================================
 *                       send_server_command
 * ================================================================
*/

static PyObject* PyMinqlx_SendServerCommand(PyObject* self, PyObject* args) {
    PyObject* client_id;
    int i;
    char* cmd;
    if (!PyArg_ParseTuple(args, "Os:send_server_command", &client_id, &cmd))
        return NULL;
    
    if (client_id == Py_None) {
        My_SV_SendServerCommand(NULL, "%s\n", cmd); // Send to all.
        Py_RETURN_TRUE;
    }
    else if (PyLong_Check(client_id)) {
        i = PyLong_AsLong(client_id);
        if (i >= 0 && i < sv_maxclients->integer) {
            if (svs->clients[i].state != CS_ACTIVE)
                Py_RETURN_FALSE;
            else {
                My_SV_SendServerCommand(&svs->clients[i], "%s\n", cmd);
                Py_RETURN_TRUE;
            }
        }
    }
    
    PyErr_Format(PyExc_ValueError,
                 "client_id needs to be a number from 0 to %d, or None.",
                 sv_maxclients->integer);
    return NULL;
}

/*
 * ================================================================
 *                          client_command
 * ================================================================
*/

static PyObject* PyMinqlx_ClientCommand(PyObject* self, PyObject* args) {
    int i;
    char* cmd;
    if (!PyArg_ParseTuple(args, "is:client_command", &i, &cmd))
        return NULL;

	if (i >= 0 && i < sv_maxclients->integer) {
		if (svs->clients[i].state == CS_FREE || svs->clients[i].state == CS_ZOMBIE)
			Py_RETURN_FALSE;
		else {
			My_SV_ExecuteClientCommand(&svs->clients[i], cmd, qtrue);
			Py_RETURN_TRUE;
		}
	}

    PyErr_Format(PyExc_ValueError,
                 "client_id needs to be a number from 0 to %d, or None.",
                 sv_maxclients->integer);
    return NULL;
}

/*
 * ================================================================
 *                         console_command
 * ================================================================
*/

static PyObject* PyMinqlx_ConsoleCommand(PyObject* self, PyObject* args) {
    char* cmd;
    if (!PyArg_ParseTuple(args, "s:console_command", &cmd))
        return NULL;

    Cbuf_ExecuteText(EXEC_INSERT, cmd);

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                           get_cvar
 * ================================================================
*/

static PyObject* PyMinqlx_GetCvar(PyObject* self, PyObject* args) {
    char* name;
    if (!PyArg_ParseTuple(args, "s:get_cvar", &name))
        return NULL;

    cvar_t* cvar = Cvar_FindVar(name);
    if (cvar) {
    	return PyUnicode_FromString(cvar->string);
    }

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                           set_cvar
 * ================================================================
*/

static PyObject* PyMinqlx_SetCvar(PyObject* self, PyObject* args) {
    char *name, *value;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "ss|i:set_cvar", &name, &value, &flags))
        return NULL;

    cvar_t* var = Cvar_FindVar(name);
    if (!var) {
        Cvar_Get(name, value, flags);
        Py_RETURN_TRUE;
    }
    
    Cvar_Set2(name, value, qfalse);
    Py_RETURN_FALSE;
}

/*
 * ================================================================
 *                           set_cvar_limit
 * ================================================================
*/

static PyObject* PyMinqlx_SetCvarLimit(PyObject* self, PyObject* args) {
    char *name, *value, *min, *max;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "ssss|i:set_cvar_limit", &name, &value, &min, &max, &flags))
        return NULL;

    Cvar_GetLimit(name, value, min, max, flags);
    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                             kick
 * ================================================================
*/

static PyObject* PyMinqlx_Kick(PyObject* self, PyObject* args) {
    int i;
    PyObject* reason;
    if (!PyArg_ParseTuple(args, "iO:kick", &i, &reason))
        return NULL;

	if (i >= 0 && i < sv_maxclients->integer) {
		if (svs->clients[i].state != CS_ACTIVE) {
			PyErr_Format(PyExc_ValueError,
					"client_id must be None or the ID of an active player.");
			return NULL;
		}
		else if (reason == Py_None || (PyUnicode_Check(reason) && PyUnicode_AsUTF8(reason)[0] == 0)) {
			// Default kick message for None or empty strings.
			My_SV_DropClient(&svs->clients[i], "was kicked.");
		}
		else if (PyUnicode_Check(reason)) {
			My_SV_DropClient(&svs->clients[i], PyUnicode_AsUTF8(reason));
		}
	}
	else {
		PyErr_Format(PyExc_ValueError,
				"client_id needs to be a number from 0 to %d, or None.",
				sv_maxclients->integer);
		return NULL;
	}

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                          console_print
 * ================================================================
*/

static PyObject* PyMinqlx_ConsolePrint(PyObject* self, PyObject* args) {
    char* text;
    if (!PyArg_ParseTuple(args, "s:console_print", &text))
        return NULL;

    My_Com_Printf("%s\n", text);

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                          get_configstring
 * ================================================================
*/

static PyObject* PyMinqlx_GetConfigstring(PyObject* self, PyObject* args) {
    int i;
    char csbuffer[4096];
    if (!PyArg_ParseTuple(args, "i:get_configstring", &i))
        return NULL;
    else if (i < 0 || i > MAX_CONFIGSTRINGS) {
		PyErr_Format(PyExc_ValueError,
						 "index needs to be a number from 0 to %d.",
						 MAX_CONFIGSTRINGS);
		return NULL;
	}

    SV_GetConfigstring(i, csbuffer, sizeof(csbuffer));
    return PyUnicode_DecodeUTF8(csbuffer, strlen(csbuffer), "ignore");
}

/*
 * ================================================================
 *                          set_configstring
 * ================================================================
*/

static PyObject* PyMinqlx_SetConfigstring(PyObject* self, PyObject* args) {
    int i;
    char* cs;
    if (!PyArg_ParseTuple(args, "is:set_configstring", &i, &cs))
        return NULL;
    else if (i < 0 || i > MAX_CONFIGSTRINGS) {
    	PyErr_Format(PyExc_ValueError,
    	                 "index needs to be a number from 0 to %d.",
						 MAX_CONFIGSTRINGS);
		return NULL;
    }

    SV_SetConfigstring(i, cs);

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                          force_vote
 * ================================================================
*/

static PyObject* PyMinqlx_ForceVote(PyObject* self, PyObject* args) {
	int pass;
    if (!PyArg_ParseTuple(args, "p:force_vote", &pass))
        return NULL;

    if (!level->voteTime) {
    	// No active vote.
    	Py_RETURN_FALSE;
    }
    else if (pass && level->voteTime) {
    	// We tell the server every single client voted yes, making it pass in the next G_RunFrame.
		for (int i = 0; i < sv_maxclients->integer; i++) {
			if (svs->clients[i].state == CS_ACTIVE)
				g_entities[i].client->pers.voteState = VOTE_YES;
		}
    }
    else if (!pass && level->voteTime) {
    	// If we tell the server the vote is over, it'll fail it right away.
		level->voteTime -= 30000;
    }

    Py_RETURN_TRUE;
}

/*
 * ================================================================
 *                       add_console_command
 * ================================================================
*/

static PyObject* PyMinqlx_AddConsoleCommand(PyObject* self, PyObject* args) {
    char* cmd;
    if (!PyArg_ParseTuple(args, "s:add_console_command", &cmd))
        return NULL;

    Cmd_AddCommand(cmd, PyCommand);

    Py_RETURN_NONE;
}

/*
 * ================================================================
 *                         register_handler
 * ================================================================
*/

static PyObject* PyMinqlx_RegisterHandler(PyObject* self, PyObject* args) {
    char* event;
    PyObject* new_handler;

    if (!PyArg_ParseTuple(args, "sO:register_handler", &event, &new_handler)) {
    	return NULL;
    }
    else if (new_handler != Py_None && !PyCallable_Check(new_handler)) {
		PyErr_SetString(PyExc_TypeError, "The handler must be callable.");
		return NULL;
	}

	for (handler_t* h = handlers; h->name; h++) {
		if (!strcmp(h->name, event)) {
			Py_XDECREF(*h->handler);
			if (new_handler == Py_None)
				*h->handler = NULL;
			else {
				*h->handler = new_handler;
				Py_INCREF(new_handler);
			}

			Py_RETURN_NONE;
		}
	}

	PyErr_SetString(PyExc_ValueError, "Invalid event.");
	return NULL;
}

/*
 * ================================================================
 *                          player_state
 * ================================================================
*/

static PyObject* PyMinqlx_PlayerState(PyObject* self, PyObject* args) {
    int client_id;

    if (!PyArg_ParseTuple(args, "i:player_state", &client_id))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_NONE;

    PyObject* state = PyStructSequence_New(&player_state_type);
    PyStructSequence_SetItem(state, 0, PyBool_FromLong(g_entities[client_id].client->ps.pm_type == 0));

    PyObject* pos = PyStructSequence_New(&vector3_type);
    PyStructSequence_SetItem(pos, 0,
        PyFloat_FromDouble(g_entities[client_id].client->ps.origin[0]));
    PyStructSequence_SetItem(pos, 1,
        PyFloat_FromDouble(g_entities[client_id].client->ps.origin[1]));
    PyStructSequence_SetItem(pos, 2,
        PyFloat_FromDouble(g_entities[client_id].client->ps.origin[2]));
    PyStructSequence_SetItem(state, 1, pos);

    PyObject* vel = PyStructSequence_New(&vector3_type);
    PyStructSequence_SetItem(vel, 0,
        PyFloat_FromDouble(g_entities[client_id].client->ps.velocity[0]));
    PyStructSequence_SetItem(vel, 1,
        PyFloat_FromDouble(g_entities[client_id].client->ps.velocity[1]));
    PyStructSequence_SetItem(vel, 2,
        PyFloat_FromDouble(g_entities[client_id].client->ps.velocity[2]));
    PyStructSequence_SetItem(state, 2, vel);

    PyStructSequence_SetItem(state, 3, PyLong_FromLongLong(g_entities[client_id].health));
    PyStructSequence_SetItem(state, 4, PyLong_FromLongLong(g_entities[client_id].client->ps.stats[STAT_ARMOR]));
    PyStructSequence_SetItem(state, 5, PyBool_FromLong(g_entities[client_id].client->noclip));
    PyStructSequence_SetItem(state, 6, PyLong_FromLongLong(g_entities[client_id].client->ps.weapon));

    // Get weapons and ammo count.
    PyObject* weapons = PyStructSequence_New(&weapons_type);
    PyObject* ammo = PyStructSequence_New(&weapons_type);
    for (int i = 0; i < 15; i++) {
        PyStructSequence_SetItem(weapons, i, PyBool_FromLong(g_entities[client_id].client->ps.stats[STAT_WEAPONS] & (1 << (i+1))));
        PyStructSequence_SetItem(ammo, i, PyLong_FromLongLong(g_entities[client_id].client->ps.ammo[i+1]));
    }
    PyStructSequence_SetItem(state, 7, weapons);  
    PyStructSequence_SetItem(state, 8, ammo);

    return state;
}

/*
 * ================================================================
 *                          player_stats
 * ================================================================
*/

static PyObject* PyMinqlx_PlayerStats(PyObject* self, PyObject* args) {
    int client_id;

    if (!PyArg_ParseTuple(args, "i:player_stats", &client_id))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_NONE;

    PyObject* stats = PyStructSequence_New(&player_stats_type);
    PyStructSequence_SetItem(stats, 0, PyLong_FromLongLong(g_entities[client_id].client->ps.persistant[0]));
    PyStructSequence_SetItem(stats, 1, PyLong_FromLongLong(g_entities[client_id].client->expandedStats.numKills));
    PyStructSequence_SetItem(stats, 2, PyLong_FromLongLong(g_entities[client_id].client->expandedStats.numDeaths));
    PyStructSequence_SetItem(stats, 3, PyLong_FromLongLong(g_entities[client_id].client->expandedStats.totalDamageDealt));
    PyStructSequence_SetItem(stats, 4, PyLong_FromLongLong(g_entities[client_id].client->expandedStats.totalDamageTaken));

    return stats;
}

/*
 * ================================================================
 *                          set_position
 * ================================================================
*/

static PyObject* PyMinqlx_SetPosition(PyObject* self, PyObject* args) {
    int client_id;
    PyObject* new_position;

    if (!PyArg_ParseTuple(args, "iO:set_position", &client_id, &new_position))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;
    else if (!PyObject_TypeCheck(new_position, &vector3_type)) {
        PyErr_Format(PyExc_ValueError, "Argument must be of type minqlx.Vector3.");
        return NULL;
    }

    g_entities[client_id].client->ps.origin[0] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_position, 0));
    g_entities[client_id].client->ps.origin[1] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_position, 1));
    g_entities[client_id].client->ps.origin[2] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_position, 2));

    Py_RETURN_TRUE;
}

/*
 * ================================================================
 *                          set_velocity
 * ================================================================
*/

static PyObject* PyMinqlx_SetVelocity(PyObject* self, PyObject* args) {
    int client_id;
    PyObject* new_velocity;

    if (!PyArg_ParseTuple(args, "iO:set_velocity", &client_id, &new_velocity))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;
    else if (!PyObject_TypeCheck(new_velocity, &vector3_type)) {
        PyErr_Format(PyExc_ValueError, "Argument must be of type minqlx.Vector3.");
        return NULL;
    }

    g_entities[client_id].client->ps.velocity[0] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_velocity, 0));
    g_entities[client_id].client->ps.velocity[1] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_velocity, 1));
    g_entities[client_id].client->ps.velocity[2] =
        (float)PyFloat_AsDouble(PyStructSequence_GetItem(new_velocity, 2));

    Py_RETURN_TRUE;
}

/*
* ================================================================
*                             noclip
* ================================================================
*/

static PyObject* PyMinqlx_NoClip(PyObject* self, PyObject* args) {
    int client_id, activate;
    if (!PyArg_ParseTuple(args, "ip:noclip", &client_id, &activate))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;

    if ((activate && g_entities[client_id].client->noclip) || (!activate && !g_entities[client_id].client->noclip)) {
        // Change was made.
        Py_RETURN_FALSE;
    }

    g_entities[client_id].client->noclip = activate ? qtrue : qfalse;
    Py_RETURN_TRUE;
}

/*
* ================================================================
*                           set_health
* ================================================================
*/

static PyObject* PyMinqlx_SetHealth(PyObject* self, PyObject* args) {
    int client_id, health;
    if (!PyArg_ParseTuple(args, "ii:set_health", &client_id, &health))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;

    g_entities[client_id].health = health;
    Py_RETURN_TRUE;
}

/*
* ================================================================
*                           set_armor
* ================================================================
*/

static PyObject* PyMinqlx_SetArmor(PyObject* self, PyObject* args) {
    int client_id, armor;
    if (!PyArg_ParseTuple(args, "ii:set_armor", &client_id, &armor))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;

    g_entities[client_id].client->ps.stats[STAT_ARMOR] = armor;
    Py_RETURN_TRUE;
}

/*
* ================================================================
*                           set_weapons
* ================================================================
*/

static PyObject* PyMinqlx_SetWeapons(PyObject* self, PyObject* args) {
    int client_id, weapons;
    if (!PyArg_ParseTuple(args, "ii:set_weapons", &client_id, &weapons))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;

    g_entities[client_id].client->ps.stats[STAT_WEAPONS] = weapons;
    Py_RETURN_TRUE;
}

/*
* ================================================================
*                           set_ammo
* ================================================================
*/

static PyObject* PyMinqlx_SetAmmo(PyObject* self, PyObject* args) {
    int client_id, weapon, ammo;
    if (!PyArg_ParseTuple(args, "iii:set_ammo", &client_id, &weapon, &ammo))
        return NULL;
    else if (client_id < 0 || client_id >= sv_maxclients->integer) {
        PyErr_Format(PyExc_ValueError,
                     "client_id needs to be a number from 0 to %d.",
                     sv_maxclients->integer);
        return NULL;
    }
    else if (weapon < 0 || weapon > 15) {
        PyErr_Format(PyExc_ValueError, "weapon number needs to be a number from 0 to 15.");
        return NULL;
    }
    else if (!g_entities[client_id].client)
        Py_RETURN_FALSE;

    g_entities[client_id].client->ps.ammo[weapon] = ammo;
    Py_RETURN_TRUE;
}

/*
 * ================================================================
 *             Module definition and initialization
 * ================================================================
*/

static PyMethodDef minqlxMethods[] = {
    {"player_info", PyMinqlx_PlayerInfo, METH_VARARGS,
     "Returns a dictionary with information about a player by ID."},
	{"players_info", PyMinqlx_PlayersInfo, METH_NOARGS,
	 "Returns a list with dictionaries with information about all the players on the server."},
	{"get_userinfo", PyMinqlx_GetUserinfo, METH_VARARGS,
	 "Returns a string with a player's userinfo."},
    {"send_server_command", PyMinqlx_SendServerCommand, METH_VARARGS,
     "Sends a server command to either one specific client or all the clients."},
	{"client_command", PyMinqlx_ClientCommand, METH_VARARGS,
	 "Tells the server to process a command from a specific client."},
	{"console_command", PyMinqlx_ConsoleCommand, METH_VARARGS,
	 "Executes a command as if it was executed from the server console."},
	{"get_cvar", PyMinqlx_GetCvar, METH_VARARGS,
	 "Gets a cvar."},
	{"set_cvar", PyMinqlx_SetCvar, METH_VARARGS,
	 "Sets a cvar."},
    {"set_cvar_limit", PyMinqlx_SetCvarLimit, METH_VARARGS,
     "Sets a non-string cvar with a minimum and maximum value."},
	{"kick", PyMinqlx_Kick, METH_VARARGS,
	 "Kick a player and allowing the admin to supply a reason for it."},
	{"console_print", PyMinqlx_ConsolePrint, METH_VARARGS,
	 "Prints text on the console. If used during an RCON command, it will be printed in the player's console."},
	{"get_configstring", PyMinqlx_GetConfigstring, METH_VARARGS,
	 "Get a configstring."},
	{"set_configstring", PyMinqlx_SetConfigstring, METH_VARARGS,
	 "Sets a configstring and sends it to all the players on the server."},
	{"force_vote", PyMinqlx_ForceVote, METH_VARARGS,
	 "Forces the current vote to either fail or pass."},
	{"add_console_command", PyMinqlx_AddConsoleCommand, METH_VARARGS,
	 "Adds a console command that will be handled by Python code."},
    {"register_handler", PyMinqlx_RegisterHandler, METH_VARARGS,
     "Register an event handler. Can be called more than once per event, but only the last one will work."},
    {"player_state", PyMinqlx_PlayerState, METH_VARARGS,
     "Get information about the player's state in the game."},
    {"player_stats", PyMinqlx_PlayerStats, METH_VARARGS,
     "Get some player stats."},
    {"set_position", PyMinqlx_SetPosition, METH_VARARGS,
     "Sets a player's position vector."},
    {"set_velocity", PyMinqlx_SetVelocity, METH_VARARGS,
     "Sets a player's velocity vector."},
    {"noclip", PyMinqlx_NoClip, METH_VARARGS,
     "Sets noclip for a player."},
    {"set_health", PyMinqlx_SetHealth, METH_VARARGS,
     "Sets a player's health."},
    {"set_armor", PyMinqlx_SetArmor, METH_VARARGS,
     "Sets a player's armor."},
    {"set_weapons", PyMinqlx_SetWeapons, METH_VARARGS,
     "Sets a player's weapons."},
    {"set_ammo", PyMinqlx_SetAmmo, METH_VARARGS,
     "Sets a player's ammo."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef minqlxModule = {
    PyModuleDef_HEAD_INIT, "minqlx", NULL, -1, minqlxMethods,
    NULL, NULL, NULL, NULL
};

static PyObject* PyMinqlx_InitModule(void) {
    PyObject* module = PyModule_Create(&minqlxModule);
    
    // Set minqlx version.
    PyModule_AddStringConstant(module, "__version__", MINQLX_VERSION);
    
    // Set IS_DEBUG.
    #ifndef NDEBUG
    PyModule_AddObject(module, "DEBUG", Py_True);
    #else
    PyModule_AddObject(module, "DEBUG", Py_False);
    #endif
    
    // Set a bunch of constants. We set them here because if you define functions in Python that use module
    // constants as keyword defaults, we have to always make sure they're exported first, and fuck that.
    PyModule_AddIntMacro(module, RET_NONE);
    PyModule_AddIntMacro(module, RET_STOP);
    PyModule_AddIntMacro(module, RET_STOP_EVENT);
    PyModule_AddIntMacro(module, RET_STOP_ALL);
    PyModule_AddIntMacro(module, RET_USAGE);
    PyModule_AddIntMacro(module, PRI_HIGHEST);
    PyModule_AddIntMacro(module, PRI_HIGH);
    PyModule_AddIntMacro(module, PRI_NORMAL);
    PyModule_AddIntMacro(module, PRI_LOW);
    PyModule_AddIntMacro(module, PRI_LOWEST);

    // Cvar flags.
    PyModule_AddIntMacro(module, CVAR_ARCHIVE);
    PyModule_AddIntMacro(module, CVAR_USERINFO);
    PyModule_AddIntMacro(module, CVAR_SERVERINFO);
    PyModule_AddIntMacro(module, CVAR_SYSTEMINFO);
    PyModule_AddIntMacro(module, CVAR_INIT);
    PyModule_AddIntMacro(module, CVAR_LATCH);
    PyModule_AddIntMacro(module, CVAR_ROM);
    PyModule_AddIntMacro(module, CVAR_USER_CREATED);
    PyModule_AddIntMacro(module, CVAR_TEMP);
    PyModule_AddIntMacro(module, CVAR_CHEAT);
    PyModule_AddIntMacro(module, CVAR_NORESTART);

    // Privileges.
    PyModule_AddIntMacro(module, PRIV_NONE);
    PyModule_AddIntMacro(module, PRIV_MOD);
    PyModule_AddIntMacro(module, PRIV_ADMIN);
    PyModule_AddIntMacro(module, PRIV_ROOT);
    PyModule_AddIntMacro(module, PRIV_BANNED);

    // Connection states.
    PyModule_AddIntMacro(module, CS_FREE);
    PyModule_AddIntMacro(module, CS_ZOMBIE);
    PyModule_AddIntMacro(module, CS_CONNECTED);
    PyModule_AddIntMacro(module, CS_PRIMED);
    PyModule_AddIntMacro(module, CS_ACTIVE);

    // Teams.
    PyModule_AddIntMacro(module, TEAM_FREE);
    PyModule_AddIntMacro(module, TEAM_RED);
    PyModule_AddIntMacro(module, TEAM_BLUE);
    PyModule_AddIntMacro(module, TEAM_SPECTATOR);

    // Weapons.
    PyModule_AddIntMacro(module, WP_NONE);
    PyModule_AddIntMacro(module, WP_GAUNTLET);
    PyModule_AddIntMacro(module, WP_MACHINEGUN);
    PyModule_AddIntMacro(module, WP_SHOTGUN);
    PyModule_AddIntMacro(module, WP_GRENADE_LAUNCHER);
    PyModule_AddIntMacro(module, WP_ROCKET_LAUNCHER);
    PyModule_AddIntMacro(module, WP_LIGHTNING);
    PyModule_AddIntMacro(module, WP_RAILGUN);
    PyModule_AddIntMacro(module, WP_PLASMAGUN);
    PyModule_AddIntMacro(module, WP_BFG);
    PyModule_AddIntMacro(module, WP_GRAPPLING_HOOK);
    PyModule_AddIntMacro(module, WP_NAILGUN);
    PyModule_AddIntMacro(module, WP_PROX_LAUNCHER);
    PyModule_AddIntMacro(module, WP_CHAINGUN);
    PyModule_AddIntMacro(module, WP_HMG);
    PyModule_AddIntMacro(module, WP_HANDS);

    // Initialize struct sequence types.
    PyStructSequence_InitType(&player_info_type, &player_info_desc);
    PyStructSequence_InitType(&player_state_type, &player_state_desc);
    PyStructSequence_InitType(&player_stats_type, &player_stats_desc);
    PyStructSequence_InitType(&vector3_type, &vector3_desc);
    PyStructSequence_InitType(&weapons_type, &weapons_desc);
    Py_INCREF((PyObject*)&player_info_type);
    Py_INCREF((PyObject*)&player_state_type);
    Py_INCREF((PyObject*)&player_stats_type);
    Py_INCREF((PyObject*)&vector3_type);
    Py_INCREF((PyObject*)&weapons_type);
    // Add new types.
    PyModule_AddObject(module, "PlayerInfo", (PyObject*)&player_info_type);
    PyModule_AddObject(module, "PlayerState", (PyObject*)&player_state_type);
    PyModule_AddObject(module, "PlayerStats", (PyObject*)&player_stats_type);
    PyModule_AddObject(module, "Vector3", (PyObject*)&vector3_type);
    PyModule_AddObject(module, "Weapons", (PyObject*)&weapons_type);
    
    return module;
}

int PyMinqlx_IsInitialized(void) {
   return initialized; 
}

PyMinqlx_InitStatus_t PyMinqlx_Initialize(void) {
    if (PyMinqlx_IsInitialized()) {
        DebugPrint("%s was called while already initialized!\n", __func__);
        return PYM_ALREADY_INITIALIZED;
    }
    
    DebugPrint("Initializing Python...\n");
    Py_SetProgramName(PYTHON_FILENAME);
    PyImport_AppendInittab("_minqlx", &PyMinqlx_InitModule);
    Py_Initialize();
    PyEval_InitThreads();
    
    // Add the main module.
    PyObject* main_module = PyImport_AddModule("__main__");
    PyObject* main_dict = PyModule_GetDict(main_module);
    // Run script to load pyminqlx.
    PyObject* res = PyRun_String(loader, Py_file_input, main_dict, main_dict);
    if (res == NULL) {
		DebugPrint("PyRun_String() returned NULL. Did you modify the loader?\n");
		return PYM_MAIN_SCRIPT_ERROR;
	}
    PyObject* ret = PyDict_GetItemString(main_dict, "ret");
    Py_XDECREF(ret);
    Py_DECREF(res);
    if (ret == NULL) {
		DebugPrint("The loader script return value doesn't exist?\n");
		return PYM_MAIN_SCRIPT_ERROR;
	}
	else if (ret != Py_True) {
		// No need to print anything, since the traceback should be printed already.
		return PYM_MAIN_SCRIPT_ERROR;
	}
    
    mainstate = PyEval_SaveThread();
    initialized = 1;
    DebugPrint("Python initialized!\n");
    return PYM_SUCCESS;
}

PyMinqlx_InitStatus_t PyMinqlx_Finalize(void) {
    if (!PyMinqlx_IsInitialized()) {
        DebugPrint("%s was called before being initialized!\n", __func__);
        return PYM_NOT_INITIALIZED_ERROR;
    }
    
    for (handler_t* h = handlers; h->name; h++) {
		*h->handler = NULL;
	}

    PyEval_RestoreThread(mainstate);
    Py_Finalize();
    initialized = 0;
    
    return PYM_SUCCESS;
}
