#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")

#define APP_NAME L"MiniLaunch2000"
#define MAX_ITEMS 512
#define WM_TRAY (WM_APP + 1)
#define ID_FIRST_ITEM 1000
#define ID_MENU_RELOAD 40001
#define ID_MENU_OPENINI 40002
#define ID_MENU_ADD 40003
#define ID_MENU_EXIT 40004
#define ID_MENU_ONTOP 40005
#define ID_MENU_EDITINI 40006
#define ID_MENU_DELETEITEM 40007

enum MENU_THEME_MODE { MENU_THEME_AUTO = 0, MENU_THEME_DARK = 1, MENU_THEME_LIGHT = 2 };

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

typedef struct ITEM {
    WCHAR name[128];
    WCHAR path[MAX_PATH];
    WCHAR args[512];
    WCHAR workdir[MAX_PATH];
    WCHAR iconPath[MAX_PATH];
    int iconIndex;
    HICON hIcon;
    HWND hBtn;
} ITEM;

static HINSTANCE g_hInst;
static HWND g_hwnd;
static ITEM g_items[MAX_ITEMS];
static int g_count = 0;
static int g_iconSize = 16;
static int g_cellW = 24;
static int g_cellH = 24;
static int g_maxColumns = 16;
static int g_margin = 4;
static BOOL g_stayOnTop = TRUE;
static BOOL g_showCaption = FALSE;
static BOOL g_transparentMode = FALSE;
static int g_bgOpacity = 210;
static int g_iconOpacity = 255;
static BOOL g_pressFeedback = TRUE;
static int g_pressOffset = 2;
static int g_pressedItem = -1;
static int g_hoverItem = -1;
static BOOL g_trackingMouse = FALSE;
static BOOL g_hoverBorder = TRUE;
static BOOL g_hoverGlow = TRUE;
static COLORREF g_hoverColor = RGB(77, 163, 255);
static int g_hoverBorderOpacity = 170;
static int g_hoverGlowOpacity = 55;
static int g_hoverBorderWidth = 1;
static int g_hoverGlowSize = 3;
static int g_hoverRadius = 5;
static MENU_THEME_MODE g_menuTheme = MENU_THEME_AUTO;
static int g_startX = CW_USEDEFAULT;
static int g_startY = CW_USEDEFAULT;
static BOOL g_haveSavedPos = FALSE;
static WCHAR g_ini[MAX_PATH];
static HFONT g_font;
static HWND g_tooltip;

static void GetExeDirFile(const WCHAR* file, WCHAR* out, DWORD cch) {
    GetModuleFileNameW(NULL, out, cch);
    PathRemoveFileSpecW(out);
    PathAppendW(out, file);
}

static void Trim(WCHAR* s) {
    size_t n = wcslen(s);
    while (n && (s[n-1] == L' ' || s[n-1] == L'\t' || s[n-1] == L'\r' || s[n-1] == L'\n')) s[--n] = 0;
}


static BOOL ReadIniIntValue(const WCHAR* section, const WCHAR* key, int* out) {
    WCHAR buf[64] = L"";
    GetPrivateProfileStringW(section, key, L"", buf, 64, g_ini);
    Trim(buf);
    if (!buf[0]) return FALSE;
    *out = _wtoi(buf);
    return TRUE;
}


static COLORREF ReadIniColorValue(const WCHAR* section, const WCHAR* key, COLORREF defColor) {
    WCHAR buf[64] = L"";
    GetPrivateProfileStringW(section, key, L"", buf, 64, g_ini);
    Trim(buf);
    if (!buf[0]) return defColor;
    unsigned int v = 0;
    if (wcsncmp(buf, L"0x", 2) == 0 || wcsncmp(buf, L"0X", 2) == 0) {
        swscanf(buf + 2, L"%x", &v);
    } else if (buf[0] == L'#') {
        swscanf(buf + 1, L"%x", &v);
    } else {
        swscanf(buf, L"%x", &v);
    }
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

static int ClampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}


typedef enum _PREFERRED_APP_MODE {
    APPMODE_DEFAULT = 0,
    APPMODE_ALLOW_DARK = 1,
    APPMODE_FORCE_DARK = 2,
    APPMODE_FORCE_LIGHT = 3,
    APPMODE_MAX = 4
} PREFERRED_APP_MODE;

typedef PREFERRED_APP_MODE (WINAPI *PFN_SetPreferredAppMode)(PREFERRED_APP_MODE appMode);
typedef void (WINAPI *PFN_FlushMenuThemes)(void);

static BOOL IsSystemAppsUseDarkTheme(void) {
    DWORD value = 1;
    DWORD cb = sizeof(value);
    LONG r = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &cb);
    // Missing key: default to light, matching old Windows behavior.
    if (r != ERROR_SUCCESS) return FALSE;
    return value == 0;
}

static MENU_THEME_MODE ParseMenuThemeValue(const WCHAR* text) {
    if (!text || !text[0]) return MENU_THEME_AUTO;
    if (lstrcmpiW(text, L"Dark") == 0) return MENU_THEME_DARK;
    if (lstrcmpiW(text, L"Light") == 0) return MENU_THEME_LIGHT;
    return MENU_THEME_AUTO;
}

static BOOL ShouldUseDarkMenu(void) {
    if (g_menuTheme == MENU_THEME_DARK) return TRUE;
    if (g_menuTheme == MENU_THEME_LIGHT) return FALSE;
    return IsSystemAppsUseDarkTheme();
}

static void ApplyMenuTheme(void) {
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;

    PFN_SetPreferredAppMode SetPreferredAppModeFn = (PFN_SetPreferredAppMode)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    PFN_FlushMenuThemes FlushMenuThemesFn = (PFN_FlushMenuThemes)GetProcAddress(ux, MAKEINTRESOURCEA(136));

    if (SetPreferredAppModeFn) {
        PREFERRED_APP_MODE mode = ShouldUseDarkMenu() ? APPMODE_FORCE_DARK : APPMODE_FORCE_LIGHT;
        SetPreferredAppModeFn(mode);
    }
    if (FlushMenuThemesFn) FlushMenuThemesFn();

    FreeLibrary(ux);
}

