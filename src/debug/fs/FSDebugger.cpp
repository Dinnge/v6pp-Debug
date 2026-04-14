#include "FSDebugger.h"
#include "../../include/Video.h"
#include "../../include/Kernel.h"
#include "../../include/ATADriver.h"
#include "../../include/DeviceManager.h"
#include "../../include/IOPort.h"

static int wait_for_ata_mask(unsigned char mask, unsigned char expected)
{
    for (int ticks = 0; ticks < 100000; ticks++) {
        unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
        if ((status & mask) == expected) {
            return 1;
        }
    }
    return 0;
}

FSDebugger::FSDebugger()
    : m_FileSystem(nullptr)
    , m_FileManager(nullptr)
    , m_BufferManager(nullptr)
    , m_OutputWriter(nullptr)
    , m_OutputContext(nullptr)
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

void FSDebugger::SetOutputWriter(OutputWriter writer, void* context)
{
    m_OutputWriter = writer;
    m_OutputContext = context;
}

void FSDebugger::ResetOutputWriter()
{
    m_OutputWriter = nullptr;
    m_OutputContext = nullptr;
}

void FSDebugger::Write(const char* text)
{
    if (m_OutputWriter) {
        m_OutputWriter(text, m_OutputContext);
        return;
    }
    Diagnose::Write(text);
}

void FSDebugger::PrintHelp()
{
    Write("=== File System Debugger Help ===\n");
    Write("help          - Show this help message\n");
    Write("block <n>     - View disk block n\n");
    Write("inode <n>     - View inode n\n");
    Write("ls <path>     - List directory\n");
    Write("super         - View superblock\n");
    Write("inodes        - List all inodes\n");
    Write("trace <path>  - Trace directory traversal\n");
    Write("================================\n");
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
            Write("\n0x");
            int_to_hex(i, buf, 32);
            Write(buf + 24); // 只显示后8位
            Write(": ");
        }
        
        const char* hex_chars = "0123456789abcdef";
        char hex_byte[3];
        hex_byte[0] = hex_chars[(data[i] >> 4) & 0x0F];
        hex_byte[1] = hex_chars[data[i] & 0x0F];
        hex_byte[2] = '\0';
        Write(hex_byte);
        Write(" ");
    }
    Write("\n");
}

void FSDebugger::PrintAscii(const unsigned char* data, int size)
{
    char buf[2];
    buf[1] = '\0';
    
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0) {
            Write("\n0x");
            char hex_buf[32];
            int_to_hex(i, hex_buf, 32);
            Write(hex_buf + 24); // 只显示后8位
            Write(": ");
        }
        
        unsigned char c = data[i];
        buf[0] = (c >= 32 && c < 127) ? c : '.';
        Write(buf);
    }
    Write("\n");
}

void FSDebugger::ViewDiskBlock(int blockNo)
{
    unsigned char block[Inode::BLOCK_SIZE];

    if (!ReadRawSector(blockNo, block)) {
        Write("Error: Could not read block\n");
        return;
    }

    Write("=== Viewing disk block ");
    char buf[32];
    int_to_dec(blockNo, buf, 32);
    Write(buf + 22);
    Write(" ===\n");

    Write("Hex dump:\n");
    PrintHex(block, Inode::BLOCK_SIZE);
    Write("\nASCII dump:\n");
    PrintAscii(block, Inode::BLOCK_SIZE);
}

