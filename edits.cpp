#include "frame.h"
#include <cstdint>
#include <cstring>
#include <cstdio>


char Edits::cClassName[] = "mdledithexcontrol";
LRESULT CALLBACK EditsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
HWND Edits::hIntEdit;
HWND Edits::hUIntEdit;
HWND Edits::hFloatEdit;

Edits::Edits(){
    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = GetModuleHandle(nullptr); // Instance of the application
    WindowClass.lpfnWndProc = EditsProc; // Pointer to callback procedure

    // #2 Class styles
    WindowClass.style = CS_DBLCLKS; // Class styles

    // #3 Background
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); //(HBRUSH) (COLOR_WINDOW); // Background brush

    // #4 Cursor
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor

    // #5 Icon
    WindowClass.hIcon = NULL; // Class Icon
    WindowClass.hIconSm = NULL; // Small icon for this class

    // #6 Menu
    WindowClass.lpszMenuName = NULL; // Menu Resource

    // #7 Other
    WindowClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    WindowClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure

    sBuffer = nullptr;
    nKnownArray = nullptr;
}

bool Edits::Run(HWND hParent, UINT nID){
    if(!RegisterClassEx(&WindowClass)) return false;

    RECT rcParent;
    GetClientRect(hParent, &rcParent);

    hMe = CreateWindowEx(WS_EX_TOPMOST, cClassName, "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                        ME_HEX_WIN_OFFSET_X, ME_TABS_OFFSET_Y_TOP, ME_HEX_WIN_SIZE_X, rcParent.bottom - ME_TABS_OFFSET_Y_BOTTOM - ME_TABS_OFFSET_Y_TOP,
                        hParent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(nID)), GetModuleHandle(nullptr), nullptr);
    if(!hMe) return false;
    ShowWindow(hMe, false);
    return true;
}

extern Edits Edit1;

namespace {
    std::string StatusBarPartText(int nPart){
        if(hStatusBar == NULL) return std::string();
        LRESULT nLengthInfo = SendMessage(hStatusBar, SB_GETTEXTLENGTH, static_cast<WPARAM>(nPart), 0);
        int nChars = LOWORD(nLengthInfo);
        if(nChars < 0) nChars = 0;
        std::string sText(static_cast<std::size_t>(nChars) + 1, '\0');
        SendMessage(hStatusBar, SB_GETTEXT, static_cast<WPARAM>(nPart), reinterpret_cast<LPARAM>(&sText[0]));
        sText.resize(std::strlen(sText.c_str()));
        return sText;
    }

    void SetStatusBarPartIfChanged(int nPart, const std::string & sCaption){
        if(hStatusBar == NULL) return;
        if(StatusBarPartText(nPart) != sCaption){
            SendMessage(hStatusBar, SB_SETTEXT, static_cast<WPARAM>(nPart), reinterpret_cast<LPARAM>(sCaption.c_str()));
        }
    }
}

