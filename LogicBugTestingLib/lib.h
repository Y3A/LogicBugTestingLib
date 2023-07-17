#pragma once

typedef VOID(*OPLOCKCB)(HANDLE hFile);
typedef BOOL(*PROBEFILECMP)(PWIN32_FIND_DATAW);
typedef VOID(*PROBEFILECB)(LPCWSTR FileName);

HANDLE CreateSymLinkW(LPCWSTR ToCreate, LPCWSTR CreateFrom);
// Example: CreateSymLinkW(L"\\RPC Control\\deleteme", L"C:\\Windows\\System32\\ntdll.dll");
// Creates new symlink \RPC Control\deleteme pointing to ntdll
// Error returns NULL handle and sets last error

HANDLE CreateFakeDeviceMapW(LPCWSTR DrivePath, LPCWSTR FakeDirectory);
// Example: CreateFakeDeviceMapW(L"C:", L"C:\\Test");
// Overrides the per user dos device symlink of C drive to C:\Test\ directory
// Error returns NULL handle and sets last error

BOOL CreateOpLockBlockingW(LPCWSTR FilePath, OPLOCKCB Callback, BOOL IsDir);
// Example: CreateOpLockBlockingW(L"C:\\Windows\\Temp\\rollback.cmd", DoAction, FALSE);
// OpLock all operations to rollback.cmd and invokes callback upon any operation
// Callback takes a single parameter which is the handle to the oplocked file
// Error returns FALSE and sets last error, Success blocks

BOOL CreateJunctionW(LPCWSTR VictimDirectory, LPCWSTR TargetDirectory, BOOL Delete);
// Example: CreateJunctionW(L"C:\\Windows\\Temp\\Backups", L"\\??\\C:\\Windows\\Writable", FALSE);
// Creates junction point from backups to writable
// Note that TargetDirectory must be a full NT Path, starting with \??\
// Set Delete to TRUE to remove junction point
// Error returns FALSE and sets last error

BOOL MoveFileByHandleW(HANDLE hFile, LPCWSTR NewFile);
// Example: MoveFileByHandleW(hFile, L"C:\\Users\\User\\desktop\\cpy");
// Renames a file/directory by its handle. Normally used with oplock callback;
// Error returns FALSE and sets last error

BOOL IsRedirectionTrustPolicyEnforced(DWORD Pid, DWORD *Enforced);
// Example: IsRedirectionTrustPolicyEnforced(1, &isEnforced);
// Checks if the redirection trust policy(disability to follow filesystem junctions created by non-admin users) is enforced
// Error returns FALSE and sets last error, Success returned TRUE and result via the <Enforced> argument

BOOL ProbeFileRunCallbackBlockingW(LPCWSTR Directory, PROBEFILECMP Compare, PROBEFILECB Callback);
// Example: ProbeFileRunCallbackBlockingW(L"C:\\Windows\\Temp\\InstallDir\\*", Compare, DoAction);
// Note: the directory has to end with "\\*"
// Infinite loop running Compare on all files in directory until it returns TRUE, then run Callback
// Compare takes a pointer to a WIN32_FIND_DATAW structure, Callback takes the filename
// Error returns FALSE and sets last error, Success blocks

BOOL SetAllProcessToGlobalDeviceMap(VOID);
// Example: SetAllProcessToGlobalDeviceMap();
// Sets all process the user has access to(therefore sees the user's device map) to use the \\GLOBAL?? directory as default process device map
// In the case we want to override the per user device map, running processes like explorer.exe can still function normally
// Error returns FALSE and sets last error
