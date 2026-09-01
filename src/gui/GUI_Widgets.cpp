#include "GUI_Internal.h"
#include <cmath>

struct SpinnerData {
    int step = 0;
    bool active = false;
    COLORREF color = RGB(0, 120, 240);  // vibrant blue default
};

static std::map<HWND, SpinnerData> g_spinners;

static LRESULT CALLBACK EZSpinnerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_spinners[hwnd] = SpinnerData();
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            g_spinners.erase(hwnd);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1 && g_spinners.count(hwnd)) {
                g_spinners[hwnd].step = (g_spinners[hwnd].step + 1) % 12;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            HWND parent = GetParent(hwnd);
            HBRUSH bgBrush = NULL;
            if (g_gui.styles.count(parent) && g_gui.styles[parent].bgBrush) {
                bgBrush = g_gui.styles[parent].bgBrush;
            } else if (g_gui.styles.count(hwnd) && g_gui.styles[hwnd].bgBrush) {
                bgBrush = g_gui.styles[hwnd].bgBrush;
            } else if (g_gui.currentTheme == "dark") {
                if (!g_gui.darkBrush) g_gui.darkBrush = CreateSolidBrush(RGB(30, 30, 30));
                bgBrush = g_gui.darkBrush;
            } else {
                bgBrush = (HBRUSH)(COLOR_BTNFACE + 1);
            }
            FillRect(memDC, &rc, bgBrush);

            SpinnerData& sp = g_spinners[hwnd];
            int currentStep = sp.step;
            COLORREF baseColor = sp.color;
            int baseR = GetRValue(baseColor);
            int baseG = GetGValue(baseColor);
            int baseB = GetBValue(baseColor);

            double cx = width / 2.0;
            double cy = height / 2.0;
            double radius = ((std::min)(width, height) / 2.0) - 3.0;
            if (radius < 4.0) radius = 4.0;
            double dotRadius = radius * 0.22;
            if (dotRadius < 2.0) dotRadius = 2.0;

            const double PI = 3.14159265358979323846;

            for (int i = 0; i < 12; ++i) {
                double angle = i * (2.0 * PI / 12.0) - (PI / 2.0);
                double dist = radius * 0.72;
                double px = cx + dist * cos(angle);
                double py = cy + dist * sin(angle);

                int delta = (i - currentStep + 12) % 12;
                double factor = (12.0 - delta) / 12.0;
                if (factor < 0.15) factor = 0.15;

                int r = (int)(baseR * factor);
                int g = (int)(baseG * factor);
                int b = (int)(baseB * factor);

                HBRUSH dotBrush = CreateSolidBrush(RGB(r, g, b));
                HPEN dotPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
                HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, dotBrush);
                HPEN oldPen = (HPEN)SelectObject(memDC, dotPen);

                double dr = dotRadius * (0.6 + 0.4 * factor);
                Ellipse(memDC, (int)(px - dr), (int)(py - dr), (int)(px + dr + 0.5), (int)(py + dr + 0.5));

                SelectObject(memDC, oldBrush);
                SelectObject(memDC, oldPen);
                DeleteObject(dotBrush);
                DeleteObject(dotPen);
            }

            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static Value builtin_gui_set_color(RuntimeContext& interp, const std::vector<Value>& args) {
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    g_gui.styles[hwnd].textColor = RGB((int)vNum(args[4]), (int)vNum(args[5]), (int)vNum(args[6]));
    if (g_gui.styles[hwnd].bgBrush) DeleteObject(g_gui.styles[hwnd].bgBrush);
    g_gui.styles[hwnd].bgBrush = CreateSolidBrush(RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3])));
    char className[32];
    GetClassName(hwnd, className, 32);
    if (std::string(className) == "Button") {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        int btnType = style & 0xF;
        if (btnType == BS_PUSHBUTTON || btnType == BS_DEFPUSHBUTTON) {
            SetWindowLong(hwnd, GWL_STYLE, (style & ~0xF) | BS_OWNERDRAW);
        }
    }
    return Value();
}

