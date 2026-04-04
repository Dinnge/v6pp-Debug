// #include "gdb_protocol.h"
// #include "gdb_socket.h"
// #include "gdb_registers.h"
// #include "gdb_memory.h"
// #include "gdb_breakpoints.h"
// #include "../../include/Video.h"
// #include "../debug.h"

// // Íâ²¿ÒýÓÃµ÷ÊÔÆ÷×´Ì¬
// extern DebuggerState g_debugger;

// // ×Ô¶¨Òå×Ö·û´®º¯Êý£¨ÄÚºË»·¾³£©
// static int strlen(const char* s) {
//     int len = 0;
//     while (s[len] != '\0') len++;
//     return len;
// }

// static void strcpy(char* dest, const char* src) {
//     int i = 0;
//     while (src[i] != '\0') {
//         dest[i] = src[i];
//         i++;
//     }
//     dest[i] = '\0';
// }

// static int strcmp(const char* s1, const char* s2) {
//     int i = 0;
//     while (s1[i] != '\0' && s2[i] != '\0') {
//         if (s1[i] != s2[i]) return s1[i] - s2[i];
//         i++;
//     }
//     return s1[i] - s2[i];
// }

// static int strncmp(const char* s1, const char* s2, int n) {
//     for (int i = 0; i < n; i++) {
//         if (s1[i] != s2[i]) return s1[i] - s2[i];
//         if (s1[i] == '\0') return 0;
//     }
//     return 0;
// }

// // È«¾Ö socket ±äÁ¿
// static Socket g_client_socket = (Socket)-1;

// // ÉèÖÃµ±Ç°Á¬½ÓµÄ¿Í»§¶Ë socket
// void gdb_set_client_socket(Socket sock) {
//     g_client_socket = sock;
// }

// // »ñÈ¡µ±Ç°¿Í»§¶Ë socket
// Socket gdb_get_client_socket(void) {
//     return g_client_socket;
// }

// int gdb_recv_packet(char* buffer, int buffer_size) {
//     if (g_client_socket == (Socket)-1) return -1;

//     // ´®¿Ú²ãÒÑ¾­ÊµÏÖÁË¶ÔÍêÕû RSP °üµÄ½âÎö£¨serial_recv_packet ·µ»Ø°üÌå³¤¶È£¬
//     // ²»°üº¬ÆðÊ¼ '$' ÓëÐ£ÑéºÍ£©£¬Òò´ËÕâÀïÖ±½ÓÒ»´ÎÐÔ´Ó socket ²ã¶ÁÈ¡ÍêÕû°ü¡£
//     // ·µ»ØÖµÓïÒå£º>0 °ü³¤¶È£¬0 ±íÊ¾µ±Ç°ÎÞÊý¾Ý£¬<0 ±íÊ¾Á¬½Ó¹Ø±Õ»ò´íÎó¡£
//     int recv_len = gdb_socket_recv(g_client_socket, buffer, buffer_size);
//     if (recv_len < 0) {
//         return -1; // Á¬½Ó´íÎó»òÒÑ¹Ø±Õ
//     }
//     if (recv_len == 0) {
//         return 0; // ÔÝÎÞÍêÕû°ü
//     }

//     // È·±£×Ö·û´®ÖÕÖ¹
//     if (recv_len >= buffer_size) recv_len = buffer_size - 1;
//     buffer[recv_len] = '\0';
//     // ÈÕÖ¾½ÓÊÕµ½µÄ°üÌå£¨±ãÓÚµ÷ÊÔÎÕÊÖ£©
//     Diagnose::Write("RX: ");
//     Diagnose::Write(buffer);
//     Diagnose::Write("\n");
//     return recv_len;
// }

// void gdb_send_packet(char *data)
// {
//     if (g_client_socket == (Socket)-1) return;

//     char buffer[DEBUG_BUFFER_SIZE];
//     int len = strlen(data);

//     // ¼ÆËãÐ£ÑéºÍ
//     unsigned char checksum = 0;
//     for (int i = 0; i < len; i++) {
//         checksum += (unsigned char)data[i];
//     }

//     // ¹¹ÔìÊý¾Ý°ü: $Êý¾Ý#Ð£ÑéºÍ
//     int pos = 0;
//     buffer[pos++] = '$';
//     // Ö±½Ó¸´ÖÆ data ÖÐµÄ×Ö½Ú£¨²»ÒªÐ´Èë¶îÍâµÄ '\0' ×Ö½Úµ½Êý¾Ý²¿·Ö£©
//     for (int i = 0; i < len; i++) {
//         buffer[pos++] = data[i];
//     }
//     buffer[pos++] = '#';

//     // ×ª»»Ð£ÑéºÍÎªÊ®Áù½øÖÆ
//     char hex[3];
//     hex[0] = "0123456789abcdef"[(checksum >> 4) & 0x0F];
//     hex[1] = "0123456789abcdef"[checksum & 0x0F];
//     buffer[pos++] = hex[0];
//     buffer[pos++] = hex[1];
//     buffer[pos] = '\0';

//     // ·¢ËÍµ½ GDB£¬ÏÈ¼ÇÂ¼Òª·¢ËÍµÄÔ­Ê¼°üÄÚÈÝÒÔ±ãµ÷ÊÔ
//     Diagnose::Write("TX: ");
//     Diagnose::Write(buffer);
//     Diagnose::Write("\n");
//     gdb_socket_send(g_client_socket, buffer, pos);
// }

// void gdb_send_ok(void) {
//     gdb_send_packet("OK");
// }

// void gdb_send_error(int code) {
//     char buffer[16];
//     buffer[0] = 'E';
//     // ×ª»»´íÎóÂëÎªÊ®Áù½øÖÆ
//     const char* hex = "0123456789abcdef";
//     buffer[1] = hex[(code >> 4) & 0x0F];
//     buffer[2] = hex[code & 0x0F];
//     buffer[3] = '\0';
//     gdb_send_packet(buffer);
// }

// GDBCommand gdb_parse_command(char* packet) {
//     if (!packet) return GDB_CMD_UNKNOWN;
//     char cmd = packet[0];
//     switch (cmd) {
//         case 'c': return GDB_CMD_CONTINUE;
//         case 's': return GDB_CMD_STEP;
//         case 'g': return GDB_CMD_READ_REG;
//         case 'G': return GDB_CMD_WRITE_REG;
//         case 'm': return GDB_CMD_READ_MEM;
//         case 'M': return GDB_CMD_WRITE_MEM;
//         case 'Z': return GDB_CMD_SET_BREAK;
//         case 'z': return GDB_CMD_REMOVE_BREAK;
//         case 'q': return GDB_CMD_QUERY;
//         case 'v': return GDB_CMD_VENDOR;;
//         case '?': return GDB_CMD_SIGNAL;
//         default: return GDB_CMD_UNKNOWN;
//     }
// }

