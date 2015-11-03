#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "common.h"
#include "quake_common.h"
#include "patterns.h"
#include "maps_parser.h"
#ifndef NOPY
#include "pyminqlx.h"
#endif

// For comparison with the dedi's executable name to avoid segfaulting
// bash and the likes if we run this through a script.
extern char* __progname;

#if defined(__x86_64__) || defined(_M_X64)
const char qzeroded[] = "qzeroded.x64";
const char qagame_name[] = "qagamex64.so";
#elif defined(__i386) || defined(_M_IX86)
const char qzeroded[] = "qzeroded.x86";
const char qagame_name[] = "qagamei386.so";
#endif

// Global variables.
int common_initialized = 0;
int cvars_initialized = 0;
server_t* sv;
serverStatic_t* svs;

Com_Printf_ptr Com_Printf;
Cmd_AddCommand_ptr Cmd_AddCommand;
Cmd_Args_ptr Cmd_Args;
Cmd_Argv_ptr Cmd_Argv;
Cmd_Argc_ptr Cmd_Argc;
Cmd_ArgsFrom_ptr Cmd_ArgsFrom;
Cmd_TokenizeString_ptr Cmd_TokenizeString;
Cbuf_ExecuteText_ptr Cbuf_ExecuteText;
Cvar_FindVar_ptr Cvar_FindVar;
Cvar_Get_ptr Cvar_Get;
Cvar_GetLimit_ptr Cvar_GetLimit;
Cvar_Set2_ptr Cvar_Set2;
SV_SendServerCommand_ptr SV_SendServerCommand;
SV_ExecuteClientCommand_ptr SV_ExecuteClientCommand;
SV_ClientEnterWorld_ptr SV_ClientEnterWorld;
SV_Shutdown_ptr SV_Shutdown;
SV_Map_f_ptr SV_Map_f;
SV_SetConfigstring_ptr SV_SetConfigstring;
SV_GetConfigstring_ptr SV_GetConfigstring;
SV_DropClient_ptr SV_DropClient;
Sys_SetModuleOffset_ptr Sys_SetModuleOffset;

// VM functions
G_RunFrame_ptr G_RunFrame;
G_AddEvent_ptr G_AddEvent;
G_InitGame_ptr G_InitGame;
CheckPrivileges_ptr CheckPrivileges;
ClientConnect_ptr ClientConnect;
ClientDisconnect_ptr ClientDisconnect;

// VM global variables.
gentity_t* g_entities;
level_locals_t* level;
adminCmd_t* admin_commands;

// Cvars.
cvar_t* sv_maxclients;
// Fake admin stuff.
gentity_t fake_entity = {0};
gclient_t fake_client = {0};

// TODO: Make it output everything to a file too.
void DebugPrint(const char* fmt, ...) {
    va_list args;
	va_start(args, fmt);
    printf(DEBUG_PRINT_PREFIX);
    vprintf(fmt, args);
	va_end(args);
}

// TODO: Make it output everything to a file too.
void DebugError(const char* fmt, const char* file, int line, const char* func, ...) {
    va_list args;
	va_start(args, func);
    fprintf(stderr, DEBUG_ERROR_FORMAT, file, line, func);
    vfprintf(stderr, fmt, args);
	va_end(args);
}

