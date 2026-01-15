// #include "gdb_protocol.h"
// #include "gdb_socket.h"
// #include "gdb_registers.h"
// #include "gdb_memory.h"
// #include "gdb_breakpoints.h"
// #include "../../include/Video.h"
// #include "../debug.h"

// // 外部引用调试器状态
// extern DebuggerState g_debugger;

// // 自定义字符串函数（内核环境）
// static int strlen(const char* s) {
//     int len = 0;
//     while (s[len] != '\0') len++;
//     return len;
// }

// static void strcpy(char* dest, const char* src) {
//     int i = 0;
//     while (src[i] != '\0') {
//         dest[i] = src[i];
//         i++;
//     }
//     dest[i] = '\0';
// }

// static int strcmp(const char* s1, const char* s2) {
//     int i = 0;
//     while (s1[i] != '\0' && s2[i] != '\0') {
//         if (s1[i] != s2[i]) return s1[i] - s2[i];
//         i++;
//     }
//     return s1[i] - s2[i];
// }

// static int strncmp(const char* s1, const char* s2, int n) {
//     for (int i = 0; i < n; i++) {
//         if (s1[i] != s2[i]) return s1[i] - s2[i];
//         if (s1[i] == '\0') return 0;
//     }
//     return 0;
// }

// // 全局 socket 变量
// static Socket g_client_socket = (Socket)-1;

// // 设置当前连接的客户端 socket
// void gdb_set_client_socket(Socket sock) {
//     g_client_socket = sock;
// }

// // 获取当前客户端 socket
// Socket gdb_get_client_socket(void) {
//     return g_client_socket;
// }

// int gdb_recv_packet(char* buffer, int buffer_size) {
//     if (g_client_socket == (Socket)-1) return -1;

//     // 串口层已经实现了对完整 RSP 包的解析（serial_recv_packet 返回包体长度，
//     // 不包含起始 '$' 与校验和），因此这里直接一次性从 socket 层读取完整包。
//     // 返回值语义：>0 包长度，0 表示当前无数据，<0 表示连接关闭或错误。
//     int recv_len = gdb_socket_recv(g_client_socket, buffer, buffer_size);
//     if (recv_len < 0) {
//         return -1; // 连接错误或已关闭
//     }
//     if (recv_len == 0) {
//         return 0; // 暂无完整包
//     }

//     // 确保字符串终止
//     if (recv_len >= buffer_size) recv_len = buffer_size - 1;
//     buffer[recv_len] = '\0';
//     // 日志接收到的包体（便于调试握手）
//     Diagnose::Write("RX: ");
//     Diagnose::Write(buffer);
//     Diagnose::Write("\n");
//     return recv_len;
// }

// void gdb_send_packet(char *data)
// {
//     if (g_client_socket == (Socket)-1) return;

//     char buffer[DEBUG_BUFFER_SIZE];
//     int len = strlen(data);

//     // 计算校验和
//     unsigned char checksum = 0;
//     for (int i = 0; i < len; i++) {
//         checksum += (unsigned char)data[i];
//     }

//     // 构造数据包: $数据#校验和
//     int pos = 0;
//     buffer[pos++] = '$';
//     // 直接复制 data 中的字节（不要写入额外的 '\0' 字节到数据部分）
//     for (int i = 0; i < len; i++) {
//         buffer[pos++] = data[i];
//     }
//     buffer[pos++] = '#';

//     // 转换校验和为十六进制
//     char hex[3];
//     hex[0] = "0123456789abcdef"[(checksum >> 4) & 0x0F];
//     hex[1] = "0123456789abcdef"[checksum & 0x0F];
//     buffer[pos++] = hex[0];
//     buffer[pos++] = hex[1];
//     buffer[pos] = '\0';

//     // 发送到 GDB，先记录要发送的原始包内容以便调试
//     Diagnose::Write("TX: ");
//     Diagnose::Write(buffer);
//     Diagnose::Write("\n");
//     gdb_socket_send(g_client_socket, buffer, pos);
// }

// void gdb_send_ok(void) {
//     gdb_send_packet("OK");
// }

// void gdb_send_error(int code) {
//     char buffer[16];
//     buffer[0] = 'E';
//     // 转换错误码为十六进制
//     const char* hex = "0123456789abcdef";
//     buffer[1] = hex[(code >> 4) & 0x0F];
//     buffer[2] = hex[code & 0x0F];
//     buffer[3] = '\0';
//     gdb_send_packet(buffer);
// }

