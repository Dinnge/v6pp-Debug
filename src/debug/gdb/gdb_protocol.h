#ifndef V6PP_GDB_PROTOCOL_H
#define V6PP_GDB_PROTOCOL_H

#include "gdb_socket.h"
#include "../debug.h"

// GDB 协议函数
void gdb_send_packet(char* data);
void gdb_send_ok(void);
void gdb_send_error(int code);
GDBCommand gdb_parse_command(char* packet);

// 命令处理函数
void gdb_handle_continue(char* packet);
void gdb_handle_step(char* packet);
void gdb_handle_read_registers(char* packet);
void gdb_handle_write_registers(char* packet);
void gdb_handle_read_memory(char* packet);
void gdb_handle_write_memory(char* packet);
void gdb_handle_set_breakpoint(char* packet);
void gdb_handle_remove_breakpoint(char* packet);

// Socket ����
void gdb_set_client_socket(Socket sock);
Socket gdb_get_client_socket(void);
#endif // V6PP_GDB_PROTOCOL_H
