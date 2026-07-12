#include "GUI_Internal.h"
#include <windows.h>

void registerGUIDrawingBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("gui_clear", Value::makeNativeFunction("gui_clear", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int r = (int)vNum(args[1]);
            int g = (int)vNum(args[2]);
            int b = (int)vNum(args[3]);
            HDC hdc = GetDC(hwnd);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    interp.defineGlobal("gui_draw_rect", Value::makeNativeFunction("gui_draw_rect", 8,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]);
            int y = (int)vNum(args[2]);
            int w = (int)vNum(args[3]);
            int h = (int)vNum(args[4]);
            int r = (int)vNum(args[5]);
            int g = (int)vNum(args[6]);
            int b = (int)vNum(args[7]);
            HDC hdc = GetDC(hwnd);
            RECT rect = { x, y, x + w, y + h };
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    interp.defineGlobal("gui_draw_circle", Value::makeNativeFunction("gui_draw_circle", 7,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int x = (int)vNum(args[1]);
            int y = (int)vNum(args[2]);
            int radius = (int)vNum(args[3]);
            int r = (int)vNum(args[4]);
            int g = (int)vNum(args[5]);
            int b = (int)vNum(args[6]);
            HDC hdc = GetDC(hwnd);
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    interp.defineGlobal("gui_draw_text", Value::makeNativeFunction("gui_draw_text", 8,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            std::string text = vStr(args[1]);
            int x = (int)vNum(args[2]);
            int y = (int)vNum(args[3]);
            int size = (int)vNum(args[4]);
            int r = (int)vNum(args[5]);
            int g = (int)vNum(args[6]);
            int b = (int)vNum(args[7]);
            HDC hdc = GetDC(hwnd);
            SetTextColor(hdc, RGB(r, g, b));
            SetBkMode(hdc, TRANSPARENT);
            HFONT font = CreateFontA(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            TextOutA(hdc, x, y, text.c_str(), text.length());
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            ReleaseDC(hwnd, hdc);
            return Value();
        }));

    interp.defineGlobal("gui_is_key_down", Value::makeNativeFunction("gui_is_key_down", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int key = (int)vNum(args[0]);
            return Value((double)((GetAsyncKeyState(key) & 0x8000) ? 1 : 0));
        }));
}
