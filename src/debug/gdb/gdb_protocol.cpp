#include "gdb_protocol.h"
#include "gdb_socket.h"

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

static int sprintf(char* str, const char* format, ...) {
    // 简化版本：只支持 %02x 格式
    // 实际使用时根据需要扩展
    int pos = 0;
    if (format[0] == 'E' && format[1] == '%' && format[2] == '0' && format[3] == '2' && format[4] == 'x') {
        // 错误码格式
        str[pos++] = format[0];
        // 这里需要可变参数支持，简化处理
        // 暂时不实现完整sprintf
    }
    str[pos] = '\0';
    return pos;
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
        default: return GDB_CMD_UNKNOWN;
    }
}

void gdb_handle_continue(char* p) { gdb_send_ok(); }
void gdb_handle_step(char* p) { gdb_send_ok(); }
void gdb_handle_read_registers(char* p) { gdb_send_packet("00"); }
void gdb_handle_write_registers(char* p) { gdb_send_ok(); }
void gdb_handle_read_memory(char* p) { gdb_send_packet("00"); }
void gdb_handle_write_memory(char* p) { gdb_send_ok(); }
void gdb_handle_set_breakpoint(char* p) { gdb_send_ok(); }
void gdb_handle_remove_breakpoint(char* p) { gdb_send_ok(); }
