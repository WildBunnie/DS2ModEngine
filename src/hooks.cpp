#include "hooks.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <filesystem>

#include <MinHook.h>

#ifdef _M_IX86
typedef struct {
    wchar_t* string;
    void* unk;
    uint32_t unknown1;
    uint32_t unknown2;
    uint32_t length;
    uint32_t capacity;
} DLString;
#elif defined(_M_X64)
typedef struct {
    wchar_t* string;
    void* unk;
    uint64_t length;
    uint64_t capacity;
} DLString;
#endif

typedef INT(__stdcall* getaddrinfo_t)(PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA* pHints, PADDRINFOA* ppResult);
typedef size_t(__thiscall* virtual_to_archive_path_t)(uintptr_t, DLString*);
typedef HANDLE(__stdcall* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

getaddrinfo_t original_getaddrinfo;
virtual_to_archive_path_t original_virtual_to_archive_path;
CreateFileW_t original_CreateFileW;

wchar_t* override_dir = NULL;

INT __stdcall detour_getaddrinfo(PCSTR address, PCSTR port, const ADDRINFOA* pHints, PADDRINFOA* ppResult)
{
#ifdef _M_IX86
    const char* address_to_block = "frpg2-steam-ope.fromsoftware.jp";
#elif defined(_M_X64)
    const char* address_to_block = "frpg2-steam64-ope-login.fromsoftware-game.net";
#endif

    if (address && strcmp(address, address_to_block) == 0) {
        return EAI_FAIL;
    }

    return original_getaddrinfo(address, port, pHints, ppResult);
}

bool force_offline()
{
    LPVOID target = nullptr;
    if (MH_CreateHookApiEx(L"ws2_32", "getaddrinfo", &detour_getaddrinfo, (LPVOID*)&original_getaddrinfo, &target) != MH_OK)
        return false;

    if (MH_EnableHook(target) != MH_OK)
        return false;

    return true;
}

#ifdef _M_IX86
size_t __fastcall detour_virtual_to_archive_path(uintptr_t param_1, void* _edx, DLString* path)
#elif defined(_M_X64)
size_t __cdecl detour_virtual_to_archive_path(uintptr_t param_1, DLString* path)
#endif
{
    if (path && path->string && path->length > 10)
    {
        if (wcsncmp(path->string, L"gamedata:", 9) == 0)
        {
            std::wstring_view rest_of_path(path->string + 10, path->length - 10);
            std::filesystem::path override_path = std::filesystem::path(override_dir) / std::wstring(rest_of_path);

            if (std::filesystem::exists(override_path))
            {
                path->string[0] = L'.';
                path->string[1] = L'/';
                path->string[2] = L'/';
                path->string[3] = L'/';
                path->string[4] = L'/';
                path->string[5] = L'/';
                path->string[6] = L'/';
                path->string[7] = L'/';
                path->string[8] = L'/';
                path->string[9] = L'/';
            }
        }
    }

    return original_virtual_to_archive_path(param_1, path);
}

HANDLE __stdcall detour_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    std::filesystem::path override_path;

    if (lpFileName)
    {
        wchar_t current_dir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, current_dir);

        size_t curr_dir_len = wcslen(current_dir);
        if (wcsncmp(lpFileName, current_dir, curr_dir_len) == 0)
        {
            const wchar_t* relative_path = lpFileName + curr_dir_len;
            if (*relative_path == L'\\' || *relative_path == L'/') relative_path++;

            std::filesystem::path override_path = std::filesystem::path(current_dir) / override_dir / relative_path;

            if (std::filesystem::exists(override_path))
            {
                LPCWSTR path = override_path.c_str();
                return original_CreateFileW(path, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
            }
        }
    }

    return original_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

bool setup_asset_override_hooks(wchar_t* dir)
{
    override_dir = dir;

    uintptr_t base_address = (uintptr_t)GetModuleHandleA("DarkSoulsII.exe");

#ifdef _M_IX86
    uintptr_t address = base_address + 0x57EB70;
#elif defined(_M_X64)
    uintptr_t address = base_address + 0x89C5A0;
#endif

    if (MH_CreateHook((LPVOID)address, &detour_virtual_to_archive_path, (LPVOID*)(&original_virtual_to_archive_path)) != MH_OK)
        return false;

    if (MH_EnableHook((LPVOID)address) != MH_OK)
        return false;

    LPVOID target = nullptr;
    if (MH_CreateHookApiEx(L"kernel32", "CreateFileW", &detour_CreateFileW, (LPVOID*)&original_CreateFileW, &target) != MH_OK)
        return false;

    if (MH_EnableHook(target) != MH_OK)
        return false;

    return true;
}