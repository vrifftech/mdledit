#ifndef BEAD_WINCTRL_H_INCLUDED
#define BEAD_WINCTRL_H_INCLUDED

#include "win32_compat.h"
#include <commctrl.h>
#include <string> //for std::string

HTREEITEM TreeView_GetNthChild(HWND hwndTree, HTREEITEM htiParent, int nChild = 0);
void TreeView_DeleteAllChildren(HWND hwndTree, HTREEITEM htiParent);
bool TreeView_ExpandAll(HWND hwndTV, HTREEITEM hItem, UINT flag);
HTREEITEM TreeView_GetChildByText(HWND hwndTree, HTREEITEM htiParent, const std::string & sText);
HTREEITEM TreeView_GetChildByText(HWND hwndTree, HTREEITEM htiParent, const std::wstring & sText);
int TabCtrl_GetTabIndexByText(HWND hwndTabs, const std::string & sText);
bool TabCtrl_AppendTab(HWND hTabControl, const std::string & sName);
std::string TabCtrl_GetCurSelName(HWND hTabcontrol);
bool Font_IsInstalled(const std::string & sFont);

#endif // BEAD_WINCTRL_H_INCLUDED
