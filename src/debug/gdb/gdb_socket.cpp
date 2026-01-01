// GDB Socket 通信层实现 - 串口版本
// 使用 8250 UART 串口实现 GDB 远程调试协议

#include "gdb_socket.h"
#include "gdb_serial.h"
#include "../../include/Video.h"

// 初始化标志
static int g_socket_initialized = 0;
static Socket g_active_socket = 0;

// 初始化 socket 层（实际上是串口）
int gdb_socket_init(void) {
    if (g_socket_initialized) {
        return 0;
    }

    // 初始化串口
    if (serial_init() != 0) {
        Diagnose::Write("Failed to initialize serial port\n");
        return -1;
    }

    g_socket_initialized = 1;
    g_active_socket = 1;
    Diagnose::Write("Socket layer initialized (serial mode)\n");
    return 0;
}

// 创建 socket（实际上是准备串口）
Socket gdb_socket_create(void) {
    if (!g_socket_initialized) {
        return (Socket)-1;
    }
    Diagnose::Write("Socket: serial port ready\n");
    return g_active_socket;
}

// 绑定端口（串口不使用端口，保留接口）
int gdb_socket_bind(Socket sock, int port) {
    Diagnose::Write("Socket: using COM1 (no port binding needed)\n");
    return 0;
}

// 监听连接（串口直接可用）
int gdb_socket_listen(Socket sock) {
    Diagnose::Write("Socket: listening on COM1\n");
    return 0;
}

// 接受连接（串口没有连接概念，直接返回）
Socket gdb_socket_accept(Socket sock) {
    Diagnose::Write("Socket: serial connection accepted (waiting for GDB client)\n");
    return g_active_socket;
}

// 接收数据（从串口接收）
int gdb_socket_recv(Socket sock, char* buffer, int size) {
    if (!g_socket_initialized) {
        return -1;
    }

    // 尝试接收数据包（GDB 协议格式）
    int len = serial_recv_packet(buffer, size);

    if (len > 0) {
        Diagnose::Write("Socket: received packet (");
        char len_str[16];
        int i = 0;
        int tmp = len;
        do {
            len_str[i++] = '0' + (tmp % 10);
            tmp /= 10;
        } while (tmp > 0);
        // 反转字符串
        for (int j = 0; j < i / 2; j++) {
            char t = len_str[j];
            len_str[j] = len_str[i - j - 1];
            len_str[i - j - 1] = t;
        }
        len_str[i] = '\0';
        Diagnose::Write(len_str);
        Diagnose::Write(" bytes)\n");
    }

    return len;
}

// 发送数据（通过串口发送）
int gdb_socket_send(Socket sock, const char* data, int size) {
    if (!g_socket_initialized) {
        return -1;
    }

    // 直接发送数据
    int sent = serial_write(data, size);

    Diagnose::Write("Socket: sent ");
    char len_str[16];
    int i = 0;
    int tmp = sent;
    do {
        len_str[i++] = '0' + (tmp % 10);
        tmp /= 10;
    } while (tmp > 0);
    // 反转字符串
    for (int j = 0; j < i / 2; j++) {
        char t = len_str[j];
        len_str[j] = len_str[i - j - 1];
        len_str[i - j - 1] = t;
    }
    len_str[i] = '\0';
    Diagnose::Write(len_str);
    Diagnose::Write(" bytes\n");

    return sent;
}

// 关闭 socket（串口无需关闭）
void gdb_socket_close(Socket sock) {
    Diagnose::Write("Socket: closed (serial port remains active)\n");
}

// 非阻塞设置（串口始终非阻塞）
void gdb_socket_set_nonblocking(Socket sock, int nonblock) {
    // 串口接收函数本来就是非阻塞的，无需设置
}

// 检查是否有数据可读
int gdb_socket_readable(Socket sock) {
    return serial_readable();
}

