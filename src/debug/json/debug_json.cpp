#include "debug_json.h"

#include "../gdb/gdb_registers.h"
#include "../gdb/gdb_memory.h"
#include "../fs/fs_trace.h"
#include "../../include/Kernel.h"
#include "../../include/ProcessManager.h"
#include "../../include/DeviceManager.h"
#include "../../include/FileSystem.h"
#include "../../include/FileManager.h"
#include "../../include/ATADriver.h"
#include "../../include/IOPort.h"
#include "../../include/User.h"

extern ProcessManager g_ProcessManager;
extern FileSystem g_FileSystem;

namespace {

const int kJsonDocumentMax = 32768;
const int kTraceLineMax = 128;
const int kTraceLineCountMax = 80;
const int kBacktraceFrameMax = 16;
const int kPathMax = 256;
const int kFilePreviewMax = 1024;

struct JsonBuilder {
    char* buffer;
    int size;
    int pos;
    int overflow;
};

struct TraceCapture {
    char lines[kTraceLineCountMax][kTraceLineMax];
    int count;
};

struct BacktraceFrame {
    uint32_t eip;
    uint32_t ebp;
};

struct DiskPathLookupResult {
    DiskInode inode;
    int inodeNo;
    char path[kPathMax];
};

static void jb_init(JsonBuilder* jb, char* buffer, int size) {
    jb->buffer = buffer;
    jb->size = size;
    jb->pos = 0;
    jb->overflow = 0;
    if (size > 0) {
        buffer[0] = '\0';
    }
}

static void jb_terminate(JsonBuilder* jb) {
    if (jb->size <= 0) {
        return;
    }
    int end = jb->pos;
    if (end >= jb->size) {
        end = jb->size - 1;
    }
    jb->buffer[end] = '\0';
}

static void jb_append_char(JsonBuilder* jb, char ch) {
    if (jb->pos >= jb->size - 1) {
        jb->overflow = 1;
        return;
    }
    jb->buffer[jb->pos++] = ch;
    jb->buffer[jb->pos] = '\0';
}

static void jb_append_text(JsonBuilder* jb, const char* text) {
    if (text == 0) {
        return;
    }
    for (int i = 0; text[i] != '\0'; i++) {
        jb_append_char(jb, text[i]);
    }
}

static void jb_append_uint(JsonBuilder* jb, uint32_t value) {
    char rev[16];
    int rev_pos = 0;
    if (value == 0) {
        jb_append_char(jb, '0');
        return;
    }
    while (value > 0 && rev_pos < 16) {
        rev[rev_pos++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (rev_pos > 0) {
        jb_append_char(jb, rev[--rev_pos]);
    }
}

static void jb_append_int(JsonBuilder* jb, int value) {
    if (value < 0) {
        jb_append_char(jb, '-');
        jb_append_uint(jb, (uint32_t)(0 - value));
        return;
    }
    jb_append_uint(jb, (uint32_t)value);
}

static void jb_append_hex32(JsonBuilder* jb, uint32_t value) {
    const char* hex = "0123456789abcdef";
    jb_append_text(jb, "\"0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        jb_append_char(jb, hex[(value >> shift) & 0x0F]);
    }
    jb_append_char(jb, '"');
}

static void jb_append_bool(JsonBuilder* jb, int value) {
    jb_append_text(jb, value ? "true" : "false");
}

static void jb_append_escaped(JsonBuilder* jb, const char* text) {
    jb_append_char(jb, '"');
    if (text != 0) {
        for (int i = 0; text[i] != '\0'; i++) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '"' || ch == '\\') {
                jb_append_char(jb, '\\');
                jb_append_char(jb, (char)ch);
            } else if (ch == '\n') {
                jb_append_text(jb, "\\n");
            } else if (ch == '\r') {
                jb_append_text(jb, "\\r");
            } else if (ch == '\t') {
                jb_append_text(jb, "\\t");
            } else if (ch < 32) {
                jb_append_text(jb, "\\u00");
                const char* hex = "0123456789abcdef";
                jb_append_char(jb, hex[(ch >> 4) & 0x0F]);
                jb_append_char(jb, hex[ch & 0x0F]);
            } else {
                jb_append_char(jb, (char)ch);
            }
        }
    }
    jb_append_char(jb, '"');
}

static int str_eq(const char* lhs, const char* rhs) {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0;
        }
        lhs++;
        rhs++;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static int str_starts_with(const char* text, const char* prefix) {
    if (text == 0 || prefix == 0) {
        return 0;
    }
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void safe_str_copy(char* dst, int dst_size, const char* src) {
    if (dst == 0 || dst_size <= 0) {
        return;
    }
    int pos = 0;
    if (src != 0) {
        while (src[pos] != '\0' && pos < dst_size - 1) {
            dst[pos] = src[pos];
            pos++;
        }
    }
    dst[pos] = '\0';
}

static uint32_t parse_u32(const char* text, int base) {
    uint32_t value = 0;
    if (text == 0) {
        return 0;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    while (*text != '\0') {
        char ch = *text++;
        uint32_t digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = (uint32_t)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = (uint32_t)(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            digit = (uint32_t)(ch - 'A' + 10);
        } else {
            break;
        }
        if ((base == 10 && digit >= 10) || (base == 16 && digit >= 16)) {
            break;
        }
        value = value * (uint32_t)base + digit;
    }

    return value;
}

static const char* process_state_name(Process::ProcessState state) {
    switch (state) {
        case Process::SNULL: return "unused";
        case Process::SSLEEP: return "sleep";
        case Process::SWAIT: return "wait";
        case Process::SRUN: return "run";
        case Process::SIDL: return "idle";
        case Process::SZOMB: return "zombie";
        case Process::SSTOP: return "stop";
        default: return "unknown";
    }
}

static void append_process_flags(JsonBuilder* jb, int flags) {
    int first = 1;
    jb_append_char(jb, '[');
    if (flags & Process::SLOAD) {
        jb_append_escaped(jb, "loaded");
        first = 0;
    }
    if (flags & Process::SSYS) {
        if (!first) jb_append_char(jb, ',');
        jb_append_escaped(jb, "system");
        first = 0;
    }
    if (flags & Process::SLOCK) {
        if (!first) jb_append_char(jb, ',');
        jb_append_escaped(jb, "locked");
        first = 0;
    }
    if (flags & Process::SSWAP) {
        if (!first) jb_append_char(jb, ',');
        jb_append_escaped(jb, "swapped");
        first = 0;
    }
    if (flags & Process::STRC) {
        if (!first) jb_append_char(jb, ',');
        jb_append_escaped(jb, "traced");
        first = 0;
    }
    if (flags & Process::STWED) {
        if (!first) jb_append_char(jb, ',');
        jb_append_escaped(jb, "trace-wait");
    }
    jb_append_char(jb, ']');
}

static void capture_trace_writer(const char* text, void* context) {
    TraceCapture* capture = (TraceCapture*)context;
    if (capture == 0 || text == 0 || capture->count >= kTraceLineCountMax) {
        return;
    }
    if (str_starts_with(text, "===")) {
        return;
    }
    safe_str_copy(capture->lines[capture->count], kTraceLineMax, text);
    capture->count++;
}

static void emit_text(DebugJsonWriter writer, void* context, const char* text) {
    if (writer != 0 && text != 0) {
        writer(text, context);
    }
}

static int wait_for_ata_mask(unsigned char mask, unsigned char expected) {
    for (int ticks = 0; ticks < 100000; ticks++) {
        unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
        if ((status & mask) == expected) {
            return 1;
        }
    }
    return 0;
}

static int read_raw_sector(int sector_no, unsigned char* buffer) {
    if (sector_no < 0 || buffer == 0) {
        return 0;
    }

    if (!wait_for_ata_mask(ATADriver::HD_DEVICE_BUSY, 0)) {
        return 0;
    }

    IOPort::OutByte(ATADriver::CTRL_PORT, 0);
    IOPort::OutByte(ATADriver::MODE_PORT,
                    ATADriver::MODE_IDE |
                    ATADriver::MODE_LBA28 |
                    ((sector_no >> 24) & 0x0F));
    IOPort::OutByte(ATADriver::NSECTOR_PORT, 1);
    IOPort::OutByte(ATADriver::BLKNO_PORT_1, sector_no & 0xFF);
    IOPort::OutByte(ATADriver::BLKNO_PORT_2, (sector_no >> 8) & 0xFF);
    IOPort::OutByte(ATADriver::BLKNO_PORT_3, (sector_no >> 16) & 0xFF);
    IOPort::OutByte(ATADriver::CMD_PORT, ATADriver::HD_READ);

    for (int ticks = 0; ticks < 100000; ticks++) {
        unsigned char status = IOPort::InByte(ATADriver::STATUS_PORT);
        if (status & ATADriver::HD_ERROR) {
            return 0;
        }
        if ((status & ATADriver::HD_DEVICE_BUSY) == 0 &&
            (status & ATADriver::HD_DEVICE_REQUEST) != 0) {
            for (int i = 0; i < Inode::BLOCK_SIZE / 2; i++) {
                unsigned short word = IOPort::InWord(ATADriver::DATA_PORT);
                buffer[i * 2] = (unsigned char)(word & 0xFF);
                buffer[i * 2 + 1] = (unsigned char)((word >> 8) & 0xFF);
            }
            return 1;
        }
    }

    return 0;
}

static int read_disk_inode(int inode_no, DiskInode* out) {
    if (inode_no <= 0 || out == 0) {
        return 0;
    }

    unsigned char block[Inode::BLOCK_SIZE];
    int block_no = FileSystem::INODE_ZONE_START_SECTOR +
                   inode_no / FileSystem::INODE_NUMBER_PER_SECTOR;
    int offset = (inode_no % FileSystem::INODE_NUMBER_PER_SECTOR) * (int)sizeof(DiskInode);

    if (!read_raw_sector(block_no, block)) {
        return 0;
    }

    int* src = (int*)(block + offset);
    int* dst = (int*)out;
    for (int i = 0; i < (int)(sizeof(DiskInode) / sizeof(int)); i++) {
        dst[i] = src[i];
    }
    return 1;
}

static int process_manager_ready() {
    return g_ProcessManager.process[0].p_stat != Process::SNULL;
}

static int filesystem_ready() {
    return g_FileSystem.m_Mount[0].m_spb != 0;
}

static void normalize_path(const char* raw_path, char* out, int out_size) {
    if (out == 0 || out_size <= 0) {
        return;
    }

    int pos = 0;
    out[pos++] = '/';

    if (raw_path == 0 || raw_path[0] == '\0') {
        out[pos] = '\0';
        return;
    }

    const char* src = raw_path;
    while (*src == '/') {
        src++;
    }

    int last_was_slash = 1;
    while (*src != '\0' && pos < out_size - 1) {
        char ch = *src++;
        if (ch == '\\') {
            ch = '/';
        }
        if (ch == '/') {
            if (last_was_slash) {
                continue;
            }
            last_was_slash = 1;
        } else {
            last_was_slash = 0;
        }
        out[pos++] = ch;
    }

    while (pos > 1 && out[pos - 1] == '/') {
        pos--;
    }
    out[pos] = '\0';
}

static void copy_directory_entry_name(const char* entry_name, char* out, int out_size) {
    if (out == 0 || out_size <= 0) {
        return;
    }

    int end = DirectoryEntry::DIRSIZ;
    while (end > 0 && entry_name[end - 1] == '\0') {
        end--;
    }

    int copy_len = end;
    if (copy_len > out_size - 1) {
        copy_len = out_size - 1;
    }

    for (int i = 0; i < copy_len; i++) {
        out[i] = entry_name[i];
    }
    out[copy_len] = '\0';
}

static int directory_entry_name_equals(const char* entry_name, const char* component) {
    if (entry_name == 0 || component == 0) {
        return 0;
    }

    for (int i = 0; i < DirectoryEntry::DIRSIZ; i++) {
        char lhs = entry_name[i];
        char rhs = component[i];

        if (rhs == '\0') {
            while (i < DirectoryEntry::DIRSIZ) {
                if (entry_name[i] != '\0') {
                    return 0;
                }
                i++;
            }
            return 1;
        }

        if (lhs != rhs) {
            return 0;
        }
    }

    return component[DirectoryEntry::DIRSIZ] == '\0';
}

static int extract_path_component(const char*& path, char* component, int component_size) {
    if (path == 0 || component == 0 || component_size <= 0) {
        return 0;
    }

    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        component[0] = '\0';
        return 0;
    }

    int len = 0;
    while (*path != '\0' && *path != '/') {
        if (len < component_size - 1) {
            component[len++] = *path;
        }
        path++;
    }
    component[len] = '\0';
    return 1;
}

static int map_disk_inode_block(const DiskInode& inode, int lbn) {
    if (lbn < 0 || lbn >= Inode::HUGE_FILE_BLOCK) {
        return 0;
    }

    if (lbn < Inode::SMALL_FILE_BLOCK) {
        return inode.d_addr[lbn];
    }

    unsigned char first_block[Inode::BLOCK_SIZE];
    unsigned char second_block[Inode::BLOCK_SIZE];
    int index = 0;

    if (lbn < Inode::LARGE_FILE_BLOCK) {
        index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6;
    } else {
        index = (lbn - Inode::LARGE_FILE_BLOCK) /
                (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8;
    }

    int first_sector = inode.d_addr[index];
    if (first_sector == 0 || !read_raw_sector(first_sector, first_block)) {
        return 0;
    }

    int* first_table = (int*)first_block;
    if (index >= 8) {
        int first_index = ((lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) %
                          Inode::ADDRESS_PER_INDEX_BLOCK;
        int second_sector = first_table[first_index];
        if (second_sector == 0 || !read_raw_sector(second_sector, second_block)) {
            return 0;
        }
        first_table = (int*)second_block;
    }

    int leaf_index = 0;
    if (lbn < Inode::LARGE_FILE_BLOCK) {
        leaf_index = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    } else {
        leaf_index = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    }

    return first_table[leaf_index];
}

static int lookup_disk_path(const char* raw_path, DiskPathLookupResult* result) {
    if (result == 0 || !filesystem_ready()) {
        return 0;
    }

    normalize_path(raw_path, result->path, sizeof(result->path));

    int current_inode_no = FileSystem::ROOTINO;
    DiskInode current_inode;
    if (!read_disk_inode(current_inode_no, &current_inode)) {
        return 0;
    }

    const char* cursor = result->path;
    char component[DirectoryEntry::DIRSIZ + 1];
    while (extract_path_component(cursor, component, sizeof(component))) {
        if ((current_inode.d_mode & Inode::IFDIR) == 0) {
            return 0;
        }

        const int entry_size = (int)sizeof(DirectoryEntry);
        const int total_entries = current_inode.d_size / entry_size;
        int found = 0;

        for (int entry_index = 0; entry_index < total_entries && !found; entry_index++) {
            int offset = entry_index * entry_size;
            int lbn = offset / Inode::BLOCK_SIZE;
            int block_offset = offset % Inode::BLOCK_SIZE;
            int sector = map_disk_inode_block(current_inode, lbn);
            unsigned char block[Inode::BLOCK_SIZE];
            if (sector == 0 || !read_raw_sector(sector, block)) {
                return 0;
            }

            DirectoryEntry* entry = (DirectoryEntry*)(block + block_offset);
            if (entry->m_ino == 0 || !directory_entry_name_equals(entry->m_name, component)) {
                continue;
            }

            current_inode_no = entry->m_ino;
            if (!read_disk_inode(current_inode_no, &current_inode)) {
                return 0;
            }
            found = 1;
        }

        if (!found) {
            return 0;
        }
    }

    result->inodeNo = current_inode_no;
    result->inode = current_inode;
    return 1;
}

static int read_inode_preview_bytes(const DiskInode& inode,
                                    unsigned char* buffer,
                                    int buffer_size,
                                    int* out_len) {
    if (buffer == 0 || buffer_size <= 0) {
        return 0;
    }

    int preview_len = inode.d_size;
    if (preview_len > buffer_size) {
        preview_len = buffer_size;
    }
    if (preview_len < 0) {
        preview_len = 0;
    }

    int copied = 0;
    while (copied < preview_len) {
        int lbn = copied / Inode::BLOCK_SIZE;
        int sector = map_disk_inode_block(inode, lbn);
        unsigned char block[Inode::BLOCK_SIZE];
        if (sector == 0 || !read_raw_sector(sector, block)) {
            return 0;
        }

        int block_offset = copied % Inode::BLOCK_SIZE;
        int chunk = Inode::BLOCK_SIZE - block_offset;
        int left = preview_len - copied;
        if (chunk > left) {
            chunk = left;
        }

        for (int i = 0; i < chunk; i++) {
            buffer[copied + i] = block[block_offset + i];
        }
        copied += chunk;
    }

    if (out_len != 0) {
        *out_len = preview_len;
    }
    return 1;
}

static void append_ascii_preview(JsonBuilder* jb, const unsigned char* data, int len) {
    char preview[kFilePreviewMax + 1];
    int copy_len = len;
    if (copy_len > kFilePreviewMax) {
        copy_len = kFilePreviewMax;
    }

    for (int i = 0; i < copy_len; i++) {
        unsigned char ch = data[i];
        if (ch == '\n' || ch == '\r' || ch == '\t' || (ch >= 32 && ch <= 126)) {
            preview[i] = (char)ch;
        } else {
            preview[i] = '.';
        }
    }
    preview[copy_len] = '\0';
    jb_append_escaped(jb, preview);
}

static int try_get_current_process(Process** out_process, User** out_user) {
    if (!process_manager_ready()) {
        return 0;
    }

    User* user = (User*)Kernel::USER_ADDRESS;
    if (user == 0 || user->u_procp == 0) {
        return 0;
    }

    if (out_process) {
        *out_process = user->u_procp;
    }
    if (out_user) {
        *out_user = user;
    }
    return 1;
}

static int collect_backtrace(BacktraceFrame* frames, int max_frames) {
    if (frames == 0 || max_frames <= 0) {
        return 0;
    }

    gdb_registers_save();

    uint32_t eip = gdb_get_register_value(GDB_REG_EIP);
    uint32_t ebp = gdb_get_register_value(GDB_REG_EBP);
    int count = 0;

    frames[count].eip = eip;
    frames[count].ebp = ebp;
    count++;

    while (count < max_frames) {
        if (ebp == 0 || (ebp & 0x3) != 0) {
            break;
        }
        if (!gdb_check_memory_access(ebp) || !gdb_check_memory_access(ebp + 4)) {
            break;
        }

        uint32_t* frame_ptr = (uint32_t*)ebp;
        uint32_t next_ebp = frame_ptr[0];
        uint32_t return_eip = frame_ptr[1];
        if (next_ebp <= ebp || return_eip == 0) {
            break;
        }

        frames[count].eip = return_eip;
        frames[count].ebp = next_ebp;
        count++;
        ebp = next_ebp;
    }

    return count;
}

static void append_memory_descriptor(JsonBuilder* jb, const MemoryDescriptor& md) {
    jb_append_char(jb, '{');
    jb_append_text(jb, "\"textStart\":"); jb_append_hex32(jb, md.m_TextStartAddress);
    jb_append_text(jb, ",\"textSize\":"); jb_append_uint(jb, md.m_TextSize);
    jb_append_text(jb, ",\"dataStart\":"); jb_append_hex32(jb, md.m_DataStartAddress);
    jb_append_text(jb, ",\"dataSize\":"); jb_append_uint(jb, md.m_DataSize);
    jb_append_text(jb, ",\"stackSize\":"); jb_append_uint(jb, md.m_StackSize);
    jb_append_char(jb, '}');
}

static void build_registers_object(JsonBuilder* jb) {
    gdb_registers_save();
    GDBRegisters* regs = gdb_get_registers();
    if (regs == 0) {
        jb_append_text(jb, "{\"status\":\"unavailable\"}");
        return;
    }

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"eax\":"); jb_append_hex32(jb, regs->eax);
    jb_append_text(jb, ",\"ecx\":"); jb_append_hex32(jb, regs->ecx);
    jb_append_text(jb, ",\"edx\":"); jb_append_hex32(jb, regs->edx);
    jb_append_text(jb, ",\"ebx\":"); jb_append_hex32(jb, regs->ebx);
    jb_append_text(jb, ",\"esp\":"); jb_append_hex32(jb, regs->esp);
    jb_append_text(jb, ",\"ebp\":"); jb_append_hex32(jb, regs->ebp);
    jb_append_text(jb, ",\"esi\":"); jb_append_hex32(jb, regs->esi);
    jb_append_text(jb, ",\"edi\":"); jb_append_hex32(jb, regs->edi);
    jb_append_text(jb, ",\"eip\":"); jb_append_hex32(jb, regs->eip);
    jb_append_text(jb, ",\"eflags\":"); jb_append_hex32(jb, regs->eflags);
    jb_append_text(jb, ",\"cs\":"); jb_append_hex32(jb, regs->cs);
    jb_append_text(jb, ",\"ss\":"); jb_append_hex32(jb, regs->ss);
    jb_append_text(jb, ",\"ds\":"); jb_append_hex32(jb, regs->ds);
    jb_append_text(jb, ",\"es\":"); jb_append_hex32(jb, regs->es);
    jb_append_text(jb, ",\"fs\":"); jb_append_hex32(jb, regs->fs);
    jb_append_text(jb, ",\"gs\":"); jb_append_hex32(jb, regs->gs);
    jb_append_char(jb, '}');
}

static void build_backtrace_array(JsonBuilder* jb) {
    BacktraceFrame frames[kBacktraceFrameMax];
    int frame_count = collect_backtrace(frames, kBacktraceFrameMax);

    jb_append_text(jb, "\"frames\":[");
    for (int i = 0; i < frame_count; i++) {
        if (i != 0) {
            jb_append_char(jb, ',');
        }
        jb_append_text(jb, "{\"index\":");
        jb_append_uint(jb, (uint32_t)i);
        jb_append_text(jb, ",\"pc\":");
        jb_append_hex32(jb, frames[i].eip);
        jb_append_text(jb, ",\"framePointer\":");
        jb_append_hex32(jb, frames[i].ebp);
        jb_append_char(jb, '}');
    }
    jb_append_char(jb, ']');
}

static void build_current_process_object(JsonBuilder* jb) {
    Process* current = 0;
    User* user = 0;
    if (!try_get_current_process(&current, &user)) {
        jb_append_text(jb, "{\"status\":\"not-ready\"}");
        return;
    }

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"pid\":"); jb_append_int(jb, current->p_pid);
    jb_append_text(jb, ",\"ppid\":"); jb_append_int(jb, current->p_ppid);
    jb_append_text(jb, ",\"state\":"); jb_append_escaped(jb, process_state_name(current->p_stat));
    jb_append_text(jb, ",\"priority\":"); jb_append_int(jb, current->p_pri);
    jb_append_text(jb, ",\"nice\":"); jb_append_int(jb, current->p_nice);
    jb_append_text(jb, ",\"cpu\":"); jb_append_int(jb, current->p_cpu);
    jb_append_text(jb, ",\"size\":"); jb_append_uint(jb, current->p_size);
    jb_append_text(jb, ",\"addr\":"); jb_append_hex32(jb, current->p_addr);
    jb_append_text(jb, ",\"waitChannel\":"); jb_append_hex32(jb, current->p_wchan);
    jb_append_text(jb, ",\"signal\":"); jb_append_int(jb, current->p_sig);
    jb_append_text(jb, ",\"uid\":"); jb_append_int(jb, current->p_uid);
    jb_append_text(jb, ",\"cwd\":"); jb_append_escaped(jb, user->u_curdir);
    jb_append_text(jb, ",\"flags\":");
    append_process_flags(jb, current->p_flag);
    jb_append_text(jb, ",\"memory\":");
    append_memory_descriptor(jb, user->u_MemoryDescriptor);
    jb_append_char(jb, '}');
}

static void build_processes_object(JsonBuilder* jb) {
    if (!process_manager_ready()) {
        jb_append_text(jb, "{\"status\":\"not-ready\",\"items\":[]}");
        return;
    }

    Process* current = 0;
    try_get_current_process(&current, 0);

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"items\":[");
    int first = 1;
    for (int i = 0; i < ProcessManager::NPROC; i++) {
        Process& proc = g_ProcessManager.process[i];
        if (proc.p_stat == Process::SNULL) {
            continue;
        }
        if (!first) {
            jb_append_char(jb, ',');
        }
        first = 0;

        jb_append_char(jb, '{');
        jb_append_text(jb, "\"slot\":"); jb_append_uint(jb, (uint32_t)i);
        jb_append_text(jb, ",\"pid\":"); jb_append_int(jb, proc.p_pid);
        jb_append_text(jb, ",\"ppid\":"); jb_append_int(jb, proc.p_ppid);
        jb_append_text(jb, ",\"state\":"); jb_append_escaped(jb, process_state_name(proc.p_stat));
        jb_append_text(jb, ",\"priority\":"); jb_append_int(jb, proc.p_pri);
        jb_append_text(jb, ",\"nice\":"); jb_append_int(jb, proc.p_nice);
        jb_append_text(jb, ",\"cpu\":"); jb_append_int(jb, proc.p_cpu);
        jb_append_text(jb, ",\"residentTicks\":"); jb_append_int(jb, proc.p_time);
        jb_append_text(jb, ",\"size\":"); jb_append_uint(jb, proc.p_size);
        jb_append_text(jb, ",\"addr\":"); jb_append_hex32(jb, proc.p_addr);
        jb_append_text(jb, ",\"waitChannel\":"); jb_append_hex32(jb, proc.p_wchan);
        jb_append_text(jb, ",\"signal\":"); jb_append_int(jb, proc.p_sig);
        jb_append_text(jb, ",\"uid\":"); jb_append_int(jb, proc.p_uid);
        jb_append_text(jb, ",\"current\":"); jb_append_bool(jb, current == &proc);
        jb_append_text(jb, ",\"flags\":");
        append_process_flags(jb, proc.p_flag);
        jb_append_char(jb, '}');
    }
    jb_append_text(jb, "]}");
}

static void append_superblock(JsonBuilder* jb, const SuperBlock& sb) {
    jb_append_char(jb, '{');
    jb_append_text(jb, "\"isize\":"); jb_append_int(jb, sb.s_isize);
    jb_append_text(jb, ",\"fsize\":"); jb_append_int(jb, sb.s_fsize);
    jb_append_text(jb, ",\"nfree\":"); jb_append_int(jb, sb.s_nfree);
    jb_append_text(jb, ",\"ninode\":"); jb_append_int(jb, sb.s_ninode);
    jb_append_text(jb, ",\"flock\":"); jb_append_bool(jb, sb.s_flock != 0);
    jb_append_text(jb, ",\"ilock\":"); jb_append_bool(jb, sb.s_ilock != 0);
    jb_append_text(jb, ",\"fmod\":"); jb_append_bool(jb, sb.s_fmod != 0);
    jb_append_text(jb, ",\"ronly\":"); jb_append_bool(jb, sb.s_ronly != 0);
    jb_append_text(jb, ",\"time\":"); jb_append_int(jb, sb.s_time);
    jb_append_text(jb, ",\"freePreview\":[");
    int free_preview = sb.s_nfree < 8 ? sb.s_nfree : 8;
    for (int i = 0; i < free_preview; i++) {
        if (i != 0) jb_append_char(jb, ',');
        jb_append_int(jb, sb.s_free[i]);
    }
    jb_append_text(jb, "],\"inodePreview\":[");
    int inode_preview = sb.s_ninode < 8 ? sb.s_ninode : 8;
    for (int i = 0; i < inode_preview; i++) {
        if (i != 0) jb_append_char(jb, ',');
        jb_append_int(jb, sb.s_inode[i]);
    }
    jb_append_text(jb, "]}");
}

static void build_trace_array(JsonBuilder* jb) {
    TraceCapture capture;
    capture.count = 0;
    for (int i = 0; i < kTraceLineCountMax; i++) {
        capture.lines[i][0] = '\0';
    }

    fs_trace_dump(capture_trace_writer, &capture);

    jb_append_char(jb, '[');
    int first = 1;
    for (int i = 0; i < capture.count; i++) {
        if (capture.lines[i][0] == '\0') {
            continue;
        }
        if (!first) {
            jb_append_char(jb, ',');
        }
        first = 0;
        jb_append_escaped(jb, capture.lines[i]);
    }
    jb_append_char(jb, ']');
}

static void build_filesystem_object(JsonBuilder* jb) {
    if (!filesystem_ready()) {
        jb_append_text(jb, "{\"status\":\"not-ready\"}");
        return;
    }

    SuperBlock* sb = g_FileSystem.m_Mount[0].m_spb;
    jb_append_char(jb, '{');
    jb_append_text(jb, "\"superblock\":");
    append_superblock(jb, *sb);
    jb_append_text(jb, ",\"mounts\":[");
    int first = 1;
    for (int i = 0; i < FileSystem::NMOUNT; i++) {
        Mount& mount = g_FileSystem.m_Mount[i];
        if (mount.m_spb == 0) {
            continue;
        }
        if (!first) {
            jb_append_char(jb, ',');
        }
        first = 0;

        jb_append_char(jb, '{');
        jb_append_text(jb, "\"slot\":"); jb_append_uint(jb, (uint32_t)i);
        jb_append_text(jb, ",\"dev\":"); jb_append_int(jb, mount.m_dev);
        jb_append_text(jb, ",\"inode\":");
        if (mount.m_inodep != 0) {
            jb_append_int(jb, mount.m_inodep->i_number);
        } else {
            jb_append_int(jb, -1);
        }
        jb_append_char(jb, '}');
    }
    jb_append_text(jb, "],\"trace\":");
    build_trace_array(jb);
    jb_append_char(jb, '}');
}

static void build_directory_object(JsonBuilder* jb, const char* annex) {
    const char* raw_path = annex + 9;
    DiskPathLookupResult result;
    int ok = lookup_disk_path(raw_path, &result);

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"path\":");
    jb_append_escaped(jb, ok ? result.path : "/");
    jb_append_text(jb, ",\"ok\":");
    jb_append_bool(jb, ok);

