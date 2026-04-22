#include "../debug.h"

#include <stdint.h>

#ifdef EARLY_BOOT_GDB

extern "C" uint32_t g_kernel_debug_saved_esp = 0;
extern "C" uint8_t g_kernel_debug_stack[16 * 1024] = {};

extern "C" void kernel_debug_checkpoint_dispatch(struct pt_regs* regs, struct pt_context* context) {
    debugger_enter(DEBUG_TRAP_UNKNOWN, regs, context);
}

extern "C" void kernel_debug_checkpoint_body(void) {
}

extern "C" __attribute__((naked)) void kernel_debug_checkpoint(void) {
    __asm__ __volatile__(
        "movl %esp, g_kernel_debug_saved_esp\n\t"
        "movl $g_kernel_debug_stack + 16384, %esp\n\t"
        "andl $0xfffffff0, %esp\n\t"
        "subl $64, %esp\n\t"

        "movl %eax, 40(%esp)\n\t"
        "movl %ebp, 36(%esp)\n\t"
        "movl %edi, 32(%esp)\n\t"
        "movl %esi, 28(%esp)\n\t"
        "movl %edx, 24(%esp)\n\t"
        "movl %ecx, 20(%esp)\n\t"
        "movl %ebx, 16(%esp)\n\t"
        "movl $0, 0(%esp)\n\t"
        "movl $0, 4(%esp)\n\t"

        "xorl %eax, %eax\n\t"
        "movw %ds, %ax\n\t"
        "movl %eax, 8(%esp)\n\t"
        "xorl %eax, %eax\n\t"
        "movw %es, %ax\n\t"
        "movl %eax, 12(%esp)\n\t"

        "movl g_kernel_debug_saved_esp, %edx\n\t"
        "movl (%edx), %eax\n\t"
        "movl %eax, 44(%esp)\n\t"
        "xorl %eax, %eax\n\t"
        "movw %cs, %ax\n\t"
        "movl %eax, 48(%esp)\n\t"
        "pushfl\n\t"
        "popl %eax\n\t"
        "movl %eax, 52(%esp)\n\t"
        "leal 4(%edx), %eax\n\t"
        "movl %eax, 56(%esp)\n\t"
        "xorl %eax, %eax\n\t"
        "movw %ss, %ax\n\t"
        "movl %eax, 60(%esp)\n\t"

        "leal 44(%esp), %edx\n\t"
        "movl %esp, %eax\n\t"
        "pushl %edx\n\t"
        "pushl %eax\n\t"
        "call kernel_debug_checkpoint_dispatch\n\t"
        "addl $8, %esp\n\t"

        "movl g_kernel_debug_saved_esp, %edx\n\t"
        "movl 44(%esp), %eax\n\t"
        "movl %eax, (%edx)\n\t"

        "movl 8(%esp), %eax\n\t"
        "movw %ax, %ds\n\t"
        "movl 12(%esp), %eax\n\t"
        "movw %ax, %es\n\t"
        "movl 52(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "popfl\n\t"

        "movl 16(%esp), %ebx\n\t"
        "movl 20(%esp), %ecx\n\t"
        "movl 24(%esp), %edx\n\t"
        "movl 28(%esp), %esi\n\t"
        "movl 32(%esp), %edi\n\t"
        "movl 36(%esp), %ebp\n\t"
        "movl 40(%esp), %eax\n\t"

        "movl g_kernel_debug_saved_esp, %esp\n\t"
        "ret\n\t");
}

#else

extern "C" void kernel_debug_checkpoint(void) {
}

extern "C" void kernel_debug_checkpoint_body(void) {
}

#endif