// GDBCommand gdb_parse_command(char* packet) {
//     if (!packet) return GDB_CMD_UNKNOWN;
//     char cmd = packet[0];
//     switch (cmd) {
//         case 'c': return GDB_CMD_CONTINUE;
//         case 's': return GDB_CMD_STEP;
//         case 'g': return GDB_CMD_READ_REG;
//         case 'G': return GDB_CMD_WRITE_REG;
//         case 'm': return GDB_CMD_READ_MEM;
//         case 'M': return GDB_CMD_WRITE_MEM;
//         case 'Z': return GDB_CMD_SET_BREAK;
//         case 'z': return GDB_CMD_REMOVE_BREAK;
//         case 'q': return GDB_CMD_QUERY;
//         case 'v': return GDB_CMD_VENDOR;;
//         case '?': return GDB_CMD_SIGNAL;
//         default: return GDB_CMD_UNKNOWN;
//     }
// }

// // 处理继续执行命令
// void gdb_handle_continue(char *p) {
//     gdb_send_ok();
//     g_debugger.mode = DEBUG_MODE_CONTINUE;
//     Diagnose::Write("Continuing execution...\n");
// }

// // 处理单步执行命令
// void gdb_handle_step(char *p) {
//     gdb_send_ok();
//     g_debugger.mode = DEBUG_MODE_STEP;
//     Diagnose::Write("Stepping...\n");
// }

// // 处理读取寄存器命令
// void gdb_handle_read_registers(char* p) {
//     // 先保存当前寄存器
//     gdb_registers_save();

//     // 转换为 GDB 格式字符串
//     char reg_str[512];
//     gdb_registers_to_string(reg_str, sizeof(reg_str));

//     // 发送给 GDB
//     gdb_send_packet(reg_str);
// }

// // 处理写入寄存器命令
// void gdb_handle_write_registers(char* p) {
//     // 解析寄存器数据（GDB 格式：GXX...）
//     // 跳过 'G' 前缀
//     if (p[0] != 'G') {
//         gdb_send_error(0);
//         return;
//     }

//     char* data = p + 1;
//     uint32_t value = 0;

//     // 解析十六进制字符串
//     int pos = 0;
//     for (int i = 0; i < GDB_REG_COUNT; i++) {
//         value = 0;
//         for (int j = 0; j < 4; j++) {
//             char c1 = data[pos++];
//             char c2 = data[pos++];
//             uint8_t byte = 0;

//             if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
//             else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
//             else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

//             if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
//             else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
//             else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

//             value |= byte << (j * 8);
//         }

//         gdb_set_register(i, value);
//     }

//     gdb_send_ok();
// }

// // 处理读取内存命令（格式：m地址,长度）
// void gdb_handle_read_memory(char* p) {
//     // 解析地址和长度
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // 跳过 'm' 前缀
//     char* ptr = p + 1;

//     // 解析地址（十六进制）
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     // 跳过 ','
//     if (*ptr == ',') ptr++;

//     // 解析长度
//     while (*ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             len = (len << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             len = (len << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             len = (len << 4) | (c - 'A' + 10);
//         }
//     }

//     // 限制最大长度
//     if (len > DEBUG_BUFFER_SIZE / 2) {
//         len = DEBUG_BUFFER_SIZE / 2;
//     }

//     // 读取内存
//     static char mem_buffer[DEBUG_BUFFER_SIZE];
//     if (gdb_read_memory(addr, mem_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     // 转换为十六进制字符串
//     static char hex_buffer[DEBUG_BUFFER_SIZE * 2];
//     const char* hex_chars = "0123456789abcdef";
//     int hex_pos = 0;

//     for (uint32_t i = 0; i < len; i++) {
//         hex_buffer[hex_pos++] = hex_chars[(mem_buffer[i] >> 4) & 0x0F];
//         hex_buffer[hex_pos++] = hex_chars[mem_buffer[i] & 0x0F];
//     }

//     hex_buffer[hex_pos] = '\0';
//     gdb_send_packet(hex_buffer);
// }

// // 处理写入内存命令（格式：M地址,长度:数据）
// void gdb_handle_write_memory(char* p) {
//     // 解析地址和长度
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // 跳过 'M' 前缀
//     char* ptr = p + 1;

