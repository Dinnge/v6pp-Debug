// GDB 断点管理实现

#include "gdb_breakpoints.h"
#include "../../include/Video.h"

// 断点表
static GDBBreakpoint g_breakpoints[GDB_MAX_BREAKPOINTS];
static int g_breakpoint_count = 0;

// 断点指令 (int 3)
#define BREAKPOINT_INSTRUCTION 0xCC

// 初始化断点表
void gdb_breakpoints_init(void) {
    g_breakpoint_count = 0;
    for (int i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
        g_breakpoints[i].addr = 0;
        g_breakpoints[i].type = GDB_BREAK_SOFTWARE;
        g_breakpoints[i].orig_byte = 0;
        g_breakpoints[i].enabled = 0;
    }
}

// 添加断点
int gdb_add_breakpoint(uint32_t addr, GDBBreakpointType type) {
    // 检查是否已存在
    if (gdb_find_breakpoint(addr) != NULL) {
        return 0;  // 已存在
    }

    // 检查是否达到最大数量
    if (g_breakpoint_count >= GDB_MAX_BREAKPOINTS) {
        return -1;  // 断点表满
    }

    // 只支持软件断点
    if (type != GDB_BREAK_SOFTWARE) {
        return -2;  // 不支持的断点类型
    }

    // 保存原始字节
    uint8_t* mem_ptr = (uint8_t*)addr;
    g_breakpoints[g_breakpoint_count].orig_byte = *mem_ptr;

    // 写入断点指令
    *mem_ptr = BREAKPOINT_INSTRUCTION;

    // 更新断点信息
    g_breakpoints[g_breakpoint_count].addr = addr;
    g_breakpoints[g_breakpoint_count].type = type;
    g_breakpoints[g_breakpoint_count].enabled = 1;
    g_breakpoint_count++;

    Diagnose::Write("Breakpoint added at ");
    Diagnose::Write("0x");
    // 简化：不打印地址
    Diagnose::Write("\n");

    return 0;
}

// 移除断点
int gdb_remove_breakpoint(uint32_t addr) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (g_breakpoints[i].addr == addr && g_breakpoints[i].enabled) {
            // 恢复原始字节
            uint8_t* mem_ptr = (uint8_t*)addr;
            *mem_ptr = g_breakpoints[i].orig_byte;

            // 标记为禁用
            g_breakpoints[i].enabled = 0;

            Diagnose::Write("Breakpoint removed at ");
            Diagnose::Write("0x");
            Diagnose::Write("\n");

            return 0;
        }
    }

    return -1;  // 断点不存在
}

// 查找断点
GDBBreakpoint* gdb_find_breakpoint(uint32_t addr) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (g_breakpoints[i].addr == addr && g_breakpoints[i].enabled) {
            return &g_breakpoints[i];
        }
    }
    return NULL;
}

// 启用所有断点
void gdb_enable_breakpoints(void) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (!g_breakpoints[i].enabled && g_breakpoints[i].addr != 0) {
            uint8_t* mem_ptr = (uint8_t*)g_breakpoints[i].addr;
            g_breakpoints[i].orig_byte = *mem_ptr;
            *mem_ptr = BREAKPOINT_INSTRUCTION;
            g_breakpoints[i].enabled = 1;
        }
    }
}

// 禁用所有断点
void gdb_disable_breakpoints(void) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (g_breakpoints[i].enabled && g_breakpoints[i].addr != 0) {
            uint8_t* mem_ptr = (uint8_t*)g_breakpoints[i].addr;
            *mem_ptr = g_breakpoints[i].orig_byte;
            g_breakpoints[i].enabled = 0;
        }
    }
}