// // ´¦Àí¼ÌÐøÖ´ÐÐÃüÁî
// void gdb_handle_continue(char *p) {
//     gdb_send_ok();
//     g_debugger.mode = DEBUG_MODE_CONTINUE;
//     Diagnose::Write("Continuing execution...\n");
// }

// // ´¦Àíµ¥²½Ö´ÐÐÃüÁî
// void gdb_handle_step(char *p) {
//     gdb_send_ok();
//     g_debugger.mode = DEBUG_MODE_STEP;
//     Diagnose::Write("Stepping...\n");
// }

// // ´¦Àí¶ÁÈ¡¼Ä´æÆ÷ÃüÁî
// void gdb_handle_read_registers(char* p) {
//     // ÏÈ±£´æµ±Ç°¼Ä´æÆ÷
//     gdb_registers_save();

//     // ×ª»»Îª GDB ¸ñÊ½×Ö·û´®
//     char reg_str[512];
//     gdb_registers_to_string(reg_str, sizeof(reg_str));

//     // ·¢ËÍ¸ø GDB
//     gdb_send_packet(reg_str);
// }

// // ´¦ÀíÐ´Èë¼Ä´æÆ÷ÃüÁî
// void gdb_handle_write_registers(char* p) {
//     // ½âÎö¼Ä´æÆ÷Êý¾Ý£¨GDB ¸ñÊ½£ºGXX...£©
//     // Ìø¹ý 'G' Ç°×º
//     if (p[0] != 'G') {
//         gdb_send_error(0);
//         return;
//     }

//     char* data = p + 1;
//     uint32_t value = 0;

//     // ½âÎöÊ®Áù½øÖÆ×Ö·û´®
//     int pos = 0;
//     for (int i = 0; i < GDB_REG_COUNT; i++) {
//         value = 0;
//         for (int j = 0; j < 4; j++) {
//             char c1 = data[pos++];
//             char c2 = data[pos++];
//             uint8_t byte = 0;

//             if (c1 >= '0' && c1 <= '9') byte |= (c1 - '0') << 4;
//             else if (c1 >= 'a' && c1 <= 'f') byte |= (c1 - 'a' + 10) << 4;
//             else if (c1 >= 'A' && c1 <= 'F') byte |= (c1 - 'A' + 10) << 4;

//             if (c2 >= '0' && c2 <= '9') byte |= (c2 - '0');
//             else if (c2 >= 'a' && c2 <= 'f') byte |= (c2 - 'a' + 10);
//             else if (c2 >= 'A' && c2 <= 'F') byte |= (c2 - 'A' + 10);

//             value |= byte << (j * 8);
//         }

//         gdb_set_register(i, value);
//     }

//     gdb_send_ok();
// }

// // ´¦Àí¶ÁÈ¡ÄÚ´æÃüÁî£¨¸ñÊ½£ºmµØÖ·,³¤¶È£©
// void gdb_handle_read_memory(char* p) {
//     // ½âÎöµØÖ·ºÍ³¤¶È
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // Ìø¹ý 'm' Ç°×º
//     char* ptr = p + 1;

//     // ½âÎöµØÖ·£¨Ê®Áù½øÖÆ£©
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

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎö³¤¶È
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

//     // ÏÞÖÆ×î´ó³¤¶È
//     if (len > DEBUG_BUFFER_SIZE / 2) {
//         len = DEBUG_BUFFER_SIZE / 2;
//     }

//     // ¶ÁÈ¡ÄÚ´æ
//     static char mem_buffer[DEBUG_BUFFER_SIZE];
//     if (gdb_read_memory(addr, mem_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     // ×ª»»ÎªÊ®Áù½øÖÆ×Ö·û´®
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

// // ´¦ÀíÐ´ÈëÄÚ´æÃüÁî£¨¸ñÊ½£ºMµØÖ·,³¤¶È:Êý¾Ý£©
// void gdb_handle_write_memory(char* p) {
//     // ½âÎöµØÖ·ºÍ³¤¶È
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // Ìø¹ý 'M' Ç°×º
//     char* ptr = p + 1;

//     // ½âÎöµØÖ·
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

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎö³¤¶È
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

//     // Ìø¹ý ':'
//     if (*ptr == ':') ptr++;

//     // ½âÎöÊý¾Ý
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

//     // Ð´ÈëÄÚ´æ
//     if (gdb_write_memory(addr, data_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     gdb_send_ok();
// }

// // ´¦ÀíÉèÖÃ¶ÏµãÃüÁî£¨¸ñÊ½£ºZÀàÐÍ,µØÖ·,³¤¶È£©
// void gdb_handle_set_breakpoint(char* p) {
//     if (p[0] != 'Z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // ½âÎöÀàÐÍ
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎöµØÖ·
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

//     // Ìí¼Ó¶Ïµã
//     if (gdb_add_breakpoint(addr, type) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }

// // ´¦ÀíÒÆ³ý¶ÏµãÃüÁî£¨¸ñÊ½£ºzÀàÐÍ,µØÖ·,³¤¶È£©
// void gdb_handle_remove_breakpoint(char* p) {
//     if (p[0] != 'z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // ½âÎöÀàÐÍ
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎöµØÖ·
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

//     // ÒÆ³ý¶Ïµã
//     if (gdb_remove_breakpoint(addr) != 0) {
//         gdb_send_error(1);
//     } else {
//         gdb_send_ok();
//     }
// }

// // ´¦Àí²éÑ¯ÃüÁî
// void gdb_handle_query(char* p) {
//     // qSupported - GDB ÌØÐÔ²éÑ¯
//     if (p[0] == 'q' && p[1] == 'S') {
//         // ·µ»ØÖ§³ÖµÄÌØÐÔ£¬Ã÷È·ÅÅ³ýÎ´ÖªÏî
//         gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;xmlRegisters-;timeout-;QStartNoAckMode-");
//         return;
//     }

//     // vMustReplyEmpty - ±ØÐë»Ø¸´¿Õ
//     if (strcmp(p, "vMustReplyEmpty") == 0) {
//         gdb_send_packet("");
//         return;
//     }

//     // vCont? - ²éÑ¯Ö§³ÖµÄ¼ÌÐøÃüÁî
//     if (strcmp(p, "vCont?") == 0) {
//         gdb_send_packet("vCont;c;C;s;S");
//         return;
//     }

//     // qC - ²éÑ¯µ±Ç°Ïß³Ì ID
//     if (p[0] == 'q' && p[1] == 'C' && p[2] == '\0') {
//         gdb_send_packet("QC1");
//         return;
//     }

//     // qAttached - ²éÑ¯ÊÇ·ñattached
//     if (strcmp(p, "qAttached") == 0) {
//         gdb_send_packet("1");  // ÒÑattached
//         return;
//     }

