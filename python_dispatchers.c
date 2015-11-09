#include <Python.h>

#include "pyminqlx.h"
#include "quake_common.h"

int in_clientconnect = 0;

int ClientCommandDispatcher(int client_id, const char* cmd) {
    int ret = 1;
    if (!client_command_handler)
        return ret; // No registered handler.
    
    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyObject_CallFunction(client_command_handler, "is", client_id, cmd);
    
    // Only change to 0 if we got False returned to us.
    if (result == NULL) {
        DebugError("PyObject_CallFunction() returned NULL.\n",
        		__FILE__, __LINE__, __func__);
        PyGILState_Release(gstate);
        return ret;
    }
    else if (PyBool_Check(result) && result == Py_False) {
        ret = 0;
    }
    
    Py_XDECREF(result);

    PyGILState_Release(gstate);
    return ret;
}

int ServerCommandDispatcher(int client_id, const char* cmd) {
    int ret = 1;
    if (!server_command_handler)
        return ret; // No registered handler.

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyObject_CallFunction(server_command_handler, "is", client_id, cmd);

    // Only change to 0 if we got False returned to us.
    if (result == NULL) {
        DebugError("PyObject_CallFunction() returned NULL.\n",
        		__FILE__, __LINE__, __func__);
        PyGILState_Release(gstate);
        return ret;
    }
    else if (PyBool_Check(result) && result == Py_False) {
        ret = 0;
    }

    Py_XDECREF(result);

    PyGILState_Release(gstate);
    return ret;
}

void FrameDispatcher(void) {
    if (!frame_handler)
        return; // No registered handler.

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyObject_CallObject(frame_handler, NULL);

    Py_XDECREF(result);

    PyGILState_Release(gstate);
    return;
}

char* ClientConnectDispatcher(int client_id, int is_bot) {
	char* ret = NULL;
	if (!client_connect_handler)
		return ret; // No registered handler.

	PyGILState_STATE gstate = PyGILState_Ensure();

	// Tell PyMinqlx_PlayerInfo it's OK to get player info for someone with CS_FREE.
	in_clientconnect = 1;
	PyObject* result = PyObject_CallFunction(client_connect_handler, "iO", client_id, is_bot ? Py_True : Py_False);
	in_clientconnect = 0;

	if (result == NULL)
		DebugError("PyObject_CallFunction() returned NULL.\n",
				__FILE__, __LINE__, __func__);
	else if (PyBool_Check(result) && result == Py_False)
		ret = "You are banned from this server.";
	else if (PyUnicode_Check(result))
		ret = PyUnicode_AsUTF8(result);

	Py_XDECREF(result);

	PyGILState_Release(gstate);
	return ret;
}

void ClientDisconnectDispatcher(int client_id, const char* reason) {
	if (!client_disconnect_handler)
		return; // No registered handler.

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyObject_CallFunction(client_disconnect_handler, "is", client_id, reason);
	if (result == NULL)
		DebugError("PyObject_CallFunction() returned NULL.\n",
				__FILE__, __LINE__, __func__);

	Py_XDECREF(result);

	PyGILState_Release(gstate);
	return;
}

// Does not trigger on bots.
int ClientLoadedDispatcher(int client_id) {
	int ret = 1;
	if (!client_loaded_handler)
		return ret; // No registered handler.

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyObject_CallFunction(client_loaded_handler, "i", client_id);

	// Only change to 0 if we got False returned to us.
	if (result == NULL) {
		DebugError("PyObject_CallFunction() returned NULL.\n",
				__FILE__, __LINE__, __func__);
		PyGILState_Release(gstate);
		return ret;
	}
	else if (PyBool_Check(result) && result == Py_False) {
		ret = 0;
	}

	Py_XDECREF(result);

	PyGILState_Release(gstate);
	return ret;
}

void NewGameDispatcher(int restart) {
	if (!new_game_handler)
		return; // No registered handler.

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyObject_CallFunction(new_game_handler, "O", restart ? Py_True : Py_False);

	if (result == NULL)
		DebugError("PyObject_CallFunction() returned NULL.\n", __FILE__, __LINE__, __func__);

	Py_XDECREF(result);

	PyGILState_Release(gstate);
	return;
}

char* SetConfigstringDispatcher(int index, char* value) {
	char* ret = value;
	if (!set_configstring_handler)
		return ret; // No registered handler.

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyObject_CallFunction(set_configstring_handler, "is", index, value);

	if (result == NULL)
		DebugError("PyObject_CallFunction() returned NULL.\n",
				__FILE__, __LINE__, __func__);
	else if (PyBool_Check(result) && result == Py_False)
		ret = NULL;
	else if (PyUnicode_Check(result))
		ret = PyUnicode_AsUTF8(result);

	Py_XDECREF(result);

	PyGILState_Release(gstate);
	return ret;
}

void RconDispatcher(const char* cmd) {
    if (!rcon_handler)
        return; // No registered handler.

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyObject_CallFunction(rcon_handler, "s", cmd);

    if (result == NULL)
        DebugError("PyObject_CallFunction() returned NULL.\n",
                __FILE__, __LINE__, __func__);
    Py_XDECREF(result);

    PyGILState_Release(gstate);
}

char* ConsolePrintDispatcher(char* text) {
    char* ret = text;
    if (!console_print_handler)
        return ret; // No registered handler.

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyObject_CallFunction(console_print_handler, "y", text);

    if (result == NULL)
        DebugError("PyObject_CallFunction() returned NULL.\n",
                __FILE__, __LINE__, __func__);
    else if (PyBool_Check(result) && result == Py_False)
        ret = NULL;
    else if (PyUnicode_Check(result))
        ret = PyUnicode_AsUTF8(result);

    Py_XDECREF(result);

    PyGILState_Release(gstate);
    return ret;
}
