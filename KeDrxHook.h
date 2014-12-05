#ifndef __KEDRXHOOK__H__
#define __KEDRXHOOK__H__

#include <ntddk.h>

#pragma pack(push,1)

typedef struct _idtr{
	short IDTLimit;				//�����ж�������������ƣ��������ֽ�
	unsigned int IDTBase;		//�����ж���������Ļ�ַ���������ֽ�
}IDTR,*PIDTR;

typedef struct _IDTENTRY{
	unsigned short LowOffset;
	unsigned short selector;
	unsigned char unused_lo;
	unsigned char segment_type:4;	//0x0E is an interrupt gate 
	unsigned char system_segment_flag:1;
	unsigned char DPL:2;			//descriptor privilege level 
	unsigned char P:1;				/*present*/
	unsigned short HiOffset;
}IDTENTRY,*PIDTENTRY; 

#pragma pack(pop)

void SetDebugPortDrxHook();
void UnDebugPortDrxHook();

ULONG SetInterruptHook(ULONG IntNum, ULONG NewInterruptHandle);
void UnInterruptHook(ULONG IntNum, ULONG OldInterruptHandle);

#define CloseWP() \
		_asm{cli}\
		_asm{mov eax, cr0}\
		_asm{and eax, ~0x10000}\
		_asm{mov cr0, eax}
#define OpenWP() \
		_asm{mov eax, cr0}\
		_asm{or eax, 0x10000}\
		_asm{mov cr0, eax}\
		_asm{sti}

#endif