LRESULT CALLBACK EditsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    Edits * Edit = nullptr;
    if(GetDlgCtrlID(hwnd) == IDC_MAIN_EDIT) Edit = &Edit1;
    if(Edit == nullptr){
        std::cout << "MAJORMAJORMAJOR ERROR!! Running the EditsProc() for an unexisting Edit :S \n";
        return DefWindowProc (hwnd, message, wParam, lParam);
    }
    //After here, Edit is definitely not NULL

	PAINTSTRUCT ps;
	HDC hdc;
    SCROLLINFO si;

    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);

    static bool bDrag;
    static bool bDblClick;
    static bool bClick;
    static int nArea;
    static int nClickArea;

    /* handle the messages */
    switch(message){
        case WM_CREATE:
        {
                bDrag = false;
                bDblClick = false;
                bClick = false;
                nArea = 0;
                nClickArea = 0;

                //Initialize points
                Edit->ptClick.x = -1;
                Edit->ptClick.y = -1;
                Edit->ptRelease.x = -1;
                Edit->ptRelease.y = -1;
                Edit->ptHover.x = -1;
                Edit->ptHover.y = -1;

                //The extreme values are then defined as everything that is initially visible
                Edit->UpdateClientRect();
                Edit->yMaxScroll = Edit->rcClient.bottom;
                Edit->yCurrentScroll = 0;

                Edit->hScrollVert = CreateWindowEx(WS_EX_TOPMOST, "SCROLLBAR", "",
                                                    WS_CHILD | WS_VISIBLE | SBS_VERT | SBS_RIGHTALIGN		,
                                                    ME_HEX_WIN_OFFSET_X + ME_HEX_WIN_SIZE_X - ME_SCROLLBAR_X, 0, GetSystemMetrics(SM_CXHTHUMB), Edit->rcClient.bottom,
                                                    hwnd, (HMENU) IDC_SBS_SBV, GetModuleHandle(nullptr), nullptr);
                ShowWindow(Edit->hScrollVert, false);
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
                si.nMax = Edit->yMaxScroll;
                si.nMin = 0;
                si.nPage = Edit->rcClient.bottom;
                si.nPos = 0;
                SetScrollInfo(Edit->hScrollVert, SB_CTL, &si, true);
        }
        break;
        case WM_COMMAND:
        {
            (void)wParam;
            (void)lParam;
            // Reserved for future editor commands.
        }
        break;
        case WM_ERASEBKGND:
        {
            return 1;
        }
        case WM_MOUSEMOVE:
        {
            if(Edit->sBuffer == nullptr) return DefWindowProc(hwnd, message, wParam, lParam);
            if(Edit->sBuffer->empty()) return DefWindowProc(hwnd, message, wParam, lParam);

            //Determine drag

            if(!bDrag && (bClick || bDblClick)) bDrag = true;

            //Determine area
            if(!bDrag){
                if(xPos < 64) nArea = -1;
                else if(xPos > 65 && xPos < 450) nArea = 1;
                else if(xPos > 454 && xPos < 587) nArea = 2;
                else nArea = 0;
            }

            if(nArea == 1){
                SetCursor(LoadCursor(NULL, IDC_IBEAM));
                SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) LoadCursor(NULL, IDC_IBEAM));
            }
            else if(nArea == 2){
                SetCursor(LoadCursor(NULL, IDC_IBEAM));
                SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) LoadCursor(NULL, IDC_IBEAM));
            }
            else{
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR) LoadCursor(NULL, IDC_ARROW));
            }

            bool bThreshold = false;
            if(nArea == 1){
                int nCurrentY = (yPos + Edit->yCurrentScroll - 1) / ME_EDIT_NEXT_ROW;
                nCurrentY = std::max((0 + Edit->yCurrentScroll - 1 + 10) / ME_EDIT_NEXT_ROW, nCurrentY);
                nCurrentY = std::min( (int) (Edit->rcClient.bottom + Edit->yCurrentScroll - 1) / ME_EDIT_NEXT_ROW - 1, nCurrentY);
                if(Edit->ptHover.y != nCurrentY){
                    Edit->ptHover.y = nCurrentY;
                    bThreshold = true;
                }
                int nCurrentX = (xPos + 2 - ME_EDIT_ROWNUM_OFFSET) / ME_EDIT_CHAR_SIZE_X;
                nCurrentX = std::max(nCurrentX, 0);
                nCurrentX = std::min(nCurrentX, 47);
                if(Edit->ptHover.x != nCurrentX){
                    Edit->ptHover.x = nCurrentX;
                    bThreshold = true;
                }
            }
            else if(nArea == 2){
                int nCurrentY = (yPos + Edit->yCurrentScroll - 1) / ME_EDIT_NEXT_ROW;
                nCurrentY = std::max((0 + Edit->yCurrentScroll - 1 + 10) / ME_EDIT_NEXT_ROW, nCurrentY);
                nCurrentY = std::min((int)(Edit->rcClient.bottom + Edit->yCurrentScroll - 1) / ME_EDIT_NEXT_ROW - 1, nCurrentY);
                if(Edit->ptHover.y != nCurrentY){
                    Edit->ptHover.y = nCurrentY;
                    bThreshold = true;
                }
                int nCurrentX = (xPos + 2 - ME_EDIT_CHARSET_OFFSET) / ME_EDIT_CHAR_SIZE_X * 3;
                nCurrentX = std::max(nCurrentX, 0);
                nCurrentX = std::min(nCurrentX, 47);
                if(Edit->ptHover.x != nCurrentX){
                    Edit->ptHover.x = nCurrentX;
                    bThreshold = true;
                }
            }

            int nLength = HIWORD(SendMessage(hStatusBar, SB_GETTEXTLENGTH, 3, 0));
            if(nLength > 0){
                std::string sGet;
                sGet.resize(nLength);
                SendMessage(hStatusBar, SB_GETTEXT, 3, (LPARAM) sGet.c_str());
                if(sGet.size() > 0 && sGet.at(0) != 0 && nArea == 0){
                    sGet = "";
                    SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(3, 0), NULL), (LPARAM) sGet.c_str());
                }
            }

            if(bThreshold){
                if(nArea > 0){
                    std::string sType = TabCtrl_GetCurSelName(hTabs);
                    if(sType == "MDL") Edit->UpdateStatusPositionModel();
                    else if(sType == "MDX") Edit->UpdateStatusPositionMdx();
                    else if(sType == "WOK" || sType == "PWK" || sType == "DWK 0" || sType == "DWK 1" || sType == "DWK 2") Edit->UpdateStatusPositionBwm(sType);
                }

                if(bDrag){
                    Edit->bSelection = true; /// If we are dragging, we are also selecting.
                    Edit->DetermineSelection();
                    Edit->UpdateStatusBar();
                    InvalidateRect(hwnd, &Edit->rcClient, false);
                    Edit->ptPrevious = Edit->ptHover;
                    Edit->PrintValues();
                }
            }
        }
        break;
        case WM_LBUTTONDBLCLK:
        {
            if(Edit->sBuffer == nullptr) return DefWindowProc(hwnd, message, wParam, lParam);
            if(Edit->sBuffer->empty()) return DefWindowProc(hwnd, message, wParam, lParam);

            if(nArea == 1){ //We are in the byte view region
                nClickArea = nArea;

                bDblClick = true;

                Edit->ptPrevious = Edit->ptHover;
                Edit->bSelection = true; /// true because we select by double clicking
                Edit->DetermineSelection();
                Edit->UpdateStatusBar();
                InvalidateRect(hwnd, &Edit->rcClient, false);
                Edit->PrintValues();

            }
            else if(nArea == -1){
                bHexLocation = !bHexLocation;
                InvalidateRect(hwnd, &Edit->rcClient, false);
                Edit->UpdateStatusBar();
            }
        }
        break;
        case WM_LBUTTONDOWN:
        {
            if(Edit->sBuffer == nullptr) return DefWindowProc(hwnd, message, wParam, lParam);
            if(Edit->sBuffer->empty()) return DefWindowProc(hwnd, message, wParam, lParam);

            if(nArea == 1 || nArea == 2){ //We are in the byte or chAR view region
                nClickArea = nArea;
                SetCapture(hwnd); //Capture the mouse even if it leaves the window

                /// User pressed shift. Do not take this as a new click and a cancellation of the previous selection,
                /// but as if by doing this, we are actually proceeding with a dragging operation.
                /// The user must have selected something beforehand.
                if(wParam & MK_SHIFT && Edit->bSelection){
                    bDrag = true; /// Make it as if we were dragging

                    /// I forgot what this does - it probably doesn't matter because if click was undefined, bSelection wouldn't be on anyway,
                    /// we would skip this if anyway.
                    if(Edit->ptClick.x == -1 || Edit->ptClick.y == -1)
                        Edit->ptClick = Edit->ptHover;
                }
                else{
                    if(Edit->bSelection){ /// If there was a previous selection (or we were dragging)
                        Edit->bSelection = false; /// by clicking we clear the selection
                        bDrag = false; /// we stop dragging
                    }

                    bClick = true;

                    Edit->ptPrevious = Edit->ptHover;
                    Edit->ptClick = Edit->ptHover;
                }

                Edit->DetermineSelection();
                InvalidateRect(hwnd, &Edit->rcClient, false);
                Edit->UpdateStatusBar();
                Edit->PrintValues();

            }
        }
        break;
        case WM_LBUTTONUP:
        {
            if(Edit->sBuffer == nullptr) return DefWindowProc(hwnd, message, wParam, lParam);
            if(Edit->sBuffer->empty()) return DefWindowProc(hwnd, message, wParam, lParam);

            ReleaseCapture(); //Release mouse

            if(bDrag){ /// If we are dragging
                Edit->ptRelease = Edit->ptHover; /// The current hover point is our release point
                if((Edit->ptRelease.x != Edit->ptClick.x || Edit->ptRelease.y != Edit->ptClick.y) && Edit->ptClick.x != -1 && Edit->ptClick.y != -1)
                    Edit->bSelection = true; /// If the drag selection is valid, selection is true
                else if(bDblClick) Edit->bSelection = true; /// If we just double-clicked, selection is true
                else Edit->bSelection = false; /// If we simply clicked and released, selection is false
            }

            bDrag = false;
            bDblClick = false;
            bClick = false;

            //Edit->DetermineSelection();
            InvalidateRect(hwnd, &Edit->rcClient, false);
            Edit->UpdateStatusBar();
            Edit->PrintValues();

        }
        break;
        case WM_PAINT:
        {

            HDC hdcReal = BeginPaint(hwnd, &ps);

            hdc = CreateCompatibleDC(hdcReal);

            HBITMAP memBM = CreateCompatibleBitmap(hdcReal, ME_HEX_WIN_SIZE_X, Edit->rcClient.bottom);
            HBITMAP hBMold = (HBITMAP) SelectObject(hdc, memBM);

            HBRUSH hFill = CreateSolidBrush(RGB(255, 255, 255));
            Edit->UpdateClientRect();
            RECT rcFile = Edit->rcClient;
            rcFile.left = 0;
            rcFile.top = 0;
            //rcFile.bottom = Edit->yMaxScroll;
            FillRect(hdc, &rcFile, hFill);
            DeleteObject(hFill);

            /**/
            if(Edit->sBuffer && !Edit->sBuffer->empty()){

                /// Draw the two separator lines
                HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
                SelectObject(hdc, hPenLine);
                MoveToEx(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_SEPARATOR_OFFSET, 0, NULL);
                LineTo(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_SEPARATOR_OFFSET, Edit->rcClient.bottom);
                MoveToEx(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_SEPARATOR_2_OFFSET, 0, NULL);
                LineTo(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_SEPARATOR_2_OFFSET, Edit->rcClient.bottom);
                DeleteObject(hPenLine);

                SetTextColor(hdc, RGB(50, 50, 50));
                //SetBkColor(hdc, GetSysColor(COLOR_MENU));
                SelectObject(hdc, hMonospace);


                /// Determine whether we are hiliting.
                bool bHilite = false;
                POINT ptEnd;
                if(bDrag) ptEnd = Edit->ptHover;
                else ptEnd = Edit->ptRelease;
                if(Edit->bSelection || bDrag){
                        bHilite = true;
                        //std::cout << string_format("bSelection: %i bDrag: %i\n", Edit->bSelection, bDrag);
                }
                if(ptEnd.y < Edit->ptClick.y || (ptEnd.y == Edit->ptClick.y && ptEnd.x < Edit->ptClick.x)){
                }
                else{
                }

                int n = Edit->yCurrentScroll / ME_EDIT_NEXT_ROW; /// n is the number of rows that are scrolled away, it will be ++ed later
                int nMaxRowsInScreen = (Edit->rcClient.bottom) / ME_EDIT_NEXT_ROW + 2; /// Max number of rows the screen may display
                int nMax = n + nMaxRowsInScreen; /// Max number of rows possible by the current scroll
                nMax = std::min(nMax, (int) Edit->sBuffer->size()/16 + 1); /// Cap the max number of rows by the actual number of rows of the data

                int nDataKnown;
                char cHexText[48];
                char cHexCompareText[48];
                std::string sType = TabCtrl_GetCurSelName(hTabs);

                for(; n < nMax; n++){
                    SetTextColor(hdc, RGB(50, 50, 50));
                    SetBkColor(hdc, RGB(255, 255, 255));
                    std::stringstream ssIntPrint;
                    ssIntPrint << std::uppercase << (bHexLocation ? std::hex : std::dec) << n * 16;
                    char cCounter[32];
                    std::snprintf(cCounter, sizeof(cCounter), "%s", ssIntPrint.str().c_str());
                    AddSignificantZeroes(cCounter, 8);
                    ExtTextOut(hdc, ME_EDIT_PADDING_LEFT, ME_EDIT_PADDING_TOP + n * ME_EDIT_NEXT_ROW - Edit->yCurrentScroll, 0, nullptr, cCounter, strlen(cCounter), nullptr);

                    int nBytes = std::max(0, std::min(16, static_cast<int>(Edit->sBuffer->size()) - n * 16));
                    int nCompareBytes = std::max(0, std::min(16, static_cast<int>(Edit->sCompareBuffer->size()) - n * 16));
                    if(nBytes > 0) CharsToHex(cHexText, *Edit->sBuffer, n * 16, nBytes);
                    else cHexText[0] = '\0';
                    if(nCompareBytes > 0) CharsToHex(cHexCompareText, *Edit->sCompareBuffer, n * 16, nCompareBytes);
                    else cHexCompareText[0] = '\0';
                    int nHexLength = strlen(cHexText);
                    for(int i = 0; i < nHexLength; i++){
                        if(static_cast<int>(Edit->nKnownArray->size()) > n*16 + i / 3) nDataKnown = Edit->nKnownArray->at(n*16 + i / 3) & 0xFFFF;
                        else nDataKnown = 0;
                        COLORREF rgbUnderline = DataColor(nDataKnown, false);// RGB(0,0,0);
                        COLORREF rgbText = DataColor(nDataKnown, false);// RGB(0,0,0);
                        COLORREF rgbBackground = RGB(255,255,255);
                        bool bHighlited = false, bDifferent = false, bOutOfRange = false;
                        if(bHilite && (
                           ((Edit->nSelectStart / 16) < n && (Edit->nSelectEnd / 16) > n)
                           || ((Edit->nSelectStart / 16) == n && (Edit->nSelectEnd / 16) == n && (Edit->nSelectStart % 16 * 3) <= i && (Edit->nSelectEnd % 16 * 3) >= i - 1)
                           || ((Edit->nSelectStart / 16) == n && (Edit->nSelectEnd / 16) != n && (Edit->nSelectStart % 16 * 3) <= i)
                           || ((Edit->nSelectEnd / 16) == n && (Edit->nSelectStart / 16) != n && (Edit->nSelectEnd % 16 * 3) >= i - 1))){
                            bHighlited = true;
                        }

                        if(bShowDiff && Edit->Compare(n*16 + i / 3) > 0){
                            if(i % 3 < 2 || // ignore the space
                               (Edit->Compare(n*16 + i / 3) == Edit->Compare(n*16 + i / 3 + 1) && Edit->Compare(n*16 + i / 3) == 2) || // color the space if out of range
                               (Edit->Compare(n*16 + i / 3) == Edit->Compare(n*16 + i / 3 + 1) && (Edit->nKnownArray->at(n*16 + i / 3) & 0xFFFF) == (Edit->nKnownArray->at(n*16 + i / 3 + 1) & 0xFFFF))) //color the space if same known value
                            {
                                if(Edit->Compare(n*16 + i / 3) == 1){ // Different
                                    bDifferent = true;
                                }
                                if(Edit->Compare(n*16 + i / 3) == 2){
                                    bOutOfRange = true;
                                }
                            }
                        }

                        bool bKeepTreeItemSelection = false;
                        if(bHighlited){
                            if(bDifferent){
                                if(nClickArea == 1){
                                    rgbText = RGB(255, 170, 70);
                                    rgbUnderline = RGB(255, 170, 70);
                                }
                                else{
                                    rgbText = RGB(255, 210, 170);
                                    rgbUnderline = RGB(255, 210, 170);
                                }
                            }
                            else{
                                rgbText = RGB(255, 255, 255);
                                rgbUnderline = RGB(255,255,255);
                            }
                            if(nClickArea == 1) rgbBackground = RGB(135, 135, 255);
                            else rgbBackground = RGB(185, 185, 255);
                        }
                        else if(bDifferent){
                            rgbText = RGB(255, 255, 255);
                            rgbUnderline = RGB(255,255,255);
                            rgbBackground = DataColor(nDataKnown, false);
                        }
                        else if(bOutOfRange){
                            rgbBackground = RGB(230, 180, 230); // Out of range
                        }
                        SetTextColor(hdc, rgbText);
                        SetBkColor(hdc, rgbBackground);
                        if(cHexText[i] != ' ' || (rgbBackground != RGB(255,255,255) && !bKeepTreeItemSelection)){
                            ExtTextOut(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_ROWNUM_OFFSET + i * ME_EDIT_CHAR_SIZE_X, ME_EDIT_PADDING_TOP + n * ME_EDIT_NEXT_ROW - Edit->yCurrentScroll, 0, nullptr,
                                       (bHighlited && bDifferent ? &cHexCompareText[i] : &cHexText[i]), 1, nullptr);
                        }

                        int nVertical = ME_EDIT_PADDING_TOP + n * ME_EDIT_NEXT_ROW - Edit->yCurrentScroll + 12;
                        int nHorizontal = ME_EDIT_PADDING_LEFT + ME_EDIT_ROWNUM_OFFSET + i * ME_EDIT_CHAR_SIZE_X;
                        int nExtra = -1;
                        if(nHexLength >= i+1 && !(Edit->nKnownArray->at(n*16 + i / 3) & 0x00010000)) nExtra = 0;

                        if(bShowGroup && (i % 3 == 0 && (n*16 + i / 3 - 1 < 0 || Edit->nKnownArray->at(n*16 + i / 3 - 1) & 0x00010000))){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, rgbUnderline);
                            SelectObject(hdc, hPenUnderline);
                            MoveToEx(hdc, nHorizontal - 1,  nVertical, NULL);
                            LineTo(hdc, nHorizontal - 1,  nVertical - 3);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }
                        if(bShowDataStruct && (i % 3 == 0 && (n*16 + i / 3 - 1 < 0 || Edit->nKnownArray->at(n*16 + i / 3 - 1) & 0x00020000))){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, RGB(0,0,0));
                            SelectObject(hdc, hPenUnderline);
                            MoveToEx(hdc, nHorizontal - 3,  nVertical - 3, NULL);
                            LineTo(hdc, nHorizontal - 3,  nVertical - 12);
                            LineTo(hdc, nHorizontal + 5,  nVertical - 12);
                            //MoveToEx(hdc, nHorizontal - 3,  nVertical - 5, NULL);
                            //LineTo(hdc, nHorizontal - 3,  nVertical + 2);
                            //LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra + 1,  nVertical + 2);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }

                        if(bShowGroup && (cHexText[i] != ' ' || !(Edit->nKnownArray->at(n*16 + i / 3) & 0x00010000))){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, rgbUnderline);
                            SelectObject(hdc, hPenUnderline);
                            MoveToEx(hdc, nHorizontal - 1,  nVertical, NULL);
                            LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra, nVertical);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }

                        if(bShowGroup && (i % 3 == 1 && Edit->nKnownArray->at(n*16 + i / 3) & 0x00010000)){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, rgbUnderline);
                            SelectObject(hdc, hPenUnderline);
                            MoveToEx(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra,  nVertical, NULL);
                            LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra,  nVertical - 3);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }
                        else if(bShowGroup && (i % 3 == 2 && (cHexText[i] != ' ' || rgbBackground != RGB(255,255,255)) && Edit->nKnownArray->at(n*16 + i / 3) & 0x00010000)){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, rgbUnderline);
                            SelectObject(hdc, hPenUnderline);
                            MoveToEx(hdc, nHorizontal + nExtra,  nVertical, NULL);
                            LineTo(hdc, nHorizontal + nExtra,  nVertical - 3);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }
                        /*
                        if(i % 3 == 1 && Edit->nKnownArray->at(n*16 + i / 3) & 0x00020000){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, RGB(0,0,0));
                            SelectObject(hdc, hPenUnderline);
                            //MoveToEx(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra + 1,  nVertical - 8, NULL);
                            //LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra + 1,  nVertical - 13);
                            //LineTo(hdc, nHorizontal - 2,  nVertical - 13);
                            MoveToEx(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra + 4,  nVertical - 12, NULL);
                            LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra + 4,  nVertical);
                            //LineTo(hdc, nHorizontal + ME_EDIT_CHAR_SIZE_X + nExtra,  nVertical);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }
                        else if(i % 3 == 2 && Edit->nKnownArray->at(n*16 + i / 3) & 0x00020000){
                            HPEN hPenUnderline = CreatePen(PS_SOLID, 1, RGB(0,0,0));
                            SelectObject(hdc, hPenUnderline);
                            //MoveToEx(hdc, nHorizontal + nExtra + 1,  nVertical - 8, NULL);
                            //LineTo(hdc, nHorizontal + nExtra + 1,  nVertical - 13);
                            //LineTo(hdc, nHorizontal - ME_EDIT_CHAR_SIZE_X - 2,  nVertical - 13);
                            MoveToEx(hdc, nHorizontal + nExtra + 4,  nVertical - 12, NULL);
                            LineTo(hdc, nHorizontal + nExtra + 4,  nVertical);
                            //LineTo(hdc, nHorizontal + nExtra,  nVertical);
                            DeleteObject(hPenUnderline);
                            SelectObject(hdc, hMonospace);
                        }
                        */


                        /// Do a completely separate draw for the charset.
                        if(i % 3 == 0){
                            SetTextColor(hdc, RGB(220, 220, 220));
                            SetBkColor(hdc, RGB(255, 255, 255));

                            if(bHighlited){
                                if(bDifferent){
                                    if(nClickArea == 2){
                                        SetTextColor(hdc, RGB(255, 170, 70));
                                    }
                                    else{
                                        SetTextColor(hdc, RGB(255, 210, 170));
                                    }
                                }
                                else{
                                    SetTextColor(hdc, RGB(255, 255, 255));
                                }
                                if(nClickArea == 2) SetBkColor(hdc, RGB(135, 135, 255));
                                else SetBkColor(hdc, RGB(185, 185, 255));
                            }
                            else if(nDataKnown == 3){
                                SetTextColor(hdc, DataColor(nDataKnown, false));
                            }
                            if(static_cast<int>(Edit->sBuffer->size()) > n*16 + (i/3)){
                                char cToWrite = (bHighlited && bDifferent ? Edit->sCompareBuffer->at(n*16 + (i/3)) : Edit->sBuffer->at(n * 16 + (i/3)));
                                PrepareCharForDisplay(&cToWrite);
                                ExtTextOut(hdc, ME_EDIT_PADDING_LEFT + ME_EDIT_CHARSET_OFFSET + (i/3) * ME_EDIT_CHAR_SIZE_X, ME_EDIT_PADDING_TOP + n * ME_EDIT_NEXT_ROW - Edit->yCurrentScroll, 0, nullptr, &cToWrite, 1, nullptr);
                            }
                        }
                    }
                }
            }
            /**/

            BitBlt(hdcReal, 0, 0, ME_HEX_WIN_SIZE_X, Edit->rcClient.bottom, hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, hBMold);
            DeleteObject(memBM);
            DeleteDC(hdc);

            EndPaint(hwnd, &ps);
        }
        break;
        case WM_VSCROLL:
        {
            int yNewPos;    /* new position */

            si.cbSize = sizeof(si);
            si.fMask  = SIF_DISABLENOSCROLL | SIF_POS | SIF_TRACKPOS | SIF_PAGE;
            GetScrollInfo(Edit->hScrollVert, SB_CTL, &si);

            switch(LOWORD(wParam)){
                case SB_TOP:
                    yNewPos = 0;
                break;
                case SB_BOTTOM:
                    yNewPos = Edit->yMaxScroll - (signed int) si.nPage; // - nPage ?
                break;
                /* User clicked the shaft above the scroll box. */
                case SB_PAGEUP:
                    yNewPos = Edit->yCurrentScroll - (signed int) si.nPage;
                    //yNewPos = std::min(Edit->yMaxScroll - (signed int) si.nPage, yNewPos);
                    //yNewPos = std::max(0, yNewPos);
                break;
                /* User clicked the shaft below the scroll box. */
                case SB_PAGEDOWN:
                    yNewPos = Edit->yCurrentScroll + (signed int) si.nPage;
                    //yNewPos = std::min(Edit->yMaxScroll - (signed int) si.nPage, yNewPos);
                    //yNewPos = std::max(0, yNewPos);
                break;
                /* User clicked the top arrow. */
                case SB_LINEUP:
                    yNewPos = Edit->yCurrentScroll - ME_EDIT_NEXT_ROW;
                    //yNewPos = std::max(0, yNewPos);
                break;
                /* User clicked the bottom arrow. */
                case SB_LINEDOWN:
                    yNewPos = Edit->yCurrentScroll + ME_EDIT_NEXT_ROW;
                    ////yNewPos = std::min(Edit->yMaxScroll - (signed int) si.nPage, yNewPos);
                break;
                /* User dragged the scroll box. */
                case SB_THUMBPOSITION:
                    yNewPos = (signed int) si.nPos;
                    //std::cout << string_format("WM_HSCROLL: Thumb: %i \n", yNewPos);
                    //yNewPos = std::min(Edit->yMaxScroll - (signed int) si.nPage, yNewPos);
                    //std::cout << string_format("WM_HSCROLL: Thumb corrected: %i \n", yNewPos);
                break;
                /* User is dragging the scroll box. */
                case SB_THUMBTRACK:
                    yNewPos = (signed int) si.nTrackPos;
                    //std::cout << string_format("WM_HSCROLL: Thumbtrack: %i \n", yNewPos);
                    //std::cout << string_format("WM_HSCROLL: Thumbtrack corrected: %i (compared to %i - %i)\n", yNewPos, Edit->yMaxScroll, (signed int) si.nPage);
                break;
                default:
                    yNewPos = Edit->yCurrentScroll;
            }
            //yNewPos = min(Edit->yMaxScroll, yNewPos);
            //yNewPos = max(0, yNewPos);

            /* If the current position does not change, do not scroll.*/
            if (yNewPos == Edit->yCurrentScroll) break;

            //CUSTOM ADDITION, make it so that we can only scroll for discrete amounts
            /* Reset the current scroll position. */
            yNewPos = (yNewPos / ME_EDIT_NEXT_ROW) * ME_EDIT_NEXT_ROW;
            yNewPos = std::max(0, yNewPos);
            yNewPos = std::min(Edit->yMaxScroll - (signed int) si.nPage, yNewPos);
            Edit->yCurrentScroll = yNewPos;


            /* Reset the current scroll position. */
            //Edit->yCurrentScroll = yNewPos;

           /*
            * DONT SCROLL THE WINDOW, ITS THE SOURCE OF ALL EVIL
            * JUST REPAINT NORMALLY
            */
            InvalidateRect(hwnd, &Edit->rcClient, false);
            UpdateWindow(hwnd);

            /* Reset the scroll bar. */
            si.cbSize = sizeof(si);
            si.fMask  = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE;
            si.nPos   = Edit->yCurrentScroll;
            si.nMax   = Edit->yMaxScroll;
            si.nMin   = 0;
            SetScrollInfo(Edit->hScrollVert, SB_CTL, &si, true);

            //std::cout << string_format("Zoom DEBUG: Zooming. \nnMinP=%i, nMaxP=%i, xCurrentScroll=%i, \nxCurrentScroll+rcClient.right/2=%i\n0=%i, Edit->yMaxScroll=%i, Edit->yCurrentScroll=%i, \nyCurrentScroll+(rcClient.bottom-nNonScreenY)/2=%i\n", nMinP, nMaxP, xCurrentScroll, xCurrentScroll+rcClient.right/2, 0, Edit->yMaxScroll, Edit->yCurrentScroll, Edit->yCurrentScroll+(rcClient.bottom-nNonScreenY)/2);

            return 0;
        }
        break;
        case WM_MOUSEWHEEL:
        {
            si.cbSize = sizeof(si);
            si.fMask  = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE;
            GetScrollInfo(Edit->hScrollVert, SB_CTL, &si);

            signed short nHiword = HIWORD(wParam);
            //std::cout << "WM_MOUSEWHEEL: Hiword=" << nHiword << ", Page=" << si.nPage << ", scrollDelta=" << (signed) si.nPage / 3 * (nHiword/120) << ", CurrentScroll=" << Edit->yCurrentScroll << ", NewScroll=" << Edit->yCurrentScroll - (signed) si.nPage / 3 * (nHiword/120) << "\n";
            Edit->yCurrentScroll -= (signed) (si.nPage / 6 * (nHiword/120));
            Edit->yCurrentScroll = std::max(si.nMin, std::min(Edit->yCurrentScroll, (signed) (si.nMax - si.nPage)));
            Edit->yCurrentScroll = (Edit->yCurrentScroll / ME_EDIT_NEXT_ROW) * ME_EDIT_NEXT_ROW;
            InvalidateRect(hwnd, &Edit->rcClient, false);
            UpdateWindow(hwnd);

            si.fMask  = SIF_DISABLENOSCROLL | SIF_POS;
            si.nPos   = Edit->yCurrentScroll;
            SetScrollInfo(Edit->hScrollVert, SB_CTL, &si, true);
        }
        break;
        /* for messages that we don't deal with */
        case WM_DESTROY:
            //Close
        default:
        {
            return DefWindowProc (hwnd, message, wParam, lParam);
        }
    }

    return 0;
}