    if (!ok) {
        jb_append_text(jb, ",\"message\":\"path-not-found\",\"entries\":[]}");
        return;
    }

    jb_append_text(jb, ",\"inode\":");
    jb_append_int(jb, result.inodeNo);
    jb_append_text(jb, ",\"mode\":");
    jb_append_hex32(jb, result.inode.d_mode);
    jb_append_text(jb, ",\"entries\":[");

    const int entry_size = (int)sizeof(DirectoryEntry);
    const int total_entries = result.inode.d_size / entry_size;
    int first = 1;

    for (int entry_index = 0; entry_index < total_entries; entry_index++) {
        int offset = entry_index * entry_size;
        int lbn = offset / Inode::BLOCK_SIZE;
        int block_offset = offset % Inode::BLOCK_SIZE;
        int sector = map_disk_inode_block(result.inode, lbn);
        unsigned char block[Inode::BLOCK_SIZE];
        if (sector == 0 || !read_raw_sector(sector, block)) {
            continue;
        }

        DirectoryEntry* entry = (DirectoryEntry*)(block + block_offset);
        if (entry->m_ino == 0) {
            continue;
        }

        DiskInode child_inode;
        if (!read_disk_inode(entry->m_ino, &child_inode)) {
            continue;
        }

        char name[DirectoryEntry::DIRSIZ + 1];
        char full_path[kPathMax + DirectoryEntry::DIRSIZ + 2];
        copy_directory_entry_name(entry->m_name, name, sizeof(name));

        if (result.path[0] == '/' && result.path[1] == '\0') {
            safe_str_copy(full_path, sizeof(full_path), "/");
            safe_str_copy(full_path + 1, sizeof(full_path) - 1, name);
        } else {
            safe_str_copy(full_path, sizeof(full_path), result.path);
            int end = 0;
            while (full_path[end] != '\0' && end < (int)sizeof(full_path) - 1) {
                end++;
            }
            if (end < (int)sizeof(full_path) - 1) {
                full_path[end++] = '/';
                full_path[end] = '\0';
            }
            safe_str_copy(full_path + end, sizeof(full_path) - end, name);
        }

        if (!first) {
            jb_append_char(jb, ',');
        }
        first = 0;

        jb_append_char(jb, '{');
        jb_append_text(jb, "\"name\":");
        jb_append_escaped(jb, name);
        jb_append_text(jb, ",\"path\":");
        jb_append_escaped(jb, full_path);
        jb_append_text(jb, ",\"inode\":");
        jb_append_int(jb, entry->m_ino);
        jb_append_text(jb, ",\"mode\":");
        jb_append_hex32(jb, child_inode.d_mode);
        jb_append_text(jb, ",\"size\":");
        jb_append_int(jb, child_inode.d_size);
        jb_append_text(jb, ",\"isDirectory\":");
        jb_append_bool(jb, (child_inode.d_mode & Inode::IFDIR) != 0);
        jb_append_char(jb, '}');
    }

