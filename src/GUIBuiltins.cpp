#include "GUIBuiltins.h"
#include "RuntimeContext.h"
#include "Value.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <map>
#include <string>
#include <vector>
#include <dwmapi.h>
#include <uxtheme.h>




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
    std::map<UINT_PTR, Value> timerCallbacks;
    UINT_PTR nextTimerId = 1;
    int nextHandle = 1;
    RuntimeContext* currentInterpreter = nullptr;
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
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON && g_gui.styles.count(dis->hwndItem)) {
                WidgetStyle& style = g_gui.styles[dis->hwndItem];
                HDC hdc = dis->hDC;
                RECT rc = dis->rcItem;
                
                // Background
                HBRUSH bgBrush = style.bgBrush ? style.bgBrush : GetSysColorBrush(COLOR_BTNFACE);
                FillRect(hdc, &rc, bgBrush);
                
                // Pressed effect
                if (dis->itemState & ODS_SELECTED) {
                    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FrameRect(hdc, &rc, dimBrush);
                    DeleteObject(dimBrush);
                    InflateRect(&rc, -1, -1);
                }
                
                // Border
                FrameRect(hdc, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                
                // Text
                SetBkMode(hdc, TRANSPARENT);
                if (style.textColor != CLR_INVALID)
                    SetTextColor(hdc, style.textColor);
                else
                    SetTextColor(hdc, RGB(255, 255, 255));
                
                if (style.hFont)
                    SelectObject(hdc, style.hFont);
                
                char text[256];
                GetWindowText(dis->hwndItem, text, 256);
                DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                // Focus rect
                if (dis->itemState & ODS_FOCUS) {
                    RECT focusRc = dis->rcItem;
                    InflateRect(&focusRc, -3, -3);
                    DrawFocusRect(hdc, &focusRc);
                }
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
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
            // Check if this is a slider/trackbar notification
            HWND ctrlHwnd = (HWND)lParam;
            if (ctrlHwnd != NULL && g_gui.callbacks.count(ctrlHwnd)) {
                Value callback = g_gui.callbacks[ctrlHwnd];
                if (g_gui.currentInterpreter && callback.isCallable()) {
                    try {
                        g_gui.currentInterpreter->callFunction(callback, {}, 0, "native");
                    } catch (const std::exception& e) {
                        printf("GUI Slider Error: %s\n", e.what());
                    }
                }
                return 0;
            }
            // Otherwise handle scroll panel scrolling
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
        case WM_TIMER: {
            UINT_PTR timerId = wParam;
            if (g_gui.timerCallbacks.count(timerId)) {
                Value callback = g_gui.timerCallbacks[timerId];
                if (g_gui.currentInterpreter && callback.isCallable()) {
                    try {
                        g_gui.currentInterpreter->callFunction(callback, {}, 0, "native");
                    } catch (const std::exception& e) {
                        printf("GUI Timer Error: %s\n", e.what());
                    }
                }
            }
            return 0;
        }
        case WM_DESTROY:
            if (hwnd == g_gui.mainWindow) {
                // Kill all timers
                for (auto& [id, cb] : g_gui.timerCallbacks) {
                    KillTimer(hwnd, id);
                }
                g_gui.timerCallbacks.clear();
                PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void registerGUIBuiltins(RuntimeContext& interp) {
    g_gui.currentInterpreter = &interp;
    g_gui.hInstance = GetModuleHandle(NULL);
    
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS;
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
    
    // gui_create_window(title, [x, y], w, h)
    interp.defineGlobal("gui_create_window", Value::makeNativeFunction("gui_create_window", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() != 3 && args.size() != 5) {
                interp.runtimeError("Expected 3 or 5 arguments", 0, "native");
                return Value();
            }
            std::string title = vStr(args[0]);
            int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w, h;
            if (args.size() == 3) {
                w = (int)vNum(args[1]);
                h = (int)vNum(args[2]);
            } else {
                x = (int)vNum(args[1]);
                y = (int)vNum(args[2]);
                w = (int)vNum(args[3]);
                h = (int)vNum(args[4]);
            }
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                x, y, w, h, NULL, NULL, g_gui.hInstance, NULL);
            
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_dropdown(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_dropdown", Value::makeNativeFunction("gui_create_dropdown", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_panel(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_panel", Value::makeNativeFunction("gui_create_panel", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_scroll_panel(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_scroll_panel", Value::makeNativeFunction("gui_create_scroll_panel", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = TabCtrl_GetCurSel(hwnd);
            return Value((double)idx);
        }));

    // gui_show(handle)
    interp.defineGlobal("gui_show", Value::makeNativeFunction("gui_show", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ShowWindow(hwnd, SW_SHOW);
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            UpdateWindow(hwnd);
            return Value();
        }));

    // gui_hide(handle)
    interp.defineGlobal("gui_hide", Value::makeNativeFunction("gui_hide", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ShowWindow(hwnd, SW_HIDE);
            return Value();
        }));

    // gui_add_dropdown_item(handle, text)
    interp.defineGlobal("gui_add_dropdown_item", Value::makeNativeFunction("gui_add_dropdown_item", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)vStr(args[1]).c_str());
            return Value();
        }));

    // gui_get_dropdown_selected(handle)
    interp.defineGlobal("gui_get_dropdown_selected", Value::makeNativeFunction("gui_get_dropdown_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) return Value("");
            char buffer[256];
            SendMessage(hwnd, CB_GETLBTEXT, idx, (LPARAM)buffer);
            return Value(std::string(buffer));
        }));

    // gui_set_color(handle, bgR, bgG, bgB, textR, textG, textB)
    interp.defineGlobal("gui_set_color", Value::makeNativeFunction("gui_set_color", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            COLORREF b = RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]));
            COLORREF t = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
            
            g_gui.styles[hwnd].textColor = t;
            if (g_gui.styles[hwnd].bgBrush) DeleteObject(g_gui.styles[hwnd].bgBrush);
            g_gui.styles[hwnd].bgBrush = CreateSolidBrush(b);
            
            // If this is a BUTTON control, make it owner-drawn so colors take effect
            char className[32];
            GetClassName(hwnd, className, 32);
            if (std::string(className) == "Button") {
                LONG style = GetWindowLong(hwnd, GWL_STYLE);
                // Only convert push buttons (not checkboxes/radios)
                int btnType = style & 0xF;
                if (btnType == BS_PUSHBUTTON || btnType == BS_DEFPUSHBUTTON) {
                    style = (style & ~0xF) | BS_OWNERDRAW;
                    SetWindowLong(hwnd, GWL_STYLE, style);
                }
            }
            
            InvalidateRect(hwnd, NULL, TRUE);
            return Value();
        }));

    interp.defineGlobal("gui_set_font", Value::makeNativeFunction("gui_set_font", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string name = vStr(args[1]);
            int size = (int)vNum(args[2]);
            
            if (g_gui.styles[hwnd].hFont) DeleteObject(g_gui.styles[hwnd].hFont);
            g_gui.styles[hwnd].hFont = CreateFont(-(int)size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name.c_str());
            
            SendMessage(hwnd, WM_SETFONT, (WPARAM)g_gui.styles[hwnd].hFont, TRUE);
            return Value();
        }));

    interp.defineGlobal("gui_get_value", Value::makeNativeFunction("gui_get_value", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            char buffer[4096];
            GetWindowText(hwnd, buffer, 4096);
            return Value(std::string(buffer));
        }));

    interp.defineGlobal("gui_set_value", Value::makeNativeFunction("gui_set_value", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SetWindowText(hwnd, vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_alert", Value::makeNativeFunction("gui_alert", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            MessageBox(NULL, vStr(args[1]).c_str(), vStr(args[0]).c_str(), MB_OK | MB_ICONINFORMATION);
            return Value();
        }));

    // gui_set_theme(name)
    interp.defineGlobal("gui_set_theme", Value::makeNativeFunction("gui_set_theme", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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

    // gui_set_callback(handle, callback) - set/update callback for any widget
    interp.defineGlobal("gui_set_callback", Value::makeNativeFunction("gui_set_callback", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            g_gui.callbacks[hwnd] = args[1];
            return Value();
        }));

    // gui_create_checkbox(winHandle, text, x, y, w, h)
    interp.defineGlobal("gui_create_checkbox", Value::makeNativeFunction("gui_create_checkbox", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            HWND hwnd = CreateWindow("BUTTON", text.c_str(), WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_get_checked(handle) - returns 1 if checked, 0 if not
    interp.defineGlobal("gui_get_checked", Value::makeNativeFunction("gui_get_checked", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            return Value((double)(SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0));
        }));

    // gui_set_checked(handle, state) - set checkbox state
    interp.defineGlobal("gui_set_checked", Value::makeNativeFunction("gui_set_checked", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, BM_SETCHECK, (int)vNum(args[1]) ? BST_CHECKED : BST_UNCHECKED, 0);
            return Value();
        }));

    // gui_create_radio(winHandle, text, x, y, w, h, isGroupStart)
    interp.defineGlobal("gui_create_radio", Value::makeNativeFunction("gui_create_radio", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            bool groupStart = (int)vNum(args[6]) != 0;
            DWORD style = WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON;
            if (groupStart) style |= WS_GROUP;
            HWND hwnd = CreateWindow("BUTTON", text.c_str(), style,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_create_slider(winHandle, x, y, w, h, min, max)
    interp.defineGlobal("gui_create_slider", Value::makeNativeFunction("gui_create_slider", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            int minVal = (int)vNum(args[5]), maxVal = (int)vNum(args[6]);
            HWND hwnd = CreateWindow(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            SendMessage(hwnd, TBM_SETRANGE, TRUE, MAKELPARAM(minVal, maxVal));
            SendMessage(hwnd, TBM_SETPOS, TRUE, minVal);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_get_slider(handle)
    interp.defineGlobal("gui_get_slider", Value::makeNativeFunction("gui_get_slider", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            return Value((double)SendMessage(hwnd, TBM_GETPOS, 0, 0));
        }));

    // gui_set_slider(handle, pos)
    interp.defineGlobal("gui_set_slider", Value::makeNativeFunction("gui_set_slider", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, TBM_SETPOS, TRUE, (int)vNum(args[1]));
            return Value();
        }));

    // gui_create_progress(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_progress", Value::makeNativeFunction("gui_create_progress", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow(PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            SendMessage(hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_set_progress(handle, value 0-100)
    interp.defineGlobal("gui_set_progress", Value::makeNativeFunction("gui_set_progress", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, PBM_SETPOS, (int)vNum(args[1]), 0);
            return Value();
        }));

    // gui_set_progress_range(handle, min, max)
    interp.defineGlobal("gui_set_progress_range", Value::makeNativeFunction("gui_set_progress_range", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, PBM_SETRANGE, 0, MAKELPARAM((int)vNum(args[1]), (int)vNum(args[2])));
            return Value();
        }));

    // gui_create_listview(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_listview", Value::makeNativeFunction("gui_create_listview", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow(WC_LISTVIEW, "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_listview_add_column(handle, text, width)
    interp.defineGlobal("gui_listview_add_column", Value::makeNativeFunction("gui_listview_add_column", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int width = (int)vNum(args[2]);
            int colCount = Header_GetItemCount(ListView_GetHeader(hwnd));
            LVCOLUMN col = {0};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.cx = width;
            col.pszText = (LPSTR)text.c_str();
            col.iSubItem = colCount;
            ListView_InsertColumn(hwnd, colCount, &col);
            return Value();
        }));

    // gui_listview_add_row(handle, itemsArray)
    interp.defineGlobal("gui_listview_add_row", Value::makeNativeFunction("gui_listview_add_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (!args[1].isArray()) return Value();
            const auto& items = args[1].asArray();
            int rowIdx = ListView_GetItemCount(hwnd);
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = rowIdx;
            lvi.iSubItem = 0;
            std::string firstText = items.empty() ? "" : items[0].toString();
            lvi.pszText = (LPSTR)firstText.c_str();
            ListView_InsertItem(hwnd, &lvi);
            for (size_t i = 1; i < items.size(); i++) {
                std::string cellText = items[i].toString();
                ListView_SetItemText(hwnd, rowIdx, (int)i, (LPSTR)cellText.c_str());
            }
            return Value((double)rowIdx);
        }));

    // gui_listview_clear(handle)
    interp.defineGlobal("gui_listview_clear", Value::makeNativeFunction("gui_listview_clear", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ListView_DeleteAllItems(hwnd);
            return Value();
        }));

    // gui_listview_get_selected(handle) - returns selected row index or -1
    interp.defineGlobal("gui_listview_get_selected", Value::makeNativeFunction("gui_listview_get_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
            return Value((double)idx);
        }));

    // gui_listview_get_row(handle, index) - returns array of cell values
    interp.defineGlobal("gui_listview_get_row", Value::makeNativeFunction("gui_listview_get_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int row = (int)vNum(args[1]);
            int colCount = Header_GetItemCount(ListView_GetHeader(hwnd));
            std::vector<Value> cells;
            for (int i = 0; i < colCount; i++) {
                char buf[256] = {0};
                ListView_GetItemText(hwnd, row, i, buf, 256);
                cells.push_back(Value(std::string(buf)));
            }
            return Value::makeArray(cells);
        }));

    // gui_listview_remove_row(handle, index)
    interp.defineGlobal("gui_listview_remove_row", Value::makeNativeFunction("gui_listview_remove_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ListView_DeleteItem(hwnd, (int)vNum(args[1]));
            return Value();
        }));

    // num(v)
    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            try {
                return Value(std::stod(args[0].asString()));
            } catch (...) {
                return Value(0.0);
            }
        }));

    // gui_set_timer(winHandle, intervalMs, callback)
    interp.defineGlobal("gui_set_timer", Value::makeNativeFunction("gui_set_timer", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int interval = (int)vNum(args[1]);
            UINT_PTR timerId = g_gui.nextTimerId++;
            g_gui.timerCallbacks[timerId] = args[2];
            SetTimer(hwnd, timerId, interval, NULL);
            return Value((double)timerId);
        }));

    // gui_kill_timer(winHandle, timerId)
    interp.defineGlobal("gui_kill_timer", Value::makeNativeFunction("gui_kill_timer", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            UINT_PTR timerId = (UINT_PTR)vNum(args[1]);
            KillTimer(hwnd, timerId);
            g_gui.timerCallbacks.erase(timerId);
            return Value();
        }));

    // ===== DIALOG HELPERS =====

    // gui_dialog_confirm(title, message) -> true/false
    interp.defineGlobal("gui_dialog_confirm", Value::makeNativeFunction("gui_dialog_confirm", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]);
            std::string msg = vStr(args[1]);
            int result = MessageBox(g_gui.mainWindow, msg.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
            return Value(result == IDYES);
        }));

    // gui_dialog_input(title, message) -> string or nil
    interp.defineGlobal("gui_dialog_input", Value::makeNativeFunction("gui_dialog_input", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]);
            std::string msg = vStr(args[1]);
            // Simple input via a prompt dialog isn't built-in to Win32
            // Use MessageBox with a note to use gui forms instead
            MessageBox(g_gui.mainWindow, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
            return Value();
        }));

    // gui_dialog_open_file(title, filter) -> filepath string or ""
    interp.defineGlobal("gui_dialog_open_file", Value::makeNativeFunction("gui_dialog_open_file", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]);
            std::string rawFilter = vStr(args[1]);
            
            // Convert filter "*.png" to OPENFILENAME format "Images\0*.png\0All\0*.*\0"
            std::string filter = "Files\0" + rawFilter + "\0All Files\0*.*\0";
            // Need double null termination
            std::vector<char> filterBuf(filter.begin(), filter.end());
            filterBuf.push_back('\0');
            
            char filename[MAX_PATH] = {0};
            OPENFILENAME ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_gui.mainWindow;
            ofn.lpstrFilter = filterBuf.data();
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title.c_str();
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            
            if (GetOpenFileName(&ofn)) {
                return Value(std::string(filename));
            }
            return Value("");
        }));

    // gui_dialog_save_file(title, filter) -> filepath string or ""
    interp.defineGlobal("gui_dialog_save_file", Value::makeNativeFunction("gui_dialog_save_file", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]);
            std::string rawFilter = vStr(args[1]);
            
            std::string filter = "Files\0" + rawFilter + "\0All Files\0*.*\0";
            std::vector<char> filterBuf(filter.begin(), filter.end());
            filterBuf.push_back('\0');
            
            char filename[MAX_PATH] = {0};
            OPENFILENAME ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_gui.mainWindow;
            ofn.lpstrFilter = filterBuf.data();
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title.c_str();
            ofn.Flags = OFN_OVERWRITEPROMPT;
            
            if (GetSaveFileName(&ofn)) {
                return Value(std::string(filename));
            }
            return Value("");
        }));

    // gui_dialog_open_folder() -> folder path string or ""
    interp.defineGlobal("gui_dialog_open_folder", Value::makeNativeFunction("gui_dialog_open_folder", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            BROWSEINFO bi = {0};
            bi.hwndOwner = g_gui.mainWindow;
            bi.lpszTitle = "Select Folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            
            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl) {
                char path[MAX_PATH];
                SHGetPathFromIDList(pidl, path);
                CoTaskMemFree(pidl);
                return Value(std::string(path));
            }
            return Value("");
        }));

    // gui_dialog_color_picker() -> [r, g, b] or nil
    interp.defineGlobal("gui_dialog_color_picker", Value::makeNativeFunction("gui_dialog_color_picker", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            static COLORREF customColors[16] = {0};
            CHOOSECOLOR cc = {0};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = g_gui.mainWindow;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            
            if (ChooseColor(&cc)) {
                std::vector<Value> rgb;
                rgb.push_back(Value((double)GetRValue(cc.rgbResult)));
                rgb.push_back(Value((double)GetGValue(cc.rgbResult)));
                rgb.push_back(Value((double)GetBValue(cc.rgbResult)));
                return Value::makeArray(rgb);
            }
            return Value();
        }));

    // ===== TREEVIEW =====

    // gui_create_treeview(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_treeview", Value::makeNativeFunction("gui_create_treeview", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow(WC_TREEVIEW, "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_treeview_add(handle, parentItem, text) -> item handle (as double)
    // parentItem = 0 for root
    interp.defineGlobal("gui_treeview_add", Value::makeNativeFunction("gui_treeview_add", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HTREEITEM parentItem = (HTREEITEM)(intptr_t)(int64_t)vNum(args[1]);
            if (parentItem == 0) parentItem = TVI_ROOT;
            std::string text = vStr(args[2]);
            
            TVINSERTSTRUCT tvins = {0};
            tvins.hParent = parentItem;
            tvins.hInsertAfter = TVI_LAST;
            tvins.item.mask = TVIF_TEXT;
            tvins.item.pszText = (LPSTR)text.c_str();
            HTREEITEM item = TreeView_InsertItem(hwnd, &tvins);
            return Value((double)(intptr_t)item);
        }));

    // gui_treeview_get_selected(handle) -> text of selected item
    interp.defineGlobal("gui_treeview_get_selected", Value::makeNativeFunction("gui_treeview_get_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HTREEITEM sel = TreeView_GetSelection(hwnd);
            if (!sel) return Value("");
            char buf[256] = {0};
            TVITEM tvi = {0};
            tvi.mask = TVIF_TEXT;
            tvi.hItem = sel;
            tvi.pszText = buf;
            tvi.cchTextMax = 256;
            TreeView_GetItem(hwnd, &tvi);
            return Value(std::string(buf));
        }));

    // gui_treeview_clear(handle)
    interp.defineGlobal("gui_treeview_clear", Value::makeNativeFunction("gui_treeview_clear", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            TreeView_DeleteAllItems(hwnd);
            return Value();
        }));

    // ===== DATEPICKER =====

    // gui_create_datepicker(winHandle, x, y, w, h)
    interp.defineGlobal("gui_create_datepicker", Value::makeNativeFunction("gui_create_datepicker", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindow(DATETIMEPICK_CLASS, "",
                WS_CHILD | WS_VISIBLE | DTS_SHORTDATECENTURYFORMAT,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_datepicker_get(handle) -> "YYYY-MM-DD"
    interp.defineGlobal("gui_datepicker_get", Value::makeNativeFunction("gui_datepicker_get", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SYSTEMTIME st;
            DateTime_GetSystemtime(hwnd, &st);
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            return Value(std::string(buf));
        }));

    // gui_datepicker_set(handle, year, month, day)
    interp.defineGlobal("gui_datepicker_set", Value::makeNativeFunction("gui_datepicker_set", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SYSTEMTIME st = {0};
            st.wYear = (WORD)vNum(args[1]);
            st.wMonth = (WORD)vNum(args[2]);
            st.wDay = (WORD)vNum(args[3]);
            DateTime_SetSystemtime(hwnd, GDT_VALID, &st);
            return Value();
        }));

    // ===== SPINNER / NUMERIC INPUT =====

    // gui_create_spinner(winHandle, x, y, w, h, min, max)
    interp.defineGlobal("gui_create_spinner", Value::makeNativeFunction("gui_create_spinner", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            int minVal = (int)vNum(args[5]), maxVal = (int)vNum(args[6]);
            
            // Create edit control (the buddy)
            HWND editHwnd = CreateWindow("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT,
                x, y, w - 20, h, parent, NULL, g_gui.hInstance, NULL);
            
            // Create up-down control
            HWND udHwnd = CreateWindow(UPDOWN_CLASS, "",
                WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                0, 0, 0, 0, parent, NULL, g_gui.hInstance, NULL);
            
            SendMessage(udHwnd, UDM_SETBUDDY, (WPARAM)editHwnd, 0);
            SendMessage(udHwnd, UDM_SETRANGE32, minVal, maxVal);
            SendMessage(udHwnd, UDM_SETPOS32, 0, minVal);
            
            // Store the edit control handle (that's what the user interacts with)
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = editHwnd;
            // Also store the updown handle internally
            int udHandle = g_gui.nextHandle++;
            g_gui.handleMap[udHandle] = udHwnd;
            return Value((double)handle);
        }));

    // gui_spinner_get(handle) -> number
    interp.defineGlobal("gui_spinner_get", Value::makeNativeFunction("gui_spinner_get", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            char buf[64];
            GetWindowText(hwnd, buf, 64);
            try { return Value(std::stod(buf)); } catch (...) { return Value(0.0); }
        }));

    // gui_spinner_set(handle, value)
    interp.defineGlobal("gui_spinner_set", Value::makeNativeFunction("gui_spinner_set", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SetWindowText(hwnd, std::to_string((int)vNum(args[1])).c_str());
            return Value();
        }));

    // ===== IMAGE WIDGET =====

    // gui_create_image(winHandle, path, x, y, w, h)
    interp.defineGlobal("gui_create_image", Value::makeNativeFunction("gui_create_image", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::string path = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            
            HWND hwnd = CreateWindow("STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,
                x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            
            HBITMAP hbmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, w, h, LR_LOADFROMFILE);
            if (hbmp) {
                SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
            }
            
            int handle = g_gui.nextHandle++;
            g_gui.handleMap[handle] = hwnd;
            return Value((double)handle);
        }));

    // gui_set_image(handle, path)
    interp.defineGlobal("gui_set_image", Value::makeNativeFunction("gui_set_image", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string path = vStr(args[1]);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBITMAP oldBmp = (HBITMAP)SendMessage(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
            HBITMAP hbmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, rc.right, rc.bottom, LR_LOADFROMFILE);
            if (hbmp) {
                SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
                if (oldBmp) DeleteObject(oldBmp);
            }
            return Value();
        }));

    // ===== WIDGET POSITION/SIZE =====

    // gui_set_pos(handle, x, y, w, h)
    interp.defineGlobal("gui_set_pos", Value::makeNativeFunction("gui_set_pos", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER);
            return Value();
        }));

    // gui_set_enabled(handle, enabled)
    interp.defineGlobal("gui_set_enabled", Value::makeNativeFunction("gui_set_enabled", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            EnableWindow(hwnd, (int)vNum(args[1]) != 0);
            return Value();
        }));

    interp.defineGlobal("gui_run", Value::makeNativeFunction("gui_run", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            g_gui.currentInterpreter = &interp;
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            return Value();
        }));

    // ===== GAME / DRAWING PRIMITIVES =====

    // gui_is_key_down(vkCode)
    interp.defineGlobal("gui_is_key_down", Value::makeNativeFunction("gui_is_key_down", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int vk = (int)vNum(args[0]);
            return Value((double)((GetAsyncKeyState(vk) & 0x8000) ? 1 : 0));
        }));

    // gui_draw_rect(handle, x, y, w, h, r, g, b)
    interp.defineGlobal("gui_draw_rect", Value::makeNativeFunction("gui_draw_rect", 8,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            COLORREF color = RGB((int)vNum(args[5]), (int)vNum(args[6]), (int)vNum(args[7]));
            
            HDC hdc = GetDC(hwnd);
            HBRUSH brush = CreateSolidBrush(color);
            RECT rect = { x, y, x + w, y + h };
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    // gui_draw_circle(handle, x, y, r, r2, g, b)
    interp.defineGlobal("gui_draw_circle", Value::makeNativeFunction("gui_draw_circle", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), radius = (int)vNum(args[3]);
            COLORREF color = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
            
            HDC hdc = GetDC(hwnd);
            HBRUSH brush = CreateSolidBrush(color);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            
            Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
            
            SelectObject(hdc, oldBrush);
            DeleteObject(brush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    // gui_clear(handle, r, g, b)
    interp.defineGlobal("gui_clear", Value::makeNativeFunction("gui_clear", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            COLORREF color = RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]));
            
            HDC hdc = GetDC(hwnd);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    // gui_get_client_size(handle) -> [w, h]
    interp.defineGlobal("gui_get_client_size", Value::makeNativeFunction("gui_get_client_size", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT rect;
            GetClientRect(hwnd, &rect);
            std::vector<Value> size = { Value((double)rect.right), Value((double)rect.bottom) };
            return Value::makeArray(size);
        }));

    // gui_draw_text(handle, text, x, y, size, r, g, b)
    interp.defineGlobal("gui_draw_text", Value::makeNativeFunction("gui_draw_text", 8,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), size = (int)vNum(args[4]);
            COLORREF color = RGB((int)vNum(args[5]), (int)vNum(args[6]), (int)vNum(args[7]));
            
            HDC hdc = GetDC(hwnd);
            HFONT hFont = CreateFont(-(int)size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            SetTextColor(hdc, color);
            SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, x, y, text.c_str(), (int)text.length());
            
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));
}
