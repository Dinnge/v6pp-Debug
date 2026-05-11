/* 内核启动入口与早期初始化逻辑。 */

#include "Video.h"
#include "Simple.h"
#include "IOPort.h"
#include "Chip8253.h"
#include "Chip8259A.h"
#include "Machine.h"
#include "IDT.h"
#include "Assembly.h"
#include "Kernel.h"
#include "TaskStateSegment.h"

#include "PageDirectory.h"
#include "PageTable.h"
#include "SystemCall.h"

#include "Exception.h"
#include "DMA.h"
#include "CRT.h"
#include "TimeInterrupt.h"
#include "PEParser.h"
#include "CMOSTime.h"
#include "./Lib.h"

#include "vesa/svga.h"
#include "vesa/console.h"

#include "libyrosstd/sys/types.h"
#include "libyrosstd/string.h"
#include "../debug/debug.h"

// 调试器入口。
extern "C" void debug_start(void);
extern "C" void debug_stop(void);
extern "C" void debug_hook(void);
// extern "C" void debugger_enter(void);

bool isInit = false;

extern "C" void MasterIRQ7()
{
	SaveContext();
	
	Diagnose::Write("IRQ7 from Master 8259A!\n");
	
	// IRQ7 可能是主片 8259A 的伪中断，这里只发送 EOI 后返回。
	IOPort::OutByte(Chip8259A::MASTER_IO_PORT_1, Chip8259A::EOI);

	RestoreContext();

	Leave();

	InterruptReturn();
}


static void callCtors()
{
	extern void (*__CTOR_LIST__)();
	extern void (* __CTOR_END__)();
	
	
	void (**constructor)() = &__CTOR_LIST__;

	
	while(constructor != &__CTOR_END__) // 依次调用链接脚本收集到的全局/静态构造函数。
	{
		(*constructor)();
		constructor++;
	}
}

static void initBss() {  // https://github.com/FlowerBlackG/YurongOS/blob/master/src/misc/main.cpp
	extern unsigned int __BSS_START__;
    extern unsigned int __BSS_END__;


    unsigned int bssStart = (unsigned int) &__BSS_START__;
    unsigned int bssEnd = (unsigned int) &__BSS_END__;

    for (unsigned int pos = bssStart; pos < bssEnd; pos++) {
        * ((char*) pos) = 0;
    }
}

static void callDtors()
{
	extern void (* __DTOR_LIST__)();
	extern void (* __DTOR_END__)();
	
	void (**deconstructor)() = &__DTOR_LIST__;
	
	while(deconstructor != &__DTOR_END__)
	{
		(*deconstructor)();
		++deconstructor;
	}
}


void main0(void)
{
	Machine& machine = Machine::Instance();

#ifdef EARLY_BOOT_GDB
	/*
	 * 在刚进入 C++ 内核主流程时先切到完整调试器，再继续后续初始化。
	 * 这样从 sector2 继续执行后，可以先落到稳定的 C++ 检查点，
	 * 再设置高级语言断点，避免跨阶段直接命中用户断点时的早期恢复问题。
	 */
	debug_start();
	EARLY_BOOT_GDB_CHECKPOINT();
#endif

	Chip8253::Init(60);	// 初始化 PIT 定时器。
	Chip8259A::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_TIMER);		
	DMA::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_IDE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_SLAVE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_KBD);

	/* 启用 COM1 中断，支持串口调试中的 Ctrl+C 打断。 */

	//init gdt
	machine.InitGDT();
	machine.LoadGDT();
	//init idt
	machine.InitIDT();	
	machine.LoadIDT();

	machine.InitPageDirectory();    // 建立内核页目录。
	Machine::Instance().InitUserPageTable();     // 建立用户态页表模板。
	machine.EnablePageProtection();    // 开启分页保护。

	// 重新装载内核数据段寄存器。
	__asm
		(" \
		mov $0x10, %ax\n\t \
		mov %ax, %ds\n\t \
		mov %ax, %ss\n\t \
		mov %ax, %es\n\t"
		);

	// 切换到内核高地址栈并跳转到高地址代码段继续执行。
	__asm
		(
		" \
		mov $0xc0400000, %ebp \n\t \
		mov $0xc0400000, %esp \n\t \
		jmp $0x8, $next"
		);
	
	__asm ("ud2");
}

/* 用户态运行时跳板：调用入口点，返回后执行 exit(0)。 */
extern "C" void runtime()
{
	__asm("	leave;	\
			movl %%esp, %%ebp;	\
			call *%%eax;		\
			movl $1, %%eax;	\
			movl $0, %%ebx;	\
			int $0x80"::);
}

/* 通过 execv 系统调用启动首个用户态 Shell。 */
extern "C" void ExecShell()
{
	int argc = 0;
	char* argv = NULL;
	const char* pathname = "/Shell.exe";
	__asm ("int $0x80"::"a"(11/* execv */),"b"(pathname),"c"(argc),"d"(argv));
	return;
}

#if 0
/* 简单延时测试函数。 */
extern "C" void Delay()
{
	for ( int i = 0; i < 50; i++ )
		for ( int j = 0; j < 10000; j++ )
		{
			int a;
			int b;
			int c=a+b;
			c++;
		}
}
#endif


