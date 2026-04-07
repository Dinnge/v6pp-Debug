file target/objs/kernel.exe
set pagination off
set confirm off
set architecture i386
set debug remote 1
set remotelogfile target/gdb-remote.log
target remote localhost:1234
