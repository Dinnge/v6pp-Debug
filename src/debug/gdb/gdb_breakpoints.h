// GDB 断点管理头文件

#ifndef GDB_BREAKPOINTS_H
#define GDB_BREAKPOINTS_H

#include "../../libyrosstd/sys/types.h"

// 断点类型
typedef enum {
    GDB_BREAK_SOFTWARE = 0,  // 软件断点 (0xCC)
    GDB_BREAK_HARDWARE = 1,   // 硬件断点
    GDB_BREAK_WRITEWATCH = 2,  // 写观察点
    GDB_BREAK_READWATCH = 3,   // 读观察点
    GDB_BREAK_ACCESSWATCH = 4  // 访问观察点
} GDBBreakpointType;

// 断点信息
typedef struct {
    uint32_t addr;           // 断点地址
    GDBBreakpointType type;  // 断点类型
    uint8_t orig_byte;       // 原始字节（用于恢复）
    int enabled;             // 是否启用
    int inserted;            // 是否已经写入 INT3
} GDBBreakpoint;

// 最大断点数量
#define GDB_MAX_BREAKPOINTS 16

// 断点管理函数
void gdb_breakpoints_init(void);

// 添加断点
int gdb_add_breakpoint(uint32_t addr, GDBBreakpointType type);

// 移除断点
int gdb_remove_breakpoint(uint32_t addr);

// 查找断点
GDBBreakpoint* gdb_find_breakpoint(uint32_t addr);

// 启用所有断点
void gdb_enable_breakpoints(void);

// 禁用所有断点
void gdb_disable_breakpoints(void);

int gdb_prepare_breakpoint_resume(uint32_t addr);
int gdb_has_pending_breakpoint_resume(void);
void gdb_set_pending_breakpoint_auto_continue(int auto_continue);
int gdb_get_pending_breakpoint_auto_continue(void);
int gdb_finish_breakpoint_resume(void);
void gdb_cancel_pending_breakpoint_resume(void);

#endif // GDB_BREAKPOINTS_H
