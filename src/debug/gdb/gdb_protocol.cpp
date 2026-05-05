#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_memory.h"
#include "gdb_breakpoints.h"
#include "../../include/Video.h"
#include "../debug.h"
#include "../fs/fs_debugger_integration.h"
#include "../json/debug_json.h"

extern "C" DebuggerState g_debugger;

static int strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

static int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char* gdb_strchr(const char* str, char c) {
    while (*str != '\0') {
        if (*str == c) return (char*)str;
        str++;
    }
    return 0;
}

int gdb_strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

uint8_t hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

uint32_t hex_str_to_uint(const char* hex_str) {
    uint32_t result = 0;
    while (*hex_str != '\0') {
        result = (result << 4) | hex_char_to_value(*hex_str);
        hex_str++;
    }
    return result;
}

uint32_t swap_endian_32(uint32_t value) {
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

uint32_t gdb_hex_to_host32(const char* hex_str) {
    uint32_t result = 0;
    
    for (int i = 0; i < 8; i += 2) {
        uint8_t byte = (hex_char_to_value(hex_str[i]) << 4) | 
                      hex_char_to_value(hex_str[i+1]);
        result = (result << 8) | byte;
    }
    
    return swap_endian_32(result);
}

void host32_to_gdb_hex(uint32_t value, char* output) {
    uint32_t be_value = ((value & 0xFF) << 24) |
                       ((value & 0xFF00) << 8) |
                       ((value & 0xFF0000) >> 8) |
                       ((value & 0xFF000000) >> 24);
    
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        output[i] = hex_chars[(be_value >> shift) & 0xF];
    }
    output[8] = '\0';
}

static Socket g_client_socket = (Socket)-1;
static int g_current_packet_length = 0;
static int g_range_step_active = 0;
static uint32_t g_range_step_start = 0;
static uint32_t g_range_step_end = 0;
static int g_range_step_breakpoint_active = 0;
static uint32_t g_range_step_breakpoint_addr = 0;
static uint8_t g_range_step_breakpoint_orig_byte = 0;
// static GDBRegisters g_reg_context;


void gdb_set_client_socket(Socket sock) {
    g_client_socket = sock;
}

Socket gdb_get_client_socket(void) {
    return g_client_socket;
}

void gdb_set_current_packet_length(int len) {
    g_current_packet_length = len;
}

int gdb_get_current_packet_length(void) {
    return g_current_packet_length;
}

void gdb_clear_range_step(void) {
    if (g_range_step_breakpoint_active) {
        uint8_t* mem_ptr = (uint8_t*)g_range_step_breakpoint_addr;
        *mem_ptr = g_range_step_breakpoint_orig_byte;
    }

    g_range_step_active = 0;
    g_range_step_start = 0;
    g_range_step_end = 0;
    g_range_step_breakpoint_active = 0;
    g_range_step_breakpoint_addr = 0;
    g_range_step_breakpoint_orig_byte = 0;
}

int gdb_has_range_step(void) {
    return g_range_step_active;
}

int gdb_range_step_should_stop(uint32_t pc) {
    if (!g_range_step_active) {
        return 1;
    }

    if (pc < g_range_step_start || pc >= g_range_step_end) {
        gdb_clear_range_step();
        return 1;
    }

    return 0;
}

static int gdb_is_call_instruction(uint32_t pc) {
    const uint8_t* insn = (const uint8_t*)pc;
    if (!insn) {
        return 0;
    }

    if (insn[0] == 0xE8 || insn[0] == 0x9A) {
        return 1;
    }

    if (insn[0] == 0xFF) {
        uint8_t modrm = insn[1];
        uint8_t reg = (modrm >> 3) & 0x7;
        if (reg == 2 || reg == 3) {
            return 1;
        }
    }

    return 0;
}

static int gdb_arm_range_step_breakpoint(uint32_t addr) {
    if (gdb_find_breakpoint(addr) != 0) {
        return 1;
    }

    if (g_range_step_breakpoint_active && g_range_step_breakpoint_addr == addr) {
        return 1;
    }

    uint8_t* mem_ptr = (uint8_t*)addr;
    g_range_step_breakpoint_addr = addr;
    g_range_step_breakpoint_orig_byte = *mem_ptr;
    *mem_ptr = 0xCC;
    g_range_step_breakpoint_active = 1;
    return 1;
}

