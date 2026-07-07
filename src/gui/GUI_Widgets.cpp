#include "GUI_Internal.h"

void registerGUIWidgetsBuiltins(RuntimeContext& interp) {
    // NEW WIDGET CREATORS
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

    // EXISTING BUILTINS (Re-adapted from previously viewed file)
    
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
