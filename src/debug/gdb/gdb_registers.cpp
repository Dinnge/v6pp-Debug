// GDB 寄存器管理实现

#include "gdb_registers.h"
#include "../../include/Video.h"

// 全局寄存器上下文
static GDBRegisters g_reg_context;

// 十六进制字符表
static const char* hex_chars = "0123456789abcdef";

// 初始化寄存器
void gdb_registers_init(void) {
    // 清零所有寄存器
    for (size_t i = 0; i < sizeof(GDBRegisters); i++) {
        ((char*)&g_reg_context)[i] = 0;
    }
}

// 保存当前寄存器（使用汇编）
void gdb_registers_save(void) {
    __asm__ volatile (
        "movl %%eax, %0\n\t"
        "movl %%ecx, %1\n\t"
        "movl %%edx, %2\n\t"
        "movl %%ebx, %3\n\t"
        "movl %%esp, %4\n\t"
        "movl %%ebp, %5\n\t"
        "movl %%esi, %6\n\t"
        "movl %%edi, %7\n\t"
        "popl %%eax\n\t"
        "movl %%eax, %8\n\t"
        "pushfl\n\t"
        "popl %%eax\n\t"
        "movl %%eax, %9\n\t"
        "movw %%cs, %%ax\n\t"
        "movl %%eax, %10\n\t"
        "movw %%ss, %%ax\n\t"
        "movl %%eax, %11\n\t"
        "movw %%ds, %%ax\n\t"
        "movl %%eax, %12\n\t"
        "movw %%es, %%ax\n\t"
        "movl %%eax, %13\n\t"
        "movw %%fs, %%ax\n\t"
        "movl %%eax, %14\n\t"
        "movw %%gs, %%ax\n\t"
        "movl %%eax, %15"
        : "=m"(g_reg_context.eax), "=m"(g_reg_context.ecx),
          "=m"(g_reg_context.edx), "=m"(g_reg_context.ebx),
          "=m"(g_reg_context.esp), "=m"(g_reg_context.ebp),
          "=m"(g_reg_context.esi), "=m"(g_reg_context.edi),
          "=m"(g_reg_context.eip), "=m"(g_reg_context.eflags),
          "=m"(g_reg_context.cs), "=m"(g_reg_context.ss),
          "=m"(g_reg_context.ds), "=m"(g_reg_context.es),
          "=m"(g_reg_context.fs), "=m"(g_reg_context.gs)
        : : "ax", "cx", "dx", "bx", "si", "di"
    );
}

// 恢复寄存器
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

// 获取当前寄存器
GDBRegisters* gdb_get_registers(void) {
    return &g_reg_context;
}

// 设置寄存器值
void gdb_set_register(int reg_num, uint32_t value) {
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
}

// 获取寄存器值
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

// 将字节转换为两个十六进制字符
static void byte_to_hex(uint8_t byte, char* out) {
    out[0] = hex_chars[(byte >> 4) & 0x0F];
    out[1] = hex_chars[byte & 0x0F];
}

// 将寄存器转换为 GDB 格式字符串
void gdb_registers_to_string(char* buffer, int buffer_size) {
    int pos = 0;

    // 按照 GDB 协议顺序输出寄存器值
    uint32_t* regs = (uint32_t*)&g_reg_context;
    for (int i = 0; i < GDB_REG_COUNT; i++) {
        uint32_t reg_value = regs[i];
        // 小端序：低字节在前
        for (int j = 0; j < 4; j++) {
            uint8_t byte = (reg_value >> (j * 8)) & 0xFF;
            byte_to_hex(byte, buffer + pos);
            pos += 2;
        }
    }

    buffer[pos] = '\0';
}