int gdb_activate_range_step(uint32_t current_pc, uint32_t start, uint32_t end) {
    gdb_clear_range_step();

    g_range_step_active = 1;
    g_range_step_start = start;
    g_range_step_end = end;
    if (current_pc == start && gdb_is_call_instruction(current_pc)) {
        return gdb_arm_range_step_breakpoint(end);
    }
    return 0;
}

int gdb_consume_range_step_breakpoint(uint32_t addr) {
    if (!g_range_step_breakpoint_active || addr != g_range_step_breakpoint_addr) {
        return 0;
    }

    uint8_t* mem_ptr = (uint8_t*)g_range_step_breakpoint_addr;
    *mem_ptr = g_range_step_breakpoint_orig_byte;

    g_range_step_breakpoint_active = 0;
    g_range_step_breakpoint_addr = 0;
    g_range_step_breakpoint_orig_byte = 0;
    return 1;
}

int gdb_prepare_range_step_resume(uint32_t current_pc) {
    if (!g_range_step_active) {
        return 0;
    }

    if (!gdb_is_call_instruction(current_pc)) {
        return 0;
    }

    return gdb_arm_range_step_breakpoint(g_range_step_end);
}

int gdb_recv_packet(char* buffer, int buffer_size) {
    if (g_client_socket == (Socket)-1) return -1;

    int state = 0;
    int packet_len = 0;
    unsigned char recv_checksum = 0;
    unsigned char calc_checksum = 0;
    char recv_byte;

    while (1) {
        int recv_len = gdb_socket_recv(g_client_socket, &recv_byte, 1);
        if (recv_len <= 0) {
            return -1;
        }

        char c = recv_byte;

        switch (state) {
            case 0:
                if (c == '$') {
                    packet_len = 0;
                    calc_checksum = 0;
                    recv_checksum = 0;
                    state = 1;
                } else if (c == '+') {
                    continue;
                } else if (c == '-') {
                    return -2;
                }
                break;

            case 1:
                if (c == '#') {
                    state = 2;
                } else if (packet_len < buffer_size - 1) {
                    buffer[packet_len++] = c;
                    calc_checksum += (unsigned char)c;
                } else {
                    state = 0;
                }
                break;

            case 2:
                if (c >= '0' && c <= '9') {
                    recv_checksum = (c - '0') << 4;
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum = (c - 'a' + 10) << 4;
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum = (c - 'A' + 10) << 4;
                } else {
                    state = 0;
                    break;
                }
                state = 3;
                break;

            case 3:
                if (c >= '0' && c <= '9') {
                    recv_checksum |= (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum |= (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum |= (c - 'A' + 10);
                } else {
                    state = 0;
                    break;
                }

                if (recv_checksum == calc_checksum) {
                    buffer[packet_len] = '\0';
                    char ack = '+';
                    gdb_socket_send(g_client_socket, &ack, 1);
                    return packet_len;
                } else {
                    char nack = '-';
                    gdb_socket_send(g_client_socket, &nack, 1);
                    state = 0;
                }
                break;
        }
    }
}

void gdb_send_packet(char* data) {
    if (g_client_socket == (Socket)-1) return;

    char buffer[DEBUG_BUFFER_SIZE];
    int len = strlen(data);

    unsigned char checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += (unsigned char)data[i];
    }

    int pos = 0;
    buffer[pos++] = '$';
    strcpy(buffer + pos, data);
    pos += len;
    buffer[pos++] = '#';

    char hex[3];
    hex[0] = "0123456789abcdef"[(checksum >> 4) & 0x0F];
    hex[1] = "0123456789abcdef"[checksum & 0x0F];
    buffer[pos++] = hex[0];
    buffer[pos++] = hex[1];
    buffer[pos] = '\0';

    gdb_socket_send(g_client_socket, buffer, pos);
}

static void gdb_send_console_output(const char* text) {
    if (g_client_socket == (Socket)-1 || text == nullptr) return;

    const char* hex = "0123456789abcdef";
    const int max_chunk_bytes = 900;
    char packet[(max_chunk_bytes * 2) + 2];
    int text_pos = 0;

    while (text[text_pos] != '\0') {
        int packet_pos = 0;
        packet[packet_pos++] = 'O';

        int bytes_encoded = 0;
        while (text[text_pos] != '\0' && bytes_encoded < max_chunk_bytes) {
            unsigned char ch = (unsigned char)text[text_pos++];
            packet[packet_pos++] = hex[(ch >> 4) & 0x0F];
            packet[packet_pos++] = hex[ch & 0x0F];
            bytes_encoded++;
        }

        if (bytes_encoded == 0) {
            break;
        }

        packet[packet_pos] = '\0';
        gdb_send_packet(packet);
    }
}

static void gdb_fs_debugger_output_writer(const char* text, void* context) {
    (void)context;
    gdb_send_console_output(text);
}

static void gdb_json_output_writer(const char* text, void* context) {
    (void)context;
    gdb_send_console_output(text);
}

static int is_fs_debugger_monitor_command(const char* cmd) {
    if (cmd == nullptr) return 0;
    if (strncmp(cmd, "qfs:", 4) == 0) return 1;
    if (strcmp(cmd, "super") == 0) return 1;
    if (strcmp(cmd, "inodes") == 0) return 1;
    if (strncmp(cmd, "block ", 6) == 0) return 1;
    if (strncmp(cmd, "inode ", 6) == 0) return 1;
    if (strncmp(cmd, "ls ", 3) == 0) return 1;
    if (strncmp(cmd, "trace ", 6) == 0) return 1;
    if (strncmp(cmd, "dumpblock ", 10) == 0) return 1;
    if (strncmp(cmd, "showinode ", 10) == 0) return 1;
    if (strcmp(cmd, "txtrace") == 0) return 1;
    if (strcmp(cmd, "fshelp") == 0) return 1;
    return 0;
}

void gdb_send_ok(void) {
    gdb_send_packet((char*)"OK");
}

void gdb_send_error(int code) {
    char buffer[16];
    buffer[0] = 'E';
    const char* hex = "0123456789abcdef";
    buffer[1] = hex[(code >> 4) & 0x0F];
    buffer[2] = hex[code & 0x0F];
    buffer[3] = '\0';
    gdb_send_packet(buffer);
}

GDBCommand gdb_parse_command(char* packet) {
    if (!packet) return GDB_CMD_UNKNOWN;
    if (strcmp(packet, "vCont?") == 0) {
        return GDB_CMD_QUERY;
    }
    
    if (strncmp(packet, "vCont;c", 7) == 0) {
        return GDB_CMD_CONTINUE;
    }
    
    if (strncmp(packet, "vCont;s", 7) == 0) {
        return GDB_CMD_STEP;
    }

    if (strncmp(packet, "vCont;r", 7) == 0) {
        return GDB_CMD_STEP;
    }
    char cmd = packet[0];
    switch (cmd) {
        case 'c': return GDB_CMD_CONTINUE;
        case 's': return GDB_CMD_STEP;
        case 'g': return GDB_CMD_READ_REG;
        case 'G': return GDB_CMD_WRITE_REG;
        case 'P': return GDB_CMD_WRITE_SINGLE_REG;
        case 'D': return GDB_CMD_DETACH;
        case 'm': return GDB_CMD_READ_MEM;
        case 'M': return GDB_CMD_WRITE_MEM;
        case 'X': return GDB_CMD_WRITE_MEM_BINARY; 
        case 'Z': return GDB_CMD_SET_BREAK;
        case 'z': return GDB_CMD_REMOVE_BREAK;
        case 'q': return GDB_CMD_QUERY;
        case 'v': return GDB_CMD_VENDOR;
        case '?': return GDB_CMD_SIGNAL;
        case 'H': return GDB_CMD_THREAD;
        default: return GDB_CMD_UNKNOWN;
    }
}

void gdb_handle_continue(char* packet) {
    if (strcmp(packet, "c") == 0 || strncmp(packet, "vCont;c", 7) == 0) {
        gdb_clear_range_step();
        if (gdb_has_pending_breakpoint_resume()) {
            gdb_set_pending_breakpoint_auto_continue(1);
            gdb_set_single_step();
        } else {
            gdb_clear_single_step();
        }
        gdb_registers_commit_to_trap_frame();
        g_debugger.mode = DEBUG_MODE_CONTINUE;
        g_debugger.resume_requested = 1;
    } else {
        gdb_send_error(0);
    }
}

static int gdb_parse_range_step_packet(char* packet, uint32_t* start, uint32_t* end) {
    if (strncmp(packet, "vCont;r", 7) != 0) {
        return 0;
    }

    char* ptr = packet + 7;
    uint32_t parsed_start = 0;
    uint32_t parsed_end = 0;

    while (*ptr != ',' && *ptr != '\0') {
        char c = *ptr++;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            parsed_start = (parsed_start << 4) | hex_char_to_value(c);
        } else {
            return 0;
        }
    }

    if (*ptr != ',') {
        return 0;
    }
    ptr++;

    while (*ptr != ':' && *ptr != ';' && *ptr != '\0') {
        char c = *ptr++;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            parsed_end = (parsed_end << 4) | hex_char_to_value(c);
        } else {
            return 0;
        }
    }

    if (parsed_end <= parsed_start) {
        return 0;
    }

    *start = parsed_start;
    *end = parsed_end;
    return 1;
}

