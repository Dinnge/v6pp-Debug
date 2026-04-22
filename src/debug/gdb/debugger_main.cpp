// 调试器主循环实现

#include "../debug.h"
#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_breakpoints.h"
#include "gdb_serial.h"
#include "../../include/Video.h"
#include "../../include/Assembly.h"


// 调试器全局状态
extern "C" DebuggerState g_debugger = {0};
static Socket g_listen_socket = (Socket)-1;

int debugger_is_target_running(void) {
    return g_debugger.target_running;
}

void debugger_set_target_running(int running) {
    g_debugger.target_running = running ? 1 : 0;
}

// 初始化调试器
int debugger_init(void) {
    g_debugger.enabled = 1;
    g_debugger.listening = 1;
    g_debugger.connected = 0;
    g_debugger.buffer_pos = 0;
    g_debugger.resume_requested = 0;
    g_debugger.mode = DEBUG_MODE_NONE;
    g_debugger.target_running = 0;

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
    char packet_buffer[DEBUG_BUFFER_SIZE];
    int waiting_count = 0;

    debugger_set_target_running(0);
    serial_set_rx_interrupt_enabled(0);
    
    // 主循环
    while (g_debugger.enabled) {
        // 阶段1: 等待GDB连接（接收第一个有效数据包）
        if (!g_debugger.connected) {
            waiting_count++;
            
            // // 每1000次循环输出一次等待信息
            // if (waiting_count % 100000 == 0) {
            //     Diagnose::Write("Waiting for GDB connection... (Listening on COM1)\n");
            // }
            
            int packet_len = serial_recv_packet_with_interrupt(packet_buffer, DEBUG_BUFFER_SIZE);

            // 尝试接收一个数据包（非阻塞）
            // int packet_len = serial_recv_packet(packet_buffer, DEBUG_BUFFER_SIZE);
            
            if (packet_len > 0) {
                // 成功接收到第一个数据包，标记为已连接
                g_debugger.connected = 1;
                if (packet_len == 1 && packet_buffer[0] == 0x03) {
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
                Diagnose::Write("[GDB] Packet receive error while waiting for client\n");
            }
            
            // 短暂延迟避免CPU占用过高
            for (volatile int i = 0; i < 5000; i++);
            continue;
        }
        
        // 阶段2: 已连接状态，处理GDB命令
        else if (g_debugger.connected) {
            // 尝试接收数据包（非阻塞）
            int packet_len = serial_recv_packet_with_interrupt(packet_buffer, DEBUG_BUFFER_SIZE);
            
            if (packet_len > 0) {
                gdb_set_current_packet_length(packet_len);
                if (packet_len == 1 && packet_buffer[0] == 0x03) {
                    // 触发调试异常，使程序重新进入调试器
                    asm volatile("int $0x01");
                    continue;
                }

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
                    case GDB_CMD_DETACH:
                        gdb_send_ok();
                        g_debugger.connected = 0;
                        gdb_set_client_socket((Socket)-1);
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
                        break;
                }
                if (g_debugger.resume_requested) {
                    g_debugger.resume_requested = 0;
                    debugger_set_target_running(1);
                    return;
                }
            } else if (packet_len < 0) {
                // 接收错误，可能是连接断开
                g_debugger.connected = 0;
                Diagnose::Write("[GDB] Client disconnected or packet error\n");
            }
            // 如果packet_len == 0，表示没有数据，继续循环
            
            // 短暂延迟
            for (volatile int i = 0; i < 20000; i++);
        }
    }
}