//     // 解析地址
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     // 跳过 ','
//     if (*ptr == ',') ptr++;

//     // 解析长度
//     while (*ptr != ':' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             len = (len << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             len = (len << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             len = (len << 4) | (c - 'A' + 10);
//         }
//     }

//     // 跳过 ':'
//     if (*ptr == ':') ptr++;

//     // 解析数据
//     static char data_buffer[DEBUG_BUFFER_SIZE];
//     const char* hex_chars = "0123456789abcdef";

//     for (uint32_t i = 0; i < len; i++) {
//         char c1 = ptr[0];
//         char c2 = ptr[1];
//         ptr += 2;

//         uint8_t byte = 0;
//         if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
//         else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
//         else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

//         if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
//         else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
//         else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

//         data_buffer[i] = byte;
//     }

//     // 写入内存
//     if (gdb_write_memory(addr, data_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     gdb_send_ok();
// }

// // 处理设置断点命令（格式：Z类型,地址,长度）
// void gdb_handle_set_breakpoint(char* p) {
//     if (p[0] != 'Z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // 解析类型
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // 跳过 ','
//     if (*ptr == ',') ptr++;

//     // 解析地址
//     addr = 0;
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     // 添加断点
//     if (gdb_add_breakpoint(addr, type) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }

// // 处理移除断点命令（格式：z类型,地址,长度）
// void gdb_handle_remove_breakpoint(char* p) {
//     if (p[0] != 'z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // 解析类型
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // 跳过 ','
//     if (*ptr == ',') ptr++;

//     // 解析地址
//     addr = 0;
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     // 移除断点
//     if (gdb_remove_breakpoint(addr) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }

// // 处理查询命令
// void gdb_handle_query(char* p) {
//     // qSupported - GDB 特性查询
//     if (p[0] == 'q' && p[1] == 'S') {
//         // 返回支持的特性，明确排除未知项
//         gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;xmlRegisters-;timeout-;QStartNoAckMode-");
//         return;
//     }

//     // vMustReplyEmpty - 必须回复空
//     if (strcmp(p, "vMustReplyEmpty") == 0) {
//         gdb_send_packet("");
//         return;
//     }

//     // vCont? - 查询支持的继续命令
//     if (strcmp(p, "vCont?") == 0) {
//         gdb_send_packet("vCont;c;C;s;S");
//         return;
//     }

//     // qC - 查询当前线程 ID
//     if (p[0] == 'q' && p[1] == 'C' && p[2] == '\0') {
//         gdb_send_packet("QC1");
//         return;
//     }

//     // qAttached - 查询是否attached
//     if (strcmp(p, "qAttached") == 0) {
//         gdb_send_packet("1");  // 已attached
//         return;
//     }

//     // 其他查询命令返回空
//     gdb_send_packet("");
// }

// // 处理信号命令
// void gdb_handle_signal(char* p) {
//     // 发送停止信号 (S05 = SIGTRAP)
//     gdb_send_packet("S05");
// }
#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_memory.h"
#include "gdb_breakpoints.h"
#include "../../include/Video.h"
#include "../debug.h"

// 外部引用调试器状态
extern "C" DebuggerState g_debugger;

// 自定义字符串函数（内核环境）
static int strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

static int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
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