void Edits::PrintValues(bool bCheck){
    if(nSelectStart < 0 ||
       nSelectEnd < 0 ||
       !bCheck){
        SetWindowText(hIntEdit, "");
        SetWindowText(hUIntEdit, "");
        SetWindowText(hFloatEdit, "");
        return;
    }

    int nSize = nSelectEnd - nSelectStart + 1;
    std::string sString;
    char cInt [255];
    char cUInt [255];
    char cFloat [255];
    for(int n = 0; nSelectStart + n <= nSelectEnd; n++){
        sString += (bShowDiff && Compare(nSelectStart+n) == 1 ? sCompareBuffer->at(nSelectStart+n) : sBuffer->at(nSelectStart+n));
    }

    if(nSize == 1){
        int8_t nSigned = 0;
        uint8_t nUnsigned = 0;
        std::memcpy(&nSigned, sString.data(), sizeof(nSigned));
        std::memcpy(&nUnsigned, sString.data(), sizeof(nUnsigned));
        std::snprintf(cInt, sizeof(cInt), "%hhi", static_cast<int>(nSigned));
        std::snprintf(cUInt, sizeof(cUInt), "%hhu", static_cast<unsigned int>(nUnsigned));
        cFloat[0] = '\0';
    }
    else if(nSize == 2){
        int16_t nSigned = 0;
        uint16_t nUnsigned = 0;
        std::memcpy(&nSigned, sString.data(), sizeof(nSigned));
        std::memcpy(&nUnsigned, sString.data(), sizeof(nUnsigned));
        std::snprintf(cInt, sizeof(cInt), "%hi", static_cast<int>(nSigned));
        std::snprintf(cUInt, sizeof(cUInt), "%hu", static_cast<unsigned int>(nUnsigned));
        cFloat[0] = '\0';

    }
    else if(nSize == 4){
        int32_t nSigned = 0;
        uint32_t nUnsigned = 0;
        float fValue = 0.0f;
        std::memcpy(&nSigned, sString.data(), sizeof(nSigned));
        std::memcpy(&nUnsigned, sString.data(), sizeof(nUnsigned));
        std::memcpy(&fValue, sString.data(), sizeof(fValue));
        std::snprintf(cInt, sizeof(cInt), "%i", nSigned);
        std::snprintf(cUInt, sizeof(cUInt), "%u", static_cast<unsigned int>(nUnsigned));
        std::snprintf(cFloat, sizeof(cFloat), "%.16f", fValue);
    }
    else if(nSize == 8){
        int32_t nSigned = 0;
        uint32_t nUnsigned = 0;
        double fValue = 0.0;
        std::memcpy(&nSigned, sString.data(), sizeof(nSigned));
        std::memcpy(&nUnsigned, sString.data(), sizeof(nUnsigned));
        std::memcpy(&fValue, sString.data(), sizeof(fValue));
        std::snprintf(cInt, sizeof(cInt), "%li", static_cast<long>(nSigned));
        std::snprintf(cUInt, sizeof(cUInt), "%lu", static_cast<unsigned long>(nUnsigned));
        std::snprintf(cFloat, sizeof(cFloat), "%.16g", fValue);
    }
    else{
        cInt[0] = '\0';
        cUInt[0] = '\0';
        cFloat[0] = '\0';
    }
    TruncateDec(cFloat);
    SetWindowText(hIntEdit, cInt);
    SetWindowText(hUIntEdit, cUInt);
    SetWindowText(hFloatEdit, cFloat);
}

