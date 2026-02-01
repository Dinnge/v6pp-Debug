// // GDB 寄存器管理实现

// #include "gdb_registers.h"
// #include "../../include/Video.h"

// // 全局寄存器上下文
// static GDBRegisters g_reg_context;

// // 十六进制字符表
// static const char* hex_chars = "0123456789abcdef";

// // 初始化寄存器
// void gdb_registers_init(void) {
//     // 清零所有寄存器
//     for (size_t i = 0; i < sizeof(GDBRegisters); i++) {
//         ((char*)&g_reg_context)[i] = 0;
//     }
// }

// // 保存当前寄存器（使用汇编）
// void gdb_registers_save(void) {
//     // 添加诊断输出
//     Diagnose::Write("[DEBUG] gdb_registers_save called\n");
    
//     // 简单的寄存器保存，避免危险的堆栈操作
//     uint32_t esp_val, ebp_val;
    
//     __asm__ volatile (
//         "movl %%eax, %0\n\t"
//         "movl %%ecx, %1\n\t"
//         "movl %%edx, %2\n\t"
//         "movl %%ebx, %3\n\t"
//         "movl %%esp, %4\n\t"
//         "movl %%ebp, %5\n\t"
//         "movl %%esi, %6\n\t"
//         "movl %%edi, %7"
//         : "=m"(g_reg_context.eax), "=m"(g_reg_context.ecx),
//           "=m"(g_reg_context.edx), "=m"(g_reg_context.ebx),
//           "=m"(esp_val), "=m"(ebp_val),
//           "=m"(g_reg_context.esi), "=m"(g_reg_context.edi)
//         : : "memory"
//     );
    
//     // 保存ESP和EBP
//     g_reg_context.esp = esp_val;
//     g_reg_context.ebp = ebp_val;
    
//     // 安全地获取EIP：通过读取调用者的返回地址
//     // 注意：这假设函数是通过call指令调用的
//     // 对于异常处理，这不一定正确，但比pop安全
//     uint32_t eip_val = 0;
//     if (esp_val > 0x1000 && esp_val < 0xFFFFFFFF - 4) {
//         // 尝试读取返回地址（在ESP指向的位置）
//         eip_val = *((uint32_t*)esp_val);
//     }
//     g_reg_context.eip = eip_val;
    
//     // 获取EFLAGS
//     uint32_t eflags_val;
//     __asm__ volatile (
//         "pushfl\n\t"
//         "popl %0"
//         : "=r"(eflags_val)
//         : : "memory"
//     );
//     g_reg_context.eflags = eflags_val;
    
//     // 获取段寄存器
//     uint32_t cs_val, ss_val, ds_val, es_val, fs_val, gs_val;
//     __asm__ volatile (
//         "movw %%cs, %w0\n\t"
//         "movw %%ss, %w1\n\t"
//         "movw %%ds, %w2\n\t"
//         "movw %%es, %w3\n\t"
//         "movw %%fs, %w4\n\t"
//         "movw %%gs, %w5"
//         : "=r"(cs_val), "=r"(ss_val), "=r"(ds_val), 
//           "=r"(es_val), "=r"(fs_val), "=r"(gs_val)
//         : : "memory"
//     );
    
//     // 只保存低16位（段选择子）
//     g_reg_context.cs = cs_val & 0xFFFF;
//     g_reg_context.ss = ss_val & 0xFFFF;
//     g_reg_context.ds = ds_val & 0xFFFF;
//     g_reg_context.es = es_val & 0xFFFF;
//     g_reg_context.fs = fs_val & 0xFFFF;
//     g_reg_context.gs = gs_val & 0xFFFF;
    
//     Diagnose::Write("[DEBUG] gdb_registers_save completed\n");
// }

// // 恢复寄存器
// void gdb_registers_restore(void) {
//     __asm__ volatile (
//         "movl %0, %%eax\n\t"
//         "movl %1, %%ecx\n\t"
//         "movl %2, %%edx\n\t"
//         "movl %3, %%ebx\n\t"
//         "movl %4, %%esp\n\t"
//         "movl %5, %%ebp\n\t"
//         "movl %6, %%esi\n\t"
//         "movl %7, %%edi\n\t"
//         "movl %9, %%eax\n\t"
//         "pushl %%eax\n\t"
//         "popfl\n\t"
//         "pushl %8\n\t"
//         "ret"
//         : : "m"(g_reg_context.eax), "m"(g_reg_context.ecx),
//             "m"(g_reg_context.edx), "m"(g_reg_context.ebx),
//             "m"(g_reg_context.esp), "m"(g_reg_context.ebp),
//             "m"(g_reg_context.esi), "m"(g_reg_context.edi),
//             "m"(g_reg_context.eip), "m"(g_reg_context.eflags),
//             "m"(g_reg_context.cs), "m"(g_reg_context.ss),
//             "m"(g_reg_context.ds), "m"(g_reg_context.es),
//             "m"(g_reg_context.fs), "m"(g_reg_context.gs)
//         : "ax", "cx", "dx", "bx", "si", "di", "memory"
//     );
// }