// 接收 GDB 数据包（带 ACK/NACK 支持）
int gdb_recv_packet(char* buffer, int buffer_size) {
    if (g_client_socket == (Socket)-1) return -1;

    int state = 0;  // 0=等待'$', 1=接收数据, 2=等待'#', 3=读取校验和第一个字符, 4=读取校验和第二个字符
    int packet_len = 0;
    unsigned char recv_checksum = 0;
    unsigned char calc_checksum = 0;
    char recv_byte;

    while (1) {
        // 接收一个字节
        int recv_len = gdb_socket_recv(g_client_socket, &recv_byte, 1);
        if (recv_len <= 0) {
            // 无数据或连接断开
            return -1;
        }

        char c = recv_byte;

        // 状态机解析数据包
        switch (state) {
            case 0:  // 等待 '$'
                if (c == '$') {
                    packet_len = 0;
                    calc_checksum = 0;
                    recv_checksum = 0;
                    state = 1;
                } else if (c == '+') {
                    // ACK - 对我们发送的数据包的确认，忽略
                    continue;
                } else if (c == '-') {
                    // NACK - 对我们发送的数据包的否认，需要重发
                    // 返回特殊错误码让上层处理重发
                    return -2;
                }
                // 忽略其他字符，等待 '$'
                break;

            case 1:  // 接收数据内容
                if (c == '#') {
                    state = 2;
                } else if (packet_len < buffer_size - 1) {
                    buffer[packet_len++] = c;
                    calc_checksum += (unsigned char)c;
                } else {
                    // 缓冲区不足，丢弃数据包
                    state = 0;
                }
                break;

            case 2:  // 读取校验和第一个字符
                if (c >= '0' && c <= '9') {
                    recv_checksum = (c - '0') << 4;
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum = (c - 'a' + 10) << 4;
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum = (c - 'A' + 10) << 4;
                } else {
                    // 无效的校验和字符，丢弃数据包
                    state = 0;
                    break;
                }
                state = 3;
                break;

            case 3:  // 读取校验和第二个字符并验证
                if (c >= '0' && c <= '9') {
                    recv_checksum |= (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum |= (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum |= (c - 'A' + 10);
                } else {
                    // 无效的校验和字符，丢弃数据包
                    state = 0;
                    break;
                }

                // 验证校验和
                if (recv_checksum == calc_checksum) {
                    // 校验和正确，发送 ACK
                    buffer[packet_len] = '\0';
                    char ack = '+';
                    gdb_socket_send(g_client_socket, &ack, 1);
                    return packet_len;
                } else {
                    // 校验和错误，发送 NACK
                    char nack = '-';
                    gdb_socket_send(g_client_socket, &nack, 1);
                    // 重置状态机，等待重传的数据包
                    state = 0;
                }
                break;
        }
    }
}

void gdb_send_packet(char* data) {
    if (g_client_socket == (Socket)-1) return;

    // 调试日志：发送数据包（使用原子输出）
    char tx_msg[DEBUG_BUFFER_SIZE + 100];
    int msg_pos = 0;
    
    // 复制 "[GDB] TX packet: "
    const char* tx_prefix = "[GDB] TX packet: ";
    while (*tx_prefix) tx_msg[msg_pos++] = *tx_prefix++;
    
    // 复制数据内容
    int i = 0;
    while (data[i] != '\0' && i < DEBUG_BUFFER_SIZE - 1) {
        tx_msg[msg_pos++] = data[i++];
    }
    
    tx_msg[msg_pos++] = '\n';
    tx_msg[msg_pos] = '\0';
    
    Diagnose::Write(tx_msg);

    char buffer[DEBUG_BUFFER_SIZE];
    int len = strlen(data);

    // 计算校验和
    unsigned char checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += (unsigned char)data[i];
    }

    // 构造数据包: $数据#校验和
    int pos = 0;
    buffer[pos++] = '$';
    strcpy(buffer + pos, data);
    pos += len;
    buffer[pos++] = '#';

    // 转换校验和为十六进制
    char hex[3];
    hex[0] = "0123456789abcdef"[(checksum >> 4) & 0x0F];
    hex[1] = "0123456789abcdef"[checksum & 0x0F];
    buffer[pos++] = hex[0];
    buffer[pos++] = hex[1];
    buffer[pos] = '\0';

    // 发送到 GDB
    gdb_socket_send(g_client_socket, buffer, pos);
}

void gdb_send_ok(void) {
    gdb_send_packet((char*)"OK");
}

void gdb_send_error(int code) {
    char buffer[16];
    buffer[0] = 'E';
    // 转换错误码为十六进制
    const char* hex = "0123456789abcdef";
    buffer[1] = hex[(code >> 4) & 0x0F];
    buffer[2] = hex[code & 0x0F];
    buffer[3] = '\0';
    gdb_send_packet(buffer);
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
        case 'v': return GDB_CMD_VENDOR;
        case '?': return GDB_CMD_SIGNAL;
        default: return GDB_CMD_UNKNOWN;
    }
}

// 处理继续执行命令
void gdb_handle_continue(char* p) {
    gdb_send_ok();
}

// 处理单步执行命令
void gdb_handle_step(char* p) {
    gdb_send_ok();
}