void gdb_handle_step(char* packet) {
    uint32_t range_start = 0;
    uint32_t range_end = 0;

    if (strcmp(packet, "s") == 0 || strncmp(packet, "vCont;s", 7) == 0) {
        gdb_clear_range_step();
        gdb_set_pending_breakpoint_auto_continue(0);
        gdb_set_single_step();
        gdb_registers_commit_to_trap_frame();
        g_debugger.mode = DEBUG_MODE_STEP;
        g_debugger.resume_requested = 1;
    } else if (gdb_parse_range_step_packet(packet, &range_start, &range_end)) {
        int use_continue = gdb_activate_range_step(gdb_get_register_value(GDB_REG_EIP),
                                                   range_start,
                                                   range_end);
        gdb_set_pending_breakpoint_auto_continue(0);
        if (use_continue) {
            gdb_clear_single_step();
        } else {
            gdb_set_single_step();
        }
        gdb_registers_commit_to_trap_frame();
        g_debugger.mode = DEBUG_MODE_STEP;
        g_debugger.resume_requested = 1;
    } else {
        gdb_send_error(0);
    }
}

void gdb_handle_thread_command(char* packet) {
    if (strlen(packet) < 3) {
        gdb_send_packet("E01");
        return;
    }
    
    char op_type = packet[1];
    char* thread_str = packet + 2;
    
    if (strcmp(thread_str, "0") == 0 ||
        strcmp(thread_str, "1") == 0 ||
        strcmp(thread_str, "-1") == 0 ||
        strcmp(thread_str, "p0.0") == 0 ||
        strcmp(thread_str, "p1.1") == 0) {
        gdb_send_packet("OK");
    } else {
        gdb_send_packet("E01");
    }
}