static Value builtin_gui_set_font(RuntimeContext& interp, const std::vector<Value>& args) {
    if (args.size() < 3) return Value();
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    std::wstring fontName = utf8ToWide(vStr(args[1]));
    int fontSize = (int)vNum(args[2]);

    HFONT hFont = CreateFontW(
        fontSize, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName.c_str());

    if (g_gui.styles[hwnd].hFont) {
        DeleteObject(g_gui.styles[hwnd].hFont);
    }
    g_gui.styles[hwnd].hFont = hFont;
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, true);
    return Value();
}

static Value builtin_gui_set_value(RuntimeContext& interp, const std::vector<Value>& args) {
    if (args.size() < 2) return Value();
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    std::wstring text = utf8ToWide(vStr(args[1]));
    SetWindowTextW(hwnd, text.c_str());
    return Value();
}

static Value builtin_gui_get_value(RuntimeContext& interp, const std::vector<Value>& args) {
    if (args.size() < 1) return Value();
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    int len = GetWindowTextLengthW(hwnd);
    std::wstring text(len + 1, 0);
    GetWindowTextW(hwnd, &text[0], len + 1);
    text.resize(len);
    return Value(wideToUtf8(text));
}

static Value builtin_gui_set_callback(RuntimeContext& interp, const std::vector<Value>& args) {
    if (args.size() < 2) return Value();
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    g_gui.callbacks[hwnd] = args[1];
    return Value();
}

static Value builtin_gui_set_pos(RuntimeContext& interp, const std::vector<Value>& args) {
    if (args.size() < 5) return Value();
    HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
    SetWindowPos(hwnd, NULL, (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), SWP_NOZORDER);
    return Value();
}

