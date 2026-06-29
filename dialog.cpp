#include "general.h"
#include "MDL.h"

class DialogWindow{
    WNDCLASSEX WindowClass;
    static char cClassName [];
    static bool bRegistered;

  public:
    HWND hMe;
    DialogWindow();
    bool Run();
    friend LRESULT CALLBACK DialogWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

char DialogWindow::cClassName[] = "mdleditdialog";
LRESULT CALLBACK DialogWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool DialogWindow::bRegistered = false;

DialogWindow::DialogWindow(){
    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = GetModuleHandle(nullptr); // Instance of the application
    WindowClass.lpfnWndProc = DialogWindowProc; // Pointer to callback procedure

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
    WindowClass.lpszMenuName = NULL; // Menu Resource

    // #7 Other
    WindowClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    WindowClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure
}

bool DialogWindow::Run(){
    if(!bRegistered){
        if(!RegisterClassEx(&WindowClass)){
            std::cout << "Registering Window Class " << WindowClass.lpszClassName << " failed!\n";
            return false;
        }
        std::cout << "Class " << WindowClass.lpszClassName << " registered!\n";
        bRegistered = true;
    }
    //HMENU HAS to be NULL!!!!! Otherwise the function fails to create the window!
    hMe = CreateWindowEx(0, WindowClass.lpszClassName, "", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         CW_USEDEFAULT, CW_USEDEFAULT, 600, 300,
                         HWND_DESKTOP, nullptr, GetModuleHandle(nullptr), nullptr);
    if(!hMe) return false;
    ShowWindow(hMe, true);
    return true;
}

LRESULT CALLBACK DialogWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    HWND hEdit = GetDlgItem(hwnd, IDDB_EDIT);

    /* handle the messages */
    switch(message){
        case WM_CREATE:
        {
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
                "Consolas" 	// pointer to typeface name string
            );
            GetClientRect(hwnd, &rcClient);
            hEdit = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_VSCROLL,
                           rcClient.left+3, rcClient.top, rcClient.right-3, rcClient.bottom,
                           hwnd, (HMENU) IDDB_EDIT, GetModuleHandle(nullptr), nullptr);
            SendMessage(hEdit, WM_SETFONT, (WPARAM) hFont1, MAKELPARAM(TRUE, 0));
        }
        break;
        case WM_SIZE:
        {
            SetWindowPos(hEdit, nullptr, rcClient.left+3, rcClient.top, rcClient.right-3, rcClient.bottom, 0);
            //InvalidateRect(hwnd, &rcClient, false);
        }
        break;
        case WM_DESTROY:
            SetWindowText(hEdit, "");
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

void OpenViewer(MDL & Mdl, std::vector<std::string>cItem, LPARAM lParam){
    std::stringstream sName;
    std::stringstream sPrint;

    ModelHeader & Data = Mdl.GetFileData()->MH;

    sName << "Viewing " << Data.GH.sName.c_str() << " > ";

    if((cItem[0] == "")) return;

    /// Animation ///
    else if((cItem[1] == "Animations")){
        Animation * anim = (Animation*) lParam;

        sName << "Animations > " << anim->sName.c_str();
        Mdl.ConvertToAscii(CONVERT_ANIMATION, sPrint, (void*) lParam);
    }
    /// Anim Node ///
    else if((cItem[2] == "Animations") || ((cItem[4] == "Animations") && ((cItem[1] == "Children") || (cItem[1] == "Parent")))){
        Node * node = (Node*) lParam;

        sName << "Animations > " << cItem[1] << " > " << Data.Names[node->Head.nNameIndex].sName.c_str();
        Mdl.ConvertToAscii(CONVERT_ANIMATION_NODE, sPrint, (void*) lParam);
    }
    /// Geo Node ///
    else if((cItem[1] == "Geometry") || ((cItem[3] == "Geometry") && ((cItem[1] == "Children") || (cItem[1] == "Parent")))){
        Node * node = (Node*) lParam;

        sName << "Geometry > " << Data.Names[node->Head.nNameIndex].sName.c_str();

        Mdl.ConvertToAscii(CONVERT_NODE, sPrint, (void*) &node->Head.nNameIndex);
    }
    /// Controller ///
    else if(cItem[1] == "Controllers"){
        Controller * ctrl = (Controller*) lParam;

        std::string sLocation;
        if(!ctrl->nAnimation.Valid()){
            sLocation = "Geometry > ";
        }
        else{
            sLocation = "Animations > " + std::string(Data.Animations[ctrl->nAnimation].sName.c_str()) + " > ";
        }
        int nNodeIndex = Mdl.GetNodeIndexByNameIndex(ctrl->nNameIndex);
        if(nNodeIndex == -1) throw mdlexception("Vertex normal and tangent space vector calculation error: dealing with a name index that does not have a node in geometry.");
        std::string sController = ReturnControllerName(ctrl->nControllerType, Data.ArrayOfNodes.at(nNodeIndex).Head.nType);
        if(ctrl->nColumnCount & 16) sController += "bezierkey";
        else if(ctrl->nAnimation.Valid()) sController += "key";
        sName << sLocation << Data.Names[ctrl->nNameIndex].sName.c_str() << " > " << sController;
        if(!(cItem[3] == "Geometry")){
            Mdl.ConvertToAscii(CONVERT_CONTROLLER_KEYED, sPrint, (void*) lParam);
        }
        else{
            Mdl.ConvertToAscii(CONVERT_CONTROLLER_SINGLE, sPrint, (void*) lParam);
        }
    }
    else return;

    //Fix newlines
    std::string line;
    std::string text;
    while(std::getline(sPrint, line)){
        line += "\r\n";
        text += line;
    }

    //Create window
    DialogWindow ctrldata;
    if(!ctrldata.Run()){
        std::cout << "DialogWindow creation failed!\n";
    }
    SetWindowText(ctrldata.hMe, sName.str().c_str());
    SetWindowText(GetDlgItem(ctrldata.hMe, IDDB_EDIT), text.c_str());
}
