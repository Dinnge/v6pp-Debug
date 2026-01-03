// GDB 寄存器管理头文件

#ifndef GDB_REGISTERS_H
#define GDB_REGISTERS_H

#include "../../libyrosstd/sys/types.h"

// x86 寄存器定义
typedef struct {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;
    uint32_t eflags;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
} GDBRegisters;

// GDB 寄存器顺序（按照 GDB 协议要求的顺序）
// 顺序: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
//       EIP, EFLAGS, CS, SS, DS, ES, FS, GS
#define GDB_REG_EAX   0
#define GDB_REG_ECX   1
#define GDB_REG_EDX   2
#define GDB_REG_EBX   3
#define GDB_REG_ESP   4
#define GDB_REG_EBP   5
#define GDB_REG_ESI   6
#define GDB_REG_EDI   7
#define GDB_REG_EIP   8
#define GDB_REG_EFLAGS 9
#define GDB_REG_CS    10
#define GDB_REG_SS    11
#define GDB_REG_DS    12
#define GDB_REG_ES    13
#define GDB_REG_FS    14
#define GDB_REG_GS    15
#define GDB_REG_COUNT 16

// 寄存器管理函数
void gdb_registers_init(void);
void gdb_registers_save(void);
void gdb_registers_restore(void);

// 获取当前寄存器
GDBRegisters* gdb_get_registers(void);

// 设置寄存器值
void gdb_set_register(int reg_num, uint32_t value);
uint32_t gdb_get_register_value(int reg_num);

// 将寄存器转换为 GDB 格式字符串
void gdb_registers_to_string(char* buffer, int buffer_size);

#endif // GDB_REGISTERS_H
