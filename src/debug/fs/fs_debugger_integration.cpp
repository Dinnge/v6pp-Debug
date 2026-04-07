// 文件系统调试器集成
// 通过 GDB 查询命令提供文件系统调试功能

#include "FSDebugger.h"
#include "../debug.h"
#include "../../include/FileSystem.h"
#include "../../include/FileManager.h"
#include "../../include/BufferManager.h"
#include "../../include/Video.h"

// 简单的字符串操作函数，不依赖标准库
static int simple_strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static int simple_strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

static int simple_atoi(const char* s) {
    int num = 0;
    while (*s >= '0' && *s <= '9') {
        num = num * 10 + (*s - '0');
        s++;
    }
    return num;
}

// 全局文件系统调试器实例
static FSDebugger g_fs_debugger;

// 初始化文件系统调试器
void fs_debugger_init(FileSystem* fs, FileManager* fm, BufferManager* bm) {
    g_fs_debugger.Initialize(fs, fm, bm);
    Diagnose::Write("[FS Debugger] Initialized successfully\n");
}

// 处理文件系统调试查询命令
void fs_debugger_handle_query(const char* query) {
    Diagnose::Write("[FS Debugger] Query received: ");
    Diagnose::Write(query);
    Diagnose::Write("\n");
    
    // 解析查询命令
    if (simple_strncmp(query, "qfs:help", 8) == 0) {
        Diagnose::Write("=== File System Debugger Commands ===\n");
        Diagnose::Write("qfs:help    - Show this help\n");
        Diagnose::Write("qfs:super   - View superblock\n");
        Diagnose::Write("qfs:inodes  - List all inodes\n");
        Diagnose::Write("qfs:block n - View disk block n\n");
        Diagnose::Write("qfs:inode n - View inode n\n");
        Diagnose::Write("qfs:ls path - List directory\n");
        Diagnose::Write("qfs:trace p - Trace directory traversal\n");
        Diagnose::Write("=====================================\n");
    } else if (simple_strncmp(query, "qfs:super", 9) == 0) {
        g_fs_debugger.ViewSuperBlock();
    } else if (simple_strncmp(query, "qfs:inodes", 10) == 0) {
        g_fs_debugger.ListAllInodes();
    } else if (simple_strncmp(query, "qfs:block ", 10) == 0) {
        int block_no = simple_atoi(query + 10);
        g_fs_debugger.ViewDiskBlock(block_no);
    } else if (simple_strncmp(query, "qfs:inode ", 10) == 0) {
        int inode_no = simple_atoi(query + 10);
        g_fs_debugger.ViewInode(inode_no);
    } else if (simple_strncmp(query, "qfs:ls ", 7) == 0) {
        const char* path = query + 7;
        g_fs_debugger.ListDirectory(path);
    } else if (simple_strncmp(query, "qfs:trace ", 10) == 0) {
        const char* path = query + 10;
        g_fs_debugger.TraceDirectory(path);
    } else {
        Diagnose::Write("Unknown FS debugger command. Use qfs:help for available commands.\n");
    }
}