    jb_append_text(jb, "]}");
}

static void build_memory_object(JsonBuilder* jb, const char* annex) {
    const char* payload = annex + 7;
    char addr_buf[16];
    char len_buf[16];
    int addr_pos = 0;
    int len_pos = 0;

    while (*payload != '\0' && *payload != '/' && addr_pos < 15) {
        addr_buf[addr_pos++] = *payload++;
    }
    addr_buf[addr_pos] = '\0';
    if (*payload == '/') {
        payload++;
    }
    while (*payload != '\0' && len_pos < 15) {
        len_buf[len_pos++] = *payload++;
    }
    len_buf[len_pos] = '\0';

    uint32_t addr = parse_u32(addr_buf, 16);
    uint32_t len = parse_u32(len_buf, 16);
    if (len == 0) {
        len = 64;
    }
    if (len > 256) {
        len = 256;
    }

    char data[256];
    int ok = gdb_read_memory(addr, data, (int)len) >= 0;

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"address\":"); jb_append_hex32(jb, addr);
    jb_append_text(jb, ",\"length\":"); jb_append_uint(jb, len);
    jb_append_text(jb, ",\"ok\":"); jb_append_bool(jb, ok);
    jb_append_text(jb, ",\"encoding\":\"hex\",\"data\":\"");
    if (ok) {
        const char* hex = "0123456789abcdef";
        for (uint32_t i = 0; i < len; i++) {
            unsigned char byte = (unsigned char)data[i];
            jb_append_char(jb, hex[(byte >> 4) & 0x0F]);
            jb_append_char(jb, hex[byte & 0x0F]);
        }
    }
    jb_append_text(jb, "\"}");
}

