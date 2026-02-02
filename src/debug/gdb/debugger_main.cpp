// 调试器主循环实现

#include "../debug.h"
#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_breakpoints.h"
#include "gdb_serial.h"
#include "../../include/Video.h"


// 调试器全局状态
extern "C" DebuggerState g_debugger = {0};
static Socket g_listen_socket = (Socket)-1;

// 初始化调试器
int debugger_init(void) {
    g_debugger.enabled = 1;
    g_debugger.listening = 1;
    g_debugger.connected = 0;
    g_debugger.buffer_pos = 0;

    // 初始化寄存器管理
    gdb_registers_init();

    // 初始化断点管理
    gdb_breakpoints_init();

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
// static int gdb_parse_packet(char* buffer, int len, char* packet) {
//     // GDB 数据包格式: $数据#校验和
//     int i = 0;
//     int packet_len = 0;

//     while (i < len) {
//         if (buffer[i] == '$') {
//             // 找到数据包开始
//             i++;
//             while (i < len && buffer[i] != '#') {
//                 packet[packet_len++] = buffer[i++];
//             }
//             if (i < len) {
//                 packet[packet_len] = '\0';
//                 return packet_len;
//             }
//         } else if (buffer[i] == '+') {
//             // ACK
//             i++;
//         } else if (buffer[i] == '-') {
//             // NACK,需要重发
//             i++;
//         } else {
//             i++;
//         }
//     }

//     return 0;
// }

// 调试器主循环
void debugger_main(void) {
    Diagnose::Write("GDB Debugger Main Loop Started (Direct Serial Mode)\n");
    
    char packet_buffer[DEBUG_BUFFER_SIZE];
    int debug_cycle = 0;
    int waiting_count = 0;
    
    // 主循环
    while (g_debugger.enabled) {
        // 阶段1: 等待GDB连接（接收第一个有效数据包）
        if (!g_debugger.connected) {
            waiting_count++;
            
            // 每1000次循环输出一次等待信息
            if (waiting_count % 100000 == 0) {
                Diagnose::Write("Waiting for GDB connection... (Listening on COM1)\n");
            }
            
            int packet_len = serial_recv_packet_with_interrupt(packet_buffer, DEBUG_BUFFER_SIZE);

            // 尝试接收一个数据包（非阻塞）
            // int packet_len = serial_recv_packet(packet_buffer, DEBUG_BUFFER_SIZE);
            
            if (packet_len > 0) {
                // 成功接收到第一个数据包，标记为已连接
                g_debugger.connected = 1;
                Diagnose::Write("*** GDB CLIENT CONNECTED SUCCESSFULLY! ***\n");
                Diagnose::Write("First packet received: ");
                Diagnose::Write(packet_buffer);
                Diagnose::Write("\n");

                if (packet_len == 1 && packet_buffer[0] == 0x03) {
                    // 处理中断请求
                    Diagnose::Write("[GDB] 处理Ctrl+C中断请求\n");
                    
                    // 取消继续执行
                    g_debugger.resume_requested = 0;
                    // g_debugger.listening = 0;
                    
                    // 发送停止信号给GDB
                    gdb_send_packet("S02");  // SIGINT信号
                    continue;
                }
                // 设置客户端socket以便gdb_send_packet能工作
                gdb_set_client_socket(1);
                // 处理第一个包（可能是qSupported或vMustReplyEmpty等）
                gdb_handle_query(packet_buffer);
            } else if (packet_len < 0) {
                // 接收错误（可能是校验和错误）
                Diagnose::Write("[ERROR] Packet receive error (checksum mismatch?)\n");
            }
            
            // 短暂延迟避免CPU占用过高
            for (volatile int i = 0; i < 50000; i++);
            continue;
        }
        
        // 阶段2: 已连接状态，处理GDB命令
        else if (g_debugger.connected) {
            // 尝试接收数据包（非阻塞）
            int packet_len = serial_recv_packet_with_interrupt(packet_buffer, DEBUG_BUFFER_SIZE);
            
            if (packet_len > 0) {
                // 安全输出数据包内容（避免多个Diagnose::Write调用导致交错）
                char output_msg[DEBUG_BUFFER_SIZE + 100];
                int pos = 0;
                
                // 复制 "[GDB] Command received: "
                const char* prefix = "[GDB] Command received: ";
                while (*prefix) output_msg[pos++] = *prefix++;
                
                // 复制数据包内容（确保以null结尾）
                int copy_len = packet_len;
                if (copy_len > DEBUG_BUFFER_SIZE - 1) copy_len = DEBUG_BUFFER_SIZE - 1;
                for (int i = 0; i < copy_len; i++) {
                    output_msg[pos++] = packet_buffer[i];
                }
                
                output_msg[pos++] = '\n';
                output_msg[pos] = '\0';
                
                Diagnose::Write(output_msg);
                
                // 解析命令并处理
                GDBCommand cmd = gdb_parse_command(packet_buffer);
                switch (cmd) {
                    case GDB_CMD_THREAD:
                        gdb_handle_thread_command(packet_buffer);
                        break;
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
                    case GDB_CMD_WRITE_SINGLE_REG:
                        gdb_handle_write_single_register(packet_buffer);
                        break;      
                    case GDB_CMD_READ_MEM:
                        gdb_handle_read_memory(packet_buffer);
                        break;
                    case GDB_CMD_WRITE_MEM:
                        gdb_handle_write_memory(packet_buffer);
                        break;
                    case GDB_CMD_WRITE_MEM_BINARY:  // 新增：二进制格式写入
                        gdb_handle_binary_write_memory(packet_buffer);
                        break;
                    case GDB_CMD_SET_BREAK:
                        gdb_handle_set_breakpoint(packet_buffer);
                        break;
                    case GDB_CMD_REMOVE_BREAK:
                        gdb_handle_remove_breakpoint(packet_buffer);
                        break;
                    case GDB_CMD_QUERY:
                        gdb_handle_query(packet_buffer);
                        break;
                    case GDB_CMD_VENDOR:
                        gdb_handle_query(packet_buffer);
                        break;
                    case GDB_CMD_SIGNAL:
                        gdb_handle_signal(packet_buffer);
                        break;
                    default:
                        // 对于未知命令，返回OK
                        serial_send_packet((char*)"OK", 2);
                        Diagnose::Write("Sent OK response for unknown command\n");
                        break;
                }
                // 如果收到继续/单步请求，退出调试循环以恢复执行
                if (g_debugger.resume_requested) {
                    g_debugger.resume_requested = 0;
                    Diagnose::Write("Resuming execution as requested by GDB\n");
                    return;
                }
            } else if (packet_len < 0) {
                // 接收错误，可能是连接断开
                g_debugger.connected = 0;
                Diagnose::Write("[WARNING] GDB client disconnected or receive error\n");
                Diagnose::Write("Returning to waiting mode...\n");
            }
            // 如果packet_len == 0，表示没有数据，继续循环
            
            // 短暂延迟
            for (volatile int i = 0; i < 20000; i++);
        }
    }
    
    Diagnose::Write("GDB Debugger Main Loop Ended\n");
}

// 调试器主循环 - 专注于握手协议
// void debugger_main(void) {
//     Diagnose::Write("Debugger main loop started (handshake mode)\n");

//     char packet_buffer[DEBUG_BUFFER_SIZE];
//     int handshake_completed = 0;

//     while (g_debugger.enabled) {
//         // 阶段1: 等待GDB连接
//         if (!g_debugger.connected) {
//             Diagnose::Write("Waiting for GDB connection...\n");
            
//             // 检查是否有数据到达（表示GDB尝试连接）
//             if (gdb_socket_readable(g_listen_socket) > 0) {
//                 Socket client = gdb_socket_accept(g_listen_socket);
//                 if (client != (Socket)-1) {
//                     gdb_set_client_socket(client);
//                     g_debugger.connected = 1;
//                     Diagnose::Write("GDB connection detected! Starting handshake...\n");
//                 }
//             }
            
//             // 短暂延迟避免过快的循环
//             for (volatile int i = 0; i < 10000000000; i++);
//             continue;
//         }

//         // 阶段2: 握手协议
//         if (g_debugger.connected && !handshake_completed) {
//             Socket client = gdb_get_client_socket();
            
//             // 步骤1: 发送初始ACK响应GDB连接
//             Diagnose::Write("Sending initial ACK to GDB...\n");
//             char ack = '+';
//             gdb_socket_send(client, &ack, 1);
            
//             // 步骤2: 等待GDB的初始查询包（qSupported）
//             Diagnose::Write("Waiting for GDB initial query (qSupported)...\n");
//             int packet_len = gdb_recv_packet(packet_buffer, DEBUG_BUFFER_SIZE);
            
//             if (packet_len > 0) {
//                 Diagnose::Write("Received initial packet: ");
//                 Diagnose::Write(packet_buffer);
//                 Diagnose::Write("\n");
                
//                 // 检查是否是qSupported查询（自定义字符串比较）
//                 int is_qSupported = 1;
//                 const char* qSupported = "qSupported";
//                 for (int i = 0; i < 10 && qSupported[i] != '\0'; i++) {
//                     if (packet_buffer[i] != qSupported[i]) {
//                         is_qSupported = 0;
//                         break;
//                     }
//                 }
                
//                 if (packet_buffer[0] == 'q' && is_qSupported) {
//                     Diagnose::Write("Processing qSupported query...\n");
                    
//                     // 发送简单的qSupported响应
//                     char response[] = "PacketSize=4000;qXfer:features:read+";
//                     gdb_send_packet(response);
//                     Diagnose::Write("Sent qSupported response\n");
                    
//                     // 步骤3: 等待GDB确认（ACK）
//                     char ack_buffer[2];
//                     int ack_len = gdb_socket_recv(client, ack_buffer, 1);
//                     if (ack_len > 0 && ack_buffer[0] == '+') {
//                         Diagnose::Write("GDB acknowledged our response\n");
//                         handshake_completed = 1;
//                         Diagnose::Write("=== GDB HANDSHAKE COMPLETED SUCCESSFULLY ===\n");
//                         Diagnose::Write("Debugger is now ready for normal operation\n");
//                     } else {
//                         Diagnose::Write("Handshake failed: No ACK from GDB\n");
//                     }
//                 } else {
//                     Diagnose::Write("Handshake failed: Expected qSupported, got: ");
//                     Diagnose::Write(packet_buffer);
//                     Diagnose::Write("\n");
//                 }
//             } else {
//                 Diagnose::Write("Handshake failed: No initial packet received\n");
//             }
            
//             // 如果握手失败，重置连接状态
//             if (!handshake_completed) {
//                 g_debugger.connected = 0;
//                 gdb_socket_close(client);
//                 gdb_set_client_socket((Socket)-1);
//                 Diagnose::Write("Handshake failed, resetting connection state\n");
//             }
            
//             continue;
//         }

//         // 阶段3: 握手完成后的正常调试操作
//         if (handshake_completed) {
//             Socket client = gdb_get_client_socket();
//             int packet_len = gdb_recv_packet(packet_buffer, DEBUG_BUFFER_SIZE);

//             if (packet_len > 0) {
//                 Diagnose::Write("[DEBUG] Received command: ");
//                 Diagnose::Write(packet_buffer);
//                 Diagnose::Write("\n");

//                 // 简单响应测试命令
//                 if (packet_buffer[0] == 'g') {
//                     // 寄存器读取命令
//                     Diagnose::Write("Handling register read command\n");
//                     gdb_send_packet((char*)"0000000000000000000000000000000000000000000000000000000000000000");
//                 } else if (packet_buffer[0] == '?') {
//                     // 信号查询命令
//                     Diagnose::Write("Handling signal query command\n");
//                     gdb_send_packet((char*)"S05");
//                 } else {
//                     // 其他命令返回OK
//                     Diagnose::Write("Sending OK response\n");
//                     gdb_send_packet((char*)"OK");
//                 }
//             } else if (packet_len < 0) {
//                 // 连接断开
//                 g_debugger.connected = 0;
//                 handshake_completed = 0;
//                 gdb_socket_close(client);
//                 gdb_set_client_socket((Socket)-1);
//                 Diagnose::Write("GDB client disconnected\n");
//             }
//         }
//     }

//     Diagnose::Write("Debugger main loop ended\n");
// }


// 获取调试器状态
DebuggerState* debugger_get_state(void) {
    return &g_debugger;
}

// 调试器入口函数（异常发生时调用）
void debugger_enter(void) {
    Diagnose::Write("[DEBUGGER] 进入调试器，异常发生\n");
    
    // 1. 保存当前寄存器状态到调试上下文
    gdb_registers_save();
    Diagnose::Write("[DEBUGGER] 寄存器状态已保存\n");
    
    // 2. 发送停止信号给GDB（S05 = SIGTRAP）
    gdb_send_packet("S05");
    Diagnose::Write("[DEBUGGER] 已通知GDB程序停止\n");
    
    // 3. 重置调试器状态
    g_debugger.resume_requested = 0;
    g_debugger.connected = 1;  // 假设GDB已连接
    
    // 4. 进入调试器主循环
    debugger_main();
    
    // 5. 调试循环退出后的处理
    Diagnose::Write("[DEBUGGER] 调试循环结束，准备恢复执行\n");
    
    // 注意：寄存器恢复应该在debugger_main内部处理
    // 因为continue/step命令会在debugger_main中调用g_registers_restore()
}