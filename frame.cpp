#include "frame.h"
//#include "edits.h"
#include <windowsx.h>
#include <shlwapi.h>
#include "MDL.h"

char Frame::cClassName[] = "mdleditframe";
HINSTANCE Frame::hInstance = NULL;

HWND hFrame;
Edits Edit1;
HWND hTree;
HWND hStatusBar;
HWND hDisplayEdit;
HWND hTabs;
HWND hGame, hPlatform, hNeck;
MDL Model;
ReportObject ReportModel(Model);
bool bSaveReport = false;
bool bShowHex = false;
bool bShowDiff = true;
bool bShowGroup = false;
bool bShowDataStruct = false;
bool bHexLocation = false;
bool bAnalyze = false;
bool bModelHierarchy = false;
unsigned nEditSize = ME_DISPLAY_SIZE_Y;
const int nCompactOffsetTop = 1;
const int nCompactOffsetBottom = 1;
const int nCompactOffsetLeft = 1;
const int nCompactOffsetRight = 1;
std::wstring sExePath;
Version version (1,0,5, true);
WNDPROC MainTreeProc = NULL;
WNDPROC MainDisplayProc = NULL;
LRESULT APIENTRY TreeSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT APIENTRY DisplaySubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool bEditDrag = false;
HHOOK hMessageHook = NULL;
namespace {
    constexpr int WINDOW_NONHEX_WIDTH = 368;
    constexpr int WINDOW_NONHEX_HEIGHT = 610;
    constexpr int WINDOW_HEX_WIDTH = 980;
    constexpr int WINDOW_HEX_HEIGHT = 610;
}
HFONT hMonospace = NULL, hShell = NULL, hTimes = NULL;

void FixHead(){
    bool bLinkHead = Button_GetCheck(hNeck) == BST_CHECKED;
    if(bLinkHead){
        if(Model.LinkHead(bLinkHead)){
            Edit1.UpdateEdit();
            HTREEITEM hSel = TreeView_GetSelection(hTree);
            if(hSel != NULL) ProcessTreeAction(hSel, ACTION_UPDATE_DISPLAY);
        }
        else std::cout << "neck_g was not found!\n";
    }
}

Frame::Frame(HINSTANCE hInstanceCreate){
    hInstance = hInstanceCreate; // Save Instance handle

    /// Save executable path
    DWORD nExePathSize = MAX_PATH;
    for(;;){
        sExePath.assign(nExePathSize, L'\0');
        DWORD nChars = GetModuleFileNameW(hInstance, &sExePath.front(), nExePathSize);
        if(nChars == 0){
            sExePath.clear();
            break;
        }
        if(nChars < nExePathSize - 1){
            sExePath.resize(nChars);
            break;
        }
        if(nExePathSize > 32768){
            sExePath.resize(nChars);
            break;
        }
        nExePathSize *= 2;
    }
    std::cout << "Running " << to_ansi(sExePath) << "\n";

    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = hInstance; // Instance of the application
    WindowClass.lpfnWndProc = FrameProc; // Pointer to callback procedure

    // #2 Class styles
    WindowClass.style = CS_DBLCLKS; // Class styles

    // #3 Background
    WindowClass.hbrBackground = CreateSolidBrush(RGB(250, 250, 250)); //(HBRUSH) (COLOR_WINDOW); // Background brush

    // #4 Cursor
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor

    // #5 Icon
    WindowClass.hIcon = /* */ LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)); // /**/ LoadIcon (NULL, IDI_APPLICATION); //LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)); // Class Icon
    WindowClass.hIconSm = /* */ LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2)); // /**/ LoadIcon(NULL, IDI_APPLICATION); //LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)); // Small icon for this class

    // #6 Menu
    WindowClass.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN); // Menu Resource

    // #7 Other
    WindowClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    WindowClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure
}

bool Frame::Run(int nCmdShow){
    if(!RegisterClassEx(&WindowClass)) return false;

    // Legacy common-control initialization alternatives retained for reference.
#if 0
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES; // ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    INITCOMMONCONTROLSEX icx;
    icx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icx.dwICC = ICC_BAR_CLASSES; // Specify BAR classes
    InitCommonControlsEx(&icx); // Load the common control DLL
#endif

    hFrame = CreateWindowEx(0, cClassName, std::string("MDLedit "+version.Print()).c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_NONHEX_WIDTH, WINDOW_NONHEX_HEIGHT,
                        HWND_DESKTOP, nullptr, hInstance, nullptr);
    hMe = hFrame;
    if(!hMe) return false;
    ShowWindow(hMe, nCmdShow);

    return true;
}

LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if(nCode < 0) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    MSG & msg = * (MSG*) lParam;
    POINT pt = msg.pt;
    ScreenToClient(msg.hwnd, &pt);
    RECT rcClient;
    GetClientRect(msg.hwnd, &rcClient);
    if(msg.hwnd == hTree){
        if(pt.y < 5){
            msg.hwnd = hFrame;
            ScreenToClient(hFrame, &pt);
            msg.lParam = MAKELPARAM((WORD) pt.x, (WORD) pt.y);
        }
    }
    else if(msg.hwnd == hDisplayEdit){
        if(rcClient.bottom - rcClient.top - pt.y < 5){
            msg.hwnd = hFrame;
            ScreenToClient(hFrame, &pt);
            msg.lParam = MAKELPARAM((WORD) pt.x, (WORD) pt.y);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Frame::FrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    static RECT rcClient;
    //static char cFile[MAX_PATH];
    static std::wstring sFile = std::wstring(1, '\0');
    /* handle the messages */

    static HWND hIntLabel;
    static HWND hUIntLabel;
    static HWND hFloatLabel;
    switch(message){
        case WM_CREATE:
        {
            GetClientRect(hwnd, &rcClient);

            std::string sMonospace = "Consolas";
            if(!Font_IsInstalled(sMonospace)){
                std::cout << "Consolas font not installed! Switching to Courier New...\n";
                sMonospace = "Courier New";
                if(!Font_IsInstalled(sMonospace)){
                    std::cout << "Courier New font not installed! No further alternatives!\n";
                }
            }

            hMonospace = CreateFont(
                14, //Size
                0,  //??
                0,  //??
                0,  //??
                FW_REGULAR, // font weight
                FALSE,	    // italic attribute flag
                FALSE,	    // underline attribute flag
                FALSE,	    // strikeout attribute flag
                DEFAULT_CHARSET,	    // character set identifier
                OUT_DEFAULT_PRECIS	,	// output precision
                CLIP_DEFAULT_PRECIS	,	// clipping precision
                DEFAULT_QUALITY	,	    // output quality
                DEFAULT_PITCH | FF_MODERN	,	// pitch and family
                sMonospace.c_str() 	// pointer to typeface name string
            );
            hShell = CreateFont(
                14,  //Height
                0,  //Width
                0,  //??
                0,  //??
                FW_REGULAR, // font weight
                FALSE,	    // italic attribute flag
                FALSE,	    // underline attribute flag
                FALSE,	    // strikeout attribute flag
                DEFAULT_CHARSET,	    // character set identifier
                OUT_DEFAULT_PRECIS	,	// output precision
                CLIP_DEFAULT_PRECIS	,	// clipping precision
                DEFAULT_QUALITY	,	    // output quality
                DEFAULT_PITCH | FF_DONTCARE	,	// pitch and family
                "MS Shell Dlg" //"Segoe UI" 	// pointer to typeface name string
            );
            hTimes = CreateFont(
                14,  //Height
                4,  //Width
                0,  //??
                0,  //??
                FW_REGULAR, // font weight
                FALSE,	    // italic attribute flag
                FALSE,	    // underline attribute flag
                FALSE,	    // strikeout attribute flag
                DEFAULT_CHARSET,	    // character set identifier
                OUT_DEFAULT_PRECIS	,	// output precision
                CLIP_DEFAULT_PRECIS	,	// clipping precision
                DEFAULT_QUALITY	,	    // output quality
                DEFAULT_PITCH | FF_DONTCARE	,	// pitch and family
                "Times New Roman" //"Segoe UI" //"MS Shell Dlg" // 	// pointer to typeface name string
            );

            int nLabelOffsetX = ME_HEX_WIN_OFFSET_X + ME_HEX_WIN_SIZE_X + ME_DATA_LABEL_OFFSET_X;
            int nEditOffsetX = ME_HEX_WIN_OFFSET_X + ME_HEX_WIN_SIZE_X + ME_DATA_LABEL_OFFSET_X + ME_DATA_LABEL_SIZE_X + ME_DATA_EDIT_OFFSET_X;
            int nDataOffsetY [5];
            for(int n = 0; n < 5; n++) nDataOffsetY[n] = ME_BASIC_OFFSET_Y + n * ME_DATA_NEXT_ROW;
            hIntLabel = CreateWindowEx(0, "STATIC", "Int:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
                                        nLabelOffsetX, nDataOffsetY[0] + ME_DATA_LABEL_ROW_OFFSET_Y, ME_DATA_LABEL_SIZE_X, ME_DATA_LABEL_SIZE_Y,
                                        hwnd, (HMENU) IDC_LBL_INT, GetModuleHandle(nullptr), nullptr);
            hUIntLabel = CreateWindowEx(0, "STATIC", "uInt:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
                                        nLabelOffsetX, nDataOffsetY[1] + ME_DATA_LABEL_ROW_OFFSET_Y, ME_DATA_LABEL_SIZE_X, ME_DATA_LABEL_SIZE_Y,
                                        hwnd, (HMENU) IDC_LBL_UINT, GetModuleHandle(nullptr), nullptr);
            hFloatLabel = CreateWindowEx(0, "STATIC", "Float:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
                                        nLabelOffsetX, nDataOffsetY[2] + ME_DATA_LABEL_ROW_OFFSET_Y, ME_DATA_LABEL_SIZE_X, ME_DATA_LABEL_SIZE_Y,
                                        hwnd, (HMENU) IDC_LBL_FLOAT, GetModuleHandle(nullptr), nullptr);
            SendMessage(hIntLabel, WM_SETFONT, (WPARAM) hShell, MAKELPARAM(TRUE, 0));
            SendMessage(hUIntLabel, WM_SETFONT, (WPARAM) hShell, MAKELPARAM(TRUE, 0));
            SendMessage(hFloatLabel, WM_SETFONT, (WPARAM) hShell, MAKELPARAM(TRUE, 0));
            ShowWindow(hIntLabel, false);
            ShowWindow(hUIntLabel, false);
            ShowWindow(hFloatLabel, false);

            Edits::hIntEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_READONLY, // | ES_RIGHT,
                                        nEditOffsetX, nDataOffsetY[0], ME_DATA_EDIT_SIZE_X, ME_DATA_EDIT_SIZE_Y,
                                        hwnd, (HMENU) IDC_EDIT_INT, GetModuleHandle(nullptr), nullptr);
            Edits::hUIntEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_READONLY, // | ES_RIGHT,
                                        nEditOffsetX, nDataOffsetY[1], ME_DATA_EDIT_SIZE_X, ME_DATA_EDIT_SIZE_Y,
                                        hwnd, (HMENU) IDC_EDIT_UINT, GetModuleHandle(nullptr), nullptr);
            Edits::hFloatEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_READONLY, // | ES_RIGHT,
                                        nEditOffsetX, nDataOffsetY[2], ME_DATA_EDIT_SIZE_X, ME_DATA_EDIT_SIZE_Y,
                                        hwnd, (HMENU) IDC_EDIT_FLOAT, GetModuleHandle(nullptr), nullptr);
            SendMessage(Edits::hIntEdit, WM_SETFONT, (WPARAM) hMonospace, MAKELPARAM(TRUE, 0));
            SendMessage(Edits::hUIntEdit, WM_SETFONT, (WPARAM) hMonospace, MAKELPARAM(TRUE, 0));
            SendMessage(Edits::hFloatEdit, WM_SETFONT, (WPARAM) hMonospace, MAKELPARAM(TRUE, 0));
            ShowWindow(Edits::hIntEdit, false);
            ShowWindow(Edits::hUIntEdit, false);
            ShowWindow(Edits::hFloatEdit, false);

            int nButtonOffsetX [3];
            int nButtonSizeX = (rcClient.right - rcClient.left - ME_TREE_OFFSET_X - 5 - 10) / 3;
            for(int n = 0; n < 3; n++) nButtonOffsetX[n] = ME_TREE_OFFSET_X + n * (nButtonSizeX + 5);
            hGame = CreateWindowEx(0, "BUTTON", "Game", WS_VISIBLE | WS_CHILD,
                                   nButtonOffsetX[0], nDataOffsetY[3], nButtonSizeX, ME_DATA_EDIT_SIZE_Y,
                                   hwnd, (HMENU) IDC_BTN_GAME, GetModuleHandle(nullptr), nullptr);
            hPlatform = CreateWindowEx(0, "BUTTON", "Platform", WS_VISIBLE | WS_CHILD,
                                   nButtonOffsetX[1], nDataOffsetY[3], nButtonSizeX, ME_DATA_EDIT_SIZE_Y,
                                   hwnd, (HMENU) IDC_BTN_PLATFORM, GetModuleHandle(nullptr), nullptr);
            hNeck = CreateWindowEx(0, "BUTTON", "Head Link", WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE | WS_DISABLED,
                                   nButtonOffsetX[2], nDataOffsetY[3], nButtonSizeX, ME_DATA_EDIT_SIZE_Y,
                                   hwnd, (HMENU) IDC_BTN_NECK, GetModuleHandle(nullptr), nullptr);
            SendMessage(hGame, WM_SETFONT, (WPARAM) hTimes, MAKELPARAM(TRUE, 0));
            SendMessage(hPlatform, WM_SETFONT, (WPARAM) hTimes, MAKELPARAM(TRUE, 0));
            SendMessage(hNeck, WM_SETFONT, (WPARAM) hTimes, MAKELPARAM(TRUE, 0));

            hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "", hwnd, IDC_STATUSBAR);
            int nBorders [4];
            nBorders[0] = ME_STATUSBAR_PART_X;
            nBorders[1] = 2 * ME_STATUSBAR_PART_X;
            nBorders[2] = 3 * ME_STATUSBAR_PART_X;
            nBorders[3] = -1;
            SendMessage(hStatusBar, SB_SETPARTS, (WPARAM) 4, (LPARAM) nBorders);
            ShowWindow(hStatusBar, false);

            hTree = CreateWindowEx(WS_EX_TOPMOST, WC_TREEVIEW, "Structure", WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                           ME_TREE_OFFSET_X, ME_DISPLAY_OFFSET_Y + nEditSize + 1, ME_TREE_SIZE_X, rcClient.bottom - ME_DISPLAY_OFFSET_Y - 1 - nEditSize - ME_STATUSBAR_Y - ME_TREE_SIZE_DIFF_Y,//rcClient.bottom - ME_TREE_OFFSET_Y - ME_STATUSBAR_Y - ME_TREE_SIZE_DIFF_Y,
                           hwnd, (HMENU) IDC_TREEVIEW, GetModuleHandle(nullptr), nullptr);
            hDisplayEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_READONLY | ES_AUTOVSCROLL | ES_MULTILINE | WS_VSCROLL,
                                        ME_TREE_OFFSET_X, nDataOffsetY[4], ME_TREE_SIZE_X, nEditSize,
                                        hwnd, (HMENU) IDC_EDIT_DISPLAY, GetModuleHandle(nullptr), nullptr);
            SendMessage(hDisplayEdit, WM_SETFONT, (WPARAM) hMonospace, MAKELPARAM(TRUE, 0));
            //MainTreeProc = (WNDPROC) SetWindowLong(hTree, GWLP_WNDPROC, (LONG) TreeSubclassProc);
            //MainDisplayProc = (WNDPROC) SetWindowLong(hDisplayEdit, GWLP_WNDPROC, (LONG) DisplaySubclassProc);

            hTabs = CreateWindowEx(0, WC_TABCONTROL, "", WS_VISIBLE | WS_CHILD | TCS_FOCUSNEVER | TCS_FIXEDWIDTH,
                                   ME_HEX_WIN_OFFSET_X, 0, ME_HEX_WIN_OFFSET_X + ME_TABS_SIZE_X, rcClient.bottom - ME_STATUSBAR_Y,
                                   hwnd, (HMENU) IDC_TABS, GetModuleHandle(nullptr), nullptr);
            SendMessage(hTabs, WM_SETFONT, (WPARAM) hTimes, MAKELPARAM(TRUE, 0));
            ShowWindow(hTabs, false);

            if(!Edit1.Run(hwnd, IDC_MAIN_EDIT)){
                std::cout << "Major error, creation of Edit1 window failed.\n";
            }

            //hMessageHook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, NULL, GetCurrentThreadId());

            ManageIni(INI_READ);

            std::string sUpdate;
            if(Model.bK2) sUpdate = "KOTOR2";
            else sUpdate = "KOTOR1";
            SetWindowText(hGame, sUpdate.c_str());
            if(Model.bXbox) sUpdate = "XBOX";
            else sUpdate = "PC";
            SetWindowText(hPlatform, sUpdate.c_str());

            MENUITEMINFO mii;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_STATE;

            if(bShowGroup) mii.fState = MFS_CHECKED;
            else mii.fState = MFS_UNCHECKED;
            SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_GROUP, false, &mii);

            if(bShowDataStruct) mii.fState = MFS_CHECKED;
            else mii.fState = MFS_UNCHECKED;
            SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DATASTRUCT, false, &mii);


            if(bModelHierarchy) mii.fState = MFS_CHECKED;
            else mii.fState = MFS_UNCHECKED;
            SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_HIE, false, &mii);

            if(!bModelHierarchy) mii.fState = MFS_CHECKED;
            else mii.fState = MFS_UNCHECKED;
            SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_LIN, false, &mii);

            if(!bAnalyze) RemoveMenu(GetMenu(hwnd), IDM_MASS_ANALYZE, MF_BYCOMMAND);
        }
        break;
        case WM_NOTIFY:
        {
            NMHDR * nmhdr = (NMHDR *) lParam;
            int nID = nmhdr->idFrom;
            UINT nNotification = nmhdr->code;
            switch(nID){
                case IDC_TREEVIEW:
                {
                    if(nNotification == TVN_SELCHANGED){
                        NMTREEVIEW * nmtv = (NMTREEVIEW *) nmhdr;
                        ProcessTreeAction(nmtv->itemNew.hItem, ACTION_UPDATE_DISPLAY);
                    }
                    else if(nNotification == NM_RCLICK){
                        POINT pt;
                        GetCursorPos(&pt);
                        RECT rcItem;
                        HTREEITEM hSel = NULL;
                        HTREEITEM hSearch = TreeView_GetFirstVisible(hTree);
                        while(hSearch != NULL){
                            TreeView_GetItemRect(hTree, hSearch, &rcItem, true);
                            MapWindowPoints(hTree, HWND_DESKTOP, (LPPOINT) &rcItem, 2);
                            //std::cout << string_format("Point: (%i, %i), Rect: (%i, %i, %i, %i)\n", pt.x, pt.y, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom);
                            if(PtInRect(&rcItem, pt)){
                                hSel = hSearch;
                                hSearch = NULL;
                            }
                            else{
                                hSearch = TreeView_GetNextVisible(hTree, hSearch);
                            }
                        }

                        if(hSel != NULL){
                            TreeView_SelectItem(hTree, hSel);

                            MenuLineAdder mla;
                                mla.hMenu = CreatePopupMenu();
                                mla.nIndex = 0;

                            ProcessTreeAction(hSel, ACTION_ADD_MENU_LINES, (void*) &mla);

                            bool bExpanded = (TreeView_GetItemState(hTree, hSel, TVIS_EXPANDED) & TVIS_EXPANDED);
                            bool bHasChild = (TreeView_GetChild(hTree, hSel) != NULL);
                            bool bExpandedChild = false;
                            bool bChildHasChild = false;
                            HTREEITEM hChild = TreeView_GetChild(hTree, hSel);
                            while(hChild != NULL && !bExpandedChild && bExpanded && bHasChild){
                                if(TreeView_GetChild(hTree, hChild) != NULL){
                                    bChildHasChild = true;
                                }
                                if(TreeView_GetItemState(hTree, hChild, TVIS_EXPANDED) & TVIS_EXPANDED){
                                    bExpandedChild = true;
                                }
                                else hChild = TreeView_GetNextSibling(hTree, hChild);
                            }

                            if(bHasChild && !bExpanded){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_EXPAND, "Expand");
                                mla.nIndex++;
                            }
                            else if(bHasChild){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_FOLD, "Collapse");
                                mla.nIndex++;
                            }
                            else if(mla.nIndex == 0){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, 0, "Nothing");
                                mla.nIndex++;
                            }

                            if(bHasChild){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_EXPAND_ALL, "Expand all");
                                mla.nIndex++;
                                if(bExpanded){
                                    InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_FOLD_ALL, "Collapse all");
                                    mla.nIndex++;
                                }
                            }

                            if(bExpandedChild && bExpanded){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_FOLD_CHILDREN, "Collapse children only");
                                mla.nIndex++;
                            }
                            else if(bChildHasChild && bExpanded){
                                InsertMenu(mla.hMenu, mla.nIndex, MF_BYPOSITION | MF_STRING, IDPM_TV_EXPAND_CHILDREN, "Expand children only");
                                mla.nIndex++;
                            }

                            TrackPopupMenu(mla.hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
                        }
                        else{
                            //It seems like I'm not on an item at all.
                        }
                    }
                }
                break;
                case IDC_TABS:
                {
                    if(nNotification == TCN_SELCHANGE){
                        Edit1.LoadData();
                    }
                }
                break;
            }
        }
        break;
        case WM_COMMAND:
        {
            int nID = LOWORD(wParam);
            switch(nID){
                case IDM_FILE_EXIT:
                {
                    SendMessage(hwnd, WM_DESTROY, 0, 0);
                }
                break;
                case IDM_HELP:
                {
                    OpenHelpDlg();
                }
                break;
                case IDM_SHOW_REPORT:
                {
                    OpenReportDlg(Model);
                }
                break;
                case IDM_TREE_SORT_LIN:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_LIN, false, &mii);
                    if(!(mii.fState & MFS_CHECKED)){
                        bModelHierarchy = false;
                        mii.fState = MFS_CHECKED;
                        SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_LIN, false, &mii);
                        mii.fState = MFS_UNCHECKED;
                        SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_HIE, false, &mii);

                        /// Update tree view
                        HTREEITEM htiRoot = TreeView_GetRoot(hTree);
                        HTREEITEM htiAnimations = TreeView_GetChildByText(hTree, htiRoot, "Animations");
                        if(htiAnimations != NULL){
                            BuildAnimationTree(htiAnimations, Model);
                        }
                        HTREEITEM htiGeometry = TreeView_GetChildByText(hTree, htiRoot, "Geometry");
                        if(htiGeometry != NULL){
                            BuildGeometryTree(htiGeometry, Model);
                        }

                        /// Update .ini
                        ManageIni(INI_WRITE);
                    }
                }
                break;
                case IDM_TREE_SORT_HIE:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_HIE, false, &mii);
                    if(!(mii.fState & MFS_CHECKED)){
                        bModelHierarchy = true;
                        mii.fState = MFS_CHECKED;
                        SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_HIE, false, &mii);
                        mii.fState = MFS_UNCHECKED;
                        SetMenuItemInfo(GetMenu(hwnd), IDM_TREE_SORT_LIN, false, &mii);

                        /// Update tree view
                        HTREEITEM htiRoot = TreeView_GetRoot(hTree);
                        HTREEITEM htiAnimations = TreeView_GetChildByText(hTree, htiRoot, "Animations");
                        if(htiAnimations != NULL){
                            BuildAnimationTree(htiAnimations, Model);
                        }
                        HTREEITEM htiGeometry = TreeView_GetChildByText(hTree, htiRoot, "Geometry");
                        if(htiGeometry != NULL){
                            BuildGeometryTree(htiGeometry, Model);
                        }

                        /// Update .ini
                        ManageIni(INI_WRITE);
                    }
                }
                break;
                case IDM_SHOW_GROUP:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_GROUP, false, &mii);
                    bShowGroup = !(mii.fState & MFS_CHECKED); //Revert it, because user just clicked it so we need to turn it off/on
                    if(bShowGroup) mii.fState = MFS_CHECKED;
                    else mii.fState = MFS_UNCHECKED;
                    SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_GROUP, false, &mii);
                    Edit1.UpdateEdit();
                    ManageIni(INI_WRITE);
                }
                break;
                case IDM_SHOW_DATASTRUCT:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DATASTRUCT, false, &mii);
                    bShowDataStruct = !(mii.fState & MFS_CHECKED); //Revert it, because user just clicked it so we need to turn it off/on
                    if(bShowDataStruct) mii.fState = MFS_CHECKED;
                    else mii.fState = MFS_UNCHECKED;
                    SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DATASTRUCT, false, &mii);
                    Edit1.UpdateEdit();
                    ManageIni(INI_WRITE);
                }
                break;
                break;
                case IDM_MASS_TO_ASCII:
                case IDM_MASS_TO_BIN:
                case IDM_MASS_ANALYZE:
                {
                    FileEditor(hwnd, nID, sFile);
                }
                break;
                case IDM_MDL_OPEN:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    std::string sUpdate;
                    bool bSuccess = FileEditor(hwnd, nID, sFile);
                    if(bSuccess){
                        if(!Model.NodeExists("neck_g")) Button_Enable(hNeck, false);
                        else Button_Enable(hNeck, true);

                        bool bLinkHead = Model.HeadLinked();
                        if(bLinkHead) Button_SetCheck(hNeck, BST_CHECKED);
                        else Button_SetCheck(hNeck, BST_UNCHECKED);

                        if(Model.bK2) sUpdate = "KOTOR2";
                        else sUpdate = "KOTOR1";
                        SetWindowText(hGame, sUpdate.c_str());

                        if(Model.bXbox) sUpdate = "XBOX";
                        else sUpdate = "PC";
                        SetWindowText(hPlatform, sUpdate.c_str());

                        if(!Model.empty()) mii.fState = MFS_ENABLED;
                        else mii.fState = MFS_GRAYED;
                        SetMenuItemInfo(GetSubMenu(GetMenu(hwnd), 0), 1, true, &mii);
                        SetMenuItemInfo(GetMenu(hwnd), IDM_BIN_COMPARE, false, &mii);
                        SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_REPORT, false, &mii);
                        GetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);
                        if(!Model.GetCompareData().empty()) mii.fState = MFS_ENABLED | (mii.fState & MFS_CHECKED ? MFS_CHECKED : MFS_UNCHECKED);
                        else mii.fState = MFS_GRAYED | (mii.fState & MFS_CHECKED ? MFS_CHECKED : MFS_UNCHECKED);
                        SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);

                        ManageIni(INI_WRITE);
                    }
                    std::wstring sNewName = L"MDLedit " + to_wide(version.Print());
                    if(!Model.empty()) sNewName += L" (" + Model.GetFilename() + L")";
                    SetWindowTextW(hwnd, sNewName.c_str());
                }
                break;
                case IDM_BIN_COMPARE:
                {
                    FileEditor(hwnd, nID, sFile);
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);
                    if(!Model.GetCompareData().empty()) mii.fState = MFS_ENABLED | (mii.fState & MFS_CHECKED ? MFS_CHECKED : MFS_UNCHECKED);
                    else mii.fState = MFS_GRAYED | (mii.fState & MFS_CHECKED ? MFS_CHECKED : MFS_UNCHECKED);
                    SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);
                    Edit1.UpdateEdit();
                }
                break;
                case IDM_SHOW_DIFF:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);
                    bShowDiff = !(mii.fState & MFS_CHECKED); //Revert it, because user just clicked it so we need to turn it off/on
                    if(bShowDiff) mii.fState = MFS_CHECKED;
                    else mii.fState = MFS_UNCHECKED;
                    SetMenuItemInfo(GetMenu(hwnd), IDM_SHOW_DIFF, false, &mii);
                    Edit1.UpdateEdit();
                    Edit1.PrintValues();
                }
                break;
                case IDM_BIN_SAVE:
                {
                    int nReturn = IDOK;
                    if(Model.GetBuffer().empty()) nReturn = IDCANCEL;
                    if(nReturn == IDOK) (void) FileEditor(hwnd, nID, sFile);
                }
                break;
                case IDM_ASCII_SAVE:
                {
                    int nReturn = IDOK;
                    if(!Model.GetFileData()) nReturn = IDCANCEL;
                    if(!Model.Mdx && nReturn == IDOK) nReturn = MessageBox(hwnd, "Warning! No MDX is loaded! MDLedit can still export without the MDX data, but this means exporting without weights, UVs, smoothing groups and for xbox binaries also vert coords.", "Warning!", MB_OKCANCEL | MB_ICONWARNING);
                    if(nReturn == IDOK) (void) FileEditor(hwnd, nID, sFile);
                }
                break;
                case IDC_BTN_NECK:
                {
                    if(!Model.empty()){
                        bool bLinkHead = !(Button_GetCheck(hNeck) == BST_CHECKED);
                        if(Model.LinkHead(bLinkHead)){
                            if(bLinkHead) Button_SetCheck(hNeck, BST_CHECKED);
                            else Button_SetCheck(hNeck, BST_UNCHECKED);
                            Edit1.UpdateEdit();
                            HTREEITEM hSel = TreeView_GetSelection(hTree);
                            if(hSel != NULL) ProcessTreeAction(hSel, ACTION_UPDATE_DISPLAY);
                        }
                        else std::cout << "neck_g was not found!\n";
                    }
                }
                break;
                case IDC_BTN_GAME:
                {
                    bool bConfirm = true;

                    if(Model.GetFileData()){
                        FileHeader & Data = *Model.GetFileData();

                        if(Data.MH.cSupermodelName.c_str() != std::string("NULL"))
                            bConfirm = IDYES == WarningYesNo("This model was loaded with a supermodel from the currently selected game. "
                                                             "Are you sure you want to change the target game?");
                        //if(Model.nSupermodel == 2 && Model.bK2 || Model.nSupermodel == 1 && !Model.bK2)
                        //    bConfirm = IDYES == WarningYesNo("This model refers to a supermodel from the currently selected game. Are you sure you want to change the target game for this model?");
                    }

                    if(bConfirm){
                        Model.bK2 = !Model.bK2;
                        if(Model.GetFileData()){
                            Model.Compile();
                            FixHead();
                            Edit1.LoadData();
                        }

                        std::string sUpdate;
                        if(Model.bK2) sUpdate = "KOTOR2";
                        else sUpdate = "KOTOR1";
                        SetWindowText(hGame, sUpdate.c_str());
                        ManageIni(INI_WRITE);
                    }
                }
                break;
                case IDC_BTN_PLATFORM:
                {
                    Model.bXbox = !Model.bXbox;
                    if(Model.GetFileData()){
                        Model.Compile();
                        FixHead();
                        Edit1.LoadData();
                    }

                    std::string sUpdate;
                    if(Model.bXbox) sUpdate = "XBOX";
                    else sUpdate = "PC";
                    SetWindowText(hPlatform, sUpdate.c_str());
                    ManageIni(INI_WRITE);
                }
                break;
                case IDD_EDITOR_DLG:
                {
                    Edit1.UpdateEdit();
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    if(hSel != NULL) ProcessTreeAction(hSel, ACTION_UPDATE_DISPLAY);
                }
                break;
                case IDM_SETTINGS:
                {
                    DialogBoxParam(NULL, MAKEINTRESOURCE(DLG_SETTINGS), hwnd, SettingsProc, (LPARAM) &Model);
                }
                break;
                case IDM_EDIT_TEXTURES:
                {
                    if(2 == DialogBoxParam(NULL, MAKEINTRESOURCE(DLG_EDIT_TEXTURES), hwnd, TexturesProc, (LPARAM) &Model)){
                        std::cout << "Cause model reprocessing!\n";
                        Model.Compile();
                        FixHead();
                    }
                    Edit1.UpdateEdit();
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    if(hSel != NULL) ProcessTreeAction(hSel, ACTION_UPDATE_DISPLAY);
                }
                break;
                case IDM_VIEW_HEX:
                {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_STATE;
                    GetMenuItemInfo(GetMenu(hwnd), IDM_VIEW_HEX, false, &mii);
                    bShowHex = !(mii.fState & MFS_CHECKED); //Revert it, because user just clicked it so we need to turn it off/on
                    if(bShowHex) mii.fState = MFS_CHECKED;
                    else mii.fState = MFS_UNCHECKED;
                    SetMenuItemInfo(GetMenu(hwnd), IDM_VIEW_HEX, false, &mii);

                    Edit1.ShowHideEdit();
                    if(bShowHex){
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_INT), true);
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_UINT), true);
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_FLOAT), true);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_INT), true);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_UINT), true);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_FLOAT), true);
                        ShowWindow(hTabs, true);
                        ShowWindow(hStatusBar, true);
                        Edit1.UpdateEdit();
                        SetWindowPos(hwnd, nullptr, 0, 0, WINDOW_HEX_WIDTH, WINDOW_HEX_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);

                    }
                    else{
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_INT), false);
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_UINT), false);
                        ShowWindow(GetDlgItem(hwnd, IDC_LBL_FLOAT), false);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_INT), false);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_UINT), false);
                        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_FLOAT), false);
                        ShowWindow(hTabs, false);
                        ShowWindow(hStatusBar, false);
                        SetWindowPos(hwnd, nullptr, 0, 0, WINDOW_NONHEX_WIDTH, WINDOW_NONHEX_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
                    }

                }
                break;
                case IDM_MDLEDIT:
                {
                    DialogBox(NULL, MAKEINTRESOURCE(DLG_ABOUT), hwnd, AboutProc);
                }
                break;
                case IDPM_TV_FOLD:
                case IDPM_TV_EXPAND:
                {
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    TreeView_Expand(hTree, hSel, TVE_TOGGLE);
                }
                break;
                case IDPM_TV_FOLD_ALL:
                case IDPM_TV_EXPAND_ALL:
                {
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    if(!TreeView_ExpandAll(hTree, hSel, (IDPM_TV_FOLD_ALL == nID ? TVE_COLLAPSE : TVE_EXPAND))) std::cout << "Expand/Collapse All failed!\n";
                }
                break;
                case IDPM_TV_FOLD_CHILDREN:
                {
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    HTREEITEM hChild = TreeView_GetChild(hTree, hSel);
                    while(hChild != NULL){
                        TreeView_Expand(hTree, hChild, TVE_COLLAPSE);
                        hChild = TreeView_GetNextSibling(hTree, hChild);
                    }
                }
                break;
                case IDPM_TV_EXPAND_CHILDREN:
                {
                    HTREEITEM hSel = TreeView_GetSelection(hTree);
                    HTREEITEM hChild = TreeView_GetChild(hTree, hSel);
                    while(hChild != NULL){
                        TreeView_Expand(hTree, hChild, TVE_EXPAND);
                        hChild = TreeView_GetNextSibling(hTree, hChild);
                    }
                }
                break;
                case IDPM_VIEW_ASCII:
                {
                    ProcessTreeAction(TreeView_GetSelection(hTree), ACTION_OPEN_VIEWER, NULL);
                }
                break;
                case IDPM_OPEN_EDITOR:
                {
                    ProcessTreeAction(TreeView_GetSelection(hTree), ACTION_OPEN_EDITOR, NULL);
                }
                break;
                case IDPM_SCROLL:
                {
                    ProcessTreeAction(TreeView_GetSelection(hTree), ACTION_SCROLL, NULL);
                }
                break;
            }
        }
        break;
        case WM_SIZE:
        {
            GetClientRect(hwnd, &rcClient);
            if(bShowHex){
                SetWindowPos(hDisplayEdit, nullptr,
                             ME_TREE_OFFSET_X,
                             ME_DISPLAY_OFFSET_Y,
                             (rcClient.right - rcClient.left) - ME_HEX_WIN_OFFSET_X - ME_TREE_OFFSET_X - 5,
                             nEditSize,
                             0);
                SetWindowPos(hTree, nullptr,
                             ME_TREE_OFFSET_X,
                             ME_DISPLAY_OFFSET_Y + nEditSize + 2, //ME_TREE_OFFSET_Y,
                             (rcClient.right - rcClient.left) - ME_HEX_WIN_OFFSET_X - ME_TREE_OFFSET_X - 5,
                             rcClient.bottom - (ME_DISPLAY_OFFSET_Y + nEditSize + 2) - ME_STATUSBAR_Y - ME_TREE_SIZE_DIFF_Y + 5,
                             0);
                int nEditOffsetX = ME_HEX_WIN_OFFSET_X + ME_HEX_WIN_SIZE_X + ME_DATA_LABEL_OFFSET_X + ME_DATA_LABEL_SIZE_X + ME_DATA_EDIT_OFFSET_X;
                SetWindowPos(Edits::hIntEdit, nullptr,
                             0,
                             0,
                             (rcClient.right - rcClient.left) - nEditOffsetX - 5,
                             ME_DATA_EDIT_SIZE_Y,
                             SWP_NOMOVE);
                SetWindowPos(Edits::hUIntEdit, nullptr,
                             0,
                             0,
                             (rcClient.right - rcClient.left) - nEditOffsetX - 5,
                             ME_DATA_EDIT_SIZE_Y,
                             SWP_NOMOVE);
                SetWindowPos(Edits::hFloatEdit, nullptr,
                             0,
                             0,
                             (rcClient.right - rcClient.left) - nEditOffsetX - 5,
                             ME_DATA_EDIT_SIZE_Y,
                             SWP_NOMOVE);
                int nButtonSizeX = (rcClient.right - rcClient.left - ME_TREE_OFFSET_X + 1 - 4) / 3;
                int nDataOffsetY [5];
                for(int n = 0; n < 5; n++) nDataOffsetY[n] = ME_BASIC_OFFSET_Y + n * ME_DATA_NEXT_ROW;
                SetWindowPos(hGame, nullptr,
                             ME_TREE_OFFSET_X - 1,
                             nDataOffsetY[3],
                             nButtonSizeX,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
                SetWindowPos(hPlatform, nullptr,
                             ME_TREE_OFFSET_X - 1 + nButtonSizeX,
                             nDataOffsetY[3],
                             rcClient.right - rcClient.left - 4 - nButtonSizeX - ME_TREE_OFFSET_X + 1 - nButtonSizeX,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
                SetWindowPos(hNeck, nullptr,
                             rcClient.right - rcClient.left - 4 - nButtonSizeX,
                             nDataOffsetY[3],
                             nButtonSizeX,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
            }
            else{
                SetWindowPos(hDisplayEdit, nullptr,
                              nCompactOffsetLeft,
                              ME_DATA_EDIT_SIZE_Y,
                              (rcClient.right - rcClient.left) - nCompactOffsetLeft - nCompactOffsetRight,
                              nEditSize,
                              0);
                SetWindowPos(hTree, nullptr,
                              nCompactOffsetLeft,
                              nCompactOffsetTop + nEditSize + ME_DATA_EDIT_SIZE_Y,
                              (rcClient.right - rcClient.left) - nCompactOffsetLeft - nCompactOffsetRight,
                              rcClient.bottom - (nCompactOffsetTop + nEditSize + ME_DATA_EDIT_SIZE_Y + nCompactOffsetBottom),
                              0);
                int nButtonSizeX = (rcClient.right - rcClient.left - nCompactOffsetRight - nCompactOffsetLeft) / 3;
                SetWindowPos(hGame, nullptr,
                             0, //nCompactOffsetLeft,
                             0, //nCompactOffsetTop,
                             nButtonSizeX,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
                SetWindowPos(hPlatform, nullptr,
                             nButtonSizeX, //nCompactOffsetLeft + nButtonSizeX + 1,
                             0, //nCompactOffsetTop,
                             rcClient.right - rcClient.left - 2*nButtonSizeX, //rcClient.right - rcClient.left - nCompactOffsetRight - nCompactOffsetLeft - 2*nButtonSizeX - 2,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
                SetWindowPos(hNeck, nullptr,
                             rcClient.right - rcClient.left - nButtonSizeX, //rcClient.right - rcClient.left - nCompactOffsetRight - nButtonSizeX,
                             0, //nCompactOffsetTop,
                             nButtonSizeX,
                             ME_DATA_EDIT_SIZE_Y,
                             0);
            }

            if(bShowHex) SetWindowPos(hTabs, nullptr, ME_HEX_WIN_OFFSET_X, 0, ME_HEX_WIN_OFFSET_X + ME_TABS_SIZE_X, rcClient.bottom - ME_STATUSBAR_Y, 0);
            if(bShowHex) Edit1.Resize();

            // Resize the statusbar;
            RECT rcStatus;
            GetClientRect(hStatusBar, &rcStatus);
            SendMessage(hStatusBar,message,wParam,lParam);
            InvalidateRect(hStatusBar, &rcStatus, false);
        }
        break;
        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC) wParam;
            HWND hControl = (HWND) lParam;

            if(hControl == Edits::hIntEdit ||
               hControl == Edits::hUIntEdit ||
               hControl == Edits::hFloatEdit ||
               hControl == hDisplayEdit ){
                SetBkColor(hdcStatic, RGB(255, 255, 255));
                return (INT_PTR) CreateSolidBrush(RGB(255, 255, 255));
            }
            else{
                SetBkColor(hdcStatic, RGB(250, 250, 250));
                return (INT_PTR) CreateSolidBrush(RGB(250, 250, 250));
            }
        }
        case WM_DESTROY:
        {
            if(!Model.empty()){
                TreeView_DeleteAllItems(hTree);
                Model.FlushAll();
            }
            if(hMessageHook != NULL) UnhookWindowsHookEx(hMessageHook);
            PostQuitMessage(0);       /* send a WM_QUIT to the message queue */
        }
        break;
        /* for messages that we don't deal with */
        default:
        {
            return DefWindowProc (hwnd, message, wParam, lParam);
        }
    }

    return 0;
}

LRESULT APIENTRY TreeSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    switch(message){
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        {
            if(message == WM_NCMOUSEMOVE) message = WM_MOUSEMOVE;
            if(message == WM_NCLBUTTONDOWN) message = WM_LBUTTONDOWN;
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            if(pt.y < 5){
                MapWindowPoints(hwnd, hFrame, &pt, 1);
                return CallWindowProc(Frame::FrameProc, hFrame, message, wParam, MAKELPARAM((WORD) pt.x, (WORD) pt.y));
            }
        }
        break;
    }

    return CallWindowProc(MainTreeProc, hwnd, message, wParam, lParam);
}