static void build_inode_object(JsonBuilder* jb, const char* annex) {
    const char* payload = annex + 6;
    int inode_no = (int)parse_u32(payload, 10);
    if (inode_no == 0) {
        inode_no = (int)parse_u32(payload, 16);
    }

    DiskInode inode;
    int ok = read_disk_inode(inode_no, &inode);

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"inode\":"); jb_append_int(jb, inode_no);
    jb_append_text(jb, ",\"ok\":"); jb_append_bool(jb, ok);
    if (ok) {
        jb_append_text(jb, ",\"mode\":"); jb_append_hex32(jb, inode.d_mode);
        jb_append_text(jb, ",\"nlink\":"); jb_append_int(jb, inode.d_nlink);
        jb_append_text(jb, ",\"uid\":"); jb_append_int(jb, inode.d_uid);
        jb_append_text(jb, ",\"gid\":"); jb_append_int(jb, inode.d_gid);
        jb_append_text(jb, ",\"size\":"); jb_append_int(jb, inode.d_size);
        jb_append_text(jb, ",\"atime\":"); jb_append_int(jb, inode.d_atime);
        jb_append_text(jb, ",\"mtime\":"); jb_append_int(jb, inode.d_mtime);
        jb_append_text(jb, ",\"addr\":[");
        for (int i = 0; i < 10; i++) {
            if (i != 0) jb_append_char(jb, ',');
            jb_append_int(jb, inode.d_addr[i]);
        }
        jb_append_char(jb, ']');
    }
    jb_append_char(jb, '}');
}