void registerGUIWidgetsBuiltins(RuntimeContext& interp) {
    // NEW WIDGET CREATORS
    interp.defineGlobal("gui_create_textarea", Value::makeNativeFunction("gui_create_textarea", 5,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             HWND hwnd = CreateWindowExW(0, L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
                                                                                                         (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_create_groupbox", Value::makeNativeFunction("gui_create_groupbox", 6,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             std::wstring text = utf8ToWide(vStr(args[1]));
                                                                             HWND hwnd = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                                                                                                         (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_create_separator", Value::makeNativeFunction("gui_create_separator", 5,
                                                                          [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                              HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                              HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
                                                                                                          (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                              return Value(registerWidgetHandle(hwnd));
                                                                          }));

    interp.defineGlobal("gui_set_tooltip", Value::makeNativeFunction("gui_set_tooltip", 2,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         HWND hwndTT = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                                                                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, NULL, g_gui.hInstance, NULL);
                                                                         TOOLINFOW ti = {0};
                                                                         ti.cbSize = sizeof(TOOLINFOW);
                                                                         ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                                                                         ti.hwnd = hwnd;
                                                                         ti.uId = (UINT_PTR)hwnd;
                                                                         std::wstring text = utf8ToWide(vStr(args[1]));
                                                                         ti.lpszText = (LPWSTR)text.c_str();
                                                                         SendMessageW(hwndTT, TTM_ADDTOOLW, 0, (LPARAM)&ti);
                                                                         return Value();
                                                                     }));

    // EXISTING BUILTINS (Re-adapted from previously viewed file)
    interp.defineGlobal("gui_set_color", Value::makeNativeFunction("gui_set_color", 7, builtin_gui_set_color));
    interp.defineGlobal("gui_set_font", Value::makeNativeFunction("gui_set_font", 3, builtin_gui_set_font));
    interp.defineGlobal("gui_set_value", Value::makeNativeFunction("gui_set_value", 2, builtin_gui_set_value));
    interp.defineGlobal("gui_get_value", Value::makeNativeFunction("gui_get_value", 1, builtin_gui_get_value));
    interp.defineGlobal("gui_set_callback", Value::makeNativeFunction("gui_set_callback", 2, builtin_gui_set_callback));
    interp.defineGlobal("gui_set_pos", Value::makeNativeFunction("gui_set_pos", 5, builtin_gui_set_pos));
    interp.defineGlobal("gui_set_enabled", Value::makeNativeFunction("gui_set_enabled", 2,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         EnableWindow(hwnd, (int)vNum(args[1]));
                                                                         return Value();
                                                                     }));

    interp.defineGlobal("gui_create_checkbox", Value::makeNativeFunction("gui_create_checkbox", 6,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             std::wstring text = utf8ToWide(vStr(args[1]));
                                                                             HWND hwnd = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                                                                                         (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_create_radio", Value::makeNativeFunction("gui_create_radio", 7,
                                                                      [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                          HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                          int groupStart = (int)vNum(args[6]);
                                                                          DWORD style = WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON;
                                                                          if (groupStart) style |= WS_GROUP;
                                                                          std::wstring text = utf8ToWide(vStr(args[1]));
                                                                          HWND hwnd = CreateWindowExW(0, L"BUTTON", text.c_str(), style,
                                                                                                      (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
                                                                          return Value(registerWidgetHandle(hwnd));
                                                                      }));

    interp.defineGlobal("gui_get_checked", Value::makeNativeFunction("gui_get_checked", 1,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         return Value((double)SendMessage(hwnd, BM_GETCHECK, 0, 0));
                                                                     }));

    interp.defineGlobal("gui_set_checked", Value::makeNativeFunction("gui_set_checked", 2,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         SendMessage(hwnd, BM_SETCHECK, (int)vNum(args[1]), 0);
                                                                         return Value();
                                                                     }));

    interp.defineGlobal("gui_create_slider", Value::makeNativeFunction("gui_create_slider", 7,
                                                                       [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                           HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                           HWND hwnd = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_TOOLTIPS,
                                                                                                       (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                           SendMessage(hwnd, TBM_SETRANGE, true, MAKELPARAM((int)vNum(args[5]), (int)vNum(args[6])));
                                                                           return Value(registerWidgetHandle(hwnd));
                                                                       }));

    interp.defineGlobal("gui_get_slider", Value::makeNativeFunction("gui_get_slider", 1,
                                                                    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                        HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                        return Value((double)SendMessage(hwnd, TBM_GETPOS, 0, 0));
                                                                    }));

    interp.defineGlobal("gui_set_slider", Value::makeNativeFunction("gui_set_slider", 2,
                                                                    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                        HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                        SendMessage(hwnd, TBM_SETPOS, true, (int)vNum(args[1]));
                                                                        return Value();
                                                                    }));

    interp.defineGlobal("gui_create_progress", Value::makeNativeFunction("gui_create_progress", 5,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             HWND hwnd = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                                                                                                         (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_set_progress", Value::makeNativeFunction("gui_set_progress", 2,
                                                                      [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                          HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                          SendMessage(hwnd, PBM_SETPOS, (int)vNum(args[1]), 0);
                                                                          return Value();
                                                                      }));

    interp.defineGlobal("gui_set_progress_range", Value::makeNativeFunction("gui_set_progress_range", 3,
                                                                            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                SendMessage(hwnd, PBM_SETRANGE32, (int)vNum(args[1]), (int)vNum(args[2]));
                                                                                return Value();
                                                                            }));

    interp.defineGlobal("gui_create_listview", Value::makeNativeFunction("gui_create_listview", 5,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             HWND hwnd = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                                                                                         (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                             SendMessage(hwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_listview_add_column", Value::makeNativeFunction("gui_listview_add_column", 3,
                                                                             [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                 HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                 LVCOLUMNW lvc = {0};
                                                                                 lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
                                                                                 std::wstring text = utf8ToWide(vStr(args[1]));
                                                                                 lvc.pszText = (LPWSTR)text.c_str();
                                                                                 lvc.cx = (int)vNum(args[2]);
                                                                                 lvc.fmt = LVCFMT_LEFT;
                                                                                 HWND hHeader = (HWND)SendMessage(hwnd, LVM_GETHEADER, 0, 0);
                                                                                 int count = (int)SendMessage(hHeader, HDM_GETITEMCOUNT, 0, 0);
                                                                                 lvc.iSubItem = count;
                                                                                 SendMessageW(hwnd, LVM_INSERTCOLUMNW, count, (LPARAM)&lvc);
                                                                                 return Value();
                                                                             }));

    interp.defineGlobal("gui_listview_add_row", Value::makeNativeFunction("gui_listview_add_row", 2,
                                                                          [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                              HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                              if (!args[1].isArray()) return Value();
                                                                              auto items = args[1].asArray().getElementsCopy();
                                                                              if (items.empty()) return Value();

                                                                              int count = SendMessage(hwnd, LVM_GETITEMCOUNT, 0, 0);
                                                                              LVITEMW lvi = {0};
                                                                              lvi.mask = LVIF_TEXT;
                                                                              lvi.iItem = count;
                                                                              lvi.iSubItem = 0;
                                                                              std::wstring first = utf8ToWide(vStr(items[0]));
                                                                              lvi.pszText = (LPWSTR)first.c_str();
                                                                              SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

                                                                              for (size_t i = 1; i < items.size(); i++) {
                                                                                  std::wstring sub = utf8ToWide(vStr(items[i]));
                                                                                  LVITEMW lviSub = {0};
                                                                                  lviSub.iSubItem = i;
                                                                                  lviSub.pszText = (LPWSTR)sub.c_str();
                                                                                  SendMessageW(hwnd, LVM_SETITEMTEXTW, count, (LPARAM)&lviSub);
                                                                              }
                                                                              return Value((double)count);
                                                                          }));

    interp.defineGlobal("gui_listview_clear", Value::makeNativeFunction("gui_listview_clear", 1,
                                                                        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                            SendMessage(hwnd, LVM_DELETEALLITEMS, 0, 0);
                                                                            return Value();
                                                                        }));

    interp.defineGlobal("gui_listview_get_selected", Value::makeNativeFunction("gui_listview_get_selected", 1,
                                                                               [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                   HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                   int sel = SendMessage(hwnd, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
                                                                                   return Value((double)sel);
                                                                               }));

    interp.defineGlobal("gui_listview_get_row", Value::makeNativeFunction("gui_listview_get_row", 2,
                                                                          [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                              HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                              int row = (int)vNum(args[1]);
                                                                              HWND hHeader = (HWND)SendMessage(hwnd, LVM_GETHEADER, 0, 0);
                                                                              int cols = (int)SendMessage(hHeader, HDM_GETITEMCOUNT, 0, 0);
                                                                              std::vector<Value> rowData;
                                                                              for (int i = 0; i < cols; i++) {
                                                                                  wchar_t buf[2048] = {0};
                                                                                  LVITEMW lvi = {0};
                                                                                  lvi.iSubItem = i;
                                                                                  lvi.cchTextMax = 2048;
                                                                                  lvi.pszText = buf;
                                                                                  SendMessageW(hwnd, LVM_GETITEMTEXTW, row, (LPARAM)&lvi);
                                                                                  rowData.push_back(Value(wideToUtf8(buf)));
                                                                              }
                                                                              return Value::makeArray(rowData);
                                                                          }));

    interp.defineGlobal("gui_listview_remove_row", Value::makeNativeFunction("gui_listview_remove_row", 2,
                                                                             [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                 HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                 SendMessage(hwnd, LVM_DELETEITEM, (int)vNum(args[1]), 0);
                                                                                 return Value();
                                                                             }));

    interp.defineGlobal("gui_create_treeview", Value::makeNativeFunction("gui_create_treeview", 5,
                                                                         [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                             HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                             HWND hwnd = CreateWindow(WC_TREEVIEW, "", WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
                                                                                                      (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                             return Value(registerWidgetHandle(hwnd));
                                                                         }));

    interp.defineGlobal("gui_treeview_add", Value::makeNativeFunction("gui_treeview_add", 3,
                                                                      [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                          HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                          HTREEITEM parentItem = args[1].isNumber() && vNum(args[1]) != 0 ? (HTREEITEM)(intptr_t)vNum(args[1]) : TVI_ROOT;
                                                                          TVINSERTSTRUCT tvis = {0};
                                                                          tvis.hParent = parentItem;
                                                                          tvis.hInsertAfter = TVI_LAST;
                                                                          tvis.item.mask = TVIF_TEXT;
                                                                          std::string text = vStr(args[2]);
                                                                          tvis.item.pszText = (LPSTR)text.c_str();
                                                                          HTREEITEM hItem = (HTREEITEM)SendMessage(hwnd, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                                                                          return Value((double)(intptr_t)hItem);
                                                                      }));

    interp.defineGlobal("gui_treeview_get_selected", Value::makeNativeFunction("gui_treeview_get_selected", 1,
                                                                               [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                   HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                   HTREEITEM hItem = (HTREEITEM)SendMessage(hwnd, TVM_GETNEXTITEM, TVGN_CARET, 0);
                                                                                   if (!hItem) return Value("");
                                                                                   char buf[1024] = {0};
                                                                                   TVITEM tvi = {0};
                                                                                   tvi.mask = TVIF_TEXT;
                                                                                   tvi.hItem = hItem;
                                                                                   tvi.pszText = buf;
                                                                                   tvi.cchTextMax = 1024;
                                                                                   SendMessage(hwnd, TVM_GETITEM, 0, (LPARAM)&tvi);
                                                                                   return Value(std::string(buf));
                                                                               }));

    interp.defineGlobal("gui_treeview_clear", Value::makeNativeFunction("gui_treeview_clear", 1,
                                                                        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                            SendMessage(hwnd, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
                                                                            return Value();
                                                                        }));

    interp.defineGlobal("gui_create_datepicker", Value::makeNativeFunction("gui_create_datepicker", 5,
                                                                           [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                               HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                               HWND hwnd = CreateWindow(DATETIMEPICK_CLASS, "", WS_VISIBLE | WS_CHILD | DTS_SHORTDATEFORMAT,
                                                                                                        (int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), parent, NULL, g_gui.hInstance, NULL);
                                                                               return Value(registerWidgetHandle(hwnd));
                                                                           }));

    interp.defineGlobal("gui_datepicker_get", Value::makeNativeFunction("gui_datepicker_get", 1,
                                                                        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                            SYSTEMTIME st;
                                                                            SendMessage(hwnd, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
                                                                            return Value::makeArray({Value((double)st.wYear), Value((double)st.wMonth), Value((double)st.wDay)});
                                                                        }));

    interp.defineGlobal("gui_datepicker_set", Value::makeNativeFunction("gui_datepicker_set", 4,
                                                                        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                            SYSTEMTIME st = {0};
                                                                            st.wYear = (WORD)vNum(args[1]);
                                                                            st.wMonth = (WORD)vNum(args[2]);
                                                                            st.wDay = (WORD)vNum(args[3]);
                                                                            SendMessage(hwnd, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
                                                                            return Value();
                                                                        }));

    interp.defineGlobal("gui_create_spinner", Value::makeNativeFunction("gui_create_spinner", 7,
                                                                        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                            HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                            int x = (int)vNum(args[1]), y = (int)vNum(args[2]), w = (int)vNum(args[3]), h = (int)vNum(args[4]);
                                                                            HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_RIGHT | ES_NUMBER,
                                                                                                        x, y, w - 20, h, parent, NULL, g_gui.hInstance, NULL);
                                                                            HWND hSpin = CreateWindowEx(0, UPDOWN_CLASS, "", WS_CHILD | WS_VISIBLE | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                                                                                                        x + w - 20, y, 20, h, parent, NULL, g_gui.hInstance, NULL);
                                                                            SendMessage(hSpin, UDM_SETBUDDY, (WPARAM)hEdit, 0);
                                                                            SendMessage(hSpin, UDM_SETRANGE32, (int)vNum(args[5]), (int)vNum(args[6]));

                                                                            // Register both the edit box and the spinner. We will return the spinner handle,
                                                                            // as its value is manipulated via the UDM messages.
                                                                            int handleEdit = g_gui.nextHandle++;
                                                                            g_gui.handleMap[handleEdit] = hEdit;
                                                                            g_gui.reverseHandleMap[hEdit] = handleEdit;
                                                                            int handleSpin = g_gui.nextHandle++;
                                                                            g_gui.handleMap[handleSpin] = hSpin;
                                                                            g_gui.reverseHandleMap[hSpin] = handleSpin;

                                                                            return Value((double)handleSpin);
                                                                        }));

    interp.defineGlobal("gui_spinner_get", Value::makeNativeFunction("gui_spinner_get", 1,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         return Value((double)SendMessage(hwnd, UDM_GETPOS32, 0, 0));
                                                                     }));

    interp.defineGlobal("gui_spinner_set", Value::makeNativeFunction("gui_spinner_set", 2,
                                                                     [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                         HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                         SendMessage(hwnd, UDM_SETPOS32, 0, (LPARAM)vNum(args[1]));
                                                                         return Value();
                                                                     }));

    interp.defineGlobal("gui_create_image", Value::makeNativeFunction("gui_create_image", 6,
                                                                      [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                          HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                          std::string path = vStr(args[1]);
                                                                          HWND hwnd = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | SS_BITMAP,
                                                                                                   (int)vNum(args[2]), (int)vNum(args[3]), (int)vNum(args[4]), (int)vNum(args[5]), parent, NULL, g_gui.hInstance, NULL);
                                                                          HBITMAP hBmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
                                                                          if (hBmp) SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
                                                                          return Value(registerWidgetHandle(hwnd));
                                                                      }));

    interp.defineGlobal("gui_set_image", Value::makeNativeFunction("gui_set_image", 2,
                                                                   [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                       HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                       std::string path = vStr(args[1]);
                                                                       HBITMAP hBmp = (HBITMAP)LoadImage(NULL, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
                                                                       if (hBmp) {
                                                                           HBITMAP hOld = (HBITMAP)SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
                                                                           if (hOld) DeleteObject(hOld);
                                                                       }
                                                                       return Value();
                                                                   }));

    // ANIMATED WEB-STYLE LOADING SPINNER
    WNDCLASSEXW wcs = {0};
    wcs.cbSize = sizeof(WNDCLASSEXW);
    wcs.lpfnWndProc = EZSpinnerWndProc;
    wcs.hInstance = g_gui.hInstance;
    wcs.lpszClassName = L"EZLoadingSpinner";
    wcs.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassExW(&wcs);

    interp.defineGlobal("gui_create_loading_spinner", Value::makeNativeFunction("gui_create_loading_spinner", 4,
                                                                                [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                    HWND parent = g_gui.handleMap[(int)vNum(args[0])];
                                                                                    int size = (int)vNum(args[3]);
                                                                                    HWND hwnd = CreateWindowExW(0, L"EZLoadingSpinner", L"", WS_CHILD,
                                                                                                                (int)vNum(args[1]), (int)vNum(args[2]), size, size, parent, NULL, g_gui.hInstance, NULL);
                                                                                    return Value(registerWidgetHandle(hwnd));
                                                                                }));

    interp.defineGlobal("gui_start_loading_spinner", Value::makeNativeFunction("gui_start_loading_spinner", 1,
                                                                               [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                   HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                   ShowWindow(hwnd, SW_SHOW);
                                                                                   SetTimer(hwnd, 1, 35, NULL);
                                                                                   if (g_spinners.count(hwnd)) g_spinners[hwnd].active = true;
                                                                                   return Value();
                                                                               }));

    interp.defineGlobal("gui_stop_loading_spinner", Value::makeNativeFunction("gui_stop_loading_spinner", 1,
                                                                              [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                  HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                  KillTimer(hwnd, 1);
                                                                                  ShowWindow(hwnd, SW_HIDE);
                                                                                  if (g_spinners.count(hwnd)) g_spinners[hwnd].active = false;
                                                                                  return Value();
                                                                              }));

    interp.defineGlobal("gui_set_loading_spinner_color", Value::makeNativeFunction("gui_set_loading_spinner_color", 4,
                                                                                   [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                                                                                       HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
                                                                                       if (g_spinners.count(hwnd)) {
                                                                                           g_spinners[hwnd].color = RGB((int)vNum(args[1]), (int)vNum(args[2]), (int)vNum(args[3]));
                                                                                           InvalidateRect(hwnd, NULL, FALSE);
                                                                                       }
                                                                                       return Value();
                                                                                   }));
}
