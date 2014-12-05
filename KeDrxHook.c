#include "KeDrxHook.h"

IDTR gIDTR;
IDTENTRY *gIDTEntry;
ULONG gOldInt01Handle;
ULONG gOldZwCreateFile;

ULONG SetInterruptHook(ULONG IntNum, ULONG NewInterruptHandle)
{
	ULONG OldIntHandle;
	_asm
	{
		sidt gIDTR
	}
	gIDTEntry = (PIDTENTRY)gIDTR.IDTBase;
	
	OldIntHandle = gIDTEntry[IntNum].HiOffset<<16 | gIDTEntry[IntNum].LowOffset;

	CloseWP();
	gIDTEntry[IntNum].LowOffset = (USHORT)NewInterruptHandle;
	gIDTEntry[IntNum].HiOffset = (USHORT)(NewInterruptHandle >> 16);
	OpenWP();

	return OldIntHandle;
}

void UnInterruptHook(ULONG IntNum, ULONG OldInterruptHandle)
{
	CloseWP();
	gIDTEntry[IntNum].LowOffset = (USHORT)OldInterruptHandle;
	gIDTEntry[IntNum].HiOffset = (USHORT)(OldInterruptHandle >> 16);
	OpenWP();
}

__declspec(naked) void NewZwCreateFile()
{
	_asm
	{
		pushad
		pushfd
	}
	KdPrint(("new ZwCreateFile"));
	_asm
	{
		popfd
		popad

		pushfd
		push cs
		push ZwCreateFile
		or DWORD ptr[esp+8], 0x10000
		iretd
	}
}

int g_bExit = 0;
__declspec(naked) void ExecBreakPointKiTrap01()
{
	__asm
	{
		pushfd;
		push eax; 
		mov eax,dr6;
		test eax,0x2000;
		jz NOT_EDIT_DRX;	//�ж�DRX�Ĳ�������ת

		and eax,0xFFFFDFFF;
		mov dr6,eax;		//���DR6�ı�־ 
		cmp g_bExit,0;
		jnz MY_DRV_EXIT;	//����Unload 

		mov eax,[esp+8];	//��ȡ��ջ�е�EIP 
		add eax,3;			//�������ж�DRX�Ĳ���ȫ����3���ֽڵ�
		mov [esp+8],eax;	//�޸�EIP,������ǰָ��,����ʱִ������ָ��
		jmp MY_INT_END; 
		
NOT_EDIT_DRX:
		mov eax,dr6;
		test eax,0x1;
		jz SYS_INT;			//����Dr0�������ж�����ת������ԭISR 

		mov eax,[esp+8];
		cmp eax,ZwCreateFile;//�ж�һ���ǲ���ZwCreateFile�����Ե�ַ 
		jnz SYS_INT; 

		mov eax,offset NewZwCreateFile;
		mov [esp+8],eax;	//�޸Ķ�ջ�е�EIP,ʵ�ַ���ʱ��ת 

MY_INT_END: 
		mov eax,dr7;
		or eax,0x2000;		//�ָ�GDλ 
		mov dr7,eax; 
MY_DRV_EXIT:				//��������UnLoadʱ,���ָ�Dr7 
		pop eax;
		popfd;
		iretd; 
SYS_INT: 
		pop eax;
		popfd; 
		jmp gOldInt01Handle; 
	}
}

PULONG gDebugPort;
ULONG DebugPortValue;
__declspec(naked) void AntiZeroDebugPort()
{
	_asm
	{
		pushad
		pushfd
		
		mov eax, dr7
		and eax, ~3
		mov dr7, eax
		
	}
	if(*gDebugPort == 0)
	{
		*gDebugPort = DebugPortValue;
	}
	else
	{
		DebugPortValue = *gDebugPort;
		//KdPrint(("DebugPort Value:%08X", *gDebugPort));
	}

	_asm
	{
		mov eax, dr7
		or eax, 3
		mov dr7, eax

		popfd
		popad
		ret
	}
}

__declspec(naked) void DataBreakPointKiTrap01()
{
	__asm
	{
		pushfd;
		push eax; 
		mov eax,dr6;
		test eax,0x2000;
		jz NOT_EDIT_DRX;	//�ж�DRX�Ĳ�������ת

		and eax,0xFFFFDFFF;
		mov dr6,eax;		//���DR6�ı�־ 
		cmp g_bExit,0;
		jnz MY_DRV_EXIT;	//����Unload 

		mov eax,[esp+8];	//��ȡ��ջ�е�EIP 
		add eax,3;			//�������ж�DRX�Ĳ���ȫ����3���ֽڵ�
		mov [esp+8],eax;	//�޸�EIP,������ǰָ��,����ʱִ������ָ��
		jmp MY_INT_END; 
		
NOT_EDIT_DRX:
		mov eax,dr6;
		test eax,0x1;
		jz SYS_INT;			//����Dr0�������ж�����ת������ԭISR 

		call AntiZeroDebugPort

MY_INT_END: 
		mov eax,dr7;
		or eax,0x2000;		//�ָ�GDλ 
		mov dr7,eax; 
MY_DRV_EXIT:				//��������UnLoadʱ,���ָ�Dr7 
		pop eax;
		popfd;
		iretd; 
SYS_INT: 
		pop eax;
		popfd; 
		jmp gOldInt01Handle; 
	}
}