void monitor_execution_mode(void) {
    // 恢复寄存器，让程序开始执行
    if (is_reg_context_valid()) {
        gdb_registers_restore();
    }
    
    // 使用简单的计数器而不是系统时间
    uint32_t check_counter = 0;
    const uint32_t CHECK_INTERVAL = 10000;  // 检查间隔（循环次数）
    
    // 关键：这里不退出，而是循环检查中断
    while (1) {
        check_counter++;
        
        // 定期检查串口是否有中断
        if (check_counter % CHECK_INTERVAL == 0) {
            check_counter = 0;
            
            // 检查Ctrl+C中断（非阻塞）
            int ch = serial_inb_nb();
            if (ch >= 0) {
                unsigned char c = (unsigned char)ch;
                if (c == 0x03) {  // Ctrl+C
                    // 触发调试异常，使程序重新进入调试器
                    asm volatile("int $0x01");
                    
                    // 注意：这里不要break，因为触发异常后
                    // 程序会通过异常处理程序重新进入debugger_main
                    // 这个函数会被中断，控制权转移
                    return;
                }
            }
        }
        
        // 检查程序是否因异常重新进入调试器
        // 如果有新的调试会话开始，resume_requested会被重置
        if (!g_debugger.resume_requested) {
            // 程序遇到了新的异常（断点、单步等），已重新进入debugger_main
            break;
        }
        
        // 短暂延迟避免忙等待
        for (volatile int i = 0; i < 100; i++);
    }
}

// 获取调试器状态
DebuggerState* debugger_get_state(void) {
    return &g_debugger;
}

static int debugger_handle_breakpoint_trap(void) {
    GDBRegisters* regs = gdb_get_registers();
    if (!regs || regs->eip == 0) {
        return 0;
    }

    uint32_t breakpoint_addr = regs->eip - 1;
    if (gdb_consume_range_step_breakpoint(breakpoint_addr)) {
        regs->eip = breakpoint_addr;
        gdb_clear_single_step();
        gdb_registers_commit_to_trap_frame();
        return 0;
    }

    if (gdb_prepare_breakpoint_resume(breakpoint_addr) != 0) {
        return 0;
    }

    gdb_clear_range_step();
    regs->eip = breakpoint_addr;
    gdb_clear_single_step();
    gdb_registers_commit_to_trap_frame();
    return 1;
}

static int debugger_handle_debug_trap(void) {
    int auto_continue = 0;

    if (gdb_has_pending_breakpoint_resume()) {
        auto_continue = gdb_get_pending_breakpoint_auto_continue();
        gdb_finish_breakpoint_resume();
    }

    if (gdb_has_range_step()) {
        uint32_t pc = gdb_get_register_value(GDB_REG_EIP);
        if (!gdb_range_step_should_stop(pc)) {
            if (gdb_prepare_range_step_resume(pc)) {
                gdb_clear_single_step();
            } else {
                gdb_set_single_step();
            }
            gdb_registers_commit_to_trap_frame();
            debugger_set_target_running(1);
            return 1;
        }
    }

    if (auto_continue) {
        gdb_clear_single_step();
        gdb_registers_commit_to_trap_frame();
        debugger_set_target_running(1);
        return 1;
    }

    return 0;
}

// 调试器入口函数（异常发生时调用）
void debugger_enter(DebugTrapType trap_type, struct pt_regs* regs, struct pt_context* context) {
    debugger_set_target_running(0);
    serial_set_rx_interrupt_enabled(0);

    // Lifecycle debugging reuses the same serial transport from bootloader/sector2.
    // When we break into the kernel stub, GDB is already attached and waiting for
    // an immediate stop reply, so bind the serial-backed pseudo-socket before
    // sending the first packet.
    gdb_set_client_socket(1);
    g_debugger.connected = 1;

    gdb_registers_invalidate();
    gdb_registers_bind_trap_frame(regs, context);

    if (trap_type == DEBUG_TRAP_BREAKPOINT) {
        debugger_handle_breakpoint_trap();
    } else if (trap_type == DEBUG_TRAP_DEBUG && debugger_handle_debug_trap()) {
        return;
    }

    gdb_send_packet((char*)"T05thread:1;");
    g_debugger.resume_requested = 0;

    debugger_main();

    gdb_registers_commit_to_trap_frame();
}
