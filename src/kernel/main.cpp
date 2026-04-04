/* ÄÚºËµÄ³õÊ¼»¯ */

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

// debugº¯Êý
extern "C" void debug_start(void);
extern "C" void debug_stop(void);
extern "C" void debug_hook(void);
// µ÷ÊÔÆ÷Èë¿Úº¯ÊýÉùÃ÷
// extern "C" void debugger_enter(void);

bool isInit = false;

extern "C" void MasterIRQ7()
{
	SaveContext();
	
	Diagnose::Write("IRQ7 from Master 8259A!\n");
	
	//ÐèÒªÔÚÖÐ¶Ï´¦Àí³ÌÐòÄ©Î²ÏÈ8259A·¢ËÍEOIÃüÁî
	//ÊµÑé·¢ÏÖ£ºÓÐÃ»ÓÐÏÂÃæIOPort::OutByte(0x27, 0x20);Õâ¾äÔËÐÐÐ§¹û¶¼Ò»Ñù£¬±¾À´ÒÔÎª
	//·¢ËÍEOIÃüÁîÖ®ºó»áÓÐºóÐøµÄIRQ7ÖÐ¶Ï½øÈë£¬ µ«ÊÔÏÂÀ´½á¹ûÊÇIRQ7Ö»»á²úÉúÒ»´Î¡£
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

	
	//constructor++;   
		/*  (¿ÉÒÔÏÈ¿´Ò»ÏÂÁ´½Ó½Å±¾£ºLink.ld)
		Link scriptÖÐÐÞ¸Ä¹ýºó£¬ÕâÀïµÄtotalÒÑ¾­²»ÊÇconstructorµÄ¸öÊýÁË£¬
		_CTOR_LIST__µÄµÚÒ»¸öµ¥Ôª¿ªÊ¼¾ÍÊÇglobal/static¶ÔÏóµÄconstructor£¬
		ËùÒÔ²»ÓÃ constructor++; 
		*/
	
	while(constructor != &__CTOR_END__) //total²»ÊÇconstructorµÄÊýÁ¿£¬¶øÊÇÓÃÓÚ¼ì²âÊÇ·ñµ½ÁË_CTOR_LIST__µÄÄ©Î²
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

// ¼üÅÌÖÐ¶Ï´¦Àí³ÌÐò (IRQ1 -> ÖÐ¶ÏÏòÁ¿ 0x21)
// void keyboard_interrupt_handler(void) {
//     // ¶ÁÈ¡¼üÅÌÉ¨ÃèÂë
//     uint8_t scancode = inb(0x60);
    
//     // Ctrl+C µÄÉ¨ÃèÂëÊÇ 0x03£¨ASCII Âë£©
//     if (scancode == 0x03) {
//         // ´¥·¢µ÷ÊÔÒì³££¬½øÈëµ÷ÊÔÆ÷
//         asm volatile("int $0x01");
//     }
    
//     // ·¢ËÍÖÐ¶Ï½áÊøÐÅºÅ¸ø PIC
//     outb(0x20, 0x20);
// }

// // Ìí¼Óµ½main.cpp»òIDTÏà¹ØÎÄ¼þ
// void register_interrupt_handler(uint8_t vector, void (*handler)()) {
//     // Í¨¹ýMachineÀà×¢²áÖÐ¶Ï´¦Àí³ÌÐò
//     Machine::Instance().SetInterruptHandler(vector, handler);
    
//     // »òÕßÖ±½Ó²Ù×÷IDT£¨Èç¹ûÔÊÐí£©
//     // extern IDT g_IDT;  // È«¾ÖIDT¶ÔÏó
//     // g_IDT.SetHandler(vector, handler);
// }


// // ³õÊ¼»¯¼üÅÌÖÐ¶Ï
// void init_keyboard_interrupt() {
//     // ÉèÖÃ IRQ1 ´¦Àí³ÌÐò
//     register_interrupt_handler(0x21, keyboard_interrupt_handler);
    
//     // ÆôÓÃ¼üÅÌÖÐ¶Ï
//     enable_irq(1);  // IRQ1
    
//     Diagnose::Write("[DEBUG] ¼üÅÌÖÐ¶ÏÒÑÆôÓÃ£¬Ctrl+C ¼ì²â¼¤»î\n");
// }


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

	Chip8253::Init(60);	//³õÊ¼»¯Ê±ÖÓÖÐ¶ÏÐ¾Æ¬
	Chip8259A::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_TIMER);		
	DMA::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_IDE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_SLAVE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_KBD);

	/* ÆôÓÃ COM1 ´®¿ÚÖÐ¶Ï (IRQ4) ÒÔ±ãÔÚÖ´ÐÐÊ±ÄÜ²¶»ñ Ctrl+C */


	//init gdt
	machine.InitGDT();
	machine.LoadGDT();
	//init idt
	machine.InitIDT();	
	machine.LoadIDT();

	machine.InitPageDirectory();    // ³õÊ¼»¯Ò³Ä¿Â¼¡¢ºËÐÄÌ¬Ò³±í
	Machine::Instance().InitUserPageTable();     // ³õÊ¼»¯ÓÃ»§Ì¬Ò³±í
	machine.EnablePageProtection();    //¿ªÆô·ÖÒ³Ä£Ê½
	/* 
	 * InitPageDirectory()ÖÐ½«ÏßÐÔµØÖ·0-4MÓ³Éäµ½ÎïÀíÄÚ´æ
	 * 0-4MÊÇÎª±£Ö¤´Ë×¢ÊÍÒÔÏÂÖÁ±¾º¯Êý½áÎ²µÄ´úÂëÕýÈ·Ö´ÐÐ£¡
	 *
	 * ÏÖÔÚ£¬³ýÁËCSÊÇÄÚºË³õÊ¼»¯½×¶ÎµÄ¶ÎÑ¡Ôñ×Ó£¬ÆäÓà¶Î¼Ä´æÆ÷È«ÊÇbootÊ¹ÓÃµÄ¶ÎÑ¡Ôñ×Ó£¬ÓÈÆäÊÇSS¡£
	 * ·Ö¶Îµ¥Ôª¸ø³öµÄÏßÐÔµØÖ·ÊÇ[0,4M)¡£¿ªÆô·ÖÒ³Ä£Ê½ºó£¬Ò»¶¨ÒªÓÐÕâ¶Î¿Õ¼äµÄÓ³Éä¹ØÏµ£¬·ñÔò£¬Í¨²»¹ý¡£
	 * [4M£¬8M)¿Õ¼äÓÃ»§Çø£¬²»Ó¦¸Ã±»Ó³Éä£¬ËùÒÔÏÈ¿Õ×Å£¬InitUserPageTable(),baseÌî0¡£
	 */

	//Ê¹ÓÃ0x10¶Î¼Ä´æÆ÷
	__asm
		(" \
		mov $0x10, %ax\n\t \
		mov %ax, %ds\n\t \
		mov %ax, %ss\n\t \
		mov %ax, %es\n\t"
		);

	//½«³õÊ¼»¯¶ÑÕ»ÉèÖÃÎª0xc0400000£¬ÕâÀïÆÆ»µÁË·â×°ÐÔ£¬¿¼ÂÇÊ¹ÓÃ¸üºÃµÄ·½·¨
	__asm
		(
		" \
		mov $0xc0400000, %ebp \n\t \
		mov $0xc0400000, %esp \n\t \
		jmp $0x8, $next"
		);
	
	__asm ("ud2");
}