// void gdb_handle_read_registers(char* p) {

//     if (p == NULL || p[0] == '\0') {
        
//         char default_regs[129];
//         for (int i = 0; i < 128; i++) {
//             default_regs[i] = '0';
//         }
//         default_regs[128] = '\0';
        
//         gdb_send_packet(default_regs);
//         return;
//     }

//     // Diagnose::Write("[DEBUG] Reading registers with BIG-ENDIAN format\n");
    
//     uint32_t regs[16] = {0};
    
//     __asm__ __volatile__ (
//         "movl %%eax, %0\n\t"
//         "movl %%ecx, %1\n\t" 
//         "movl %%edx, %2\n\t"
//         "movl %%ebx, %3\n\t"
//         "movl %%esp, %4\n\t"
//         "movl %%ebp, %5\n\t"
//         "movl %%esi, %6\n\t"
//         "movl %%edi, %7\n\t"
//         : "=m"(regs[0]), "=m"(regs[1]), "=m"(regs[2]), "=m"(regs[3]),
//           "=m"(regs[4]), "=m"(regs[5]), "=m"(regs[6]), "=m"(regs[7])
//         :
//         : "memory"
//     );
    
//     // EIP
//     __asm__ __volatile__ (
//         "call 1f\n\t"
//         "1: popl %0\n\t"
//         : "=r"(regs[8])
//     );
    
//     // EFLAGS
//     __asm__ __volatile__ (
//         "pushfl\n\t"
//         "popl %0\n\t"
//         : "=r"(regs[9])
//     );
    
//     uint16_t temp16;
//     __asm__ __volatile__ ("movw %%cs, %0" : "=r"(temp16));
//     regs[10] = temp16;
//     __asm__ __volatile__ ("movw %%ss, %0" : "=r"(temp16));
//     regs[11] = temp16;
//     __asm__ __volatile__ ("movw %%ds, %0" : "=r"(temp16));
//     regs[12] = temp16;
//     __asm__ __volatile__ ("movw %%es, %0" : "=r"(temp16));
//     regs[13] = temp16;
//     __asm__ __volatile__ ("movw %%fs, %0" : "=r"(temp16));
//     regs[14] = temp16;
//     __asm__ __volatile__ ("movw %%gs, %0" : "=r"(temp16));
//     regs[15] = temp16;
    
//     char le_display[9], be_display[9];
    
//     // host32_to_gdb_hex(regs[4], be_display);
    
