// 调试器集成模块 - 连接到内核

#include "../debug.h"
#include "../../include/Kernel.h"
#include "../../include/Video.h"

// 调试器启动入口
extern "C" void debug_start(void) {
    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Starting...\n");
    Diagnose::Write("===================================\n\n");

    // 初始化调试器
    if (debugger_init() != 0) {
        Diagnose::Write("Failed to initialize debugger!\n");
        return;
    }

    // 启动调试器主循环
    debugger_main();
}

// 调试器停止入口
extern "C" void debug_stop(void) {
    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Stopped\n");
    Diagnose::Write("===================================\n");
}

// 调试器钩子 - 从异常处理程序调用
extern "C" void debug_hook(void) {
    Diagnose::Write("Debug hook triggered\n");
    // TODO: 保存上下文
    // TODO: 检查断点
    // TODO: 与 GDB 通信
}
