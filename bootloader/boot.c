#include "boot.h"

#define SECTSIZE 512

/*
void bootMain(void) {
	int i = 0;
	void (*elf)(void);
	elf = (void(*)(void))0x100000; // kernel is loaded to location 0x100000
	for (i = 0; i < 200; i ++) {
		//readSect((void*)elf + i*512, i+1);
		readSect((void*)elf + i*512, i+9);
	}
	elf(); // jumping to the loaded program
}
*/

void bootMain(void) {
	int i = 0;
	//int phoff = 0x34; // gcc版本较高(gcc版本高于gcc4)请注释掉此行
	//int offset = 0x1000; 
	unsigned int elf = 0x100000;
	void (*kMainEntry)(void);
	
	kMainEntry = (void(*)(void))0x100000;

	//读入内核
	for (i = 0; i < 200; i++) {
		readSect((void*)(elf + i*512), 1+i);
	}


	// TODO_ok: 填写kMainEntry、phoff、offset
	// gcc版本较高不需要填写 phoff 和 offset
	// kMainEntry = (void(*)(void))(...elf...); 这里给出一个提示，注意阅读boot.h中关于ELFHeader相关代码
	
	struct ELFHeader *elfHeader = (struct ELFHeader *)elf;

	// 关键：在任何搬移发生前，先保存好入口地址
	kMainEntry = (void(*)(void))(elfHeader->entry);

	struct ProgramHeader *ph = (struct ProgramHeader *)(elf + elfHeader->phoff);
	unsigned int offset = ph->off;
	unsigned int vaddr = ph->vaddr;

	for (i = 0; i < 200 * 512; i++) {
		*(unsigned char *)(vaddr + i) = *(unsigned char *)(elf + i + offset);
	}


	// 此时绝对不要再读取 elfHeader->entry，因为内存已经被覆盖了
	kMainEntry();
}

void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
