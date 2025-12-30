#ifndef V6PP_GDB_SOCKET_H
#define V6PP_GDB_SOCKET_H

#include "../debug.h"

// Socket 定义
typedef int Socket;

// 初始化 socket 层
int gdb_socket_init(void);

// 创建 TCP socket
Socket gdb_socket_create(void);

// 绑定端口
int gdb_socket_bind(Socket sock, int port);

// 监听连接
int gdb_socket_listen(Socket sock);

// 接受连接
Socket gdb_socket_accept(Socket sock);

// 接收数据
int gdb_socket_recv(Socket sock, char* buffer, int size);

// 发送数据
int gdb_socket_send(Socket sock, const char* data, int size);

// 关闭 socket
void gdb_socket_close(Socket sock);

// 非阻塞设置
void gdb_socket_set_nonblocking(Socket sock, int nonblock);

// 检查是否有数据可读
int gdb_socket_readable(Socket sock);

#endif // V6PP_GDB_SOCKET_H
