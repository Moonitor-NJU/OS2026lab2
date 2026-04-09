#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

static int inputStartRow = 0;
static int inputStartCol = 0;

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
	char c = 0;
	int pos = 0;
	uint16_t data = 0;

	if (code == 0x0e) { /* backspace */
		if (displayRow > inputStartRow ||
			(displayRow == inputStartRow && displayCol > inputStartCol)) {
			displayCol--;
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"((uint16_t)0x0720), "r"(pos + 0xb8000));

			if (bufferHead != bufferTail) {
				bufferTail = (bufferTail - 1 + MAX_KEYBUFFER_SIZE) % MAX_KEYBUFFER_SIZE;
			}
		}
	} else if (code == 0x1c) { /* enter */
		if (((bufferTail + 1) % MAX_KEYBUFFER_SIZE) != bufferHead) {
			keyBuffer[bufferTail] = '\n';
			bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		}

		displayRow++;
		displayCol = 0;
		inputStartRow = displayRow;
		inputStartCol = displayCol;
	} else if (code < 0x81) {
		c = getChar(code);
		if (c != 0) {
			pos = (80 * displayRow + displayCol) * 2;
			data = 0x0700 | (uint8_t)c;
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol++;

			if (((bufferTail + 1) % MAX_KEYBUFFER_SIZE) != bufferHead) {
				keyBuffer[bufferTail] = c;
				bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
			}
		}
	}

	if (displayCol >= 80) {
		displayCol = 0;
		displayRow++;
	}
	if (displayRow >= 25) {
		scrollScreen();
		displayRow = 24;
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
			pos = (80 * displayRow + displayCol) * 2;
			data = (uint8_t)character | (0x0c << 8);
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
		}

		if (displayCol >= 80) {
			displayCol = 0;
			displayRow++;
		}
		if (displayRow >= 25) {
			scrollScreen();
			displayRow = 24;
		}
	}

	inputStartRow = displayRow;
	inputStartCol = displayCol;
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
	// 检查缓冲区是否为空
	while (bufferHead == bufferTail) {
		/* busy wait */
	}

	tf->eax = keyBuffer[bufferHead];
	bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
}

void syscallGetStr(struct TrapFrame *tf){
	/* TODO: 自由实现 */
	int sel = USEL(SEG_UDATA);
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	char ch = 0;

	asm volatile("movw %0, %%es"::"m"(sel));

	if (size <= 0) {
		tf->eax = 0;
		return;
	}

	while (i < size - 1) {
		while (bufferHead == bufferTail) {
			/* busy wait */
		}

		ch = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

		if (ch == '\n' || ch == '\r') {
			break;
		}


		asm volatile("movb %b0, %%es:(%1)"::"q"(ch), "r"(str + i));
		i++;
	}

	ch = '\0';
	asm volatile("movb %b0, %%es:(%1)"::"q"(ch), "r"(str + i));

	tf->eax = i;
}