static void build_block_object(JsonBuilder* jb, const char* annex) {
    const char* payload = annex + 6;
    int block_no = (int)parse_u32(payload, 10);
    if (block_no == 0) {
        block_no = (int)parse_u32(payload, 16);
    }

    unsigned char block[Inode::BLOCK_SIZE];
    int ok = read_raw_sector(block_no, block);

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"block\":"); jb_append_int(jb, block_no);
    jb_append_text(jb, ",\"ok\":"); jb_append_bool(jb, ok);
    jb_append_text(jb, ",\"encoding\":\"hex\",\"data\":\"");
    if (ok) {
        const char* hex = "0123456789abcdef";
        for (int i = 0; i < Inode::BLOCK_SIZE; i++) {
            unsigned char byte = block[i];
            jb_append_char(jb, hex[(byte >> 4) & 0x0F]);
            jb_append_char(jb, hex[byte & 0x0F]);
        }
    }
    jb_append_text(jb, "\"}");
}

static void build_file_object(JsonBuilder* jb, const char* annex) {
    const char* raw_path = annex + 4;
    DiskPathLookupResult result;
    int ok = lookup_disk_path(raw_path, &result);

    jb_append_char(jb, '{');
    jb_append_text(jb, "\"path\":");
    jb_append_escaped(jb, ok ? result.path : "/");
    jb_append_text(jb, ",\"ok\":");
    jb_append_bool(jb, ok);

    if (!ok) {
        jb_append_text(jb, ",\"message\":\"path-not-found\"}");
        return;
    }

    jb_append_text(jb, ",\"inode\":");
    jb_append_int(jb, result.inodeNo);
    jb_append_text(jb, ",\"mode\":");
    jb_append_hex32(jb, result.inode.d_mode);
    jb_append_text(jb, ",\"size\":");
    jb_append_int(jb, result.inode.d_size);
    jb_append_text(jb, ",\"isDirectory\":");
    jb_append_bool(jb, (result.inode.d_mode & Inode::IFDIR) != 0);

    if ((result.inode.d_mode & Inode::IFDIR) != 0) {
        jb_append_text(jb, ",\"message\":\"is-directory\"}");
        return;
    }

    unsigned char data[kFilePreviewMax];
    int preview_len = 0;
    int read_ok = read_inode_preview_bytes(result.inode, data, sizeof(data), &preview_len);
    jb_append_text(jb, ",\"previewLength\":");
    jb_append_int(jb, preview_len);
    jb_append_text(jb, ",\"truncated\":");
    jb_append_bool(jb, result.inode.d_size > preview_len);
    jb_append_text(jb, ",\"encoding\":\"hex\",\"data\":\"");

    if (read_ok) {
        const char* hex = "0123456789abcdef";
        for (int i = 0; i < preview_len; i++) {
            unsigned char byte = data[i];
            jb_append_char(jb, hex[(byte >> 4) & 0x0F]);
            jb_append_char(jb, hex[byte & 0x0F]);
        }
    }

    jb_append_text(jb, "\",\"asciiPreview\":");
    if (read_ok) {
        append_ascii_preview(jb, data, preview_len);
    } else {
        jb_append_escaped(jb, "");
    }
    jb_append_text(jb, ",\"readOk\":");
    jb_append_bool(jb, read_ok);
    jb_append_char(jb, '}');
}

