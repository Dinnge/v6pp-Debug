// GDB 内存管理头文仄1�7

#ifndef GDB_MEMORY_H
#define GDB_MEMORY_H

#include "../../libyrosstd/sys/types.h"

// 读取内存
int gdb_read_memory(uint32_t addr, char* buffer, int len);

// 写入内存
int gdb_write_memory(uint32_t addr, const char* data, int len);

// 棢�查地坢�是否可访闄1�7
int gdb_check_memory_access(uint32_t addr);

#endif // GDB_MEMORY_H
