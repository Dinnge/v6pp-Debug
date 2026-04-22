/* 020302090802060108040304 */

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

// debug02040805
extern "C" void debug_start(void);
extern "C" void debug_stop(void);
extern "C" void debug_hook(void);
// 08¡Â080804¡Â060507030204080507¨´01¡Â
// extern "C" void debugger_enter(void);

bool isInit = false;

extern "C" void MasterIRQ7()
{
	SaveContext();
	
	Diagnose::Write("IRQ7 from Master 8259A!\n");
	
	//04¨¨0609080300040903070708¨ª060004¨°0208020503068259A¡¤040901EOI01¨¹0906
	//080805¨¦¡¤040300050207040103070403000103IOPort::OutByte(0x27, 0x20);090906010809040404¡ì01040904060305¨´0501¡À06080706080209
	//¡¤040901EOI01¨¹0906000302¨®03¨¢070402¨®04030802IRQ700040903050306050501 080008080300080705¨¢01040805IRQ7000303¨¢05¨²07¨²060307020305
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
		/*  (070706080306070706030300090705070503¡À060502Link.ld)
		Link script000404070002010502¨®0501090908070802total0605060205030805constructor08020002080509090501
		_CTOR_LIST__0802080306030002080608090709080406010805global/static090803¨®0802constructor0501
		09¨´060805030701 constructor++; 
		*/
	
	while(constructor != &__CTOR_END__) //total05030805constructor0802080509070501090308050701070304¨¬05090805¡¤0908050909_CTOR_LIST__080202080205
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

// 04¨¹030000040903070708¨ª060004¨° (IRQ1 -> 0004090303¨°0907 0x21)
// void keyboard_interrupt_handler(void) {
//     // 0909060304¨¹030007¡§01¨¨0005
//     uint8_t scancode = inb(0x60);
    
//     // Ctrl+C 080207¡§01¨¨00050805 0x0305¡§ASCII 00050508
//     if (scancode == 0x03) {
//         // 0706¡¤0408¡Â080806¨¬060505010503060508¡Â080804¡Â
//         asm volatile("int $0x01");
//     }
    
//     // ¡¤0409010004090305¨¢0803040302030003 PIC
//     outb(0x20, 0x20);
// }

// // 00¨ª04070805main.cpp03¨°IDT03¨¤010102020406
// void register_interrupt_handler(uint8_t vector, void (*handler)()) {
//     // 01¡§0105Machine08¨¤¡Á0405¨¢00040903070708¨ª060004¨°
//     Machine::Instance().SetInterruptHandler(vector, handler);
    
//     // 03¨°090800¡À05070502¡Á¡ÂIDT05¡§06040104080804¨ª0508
//     // extern IDT g_IDT;  // 06000600IDT090803¨®
//     // g_IDT.SetHandler(vector, handler);
// }


// // 06010804030404¨¹030000040903
// void init_keyboard_interrupt() {
//     // 07¨¨0001 IRQ1 070708¨ª060004¨°
//     register_interrupt_handler(0x21, keyboard_interrupt_handler);
    
//     // 0400070104¨¹030000040903
//     enable_irq(1);  // IRQ1
    
//     Diagnose::Write("[DEBUG] 04¨¹0300000409030605040007010501Ctrl+C 04¨¬050904¡è0306\n");
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

	Chip8253::Init(60);	//06010804030408¡À00070004090304060401
	Chip8259A::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_TIMER);		
	DMA::Init();
	Chip8259A::IrqEnable(Chip8259A::IRQ_IDE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_SLAVE);
	Chip8259A::IrqEnable(Chip8259A::IRQ_KBD);

	/* 04000701 COM1 0703070300040903 (IRQ4) 0608¡À0008030007040408¡À020505090309 Ctrl+C */


	//init gdt
	machine.InitGDT();
	machine.LoadGDT();
	//init idt
	machine.InitIDT();	
	machine.LoadIDT();

	machine.InitPageDirectory();    // 06010804030406060207000403040209040200010606¡À¨ª
	Machine::Instance().InitUserPageTable();     // 060108040304070103¡ì00010606¡À¨ª
	machine.EnablePageProtection();    //07090400¡¤00060602050805
	/* 
	 * InitPageDirectory()0004050003080408080100¡¤0-4M070607010805020708¨ª02030703
	 * 0-4M08050209¡À0500¡è0709¡Á040801060803000009¡À060204080505¨¢0205080207¨²0005090506¡¤000704040503
	 *
	 * 03000803050106050909CS08050203020906010804030405¡Á09020802090205030809¡Á070501040107¨¤09020402070304¡Â06000805boot080107010802090205030809¡Á070501070604010805SS0305
	 * ¡¤0009020806080900030602080203080408080100¡¤0805[0,4M)030507090400¡¤0006060205080502¨®0501060309¡§060907040909090207090401080207060701010103080501¡¤0908¨°050101¡§050301050305
	 * [4M05018M)07090401070103¡ì05030501050307070001¡À0307060701050109¨´060803060709¡Á030501InitUserPageTable(),base000600305
	 */

	//080107010x1009020402070304¡Â
	__asm
		(" \
		mov $0x10, %ax\n\t \
		mov %ax, %ds\n\t \
		mov %ax, %ss\n\t \
		mov %ax, %es\n\t"
		);

	//05000601080403040905090307¨¨000102090xc0400000050109090807040403080909¡¤09¡Á¡ã04080501070400050801070100¨¹02010802¡¤05¡¤¡§
	__asm
		(
		" \
		mov $0xc0400000, %ebp \n\t \
		mov $0xc0400000, %esp \n\t \
		jmp $0x8, $next"
		);
	
	__asm ("ud2");
}