int Edits::Compare(unsigned nPos){
    if(sCompareBuffer == nullptr || sBuffer == nullptr || sCompareBuffer->size() == 0 || sBuffer->size() == 0 || nPos >= sBuffer->size()) return -1; // Don't mark
    if(nPos >= sCompareBuffer->size()) return 2; // Out of range
    if(sBuffer->at(nPos) != sCompareBuffer->at(nPos)) return 1; // Different
    return 0; // Not different
}

void Edits::Cleanup(){
    nKnownArray = nullptr;
    sBuffer = nullptr;
    ShowWindow(hScrollVert, false);
    UpdateEdit();
}

void Edits::LoadData(){
    sSelected = TabCtrl_GetCurSelName(hTabs);
    if(sSelected == "MDL" && !Model.empty()){
        nKnownArray = &Model.GetKnownData();
        sCompareBuffer = &Model.GetCompareData();
        sBuffer = &Model.GetBuffer();
    }
    else if(sSelected == "MDX" && Model.Mdx){
        nKnownArray = &Model.Mdx->GetKnownData();
        sCompareBuffer = &Model.Mdx->GetCompareData();
        sBuffer = &Model.Mdx->GetBuffer();
    }
    else if(sSelected == "WOK" && Model.Wok){
        nKnownArray = &Model.Wok->GetKnownData();
        sCompareBuffer = &Model.Wok->GetCompareData();
        sBuffer = &Model.Wok->GetBuffer();
    }
    else if(sSelected == "PWK" && Model.Pwk){
        nKnownArray = &Model.Pwk->GetKnownData();
        sCompareBuffer = &Model.Pwk->GetCompareData();
        sBuffer = &Model.Pwk->GetBuffer();
    }
    else if(sSelected == "DWK 0" && Model.Dwk0){
        nKnownArray = &Model.Dwk0->GetKnownData();
        sCompareBuffer = &Model.Dwk0->GetCompareData();
        sBuffer = &Model.Dwk0->GetBuffer();
    }
    else if(sSelected == "DWK 1" && Model.Dwk1){
        nKnownArray = &Model.Dwk1->GetKnownData();
        sCompareBuffer = &Model.Dwk1->GetCompareData();
        sBuffer = &Model.Dwk1->GetBuffer();
    }
    else if(sSelected == "DWK 2" && Model.Dwk2){
        nKnownArray = &Model.Dwk2->GetKnownData();
        sCompareBuffer = &Model.Dwk2->GetCompareData();
        sBuffer = &Model.Dwk2->GetBuffer();
    }
    else{
        nKnownArray = nullptr;
        sCompareBuffer = nullptr;
        sBuffer = nullptr;
    }

    if(!bShowHex) return;
    if(sBuffer != nullptr && nKnownArray != nullptr){
        if(sBuffer->empty()) ShowWindow(hScrollVert, false);
        else ShowWindow(hScrollVert, true);

        SetClassLongPtr(hMe, GCLP_HCURSOR, (LONG_PTR) LoadCursor(NULL, IDC_ARROW));
        ptHover.x = -1;
        ptHover.y = -1;
        ptPrevious.x = -1;
        ptPrevious.y = -1;
        ptClick.x = -1;
        ptClick.y = -1;
        ptRelease.x = -1;
        ptRelease.y = -1;
        nSelectStart = -1;
        nSelectEnd = -1;
        bSelection = false;
        yMaxScroll = ((sBuffer->size() - 1)/16 + 2) * ME_EDIT_NEXT_ROW;
        yCurrentScroll = 0;
    }
    else{
        SetClassLongPtr(hMe, GCLP_HCURSOR, (LONG_PTR) LoadCursor(NULL, IDC_ARROW));
        ShowWindow(hScrollVert, false);
    }
    UpdateEdit();
    UpdateStatusBar();
    SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(3, 0), NULL), (LPARAM) "");
}

