#include "general.h"
#include "win32_compat.h"
#include "MDL.h"
#include <fstream>
#include <regex>
#include <algorithm>
#include <memory> //for std::unique_ptr

class ReportDlgWindow{
    WNDCLASSEX WindowClass;
    static char cClassName [];
    static bool bRegistered;
    MDL * MdlPtr = nullptr;

  public:
    HWND hMe;
    ReportDlgWindow(MDL & Mdl);
    bool Run();
    friend LRESULT CALLBACK ReportDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

char ReportDlgWindow::cClassName[] = "mdlreportdlg";
bool ReportDlgWindow::bRegistered = false;
LRESULT CALLBACK ReportDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
std::unique_ptr<ReportDlgWindow> ReportDlgWindowInstance;

ReportDlgWindow::ReportDlgWindow(MDL & Mdl): MdlPtr(&Mdl) {
    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = GetModuleHandle(nullptr); // Instance of the application
    WindowClass.lpfnWndProc = ReportDlgWindowProc; // Pointer to callback procedure

    // #2 Class styles
    WindowClass.style = CS_DBLCLKS; // Class styles

    // #3 Background
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); //(HBRUSH) (COLOR_WINDOW); // Background brush

    // #4 Cursor
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor

    // #5 Icon
    WindowClass.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_DLG_ICON)); //NULL; // Class Icon
    WindowClass.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_DLG_ICON)); //NULL; // Small icon for this class

    // #6 Menu
    WindowClass.lpszMenuName = MAKEINTRESOURCE(IDM_EDITOR_DLG); // Menu Resource

    // #7 Other
    WindowClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    WindowClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure
}

bool ReportDlgWindow::Run(){
    if(!bRegistered){
        if(!RegisterClassEx(&WindowClass)){
            std::cout << "Registering Window Class " << WindowClass.lpszClassName << " failed!\n";
            return false;
        }
        std::cout << "Class " << WindowClass.lpszClassName << " registered!\n";
        bRegistered = true;
    }
    //HMENU *has* to be NULL!!!!! Otherwise the function fails to create the window!
    hMe = CreateWindowEx(0, WindowClass.lpszClassName, "", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         CW_USEDEFAULT, CW_USEDEFAULT, 600, 300,
                         HWND_DESKTOP, nullptr, GetModuleHandle(nullptr), this);
    if(!hMe) return false;
    ShowWindow(hMe, true);
    return true;
}