/* Ó¦ÓÃ³ÌÐò´Ómain·µ»Ø£¬½ø³Ì¾ÍÖÕÖ¹ÁË£¬ÕâÈ«ÊÇruntime()µÄ¹¦ÀÍ¡£Ã»ÓÐËü£¬¾ÍÖ»ÄÜÓÃexitÖÕÖ¹½ø³ÌÁË¡£xV6Ã»Õâ¸ö¹¦ÄÜ^-^ */
extern "C" void runtime()
{
	/*
	1. Ïú»ÙruntimeµÄstack Frame
	2. espÖÐÖ¸ÏòÓÃ»§Õ»ÖÐargcÎ»ÖÃ£¬¶øebpÉÐÎ´ÕýÈ·³õÊ¼»¯
	3. eaxÖÐ´æ·Å¿ÉÖ´ÐÐ³ÌÐòEntryPoint
	4~6. exit(0)½áÊø½ø³Ì
	*/
	__asm("	leave;	\
			movl %%esp, %%ebp;	\
			call *%%eax;		\
			movl $1, %%eax;	\
			movl $0, %%ebx;	\
			int $0x80"::);
}

/*
  * 1#½ø³ÌÔÚÖ´ÐÐÍêMoveToUserStack()´Óring0ÍË³öµ½ring3ÓÅÏÈ¼¶ºó£¬»áµ÷ÓÃExecShell()£¬´Ëº¯ÊýÍ¨¹ý"int $0x80"
  * (EAX=execvÏµÍ³µ÷ÓÃºÅ)¼ÓÔØ¡°/Shell.exe¡±³ÌÐò£¬Æä¹¦ÄÜÏàµ±ÓÚÔÚÓÃ»§³ÌÐòÖÐÖ´ÐÐÏµÍ³µ÷ÓÃexecv(char* pathname, char* argv[])¡£
  */
extern "C" void ExecShell()
{
	int argc = 0;
	char* argv = NULL;
	const char* pathname = "/Shell.exe";
	__asm ("int $0x80"::"a"(11/* execv */),"b"(pathname),"c"(argc),"d"(argv));
	return;
}

