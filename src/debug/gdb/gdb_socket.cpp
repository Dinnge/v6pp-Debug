// GDB Socket 通信层实现 - 模拟版本

#include "gdb_socket.h"
#include "../../include/Video.h"


// 初始化标志
static int g_socket_initialized = 0;
// 模拟数据缓冲区
static char g_send_buffer[DEBUG_BUFFER_SIZE];
static char g_recv_buffer[DEBUG_BUFFER_SIZE];
static int g_recv_pos = 0;
static int g_send_pos = 0;

// 初始化 socket 层
int gdb_socket_init(void) {
    if (g_socket_initialized) {
        return 0;
    }

    g_socket_initialized = 1;
    Diagnose::Write("Socket layer initialized (simulation mode)\n");
    return 0;
}

// 创建 TCP socket
Socket gdb_socket_create(void) {
    Diagnose::Write("Socket: created\n");
    return (Socket)1;
}

// 绑定端口
int gdb_socket_bind(Socket sock, int port) {
    Diagnose::Write("Socket: bound to port ");
    Diagnose::Write("1234");
    Diagnose::Write("\n");
    return 0;
}

// 监听连接
int gdb_socket_listen(Socket sock) {
    Diagnose::Write("Socket: listening\n");
    return 0;
}

// 接受连接
Socket gdb_socket_accept(Socket sock) {
    Diagnose::Write("GDB client connected (simulated)\n");
    return (Socket)2;
}

// 接收数据
int gdb_socket_recv(Socket sock, char* buffer, int size) {
    // 模拟: 检查是否有数据
    if (g_recv_pos > 0) {
        int len = g_recv_pos < size ? g_recv_pos : size;
        for (int i = 0; i < len; i++) {
            buffer[i] = g_recv_buffer[i];
        }
        g_recv_pos -= len;
        return len;
    }
    return -1;
}

// 发送数据
int gdb_socket_send(Socket sock, const char* data, int size) {
    Diagnose::Write("Socket: sending ");
    Diagnose::Write("data");
    Diagnose::Write("\n");
    return size;
}

// 关闭 socket
void gdb_socket_close(Socket sock) {
    Diagnose::Write("Socket: closed\n");
}

// 设置非阻塞
void gdb_socket_set_nonblocking(Socket sock, int nonblock) {
    // 模拟版本无需实现
}

// 检查是否有数据可读
int gdb_socket_readable(Socket sock) {
    return g_recv_pos > 0 ? 1 : 0;
}

// 模拟: 用于测试,向接收缓冲区写入数据
void gdb_socket_simulate_recv(const char* data, int len) {
    int copy_len = len < DEBUG_BUFFER_SIZE ? len : DEBUG_BUFFER_SIZE;
    for (int i = 0; i < copy_len; i++) {
        g_recv_buffer[i] = data[i];
    }
    g_recv_pos = copy_len;
}