static void LoadStartupPosition(void) {
    int x = 0, y = 0;
    if (!ReadIniIntValue(L"Settings", L"WindowX", &x) || !ReadIniIntValue(L"Settings", L"WindowY", &y)) {
        g_haveSavedPos = FALSE;
        g_startX = CW_USEDEFAULT;
        g_startY = CW_USEDEFAULT;
        return;
    }

    // Keep the restored position inside the virtual desktop, useful after changing monitors.
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (x < vx - 200 || x > vx + vw - 8 || y < vy - 200 || y > vy + vh - 8) {
        g_haveSavedPos = FALSE;
        g_startX = CW_USEDEFAULT;
        g_startY = CW_USEDEFAULT;
        return;
    }
    g_haveSavedPos = TRUE;
    g_startX = x;
    g_startY = y;
}

static void SaveWindowPlacement(void) {
    if (!g_hwnd || !IsWindow(g_hwnd)) return;
    RECT r;
    if (!GetWindowRect(g_hwnd, &r)) return;
    WCHAR buf[32];
    wsprintfW(buf, L"%d", r.left);  WritePrivateProfileStringW(L"Settings", L"WindowX", buf, g_ini);
    wsprintfW(buf, L"%d", r.top);   WritePrivateProfileStringW(L"Settings", L"WindowY", buf, g_ini);
    wsprintfW(buf, L"%d", r.right - r.left);  WritePrivateProfileStringW(L"Settings", L"WindowW", buf, g_ini);
    wsprintfW(buf, L"%d", r.bottom - r.top);  WritePrivateProfileStringW(L"Settings", L"WindowH", buf, g_ini);
}

static void DestroyIcons(void) {
    for (int i=0;i<g_count;i++) {
        if (g_items[i].hIcon) { DestroyIcon(g_items[i].hIcon); g_items[i].hIcon = NULL; }
    }
}

static HICON LoadItemIcon(ITEM* it) {
    WCHAR src[MAX_PATH];
    lstrcpynW(src, it->iconPath[0] ? it->iconPath : it->path, MAX_PATH);
    if (!src[0]) return NULL;
    HICON big = NULL, small = NULL;
    UINT got = ExtractIconExW(src, it->iconIndex, &big, &small, 1);
    HICON h = NULL;
    if (got) {
        if (g_iconSize <= 16 && small) h = small;
        else if (big) h = big;
        else h = small;
        if (h == small && big) DestroyIcon(big);
        if (h == big && small) DestroyIcon(small);
    }
    if (!h) {
        SHFILEINFOW sfi = {0};
        DWORD flags = SHGFI_ICON | (g_iconSize <= 16 ? SHGFI_SMALLICON : SHGFI_LARGEICON);
        if (SHGetFileInfoW(src, 0, &sfi, sizeof(sfi), flags)) h = sfi.hIcon;
    }
    return h;
}

static void WriteDefaultIni(void) {
    if (PathFileExistsW(g_ini)) return;
    WCHAR notepad[MAX_PATH] = L"notepad.exe";
    WCHAR calc[MAX_PATH] = L"calc.exe";
    FILE* f = _wfopen(g_ini, L"wb");
    if (!f) return;
    const unsigned char bom[2] = {0xFF,0xFE};
    fwrite(bom,1,2,f);
    fwprintf(f, L"[Settings]\r\nIconSize=16\r\nMaxColumns=16\r\n; Columns 仍兼容旧配置；建议改用 MaxColumns。\r\nCellWidth=24\r\nCellHeight=24\r\nMargin=4\r\nStayOnTop=1\r\nShowCaption=0\r\nTransparentMode=1\r\nBackgroundOpacity=210\r\nIconOpacity=255\r\nPressFeedback=1\r\nPressOffset=2\r\nHoverBorder=1\r\nHoverGlow=1\r\nHoverBorderColor=0x4DA3FF\r\nHoverBorderOpacity=170\r\nHoverBorderWidth=1\r\nHoverGlowOpacity=55\r\nHoverGlowSize=3\r\nHoverRadius=5\r\nMenuTheme=Auto\r\n; MenuTheme=Auto/Dark/Light，控制右键菜单主题；Win7 自动退回浅色。\r\n; 兼容旧配置：若没有 BackgroundOpacity，则读取旧的 Opacity。\r\n; TransparentMode=1 时使用黑色半透明工具条，只绘制图标，不显示按钮边框。\r\n; BackgroundOpacity 只控制背景/边框透明度，IconOpacity 只控制图标透明度，范围 0~255。\r\n; PressFeedback=1 开启点击图标时的轻微下坠反馈，PressOffset 控制下坠像素。\r\n; HoverBorder/HoverGlow 控制鼠标悬停时的边框与光晕。\r\n; WindowX/WindowY 会在退出时自动保存。\r\nWindowX=\r\nWindowY=\r\nWindowW=\r\nWindowH=\r\n\r\n");
    fwprintf(f, L"; Path 可填 exe、bat、cmd、lnk、文件夹或网址。\r\n; Args 是启动参数，WorkDir 是工作目录，Icon 可单独指定图标文件。\r\n\r\n");
    fwprintf(f, L"[Item1]\r\nName=Notepad\r\nPath=%s\r\nArgs=\r\nWorkDir=\r\nIcon=\r\nIconIndex=0\r\n\r\n", notepad);
    fwprintf(f, L"[Item2]\r\nName=Calculator\r\nPath=%s\r\nArgs=\r\nWorkDir=\r\nIcon=\r\nIconIndex=0\r\n", calc);
    fclose(f);
}

