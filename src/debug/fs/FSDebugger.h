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
    typedef void (*OutputWriter)(const char* text, void* context);

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

    void SetOutputWriter(OutputWriter writer, void* context);
    void ResetOutputWriter();

private:
    struct DirectoryLookupResult {
        DiskInode inode;
        int inodeNo;
    };

    void Write(const char* text);
    void PrintHex(const unsigned char* data, int size);
    void PrintAscii(const unsigned char* data, int size);
    void PrintInodeInfo(DiskInode* dinode);
    void PrintDirectoryEntry(const char* name, int inodeNo);
    void DecodeFileMode(unsigned int mode);
    void TraverseDirectory(Inode* dirInode);
    bool ReadRawSector(int sectorNo, unsigned char* buffer);
    bool ReadRawBytes(int sectorNo, int sectorCount, unsigned char* buffer);
    bool ReadDiskInode(int inodeNo, DiskInode* out);
    int MapDiskInodeBlock(const DiskInode& inode, int lbn);
    bool LookupPath(const char* path, DirectoryLookupResult* result, bool trace);
    void TraverseDirectoryDisk(const DiskInode& dirInode);
    bool ExtractPathComponent(const char*& path, char* component);
    bool DirectoryEntryNameEquals(const char* entryName, const char* component);
    void CopyDirectoryEntryName(const char* entryName, char* out);

    FileSystem* m_FileSystem;
    FileManager* m_FileManager;
    BufferManager* m_BufferManager;
    OutputWriter m_OutputWriter;
    void* m_OutputContext;
};

#endif