HWND Edits::GetWindowHandle(){
    return hMe;
}

void Edits::UpdateClientRect(){
    RECT rcParent;
    GetClientRect(GetParent(hMe), &rcParent);
    rcClient.top = 0;
    rcClient.left = ME_HEX_WIN_OFFSET_X;
    rcClient.bottom = rcParent.bottom - ME_STATUSBAR_Y - ME_TABS_OFFSET_Y_BOTTOM - ME_TABS_OFFSET_Y_TOP;
    rcClient.right = ME_HEX_WIN_SIZE_X;
}

void Edits::ShowHideEdit(){
    ShowWindow(hMe, bShowHex);
    if(bShowHex){
        LoadData();
    }
}

void Edits::UpdateEdit(){
    if(!bShowHex) return;
    UpdateClientRect();
    InvalidateRect(hMe, &rcClient, false);
    if(sBuffer == nullptr) return;
    if(!sBuffer->empty()){
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
        si.nMax = yMaxScroll;
        si.nMin = 0;
        si.nPos = yCurrentScroll;
        si.nPage = rcClient.bottom;
        SetScrollInfo(hScrollVert, SB_CTL, &si, true);
    }
}

void Edits::Resize(){
    UpdateClientRect();
    SetWindowPos(hMe, nullptr, rcClient.left, ME_TABS_OFFSET_Y_TOP, ME_HEX_WIN_SIZE_X, rcClient.bottom, 0);
    SetWindowPos(hScrollVert, nullptr, ME_HEX_WIN_OFFSET_X + ME_HEX_WIN_SIZE_X - ME_SCROLLBAR_X, 0, GetSystemMetrics(SM_CXHTHUMB), rcClient.bottom, 0);
    UpdateEdit();
}

void Edits::DetermineSelection(){
    POINT ptLow;
    POINT ptHigh;
    if(ptClick.y > ptHover.y || (ptClick.y == ptHover.y && ptClick.x > ptHover.x)){
        ptHigh = ptClick;
        ptLow = ptHover;
    }
    else{
        ptLow = ptClick;
        ptHigh = ptHover;
    }

    nSelectStart = ptLow.y * 16 + (ptLow.x + 1) / 3;
    if(ptHigh.x <= 0) nSelectEnd = ptHigh.y * 16 - 1;
    else nSelectEnd = ptHigh.y * 16 + (ptHigh.x - 1) / 3;
    // Reversed selections are invalid; out-of-range selections are clamped
    // to the file end below.
    if(nSelectEnd < nSelectStart){
        nSelectStart = -1;
        nSelectEnd = -1;
    }
    nSelectEnd = std::min(nSelectEnd, (int) sBuffer->size() - 1);
    nSelectStart = std::min(nSelectStart, (int) sBuffer->size() - 1);
}

void Edits::UpdateStatusBar(bool bCheck){
    char cString1 [255];
    char cString2 [255];
    char cString3 [255];
    if(nSelectEnd == -1 || nSelectStart == -1 || !bCheck || sBuffer == nullptr){
        SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(0, 0), NULL), (LPARAM) "");
        SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(1, 0), NULL), (LPARAM) "");
        SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(2, 0), NULL), (LPARAM) "");
        SetWindowText(hIntEdit, "");
        SetWindowText(hUIntEdit, "");
        SetWindowText(hFloatEdit, "");
    }
    else{
        if(bHexLocation){
            std::snprintf(cString1, sizeof(cString1), "Offset: %X", nSelectStart);
            std::snprintf(cString2, sizeof(cString2), "Block: %X-%X", nSelectStart, nSelectEnd);
            std::snprintf(cString3, sizeof(cString3), "Length: %X", nSelectEnd - nSelectStart + 1);
        }
        else{
            std::snprintf(cString1, sizeof(cString1), "Offset: %i", nSelectStart);
            std::snprintf(cString2, sizeof(cString2), "Block: %i-%i", nSelectStart, nSelectEnd);
            std::snprintf(cString3, sizeof(cString3), "Length: %i", nSelectEnd - nSelectStart + 1);
        }
        SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(0, 0), NULL), (LPARAM) cString1);
        if(nSelectStart == nSelectEnd) SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(1, 0), NULL), (LPARAM) "");
        else SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(1, 0), NULL), (LPARAM) cString2);
        SendMessage(hStatusBar, SB_SETTEXT, MAKEWPARAM(MAKEWORD(2, 0), NULL), (LPARAM) cString3);
    }
}

COLORREF DataColor(int nDataKnown, bool bHilite){
    if(bHilite) return RGB(255, 255, 255);
    switch(nDataKnown){
        case 10: //known unknowns, mark red
            return RGB(255, 0, 0);
        case 1: //anything that counts something, unsigned by definition. (uint32 or uint16)
            return RGB(235, 220, 0);
        case 2: //float
            return RGB(76, 206, 20);
        case 3: //string
            return RGB(110, 110, 110);
        case 4: //int ((u)int32)
            return RGB(0, 200, 200);
        case 5: //short ((u)int16)
            return RGB(200, 0, 200);
        case 6: //offset/pointer (uint32)
            return RGB(100, 100, 200);
        case 7: //byte
            return RGB(165, 103, 16);
        case 8: //Possibly constant/meaningless/padding, but not random data
            return RGB(232, 232, 232);
        case 9: //function pointer - it can make the beginning certain structures stand out more
            return RGB(255, 160, 80);
        case 11: //Padding/unused portions filled with random data
            return RGB(162, 177, 90);
    }
    return RGB(0, 0, 0);
}


