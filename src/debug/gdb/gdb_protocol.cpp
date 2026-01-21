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

char* gdb_strchr(const char* str, char c) {
    while (*str != '\0') {
        if (*str == c) return (char*)str;
        str++;
    }
    return 0;
}

int gdb_strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

uint8_t hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

// 十六进制字符串转整数
uint32_t hex_str_to_uint(const char* hex_str) {
    uint32_t result = 0;
    while (*hex_str != '\0') {
        result = (result << 4) | hex_char_to_value(*hex_str);
        hex_str++;
    }
    return result;
}

// 解决大小端不一致的问题
// 交换字节序
uint32_t swap_endian_32(uint32_t value) {
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

// 添加字节序转换辅助函数
uint32_t gdb_hex_to_host32(const char* hex_str) {
    uint32_t result = 0;
    
    // 解析大端序十六进制
    for (int i = 0; i < 8; i += 2) {
        uint8_t byte = (hex_char_to_value(hex_str[i]) << 4) | 
                      hex_char_to_value(hex_str[i+1]);
        result = (result << 8) | byte;
    }
    
    return swap_endian_32(result);
}

void host32_to_gdb_hex(uint32_t value, char* output) {
    // 主机小端序转GDB大端序
    uint32_t be_value = ((value & 0xFF) << 24) |
                       ((value & 0xFF00) << 8) |
                       ((value & 0xFF0000) >> 8) |
                       ((value & 0xFF000000) >> 24);
    
    // 转换为十六进制
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        output[i] = hex_chars[(be_value >> shift) & 0xF];
    }
    output[8] = '\0';
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

void gdb_handle_read_registers(char* p) {

    if (p == NULL || p[0] == '\0') {
        Diagnose::Write("[GDB] 警告: 接收到空指针或空数据包，返回默认寄存器值\n");
        
        // 返回默认寄存器值（全零）
        char default_regs[129];
        for (int i = 0; i < 128; i++) {
            default_regs[i] = '0';
        }
        default_regs[128] = '\0';
        
        gdb_send_packet(default_regs);
        return;
    }

    Diagnose::Write("[DEBUG] Reading registers with BIG-ENDIAN format\n");
    
    uint32_t regs[16] = {0};
    
    // 读取真实寄存器值（小端序）
    __asm__ __volatile__ (
        "movl %%eax, %0\n\t"
        "movl %%ecx, %1\n\t" 
        "movl %%edx, %2\n\t"
        "movl %%ebx, %3\n\t"
        "movl %%esp, %4\n\t"
        "movl %%ebp, %5\n\t"
        "movl %%esi, %6\n\t"
        "movl %%edi, %7\n\t"
        : "=m"(regs[0]), "=m"(regs[1]), "=m"(regs[2]), "=m"(regs[3]),
          "=m"(regs[4]), "=m"(regs[5]), "=m"(regs[6]), "=m"(regs[7])
        :
        : "memory"
    );
    
    // EIP
    __asm__ __volatile__ (
        "call 1f\n\t"
        "1: popl %0\n\t"
        : "=r"(regs[8])
    );
    
    // EFLAGS
    __asm__ __volatile__ (
        "pushfl\n\t"
        "popl %0\n\t"
        : "=r"(regs[9])
    );
    
    // 段寄存器
    uint16_t temp16;
    __asm__ __volatile__ ("movw %%cs, %0" : "=r"(temp16));
    regs[10] = temp16;
    __asm__ __volatile__ ("movw %%ss, %0" : "=r"(temp16));
    regs[11] = temp16;
    __asm__ __volatile__ ("movw %%ds, %0" : "=r"(temp16));
    regs[12] = temp16;
    __asm__ __volatile__ ("movw %%es, %0" : "=r"(temp16));
    regs[13] = temp16;
    __asm__ __volatile__ ("movw %%fs, %0" : "=r"(temp16));
    regs[14] = temp16;
    __asm__ __volatile__ ("movw %%gs, %0" : "=r"(temp16));
    regs[15] = temp16;
    
    // 调试输出：显示关键寄存器转换前后的值
    char le_display[9], be_display[9];
    host32_to_gdb_hex(regs[8], be_display);  // 使用您现有的函数
    Diagnose::Write("[DEBUG] EIP min=0x%08x →  max=0x%s\n", regs[8], be_display);
    
    // host32_to_gdb_hex(regs[4], be_display);
    // Diagnose::Write("[DEBUG] ESP转换: 小端序=0x%08x → 大端序=0x%s\n", regs[4], be_display);
    
    // host32_to_gdb_hex(regs[0], be_display);
    // Diagnose::Write("[DEBUG] EAX转换: 小端序=0x%08x → 大端序=0x%s\n", regs[0], be_display);
    
    // 关键修改：将所有寄存器值转换为大端序格式发送给GDB
    char response[129];
    
    for (int i = 0; i < 16; i++) {
        // 使用您现有的函数：小端序主机值 → 大端序十六进制字符串
        host32_to_gdb_hex(regs[i], response + i * 8);
    }
    response[128] = '\0';
    
    gdb_send_packet(response);
}

// 处理写入寄存器命令 需要将大端转为小端
void gdb_handle_write_registers(char* p) {
    if (p[0] != 'G') {
        gdb_send_error(0);
        return;
    }

    char* data = p + 1;
    
    for (int reg_index = 0; reg_index < GDB_REG_COUNT; reg_index++) {
        // 提取8字符十六进制
        char reg_hex[9] = {0};
        for (int i = 0; i < 8; i++) {
            reg_hex[i] = data[reg_index * 8 + i];
        }
        reg_hex[8] = '\0';
        
        // 关键修改：大端序→小端序转换
        uint32_t reg_value = gdb_hex_to_host32(reg_hex);
        
        // 设置寄存器
        gdb_set_register(reg_index, reg_value);
        //     gdb_send_error(0);
        //     return;
        // }
    }
    
    gdb_send_ok();
}

// 处理读取内存命令（格式：m地址,长度）
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

// 安全的内存读取函数
int gdb_safe_read_memory(uint32_t addr, char* buffer, uint32_t len) {
    // 安全检查1：只允许内核空间访问
    if (addr < 0xC0000000) {
        Diagnose::Write("[GDB] 内存读取失败：拒绝用户空间访问 0x%08x\n", addr);
        return -1;
    }
    
    // 安全检查2：边界检查
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        Diagnose::Write("[GDB] 内存读取失败：无效长度 %u\n", len);
        return -1;
    }
    
    // 安全检查3：地址范围检查
    if (addr + len < addr) {  // 检查溢出
        Diagnose::Write("[GDB] 内存读取失败：地址溢出 0x%08x + %u\n", addr, len);
        return -1;
    }
    
    Diagnose::Write("[GDB] 安全读取: 地址=0x%08x, 长度=%u\n", addr, len);
    
    // 直接内存读取
    for (uint32_t i = 0; i < len; i++) {
        char* src = (char*)(addr + i);
        buffer[i] = *src;
    }
    
    return 0;
}

