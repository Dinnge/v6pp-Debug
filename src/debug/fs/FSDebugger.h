#ifndef FS_DEBUGGER_H
#define FS_DEBUGGER_H

#include "../../include/FileSystem.h"
#include "../../include/INode.h"
#include "../../include/BufferManager.h"
#include "../../include/FileManager.h"
#include "../../include/Utility.h"

class FSDebugger
{
public:
    FSDebugger();
    ~FSDebugger();

    void Initialize(FileSystem* fs, FileManager* fm, BufferManager* bm);
    
    void PrintHelp();

    void ViewDiskBlock(int blockNo);

    void ViewInode(int inodeNo);

    void ListDirectory(const char* path);

    void ViewSuperBlock();

    void ListAllInodes();

    void TraceDirectory(const char* path);

private:
    void PrintHex(const unsigned char* data, int size);
    void PrintAscii(const unsigned char* data, int size);
    void PrintInodeInfo(DiskInode* dinode);
    void PrintDirectoryEntry(const char* name, int inodeNo);
    void DecodeFileMode(unsigned int mode);
    void TraverseDirectory(Inode* dirInode);

    FileSystem* m_FileSystem;
    FileManager* m_FileManager;
    BufferManager* m_BufferManager;
};

#endif