//     // ÆäËû²éÑ¯ÃüÁî·µ»Ø¿Õ
//     gdb_send_packet("");
// }

// // ´¦ÀíÐÅºÅÃüÁî
// void gdb_handle_signal(char* p) {
//     // ·¢ËÍÍ£Ö¹ÐÅºÅ (S05 = SIGTRAP)
//     gdb_send_packet("S05");
// }
#include "gdb_protocol.h"
#include "gdb_socket.h"
#include "gdb_registers.h"
#include "gdb_memory.h"
#include "gdb_breakpoints.h"
#include "../../include/Video.h"
#include "../debug.h"

// Íâ²¿ÒýÓÃµ÷ÊÔÆ÷×´Ì¬
extern "C" DebuggerState g_debugger;

// ×Ô¶¨Òå×Ö·û´®º¯Êý£¨ÄÚºË»·¾³£©
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

// Ê®Áù½øÖÆ×Ö·û´®×ªÕûÊý
uint32_t hex_str_to_uint(const char* hex_str) {
    uint32_t result = 0;
    while (*hex_str != '\0') {
        result = (result << 4) | hex_char_to_value(*hex_str);
        hex_str++;
    }
    return result;
}

// ½â¾ö´óÐ¡¶Ë²»Ò»ÖÂµÄÎÊÌâ
// ½»»»×Ö½ÚÐò
uint32_t swap_endian_32(uint32_t value) {
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

// Ìí¼Ó×Ö½ÚÐò×ª»»¸¨Öúº¯Êý
uint32_t gdb_hex_to_host32(const char* hex_str) {
    uint32_t result = 0;
    
    // ½âÎö´ó¶ËÐòÊ®Áù½øÖÆ
    for (int i = 0; i < 8; i += 2) {
        uint8_t byte = (hex_char_to_value(hex_str[i]) << 4) | 
                      hex_char_to_value(hex_str[i+1]);
        result = (result << 8) | byte;
    }
    
    return swap_endian_32(result);
}

void host32_to_gdb_hex(uint32_t value, char* output) {
    // Ö÷»úÐ¡¶ËÐò×ªGDB´ó¶ËÐò
    uint32_t be_value = ((value & 0xFF) << 24) |
                       ((value & 0xFF00) << 8) |
                       ((value & 0xFF0000) >> 8) |
                       ((value & 0xFF000000) >> 24);
    
    // ×ª»»ÎªÊ®Áù½øÖÆ
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        output[i] = hex_chars[(be_value >> shift) & 0xF];
    }
    output[8] = '\0';
}

// È«¾Ö socket ±äÁ¿
static Socket g_client_socket = (Socket)-1;
// È«¾Ö¼Ä´æÆ÷ÉÏÏÂÎÄ
// static GDBRegisters g_reg_context;


// ÉèÖÃµ±Ç°Á¬½ÓµÄ¿Í»§¶Ë socket
void gdb_set_client_socket(Socket sock) {
    g_client_socket = sock;
}

// »ñÈ¡µ±Ç°¿Í»§¶Ë socket
Socket gdb_get_client_socket(void) {
    return g_client_socket;
}