bool FSDebugger::ReadRawSector(int sectorNo, unsigned char* buffer)
{
    if (sectorNo < 0 || buffer == nullptr) {
        return false;
    }

    if (!wait_for_ata_mask(ATADriver::HD_DEVICE_BUSY, 0)) {
        return false;
    }

    IOPort::OutByte(ATADriver::CTRL_PORT, 0);
    IOPort::OutByte(ATADriver::MODE_PORT,
                     ATADriver::MODE_IDE |
                     ATADriver::MODE_LBA28 |
                     ((sectorNo >> 24) & 0x0F));
    IOPort::OutByte(ATADriver::NSECTOR_PORT, 1);
    IOPort::OutByte(ATADriver::BLKNO_PORT_1, sectorNo & 0xFF);
    IOPort::OutByte(ATADriver::BLKNO_PORT_2, (sectorNo >> 8) & 0xFF);
    IOPort::OutByte(ATADriver::BLKNO_PORT_3, (sectorNo >> 16) & 0xFF);
    IOPort::OutByte(ATADriver::CMD_PORT, ATADriver::HD_READ);

    for (int ticks = 0; ticks < 100000; ticks++) {
        unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
        if (status & ATADriver::HD_ERROR) {
            return false;
        }
        if ((status & ATADriver::HD_DEVICE_BUSY) == 0 &&
            (status & ATADriver::HD_DEVICE_REQUEST) != 0) {
            for (int i = 0; i < Inode::BLOCK_SIZE / 2; i++) {
                unsigned short word = IOPort::InWord(ATADriver::DATA_PORT);
                buffer[i * 2] = (unsigned char)(word & 0xFF);
                buffer[i * 2 + 1] = (unsigned char)((word >> 8) & 0xFF);
            }
            return true;
        }
    }

    return false;
}

bool FSDebugger::ReadRawBytes(int sectorNo, int sectorCount, unsigned char* buffer)
{
    if (sectorNo < 0 || sectorCount <= 0 || buffer == nullptr) {
        return false;
    }

    for (int i = 0; i < sectorCount; i++) {
        if (!ReadRawSector(sectorNo + i, buffer + i * Inode::BLOCK_SIZE)) {
            return false;
        }
    }

    return true;
}

bool FSDebugger::ReadDiskInode(int inodeNo, DiskInode* out)
{
    if (inodeNo <= 0 || out == nullptr) {
        return false;
    }

    unsigned char block[Inode::BLOCK_SIZE];
    int blockNo = FileSystem::INODE_ZONE_START_SECTOR +
                  inodeNo / FileSystem::INODE_NUMBER_PER_SECTOR;
    int offset = (inodeNo % FileSystem::INODE_NUMBER_PER_SECTOR) * (int)sizeof(DiskInode);

    if (!ReadRawSector(blockNo, block)) {
        return false;
    }

    Utility::DWordCopy((int*)(block + offset), (int*)out, sizeof(DiskInode) / sizeof(int));
    return true;
}

