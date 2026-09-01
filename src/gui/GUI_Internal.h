#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include "gui/GUIBuiltins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
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

struct GUIState {
    HINSTANCE hInstance;
    HWND mainWindow = NULL;
    std::map<int, HWND> handleMap;
    std::map<HWND, int> reverseHandleMap;
    std::map<HWND, Value> callbacks;
    std::map<HWND, std::map<std::string, Value>> eventCallbacks;
    std::map<HWND, WidgetStyle> styles;
    std::map<HWND, HDC> memDCs;
    std::map<HWND, HBITMAP> memBitmaps;
    std::map<UINT_PTR, Value> timerCallbacks;
    std::map<HWND, std::vector<UINT_PTR>> hwndTimers;
    std::map<int, Value> menuCallbacks;
    std::map<HWND, POINT> minSizes;
    
    UINT_PTR nextTimerId = 1;
    int nextHandle = 1;
    int nextMenuId = 1000;
    
    RuntimeContext* currentInterpreter = nullptr;
    std::string currentTheme = "light";
    HBRUSH darkBrush = NULL;
};

extern GUIState g_gui;

inline double vNum(const Value& v) { return v.isNumber() ? v.asNumber() : 0.0; }
inline std::string vStr(const Value& v) { return v.toString(); }

inline std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], size);
    return wstr;
}

inline std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &str[0], size, NULL, NULL);
    return str;
}

Value fireEventCallback(HWND hwnd, const std::string& eventName, const std::vector<Value>& args = {});
bool hasEvent(HWND hwnd, const std::string& eventName);
double registerWidgetHandle(HWND hwnd);

void registerGUICoreBuiltins(RuntimeContext& interp);
void registerGUIWidgetsBuiltins(RuntimeContext& interp);
void registerGUIMenuBuiltins(RuntimeContext& interp);
void registerGUIDialogsBuiltins(RuntimeContext& interp);
void registerGUIDrawingBuiltins(RuntimeContext& interp);

#endif // GUI_INTERNAL_H
