#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;


void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		/* TODO_ok: 填好中断处理程序的调用 */
		/* 0x0d: general protection fault */
		case 0x0d:
			GProtectFaultHandle(tf);
			break;
		/* 0x21: keyboard interrupt */
		case 0x21:
			KeyboardHandle(tf);
			break;
		/* 0x80: syscall entry */
		case 0x80:
			syscallHandle(tf);
			break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	if(code == 0xe){ // 退格符
		/* TODO_ok: 只能退格当前行的用户输入 */
		if (displayCol > 0) {
			displayCol--;
			int pos = (80 * displayRow + displayCol) * 2;
            asm volatile("movw %0, (%1)"::"r"((uint16_t)0x0720),"r"(pos + 0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		/* TODO_ok: 处理回车 */
		displayRow++;
		displayCol = 0;
	}else if(code < 0x81){ // 正常字符
		/* TODO_ok: 注意大小写切换和不可打印字符处理 */
		char c = getChar(code);

		if (c != 0) {
			int pos = (80 * displayRow + displayCol) * 2;
            uint16_t data = 0x0700 | (uint8_t)c;
            asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
			displayCol++;

			if (((bufferTail + 1) % MAX_KEYBUFFER_SIZE) != bufferHead) {
				keyBuffer[bufferTail] = c;
				bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = USEL(SEG_UDATA); /* TODO_ok: user data selector */
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		
		/* TODO_ok: 完成光标维护和显存输出 */
		if (character == '\n') {
			displayRow++;
			displayCol = 0;
		} else if (character == '\r') {
			displayCol = 0;
		} else {
			data = (uint8_t)character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
			displayCol++;
		}

		if (displayCol >= 80) {
			displayCol = 0;
			displayRow++;
		}
		if (displayRow >= 25) {
			displayRow = 0;
		}
	}
	
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	/* TODO: 自由实现 */
}

void syscallGetStr(struct TrapFrame *tf){
	/* TODO: 自由实现 */
}
