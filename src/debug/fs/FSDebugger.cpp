#include "FSDebugger.h"
#include "../../include/Video.h"
#include "../../include/Kernel.h"

// 全局变量用于存储当前路径和位置
static const char* g_current_path = nullptr;
static int g_path_index = 0;

// 自定义的路径字符提供函数
static char DebuggerNextChar() {
    if (g_current_path == nullptr) {
        return '\0';
    }
    
    char c = g_current_path[g_path_index];
    if (c != '\0') {
        g_path_index++;
    }
    return c;
}

FSDebugger::FSDebugger()
    : m_FileSystem(nullptr)
    , m_FileManager(nullptr)
    , m_BufferManager(nullptr)
{
}

FSDebugger::~FSDebugger()
{
}

void FSDebugger::Initialize(FileSystem* fs, FileManager* fm, BufferManager* bm)
{
    m_FileSystem = fs;
    m_FileManager = fm;
    m_BufferManager = bm;
}

void FSDebugger::PrintHelp()
{
    Diagnose::Write("=== File System Debugger Help ===\n");
    Diagnose::Write("help          - Show this help message\n");
    Diagnose::Write("block <n>     - View disk block n\n");
    Diagnose::Write("inode <n>     - View inode n\n");
    Diagnose::Write("ls <path>     - List directory\n");
    Diagnose::Write("super         - View superblock\n");
    Diagnose::Write("inodes        - List all inodes\n");
    Diagnose::Write("trace <path>  - Trace directory traversal\n");
    Diagnose::Write("================================\n");
}

// 辅助函数：将整数转换为16进制字符串
static void int_to_hex(int value, char* buf, int len) {
    const char* hex_chars = "0123456789abcdef";
    int i = len - 1;
    buf[i] = '\0';
    i--;
    
    if (value == 0) {
        buf[i] = '0';
        i--;
    } else {
        while (value > 0 && i >= 0) {
            buf[i] = hex_chars[value & 0x0F];
            value >>= 4;
            i--;
        }
    }
    
    // 前导空格
    while (i >= 0) {
        buf[i] = ' ';
        i--;
    }
}

// 辅助函数：将整数转换为十进制字符串
static void int_to_dec(int value, char* buf, int len) {
    int i = len - 1;
    buf[i] = '\0';
    i--;
    
    if (value == 0) {
        buf[i] = '0';
        i--;
    } else {
        int is_negative = 0;
        if (value < 0) {
            is_negative = 1;
            value = -value;
        }
        
        while (value > 0 && i >= 0) {
            buf[i] = '0' + (value % 10);
            value /= 10;
            i--;
        }
        
        if (is_negative && i >= 0) {
            buf[i] = '-';
            i--;
        }
    }
    
    // 前导空格
    while (i >= 0) {
        buf[i] = ' ';
        i--;
    }
}

void FSDebugger::PrintHex(const unsigned char* data, int size)
{
    char buf[32];
    
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0) {
            Diagnose::Write("\n0x");
            int_to_hex(i, buf, 32);
            Diagnose::Write(buf + 24); // 只显示后8位
            Diagnose::Write(": ");
        }
        
        const char* hex_chars = "0123456789abcdef";
        char hex_byte[3];
        hex_byte[0] = hex_chars[(data[i] >> 4) & 0x0F];
        hex_byte[1] = hex_chars[data[i] & 0x0F];
        hex_byte[2] = '\0';
        Diagnose::Write(hex_byte);
        Diagnose::Write(" ");
    }
    Diagnose::Write("\n");
}

void FSDebugger::PrintAscii(const unsigned char* data, int size)
{
    char buf[2];
    buf[1] = '\0';
    
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0) {
            Diagnose::Write("\n0x");
            char hex_buf[32];
            int_to_hex(i, hex_buf, 32);
            Diagnose::Write(hex_buf + 24); // 只显示后8位
            Diagnose::Write(": ");
        }
        
        unsigned char c = data[i];
        buf[0] = (c >= 32 && c < 127) ? c : '.';
        Diagnose::Write(buf);
    }
    Diagnose::Write("\n");
}

