#ifndef FS_TRACE_H
#define FS_TRACE_H

typedef void (*FSTraceWriter)(const char* text, void* context);

void fs_trace_reset();
void fs_trace_log0(const char* event);
void fs_trace_log1(const char* event, int value);
void fs_trace_log2(const char* event, int value1, int value2);
void fs_trace_dump(FSTraceWriter writer, void* context);

#endif
