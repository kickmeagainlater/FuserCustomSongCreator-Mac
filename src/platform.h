#pragma once

// ── platform.h ────────────────────────────────────────────────────────────────
// Windows→macOS shims. Include before any Win32 headers.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef PLATFORM_MAC

#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>   // write, close, unlink
#include <fcntl.h>    // mkstemps
#include <time.h>     // nanosleep
#include <thread>
#include <atomic>
#include <mutex>

// ── Basic Windows types ───────────────────────────────────────────────────────
using DWORD     = uint32_t;
using WORD      = uint16_t;
using BYTE      = uint8_t;
using CHAR      = char;
using BOOL      = int;
using UINT      = unsigned int;
using LONG      = int32_t;
using ULONG     = uint32_t;
using LONGLONG  = int64_t;
using ULONGLONG = uint64_t;
using HANDLE    = void*;
using HWND      = void*;
using HINSTANCE = void*;
using WPARAM    = uintptr_t;
using LPARAM    = intptr_t;
using LRESULT   = intptr_t;

#define TRUE  1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAX_PATH 4096

using TCHAR   = char;
using LPTSTR  = char*;
using LPCTSTR = const char*;
using LPCSTR  = const char*;
using LPSTR   = char*;
#define TEXT(x) x
#define _T(x)   x

#define ZeroMemory(dest, size)      memset((dest), 0, (size))
#define CopyMemory(dest, src, size) memcpy((dest), (src), (size))

// ── Path helpers ──────────────────────────────────────────────────────────────
inline std::string NormalizePath(const std::string& p) {
    std::string out = p;
    for (char& c : out) if (c == '\\') c = '/';
    return out;
}

// ── Debugbreak ────────────────────────────────────────────────────────────────
#define __debugbreak() __builtin_trap()

// ── MSVC assert variants ──────────────────────────────────────────────────────
#include <cassert>
#define _ASSERT(x)   assert(x)
#define _ASSERTE(x)  assert(x)

// ── WINAPI / calling conventions ─────────────────────────────────────────────
#define WINAPI
#define CALLBACK
#define APIENTRY
#define __stdcall
#define __cdecl

// ── MessageBox shim ───────────────────────────────────────────────────────────
#define MB_OK        0x00
#define MB_YESNO     0x04
#define MB_ICONERROR 0x10
#define IDOK  1
#define IDYES 6
#define IDNO  7

inline int MessageBoxA(HWND, const char* text, const char* caption, UINT) {
    fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
    return IDOK;
}
#define MessageBox  MessageBoxA
#define MessageBoxW MessageBoxA

// ── ShellExecute shim ─────────────────────────────────────────────────────────
inline void ShellExecuteA(HWND, const char*, const char* file,
                           const char*, const char*, int) {
    if (file) { std::string cmd = std::string("open \"") + file + "\""; system(cmd.c_str()); }
}
#define ShellExecute  ShellExecuteA
#define SW_SHOW 5

// ── File dialog via Zenity (no AppleScript deadlock) ─────────────────────────
// osascript's `choose file` deadlocks when called from a non-main thread AND
// can hang indefinitely waiting for Automation permission on newer macOS.
// Zenity is a GTK dialog runner that works fine from any thread.
// Install: brew install zenity
// Falls back to a plain terminal prompt if zenity is not available.
//
// These run SYNCHRONOUSLY on a BACKGROUND THREAD.
// The Windows-style GetOpenFileName/GetSaveFileName macros call the async
// wrappers which spin-wait – acceptable since the user is interacting with
// Finder and the app being "paused" is normal expected behaviour.
// ─────────────────────────────────────────────────────────────────────────────

#define OFN_PATHMUSTEXIST   0x0800
#define OFN_FILEMUSTEXIST   0x1000
#define OFN_OVERWRITEPROMPT 0x0002

struct OPENFILENAME {
    DWORD        lStructSize     = sizeof(OPENFILENAME);
    HWND         hwndOwner       = nullptr;
    const CHAR*  lpstrFilter     = nullptr;
    CHAR*        lpstrFile       = nullptr;
    DWORD        nMaxFile        = 0;
    const CHAR*  lpstrTitle      = nullptr;
    DWORD        Flags           = 0;
    const CHAR*  lpstrDefExt     = nullptr;
    DWORD        nFilterIndex    = 0;
    const CHAR*  lpstrFileTitle  = nullptr;
    DWORD        nMaxFileTitle   = 0;
    const CHAR*  lpstrInitialDir = nullptr;
};

// Parse first *.ext from Win32 filter "Desc\0*.ext\0All\0*.*\0"
inline std::string _PlatExtFromFilter(const char* f) {
    if (!f) return "";
    while (*f) f++; f++;        // skip description
    std::string p(f);
    auto s = p.find('*');
    return s != std::string::npos ? p.substr(s + 1) : ""; // e.g. ".mogg"
}