namespace {
    inline bool ByteInRange(unsigned nOffset, unsigned nStart, unsigned long long nSize){
        unsigned long long nBegin = nStart;
        unsigned long long nEnd = nBegin + nSize;
        return nSize > 0 && nOffset >= nBegin && nOffset < nEnd;
    }

    inline bool SetRangeLabel(unsigned nOffset, unsigned nStart, unsigned long long nSize, const std::string & sLabel, std::string & sCaption){
        if(!ByteInRange(nOffset, nStart, nSize)) return false;
        sCaption = sLabel;
        return true;
    }

    inline bool SetIndexedRangeLabel(unsigned nOffset, unsigned nStart, unsigned long long nStride, unsigned long long nCount, const std::string & sBase, std::string & sCaption){
        if(nStride == 0 || !ByteInRange(nOffset, nStart, nStride * nCount)) return false;
        sCaption = sBase + std::to_string((nOffset - nStart) / nStride);
        return true;
    }


    inline unsigned AbsMdlOffset(unsigned nRelativeOffset){
        return MDL_OFFSET + nRelativeOffset;
    }

    inline std::string SafeNodeName(MDL & mdl, Node & node){
        try{
            return mdl.GetNodeName(node);
        }
        catch(...){
            return "Unknown";
        }
    }

    inline std::string NodeStatusPrefix(MDL & mdl, FileHeader & Data, Node & node){
        std::string sNodeName = SafeNodeName(mdl, node);
        if(node.nAnimation.Valid()){
            try{
                unsigned nAnim = node.nAnimation;
                if(nAnim < Data.MH.Animations.size()){
                    return "Animations > " + Data.MH.Animations.at(nAnim).sName + " > " + sNodeName;
                }
            }
            catch(...){
            }
        }
        return "Geometry > " + sNodeName;
    }

    inline bool MapAabbStatus(unsigned nOffset, Aabb & aabb, const std::string & sPrefix, int & nAabbIndex, std::string & sCaption){
        if(SetRangeLabel(nOffset, AbsMdlOffset(aabb.nOffset), 44, sPrefix + " > Data > Aabb > Aabb Tree > Aabb Struct " + std::to_string(nAabbIndex), sCaption)) return true;
        ++nAabbIndex;
        if(!aabb.Child1.empty() && MapAabbStatus(nOffset, aabb.Child1.front(), sPrefix, nAabbIndex, sCaption)) return true;
        if(!aabb.Child2.empty() && MapAabbStatus(nOffset, aabb.Child2.front(), sPrefix, nAabbIndex, sCaption)) return true;
        return false;
    }

    inline bool MapAnimationHeaderStatus(unsigned nOffset, Animation & anim, unsigned /*nAnimationIndex*/, std::string & sCaption){
        const unsigned nAnimStart = AbsMdlOffset(anim.nOffset);
        const std::string sAnimation = "Animations > " + anim.sName;
        const std::string sGeometryHeader = sAnimation + " > Geometry Header";

        if(SetRangeLabel(nOffset, nAnimStart + 0, 8, sGeometryHeader + " > Function Pointers", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 8, 32, sGeometryHeader + " > Name", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 40, 4, sGeometryHeader + " > Offset to Root Node", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 44, 4, sGeometryHeader + " > Number of Nodes", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 48, 24, sGeometryHeader + " > Runtime Arrays", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 72, 4, sGeometryHeader + " > Reference Count", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 76, 1, sGeometryHeader + " > Type", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 77, 3, sGeometryHeader + " > Padding", sCaption)) return true;
        if(SetRangeLabel(nOffset, nAnimStart + 80, 56, sAnimation + " > Header", sCaption)) return true;
        if(SetRangeLabel(nOffset, AbsMdlOffset(anim.EventArray.nOffset), 36ULL * anim.EventArray.nCount, sAnimation + " > Header", sCaption)) return true;

        const unsigned long long nAnimationCoreEnd = 136ULL;
        unsigned long long nEventEnd = 0;
        if(anim.EventArray.nOffset != 0){
            const unsigned long long nEventStart = AbsMdlOffset(anim.EventArray.nOffset);
            if(nEventStart >= nAnimStart) nEventEnd = nEventStart + 36ULL * anim.EventArray.nCount - nAnimStart;
        }
        const unsigned long long nUnknownRange = std::max(nAnimationCoreEnd, nEventEnd);
        if(ByteInRange(nOffset, nAnimStart, nUnknownRange)){
            sCaption = sAnimation + " > Unknown";
            return true;
        }
        return false;
    }

