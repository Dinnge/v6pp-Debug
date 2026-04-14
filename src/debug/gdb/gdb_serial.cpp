// GDB 串口通信层实现
// 实现 8250 UART 串口驱动，用于 GDB 远程调试协议

#include "gdb_serial.h"
#include "../../include/Video.h"
#include "../../include/Chip8259A.h"

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

    // 启用接收中断（IER: Received Data Available Interrupt Enable）
    outb(base + 1, 0x00);
    outb(base + 4, 0x0B);
    
    // 启用 FIFO，清空 FIFO，设置 14 字节阈值
    outb(SERIAL_FIFO_CMD_PORT(base), 0xC7);

    // 启用 RTS 和 DSR
    outb(SERIAL_MODEM_CMD_PORT(base), 0x0B);

    g_serial_initialized = 1;

    Diagnose::Write("Serial COM1 initialized (115200 baud, 8N1)\n");
    Diagnose::Write("Serial port ready for GDB connection\n");

    return 0;
}

// 发送一个字节
void serial_set_rx_interrupt_enabled(int enabled) {
    unsigned short base = SERIAL_COM1_BASE;
    outb(base + 1, enabled ? 0x01 : 0x00);
    if (enabled) {
        Chip8259A::IrqEnable(Chip8259A::IRQ_COM1);
    } else {
        Chip8259A::IrqDisable(Chip8259A::IRQ_COM1);
    }
}

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
// int serial_recv_packet(char* buffer, int max_len) {
//     int state = 0;  // 0: 等待 '$', 1: 接收数据, 2: 等待 '#', 3: 接收校验和
//     int len = 0;
//     unsigned char checksum_calc = 0;
//     unsigned char checksum_recv = 0;
//     int checksum_digits = 0;

//     while (1) {
//         int ch = serial_inb_nb();

//         if (ch < 0) {
//             return 0;  // 暂无数据
//         }

//         unsigned char c = (unsigned char)ch;

//         switch (state) {
//             case 0:  // 等待 '$'
//                 if (c == '$') {
//                     state = 1;
//                     len = 0;
//                     checksum_calc = 0;
//                 } else if (c == '+') {
//                     // ACK，忽略
//                 } else if (c == '-') {
//                     // NACK，忽略
//                 }
//                 break;

//             case 1:  // 接收数据
//                 if (c == '#') {
//                     state = 2;
//                     checksum_digits = 0;
//                     checksum_recv = 0;
//                 } else if (len < max_len - 1) {
//                     buffer[len++] = c;
//                     checksum_calc += c;
//                 }
//                 break;

//             case 2:  // 接收校验和
//                 checksum_recv <<= 4;
//                 if (c >= '0' && c <= '9') {
//                     checksum_recv |= (c - '0');
//                 } else if (c >= 'a' && c <= 'f') {
//                     checksum_recv |= (c - 'a' + 10);
//                 } else if (c >= 'A' && c <= 'F') {
//                     checksum_recv |= (c - 'A' + 10);
//                 }

//                 checksum_digits++;