static void SearchFunctions(void) {
	int failed = 0;
	module_info_t module;
	strcpy(module.name, qzeroded);
	int res = GetModuleInfo(&module);
	if (res <= 0) {
		DebugError("GetModuleInfo() returned %d.\n", __FILE__, __LINE__, __func__, res);
		failed = 1;
	}

	DebugPrint("Searching for necessary functions...\n");
	Com_Printf = (Com_Printf_ptr)PatternSearchModule(&module, PTRN_COM_PRINTF, MASK_COM_PRINTF);
	if (Com_Printf == NULL) {
		DebugPrint("ERROR: Unable to find Com_Printf.\n");
		failed = 1;
	}
	else DebugPrint("Com_Printf: %p\n", Com_Printf);

	Cmd_AddCommand = (Cmd_AddCommand_ptr)PatternSearchModule(&module, PTRN_CMD_ADDCOMMAND, MASK_CMD_ADDCOMMAND);
	if (Cmd_AddCommand == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_AddCommand.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_AddCommand: %p\n", Cmd_AddCommand);

	Cmd_Args = (Cmd_Args_ptr)PatternSearchModule(&module, PTRN_CMD_ARGS, MASK_CMD_ARGS);
	if (Cmd_Args == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_Args.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_Args: %p\n", Cmd_Args);

	Cmd_Argv = (Cmd_Argv_ptr)PatternSearchModule(&module, PTRN_CMD_ARGV, MASK_CMD_ARGV);
	if (Cmd_Argv == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_Argv.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_Argv: %p\n", Cmd_Argv);

	Cmd_Argc = (Cmd_Argc_ptr)PatternSearchModule(&module, PTRN_CMD_ARGC, MASK_CMD_ARGC);
	if (Cmd_Argc == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_Argc.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_Argc: %p\n", Cmd_Argc);

	Cmd_ArgsFrom = (Cmd_ArgsFrom_ptr)PatternSearchModule(&module, PTRN_CMD_ARGSFROM, MASK_CMD_ARGSFROM);
	if (Cmd_ArgsFrom == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_ArgsFrom.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_ArgsFrom: %p\n", Cmd_ArgsFrom);

	Cmd_TokenizeString = (Cmd_TokenizeString_ptr)PatternSearchModule(&module, PTRN_CMD_TOKENIZESTRING, MASK_CMD_TOKENIZESTRING);
	if (Cmd_TokenizeString == NULL) {
		DebugPrint("ERROR: Unable to find Cmd_TokenizeString.\n");
		failed = 1;
	}
	else DebugPrint("Cmd_TokenizeString: %p\n", Cmd_TokenizeString);

	Cbuf_ExecuteText = (Cbuf_ExecuteText_ptr)PatternSearchModule(&module, PTRN_CBUF_EXECUTETEXT, MASK_CBUF_EXECUTETEXT);
	if (Cbuf_ExecuteText == NULL) {
		DebugPrint("ERROR: Unable to find Cbuf_ExecuteText.\n");
		failed = 1;
	}
	else DebugPrint("Cbuf_ExecuteText: %p\n", Cbuf_ExecuteText);

	Cvar_FindVar = (Cvar_FindVar_ptr)PatternSearchModule(&module, PTRN_CVAR_FINDVAR, MASK_CVAR_FINDVAR);
	if (Cvar_FindVar == NULL) {
		DebugPrint("ERROR: Unable to find Cvar_FindVar.\n");
		failed = 1;
	}
	else DebugPrint("Cvar_FindVar: %p\n", Cvar_FindVar);

	Cvar_Get = (Cvar_Get_ptr)PatternSearchModule(&module, PTRN_CVAR_GET, MASK_CVAR_GET);
	if (Cvar_Get == NULL) {
		DebugPrint("ERROR: Unable to find Cvar_Get.\n");
		failed = 1;
	}
	else DebugPrint("Cvar_Get: %p\n", Cvar_Get);

	Cvar_GetLimit = (Cvar_GetLimit_ptr)PatternSearchModule(&module, PTRN_CVAR_GETLIMIT, MASK_CVAR_GETLIMIT);
	if (Cvar_GetLimit == NULL) {
		DebugPrint("ERROR: Unable to find Cvar_GetLimit.\n");
		failed = 1;
	}
	else DebugPrint("Cvar_GetLimit: %p\n", Cvar_GetLimit);

	Cvar_Set2 = (Cvar_Set2_ptr)PatternSearchModule(&module, PTRN_CVAR_SET2, MASK_CVAR_SET2);
	if (Cvar_Set2 == NULL) {
		DebugPrint("ERROR: Unable to find Cvar_Set2.\n");
		failed = 1;
	}
	else DebugPrint("Cvar_Set2: %p\n", Cvar_Set2);

	SV_SendServerCommand = (SV_SendServerCommand_ptr)PatternSearchModule(&module, PTRN_SV_SENDSERVERCOMMAND, MASK_SV_SENDSERVERCOMMAND);
	if (SV_SendServerCommand == NULL) {
		DebugPrint("ERROR: Unable to find SV_SendServerCommand.\n");
		failed = 1;
	}
	else DebugPrint("SV_SendServerCommand: %p\n", SV_SendServerCommand);

	SV_ExecuteClientCommand = (SV_ExecuteClientCommand_ptr)PatternSearchModule(&module, PTRN_SV_EXECUTECLIENTCOMMAND, MASK_SV_EXECUTECLIENTCOMMAND);
	if (SV_ExecuteClientCommand == NULL) {
		DebugPrint("ERROR: Unable to find SV_ExecuteClientCommand.\n");
		failed = 1;
	}
	else DebugPrint("SV_ExecuteClientCommand: %p\n", SV_ExecuteClientCommand);

	SV_Shutdown = (SV_Shutdown_ptr)PatternSearchModule(&module, PTRN_SV_SHUTDOWN, MASK_SV_SHUTDOWN);
	if (SV_Shutdown == NULL) {
		DebugPrint("ERROR: Unable to find SV_Shutdown.\n");
		failed = 1;
	}
	else DebugPrint("SV_Shutdown: %p\n", SV_Shutdown);

	SV_Map_f = (SV_Map_f_ptr)PatternSearchModule(&module, PTRN_SV_MAP_F, MASK_SV_MAP_F);
	if (SV_Map_f == NULL) {
		DebugPrint("ERROR: Unable to find SV_Map_f.\n");
		failed = 1;
	}
	else DebugPrint("SV_Map_f: %p\n", SV_Map_f);

	SV_ClientEnterWorld = (SV_ClientEnterWorld_ptr)PatternSearchModule(&module, PTRN_SV_CLIENTENTERWORLD, MASK_SV_CLIENTENTERWORLD);
	if (SV_ClientEnterWorld == NULL) {
		DebugPrint("ERROR: Unable to find SV_ClientEnterWorld.\n");
		failed = 1;
	}
	else DebugPrint("SV_ClientEnterWorld: %p\n", SV_ClientEnterWorld);

	SV_SetConfigstring = (SV_SetConfigstring_ptr)PatternSearchModule(&module, PTRN_SV_SETCONFIGSTRING, MASK_SV_SETCONFIGSTRING);
	if (SV_SetConfigstring == NULL) {
		DebugPrint("ERROR: Unable to find SV_SetConfigstring.\n");
		failed = 1;
	}
	else DebugPrint("SV_SetConfigstring: %p\n", SV_SetConfigstring);

	SV_GetConfigstring = (SV_GetConfigstring_ptr)PatternSearchModule(&module, PTRN_SV_GETCONFIGSTRING, MASK_SV_GETCONFIGSTRING);
	if (SV_GetConfigstring == NULL) {
		DebugPrint("ERROR: Unable to find SV_GetConfigstring.\n");
		failed = 1;
	}
	else DebugPrint("SV_GetConfigstring: %p\n", SV_GetConfigstring);

	SV_DropClient = (SV_DropClient_ptr)PatternSearchModule(&module, PTRN_SV_DROPCLIENT, MASK_SV_DROPCLIENT);
	if (SV_DropClient == NULL) {
		DebugPrint("ERROR: Unable to find SV_DropClient.\n");
		failed = 1;
	}
	else DebugPrint("SV_DropClient: %p\n", SV_DropClient);

	Sys_SetModuleOffset = (Sys_SetModuleOffset_ptr)PatternSearchModule(&module, PTRN_SYS_SETMODULEOFFSET, MASK_SYS_SETMODULEOFFSET);
	if (Sys_SetModuleOffset == NULL) {
		DebugPrint("ERROR: Unable to find Sys_SetModuleOffset.\n");
		failed = 1;
	}
	else DebugPrint("Sys_SetModuleOffset: %p\n", Sys_SetModuleOffset);

	// Cmd_Argc is really small, making it hard to search for, so we use a reference to it instead.
	Cmd_Argc = (Cmd_Argc_ptr)(*(int32_t*)OFFSET_RELP_CMD_ARGC + OFFSET_RELP_CMD_ARGC + 4);

	if (failed) {
		DebugPrint("Exiting.\n");
		exit(1);
	}
}

// NOTE: Some functions can easily and reliably be found on the VM_Call table instead.
void SearchVmFunctions(void) {
	int failed = 0;

	// For some reason, the module doesn't show up when reading /proc/self/maps.
	// Perhaps this needs to be called later? In any case, we know exactly where
	// the module is mapped, so I think this is fine. If it ever breaks, it'll
	// be trivial to fix.
	G_AddEvent = (G_AddEvent_ptr)PatternSearch((void*)((pint)qagame + 0xB000), 0xB0000, PTRN_G_ADDEVENT, MASK_G_ADDEVENT);
	if (G_AddEvent == NULL) {
		DebugPrint("ERROR: Unable to find G_AddEvent.\n");
		failed = 1;
	}
	else DebugPrint("G_AddEvent: %p\n", G_AddEvent);

	CheckPrivileges = (CheckPrivileges_ptr)PatternSearch((void*)((pint)qagame + 0xB000),
			0xB0000, PTRN_CHECKPRIVILEGES, MASK_CHECKPRIVILEGES);
	if (CheckPrivileges == NULL) {
		DebugPrint("ERROR: Unable to find CheckPrivileges.\n");
		failed = 1;
	}
	else DebugPrint("CheckPrivileges: %p\n", CheckPrivileges);

	ClientConnect = (ClientConnect_ptr)PatternSearch((void*)((pint)qagame + 0xB000),
			0xB0000, PTRN_CLIENTCONNECT, MASK_CLIENTCONNECT);
	if (ClientConnect == NULL) {
		DebugPrint("ERROR: Unable to find ClientConnect.\n");
		failed = 1;
	}
	else DebugPrint("ClientConnect: %p\n", ClientConnect);

	ClientDisconnect = (ClientDisconnect_ptr)PatternSearch((void*)((pint)qagame + 0xB000),
			0xB0000, PTRN_CLIENTDISCONNECT, MASK_CLIENTDISCONNECT);
	if (ClientDisconnect == NULL) {
		DebugPrint("ERROR: Unable to find ClientDisconnect.\n");
		failed = 1;
	}
	else DebugPrint("ClientDisconnect: %p\n", ClientDisconnect);

	if (failed) {
			DebugPrint("Exiting.\n");
			exit(1);
	}
}

// Currently called by My_Cmd_AddCommand(), since it's called at a point where we
// can safely do whatever we do below. It'll segfault if we do it at the entry
// point, since functions like Cmd_AddCommand need initialization first.
void InitializeStatic(void) {
    DebugPrint("Initializing...\n");
    
    // Set the seed for our RNG.
    srand(time(NULL));

    Cmd_AddCommand("cmd", SendServerCommand);
    Cmd_AddCommand("cp", CenterPrint);
    Cmd_AddCommand("print", RegularPrint);
    Cmd_AddCommand("slap", Slap);
    Cmd_AddCommand("slay", Slay);
#ifndef NOPY
    Cmd_AddCommand("py", PyRcon);
    Cmd_AddCommand("pycmd", PyCommand);
    Cmd_AddCommand("pyrestart", RestartPython);
#endif
	
#ifndef NOPY
	// Initialize Python and run the main script.
	PyMinqlx_InitStatus_t res = PyMinqlx_Initialize();
    if (res != PYM_SUCCESS) {
        DebugPrint("Python initialization failed: %d\n", res);
        exit(1);
    }
#endif

    common_initialized = 1;
}

// Initialize VM stuff. Needs to be called whenever Sys_SetModuleOffset is called,
// after qagame pointer has been initialized.
void InitializeVm(void) {
    DebugPrint("Initializing VM pointers...\n");
#if defined(__x86_64__) || defined(_M_X64)
    g_entities = (gentity_t*)(*(int32_t*)OFFSET_RELP_G_ENTITIES + OFFSET_RELP_G_ENTITIES + 4);
    admin_commands = (adminCmd_t*)(*(int32_t*)OFFSET_RELP_ADMINCOMMANDS + OFFSET_RELP_ADMINCOMMANDS + 4);
    level = (level_locals_t*)(*(int32_t*)OFFSET_RELP_LEVEL + OFFSET_RELP_LEVEL + 4);
#elif defined(__i386) || defined(_M_IX86)
    // TODO: admin_commands x86
    g_entities = (gentity_t*)(*(int32_t*)OFFSET_RELP_G_ENTITIES + 0xCAFF4 + (pint)qagame);
#endif
}

// Called after the game is initialized.
void InitializeCvars(void) {
    sv_maxclients = Cvar_FindVar("sv_maxclients");
    
    cvars_initialized = 1;
}

__attribute__((constructor))
void EntryPoint(void) {
	if (strcmp(__progname, qzeroded))
		return;

	SearchFunctions();

	// Initialize some key structure pointers before hooking, since we
	// might use some of the functions that could be hooked later to
	// get the pointer, such as SV_SetConfigstring.
#if defined(__x86_64__) || defined(_M_X64)
	sv = (server_t*)(*(int32_t*)OFFSET_RELP_SV + OFFSET_RELP_SV + 5);
    // 32-bit pointer. intptr_t added to suppress warning about the casting.
    svs = (serverStatic_t*)(intptr_t)(*(uint32_t*)OFFSET_PP_SVS);
#elif defined(__i386) || defined(_M_IX86)
    svs = *(serverStatic_t**)OFFSET_PP_SVS;
#endif

    // TODO: Write script to automatically set version based on git output
    //       when building and then make it print it here.
    DebugPrint("Shared library loaded!\n");
    HookStatic();

    // Initialize our fake admin stuff here, since it doesn't depend on anything
    // else being initialized, and we only need to do it once.
    fake_entity.client = &fake_client;
    fake_client.privileges = PRIV_ADMIN;
}
