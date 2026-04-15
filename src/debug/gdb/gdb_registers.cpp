#include "gdb_registers.h"
#include "../../include/Video.h"

static GDBRegisters g_reg_context;
static int g_reg_context_valid = 0;
static struct pt_regs* g_bound_regs = 0;
static struct pt_context* g_bound_context = 0;
static int g_bound_from_user = 0;

static const char* hex_chars = "0123456789abcdef";

void gdb_registers_init(void) {
    for (size_t i = 0; i < sizeof(GDBRegisters); i++) {
        ((char*)&g_reg_context)[i] = 0;
    }
    g_reg_context_valid = 0;
    g_bound_regs = 0;
    g_bound_context = 0;
    g_bound_from_user = 0;
}

int is_reg_context_valid(void) {
    return g_reg_context_valid;
}

void gdb_registers_save(void) {
    if (g_reg_context_valid) {
        return;
    }

    __asm__ volatile ("movl %%eax, %0" : "=m"(g_reg_context.eax));
    __asm__ volatile ("movl %%ecx, %0" : "=m"(g_reg_context.ecx));
    __asm__ volatile ("movl %%edx, %0" : "=m"(g_reg_context.edx));
    __asm__ volatile ("movl %%ebx, %0" : "=m"(g_reg_context.ebx));
    __asm__ volatile ("movl %%esp, %0" : "=m"(g_reg_context.esp));
    __asm__ volatile ("movl %%ebp, %0" : "=m"(g_reg_context.ebp));
    __asm__ volatile ("movl %%esi, %0" : "=m"(g_reg_context.esi));
    __asm__ volatile ("movl %%edi, %0" : "=m"(g_reg_context.edi));

    __asm__ volatile ("movw %%cs, %0" : "=m"(g_reg_context.cs));
    __asm__ volatile ("movw %%ss, %0" : "=m"(g_reg_context.ss));
    __asm__ volatile ("movw %%ds, %0" : "=m"(g_reg_context.ds));
    __asm__ volatile ("movw %%es, %0" : "=m"(g_reg_context.es));
    __asm__ volatile ("movw %%fs, %0" : "=m"(g_reg_context.fs));
    __asm__ volatile ("movw %%gs, %0" : "=m"(g_reg_context.gs));

    __asm__ __volatile__(
        "call 1f\n\t"
        "1: popl %0\n\t"
        : "=r"(g_reg_context.eip)
    );
    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0\n\t"
        : "=r"(g_reg_context.eflags)
    );

    // g_reg_context_valid = 1;
}

void gdb_registers_invalidate(void) {
    g_reg_context_valid = 0;
    g_bound_regs = 0;
    g_bound_context = 0;
    g_bound_from_user = 0;
}

void gdb_registers_bind_trap_frame(struct pt_regs* regs, struct pt_context* context) {
    g_bound_regs = regs;
    g_bound_context = context;
    g_bound_from_user = 0;

    if (!regs || !context) {
        return;
    }

    g_bound_from_user = ((context->xcs & USER_MODE) == USER_MODE) ? 1 : 0;

    g_reg_context.eax = regs->eax;
    g_reg_context.ecx = regs->ecx;
    g_reg_context.edx = regs->edx;
    g_reg_context.ebx = regs->ebx;
    g_reg_context.ebp = regs->ebp;
    g_reg_context.esi = regs->esi;
    g_reg_context.edi = regs->edi;

    g_reg_context.eip = context->eip;
    g_reg_context.eflags = context->eflags;
    g_reg_context.cs = context->xcs;
    g_reg_context.ds = regs->xds;
    g_reg_context.es = regs->xes;

    if (g_bound_from_user) {
        g_reg_context.esp = context->esp;
        g_reg_context.ss = context->xss;
    } else {
        g_reg_context.esp = (uint32_t)context + sizeof(uint32_t) * 3;
        __asm__ volatile ("movw %%ss, %0" : "=m"(g_reg_context.ss));
    }

    __asm__ volatile ("movw %%fs, %0" : "=m"(g_reg_context.fs));
    __asm__ volatile ("movw %%gs, %0" : "=m"(g_reg_context.gs));

    g_reg_context_valid = 1;
}

