// 璋冭瘯鍣ㄩ泦鎴愭ā鍧� - 杩炴帴鍒板唴鏍�

#include "../debug.h"
#include "../../include/Kernel.h"
#include "../../include/Video.h"

// 璋冭瘯鍣ㄥ惎鍔ㄥ叆鍙�
extern "C" void debug_start(void) {
    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Starting...\n");
    Diagnose::Write("===================================\n\n");

    // 鍒濆鍖栬皟璇曞櫒
    if (debugger_init() != 0) {
        Diagnose::Write("Failed to initialize debugger!\n");
        return;
    }

    // 鍚姩璋冭瘯鍣ㄤ富寰幆
    debugger_main();
}

// 璋冭瘯鍣ㄥ仠姝㈠叆鍙�
extern "C" void debug_stop(void) {
    Diagnose::Write("\n===================================\n");
    Diagnose::Write("V6++ Debugger Stopped\n");
    Diagnose::Write("===================================\n");
}

// 璋冭瘯鍣ㄩ挬瀛� - 浠庡紓甯稿鐞嗙▼搴忚皟鐢�
extern "C" void debug_hook(void) {
    Diagnose::Write("Debug hook triggered\n");
    // TODO: 淇濆瓨涓婁笅鏂�
    // TODO: 妫€鏌ユ柇鐐�
    // TODO: 涓� GDB 閫氫俊
}