int FSDebugger::MapDiskInodeBlock(const DiskInode& inode, int lbn)
{
    if (lbn < 0 || lbn >= Inode::HUGE_FILE_BLOCK) {
        return 0;
    }

    if (lbn < Inode::SMALL_FILE_BLOCK) {
        return inode.d_addr[lbn];
    }

    unsigned char firstBlock[Inode::BLOCK_SIZE];
    unsigned char secondBlock[Inode::BLOCK_SIZE];
    int index = 0;

    if (lbn < Inode::LARGE_FILE_BLOCK) {
        index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6;
    } else {
        index = (lbn - Inode::LARGE_FILE_BLOCK) /
                (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8;
    }

    int firstSector = inode.d_addr[index];
    if (firstSector == 0 || !ReadRawSector(firstSector, firstBlock)) {
        return 0;
    }

    int* firstTable = (int*)firstBlock;
    if (index >= 8) {
        int firstIndex = ((lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) %
                         Inode::ADDRESS_PER_INDEX_BLOCK;
        int secondSector = firstTable[firstIndex];
        if (secondSector == 0 || !ReadRawSector(secondSector, secondBlock)) {
            return 0;
        }
        firstTable = (int*)secondBlock;
    }

    int leafIndex = 0;
    if (lbn < Inode::LARGE_FILE_BLOCK) {
        leafIndex = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    } else {
        leafIndex = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    }

    return firstTable[leafIndex];
}

void FSDebugger::DecodeFileMode(unsigned int mode)
{
    Write("File mode: 0x");
    char buf[32];
    int_to_hex(mode, buf, 32);
    Write(buf + 24);
    Write(" (");
    
    if (mode & Inode::IFDIR) {
        Write("d");
    } else if (mode & Inode::IFCHR) {
        Write("c");
    } else if (mode & Inode::IFBLK) {
        Write("b");
    } else {
        Write("-");
    }
    
    Write((mode & Inode::IREAD) ? "r" : "-");
    Write((mode & Inode::IWRITE) ? "w" : "-");
    Write((mode & Inode::IEXEC) ? "x" : "-");
    Write((mode & Inode::IRWXG) & Inode::IREAD ? "r" : "-");
    Write((mode & Inode::IRWXG) & Inode::IWRITE ? "w" : "-");
    Write((mode & Inode::IRWXG) & Inode::IEXEC ? "x" : "-");
    Write((mode & Inode::IRWXO) & Inode::IREAD ? "r" : "-");
    Write((mode & Inode::IRWXO) & Inode::IWRITE ? "w" : "-");
    Write((mode & Inode::IRWXO) & Inode::IEXEC ? "x" : "-");
    Write(")\n");
}

void FSDebugger::PrintInodeInfo(DiskInode* dinode)
{
    if (!dinode) return;
    
    DecodeFileMode(dinode->d_mode);
    Write("Links: ");
    char buf[32];
    int_to_dec(dinode->d_nlink, buf, 32);
    Write(buf + 22);
    Write("\n");
    Write("UID: ");
    int_to_dec(dinode->d_uid, buf, 32);
    Write(buf + 22);
    Write(", GID: ");
    int_to_dec(dinode->d_gid, buf, 32);
    Write(buf + 22);
    Write("\n");
    Write("Size: ");
    int_to_dec(dinode->d_size, buf, 32);
    Write(buf + 22);
    Write(" bytes\n");
    Write("Direct blocks: [");
    for (int i = 0; i < 6; i++) {
        int_to_dec(dinode->d_addr[i], buf, 32);
        Write(buf + 22);
        if (i < 5) Write(", ");
    }
    Write("]\n");
    Write("Indirect blocks: [");
    for (int i = 6; i < 8; i++) {
        int_to_dec(dinode->d_addr[i], buf, 32);
        Write(buf + 22);
        if (i < 7) Write(", ");
    }
    Write("]\n");
    Write("Double indirect: ");
    int_to_dec(dinode->d_addr[8], buf, 32);
    Write(buf + 22);
    Write("\n");
    Write("Triple indirect: ");
    int_to_dec(dinode->d_addr[9], buf, 32);
    Write(buf + 22);
    Write("\n");
}

void FSDebugger::ViewInode(int inodeNo)
{
    Write("=== Viewing inode ");
    char buf[32];
    int_to_dec(inodeNo, buf, 32);
    Write(buf + 22);
    Write(" ===\n");

    DiskInode dinode;
    if (ReadDiskInode(inodeNo, &dinode)) {
        PrintInodeInfo(&dinode);
    } else {
        Write("Error: Could not read inode block\n");
    }
}

void FSDebugger::ViewSuperBlock()
{
    Write("=== Superblock Information ===\n");

    unsigned char sectors[sizeof(SuperBlock)];
    if (ReadRawBytes(FileSystem::SUPER_BLOCK_SECTOR_NUMBER,
                     sizeof(SuperBlock) / Inode::BLOCK_SIZE,
                     sectors)) {
        SuperBlock* sb = (SuperBlock*)sectors;
        char buf[32];

        Write("Inode zone size (blocks): ");
        int_to_dec(sb->s_isize, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("File system size (blocks): ");
        int_to_dec(sb->s_fsize, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Free blocks available: ");
        int_to_dec(sb->s_nfree, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Free inodes available: ");
        int_to_dec(sb->s_ninode, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Modified: ");
        int_to_dec(sb->s_fmod, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Read-only: ");
        int_to_dec(sb->s_ronly, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Last update time: ");
        int_to_dec(sb->s_time, buf, 32);
        Write(buf + 22);
        Write("\n");
    } else {
        Write("Error: Could not read superblock\n");
    }
}

void FSDebugger::ListAllInodes()
{
    Write("=== Listing All Inodes ===\n");

    int totalInodes = FileSystem::INODE_ZONE_SIZE * FileSystem::INODE_NUMBER_PER_SECTOR;
    char buf[32];

    for (int inodeNo = 1; inodeNo <= totalInodes; inodeNo++) {
        DiskInode dinode;
        if (ReadDiskInode(inodeNo, &dinode) && (dinode.d_mode & Inode::IALLOC)) {
            Write("Inode ");
            int_to_dec(inodeNo, buf, 32);
            Write(buf + 22);
            Write(": ");

            if (dinode.d_mode & Inode::IFDIR) {
                Write("dir");
            } else if (dinode.d_mode & Inode::IFCHR) {
                Write("chr");
            } else if (dinode.d_mode & Inode::IFBLK) {
                Write("blk");
            } else {
                Write("reg");
            }

            Write(", size=");
            int_to_dec(dinode.d_size, buf, 32);
            Write(buf + 22);
            Write("\n");
        }
    }
}

void FSDebugger::PrintDirectoryEntry(const char* name, int inodeNo)
{
    Write("[");
    char buf[32];
    int_to_dec(inodeNo, buf, 32);
    Write(buf + 22);
    Write("] ");
    Write(name);
    Write("\n");
}

void FSDebugger::CopyDirectoryEntryName(const char* entryName, char* out)
{
    int end = DirectoryEntry::DIRSIZ;
    while (end > 0 && entryName[end - 1] == '\0') {
        end--;
    }

    for (int i = 0; i < end; i++) {
        out[i] = entryName[i];
    }
    out[end] = '\0';
}

bool FSDebugger::DirectoryEntryNameEquals(const char* entryName, const char* component)
{
    for (int i = 0; i < DirectoryEntry::DIRSIZ; i++) {
        char lhs = entryName[i];
        char rhs = component[i];

        if (rhs == '\0') {
            while (i < DirectoryEntry::DIRSIZ) {
                if (entryName[i] != '\0') {
                    return false;
                }
                i++;
            }
            return true;
        }

        if (lhs != rhs) {
            return false;
        }
    }

    return component[DirectoryEntry::DIRSIZ] == '\0';
}

bool FSDebugger::ExtractPathComponent(const char*& path, char* component)
{
    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        component[0] = '\0';
        return false;
    }

    int len = 0;
    while (*path != '\0' && *path != '/') {
        if (len < DirectoryEntry::DIRSIZ) {
            component[len++] = *path;
        }
        path++;
    }
    component[len] = '\0';
    return true;
}

void FSDebugger::TraverseDirectory(Inode* dirInode)
{
    if (!dirInode || !m_BufferManager) return;
    
    if (!(dirInode->i_mode & Inode::IFDIR)) {
        Write("Not a directory\n");
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
        const int dirEntrySize = sizeof(int) + DirectoryEntry::DIRSIZ;
        while (entryOffset < toRead) {
            if (entryOffset + dirEntrySize > toRead) {
                break;
            }

            int inodeNo = *(int*)(dirData + entryOffset);
            entryOffset += sizeof(int);
            
            if (inodeNo == 0) {
                entryOffset += DirectoryEntry::DIRSIZ;
                continue;
            }
            
            char name[DirectoryEntry::DIRSIZ + 1];
            for (int i = 0; i < DirectoryEntry::DIRSIZ; i++) {
                name[i] = dirData[entryOffset + i];
            }
            name[DirectoryEntry::DIRSIZ] = '\0';
            
            PrintDirectoryEntry(name, inodeNo);
            
            entryOffset += DirectoryEntry::DIRSIZ;
        }
        
        m_BufferManager->Brelse(bp);
        offset += toRead;
    }
}

void FSDebugger::TraverseDirectoryDisk(const DiskInode& dirInode)
{
    if ((dirInode.d_mode & Inode::IFDIR) == 0) {
        Write("Not a directory\n");
        return;
    }

    const int entrySize = sizeof(DirectoryEntry);
    const int totalEntries = dirInode.d_size / entrySize;
    unsigned char block[Inode::BLOCK_SIZE];
    char name[DirectoryEntry::DIRSIZ + 1];

    for (int entryIndex = 0; entryIndex < totalEntries; entryIndex++) {
        int offset = entryIndex * entrySize;
        int lbn = offset / Inode::BLOCK_SIZE;
        int blockOffset = offset % Inode::BLOCK_SIZE;
        int sector = MapDiskInodeBlock(dirInode, lbn);
        if (sector == 0 || !ReadRawSector(sector, block)) {
            Write("Error: Could not read directory block\n");
            return;
        }

        DirectoryEntry* entry = (DirectoryEntry*)(block + blockOffset);
        if (entry->m_ino == 0) {
            continue;
        }

        CopyDirectoryEntryName(entry->m_name, name);
        PrintDirectoryEntry(name, entry->m_ino);
    }
}

bool FSDebugger::LookupPath(const char* path, DirectoryLookupResult* result, bool trace)
{
    if (path == nullptr || result == nullptr) {
        return false;
    }

    int currentInodeNo = FileSystem::ROOTINO;
    DiskInode currentInode;
    if (!ReadDiskInode(currentInodeNo, &currentInode)) {
        return false;
    }

    if (trace) {
        Write("Start from root inode ");
        char buf[32];
        int_to_dec(currentInodeNo, buf, 32);
        Write(buf + 22);
        Write("\n");
    }

    const char* cursor = path;
    char component[DirectoryEntry::DIRSIZ + 1];
    while (ExtractPathComponent(cursor, component)) {
        if ((currentInode.d_mode & Inode::IFDIR) == 0) {
            Write("Error: Encountered non-directory during traversal\n");
            return false;
        }

        const int entrySize = sizeof(DirectoryEntry);
        const int totalEntries = currentInode.d_size / entrySize;
        bool found = false;

        for (int entryIndex = 0; entryIndex < totalEntries && !found; entryIndex++) {
            int offset = entryIndex * entrySize;
            int lbn = offset / Inode::BLOCK_SIZE;
            int blockOffset = offset % Inode::BLOCK_SIZE;
            int sector = MapDiskInodeBlock(currentInode, lbn);
            unsigned char block[Inode::BLOCK_SIZE];
            if (sector == 0 || !ReadRawSector(sector, block)) {
                return false;
            }

            DirectoryEntry* entry = (DirectoryEntry*)(block + blockOffset);
            if (entry->m_ino == 0 || !DirectoryEntryNameEquals(entry->m_name, component)) {
                continue;
            }

            currentInodeNo = entry->m_ino;
            if (!ReadDiskInode(currentInodeNo, &currentInode)) {
                return false;
            }

            if (trace) {
                Write("  -> ");
                Write(component);
                Write(" (inode ");
                char buf[32];
                int_to_dec(currentInodeNo, buf, 32);
                Write(buf + 22);
                Write(")\n");
            }
            found = true;
        }

        if (!found) {
            if (trace) {
                Write("  !! missing component: ");
                Write(component);
                Write("\n");
            }
            return false;
        }
    }

    result->inodeNo = currentInodeNo;
    result->inode = currentInode;
    return true;
}

void FSDebugger::ListDirectory(const char* path)
{
    if (!path || path[0] == '\0') {
        path = "/";
    }
    
    Write("=== Listing directory: ");
    Write(path);
    Write(" ===\n");

    DirectoryLookupResult result;
    if (LookupPath(path, &result, false)) {
        TraverseDirectoryDisk(result.inode);
    } else {
        Write("Error: Directory not found\n");
    }
}

void FSDebugger::TraceDirectory(const char* path)
{
    if (!path || path[0] == '\0') {
        path = "/";
    }
    
    Write("=== Tracing path: ");
    Write(path);
    Write(" ===\n");

    DirectoryLookupResult result;
    if (LookupPath(path, &result, true)) {
        Write("Path resolved successfully!\n");
        Write("Inode number: ");
        char buf[32];
        int_to_dec(result.inodeNo, buf, 32);
        Write(buf + 22);
        Write("\n");
        Write("Size: ");
        int_to_dec(result.inode.d_size, buf, 32);
        Write(buf + 22);
        Write(" bytes\n");
        DecodeFileMode(result.inode.d_mode);
    } else {
        Write("Error: Path not found\n");
    }
}
