#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "quake_common.h"
#include "common.h"

#ifndef NOPY
#include "pyminqlx.h"
#endif

uint64_t pending_slap = 0LL;
int slap_damages[64];
uint64_t pending_slay = 0LL;

void __cdecl SendServerCommand(void) {
    SV_SendServerCommand(NULL, "%s\n", Cmd_Args());
}

void __cdecl CenterPrint(void) {
    SV_SendServerCommand(NULL, "cp \"%s\"\n", Cmd_Args());
}

void __cdecl RegularPrint(void) {
    SV_SendServerCommand(NULL, "print \"%s\n\"\n", Cmd_Args());
}

void __cdecl Slap(void) {
    int argc = Cmd_Argc();
    if (argc < 2) {
        Com_Printf("Usage: %s <client_id> [damage]\n", Cmd_Argv(0));
        return;
    }
    int i = atoi(Cmd_Argv(1));
    if (i < 0 || i > sv_maxclients->integer) {
        Com_Printf("client_id must be a number between 0 and %d\n.", sv_maxclients->integer);
        return;
    }
    else if (argc > 2)
        slap_damages[i] = atoi(Cmd_Argv(2));
    else
        slap_damages[i] = 0;
    
    if (g_entities[i].inuse && g_entities[i].health > 0) {
        Com_Printf("Slapping...\n");
        if (slap_damages[i])
            SV_SendServerCommand(NULL, "print \"%s was slapped for %d damage!\n\"\n", svs->clients[i].name, slap_damages[i]);
        else
            SV_SendServerCommand(NULL, "print \"%s was slapped!\n\"\n", svs->clients[i].name);
        //SetPendingPlayer(&pending_slap, i);
        g_entities[i].client->ps.velocity[0] += RandomFloatWithNegative() * 200.0f;
        g_entities[i].client->ps.velocity[1] += RandomFloatWithNegative() * 200.0f;
        g_entities[i].client->ps.velocity[2] += 300.0f;
        g_entities[i].health -= slap_damages[i]; // Will be 0 if argument wasn't passed.
        if (g_entities[i].health > 0)
            G_AddEvent(&g_entities[i], EV_PAIN, 99); // 99 health = pain100_1.wav
        else
            G_AddEvent(&g_entities[i], EV_DEATH1, g_entities[i].s.number);
    }
    else
        Com_Printf("The player is currently not active.\n");
}

void __cdecl Slay(void) {
    int argc = Cmd_Argc();
    if (argc < 2) {
        Com_Printf("Usage: %s <client_id>\n", Cmd_Argv(0));
        return;
    }
    int i = atoi(Cmd_Argv(1));
    if (i < 0 || i > sv_maxclients->integer) {
        Com_Printf("client_id must be a number between 0 and %d\n.", sv_maxclients->integer);
        return;
    }
    else if (g_entities[i].inuse && g_entities[i].health > 0) {
        Com_Printf("Slaying player...\n");
        SV_SendServerCommand(NULL, "print \"%s was slain!\n\"\n", svs->clients[i].name);
        //SetPendingPlayer(&pending_slay, i);
        DebugPrint("Slaying '%s'!\n", svs->clients[i].name);
		g_entities[i].health = -40;
		G_AddEvent(&g_entities[i], EV_GIB_PLAYER, g_entities[i].s.number);
    }
    else
        Com_Printf("The player is currently not active.\n");
}

// These commands are usually not possible to use without being on the
// server as an admin, but we get around that by creating a fake
// client and calling the command as if the fake one did it.
void __cdecl AdminCommand(void) {
	if (ExecuteAdminCommand(-1, Cmd_ArgsFrom(0)))
		Com_Printf("Invalid admin command.\n");
}

#ifndef NOPY
void __cdecl PyCommand(void) {
	if (!frame_handler) {
	        return; // No registered handler.
	}
	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyObject_CallFunction(custom_command_handler, "s", Cmd_Args());
	if (result == Py_False) {
		Com_Printf("The command failed to be executed. pyminqlx found no handler.\n");
	}

	Py_XDECREF(result);
	PyGILState_Release(gstate);
}

void __cdecl RestartPython(void) {
    Com_Printf("Restarting Python...\n");
    if (PyMinqlx_IsInitialized())
    	PyMinqlx_Finalize();
    PyMinqlx_Initialize();
}
#endif