static int build_document(const char* annex, char* buffer, int buffer_size) {
    const char* kind = (annex != 0 && annex[0] != '\0') ? annex : "snapshot";
    JsonBuilder jb;
    jb_init(&jb, buffer, buffer_size);

    jb_append_char(&jb, '{');
    jb_append_text(&jb, "\"schemaVersion\":1,\"kind\":");
    jb_append_escaped(&jb, kind);

    if (str_eq(kind, "registers")) {
        jb_append_text(&jb, ",\"data\":");
        build_registers_object(&jb);
    } else if (str_eq(kind, "backtrace")) {
        jb_append_text(&jb, ",\"data\":{");
        build_backtrace_array(&jb);
        jb_append_text(&jb, "}");
    } else if (str_eq(kind, "current-process")) {
        jb_append_text(&jb, ",\"data\":");
        build_current_process_object(&jb);
    } else if (str_eq(kind, "processes")) {
        jb_append_text(&jb, ",\"data\":");
        build_processes_object(&jb);
    } else if (str_eq(kind, "filesystem")) {
        jb_append_text(&jb, ",\"data\":");
        build_filesystem_object(&jb);
    } else if (str_eq(kind, "fs-trace")) {
        jb_append_text(&jb, ",\"data\":{\"trace\":");
        build_trace_array(&jb);
        jb_append_text(&jb, "}");
    } else if (str_starts_with(kind, "memory/")) {
        jb_append_text(&jb, ",\"data\":");
        build_memory_object(&jb, kind);
    } else if (str_starts_with(kind, "inode/")) {
        jb_append_text(&jb, ",\"data\":");
        build_inode_object(&jb, kind);
    } else if (str_starts_with(kind, "block/")) {
        jb_append_text(&jb, ",\"data\":");
        build_block_object(&jb, kind);
    } else if (str_eq(kind, "directory") || str_starts_with(kind, "directory/")) {
        jb_append_text(&jb, ",\"data\":");
        build_directory_object(&jb, kind);
    } else if (str_starts_with(kind, "file/")) {
        jb_append_text(&jb, ",\"data\":");
        build_file_object(&jb, kind);
    } else {
        jb_append_text(&jb, ",\"data\":{");
        jb_append_text(&jb, "\"registers\":");
        build_registers_object(&jb);
        jb_append_text(&jb, ",\"backtrace\":{");
        build_backtrace_array(&jb);
        jb_append_text(&jb, "},\"currentProcess\":");
        build_current_process_object(&jb);
        jb_append_text(&jb, ",\"processes\":");
        build_processes_object(&jb);
        jb_append_text(&jb, ",\"filesystem\":");
        build_filesystem_object(&jb);
        jb_append_text(&jb, "}");
    }

    jb_append_char(&jb, '}');
    jb_terminate(&jb);
    return jb.overflow ? -1 : jb.pos;
}

