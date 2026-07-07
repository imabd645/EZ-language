#include "GUI_Internal.h"

GUIState g_gui;

Value fireEventCallback(HWND hwnd, const std::string& eventName, const std::vector<Value>& args) {
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

bool hasEvent(HWND hwnd, const std::string& eventName) {
    return g_gui.eventCallbacks.count(hwnd) && g_gui.eventCallbacks[hwnd].count(eventName);
}

double registerWidgetHandle(HWND hwnd) {
    int handle = g_gui.nextHandle++;
    g_gui.handleMap[handle] = hwnd;
    g_gui.reverseHandleMap[hwnd] = handle;
    return (double)handle;
}

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

void registerGUICoreBuiltins(RuntimeContext& interp) {
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
    g_gui.currentInterpreter = &interp;


    // UNIVERSAL EVENT SYSTEM
    interp.defineGlobal("gui_on", Value::makeNativeFunction("gui_on", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string eventName = vStr(args[1]);
            g_gui.eventCallbacks[hwnd][eventName] = args[2];
            return Value();
        }));

    // WINDOW MANAGEMENT
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

    interp.defineGlobal("gui_luminance", Value::makeNativeFunction("gui_luminance", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            double r = vNum(args[0]), g = vNum(args[1]), b = vNum(args[2]);
            return Value((0.299 * r + 0.587 * g + 0.114 * b) / 255.0);
        }));

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

}

void registerGUIBuiltins(RuntimeContext& interp) {
    registerGUICoreBuiltins(interp);
    registerGUIWidgetsBuiltins(interp);
    registerGUIMenuBuiltins(interp);
    registerGUIDialogsBuiltins(interp);
    registerGUIDrawingBuiltins(interp);
}
