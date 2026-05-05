#include "gdb_memory.h"

#include "../../include/Video.h"

static int gdb_is_accessible_address(uint32_t addr) {
    if (addr < 0x1000) {
        return 0;
    }

    if (addr < 0xA0000) {
        return 1;
    }

    if (addr >= 0x100000 && addr < 0x2000000) {
        return 1;
    }

    if (addr >= 0xC0000000 && addr < 0xC0400000) {
        return 1;
    }

    return 0;
}

static int gdb_check_memory_range(uint32_t addr, int len) {
    if (len < 0) {
        return 0;
    }

    if (len == 0) {
        return 1;
    }

    uint32_t end = addr + (uint32_t)len - 1;
    if (end < addr) {
        return 0;
    }

    for (uint32_t cursor = addr; cursor <= end; ++cursor) {
        if (!gdb_is_accessible_address(cursor)) {
            return 0;
        }
    }

    return 1;
}

int gdb_check_memory_access(uint32_t addr) {
    return gdb_is_accessible_address(addr);
}

int gdb_read_memory(uint32_t addr, char* buffer, int len) {
    if (!buffer || !gdb_check_memory_range(addr, len)) {
        Diagnose::Write("[GDB] Memory read denied\n");
        return -1;
    }

    char* mem_ptr = (char*)addr;
    for (int i = 0; i < len; i++) {
        buffer[i] = mem_ptr[i];
    }

    return len;
}

int gdb_write_memory(uint32_t addr, const char* data, int len) {
    if (!data || !gdb_check_memory_range(addr, len)) {
        Diagnose::Write("[GDB] Memory write denied\n");
        return -1;
    }

    char* mem_ptr = (char*)addr;
    for (int i = 0; i < len; i++) {
        mem_ptr[i] = data[i];
    }

    return len;
}
