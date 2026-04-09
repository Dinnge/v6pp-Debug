file target/objs/boot/boot_debug.elf
set pagination off
set confirm off
set architecture i386
target remote localhost:1234

define reach_sector2
  echo Continuing to sector2...\n
  continue
  echo Reconnecting to sector2 stub to refresh remote capabilities...\n
  disconnect
  target remote localhost:1234
  echo sector2 stop reached. Current PC and instructions:\n
  x/5i $pc
  info registers
end

document reach_sector2
Continue from boot.asm stub into sector2, then reconnect GDB so packet
capabilities are re-negotiated against sector2 stub before further commands.
end
