#ifndef DEBUG_JSON_H
#define DEBUG_JSON_H

#include "../../libyrosstd/sys/types.h"

typedef void (*DebugJsonWriter)(const char* text, void* context);

int debug_json_handle_qxfer(const char* object,
                            const char* annex,
                            uint32_t offset,
                            uint32_t length,
                            char* response,
                            int response_size);

int debug_json_is_monitor_command(const char* cmd);
void debug_json_handle_monitor_command(const char* cmd,
                                       DebugJsonWriter writer,
                                       void* context);

#endif