// 处理读取寄存器命令
void gdb_handle_read_registers(char* p) {
    Diagnose::Write("[DEBUG] gdb_handle_read_registers called\n");
    
    // // 避免页面错误：不调用 gdb_registers_save()，使用默认值
    // // gdb_registers_save(); // 注释掉，避免页面错误
    
    // // 转换为 GDB 格式字符串
    // char reg_str[512];
    // // 使用默认值填充：全零
    // for (int i = 0; i < 512; i++) {
    //     reg_str[i] = '0';
    // }
    
    // // GDB期望 16个寄存器 * 8个十六进制字符 = 128个字符
    // // 但我们填充 128个 '0' 字符 (16*8=128)
    // // 实际上我们需要 16*8=128个十六进制字符
    // const int hex_chars_needed = GDB_REG_COUNT * 8; // 16*8=128
    // for (int i = 0; i < hex_chars_needed; i++) {
    //     reg_str[i] = '0';
    // }
    // reg_str[hex_chars_needed] = '\0';
    
    // Diagnose::Write("[DEBUG] Sending default register values\n");
    // // 发送给 GDB
    // gdb_send_packet(reg_str);
    uint32_t regs[16] = {0};
    
    // 设置寄存器值（使用内核空间合法地址）
    regs[4] = 0xC0000000;  // ESP: 内核栈顶
    regs[8] = 0x0100fff0;  // EIP: 内核入口点（根据文档，内核加载到0x100000）
    regs[9] = 0x00000002;  // EFLAGS
    regs[10] = 0x0008;     // CS: 内核代码段
    regs[11] = 0x0010;     // SS: 内核数据段
    regs[12] = 0x0010;     // DS
    regs[13] = 0x0010;     // ES
    regs[14] = 0x0010;     // FS
    regs[15] = 0x0010;     // GS
    
    // 手动转换十六进制
    char response[129];
    char hex_chars[] = "0123456789abcdef";
    
    for (int i = 0; i < 16; i++) {
        uint32_t value = regs[i];
        int base = i * 8;
        
        // 每个字节转换为2个十六进制字符
        for (int j = 7; j >= 0; j--) {
            int nibble = (value >> (j * 4)) & 0xF;
            response[base + (7 - j)] = hex_chars[nibble];
        }
    }
    response[128] = '\0';  // 结束符
    
    gdb_send_packet(response);
}
// void gdb_handle_read_registers(char* p) {
//     Diagnose::Write("[DEBUG] gdb_handle_read_registers called\n");
    
//     // 保存当前寄存器值
//     gdb_registers_save();
    
//     // 转换为 GDB 格式字符串
//     char reg_str[512];
//     gdb_registers_to_string(reg_str, sizeof(reg_str));
    
//     Diagnose::Write("[DEBUG] Sending register values\n");
//     // 发送给 GDB
//     gdb_send_packet(reg_str);
// }

// 处理写入寄存器命令
void gdb_handle_write_registers(char* p) {
    // 解析寄存器数据（GDB 格式：GXX...）
    // 跳过 'G' 前缀
    if (p[0] != 'G') {
        gdb_send_error(0);
        return;
    }

    char* data = p + 1;
    uint32_t value = 0;

    // 解析十六进制字符串
    int pos = 0;
    for (int i = 0; i < GDB_REG_COUNT; i++) {
        value = 0;
        for (int j = 0; j < 4; j++) {
            char c1 = data[pos++];
            char c2 = data[pos++];
            uint8_t byte = 0;

            if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
            else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
            else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

            if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
            else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
            else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

            value |= byte << (j * 8);
        }

        gdb_set_register(i, value);
    }

    gdb_send_ok();
}

