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
		// TODO_ok: 填好中断处理程序的调用
		// 0x0d 是通用保护错误 (GP Fault)
        case 0xd:
            GProtectFaultHandle(tf);
            break;
        // 0x21 是我们在 idt.c 和 doIrq.S 中约定的键盘中断
        case 0x21:
            KeyboardHandle(tf);
            break;
        // 0x80 是系统调用入口
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
		// TODO_ok: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if (displayCol > 0) {
                displayCol--;
                uint16_t *vm = (uint16_t *)0xb8000;
                // 用黑底白字的空格抹除
                vm[displayRow * 80 + displayCol] = 0x0720;
            }
	}else if(code == 0x1c){ // 回车符
		// TODO_ok: 处理回车情况
		displayRow++;
        displayCol = 0;
	}else if(code < 0x81){ // 正常字符
		// TODO: 注意输入的大小写的实现、不可打印字符的处理
		
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
	int sel = USEL(SEG_UDATA);//TODO_ok: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		
		// TODO_ok: 完成光标的维护和打印到显存
		if (character == '\n') {
            // 处理换行符：行加 1，列清零
            displayRow++;
            displayCol = 0;
        } 
        else if (character == '\r') {
            // 处理回车符：列清零
            displayCol = 0;
        }
        else {
            // 正常字符打印
            // 构造显存数据：高 8 位是颜色属性，低 8 位是字符 ASCII
            // 0x0700 代表黑底白字 (属性位: 0000 0111)
            data = 0x0700 | (uint8_t)character;

            // 计算显存索引：行号 * 80 + 列号
            int pos = displayRow * 80 + displayCol;
            video_mem[pos] = data;

            // 列坐标右移
            displayCol++;
        }
        // 如果列越界（超过 80），自动换行
        if (displayCol >= 80) {
            displayCol = 0;
            displayRow++;
        }
        // 如果行越界（超过 25），需要处理
        if (displayRow >= 25) {
            // 简单处理：回到第一行（覆盖式打印）
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
	// TODO: 自由实现
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
}
