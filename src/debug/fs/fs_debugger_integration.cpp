// File-system debugger integration for GDB monitor commands.

#include "FSDebugger.h"
#include "fs_trace.h"
#include "../debug.h"
#include "../../include/FileSystem.h"
#include "../../include/FileManager.h"
#include "../../include/BufferManager.h"
#include "../../include/Video.h"

static int simple_strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

static int simple_streq(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return 0;
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

static int starts_with(const char* s, const char* prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int simple_atoi(const char* s) {
    int num = 0;
    while (*s >= '0' && *s <= '9') {
        num = num * 10 + (*s - '0');
        s++;
    }
    return num;
}

static FSDebugger g_fs_debugger;

void fs_debugger_init(FileSystem* fs, FileManager* fm, BufferManager* bm) {
    g_fs_debugger.Initialize(fs, fm, bm);
    fs_trace_reset();
    Diagnose::Write("[FS Debugger] Initialized successfully\n");
}

static void fs_debugger_write(const char* text, FSDebugger::OutputWriter writer, void* context) {
    if (writer) {
        writer(text, context);
        return;
    }
    Diagnose::Write(text);
}

void fs_debugger_handle_query(const char* query, FSDebugger::OutputWriter writer, void* context) {
    if (!writer) {
        Diagnose::Write("[FS Debugger] Query received: ");
        Diagnose::Write(query);
        Diagnose::Write("\n");
    }

    g_fs_debugger.SetOutputWriter(writer, context);

    if (simple_strncmp(query, "qfs:help", 8) == 0 || simple_streq(query, "fshelp")) {
        fs_debugger_write("=== File System Debugger Commands ===\n", writer, context);
        fs_debugger_write("qfs:help      - Show this help\n", writer, context);
        fs_debugger_write("qfs:super     - View superblock\n", writer, context);
        fs_debugger_write("qfs:inodes    - List all inodes\n", writer, context);
        fs_debugger_write("qfs:block n   - View disk block n\n", writer, context);
        fs_debugger_write("qfs:inode n   - View inode n\n", writer, context);
        fs_debugger_write("qfs:ls path   - List directory\n", writer, context);
        fs_debugger_write("qfs:trace p   - Trace directory traversal\n", writer, context);
        fs_debugger_write("dumpblock n   - Dump raw disk block n\n", writer, context);
        fs_debugger_write("showinode n   - Decode inode n\n", writer, context);
        fs_debugger_write("super         - View superblock\n", writer, context);
        fs_debugger_write("inodes        - List all inodes\n", writer, context);
        fs_debugger_write("block n       - View disk block n\n", writer, context);
        fs_debugger_write("inode n       - View inode n\n", writer, context);
        fs_debugger_write("ls path       - List directory\n", writer, context);
        fs_debugger_write("trace path    - Trace directory traversal\n", writer, context);
        fs_debugger_write("txtrace       - Show FS transaction trace log\n", writer, context);
        fs_debugger_write("=====================================\n", writer, context);
    } else if (simple_strncmp(query, "qfs:super", 9) == 0 || simple_streq(query, "super")) {
        g_fs_debugger.ViewSuperBlock();
    } else if (simple_strncmp(query, "qfs:inodes", 10) == 0 || simple_streq(query, "inodes")) {
        g_fs_debugger.ListAllInodes();
    } else if (simple_strncmp(query, "qfs:block ", 10) == 0) {
        int block_no = simple_atoi(query + 10);
        g_fs_debugger.ViewDiskBlock(block_no);
    } else if (starts_with(query, "block ")) {
        int block_no = simple_atoi(query + 6);
        g_fs_debugger.ViewDiskBlock(block_no);
    } else if (starts_with(query, "dumpblock ")) {
        int block_no = simple_atoi(query + 10);
        g_fs_debugger.ViewDiskBlock(block_no);
    } else if (simple_strncmp(query, "qfs:inode ", 10) == 0) {
        int inode_no = simple_atoi(query + 10);
        g_fs_debugger.ViewInode(inode_no);
    } else if (starts_with(query, "inode ")) {
        int inode_no = simple_atoi(query + 6);
        g_fs_debugger.ViewInode(inode_no);
    } else if (starts_with(query, "showinode ")) {
        int inode_no = simple_atoi(query + 10);
        g_fs_debugger.ViewInode(inode_no);
    } else if (simple_strncmp(query, "qfs:ls ", 7) == 0) {
        const char* path = query + 7;
        g_fs_debugger.ListDirectory(path);
    } else if (starts_with(query, "ls ")) {
        const char* path = query + 3;
        g_fs_debugger.ListDirectory(path);
    } else if (simple_strncmp(query, "qfs:trace ", 10) == 0) {
        const char* path = query + 10;
        g_fs_debugger.TraceDirectory(path);
    } else if (starts_with(query, "trace ")) {
        const char* path = query + 6;
        g_fs_debugger.TraceDirectory(path);
    } else if (simple_streq(query, "txtrace") || simple_streq(query, "qfs:txtrace")) {
        fs_trace_dump(writer, context);
    } else {
        fs_debugger_write("Unknown FS debugger command. Use qfs:help for available commands.\n", writer, context);
    }

    g_fs_debugger.ResetOutputWriter();
}
