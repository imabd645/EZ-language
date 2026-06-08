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
#include <algorithm>

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
    std::map<HWND, int> reverseHandleMap; // For O(1) clear_widgets lookup
    std::map<HWND, Value> callbacks;      // Legacy callbacks
    std::map<HWND, std::map<std::string, Value>> eventCallbacks; // New multi-event system
    std::map<HWND, WidgetStyle> styles;
    std::map<HWND, HDC> memDCs;          // Double buffering
    std::map<HWND, HBITMAP> memBitmaps;  // Double buffering
    std::map<UINT_PTR, Value> timerCallbacks;
    std::map<HWND, std::vector<UINT_PTR>> hwndTimers; // Track timers per window
    std::map<int, Value> menuCallbacks;
    std::map<HWND, POINT> minSizes;
    
    UINT_PTR nextTimerId = 1;
    int nextHandle = 1;
    int nextMenuId = 1000;
    
    RuntimeContext* currentInterpreter = nullptr;
    std::string currentTheme = "light";
    HBRUSH darkBrush = NULL;
};

static GUIState g_gui;

// Safe extractors
inline double vNum(const Value& v) { return v.isNumber() ? v.asNumber() : 0.0; }
inline std::string vStr(const Value& v) { return v.toString(); }

// Helper: Fire event safely
static Value fireEventCallback(HWND hwnd, const std::string& eventName, const std::vector<Value>& args = {}) {
    if (g_gui.eventCallbacks.count(hwnd) && g_gui.eventCallbacks[hwnd].count(eventName)) {
        Value callback = g_gui.eventCallbacks[hwnd][eventName];
        if (g_gui.currentInterpreter && callback.isCallable()) {
            try {
                return g_gui.currentInterpreter->callFunction(callback, args, 0, "native");
            } catch (const std::exception& e) {
                printf("GUI Event '%s' Error: %s\n", eventName.c_str(), e.what());
                g_gui.currentInterpreter->printStackTrace();
            }
        }
    }
    return Value();
}

