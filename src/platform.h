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

// ── File dialogs via osascript (native macOS Finder panels) ──────────────────
// Uses `choose file` / `choose file name` — shows a real Finder open/save
// sheet. No Automation permission required (Standard Additions, not app
// control). Blocks the caller until the user dismisses the dialog, which
// matches Win32 GetOpenFileName / GetSaveFileName semantics.
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

// Run a shell command and return trimmed first line of stdout, "" on failure.
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

// Build an osascript `of type` list from a Win32 filter string.
// "Audio\0*.ogg;*.flac\0\0"  →  {"ogg","flac"}
// Returns "" when no specific types (pass-through = any file).
inline std::string _OsaTypeList(const char* f) {
    if (!f) return "";
    while (*f) f++; f++;  // skip description field
    std::string result;
    for (const char* p = f; *p; ) {
        if (*p == '*' && *(p+1) == '.') {
            p += 2;
            std::string ext;
            while (*p && *p != ';') ext += *p++;
            if (!ext.empty() && ext != "*") {
                if (!result.empty()) result += ", ";
                result += "\"" + ext + "\"";
            }
        } else { ++p; }
    }
    return result.empty() ? "" : "{" + result + "}";
}

// Open-file dialog — returns selected POSIX path or "" on cancel.
inline std::string _PlatOpenDialog(const char* filter, const char* title) {
    std::string script = "POSIX path of (choose file";
    if (title && *title) script += std::string(" with prompt \"") + title + "\"";
    std::string types = _OsaTypeList(filter);
    if (!types.empty()) script += " of type " + types;
    script += ")";
    return _PlatRunCmd("osascript -e '" + script + "' 2>/dev/null");
}

// Save-file dialog — returns chosen POSIX path or "" on cancel.
inline std::string _PlatSaveDialog(const char* filter, const char* title) {
    (void)filter;
    std::string script = "POSIX path of (choose file name";
    if (title && *title) script += std::string(" with prompt \"") + title + "\"";
    script += ")";
    return _PlatRunCmd("osascript -e '" + script + "' 2>/dev/null");
}

inline bool GetOpenFileNameA(OPENFILENAME* ofn) {
    if (!ofn || !ofn->lpstrFile) return false;
    std::string result = _PlatOpenDialog(ofn->lpstrFilter, ofn->lpstrTitle);
    if (result.empty()) return false;
    strncpy(ofn->lpstrFile, result.c_str(), ofn->nMaxFile - 1);
    ofn->lpstrFile[ofn->nMaxFile - 1] = '\0';
    return true;
}

inline bool GetSaveFileNameA(OPENFILENAME* ofn) {
    if (!ofn || !ofn->lpstrFile) return false;
    std::string result = _PlatSaveDialog(ofn->lpstrFilter, ofn->lpstrTitle);
    if (result.empty()) return false;
    strncpy(ofn->lpstrFile, result.c_str(), ofn->nMaxFile - 1);
    ofn->lpstrFile[ofn->nMaxFile - 1] = '\0';
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
