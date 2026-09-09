#include "GUI_Internal.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

void registerGUIDialogsBuiltins(RuntimeContext& interp) {
    // gui_open_file_dialog(title, filter)
    interp.defineGlobal("gui_open_file_dialog", Value::makeNativeFunction("gui_open_file_dialog", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        std::string title = args[0].isString() ? args[0].asString() : "Open File";
        std::string filter = args[1].isString() ? args[1].asString() : "All Files\0*.*\0";
        
        OPENFILENAMEA ofn;
        char szFile[260];
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            return Value(std::string(ofn.lpstrFile));
        }
        return Value("");
    }));

    // gui_save_file_dialog(title, filter)
    interp.defineGlobal("gui_save_file_dialog", Value::makeNativeFunction("gui_save_file_dialog", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        std::string title = args[0].isString() ? args[0].asString() : "Save File";
        std::string filter = args[1].isString() ? args[1].asString() : "All Files\0*.*\0";
        
        OPENFILENAMEA ofn;
        char szFile[260];
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn) == TRUE) {
            return Value(std::string(ofn.lpstrFile));
        }
        return Value("");
    }));

    // gui_choose_color()
    interp.defineGlobal("gui_choose_color", Value::makeNativeFunction("gui_choose_color", 0, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        CHOOSECOLORA cc;
        static COLORREF acrCustClr[16];
        ZeroMemory(&cc, sizeof(cc));
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = NULL;
        cc.lpCustColors = (LPDWORD)acrCustClr;
        cc.rgbResult = RGB(0, 0, 0);
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (ChooseColorA(&cc) == TRUE) {
            char hex[16];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
            return Value(std::string(hex));
        }
        return Value("");
    }));

    // gui_clipboard_get()
    interp.defineGlobal("gui_clipboard_get", Value::makeNativeFunction("gui_clipboard_get", 0, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!OpenClipboard(NULL)) return Value("");
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == NULL) { CloseClipboard(); return Value(""); }
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText == NULL) { CloseClipboard(); return Value(""); }
        std::string text(pszText);
        GlobalUnlock(hData);
        CloseClipboard();
        return Value(text);
    }));

    // gui_clipboard_set(text)
    interp.defineGlobal("gui_clipboard_set", Value::makeNativeFunction("gui_clipboard_set", 1, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isString()) return Value(false);
        std::string text = args[0].asString();
        
        if (!OpenClipboard(NULL)) return Value(false);
        EmptyClipboard();
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
        if (hMem == NULL) { CloseClipboard(); return Value(false); }
        memcpy(GlobalLock(hMem), text.c_str(), text.length() + 1);
        GlobalUnlock(hMem);
        
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();
        return Value(true);
    }));
}