int splash();

extern "C" void next()
{
	// if (debugger_init() != 0) {
    // Diagnose::Write("Failed to initialize debugger!\n");
	// } else {
	// 	Diagnose::Write("Debugger ready for GDB connection...\n");
	// }
	
#ifdef USE_VESA
	    intptr_t vesaModeInfoAddr = Machine::KERNEL_SPACE_START_ADDRESS + 0x7e00;
		auto& vesaModeInfo = * (video::svga::VbeModeInfo*) vesaModeInfoAddr;
		video::svga::init(&vesaModeInfo);

		Machine::Instance().InitVESAMemoryMap(
			vesaModeInfo.framebuffer,
			video::svga::VESA_SCREEN_VADDR,
			video::svga::bytesPerPixel * vesaModeInfo.height * vesaModeInfo.width
		);

		video::console::init();
		video::console::writeOutput("VESA enabled.\n", -1, 0xfeba07);
	
#endif
	

	// 刷新用户态页表，确保低地址映射可用。
	Machine::Instance().InitUserPageTable();
	FlushPageDirectory();
	

	Machine::Instance().LoadTaskRegister();
	
	/* 读取 CMOS 时间并初始化内核时钟。 */
	struct SystemTime cTime;
	CMOSTime::ReadCMOSTime(&cTime);
	Time::time = Utility::MakeKernelTime(&cTime);

	/* 读取扩展内存大小。 */
	unsigned short memSize = 0;	/* size in KB */
	unsigned char lowMem, highMem;

	lowMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_LOW);
	highMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_HIGH);
	memSize = (highMem << 8) + lowMem;

	/* 计入 1MB 以下常规内存后更新物理内存总量。 */
	memSize += 1024; /* KB */
	PageManager::PHY_MEM_SIZE = memSize * 1024;
	UserPageManager::USER_PAGE_POOL_SIZE = PageManager::PHY_MEM_SIZE - UserPageManager::USER_PAGE_POOL_START_ADDR;

	debug_start();
#ifndef EARLY_BOOT_GDB
	__asm__ __volatile__("int3");
#else
	EARLY_BOOT_GDB_CHECKPOINT();
#endif

	/* 初始化各子系统并创建 0 号进程。 */
	Kernel::Instance().Initialize();	
	Kernel::Instance().GetProcessManager().SetupProcessZero();
	isInit = true;

	Kernel::Instance().GetFileSystem().LoadSuperBlock();
	Diagnose::Write("Unix V6++ FileSystem Loaded......OK\n");
#ifdef EARLY_BOOT_GDB
	EARLY_BOOT_GDB_CHECKPOINT();
#endif

	// Diagnose::Write("test \n");

    // 预留的手动断入调试器入口。
	// Diagnose::Write("\n[DEBUG] Entering debugger manually...\n");
    // debugger_enter();

	/* 初始化根目录 inode 与当前工作目录。 */
	FileManager& fileMgr = Kernel::Instance().GetFileManager();

	//fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	fileMgr.rootDirInode->i_flag &= (~Inode::ILOCK);

	User& us = Kernel::Instance().GetUser();
	us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	//us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	us.u_cdir->i_flag &= (~Inode::ILOCK);
	strcpy(us.u_curdir, "/");

	/* 打开控制台 TTY，建立标准输入输出。 */
	int fd_tty = lib_open("/dev/tty1", File::FREAD);

	if ( fd_tty != 0 )
	{
		Utility::Panic("STDIN Error!");
	}
	fd_tty = lib_open("/dev/tty1", File::FWRITE);
	if ( fd_tty != 1 )
	{
		Utility::Panic("STDOUT Error!");
	}
	Diagnose::TraceOn();

#ifdef ENABLE_SPLASH
	// show splash.
	splash();
#endif

	unsigned char* runtimeSrc = (unsigned char*)runtime;
	unsigned char* runtimeDst = 0x00000000;
	for (unsigned int i = 0; i < (unsigned long)ExecShell - (unsigned long)runtime; i++)
	{
		*runtimeDst++ = *runtimeSrc++;
	}

    //us.u_MemoryDescriptor.Release();

	int pid = Kernel::Instance().GetProcessManager().NewProc();         /* 创建 1 号进程。 */
	if( 0 == pid )     /* 子进程先进入调度器。 */
	{
		us.u_procp->p_ttyp = NULL;
		Kernel::Instance().GetProcessManager().Sched();
	}
	else               /* 父进程负责准备并启动 shell.exe。 */
	{
		Machine::Instance().InitUserPageTable();      // 重新准备用户态页表。
		FlushPageDirectory();

		CRT::ClearScreen();

		/* 切换到用户栈后执行 ExecShell。 */
		MoveToUserStack();
		__asm ("call *%%eax" :: "a"((unsigned long)ExecShell - 0xC0000000));   // 以用户态虚拟地址调用。
	}
}


extern "C" void kernelBridge() {  // called by sector2.asm
	initBss();

// #ifdef EARLY_BOOT_GDB
// 	Diagnose::Write("[BOOTDBG] Early kernelBridge debugger handoff.\n");
// 	debug_start();
// #endif

	callCtors();
	main0();
	callDtors();
}
