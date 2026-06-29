#include "frame.h"
#include <windowsx.h>
#include <algorithm>

namespace {
    std::string GetListViewTextString(HWND hControl, int nItem, int nSubItem, int nMaxChars = 256){
        if(hControl == NULL || nItem < 0 || nSubItem < 0 || nMaxChars <= 1) return std::string();

        std::string sText(static_cast<std::size_t>(nMaxChars), '\0');
        LVITEMA item = {};
        item.iSubItem = nSubItem;
        item.pszText = &sText[0];
        item.cchTextMax = nMaxChars;

        const LRESULT nChars = SendMessageA(hControl, LVM_GETITEMTEXTA,
                                            static_cast<WPARAM>(nItem),
                                            reinterpret_cast<LPARAM>(&item));
        if(nChars < 0) return std::string();

        const std::size_t nLength = std::min<std::size_t>(static_cast<std::size_t>(nChars),
                                                          sText.size() - 1u);
        sText.resize(nLength);
        return sText;
    }

    std::string GetWindowTextString(HWND hWnd, int nMaxChars = 256){
        if(hWnd == NULL || nMaxChars <= 0) return std::string();
        std::string sText(static_cast<std::size_t>(nMaxChars), '\0');
        const int nChars = GetWindowText(hWnd, &sText[0], nMaxChars);
        if(nChars < 0) return std::string();
        sText.resize(static_cast<std::size_t>(nChars));
        return sText;
    }
}
INT_PTR CALLBACK TexturesProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    static bool bChange = false;
    static bool bReady = false;
    MDL * Mdl = nullptr;
    if(GetWindowLongPtr(hwnd, GWLP_USERDATA) != 0) Mdl = (MDL*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch(message){
        case WM_INITDIALOG:
        {
            bChange = false;
            bReady = false;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) lParam);
            Mdl = (MDL*) lParam;

            //Create ListViews
            HWND hList1 = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_EDITLABELS | LVS_NOCOLUMNHEADER | LVS_REPORT | LVS_SINGLESEL,
                                         5, 25 + 5, 230, 180,
                                         hwnd, (HMENU) IDC_TEXTURE_LISTVIEW1, nullptr, nullptr);
            HWND hList2 = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_EDITLABELS | LVS_NOCOLUMNHEADER | LVS_REPORT | LVS_SINGLESEL,
                                         240, 25 + 5, 230, 180,
                                         hwnd, (HMENU) IDC_TEXTURE_LISTVIEW2, nullptr, nullptr);
            HWND hList3 = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_EDITLABELS | LVS_NOCOLUMNHEADER | LVS_REPORT | LVS_SINGLESEL,
                                         5, 25 + 190, 230, 60,
                                         hwnd, (HMENU) IDC_TEXTURE_LISTVIEW3, nullptr, nullptr);
            HWND hList4 = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_EDITLABELS | LVS_NOCOLUMNHEADER | LVS_REPORT | LVS_SINGLESEL,
                                         240, 25 + 190, 230, 60,
                                         hwnd, (HMENU) IDC_TEXTURE_LISTVIEW4, nullptr, nullptr);
            ListView_SetExtendedListViewStyle(hList1, LVS_EX_DOUBLEBUFFER /*| LVS_EX_AUTOSIZECOLUMNS */ | LVS_EX_CHECKBOXES);
            ListView_SetExtendedListViewStyle(hList2, LVS_EX_DOUBLEBUFFER /*| LVS_EX_AUTOSIZECOLUMNS */ | LVS_EX_CHECKBOXES);
            ListView_SetExtendedListViewStyle(hList3, LVS_EX_DOUBLEBUFFER /*| LVS_EX_AUTOSIZECOLUMNS */ | LVS_EX_CHECKBOXES);
            ListView_SetExtendedListViewStyle(hList4, LVS_EX_DOUBLEBUFFER /*| LVS_EX_AUTOSIZECOLUMNS */ | LVS_EX_CHECKBOXES);
            LVCOLUMN col;
            col.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_WIDTH;
            /*col.fmt = LVCFMT_FIXED_WIDTH; */
            col.iSubItem = 0;
            col.cx = 120;
            ListView_InsertColumn(hList1, 0, &col);
            ListView_InsertColumn(hList2, 0, &col);
            ListView_InsertColumn(hList3, 0, &col);
            ListView_InsertColumn(hList4, 0, &col);

            //Fill ListViews
            if(Mdl != nullptr){
                //std::cout << "Mdl is valid pointer\n";
                if(Mdl->GetFileData()){
                    //std::cout << "Mdl has data\n";
                    FileHeader & Data = *Mdl->GetFileData();
                    int nCount1 = 0, nCount2 = 0, nCount3 = 0, nCount4 = 0;
                    LVITEM item = {};
                    item.mask = LVIF_TEXT | LVIF_STATE;
                    item.stateMask = 0;
                    item.iSubItem  = 0;
                    item.state     = 0;
                    LVFINDINFO find = {};
                    find.flags = LVFI_STRING;
                    std::string sSearch;
                    for(std::size_t n = 0; n < Data.MH.ArrayOfNodes.size(); ++n){
                        //std::cout << "Checking node\n";
                        Node & node = Data.MH.ArrayOfNodes.at(n);
                        if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_AABB) && !(node.Head.nType & NODE_SABER)){
                            if(std::string(node.Mesh.cTexture1.c_str()) != "" && std::string(node.Mesh.cTexture1.c_str()) != "NULL"){
                                sSearch = node.Mesh.cTexture1;
                                find.psz = const_cast<char*>(sSearch.c_str());
                                if(ListView_FindItem(hList1, -1, &find) == -1){
                                    item.iItem = nCount1;
                                    item.pszText = const_cast<char*>(sSearch.c_str());
                                    const int nInserted = ListView_InsertItem(hList1, &item);
                                    if(nInserted != -1){
                                        if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1) ListView_SetCheckState(hList1, nInserted, true);
                                        nCount1++;
                                    }
                                }
                            }
                            if(node.Mesh.cTexture2.c_str() != std::string("") && node.Mesh.cTexture2.c_str() != std::string("NULL")){
                                sSearch = node.Mesh.cTexture2;
                                find.psz = const_cast<char*>(sSearch.c_str());
                                if(ListView_FindItem(hList2, -1, &find) == -1){
                                    item.iItem = nCount2;
                                    item.pszText = const_cast<char*>(sSearch.c_str());
                                    const int nInserted = ListView_InsertItem(hList2, &item);
                                    if(nInserted != -1){
                                        if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2) ListView_SetCheckState(hList2, nInserted, true);
                                        nCount2++;
                                    }
                                }
                            }
                            if(node.Mesh.cTexture3.c_str() != std::string("") && node.Mesh.cTexture3.c_str() != std::string("NULL")){
                                sSearch = node.Mesh.cTexture3;
                                find.psz = const_cast<char*>(sSearch.c_str());
                                if(ListView_FindItem(hList3, -1, &find) == -1){
                                    item.iItem = nCount3;
                                    item.pszText = const_cast<char*>(sSearch.c_str());
                                    const int nInserted = ListView_InsertItem(hList3, &item);
                                    if(nInserted != -1){
                                        if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3) ListView_SetCheckState(hList3, nInserted, true);
                                        nCount3++;
                                    }
                                }
                            }
                            if(node.Mesh.cTexture4.c_str() != std::string("") && node.Mesh.cTexture4.c_str() != std::string("NULL")){
                                sSearch = node.Mesh.cTexture4;
                                find.psz = const_cast<char*>(sSearch.c_str());
                                if(ListView_FindItem(hList4, -1, &find) == -1){
                                    item.iItem = nCount4;
                                    item.pszText = const_cast<char*>(sSearch.c_str());
                                    const int nInserted = ListView_InsertItem(hList4, &item);
                                    if(nInserted != -1){
                                        if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4) ListView_SetCheckState(hList4, nInserted, true);
                                        nCount4++;
                                    }
                                }
                            }
                        }
                    }
                    bReady = true;
                }
            }
        }
        break;
        case WM_NOTIFY:
        {
            NMHDR* hdr = (NMHDR*) lParam;
            const UINT nNotification = static_cast<UINT>(hdr->code);
            int nID = hdr->idFrom;
            HWND hControl = hdr->hwndFrom;
            switch(nID){
                case IDC_TEXTURE_LISTVIEW1:
                case IDC_TEXTURE_LISTVIEW2:
                case IDC_TEXTURE_LISTVIEW3:
                case IDC_TEXTURE_LISTVIEW4:
                {
                    if(nNotification == LVN_ITEMCHANGED){
                        if(Mdl != nullptr && bReady){
                            if(Mdl->GetFileData()){
                                NMLISTVIEW * ia = (NMLISTVIEW*) lParam;
                                bool bChecked = ListView_GetCheckState(hControl, ia->iItem);
                                //std::cout << "Model is good, bChecked=" << bChecked << "\n";
                                FileHeader & Data = *Mdl->GetFileData();
                                std::string sOldTex = GetListViewTextString(hControl, ia->iItem, 0);
                                for(std::size_t n = 0; n < Data.MH.ArrayOfNodes.size(); ++n){
                                    Node & node = Data.MH.ArrayOfNodes.at(n);
                                    if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_AABB)){
                                        if(nID == IDC_TEXTURE_LISTVIEW1 && StringEqual(sOldTex.c_str(), node.Mesh.cTexture1.c_str(), true) && (!bChecked != !(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1))){
                                            //std::cout << "Found difference. (" << Data.MH.Names.at(node.Head.nNameIndex).sName << ")\n";
                                            bChange = true;
                                            node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap ^ MDX_FLAG_TANGENT1;
                                        }
                                        else if(nID == IDC_TEXTURE_LISTVIEW2 && StringEqual(sOldTex.c_str(), node.Mesh.cTexture2.c_str(), true) && (!bChecked != !(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2))){
                                            bChange = true;
                                            node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap ^ MDX_FLAG_TANGENT2;
                                        }
                                        else if(nID == IDC_TEXTURE_LISTVIEW3 && StringEqual(sOldTex.c_str(), node.Mesh.cTexture3.c_str(), true) && (!bChecked != !(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3))){
                                            bChange = true;
                                            node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap ^ MDX_FLAG_TANGENT3;
                                        }
                                        else if(nID == IDC_TEXTURE_LISTVIEW4 && StringEqual(sOldTex.c_str(), node.Mesh.cTexture4.c_str(), true) && (!bChecked != !(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4))){
                                            bChange = true;
                                            node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap ^ MDX_FLAG_TANGENT4;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if(nNotification == LVN_BEGINLABELEDIT){
                        HWND hEdit = ListView_GetEditControl(hControl);
                        if(nID == IDC_TEXTURE_LISTVIEW3 || nID == IDC_TEXTURE_LISTVIEW4) SendMessage(hEdit, EM_LIMITTEXT, (WPARAM) 12, 0);
                        else SendMessage(hEdit, EM_LIMITTEXT, (WPARAM) 32, 0);
                        return true; /// Allow the user to edit the label. Returning false would prevent it.
                    }
                    else if(nNotification == LVN_ENDLABELEDIT){
                        NMLVDISPINFO * info = (NMLVDISPINFO *) lParam;
                        if(info->item.pszText != nullptr && Mdl !=nullptr){
                            std::string sNewTex = info->item.pszText;
                            std::string sOldTex = GetListViewTextString(hControl, info->item.iItem, 0);

                            LVFINDINFO find = {};
                            find.flags = LVFI_STRING;
                            find.psz = const_cast<char*>(sNewTex.c_str());
                            bool bRemove = false;
                            bool bCancel = false;
                            if(ListView_FindItem(hControl, -1, &find) != -1){
                                if(MessageBox(hwnd, "This texture is already used by at least one mesh. The two textures will be merged in the list. Do you want to continue anyway?", "Warning", MB_ICONWARNING | MB_YESNO) == IDYES){
                                    bRemove = true;
                                }
                                else bCancel = true;
                            }
                            if(sNewTex != sOldTex && Mdl->GetFileData() && !bCancel){
                                FileHeader & Data = *Mdl->GetFileData();
                                if(sNewTex.size() > 16) MessageBox(hwnd, "Texture name is longer than 16 characters. This may or my not cause problems in the game.", "Warning", MB_ICONWARNING | MB_OK);
                                for(std::size_t n = 0; n < Data.MH.ArrayOfNodes.size(); ++n){
                                    Node & node = Data.MH.ArrayOfNodes.at(n);
                                    if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_AABB)){
                                        if(nID == IDC_TEXTURE_LISTVIEW1 && std::string(node.Mesh.cTexture1.c_str()) == std::string(sOldTex.c_str())) Mdl->UpdateTexture(node, sNewTex, 1);
                                        else if(nID == IDC_TEXTURE_LISTVIEW2 && std::string(node.Mesh.cTexture2.c_str()) == std::string(sOldTex.c_str())) Mdl->UpdateTexture(node, sNewTex, 2);
                                        else if(nID == IDC_TEXTURE_LISTVIEW3 && std::string(node.Mesh.cTexture3.c_str()) == std::string(sOldTex.c_str())) Mdl->UpdateTexture(node, sNewTex, 3);
                                        else if(nID == IDC_TEXTURE_LISTVIEW4 && std::string(node.Mesh.cTexture4.c_str()) == std::string(sOldTex.c_str())) Mdl->UpdateTexture(node, sNewTex, 4);
                                    }
                                }
                                if(bRemove) ListView_DeleteItem(hControl, info->item.iItem);
                                else ListView_SetItemText(hControl, info->item.iItem, 0, const_cast<char*>(sNewTex.c_str()));
                            }
                        }
                    }
                }
                break;
            }
        }
        break;
        case WM_COMMAND:
        {
            (void)wParam;
            (void)lParam;
            if(Mdl != nullptr){
                // Reserved for future texture-settings commands.
            }
        }
        break;
        case WM_CLOSE:
        {
            if(Mdl != nullptr){
                if(bChange && Mdl->GetFileData()){
                    //MessageBox(hwnd, "Bumpmapping has changed. The program will now reprocess the model to apply the changes.", "Note", MB_OK | MB_ICONINFORMATION);
                    EndDialog(hwnd, 2);
                    return TRUE;
                }
            }
            EndDialog(hwnd, 1);
        }
        break;
        default:
            return FALSE;
    }
    return TRUE;
}