/* 07070701060004¨°0707main¡¤08030105010503060006010009000109090501090906000805runtime()08020107080103050103070409¨¹05010601000302050701exit000900010503060009090305xV601030909000201070205^-^ */
extern "C" void runtime()
{
	/*
	1. 03¨²0302runtime0802stack Frame
	2. esp0004000003¨°070103¡ì09030004argc0203000105010903ebp07040207090506¡¤060108040304
	3. eax00040703¡¤03070700070404060004¨°EntryPoint
	4~6. exit(0)05¨¢080305030600
	*/
	__asm("	leave;	\
			movl %%esp, %%ebp;	\
			call *%%eax;		\
			movl $1, %%eax;	\
			movl $0, %%ebx;	\
			int $0x80"::);
}

/*
  * 1#0503060008030007040401¨ºMoveToUserStack()0707ring0010906020805ring307030306040902¨®050103¨¢08¡Â0701ExecShell()050107090204080501¡§0105"int $0x80"
  * (EAX=execv0308010608¡Â07010203)0407080103¡ã/Shell.exe03¡À060004¨°050104010107020503¨¤08¡À07030803070103¡ì060004¨°0004000704040308010608¡Â0701execv(char* pathname, char* argv[])0305
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
/* 070902040805test0202040604040004080207¨²000503¨¢0605070105010800010509040707060807060605050104050801¡ã0509¨¹07060800*/
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
	

	// 0909000208¡À02¨°0M-4M08020203070307060701060506020503¡À03080107010909050109¨´060806090001040007060701070103¡ì000108020606¡À¨ª05010209070103¡ì0001060004¨°08090404¡Á020201¡Á04¡À00
	Machine::Instance().InitUserPageTable();
	FlushPageDirectory();
	

	Machine::Instance().LoadTaskRegister();
	
	/* 03090603CMOS08¡À05¡ã08¡À0401050107¨¨00010308010608¡À0007 */
	struct SystemTime cTime;
	CMOSTime::ReadCMOSTime(&cTime);
	/* MakeKernelTime()0404090006020203020908¡À040105010707197002¨º1080010609008¡À000908¡À05¡ã080201050805 */
	Time::time = Utility::MakeKernelTime(&cTime);

	/* 0707CMOS000403090603020708¨ª0203070307¨®0403 */
	unsigned short memSize = 0;	/* size in KB */
	unsigned char lowMem, highMem;

	/* 090908070003080505¨¨0701CMOSTime08¨¤00040802ReadCMOSByte0204080509090603CMOS0004020708¨ª0203070307¨®040304030304 */
	lowMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_LOW);
	highMem = CMOSTime::ReadCMOSByte(CMOSTime::EXTENDED_MEMORY_ABOVE_1MB_HIGH);
	memSize = (highMem << 8) + lowMem;

	/* 040707031MB06080300020708¨ª02030703050307¨°050104040900¡Á05020307030606090705010608¡Á00050302090806020308020203070307¨®0403 */
	memSize += 1024; /* KB */
	PageManager::PHY_MEM_SIZE = memSize * 1024;
	UserPageManager::USER_PAGE_POOL_SIZE = PageManager::PHY_MEM_SIZE - UserPageManager::USER_PAGE_POOL_START_ADDR;

	debug_start();