LRESULT APIENTRY DisplaySubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    //std::cout << "Display Subclass Callback Fired!\n";
    switch(message){
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        {
            if(message == WM_NCMOUSEMOVE) message = WM_MOUSEMOVE;
            if(message == WM_NCLBUTTONDOWN) message = WM_LBUTTONDOWN;
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            //std::cout << "Display coordinates: " << pt.x << ", " << pt.y << "\n";
            if(rcClient.bottom - rcClient.top - pt.y < 5){
                MapWindowPoints(hwnd, hFrame, &pt, 1);
                return CallWindowProc(Frame::FrameProc, hFrame, message, wParam, MAKELPARAM((WORD) pt.x, (WORD) pt.y));
            }
        }
        break;
    }

    return CallWindowProc(MainDisplayProc, hwnd, message, wParam, lParam);
}


void ScrollToData(MDL & /*Mdl*/, std::vector<std::string> cItem, LPARAM lParam, int nFile){
    auto item = [&](unsigned n) -> std::string { return n < cItem.size() ? cItem.at(n) : std::string(); };

    if(cItem.empty() || item(0) == "" || !lParam) return;

    auto scroll_to = [&](const std::string & sFile, long nOffset){
        int nTabIndex = TabCtrl_GetTabIndexByText(hTabs, sFile);
        if(nTabIndex == -1) return;

        TabCtrl_SetCurSel(hTabs, nTabIndex);
        Edit1.LoadData();

        if(nOffset < 0) nOffset = 0;
        long nTarget = (nOffset / 16) * ME_EDIT_NEXT_ROW;
        Edit1.yCurrentScroll = std::min<long>(Edit1.yMaxScroll, nTarget);
        if(Edit1.yCurrentScroll < 0) Edit1.yCurrentScroll = 0;
        if(bShowHex) Edit1.UpdateEdit();
    };

    if(item(1) == "Geometry"){
        scroll_to("MDL", static_cast<long>(reinterpret_cast<Node *>(lParam)->nOffset) + 12);
    }
    else if(item(3) == "Geometry" && (item(1) == "Children" || safesubstr(item(0), 0, 7) == "Parent:")){
        scroll_to("MDL", static_cast<long>(reinterpret_cast<Node *>(lParam)->nOffset) + 12);
    }
    else if(item(1) == "Animations"){
        scroll_to("MDL", static_cast<long>(reinterpret_cast<Animation *>(lParam)->nOffset) + 12);
    }
    else if(item(2) == "Animations"){
        scroll_to("MDL", static_cast<long>(reinterpret_cast<Node *>(lParam)->nOffset) + 12);
    }
    else if(item(4) == "Animations" && (item(1) == "Children" || safesubstr(item(0), 0, 7) == "Parent:")){
        scroll_to("MDL", static_cast<long>(reinterpret_cast<Node *>(lParam)->nOffset) + 12);
    }
    else if(item(0) == "Vertices" && nFile == 0){
        scroll_to("MDX", static_cast<long>(reinterpret_cast<Node *>(lParam)->Mesh.nOffsetIntoMdx));
    }
}


