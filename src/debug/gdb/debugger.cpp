// 调试器集成入口，负责把调试框架挂接到内核生命周期中。

#include "../debug.h"
#include "../../include/Kernel.h"
#include "../../include/Video.h"

// 启动调试器。
extern "C" void debug_start(void) {
    static int debugger_started = 0;
    if (debugger_started) {
        return;
    }

    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Starting...\n");
    Diagnose::Write("===================================\n\n");

    // 初始化调试器核心状态。
    if (debugger_init() != 0) {
        Diagnose::Write("Failed to initialize debugger!\n");
        return;
    }

    debugger_started = 1;
}

// 停止调试器。
extern "C" void debug_stop(void) {
    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Stopped\n");
    Diagnose::Write("===================================\n");
}

// 预留的调试钩子，可从异常处理路径触发。
extern "C" void debug_hook(void) {
    Diagnose::Write("Debug hook triggered\n");
    // TODO: 保存上下文。
    // TODO: 检查断点状态。
    // TODO: 与 GDB 进行通信。
}