//                 if (checksum_digits == 2) {
//                     // 校验和接收完毕
//                     if (checksum_recv == checksum_calc) {
//                         // 校验和正确
//                         buffer[len] = '\0';
//                         serial_send_ack();  // 发送 ACK
//                         return len;
//                     } else {
//                         // 校验和错误
//                         serial_send_nack();  // 发送 NACK
//                         state = 0;
//                         return -1;
//                     }
//                 }
//                 break;
//         }
//     }
// }
int serial_recv_packet(char* buffer, int max_len) {
    int state = 0;  // 0: 等待 '$', 1: 接收数据, 2: 等待 '#', 3: 接收校验和
    int len = 0;
    unsigned char checksum_calc = 0;
    unsigned char checksum_recv = 0;
    int checksum_digits = 0;
    
    // 添加超时机制，避免无限等待
    uint32_t timeout_counter = 0;
    const uint32_t TIMEOUT_LIMIT = 1000000;  // 超时限制

    while (1) {
        int ch = serial_inb_nb();

        if (ch < 0) {
            // 无数据可用，检查超时
            timeout_counter++;
            if (timeout_counter > TIMEOUT_LIMIT) {
                // Diagnose::Write("[SERIAL] 接收超时\n");
                return 0;
            }
            
            // 短暂延迟，避免忙等待
            for (volatile int i = 0; i < 100; i++);
            continue;
        }

        // 重置超时计数器
        timeout_counter = 0;
        
        unsigned char c = (unsigned char)ch;

        // 关键修改：检查是否是中断字符 (Ctrl+C = 0x03)
        if (c == 0x03) {
            // Diagnose::Write("[INTERRUPT] 收到Ctrl+C中断请求\n");
            buffer[0] = 0x03;  // 返回中断字符
            buffer[1] = '\0';
            return 1;  // 返回中断包长度
        }

        switch (state) {
            case 0:  // 等待 '$'
                if (c == '$') {
                    state = 1;
                    len = 0;
                    checksum_calc = 0;
                    // Diagnose::Write("[SERIAL] 开始接收GDB包\n");
                } else if (c == '+') {
                    // ACK，忽略
                    // Diagnose::Write("[SERIAL] 收到ACK\n");
                } else if (c == '-') {
                    // NACK，忽略
                    // Diagnose::Write("[SERIAL] 收到NACK\n");
                } else if (c == 0x03) {
                    // 在非监听状态下收到Ctrl+C，记录日志
                    // Diagnose::Write("[SERIAL] 收到Ctrl+C但未启用中断监听\n");
                }
                break;

            case 1:  // 接收数据
                if (c == '#') {
                    state = 2;
                    checksum_digits = 0;
                    checksum_recv = 0;
                    // Diagnose::Write("[SERIAL] 数据接收完成，等待校验和\n");
                } else if (len < max_len - 1) {
                    buffer[len++] = c;
                    checksum_calc += c;
                } else {
                    // 缓冲区溢出
                    // Diagnose::Write("[SERIAL] 缓冲区溢出，包过长\n");
                    serial_send_nack();
                    state = 0;
                    return -1;
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
                } else {
                    // 无效的校验和字符
                    // Diagnose::Write("[SERIAL] 无效校验和字符: 0x%02x\n", c);
                    serial_send_nack();
                    state = 0;
                    return -1;
                }

                checksum_digits++;

                if (checksum_digits == 2) {
                    // 校验和接收完毕
                    if (checksum_recv == checksum_calc) {
                        // 校验和正确
                        buffer[len] = '\0';
                        serial_send_ack();
                        // Diagnose::Write("[SERIAL] 包接收成功: %s\n", buffer);
                        return len;
                    } else {
                        // 校验和错误
                        Diagnose::Write("[SERIAL] 校验和错误: 计算=0x%02x, 接收=0x%02x\n", 
                                      checksum_calc, checksum_recv);
                        serial_send_nack();
                        state = 0;
                        return -1;
                    }
                }
                break;
        }
    }
}

// 专门用于检测中断的函数（非阻塞）
int check_for_interrupt(void) {
    int ch = serial_inb_nb();
    if (ch >= 0) {
        unsigned char c = (unsigned char)ch;
        if (c == 0x03) {
            Diagnose::Write("[INTERRUPT] 检测到Ctrl+C中断\n");
            return 1;
        }
    }
    return 0;
}

// 带中断检测的包接收函数
int serial_recv_packet_with_interrupt(char* buffer, int max_len) {
    // uint32_t start_time = get_system_time();
    
    while (1) {
        // 检查中断
        if (check_for_interrupt()) {
            buffer[0] = 0x03;
            buffer[1] = '\0';
            return 1;
        }
        
        // 尝试接收包
        int len = serial_recv_packet(buffer, max_len);
        if (len != 0) {
            return len;  // 成功接收到包
        }
        
        // 检查超时
        // if (timeout_ms > 0) {
        //     uint32_t current_time = get_system_time();
        //     if (current_time - start_time > timeout_ms) {
        //         return 0;  // 超时
        //     }
        // }
        
        // 短暂延迟
        for (volatile int i = 0; i < 1000; i++);
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