// ½ÓÊÕ GDB Êý¾Ý°ü£¨´ø ACK/NACK Ö§³Ö£©
int gdb_recv_packet(char* buffer, int buffer_size) {
    if (g_client_socket == (Socket)-1) return -1;

    int state = 0;  // 0=µÈ´ý'$', 1=½ÓÊÕÊý¾Ý, 2=µÈ´ý'#', 3=¶ÁÈ¡Ð£ÑéºÍµÚÒ»¸ö×Ö·û, 4=¶ÁÈ¡Ð£ÑéºÍµÚ¶þ¸ö×Ö·û
    int packet_len = 0;
    unsigned char recv_checksum = 0;
    unsigned char calc_checksum = 0;
    char recv_byte;

    while (1) {
        // ½ÓÊÕÒ»¸ö×Ö½Ú
        int recv_len = gdb_socket_recv(g_client_socket, &recv_byte, 1);
        if (recv_len <= 0) {
            // ÎÞÊý¾Ý»òÁ¬½Ó¶Ï¿ª
            return -1;
        }

        char c = recv_byte;

        // ×´Ì¬»ú½âÎöÊý¾Ý°ü
        switch (state) {
            case 0:  // µÈ´ý '$'
                if (c == '$') {
                    packet_len = 0;
                    calc_checksum = 0;
                    recv_checksum = 0;
                    state = 1;
                } else if (c == '+') {
                    // ACK - ¶ÔÎÒÃÇ·¢ËÍµÄÊý¾Ý°üµÄÈ·ÈÏ£¬ºöÂÔ
                    continue;
                } else if (c == '-') {
                    // NACK - ¶ÔÎÒÃÇ·¢ËÍµÄÊý¾Ý°üµÄ·ñÈÏ£¬ÐèÒªÖØ·¢
                    // ·µ»ØÌØÊâ´íÎóÂëÈÃÉÏ²ã´¦ÀíÖØ·¢
                    return -2;
                }
                // ºöÂÔÆäËû×Ö·û£¬µÈ´ý '$'
                break;

            case 1:  // ½ÓÊÕÊý¾ÝÄÚÈÝ
                if (c == '#') {
                    state = 2;
                } else if (packet_len < buffer_size - 1) {
                    buffer[packet_len++] = c;
                    calc_checksum += (unsigned char)c;
                } else {
                    // »º³åÇø²»×ã£¬¶ªÆúÊý¾Ý°ü
                    state = 0;
                }
                break;

            case 2:  // ¶ÁÈ¡Ð£ÑéºÍµÚÒ»¸ö×Ö·û
                if (c >= '0' && c <= '9') {
                    recv_checksum = (c - '0') << 4;
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum = (c - 'a' + 10) << 4;
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum = (c - 'A' + 10) << 4;
                } else {
                    // ÎÞÐ§µÄÐ£ÑéºÍ×Ö·û£¬¶ªÆúÊý¾Ý°ü
                    state = 0;
                    break;
                }
                state = 3;
                break;

            case 3:  // ¶ÁÈ¡Ð£ÑéºÍµÚ¶þ¸ö×Ö·û²¢ÑéÖ¤
                if (c >= '0' && c <= '9') {
                    recv_checksum |= (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    recv_checksum |= (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    recv_checksum |= (c - 'A' + 10);
                } else {
                    // ÎÞÐ§µÄÐ£ÑéºÍ×Ö·û£¬¶ªÆúÊý¾Ý°ü
                    state = 0;
                    break;
                }

                // ÑéÖ¤Ð£ÑéºÍ
                if (recv_checksum == calc_checksum) {
                    // Ð£ÑéºÍÕýÈ·£¬·¢ËÍ ACK
                    buffer[packet_len] = '\0';
                    char ack = '+';
                    gdb_socket_send(g_client_socket, &ack, 1);
                    return packet_len;
                } else {
                    // Ð£ÑéºÍ´íÎó£¬·¢ËÍ NACK
                    char nack = '-';
                    gdb_socket_send(g_client_socket, &nack, 1);
                    // ÖØÖÃ×´Ì¬»ú£¬µÈ´ýÖØ´«µÄÊý¾Ý°ü
                    state = 0;
                }
                break;
        }
    }
}

void gdb_send_packet(char* data) {
    if (g_client_socket == (Socket)-1) return;

    // µ÷ÊÔÈÕÖ¾£º·¢ËÍÊý¾Ý°ü£¨Ê¹ÓÃÔ­×ÓÊä³ö£©
    char tx_msg[DEBUG_BUFFER_SIZE + 100];
    int msg_pos = 0;
    
    // ¸´ÖÆ "[GDB] TX packet: "
    const char* tx_prefix = "[GDB] TX packet: ";
    while (*tx_prefix) tx_msg[msg_pos++] = *tx_prefix++;
    
    // ¸´ÖÆÊý¾ÝÄÚÈÝ
    int i = 0;
    while (data[i] != '\0' && i < DEBUG_BUFFER_SIZE - 1) {
        tx_msg[msg_pos++] = data[i++];
    }
    
    tx_msg[msg_pos++] = '\n';
    tx_msg[msg_pos] = '\0';
    
    Diagnose::Write(tx_msg);

    char buffer[DEBUG_BUFFER_SIZE];
    int len = strlen(data);

    // ¼ÆËãÐ£ÑéºÍ
    unsigned char checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += (unsigned char)data[i];
    }

    // ¹¹ÔìÊý¾Ý°ü: $Êý¾Ý#Ð£ÑéºÍ
    int pos = 0;
    buffer[pos++] = '$';
    strcpy(buffer + pos, data);
    pos += len;
    buffer[pos++] = '#';

    // ×ª»»Ð£ÑéºÍÎªÊ®Áù½øÖÆ
    char hex[3];
    hex[0] = "0123456789abcdef"[(checksum >> 4) & 0x0F];
    hex[1] = "0123456789abcdef"[checksum & 0x0F];
    buffer[pos++] = hex[0];
    buffer[pos++] = hex[1];
    buffer[pos] = '\0';

    // ·¢ËÍµ½ GDB
    gdb_socket_send(g_client_socket, buffer, pos);
}

void gdb_send_ok(void) {
    gdb_send_packet((char*)"OK");
}

void gdb_send_error(int code) {
    char buffer[16];
    buffer[0] = 'E';
    // ×ª»»´íÎóÂëÎªÊ®Áù½øÖÆ
    const char* hex = "0123456789abcdef";
    buffer[1] = hex[(code >> 4) & 0x0F];
    buffer[2] = hex[code & 0x0F];
    buffer[3] = '\0';
    gdb_send_packet(buffer);
}

GDBCommand gdb_parse_command(char* packet) {
    if (!packet) return GDB_CMD_UNKNOWN;
    // Ê×ÏÈ¼ì²éÌØÊâÇé¿ö
    if (strcmp(packet, "vCont?") == 0) {
        return GDB_CMD_QUERY;  // vCont? ÊÇ²éÑ¯
    }
    
    if (strncmp(packet, "vCont;c", 7) == 0) {
        return GDB_CMD_CONTINUE;  // vCont;c... ÊÇ¼ÌÐø
    }
    
    if (strncmp(packet, "vCont;s", 7) == 0) {
        return GDB_CMD_STEP;  // vCont;s... ÊÇµ¥²½
    }
    char cmd = packet[0];
    switch (cmd) {
        case 'c': return GDB_CMD_CONTINUE;
        case 's': return GDB_CMD_STEP;
        case 'g': return GDB_CMD_READ_REG;
        case 'G': return GDB_CMD_WRITE_REG;
        case 'P': return GDB_CMD_WRITE_SINGLE_REG;
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

// ´¦Àí¼ÌÐøÖ´ÐÐÃüÁî
void gdb_handle_continue(char* packet) {
    Diagnose::Write("[GDB] continue command: %s\n", packet);

    if (strcmp(packet, "c") == 0 || strncmp(packet, "vCont;c", 7) == 0) {
        GDBRegisters* regs = gdb_get_registers();
        regs->eflags |= 0x200;
        gdb_clear_single_step();
        gdb_registers_commit_to_trap_frame();
        g_debugger.mode = DEBUG_MODE_CONTINUE;
        g_debugger.resume_requested = 1;
        Diagnose::Write("[CONTINUE] resume requested\n");
    } else {
        Diagnose::Write("[ERROR] invalid continue command: %s\n", packet);
        gdb_send_error(0);
    }
}

void gdb_handle_step(char* packet) {
    Diagnose::Write("[GDB] step command: %s\n", packet);

    if (strcmp(packet, "s") == 0 || strncmp(packet, "vCont;s", 7) == 0) {
        GDBRegisters* regs = gdb_get_registers();
        regs->eflags |= 0x200;
        gdb_set_single_step();
        gdb_registers_commit_to_trap_frame();
        g_debugger.mode = DEBUG_MODE_STEP;
        g_debugger.resume_requested = 1;
        Diagnose::Write("[STEP] single-step requested\n");
    } else {
        Diagnose::Write("[ERROR] invalid step command: %s\n", packet);
        gdb_send_error(0);
    }
}

void gdb_handle_thread_command(char* packet) {
    // ¸ñÊ½: H<²Ù×÷ÀàÐÍ><Ïß³ÌºÅ>
    if (strlen(packet) < 3) {
        gdb_send_packet("E01");  // ¸ñÊ½´íÎó
        return;
    }
    
    char op_type = packet[1];  // 'g', 'c', »ò 's'
    char* thread_str = packet + 2;
    
    Diagnose::Write("[GDB] Ïß³ÌÃüÁî: ²Ù×÷=%c, Ïß³Ì=%s\n", op_type, thread_str);
    
    // µ¥Ïß³ÌÏµÍ³£¬Ö»Ö§³ÖÏß³Ì0ºÍ1
    if (strcmp(thread_str, "0") == 0 || strcmp(thread_str, "1") == 0) {
        gdb_send_packet("OK");
    } else {
        gdb_send_packet("E01");  // ÎÞÐ§Ïß³Ì
    }
}

// void gdb_handle_read_registers(char* p) {

//     if (p == NULL || p[0] == '\0') {
//         Diagnose::Write("[GDB] ¾¯¸æ: ½ÓÊÕµ½¿ÕÖ¸Õë»ò¿ÕÊý¾Ý°ü£¬·µ»ØÄ¬ÈÏ¼Ä´æÆ÷Öµ\n");
        
//         // ·µ»ØÄ¬ÈÏ¼Ä´æÆ÷Öµ£¨È«Áã£©
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
    
//     // ¶ÁÈ¡ÕæÊµ¼Ä´æÆ÷Öµ£¨Ð¡¶ËÐò£©
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
    
//     // ¶Î¼Ä´æÆ÷
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
    
//     // µ÷ÊÔÊä³ö£ºÏÔÊ¾¹Ø¼ü¼Ä´æÆ÷×ª»»Ç°ºóµÄÖµ
//     char le_display[9], be_display[9];
//     host32_to_gdb_hex(regs[8], be_display);  // Ê¹ÓÃÄúÏÖÓÐµÄº¯Êý
//     Diagnose::Write("[DEBUG] EIP min=0x%08x ¡ú  max=0x%s\n", regs[8], be_display);
    
//     // host32_to_gdb_hex(regs[4], be_display);
//     // Diagnose::Write("[DEBUG] ESP×ª»»: Ð¡¶ËÐò=0x%08x ¡ú ´ó¶ËÐò=0x%s\n", regs[4], be_display);
    
//     // host32_to_gdb_hex(regs[0], be_display);
//     // Diagnose::Write("[DEBUG] EAX×ª»»: Ð¡¶ËÐò=0x%08x ¡ú ´ó¶ËÐò=0x%s\n", regs[0], be_display);
    
//     // ¹Ø¼üÐÞ¸Ä£º½«ËùÓÐ¼Ä´æÆ÷Öµ×ª»»Îª´ó¶ËÐò¸ñÊ½·¢ËÍ¸øGDB
//     char response[129];
    
//     for (int i = 0; i < 16; i++) {
//         // Ê¹ÓÃÄúÏÖÓÐµÄº¯Êý£ºÐ¡¶ËÐòÖ÷»úÖµ ¡ú ´ó¶ËÐòÊ®Áù½øÖÆ×Ö·û´®
//         host32_to_gdb_hex(regs[i], response + i * 8);
//     }
//     response[128] = '\0';
    
//     gdb_send_packet(response);
// }

void gdb_handle_read_registers(char* p) {
    // È·±£ÔÚ½øÈëµ÷ÊÔÆ÷Ê±µ÷ÓÃ¹ý `gdb_registers_save()`£¬ÕâÀïÎªÁË±£ÏÕÒ²µ÷ÓÃÒ»´Î¡£
    gdb_registers_save();

    // Ê¹ÓÃÒÑÓÐµÄ×ª»»º¯Êý¹¹Ôì GDB ¸ñÊ½×Ö·û´®²¢·¢ËÍ¡£
    char reg_str[512];
    gdb_registers_to_string(reg_str, sizeof(reg_str));
    gdb_send_packet(reg_str);
}

// ´¦ÀíÐ´Èë¼Ä´æÆ÷ÃüÁî ÐèÒª½«´ó¶Ë×ªÎªÐ¡¶Ë
void gdb_handle_write_registers(char* p) {
    if (p[0] != 'G') {
        gdb_send_error(0);
        return;
    }

    char* data = p + 1;
    int data_len = strlen(data);
    
    // ÑéÖ¤Êý¾Ý³¤¶È£ºÃ¿¸ö¼Ä´æÆ÷8¸öÊ®Áù½øÖÆ×Ö·û
    if (data_len != GDB_REG_COUNT * 8) {
        gdb_send_error(0);
        return;
    }
    
    for (int reg_index = 0; reg_index < GDB_REG_COUNT; reg_index++) {
        // ÌáÈ¡8×Ö·ûÊ®Áù½øÖÆ
        char reg_hex[9] = {0};
        for (int i = 0; i < 8; i++) {
            reg_hex[i] = data[reg_index * 8 + i];
        }
        reg_hex[8] = '\0';
        
        // ¹Ø¼üÐÞ¸´£ºÖ±½Ó×ª»»£¬²»Òª×Ö½ÚÐò×ª»»
        uint32_t reg_value = hex_str_to_uint(reg_hex);
        
        // ÉèÖÃ¼Ä´æÆ÷
        gdb_set_register(reg_index, reg_value);
    }
    
    gdb_send_ok();
}

// ´¦ÀíPÃüÁî£ºÐ´Èëµ¥¸ö¼Ä´æÆ÷
// ¸ñÊ½: Pn=xxxxxxxx  £¨nÊÇ¼Ä´æÆ÷±àºÅ£¬xxxxxxxxÊÇ¼Ä´æÆ÷Öµ£©
void gdb_handle_write_single_register(char* p) {
    Diagnose::Write("[GDB] ½øÈëgdb_handle_write_single_registerº¯Êý\n");
    if (p[0] != 'P') {
        gdb_send_error(0);
        return;
    }
    
    // Ìø¹ý'P'
    char* data = p + 1;
    
    // ²éÕÒ'='·Ö¸ô·û
    char* equal_sign = gdb_strchr(data, '=');
    if (!equal_sign) {
        Diagnose::Write("[GDB] ÎÞÐ§µÄPÃüÁî¸ñÊ½: %s\n", p);
        gdb_send_error(0);
        return;
    }
    
    // ÌáÈ¡¼Ä´æÆ÷±àºÅ
    char reg_str[16] = {0};
    int reg_len = equal_sign - data;
    
    // Ê¹ÓÃ×Ô¶¨ÒåµÄ×Ö·û´®¸´ÖÆ
    for (int i = 0; i < reg_len && i < 15; i++) {
        reg_str[i] = data[i];
    }
    reg_str[reg_len] = '\0';
    
    // ÌáÈ¡¼Ä´æÆ÷Öµ
    char* value_str = equal_sign + 1;
    
    // ×ª»»ÎªÕûÊý - Ê¹ÓÃ×Ô¶¨ÒåµÄ×Ö·û´®×ªÕûÊý
    int reg_num = 0;
    for (int i = 0; i < reg_len && reg_str[i] >= '0' && reg_str[i] <= '9'; i++) {
        reg_num = reg_num * 10 + (reg_str[i] - '0');
    }
    
    // Ê¹ÓÃ×Ô¶¨ÒåµÄÊ®Áù½øÖÆ×ª»»º¯Êý
    uint32_t reg_value = hex_str_to_uint(value_str);
    
    // ×Ö½ÚÐò×ª»»£ºGDB·¢ËÍÐ¡¶ËÐò
    uint32_t host_value = 
        ((reg_value & 0x000000FF) << 24) |  // ×Ö½Ú0 ¡ú ×Ö½Ú3
        ((reg_value & 0x0000FF00) << 8)  |  // ×Ö½Ú1 ¡ú ×Ö½Ú2  
        ((reg_value & 0x00FF0000) >> 8)  |  // ×Ö½Ú2 ¡ú ×Ö½Ú1
        ((reg_value & 0xFF000000) >> 24);   // ×Ö½Ú3 ¡ú ×Ö½Ú0
    
    Diagnose::Write("[GDB] PÃüÁî: ¼Ä´æÆ÷%d = 0x%s -> 0x%08x\n", 
                   reg_num, value_str, host_value);
    
    // ÑéÖ¤¼Ä´æÆ÷±àºÅ
    if (reg_num < 0 || reg_num > GDB_REG_COUNT) {
        Diagnose::Write("[GDB] ÎÞÐ§µÄ¼Ä´æÆ÷±àºÅ: %d\n", reg_num);
        gdb_send_error(0);
        return;
    }
    
    // ÉèÖÃ¼Ä´æÆ÷
    gdb_set_register(reg_num, host_value);
    
    gdb_send_ok();
}

// ´¦Àí¶ÁÈ¡ÄÚ´æÃüÁî£¨¸ñÊ½£ºmµØÖ·,³¤¶È£©
// void gdb_handle_read_memory(char* p) {
//     // ½âÎöµØÖ·ºÍ³¤¶È
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // Ìø¹ý 'm' Ç°×º
//     char* ptr = p + 1;

//     // ½âÎöµØÖ·£¨Ê®Áù½øÖÆ£©
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

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎö³¤¶È
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

//     // ÏÞÖÆ×î´ó³¤¶È
//     if (len > DEBUG_BUFFER_SIZE / 2) {
//         len = DEBUG_BUFFER_SIZE / 2;
//     }

//     // ¶ÁÈ¡ÄÚ´æ
//     static char mem_buffer[DEBUG_BUFFER_SIZE];
//     if (gdb_read_memory(addr, mem_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     // ×ª»»ÎªÊ®Áù½øÖÆ×Ö·û´®
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

// °²È«µÄÄÚ´æ¶ÁÈ¡º¯Êý
int gdb_safe_read_memory(uint32_t addr, char* buffer, uint32_t len) {
    // °²È«¼ì²é1£ºÖ»ÔÊÐíÄÚºË¿Õ¼ä·ÃÎÊ
    // if (addr < 0xC0000000) {
    //     Diagnose::Write("[GDB] ÄÚ´æ¶ÁÈ¡Ê§°Ü£º¾Ü¾øÓÃ»§¿Õ¼ä·ÃÎÊ 0x%08x\n", addr);
    //     return -1;
    // }
    
    // °²È«¼ì²é2£º±ß½ç¼ì²é
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        Diagnose::Write("[GDB] ÄÚ´æ¶ÁÈ¡Ê§°Ü£ºÎÞÐ§³¤¶È %u\n", len);
        return -1;
    }
    
    // °²È«¼ì²é3£ºµØÖ··¶Î§¼ì²é
    if (addr >= 0xc0400000) {  // ¼ì²éÒç³ö
        Diagnose::Write("[GDB] ÄÚ´æ¶ÁÈ¡Ê§°Ü£ºµØÖ·Òç³ö 0x%08x + %u\n", addr, len);
        return -1;
    }
    
    Diagnose::Write("[GDB] °²È«¶ÁÈ¡: µØÖ·=0x%08x, ³¤¶È=%u\n", addr, len);
    
    // Ö±½ÓÄÚ´æ¶ÁÈ¡
    for (uint32_t i = 0; i < len; i++) {
        char* src = (char*)(addr + i);
        buffer[i] = *src;
    }
    
    return 0;
}

void gdb_handle_read_memory(char* packet) {
    // ¸ñÊ½: maddr,length
    char* comma = gdb_strchr(packet, ',');
    if (!comma) {
        gdb_send_packet("E01");
        return;
    }
    
    // ÌáÈ¡µØÖ·
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    // ¹Ø¼üÐÞ¸Ä£ºÍ³Ò»Ê¹ÓÃÖ±½Ó½âÎö£¬²»½øÐÐ×Ö½ÚÐò×ª»»
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    // ½âÎö³¤¶È
    char* len_str = comma + 1;
    uint32_t len = hex_str_to_uint(len_str);
    
    // ÏÞÖÆ³¤¶È
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    // °²È«¼ì²é
    if (host_addr >= 0xc0400000) {
        gdb_send_packet("E00");
        return;
    }
    
    // ÄÚ´æ¶ÁÈ¡ - Ê¹ÓÃÕýÈ·µÄhost_addr
    static char mem_buffer[DEBUG_BUFFER_SIZE];
    if (gdb_safe_read_memory(host_addr, mem_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    // ×ª»»ÎªÊ®Áù½øÖÆ
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


// ´¦ÀíÐ´ÈëÄÚ´æÃüÁî£¨¸ñÊ½£ºMµØÖ·,³¤¶È:Êý¾Ý£©
// void gdb_handle_write_memory(char* p) {
//     // ½âÎöµØÖ·ºÍ³¤¶È
//     uint32_t addr = 0;
//     uint32_t len = 0;

//     // Ìø¹ý 'M' Ç°×º
//     char* ptr = p + 1;

//     // ½âÎöµØÖ·
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

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎö³¤¶È
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

//     // Ìø¹ý ':'
//     if (*ptr == ':') ptr++;

//     // ½âÎöÊý¾Ý
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

//     // Ð´ÈëÄÚ´æ
//     if (gdb_write_memory(addr, data_buffer, len) < 0) {
//         gdb_send_error(0);
//         return;
//     }

//     gdb_send_ok();
// }

int gdb_safe_write_memory(uint32_t addr, const char* data, uint32_t len) {
    // °²È«¼ì²é1£ºÖ»ÔÊÐíÄÚºË¿Õ¼ä·ÃÎÊ
    // if (addr < 0xC0000000) {
    //     return -1;
    // }
    
    // °²È«¼ì²é2£º±ß½ç¼ì²é
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        return -1;
    }
    
    if (addr >= 0xc0400000) {  // ¼ì²éÒç³ö
        Diagnose::Write("[GDB] ÄÚ´æ¶ÁÈ¡Ê§°Ü£ºµØÖ·Òç³ö 0x%08x + %u\n", addr, len);
        return -1;
    }

    // °²È«¼ì²é3£ºµØÖ··¶Î§¼ì²é
    if (addr + len < addr) {  // ¼ì²éÒç³ö
        return -1;
    }
    
    // Ö±½ÓÄÚ´æÐ´Èë
    for (uint32_t i = 0; i < len; i++) {
        char* dest = (char*)(addr + i);
        *dest = data[i];
    }
    
    return 0;
}

// void gdb_handle_write_memory(char* packet) {
//     Diagnose::Write("[GDB] ÄÚ´æÐ´Èë: %s\n", packet);
    
//     // ¸ñÊ½: Maddr,length:data
//     char* comma = gdb_strchr(packet, ',');
//     if (!comma) {
//         Diagnose::Write("[GDB] ÎÞÐ§¸ñÊ½: È±ÉÙ¶ººÅ\n");
//         gdb_send_packet("E01");
//         return;
//     }
    
//     char* colon = gdb_strchr(comma, ':');
//     if (!colon) {
//         Diagnose::Write("[GDB] ÎÞÐ§¸ñÊ½: È±ÉÙÃ°ºÅ\n");
//         gdb_send_packet("E01");
//         return;
//     }
    
//     // ÌáÈ¡µØÖ·
//     int addr_len = comma - (packet + 1);
//     char addr_hex[9] = {0};
//     for (int i = 0; i < addr_len && i < 8; i++) {
//         addr_hex[i] = packet[1 + i];
//     }
//     addr_hex[addr_len] = '\0';
    
//     uint32_t host_addr = gdb_hex_to_host32(addr_hex);
    
//     // ÌáÈ¡³¤¶È
//     int len_len = colon - (comma + 1);
//     char len_hex[9] = {0};
//     for (int i = 0; i < len_len && i < 8; i++) {
//         len_hex[i] = comma[1 + i];
//     }
//     len_hex[len_len] = '\0';
    
//     uint32_t len = hex_str_to_uint(len_hex);
    
//     // ÌáÈ¡Êý¾Ý
//     char* data_str = colon + 1;
//     int data_len = gdb_strlen(data_str);
    
//     // ÑéÖ¤Êý¾Ý³¤¶È
//     if (data_len != len * 2) {
//         Diagnose::Write("[GDB] Êý¾Ý³¤¶È²»Æ¥Åä: ÆÚÍû=%u, Êµ¼Ê=%u\n", len * 2, data_len);
//         gdb_send_packet("E03");
//         return;
//     }
    
//     Diagnose::Write("[GDB] ½âÎö: µØÖ·=0x%08x, ³¤¶È=%u\n", host_addr, len);
    
//     // ÏÞÖÆ³¤¶È
//     if (len == 0 || len > DEBUG_BUFFER_SIZE) {
//         len = DEBUG_BUFFER_SIZE;
//     }
    
//     // °²È«¼ì²é
//     if (host_addr < 0xC0000000) {
//         Diagnose::Write("[GDB] ¾Ü¾øÓÃ»§¿Õ¼äÐ´Èë: 0x%08x\n", host_addr);
//         gdb_send_packet("E00");
//         return;
//     }
    
//     // ½âÎöÊ®Áù½øÖÆÊý¾Ý
//     static char data_buffer[DEBUG_BUFFER_SIZE];
//     for (uint32_t i = 0; i < len; i++) {
//         char c1 = data_str[i*2];
//         char c2 = data_str[i*2 + 1];
        
//         uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
//         data_buffer[i] = (char)byte;
//     }
    
//     // Ð´ÈëÄÚ´æ
//     // if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
//     //     gdb_send_packet("E02");
//     //     return;
//     // }
    
//     Diagnose::Write("[GDB] Ð´Èë³É¹¦: µØÖ·=0x%08x\n", host_addr);
//     gdb_send_packet("OK");
// }
void gdb_handle_write_memory(char* packet) {
    // ¸ñÊ½: Maddr,length:data
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
    
    // ÌáÈ¡µØÖ· - ¹Ø¼üÐÞ¸Ä£ºÖ±½Ó½âÎö£¬²»½øÐÐ×Ö½ÚÐò×ª»»
    int addr_len = comma - (packet + 1);
    char addr_hex[9] = {0};
    for (int i = 0; i < addr_len && i < 8; i++) {
        addr_hex[i] = packet[1 + i];
    }
    addr_hex[addr_len] = '\0';
    
    // ¹Ø¼üÐÞ¸Ä£ºÖ±½Ó½âÎöÊ®Áù½øÖÆ×Ö·û´®Îªuint32_t
    uint32_t host_addr = hex_str_to_uint(addr_hex);
    
    // ÌáÈ¡³¤¶È
    int len_len = colon - (comma + 1);
    char len_hex[9] = {0};
    for (int i = 0; i < len_len && i < 8; i++) {
        len_hex[i] = comma[1 + i];
    }
    len_hex[len_len] = '\0';
    
    uint32_t len = hex_str_to_uint(len_hex);
    
    // ÌáÈ¡Êý¾Ý
    char* data_str = colon + 1;
    int data_len = gdb_strlen(data_str);
    
    // ÑéÖ¤Êý¾Ý³¤¶È
    if (data_len != len * 2) {
        gdb_send_packet("E03");
        return;
    }
    
    // ÏÞÖÆ³¤¶È
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        len = DEBUG_BUFFER_SIZE;
    }
    
    // °²È«¼ì²é
    if (host_addr < 0xC0000000) {
        gdb_send_packet("E00");
        return;
    }
    
    // ½âÎöÊ®Áù½øÖÆÊý¾Ý
    static char data_buffer[DEBUG_BUFFER_SIZE];
    for (uint32_t i = 0; i < len; i++) {
        char c1 = data_str[i*2];
        char c2 = data_str[i*2 + 1];
        
        uint8_t byte = (hex_char_to_value(c1) << 4) | hex_char_to_value(c2);
        data_buffer[i] = (char)byte;
    }
    
    // ¹Ø¼üÐÞ¸Ä£ºÈ¡Ïû×¢ÊÍ£¬ÆôÓÃÄÚ´æÐ´Èë
    if (gdb_safe_write_memory(host_addr, data_buffer, len) < 0) {
        gdb_send_packet("E02");
        return;
    }
    
    gdb_send_packet("OK");
}

void gdb_handle_binary_write_memory(char* packet) {
    // ¸ñÊ½: Xaddr,length:binary_data
    char* comma = gdb_strchr(packet, ',');
    char* colon = gdb_strchr(comma, ':');
    
    if (!comma || !colon) {
        gdb_send_packet("E01");  // ¸ñÊ½´íÎó
        return;
    }
    
    // ÌáÈ¡µØÖ·
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
    
    // ÌáÈ¡³¤¶È
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
    
    // ÌáÈ¡¶þ½øÖÆÊý¾Ý
    char* binary_data = colon + 1;
    int data_len = gdb_strlen(binary_data);
    
    // ¹Ø¼üÐÞ¸´£ºÑéÖ¤Êý¾Ý³¤¶È
    if (data_len != len) {
        gdb_send_packet("E03");  // Êý¾Ý³¤¶È²»Æ¥Åä
        return;
    }
    
    // ÏÞÖÆ³¤¶È
    if (len == 0 || len > DEBUG_BUFFER_SIZE) {
        gdb_send_packet("E02");  // ³¤¶È´íÎó
        return;
    }
    
    Diagnose::Write("[GDB] ¶þ½øÖÆÐ´Èë: µØÖ·=0x%08x, ³¤¶È=%u\n", host_addr, len);
    
    // °²È«Ð´Èë
    if (gdb_safe_write_memory(host_addr, binary_data, len) < 0) {
        gdb_send_packet("E04");  // Ð´ÈëÊ§°Ü
        return;
    }
    
    gdb_send_packet("OK");
}


// ´¦ÀíÉèÖÃ¶ÏµãÃüÁî£¨¸ñÊ½£ºZÀàÐÍ,µØÖ·,³¤¶È£©
// void gdb_handle_set_breakpoint(char* p) {
//     if (p[0] != 'Z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // ½âÎöÀàÐÍ
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎöµØÖ·
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

//     // Ìí¼Ó¶Ïµã
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

    // ½âÎöÀàÐÍ
    type = (GDBBreakpointType)(*ptr++ - '0');

    // Ìø¹ý ','
    if (*ptr == ',') ptr++;

    // ¹Ø¼üÐÞ¸Ä£ºÌáÈ¡Ê®Áù½øÖÆ×Ö·û´®£¬Ö±½Ó½âÎö£¨²»½øÐÐ×Ö½ÚÐò×ª»»£©
    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    // Ö±½Ó½âÎöÎªuint32_t£¨²»½øÐÐ×Ö½ÚÐò×ª»»£©
    addr = hex_str_to_uint(addr_hex);
    
    // °²È«¼ì²é£ºÈ·±£ÊÇÄÚºË¿Õ¼äµØÖ·
    if (addr < 0xC0000000) {
        gdb_send_error(0);  // ¾Ü¾øÓÃ»§¿Õ¼ä¶Ïµã
        return;
    }

    // Ìí¼Ó¶Ïµã
    if (gdb_add_breakpoint(addr, type) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

// ´¦ÀíÒÆ³ý¶ÏµãÃüÁî£¨¸ñÊ½£ºzÀàÐÍ,µØÖ·,³¤¶È£©
// void gdb_handle_remove_breakpoint(char* p) {
//     if (p[0] != 'z') {
//         gdb_send_error(0);
//         return;
//     }

//     char* ptr = p + 1;
//     GDBBreakpointType type;
//     uint32_t addr;

//     // ½âÎöÀàÐÍ
//     type = (GDBBreakpointType)(*ptr++ - '0');

//     // Ìø¹ý ','
//     if (*ptr == ',') ptr++;

//     // ½âÎöµØÖ·
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

//     // ÒÆ³ý¶Ïµã
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
    
    // ÌáÈ¡µØÖ·×Ö·û´®
    char addr_hex[9] = {0};
    int addr_len = 0;
    
    while (*ptr != ',' && *ptr != '\0' && addr_len < 8) {
        addr_hex[addr_len++] = *ptr++;
    }
    addr_hex[addr_len] = '\0';
    
    // Ö±½Ó½âÎöµØÖ·£¨²»½øÐÐ×Ö½ÚÐò×ª»»£©
    uint32_t addr = hex_str_to_uint(addr_hex);
    
    // °²È«¼ì²é
    if (addr < 0xC0000000) {
        gdb_send_error(0);
        return;
    }

    // ÒÆ³ý¶Ïµã
    if (gdb_remove_breakpoint(addr) != 0) {
        gdb_send_error(1);
    } else {
        gdb_send_ok();
    }
}

void gdb_handle_query(char* p) {
    char query_msg[DEBUG_BUFFER_SIZE + 100];
    int pos = 0;
    const char* prefix = "[GDB] Query packet: ";
    while (*prefix) query_msg[pos++] = *prefix++;
    int i = 0;
    while (p[i] != '\0' && i < DEBUG_BUFFER_SIZE - 1) {
        query_msg[pos++] = p[i++];
    }
    query_msg[pos++] = '\n';
    query_msg[pos] = '\0';
    Diagnose::Write(query_msg);

    // qSupported - GDB ÌØÐÔ²éÑ¯
    if (strncmp(p, "qSupported", 10) == 0) {
        gdb_send_packet((char*)"PacketSize=1000;qRelocInsn+;multiprocess+;vContSupported+;QStartNoAckMode-;timeout-;qXfer:features:read-;qXfer:threads:read-");
        return;
    }

    // vMustReplyEmpty - ±ØÐë»Ø¸´¿Õ
    if (strcmp(p, "vMustReplyEmpty") == 0) {
        gdb_send_packet((char*)"");
        return;
    }

    // vCont? - ²éÑ¯Ö§³ÖµÄ¼ÌÐøÃüÁî
    if (strcmp(p, "vCont?") == 0) {
        gdb_send_packet((char*)"vCont;c;C;s;S");
        return;
    }

    // qC - ²éÑ¯µ±Ç°Ïß³Ì ID
    if (strcmp(p, "qC") == 0) {
        gdb_send_packet((char*)"QC1");
        return;
    }

    // qAttached - ²éÑ¯ÊÇ·ñattached
    if (strcmp(p, "qAttached") == 0) {
        gdb_send_packet((char*)"1");
        return;
    }

    // qTStatus - Ïß³Ì×´Ì¬²éÑ¯
    if (strcmp(p, "qTStatus") == 0) {
        gdb_send_packet((char*)"T0");  // ²»Ö§³Ö¸ú×Ù
        return;
    }

    // qfThreadInfo - ²éÑ¯µÚÒ»¸öÏß³Ì
    if (strcmp(p, "qfThreadInfo") == 0) {
        gdb_send_packet((char*)"m1");  // µ¥Ïß³ÌÏµÍ³£¬Ö»ÓÐÏß³Ì1
        return;
    }

    // qsThreadInfo - ²éÑ¯ÏÂÒ»¸öÏß³Ì
    if (strcmp(p, "qsThreadInfo") == 0) {
        gdb_send_packet((char*)"l");   // ÁÐ±í½áÊø
        return;
    }

    // QStartNoAckMode - ½ûÓÃACKÄ£Ê½
    if (strcmp(p, "QStartNoAckMode") == 0) {
        gdb_send_packet((char*)"OK");  // È·ÈÏÖ§³Ö
        return;
    }

    if (strcmp(p, "?") == 0) {
        gdb_send_packet("S05");  // ·¢ËÍÍ£Ö¹ÐÅºÅ
        Diagnose::Write("[GDB] ÏìÓ¦ÐÅºÅ²éÑ¯\n");
    }

    // ÆäËû²éÑ¯ÃüÁî·µ»Ø¿Õ
    gdb_send_packet((char*)"");
}

// ´¦ÀíÐÅºÅÃüÁî
void gdb_handle_signal(char* p) {
    // ·¢ËÍÍ£Ö¹ÐÅºÅ (S05 = SIGTRAP)
    gdb_send_packet((char*)"S05");
}