//     // host32_to_gdb_hex(regs[0], be_display);
    
//     char response[129];
    
//     for (int i = 0; i < 16; i++) {
//         host32_to_gdb_hex(regs[i], response + i * 8);
//     }
//     response[128] = '\0';
    
//     gdb_send_packet(response);
// }

void gdb_handle_read_registers(char* p) {
    gdb_registers_save();

    char reg_str[512];
    gdb_registers_to_string(reg_str, sizeof(reg_str));
    gdb_send_packet(reg_str);
}

void gdb_handle_write_registers(char* p) {
    if (p[0] != 'G') {
        gdb_send_error(0);
        return;
    }

    char* data = p + 1;
    int data_len = strlen(data);
    
    if (data_len != GDB_REG_COUNT * 8) {
        gdb_send_error(0);
        return;
    }
    
    for (int reg_index = 0; reg_index < GDB_REG_COUNT; reg_index++) {
        char reg_hex[9] = {0};
        for (int i = 0; i < 8; i++) {
            reg_hex[i] = data[reg_index * 8 + i];
        }
        reg_hex[8] = '\0';
        
        uint32_t reg_value = hex_str_to_uint(reg_hex);
        
        gdb_set_register(reg_index, reg_value);
    }
    
    gdb_send_ok();
}

void gdb_handle_write_single_register(char* p) {
    Diagnose::Write("[GDB] ½øÈëgdb_handle_write_single_registerº¯Êý\n");
    if (p[0] != 'P') {
        gdb_send_error(0);
        return;
    }
    
    char* data = p + 1;
    
    char* equal_sign = gdb_strchr(data, '=');
    if (!equal_sign) {
        Diagnose::Write("[GDB] ÎÞÐ§µÄPÃüÁî¸ñÊ½: %s\n", p);
        gdb_send_error(0);
        return;
    }
    
    char reg_str[16] = {0};
    int reg_len = equal_sign - data;
    
    for (int i = 0; i < reg_len && i < 15; i++) {
        reg_str[i] = data[i];
    }
    reg_str[reg_len] = '\0';
    
    char* value_str = equal_sign + 1;
    
    int reg_num = 0;
    for (int i = 0; i < reg_len && reg_str[i] >= '0' && reg_str[i] <= '9'; i++) {
        reg_num = reg_num * 10 + (reg_str[i] - '0');
    }
    
    uint32_t reg_value = hex_str_to_uint(value_str);
    
    uint32_t host_value = 
        ((reg_value & 0x000000FF) << 24) |
        ((reg_value & 0x0000FF00) << 8)  |
        ((reg_value & 0x00FF0000) >> 8)  |
        ((reg_value & 0xFF000000) >> 24);
    
    Diagnose::Write("[GDB] PÃüÁî: ¼Ä´æÆ÷%d = 0x%s -> 0x%08x\n", 
                   reg_num, value_str, host_value);
    
    if (reg_num < 0 || reg_num > GDB_REG_COUNT) {
        Diagnose::Write("[GDB] ÎÞÐ§µÄ¼Ä´æÆ÷±àºÅ: %d\n", reg_num);
        gdb_send_error(0);
        return;
    }
    
    gdb_set_register(reg_num, host_value);
    
    gdb_send_ok();
}

// void gdb_handle_read_memory(char* p) {
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     char* ptr = p + 1;

//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     if (*ptr == ',') ptr++;

//     while (*ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             len = (len << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             len = (len << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             len = (len << 4) | (c - 'A' + 10);
//         }
//     }

//     if (len > DEBUG_BUFFER_SIZE / 2) {
//         len = DEBUG_BUFFER_SIZE / 2;
//     }

//     static char mem_buffer[DEBUG_BUFFER_SIZE];
//     if (gdb_read_memory(addr, mem_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     static char hex_buffer[DEBUG_BUFFER_SIZE * 2];
//     const char* hex_chars = "0123456789abcdef";
//     int hex_pos = 0;

//     for (uint32_t i = 0; i < len; i++) {
//         hex_buffer[hex_pos++] = hex_chars[(mem_buffer[i] >> 4) & 0x0F];
//         hex_buffer[hex_pos++] = hex_chars[mem_buffer[i] & 0x0F];
//     }

//     hex_buffer[hex_pos] = '\0';
//     gdb_send_packet(hex_buffer);
// }

int gdb_safe_read_memory(uint32_t addr, char* buffer, uint32_t len) {
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        return -1;
    }

    return gdb_read_memory(addr, buffer, (int)len) < 0 ? -1 : 0;
}