LRESULT CALLBACK ReportDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    HWND hEdit = GetDlgItem(hwnd, IDDB_EDIT);
    ReportDlgWindow* reportdlg = nullptr;
    if(GetWindowLongPtr(hwnd, GWLP_USERDATA) != 0) reportdlg = (ReportDlgWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    /* handle the messages */
    switch(message){
        case WM_CREATE:
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*) lParam)->lpCreateParams);
            reportdlg = (ReportDlgWindow*) ((CREATESTRUCT*) lParam)->lpCreateParams;

            std::string sMonospace = "Consolas";
            if(!Font_IsInstalled(sMonospace)){
                std::cout << "Consolas font not installed! Switching to Courier New...\n";
                sMonospace = "Courier New";
                if(!Font_IsInstalled(sMonospace)){
                    std::cout << "Courier New font not installed! No further alternatives!\n";
                }
            }

            HFONT hFont1 = CreateFont(
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
                DEFAULT_PITCH | FF_DONTCARE	,	// pitch and family
                sMonospace.c_str() 	// pointer to typeface name string
            );

            GetClientRect(hwnd, &rcClient);
            hEdit = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                           rcClient.left, rcClient.top, rcClient.right, rcClient.bottom,
                           hwnd, (HMENU) IDDB_EDIT, GetModuleHandle(nullptr), nullptr);
            SendMessage(hEdit, WM_SETFONT, (WPARAM) hFont1, MAKELPARAM(TRUE, 0));

            std::string sContents (reportdlg->MdlPtr->GetReport().str());
            std::regex e ("\n");
            sContents = std::regex_replace(sContents, e, "\r\n");
            SetWindowTextW(hwnd, (L"Report for " + reportdlg->MdlPtr->GetFilename()).c_str());
            SetWindowText(hEdit, sContents.c_str());
        }
        break;
        case WM_COMMAND:
        {
            int nID = LOWORD(wParam);
            switch(nID){
                case IDDB_SAVE:
                {
                    /// Get the mdl path, remove .ascii and .mdl extensions, add "_report.txt".
                    if(reportdlg == nullptr) break;
                    if(reportdlg->MdlPtr == nullptr) break;
                    std::wstring sFile = reportdlg->MdlPtr->GetFullPath();
                    if(safesubstr(sFile, sFile.size() - 6, 6) == L".ascii") sFile = safesubstr(sFile, 0, sFile.size() - 6);
                    if(safesubstr(sFile, sFile.size() - 4, 4) == L".mdl") sFile = safesubstr(sFile, 0, sFile.size() - 4);
                    sFile += L"_report.txt";

                    OPENFILENAMEW ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"Text file (*.txt)\0*.txt\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST;
                    while(true){
                        PrepareFileDialogBuffer(sFile);
                        ofn.lpstrFile = &sFile.front(); //The open dialog will update sFile with the file path
                        if(GetSaveFileNameW(&ofn)){
                            TrimFileDialogBuffer(sFile);
                            /// Now we check if this file exists already
                            if(PathFileExistsW(sFile.c_str())){
                                int nDecision = WarningYesNoCancel(L"The file '" + std::wstring(PathFindFileNameW(sFile.c_str())) + L"' already exists! Do you want to overwrite?");
                                if(nDecision == IDCANCEL) break;
                                else if(nDecision == IDNO) continue; /// This will run the file selection dialog a second time
                            }

                            std::cout << "Writing file:\n" << sFile.c_str() << "\n";

                            /// Start timer
                            Timer t1;

                            /// Create file
                            //std::ofstream file(sFile, std::fstream::out);
                            HANDLE hFile = bead_CreateWriteFile(sFile);

                            //if(!file.is_open()){
                            if(hFile == INVALID_HANDLE_VALUE){
                                std::cout << "File creation failed for " << sFile.c_str() << ". Aborting.\n";
                                break;
                            }

                            /// Write and close file
                            //file << reportdlg->MdlPtr->GetReport().str();
                            if(!bead_WriteFile(hFile, reportdlg->MdlPtr->GetReport().str())){
                                std::cout << "File write failed for " << to_ansi(sFile) << ".\n";
                            }
                            //file.close();
                            CloseHandle(hFile);

                            /// Report time
                            std::cout << "Wrote file in: " << t1.GetTime() << "\n";
                            break;
                        }
                        else break;
                    }
                }
                break;
            }
        }
        break;
        case WM_SIZE:
        {
            SetWindowPos(hEdit, nullptr, rcClient.left, rcClient.top, rcClient.right, rcClient.bottom, 0);
        }
        break;
        case WM_CTLCOLORSTATIC:
            {
                HDC hdcEdit = (HDC) wParam;
                HBRUSH hBackground = CreateSolidBrush(RGB(255, 255, 255));
                SetTextColor(hdcEdit, RGB(0, 0, 0));
                SetBkColor(hdcEdit, RGB(255, 255, 255));
                return (INT_PTR) hBackground;
            }
        break;
        case WM_DESTROY:
            SetWindowText(hEdit, "");
            ReportDlgWindowInstance.reset();
        break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
        break;
        default:
        {
            return DefWindowProc (hwnd, message, wParam, lParam);
        }
    }
    return 0;
}

void OpenReportDlg(MDL & Mdl){
    if(ReportDlgWindowInstance){
        ReportDlgWindow & reportdlg = *ReportDlgWindowInstance;
        SetWindowPos(reportdlg.hMe, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        return;
    }
    ReportDlgWindowInstance.reset(new ReportDlgWindow(Mdl));
    ReportDlgWindow & reportdlg = *ReportDlgWindowInstance;
    if(!reportdlg.Run()){
        std::cout << "ReportDlgWindow creation failed!\n";
    }
}