// // 获取当前寄存器
// GDBRegisters* gdb_get_registers(void) {
//     return &g_reg_context;
// }

// // 设置寄存器值
// void gdb_set_register(int reg_num, uint32_t value) {
//     switch (reg_num) {
//         case GDB_REG_EAX:   g_reg_context.eax = value; break;
//         case GDB_REG_ECX:   g_reg_context.ecx = value; break;
//         case GDB_REG_EDX:   g_reg_context.edx = value; break;
//         case GDB_REG_EBX:   g_reg_context.ebx = value; break;
//         case GDB_REG_ESP:   g_reg_context.esp = value; break;
//         case GDB_REG_EBP:   g_reg_context.ebp = value; break;
//         case GDB_REG_ESI:   g_reg_context.esi = value; break;
//         case GDB_REG_EDI:   g_reg_context.edi = value; break;
//         case GDB_REG_EIP:   g_reg_context.eip = value; break;
//         case GDB_REG_EFLAGS: g_reg_context.eflags = value; break;
//         case GDB_REG_CS:    g_reg_context.cs = value; break;
//         case GDB_REG_SS:    g_reg_context.ss = value; break;
//         case GDB_REG_DS:    g_reg_context.ds = value; break;
//         case GDB_REG_ES:    g_reg_context.es = value; break;
//         case GDB_REG_FS:    g_reg_context.fs = value; break;
//         case GDB_REG_GS:    g_reg_context.gs = value; break;
//     }
// }

// // 获取寄存器值
// uint32_t gdb_get_register_value(int reg_num) {
//     switch (reg_num) {
//         case GDB_REG_EAX:   return g_reg_context.eax;
//         case GDB_REG_ECX:   return g_reg_context.ecx;
//         case GDB_REG_EDX:   return g_reg_context.edx;
//         case GDB_REG_EBX:   return g_reg_context.ebx;
//         case GDB_REG_ESP:   return g_reg_context.esp;
//         case GDB_REG_EBP:   return g_reg_context.ebp;
//         case GDB_REG_ESI:   return g_reg_context.esi;
//         case GDB_REG_EDI:   return g_reg_context.edi;
//         case GDB_REG_EIP:   return g_reg_context.eip;
//         case GDB_REG_EFLAGS: return g_reg_context.eflags;
//         case GDB_REG_CS:    return g_reg_context.cs;
//         case GDB_REG_SS:    return g_reg_context.ss;
//         case GDB_REG_DS:    return g_reg_context.ds;
//         case GDB_REG_ES:    return g_reg_context.es;
//         case GDB_REG_FS:    return g_reg_context.fs;
//         case GDB_REG_GS:    return g_reg_context.gs;
//         default: return 0;
//     }
// }

// // 将字节转换为两个十六进制字符
// static void byte_to_hex(uint8_t byte, char* out) {
//     out[0] = hex_chars[(byte >> 4) & 0x0F];
//     out[1] = hex_chars[byte & 0x0F];
// }

// // 将寄存器转换为 GDB 格式字符串
// void gdb_registers_to_string(char* buffer, int buffer_size) {
//     int pos = 0;

//     // 按照 GDB 协议顺序输出寄存器值
//     uint32_t* regs = (uint32_t*)&g_reg_context;
//     for (int i = 0; i < GDB_REG_COUNT; i++) {
//         uint32_t reg_value = regs[i];
//         // 小端序：低字节在前
//         for (int j = 0; j < 4; j++) {
//             uint8_t byte = (reg_value >> (j * 8)) & 0xFF;
//             byte_to_hex(byte, buffer + pos);
//             pos += 2;
//         }
//     }

//     buffer[pos] = '\0';
// }

// // 设置单步执行标志（TF flag in EFLAGS）
// void gdb_set_single_step(void) {
//     g_reg_context.eflags |= 0x100;  // Set TF (Trap Flag)
// }

