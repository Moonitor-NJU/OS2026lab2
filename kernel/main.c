#include "common.h"
#include "x86.h"
#include "device.h"

void kEntry(void) {

	// Interruption is disabled in bootloader

	initSerial();// initialize serial port
	putChar('y');
	// TODO: 做一系列初始化
	initVga(); // initialize vga device
				// initialize keyboard device
	initIdt();        	// initialize idt
	putChar('1');

	initIntr();			// iniialize 8259a
	putChar('2');

	initSeg();	 		// initialize gdt, tss
	putChar('3');

	initKeyTable();
	
	loadUMain(); // load user program, enter user space
	enableInterrupt();

	while(1);
	assert(0);
}
