#include "dinput8/dinput8.h"
#include "hooks.h"
#include "patches.h"

#include <cstdio>
#include <iostream>

#include <MinHook.h>

bool load_extra_dlls(wchar_t* dlls_dir)
{
    wchar_t search_path[MAX_PATH];
    swprintf(search_path, MAX_PATH, L"%s\\*.dll", dlls_dir);

    WIN32_FIND_DATAW file_data;
    HANDLE hFind = FindFirstFileW(search_path, &file_data);

    if (hFind == INVALID_HANDLE_VALUE) return true;

    do {
        wchar_t dll_path[MAX_PATH];
        swprintf(dll_path, MAX_PATH, L"%s\\%s", dlls_dir, file_data.cFileName);

        HMODULE hModule = LoadLibraryW(dll_path);
        if (hModule == NULL) {
            wprintf(L"Failed to load %s\n", dll_path);
            return false;
        }
    } while (FindNextFileW(hFind, &file_data));

    FindClose(hFind);
    return true;
}

void main()
{

#ifdef _DEBUG
    bool debug = true;
#else
    bool debug = GetPrivateProfileIntW(L"debug", L"showDebugLog", 0, L".\\ds2modengine.ini") == 1;
#endif

    if (debug) {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    }

    if (MH_Initialize() != MH_OK) {
        std::cout << "[ModEngine] Error initializing minhook." << std::endl;
        return;
    }

    // force offline
    int blockNetworkAccess = GetPrivateProfileIntW(L"online", L"blockNetworkAccess", 1, L".\\ds2modengine.ini") == 1;
    if (blockNetworkAccess && !force_offline()) {
        std::cout << "[ModEngine] Error forcing game to offline." << std::endl;
        return;
    }

    // alternate save file
    int useAlternateSaveFile = GetPrivateProfileIntW(L"savefile", L"useAlternateSaveFile", 1, L".\\ds2modengine.ini") == 1;
    if (useAlternateSaveFile && !patch_save_file()) {
        std::cout << "[ModEngine] Error patching the save file." << std::endl;
        return;
    }

    // asset override hooks
    wchar_t* asset_override_dir = (wchar_t*)malloc(1000);
    GetPrivateProfileStringW(L"files", L"modOverrideDirectory", L"\\", asset_override_dir, 500, L".\\ds2modengine.ini");

    int useModOverrideDirectory = GetPrivateProfileIntW(L"files", L"useModOverrideDirectory", 1, L".\\ds2modengine.ini") == 1;
    if (useModOverrideDirectory && !setup_asset_override_hooks(asset_override_dir)) {
        std::cout << "[ModEngine] Error setting up asset override hooks." << std::endl;
        return;
    }

    // load mods
    wchar_t* dlls_dir = (wchar_t*)malloc(1000);
    GetPrivateProfileStringW(L"files", L"extraDllsDirectory", L"\\", dlls_dir, 500, L".\\ds2modengine.ini");

    int useExtraDLLsDirectory = GetPrivateProfileIntW(L"files", L"useExtraDLLsDirectory", 1, L".\\ds2modengine.ini") == 1;
    if (useExtraDLLsDirectory && !load_extra_dlls(dlls_dir)) {
        std::cout << "[ModEngine] Error loading extra dlls." << std::endl;
        return;
    }

    // qol
    if (!patch_qol()) {
        std::cout << "[ModEngine] Error applying quality of life patches." << std::endl;
        return;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        LoadOriginalDll();
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)main, NULL, NULL, NULL);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        FreeOriginalDll();
    }
    return TRUE;
}