    inline bool MapNodeStatus(unsigned nOffset, MDL & mdl, FileHeader & Data, Node & node, std::string & sCaption){
        if(node.nOffset == 0) return false;
        const unsigned nNodeStart = AbsMdlOffset(node.nOffset);
        const std::string sNode = NodeStatusPrefix(mdl, Data, node);
        const bool bAnimNode = node.nAnimation.Valid();

        if(SetRangeLabel(nOffset, nNodeStart + 0, 2, sNode + " > Node Type", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 2, 2, sNode + " > Supernode Number", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 4, 2, sNode + " > Node Number", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 6, 2, sNode + (bAnimNode ? " > Header" : " > Header > Basic"), sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 8, 4, sNode + " > Offset to Root", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 12, 4, sNode + " > Offset to Parent", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 16, 12, sNode + " > Position", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 28, 16, sNode + " > Orientation", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 44, 12, sNode + " > Child Array", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 56, 12, sNode + " > Controller Array", sCaption)) return true;
        if(SetRangeLabel(nOffset, nNodeStart + 68, 12, sNode + " > Controller Data Array", sCaption)) return true;

        if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Head.ChildrenArray.nOffset), 4, node.Head.ChildrenArray.nCount, sNode + (bAnimNode ? " > Child Pointers > Pointer " : " > Data > Child Array > Pointer "), sCaption)) return true;
        if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Head.ControllerArray.nOffset), 16, node.Head.ControllerArray.nCount, sNode + (bAnimNode ? " > Controllers > Controller " : " > Data > Controllers > Controller "), sCaption)) return true;
        if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Head.ControllerDataArray.nOffset), 4, node.Head.ControllerDataArray.nCount, sNode + (bAnimNode ? " > Controller Data > Float " : " > Data > Controller Data > Float "), sCaption)) return true;

        if(!node.Head.nType.Valid()) return false;

        if(node.Head.nType & NODE_LIGHT){
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_LIGHT);
            const std::string sLight = sNode + " > Header > Light";
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_LIGHT, sLight, sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Light.FlareTextureNameArray.nOffset), 4ULL * node.Light.FlareTextureNameArray.nCount, sLight, sCaption)) return true;
            for(Name & name : node.Light.FlareTextureNames){
                if(SetRangeLabel(nOffset, AbsMdlOffset(name.nOffset), name.sName.size() + 1, sLight, sCaption)) return true;
            }
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Light.FlareSizeArray.nOffset), 4ULL * node.Light.FlareSizeArray.nCount, sLight, sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Light.FlarePositionArray.nOffset), 4ULL * node.Light.FlarePositionArray.nCount, sLight, sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Light.FlareColorShiftArray.nOffset), 12ULL * node.Light.FlareColorShiftArray.nCount, sLight, sCaption)) return true;
        }

        if(node.Head.nType & NODE_EMITTER){
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_EMITTER);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_EMITTER, sNode + " > Header > Emitter", sCaption)) return true;
        }

        if(node.Head.nType & NODE_REFERENCE){
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_REFERENCE);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_REFERENCE, sNode + " > Header > Reference", sCaption)) return true;
        }

        if(node.Head.nType & NODE_MESH){
            const std::string sMesh = sNode + " > Header > Mesh";
            const std::string sMeshData = sNode;
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_MESH);
            unsigned nMeshSize = NODE_SIZE_MESH;
            if(mdl.bXbox && nMeshSize >= 4) nMeshSize -= 4;
            if(!mdl.bK2 && nMeshSize >= 8) nMeshSize -= 8;
            if(SetRangeLabel(nOffset, nStart, nMeshSize, sMesh, sCaption)){
                if(ByteInRange(nOffset, nStart + 304, 2)) sCaption = sMeshData + " > Data > Mesh > Pointer to Vert Number";
                return true;
            }
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Mesh.IndexCounterArray.nOffset), 4, sMeshData + " > Data > Mesh > Inverted Counter", sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Mesh.IndexLocationArray.nOffset), 4, sMeshData + " > Data > Mesh > Pointer to Vert Indices", sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Mesh.MeshInvertedCounterArray.nOffset), 4, sMeshData + " > Data > Mesh > Inverted Counter", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Mesh.FaceArray.nOffset), 32, node.Mesh.FaceArray.nCount, sMeshData + " > Data > Mesh > Faces > Face ", sCaption)) return true;
            if(node.Mesh.IndexLocationArray.nCount > 0 && SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Mesh.nVertIndicesLocation), 6, node.Mesh.FaceArray.nCount, sMeshData + " > Data > Mesh > Vert Indices > Face ", sCaption)) return true;
            if(!mdl.bXbox && SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Mesh.nOffsetToVertArray), 12, node.Mesh.nNumberOfVerts, sMeshData + " > Data > Mesh > Vert Coordinates > Vert ", sCaption)) return true;
        }

        if(node.Head.nType & NODE_DANGLY){
            const std::string sDangly = sNode + " > Header > Danglymesh";
            const std::string sDanglyData = sNode;
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_DANGLY);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_DANGLY, sDangly, sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Dangly.ConstraintArray.nOffset), 4, node.Dangly.ConstraintArray.nCount, sDanglyData + " > Data > Danglymesh > Constraints > Constraint ", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Dangly.nOffsetToData2), 12, node.Dangly.ConstraintArray.nCount, sDanglyData + " > Data > Danglymesh > Data2 > Vertex ", sCaption)) return true;
        }

        if(node.Head.nType & NODE_SKIN){
            const std::string sSkin = sNode + " > Header > Skin";
            const std::string sSkinData = sNode;
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_SKIN);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_SKIN, sSkin, sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Skin.nOffsetToBonemap), mdl.bXbox ? 2 : 4, node.Skin.nNumberOfBonemap, sSkinData + " > Data > Skin > Bonemap > ", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Skin.QBoneArray.nOffset), 16, node.Skin.QBoneArray.nCount, sSkinData + " > Data > Skin > Q-Bones > ", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Skin.TBoneArray.nOffset), 12, node.Skin.TBoneArray.nCount, sSkinData + " > Data > Skin > T-Bones > ", sCaption)) return true;
            if(SetRangeLabel(nOffset, AbsMdlOffset(node.Skin.Array8Array.nOffset), 4ULL * node.Skin.Array8Array.nCount, sSkinData + " > Data > Skin > Array8 (unused)", sCaption)) return true;
        }

        if(node.Head.nType & NODE_AABB){
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_AABB);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_AABB, sNode + " > Header > Aabb", sCaption)) return true;
            int nAabbIndex = 0;
            if(MapAabbStatus(nOffset, node.Walkmesh.RootAabb, sNode, nAabbIndex, sCaption)) return true;
        }

        if(node.Head.nType & NODE_SABER){
            const std::string sSaberHeader = sNode + " > Header > Lightsaber";
            const std::string sSaberData = sNode;
            unsigned nStart = nNodeStart + mdl.GetHeaderOffset(node, NODE_SABER);
            if(SetRangeLabel(nOffset, nStart, NODE_SIZE_SABER, sSaberHeader, sCaption)) return true;
            const unsigned nCount = static_cast<unsigned>(node.Saber.SaberData.size());
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Saber.nOffsetToSaberVerts), 12, nCount, sSaberData + " > Data > Lightsaber > Vertices > Vertex ", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Saber.nOffsetToSaberUVs), 8, nCount, sSaberData + " > Data > Lightsaber > UVs > UV ", sCaption)) return true;
            if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(node.Saber.nOffsetToSaberNormals), 12, nCount, sSaberData + " > Data > Lightsaber > Normals > Normal ", sCaption)) return true;
        }

        unsigned long long nOwnedDataEnd = nNodeStart + NODE_SIZE_HEADER;
        auto ExtendDataEnd = [&nOwnedDataEnd](unsigned nStart, unsigned long long nSize){
            if(nStart == 0 || nSize == 0) return;
            nOwnedDataEnd = std::max(nOwnedDataEnd, static_cast<unsigned long long>(AbsMdlOffset(nStart)) + nSize);
        };
        ExtendDataEnd(node.Head.ChildrenArray.nOffset, 4ULL * node.Head.ChildrenArray.nCount);
        ExtendDataEnd(node.Head.ControllerArray.nOffset, 16ULL * node.Head.ControllerArray.nCount);
        ExtendDataEnd(node.Head.ControllerDataArray.nOffset, 4ULL * node.Head.ControllerDataArray.nCount);
        if(node.Head.nType.Valid() && (node.Head.nType & NODE_MESH)){
            ExtendDataEnd(node.Mesh.IndexCounterArray.nOffset, 4ULL * node.Mesh.IndexCounterArray.nCount);
            ExtendDataEnd(node.Mesh.IndexLocationArray.nOffset, 4ULL * node.Mesh.IndexLocationArray.nCount);
            ExtendDataEnd(node.Mesh.MeshInvertedCounterArray.nOffset, 4ULL * node.Mesh.MeshInvertedCounterArray.nCount);
            ExtendDataEnd(node.Mesh.FaceArray.nOffset, 32ULL * node.Mesh.FaceArray.nCount);
            ExtendDataEnd(node.Mesh.nVertIndicesLocation, 6ULL * node.Mesh.FaceArray.nCount);
            ExtendDataEnd(node.Mesh.nOffsetToVertArray, 12ULL * node.Mesh.nNumberOfVerts);
        }
        if(node.Head.nType.Valid() && (node.Head.nType & NODE_DANGLY)){
            ExtendDataEnd(node.Dangly.ConstraintArray.nOffset, 4ULL * node.Dangly.ConstraintArray.nCount);
            ExtendDataEnd(node.Dangly.nOffsetToData2, 12ULL * node.Dangly.ConstraintArray.nCount);
        }
        if(node.Head.nType.Valid() && (node.Head.nType & NODE_SKIN)){
            ExtendDataEnd(node.Skin.nOffsetToBonemap, (mdl.bXbox ? 2ULL : 4ULL) * node.Skin.nNumberOfBonemap);
            ExtendDataEnd(node.Skin.QBoneArray.nOffset, 16ULL * node.Skin.QBoneArray.nCount);
            ExtendDataEnd(node.Skin.TBoneArray.nOffset, 12ULL * node.Skin.TBoneArray.nCount);
            ExtendDataEnd(node.Skin.Array8Array.nOffset, 4ULL * node.Skin.Array8Array.nCount);
        }
        if(node.Head.nType.Valid() && (node.Head.nType & NODE_SABER)){
            const unsigned nCount = static_cast<unsigned>(node.Saber.SaberData.size());
            ExtendDataEnd(node.Saber.nOffsetToSaberVerts, 12ULL * nCount);
            ExtendDataEnd(node.Saber.nOffsetToSaberUVs, 8ULL * nCount);
            ExtendDataEnd(node.Saber.nOffsetToSaberNormals, 12ULL * nCount);
        }
        if(nOffset >= nNodeStart + NODE_SIZE_HEADER && nOffset < nOwnedDataEnd){
            sCaption = sNode + " > Data > Unknown";
            return true;
        }

        return false;
    }

    inline std::string MdlStatusLabelAt(MDL & mdl, FileHeader & Data, unsigned nOffset){
        std::string sCaption = "Geometry > Unknown";
        const std::string sFileHeader = "File Header";
        const std::string sGeometryHeaderLabel = "Geometry Header";
        const std::string sModelHeaderLabel = "Model Header";

        if(SetRangeLabel(nOffset, 0, 4, sFileHeader + " > Padding", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, 4, 4, sFileHeader + " > MDL File Size", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, 8, 4, sFileHeader + " > MDX File Size", sCaption)) return sCaption;

        const unsigned nGeometryHeader = MDL_OFFSET;
        if(SetRangeLabel(nOffset, nGeometryHeader + 0, 8, sGeometryHeaderLabel + " > Function Pointers", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 8, 32, sGeometryHeaderLabel + " > Name", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 40, 4, sGeometryHeaderLabel + " > Offset to Root Node", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 44, 4, sGeometryHeaderLabel + " > Number of Nodes", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 48, 24, sGeometryHeaderLabel + " > Runtime Arrays", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 72, 4, sGeometryHeaderLabel + " > Reference Count", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 76, 1, sGeometryHeaderLabel + " > Type", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nGeometryHeader + 77, 3, sGeometryHeaderLabel + " > Padding", sCaption)) return sCaption;

        const unsigned nModelHeader = nGeometryHeader + 80;
        if(SetRangeLabel(nOffset, nModelHeader + 0, 1, sModelHeaderLabel + " > Classification", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 1, 2, sModelHeaderLabel + " > Unknown1", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 3, 1, sModelHeaderLabel + " > Affected By Fog", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 4, 4, sModelHeaderLabel + " > Number of Child Models", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 8, 4, sModelHeaderLabel + " > Offset to Animation Array", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 12, 8, sModelHeaderLabel + " > Number of Animations", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 20, 4, sModelHeaderLabel + " > Supermodel Reference", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 24, 12, sModelHeaderLabel + " > Bounding Box Min", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 36, 12, sModelHeaderLabel + " > Bounding Box Max", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 48, 4, sModelHeaderLabel + " > Radius", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 52, 4, sModelHeaderLabel + " > Animation Scale", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 56, 32, sModelHeaderLabel + " > Supermodel Name", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 88, 4, sModelHeaderLabel + " > Offset to Head Root", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 92, 4, sModelHeaderLabel + " > Padding", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 96, 4, sModelHeaderLabel + " > MDX File Size", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 100, 4, sModelHeaderLabel + " > MDX Data Offset", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 104, 4, sModelHeaderLabel + " > Offset to Name Array", sCaption)) return sCaption;
        if(SetRangeLabel(nOffset, nModelHeader + 108, 8, sModelHeaderLabel + " > Number of Names", sCaption)) return sCaption;

        if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(Data.MH.NameArray.nOffset), 4, Data.MH.NameArray.nCount, "Name Array > Pointers > Pointer ", sCaption)) return sCaption;
        for(Name & name : Data.MH.Names){
            if(SetRangeLabel(nOffset, AbsMdlOffset(name.nOffset), name.sName.size() + 1, "Name Array > Strings > \"" + name.sName + "\"", sCaption)) return sCaption;
        }

        if(SetIndexedRangeLabel(nOffset, AbsMdlOffset(Data.MH.AnimationArray.nOffset), 4, Data.MH.AnimationArray.nCount, "Animations > Pointers > Pointer", sCaption)) return sCaption;
        for(unsigned n = 0; n < Data.MH.Animations.size(); ++n){
            Animation & anim = Data.MH.Animations.at(n);
            if(MapAnimationHeaderStatus(nOffset, anim, n, sCaption)) return sCaption;
        }

        for(Node & node : Data.MH.ArrayOfNodes){
            if(MapNodeStatus(nOffset, mdl, Data, node, sCaption)) return sCaption;
        }
        for(Animation & anim : Data.MH.Animations){
            for(Node & node : anim.ArrayOfNodes){
                if(MapNodeStatus(nOffset, mdl, Data, node, sCaption)) return sCaption;
            }
        }

        return sCaption;
    }
}

