#include "gdb_protocol.h"
// #include <string.h>
// #include <stdio.h>

void gdb_send_packet(char* data) {
}

void gdb_send_ok(void) {
    gdb_send_packet("OK");
}

void gdb_send_error(int code) {
}

GDBCommand gdb_parse_command(char* packet) {
    if (!packet) return GDB_CMD_UNKNOWN;
    char cmd = packet[0];
    switch (cmd) {
        case 'c': return GDB_CMD_CONTINUE;
        case 's': return GDB_CMD_STEP;
        case 'g': return GDB_CMD_READ_REG;
        case 'G': return GDB_CMD_WRITE_REG;
        case 'm': return GDB_CMD_READ_MEM;
        case 'M': return GDB_CMD_WRITE_MEM;
        case 'Z': return GDB_CMD_SET_BREAK;
        case 'z': return GDB_CMD_REMOVE_BREAK;
        case 'q': return GDB_CMD_QUERY;
        default: return GDB_CMD_UNKNOWN;
    }
}

// 全局 socket 变量
static Socket g_client_socket = (Socket)-1;

// 设置当前连接的客户端 socket
void gdb_set_client_socket(Socket sock) {
    g_client_socket = sock;
}

// 获取当前客户端 socket
Socket gdb_get_client_socket(void) {
    return g_client_socket;
}

void gdb_handle_continue(char* p) { gdb_send_ok(); }
void gdb_handle_step(char* p) { gdb_send_ok(); }
void gdb_handle_read_registers(char* p) { gdb_send_packet("00"); }
void gdb_handle_write_registers(char* p) { gdb_send_ok(); }
void gdb_handle_read_memory(char* p) { gdb_send_packet("00"); }
void gdb_handle_write_memory(char* p) { gdb_send_ok(); }
void gdb_handle_set_breakpoint(char* p) { gdb_send_ok(); }
void gdb_handle_remove_breakpoint(char* p) { gdb_send_ok(); }
