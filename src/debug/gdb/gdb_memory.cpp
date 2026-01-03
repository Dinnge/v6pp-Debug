// GDB 内存管理实现

#include "gdb_memory.h"
#include "../../include/Video.h"

// 十六进制字符衄1�7
static const char* hex_chars = "0123456789abcdef";

// 棢�查地坢�是否可访闄1�7
int gdb_check_memory_access(uint32_t addr) {
    // 箢�单检查：避免访问内核空间外或 NULL
    if (addr == 0) return 0;
    if (addr < 0x1000) return 0;  // 低地坢�保留
    // 可以添加更多棢�柄1�7...

    return 1;
}

// 读取内存
int gdb_read_memory(uint32_t addr, char* buffer, int len) {
    if (!gdb_check_memory_access(addr)) {
        return -1;
    }

    // 直接读取内存
    char* mem_ptr = (char*)addr;

    for (int i = 0; i < len; i++) {
        buffer[i] = mem_ptr[i];
    }

    return len;
}

// 写入内存
int gdb_write_memory(uint32_t addr, const char* data, int len) {
    if (!gdb_check_memory_access(addr)) {
        return -1;
    }

    // 直接写入内存
    char* mem_ptr = (char*)addr;

    for (int i = 0; i < len; i++) {
        mem_ptr[i] = data[i];
    }

    return len;
}
