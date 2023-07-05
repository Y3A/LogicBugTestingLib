# LogicBugTestingLib

WIP tool wrapping small functionalities that help with filesystem logic bug triage.   
Current functionalites:
```c
HANDLE CreateSymLinkW(LPCWSTR ToCreate, LPCWSTR CreateFrom);
// Example: CreateSymLinkW(L"\\??\\RPC Control\\deleteme", L"C:\\Windows\\System32\\ntdll.dll");
// Creates new symlink \RPC Control\deleteme pointing to ntdll
// Error returns NULL handle and sets last error
```
```c
HANDLE CreateFakeDeviceMapW(LPCWSTR DrivePath, LPCWSTR FakeDirectory);
// Example: CreateFakeDeviceMapW(L"C:", L"C:\\Test");
// Overrides the per user dos device symlink of C drive to C:\Test\ directory
// Error returns NULL handle and sets last error
```
```c
BOOL CreateOpLockBlockingW(LPCWSTR FilePath, DWORD AllowedOperation, PVOID Callback, BOOL IsDir);
// Example: CreateOpLockBlockingW(L"C:\\Windows\\Temp\\rollback.cmd", FILE_SHARE_READ, DoAction, FALSE);
// OpLock write and delete operations(allow read) to rollback.cmds and invokes callback upon those operations
// Callback takes a single parameter which is the handle to the oplocked file
// Error returns FALSE and sets last error, Success blocks
```
```c
BOOL CreateJunctionW(LPCWSTR VictimDirectory, LPCWSTR TargetDirectory, BOOL Delete);
// Example: CreateJunctionW(L"C:\\Windows\\Temp\\Backups", L"\\??\\C:\\Windows\\Writable", FALSE);
// Creates junction point from backups to writable
// Note that TargetDirectory must be a full NT Path, starting with \??\
// Set Delete to TRUE to remove junction point
// Error returns FALSE and sets last error
```
```c
BOOL MoveFileByHandleW(HANDLE hFile, LPCWSTR NewFile);
// Example: MoveFileByHandleW(hFile, L"C:\\Users\\User\\desktop\\cpy");
// Renames a file/directory by its handle. Normally used with oplock callback;
// Error returns FALSE and sets last error
```
