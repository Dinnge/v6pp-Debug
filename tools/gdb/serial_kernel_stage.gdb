file target/objs/kernel.exe
set pagination off
set confirm off
set architecture i386
set breakpoint pending on

target remote 127.0.0.1:1234

echo Connected to the kernel-entry stub.\n
echo Current stop should be at greatstart / c0100000.\n
echo Set breakpoints in C++ (for example: b main0 or b Kernel::Initialize) and use 'c'.\n