static void LoadConfig(void) {
    DestroyIcons();
    g_count = 0;
    g_hoverItem = -1;
    g_pressedItem = -1;
    g_iconSize = GetPrivateProfileIntW(L"Settings", L"IconSize", 16, g_ini);
    if (g_iconSize < 12) g_iconSize = 12;
    if (g_iconSize > 64) g_iconSize = 64;
    // Auto-width mode: window width follows icon count, and wraps after MaxColumns.
    // For compatibility, old INI key "Columns" is still accepted when MaxColumns is absent.
    g_maxColumns = GetPrivateProfileIntW(L"Settings", L"MaxColumns", -1, g_ini);
    if (g_maxColumns < 0) g_maxColumns = GetPrivateProfileIntW(L"Settings", L"Columns", 16, g_ini);
    if (g_maxColumns < 1) g_maxColumns = 1;
    if (g_maxColumns > 64) g_maxColumns = 64;
    g_cellW = GetPrivateProfileIntW(L"Settings", L"CellWidth", g_iconSize + 8, g_ini);
    g_cellH = GetPrivateProfileIntW(L"Settings", L"CellHeight", g_iconSize + 8, g_ini);
    g_margin = GetPrivateProfileIntW(L"Settings", L"Margin", 4, g_ini);
    g_stayOnTop = GetPrivateProfileIntW(L"Settings", L"StayOnTop", 1, g_ini) ? TRUE : FALSE;
    g_showCaption = GetPrivateProfileIntW(L"Settings", L"ShowCaption", 0, g_ini) ? TRUE : FALSE;
    g_transparentMode = GetPrivateProfileIntW(L"Settings", L"TransparentMode", 1, g_ini) ? TRUE : FALSE;
    // Split opacity: background and icons can be tuned separately.
    // Old INI key "Opacity" is still accepted as the background opacity fallback.
    g_bgOpacity = GetPrivateProfileIntW(L"Settings", L"BackgroundOpacity", -1, g_ini);
    if (g_bgOpacity < 0) g_bgOpacity = GetPrivateProfileIntW(L"Settings", L"Opacity", 210, g_ini);
    g_iconOpacity = GetPrivateProfileIntW(L"Settings", L"IconOpacity", 255, g_ini);
    if (g_bgOpacity < 0) g_bgOpacity = 0;
    if (g_bgOpacity > 255) g_bgOpacity = 255;
    if (g_iconOpacity < 0) g_iconOpacity = 0;
    if (g_iconOpacity > 255) g_iconOpacity = 255;
    g_pressFeedback = GetPrivateProfileIntW(L"Settings", L"PressFeedback", 1, g_ini) ? TRUE : FALSE;
    g_pressOffset = GetPrivateProfileIntW(L"Settings", L"PressOffset", 2, g_ini);
    if (g_pressOffset < 0) g_pressOffset = 0;
    if (g_pressOffset > 8) g_pressOffset = 8;
    g_hoverBorder = GetPrivateProfileIntW(L"Settings", L"HoverBorder", 1, g_ini) ? TRUE : FALSE;
    g_hoverGlow = GetPrivateProfileIntW(L"Settings", L"HoverGlow", 1, g_ini) ? TRUE : FALSE;
    g_hoverColor = ReadIniColorValue(L"Settings", L"HoverBorderColor", RGB(77, 163, 255));
    g_hoverBorderOpacity = ClampInt(GetPrivateProfileIntW(L"Settings", L"HoverBorderOpacity", 170, g_ini), 0, 255);
    g_hoverBorderWidth = ClampInt(GetPrivateProfileIntW(L"Settings", L"HoverBorderWidth", 1, g_ini), 1, 6);
    g_hoverGlowOpacity = ClampInt(GetPrivateProfileIntW(L"Settings", L"HoverGlowOpacity", 55, g_ini), 0, 255);
    g_hoverGlowSize = ClampInt(GetPrivateProfileIntW(L"Settings", L"HoverGlowSize", 3, g_ini), 0, 12);
    g_hoverRadius = ClampInt(GetPrivateProfileIntW(L"Settings", L"HoverRadius", 5, g_ini), 0, 16);
    WCHAR menuThemeBuf[32] = L"Auto";
    GetPrivateProfileStringW(L"Settings", L"MenuTheme", L"Auto", menuThemeBuf, 32, g_ini);
    Trim(menuThemeBuf);
    g_menuTheme = ParseMenuThemeValue(menuThemeBuf);
    ApplyMenuTheme();
    for (int i=1; i<=MAX_ITEMS; i++) {
        WCHAR sec[32]; wsprintfW(sec, L"Item%d", i);
        WCHAR path[MAX_PATH] = L"";
        GetPrivateProfileStringW(sec, L"Path", L"", path, MAX_PATH, g_ini);
        Trim(path);
        if (!path[0]) continue;
        ITEM* it = &g_items[g_count];
        ZeroMemory(it, sizeof(ITEM));
        GetPrivateProfileStringW(sec, L"Name", path, it->name, 128, g_ini);
        lstrcpynW(it->path, path, MAX_PATH);
        GetPrivateProfileStringW(sec, L"Args", L"", it->args, 512, g_ini);
        GetPrivateProfileStringW(sec, L"WorkDir", L"", it->workdir, MAX_PATH, g_ini);
        GetPrivateProfileStringW(sec, L"Icon", L"", it->iconPath, MAX_PATH, g_ini);
        it->iconIndex = GetPrivateProfileIntW(sec, L"IconIndex", 0, g_ini);
        it->hIcon = LoadItemIcon(it);
        g_count++;
        if (g_count >= MAX_ITEMS) break;
    }
}