void gdb_handle_read_memory(char* packet) {
    char* comma = gdb_strchr(packet, ',');
    if (!comma) {
        gdb_send_packet("E01");
        return;
    }
    
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    char* len_str = comma + 1;
    uint32_t len = hex_str_to_uint(len_str);
    
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    static char mem_buffer[DEBUG_BUFFER_SIZE];
    if (gdb_safe_read_memory(host_addr, mem_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    static char hex_buffer[DEBUG_BUFFER_SIZE * 2 + 1];
    const char hex_chars[] = "0123456789abcdef";
    
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = (uint8_t)mem_buffer[i];
        hex_buffer[i*2] = hex_chars[(byte >> 4) & 0x0F];
        hex_buffer[i*2 + 1] = hex_chars[byte & 0x0F];
    }
    hex_buffer[len*2] = '\0';
    
    gdb_send_packet(hex_buffer);
}


// void gdb_handle_write_memory(char* p) {
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     char* ptr = p + 1;

//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     if (*ptr == ',') ptr++;

//     while (*ptr != ':' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             len = (len << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             len = (len << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             len = (len << 4) | (c - 'A' + 10);
//         }
//     }

//     if (*ptr == ':') ptr++;

//     static char data_buffer[DEBUG_BUFFER_SIZE];
//     const char* hex_chars = "0123456789abcdef";

//     for (uint32_t i = 0; i < len; i++) {
//         char c1 = ptr[0];
//         char c2 = ptr[1];
//         ptr += 2;

//         uint8_t byte = 0;
//         if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
//         else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
//         else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

//         if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
//         else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
//         else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

//         data_buffer[i] = byte;
//     }

//     if (gdb_write_memory(addr, data_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     gdb_send_ok();
// }

int gdb_safe_write_memory(uint32_t addr, const char* data, uint32_t len) {
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        return -1;
    }

    if (addr + len < addr) {
        return -1;
    }

    return gdb_write_memory(addr, data, (int)len) < 0 ? -1 : 0;
}

// void gdb_handle_write_memory(char* packet) {
    
//     char* comma = gdb_strchr(packet, ',');
//     if (!comma) {
//         gdb_send_packet("E01");
//         return;
//     }
    
//     char* colon = gdb_strchr(comma, ':');
//     if (!colon) {
//         gdb_send_packet("E01");
//         return;
//     }
    
//     int addr_len = comma - (packet + 1);
//     char addr_hex[9] = {0};
//     for (int i = 0; i < addr_len && i < 8; i++) {
//         addr_hex[i] = packet[1 + i];
//     }
//     addr_hex[addr_len] = '\0';
    
//     uint32_t host_addr = gdb_hex_to_host32(addr_hex);
    
//     int len_len = colon - (comma + 1);
//     char len_hex[9] = {0};
//     for (int i = 0; i < len_len && i < 8; i++) {
//         len_hex[i] = comma[1 + i];
//     }
//     len_hex[len_len] = '\0';
    
//     uint32_t len = hex_str_to_uint(len_hex);
    
//     char* data_str = colon + 1;
//     int data_len = gdb_strlen(data_str);
    
//     if (data_len != len * 2) {
//         gdb_send_packet("E03");
//         return;
//     }
    
    
//     if (len == 0 || len > DEBUG_BUFFER_SIZE) {
//         len = DEBUG_BUFFER_SIZE;
//     }
    
//     if (host_addr < 0xC0000000) {
//         gdb_send_packet("E00");
//         return;
//     }
    
//     static char data_buffer[DEBUG_BUFFER_SIZE];
//     for (uint32_t i = 0; i < len; i++) {
//         char c1 = data_str[i*2];
//         char c2 = data_str[i*2 + 1];
        
//         uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
//         data_buffer[i] = (char)byte;
//     }
    
//     // if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
//     //     gdb_send_packet("E02");
//     //     return;
//     // }
    
