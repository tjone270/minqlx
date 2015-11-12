// TODO: Rewrite this crap.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>

// For when we have an address and want to know exactly where it is
// so we can edit it whenever needed.
static int findAddress32(void* target, uint32_t address, size_t size) {
	for (size_t i = 0; i < size; i++)
		if (*(uint32_t*)((uint64_t)target + i) == address) return i;

	return -1;
}

static int findAddress64(void* target, uint64_t address, size_t size) {
	for (size_t i = 0; i < size; i++)
		if (*(uint64_t*)((uint64_t)target + i) == address) return i;

	return -1;
}

#if defined(__x86_64__) || defined(_M_X64)
typedef uint64_t pint;
typedef int64_t sint;
#define WORST_CASE 			29
#define JUMP_SIZE 			14 // Can be 6 in some cases, but 14 is worst-case.
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
// There are cases, especially in x64, we copy a relative <64 bit jump over to the trampoline,
// but the trampoline is too far away from the intended jump that we can't even adjust it.
// In those cases, we send the jump to an extra set of trampolines that we allocate along,
// with the regular trampoline pool that holds a bunch of absolute jumps.
static void* trmps_abs; // Right after trmps array in memory.
static int last_trmp_abs = 0;

static void initializeTrampolines(void) {
	trmps = mmap(NULL, (WORST_CASE * TRMPS_ARRAY_SIZE) + (JUMP_SIZE * TRMPS_ARRAY_SIZE),
		        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	trmps_abs = (void*)((pint)trmps + (WORST_CASE * TRMPS_ARRAY_SIZE));
}

#if defined(__x86_64__) || defined(_M_X64)
#include "HDE/hde64.h"

const uint8_t PUSH = 0x68;
const uint32_t MOV_RSP4_DWORD = 0x042444C7;
const uint8_t RET = 0xC3;

static void makeJump(uint64_t target, uint64_t replacement) {
	// Create an absolute "jump" to our replacement function.
	*(uint8_t*)target = PUSH;
	*(uint32_t*)(target + 1) = replacement;
	if (replacement >> 32) {
		// Need to fill the 32 MSB as well.
		*(uint32_t*)(target + 5) = MOV_RSP4_DWORD;
		*(uint32_t*)(target + 9) = replacement >> 32;
		// Our target address is now on the stack. Let's return.
		*(uint8_t*)(target + 13) = RET;
	}
	else {
		// We just need to return here, since the address is already on the stack.
		*(uint8_t*)(target + 5) = RET;
	}
}

int Hook(void* target, void* replacement, void** func_ptr) {
    int was_ret = 0, total_length = 0, difference = 0;
    int bytes_needed, res, page_size, used_abs = 0;
    hde64s hde;

    // Check if our trampoline pool has been initialized. If not, do so.
    if (!trmps) {
    	initializeTrampolines();
    }
    else { // TODO: Implement a way to add and remove hooks.
    	if (last_trmp + 1 > TRMPS_ARRAY_SIZE) return -3;
    }

    // We can save a couple of bytes if the address can be expressed with the 32 LSB.
    bytes_needed = ((uint64_t)replacement) >> 32 ? 14 : 6;

    while (total_length < bytes_needed) {
        total_length += hde64_disasm((void*)((uint64_t)target + total_length), &hde);
        if ((hde.flags & F_ERROR) || was_ret) return -1;
        else if (IS_RET(hde)) was_ret = 1; // We want to stay within the function.
    }

    difference = total_length - bytes_needed;
    void* trmp = (void*)((uint64_t)trmps + last_trmp * WORST_CASE);

    // Copy the real function's initial bytes to our trampoline.
    memcpy(trmp, target, total_length);

    // Check if we have any relative jumps we have to fix.
	for (int len = 0; len < total_length; ) {
		hde64_disasm((void*)((uint64_t)trmp + len), &hde);
		if (IS_RELATIVE8(hde)) {
			return -2;
		}
		else if ( IS_RELATIVE16(hde) ) {
			return -3;
		}
		else if (IS_RELATIVE32(hde)) {
			if (((uint64_t)trmp - (uint64_t)target) >> 32) {
				// TODO: Can all relative stuff be rewritten to absolute?
				//       In any case, we need a way to deal with this case.
				return -5;
			}

			int pos = findAddress32(trmp + len, hde.flags & F_DISP32 ? hde.disp.disp32 : hde.imm.imm32, hde.len);
			if (pos == -1) return -4; // Should never happen, but just in case.

			if (IS_CALL_OR_JUMP(hde)) {
				uint64_t trmp_abs = (uint64_t)trmps_abs + (last_trmp_abs + used_abs) * JUMP_SIZE;
				*(uint32_t*)((uint64_t)trmp + len + pos) = trmp_abs - (uint64_t)trmp + len + hde.len;
				makeJump(trmp_abs, (uint64_t)target + total_length);
				used_abs++;
			}
			else {
				*(uint32_t*)((uint64_t)trmp + len + pos) -= (uint64_t)trmp - (uint64_t)target;
			}
		}
		else if (IS_RELATIVE64(hde)) {
			int pos = findAddress64(trmp + len, hde.imm.imm64, hde.len);
			if (pos == -1) return -6; // Should never happen, but just in case.
			*(uint64_t*)((uint64_t)trmp + len + pos) -= (uint64_t)trmp - (uint64_t)target;
		}

		len += hde.len;
	}

    // Temporarily make the .text page in question writable.
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) return errno;
    res = mprotect((void*)((uint64_t)target & ~(page_size-1)), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (res) return errno;

    makeJump((uint64_t)target, (uint64_t)replacement);
    //makeJump((uint64_t)target, (uint64_t)replacement, 1);

    // NOP out the rest, if any, out of courtesy. Removes broken opcodes.
    for (int i = 0; i < difference; i++)
        *(uint8_t*)((uint64_t)target + bytes_needed + i) = NOP;

    // TODO: Use maps_parser instead of assuming read-exec.
    res = mprotect((void*)((uint64_t)target & ~(page_size-1)), page_size, PROT_READ | PROT_EXEC);
    if (res) return errno;

    // Create a jump back to the rest of the real function on trampoline.
    makeJump((uint64_t)trmp + total_length, (uint64_t)target + total_length);

    *(uint64_t*)func_ptr = (uint64_t)trmp;

    last_trmp++;
    last_trmp_abs += used_abs;
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