// Run a shell command and return trimmed stdout, "" on failure/cancel
inline std::string _PlatRunCmd(const std::string& cmd) {
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return "";
    char buf[MAX_PATH] = {};
    bool got = fgets(buf, sizeof(buf), p) != nullptr;
    pclose(p);
    if (!got || !buf[0]) return "";
    size_t n = strlen(buf);
    if (n && buf[n-1] == '\n') buf[n-1] = '\0';
    return buf;
}

// Check if zenity is available
inline bool _PlatHasZenity() {
    static int cached = -1;
    if (cached == -1)
        cached = (system("command -v zenity >/dev/null 2>&1") == 0) ? 1 : 0;
    return cached == 1;
}

inline std::string _PlatOpenDialog(const char* filter, const char* title) {
    if (_PlatHasZenity()) {
        std::string ext = _PlatExtFromFilter(filter);
        std::string cmd = "zenity --file-selection";
        if (title) cmd += std::string(" --title='") + title + "'";
        if (!ext.empty()) cmd += " --file-filter='*" + ext + "'";
        cmd += " 2>/dev/null";
        return _PlatRunCmd(cmd);
    }
    // Fallback: prompt in terminal
    fprintf(stderr, "Enter file path: ");
    char buf[MAX_PATH] = {};
    if (!fgets(buf, sizeof(buf), stdin)) return "";
    size_t n = strlen(buf);
    if (n && buf[n-1] == '\n') buf[n-1] = '\0';
    return buf;
}

inline std::string _PlatSaveDialog(const char* filter, const char* title) {
    if (_PlatHasZenity()) {
        std::string ext = _PlatExtFromFilter(filter);
        std::string cmd = "zenity --file-selection --save --confirm-overwrite";
        if (title) cmd += std::string(" --title='") + title + "'";
        if (!ext.empty()) cmd += " --file-filter='*" + ext + "'";
        cmd += " 2>/dev/null";
        return _PlatRunCmd(cmd);
    }
    fprintf(stderr, "Enter save path: ");
    char buf[MAX_PATH] = {};
    if (!fgets(buf, sizeof(buf), stdin)) return "";
    size_t n = strlen(buf);
    if (n && buf[n-1] == '\n') buf[n-1] = '\0';
    return buf;
}

// Synchronous wrappers – CALL FROM A BACKGROUND THREAD ONLY
inline bool GetOpenFileNameA(OPENFILENAME* ofn) {
    if (!ofn || !ofn->lpstrFile) return false;
    // Run dialog on this (background) thread
    std::string result;
    std::atomic<bool> done{false};
    std::thread([&]{
        result = _PlatOpenDialog(ofn->lpstrFilter, ofn->lpstrTitle);
        done.store(true, std::memory_order_release);
    }).detach();
    while (!done.load(std::memory_order_acquire)) {
        struct timespec ts{0, 16000000}; nanosleep(&ts, nullptr);
    }
    if (result.empty()) return false;
    strncpy(ofn->lpstrFile, result.c_str(), ofn->nMaxFile - 1);
    return true;
}

inline bool GetSaveFileNameA(OPENFILENAME* ofn) {
    if (!ofn || !ofn->lpstrFile) return false;
    std::string result;
    std::atomic<bool> done{false};
    std::thread([&]{
        result = _PlatSaveDialog(ofn->lpstrFilter, ofn->lpstrTitle);
        done.store(true, std::memory_order_release);
    }).detach();
    while (!done.load(std::memory_order_acquire)) {
        struct timespec ts{0, 16000000}; nanosleep(&ts, nullptr);
    }
    if (result.empty()) return false;
    strncpy(ofn->lpstrFile, result.c_str(), ofn->nMaxFile - 1);
    return true;
}

#define GetOpenFileName  GetOpenFileNameA
#define GetSaveFileName  GetSaveFileNameA

// ── Macros (guarded against bass.h redefinition) ──────────────────────────────
#ifndef LOBYTE
#define LOBYTE(w)  ((uint8_t)(w))
#endif
#ifndef HIBYTE
#define HIBYTE(w)  ((uint8_t)(((uint16_t)(w)) >> 8))
#endif
#ifndef LOWORD
#define LOWORD(l)  ((uint16_t)(l))
#endif
#ifndef HIWORD
#define HIWORD(l)  ((uint16_t)(((uint32_t)(l)) >> 16))
#endif
#ifndef MAKELONG
#define MAKELONG(lo, hi) ((uint32_t)(((uint16_t)(lo)) | (((uint32_t)((uint16_t)(hi))) << 16)))
#endif

#endif // PLATFORM_MAC
