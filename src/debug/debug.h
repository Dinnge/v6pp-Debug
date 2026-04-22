#ifndef V6PP_DEBUG_H
#define V6PP_DEBUG_H

#include "../libyrosstd/sys/types.h"

struct pt_regs;
struct pt_context;

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
    GDB_CMD_DETACH = 'D',
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

typedef enum {
    DEBUG_MODE_NONE = 0,
    DEBUG_MODE_CONTINUE,
    DEBUG_MODE_STEP
} DebugMode;

typedef enum {
    DEBUG_TRAP_UNKNOWN = 0,
    DEBUG_TRAP_DEBUG = 1,
    DEBUG_TRAP_BREAKPOINT = 3
} DebugTrapType;

typedef struct {
    int enabled;
    int listening;
    int connected;
    DebugMode mode;
    char buffer[DEBUG_BUFFER_SIZE];
    int buffer_pos;
    int resume_requested;
    int target_running;
} DebuggerState;

int debugger_init(void);
void debugger_main(void);
void gdb_send_packet(char* data);
void gdb_send_ok(void);
void gdb_send_error(int code);
void debugger_enter(DebugTrapType trap_type, struct pt_regs* regs, struct pt_context* context);
extern "C" void kernel_debug_checkpoint(void);
#ifdef EARLY_BOOT_GDB
extern "C" uint32_t g_kernel_debug_saved_esp;
extern "C" uint8_t g_kernel_debug_stack[16 * 1024];
extern "C" void kernel_debug_checkpoint_body(void);
#define EARLY_BOOT_GDB_CHECKPOINT() kernel_debug_checkpoint()
#else
#define EARLY_BOOT_GDB_CHECKPOINT() do { } while (0)
#endif
void monitor_execution_mode(void);
int debugger_is_target_running(void);
void debugger_set_target_running(int running);

#endif
