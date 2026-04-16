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
	asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
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
		case -1:
		/* irqEmpty path: ignore unhandled vectors */
			break;
		default:
		/* ignore other vectors to avoid reboot loops */
			break;
	}

	/*
	 * Returning with user DS after syscalls/interrupts from ring3.
	 * asmDoIrq only saves general registers, so DS must be restored manually.
	 */
	{
		uint32_t cs = *(((uint32_t *)tf) + 11);
		uint16_t sel = (cs & 0x3) ? USEL(SEG_UDATA) : KSEL(SEG_KDATA);
		asm volatile("movw %%ax, %%ds"::"a"(sel));
		asm volatile("movw %%ax, %%es"::"a"(sel));
		asm volatile("movw %%ax, %%fs"::"a"(sel));
		asm volatile("movw %%ax, %%gs"::"a"(sel));
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	uint16_t data = 0;
	int pos = 0;
	char character = 0;
	static int shiftDown = 0;

	//TODO_ok
	if (code == 0x2a || code == 0x36) { /* shift press */
		shiftDown = 1;
		return;
	} else if (code == 0xaa || code == 0xb6) { /* shift release */
		shiftDown = 0;
		return;
	} else if (code == 0x0e) { /* backspace */
		if (displayCol > 0) {
			displayCol--;
			pos = (80 * displayRow + displayCol) * 2;
			data = ' ' | (0x07 << 8);
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
		}
	} else if (code == 0x1c) { /* enter */
		if (((bufferTail + 1) % MAX_KEYBUFFER_SIZE) != bufferHead) {
			keyBuffer[bufferTail] = '\n';
			bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		}
		displayRow++;
		displayCol = 0;
	} else if (code < 0x81) { /* printable key */
		character = getChar(code);
		if (character != 0) {
			if (character >= 'A' && character <= 'Z') {
				character = character - 'A' + 'a';
			}
			if (shiftDown && character >= 'a' && character <= 'z') {
				character = character - 'a' + 'A';
			}

			data = character | (0x07 << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol++;

			if (((bufferTail + 1) % MAX_KEYBUFFER_SIZE) != bufferHead) {
				keyBuffer[bufferTail] = character;
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
		asm volatile("movw %0, %%es"::"m"(sel));
		asm volatile("movb %%es:(%1), %b0":"=q"(character):"r"(str + i));

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
	volatile int *head = &bufferHead;
	volatile int *tail = &bufferTail;
	volatile uint32_t *buf = keyBuffer;

	while (*head == *tail) {
		/* busy wait */
	}

	tf->eax = buf[*head];
	*head = (*head + 1) % MAX_KEYBUFFER_SIZE;
}

void syscallGetStr(struct TrapFrame *tf){
	/* TODO: 自由实现 */
	int sel = USEL(SEG_UDATA);
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	char ch = 0;
	volatile int *head = &bufferHead;
	volatile int *tail = &bufferTail;
	volatile uint32_t *buf = keyBuffer;

	asm volatile("movw %0, %%es"::"m"(sel));

	if (size <= 0) {
		tf->eax = 0;
		return;
	}

	while (i < size - 1) {
		while (*head == *tail) {
			/* wait for keyboard input */
		}

		ch = buf[*head];
		*head = (*head + 1) % MAX_KEYBUFFER_SIZE;

		if (ch == '\n' || ch == '\r') {
			break;
		}

		asm volatile("movw %0, %%es"::"m"(sel));
		asm volatile("movb %b0, %%es:(%1)"::"q"(ch), "r"(str + i));
		i++;
	}

	ch = '\0';
	asm volatile("movw %0, %%es"::"m"(sel));
	asm volatile("movb %b0, %%es:(%1)"::"q"(ch), "r"(str + i));
	tf->eax = i;
}
