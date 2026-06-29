#include "bead_winctrl.h"
#include <stdexcept> // for standard exceptions
//#include <exception> //for std::exception

HTREEITEM TreeView_GetNthChild(HWND hwndTree, HTREEITEM htiParent, int nChild){
    if(hwndTree == NULL || nChild < 0) return NULL;

    HTREEITEM htiChild = (htiParent == NULL) ? TreeView_GetRoot(hwndTree) : TreeView_GetChild(hwndTree, htiParent);
    for(int n = 0; n < nChild && htiChild != NULL; n++){
        htiChild = TreeView_GetNextSibling(hwndTree, htiChild);
    }
    return htiChild;
}

void TreeView_DeleteAllChildren(HWND hwndTree, HTREEITEM htiParent){
    if(hwndTree == NULL || htiParent == NULL) return;
    HTREEITEM htiChild = TreeView_GetChild(hwndTree, htiParent);
    while(htiChild != NULL){
        if(!TreeView_DeleteItem(hwndTree, htiChild)) return;
        htiChild = TreeView_GetChild(hwndTree, htiParent);
    }
}

bool TreeView_ExpandAll(HWND hwndTV, HTREEITEM hItem, UINT flag){
    if(hwndTV == NULL || hItem == NULL) return false;

    HTREEITEM htiChild = TreeView_GetChild(hwndTV, hItem);
    while(htiChild != NULL){
        if(TreeView_GetChild(hwndTV, htiChild) != NULL){
            if(!TreeView_ExpandAll(hwndTV, htiChild, flag))
                return false;
        }
        htiChild = TreeView_GetNextSibling(hwndTV, htiChild);
    }

    if(!(TreeView_GetItemState(hwndTV, hItem, TVIS_EXPANDED) & TVIS_EXPANDED) && flag == TVE_COLLAPSE) return true;
    return TreeView_Expand(hwndTV, hItem, flag);
}

namespace {
    constexpr int kControlTextChars = 256;

    std::string TrimControlText(const std::string & sBuffer){
        const std::size_t nTerminator = sBuffer.find('\0');
        return nTerminator == std::string::npos ? sBuffer : sBuffer.substr(0, nTerminator);
    }

    std::wstring TrimControlText(const std::wstring & sBuffer){
        const std::size_t nTerminator = sBuffer.find(L'\0');
        return nTerminator == std::wstring::npos ? sBuffer : sBuffer.substr(0, nTerminator);
    }
}

HTREEITEM TreeView_GetChildByText(HWND hwndTree, HTREEITEM htiParent, const std::string & sText){
    HTREEITEM htiChild = TreeView_GetChild(hwndTree, htiParent);
    if(htiParent == NULL) htiChild = TreeView_GetRoot(hwndTree);
    while(htiChild != NULL){
        TVITEM tvi = {};
        std::string sBuffer(kControlTextChars, '\0');
        tvi.mask = TVIF_TEXT;
        tvi.hItem = htiChild;
        tvi.pszText = &sBuffer.front();
        tvi.cchTextMax = static_cast<int>(sBuffer.size());
        if(TreeView_GetItem(hwndTree, &tvi) && TrimControlText(sBuffer) == sText) return htiChild;
        htiChild = TreeView_GetNextSibling(hwndTree, htiChild);
    }
    return NULL;
}

HTREEITEM TreeView_GetChildByText(HWND hwndTree, HTREEITEM htiParent, const std::wstring & sText){
    HTREEITEM htiChild = TreeView_GetChild(hwndTree, htiParent);
    if(htiParent == NULL) htiChild = TreeView_GetRoot(hwndTree);
    while(htiChild != NULL){
        TVITEMW tvi = {};
        std::wstring sBuffer(kControlTextChars, L'\0');
        tvi.mask = TVIF_TEXT;
        tvi.hItem = htiChild;
        tvi.pszText = &sBuffer.front();
        tvi.cchTextMax = static_cast<int>(sBuffer.size());
        if(TreeView_GetItem(hwndTree, &tvi) && TrimControlText(sBuffer) == sText) return htiChild;
        htiChild = TreeView_GetNextSibling(hwndTree, htiChild);
    }
    return NULL;
}

int TabCtrl_GetTabIndexByText(HWND hwndTabs, const std::string & sText){
    if(hwndTabs == NULL) return -1;
    const int nTabs = TabCtrl_GetItemCount(hwndTabs);
    for(int n = 0; n < nTabs; n++){
        TCITEM tci = {};
        std::string sBuffer(kControlTextChars, '\0');
        tci.mask = TCIF_TEXT;
        tci.pszText = &sBuffer.front();
        tci.cchTextMax = static_cast<int>(sBuffer.size());
        if(TabCtrl_GetItem(hwndTabs, n, &tci) && TrimControlText(sBuffer) == sText) return n;
    }
    return -1;
}

bool TabCtrl_AppendTab(HWND hTabControl, const std::string & sName){
    if(hTabControl == NULL) return false;
    int nTabs = TabCtrl_GetItemCount(hTabControl);
    TCITEM tcAdd = {};
    tcAdd.mask = TCIF_TEXT;
    tcAdd.pszText = const_cast<char*>(sName.c_str());
    tcAdd.cchTextMax = static_cast<int>(sName.length());
    return (TabCtrl_InsertItem(hTabControl, nTabs, &tcAdd) != -1);
}

std::string TabCtrl_GetCurSelName(HWND hTabcontrol){
    if(hTabcontrol == NULL) return std::string();
    int nSel = TabCtrl_GetCurSel(hTabcontrol);
    if(nSel == -1) return std::string();

    TCITEM tcitem = {};
    std::string sReturn(kControlTextChars, '\0');
    tcitem.mask = TCIF_TEXT;
    tcitem.pszText = &sReturn.front();
    tcitem.cchTextMax = static_cast<int>(sReturn.size());
    if(!TabCtrl_GetItem(hTabcontrol, nSel, &tcitem)) return std::string();
    return TrimControlText(sReturn);
}

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX* /*lpelfe*/, NEWTEXTMETRICEX* /*lpntme*/, int /*FontType*/, LPARAM lParam){
    bool & bReturn = * (bool*) lParam;
    bReturn = true;

    return TRUE;
}

bool Font_IsInstalled(const std::string & sFont){
    // Get the screen DC
    HDC hdc = CreateCompatibleDC(NULL);

    LOGFONT lf{};
    // Any character set will do
    lf.lfCharSet = DEFAULT_CHARSET;
    // Set the facename to check for
    if(sFont.length() > 31) throw std::length_error("The font name '" + sFont + "'is too long!\n");
    const char * cFont = sFont.c_str();
    char * cFace = lf.lfFaceName;
    char * cFaceEnd = lf.lfFaceName + 32;
    while(cFace != cFaceEnd && *cFont != '\0'){
        *cFace = *cFont;
        cFace++;
        cFont++;
    }
    *cFace = '\0';

    if(hdc == NULL) return false;

    bool bReturn = false;

    // Enumerate fonts
    ::EnumFontFamiliesEx(hdc, &lf,  (FONTENUMPROC) EnumFontFamExProc, (LPARAM) &bReturn, 0);
    DeleteDC(hdc);

    return bReturn;
}
