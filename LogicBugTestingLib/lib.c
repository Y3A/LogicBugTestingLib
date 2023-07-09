#include <Windows.h>
#include <TlHelp32.h>

#include "defs.h"
#include "lib.h"

static _NtCreateSymbolicLinkObject NtCreateSymbolicLinkObject;
static _NtSetInformationProcess    NtSetInformationProcess;
static _NtOpenDirectoryObject      NtOpenDirectoryObject;

// Statics

static HANDLE CreateSymLinkInternalW(LPCWSTR ToCreate, LPCWSTR CreateFrom);

static HANDLE CreateSymLinkInternalW(LPCWSTR ToCreate, LPCWSTR CreateFrom)
{
    NTSTATUS            status = STATUS_SUCCESS;
    UNICODE_STRING      uSymLinkName = { 0 }, uSymLinkTarget = { 0 };
    OBJECT_ATTRIBUTES   oa = { 0 };
    HANDLE              ret = NULL;

    if (!NtCreateSymbolicLinkObject)
        NtCreateSymbolicLinkObject = GetProcAddress(GetModuleHandleW(L"ntdll"), "NtCreateSymbolicLinkObject");

    uSymLinkName.Buffer = ToCreate;
    uSymLinkName.Length = wcslen(ToCreate) * sizeof(WCHAR);
    uSymLinkName.MaximumLength = MAX_PATH * sizeof(WCHAR);

    uSymLinkTarget.Buffer = CreateFrom;
    uSymLinkTarget.Length = wcslen(CreateFrom) * sizeof(WCHAR);
    uSymLinkTarget.MaximumLength = MAX_PATH * sizeof(WCHAR);

    InitializeObjectAttributes(&oa, &uSymLinkName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtCreateSymbolicLinkObject(&ret, SYMBOLIC_LINK_ALL_ACCESS, &oa, &uSymLinkTarget);
    if (!NT_SUCCESS(status)) {
        SetLastError(status);
        ret = NULL;
        goto out;
    }

out:
    return ret;
}

// Exports

HANDLE CreateSymLinkW(LPCWSTR ToCreate, LPCWSTR CreateFrom)
{
    return CreateSymLinkInternalW(ToCreate, CreateFrom);
}

HANDLE CreateFakeDeviceMapW(LPCWSTR DrivePath, LPCWSTR FakeDirectory)
{
    HANDLE                      hToken = NULL, hSymLink = NULL;
    TOKEN_STATISTICS            stats = { 0 };
    DWORD                       out = 0;
    WCHAR                       symLinkName[MAX_PATH] = { 0 };
    WCHAR                       fakeDirectory[MAX_PATH] = { 0 };

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken)) {
        hToken = NULL;
        goto out;
    }

    if (!GetTokenInformation(hToken, TokenStatistics, &stats, sizeof(TOKEN_STATISTICS), &out))
        goto out;

    wsprintfW(symLinkName, L"\\Sessions\\0\\DosDevices\\%08x-%08x\\%s", stats.AuthenticationId.HighPart, stats.AuthenticationId.LowPart, DrivePath);
    wsprintfW(fakeDirectory, L"\\GLOBAL??\\%s", FakeDirectory);

    hSymLink = CreateSymLinkInternalW(symLinkName, fakeDirectory);

out:
    if (hToken)
        CloseHandle(hToken);

    return hSymLink;
}