// // 清除单步执行标志
// void gdb_clear_single_step(void) {
//     g_reg_context.eflags &= ~0x100;  // Clear TF (Trap Flag)
// }




// GDB 寄存器管理实现

#include "gdb_registers.h"
#include "../../include/Video.h"

// 全局寄存器上下文
static GDBRegisters g_reg_context;
// 软件上下文是否已由保存或写入填充（1 = 有效）
static int g_reg_context_valid = 0;

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
    // 添加诊断输出
    Diagnose::Write("[DEBUG] gdb_registers_save called\n");
    
    // 如果上下文已经有效，则不要覆盖（避免覆盖由 gdb_set_register 写入的软件上下文）
    if (g_reg_context_valid) {
        Diagnose::Write("[DEBUG] gdb_registers_save: context already valid, skip saving\n");
        return;
    }

    // 尝试安全地保存寄存器值
    // 通用寄存器
    __asm__ volatile ("movl %%eax, %0" : "=m"(g_reg_context.eax));
    __asm__ volatile ("movl %%ecx, %0" : "=m"(g_reg_context.ecx));
    __asm__ volatile ("movl %%edx, %0" : "=m"(g_reg_context.edx));
    __asm__ volatile ("movl %%ebx, %0" : "=m"(g_reg_context.ebx));
    __asm__ volatile ("movl %%esp, %0" : "=m"(g_reg_context.esp));
    __asm__ volatile ("movl %%ebp, %0" : "=m"(g_reg_context.ebp));
    __asm__ volatile ("movl %%esi, %0" : "=m"(g_reg_context.esi));
    __asm__ volatile ("movl %%edi, %0" : "=m"(g_reg_context.edi));
    
    // 段寄存器
    __asm__ volatile ("movw %%cs, %0" : "=m"(g_reg_context.cs));
    __asm__ volatile ("movw %%ss, %0" : "=m"(g_reg_context.ss));
    __asm__ volatile ("movw %%ds, %0" : "=m"(g_reg_context.ds));
    __asm__ volatile ("movw %%es, %0" : "=m"(g_reg_context.es));
    __asm__ volatile ("movw %%fs, %0" : "=m"(g_reg_context.fs));
    __asm__ volatile ("movw %%gs, %0" : "=m"(g_reg_context.gs));
    
    // EIP 和 EFLAGS 需要从异常帧中获取
    // 暂时设为默认值，但至少通用寄存器是真实的
    // g_reg_context.eip = 0x100000;  // 内核入口点（占位符）
    // g_reg_context.eflags = 0x202;  // IF=1（占位符）
    // 使用准确的汇编代码获取 EIP
    __asm__ __volatile__ (
        "call 1f\n\t"
        "1: popl %0\n\t"
        : "=r"(g_reg_context.eip)
    );
    
    // 使用准确的汇编代码获取 EFLAGS
    __asm__ __volatile__ (
        "pushfl\n\t"
        "popl %0\n\t"
        : "=r"(g_reg_context.eflags)
    );
    // 标记上下文为有效，避免后续无意覆盖
    // g_reg_context_valid = 1;
}

// 在进入新的调试上下文（trap/断点）时调用，强制下一次保存从物理寄存器刷新
void gdb_registers_invalidate(void) {
    g_reg_context_valid = 0;
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
            {
                uint32_t check;
                __asm__ __volatile__("movl %%eax, %0" : "=r"(check));
                Diagnose::Write("[DEBUG] physical EAX after write = 0x%08x\n", check);
            }
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
            {
                uint32_t check;
                __asm__ __volatile__("pushfl\n\tpopl %0" : "=r"(check));
                Diagnose::Write("[DEBUG] physical EFLAGS after write = 0x%08x\n", check);
            }
            break;
        case GDB_REG_ESP:
            Diagnose::Write("[GDB] 警告: 写 ESP 请求已记录，仅更新软件上下文\n");
            break;
        case GDB_REG_EBP:
            Diagnose::Write("[GDB] 警告: 写 EBP 请求已记录，仅更新软件上下文\n");
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
    
    if (reg_num >= 0 && reg_num < 16) {
        Diagnose::Write("[DEBUG] 设置寄存器 %s = 0x%08x\n", reg_names[reg_num], value);
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

// 设置单步执行标志（TF flag in EFLAGS）
void gdb_set_single_step(void) {
    g_reg_context.eflags |= 0x100;  // Set TF (Trap Flag)
}

// 清除单步执行标志
void gdb_clear_single_step(void) {
    g_reg_context.eflags &= ~0x100;  // Clear TF (Trap Flag)
}