#define EPROCESS_ACTIVELIST_OFFSET 0x88		//�������
#define EPROCESS_PID_OFFSET 0x84            //PID��
#define EPROCESS_IMAGENAME_OFFSET 0x174     //ӳ������
#define EPROCESS_FLINK_OFFSET 0x88          //˫�����ǰ��ָ��
#define EPROCESS_BLINK_OFFSET 0x8C          //˫����ĺ���ָ��
#define EPROCESS_OBJECTTABLE_OFFSET 0xC4
ULONG GetProcEPROCESS(const char *szProcName)
{
	ULONG EProcess,FirstEProcess;
	LIST_ENTRY* ActiveProcessLinks;
	ULONG pid,dwCount=0;
	PUCHAR pImage;
	ULONG ObjectTable;
	EProcess = FirstEProcess = (ULONG)PsGetCurrentProcess();
	//pid=*(DWORD*)((char*)EProcess+EPROCESS_PID_OFFSET);
	__try
	{   
		while(EProcess!= 0)   
		{   
			dwCount++;   
			pid= *((ULONG*)(EProcess+EPROCESS_PID_OFFSET));   
			pImage= (PUCHAR)(EProcess+EPROCESS_IMAGENAME_OFFSET);
			ObjectTable = *(PULONG)(EProcess+EPROCESS_OBJECTTABLE_OFFSET);
			if(ObjectTable)
			{
				if(_stricmp(szProcName, (const char *)pImage) == 0)
				{
					KdPrint(("[Pid=%d] Find EProcess=0x%08X %s\n", pid, EProcess, pImage));
					return EProcess;
				}
			}
			ActiveProcessLinks = (LIST_ENTRY*)(EProcess + EPROCESS_FLINK_OFFSET);   
			EProcess = (ULONG)ActiveProcessLinks->Flink - EPROCESS_FLINK_OFFSET;    
			if(EProcess == FirstEProcess)
			{
				break ;  
			}
		}
		KdPrint (( "ProcessCount = %d\n", dwCount )) ;   
	} 
	__except(1)
	{
		KdPrint(("EnumProcessList exception !"));
		return 0;
	}
	return 0;
}

#define EPROCESS_DEBUG_PORT_OFFSET 0xBC
ULONG GetDebugPortAddr(const char *szProcName)
{
	ULONG Process;
	Process = GetProcEPROCESS(szProcName);
	if(!Process)
	{
		return 0;
	}
	return Process+EPROCESS_DEBUG_PORT_OFFSET;
}

void SetExecHardBreakPoint(ULONG DrxNum, ULONG IsGD, ULONG HookAddr)
{

	ULONG ulDr7 = 0x300;									// GE LE
	ulDr7 = ulDr7 | ((ULONG)0x3 << (DrxNum*2));				// G0 L0
	ulDr7 = ulDr7 & ( ~ ((ULONG)0xF0000 << (DrxNum*4)));	// LEN REW Ϊִ�жϵ�
	ulDr7 = ulDr7 | (IsGD << 13);							// GD λ,����Drx�Ĵ���

	switch(DrxNum)
	{
		case 0:
			_asm mov eax, HookAddr
			_asm mov dr0, eax
			break;
		case 1:
			_asm mov eax, HookAddr
			_asm mov dr1, eax
			break;
		case 2:
			_asm mov eax, HookAddr
			_asm mov dr2, eax
			break;
		case 3:
			_asm mov eax, HookAddr
			_asm mov dr3, eax
			break;
		default:
			return ;
	}
	
	__asm
	{
		mov eax, dr7;
		or  eax, ulDr7;			//ҲҪ�޸�dr7:GDλ������DrX������ϵͳ�����������޸�	
		mov dr7, eax;
	}
}

void SetDataHardBreakPoint(ULONG DrxNum, ULONG IsGD, ULONG DataAddr)
{
	ULONG ulDr7 = 0x300;									// GE LE
	ulDr7 = ulDr7 | ((ULONG)0x3 << (DrxNum*2));				// G0 L0
	ulDr7 = ulDr7 | ((ULONG)0xF0000 << (DrxNum*4));			// LEN REW Ϊִ�жϵ�
	ulDr7 = ulDr7 | (IsGD << 13);							// GD λ,����Drx�Ĵ���

	KdPrint(("Data Dr7:%08X", ulDr7));

	switch(DrxNum)
	{
		case 0:
			_asm mov eax, DataAddr
			_asm mov dr0, eax
			break;
		case 1:
			_asm mov eax, DataAddr
			_asm mov dr1, eax
			break;
		case 2:
			_asm mov eax, DataAddr
			_asm mov dr2, eax
			break;
		case 3:
			_asm mov eax, DataAddr
			_asm mov dr3, eax
			break;
		default:
			return ;
	}
	
	__asm
	{
		mov eax, dr7;
		or  eax, ulDr7;			//ҲҪ�޸�dr7:GDλ������DrX������ϵͳ�����������޸�	
		mov dr7, eax;
	}
}

void SetDebugPortDrxHook()
{
	ULONG DebugPortAddr;
	DebugPortAddr = GetDebugPortAddr("client.exe");
	if(!DebugPortAddr)
	{
		return ;
	}

	//gOldZwCreateFile = (ULONG)ZwCreateFile;
	//gOldInt01Handle = SetInterruptHook(1, (ULONG)ExecBreakPointKiTrap01);
	//SetExecHardBreakPoint(0, 1, (ULONG)ZwCreateFile);

	gDebugPort = (PULONG)DebugPortAddr;
	gOldInt01Handle = SetInterruptHook(1, (ULONG)DataBreakPointKiTrap01);
	SetDataHardBreakPoint(0, 1, (ULONG)DebugPortAddr);

	return ;
}

void UnDebugPortDrxHook()
{
	g_bExit = 1;
	_asm xor eax, eax
	_asm mov dr7, eax 
	if(gOldInt01Handle != 0)
	{
		UnInterruptHook(1, gOldInt01Handle);
	}
}