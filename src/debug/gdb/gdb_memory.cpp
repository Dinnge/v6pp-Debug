// // GDB 鍐呭瓨绠＄悊瀹炵幇

// #include "gdb_memory.h"
// #include "../../include/Video.h"

// // 鍗佸叚杩涘埗瀛楃琛�1锟�7
// static const char* hex_chars = "0123456789abcdef";

// // 妫拷鏌ュ湴鍧拷鏄惁鍙闂�1锟�7
// int gdb_check_memory_access(uint32_t addr) {
//     // 绠拷鍗曟鏌ワ細閬垮厤璁块棶鍐呮牳绌洪棿澶栨垨 NULL
//     if (addr == 0) return 0;
//     if (addr < 0x1000) return 0;  // 浣庡湴鍧拷淇濈暀
//     // 鍙互娣诲姞鏇村妫拷鏌�1锟�7...

//     return 1;
// }

// // 璇诲彇鍐呭瓨
// int gdb_read_memory(uint32_t addr, char* buffer, int len) {
//     if (!gdb_check_memory_access(addr)) {
//         return -1;
//     }

//     // 鐩存帴璇诲彇鍐呭瓨
//     char* mem_ptr = (char*)addr;

//     for (int i = 0; i < len; i++) {
//         buffer[i] = mem_ptr[i];
//     }

//     return len;
// }

// // 鍐欏叆鍐呭瓨
// int gdb_write_memory(uint32_t addr, const char* data, int len) {
//     if (!gdb_check_memory_access(addr)) {
//         return -1;
//     }

//     // 鐩存帴鍐欏叆鍐呭瓨
//     char* mem_ptr = (char*)addr;

//     for (int i = 0; i < len; i++) {
//         mem_ptr[i] = data[i];
//     }

//     return len;
// }
// GDB 内存管理实现

// GDB 内存管理实现

#include "gdb_memory.h"
#include "../../include/Video.h"

// 十六进制字符表
static const char* hex_chars = "0123456789abcdef";

// 检查地址是否可访问
int gdb_check_memory_access(uint32_t addr) {
    // 拒绝 NULL 指针
    if (addr == 0) return 0;
    
    // 拒绝过低地址（可能保留）
    if (addr < 0x1000) return 0;
    
    // 允许引导加载程序区域 (0x7C00-0x7DFF)
    if (addr >= 0x7C00 && addr < 0x7E00) return 1;
    
    // 允许 BIOS 数据区域 (0x9FC00-0x9FFFF)
    if (addr >= 0x9FC00 && addr < 0xA0000) return 1;
    
    // 内核从 1MB 开始，假设内核大小不超过 64MB
    // 允许访问内核地址空间：0x100000 到 0x2000000 (32MB)
    if (addr >= 0x100000 && addr < 0x2000000) return 1;
    
    // 拒绝其他地址（包括用户空间）
    return 0;
}

// 读取内存
int gdb_read_memory(uint32_t addr, char* buffer, int len) {
    if (!gdb_check_memory_access(addr)) {
        Diagnose::Write("[GDB] Memory access denied for address 0x");
        // 简单输出地址（十六进制）
        char hex[9];
        for (int i = 7; i >= 0; i--) {
            int nibble = (addr >> (i * 4)) & 0xF;
            hex[7 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
        }
        hex[8] = '\0';
        Diagnose::Write(hex);
        Diagnose::Write("\n");
        return -1;
    }

    // 调试：输出正在访问的地址（简洁版）
    // 避免过多输出，只在必要时启用
    // Diagnose::Write("[GDB] Reading memory at 0x");
    // ... (省略详细输出)

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

