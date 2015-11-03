#include <Python.h>
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
		{NULL, NULL}
};

/*
 * ================================================================
 *                    player_info/players_info
 * ================================================================
*/

static PyObject* makePlayerDict(int client_id) {
	PyObject* ret = PyDict_New();
	if (!ret) {
        DebugError("Failed to create a new dictionary.\n",
                __FILE__, __LINE__, __func__);
        Py_RETURN_NONE;
    }

	// CLIENT ID
	PyObject* cid = PyLong_FromLong(client_id);
	if (PyDict_SetItemString(ret, "client_id", cid) == -1) {
		Py_DECREF(ret);
		Py_RETURN_NONE;
	}
	Py_DECREF(cid);

	// STATE
	PyObject* state;
	if (svs->clients[client_id].state == CS_FREE)
		state = PyUnicode_FromString("free");
	else if (svs->clients[client_id].state == CS_ZOMBIE)
		state = PyUnicode_FromString("zombie");
	else if (svs->clients[client_id].state == CS_CONNECTED)
		state = PyUnicode_FromString("connected");
	else if (svs->clients[client_id].state == CS_PRIMED)
		state = PyUnicode_FromString("primed");
	else if (svs->clients[client_id].state == CS_ACTIVE)
		state = PyUnicode_FromString("active");
	else {
		state = PyUnicode_FromString("unknown");
		DebugError("Got an unknown connection state: %d\n",
				__FILE__, __LINE__, __func__, svs->clients[client_id].state);
	}

	if (PyDict_SetItemString(ret, "state", state) == -1) {
		DebugError("Failed to add 'state' to the dictionary.\n",
				__FILE__, __LINE__, __func__);
		Py_DECREF(ret);
		Py_RETURN_NONE;
	}
	Py_DECREF(state);

	// USERINFO
	PyObject* userinfo = PyUnicode_FromString(svs->clients[client_id].userinfo);
	if (PyDict_SetItemString(ret, "userinfo", userinfo) == -1) {
		DebugError("Failed to add 'userinfo' to the dictionary.\n",
				__FILE__, __LINE__, __func__);
		Py_DECREF(ret);
		Py_RETURN_NONE;
	}
	Py_DECREF(userinfo);

    // NAME
    PyObject* name = PyUnicode_FromString(svs->clients[client_id].name);
    if (PyDict_SetItemString(ret, "name", name) == -1) {
        DebugError("Failed to add 'name' to the dictionary.\n",
                __FILE__, __LINE__, __func__);
        Py_DECREF(ret);
        Py_RETURN_NONE;
    }
    Py_DECREF(name);

	// STEAM ID
	PyObject* steam_id = PyLong_FromLongLong(svs->clients[client_id].steam_id);
	if (PyDict_SetItemString(ret, "steam_id", steam_id) == -1) {
		DebugError("Failed to add 'steam_id' to the dictionary.\n",
				__FILE__, __LINE__, __func__);
		Py_DECREF(ret);
		Py_RETURN_NONE;
	}
	Py_DECREF(steam_id);


	// Some info might not always be available, like if someone is trying to
	// connect, but has not yet been allowed to join. No need to return
	// None if that is the case. Instead return incomplete dict.
	if (svs->clients[client_id].state != CS_FREE) {
		// CONFIGSTRING
		PyObject* configstring = PyUnicode_FromString(sv->configstrings[529 + client_id]);
		if (PyDict_SetItemString(ret, "configstring", configstring) == -1) {
			DebugError("Failed to add 'configstring' to the dictionary.\n",
					__FILE__, __LINE__, __func__);
			Py_DECREF(ret);
			Py_RETURN_NONE;
		}
		Py_DECREF(configstring);

		// PRIVILEGES
		PyObject* priv = Py_None;
		if (g_entities[client_id].client->privileges == 0)
			{}
		else if (g_entities[client_id].client->privileges == 1)
			priv = PyUnicode_FromString("mod");
		else if (g_entities[client_id].client->privileges == 2)
			priv = PyUnicode_FromString("admin");
		if (PyDict_SetItemString(ret, "privileges", priv) == -1) {
			DebugError("Failed to add 'privileges' to the dictionary.\n",
					__FILE__, __LINE__, __func__);
			Py_DECREF(ret);
			Py_RETURN_NONE;
		}
	}

	return ret;
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
    else if (!in_clientconnect && (svs->clients[i].state == CS_FREE)) {
        #ifndef NDEBUG
        DebugPrint("WARNING: PyMinqlx_PlayerInfo called for CS_FREE client %d.\n", i);
        #endif
        Py_RETURN_NONE;
    }

    return makePlayerDict(i);
}

static PyObject* PyMinqlx_PlayersInfo(PyObject* self, PyObject* args) {
	PyObject* ret = PyList_New(0);

	for (int i = 0; i < sv_maxclients->integer; i++) {
		if (svs->clients[i].state == CS_FREE) {
			continue;
		}

		if (PyList_Append(ret, makePlayerDict(i)) == -1)
			return NULL; // PyList_Append sets an error for us.
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
    else if (!in_clientconnect && (svs->clients[i].state == CS_FREE || svs->clients[i].state == CS_ZOMBIE))
        Py_RETURN_NONE;

    return PyUnicode_FromString(svs->clients[i].userinfo);
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
        SV_SendServerCommand(NULL, "%s\n", cmd); // Send to all.
        Py_RETURN_TRUE;
    }
    else if (PyLong_Check(client_id)) {
        i = PyLong_AsLong(client_id);
        if (i >= 0 && i < sv_maxclients->integer) {
            if (svs->clients[i].state != CS_ACTIVE)
                Py_RETURN_FALSE;
            else {
                SV_SendServerCommand(&svs->clients[i], "%s\n", cmd);
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
			SV_ExecuteClientCommand(&svs->clients[i], cmd, qtrue);
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
			SV_DropClient(&svs->clients[i], "was kicked.");
		}
		else if (PyUnicode_Check(reason)) {
			SV_DropClient(&svs->clients[i], PyUnicode_AsUTF8(reason));
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

    Com_Printf("%s\n", text);

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
    return PyUnicode_FromString(csbuffer);
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
				g_entities[i].client->pers.voteType = VOTE_YES;
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
