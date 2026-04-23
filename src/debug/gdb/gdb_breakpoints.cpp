// GDB 断点管理实现

#include "gdb_breakpoints.h"
#include "../../include/Video.h"

// 断点表
static GDBBreakpoint g_breakpoints[GDB_MAX_BREAKPOINTS];
static int g_breakpoint_count = 0;
static GDBBreakpoint* g_pending_resume_breakpoint = 0;
static int g_pending_resume_auto_continue = 0;

// 断点指令 (int 3)
#define BREAKPOINT_INSTRUCTION 0xCC

// 初始化断点表
void gdb_breakpoints_init(void) {
    g_breakpoint_count = 0;
    g_pending_resume_breakpoint = 0;
    g_pending_resume_auto_continue = 0;
    for (int i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
        g_breakpoints[i].addr = 0;
        g_breakpoints[i].type = GDB_BREAK_SOFTWARE;
        g_breakpoints[i].orig_byte = 0;
        g_breakpoints[i].enabled = 0;
        g_breakpoints[i].inserted = 0;
    }
}

static int gdb_insert_breakpoint(GDBBreakpoint* bp) {
    if (!bp || !bp->enabled || bp->inserted) {
        return 0;
    }

    uint8_t* mem_ptr = (uint8_t*)bp->addr;
    *mem_ptr = BREAKPOINT_INSTRUCTION;
    bp->inserted = 1;
    return 0;
}

static int gdb_restore_breakpoint_byte(GDBBreakpoint* bp) {
    if (!bp || !bp->inserted) {
        return 0;
    }

    uint8_t* mem_ptr = (uint8_t*)bp->addr;
    *mem_ptr = bp->orig_byte;
    bp->inserted = 0;
    return 0;
}

// 添加断点
int gdb_add_breakpoint(uint32_t addr, GDBBreakpointType type) {
    // 检查是否已存在
    if (gdb_find_breakpoint(addr) != NULL) {
        return 0;  // 已存在
    }

    // 内核态统一用 INT3 模拟执行断点，兼容 Z0/Z1 两种执行断点请求。
    if (type != GDB_BREAK_SOFTWARE && type != GDB_BREAK_HARDWARE) {
        return -2;  // 不支持的断点类型
    }

    GDBBreakpoint* slot = 0;
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (!g_breakpoints[i].enabled) {
            slot = &g_breakpoints[i];
            break;
        }
    }
    if (!slot) {
        if (g_breakpoint_count >= GDB_MAX_BREAKPOINTS) {
            return -1;  // 断点表满
        }
        slot = &g_breakpoints[g_breakpoint_count++];
    }

    uint8_t* mem_ptr = (uint8_t*)addr;
    slot->addr = addr;
    slot->type = type;
    slot->orig_byte = *mem_ptr;
    slot->enabled = 1;
    slot->inserted = 0;
    gdb_insert_breakpoint(slot);

    return 0;
}

// 移除断点
int gdb_remove_breakpoint(uint32_t addr) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (g_breakpoints[i].addr == addr && g_breakpoints[i].enabled) {
            gdb_restore_breakpoint_byte(&g_breakpoints[i]);

            g_breakpoints[i].enabled = 0;
            g_breakpoints[i].inserted = 0;
            if (g_pending_resume_breakpoint == &g_breakpoints[i]) {
                g_pending_resume_breakpoint = 0;
                g_pending_resume_auto_continue = 0;
            }

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
        if (g_breakpoints[i].enabled && g_breakpoints[i].addr != 0) {
            gdb_insert_breakpoint(&g_breakpoints[i]);
        }
    }
}

// 禁用所有断点
void gdb_disable_breakpoints(void) {
    for (int i = 0; i < g_breakpoint_count; i++) {
        if (g_breakpoints[i].enabled && g_breakpoints[i].addr != 0) {
            gdb_restore_breakpoint_byte(&g_breakpoints[i]);
        }
    }
}

int gdb_prepare_breakpoint_resume(uint32_t addr) {
    GDBBreakpoint* bp = gdb_find_breakpoint(addr);
    if (!bp) {
        return -1;
    }

    gdb_restore_breakpoint_byte(bp);
    g_pending_resume_breakpoint = bp;
    g_pending_resume_auto_continue = 0;
    return 0;
}

int gdb_has_pending_breakpoint_resume(void) {
    return g_pending_resume_breakpoint != 0;
}

void gdb_set_pending_breakpoint_auto_continue(int auto_continue) {
    g_pending_resume_auto_continue = auto_continue ? 1 : 0;
}

int gdb_get_pending_breakpoint_auto_continue(void) {
    return g_pending_resume_auto_continue;
}

int gdb_finish_breakpoint_resume(void) {
    if (!g_pending_resume_breakpoint) {
        return -1;
    }

    gdb_insert_breakpoint(g_pending_resume_breakpoint);
    g_pending_resume_breakpoint = 0;
    g_pending_resume_auto_continue = 0;
    return 0;
}

void gdb_cancel_pending_breakpoint_resume(void) {
    g_pending_resume_breakpoint = 0;
    g_pending_resume_auto_continue = 0;
}