void Edits::UpdateStatusPositionModel(){
    std::unique_ptr<FileHeader> & FileData = Model.GetFileData();
    if(!FileData) return;

    const int nPos = ptHover.y * 16 + (ptHover.x) / 3;
    std::string sCaption;
    if(nPos >= 0 && static_cast<unsigned>(nPos) < Model.GetBuffer().size()){
        sCaption = MdlStatusLabelAt(Model, *FileData, static_cast<unsigned>(nPos));
    }

    SetStatusBarPartIfChanged(3, sCaption);
}

void Edits::UpdateStatusPositionMdx(){
    if(!Model.Mdx) return;
    std::unique_ptr<FileHeader> & FileData = Model.GetFileData();
    if(!FileData) return;

    const int nPos = ptHover.y * 16 + (ptHover.x) / 3;
    std::string sCaption;
    if(nPos >= 0 && static_cast<unsigned>(nPos) < Model.Mdx->GetBuffer().size()){
        sCaption = "MDX > Unknown";
        const unsigned nOffset = static_cast<unsigned>(nPos);
        for(Node & node : FileData->MH.ArrayOfNodes){
            if(!node.Head.nType.Valid() || !(node.Head.nType & NODE_MESH)) continue;
            if(node.Mesh.nMdxDataSize == 0) continue;

            const unsigned long long nStart = node.Mesh.nOffsetIntoMdx;
            const unsigned long long nStride = node.Mesh.nMdxDataSize;
            const unsigned long long nVerts = node.Mesh.nNumberOfVerts;
            const unsigned long long nEnd = nStart + nStride * (nVerts + 1);
            if(nOffset < nStart || nOffset >= nEnd) continue;

            const unsigned long long nEntry = (nOffset - nStart) / nStride;
            std::string sNodeName = "Unknown";
            try{
                sNodeName = Model.GetNodeName(node);
            }
            catch(...){
            }
            sCaption = "MDX > " + sNodeName;
            if(nEntry == nVerts) sCaption += " > Extra Data";
            else sCaption += " > Vertex " + std::to_string(nEntry);
            break;
        }
    }

    SetStatusBarPartIfChanged(3, sCaption);
}

void Edits::UpdateStatusPositionBwm(const std::string & sType){
    if(!Model.GetFileData()) return;
    BWM * ptr_bwm = nullptr;
    if(sType == "WOK" && Model.Wok) ptr_bwm = Model.Wok.get();
    else if(sType == "PWK" && Model.Pwk) ptr_bwm = Model.Pwk.get();
    else if(sType == "DWK 0" && Model.Dwk0) ptr_bwm = Model.Dwk0.get();
    else if(sType == "DWK 1" && Model.Dwk1) ptr_bwm = Model.Dwk1.get();
    else if(sType == "DWK 2" && Model.Dwk2) ptr_bwm = Model.Dwk2.get();
    if(ptr_bwm == nullptr) return;

    const int nPos = ptHover.y * 16 + (ptHover.x) / 3;
    std::string sCaption;
    if(nPos >= 0 && static_cast<unsigned>(nPos) < ptr_bwm->GetBuffer().size()){
        const unsigned nOffset = static_cast<unsigned>(nPos);
        sCaption = sType + " > Unknown";
        std::unique_ptr<BWMHeader> & DataPtr = ptr_bwm->GetData();
        if(DataPtr){
            BWMHeader & Data = *DataPtr;
            auto Prefix = [&sType](const std::string & sLabel){ return sType + sLabel; };
            auto InRange = [nOffset](unsigned nStart, unsigned long long nSize){
                const unsigned long long nBegin = nStart;
                const unsigned long long nEnd = nBegin + nSize;
                return nSize > 0 && nOffset >= nBegin && nOffset < nEnd;
            };
            auto SetIndexed = [&](unsigned nStart, unsigned long long nStride, unsigned long long nCount, const std::string & sBase){
                if(!InRange(nStart, nStride * nCount)) return false;
                sCaption = Prefix(sBase + std::to_string((nOffset - nStart) / nStride));
                return true;
            };

            if(InRange(0, 8)) sCaption = Prefix(" > Header > Version");
            else if(InRange(8, 4)) sCaption = Prefix(" > Header > Walkmesh Type");
            else if(InRange(12, 48)) sCaption = Prefix(" > Header > Use Hooks");
            else if(InRange(60, 12)) sCaption = Prefix(" > Header > Position");
            else if(InRange(72, 4)) sCaption = Prefix(" > Header > Number of Vertices");
            else if(InRange(76, 4)) sCaption = Prefix(" > Header > Offset to Vertex Coordinates");
            else if(InRange(80, 4)) sCaption = Prefix(" > Header > Number of Faces");
            else if(InRange(84, 4)) sCaption = Prefix(" > Header > Offset to Vertex Indices");
            else if(InRange(88, 4)) sCaption = Prefix(" > Header > Offset to Material IDs");
            else if(InRange(92, 4)) sCaption = Prefix(" > Header > Offset to Face Normals");
            else if(InRange(96, 4)) sCaption = Prefix(" > Header > Offset to Plane Distances");
            else if(InRange(100, 4)) sCaption = Prefix(" > Header > Number of Aabb Structs");
            else if(InRange(104, 4)) sCaption = Prefix(" > Header > Offset to Aabb Structs");
            else if(InRange(108, 4)) sCaption = Prefix(" > Header > Unknown");
            else if(InRange(112, 4)) sCaption = Prefix(" > Header > Number of Adjacent Edges");
            else if(InRange(116, 4)) sCaption = Prefix(" > Header > Offset to Adjacent Edges");
            else if(InRange(120, 4)) sCaption = Prefix(" > Header > Number of Outer Edges");
            else if(InRange(124, 4)) sCaption = Prefix(" > Header > Offset to Outer Edges");
            else if(InRange(128, 4)) sCaption = Prefix(" > Header > Number of Perimeters");
            else if(InRange(132, 4)) sCaption = Prefix(" > Header > Offset to Perimeters");
            else if(SetIndexed(Data.nOffsetToVerts, 12, Data.nNumberOfVerts, " > Vertices > Vertex ")){}
            else if(InRange(Data.nOffsetToIndices, 12ULL * Data.nNumberOfFaces)){
                const unsigned nLocal = nOffset - Data.nOffsetToIndices;
                sCaption = Prefix(" > Vertex Indices > Face " + std::to_string(nLocal / 12) + " > Index " + std::to_string((nLocal % 12) / 4));
            }
            else if(SetIndexed(Data.nOffsetToMaterials, 4, Data.nNumberOfFaces, " > Materials > Material ")){}
            else if(SetIndexed(Data.nOffsetToNormals, 12, Data.nNumberOfFaces, " > Face Normals > Normal ")){}
            else if(SetIndexed(Data.nOffsetToDistances, 4, Data.nNumberOfFaces, " > Plane Distances > Distance ")){}
            else if(SetIndexed(Data.nOffsetToAabb, 44, Data.nNumberOfAabb, " > Aabb Tree > Aabb Struct ")){}
            else if(InRange(Data.nOffsetToAdjacentFaces, 12ULL * Data.nNumberOfAdjacentFaces)){
                const unsigned nLocal = nOffset - Data.nOffsetToAdjacentFaces;
                sCaption = Prefix(" > Adjacent Faces > Face " + std::to_string(nLocal / 12) + " > Edge " + std::to_string((nLocal % 12) / 4));
            }
            else if(SetIndexed(Data.nOffsetToEdges, 8, Data.nNumberOfEdges, " > Edges > Edge ")){}
            else if(SetIndexed(Data.nOffsetToPerimeters, 4, Data.nNumberOfPerimeters, " > Perimeters > Perimeter ")){}
        }
    }

    SetStatusBarPartIfChanged(3, sCaption);
}
