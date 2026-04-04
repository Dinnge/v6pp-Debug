#include "SerialInterrupt.h"
#include "Kernel.h"
#include "Regs.h"
#include "IOPort.h"
#include "Chip8259A.h"
#include "../debug/debug.h"
#include "../include/Video.h"

void SerialInterrupt::SerialInterruptEntrance()
{
    SaveContext();
    SwitchToKernel();

    unsigned char ch = IOPort::InByte(0x3F8);
    IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);

    if (ch == 0x03 && debugger_is_target_running()) {
        Diagnose::Write("[DBG] pause request seen in serial IRQ\n");
        asm volatile("int $0x01");
    }

    RestoreContext();
    Leave();
    InterruptReturn();
}