void ProcessTreeAction(HTREEITEM hItem, const int & nAction, void * Pointer){
    std::vector<std::string> cItem;
    cItem.reserve(40);
    LPARAM lParam;

    //Get our selected item
    TVITEM tvNewSelect = {};
    tvNewSelect.mask = TVIF_TEXT | TVIF_PARAM;
    tvNewSelect.cchTextMax = 255;
    char cGet[255] = {};
    tvNewSelect.hItem = hItem;
    tvNewSelect.pszText = cGet;
    if(hTree == NULL || hItem == NULL || !TreeView_GetItem(hTree, &tvNewSelect)) return;

    //Get pointer to data and name
    cItem.push_back(cGet);
    lParam = tvNewSelect.lParam;

    //Fill cItem with all the ancestors of our item
    std::string FilenameModel = to_ansi(Model.GetFilename());
    std::string FilenameWalkmesh = to_ansi(Model.Wok ? Model.Wok->GetFilename() : L"");
    std::string FilenamePwk = to_ansi(Model.Pwk ? Model.Pwk->GetFilename() : L"");
    std::string FilenameDwk0 = to_ansi(Model.Dwk0 ? Model.Dwk0->GetFilename() : L"");
    std::string FilenameDwk1 = to_ansi(Model.Dwk1 ? Model.Dwk1->GetFilename() : L"");
    std::string FilenameDwk2 = to_ansi(Model.Dwk2 ? Model.Dwk2->GetFilename() : L"");
    int nFile = -1;
    while((hItem != NULL) &&
          !(cItem.front() == FilenameModel) &&
          !(cItem.front() == FilenameWalkmesh) &&
          !(cItem.front() == FilenamePwk) &&
          !(cItem.front() == FilenameDwk0) &&
          !(cItem.front() == FilenameDwk1) &&
          !(cItem.front() == FilenameDwk2)){
        tvNewSelect.hItem = TreeView_GetParent(hTree, tvNewSelect.hItem);
        tvNewSelect.pszText = cGet;
        TreeView_GetItem(hTree, &tvNewSelect);
        cItem.push_back(cGet);

        if(cItem.back() == FilenameModel) nFile = 0;
        else if(cItem.back() == FilenameWalkmesh) nFile = 1;
        else if(cItem.back() == FilenamePwk) nFile = 2;
        else if(cItem.back() == FilenameDwk0) nFile = 3;
        else if(cItem.back() == FilenameDwk1) nFile = 4;
        else if(cItem.back() == FilenameDwk2) nFile = 5;
        else{
            continue;
        }
        break;
    }

    //Perform desired action
    if(nAction == ACTION_UPDATE_DISPLAY){
        std::stringstream sPrint;
        //Determine cPrint that is to be shown
        if(hItem != NULL){
            DetermineDisplayText(cItem, sPrint, lParam);

            if(bShowHex) Edit1.UpdateEdit();
        }

        //Update DisplayEdit
        SetWindowText(hDisplayEdit, sPrint.str().c_str());
    }
    else if(nAction == ACTION_ADD_MENU_LINES){
        AddMenuLines(Model, cItem, lParam, (MenuLineAdder*) Pointer, nFile);
    }
    else if(nAction == ACTION_OPEN_VIEWER){
        OpenViewer(Model, cItem, lParam);
    }
    else if(nAction == ACTION_OPEN_EDITOR){
        OpenEditorDlg(Model, cItem, lParam, nFile);
    }
    else if(nAction == ACTION_SCROLL){
        if(bShowHex) ScrollToData(Model, cItem, lParam, nFile);
    }
}