void FSDebugger::ViewDiskBlock(int blockNo)
{
    if (!m_BufferManager) {
        Diagnose::Write("Error: Buffer manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Viewing disk block ");
    char buf[32];
    int_to_dec(blockNo, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write(" ===\n");
    
    Buf* bp = m_BufferManager->Bread(1, blockNo);
    
    if (bp) {
        Diagnose::Write("Hex dump:\n");
        PrintHex((const unsigned char*)bp->b_addr, 512);
        Diagnose::Write("\nASCII dump:\n");
        PrintAscii((const unsigned char*)bp->b_addr, 512);
        m_BufferManager->Brelse(bp);
    } else {
        Diagnose::Write("Error: Could not read block\n");
    }
}

void FSDebugger::DecodeFileMode(unsigned int mode)
{
    Diagnose::Write("File mode: 0x");
    char buf[32];
    int_to_hex(mode, buf, 32);
    Diagnose::Write(buf + 24);
    Diagnose::Write(" (");
    
    if (mode & Inode::IFDIR) {
        Diagnose::Write("d");
    } else if (mode & Inode::IFCHR) {
        Diagnose::Write("c");
    } else if (mode & Inode::IFBLK) {
        Diagnose::Write("b");
    } else {
        Diagnose::Write("-");
    }
    
    Diagnose::Write((mode & Inode::IREAD) ? "r" : "-");
    Diagnose::Write((mode & Inode::IWRITE) ? "w" : "-");
    Diagnose::Write((mode & Inode::IEXEC) ? "x" : "-");
    Diagnose::Write((mode & Inode::IRWXG) & Inode::IREAD ? "r" : "-");
    Diagnose::Write((mode & Inode::IRWXG) & Inode::IWRITE ? "w" : "-");
    Diagnose::Write((mode & Inode::IRWXG) & Inode::IEXEC ? "x" : "-");
    Diagnose::Write((mode & Inode::IRWXO) & Inode::IREAD ? "r" : "-");
    Diagnose::Write((mode & Inode::IRWXO) & Inode::IWRITE ? "w" : "-");
    Diagnose::Write((mode & Inode::IRWXO) & Inode::IEXEC ? "x" : "-");
    Diagnose::Write(")\n");
}

void FSDebugger::PrintInodeInfo(DiskInode* dinode)
{
    if (!dinode) return;
    
    DecodeFileMode(dinode->d_mode);
    Diagnose::Write("Links: ");
    char buf[32];
    int_to_dec(dinode->d_nlink, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write("\n");
    Diagnose::Write("UID: ");
    int_to_dec(dinode->d_uid, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write(", GID: ");
    int_to_dec(dinode->d_gid, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write("\n");
    Diagnose::Write("Size: ");
    int_to_dec(dinode->d_size, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write(" bytes\n");
    Diagnose::Write("Direct blocks: [");
    for (int i = 0; i < 6; i++) {
        int_to_dec(dinode->d_addr[i], buf, 32);
        Diagnose::Write(buf + 22);
        if (i < 5) Diagnose::Write(", ");
    }
    Diagnose::Write("]\n");
    Diagnose::Write("Indirect blocks: [");
    for (int i = 6; i < 8; i++) {
        int_to_dec(dinode->d_addr[i], buf, 32);
        Diagnose::Write(buf + 22);
        if (i < 7) Diagnose::Write(", ");
    }
    Diagnose::Write("]\n");
    Diagnose::Write("Double indirect: ");
    int_to_dec(dinode->d_addr[8], buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write("\n");
    Diagnose::Write("Triple indirect: ");
    int_to_dec(dinode->d_addr[9], buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write("\n");
}

void FSDebugger::ViewInode(int inodeNo)
{
    if (!m_BufferManager) {
        Diagnose::Write("Error: Buffer manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Viewing inode ");
    char buf[32];
    int_to_dec(inodeNo, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write(" ===\n");
    
    int blockNo = FileSystem::INODE_ZONE_START_SECTOR + (inodeNo - 1) / FileSystem::INODE_NUMBER_PER_SECTOR;
    int offset = ((inodeNo - 1) % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
    
    Buf* bp = m_BufferManager->Bread(1, blockNo);
    
    if (bp) {
        DiskInode* dinode = (DiskInode*)((char*)bp->b_addr + offset);
        PrintInodeInfo(dinode);
        m_BufferManager->Brelse(bp);
    } else {
        Diagnose::Write("Error: Could not read inode block\n");
    }
}

void FSDebugger::ViewSuperBlock()
{
    if (!m_BufferManager) {
        Diagnose::Write("Error: Buffer manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Superblock Information ===\n");
    
    Buf* bp = m_BufferManager->Bread(1, FileSystem::SUPER_BLOCK_SECTOR_NUMBER);
    
    if (bp) {
        SuperBlock* sb = (SuperBlock*)bp->b_addr;
        char buf[32];
        
        Diagnose::Write("Inode zone size (blocks): ");
        int_to_dec(sb->s_isize, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("File system size (blocks): ");
        int_to_dec(sb->s_fsize, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Free blocks available: ");
        int_to_dec(sb->s_nfree, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Free inodes available: ");
        int_to_dec(sb->s_ninode, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Modified: ");
        int_to_dec(sb->s_fmod, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Read-only: ");
        int_to_dec(sb->s_ronly, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Last update time: ");
        int_to_dec(sb->s_time, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        
        m_BufferManager->Brelse(bp);
    } else {
        Diagnose::Write("Error: Could not read superblock\n");
    }
}

void FSDebugger::ListAllInodes()
{
    if (!m_BufferManager) {
        Diagnose::Write("Error: Buffer manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Listing All Inodes ===\n");
    
    int totalInodes = FileSystem::INODE_ZONE_SIZE * FileSystem::INODE_NUMBER_PER_SECTOR;
    char buf[32];
    
    for (int inodeNo = 1; inodeNo <= totalInodes; inodeNo++) {
        int blockNo = FileSystem::INODE_ZONE_START_SECTOR + (inodeNo - 1) / FileSystem::INODE_NUMBER_PER_SECTOR;
        int offset = ((inodeNo - 1) % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
        
        Buf* bp = m_BufferManager->Bread(1, blockNo);
        
        if (bp) {
            DiskInode* dinode = (DiskInode*)((char*)bp->b_addr + offset);
            
            if (dinode->d_mode & Inode::IALLOC) {
                Diagnose::Write("Inode ");
                int_to_dec(inodeNo, buf, 32);
                Diagnose::Write(buf + 22);
                Diagnose::Write(": ");
                
                if (dinode->d_mode & Inode::IFDIR) {
                    Diagnose::Write("dir");
                } else if (dinode->d_mode & Inode::IFCHR) {
                    Diagnose::Write("chr");
                } else if (dinode->d_mode & Inode::IFBLK) {
                    Diagnose::Write("blk");
                } else {
                    Diagnose::Write("reg");
                }
                
                Diagnose::Write(", size=");
                int_to_dec(dinode->d_size, buf, 32);
                Diagnose::Write(buf + 22);
                Diagnose::Write("\n");
            }
            
            m_BufferManager->Brelse(bp);
        }
    }
}

void FSDebugger::PrintDirectoryEntry(const char* name, int inodeNo)
{
    Diagnose::Write("[");
    char buf[32];
    int_to_dec(inodeNo, buf, 32);
    Diagnose::Write(buf + 22);
    Diagnose::Write("] ");
    Diagnose::Write(name);
    Diagnose::Write("\n");
}

void FSDebugger::TraverseDirectory(Inode* dirInode)
{
    if (!dirInode || !m_BufferManager) return;
    
    if (!(dirInode->i_mode & Inode::IFDIR)) {
        Diagnose::Write("Not a directory\n");
        return;
    }
    
    int offset = 0;
    int size = dirInode->i_size;
    
    while (offset < size) {
        int lbn = offset / Inode::BLOCK_SIZE;
        int blockOffset = offset % Inode::BLOCK_SIZE;
        
        int pbn = dirInode->Bmap(lbn);
        if (pbn == 0) break;
        
        Buf* bp = m_BufferManager->Bread(1, pbn);
        if (!bp) break;
        
        char* dirData = (char*)bp->b_addr + blockOffset;
        int remaining = Inode::BLOCK_SIZE - blockOffset;
        int toRead = (size - offset) < remaining ? (size - offset) : remaining;
        
        int entryOffset = 0;
        while (entryOffset < toRead) {
            int inodeNo = *(int*)(dirData + entryOffset);
            entryOffset += sizeof(int);
            
            if (inodeNo == 0) {
                entryOffset += 14;
                continue;
            }
            
            char name[15];
            for (int i = 0; i < 14; i++) {
                name[i] = dirData[entryOffset + i];
            }
            name[14] = '\0';
            
            PrintDirectoryEntry(name, inodeNo);
            
            entryOffset += 14;
        }
        
        m_BufferManager->Brelse(bp);
        offset += toRead;
    }
}

void FSDebugger::ListDirectory(const char* path)
{
    if (!m_FileManager) {
        Diagnose::Write("Error: File manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Listing directory: ");
    Diagnose::Write(path);
    Diagnose::Write(" ===\n");
    
    // 设置全局路径变量
    g_current_path = path;
    g_path_index = 0;
    
    Inode* dirInode = m_FileManager->NameI(DebuggerNextChar, FileManager::OPEN);
    if (dirInode) {
        TraverseDirectory(dirInode);
        dirInode->i_flag &= ~Inode::ILOCK;
        dirInode->Prele();
    } else {
        Diagnose::Write("Error: Directory not found\n");
    }
    
    // 清理
    g_current_path = nullptr;
    g_path_index = 0;
}

void FSDebugger::TraceDirectory(const char* path)
{
    if (!m_FileManager) {
        Diagnose::Write("Error: File manager not initialized\n");
        return;
    }
    
    Diagnose::Write("=== Tracing path: ");
    Diagnose::Write(path);
    Diagnose::Write(" ===\n");
    
    // 设置全局路径变量
    g_current_path = path;
    g_path_index = 0;
    
    Inode* inode = m_FileManager->NameI(DebuggerNextChar, FileManager::OPEN);
    if (inode) {
        Diagnose::Write("Path resolved successfully!\n");
        Diagnose::Write("Inode number: ");
        char buf[32];
        int_to_dec(inode->i_number, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write("\n");
        Diagnose::Write("Size: ");
        int_to_dec(inode->i_size, buf, 32);
        Diagnose::Write(buf + 22);
        Diagnose::Write(" bytes\n");
        DecodeFileMode(inode->i_mode);
        
        inode->i_flag &= ~Inode::ILOCK;
        inode->Prele();
    } else {
        Diagnose::Write("Error: Path not found\n");
    }
    
    // 清理
    g_current_path = nullptr;
    g_path_index = 0;
}
