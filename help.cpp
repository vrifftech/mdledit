#include "general.h"
#include "MDL.h"
#include <shlwapi.h>
#include <fstream>
#include <regex>
#include <algorithm>
#include <memory> //for std::unique_ptr

class HelpDlgWindow{
    WNDCLASSEX WindowClass;
    static char cClassName [];
    static bool bRegistered;

  public:
    HWND hMe;
    HelpDlgWindow();
    bool Run();
    friend LRESULT CALLBACK HelpDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

char HelpDlgWindow::cClassName[] = "mdlhelpdlg";
bool HelpDlgWindow::bRegistered = false;
LRESULT CALLBACK HelpDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
std::unique_ptr<HelpDlgWindow> HelpDlgWindowInstance;

HelpDlgWindow::HelpDlgWindow(){
    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = GetModuleHandle(nullptr); // Instance of the application
    WindowClass.lpfnWndProc = HelpDlgWindowProc; // Pointer to callback procedure

    // #2 Class styles
    WindowClass.style = CS_DBLCLKS; // Class styles

    // #3 Background
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); //(HBRUSH) (COLOR_WINDOW); // Background brush

    // #4 Cursor
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor

    // #5 Icon
    WindowClass.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_DLG_ICON)); //NULL; // Class Icon
    WindowClass.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_DLG_ICON)); // Small icon for this class

    // #6 Menu
    WindowClass.lpszMenuName = NULL; // Menu Resource

    // #7 Other
    WindowClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    WindowClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure
}

bool HelpDlgWindow::Run(){
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
                         CW_USEDEFAULT, CW_USEDEFAULT, 400, 600,
                         HWND_DESKTOP, nullptr, GetModuleHandle(nullptr), this);
    if(!hMe) return false;
    ShowWindow(hMe, true);
    return true;
}

std::string GetHelpData(const std::string & sTab);

