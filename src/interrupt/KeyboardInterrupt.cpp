#include "KeyboardInterrupt.h"
#include "Kernel.h"
#include "Regs.h"
#include "Keyboard.h"
#include "IOPort.h"
#include "Chip8259A.h"

void KeyboardInterrupt::KeyboardInterruptEntrance()
{
	// 1. 读取键盘扫描码
    // unsigned char scancode = IOPort::InByte(0x60);
    
    // // 2. 检测Ctrl+C（扫描码0x03）
    // if (scancode == 0x03) {  // Ctrl+C
    //     // 3. 触发调试异常（INT 1）
    //     asm volatile("int $0x01");
    // }
	SaveContext();			/* 保存中断现场 */

	SwitchToKernel();		/* 进入核心态 */

	CallHandler(Keyboard, KeyboardHandler);		/* 调用键盘中断设备处理子程序 */

	/* 对主8259A中断控制芯片发送EOI命令。 */
	IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);

	/* 获取由中断隐指令(即硬件实施)压入核心栈的pt_context。
	* 这样就可以访问context.xcs中的OLD_CPL，判断先前态
	* 是用户态还是核心态。
	*/
	struct pt_context *context;
	__asm__ __volatile__ ("	movl %%ebp, %0; addl $0x4, %0 " : "+m" (context) );

	if( context->xcs & USER_MODE ) /*先前为用户态*/
	{
		while(true)
		{
			X86Assembly::CLI();	/* 处理机优先级升为7级 */
			
			if(Kernel::Instance().GetProcessManager().RunRun > 0)
			{
				X86Assembly::STI();	/* 处理机优先级降为0级 */
				Kernel::Instance().GetProcessManager().Swtch();
			}
			else
			{
				break;	/* 如果runrun == 0，则退栈回到用户态继续用户程序的执行 */
			}
		}
	}
	
	RestoreContext();		/* 恢复现场 */

	Leave();				/* 手工销毁栈帧 */

	InterruptReturn();		/* 退出中断 */
}
// void KeyboardInterrupt::KeyboardInterruptEntrance()
// {
//     SaveContext();          /* 保存中断现场 */
//     SwitchToKernel();       /* 进入核心态 */
    
//     // 读取键盘扫描码（在保存上下文之后！）
//     unsigned char scancode = IOPort::InByte(0x60);
    
//     // 调试输出扫描码
//     // Diagnose::Write("[KEYBOARD] 扫描码: 0x%02x\n", scancode);
    
//     // Ctrl+C检测（需要跟踪状态）
//     static bool ctrl_pressed = false;
    
//     if (scancode < 0x80) {  // 按键按下
//         if (scancode == 0x1D) {  // Ctrl键扫描码是0x1D
//             ctrl_pressed = true;
//         } 
//         else if (scancode == 0x2E && ctrl_pressed) {  // C键扫描码是0x2E
//             // Ctrl+C组合键！
//             ctrl_pressed = false;
            
//             // 发送EOI
//             IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);
            
//             // 触发调试异常（INT 1）
//             asm volatile("int $0x01");
            
//             // 恢复现场并返回（不继续执行原有键盘处理）
//             RestoreContext();
//             Leave();
//             InterruptReturn();
//             return;  // 重要：直接返回！
//         }
//         else if (scancode != 0x1D) {
//             // 其他键按下，重置Ctrl状态
//             ctrl_pressed = false;
//         }
//     } else {
//         // 按键释放
//         unsigned char release_code = scancode - 0x80;
//         if (release_code == 0x1D) {  // Ctrl键释放
//             ctrl_pressed = false;
//         }
//     }
    
//     // 原有的键盘处理
//     CallHandler(Keyboard, KeyboardHandler);
    
//     /* 对主8259A中断控制芯片发送EOI命令。 */
//     IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);
    
//     /* 获取由中断隐指令(即硬件实施)压入核心栈的pt_context。*/
//     struct pt_context *context;
//     __asm__ __volatile__ ("	movl %%ebp, %0; addl $0x4, %0 " : "+m" (context) );
    
//     if( context->xcs & USER_MODE ) /*先前为用户态*/
//     {
//         while(true)
//         {
//             X86Assembly::CLI();	/* 处理机优先级升为7级 */
            
//             if(Kernel::Instance().GetProcessManager().RunRun > 0)
//             {
//                 X86Assembly::STI();	/* 处理机优先级降为0级 */
//                 Kernel::Instance().GetProcessManager().Swtch();
//             }
//             else
//             {
//                 break;	/* 如果runrun == 0，则退栈回到用户态继续用户程序的执行 */
//             }
//         }
//     }
    
//     RestoreContext();		/* 恢复现场 */
//     Leave();				/* 手工销毁栈帧 */
//     InterruptReturn();		/* 退出中断 */
// }