static void emit_backtrace_text(DebugJsonWriter writer, void* context) {
    BacktraceFrame frames[kBacktraceFrameMax];
    int frame_count = collect_backtrace(frames, kBacktraceFrameMax);
    char line[96];

    emit_text(writer, context, "=== Kernel Backtrace ===\n");
    for (int i = 0; i < frame_count; i++) {
        JsonBuilder jb;
        jb_init(&jb, line, sizeof(line));
        jb_append_char(&jb, '#');
        jb_append_uint(&jb, (uint32_t)i);
        jb_append_text(&jb, " pc=");
        jb_append_hex32(&jb, frames[i].eip);
        jb_append_text(&jb, " fp=");
        jb_append_hex32(&jb, frames[i].ebp);
        jb_append_char(&jb, '\n');
        jb_terminate(&jb);
        emit_text(writer, context, line);
    }
    emit_text(writer, context, "=== End Kernel Backtrace ===\n");
}

} // namespace

int debug_json_handle_qxfer(const char* object,
                            const char* annex,
                            uint32_t offset,
                            uint32_t length,
                            char* response,
                            int response_size) {
    if (object == 0 || annex == 0 || response == 0 || response_size < 2) {
        return 0;
    }
    if (!str_eq(object, "v6pp-json")) {
        return 0;
    }

    static char document[kJsonDocumentMax];
    int doc_len = build_document(annex, document, sizeof(document));
    if (doc_len < 0) {
        const char* error_json = "{\"schemaVersion\":1,\"kind\":\"error\",\"data\":{\"message\":\"json-buffer-overflow\"}}";
        safe_str_copy(document, sizeof(document), error_json);
        doc_len = 0;
        while (document[doc_len] != '\0') {
            doc_len++;
        }
    }

    if ((int)offset >= doc_len) {
        response[0] = 'l';
        response[1] = '\0';
        return 1;
    }

    int available = doc_len - (int)offset;
    int max_copy = response_size - 2;
    int copy_len = (int)length;
    if (copy_len > available) {
        copy_len = available;
    }
    if (copy_len > max_copy) {
        copy_len = max_copy;
    }
    if (copy_len < 0) {
        copy_len = 0;
    }

    response[0] = ((int)offset + copy_len) < doc_len ? 'm' : 'l';
    for (int i = 0; i < copy_len; i++) {
        response[1 + i] = document[offset + i];
    }
    response[1 + copy_len] = '\0';
    return copy_len + 1;
}

