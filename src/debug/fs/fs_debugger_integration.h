#ifndef FS_DEBUGGER_INTEGRATION_H
#define FS_DEBUGGER_INTEGRATION_H

#include "../../include/FileSystem.h"
#include "../../include/FileManager.h"
#include "../../include/BufferManager.h"

// 初始化文件系统调试器
void fs_debugger_init(FileSystem* fs, FileManager* fm, BufferManager* bm);

// 处理文件系统调试查询命令
void fs_debugger_handle_query(const char* query);

#endif
