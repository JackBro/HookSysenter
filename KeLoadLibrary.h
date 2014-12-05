#ifndef __KELOADLIBRARY__H__
#define __KELOADLIBRARY__H__

#include <NTDDK.h>

PVOID KeLoadLibrary(PCWSTR lpLibFileName, PVOID OldImageBase);
PVOID KeImageFile(unsigned char* FileBuffer, PVOID OldImageBase);
void FixImportTable(IN PVOID ImageBase);
PVOID GetModuleBase(PCHAR szModuleBase);
PVOID
MiFindExportedRoutineByName (
    IN PVOID DllBase,
    IN PANSI_STRING AnsiImageRoutineName
    );
void FixBaseRelocTable(IN PVOID ImageBase, IN PVOID OldImageBase);
PVOID FixNewKiServiceTable(IN PVOID ImageBase, IN PVOID OldImageBase);

PVOID GetNtOsName(PCHAR szModuleBase);


#define NUMBER_SERVICE_TABLES 2

typedef struct _KSERVICE_TABLE_DESCRIPTOR {
    PULONG Base;					// SSDT (System Service Dispatch Table)�Ļ���ַ
    PULONG Count;						// ���� checked builds, ���� SSDT ��ÿ�����񱻵��õĴ���
    ULONG Limit;						// �������ĸ���, NumberOfService * 4 ����������ַ��Ĵ�С
    PUCHAR Number;						// SSPT(System Service Parameter Table)�Ļ���ַ
} KSERVICE_TABLE_DESCRIPTOR, *PKSERVICE_TABLE_DESCRIPTOR;

//������ ntoskrnl.exe �������� SSDT
extern PKSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTable;
extern KSERVICE_TABLE_DESCRIPTOR NewKeServiceDescriptorTable;

#endif