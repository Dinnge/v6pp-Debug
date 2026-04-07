file target/objs/boot/boot_debug.elf
set pagination off
set confirm off
set architecture i386
target remote localhost:1234

define reach_sector2
  echo Continuing to sector2...\n
  continue
  symbol-file build/kernel/kernel.exe
  echo sector2 stop reached. Current PC and instructions:\n
  x/5i $pc
  info registers
end

document reach_sector2
Continue from the boot-sector stub into sector2, then load kernel symbols
and print the current PC, nearby instructions, and registers.
end
