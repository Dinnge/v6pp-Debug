// 调试器主循环实现

#include "../debug.h"
#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "../../include/Video.h"

// 调试器全局状态
static DebuggerState g_debugger = {0};
static Socket g_listen_socket = (Socket)-1;

// 初始化调试器
int debugger_init(void) {
    g_debugger.enabled = 1;
    g_debugger.listening = 0;
    g_debugger.connected = 0;
    g_debugger.buffer_pos = 0;

    // 初始化 socket 层
    if (gdb_socket_init() != 0) {
        Diagnose::Write("Failed to initialize socket layer\n");
        return -1;
    }

    // 创建监听 socket
    g_listen_socket = gdb_socket_create();
    if (g_listen_socket == (Socket)-1) {
        Diagnose::Write("Failed to create listen socket\n");
        return -1;
    }

    // 绑定端口
    if (gdb_socket_bind(g_listen_socket, DEBUG_PORT) != 0) {
        Diagnose::Write("Failed to bind to port ");
        Diagnose::Write("1234");
        Diagnose::Write("\n");
        return -1;
    }

    // 开始监听
    if (gdb_socket_listen(g_listen_socket) != 0) {
        Diagnose::Write("Failed to listen on socket\n");
        return -1;
    }

    // 设置为非阻塞
    gdb_socket_set_nonblocking(g_listen_socket, 1);

    g_debugger.listening = 1;
    Diagnose::Write("Debugger initialized\n");
    Diagnose::Write("Listening on port ");
    Diagnose::Write("1234");
    Diagnose::Write("...\n");

    return 0;
}

// 解析 GDB 数据包
static int gdb_parse_packet(char* buffer, int len, char* packet) {
    // GDB 数据包格式: $数据#校验和
    int i = 0;
    int packet_len = 0;

    while (i < len) {
        if (buffer[i] == '$') {
            // 找到数据包开始
            i++;
            while (i < len && buffer[i] != '#') {
                packet[packet_len++] = buffer[i++];
            }
            if (i < len) {
                packet[packet_len] = '\0';
                return packet_len;
            }
        } else if (buffer[i] == '+') {
            // ACK
            i++;
        } else if (buffer[i] == '-') {
            // NACK,需要重发
            i++;
        } else {
            i++;
        }
    }

    return 0;
}

// 调试器主循环
void debugger_main(void) {
    Diagnose::Write("Debugger main loop started\n");

    char recv_buffer[DEBUG_BUFFER_SIZE];
    char packet_buffer[DEBUG_BUFFER_SIZE];

    while (g_debugger.enabled) {
        // 接受连接
        if (!g_debugger.connected && g_debugger.listening) {
            Socket client = gdb_socket_accept(g_listen_socket);
            if (client != (Socket)-1) {
                g_debugger.connected = 1;
                gdb_set_client_socket(client);
                Diagnose::Write("GDB client connected!\n");
            }
        }

        // 接收数据
        if (g_debugger.connected) {
            Socket client = gdb_get_client_socket();
            if (client != (Socket)-1) {
                int recv_len = gdb_socket_recv(client, recv_buffer, DEBUG_BUFFER_SIZE);
                if (recv_len > 0) {
                    Diagnose::Write("Received packet\n");

                    // 解析数据包
                    int packet_len = gdb_parse_packet(recv_buffer, recv_len, packet_buffer);
                    if (packet_len > 0) {
                        // 解析命令
                        GDBCommand cmd = gdb_parse_command(packet_buffer);

                        // 处理命令
                        switch (cmd) {
                            case GDB_CMD_CONTINUE:
                                gdb_handle_continue(packet_buffer);
                                break;
                            case GDB_CMD_STEP:
                                gdb_handle_step(packet_buffer);
                                break;
                            case GDB_CMD_READ_REG:
                                gdb_handle_read_registers(packet_buffer);
                                break;
                            case GDB_CMD_WRITE_REG:
                                gdb_handle_write_registers(packet_buffer);
                                break;
                            case GDB_CMD_READ_MEM:
                                gdb_handle_read_memory(packet_buffer);
                                break;
                            case GDB_CMD_WRITE_MEM:
                                gdb_handle_write_memory(packet_buffer);
                                break;
                            case GDB_CMD_SET_BREAK:
                                gdb_handle_set_breakpoint(packet_buffer);
                                break;
                            case GDB_CMD_REMOVE_BREAK:
                                gdb_handle_remove_breakpoint(packet_buffer);
                                break;
                            case GDB_CMD_QUERY:
                                // 暂时忽略查询命令
                                gdb_send_packet("");
                                break;
                            default:
                                break;
                        }
                    }
                } else if (recv_len < 0) {
                    // 连接断开
                    g_debugger.connected = 0;
                    gdb_socket_close(client);
                    gdb_set_client_socket((Socket)-1);
                    Diagnose::Write("GDB client disconnected\n");
                }
            }
        }
    }

    Diagnose::Write("Debugger main loop ended\n");
}

// 获取调试器状态
DebuggerState* debugger_get_state(void) {
    return &g_debugger;
}