void gdb_handle_read_memory(char* packet) {
    // 格式: maddr,length
    char* comma = gdb_strchr(packet, ',');
    if (!comma) {
        gdb_send_packet("E01");
        return;
    }
    
    // 提取地址
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    // 关键修改：统一使用直接解析，不进行字节序转换
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    // 解析长度
    char* len_str = comma + 1;
    uint32_t len = hex_str_to_uint(len_str);
    
    // 限制长度
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    // 安全检查
    if (host_addr < 0xC0000000) {
        gdb_send_packet("E00");
        return;
    }
    
    // 内存读取 - 使用正确的host_addr
    static char mem_buffer[DEBUG_BUFFER_SIZE];
    if (gdb_safe_read_memory(host_addr, mem_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    // 转换为十六进制
    static char hex_buffer[DEBUG_BUFFER_SIZE * 2 + 1];
    const char hex_chars[] = "0123456789abcdef";
    
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = (uint8_t)mem_buffer[i];
        hex_buffer[i*2] = hex_chars[(byte >> 4) & 0x0F];
        hex_buffer[i*2 + 1] = hex_chars[byte & 0x0F];
    }
    hex_buffer[len*2] = '\0';
    
    gdb_send_packet(hex_buffer);
}


// 处理写入内存命令（格式：M地址,长度:数据）
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

int gdb_safe_write_memory(uint32_t addr, const char* data, uint32_t len) {
    // 安全检查1：只允许内核空间访问
    if (addr < 0xC0000000) {
        return -1;
    }
    
    // 安全检查2：边界检查
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        return -1;
    }
    
    // 安全检查3：地址范围检查
    if (addr + len < addr) {  // 检查溢出
        return -1;
    }
    
    // 直接内存写入
    for (uint32_t i = 0; i < len; i++) {
        char* dest = (char*)(addr + i);
        *dest = data[i];
    }
    
    return 0;
}

// void gdb_handle_write_memory(char* packet) {
//     Diagnose::Write("[GDB] 内存写入: %s\n", packet);
    
//     // 格式: Maddr,length:data
//     char* comma = gdb_strchr(packet, ',');
//     if (!comma) {
//         Diagnose::Write("[GDB] 无效格式: 缺少逗号\n");
//         gdb_send_packet("E01");
//         return;
//     }
    
//     char* colon = gdb_strchr(comma, ':');
//     if (!colon) {
//         Diagnose::Write("[GDB] 无效格式: 缺少冒号\n");
//         gdb_send_packet("E01");
//         return;
//     }
    
//     // 提取地址
//     int addr_len = comma - (packet + 1);
//     char addr_hex[9] = {0};
//     for (int i = 0; i < addr_len && i < 8; i++) {
//         addr_hex[i] = packet[1 + i];
//     }
//     addr_hex[addr_len] = '\0';
    
