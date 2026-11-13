/*
 * Breath of Fire IV (Steam 4249150 / GOG build) — DirectInput re-acquire fix for Wine/Proton.
 *
 * The game acquires its DirectInput keyboard/mouse exactly once, with
 * DISCL_FOREGROUND. Whenever another Wine window in the same prefix (e.g. the
 * CheatHappens Aurora trainer) becomes foreground, Wine's dinput unacquires the
 * device and every later GetDeviceState() returns DIERR_NOTACQUIRED. The game
 * never calls Acquire() again, so the keyboard is dead until restart. (Same if
 * the game window was not foreground at the moment of the first Acquire.)
 *
 * This DLL is loaded as a proxy for version.dll (which BOF4.exe imports). It
 * patches the GetDeviceState/GetDeviceData slots of Wine's IDirectInputDevice7A
 * vtable so that a "not acquired / input lost" result triggers an Acquire() and
 * one retry — i.e. what a well-behaved DirectInput app does itself. While the
 * game is not foreground Acquire() simply keeps failing, so nothing leaks into
 * the game from other windows.
 *
 * Build: see build.sh. Install: copy version.dll next to BOF4.exe and load it
 * with WINEDLLOVERRIDES=version=n,b (done by the local protonfix).
 */
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>

static const GUID my_IID_IDirectInput7A       = {0x9a4cb684,0x236d,0x11d3,{0x8e,0x9d,0x00,0xc0,0x4f,0x68,0x44,0xae}};
static const GUID my_IID_IDirectInputDevice7A = {0x57d7c6bc,0x2356,0x11d3,{0x8e,0x9d,0x00,0xc0,0x4f,0x68,0x44,0xae}};
static const GUID my_GUID_SysKeyboard         = {0x6f1d2b61,0xd5a0,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};

/* ---- version.dll forwarding ------------------------------------------------ */
#define NFUNCS 17
static const char *const real_names[NFUNCS] = {
    "GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
    "GetFileVersionInfoExW", "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW", "GetFileVersionInfoW",
    "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW",
    "VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW",
};
void *real_funcs[NFUNCS];

#define STUB(n) __attribute__((naked)) void stub##n(void) { __asm__("jmp *_real_funcs+4*" #n); }
STUB(0) STUB(1) STUB(2) STUB(3) STUB(4) STUB(5) STUB(6) STUB(7) STUB(8)
STUB(9) STUB(10) STUB(11) STUB(12) STUB(13) STUB(14) STUB(15) STUB(16)

/* ---- tiny logger (stderr -> Proton log when PROTON_LOG=1) ------------------- */
static void say(const char *msg)
{
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD n;
    if (h && h != INVALID_HANDLE_VALUE)
    {
        WriteFile(h, "bof4-inputfix: ", 15, &n, NULL);
        WriteFile(h, msg, lstrlenA(msg), &n, NULL);
        WriteFile(h, "\n", 1, &n, NULL);
    }
}

/* ---- the actual fix --------------------------------------------------------- */
typedef HRESULT (WINAPI *GetDeviceState_t)(IDirectInputDevice7A *, DWORD, void *);
typedef HRESULT (WINAPI *GetDeviceData_t)(IDirectInputDevice7A *, DWORD, DIDEVICEOBJECTDATA *, DWORD *, DWORD);
static GetDeviceState_t orig_GetDeviceState;
static GetDeviceData_t  orig_GetDeviceData;

static BOOL lost(HRESULT hr) { return hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST; }

static HRESULT WINAPI fix_GetDeviceState(IDirectInputDevice7A *dev, DWORD size, void *data)
{
    HRESULT hr = orig_GetDeviceState(dev, size, data);
    if (lost(hr) && SUCCEEDED(dev->lpVtbl->Acquire(dev)))
        hr = orig_GetDeviceState(dev, size, data);
    return hr;
}

static HRESULT WINAPI fix_GetDeviceData(IDirectInputDevice7A *dev, DWORD size, DIDEVICEOBJECTDATA *data,
                                        DWORD *count, DWORD flags)
{
    DWORD saved = count ? *count : 0;
    HRESULT hr = orig_GetDeviceData(dev, size, data, count, flags);
    if (lost(hr) && SUCCEEDED(dev->lpVtbl->Acquire(dev)))
    {
        if (count) *count = saved;
        hr = orig_GetDeviceData(dev, size, data, count, flags);
    }
    return hr;
}

static DWORD WINAPI fix_thread(void *arg)
{
    typedef HRESULT (WINAPI *create_t)(HINSTANCE, DWORD, REFIID, void **, IUnknown *);
    char path[MAX_PATH];
    HMODULE dinput;
    create_t create;
    IDirectInput7A *di = NULL;
    IDirectInputDevice7A *dev = NULL;
    IDirectInputDevice7AVtbl *vtbl;
    DWORD old;

    /* Wine's builtin dinput, by full path: the game folder has GOG's own dinput.dll wrapper. */
    GetSystemDirectoryA(path, MAX_PATH - 16);
    lstrcatA(path, "\\dinput.dll");
    if (!(dinput = LoadLibraryA(path))) { say("cannot load system dinput.dll"); return 1; }
    if (!(create = (create_t)(void *)GetProcAddress(dinput, "DirectInputCreateEx")))
    { say("DirectInputCreateEx not found"); return 1; }

    if (FAILED(create(GetModuleHandleA(NULL), 0x0700, &my_IID_IDirectInput7A, (void **)&di, NULL)) || !di)
    { say("DirectInputCreateEx failed"); return 1; }
    if (FAILED(di->lpVtbl->CreateDeviceEx(di, &my_GUID_SysKeyboard, &my_IID_IDirectInputDevice7A,
                                           (void **)&dev, NULL)) || !dev)
    { say("CreateDeviceEx(keyboard) failed"); di->lpVtbl->Release(di); return 1; }

    /* All of Wine's ANSI dinput devices (keyboard, mouse, joystick) share this vtable. */
    vtbl = dev->lpVtbl;
    if (vtbl->GetDeviceState != fix_GetDeviceState &&
        VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &old))
    {
        orig_GetDeviceState = vtbl->GetDeviceState;
        orig_GetDeviceData  = vtbl->GetDeviceData;
        vtbl->GetDeviceState = fix_GetDeviceState;
        vtbl->GetDeviceData  = fix_GetDeviceData;
        VirtualProtect(vtbl, sizeof(*vtbl), old, &old);
        say("installed (DirectInput devices now re-acquire after focus loss)");
    }
    else say("could not patch dinput vtable");

    dev->lpVtbl->Release(dev);
    di->lpVtbl->Release(di);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        char path[MAX_PATH];
        HMODULE real;
        HANDLE thread;
        int i;

        DisableThreadLibraryCalls(inst);
        GetSystemDirectoryA(path, MAX_PATH - 16);
        lstrcatA(path, "\\version.dll");
        if (!(real = LoadLibraryA(path)) || real == (HMODULE)inst) return FALSE;
        for (i = 0; i < NFUNCS; i++) real_funcs[i] = (void *)GetProcAddress(real, real_names[i]);

        if ((thread = CreateThread(NULL, 0, fix_thread, NULL, 0, NULL))) CloseHandle(thread);
    }
    return TRUE;
}
