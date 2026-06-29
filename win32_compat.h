#ifndef WIN32_COMPAT_H_INCLUDED
#define WIN32_COMPAT_H_INCLUDED

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cderr.h>
#include <shlwapi.h>

#endif // WIN32_COMPAT_H_INCLUDED
