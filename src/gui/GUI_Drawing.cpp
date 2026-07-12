#include "GUI_Internal.h"
#include <windows.h>

void registerGUIDrawingBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("gui_begin_draw", Value::makeNativeFunction("gui_begin_draw", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (!g_gui.memDCs.count(hwnd)) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                HDC hdc = GetDC(hwnd);
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP hbm = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
                SelectObject(memDC, hbm);
                g_gui.memDCs[hwnd] = memDC;
                g_gui.memBitmaps[hwnd] = hbm;
                ReleaseDC(hwnd, hdc);
            }
            return Value();
        }));

    interp.defineGlobal("gui_end_draw", Value::makeNativeFunction("gui_end_draw", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            if (g_gui.memDCs.count(hwnd)) {
                HDC memDC = g_gui.memDCs[hwnd];
                HDC hdc = GetDC(hwnd);
                RECT rect;
                GetClientRect(hwnd, &rect);
                BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
                ReleaseDC(hwnd, hdc);
            }
            return Value();
        }));

    interp.defineGlobal("gui_clear", Value::makeNativeFunction("gui_clear", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            int r = (int)vNum(args[1]);
            int g = (int)vNum(args[2]);
            int b = (int)vNum(args[3]);
            bool useMem = g_gui.memDCs.count(hwnd);
            HDC hdc = useMem ? g_gui.memDCs[hwnd] : GetDC(hwnd);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            if (!useMem) ReleaseDC(hwnd, hdc);
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
            bool useMem = g_gui.memDCs.count(hwnd);
            HDC hdc = useMem ? g_gui.memDCs[hwnd] : GetDC(hwnd);
            RECT rect = { x, y, x + w, y + h };
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            if (!useMem) ReleaseDC(hwnd, hdc);
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
            bool useMem = g_gui.memDCs.count(hwnd);
            HDC hdc = useMem ? g_gui.memDCs[hwnd] : GetDC(hwnd);
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
            if (!useMem) ReleaseDC(hwnd, hdc);
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
            bool useMem = g_gui.memDCs.count(hwnd);
            HDC hdc = useMem ? g_gui.memDCs[hwnd] : GetDC(hwnd);
            SetTextColor(hdc, RGB(r, g, b));
            SetBkMode(hdc, TRANSPARENT);
            
            static std::map<int, HFONT> fontCache;
            if (!fontCache.count(size)) {
                fontCache[size] = CreateFontA(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
            }
            HFONT font = fontCache[size];
            
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            TextOutA(hdc, x, y, text.c_str(), text.length());
            SelectObject(hdc, oldFont);
            
            if (!useMem) ReleaseDC(hwnd, hdc);
            return Value();
        }));

    interp.defineGlobal("gui_is_key_down", Value::makeNativeFunction("gui_is_key_down", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int key = (int)vNum(args[0]);
            return Value((double)((GetAsyncKeyState(key) & 0x8000) ? 1 : 0));
        }));

    interp.defineGlobal("gui_get_size", Value::makeNativeFunction("gui_get_size", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            HWND hwnd = g_gui.handleMap[(int)vNum(args[0])];
            RECT rect;
            GetClientRect(hwnd, &rect);
            std::vector<Value> sizeArr = { Value((double)rect.right), Value((double)rect.bottom) };
            return Value::makeArray(sizeArr);
        }));
}
