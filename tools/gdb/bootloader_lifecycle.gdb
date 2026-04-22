set pagination off
set confirm off
set breakpoint pending on
set architecture i386

file target/objs/boot/boot_debug.elf
add-symbol-file target/objs/kernel.exe

target remote 127.0.0.1:1234

echo Connected to the lifecycle stub.\n
echo Stop sequence:\n
echo   1. bootloader protected-mode entry\n
echo   2. sector2 / greatstart entry (kernel symbols auto-loaded)\n
echo   3. C++ entry checkpoint at main0()\n
echo   4. file-system checkpoint after LoadSuperBlock()\n
echo Use 'c' twice to reach C++, then 'c' once more for the FS checkpoint.\n
