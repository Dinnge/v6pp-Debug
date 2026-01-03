#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_memory.h"
#include "gdb_breakpoints.h"

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

void gdb_send_packet(char* data) {
    if (g_client_socket == (Socket)-1) return;

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
    gdb_send_packet("OK");
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
        case 'v': return GDB_CMD_VENDOR;;
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
    // 先保存当前寄存器
    gdb_registers_save();

    // 转换为 GDB 格式字符串
    char reg_str[512];
    gdb_registers_to_string(reg_str, sizeof(reg_str));

    // 发送给 GDB
    gdb_send_packet(reg_str);
}

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
void gdb_handle_query(char* p) {
    // qSupported - GDB 特性查询
    if (p[0] == 'q' && p[1] == 'S') {
        gdb_send_packet("PacketSize=1000;qRelocInsn+;multiprocess+");
        return;
    }

    // vMustReplyEmpty - 必须回复空
    if (strcmp(p, "vMustReplyEmpty") == 0) {
        gdb_send_packet("");
        return;
    }

    // vCont? - 查询支持的继续命令
    if (strcmp(p, "vCont?") == 0) {
        gdb_send_packet("vCont;c;C;s;S");
        return;
    }

    // qC - 查询当前线程 ID
    if (p[0] == 'q' && p[1] == 'C' && p[2] == '\0') {
        gdb_send_packet("QC1");
        return;
    }

    // qAttached - 查询是否attached
    if (strcmp(p, "qAttached") == 0) {
        gdb_send_packet("1");  // 已attached
        return;
    }

    // 其他查询命令返回空
    gdb_send_packet("");
}

// 处理信号命令
void gdb_handle_signal(char* p) {
    // 发送停止信号 (S05 = SIGTRAP)
    gdb_send_packet("S05");
}
