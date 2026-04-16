#include "x86.h"
#include "device.h"

SegDesc gdt[NR_SEGMENTS];       // the new GDT, NR_SEGMENTS=7, defined in x86/memory.h
TSS tss;

void initSeg() { // setup kernel segements
	gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_KERN);
	gdt[SEG_KDATA] = SEG(STA_W,         0,       0xffffffff, DPL_KERN);
	//gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_USER);
	gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0x00200000,0x000fffff, DPL_USER);
	//gdt[SEG_UDATA] = SEG(STA_W,         0,       0xffffffff, DPL_USER);
	gdt[SEG_UDATA] = SEG(STA_W,         0x00200000,0x000fffff, DPL_USER);
	gdt[SEG_TSS] = SEG16(STS_T32A,      &tss, sizeof(TSS)-1, DPL_KERN);
	gdt[SEG_TSS].s = 0;
	setGdt(gdt, sizeof(gdt)); // gdt is set in bootloader, here reset gdt in kernel

	/*
	 * 初始化TSS
	 */
	tss.esp0 = 0x1fffff;
	tss.ss0 = KSEL(SEG_KDATA);
	asm volatile("ltr %%ax":: "a" (KSEL(SEG_TSS)));

	/*设置正确的段寄存器*/
	asm volatile("movw %%ax,%%ds":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%es":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%fs":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%gs":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%ss":: "a" (KSEL(SEG_KDATA)));

	lLdt(0);
	
}

void enterUserSpace(uint32_t entry) {
	/*
	 * Before enter user space 
	 * you should set the right segment registers here
	 * and use 'iret' to jump to ring3
	 */
	uint32_t EFLAGS = 0;
	asm volatile("movw %%ax, %%ds":: "a" (USEL(SEG_UDATA)));
	asm volatile("movw %%ax, %%es":: "a" (USEL(SEG_UDATA)));
	asm volatile("movw %%ax, %%fs":: "a" (USEL(SEG_UDATA)));
	asm volatile("movw %%ax, %%gs":: "a" (USEL(SEG_UDATA)));
	asm volatile("pushl %0":: "r" (USEL(SEG_UDATA))); // push ss
	/* User stack offset in user data segment [0x0, 0x000fffff]. */
	asm volatile("pushl %0":: "r" (0x000ffffc));
	asm volatile("pushfl"); //push eflags, sti
	asm volatile("popl %0":"=r" (EFLAGS));
	asm volatile("pushl %0"::"r"(EFLAGS|0x200));
	asm volatile("pushl %0":: "r" (USEL(SEG_UCODE))); // push cs
	asm volatile("pushl %0":: "r" (entry)); 
	asm volatile("iret");
}

/*
kernel is loaded to location 0x100000, i.e., 1MB
size of kernel is not greater than 200*512 bytes, i.e., 100KB
user program is loaded to location 0x200000, i.e., 2MB
size of user program is not greater than 200*512 bytes, i.e., 100KB
*/

void loadUMain(void) {
	// TODO_ok: 参照bootloader加载内核的方式
	int i = 0;
	int j = 0;
	unsigned int elf = 0x200000;
	uint32_t uMainEntry;
	uint32_t phnum;
	struct ELFHeader *elfHeader;
	struct ProgramHeader *ph;
	struct ProgramHeader phCopy[16];

	for (i = 0; i < 200; i++) {
		readSect((void*)(elf + i * 512), 201 + i);
	}

	elfHeader = (struct ELFHeader *)elf;
	uMainEntry = elfHeader->entry;
	phnum = elfHeader->phnum;

	ph = (struct ProgramHeader *)(elf + elfHeader->phoff);
	if (phnum > 16) {
		phnum = 16;
	}
	for (i = 0; i < (int)phnum; i++) {
		phCopy[i] = ph[i];
	}

	for (i = 0; i < (int)phnum; i++) {
		if (phCopy[i].type != 1) { /* PT_LOAD */
			continue;
		}

		j = 0;
		for (j = 0; j < (int)phCopy[i].filesz; j++) {
			*(unsigned char *)(elf + phCopy[i].vaddr + j) =
				*(unsigned char *)(elf + phCopy[i].off + j);
		}
		for (; j < (int)phCopy[i].memsz; j++) {
			*(unsigned char *)(elf + phCopy[i].vaddr + j) = 0;
		}
	}

	enterUserSpace(uMainEntry);
}