void ManageIni(IniConst Action){
    std::wstring sIni = JoinPath(ParentPath(sExePath), L"mdledit.ini");

    if(PathFileExistsW(sIni.c_str())){
        if(Action == INI_READ) std::cout << "Reading ";
        if(Action == INI_WRITE) std::cout << "Writing ";
        std::cout << "INI file: " << to_ansi(sIni) << ".\n";
        IniFile Ini;
        Ini.AddIniOption("AreaWeighting", DT_bool, &Model.bSmoothAreaWeighting);
        Ini.AddIniOption("AngleWeighting", DT_bool, &Model.bSmoothAngleWeighting);
        Ini.AddIniOption("DetermineSmoothing", DT_bool, &Model.bDetermineSmoothing);
        Ini.AddIniOption("WriteAnimations", DT_bool, &Model.bWriteAnimations);
        Ini.AddIniOption("SkinToTrimesh", DT_bool, &Model.bSkinToTrimesh);
        Ini.AddIniOption("LightsaberToTrimesh", DT_bool, &Model.bLightsaberToTrimesh);
        Ini.AddIniOption("BezierToLinear", DT_bool, &Model.bBezierToLinear);
        Ini.AddIniOption("ExportWOK", DT_bool, &Model.bExportWok);
        Ini.AddIniOption("WOKCoords", DT_bool, &Model.bUseWokData);
        Ini.AddIniOption("UseDotAscii", DT_bool, &bDotAsciiDefault);
        Ini.AddIniOption("KOTOR2", DT_bool, &Model.bK2);
        Ini.AddIniOption("XBOX", DT_bool, &Model.bXbox);
        Ini.AddIniOption("ShowGroup", DT_bool, &bShowGroup);
        Ini.AddIniOption("ShowDataStruct", DT_bool, &bShowDataStruct);
        Ini.AddIniOption("HexLocation", DT_bool, &bHexLocation);
        Ini.AddIniOption("SaveReport", DT_bool, &bSaveReport);
        Ini.AddIniOption("MinimizeVerts", DT_bool, &Model.bMinimizeVerts);
        Ini.AddIniOption("WeldGeometry", DT_bool, &Model.bMinimizeVerts2);
        Ini.AddIniOption("UseCreaseAngle", DT_bool, &Model.bCreaseAngle);
        Ini.AddIniOption("CreaseAngle", DT_uint, &Model.nCreaseAngle);
        Ini.AddIniOption("TreeHierarchy", DT_bool, &bModelHierarchy);
        if(Model.bDebug || Action == INI_READ) Ini.AddIniOption("Debug", DT_bool, &Model.bDebug);
        if(Model.bWriteSmoothing || Action == INI_READ) Ini.AddIniOption("WriteSmoothingArray", DT_bool, &Model.bWriteSmoothing);
        if(bAnalyze || Action == INI_READ) Ini.AddIniOption("Analyze", DT_bool, &bAnalyze);
        try{
            if(Action == INI_READ) Ini.ReadIni(sIni);
            else if(Action == INI_WRITE) Ini.WriteIni(sIni);
        }
        catch(const std::exception & e){
            std::cout << "A standard exception occurred while reading the INI file:\n" << e.what() << "\n";
            return;
        }
    }
    else std::cout << sIni.c_str() << " not found.\n";
}