void gdb_registers_commit_to_trap_frame(void) {
    if (!g_bound_regs || !g_bound_context || !g_reg_context_valid) {
        return;
    }

    g_bound_regs->eax = g_reg_context.eax;
    g_bound_regs->ecx = g_reg_context.ecx;
    g_bound_regs->edx = g_reg_context.edx;
    g_bound_regs->ebx = g_reg_context.ebx;
    g_bound_regs->ebp = g_reg_context.ebp;
    g_bound_regs->esi = g_reg_context.esi;
    g_bound_regs->edi = g_reg_context.edi;
    g_bound_regs->xds = g_reg_context.ds;
    g_bound_regs->xes = g_reg_context.es;

    g_bound_context->eip = g_reg_context.eip;
    g_bound_context->eflags = g_reg_context.eflags;
    g_bound_context->xcs = g_reg_context.cs;
    if (g_bound_from_user) {
        g_bound_context->esp = g_reg_context.esp;
        g_bound_context->xss = g_reg_context.ss;
    }
}

void gdb_registers_restore(void) {
    __asm__ volatile (
        "movl %0, %%eax\n\t"
        "movl %1, %%ecx\n\t"
        "movl %2, %%edx\n\t"
        "movl %3, %%ebx\n\t"
        "movl %4, %%esp\n\t"
        "movl %5, %%ebp\n\t"
        "movl %6, %%esi\n\t"
        "movl %7, %%edi\n\t"
        "movl %9, %%eax\n\t"
        "pushl %%eax\n\t"
        "popfl\n\t"
        "pushl %8\n\t"
        "ret"
        : : "m"(g_reg_context.eax), "m"(g_reg_context.ecx),
            "m"(g_reg_context.edx), "m"(g_reg_context.ebx),
            "m"(g_reg_context.esp), "m"(g_reg_context.ebp),
            "m"(g_reg_context.esi), "m"(g_reg_context.edi),
            "m"(g_reg_context.eip), "m"(g_reg_context.eflags),
            "m"(g_reg_context.cs), "m"(g_reg_context.ss),
            "m"(g_reg_context.ds), "m"(g_reg_context.es),
            "m"(g_reg_context.fs), "m"(g_reg_context.gs)
        : "ax", "cx", "dx", "bx", "si", "di", "memory"
    );
}

GDBRegisters* gdb_get_registers(void) {
    return &g_reg_context;
}