LRESULT CALLBACK HelpDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    HWND hTabs = GetDlgItem(hwnd, IDC_TABS);
    HWND hEdit = GetDlgItem(hwnd, IDDB_EDIT);

    /* handle the messages */
    switch(message){
        case WM_CREATE:
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*) lParam)->lpCreateParams);

            std::string sMonospace = "Consolas";
            if(!Font_IsInstalled(sMonospace)){
                std::cout << "Consolas font not installed! Switching to Courier New...\n";
                sMonospace = "Courier New";
                if(!Font_IsInstalled(sMonospace)){
                    std::cout << "Courier New font not installed! No further alternatives!\n";
                }
            }

            HFONT hFont4 = CreateFont(
                12,  //Height
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
                "MS Shell Dlg" //"Segoe UI" //"MS Shell Dlg" // 	// pointer to typeface name string
            );

            GetClientRect(hwnd, &rcClient);
            hTabs = CreateWindowEx(0, WC_TABCONTROL, "", WS_VISIBLE | WS_CHILD | TCS_FOCUSNEVER | TCS_MULTILINE,
                                   rcClient.left, rcClient.top, rcClient.right + 2, rcClient.bottom + 1,
                                   hwnd, (HMENU) IDC_TABS, GetModuleHandle(nullptr), nullptr);
            SendMessage(hTabs, WM_SETFONT, (WPARAM) hFont4, MAKELPARAM(TRUE, 0));
            hEdit= CreateWindowEx(0, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                                   rcClient.left + 5, rcClient.top + 18 * TabCtrl_GetRowCount(hTabs) + 6, rcClient.right - 8, rcClient.bottom - 18 * TabCtrl_GetRowCount(hTabs) - 9,
                                   hwnd, (HMENU) IDDB_EDIT, GetModuleHandle(nullptr), nullptr);
            SendMessage(hEdit, WM_SETFONT, (WPARAM) hFont4, MAKELPARAM(TRUE, 0));

            TabCtrl_AppendTab(hTabs, "Basics");
            TabCtrl_AppendTab(hTabs, "File Types");
            TabCtrl_AppendTab(hTabs, "Model Loading");
            TabCtrl_AppendTab(hTabs, "Model Saving");
            TabCtrl_AppendTab(hTabs, "Batch Processing");
            TabCtrl_AppendTab(hTabs, "Model Editing");
            TabCtrl_AppendTab(hTabs, "Hex View");
            TabCtrl_AppendTab(hTabs, "Vertex Normals");
            TabCtrl_AppendTab(hTabs, "Head Link");

            SetWindowText(hwnd, "Help");
            SetWindowText(hEdit, GetHelpData(TabCtrl_GetCurSelName(hTabs)).c_str());
        }
        break;
        case WM_NOTIFY:
        {
            NMHDR * nmhdr = (NMHDR *) lParam;
            int nID = nmhdr->idFrom;
            UINT nNotification = nmhdr->code;
            switch(nID){
                case IDC_TABS:
                {
                    if(nNotification == TCN_SELCHANGE){
                        SetWindowText(hEdit, GetHelpData(TabCtrl_GetCurSelName(hTabs)).c_str());
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
                case IDDB_SAVE:
                {
                }
                break;
            }
        }
        break;
        case WM_SIZE:
        {
            SetWindowPos(hTabs, nullptr, rcClient.left, rcClient.top, rcClient.right + 2, rcClient.bottom + 1, 0);
            SetWindowPos(hEdit, nullptr, rcClient.left + 5, rcClient.top + 18 * TabCtrl_GetRowCount(hTabs) + 6, rcClient.right - 8, rcClient.bottom - 18 * TabCtrl_GetRowCount(hTabs) - 9, 0);
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
            HelpDlgWindowInstance.reset();
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

void OpenHelpDlg(){
    if(HelpDlgWindowInstance){
        HelpDlgWindow & helpdlg = *HelpDlgWindowInstance;
        SetWindowPos(helpdlg.hMe, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        return;
    }
    HelpDlgWindowInstance.reset(new HelpDlgWindow);
    HelpDlgWindow & helpdlg = *HelpDlgWindowInstance;
    if(!helpdlg.Run()){
        std::cout << "HelpDlgWindow creation failed!\n";
    }
}

#define nl "\r\n"
std::string GetHelpData(const std::string & sTab){
    std::stringstream ssHelp;
    //char nl [] = "\r\n"; /// New Line
    if(sTab == "Basics"){
        ssHelp <<
           "MDLedit allows you to compile and decompile models for the games:"
        nl " - Star Wars Knights of the Old Republic"
        nl " - Star Wars Knights of the Old Republic II - the Sith Lords"
        nl "for both the PC and XBOX version of the games, as well as to directly edit them to a certain extent."
        nl ""
        nl "It is possible for MDLedit to remember your settings. To enable that create a new empty file in the same directory as MDLedit's executable and "
           "name it 'mdledit.ini'."
        ;
    }
    else if(sTab == "File Types"){
        ssHelp <<
           "The model files that the games use are in the binary format. This means that if you open them with a text editor you won't be able to read them "
           "because they are meant to be read by the engine. "
           "To be able to do anything with a model, we need to convert it to the ascii format first. An ascii file can be read with a text editor. "
           "The ascii format is the one that is required by KOTORmax and KotorBlender. It is also the format that these tools produce when you export "
           "your model. "
           "The process of converting binary to ascii is called decompilation and the process of converting ascii to binary is called compilation. "
           "After you're done editing a model with a 3D editor you always need to compile it so the game can read it."
        nl ""
        nl "MDLedit handles the following types of binary files:"
        nl " * model files (.mdl): This is the file that contains most of the information about the model."
        nl " * model extension files (.mdx): This file contains information about the vertices of all meshes in the model."
        nl " * area walkmesh files (.wok): This file contains information about the area walkmesh."
        nl " * placeable walkmesh files (.pwk): This file contains information about the placeable walkmesh."
        nl " * door walkmesh files (.dwk): This file contains information about the door walkmesh. These files always come in triplets, because the "
           "engine supports swinging doors that open to either side. "
           "The three .dwks represent the possible states: closed, open to one side, open to the other side."
        nl ""
        nl "MDLedit handles the following types of ascii files:"
        nl " * model files (.mdl or .mdl.ascii): This is the file that contains most of the information about the model, including vertex information."
        nl " * placeable walkmesh files (.pwk or .pwk.ascii): This file contains information about the placeable walkmesh."
        nl " * door walkmesh files (.dwk or .dwk.ascii): This file contains information about the door walkmesh. Unlike the binary file, this file contains "
           "the information about all three states, so there is only one ascii .dwk that goes with its .mdl."
        nl " * area walkmesh files (.wok or .wok.ascii): This file contains information about the area walkmesh. MDLedit can output this file, but it does "
           "not read it because the information it contains is already contained in the .mdl."
        nl ""
        nl "MDLedit will automatically determine whether the file is binary or ascii when it is loaded. "
        nl ""
        nl "MDLedit is always able to load and save both .XXX and .XXX.ascii files in the ascii format. There is an option under Edit -> Settings that will allow you to "
           "set the .XXX.ascii-type extension as the default. This will have the effect that files are saved as .XXX.ascii and not .XXX when batch converting "
           "to ascii and that the default extension that MDLedit will suggest to save a loaded model as will be .XXX.ascii. It also affects the order in which "
           "MDLedit checks for the extensions when loading ascii dwk and pwk files. If .ascii as default is set, then it will first try to find an ascii "
           "FILENAME.XXX.ascii and if it doesn't find it, it will check for a FILENAME.XXX. If the .ascii default option is not set, then the order will be reversed."
        ;
    }
    else if(sTab == "Model Loading"){
        ssHelp <<
           "To load up a model, go to File -> Load and select your .mdl or .mdl.ascii file. MDLedit will automatically determine whether it is loading "
           "a binary or ascii .mdl file."
        nl ""
        nl "If the model file is in ascii format, then MDLedit will also search the same directory for files with the same name and a .pwk(.ascii) or "
           ".dwk(.ascii) extension. It will only load them if they are in the right format. The .ascii by default option in Edit -> Settings determines "
           "the order in which MDLedit checks for the files (either .XXX first or .XXX.ascii first)."
        nl "Note: MDLedit will not look for a .wok(.ascii) file. MDLedit assumes that any model with a binary .wok file also has an aabb node in the "
           ".mdl. The two meshes should always be compatible, so MDLedit will use the aabb node data to construct the binary .wok file; hence, it does not "
           "need to load an ascii .wok(.ascii) file."
        nl ""
        nl "Ascii files are not specified for game (K1 vs K2) or platform (PC vs XBOX), so when they are loaded, the binary code is written according "
           "to the current game and platform setting. The binary code will be updated automatically when these settings are changed."
        nl ""
        nl "When loading an ascii file, the option 'Minimize (weld) vertices' becomes relevant. If turned on, MDLedit will weld any vertices it can without "
           "leaving any visual effect on the model. In other words, it will use the minimum number of vertices necessary, lowering the size of the "
           "binary files."
        nl ""
        nl "If the model file is in binary format, then MDLedit will also search the same directory for files with the same name and a .mdx, .pwk and "
           ".wok extension, as well as three .dwk files, each named the same as the .mdl file plus a digit (0, 1 or 2) representing the three states of "
           "a swinging door that opens to either side."
        nl ""
        nl "Binary files are specific to a game and platform, the settings will therefore change automatically to reflect that."
        nl ""
        nl "Once all the files are loaded the model is read and processed. MDLedit will show a progress bar for the duration of the processing. At certain "
           "points during the processing that can take a while it is possible to cancel it by pressing ESC."
        nl "When "
           "loading binary files, the processing only consists of the smoothing group calculation algorithm, but this algorithm may end up taking a LOT "
           "of time. The progress window will say 'Recalculating Vectors...' at that point. This may seem as though the application hung, but "
           "the application is actually working and will finish, it just may take a REALLY LONG time. Do not try to wait it out, press the ESC key to abort "
           "the smoothing group calculation."
        nl "Because of this possibility, there is an option under Edit -> Settings that will let you turn the smoothing group calculation algorithm off. "
           "Of course, if you turn it off the ascii file will not contain any smoothing information."
        nl ""
        nl "The processing is much more extensive in the case of .ascii files, but will never take a REALLY long time."
        nl ""
        nl "After the model has been processed the data is loaded into the Tree View and the Hex View. You may now proceed to explore the model, edit it, "
           "or export it again."
        nl ""
        nl "If an error occurs during reading or processing, MDLedit will either let you know immediately (if it is a potentially fatal error) or just "
           "write it into its report, which you can view by clicking View -> Show Report. If you turn on automatic report writing under Edit -> Settings "
           "then the report will automatically be written as MODELNAME_report.txt to the model's directory."
        ;
    }
    else if(sTab == "Model Saving"){
        ssHelp <<
           "To save a loaded model, go to File -> Save and select whether you want to export an ascii or binary file. Next, the standard saving dialog "
           "box will pop up. If you enter the name of an existing file, MDLedit will prompt you and ask whether you want to overwrite. It will only do "
           "this for the .mdl file itself! It will automatically overwrite .mdx, .pwk, .dwk, etc. files whose names happen to match."
        nl ""
        nl "There are several options related to saving ascii files under Edit -> Settings."
        nl ""
        nl "'Write Animations' - MDLedit will only write the geometry into the ascii file."
        nl ""
        nl "'Export WOK file' - MDLedit will also export a .wok(.ascii) file, which will be needed by KotorBlender and MDLOps."
        nl ""
        nl "'Use coordinates from WOK file' - Sometimes the geometries of the aabb mesh and the wok mesh will be different in vanilla files. "
           "This option will allow you to make use of either one or the other. To avoid problems with the walkmesh this should be turned on by default."
        nl ""
        nl "'Use .ascii extension by default' - See the 'File Types' tab."
        ;
    }
    else if(sTab == "Batch Processing"){
        ssHelp <<
           "Under File -> Batch it is possible to convert several files at once to either binary or ascii. With this option, all selected files are "
           "loaded whether they are binary or ascii, then they are saved according to the current settings as FILENAME-mdledit.mdl(.ascii). "
           "Whether MDLedit saves ascii files as .XXX or .XXX.ascii depends on the .ascii default setting under Edit -> Settings."
           "It also possible to do this with only a single file; this is the quickest way of simply converting a file to ascii or binary."
        nl ""
        nl "During batch processing, you may press the escape key and the process will halt once it finishes converting the current model."
        ;
    }
    else if(sTab == "Model Editing"){
        ssHelp <<
           "It is possible to edit the model to some extent with MDLedit."
        nl ""
        nl "Under Edit -> Textures, MDLedit will list all the textures in the current model. By clicking on a selected texture name, you will be "
           "able to edit its name and the change will apply to all occurrences of that texture name. There is also a checkbox next to every texture "
           "name, which controls whether those textures are bumpmappable."
        nl ""
        nl "On some nodes in the Tree View, right-clicking will show the option 'Edit values'. When clicked, a new window will appear listing the "
           "values in an ascii-like format. The values that can be edited are generally the ones that cannot break the structure of the file itself, "
           "though they may possibly still crash the game if you enter some impossible values. It is also not possible to add data to the file this "
           "way, only change it. "
           "After you're done editing the values, click File -> Save and the new values will be saved. The binary code is changed only where new values "
           "are applied, so it is possible to use this method to only change specific values in the file and nothing else."
        nl ""
        nl "Currently it is not possible to edit all the values that could possibly be edited, though a lot of them will be added in the future. "
           "While it is possible to edit most controller values, it is currently not possible to edit compressed quaternions."
        ;
    }
    else if(sTab == "Hex View"){
        ssHelp <<
           "The Hex View allows you to view the binary code of the model's files."
        nl ""
        nl "The binary code is color-coded per data type, here is the legend:"
        nl "1) Yellow - a counter, by definition an unsigned integer, size varies"
        nl "2) Green - an IEEE-754 4-byte floating point (float)"
        nl "3) Dark Gray - a null-terminated string of characters (string)"
        nl "4) Light Blue - a signed or unsigned 4-byte integer (int32)"
        nl "5) Purple - a signed or unsigned 2-byte integer (int16)"
        nl "6) Dark Blue - an offset/pointer, always a 4-byte unsigned integer (uint32)"
        nl "7) Brown - a signed or unsigned 1-byte integer (byte)"
        nl "8) Light Gray - invariant data across all models that either has runtime uses or is just padding"
        nl "9) Orange - function pointers, which come in pairs, are unsigned 4-byte integers (uint32)"
        nl "10) Red - data that represents something that we do not yet understand"
        nl "11) Light Olive-Gray - meaningless data that is mostly just padding or maybe space reserved for runtime uses"
        nl "12) Black - unparsed data"
        nl ""
        nl "By double-clicking the offset counters, you can toggle between decimal and hexadecimal display of the counters, but also the offsets that are "
           "displayed in the status bar."
        nl ""
        nl "Double-clicking the binary code will jump you to the corresponding node in the Tree View."
        nl ""
        nl "Highlighting bytes will show their int, uint and float values in the top right, where such values are representable by the highlighted bytes."
        nl ""
        nl "The 'Group Bytes by Data Type' option under View will use underlining to group together the bytes that form the same piece of data. "
        nl ""
        nl "The 'Show Data Struct Boundaries' option under View will use a mark to indicate borders between data structures. "
           "It is also used to delimit the elements inside an array. "
        nl ""
        nl "The 'Compare to ...' option under View will allow you to pick a binary file. MDLedit will then load its contents and compare its binary code to the one "
           "of the currently open model. The result is only visible in Hex Mode and only if View -> Show Differences is turned on. "
           "If there is a difference in a byte between the open model and the one it's being compared to, then the relevant bytes will be displayed as white text on "
           "a shaded background, the color of which is the one the text would otherwise have."
        ;
    }
    else if(sTab == "Vertex Normals"){
        ssHelp <<
           "Every vertex in an MDL file has a normal vector associated with it. This vector determines how light interacts with the object's surface. "
           "Through these vectors it is possible to achieve smoothing. MDLedit has several options related to Vertex Normals under Edit -> Settings."
        nl ""
           "Vertex normals are calculated by putting together the face normals vectors of all the faces that the vertex either belongs to or smooths to "
           "(the faces the vertex smooths to maybe be in other elements or meshes as well). "
           "Whether the vertex smooths to that face is determined from the smoothing groups that are given for every face. If a face the vertex belongs to "
           "has at least one matching smoothing group with another face, then this other face's normal is calculated in as well. "
           "Provided the other face has a vertex in the same position as our vertex under consideration, of course."
        nl ""
        nl "There are "
           "three options that affect the vertex normal calculation at this point. They can be found in Edit -> Settings."
        nl ""
        nl "If area weighting is turned on then the face normals are multiplied by "
           "the face's area before they are incorporated into the vertex normal. This means that bigger faces will have a greater influence on the vertex "
           "normal than smaller faces."
        nl ""
        nl "If angle weighting is turned on then the face normals are multiplied by the angle of the corner of the face touching that vertex. Faces with a "
           "bigger angle in that corner have a greater influence on the vertex normal."
        nl ""
        nl "If the crease angle is set, then MDLedit will perform a check based on the angle between the emerging vertex normal and the face normal "
           "currently under consideration. If the angle is greater than the set crease angle MDLedit will ignore that face normal."
        nl ""
        nl "Angle and area weighting should be used based on what looks best in the game. The crease angle, on the other hand, is supposed to correct the model if "
           "because of it's smoothing group setup the vertex normal may be unwantedly affected by certain faces which may cause it to point in a wrong direction. "
           "In the game, this will appear as a black or shaded spot around the problematic vertex."
        nl ""
        nl "When loading a binary model, MDLedit will try to calculate the smoothing groups from the vertex normals (if this is enabled under Edit -> Settings). "
           "It does that by calculating the vertex normal "
           "based on every possible combination of smoothing groups on the surrounding faces and looks for a match. "
           "Since this vertex normal calculation also uses the options under "
           "Edit -> Settings, those options need to match the options that were used to compile the binary model for the smoothing group calculation to be "
           "successful. Therefore, you may need to try a few different options before you get the right results if you do not know how the model was compiled. "
           "Vanilla models always have just the area weighting enabled, so this is only really a problem when decompiling custom models. The results may be bad "
           "regardless, though, if the model was compiled with a version of MDLOps older than v1.0."
        ;
    }
    else if(sTab == "Head Link"){
        ssHelp <<
           "The head link needs to be enabled for heads to work properly in the game. This functionality is also provided by VarsityPuppet's Head Fixer. "
        nl ""
        nl "There is a pointer in the model header that either points to the root node or the neck bone node. If head link is enabled then that means that "
           "the pointer points to the neck bone node. If a model is used as a head and the head link is not enabled, then the head may appear to be detached "
           "from the body. The head link is preserved in the ascii files and may also be toggled in both KOTORmax and KotorBlender."
        ;
    }
    return ssHelp.str();
}


#define nl "\r\n"
INT_PTR CALLBACK AboutProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM /*lParam*/){
    switch(message){
        case WM_INITDIALOG:
        {
            std::string sText;
            sText =  "MDLedit " + version.Print();
            SetWindowText(hwnd, sText.c_str());
            sText +=
            nl "by bead-v"
            nl
            nl "This application was based on the knowledge of the MDL format from CChargin's MDLOps, later discoveries "
               "on deadlystream.com forums and also largely on ndix UR's work on the new MDLOps. "
            nl "A big thank you goes out to all the people who contributed to this knowledge, including:"
            nl "CChargin, Magnusll, JdNoa, ndix UR, VarsityPuppet, FairStrides, DarthSapiens and others"
            nl
            nl "I would also like to thank these people for their tremendous help with testing, feedback and suggestions:"
            nl "DarthParametric, JCarter426, Quanon, VarsityPuppet, FairStrides"
            nl
            nl "A very special thanks goes to ndix UR, both for sharing his very complete knowledge of the format and his "
               "advice, support and encouragement during the development of this program."
               "";
            SetWindowText(GetDlgItem(hwnd, DLG_ID_STATIC), sText.c_str());
        }
        break;
        case WM_CLOSE:
        {
            EndDialog(hwnd, wParam);
        }
        break;
        default:
            return FALSE;
    }
    return TRUE;
}