static bool hasEvent(HWND hwnd, const std::string& eventName) {
    return g_gui.eventCallbacks.count(hwnd) && g_gui.eventCallbacks[hwnd].count(eventName);
}

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
            if (g_gui.styles.count(hwnd) && g_gui.styles[hwnd].bgBrush) {
                FillRect(hdc, &rect, g_gui.styles[hwnd].bgBrush);
                return 1;
            }
            if (g_gui.currentTheme == "dark") {
                if (!g_gui.darkBrush) g_gui.darkBrush = CreateSolidBrush(RGB(30, 30, 30));
                FillRect(hdc, &rect, g_gui.darkBrush);
                return 1;
            }
            break;
        }
        case WM_COMMAND: {
            HWND childHwnd = (HWND)lParam;
            WORD notifCode = HIWORD(wParam);
            WORD menuId = LOWORD(wParam);

            if (childHwnd != NULL) {
                // Control events
                if (notifCode == EN_CHANGE) {
                    fireEventCallback(childHwnd, "change");
                } else if (notifCode == BN_CLICKED || notifCode == CBN_SELCHANGE) {
                    // Legacy click
                    if (g_gui.callbacks.count(childHwnd)) {
                        Value callback = g_gui.callbacks[childHwnd];
                        if (g_gui.currentInterpreter && callback.isCallable()) {
                            try { g_gui.currentInterpreter->callFunction(callback, {}, 0, "native"); } 
                            catch (const std::exception& e) { printf("GUI Click Error: %s\n", e.what()); }
                        }
                    }
                    fireEventCallback(childHwnd, "click");
                }
            } else if (notifCode == 0) {
                // Menu selection
                if (g_gui.menuCallbacks.count(menuId)) {
                    Value callback = g_gui.menuCallbacks[menuId];
                    if (g_gui.currentInterpreter && callback.isCallable()) {
                        try { g_gui.currentInterpreter->callFunction(callback, {}, 0, "native"); } 
                        catch (const std::exception& e) { printf("GUI Menu Error: %s\n", e.what()); }
                    }
                }
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            HWND ctrl = nmhdr->hwndFrom;

            if (nmhdr->code == TCN_SELCHANGE) {
                if (g_gui.callbacks.count(ctrl)) {
                    Value callback = g_gui.callbacks[ctrl];
                    if (g_gui.currentInterpreter && callback.isCallable()) {
                        try { g_gui.currentInterpreter->callFunction(callback, {}, 0, "native"); } 
                        catch (const std::exception& e) { printf("GUI Tab Error: %s\n", e.what()); }
                    }
                }
                fireEventCallback(ctrl, "change");
            } else if (nmhdr->code == TVN_SELCHANGED || nmhdr->code == DTN_DATETIMECHANGE) {
                fireEventCallback(ctrl, "change");
            } else if (nmhdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW nmlv = (LPNMLISTVIEW)lParam;
                if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
                    fireEventCallback(ctrl, "change");
                }
            }
            break;
        }
        case WM_DRAWITEM: {
            // Unchanged from existing (omitted for brevity, assume original logic here)
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON && g_gui.styles.count(dis->hwndItem)) {
                WidgetStyle& style = g_gui.styles[dis->hwndItem];
                HDC hdc = dis->hDC;
                RECT rc = dis->rcItem;
                HBRUSH bgBrush = style.bgBrush ? style.bgBrush : GetSysColorBrush(COLOR_BTNFACE);
                FillRect(hdc, &rc, bgBrush);
                if (dis->itemState & ODS_SELECTED) {
                    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FrameRect(hdc, &rc, dimBrush);
                    DeleteObject(dimBrush);
                    InflateRect(&rc, -1, -1);
                }
                FrameRect(hdc, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                SetBkMode(hdc, TRANSPARENT);
                if (style.textColor != CLR_INVALID) SetTextColor(hdc, style.textColor);
                else SetTextColor(hdc, RGB(255, 255, 255));
                if (style.hFont) SelectObject(hdc, style.hFont);
                char text[256];
                GetWindowText(dis->hwndItem, text, 256);
                DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (dis->itemState & ODS_FOCUS) {
                    RECT focusRc = dis->rcItem;
                    InflateRect(&focusRc, -3, -3);
                    DrawFocusRect(hdc, &focusRc);
                }
                return TRUE;
            }
            break;
        }
        case WM_VSCROLL:
        case WM_HSCROLL: {
            HWND ctrlHwnd = (HWND)lParam;
            if (ctrlHwnd != NULL) {
                if (g_gui.callbacks.count(ctrlHwnd)) {
                    Value callback = g_gui.callbacks[ctrlHwnd];
                    if (g_gui.currentInterpreter && callback.isCallable()) {
                        try { g_gui.currentInterpreter->callFunction(callback, {}, 0, "native"); } 
                        catch (const std::exception& e) { printf("GUI Slider Error: %s\n", e.what()); }
                    }
                }
                fireEventCallback(ctrlHwnd, "change");
                if (g_gui.eventCallbacks.count(ctrlHwnd) || g_gui.callbacks.count(ctrlHwnd)) {
                    return 0;
                }
            }
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
                    try { g_gui.currentInterpreter->callFunction(callback, {}, 0, "native"); } 
                    catch (const std::exception& e) { printf("GUI Timer Error: %s\n", e.what()); }
                }
            }
            return 0;
        }
        case WM_SIZE: {
            fireEventCallback(hwnd, "resize", { Value((double)LOWORD(lParam)), Value((double)HIWORD(lParam)) });
            break;
        }
        case WM_SETFOCUS: {
            fireEventCallback(hwnd, "focus");
            break;
        }
        case WM_KILLFOCUS: {
            fireEventCallback(hwnd, "blur");
            break;
        }
        case WM_RBUTTONDOWN: {
            fireEventCallback(hwnd, "rclick", { Value((double)LOWORD(lParam)), Value((double)HIWORD(lParam)) });
            break;
        }
        case WM_LBUTTONDBLCLK: {
            fireEventCallback(hwnd, "dblclick", { Value((double)LOWORD(lParam)), Value((double)HIWORD(lParam)) });
            break;
        }
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_HOVER | TME_LEAVE, hwnd, 100 };
            TrackMouseEvent(&tme);
            fireEventCallback(hwnd, "hover", { Value((double)LOWORD(lParam)), Value((double)HIWORD(lParam)) });
            break;
        }
        case WM_KEYDOWN: {
            char keyChar = MapVirtualKey((UINT)wParam, MAPVK_VK_TO_CHAR);
            fireEventCallback(hwnd, "keydown", { Value((double)wParam), Value(std::string(1, keyChar)) });
            break;
        }
        case WM_KEYUP: {
            fireEventCallback(hwnd, "keyup", { Value((double)wParam) });
            break;
        }
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            if (count > 0) {
                char path[MAX_PATH];
                DragQueryFile(hDrop, 0, path, MAX_PATH);
                fireEventCallback(hwnd, "drop", { Value(std::string(path)) });
            }
            DragFinish(hDrop);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            if (g_gui.minSizes.count(hwnd)) {
                LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
                mmi->ptMinTrackSize.x = g_gui.minSizes[hwnd].x;
                mmi->ptMinTrackSize.y = g_gui.minSizes[hwnd].y;
            }
            break;
        }
        case WM_CLOSE: {
            if (hasEvent(hwnd, "close")) {
                Value result = fireEventCallback(hwnd, "close");
                if (result.isBool() && !result.asBool()) return 0; // Veto close
            }
            if (hwnd == g_gui.mainWindow) {
                for (auto& [id, cb] : g_gui.timerCallbacks) KillTimer(hwnd, id);
                g_gui.timerCallbacks.clear();
                PostQuitMessage(0);
            } else {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY: {
            // Clean up resources for this window
            if (g_gui.reverseHandleMap.count(hwnd)) {
                int handle = g_gui.reverseHandleMap[hwnd];
                g_gui.handleMap.erase(handle);
                g_gui.reverseHandleMap.erase(hwnd);
            }
            g_gui.callbacks.erase(hwnd);
            g_gui.eventCallbacks.erase(hwnd);
            g_gui.minSizes.erase(hwnd);
            if (g_gui.hwndTimers.count(hwnd)) {
                for (UINT_PTR id : g_gui.hwndTimers[hwnd]) {
                    KillTimer(hwnd, id);
                    g_gui.timerCallbacks.erase(id);
                }
                g_gui.hwndTimers.erase(hwnd);
            }
            if (g_gui.styles.count(hwnd)) {
                if (g_gui.styles[hwnd].bgBrush) DeleteObject(g_gui.styles[hwnd].bgBrush);
                if (g_gui.styles[hwnd].hFont) DeleteObject(g_gui.styles[hwnd].hFont);
                g_gui.styles.erase(hwnd);
            }
            if (hwnd == g_gui.mainWindow) {
                PostQuitMessage(0);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Helper to register widget handles
static double registerWidgetHandle(HWND hwnd) {
    int handle = g_gui.nextHandle++;
    g_gui.handleMap[handle] = hwnd;
    g_gui.reverseHandleMap[hwnd] = handle;
    return (double)handle;
}

void registerGUIBuiltins(RuntimeContext& interp) {
    g_gui.currentInterpreter = &interp;
    g_gui.hInstance = GetModuleHandle(NULL);
    
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = EZWndProc;
    wc.hInstance = g_gui.hInstance;
    wc.lpszClassName = "EZWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    // ============================================================================
    // UNIVERSAL EVENT SYSTEM
    // ============================================================================
    interp.defineGlobal("gui_on", Value::makeNativeFunction("gui_on", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string eventName = vStr(args[1]);
            g_gui.eventCallbacks[hwnd][eventName] = args[2];
            return Value();
        }));

    // ============================================================================
    // WINDOW MANAGEMENT
    // ============================================================================
    interp.defineGlobal("gui_set_title", Value::makeNativeFunction("gui_set_title", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SetWindowText(hwnd, vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_resize", Value::makeNativeFunction("gui_resize", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SetWindowPos(hwnd, NULL, 0, 0, (int)vNum(args[1]), (int)vNum(args[2]), SWP_NOMOVE | SWP_NOZORDER);
            return Value();
        }));

    interp.defineGlobal("gui_get_size", Value::makeNativeFunction("gui_get_size", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT r; GetWindowRect(hwnd, &r);
            return Value::makeArray({ Value((double)(r.right - r.left)), Value((double)(r.bottom - r.top)) });
        }));

    interp.defineGlobal("gui_get_pos", Value::makeNativeFunction("gui_get_pos", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT r; GetWindowRect(hwnd, &r);
            return Value::makeArray({ Value((double)r.left), Value((double)r.top) });
        }));

    interp.defineGlobal("gui_center_window", Value::makeNativeFunction("gui_center_window", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT r; GetWindowRect(hwnd, &r);
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(hwnd, NULL, (sw - w) / 2, (sh - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return Value();
        }));

    interp.defineGlobal("gui_set_always_on_top", Value::makeNativeFunction("gui_set_always_on_top", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HWND pos = (int)vNum(args[1]) ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(hwnd, pos, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return Value();
        }));

    interp.defineGlobal("gui_set_opacity", Value::makeNativeFunction("gui_set_opacity", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)vNum(args[1]), LWA_ALPHA);
            return Value();
        }));

    interp.defineGlobal("gui_minimize", Value::makeNativeFunction("gui_minimize", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ShowWindow(g_gui.handleMap[(int)vNum(args[0])], SW_MINIMIZE); return Value(); }));
    interp.defineGlobal("gui_maximize", Value::makeNativeFunction("gui_maximize", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ShowWindow(g_gui.handleMap[(int)vNum(args[0])], SW_MAXIMIZE); return Value(); }));
    interp.defineGlobal("gui_restore", Value::makeNativeFunction("gui_restore", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ShowWindow(g_gui.handleMap[(int)vNum(args[0])], SW_RESTORE); return Value(); }));

    interp.defineGlobal("gui_set_icon", Value::makeNativeFunction("gui_set_icon", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HICON hIcon = (HICON)LoadImage(NULL, vStr(args[1]).c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            if (hIcon) SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            return Value();
        }));

    interp.defineGlobal("gui_set_min_size", Value::makeNativeFunction("gui_set_min_size", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            g_gui.minSizes[hwnd] = { (LONG)vNum(args[1]), (LONG)vNum(args[2]) };
            return Value();
        }));

    interp.defineGlobal("gui_accept_drop", Value::makeNativeFunction("gui_accept_drop", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            DragAcceptFiles(g_gui.handleMap[(int)vNum(args[0])], TRUE);
            return Value();
        }));

    // ============================================================================
    // NEW WIDGET CREATORS
    // ============================================================================
    interp.defineGlobal("gui_create_textarea", Value::makeNativeFunction("gui_create_textarea", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_groupbox", Value::makeNativeFunction("gui_create_groupbox", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("BUTTON", vStr(args[1]).c_str(), WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_separator", Value::makeNativeFunction("gui_create_separator", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_set_tooltip", Value::makeNativeFunction("gui_set_tooltip", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, NULL, g_gui.hInstance, NULL);
            TOOLINFO ti = {0};
            ti.cbSize = sizeof(TOOLINFO);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = hwnd;
            ti.uId = (UINT_PTR)hwnd;
            std::string text = vStr(args[1]);
            ti.lpszText = (LPSTR)text.c_str();
            SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti);
            return Value();
        }));

    // ============================================================================
    // MENU SYSTEM
    // ============================================================================
    interp.defineGlobal("gui_create_menubar", Value::makeNativeFunction("gui_create_menubar", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HMENU hMenu = CreateMenu();
            SetMenu(hwnd, hMenu);
            return Value((double)(intptr_t)hMenu);
        }));

    interp.defineGlobal("gui_add_menu", Value::makeNativeFunction("gui_add_menu", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HMENU hParent = (HMENU)(intptr_t)vNum(args[0]);
            HMENU hSub = CreatePopupMenu();
            AppendMenu(hParent, MF_POPUP | MF_STRING, (UINT_PTR)hSub, vStr(args[1]).c_str());
            return Value((double)(intptr_t)hSub);
        }));

    interp.defineGlobal("gui_add_menu_item", Value::makeNativeFunction("gui_add_menu_item", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HMENU hMenu = (HMENU)(intptr_t)vNum(args[0]);
            int id = g_gui.nextMenuId++;
            AppendMenu(hMenu, MF_STRING, id, vStr(args[1]).c_str());
            return Value((double)id);
        }));

    interp.defineGlobal("gui_add_menu_separator", Value::makeNativeFunction("gui_add_menu_separator", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HMENU hMenu = (HMENU)(intptr_t)vNum(args[0]);
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            return Value();
        }));

    interp.defineGlobal("gui_set_menu_callback", Value::makeNativeFunction("gui_set_menu_callback", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            g_gui.menuCallbacks[(int)vNum(args[0])] = args[1];
            return Value();
        }));

    interp.defineGlobal("gui_create_context_menu", Value::makeNativeFunction("gui_create_context_menu", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value((double)(intptr_t)CreatePopupMenu());
        }));

    interp.defineGlobal("gui_show_context_menu", Value::makeNativeFunction("gui_show_context_menu", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HMENU hMenu = (HMENU)(intptr_t)vNum(args[0]);
            POINT pt = { (LONG)vNum(args[1]), (LONG)vNum(args[2]) };
            ClientToScreen(g_gui.mainWindow, &pt);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_gui.mainWindow, NULL);
            return Value();
        }));

    // ============================================================================
    // BARS (Status & Toolbar)
    // ============================================================================
    interp.defineGlobal("gui_create_statusbar", Value::makeNativeFunction("gui_create_statusbar", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(STATUSCLASSNAME, "", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_set_status_text", Value::makeNativeFunction("gui_set_status_text", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            SendMessage(hwnd, SB_SETTEXT, 0, (LPARAM)vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_create_toolbar", Value::makeNativeFunction("gui_create_toolbar", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_NODIVIDER,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            SendMessage(hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_add_toolbar_button", Value::makeNativeFunction("gui_add_toolbar_button", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int id = g_gui.nextMenuId++;
            g_gui.menuCallbacks[id] = args[2];
            std::string text = vStr(args[1]);
            TBBUTTON tbb = {0};
            tbb.iBitmap = I_IMAGENONE;
            tbb.idCommand = id;
            tbb.fsState = TBSTATE_ENABLED;
            tbb.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
            tbb.iString = (INT_PTR)text.c_str();
            SendMessage(hwnd, TB_ADDBUTTONS, 1, (LPARAM)&tbb);
            SendMessage(hwnd, TB_AUTOSIZE, 0, 0);
            return Value();
        }));

    // ============================================================================
    // UTILS
    // ============================================================================
    interp.defineGlobal("gui_luminance", Value::makeNativeFunction("gui_luminance", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            double r = vNum(args[0]), g = vNum(args[1]), b = vNum(args[2]);
            return Value((0.299 * r + 0.587 * g + 0.114 * b) / 255.0);
        }));

    // ============================================================================
    // EXISTING BUILTINS (Re-adapted from previously viewed file)
    // ============================================================================
    
    interp.defineGlobal("gui_create_window", Value::makeNativeFunction("gui_create_window", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() != 3 && args.size() != 5) return Value();
            std::string title = vStr(args[0]);
            int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w, h;
            if (args.size() == 3) { w = (int)vNum(args[1]); h = (int)vNum(args[2]); } 
            else { x = (int)vNum(args[1]); y = (int)vNum(args[2]); w = (int)vNum(args[3]); h = (int)vNum(args[4]); }
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                x, y, w, h, NULL, NULL, g_gui.hInstance, NULL);
            if (!g_gui.mainWindow) g_gui.mainWindow = hwnd;
            if (g_gui.currentTheme == "dark") { BOOL useD = TRUE; DwmSetWindowAttribute(hwnd, 20, &useD, sizeof(useD)); }
            int cp = 2; DwmSetWindowAttribute(hwnd, 33, &cp, sizeof(cp));
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_button", Value::makeNativeFunction("gui_create_button", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("BUTTON", vStr(args[1]).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
            g_gui.callbacks[hwnd] = args[6]; // Legacy callback
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_label", Value::makeNativeFunction("gui_create_label", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("STATIC", vStr(args[1]).c_str(), WS_VISIBLE | WS_CHILD,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_input", Value::makeNativeFunction("gui_create_input", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_dropdown", Value::makeNativeFunction("gui_create_dropdown", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_panel", Value::makeNativeFunction("gui_create_panel", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_scroll_panel", Value::makeNativeFunction("gui_create_scroll_panel", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            HWND hwnd = CreateWindowEx(0, "EZWindowClass", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL | WS_HSCROLL,
                (int)vNum(args[1]), (int)vNum(args[2]), w, h, parent, NULL, g_gui.hInstance, NULL);
            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, 1000, (UINT)h, 0 };
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            si.nMax = 1000; si.nPage = (UINT)w;
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_set_scroll_range", Value::makeNativeFunction("gui_set_scroll_range", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT rect; GetClientRect(hwnd, &rect);
            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE, 0, (int)vNum(args[2]), (UINT)(rect.bottom - rect.top), 0 };
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            si.nMax = (int)vNum(args[1]); si.nPage = (UINT)(rect.right - rect.left);
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            return Value();
        }));

    // FIXED O(N^2) gui_clear_widgets
    interp.defineGlobal("gui_clear_widgets", Value::makeNativeFunction("gui_clear_widgets", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            std::vector<HWND> children;
            HWND child = GetWindow(parent, GW_CHILD);
            while (child) { children.push_back(child); child = GetWindow(child, GW_HWNDNEXT); }
            for (HWND h : children) DestroyWindow(h); // Handled efficiently in WM_DESTROY
            return Value();
        }));

    interp.defineGlobal("gui_create_tabs", Value::makeNativeFunction("gui_create_tabs", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            g_gui.callbacks[hwnd] = args[5];
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_add_tab", Value::makeNativeFunction("gui_add_tab", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            TCITEM tie = {0}; tie.mask = TCIF_TEXT;
            std::string text = vStr(args[1]); tie.pszText = (LPSTR)text.c_str();
            TabCtrl_InsertItem(hwnd, TabCtrl_GetItemCount(hwnd), &tie);
            return Value();
        }));

    interp.defineGlobal("gui_get_tab_selected", Value::makeNativeFunction("gui_get_tab_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value((double)TabCtrl_GetCurSel(g_gui.handleMap[(int)vNum(args[0])]));
        }));

    interp.defineGlobal("gui_show", Value::makeNativeFunction("gui_show", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            ShowWindow(hwnd, SW_SHOW); SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); UpdateWindow(hwnd);
            return Value();
        }));

    interp.defineGlobal("gui_hide", Value::makeNativeFunction("gui_hide", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ShowWindow(g_gui.handleMap[(int)vNum(args[0])], SW_HIDE); return Value(); }));

    interp.defineGlobal("gui_add_dropdown_item", Value::makeNativeFunction("gui_add_dropdown_item", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            SendMessage(g_gui.handleMap[(int)vNum(args[0])], CB_ADDSTRING, 0, (LPARAM)vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_get_dropdown_selected", Value::makeNativeFunction("gui_get_dropdown_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int idx = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) return Value("");
            int len = SendMessage(hwnd, CB_GETLBTEXTLEN, idx, 0);
            std::string text(len, '\0');
            SendMessage(hwnd, CB_GETLBTEXT, idx, (LPARAM)text.data());
            return Value(text);
        }));

    interp.defineGlobal("gui_set_color", Value::makeNativeFunction("gui_set_color", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            g_gui.styles[hwnd].textColor = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
            if (g_gui.styles[hwnd].bgBrush) DeleteObject(g_gui.styles[hwnd].bgBrush);
            g_gui.styles[hwnd].bgBrush = CreateSolidBrush(RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3])));
            char className[32]; GetClassName(hwnd, className, 32);
            if (std::string(className) == "Button") {
                LONG style = GetWindowLong(hwnd, GWL_STYLE);
                int btnType = style & 0xF;
                if (btnType == BS_PUSHBUTTON || btnType == BS_DEFPUSHBUTTON)
                    SetWindowLong(hwnd, GWL_STYLE, (style & ~0xF) | BS_OWNERDRAW);
            }
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
            return Value();
        }));

    interp.defineGlobal("gui_set_font", Value::makeNativeFunction("gui_set_font", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (g_gui.styles[hwnd].hFont) DeleteObject(g_gui.styles[hwnd].hFont);
            g_gui.styles[hwnd].hFont = CreateFont(-(int)vNum(args[2]), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, vStr(args[1]).c_str());
            SendMessage(hwnd, WM_SETFONT, (WPARAM)g_gui.styles[hwnd].hFont, TRUE);
            return Value();
        }));

    // FIXED dynamic length gui_get_value
    interp.defineGlobal("gui_get_value", Value::makeNativeFunction("gui_get_value", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int len = GetWindowTextLength(hwnd);
            if (len == 0) return Value("");
            std::string text(len + 1, '\0');
            GetWindowText(hwnd, text.data(), len + 1);
            text.resize(len);
            return Value(text);
        }));

    interp.defineGlobal("gui_set_value", Value::makeNativeFunction("gui_set_value", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            SetWindowText(g_gui.handleMap[(int)vNum(args[0])], vStr(args[1]).c_str());
            return Value();
        }));

    interp.defineGlobal("gui_alert", Value::makeNativeFunction("gui_alert", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            MessageBox(NULL, vStr(args[1]).c_str(), vStr(args[0]).c_str(), MB_OK | MB_ICONINFORMATION);
            return Value();
        }));

    interp.defineGlobal("gui_set_theme", Value::makeNativeFunction("gui_set_theme", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            g_gui.currentTheme = args[0].asString();
            for (auto const& [handle, hwnd] : g_gui.handleMap) {
                BOOL useDarkMode = (g_gui.currentTheme == "dark");
                DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
            }
            return Value();
        }));

    interp.defineGlobal("gui_set_callback", Value::makeNativeFunction("gui_set_callback", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            g_gui.callbacks[g_gui.handleMap[(int)vNum(args[0])]] = args[1];
            return Value();
        }));

    interp.defineGlobal("gui_create_checkbox", Value::makeNativeFunction("gui_create_checkbox", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow("BUTTON", vStr(args[1]).c_str(), WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_get_checked", Value::makeNativeFunction("gui_get_checked", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value((double)(SendMessage(g_gui.handleMap[(int)vNum(args[0])], BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0));
        }));

    interp.defineGlobal("gui_set_checked", Value::makeNativeFunction("gui_set_checked", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            SendMessage(g_gui.handleMap[(int)vNum(args[0])], BM_SETCHECK, (int)vNum(args[1]) ? BST_CHECKED : BST_UNCHECKED, 0);
            return Value();
        }));

    interp.defineGlobal("gui_create_radio", Value::makeNativeFunction("gui_create_radio", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            DWORD style = WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON;
            if ((int)vNum(args[6])) style |= WS_GROUP;
            HWND hwnd = CreateWindow("BUTTON", vStr(args[1]).c_str(), style,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_create_slider", Value::makeNativeFunction("gui_create_slider", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int minVal = (int)vNum(args[5]), maxVal = (int)vNum(args[6]);
            HWND hwnd = CreateWindow(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL); // w, h are args 4,5 not 3,4... wait: 0:win, 1:x, 2:y, 3:w, 4:h, 5:min, 6:max.
            // Correction: args[1]=x, args[2]=y, args[3]=w, args[4]=h.
            DestroyWindow(hwnd); // Fix arg mapping
            hwnd = CreateWindow(TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            SendMessage(hwnd, TBM_SETRANGE, TRUE, MAKELPARAM(minVal, maxVal));
            SendMessage(hwnd, TBM_SETPOS, TRUE, minVal);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_get_slider", Value::makeNativeFunction("gui_get_slider", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { return Value((double)SendMessage(g_gui.handleMap[(int)vNum(args[0])], TBM_GETPOS, 0, 0)); }));
    interp.defineGlobal("gui_set_slider", Value::makeNativeFunction("gui_set_slider", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { SendMessage(g_gui.handleMap[(int)vNum(args[0])], TBM_SETPOS, TRUE, (int)vNum(args[1])); return Value(); }));

    interp.defineGlobal("gui_create_progress", Value::makeNativeFunction("gui_create_progress", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            SendMessage(hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_set_progress", Value::makeNativeFunction("gui_set_progress", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { SendMessage(g_gui.handleMap[(int)vNum(args[0])], PBM_SETPOS, (int)vNum(args[1]), 0); return Value(); }));
    interp.defineGlobal("gui_set_progress_range", Value::makeNativeFunction("gui_set_progress_range", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { SendMessage(g_gui.handleMap[(int)vNum(args[0])], PBM_SETRANGE, 0, MAKELPARAM((int)vNum(args[1]), (int)vNum(args[2]))); return Value(); }));

    interp.defineGlobal("gui_create_listview", Value::makeNativeFunction("gui_create_listview", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_listview_add_column", Value::makeNativeFunction("gui_listview_add_column", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int colCount = Header_GetItemCount(ListView_GetHeader(hwnd));
            LVCOLUMN col = {0}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; col.cx = (int)vNum(args[2]);
            col.pszText = (LPSTR)text.c_str(); col.iSubItem = colCount;
            ListView_InsertColumn(hwnd, colCount, &col); return Value();
        }));

    interp.defineGlobal("gui_listview_add_row", Value::makeNativeFunction("gui_listview_add_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (!args[1].isArray()) return Value();
            const auto& items = args[1].asArray();
            int rowIdx = ListView_GetItemCount(hwnd);
            LVITEM lvi = {0}; lvi.mask = LVIF_TEXT; lvi.iItem = rowIdx; lvi.iSubItem = 0;
            std::string firstText = items.empty() ? "" : items[0].toString(); lvi.pszText = (LPSTR)firstText.c_str();
            ListView_InsertItem(hwnd, &lvi);
            for (size_t i = 1; i < items.size(); i++) {
                std::string cellText = items[i].toString();
                ListView_SetItemText(hwnd, rowIdx, (int)i, (LPSTR)cellText.c_str());
            }
            return Value((double)rowIdx);
        }));

    interp.defineGlobal("gui_listview_clear", Value::makeNativeFunction("gui_listview_clear", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ListView_DeleteAllItems(g_gui.handleMap[(int)vNum(args[0])]); return Value(); }));
    interp.defineGlobal("gui_listview_get_selected", Value::makeNativeFunction("gui_listview_get_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { return Value((double)ListView_GetNextItem(g_gui.handleMap[(int)vNum(args[0])], -1, LVNI_SELECTED)); }));
    
    interp.defineGlobal("gui_listview_get_row", Value::makeNativeFunction("gui_listview_get_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; int row = (int)vNum(args[1]);
            int colCount = Header_GetItemCount(ListView_GetHeader(hwnd));
            std::vector<Value> cells;
            for (int i = 0; i < colCount; i++) {
                int len = 512; std::string text(len, '\0');
                ListView_GetItemText(hwnd, row, i, text.data(), len);
                cells.push_back(Value(std::string(text.c_str())));
            }
            return Value::makeArray(cells);
        }));

    interp.defineGlobal("gui_listview_remove_row", Value::makeNativeFunction("gui_listview_remove_row", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { ListView_DeleteItem(g_gui.handleMap[(int)vNum(args[0])], (int)vNum(args[1])); return Value(); }));

    // num
    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            try { return Value(std::stod(args[0].asString())); } catch (...) { return Value(0.0); }
        }));

    interp.defineGlobal("gui_set_timer", Value::makeNativeFunction("gui_set_timer", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            UINT_PTR timerId = g_gui.nextTimerId++;
            g_gui.timerCallbacks[timerId] = args[2];
            g_gui.hwndTimers[hwnd].push_back(timerId); // Track it!
            SetTimer(hwnd, timerId, (int)vNum(args[1]), NULL);
            return Value((double)timerId);
        }));

    interp.defineGlobal("gui_kill_timer", Value::makeNativeFunction("gui_kill_timer", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            UINT_PTR timerId = (UINT_PTR)vNum(args[1]);
            KillTimer(hwnd, timerId);
            g_gui.timerCallbacks.erase(timerId);
            auto& timers = g_gui.hwndTimers[hwnd];
            timers.erase(std::remove(timers.begin(), timers.end(), timerId), timers.end());
            return Value();
        }));

    interp.defineGlobal("gui_dialog_confirm", Value::makeNativeFunction("gui_dialog_confirm", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value(MessageBox(g_gui.mainWindow, vStr(args[1]).c_str(), vStr(args[0]).c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES);
        }));
    interp.defineGlobal("gui_dialog_input", Value::makeNativeFunction("gui_dialog_input", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            MessageBox(g_gui.mainWindow, vStr(args[1]).c_str(), vStr(args[0]).c_str(), MB_OK | MB_ICONINFORMATION); return Value();
        }));

    interp.defineGlobal("gui_dialog_open_file", Value::makeNativeFunction("gui_dialog_open_file", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]), rawFilter = vStr(args[1]);
            std::string filter = "Files\0" + rawFilter + "\0All Files\0*.*\0";
            std::vector<char> filterBuf(filter.begin(), filter.end()); filterBuf.push_back('\0');
            char filename[MAX_PATH] = {0}; OPENFILENAME ofn = {0};
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_gui.mainWindow;
            ofn.lpstrFilter = filterBuf.data(); ofn.lpstrFile = filename; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title.c_str(); ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileName(&ofn)) return Value(std::string(filename)); return Value("");
        }));

    interp.defineGlobal("gui_dialog_save_file", Value::makeNativeFunction("gui_dialog_save_file", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string title = vStr(args[0]), rawFilter = vStr(args[1]);
            std::string filter = "Files\0" + rawFilter + "\0All Files\0*.*\0";
            std::vector<char> filterBuf(filter.begin(), filter.end()); filterBuf.push_back('\0');
            char filename[MAX_PATH] = {0}; OPENFILENAME ofn = {0};
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_gui.mainWindow;
            ofn.lpstrFilter = filterBuf.data(); ofn.lpstrFile = filename; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title.c_str(); ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileName(&ofn)) return Value(std::string(filename)); return Value("");
        }));

    interp.defineGlobal("gui_dialog_open_folder", Value::makeNativeFunction("gui_dialog_open_folder", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            BROWSEINFO bi = {0}; bi.hwndOwner = g_gui.mainWindow; bi.lpszTitle = "Select Folder"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl) { char path[MAX_PATH]; SHGetPathFromIDList(pidl, path); CoTaskMemFree(pidl); return Value(std::string(path)); }
            return Value("");
        }));

    interp.defineGlobal("gui_dialog_color_picker", Value::makeNativeFunction("gui_dialog_color_picker", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            static COLORREF customColors[16] = {0}; CHOOSECOLOR cc = {0};
            cc.lStructSize = sizeof(cc); cc.hwndOwner = g_gui.mainWindow; cc.lpCustColors = customColors; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) return Value::makeArray({ Value((double)GetRValue(cc.rgbResult)), Value((double)GetGValue(cc.rgbResult)), Value((double)GetBValue(cc.rgbResult)) });
            return Value();
        }));

    interp.defineGlobal("gui_create_treeview", Value::makeNativeFunction("gui_create_treeview", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(WC_TREEVIEW, "", WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_treeview_add", Value::makeNativeFunction("gui_treeview_add", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            HTREEITEM parentItem = (HTREEITEM)(intptr_t)(int64_t)vNum(args[1]); if (parentItem == 0) parentItem = TVI_ROOT;
            std::string text = vStr(args[2]); TVINSERTSTRUCT tvins = {0};
            tvins.hParent = parentItem; tvins.hInsertAfter = TVI_LAST; tvins.item.mask = TVIF_TEXT; tvins.item.pszText = (LPSTR)text.c_str();
            return Value((double)(intptr_t)TreeView_InsertItem(hwnd, &tvins));
        }));

    interp.defineGlobal("gui_treeview_get_selected", Value::makeNativeFunction("gui_treeview_get_selected", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; HTREEITEM sel = TreeView_GetSelection(hwnd); if (!sel) return Value("");
            char buf[256] = {0}; TVITEM tvi = {0}; tvi.mask = TVIF_TEXT; tvi.hItem = sel; tvi.pszText = buf; tvi.cchTextMax = 256;
            TreeView_GetItem(hwnd, &tvi); return Value(std::string(buf));
        }));

    interp.defineGlobal("gui_treeview_clear", Value::makeNativeFunction("gui_treeview_clear", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { TreeView_DeleteAllItems(g_gui.handleMap[(int)vNum(args[0])]); return Value(); }));

    interp.defineGlobal("gui_create_datepicker", Value::makeNativeFunction("gui_create_datepicker", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            HWND hwnd = CreateWindow(DATETIMEPICK_CLASS, "", WS_CHILD | WS_VISIBLE | DTS_SHORTDATECENTURYFORMAT,
                (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_datepicker_get", Value::makeNativeFunction("gui_datepicker_get", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; SYSTEMTIME st; DateTime_GetSystemtime(hwnd, &st);
            char buf[32]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); return Value(std::string(buf));
        }));
    interp.defineGlobal("gui_datepicker_set", Value::makeNativeFunction("gui_datepicker_set", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; SYSTEMTIME st = {0};
            st.wYear = (WORD)vNum(args[1]); st.wMonth = (WORD)vNum(args[2]); st.wDay = (WORD)vNum(args[3]);
            DateTime_SetSystemtime(hwnd, GDT_VALID, &st); return Value();
        }));

    interp.defineGlobal("gui_create_spinner", Value::makeNativeFunction("gui_create_spinner", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]), minVal = (int)vNum(args[5]), maxVal = (int)vNum(args[6]);
            HWND editHwnd = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_RIGHT, x, y, w - 20, h, parent, NULL, g_gui.hInstance, NULL);
            HWND udHwnd = CreateWindow(UPDOWN_CLASS, "", WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0, parent, NULL, g_gui.hInstance, NULL);
            SendMessage(udHwnd, UDM_SETBUDDY, (WPARAM)editHwnd, 0); SendMessage(udHwnd, UDM_SETRANGE32, minVal, maxVal); SendMessage(udHwnd, UDM_SETPOS32, 0, minVal);
            int udHandle = g_gui.nextHandle++; g_gui.handleMap[udHandle] = udHwnd; g_gui.reverseHandleMap[udHwnd] = udHandle;
            return Value(registerWidgetHandle(editHwnd));
        }));

    interp.defineGlobal("gui_spinner_get", Value::makeNativeFunction("gui_spinner_get", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; char buf[64]; GetWindowText(hwnd, buf, 64);
            try { return Value(std::stod(buf)); } catch (...) { return Value(0.0); }
        }));
    interp.defineGlobal("gui_spinner_set", Value::makeNativeFunction("gui_spinner_set", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { SetWindowText(g_gui.handleMap[(int)vNum(args[0])], std::to_string((int)vNum(args[1])).c_str()); return Value(); }));

    interp.defineGlobal("gui_create_image", Value::makeNativeFunction("gui_create_image", 6,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND parent = g_gui.handleMap[(int)vNum(args[0])]; std::string path = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), w = (int)vNum(args[4]), h = (int)vNum(args[5]);
            HWND hwnd = CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, x, y, w, h, parent, NULL, g_gui.hInstance, NULL);
            HBITMAP hbmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, w, h, LR_LOADFROMFILE);
            if (hbmp) SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
            return Value(registerWidgetHandle(hwnd));
        }));

    interp.defineGlobal("gui_set_image", Value::makeNativeFunction("gui_set_image", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; std::string path = vStr(args[1]);
            RECT rc; GetClientRect(hwnd, &rc);
            HBITMAP oldBmp = (HBITMAP)SendMessage(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
            HBITMAP hbmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, rc.right, rc.bottom, LR_LOADFROMFILE);
            if (hbmp) { SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp); if (oldBmp) DeleteObject(oldBmp); }
            return Value();
        }));

    interp.defineGlobal("gui_set_pos", Value::makeNativeFunction("gui_set_pos", 5,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            SetWindowPos(g_gui.handleMap[(int)vNum(args[0])], NULL, (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), SWP_NOZORDER); return Value();
        }));
    interp.defineGlobal("gui_set_enabled", Value::makeNativeFunction("gui_set_enabled", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { EnableWindow(g_gui.handleMap[(int)vNum(args[0])], (int)vNum(args[1]) != 0); return Value(); }));
    interp.defineGlobal("gui_run", Value::makeNativeFunction("gui_run", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            g_gui.currentInterpreter = &interp; MSG msg;
            while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
            return Value();
        }));

    interp.defineGlobal("gui_is_key_down", Value::makeNativeFunction("gui_is_key_down", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value { return Value((double)((GetAsyncKeyState((int)vNum(args[0])) & 0x8000) ? 1 : 0)); }));
    auto getDCForDraw = [](HWND hwnd, bool& isMem) -> HDC {
        if (g_gui.memDCs.count(hwnd)) { isMem = true; return g_gui.memDCs[hwnd]; }
        isMem = false; return GetDC(hwnd);
    };
    auto releaseDCForDraw = [](HWND hwnd, HDC hdc, bool isMem) {
        if (!isMem) ReleaseDC(hwnd, hdc);
    };

    interp.defineGlobal("gui_begin_draw", Value::makeNativeFunction("gui_begin_draw", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (!g_gui.memDCs.count(hwnd)) {
                HDC hdcWin = GetDC(hwnd);
                HDC hdcMem = CreateCompatibleDC(hdcWin);
                RECT rc; GetClientRect(hwnd, &rc);
                HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, rc.right, rc.bottom);
                SelectObject(hdcMem, hBmp);
                g_gui.memDCs[hwnd] = hdcMem;
                g_gui.memBitmaps[hwnd] = hBmp;
                ReleaseDC(hwnd, hdcWin);
            }
            return Value();
        }));

    interp.defineGlobal("gui_end_draw", Value::makeNativeFunction("gui_end_draw", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (g_gui.memDCs.count(hwnd)) {
                HDC hdcWin = GetDC(hwnd);
                HDC hdcMem = g_gui.memDCs[hwnd];
                RECT rc; GetClientRect(hwnd, &rc);
                BitBlt(hdcWin, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
                DeleteDC(hdcMem);
                DeleteObject(g_gui.memBitmaps[hwnd]);
                g_gui.memDCs.erase(hwnd);
                g_gui.memBitmaps.erase(hwnd);
                ReleaseDC(hwnd, hdcWin);
            }
            return Value();
        }));

    interp.defineGlobal("gui_draw_rect", Value::makeNativeFunction("gui_draw_rect", 8,
        [getDCForDraw, releaseDCForDraw](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
            COLORREF color = RGB((int)vNum(args[5]), (int)vNum(args[6]), (int)vNum(args[7]));
            bool isMem = false; HDC hdc = getDCForDraw(hwnd, isMem);
            HBRUSH brush = CreateSolidBrush(color); RECT rect = { x, y, x + w, y + h }; FillRect(hdc, &rect, brush);
            DeleteObject(brush); releaseDCForDraw(hwnd, hdc, isMem); return Value();
        }));
    interp.defineGlobal("gui_draw_circle", Value::makeNativeFunction("gui_draw_circle", 7,
        [getDCForDraw, releaseDCForDraw](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; int x = (int)vNum(args[1]), y = (int)vNum(args[2]), radius = (int)vNum(args[3]);
            COLORREF color = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
            bool isMem = false; HDC hdc = getDCForDraw(hwnd, isMem);
            HBRUSH brush = CreateSolidBrush(color); HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN pen = CreatePen(PS_SOLID, 1, color); HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
            SelectObject(hdc, oldBrush); DeleteObject(brush); SelectObject(hdc, oldPen); DeleteObject(pen); releaseDCForDraw(hwnd, hdc, isMem); return Value();
        }));
    interp.defineGlobal("gui_clear", Value::makeNativeFunction("gui_clear", 4,
        [getDCForDraw, releaseDCForDraw](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; COLORREF color = RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]));
            bool isMem = false; HDC hdc = getDCForDraw(hwnd, isMem);
            RECT rect; GetClientRect(hwnd, &rect); HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &rect, brush); DeleteObject(brush); releaseDCForDraw(hwnd, hdc, isMem); return Value();
        }));
    interp.defineGlobal("gui_get_client_size", Value::makeNativeFunction("gui_get_client_size", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; RECT rect; GetClientRect(hwnd, &rect);
            return Value::makeArray({ Value((double)rect.right), Value((double)rect.bottom) });
        }));
    interp.defineGlobal("gui_draw_text", Value::makeNativeFunction("gui_draw_text", 8,
        [getDCForDraw, releaseDCForDraw](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])]; std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]), y = (int)vNum(args[3]), size = (int)vNum(args[4]);
            COLORREF color = RGB((int)vNum(args[5]), (int)vNum(args[6]), (int)vNum(args[7]));
            bool isMem = false; HDC hdc = getDCForDraw(hwnd, isMem);
            HFONT hFont = CreateFont(-(int)size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont); SetTextColor(hdc, color); SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, x, y, text.c_str(), (int)text.length());
            SelectObject(hdc, oldFont); DeleteObject(hFont); releaseDCForDraw(hwnd, hdc, isMem); return Value();
        }));
}
