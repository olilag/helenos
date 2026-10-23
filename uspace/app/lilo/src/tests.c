/*
 * Copyright (c) 2021 Vit Kabele
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup lilo
 * @{
 */
/**
 * @file
 */

/* ------------------------- FILE INCLUSION --------------------------------- */

#include <masq.h>
#include <as.h>
#include <assert.h>
#include <elf/elf_mod.h>
#include <errno.h>
#include <stdio.h>
#include <str.h>
#include <malloc.h>

#include "linux/auxvec.h"
#include "x86_64-linux-generic/asm/unistd_64.h"
#include "syscalls.h"

/* ------------------------- MACROS ----------------------------------------- */
#define STACK_SIZE 1024
#define HEXDUMP_COLS 4

#define __unused __attribute__((unused))
#define TEST_PASS printf("Test: %s PASSED\n", __FUNCTION__)
#define TEST_ASSERT(cond)                                                      \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("Test: %s FAILED (%s)\n", __FUNCTION__, #cond); \
			return;                     \
		}                            \
	}while(0)

#define TEST_RAX_VAL 0xA
#define TEST_RBX_VAL 0xB
#define TEST_RDX_VAL 0xD
#define TEST_RDI_VAL 0xF
#define TEST_RSI_VAL 0xE
#define TEST_RSP_VAL 0xC
#define TEST_RBP_VAL 0x1
#define TEST_R8_VAL 0x8
#define TEST_R9_VAL 0x9
#define TEST_R10_VAL 0x10
#define TEST_R12_VAL 0x12
#define TEST_R13_VAL 0x13
#define TEST_R14_VAL 0x14
#define TEST_R15_VAL 0x15

/* ------------------------- TYPE DECLARATIONS ------------------------------ */
/* ------------------------- EXTERNAL FUNCTION DECLARATIONS ----------------- */
extern void set_regs_func(void);
extern void mul_regs_func(void);

/* ------------------------- LOCAL FUNCTION DECLARATIONS -------------------- */
static void test_set_regs(void);
static void test_mul_regs(void);
static void print_context(masq_t *context) __unused;
static void print_stack(uint64_t *stack, size_t stack_size) __unused;
static __unused void hexdump(void* addr, size_t count);

/* ------------------------- OBJECT DEFINITIONS ----------------------------- */

/* ------------------------- PUBLIC FUNCTION DEFINITIONS -------------------- */

void run_tests(void);
void run_tests(){
	test_set_regs();
	test_mul_regs();
}


/* ------------------------- STATIC FUNCTION DECLARATIONS ------------------- */

static void hexdump(void* addr, size_t count){
	const uint8_t *byte_addr = addr;
	for (size_t row = 0; row < count/HEXDUMP_COLS; ++row){
		printf("%p: ", (void*)(byte_addr + row*HEXDUMP_COLS));
		for (size_t col = 0; col < HEXDUMP_COLS; ++col){
			const uint8_t* offset = byte_addr + (row * HEXDUMP_COLS) + col;
			printf("%X", *offset);
		}
		printf("\n");
	}
}

static void print_context(masq_t *context)
{
	printf("Emulated context:\n");
	printf("rax = 0x%016lX rbx = 0x%016lX rcx = 0x%016lX\n", context->rax,
	    context->rbx, context->rcx);
}

static void print_stack(uint64_t *stack, size_t stack_size)
{
        printf("Emulated stack:\n%p: 0x%016lX 0x%016lX 0x%016lX\n",
               &stack[stack_size - 4], stack[stack_size - 3],
               stack[stack_size - 2], stack[stack_size - 1]);
}
/**
 * Test a call to the emulated function and check the values of the registers
 * on return.
 */
static void test_set_regs(void){
	masq_t test_context = {
		.rcx = (uint64_t) set_regs_func,
		.rsp = 0x0 // This test should not use the stack.
	};
	masq(&test_context);
	TEST_ASSERT(test_context.rax == TEST_RAX_VAL);
	TEST_ASSERT(test_context.rbx == TEST_RBX_VAL);
	TEST_ASSERT(test_context.rdx == TEST_RDX_VAL);
	TEST_ASSERT(test_context.rdi == TEST_RDI_VAL);
	TEST_ASSERT(test_context.rsi == TEST_RSI_VAL);
	TEST_ASSERT(test_context.rsp == TEST_RSP_VAL);
	TEST_ASSERT(test_context.rbp == TEST_RBP_VAL);
	TEST_ASSERT(test_context.r8  == TEST_R8_VAL);
	TEST_ASSERT(test_context.r9  == TEST_R9_VAL);
	TEST_ASSERT(test_context.r10 == TEST_R10_VAL);
	TEST_ASSERT(test_context.r12 == TEST_R12_VAL);
	TEST_ASSERT(test_context.r13 == TEST_R13_VAL);
	TEST_ASSERT(test_context.r14 == TEST_R14_VAL);
	TEST_ASSERT(test_context.r15 == TEST_R15_VAL);
	TEST_PASS;
}