int debug_json_is_monitor_command(const char* cmd) {
    if (cmd == 0) {
        return 0;
    }
    if (str_eq(cmd, "bt")) return 1;
    if (str_eq(cmd, "json")) return 1;
    if (str_eq(cmd, "v6json")) return 1;
    if (str_eq(cmd, "v6json help")) return 1;
    if (str_starts_with(cmd, "json ")) return 1;
    if (str_starts_with(cmd, "v6json ")) return 1;
    return 0;
}

void debug_json_handle_monitor_command(const char* cmd,
                                       DebugJsonWriter writer,
                                       void* context) {
    if (cmd == 0) {
        return;
    }

    if (str_eq(cmd, "bt")) {
        emit_backtrace_text(writer, context);
        return;
    }

    if (str_eq(cmd, "json") || str_eq(cmd, "v6json") || str_eq(cmd, "v6json help")) {
        emit_text(writer, context, "v6json <kind>\n");
        emit_text(writer, context, "  kinds: snapshot, registers, backtrace, current-process, processes,\n");
        emit_text(writer, context, "         filesystem, fs-trace, directory[/path], file/<path>,\n");
        emit_text(writer, context, "         memory/<addr-hex>/<len-hex>, inode/<n>, block/<n>\n");
        emit_text(writer, context, "  raw packet form: qXfer:v6pp-json:read:<kind>:<offset>,<length>\n");
        return;
    }

    const char* kind = cmd;
    if (str_starts_with(cmd, "json ")) {
        kind = cmd + 5;
    } else if (str_starts_with(cmd, "v6json ")) {
        kind = cmd + 7;
    }

    char document[kJsonDocumentMax];
    if (build_document(kind, document, sizeof(document)) < 0) {
        emit_text(writer, context, "{\"schemaVersion\":1,\"kind\":\"error\",\"data\":{\"message\":\"json-buffer-overflow\"}}\n");
        return;
    }
    emit_text(writer, context, document);
    emit_text(writer, context, "\n");
}