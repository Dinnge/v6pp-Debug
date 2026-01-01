// GDB 串口通信层实现
// 实现 8250 UART 串口驱动，用于 GDB 远程调试协议

#include "gdb_serial.h"
#include "../../include/Video.h"

// 初始化标志
static int g_serial_initialized = 0;

// 端口 I/O 操作（需要根据 V6++ 的 I/O 接口调整）
static inline void outb(unsigned short port, unsigned char data) {
    // V6++ 的端口输出函数
    // 注意：需要根据 V6++ 实际的 I/O 接口调用
    // 这里使用内联汇编或 V6++ 提供的接口
    __asm__ volatile ("outb %0, %1" : : "a"(data), "d"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char data;
    __asm__ volatile ("inb %1, %0" : "=a"(data) : "d"(port));
    return data;
}

// 串口初始化
int serial_init(void) {
    if (g_serial_initialized) {
        return 0;
    }

    unsigned short base = SERIAL_COM1_BASE;

    // 禁用中断
    outb(SERIAL_MODEM_CMD_PORT(base), 0x00);

    // 启用 DLAB（设置波特率除数）
    outb(SERIAL_LINE_CMD_PORT(base), 0x80);

    // 设置波特率（除数低字节和高字节）
    outb(SERIAL_DATA_PORT(base), SERIAL_BAUD_DIVISOR & 0xFF);
    outb(SERIAL_DATA_PORT(base) + 1, (SERIAL_BAUD_DIVISOR >> 8) & 0xFF);

    // 配置串口：8位数据，无校验位，1个停止位 (8N1)
    // DLAB = 0, 8位数据，无校验，1停止位
    outb(SERIAL_LINE_CMD_PORT(base), 0x03);

    // 启用 FIFO，清空 FIFO，设置 14 字节阈值
    outb(SERIAL_FIFO_CMD_PORT(base), 0xC7);

    // 启用 RTS 和 DSR
    outb(SERIAL_MODEM_CMD_PORT(base), 0x03);

    g_serial_initialized = 1;

    Diagnose::Write("Serial COM1 initialized (115200 baud, 8N1)\n");
    Diagnose::Write("Serial port ready for GDB connection\n");

    return 0;
}

// 发送一个字节
void serial_outb(unsigned char data) {
    unsigned short base = SERIAL_COM1_BASE;

    // 等待发送缓冲区为空
    while ((inb(SERIAL_LINE_STATUS_PORT(base)) & 0x20) == 0) {
        // 等待
    }

    // 发送数据
    outb(SERIAL_DATA_PORT(base), data);
}

// 接收一个字节（阻塞）
unsigned char serial_inb(void) {
    unsigned short base = SERIAL_COM1_BASE;

    // 等待数据到达
    while ((inb(SERIAL_LINE_STATUS_PORT(base)) & 0x01) == 0) {
        // 等待
    }

    // 读取数据
    return inb(SERIAL_DATA_PORT(base));
}

// 接收一个字节（非阻塞）
int serial_inb_nb(void) {
    unsigned short base = SERIAL_COM1_BASE;

    // 检查是否有数据
    if ((inb(SERIAL_LINE_STATUS_PORT(base)) & 0x01) == 0) {
        return -1;  // 无数据
    }

    // 读取数据
    return (int)inb(SERIAL_DATA_PORT(base));
}

// 发送数据
int serial_write(const char* data, int len) {
    int sent = 0;

    for (int i = 0; i < len; i++) {
        serial_outb((unsigned char)data[i]);
        sent++;
    }

    return sent;
}

// 接收数据（非阻塞）
int serial_read(char* buffer, int max_len) {
    int received = 0;

    while (received < max_len) {
        int ch = serial_inb_nb();

        if (ch < 0) {
            break;  // 没有更多数据
        }

        buffer[received++] = (char)ch;
    }

    return received;
}

// 检查是否有数据可读
int serial_readable(void) {
    unsigned short base = SERIAL_COM1_BASE;
    return (inb(SERIAL_LINE_STATUS_PORT(base)) & 0x01) ? 1 : 0;
}

// 发送数据包（带校验和）
int serial_send_packet(const char* data, int len) {
    unsigned char checksum = 0;

    // 发送起始符 '$'
    serial_outb('$');

    // 发送数据并计算校验和
    for (int i = 0; i < len; i++) {
        serial_outb((unsigned char)data[i]);
        checksum += (unsigned char)data[i];
    }

    // 发送结束符 '#'
    serial_outb('#');

    // 发送校验和（十六进制）
    const char* hex = "0123456789abcdef";
    serial_outb((unsigned char)hex[(checksum >> 4) & 0x0F]);
    serial_outb((unsigned char)hex[checksum & 0x0F]);

    return len;
}

// 接收数据包（带校验和）
int serial_recv_packet(char* buffer, int max_len) {
    int state = 0;  // 0: 等待 '$', 1: 接收数据, 2: 等待 '#', 3: 接收校验和
    int len = 0;
    unsigned char checksum_calc = 0;
    unsigned char checksum_recv = 0;
    int checksum_digits = 0;

    while (1) {
        int ch = serial_inb_nb();

        if (ch < 0) {
            return 0;  // 暂无数据
        }

        unsigned char c = (unsigned char)ch;

        switch (state) {
            case 0:  // 等待 '$'
                if (c == '$') {
                    state = 1;
                    len = 0;
                    checksum_calc = 0;
                } else if (c == '+') {
                    // ACK，忽略
                } else if (c == '-') {
                    // NACK，忽略
                }
                break;

            case 1:  // 接收数据
                if (c == '#') {
                    state = 2;
                    checksum_digits = 0;
                    checksum_recv = 0;
                } else if (len < max_len - 1) {
                    buffer[len++] = c;
                    checksum_calc += c;
                }
                break;

            case 2:  // 接收校验和
                checksum_recv <<= 4;
                if (c >= '0' && c <= '9') {
                    checksum_recv |= (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    checksum_recv |= (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    checksum_recv |= (c - 'A' + 10);
                }

                checksum_digits++;

                if (checksum_digits == 2) {
                    // 校验和接收完毕
                    if (checksum_recv == checksum_calc) {
                        // 校验和正确
                        buffer[len] = '\0';
                        serial_send_ack();  // 发送 ACK
                        return len;
                    } else {
                        // 校验和错误
                        serial_send_nack();  // 发送 NACK
                        state = 0;
                        return -1;
                    }
                }
                break;
        }
    }
}

// 发送ACK确认
void serial_send_ack(void) {
    serial_outb('+');
}

// 发送NACK重传请求
void serial_send_nack(void) {
    serial_outb('-');
}
