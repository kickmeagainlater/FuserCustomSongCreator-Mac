#ifdef PLATFORM_MAC
// ═══════════════════════════════════════════════════════════════════════════
// macOS: GLFW + OpenGL 3.2
// ═══════════════════════════════════════════════════════════════════════════
#include <OpenGL/gl3.h>
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM ""

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "platform.h"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// Undef our macro versions so bass.h's identical definitions don't warn
#undef LOBYTE
#undef HIBYTE
#undef LOWORD
#undef HIWORD
#undef MAKELONG
#include "bass/bass.h"
#include "configfile.h"

size_t window_width  = 1280;
size_t window_height = 800;

extern void custom_song_creator_update(size_t width, size_t height);
// set_g_pd3dDevice is defined in custom_song_creator.cpp, taking ID3D11Device* (stubbed struct on Mac)
struct ID3D11Device;  // forward decl (stub type from ImageFile.h guards)
extern void set_g_pd3dDevice(ID3D11Device* p);
extern void initAudio();
extern bool unsavedChanges;
extern bool closePressed;
extern bool filenameArg;
extern std::string filenameArgPath;

// HWND G_hwnd not needed on Mac (GLFW handles window)
void* G_hwnd = nullptr;

ConfigFile fcsc_cfg;

static void glfw_error_cb(int error, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

// macOS config path: ~/Library/Application Support/FuserCustomsCreator/config
static std::string GetConfigPath() {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    std::string dir = std::string(home) + "/Library/Application Support/FuserCustomsCreator";
    std::filesystem::create_directories(dir);
    return dir + "/config";
}

// Try to find a usable monospace font on macOS
static const char* FindMonoFont() {
    static const char* candidates[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/Library/Fonts/Courier New.ttf",
        nullptr
    };
    for (int i = 0; candidates[i]; ++i)
        if (std::filesystem::exists(candidates[i])) return candidates[i];
    return nullptr;
}

int main(int argc, char** argv)
{
    // Command-line file argument (mirrors Windows CommandLineToArgvW behaviour)
    if (argc > 1) {
        filenameArg = true;
        filenameArgPath = argv[1];
    }

    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        (int)window_width, (int)window_height,
        "Fuser Custom Song Creator", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Keep window_width/height in sync on resize
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h){
        window_width  = (size_t)w;
        window_height = (size_t)h;
        glViewport(0, 0, w, h);
    });

    // Mirror WM_CLOSE / unsavedChanges behaviour
    glfwSetWindowCloseCallback(window, [](GLFWwindow* w){
        if (unsavedChanges) {
            closePressed = true;
            glfwSetWindowShouldClose(w, GLFW_FALSE); // let app handle it
        }
        // else let the default close proceed
    });

    initAudio();

    // Load config from ~/Library/Application Support/...
    std::string cfgPath = GetConfigPath();
    std::wstring cfgPathW(cfgPath.begin(), cfgPath.end());
    fcsc_cfg.path = cfgPathW;
    fcsc_cfg.loadConfig(cfgPathW);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Load fonts – fall back gracefully on Mac
    const char* monoFont = FindMonoFont();
    if (monoFont) {
        ImFontConfig cfg;
        io.Fonts->AddFontFromFileTTF(monoFont, 14.0f);
        // Add CJK/Korean ranges via a system CJK font if available
        const char* cjkFont = "/System/Library/Fonts/PingFang.ttc";
        if (std::filesystem::exists(cjkFont)) {
            cfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(cjkFont, 16.0f, &cfg,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            io.Fonts->AddFontFromFileTTF(cjkFont, 16.0f, &cfg,
                io.Fonts->GetGlyphRangesJapanese());
            io.Fonts->AddFontFromFileTTF(cjkFont, 16.0f, &cfg,
                io.Fonts->GetGlyphRangesKorean());
        }
        io.Fonts->Build();
    }
    // If no font found, ImGui uses its built-in default – still works fine.

    ImVec4 clear = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        set_g_pd3dDevice(nullptr);  // no-op on Mac
        custom_song_creator_update(window_width, window_height);

        ImGui::Render();
        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(clear.x, clear.y, clear.z, clear.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#else
// ═══════════════════════════════════════════════════════════════════════════
// Windows: DirectX 11 + Win32 (original, preserved exactly)
// ═══════════════════════════════════════════════════════════════════════════
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "bass/bass.h"
#include "configfile.h"
#include <vector>
#include <codecvt>

static ID3D11Device*            g_pd3dDevice          = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext    = NULL;
static IDXGISwapChain*          g_pSwapChain           = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

size_t window_width  = 1280;
size_t window_height = 800;

extern void custom_song_creator_update(size_t width, size_t height);
extern void set_g_pd3dDevice(ID3D11Device* g_pd3dDevice);
extern void initAudio();
extern bool unsavedChanges;
extern bool closePressed;
extern bool filenameArg;
extern std::string filenameArgPath;

HWND G_hwnd;
ConfigFile fcsc_cfg;

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPSTR lpCmdLine, int nShowCmd)
{
#ifdef _DEBUG
    if (!AttachConsole(-1)) { AllocConsole(); ShowWindow(GetConsoleWindow(), SW_SHOW); }
    FILE* fp;
    freopen_s(&fp, "CONOIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
#endif

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("Fuser Custom Song Creator"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Fuser Custom Song Creator"),
        WS_OVERLAPPEDWINDOW, 100, 100, (int)window_width, (int)window_height,
        NULL, NULL, wc.hInstance, NULL);
    G_hwnd = hwnd;
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
    initAudio();

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImFont* font0    = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Consola.ttf", 14.0f);
    ImFontConfig config; config.MergeMode = true;
    ImFont* fontcsc  = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc",   16.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    ImFont* fontctr  = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msjh.ttc",   16.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* fontkor  = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\malgun.ttf", 16.0f, &config, io.Fonts->GetGlyphRangesKorean());
    ImFont* fontjpn  = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\YuGothR.ttc",16.0f, &config, io.Fonts->GetGlyphRangesJapanese());
    if (font0 && fontcsc && fontctr && fontkor && fontjpn) io.Fonts->Build();

    ImVec4 clear = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    PWSTR appDataPath = nullptr;
    LPWSTR* szArglist; int nArgs;
    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (!szArglist) return 1;
    if (nArgs > 1) {
        filenameArg = true;
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        filenameArgPath = converter.to_bytes(szArglist[1]);
    }
    LocalFree(szArglist);

    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath) == S_OK) {
        std::wstring appDataFolderPath(appDataPath);
        CoTaskMemFree(appDataPath);
        std::wstring configFolder = appDataFolderPath + L"\\FuserCustomsCreator";
        std::wstring configFile   = configFolder      + L"\\config";
        fcsc_cfg.path = configFile;
        CreateDirectoryW(configFolder.c_str(), NULL);
        fcsc_cfg.loadConfig(configFile);
    }

    MSG msg; ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg); continue;
        }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        set_g_pd3dDevice(g_pd3dDevice);
        custom_song_creator_update(window_width, window_height);
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd; ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount=2; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator=60; sd.BufferDesc.RefreshRate.Denominator=1;
    sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=hWnd;
    sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fla[2]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    if (D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,fla,2,
        D3D11_SDK_VERSION,&sd,&g_pSwapChain,&g_pd3dDevice,&fl,&g_pd3dDeviceContext)!=S_OK)
        return false;
    CreateRenderTarget(); return true;
}
void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if(g_pSwapChain)       {g_pSwapChain->Release();g_pSwapChain=NULL;}
    if(g_pd3dDeviceContext){g_pd3dDeviceContext->Release();g_pd3dDeviceContext=NULL;}
    if(g_pd3dDevice)       {g_pd3dDevice->Release();g_pd3dDevice=NULL;}
}
void CreateRenderTarget() {
    ID3D11Texture2D* bb;
    g_pSwapChain->GetBuffer(0,IID_PPV_ARGS(&bb));
    g_pd3dDevice->CreateRenderTargetView(bb,NULL,&g_mainRenderTargetView);
    bb->Release();
}
void CleanupRenderTarget() {
    if(g_mainRenderTargetView){g_mainRenderTargetView->Release();g_mainRenderTargetView=NULL;}
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd,msg,wParam,lParam)) return true;
    switch(msg) {
    case WM_CLOSE:
        if (unsavedChanges) { closePressed = true; }
        else { DestroyWindow(hWnd); }
        return 0;
    case WM_SIZE:
        if(g_pd3dDevice&&wParam!=SIZE_MINIMIZED){
            CleanupRenderTarget();
            window_width=(size_t)LOWORD(lParam); window_height=(size_t)HIWORD(lParam);
            g_pSwapChain->ResizeBuffers(0,(UINT)LOWORD(lParam),(UINT)HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN,0);
            CreateRenderTarget();
        } return 0;
    case WM_SYSCOMMAND:
        if((wParam&0xfff0)==SC_KEYMENU) return 0; break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd,msg,wParam,lParam);
}
#endif // PLATFORM_MAC