static void AddTooltipForButton(HWND hBtn, const WCHAR* text) {
    if (!g_tooltip) return;
    TOOLINFOW ti = {0};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = g_hwnd;
    ti.uId = (UINT_PTR)hBtn;
    ti.lpszText = (LPWSTR)text;
    SendMessageW(g_tooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

static void AddTooltipForRect(int id, RECT rc, const WCHAR* text) {
    if (!g_tooltip) return;
    TOOLINFOW ti = {0};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = g_hwnd;
    ti.uId = (UINT_PTR)id;
    ti.rect = rc;
    ti.lpszText = (LPWSTR)text;
    SendMessageW(g_tooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

static void ClearButtons(void) {
    for (int i=0;i<MAX_ITEMS;i++) {
        if (g_items[i].hBtn) { DestroyWindow(g_items[i].hBtn); g_items[i].hBtn = NULL; }
    }
    if (g_tooltip) { DestroyWindow(g_tooltip); g_tooltip = NULL; }
}

static int ActualColumns(void) {
    int actualColumns = g_count;
    if (actualColumns < 1) actualColumns = 1;
    if (actualColumns > g_maxColumns) actualColumns = g_maxColumns;
    return actualColumns;
}

static int HitTestItem(int x, int y) {
    int actualColumns = ActualColumns();
    if (x < g_margin || y < g_margin) return -1;
    int col = (x - g_margin) / g_cellW;
    int row = (y - g_margin) / g_cellH;
    if (col < 0 || col >= actualColumns || row < 0) return -1;
    int idx = row * actualColumns + col;
    if (idx < 0 || idx >= g_count) return -1;

    // In transparent mode there are no real BUTTON controls.
    // Only the visible icon rectangle should be clickable; the padding around
    // the icon remains draggable just like the v3.4 baseline button edge area.
    int iconX = g_margin + col * g_cellW + (g_cellW - g_iconSize) / 2;
    int iconY = g_margin + row * g_cellH + (g_cellH - g_iconSize) / 2;
    if (x < iconX || x >= iconX + g_iconSize || y < iconY || y >= iconY + g_iconSize) return -1;
    return idx;
}


static int HitTestHoverItem(int x, int y) {
    int actualColumns = ActualColumns();
    if (x < g_margin || y < g_margin) return -1;
    int col = (x - g_margin) / g_cellW;
    int row = (y - g_margin) / g_cellH;
    if (col < 0 || col >= actualColumns || row < 0) return -1;
    int idx = row * actualColumns + col;
    if (idx < 0 || idx >= g_count) return -1;

    // Hover follows the visible icon area, with a small tolerance so tiny 16x16 icons are easy to target.
    int iconX = g_margin + col * g_cellW + (g_cellW - g_iconSize) / 2;
    int iconY = g_margin + row * g_cellH + (g_cellH - g_iconSize) / 2;
    int tol = 2;
    if (x < iconX - tol || x >= iconX + g_iconSize + tol || y < iconY - tol || y >= iconY + g_iconSize + tol) return -1;
    return idx;
}

static void RenderLayeredToolbar(void);

static void LayoutButtons(void) {
    ClearButtons();
    g_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, g_hwnd, NULL, g_hInst, NULL);

    // Actual columns are automatic:
    // 0 items -> 1 column placeholder, 1..MaxColumns -> exactly item count, >MaxColumns -> wrap.
    int actualColumns = ActualColumns();
    int rows = (g_count + actualColumns - 1) / actualColumns;
    if (rows < 1) rows = 1;
    int w = g_margin*2 + actualColumns*g_cellW;
    int h = g_margin*2 + rows*g_cellH;
    DWORD style = WS_POPUP | WS_CLIPCHILDREN;
    if (g_showCaption) style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    SetWindowLongPtrW(g_hwnd, GWL_STYLE, style);

    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    if (g_transparentMode) ex |= WS_EX_LAYERED;
    else ex &= ~WS_EX_LAYERED;
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
    // In transparent mode we use UpdateLayeredWindow for per-pixel alpha,
    // so background opacity does not affect icon opacity.

    SetWindowPos(g_hwnd, g_stayOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,w,h,
        SWP_NOMOVE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    RECT wr = {0,0,w,h}; AdjustWindowRectEx(&wr, style, FALSE, (DWORD)GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE));
    SetWindowPos(g_hwnd, NULL, 0,0, wr.right-wr.left, wr.bottom-wr.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    for (int i=0;i<g_count;i++) {
        int col = i % actualColumns;
        int row = i / actualColumns;
        int x = g_margin + col*g_cellW;
        int y = g_margin + row*g_cellH;
        if (g_transparentMode) {
            RECT rc = { x, y, x + g_cellW, y + g_cellH };
            AddTooltipForRect(ID_FIRST_ITEM+i, rc, g_items[i].name[0] ? g_items[i].name : g_items[i].path);
        } else {
            HWND b = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD|WS_VISIBLE|BS_ICON|BS_FLAT,
                x, y, g_cellW, g_cellH, g_hwnd, (HMENU)(INT_PTR)(ID_FIRST_ITEM+i), g_hInst, NULL);
            g_items[i].hBtn = b;
            if (g_items[i].hIcon) SendMessageW(b, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_items[i].hIcon);
            AddTooltipForButton(b, g_items[i].name[0] ? g_items[i].name : g_items[i].path);
        }
    }
    if (g_transparentMode) RenderLayeredToolbar();
    else InvalidateRect(g_hwnd, NULL, TRUE);
}

static void CompositeIconPixel(BYTE* dst, const BYTE* src) {
    int sa = src[3];
    if (sa == 0) {
        // Some old icons/masks drawn by GDI may leave alpha at 0 but RGB non-zero.
        if (src[0] || src[1] || src[2]) sa = 255;
        else return;
    }
    sa = (sa * g_iconOpacity + 127) / 255;
    if (sa <= 0) return;

    // DIB byte order is BGRA. Source RGB from DrawIconEx is treated as straight alpha.
    int inv = 255 - sa;
    dst[0] = (BYTE)((src[0] * sa + dst[0] * inv + 127) / 255);
    dst[1] = (BYTE)((src[1] * sa + dst[1] * inv + 127) / 255);
    dst[2] = (BYTE)((src[2] * sa + dst[2] * inv + 127) / 255);
    // Alpha-over composition against background alpha.
    dst[3] = (BYTE)(sa + (dst[3] * inv + 127) / 255);
}

static HBITMAP Create32bppDib(HDC refDc, int w, int h, BYTE** bitsOut) {
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(refDc, &bi, DIB_RGB_COLORS, (void**)bitsOut, NULL, 0);
}


static BOOL PointInRoundedRect(int x, int y, int l, int t, int r, int b, int radius) {
    if (x < l || x >= r || y < t || y >= b) return FALSE;
    if (radius <= 0) return TRUE;
    int rad = radius;
    int cx = x;
    int cy = y;
    if (x < l + rad) cx = l + rad;
    else if (x >= r - rad) cx = r - rad - 1;
    if (y < t + rad) cy = t + rad;
    else if (y >= b - rad) cy = b - rad - 1;
    int dx = x - cx;
    int dy = y - cy;
    return (dx*dx + dy*dy) <= rad*rad;
}

static void AlphaBlendPixel(BYTE* dst, COLORREF color, int alpha) {
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    int inv = 255 - alpha;
    BYTE b = (BYTE)GetBValue(color);
    BYTE g = (BYTE)GetGValue(color);
    BYTE r = (BYTE)GetRValue(color);
    dst[0] = (BYTE)((b * alpha + dst[0] * inv + 127) / 255);
    dst[1] = (BYTE)((g * alpha + dst[1] * inv + 127) / 255);
    dst[2] = (BYTE)((r * alpha + dst[2] * inv + 127) / 255);
    dst[3] = (BYTE)(alpha + (dst[3] * inv + 127) / 255);
}

static void DrawRoundedFillAlpha(BYTE* bits, int w, int h, int l, int t, int r, int b, int radius, COLORREF color, int alpha) {
    if (!bits || alpha <= 0) return;
    if (l < 0) l = 0; if (t < 0) t = 0; if (r > w) r = w; if (b > h) b = h;
    if (l >= r || t >= b) return;
    for (int y=t; y<b; y++) {
        BYTE* p = bits + (y*w + l) * 4;
        for (int x=l; x<r; x++, p+=4) {
            if (PointInRoundedRect(x, y, l, t, r, b, radius)) AlphaBlendPixel(p, color, alpha);
        }
    }
}

static void DrawRoundedBorderAlpha(BYTE* bits, int w, int h, int l, int t, int r, int b, int radius, int borderWidth, COLORREF color, int alpha) {
    if (!bits || alpha <= 0 || borderWidth <= 0) return;
    int ol = l, ot = t, orr = r, ob = b;
    if (l < 0) l = 0; if (t < 0) t = 0; if (r > w) r = w; if (b > h) b = h;
    if (l >= r || t >= b) return;
    int il = ol + borderWidth, it = ot + borderWidth, ir = orr - borderWidth, ib = ob - borderWidth;
    int innerRadius = radius - borderWidth;
    if (innerRadius < 0) innerRadius = 0;
    for (int y=t; y<b; y++) {
        BYTE* p = bits + (y*w + l) * 4;
        for (int x=l; x<r; x++, p+=4) {
            BOOL outer = PointInRoundedRect(x, y, ol, ot, orr, ob, radius);
            BOOL inner = PointInRoundedRect(x, y, il, it, ir, ib, innerRadius);
            if (outer && !inner) AlphaBlendPixel(p, color, alpha);
        }
    }
}

static void DrawHoverEffect(BYTE* bits, int w, int h) {
    if (g_hoverItem < 0 || g_hoverItem >= g_count) return;
    if (!g_hoverBorder && !g_hoverGlow) return;
    int actualColumns = ActualColumns();
    int col = g_hoverItem % actualColumns;
    int row = g_hoverItem / actualColumns;
    int l = g_margin + col*g_cellW + 1;
    int t = g_margin + row*g_cellH + 1;
    int r = l + g_cellW - 2;
    int b = t + g_cellH - 2;
    if (g_hoverGlow && g_hoverGlowSize > 0) {
        DrawRoundedFillAlpha(bits, w, h, l - g_hoverGlowSize, t - g_hoverGlowSize, r + g_hoverGlowSize, b + g_hoverGlowSize,
            g_hoverRadius + g_hoverGlowSize, g_hoverColor, g_hoverGlowOpacity);
    }
    if (g_hoverBorder) {
        DrawRoundedBorderAlpha(bits, w, h, l, t, r, b, g_hoverRadius, g_hoverBorderWidth, g_hoverColor, g_hoverBorderOpacity);
    }
}

static void RenderLayeredToolbar(void) {
    if (!g_hwnd || !g_transparentMode) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    BYTE* bits = NULL;
    HBITMAP bmp = Create32bppDib(screen, w, h, &bits);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        ReleaseDC(NULL, screen);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    // Fill black background with independent alpha.
    for (int y=0; y<h; y++) {
        BYTE* p = bits + y * w * 4;
        for (int x=0; x<w; x++, p+=4) {
            p[0] = 0; p[1] = 0; p[2] = 0; p[3] = (BYTE)g_bgOpacity;
        }
    }

    // Draw optional hover border/glow before icons, so icons remain crisp and fully visible.
    DrawHoverEffect(bits, w, h);

    // Render icons onto a temporary transparent DIB, then composite them over the background.
    HDC iconDc = CreateCompatibleDC(screen);
    BYTE* iconBits = NULL;
    HBITMAP iconBmp = Create32bppDib(screen, w, h, &iconBits);
    HGDIOBJ oldIconBmp = NULL;
    if (iconBmp && iconBits) {
        oldIconBmp = SelectObject(iconDc, iconBmp);
        ZeroMemory(iconBits, w * h * 4);

        int actualColumns = ActualColumns();
        for (int i=0; i<g_count; i++) {
            int col = i % actualColumns;
            int row = i / actualColumns;
            int x = g_margin + col*g_cellW + (g_cellW - g_iconSize) / 2;
            int y = g_margin + row*g_cellH + (g_cellH - g_iconSize) / 2;
            if (g_pressFeedback && i == g_pressedItem) y += g_pressOffset;
            if (g_items[i].hIcon) DrawIconEx(iconDc, x, y, g_items[i].hIcon, g_iconSize, g_iconSize, 0, NULL, DI_NORMAL);
        }

        for (int i=0; i<w*h; i++) {
            CompositeIconPixel(bits + i*4, iconBits + i*4);
        }
    }

    POINT src = {0,0};
    POINT dst;
    RECT wr;
    GetWindowRect(g_hwnd, &wr);
    dst.x = wr.left; dst.y = wr.top;
    SIZE sz = {w,h};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwnd, screen, &dst, &sz, dc, &src, 0, &bf, ULW_ALPHA);

    if (oldIconBmp) SelectObject(iconDc, oldIconBmp);
    if (iconBmp) DeleteObject(iconBmp);
    DeleteDC(iconDc);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
}

static void PaintTransparentToolbar(HWND h) {
    // The transparent toolbar is rendered by UpdateLayeredWindow.
    // Keep WM_PAINT empty to avoid applying whole-window alpha to icons.
    PAINTSTRUCT ps;
    BeginPaint(h, &ps);
    EndPaint(h, &ps);
}



// Dark-aware modal confirmation dialog.  Keep it self-contained so future
// configuration windows can reuse the same theme decision path.
#define ID_DLG_YES 51001
#define ID_DLG_NO  51002
#define DARK_DIALOG_CLASS L"MiniLaunch2000DarkDialog"

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

struct THEMED_CONFIRM_DATA {
    const WCHAR* title;
    const WCHAR* message;
    BOOL dark;
    int result;
    HFONT font;
    HBRUSH bgBrush;
    HBRUSH editBrush;
    COLORREF bg;
    COLORREF panel;
    COLORREF text;
    COLORREF subText;
    COLORREF border;
    HWND hOwner;
};

static void ApplyDarkTitleBar(HWND hwnd, BOOL dark) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute fn = (PFN_DwmSetWindowAttribute)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (fn) {
        BOOL value = dark ? TRUE : FALSE;
        fn(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        // Older Win10 builds used attribute 19.
        DWORD oldAttr = 19;
        fn(hwnd, oldAttr, &value, sizeof(value));
    }
    FreeLibrary(dwm);
}

static void DrawThemedButton(const DRAWITEMSTRUCT* dis, THEMED_CONFIRM_DATA* d) {
    BOOL hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    BOOL down = (dis->itemState & ODS_SELECTED) != 0;
    BOOL focus = (dis->itemState & ODS_FOCUS) != 0;
    BOOL disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = d->dark ? RGB(48,48,48) : RGB(245,245,245);
    COLORREF br = d->dark ? RGB(88,88,88) : RGB(170,170,170);
    COLORREF tx = d->dark ? RGB(245,245,245) : RGB(20,20,20);
    if (hot) bg = d->dark ? RGB(58,58,58) : RGB(232,242,255);
    if (down) bg = d->dark ? RGB(70,70,70) : RGB(210,230,255);
    if (disabled) tx = d->dark ? RGB(120,120,120) : RGB(150,150,150);

    HBRUSH hb = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, hb);
    DeleteObject(hb);

    HPEN pen = CreatePen(PS_SOLID, focus ? 2 : 1, focus ? RGB(77,163,255) : br);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
    RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 8, 8);
    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(pen);

    WCHAR text[128] = L"";
    GetWindowTextW(dis->hwndItem, text, 128);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, tx);
    HFONT oldFont = (HFONT)SelectObject(dis->hDC, d->font ? d->font : GetStockObject(DEFAULT_GUI_FONT));
    RECT tr = dis->rcItem;
    DrawTextW(dis->hDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

static LRESULT CALLBACK ThemedConfirmProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    THEMED_CONFIRM_DATA* d = (THEMED_CONFIRM_DATA*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE:
        d = (THEMED_CONFIRM_DATA*)((CREATESTRUCTW*)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    case WM_CREATE: {
        d = (THEMED_CONFIRM_DATA*)((CREATESTRUCTW*)l)->lpCreateParams;
        ApplyDarkTitleBar(h, d->dark);
        RECT cr; GetClientRect(h, &cr);
        HWND icon = CreateWindowExW(0, L"STATIC", L"?", WS_CHILD | WS_VISIBLE | SS_CENTER,
            18, 22, 32, 32, h, NULL, g_hInst, NULL);
        HWND txt = CreateWindowExW(0, L"STATIC", d->message, WS_CHILD | WS_VISIBLE | SS_LEFT,
            62, 18, cr.right - 82, 78, h, NULL, g_hInst, NULL);
        HWND yes = CreateWindowExW(0, L"BUTTON", L"是(&Y)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            cr.right - 184, cr.bottom - 46, 78, 28, h, (HMENU)ID_DLG_YES, g_hInst, NULL);
        HWND no = CreateWindowExW(0, L"BUTTON", L"否(&N)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            cr.right - 96, cr.bottom - 46, 78, 28, h, (HMENU)ID_DLG_NO, g_hInst, NULL);
        if (d->font) {
            SendMessageW(icon, WM_SETFONT, (WPARAM)d->font, TRUE);
            SendMessageW(txt, WM_SETFONT, (WPARAM)d->font, TRUE);
            SendMessageW(yes, WM_SETFONT, (WPARAM)d->font, TRUE);
            SendMessageW(no, WM_SETFONT, (WPARAM)d->font, TRUE);
        }
        SetFocus(no);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r);
        FillRect((HDC)w, &r, d->bgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, d->text);
        return (LRESULT)d->bgBrush;
    }
    case WM_DRAWITEM:
        if (d) { DrawThemedButton((DRAWITEMSTRUCT*)l, d); return TRUE; }
        break;
    case WM_COMMAND:
        if (LOWORD(w) == ID_DLG_YES || LOWORD(w) == ID_DLG_NO) {
            d->result = (LOWORD(w) == ID_DLG_YES) ? IDYES : IDNO;
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { d->result = IDNO; DestroyWindow(h); return 0; }
        if (w == VK_RETURN) { d->result = IDYES; DestroyWindow(h); return 0; }
        break;
    case WM_CLOSE:
        d->result = IDNO;
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

static int ThemedConfirm(HWND owner, const WCHAR* message, const WCHAR* title) {
    BOOL dark = ShouldUseDarkMenu();
    THEMED_CONFIRM_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.title = title ? title : APP_NAME;
    data.message = message ? message : L"";
    data.dark = dark;
    data.result = IDNO;
    data.bg = dark ? RGB(32,32,32) : RGB(250,250,250);
    data.panel = dark ? RGB(42,42,42) : RGB(255,255,255);
    data.text = dark ? RGB(240,240,240) : RGB(20,20,20);
    data.subText = dark ? RGB(200,200,200) : RGB(80,80,80);
    data.border = dark ? RGB(72,72,72) : RGB(210,210,210);
    data.hOwner = owner;
    data.bgBrush = CreateSolidBrush(data.bg);
    data.editBrush = CreateSolidBrush(data.panel);
    data.font = g_font ? g_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ThemedConfirmProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wc.hbrBackground = data.bgBrush;
    wc.lpszClassName = DARK_DIALOG_CLASS;
    RegisterClassExW(&wc);

    int w = 360, h = 146;
    RECT orc;
    if (owner && GetWindowRect(owner, &orc)) {
        int x = orc.left + ((orc.right - orc.left) - w) / 2;
        int y = orc.top + ((orc.bottom - orc.top) - h) / 2;
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (x < vx) x = vx; if (y < vy) y = vy;
        if (x + w > vx + vw) x = vx + vw - w;
        if (y + h > vy + vh) y = vy + vh - h;
        orc.left = x; orc.top = y;
    } else {
        orc.left = CW_USEDEFAULT; orc.top = CW_USEDEFAULT;
    }

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
        DARK_DIALOG_CLASS, data.title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
        orc.left, orc.top, w, h, owner, NULL, g_hInst, &data);
    if (!dlg) {
        if (data.bgBrush) DeleteObject(data.bgBrush);
        if (data.editBrush) DeleteObject(data.editBrush);
        return MessageBoxW(owner, message, title ? title : APP_NAME, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    }

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    if (data.bgBrush) DeleteObject(data.bgBrush);
    if (data.editBrush) DeleteObject(data.editBrush);
    return data.result;
}

static void RunItem(int idx) {
    if (idx < 0 || idx >= g_count) return;
    ITEM* it = &g_items[idx];
    HINSTANCE r = ShellExecuteW(g_hwnd, L"open", it->path, it->args[0] ? it->args : NULL, it->workdir[0] ? it->workdir : NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) MessageBoxW(g_hwnd, L"启动失败。请检查 INI 中的 Path/Args/WorkDir。", APP_NAME, MB_ICONERROR);
}

static void ReloadAll(void) { LoadConfig(); LayoutButtons(); }

static void OpenIni(BOOL edit) {
    ShellExecuteW(g_hwnd, edit ? L"edit" : L"open", g_ini, NULL, NULL, SW_SHOWNORMAL);
}


static void WriteItemToSection(const WCHAR* sec, const ITEM* it) {
    WritePrivateProfileStringW(sec, L"Name", it->name, g_ini);
    WritePrivateProfileStringW(sec, L"Path", it->path, g_ini);
    WritePrivateProfileStringW(sec, L"Args", it->args, g_ini);
    WritePrivateProfileStringW(sec, L"WorkDir", it->workdir, g_ini);
    WritePrivateProfileStringW(sec, L"Icon", it->iconPath, g_ini);
    WCHAR buf[32]; wsprintfW(buf, L"%d", it->iconIndex);
    WritePrivateProfileStringW(sec, L"IconIndex", buf, g_ini);
}

static void DeleteItemFromConfig(int idx) {
    if (idx < 0 || idx >= g_count) return;

    WCHAR msg[512];
    const WCHAR* name = g_items[idx].name[0] ? g_items[idx].name : g_items[idx].path;
    wsprintfW(msg, L"确定要删除这个图标吗？\r\n\r\n%s", name);
    if (ThemedConfirm(g_hwnd, msg, APP_NAME) != IDYES) return;

    // Remove all old Item sections first, then write the remaining items back sequentially.
    // This keeps the INI clean and avoids holes like [Item1], [Item3] after deletion.
    for (int i = 1; i <= MAX_ITEMS; i++) {
        WCHAR sec[32]; wsprintfW(sec, L"Item%d", i);
        WritePrivateProfileStringW(sec, NULL, NULL, g_ini);
    }

    int outIndex = 1;
    for (int i = 0; i < g_count; i++) {
        if (i == idx) continue;
        WCHAR sec[32]; wsprintfW(sec, L"Item%d", outIndex++);
        WriteItemToSection(sec, &g_items[i]);
    }

    ReloadAll();
}

static void AppendItemFromDialog(void) {
    WCHAR file[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Programs and shortcuts\0*.exe;*.bat;*.cmd;*.lnk;*.msc;*.cpl\0All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return;
    int n = g_count + 1;
    WCHAR sec[32]; wsprintfW(sec, L"Item%d", n);
    WCHAR title[128]; lstrcpynW(title, PathFindFileNameW(file), 128); PathRemoveExtensionW(title);
    WritePrivateProfileStringW(sec, L"Name", title, g_ini);
    WritePrivateProfileStringW(sec, L"Path", file, g_ini);
    WritePrivateProfileStringW(sec, L"Args", L"", g_ini);
    WritePrivateProfileStringW(sec, L"WorkDir", L"", g_ini);
    WritePrivateProfileStringW(sec, L"Icon", L"", g_ini);
    WritePrivateProfileStringW(sec, L"IconIndex", L"0", g_ini);
    ReloadAll();
}

static void ShowContextMenu(POINT pt, int itemIndex) {
    ApplyMenuTheme();
    HMENU m = CreatePopupMenu();
    if (itemIndex >= 0 && itemIndex < g_count) {
        AppendMenuW(m, MF_STRING, ID_MENU_DELETEITEM, L"删除此图标");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(m, MF_STRING, ID_MENU_ADD, L"添加程序...");
    AppendMenuW(m, MF_STRING, ID_MENU_OPENINI, L"打开配置 INI");
    AppendMenuW(m, MF_STRING, ID_MENU_EDITINI, L"编辑配置 INI");
    AppendMenuW(m, MF_STRING, ID_MENU_RELOAD, L"重新加载");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (g_stayOnTop ? MF_CHECKED : 0), ID_MENU_ONTOP, L"窗口置顶");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_MENU_EXIT, L"退出");
    SetForegroundWindow(g_hwnd);
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(m);
    switch(id) {
        case ID_MENU_DELETEITEM: DeleteItemFromConfig(itemIndex); break;
        case ID_MENU_ADD: AppendItemFromDialog(); break;
        case ID_MENU_OPENINI: OpenIni(FALSE); break;
        case ID_MENU_EDITINI: OpenIni(TRUE); break;
        case ID_MENU_RELOAD: ReloadAll(); break;
        case ID_MENU_ONTOP:
            g_stayOnTop = !g_stayOnTop;
            WritePrivateProfileStringW(L"Settings", L"StayOnTop", g_stayOnTop ? L"1" : L"0", g_ini);
            LayoutButtons(); break;
        case ID_MENU_EXIT: SaveWindowPlacement(); DestroyWindow(g_hwnd); break;
    }
}

static void AddTray(void) {
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!nid.hIcon) nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpynW(nid.szTip, APP_NAME, ARRAYSIZE(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}
static void DelTray(void) {
    NOTIFYICONDATAW nid = {0}; nid.cbSize = sizeof(nid); nid.hWnd = g_hwnd; nid.uID = 1; Shell_NotifyIconW(NIM_DELETE, &nid);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch(msg) {
    case WM_CREATE:
        // CreateWindowExW sends WM_CREATE before it returns, so the global g_hwnd
        // has not been assigned yet in wWinMain.  Use the real window handle now,
        // otherwise the first LayoutButtons() creates no visible child buttons.
        g_hwnd = h;
        AddTray();
        ReloadAll();
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) >= ID_FIRST_ITEM && LOWORD(w) < ID_FIRST_ITEM + MAX_ITEMS) {
            RunItem(LOWORD(w) - ID_FIRST_ITEM);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if (g_transparentMode) return 1;
        break;
    case WM_PAINT:
        if (g_transparentMode) { PaintTransparentToolbar(h); return 0; }
        break;
    case WM_CONTEXTMENU: {
        POINT pt;
        int itemIndex = -1;
        if ((HWND)w && (HWND)w != h) {
            HWND child = (HWND)w;
            int cid = GetDlgCtrlID(child);
            if (cid >= ID_FIRST_ITEM && cid < ID_FIRST_ITEM + MAX_ITEMS) itemIndex = cid - ID_FIRST_ITEM;
            pt.x = GET_X_LPARAM(l); pt.y = GET_Y_LPARAM(l);
            if (pt.x == -1 && pt.y == -1) { RECT r; GetWindowRect(child, &r); pt.x = r.left; pt.y = r.bottom; }
        } else {
            pt.x = GET_X_LPARAM(l); pt.y = GET_Y_LPARAM(l);
            if (pt.x == -1 && pt.y == -1) {
                RECT r; GetWindowRect(h, &r); pt.x = r.left; pt.y = r.bottom;
            } else if (g_transparentMode) {
                POINT client = pt;
                ScreenToClient(h, &client);
                itemIndex = HitTestHoverItem(client.x, client.y);
            }
        }
        ShowContextMenu(pt, itemIndex); return 0;
    }
    case WM_LBUTTONDOWN:
        if (g_transparentMode) {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            g_pressedItem = HitTestItem(x, y);
            if (g_pressedItem >= 0) { SetCapture(h); RenderLayeredToolbar(); return 0; }
        }
        if (!g_showCaption) SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_LBUTTONUP:
        if (g_transparentMode && g_pressedItem >= 0) {
            int old = g_pressedItem;
            g_pressedItem = -1;
            ReleaseCapture();
            RenderLayeredToolbar();
            int idx = HitTestItem(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            if (idx == old) RunItem(idx);
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        if (g_transparentMode && g_pressedItem >= 0) { g_pressedItem = -1; RenderLayeredToolbar(); }
        break;
    case WM_MOUSEMOVE:
        if (g_transparentMode) {
            if (!g_trackingMouse) {
                TRACKMOUSEEVENT tme;
                ZeroMemory(&tme, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = h;
                if (TrackMouseEvent(&tme)) g_trackingMouse = TRUE;
            }
            int newHover = HitTestHoverItem(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            if (newHover != g_hoverItem) {
                g_hoverItem = newHover;
                RenderLayeredToolbar();
            } else if (g_pressedItem >= 0) {
                RenderLayeredToolbar();
            }
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        if (g_transparentMode) {
            g_trackingMouse = FALSE;
            if (g_hoverItem != -1) { g_hoverItem = -1; RenderLayeredToolbar(); }
            return 0;
        }
        break;
    case WM_MOVE:
    case WM_SIZE:
        if (g_transparentMode) RenderLayeredToolbar();
        break;
    case WM_TRAY:
        if (l == WM_LBUTTONDBLCLK) ShowWindow(h, IsWindowVisible(h) ? SW_HIDE : SW_SHOW);
        if (l == WM_RBUTTONUP) { POINT pt; GetCursorPos(&pt); ShowContextMenu(pt, -1); }
        return 0;
    case WM_CLOSE:
        SaveWindowPlacement();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        SaveWindowPlacement();
        DelTray(); DestroyIcons(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInst;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_BAR_CLASSES }; InitCommonControlsEx(&icc);
    GetExeDirFile(L"MiniLaunch2000.ini", g_ini, MAX_PATH);
    WriteDefaultIni();
    WCHAR menuThemeBuf[32] = L"Auto";
    GetPrivateProfileStringW(L"Settings", L"MenuTheme", L"Auto", menuThemeBuf, 32, g_ini);
    Trim(menuThemeBuf);
    g_menuTheme = ParseMenuThemeValue(menuThemeBuf);
    ApplyMenuTheme();
    LoadStartupPosition();
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!wc.hIconSm) wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = APP_NAME;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, APP_NAME, APP_NAME, WS_POPUP,
        g_startX, g_startY, 400, 80, NULL, NULL, hInst, NULL);
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}