#if 0
/* ´Ëº¯ÊýtestÎÄ¼þ¼ÐÖÐµÄ´úÂë»áÒýÓÃ£¬µ«Ã²ËÆ¿ÉÒÔÉ¾³ý£¬¼ÇµÃ°ÑËüÉ¾µô*/
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
	

	// Õâ¸öÊ±ºò0M-4MµÄÄÚ´æÓ³ÉäÒÑ¾­²»±»Ê¹ÓÃÁË£¬ËùÒÔÒªÖØÐÂÓ³ÉäÓÃ»§Ì¬µÄÒ³±í£¬ÎªÓÃ»§Ì¬³ÌÐòÔËÐÐ×öºÃ×¼±¸
	Machine::Instance().InitUserPageTable();
	FlushPageDirectory();
	

	Machine::Instance().LoadTaskRegister();
	
	/* »ñÈ¡CMOSµ±Ç°Ê±¼ä£¬ÉèÖÃÏµÍ³Ê±ÖÓ */
	struct SystemTime cTime;
	CMOSTime::ReadCMOSTime(&cTime);
	/* MakeKernelTime()¼ÆËã³öÄÚºËÊ±¼ä£¬´Ó1970Äê1ÔÂ1ÈÕ0Ê±ÖÁµ±Ç°µÄÃëÊý */
	Time::time = Utility::MakeKernelTime(&cTime);

	/* ´ÓCMOSÖÐ»ñÈ¡ÎïÀíÄÚ´æ´óÐ¡ */
	unsigned short memSize = 0;	/* size in KB */
	unsigned char lowMem, highMem;

	/* ÕâÀïÖ»ÊÇ½èÓÃCMOSTimeÀàÖÐµÄReadCMOSByteº¯Êý¶ÁÈ¡CMOSÖÐÎïÀíÄÚ´æ´óÐ¡ÐÅÏ¢ */
	lowMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_LOW);
	highMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_HIGH);
	memSize = (highMem << 8) + lowMem;

	/* ¼ÓÉÏ1MBÒÔÏÂÎïÀíÄÚ´æÇøÓò£¬¼ÆËã×ÜÄÚ´æÈÝÁ¿£¬ÒÔ×Ö½ÚÎªµ¥Î»µÄÄÚ´æ´óÐ¡ */
	memSize += 1024; /* KB */
	PageManager::PHY_MEM_SIZE = memSize * 1024;
	UserPageManager::USER_PAGE_POOL_SIZE = PageManager::PHY_MEM_SIZE - UserPageManager::USER_PAGE_POOL_START_ADDR;

	/* ÕæÕý²Ù×÷ÏµÍ³ÄÚºË³õÊ¼»¯Âß¼­	 */
	Kernel::Instance().Initialize();	
	Kernel::Instance().GetProcessManager().SetupProcessZero();
	isInit = true;

	Kernel::Instance().GetFileSystem().LoadSuperBlock();
	Diagnose::Write("Unix V6++ FileSystem Loaded......OK\n");

	// Diagnose::Write("test \n");

    // ========================================
    // Ìí¼Óµ÷ÊÔÆ÷²âÊÔ´úÂë -2253217 DMY
    // ========================================
	// Diagnose::Write("\n[DEBUG] Entering debugger manually...\n");
    // debugger_enter();

	/*  ³õÊ¼»¯rootDirInodeºÍÓÃ»§µ±Ç°¹¤×÷Ä¿Â¼£¬ÒÔ±ãNameI()Õý³£¹¤×÷ */
	FileManager& fileMgr = Kernel::Instance().GetFileManager();

	//fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	fileMgr.rootDirInode->i_flag &= (~Inode::ILOCK);

	User& us = Kernel::Instance().GetUser();
	us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	//us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	us.u_cdir->i_flag &= (~Inode::ILOCK);
	strcpy(us.u_curdir, "/");

	/* ´ò¿ªTTyÉè±¸ */
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

// #ifdef KERNEL_GDB_AUTOSTART
	debug_start();
// #endif
	
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

	int pid = Kernel::Instance().GetProcessManager().NewProc();         /* 0#½ø³Ì´´½¨1#½ø³Ì */
	if( 0 == pid )     /* 0#½ø³ÌÖ´ÐÐSched()£¬³ÉÎªÏµÍ³ÖÐÓÀÔ¶ÔËÐÐÔÚºËÐÄÌ¬µÄÎ¨Ò»½ø³Ì  */
	{
		us.u_procp->p_ttyp = NULL;
		Kernel::Instance().GetProcessManager().Sched();
	}
	else               /* 1#½ø³ÌÖ´ÐÐÓ¦ÓÃ³ÌÐòshell.exe,ÊÇÆÕÍ¨½ø³Ì  */
	{
		Machine::Instance().InitUserPageTable();      //ÕâÊÇÖ±½ÓÐ´0x202,0x203Ò³±í£¬Ã»Ïà¶ÔÐéÊµµØÖ·Ó³Éä±íÒ»Ñùokay£¡
		FlushPageDirectory();

		CRT::ClearScreen();

		/* 1#½ø³Ì»ØÓÃ»§Ì¬£¬Ö´ÐÐexec("shell.exe")ÏµÍ³µ÷ÓÃ*/
		MoveToUserStack();
		__asm ("call *%%eax" :: "a"((unsigned long)ExecShell - 0xC0000000));   //Òª·ÃÎÊÓÃ»§Õ»£¬ËùÒÔÒ»¶¨ÒªÓÐÓ³Éä£¡
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