void gdb_set_register(int reg_num, uint32_t value) {
    // 首先更新软件上下文
    switch (reg_num) {
        case GDB_REG_EAX:   g_reg_context.eax = value; break;
        case GDB_REG_ECX:   g_reg_context.ecx = value; break;
        case GDB_REG_EDX:   g_reg_context.edx = value; break;
        case GDB_REG_EBX:   g_reg_context.ebx = value; break;
        case GDB_REG_ESP:   g_reg_context.esp = value; break;
        case GDB_REG_EBP:   g_reg_context.ebp = value; break;
        case GDB_REG_ESI:   g_reg_context.esi = value; break;
        case GDB_REG_EDI:   g_reg_context.edi = value; break;
        case GDB_REG_EIP:   g_reg_context.eip = value; break;
        case GDB_REG_EFLAGS: g_reg_context.eflags = value; break;
        case GDB_REG_CS:    g_reg_context.cs = value; break;
        case GDB_REG_SS:    g_reg_context.ss = value; break;
        case GDB_REG_DS:    g_reg_context.ds = value; break;
        case GDB_REG_ES:    g_reg_context.es = value; break;
        case GDB_REG_FS:    g_reg_context.fs = value; break;
        case GDB_REG_GS:    g_reg_context.gs = value; break;
    }
    // 标记软件上下文为有效（由写操作更新）
    g_reg_context_valid = 1;

    // 尽量将修改写回到物理寄存器（仅对安全的寄存器）。ESP/EBP/EIP/段寄存器
    // 不在此处直接写回以避免破坏当前执行上下文。
    switch (reg_num) {
        case GDB_REG_EAX:
            __asm__ __volatile__("movl %0, %%eax" : : "r"(value) : "eax", "memory");
            break;
        case GDB_REG_ECX:
            __asm__ __volatile__("movl %0, %%ecx" : : "r"(value) : "ecx", "memory");
            break;
        case GDB_REG_EDX:
            __asm__ __volatile__("movl %0, %%edx" : : "r"(value) : "edx", "memory");
            break;
        case GDB_REG_EBX:
            __asm__ __volatile__("movl %0, %%ebx" : : "r"(value) : "ebx", "memory");
            break;
        case GDB_REG_ESI:
            __asm__ __volatile__("movl %0, %%esi" : : "r"(value) : "esi", "memory");
            break;
        case GDB_REG_EDI:
            __asm__ __volatile__("movl %0, %%edi" : : "r"(value) : "edi", "memory");
            break;
        case GDB_REG_EFLAGS:
            __asm__ __volatile__("pushl %0\n\tpopfl" : : "r"(value) : "cc", "memory");
            break;
        case GDB_REG_ESP:
            break;
        case GDB_REG_EBP:
            break;
        default:
            // EIP, 段寄存器等不在此处写回
            break;
    }
    
    // 调试输出
    const char* reg_names[] = {
        "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",
        "EIP", "EFLAGS", "CS", "SS", "DS", "ES", "FS", "GS"
    };
    
    (void)reg_names;
}

uint32_t gdb_get_register_value(int reg_num) {
    switch (reg_num) {
        case GDB_REG_EAX:   return g_reg_context.eax;
        case GDB_REG_ECX:   return g_reg_context.ecx;
        case GDB_REG_EDX:   return g_reg_context.edx;
        case GDB_REG_EBX:   return g_reg_context.ebx;
        case GDB_REG_ESP:   return g_reg_context.esp;
        case GDB_REG_EBP:   return g_reg_context.ebp;
        case GDB_REG_ESI:   return g_reg_context.esi;
        case GDB_REG_EDI:   return g_reg_context.edi;
        case GDB_REG_EIP:   return g_reg_context.eip;
        case GDB_REG_EFLAGS: return g_reg_context.eflags;
        case GDB_REG_CS:    return g_reg_context.cs;
        case GDB_REG_SS:    return g_reg_context.ss;
        case GDB_REG_DS:    return g_reg_context.ds;
        case GDB_REG_ES:    return g_reg_context.es;
        case GDB_REG_FS:    return g_reg_context.fs;
        case GDB_REG_GS:    return g_reg_context.gs;
        default: return 0;
    }
}

static void byte_to_hex(uint8_t byte, char* out) {
    out[0] = hex_chars[(byte >> 4) & 0x0F];
    out[1] = hex_chars[byte & 0x0F];
}

void gdb_registers_to_string(char* buffer, int buffer_size) {
    (void)buffer_size;
    int pos = 0;
    uint32_t* regs = (uint32_t*)&g_reg_context;

    for (int i = 0; i < GDB_REG_COUNT; i++) {
        uint32_t reg_value = regs[i];
        for (int j = 0; j < 4; j++) {
            uint8_t byte = (reg_value >> (j * 8)) & 0xFF;
            byte_to_hex(byte, buffer + pos);
            pos += 2;
        }
    }

    buffer[pos] = '\0';
}

void gdb_set_single_step(void) {
    g_reg_context.eflags |= 0x100;
}

void gdb_clear_single_step(void) {
    g_reg_context.eflags &= ~0x100;
}
