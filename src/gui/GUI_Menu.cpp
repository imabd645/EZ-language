#include "GUI_Internal.h"\n\nvoid registerGUIMenuBuiltins(RuntimeContext& interp) {
    // MENU SYSTEM
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

    // BARS (Status & Toolbar)
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

    // UTILS
}\n