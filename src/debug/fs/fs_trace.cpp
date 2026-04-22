#include "fs_trace.h"
#include "../debug.h"
#include "../../include/Video.h"

namespace {

const int kTraceCapacity = 64;
const int kTraceLineSize = 96;

char g_trace_lines[kTraceCapacity][kTraceLineSize];
int g_trace_next = 0;
int g_trace_count = 0;

static int simple_strlen(const char* text) {
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static void append_char(char* dst, int& pos, char ch) {
    if (pos < kTraceLineSize - 1) {
        dst[pos++] = ch;
    }
}

static void append_text(char* dst, int& pos, const char* text) {
    for (int i = 0; text[i] != '\0'; i++) {
        append_char(dst, pos, text[i]);
    }
}

static void append_int(char* dst, int& pos, int value) {
    if (value == 0) {
        append_char(dst, pos, '0');
        return;
    }

    if (value < 0) {
        append_char(dst, pos, '-');
        value = -value;
    }

    char rev[16];
    int rev_pos = 0;
    while (value > 0 && rev_pos < 16) {
        rev[rev_pos++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (rev_pos > 0) {
        append_char(dst, pos, rev[--rev_pos]);
    }
}

static void emit_line(const char* line, FSTraceWriter writer, void* context) {
    if (writer) {
        writer(line, context);
        return;
    }
    Diagnose::Write(line);
}

static void store_line(const char* line) {
    int len = simple_strlen(line);
    if (len >= kTraceLineSize) {
        len = kTraceLineSize - 1;
    }

    for (int i = 0; i < len; i++) {
        g_trace_lines[g_trace_next][i] = line[i];
    }
    g_trace_lines[g_trace_next][len] = '\0';

    g_trace_next = (g_trace_next + 1) % kTraceCapacity;
    if (g_trace_count < kTraceCapacity) {
        g_trace_count++;
    }
}

static void log_values(const char* event, int value_count, int value1, int value2) {
    char line[kTraceLineSize];
    int pos = 0;

    append_text(line, pos, "[fs] ");
    append_text(line, pos, event);

    if (value_count > 0) {
        append_text(line, pos, " (");
        append_int(line, pos, value1);
        if (value_count > 1) {
            append_text(line, pos, ", ");
            append_int(line, pos, value2);
        }
        append_char(line, pos, ')');
    }

    append_char(line, pos, '\n');
    line[pos] = '\0';
    store_line(line);
}

}

void fs_trace_reset() {
    g_trace_next = 0;
    g_trace_count = 0;
    for (int i = 0; i < kTraceCapacity; i++) {
        g_trace_lines[i][0] = '\0';
    }
}

void fs_trace_log0(const char* event) {
    log_values(event, 0, 0, 0);
}

void fs_trace_log1(const char* event, int value) {
    log_values(event, 1, value, 0);
}

void fs_trace_log2(const char* event, int value1, int value2) {
    log_values(event, 2, value1, value2);
}

void fs_trace_dump(FSTraceWriter writer, void* context) {
    emit_line("=== FS Transaction Trace ===\n", writer, context);
    if (g_trace_count == 0) {
        emit_line("(no file-system activity recorded yet)\n", writer, context);
        emit_line("=== End FS Transaction Trace ===\n", writer, context);
        return;
    }

    int start = g_trace_next - g_trace_count;
    while (start < 0) {
        start += kTraceCapacity;
    }

    for (int i = 0; i < g_trace_count; i++) {
        int idx = (start + i) % kTraceCapacity;
        emit_line(g_trace_lines[idx], writer, context);
    }
    emit_line("=== End FS Transaction Trace ===\n", writer, context);
}
