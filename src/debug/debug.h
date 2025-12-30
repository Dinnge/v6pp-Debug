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
    GDB_CMD_READ_MEM = 'm',
    GDB_CMD_WRITE_MEM = 'M',
    GDB_CMD_SET_BREAK = 'Z',
    GDB_CMD_REMOVE_BREAK = 'z',
    GDB_CMD_QUERY = 'q',
} GDBCommand;

typedef struct {
    int enabled;
    int listening;
    int connected;
    char buffer[DEBUG_BUFFER_SIZE];
    int buffer_pos;
} DebuggerState;

int debugger_init(void);
void debugger_main(void);
void gdb_send_packet(char* data);
void gdb_send_ok(void);
void gdb_send_error(int code);

#endif
