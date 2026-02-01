#ifndef V6PP_DEBUG_H
#define V6PP_DEBUG_H

#include "../libyrosstd/sys/types.h"

#define DEBUG_ENABLED 1
#define DEBUG_PORT 1234
#define DEBUG_BUFFER_SIZE 1024

typedef enum {
    GDB_CMD_UNKNOWN = 0,
    GDB_CMD_CONTINUE = 'c',
    GDB_CMD_STEP = 's',
    GDB_CMD_READ_REG = 'g',
    GDB_CMD_WRITE_REG = 'G',
    GDB_CMD_WRITE_SINGLE_REG = 'P',
    GDB_CMD_READ_MEM = 'm',
    GDB_CMD_WRITE_MEM = 'M',
    GDB_CMD_WRITE_MEM_BINARY = 'X',
    GDB_CMD_SET_BREAK = 'Z',
    GDB_CMD_REMOVE_BREAK = 'z',
    GDB_CMD_QUERY = 'q',
    GDB_CMD_SIGNAL = '?',
    GDB_CMD_VENDOR = 'v',
    GDB_CMD_THREAD = 'H',
} GDBCommand;

// 调试模式枚举
typedef enum
{
    DEBUG_MODE_CONTINUE = 0,
    DEBUG_MODE_STEP = 1
} DebugMode;

typedef struct {
    int enabled;
    int listening;
    int connected;
    int mode;
    char buffer[DEBUG_BUFFER_SIZE];
    int buffer_pos;
    int resume_requested;
} DebuggerState;

int debugger_init(void);
void debugger_main(void);
void gdb_send_packet(char* data);
void gdb_send_ok(void);
void gdb_send_error(int code);
// 调试器入口函数
void debugger_enter(void);

#endif