static void test_mul_regs(void) {
	masq_t test_context = {
	    .rcx = (uint64_t) mul_regs_func,
        .rax = TEST_RAX_VAL,
        .rbx = TEST_RBX_VAL,
        .rdx = TEST_RDX_VAL,
        .rdi = TEST_RDI_VAL,
        .rsi = TEST_RSI_VAL,
        .rsp = TEST_RSP_VAL,
        .rbp = TEST_RBP_VAL,
        .r8  = TEST_R8_VAL,
        .r9  = TEST_R9_VAL,
        .r10 = TEST_R10_VAL,
        .r12 = TEST_R12_VAL,
        .r13 = TEST_R13_VAL,
        .r14 = TEST_R14_VAL,
        .r15 = TEST_R15_VAL
	};
	masq(&test_context);

    TEST_ASSERT(test_context.rax == 2 * TEST_RAX_VAL);
    TEST_ASSERT(test_context.rbx == 2 * TEST_RBX_VAL);
    TEST_ASSERT(test_context.rdx == 2 * TEST_RDX_VAL);
    TEST_ASSERT(test_context.rdi == 2 * TEST_RDI_VAL);
    TEST_ASSERT(test_context.rsi == 2 * TEST_RSI_VAL);
    TEST_ASSERT(test_context.rsp == 2 * TEST_RSP_VAL);
    TEST_ASSERT(test_context.rbp == 2 * TEST_RBP_VAL);
    TEST_ASSERT(test_context.r8  == 2 * TEST_R8_VAL);
    TEST_ASSERT(test_context.r9  == 2 * TEST_R9_VAL);
    TEST_ASSERT(test_context.r10 == 2 * TEST_R10_VAL);
    TEST_ASSERT(test_context.r12 == 2 * TEST_R12_VAL);
    TEST_ASSERT(test_context.r13 == 2 * TEST_R13_VAL);
    TEST_ASSERT(test_context.r14 == 2 * TEST_R14_VAL);
    TEST_ASSERT(test_context.r15 == 2 * TEST_R15_VAL);
    TEST_PASS;
}

__asm__(".section .text\n"
        "set_regs_func:\n"
        "movq $"quote_me(TEST_RAX_VAL)", %rax\n"
        "movq $"quote_me(TEST_RBX_VAL)", %rbx\n"
        "movq $"quote_me(TEST_RDX_VAL)", %rdx\n"
        "movq $"quote_me(TEST_RSI_VAL)", %rsi\n"
        "movq $"quote_me(TEST_RDI_VAL)", %rdi\n"
        "movq $"quote_me(TEST_RSP_VAL)", %rsp\n"
        "movq $"quote_me(TEST_RBP_VAL)", %rbp\n"
        "movq $"quote_me(TEST_R8_VAL)", %r8\n"
        "movq $"quote_me(TEST_R9_VAL)", %r9\n"
        "movq $"quote_me(TEST_R10_VAL)", %r10\n"
        "movq $"quote_me(TEST_R12_VAL)", %r12\n"
        "movq $"quote_me(TEST_R13_VAL)", %r13\n"
        "movq $"quote_me(TEST_R14_VAL)", %r14\n"
        "movq $"quote_me(TEST_R15_VAL)", %r15\n"
        "syscall\n");

__asm__(".section .text\n"
        "mul_regs_func:\n"
        "imul $2, %rax\n"
        "imul $2, %rbx\n"
        "imul $2, %rdx\n"
        "imul $2, %rsi\n"
        "imul $2, %rdi\n"
        "imul $2, %rsp\n"
        "imul $2, %rbp\n"
        "imul $2, %r8\n"
        "imul $2, %r9\n"
        "imul $2, %r10\n"
        "imul $2, %r12\n"
        "imul $2, %r13\n"
        "imul $2, %r14\n"
        "imul $2, %r15\n"
        "syscall\n");

/** @}
 */
