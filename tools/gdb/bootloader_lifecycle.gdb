set pagination off
set confirm off
set breakpoint pending on
set architecture i386

file target/objs/boot/boot_debug.elf
add-symbol-file target/objs/kernel.exe

target remote 127.0.0.1:1234

echo Connected to the lifecycle stub.\n
echo 当前停机流程：\n
echo   1. bootloader 保护模式调试入口\n
echo   2. sector2 / greatstart 调试入口（此时已加载内核符号）\n
echo   3. C++ 主流程早期检查点：main0()\n
echo   4. 文件系统前检查点：next() 中调用 Kernel::Initialize() 之前\n
echo   5. 文件系统检查点：LoadSuperBlock() 之后\n
echo 使用提示：第一次 c 到 greatstart()，第二次 c 到 main0()，之后继续 c 可进入文件系统阶段。\n
