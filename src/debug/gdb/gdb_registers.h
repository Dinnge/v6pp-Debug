#ifndef GDB_REGISTERS_H
#define GDB_REGISTERS_H

#include "../../libyrosstd/sys/types.h"
#include "../../include/Regs.h"

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

void gdb_registers_init(void);
void gdb_registers_save(void);
void gdb_registers_restore(void);
void gdb_registers_invalidate(void);
void gdb_registers_bind_trap_frame(struct pt_regs* regs, struct pt_context* context);
void gdb_registers_commit_to_trap_frame(void);

GDBRegisters* gdb_get_registers(void);
void gdb_set_register(int reg_num, uint32_t value);
uint32_t gdb_get_register_value(int reg_num);
int is_reg_context_valid(void);

void gdb_registers_to_string(char* buffer, int buffer_size);

void gdb_set_single_step(void);
void gdb_clear_single_step(void);

#endif
