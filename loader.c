#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define __cdecl __attribute__((__cdecl__))

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/ptrace.h>

#include "patterns.h"

#if defined(__x86_64__) || defined(_M_X64)
const char* qlds = "qzeroded.x64"
#elif defined(__i386) || defined(_M_IX86)
const char* qlds = "qzeroded.x86"
#endif

typedef void (__cdecl *Com_Printf_ptr)(char* fmt, ...);
typedef void (__cdecl *Cmd_AddCommand_ptr)(char* cmd, void* func);
typedef char* (__cdecl *Cmd_Args_ptr)();

Com_Printf_ptr Com_Printf;
Cmd_AddCommand_ptr Cmd_AddCommand;
Cmd_Args_ptr Cmd_Args;

void* DelayCommandAdd() {
    printf("Sleep: %d\n", sleep(7));
    printf("Calling AddCommand...\n");
    //Cmd_AddCommand("testcmd", TestCommand);

    pthread_exit(0);
}

int main(int argc, char* argv[])
{
    pid_t pid = fork();
    if (!pid) { // In child process?
        //ptrace(PTRACE_TRACEME, 0, NULL, NULL);

        pthread_t tid;
        pthread_attr_t tattr;
        printf("pthread_attr_init: %d\n", pthread_attr_init(&tattr));
        printf("pthread_attr_setdetachstate: %d\n",
               pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED));
        pthread_create(&tid, &tattr, DelayCommandAdd, NULL);

        printf("Execing %s...\n", qlds);
        execv(qlds, &argv[1]);

        // If we got here, we know it failed.
        printf("Cannot find .\n");
    }
    else {
        wait(&pid);
    }

    return 0;
}
