#include <Windows.h>

#include "defs.h"
#include "lib.h"

static _NtCreateSymbolicLinkObject NtCreateSymbolicLinkObject;

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

BOOL CreateOpLockBlockingW(LPCWSTR FilePath, DWORD AllowedOperation, PVOID Callback, BOOL IsDir)
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
        GENERIC_READ,
        AllowedOperation,
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

    ((void (*)(void))Callback)();

    ret = TRUE;

out:
    if (hEvent)
        CloseHandle(hEvent);
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    return ret;
}