#ifndef EARLY_BOOT_GDB
	__asm__ __volatile__("int3");
#else
	EARLY_BOOT_GDB_CHECKPOINT();
#endif

	/* 090309050502¡Á¡Â030801060203020906010804030400080402	 */
	Kernel::Instance().Initialize();	
	Kernel::Instance().GetProcessManager().SetupProcessZero();
	isInit = true;

	Kernel::Instance().GetFileSystem().LoadSuperBlock();
	Diagnose::Write("Unix V6++ FileSystem Loaded......OK\n");
#ifdef EARLY_BOOT_GDB
	EARLY_BOOT_GDB_CHECKPOINT();
#endif

	// Diagnose::Write("test \n");

    // ========================================
    // 00¨ª040708¡Â080804¡Â0509080807¨²0005 -2253217 DMY
    // ========================================
	// Diagnose::Write("\n[DEBUG] Entering debugger manually...\n");
    // debugger_enter();

	/*  060108040304rootDirInode0201070103¡ì08¡À05¡ã01¡è¡Á¡Â0207000405010608¡À00NameI()0905060501¡è¡Á¡Â */
	FileManager& fileMgr = Kernel::Instance().GetFileManager();

	//fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	fileMgr.rootDirInode = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	fileMgr.rootDirInode->i_flag &= (~Inode::ILOCK);

	User& us = Kernel::Instance().GetUser();
	us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, 1);
	//us.u_cdir = g_InodeTable.IGet(DeviceManager::ROOTDEV, FileSystem::ROOTINO);
	us.u_cdir->i_flag &= (~Inode::ILOCK);
	strcpy(us.u_curdir, "/");

	/* 07¨°0709TTy07¨¨¡À00 */
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

	int pid = Kernel::Instance().GetProcessManager().NewProc();         /* 0#05030600070705¡§1#05030600 */
	if( 0 == pid )     /* 0#0503060000070404Sched()05010607020903080106000407080809080904040803020904020001080202¡§060305030600  */
	{
		us.u_procp->p_ttyp = NULL;
		Kernel::Instance().GetProcessManager().Sched();
	}
	else               /* 1#050306000007040407070701060004¨°shell.exe,0805040901¡§05030600  */
	{
		Machine::Instance().InitUserPageTable();      //0909080500¡À050704070x202,0x2030606¡À¨ª0501010303¨¤090804¨¦0808080100¡¤07060701¡À¨ª060305¨´okay0503
		FlushPageDirectory();

		CRT::ClearScreen();

		/* 1#050306000301070103¡ì0001050100070404exec("shell.exe")0308010608¡Â0701*/
		MoveToUserStack();
		__asm ("call *%%eax" :: "a"((unsigned long)ExecShell - 0xC0000000));   //0609¡¤010208070103¡ì0903050109¨´0608060309¡§06090704070607010503
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