BOOL CreateOpLockBlockingW(LPCWSTR FilePath, OPLOCKCB Callback, BOOL IsDir)
{
    HANDLE                          hEvent = NULL, hFile = NULL;
    OVERLAPPED                      ol = { 0 };
    DWORD                           returned;
    REQUEST_OPLOCK_INPUT_BUFFER     buf = { 0 };
    REQUEST_OPLOCK_OUTPUT_BUFFER    outBuf = { 0 };
    DWORD                           flags = FILE_FLAG_OVERLAPPED;
    BOOL                            ret = FALSE;

    buf.StructureLength = sizeof(REQUEST_OPLOCK_INPUT_BUFFER);
    buf.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
    buf.RequestedOplockLevel = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE;
    buf.Flags = REQUEST_OPLOCK_INPUT_FLAG_REQUEST;

    hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        goto out;

    ol.hEvent = hEvent;

    if (IsDir)
        flags |= (FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);

    hFile = CreateFileW(
        FilePath,
        GENERIC_ALL,
        FILE_SHARE_ALL,
        NULL,
        OPEN_EXISTING,
        flags,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE)
        goto out;

    DeviceIoControl(
        hFile,
        FSCTL_REQUEST_OPLOCK,
        &buf,
        sizeof(REQUEST_OPLOCK_INPUT_BUFFER),
        &outBuf,
        sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER),
        NULL,
        &ol
    );
    if (GetLastError() != ERROR_IO_PENDING)
        goto out;

    if (!GetOverlappedResult(hFile, &ol, &returned, TRUE))
        goto out;

    Callback(hFile);

    ret = TRUE;

out:
    if (hEvent)
        CloseHandle(hEvent);
    if (hFile && hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    return ret;
}

BOOL CreateJunctionW(LPCWSTR VictimDirectory, LPCWSTR TargetDirectory, BOOL Delete)
{
    HANDLE                  hFile = NULL;
    REPARSE_DATA_BUFFER     *buf = NULL;
    BOOL                    ret = FALSE;
    DWORD                   dataLength = 0;
    DWORD                   bytesRet;
    DWORD                   allocLength = 0;

    hFile = CreateFileW(
        VictimDirectory,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE)
        goto out;

    if (Delete)
        allocLength = sizeof(REPARSE_DATA_BUFFER);
    else {
        dataLength = (wcslen(TargetDirectory) + 1) * 2;
        allocLength = REPARSE_DATA_BUFFER_HEADER_LENGTH + dataLength + REPARSE_DATA_BUFFER_UNION_HEADER_SIZE;
    }

    buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocLength);
    if (!buf)
        goto out;
    
    buf->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    
    if (!Delete) {
        buf->ReparseDataLength = dataLength + REPARSE_DATA_BUFFER_UNION_HEADER_SIZE; 
        buf->MountPointReparseBuffer.SubstituteNameOffset = 0;
        buf->MountPointReparseBuffer.SubstituteNameLength = wcslen(TargetDirectory) * 2;
        buf->MountPointReparseBuffer.PrintNameOffset = (wcslen(TargetDirectory) + 1) * 2;
        RtlCopyMemory(&buf->MountPointReparseBuffer.PathBuffer, TargetDirectory, wcslen(TargetDirectory) * 2);
    }

    ret = DeviceIoControl(
        hFile,
        Delete ? FSCTL_DELETE_REPARSE_POINT : FSCTL_SET_REPARSE_POINT,
        buf,
        allocLength,
        NULL,
        0,
        &bytesRet,
        NULL
    );
    if (!ret)
        goto out;

    ret = TRUE;

out:
    if (hFile && hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    if (buf)
        HeapFree(GetProcessHeap(), 0, buf);

    return ret;
}

BOOL MoveFileByHandleW(HANDLE hFile, LPCWSTR NewFile)
{
    FILE_RENAME_INFO* info = NULL;
    DWORD            allocSize = sizeof(FILE_RENAME_INFO) + (wcslen(NewFile) + 1) * sizeof(WCHAR);
    BOOL             ret = FALSE;

    info = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocSize);
    if (!info)
        goto out;

    info->ReplaceIfExists = TRUE;
    info->RootDirectory = NULL;
    info->FileNameLength = wcslen(NewFile) * sizeof(WCHAR);
    RtlCopyMemory(&info->FileName, NewFile, wcslen(NewFile) * sizeof(WCHAR));

    ret = SetFileInformationByHandle(hFile, FileRenameInfo, info, allocSize);

out:
    if (info)
        HeapFree(GetProcessHeap(), 0, info);

    return ret;
}

