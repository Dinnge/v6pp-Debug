// GDB Socket 通信层实现

#include "gdb_socket.h"
#include "../../include/Video.h"

// Windows/Linux socket 差异
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VAL -1
    #define SOCKET_ERROR_VAL -1
    #define CLOSE_SOCKET close
#endif

// 初始化标志
static int g_socket_initialized = 0;

// 初始化 socket 层
int gdb_socket_init(void) {
    if (g_socket_initialized) {
        return 0;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Diagnose::Write("WSAStartup failed\n");
        return -1;
    }
#endif

    g_socket_initialized = 1;
    Diagnose::Write("Socket layer initialized\n");
    return 0;
}

// 创建 TCP socket
Socket gdb_socket_create(void) {
    Socket sock = (Socket)socket(AF_INET, SOCK_STREAM, 0);
    if ((int)sock == INVALID_SOCKET_VAL) {
        Diagnose::Write("Failed to create socket\n");
        return (Socket)-1;
    }
    return sock;
}

// 绑定端口
int gdb_socket_bind(Socket sock, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind((socket_t)sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL) {
        Diagnose::Write("Failed to bind socket\n");
        return -1;
    }
    return 0;
}

// 监听连接
int gdb_socket_listen(Socket sock) {
    if (listen((socket_t)sock, 1) == SOCKET_ERROR_VAL) {
        Diagnose::Write("Failed to listen on socket\n");
        return -1;
    }
    return 0;
}

// 接受连接
Socket gdb_socket_accept(Socket sock) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    Socket client = (Socket)accept((socket_t)sock, (struct sockaddr*)&client_addr, &addr_len);
    if ((int)client == INVALID_SOCKET_VAL) {
        return (Socket)-1;
    }

    Diagnose::Write("GDB client connected\n");
    return client;
}

// 接收数据
int gdb_socket_recv(Socket sock, char* buffer, int size) {
    int recv_len = recv((socket_t)sock, buffer, size, 0);
    if (recv_len <= 0) {
        return -1;
    }
    return recv_len;
}

// 发送数据
int gdb_socket_send(Socket sock, const char* data, int size) {
    int sent = send((socket_t)sock, data, size, 0);
    if (sent <= 0) {
        return -1;
    }
    return sent;
}

// 关闭 socket
void gdb_socket_close(Socket sock) {
    CLOSE_SOCKET((socket_t)sock);
}

// 设置非阻塞
void gdb_socket_set_nonblocking(Socket sock, int nonblock) {
#ifdef _WIN32
    unsigned long mode = nonblock ? 1 : 0;
    ioctlsocket((socket_t)sock, FIONBIO, &mode);
#else
    int flags = fcntl((socket_t)sock, F_GETFL, 0);
    fcntl((socket_t)sock, F_SETFL, nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

// 检查是否有数据可读
int gdb_socket_readable(Socket sock) {
    fd_set read_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_SET((socket_t)sock, &read_fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int result = select((int)sock + 1, &read_fds, NULL, NULL, &tv);
    return result > 0 ? 1 : 0;
}
