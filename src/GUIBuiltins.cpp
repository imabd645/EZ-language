#include "GUIBuiltins.h"
#include "Interpreter.h"
#include "Value.h"
#include <windows.h>
#include <commctrl.h>
#include <map>
#include <string>
#include <vector>
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")



struct WidgetStyle {
    HBRUSH bgBrush = NULL;
    COLORREF textColor = CLR_INVALID;
    HFONT hFont = NULL;
};

// Global state for GUI
struct GUIState {
    HINSTANCE hInstance;
    HWND mainWindow = NULL;
    std::map<int, HWND> handleMap;
    std::map<HWND, Value> callbacks;
    std::map<HWND, WidgetStyle> styles;
    int nextHandle = 1;
    Interpreter* currentInterpreter = nullptr;
    std::string currentTheme = "light";
    HBRUSH darkBrush = NULL;
};

static GUIState g_gui;

// Safe extractors to prevent variant crashes
inline double vNum(const Value& v) { return v.isNumber() ? v.asNumber() : 0.0; }
inline std::string vStr(const Value& v) { return v.toString(); }

// Window Procedure
LRESULT CALLBACK EZWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HWND childHwnd = (HWND)lParam;
            if (g_gui.styles.count(childHwnd)) {
                WidgetStyle& style = g_gui.styles[childHwnd];
                HDC hdc = (HDC)wParam;
                if (style.textColor != CLR_INVALID) SetTextColor(hdc, style.textColor);
                if (style.bgBrush) {
                    SetBkMode(hdc, TRANSPARENT);
                    return (LRESULT)style.bgBrush;
                }
            }
            if (g_gui.currentTheme == "dark") {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                if (!g_gui.darkBrush) g_gui.darkBrush = CreateSolidBrush(RGB(30, 30, 30));
                return (LRESULT)g_gui.darkBrush;
            }
            break;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Check for window-specific background color
            if (g_gui.styles.count(hwnd) && g_gui.styles[hwnd].bgBrush) {
                FillRect(hdc, &rect, g_gui.styles[hwnd].bgBrush);
                return 1;
            }
            
            // Fallback to theme
            if (g_gui.currentTheme == "dark") {
                if (!g_gui.darkBrush) g_gui.darkBrush = CreateSolidBrush(RGB(30, 30, 30));
                FillRect(hdc, &rect, g_gui.darkBrush);
                return 1;
            }
            break;
        }
        case WM_COMMAND: {
            HWND childHwnd = (HWND)lParam;
            if (lParam != 0) {
                if (g_gui.callbacks.count(childHwnd)) {
                    Value callback = g_gui.callbacks[childHwnd];
                    if (g_gui.currentInterpreter && callback.isCallable()) {
                        try {
                            g_gui.currentInterpreter->callFunction(callback, {}, 0, "native");
                        } catch (const std::exception& e) {
                            printf("GUI Callback Error: %s\n", e.what());
                            g_gui.currentInterpreter->printStackTrace();
                        }
                    }
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            // printf("WM_LBUTTONDOWN on HWND %p\n", hwnd);
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->code == TCN_SELCHANGE) {
                HWND tabHwnd = nmhdr->hwndFrom;
                if (g_gui.callbacks.count(tabHwnd)) {
                    Value callback = g_gui.callbacks[tabHwnd];
                    if (g_gui.currentInterpreter && callback.isCallable()) {
                        try {
                            g_gui.currentInterpreter->callFunction(callback, {}, 0, "native");
                        } catch (const std::exception& e) {
                            printf("GUI Tab Selection Error: %s\n", e.what());
                        }
                    }
                }
            }
            break;
        }
        case WM_VSCROLL:
        case WM_HSCROLL: {
            int bar = (msg == WM_VSCROLL) ? SB_VERT : SB_HORZ;
            SCROLLINFO si = { sizeof(si), SIF_ALL };
            GetScrollInfo(hwnd, bar, &si);
            int oldPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP: si.nPos -= 20; break;
                case SB_LINEDOWN: si.nPos += 20; break;
                case SB_PAGEUP: si.nPos -= si.nPage; break;
                case SB_PAGEDOWN: si.nPos += si.nPage; break;
                case SB_THUMBTRACK: si.nPos = HIWORD(wParam); break;
            }
            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, bar, &si, TRUE);
            GetScrollInfo(hwnd, bar, &si);
            if (si.nPos != oldPos) {
                int dx = (bar == SB_HORZ) ? (oldPos - si.nPos) : 0;
                int dy = (bar == SB_VERT) ? (oldPos - si.nPos) : 0;
                ScrollWindowEx(hwnd, dx, dy, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            SendMessage(hwnd, WM_VSCROLL, delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;
        }
        case WM_DESTROY:
            if (hwnd == g_gui.mainWindow) {
                PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void registerGUIBuiltins(Interpreter& interp) {
    g_gui.hInstance = GetModuleHandle(NULL);
    
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = EZWndProc;
    wc.hInstance = g_gui.hInstance;
    wc.lpszClassName = "EZWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    // Native Function Map
    
    // gui_create_window(title, w, h)
    interp.defineGlobal("gui_create_window", Value::makeNativeFunction("gui_create_window", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]);
            int w = (int)vNum(args[1]), h = (int)vNum(args[2]);
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, w, h, NULL, NULL, g_gui.hInstance, NULL);
            
            if (g_gui.mainWindow == NULL) {
                g_gui.mainWindow = hwnd;
            }

            if (g_gui.currentTheme == "dark") {
                BOOL useDarkMode = TRUE;
                DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode)); // DWMWA_USE_IMMERSIVE_DARK_MODE
            }
            
            // Enable Rounded Corners on Win11
            int cornerPreference = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference)); // DWMWA_WINDOW_CORNER_PREFERENCE

            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_button(winHandle, text, x, y, w, h, callback)
    interp.defineGlobal("gui_create_button", Value::makeNativeFunction("gui_create_button", 7,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            HWND hwnd = CreateWindow("BUTTON", text.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            g_gui.callbacks[hwnd] = args[6];
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_label(winHandle, text, x, y, w, h)
    interp.defineGlobal("gui_create_label", Value::makeNativeFunction("gui_create_label", 6,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            HWND hwnd = CreateWindow("STATIC", text.c_str(), WS_VISIBLE | WS_CHILD, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_input(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_input", Value::makeNativeFunction("gui_create_input", 5,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_dropdown(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_dropdown", Value::makeNativeFunction("gui_create_dropdown", 5,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_panel(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_panel", Value::makeNativeFunction("gui_create_panel", 5,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_scroll_panel(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_scroll_panel", Value::makeNativeFunction("gui_create_scroll_panel", 5,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL | WS_HSCROLL, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            
            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, 1000, (UINT)h, 0 };
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            si.nMax = 1000; si.nPage = (UINT)w;
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);

            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_set_scroll_range(handle, maxW, maxH)
    interp.defineGlobal("gui_set_scroll_range", Value::makeNativeFunction("gui_set_scroll_range", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int maxW = (int)vNum(args[1]);
            int maxH = (int)vNum(args[2]);
            
            RECT rect; GetClientRect(hwnd, &rect);
            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE, 0, maxH, (UINT)(rect.bottom - rect.top), 0 };
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            si.nMax = maxW; si.nPage = (UINT)(rect.right - rect.left);
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            return Value();
        }));

    // gui_clear_widgets(handle)
    interp.defineGlobal("gui_clear_widgets", Value::makeNativeFunction("gui_clear_widgets", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            
            // Collect children to avoid map/iterator issues during destruction
            std::vector<HWND> children;
            HWND child = GetWindow(parent, GW_CHILD);
            while (child) {
                children.push_back(child);
                child = GetWindow(child, GW_HWNDNEXT);
            }
            
            for (HWND h : children) {
                // Find handle in map to remove it
                for (auto it = g_gui.handleMap.begin(); it != g_gui.handleMap.end(); ++it) {
                    if (it->second == h) {
                        g_gui.handleMap.erase(it);
                        break;
                    }
                }
                g_gui.callbacks.erase(h);
                if (g_gui.styles.count(h)) {
                    if (g_gui.styles[h].bgBrush) DeleteObject(g_gui.styles[h].bgBrush);
                    if (g_gui.styles[h].hFont) DeleteObject(g_gui.styles[h].hFont);
                    g_gui.styles.erase(h);
                }
                DestroyWindow(h);
            }
            return Value();
        }));

    // gui_create_tabs(winHandle, x, y, w, h, callback)
    interp.defineGlobal("gui_create_tabs", Value::makeNativeFunction("gui_create_tabs", 6,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            g_gui.callbacks[hwnd] = args[5];
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_add_tab(handle, text)
    interp.defineGlobal("gui_add_tab", Value::makeNativeFunction("gui_add_tab", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            TCITEM tie = {0};
            tie.mask = TCIF_TEXT;
            std::string text = vStr(args[1]);
            tie.pszText = (LPSTR)text.c_str();
            int count = TabCtrl_GetItemCount(hwnd);
            TabCtrl_InsertItem(hwnd, count, &tie);
            return Value();
        }));

    // gui_get_tab_selected(handle)
    interp.defineGlobal("gui_get_tab_selected", Value::makeNativeFunction("gui_get_tab_selected", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = TabCtrl_GetCurSel(hwnd);
            return Value((double)idx);
        }));

    // gui_show(handle)
    interp.defineGlobal("gui_show", Value::makeNativeFunction("gui_show", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ShowWindow(hwnd, SW_SHOW);
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            UpdateWindow(hwnd);
            return Value();
        }));

    // gui_hide(handle)
    interp.defineGlobal("gui_hide", Value::makeNativeFunction("gui_hide", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ShowWindow(hwnd, SW_HIDE);
            return Value();
        }));

    // gui_add_dropdown_item(handle, text)
    interp.defineGlobal("gui_add_dropdown_item", Value::makeNativeFunction("gui_add_dropdown_item", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)vStr(args[1]).c_str());
            return Value();
        }));

    // gui_get_dropdown_selected(handle)
    interp.defineGlobal("gui_get_dropdown_selected", Value::makeNativeFunction("gui_get_dropdown_selected", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) return Value("");
            char buffer[256];
            SendMessage(hwnd, CB_GETLBTEXT, idx, (LPARAM)buffer);
            return Value(std::string(buffer));
        }));

    // gui_set_color(handle, bgR, bgG, bgB, textR, textG, textB)
    interp.defineGlobal("gui_set_color", Value::makeNativeFunction("gui_set_color", 7,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            COLORREF b = RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]));
            COLORREF t = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
            
            g_gui.styles[hwnd].textColor = t;
            if (g_gui.styles[hwnd].bgBrush) DeleteObject(g_gui.styles[hwnd].bgBrush);
            g_gui.styles[hwnd].bgBrush = CreateSolidBrush(b);
            
            InvalidateRect(hwnd, NULL, TRUE);
            return Value();
        }));

    interp.defineGlobal("gui_set_font", Value::makeNativeFunction("gui_set_font", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string name = vStr(args[1]);
            int size = (int)vNum(args[2]);
            
            if (g_gui.styles[hwnd].hFont) DeleteObject(g_gui.styles[hwnd].hFont);
            g_gui.styles[hwnd].hFont = CreateFont(-(int)size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name.c_str());
            
            SendMessage(hwnd, WM_SETFONT, (WPARAM)g_gui.styles[hwnd].hFont, TRUE);
            return Value();
        }));

    interp.defineGlobal("gui_get_value", Value::makeNativeFunction("gui_get_value", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            char buffer[4096];
            GetWindowText(hwnd, buffer, 4096);
            return Value(std::string(buffer));
        }));

    interp.defineGlobal("gui_set_value", Value::makeNativeFunction("gui_set_value", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SetWindowText(hwnd, vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_alert", Value::makeNativeFunction("gui_alert", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            MessageBox(NULL, vStr(args[1]).c_str(), vStr(args[0]).c_str(), MB_OK | MB_ICONINFORMATION);
            return Value();
        }));

    // gui_set_theme(name)
    interp.defineGlobal("gui_set_theme", Value::makeNativeFunction("gui_set_theme", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            g_gui.currentTheme = args[0].asString();
            for (auto const& [handle, hwnd] : g_gui.handleMap) {
                if (g_gui.currentTheme == "dark") {
                    BOOL useDarkMode = TRUE;
                    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
                } else {
                    BOOL useDarkMode = FALSE;
                    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return Value();
        }));

    // num(v)
    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try {
                return Value(std::stod(args[0].asString()));
            } catch (...) {
                return Value(0.0);
            }
        }));

    interp.defineGlobal("gui_run", Value::makeNativeFunction("gui_run", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            g_gui.currentInterpreter = &interp;
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            return Value();
        }));
}
