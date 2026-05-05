// GDB 串口通信层头文件

#ifndef GDB_SERIAL_H
#define GDB_SERIAL_H

#include "../../include/sys/types.h"

// 串口相关常量
#define SERIAL_COM1_BASE    0x3F8  // COM1 基地址
#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_CMD_PORT(base)      (base + 2)
#define SERIAL_LINE_CMD_PORT(base)      (base + 3)
#define SERIAL_MODEM_CMD_PORT(base)     (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

// 串口初始化配置
#define SERIAL_BAUD_DIVISOR  12  // 115200 baud (115200 / 9600 = 12)

// 数据包缓冲区大小
#define SERIAL_BUFFER_SIZE  4096

// 线程安全锁（如果内核支持）
// #define SERIAL_USE_LOCK

// 串口初始化
int serial_init(void);

// 发送一个字节
void serial_outb(unsigned char data);

// 接收一个字节（阻塞）
unsigned char serial_inb(void);

// 接收一个字节（非阻塞，返回-1表示无数据）
int serial_inb_nb(void);

// 发送数据
int serial_write(const char* data, int len);

// 接收数据（非阻塞）
int serial_read(char* buffer, int max_len);

// 检查是否有数据可读
int serial_readable(void);

// 发送数据包（带校验和）
int serial_send_packet(const char* data, int len);

// 接收数据包（带校验和）
int serial_recv_packet(char* buffer, int max_len);

int check_for_interrupt(void);

int serial_recv_packet_with_interrupt(char* buffer, int max_len);
void serial_set_rx_interrupt_enabled(int enabled);

// 发送ACK确认
void serial_send_ack(void);

// 发送NACK重传请求
void serial_send_nack(void);

#endif // GDB_SERIAL_H