BOOL IsRedirectionTrustPolicyEnforced(DWORD Pid, DWORD *Enforced)
{
    HANDLE                                          hProc = NULL;
    PROCESS_MITIGATION_REDIRECTION_TRUST_POLICY     buf = { 0 };
    BOOL                                            ret = FALSE;

    hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
    if (!hProc)
        goto out;

    ret = GetProcessMitigationPolicy(hProc, ProcessRedirectionTrustPolicy, &buf, sizeof(buf));
    if (!ret)
        goto out;

    *Enforced = buf.EnforceRedirectionTrust;
    ret = TRUE;

out:
    if (hProc)
        CloseHandle(hProc);

    return ret;
}

BOOL ProbeFileRunCallbackBlockingW(LPCWSTR Directory, PROBEFILECMP Compare, PROBEFILECB Callback)
{
    WIN32_FIND_DATAW    buf = { 0 };
    HANDLE              hFind = INVALID_HANDLE_VALUE;
    BOOL                ret = FALSE, stopLoop = FALSE;

    while (!stopLoop) {
        hFind = FindFirstFileW(Directory, &buf);
        if (hFind == INVALID_HANDLE_VALUE)
            goto out;

        do {
            if (Compare(&buf)) {
                FindClose(hFind); // prevent holding any dir lock
                hFind = INVALID_HANDLE_VALUE;
                Callback(&buf.cFileName);
                ret = TRUE;
                goto out;
            }
        } while (FindNextFileW(hFind, &buf));

        if (GetLastError() != ERROR_NO_MORE_FILES)
            goto out;

        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }

out:
    if (hFind && hFind != INVALID_HANDLE_VALUE)
        FindClose(hFind);

    return ret;
}

BOOL SetAllProcessToGlobalDeviceMap(VOID)
{
    HANDLE                  hSnapShot = NULL, hCurrentProc = NULL;
    HANDLE                  hGlobalDir = NULL;
    PROCESSENTRY32W         procEntry = { sizeof(PROCESSENTRY32W) };
    OBJECT_ATTRIBUTES       oa = { 0 };
    UNICODE_STRING          uGlobalObjectName = { 0 };
    NTSTATUS                status = STATUS_SUCCESS;
    BOOL                    ret = FALSE;

    if (!NtSetInformationProcess)
        NtSetInformationProcess = GetProcAddress(GetModuleHandleW(L"ntdll"), "NtSetInformationProcess");
    if (!NtOpenDirectoryObject)
        NtOpenDirectoryObject = GetProcAddress(GetModuleHandleW(L"ntdll"), "NtOpenDirectoryObject");

    uGlobalObjectName.Buffer = GLOBAL_DIR;
    uGlobalObjectName.Length = wcslen(GLOBAL_DIR) * sizeof(WCHAR);
    uGlobalObjectName.MaximumLength = wcslen(GLOBAL_DIR);
    InitializeObjectAttributes(&oa, &uGlobalObjectName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenDirectoryObject(&hGlobalDir, DIRECTORY_TRAVERSE, &oa);
    if (!NT_SUCCESS(status)) {
        SetLastError(status);
        goto out;
    }

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapShot == INVALID_HANDLE_VALUE)
        goto out;

    while (Process32NextW(hSnapShot, &procEntry)) {
        hCurrentProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procEntry.th32ProcessID);
        if (!hCurrentProc)
            continue;

        status = NtSetInformationProcess(hCurrentProc, ProcessDeviceMap, &hGlobalDir, sizeof(HANDLE));
        if (!NT_SUCCESS(status)) {
            SetLastError(status);
            goto out;
        }

        CloseHandle(hCurrentProc);
        hCurrentProc = NULL;
    }

    if (GetLastError() == ERROR_NO_MORE_FILES) {
        SetLastError(ERROR_SUCCESS);
        ret = TRUE;
    }

out:
    if (hCurrentProc)
        CloseHandle(hCurrentProc);
    if (hSnapShot && hSnapShot != INVALID_HANDLE_VALUE)
        CloseHandle(hSnapShot);
    if (hGlobalDir)
        CloseHandle(hGlobalDir);

    return ret;
}