INT_PTR CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    static bool bArea = false;
    static bool bAngle = false;
    static bool bCrease = false;
    static unsigned nCrease = 0;
    switch(message){
        case WM_INITDIALOG:
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) lParam);
            MDL* Mdl = (MDL*) lParam;
            bArea = Mdl->bSmoothAreaWeighting;
            bAngle = Mdl->bSmoothAngleWeighting;
            bCrease = Mdl->bCreaseAngle;
            nCrease = Mdl->nCreaseAngle;
            if(Mdl->bSmoothAngleWeighting) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_ANGLE_WEIGHT), BST_CHECKED);
            if(Mdl->bSmoothAreaWeighting) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_AREA_WEIGHT), BST_CHECKED);
            if(Mdl->bDetermineSmoothing) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_CALC_SMOOTHING), BST_CHECKED);
            if(Mdl->bWriteAnimations) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_DO_ANIMATIONS), BST_CHECKED);
            if(Mdl->bSkinToTrimesh) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_SKIN_TRIMESH), BST_CHECKED);
            if(Mdl->bLightsaberToTrimesh) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_SABER_TRIMESH), BST_CHECKED);
            if(Mdl->bBezierToLinear) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_BEZIER_LINEAR), BST_CHECKED);
            if(Mdl->bExportWok) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_EXPORT_WOK), BST_CHECKED);
            if(Mdl->bUseWokData) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_WOK_COORDS), BST_CHECKED);
            if(bDotAsciiDefault) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_DOT_ASCII), BST_CHECKED);
            if(bSaveReport) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_SAVE_REPORT), BST_CHECKED);
            if(Mdl->bMinimizeVerts) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_MIN_VERT), BST_CHECKED);
            if(Mdl->bMinimizeVerts2) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_MIN_VERT2), BST_CHECKED);
            if(Mdl->bCreaseAngle) Button_SetCheck(GetDlgItem(hwnd, DLG_ID_CREASE_ANGLE), BST_CHECKED);
            SetWindowText(GetDlgItem(hwnd, DLG_ID_CREASE_ANGLE_DEG), std::to_string(Mdl->nCreaseAngle).c_str());
        }
        break;
        case WM_COMMAND:
        {
            int nID = LOWORD(wParam);
            HWND hControl = (HWND) lParam;
            MDL * Mdl = nullptr;
            if(GetWindowLongPtr(hwnd, GWLP_USERDATA) != 0) Mdl = (MDL*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if(Mdl != nullptr){
                switch(nID){
                    case DLG_ID_AREA_WEIGHT:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bSmoothAreaWeighting = true;
                        else Mdl->bSmoothAreaWeighting = false;
                    }
                    break;
                    case DLG_ID_ANGLE_WEIGHT:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bSmoothAngleWeighting = true;
                        else Mdl->bSmoothAngleWeighting = false;
                    }
                    break;
                    case DLG_ID_CALC_SMOOTHING:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bDetermineSmoothing = true;
                        else Mdl->bDetermineSmoothing = false;
                    }
                    break;
                    case DLG_ID_DO_ANIMATIONS:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bWriteAnimations = true;
                        else Mdl->bWriteAnimations = false;
                    }
                    break;
                    case DLG_ID_SKIN_TRIMESH:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bSkinToTrimesh = true;
                        else Mdl->bSkinToTrimesh = false;
                    }
                    break;
                    case DLG_ID_SABER_TRIMESH:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bLightsaberToTrimesh = true;
                        else Mdl->bLightsaberToTrimesh = false;
                    }
                    break;
                    case DLG_ID_BEZIER_LINEAR:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bBezierToLinear = true;
                        else Mdl->bBezierToLinear = false;
                    }
                    break;
                    case DLG_ID_EXPORT_WOK:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bExportWok = true;
                        else Mdl->bExportWok = false;
                    }
                    break;
                    case DLG_ID_WOK_COORDS:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bUseWokData = true;
                        else Mdl->bUseWokData = false;
                    }
                    break;
                    case DLG_ID_DOT_ASCII:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) bDotAsciiDefault = true;
                        else bDotAsciiDefault = false;
                    }
                    break;
                    case DLG_ID_SAVE_REPORT:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) bSaveReport = true;
                        else bSaveReport = false;
                    }
                    break;
                    case DLG_ID_MIN_VERT:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bMinimizeVerts = true;
                        else Mdl->bMinimizeVerts = false;
                    }
                    break;
                    case DLG_ID_MIN_VERT2:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bMinimizeVerts2 = true;
                        else Mdl->bMinimizeVerts2 = false;
                    }
                    break;
                    case DLG_ID_CREASE_ANGLE:
                    {
                        if(Button_GetCheck(hControl) == BST_CHECKED) Mdl->bCreaseAngle = true;
                        else Mdl->bCreaseAngle = false;
                    }
                    break;
                    case DLG_ID_CREASE_ANGLE_DEG:
                    {
                        std::string sBuff = GetWindowTextString(GetDlgItem(hwnd, DLG_ID_CREASE_ANGLE_DEG));
                        if(sBuff == "") Mdl->nCreaseAngle = 60;
                        else Mdl->nCreaseAngle = stou(sBuff);
                    }
                    break;
                }
            }
        }
        break;
        case WM_CLOSE:
        {
            MDL * Mdl = nullptr;
            if(GetWindowLongPtr(hwnd, GWLP_USERDATA) != 0) Mdl = (MDL*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if(Mdl != nullptr){
                if((bArea != Mdl->bSmoothAreaWeighting || bAngle != Mdl->bSmoothAngleWeighting || bCrease != Mdl->bCreaseAngle || nCrease != Mdl->nCreaseAngle) && Mdl->GetFileData()) MessageBox(hwnd, "You need to reload the current model for the changes to take effect.", "Note", MB_OK | MB_ICONINFORMATION);
            }
            ManageIni(INI_WRITE);
            EndDialog(hwnd, wParam);
        }
        break;
        default:
            return FALSE;
    }
    return TRUE;
}
