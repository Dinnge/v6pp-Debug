#ifndef V6PP_GDB_SOCKET_H
#define V6PP_GDB_SOCKET_H

#include "../debug.h"

// Socket 定义（使用串口抽象）
typedef int Socket;

// 初始化 socket 层（实际上是串口）
int gdb_socket_init(void);

// 创建 socket（实际上是准备串口）
Socket gdb_socket_create(void);

// 绑定端口（串口不使用端口，保留接口）
int gdb_socket_bind(Socket sock, int port);

// 监听连接（串口直接可用）
int gdb_socket_listen(Socket sock);

// 接受连接（串口没有连接概念，直接返回）
Socket gdb_socket_accept(Socket sock);

// 接收数据（从串口接收）
int gdb_socket_recv(Socket sock, char* buffer, int size);

// 发送数据（通过串口发送）
int gdb_socket_send(Socket sock, const char* data, int size);

// 关闭 socket（串口无需关闭）
void gdb_socket_close(Socket sock);

// 非阻塞设置（串口始终非阻塞）
void gdb_socket_set_nonblocking(Socket sock, int nonblock);

// 检查是否有数据可读
int gdb_socket_readable(Socket sock);

#endif // V6PP_GDB_SOCKET_H