//     gdb_send_packet("OK");
// }
void gdb_handle_write_memory(char* packet) {
    char* comma = gdb_strchr(packet, ',');
    if (!comma) {
        gdb_send_packet("E01");
        return;
    }
    
    char* colon = gdb_strchr(comma, ':');
    if (!colon) {
        gdb_send_packet("E01");
        return;
    }
    
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    int len_len = colon - (comma + 1);
    char len_hex[9] = {0};
    for (int i = 0; i < len_len && i < 8; i++) {
        len_hex[i] = comma[1 + i];
    }
    len_hex[len_len] = '\0';
    
    uint32_t len = hex_str_to_uint(len_hex);
    
    char* data_str = colon + 1;
    int data_len = gdb_strlen(data_str);
    
    if (data_len != len * 2) {
        gdb_send_packet("E03");
        return;
    }
    
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    static char data_buffer[DEBUG_BUFFER_SIZE];
    for (uint32_t i = 0; i < len; i++) {
        char c1 = data_str[i*2];
        char c2 = data_str[i*2 + 1];
        
        uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
        data_buffer[i] = (char)byte;
    }
    
    if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    gdb_send_packet("OK");
}

void gdb_handle_binary_write_memory(char* packet) {
    char* comma = gdb_strchr(packet, ',');
    char* colon = gdb_strchr(comma, ':');
    
    if (!comma || !colon) {
        gdb_send_packet("E01");
        return;
    }
    
    int addr_len = comma - (packet + 1);
    if (addr_len <= 0 || addr_len > 8) {
        gdb_send_packet("E01");
        return;
    }
    
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    int len_len = colon - (comma + 1);
    if (len_len <= 0 || len_len > 8) {
        gdb_send_packet("E01");
        return;
    }
    
    char len_hex[9] = {0};
    for (int i = 0; i < len_len && i < 8; i++) {
        len_hex[i] = comma[1 + i];
    }
    len_hex[len_len] = '\0';
    uint32_t len = hex_str_to_uint(len_hex);
    
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        gdb_send_packet("E02");
        return;
    }

    const int packet_len = gdb_get_current_packet_length();
    const int payload_offset = (int)((colon + 1) - packet);
    const int encoded_len = packet_len - payload_offset;
    if (encoded_len < 0) {
        gdb_send_packet("E03");
        return;
    }

    static char decoded_data[DEBUG_BUFFER_SIZE];
    int decoded_len = 0;
    const unsigned char* encoded = (const unsigned char*)(colon + 1);

    for (int i = 0; i < encoded_len && decoded_len < (int)len; i++) {
        unsigned char ch = encoded[i];
        if (ch == '}') {
            i++;
            if (i >= encoded_len) {
                gdb_send_packet("E03");
                return;
            }
            ch = encoded[i] ^ 0x20;
        }
        decoded_data[decoded_len++] = (char)ch;
    }

    if (decoded_len != (int)len) {
        gdb_send_packet("E03");
        return;
    }

    if (gdb_safe_write_memory(host_addr, decoded_data, len) < 0) {
        gdb_send_packet("E04");
        return;
    }
    
    gdb_send_packet("OK");
}


// void gdb_handle_set_breakpoint(char* p) {
//     if (p[0] != 'Z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     type = (GDBBreakpointType)(*ptr++ - '0');

//     if (*ptr == ',') ptr++;

