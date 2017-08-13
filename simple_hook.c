// TODO: Rewrite this crap.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include "trampoline.h"

#if defined(__x86_64__) || defined(_M_X64)
typedef uint64_t pint;
typedef int64_t sint;
#define WORST_CASE 			40
#define JUMP_SIZE 			14
#define TRMPS_ARRAY_SIZE	10240
#elif defined(__i386) || defined(_M_IX86)
typedef uint32_t pint;
typedef int32_t sint;
#define WORST_CASE 			29
#define JUMP_SIZE 			5
#define TRMPS_ARRAY_SIZE	10240
#endif

#define IS_RET(hde) (hde.opcode == 0xC3||hde.opcode == 0xCB||hde.opcode == 0xC2||hde.opcode == 0xCA)
#define IS_RELATIVE8(hde) ((hde.flags & F_DISP8) || ((hde.flags & F_IMM8) && (hde.flags & F_RELATIVE)))
#define IS_RELATIVE16(hde) ((hde.flags & F_DISP16) || ((hde.flags & F_IMM16) && (hde.flags & F_RELATIVE)))
#define IS_RELATIVE32(hde) (((hde.flags & F_DISP32) && !hde.modrm_mod && (hde.modrm_mod == 5 || hde.modrm_mod == 13)) || \
    ((hde.flags & F_IMM32) && (hde.flags & F_RELATIVE)))
#define IS_RELATIVE64(hde) ((hde.flags & F_IMM64) && (hde.flags & F_RELATIVE))
#define IS_CALL_OR_JUMP(hde) (hde.opcode == 0xE9 || hde.opcode == 0xE8 || hde.opcode == 0xFF)

const uint8_t NOP = 0x90;

static void* trmps;
static int last_trmp = 0; // trmp[TRMPS_ARRAY_SIZE]

static void initializeTrampolines(void) {
	trmps = mmap(NULL, (WORST_CASE * TRMPS_ARRAY_SIZE) + (JUMP_SIZE * TRMPS_ARRAY_SIZE),
		        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

#if defined(__x86_64__) || defined(_M_X64)
#include "HDE/hde64.h"

int Hook(void* target, void* replacement, void** func_ptr) {
    TRAMPOLINE ct;
    int res, page_size;

    // Check if our trampoline pool has been initialized. If not, do so.
    if (!trmps) {
    	initializeTrampolines();
    }
    else { // TODO: Implement a way to add and remove hooks.
    	if (last_trmp + 1 > TRMPS_ARRAY_SIZE) return -3;
    }

    void* trmp = (void*)((uint64_t)trmps + last_trmp * WORST_CASE);

    ct.pTarget     = target;
    ct.pDetour     = replacement;
    ct.pTrampoline = trmp;

    if (!CreateTrampolineFunction(&ct)) {
        return -11;
    }

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) return errno;
    res = mprotect((void*)((uint64_t)target & ~(page_size-1)), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (res) return errno;

    PJMP_ABS pJmp = (PJMP_ABS)target;
    pJmp->opcode0 = 0xFF;
    pJmp->opcode1 = 0x25;
    pJmp->dummy   = 0;
    pJmp->address = replacement;

    int difference = ct.newIPs[ ct.nIP - 1 ];
    for (int i=sizeof(JMP_ABS); i<difference; i++) {
        *(uint8_t*)((uint64_t)target + i) = NOP;
    }

    *(uint64_t*)func_ptr = (uint64_t)trmp;

    last_trmp++;
    return 0;
}

#elif defined(__i386) || defined(_M_IX86)
#include "HDE/hde32.h"

const uint8_t JMP = 0xE9;

int Hook(void* target, void* replacement, void** func_ptr) {
    int was_ret = 0, total_length = 0, difference = 0, res, page_size;
    hde32s hde;
    
    while (total_length < 5) {
        total_length += hde32_disasm((void*)((uint32_t)target + total_length), &hde);
        if ((hde.flags & F_ERROR) || was_ret) return NULL;
        else if (IS_RET(hde)) was_ret = 1; // We want to stay within the function.
    }

    difference = total_length - 5;
    void* trmp = mmap(NULL, total_length + 5,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (!trmp) return errno;
    
    // Copy the real function's initial bytes to our trampoline.
    memcpy(trmp, target, total_length);

    // Check if we have any relative jumps we have to fix.
    for (int len = 0; len < total_length; ) {
    	// TODO: This could still fail for disp8 and disp16. Need to check out other hooking libs.
        len += hde32_disasm((void*)((uint32_t)trmp + len), &hde);
        if (IS_RELATIVE8(hde)) {
        	*(uint8_t*)((uint32_t)trmp + len - 1) -= (uint32_t)trmp - (uint32_t)target;
        }
        else if (IS_RELATIVE16(hde)) {
        	*(uint16_t*)((uint32_t)trmp + len - 2) -= (uint32_t)trmp - (uint32_t)target;
        }
        else if (IS_RELATIVE32(hde)) {
        	*(uint32_t*)((uint32_t)trmp + len - 4) -= (uint32_t)trmp - (uint32_t)target;
        }
    }

    // Temporarily make the .text page in question writable.
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) return errno;
    res = mprotect((void*)((uint32_t)target & ~(page_size-1)), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (res) return errno;

    // Create JMP to our replacement function.
    *(uint8_t*)target = JMP;
    *(int32_t*)((uint32_t)target + 1) = (int32_t)replacement - (int32_t)target - 5;

    // NOP out the rest, if any, out of courtesy. Removes broken opcodes.
    for (int i = 0; i < difference; i++)
        *(uint8_t*)((uint32_t)target + 5 + i) = NOP;
    
    // NOTE: Apparently POSIX doesn't provide a way to get the protection flags for a page,
    //       so we can't restore without having to make assumptions. I think you could parse
    //       /proc/self/maps, but meh. Assuming it's read and exec. Perhaps we should just
    //       leave it as it is? TODO
    res = mprotect((void*)((uint32_t)target & ~(page_size-1)), page_size, PROT_READ | PROT_EXEC);
    if (res) return errno;

    // Create a jump back to the rest of the real function on trampoline.
    *(char*)((uint32_t)trmp + total_length) = JMP;
    *(int32_t*)((uint32_t)trmp + total_length + 1) = (int32_t)target - (int32_t)trmp - 5;

    *(uint32_t*)func_ptr = (uint32_t)trmp;

    return 0;
}

#endif
