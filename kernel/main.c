#include "common.h"
#include "x86.h"
#include "device.h"

void kEntry(void) {

	// Interruption is disabled in bootloader

	initSerial();// initialize serial port
	// TODO: 做一系列初始化
	initVga(); // initialize vga device
				// initialize keyboard device
	initIdt();        	// initialize idt

	initIntr();			// iniialize 8259a

	initSeg();	 		// initialize gdt, tss

	initKeyTable();
	
	loadUMain(); // load user program, enter user space
	enableInterrupt();

	while(1);
	assert(0);
}
