#include "patches.h"

#include "vector"

#include "windows.h"
#include <iostream>

void patch_memory(uintptr_t address, const std::vector<uint8_t>& bytes)
{
    DWORD old_protect;
    VirtualProtect(reinterpret_cast<void*>(address), bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect);
    memcpy(reinterpret_cast<void*>(address), bytes.data(), bytes.size());
    VirtualProtect(reinterpret_cast<void*>(address), bytes.size(), old_protect, &old_protect);
}

bool patch_save_file()
{
    uintptr_t base_address = (uintptr_t)GetModuleHandleA("DarkSoulsII.exe");

#ifdef _M_IX86
    uintptr_t offset = 0xF389A6;
#elif defined(_M_X64)
    uintptr_t offset = 0x11B563A;
#endif

    // ugly ass code to covert wstring to byte array
    const wchar_t* extension = L"sl3";
    size_t byte_count = (wcslen(extension) + 1) * sizeof(wchar_t);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(extension);
    std::vector<uint8_t> patch_bytes(bytes, bytes + byte_count);

    patch_memory(base_address + offset, patch_bytes);

    return true;
}

bool patch_qol()
{
    uintptr_t base_address = (uintptr_t)GetModuleHandleA("DarkSoulsII.exe");

#ifdef _M_IX86
    uintptr_t no_logos = 0x1146E96;
    uintptr_t no_press_start = 0x1949B1;
#elif defined(_M_X64)
    uintptr_t no_logos = 0x160DE1A;
    uintptr_t no_press_start = 0xFDB66;
#endif

    // ported from https://github.com/r3sus/Resouls/tree/main/ds2s/mods
    bool skipLogos = GetPrivateProfileIntW(L"misc", L"skipLogos", 1, L".\\ds2modengine.ini") == 1;
    if (skipLogos) {
        // no logos
        patch_memory(base_address + no_logos, { 0x01 });
        // no "press start button"
        patch_memory(base_address + no_press_start, { 0x02 });
    }

    return true;
}