//     addr = 0;
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     if (gdb_add_breakpoint(addr, type) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }
void gdb_handle_set_breakpoint(char* p) {
    if (p[0] != 'Z') {
        gdb_send_error(0);
        return;
    }

    char* ptr = p + 1;
    GDBBreakpointType type;
    uint32_t addr;

    type = (GDBBreakpointType)(*ptr++ - '0');

    if (*ptr == ',') ptr++;

    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    addr = hex_str_to_uint(addr_hex);
    
    if (addr < 0xC0000000) {
        gdb_send_error(0);
        return;
    }

    if (gdb_add_breakpoint(addr, type) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

// void gdb_handle_remove_breakpoint(char* p) {
//     if (p[0] != 'z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     type = (GDBBreakpointType)(*ptr++ - '0');

//     if (*ptr == ',') ptr++;

//     addr = 0;
//     while (*ptr != ',' && *ptr != '\0') {
//         char c = *ptr++;
//         if (c >= '0' && c <= '9') {
//             addr = (addr << 4) | (c - '0');
//         } else if (c >= 'a' && c <= 'f') {
//             addr = (addr << 4) | (c - 'a' + 10);
//         } else if (c >= 'A' && c <= 'F') {
//             addr = (addr << 4) | (c - 'A' + 10);
//         }
//     }

//     if (gdb_remove_breakpoint(addr) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }
void gdb_handle_remove_breakpoint(char* p) {
    if (p[0] != 'z') {
        gdb_send_error(0);
        return;
    }

    char* ptr = p + 1;
    GDBBreakpointType type = (GDBBreakpointType)(*ptr++ - '0');
    
    if (*ptr == ',') ptr++;
    
    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    uint32_t addr = hex_str_to_uint(addr_hex);
    
    if (addr < 0xC0000000) {
        gdb_send_error(0);
        return;
    }

    if (gdb_remove_breakpoint(addr) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

void gdb_handle_query(char* p) {
    if (strncmp(p, "qSupported", 10) == 0) {
        gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;vContSupported+;qRcmd+;QStartNoAckMode-;swbreak+;hwbreak+;qXfer:features:read-;qXfer:threads:read-;qXfer:v6pp-json:read+");
        return;
    }

    if (strcmp(p, "vMustReplyEmpty") == 0) {
        gdb_send_packet((char*)"");
        return;
    }

    if (strcmp(p, "vCont?") == 0) {
        gdb_send_packet((char*)"vCont;c;C;s;S;r");
        return;
    }

    if (strcmp(p, "qC") == 0) {
        gdb_send_packet((char*)"QC1");
        return;
    }

    if (strcmp(p, "qAttached") == 0) {
        gdb_send_packet((char*)"1");
        return;
    }

    if (strcmp(p, "qTStatus") == 0) {
        gdb_send_packet((char*)"T0");
        return;
    }

    if (strcmp(p, "qfThreadInfo") == 0) {
        gdb_send_packet((char*)"m1");
        return;
    }

    if (strcmp(p, "qsThreadInfo") == 0) {
        gdb_send_packet((char*)"l");
        return;
    }

    if (strcmp(p, "QStartNoAckMode") == 0) {
        gdb_send_packet((char*)"OK");
        return;
    }

    if (strncmp(p, "qXfer:", 6) == 0) {
        char* object = p + 6;
        char* op_sep = gdb_strchr(object, ':');
        if (op_sep) {
            *op_sep = '\0';
            char* operation = op_sep + 1;
            char* annex_sep = gdb_strchr(operation, ':');
            if (annex_sep) {
                *annex_sep = '\0';
                char* annex = annex_sep + 1;
                char* offset_sep = gdb_strchr(annex, ':');
                if (offset_sep) {
                    *offset_sep = '\0';
                    char* offset_str = offset_sep + 1;
                    char* len_str = gdb_strchr(offset_str, ',');
                    if (len_str) {
                        *len_str = '\0';
                        uint32_t offset = hex_str_to_uint(offset_str);
                        uint32_t length = hex_str_to_uint(len_str + 1);
                        char response[DEBUG_BUFFER_SIZE];

                        if (strcmp(operation, "read") == 0) {
                            if (debug_json_handle_qxfer(object,
                                                        annex,
                                                        offset,
                                                        length,
                                                        response,
                                                        sizeof(response)) > 0) {
                                *len_str = ',';
                                *offset_sep = ':';
                                *annex_sep = ':';
                                *op_sep = ':';
                                gdb_send_packet(response);
                                return;
                            }
                        }

                        *len_str = ',';
                    }
                    *offset_sep = ':';
                }
                *annex_sep = ':';
            }
            *op_sep = ':';
        }
    }

    if (strcmp(p, "?") == 0) {
        gdb_send_packet((char*)"T05thread:1;");
        return;
    }

    if (strncmp(p, "qRcmd,", 6) == 0) {
        char* hex_cmd = p + 6;
        char cmd[DEBUG_BUFFER_SIZE];
        int cmd_len = 0;
        
        while (*hex_cmd != '\0' && cmd_len < DEBUG_BUFFER_SIZE - 1) {
            char c1 = *hex_cmd++;
            char c2 = *hex_cmd++;
            if (c1 == '\0' || c2 == '\0') break;
            
            uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
            cmd[cmd_len++] = (char)byte;
        }
        cmd[cmd_len] = '\0';
        
        if (is_fs_debugger_monitor_command(cmd)) {
            fs_debugger_handle_query(cmd, gdb_fs_debugger_output_writer, nullptr);
            gdb_send_ok();
            return;
        }

        if (debug_json_is_monitor_command(cmd)) {
            debug_json_handle_monitor_command(cmd, gdb_json_output_writer, nullptr);
            gdb_send_ok();
            return;
        }
        
        gdb_send_packet((char*)"");
        return;
    }

    if (strncmp(p, "qfs:", 4) == 0) {
        fs_debugger_handle_query(p, gdb_fs_debugger_output_writer, nullptr);
        gdb_send_ok();
        return;
    }

    gdb_send_packet((char*)"");
}

void gdb_handle_signal(char* p) {
    gdb_send_packet((char*)"T05thread:1;");
}