//     uint32_t host_addr = gdb_hex_to_host32(addr_hex);
    
//     // 提取长度
//     int len_len = colon - (comma + 1);
//     char len_hex[9] = {0};
//     for (int i = 0; i < len_len && i < 8; i++) {
//         len_hex[i] = comma[1 + i];
//     }
//     len_hex[len_len] = '\0';
    
//     uint32_t len = hex_str_to_uint(len_hex);
    
//     // 提取数据
//     char* data_str = colon + 1;
//     int data_len = gdb_strlen(data_str);
    
//     // 验证数据长度
//     if (data_len != len * 2) {
//         Diagnose::Write("[GDB] 数据长度不匹配: 期望=%u, 实际=%u\n", len * 2, data_len);
//         gdb_send_packet("E03");
//         return;
//     }
    
//     Diagnose::Write("[GDB] 解析: 地址=0x%08x, 长度=%u\n", host_addr, len);
    
//     // 限制长度
//     if (len == 0 || len > DEBUG_BUFFER_SIZE) {
//         len = DEBUG_BUFFER_SIZE;
//     }
    
//     // 安全检查
//     if (host_addr < 0xC0000000) {
//         Diagnose::Write("[GDB] 拒绝用户空间写入: 0x%08x\n", host_addr);
//         gdb_send_packet("E00");
//         return;
//     }
    
//     // 解析十六进制数据
//     static char data_buffer[DEBUG_BUFFER_SIZE];
//     for (uint32_t i = 0; i < len; i++) {
//         char c1 = data_str[i*2];
//         char c2 = data_str[i*2 + 1];
        
//         uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
//         data_buffer[i] = (char)byte;
//     }
    
//     // 写入内存
//     // if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
//     //     gdb_send_packet("E02");
//     //     return;
//     // }
    
//     Diagnose::Write("[GDB] 写入成功: 地址=0x%08x\n", host_addr);
//     gdb_send_packet("OK");
// }
void gdb_handle_write_memory(char* packet) {
    // 格式: Maddr,length:data
    char* comma = gdb_strchr(packet, ',');
    if (!comma) {
        gdb_send_packet("E01");
        return;
    }
    
    char* colon = gdb_strchr(comma, ':');
    if (!colon) {
        gdb_send_packet("E01");
        return;
    }
    
    // 提取地址 - 关键修改：直接解析，不进行字节序转换
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    // 关键修改：直接解析十六进制字符串为uint32_t
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    // 提取长度
    int len_len = colon - (comma + 1);
    char len_hex[9] = {0};
    for (int i = 0; i < len_len && i < 8; i++) {
        len_hex[i] = comma[1 + i];
    }
    len_hex[len_len] = '\0';
    
    uint32_t len = hex_str_to_uint(len_hex);
    
    // 提取数据
    char* data_str = colon + 1;
    int data_len = gdb_strlen(data_str);
    
    // 验证数据长度
    if (data_len != len * 2) {
        gdb_send_packet("E03");
        return;
    }
    
    // 限制长度
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    // 安全检查
    if (host_addr < 0xC0000000) {
        gdb_send_packet("E00");
        return;
    }
    
    // 解析十六进制数据
    static char data_buffer[DEBUG_BUFFER_SIZE];
    for (uint32_t i = 0; i < len; i++) {
        char c1 = data_str[i*2];
        char c2 = data_str[i*2 + 1];
        
        uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
        data_buffer[i] = (char)byte;
    }
    
    // 关键修改：取消注释，启用内存写入
    if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    gdb_send_packet("OK");
}


// 处理设置断点命令（格式：Z类型,地址,长度）
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

    // 关键修改：提取十六进制字符串，直接解析（不进行字节序转换）
    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    // 直接解析为uint32_t（不进行字节序转换）
    addr = hex_str_to_uint(addr_hex);
    
    // 安全检查：确保是内核空间地址
    if (addr < 0xC0000000) {
        gdb_send_error(0);  // 拒绝用户空间断点
        return;
    }

    // 添加断点
    if (gdb_add_breakpoint(addr, type) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

// 处理移除断点命令（格式：z类型,地址,长度）
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
void gdb_handle_remove_breakpoint(char* p) {
    if (p[0] != 'z') {
        gdb_send_error(0);
        return;
    }

    char* ptr = p + 1;
    GDBBreakpointType type = (GDBBreakpointType)(*ptr++ - '0');
    
    if (*ptr == ',') ptr++;
    
    // 提取地址字符串
    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    // 直接解析地址（不进行字节序转换）
    uint32_t addr = hex_str_to_uint(addr_hex);
    
    // 安全检查
    if (addr < 0xC0000000) {
        gdb_send_error(0);
        return;
    }

    // 移除断点
    if (gdb_remove_breakpoint(addr) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

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