// 处理读取内存命令（格式：m地址,长度）
void gdb_handle_read_memory(char* p) {
    // 解析地址和长度
    uint32_t addr = 0;
    uint32_t len = 0;

    // 跳过 'm' 前缀
    char* ptr = p + 1;

    // 解析地址（十六进制）
    while (*ptr != ',' && *ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            addr = (addr << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            addr = (addr << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            addr = (addr << 4) | (c - 'A' + 10);
        }
    }

    // 跳过 ','
    if (*ptr == ',') ptr++;

    // 解析长度
    while (*ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            len = (len << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            len = (len << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            len = (len << 4) | (c - 'A' + 10);
        }
    }

    // 限制最大长度
    if (len > DEBUG_BUFFER_SIZE / 2) {
        len = DEBUG_BUFFER_SIZE / 2;
    }

    // 读取内存
    static char mem_buffer[DEBUG_BUFFER_SIZE];
    if (gdb_read_memory(addr, mem_buffer, len) < 0) {
        gdb_send_error(0);
        return;
    }

    // 转换为十六进制字符串
    static char hex_buffer[DEBUG_BUFFER_SIZE * 2];
    const char* hex_chars = "0123456789abcdef";
    int hex_pos = 0;

    for (uint32_t i = 0; i < len; i++) {
        hex_buffer[hex_pos++] = hex_chars[(mem_buffer[i] >> 4) & 0x0F];
        hex_buffer[hex_pos++] = hex_chars[mem_buffer[i] & 0x0F];
    }

    hex_buffer[hex_pos] = '\0';
    gdb_send_packet(hex_buffer);
}

// 处理写入内存命令（格式：M地址,长度:数据）
void gdb_handle_write_memory(char* p) {
    // 解析地址和长度
    uint32_t addr = 0;
    uint32_t len = 0;

    // 跳过 'M' 前缀
    char* ptr = p + 1;

    // 解析地址
    while (*ptr != ',' && *ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            addr = (addr << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            addr = (addr << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            addr = (addr << 4) | (c - 'A' + 10);
        }
    }

    // 跳过 ','
    if (*ptr == ',') ptr++;

    // 解析长度
    while (*ptr != ':' && *ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            len = (len << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            len = (len << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            len = (len << 4) | (c - 'A' + 10);
        }
    }

    // 跳过 ':'
    if (*ptr == ':') ptr++;

    // 解析数据
    static char data_buffer[DEBUG_BUFFER_SIZE];
    const char* hex_chars = "0123456789abcdef";

    for (uint32_t i = 0; i < len; i++) {
        char c1 = ptr[0];
        char c2 = ptr[1];
        ptr += 2;

        uint8_t byte = 0;
        if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
        else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
        else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

        if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
        else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
        else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

        data_buffer[i] = byte;
    }

    // 写入内存
    if (gdb_write_memory(addr, data_buffer, len) < 0) {
        gdb_send_error(0);
        return;
    }

    gdb_send_ok();
}

// 处理设置断点命令（格式：Z类型,地址,长度）
void gdb_handle_set_breakpoint(char* p) {
    if (p[0] != 'Z') {
        gdb_send_error(0);
        return;
    }

    char* ptr = p + 1;
    GDBBreakpointType type;
    uint32_t addr;

    // 解析类型
    type = (GDBBreakpointType)(*ptr++ - '0');

    // 跳过 ','
    if (*ptr == ',') ptr++;

    // 解析地址
    addr = 0;
    while (*ptr != ',' && *ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            addr = (addr << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            addr = (addr << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            addr = (addr << 4) | (c - 'A' + 10);
        }
    }

    // 添加断点
    if (gdb_add_breakpoint(addr, type) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

// 处理移除断点命令（格式：z类型,地址,长度）
void gdb_handle_remove_breakpoint(char* p) {
    if (p[0] != 'z') {
        gdb_send_error(0);
        return;
    }

    char* ptr = p + 1;
    GDBBreakpointType type;
    uint32_t addr;

    // 解析类型
    type = (GDBBreakpointType)(*ptr++ - '0');

    // 跳过 ','
    if (*ptr == ',') ptr++;

    // 解析地址
    addr = 0;
    while (*ptr != ',' && *ptr != '\0') {
        char c = *ptr++;
        if (c >= '0' && c <= '9') {
            addr = (addr << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            addr = (addr << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            addr = (addr << 4) | (c - 'A' + 10);
        }
    }

    // 移除断点
    if (gdb_remove_breakpoint(addr) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

// 处理查询命令
// void gdb_handle_query(char* p) {
//     // 调试日志：收到查询包（使用原子输出）
//     char query_msg[DEBUG_BUFFER_SIZE + 100];
//     int pos = 0;
    
//     // 复制 "[GDB] Query packet: "
//     const char* prefix = "[GDB] Query packet: ";
//     while (*prefix) query_msg[pos++] = *prefix++;
    
//     // 复制数据包内容
//     int i = 0;
//     while (p[i] != '\0' && i < DEBUG_BUFFER_SIZE - 1) {
//         query_msg[pos++] = p[i++];
//     }
    
//     query_msg[pos++] = '\n';
//     query_msg[pos] = '\0';
    
//     Diagnose::Write(query_msg);

//     // qSupported - GDB 特性查询
//     if (p[0] == 'q' && p[1] == 'S') {
//         // 返回支持的特性，明确排除未知项
//         gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;QStartNoAckMode-;xmlRegisters-");
//         return;
//     }

//     // vMustReplyEmpty - 必须回复空
//     if (strcmp(p, "vMustReplyEmpty") == 0) {
//         // 立即回复空包
//         gdb_send_packet((char*)"");
//         return;
//     }

//     // vCont? - 查询支持的继续命令
//     if (strcmp(p, "vCont?") == 0) {
//         gdb_send_packet((char*)"vCont;c;C;s;S");
//         return;
//     }

//     // qC - 查询当前线程 ID
//     if (p[0] == 'q' && p[1] == 'C' && p[2] == '\0') {
//         gdb_send_packet((char*)"QC1");
//         return;
//     }

//     // qAttached - 查询是否attached
//     if (strcmp(p, "qAttached") == 0) {
//         gdb_send_packet((char*)"1");  // 已attached
//         return;
//     }

//     // qTStatus - 线程状态查询，返回空表示不支持
//     if (strcmp(p, "qTStatus") == 0) {
//         gdb_send_packet((char*)"");
//         return;
//     }

//     // qfThreadInfo / qsThreadInfo - 线程信息查询，返回空表示不支持
//     if (strncmp(p, "qfThreadInfo", 12) == 0 || strncmp(p, "qsThreadInfo", 12) == 0) {
//         gdb_send_packet((char*)"");
//         return;
//     }

//     // 其他查询命令返回空
//     gdb_send_packet((char*)"");
// }
void gdb_handle_query(char* p) {
    // 调试日志（您的实现很好）
    char query_msg[DEBUG_BUFFER_SIZE + 100];
    int pos = 0;
    const char* prefix = "[GDB] Query packet: ";
    while (*prefix) query_msg[pos++] = *prefix++;
    int i = 0;
    while (p[i] != '\0' && i < DEBUG_BUFFER_SIZE - 1) {
        query_msg[pos++] = p[i++];
    }
    query_msg[pos++] = '\n';
    query_msg[pos] = '\0';
    Diagnose::Write(query_msg);

    // qSupported - GDB 特性查询
    if (strncmp(p, "qSupported", 10) == 0) {
        gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;vContSupported+;QStartNoAckMode-;timeout-;qXfer:features:read-;qXfer:threads:read-");
        return;
    }

    // vMustReplyEmpty - 必须回复空
    if (strcmp(p, "vMustReplyEmpty") == 0) {
        gdb_send_packet((char*)"");
        return;
    }

    // vCont? - 查询支持的继续命令
    if (strcmp(p, "vCont?") == 0) {
        gdb_send_packet((char*)"vCont;c;C;s;S");
        return;
    }

    // qC - 查询当前线程 ID
    if (strcmp(p, "qC") == 0) {
        gdb_send_packet((char*)"QC1");
        return;
    }

    // qAttached - 查询是否attached
    if (strcmp(p, "qAttached") == 0) {
        gdb_send_packet((char*)"1");
        return;
    }

    // qTStatus - 线程状态查询
    if (strcmp(p, "qTStatus") == 0) {
        gdb_send_packet((char*)"");  // 不支持跟踪
        return;
    }

    // qfThreadInfo - 查询第一个线程
    if (strcmp(p, "qfThreadInfo") == 0) {
        gdb_send_packet((char*)"m1");  // 单线程系统，只有线程1
        return;
    }

    // qsThreadInfo - 查询下一个线程
    if (strcmp(p, "qsThreadInfo") == 0) {
        gdb_send_packet((char*)"l");   // 列表结束
        return;
    }

    // QStartNoAckMode - 禁用ACK模式
    if (strcmp(p, "QStartNoAckMode") == 0) {
        gdb_send_packet((char*)"OK");  // 确认支持
        return;
    }

    // 其他查询命令返回空
    gdb_send_packet((char*)"");
}

// 处理信号命令
void gdb_handle_signal(char* p) {
    // 发送停止信号 (S05 = SIGTRAP)
    gdb_send_packet((char*)"S05");
}
