#include "general.h"
#include "MDL.h"
#include <algorithm>

struct TokenDatum{
    std::string sToken;
    std::string sDataType;
    std::string sFile;
    void * data = nullptr;
    unsigned int nOffset = 0;
    unsigned int nBitflag = 0;
    unsigned int nMaxString = 0;
    int nBytes = 0;
    TokenDatum(){}
    TokenDatum(const std::string & s1, const std::string & s2, const std::string & s3, void * ptr, int n1);
    ~TokenDatum();
};

TokenDatum::TokenDatum(const std::string & s1, const std::string & s2, const std::string & s3, void * ptr, int n1): sToken(s1), sDataType(s2), sFile(s3), data(ptr), nOffset(n1)
{
    if(s2 == "bool" || s2 == "char" || s2 == "unsigned char" || s2 == "signed char") nBytes = 1;
    else if(s2 == "short" || s2 == "unsigned short" || s2 == "signed short") nBytes = 2;
    else if(s2 == "bitflag" || s2 == "double" || s2 == "int" || s2 == "unsigned int" || s2 == "signed int") nBytes = 4;
}

TokenDatum::~TokenDatum(){
}

class EditorDlgWindow: public TextFile{
    WNDCLASSEX WindowClass;
    static char cClassName [];
    static bool bRegistered;
    MDL * MdlPtr = nullptr;

    bool SaveData();

  public:
    HWND hMe;
    EditorDlgWindow();
    bool Run();
    friend LRESULT CALLBACK EditorDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void SetData(MDL & Model){
        MdlPtr = &Model;
    }
    std::vector<TokenDatum> TokenData;
    bool GetTokenData(MDL & Mdl, std::vector<std::string> cItem, LPARAM lParam, std::stringstream & ssName, int nFile);
};

char EditorDlgWindow::cClassName[] = "mdleditordlg";
LRESULT CALLBACK EditorDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool EditorDlgWindow::bRegistered = false;

EditorDlgWindow::EditorDlgWindow(){
    // #1 Basics
    WindowClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    WindowClass.lpszClassName = cClassName; // Name of this class
    WindowClass.hInstance = GetModuleHandle(nullptr); // Instance of the application
    WindowClass.lpfnWndProc = EditorDlgWindowProc; // Pointer to callback procedure

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

bool EditorDlgWindow::Run(){
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

std::vector<EditorDlgWindow> EditDlgs;

LRESULT CALLBACK EditorDlgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    HWND hEdit = GetDlgItem(hwnd, IDDB_EDIT);
    EditorDlgWindow* editdlg = nullptr;
    if(GetWindowLongPtr(hwnd, GWLP_USERDATA) != 0) editdlg = (EditorDlgWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    /* handle the messages */
    switch(message){
        case WM_CREATE:
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*) lParam)->lpCreateParams);
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
        case WM_COMMAND:
        {
            int nID = LOWORD(wParam);
            switch(nID){
                case IDDB_SAVE:
                {
                    if(editdlg == nullptr) Error("Internal error. Cannot get window data.");
                    else{
                        int nTextLength = GetWindowTextLength(hEdit) + 1;
                        //std::cout << "Edit text length: " << nTextLength << "\n";
                        std::vector<char> & sBuffer = editdlg->sBuffer;
                        sBuffer.resize(nTextLength, 0);
                        if(GetWindowText(hEdit, &sBuffer.front(), sBuffer.size()) != 0){
                            ///Passed all the error checks, time to get to business
                            if(editdlg->SaveData()){
                                SendMessage(hFrame, WM_COMMAND, MAKEWPARAM(IDD_EDITOR_DLG, IDP_DISPLAY_UPDATE), (LPARAM) hwnd);
                                //DestroyWindow(hwnd);
                            }
                            SendMessage(hFrame, WM_COMMAND, MAKEWPARAM(IDD_EDITOR_DLG, IDP_DISPLAY_UPDATE), (LPARAM) hwnd);
                        }
                        //else Error("An unknown error occurred! Could not save data!");
                    }
                }
                break;
            }
        }
        break;
        case WM_SIZE:
        {
            SetWindowPos(hEdit, nullptr, rcClient.left+3, rcClient.top, rcClient.right-3, rcClient.bottom, 0);
        }
        break;
        case WM_DESTROY:
            SetWindowText(hEdit, "");
            for(int i = 0; i < static_cast<int>(EditDlgs.size()); i++){
                if(&EditDlgs.at(i) == editdlg) EditDlgs.erase(EditDlgs.begin() + i);
            }
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

bool EditorDlgWindow::GetTokenData(MDL & mdl, std::vector<std::string> sItems, LPARAM lParam, std::stringstream & ssName, int nFile){
    ModelHeader & Data = mdl.GetFileData()->MH;
    ssName << "Editing " << Data.GH.sName.c_str() << " > ";
    if(nFile == 0){
        if(sItems.at(0) == "Header"){
            ssName << "Header";
            ModelHeader & MH = * (ModelHeader*) lParam;
            TokenData.push_back(TokenDatum("supermodel", "string", "MDL", (void*) &MH.cSupermodelName, 148));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("classification", "unsigned char", "MDL", (void*) &MH.nClassification, 92));
            TokenData.push_back(TokenDatum("unknown2", "unsigned char", "MDL", (void*) &MH.nUnknown, 94));
            TokenData.push_back(TokenDatum("animation_scale", "double", "MDL", (void*) &MH.fScale, 144));
            TokenData.push_back(TokenDatum("bmin_x", "double", "MDL", (void*) &MH.vBBmin.fX, 116));
            TokenData.push_back(TokenDatum("bmin_z", "double", "MDL", (void*) &MH.vBBmin.fZ, 124));
            TokenData.push_back(TokenDatum("bmax_y", "double", "MDL", (void*) &MH.vBBmax.fY, 132));
            TokenData.push_back(TokenDatum("bmax_z", "double", "MDL", (void*) &MH.vBBmax.fZ, 136));
            TokenData.push_back(TokenDatum("radius", "double", "MDL", (void*) &MH.fRadius, 140));
        }
        else if(sItems.at(1) == "Animations"){
            Animation & anim = * (Animation*) lParam;
            ssName << "Animations > " << anim.sName.c_str();
            TokenData.push_back(TokenDatum("animation_name", "string", "MDL", (void*) &anim.sName, MDL_OFFSET + anim.nOffset + 8));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("length", "double", "MDL", (void*) &anim.fLength, MDL_OFFSET + anim.nOffset + 80));
            TokenData.push_back(TokenDatum("transition", "double", "MDL", (void*) &anim.fTransition, MDL_OFFSET + anim.nOffset + 84));
            TokenData.push_back(TokenDatum("animroot", "string", "MDL", (void*) &anim.sAnimRoot, MDL_OFFSET + anim.nOffset + 88));
            TokenData.back().nMaxString = 32;
            for(int e = 0; e < static_cast<int>(anim.Events.size()); e++){
                TokenData.push_back(TokenDatum("event_" + std::to_string(e) + "_time", "double", "MDL", (void*) &anim.Events.at(e).fTime, MDL_OFFSET + anim.EventArray.nOffset + e * 36 + 0));
                TokenData.push_back(TokenDatum("event_" + std::to_string(e) + "_name", "string", "MDL", (void*) &anim.Events.at(e).sName, MDL_OFFSET + anim.EventArray.nOffset + e * 36 + 4));
                TokenData.back().nMaxString = 32;
            }
        }
        else if(sItems.at(1) == "Geometry" || (sItems.at(3) == "Geometry" && (sItems.at(1) == "Children" || sItems.at(1) == "Parent"))){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str();
            TokenData.push_back(TokenDatum("position_x", "double", "MDL", (void*) &node.Head.vPos.fX, MDL_OFFSET + node.nOffset + 16));
            TokenData.push_back(TokenDatum("position_y", "double", "MDL", (void*) &node.Head.vPos.fY, MDL_OFFSET + node.nOffset + 20));
            TokenData.push_back(TokenDatum("position_z", "double", "MDL", (void*) &node.Head.vPos.fZ, MDL_OFFSET + node.nOffset + 24));
            TokenData.push_back(TokenDatum("orientation_x", "double", "MDL", (void*) &node.Head.oOrient.GetQuaternion().vAxis.fX, MDL_OFFSET + node.nOffset + 32));
            TokenData.push_back(TokenDatum("orientation_y", "double", "MDL", (void*) &node.Head.oOrient.GetQuaternion().vAxis.fY, MDL_OFFSET + node.nOffset + 36));
            TokenData.push_back(TokenDatum("orientation_z", "double", "MDL", (void*) &node.Head.oOrient.GetQuaternion().vAxis.fZ, MDL_OFFSET + node.nOffset + 40));
            TokenData.push_back(TokenDatum("orientation_w", "double", "MDL", (void*) &node.Head.oOrient.GetQuaternion().fW, MDL_OFFSET + node.nOffset + 28));
        }
        else if(sItems.at(0) == "Light"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Light";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_LIGHT);
            TokenData.push_back(TokenDatum("lightpriority", "int", "MDL", (void*) &node.Light.nLightPriority, MDL_OFFSET + node.nOffset + nHeaderOffset + 64));
            TokenData.push_back(TokenDatum("ambientonly", "int", "MDL", (void*) &node.Light.nAmbientOnly, MDL_OFFSET + node.nOffset + nHeaderOffset + 68));
            TokenData.push_back(TokenDatum("dynamictype", "int", "MDL", (void*) &node.Light.nDynamicType, MDL_OFFSET + node.nOffset + nHeaderOffset + 72));
            TokenData.push_back(TokenDatum("affectdynamic", "int", "MDL", (void*) &node.Light.nAffectDynamic, MDL_OFFSET + node.nOffset + nHeaderOffset + 76));
            TokenData.push_back(TokenDatum("shadow", "int", "MDL", (void*) &node.Light.nShadow, MDL_OFFSET + node.nOffset + nHeaderOffset + 80));
            TokenData.push_back(TokenDatum("flare", "int", "MDL", (void*) &node.Light.nFlare, MDL_OFFSET + node.nOffset + nHeaderOffset + 84));
            TokenData.push_back(TokenDatum("flareradius", "double", "MDL", (void*) &node.Light.fFlareRadius, MDL_OFFSET + node.nOffset + nHeaderOffset + 0));
            TokenData.push_back(TokenDatum("fadinglight", "int", "MDL", (void*) &node.Light.nFadingLight, MDL_OFFSET + node.nOffset + nHeaderOffset + 88));
        }
        else if(sItems.at(1) == "Lens Flares"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Light > " << sItems.at(0);

            /// First let's get the number of the bone from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 11) == "Lens Flare "){
                std::string sNum (safesubstr(sItems.at(0), 11));
                try{
                    nNum = stoi(sNum, (size_t*) NULL);
                }
                catch(...){
                    Error("The label of the lens flare in the tree is not properly formatted!");
                }
            }
            if(nNum.Valid()){
                if(nNum < node.Light.FlareSizes.size()){
                    TokenData.push_back(TokenDatum("size", "double", "MDL", (void*) &node.Light.FlareSizes.at(nNum), MDL_OFFSET + node.Light.FlareSizeArray.nOffset + 4 * nNum + 0));
                }
                if(nNum < node.Light.FlarePositions.size()){
                    TokenData.push_back(TokenDatum("position", "double", "MDL", (void*) &node.Light.FlarePositions.at(nNum), MDL_OFFSET + node.Light.FlarePositionArray.nOffset + 4 * nNum + 0));
                }
                if(nNum < node.Light.FlareColorShifts.size()){
                    TokenData.push_back(TokenDatum("colorshift_r", "double", "MDL", (void*) &node.Light.FlareColorShifts.at(nNum).fR, MDL_OFFSET + node.Light.FlareColorShiftArray.nOffset + 12 * nNum + 0));
                    TokenData.push_back(TokenDatum("colorshift_g", "double", "MDL", (void*) &node.Light.FlareColorShifts.at(nNum).fG, MDL_OFFSET + node.Light.FlareColorShiftArray.nOffset + 12 * nNum + 4));
                    TokenData.push_back(TokenDatum("colorshift_b", "double", "MDL", (void*) &node.Light.FlareColorShifts.at(nNum).fB, MDL_OFFSET + node.Light.FlareColorShiftArray.nOffset + 12 * nNum + 8));
                }
            }
        }
        else if(sItems.at(0) == "Emitter"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Emitter";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_EMITTER);
            TokenData.push_back(TokenDatum("deadspace", "double", "MDL", (void*) &node.Emitter.fDeadSpace, MDL_OFFSET + node.nOffset + nHeaderOffset + 0));
            TokenData.push_back(TokenDatum("blastradius", "double", "MDL", (void*) &node.Emitter.fBlastRadius, MDL_OFFSET + node.nOffset + nHeaderOffset + 4));
            TokenData.push_back(TokenDatum("blastlength", "double", "MDL", (void*) &node.Emitter.fBlastLength, MDL_OFFSET + node.nOffset + nHeaderOffset + 8));
            TokenData.push_back(TokenDatum("branchcount", "unsigned int", "MDL", (void*) &node.Emitter.nBranchCount, MDL_OFFSET + node.nOffset + nHeaderOffset + 12));
            TokenData.push_back(TokenDatum("ctrlptsmoothing", "double", "MDL", (void*) &node.Emitter.fControlPointSmoothing, MDL_OFFSET + node.nOffset + nHeaderOffset + 16));
            TokenData.push_back(TokenDatum("xgrid", "unsigned int", "MDL", (void*) &node.Emitter.nxGrid, MDL_OFFSET + node.nOffset + nHeaderOffset + 20));
            TokenData.push_back(TokenDatum("ygrid", "unsigned int", "MDL", (void*) &node.Emitter.nyGrid, MDL_OFFSET + node.nOffset + nHeaderOffset + 24));
            TokenData.push_back(TokenDatum("spawntype", "unsigned int", "MDL", (void*) &node.Emitter.nSpawnType, MDL_OFFSET + node.nOffset + nHeaderOffset + 28));
            TokenData.push_back(TokenDatum("update", "string", "MDL", (void*) &node.Emitter.cUpdate, MDL_OFFSET + node.nOffset + nHeaderOffset + 32));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("render", "string", "MDL", (void*) &node.Emitter.cRender, MDL_OFFSET + node.nOffset + nHeaderOffset + 64));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("blend", "string", "MDL", (void*) &node.Emitter.cBlend, MDL_OFFSET + node.nOffset + nHeaderOffset + 96));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("texture", "string", "MDL", (void*) &node.Emitter.cTexture, MDL_OFFSET + node.nOffset + nHeaderOffset + 128));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("chunkname", "string", "MDL", (void*) &node.Emitter.cChunkName, MDL_OFFSET + node.nOffset + nHeaderOffset + 160));
            TokenData.back().nMaxString = 16;
            TokenData.push_back(TokenDatum("twosidedtex", "unsigned int", "MDL", (void*) &node.Emitter.nTwosidedTex, MDL_OFFSET + node.nOffset + nHeaderOffset + 176));
            TokenData.push_back(TokenDatum("loop", "unsigned int", "MDL", (void*) &node.Emitter.nLoop, MDL_OFFSET + node.nOffset + nHeaderOffset + 180));
            TokenData.push_back(TokenDatum("renderorder", "unsigned short", "MDL", (void*) &node.Emitter.nRenderOrder, MDL_OFFSET + node.nOffset + nHeaderOffset + 184));
            TokenData.push_back(TokenDatum("frameblending", "unsigned char", "MDL", (void*) &node.Emitter.nFrameBlending, MDL_OFFSET + node.nOffset + nHeaderOffset + 186));
            TokenData.push_back(TokenDatum("depthtexturename", "string", "MDL", (void*) &node.Emitter.cDepthTextureName, MDL_OFFSET + node.nOffset + nHeaderOffset + 187));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("p2p", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_P2P;
            TokenData.push_back(TokenDatum("p2p_sel", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_P2P_SEL;
            TokenData.push_back(TokenDatum("affectedbywind", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_AFFECTED_WIND;
            TokenData.push_back(TokenDatum("istinted", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_TINTED;
            TokenData.push_back(TokenDatum("bounce", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_BOUNCE;
            TokenData.push_back(TokenDatum("random", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_RANDOM;
            TokenData.push_back(TokenDatum("inherit", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_INHERIT;
            TokenData.push_back(TokenDatum("inheritvel", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_INHERIT_VEL;
            TokenData.push_back(TokenDatum("inherit_local", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_INHERIT_LOCAL;
            TokenData.push_back(TokenDatum("splat", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_SPLAT;
            TokenData.push_back(TokenDatum("inherit_part", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_INHERIT_PART;
            TokenData.push_back(TokenDatum("emitterflag12", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_DEPTH_TEXTURE;
            TokenData.push_back(TokenDatum("emitterflag13", "bitflag", "MDL", (void*) &node.Emitter.nFlags, MDL_OFFSET + node.nOffset + nHeaderOffset + 220));
            TokenData.back().nBitflag = EMITTER_FLAG_13;
        }
        else if(sItems.at(0) == "Reference"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Reference";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_REFERENCE);
            TokenData.push_back(TokenDatum("refmodel", "string", "MDL", (void*) &node.Reference.sRefModel, MDL_OFFSET + node.nOffset + nHeaderOffset + 0));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("reattachable", "unsigned int", "MDL", (void*) &node.Reference.nReattachable, MDL_OFFSET + node.nOffset + nHeaderOffset + 32));
        }
        else if(sItems.at(0) == "Mesh"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Mesh";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_MESH);
            TokenData.push_back(TokenDatum("texturenumber", "unsigned short", "MDL", (void*) &node.Mesh.nTextureNumber, MDL_OFFSET + node.nOffset + nHeaderOffset + 306));
            TokenData.push_back(TokenDatum("texture1", "string", "MDL", (void*) &node.Mesh.cTexture1, MDL_OFFSET + node.nOffset + nHeaderOffset + 88));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("texture2", "string", "MDL", (void*) &node.Mesh.cTexture2, MDL_OFFSET + node.nOffset + nHeaderOffset + 120));
            TokenData.back().nMaxString = 32;
            TokenData.push_back(TokenDatum("texture3", "string", "MDL", (void*) &node.Mesh.cTexture3, MDL_OFFSET + node.nOffset + nHeaderOffset + 152));
            TokenData.back().nMaxString = 12;
            TokenData.push_back(TokenDatum("texture4", "string", "MDL", (void*) &node.Mesh.cTexture4, MDL_OFFSET + node.nOffset + nHeaderOffset + 164));
            TokenData.back().nMaxString = 12;
            TokenData.push_back(TokenDatum("lightmapped", "char", "MDL", (void*) &node.Mesh.nHasLightmap, MDL_OFFSET + node.nOffset + nHeaderOffset + 308));
            TokenData.push_back(TokenDatum("rotatetexture", "char", "MDL", (void*) &node.Mesh.nRotateTexture, MDL_OFFSET + node.nOffset + nHeaderOffset + 309));
            TokenData.push_back(TokenDatum("backgroundgeometry", "char", "MDL", (void*) &node.Mesh.nBackgroundGeometry, MDL_OFFSET + node.nOffset + nHeaderOffset + 310));
            TokenData.push_back(TokenDatum("shadow", "char", "MDL", (void*) &node.Mesh.nShadow, MDL_OFFSET + node.nOffset + nHeaderOffset + 311));
            TokenData.push_back(TokenDatum("beaming", "char", "MDL", (void*) &node.Mesh.nBeaming, MDL_OFFSET + node.nOffset + nHeaderOffset + 312));
            TokenData.push_back(TokenDatum("render", "char", "MDL", (void*) &node.Mesh.nRender, MDL_OFFSET + node.nOffset + nHeaderOffset + 313));
            if(mdl.bK2){
                TokenData.push_back(TokenDatum("dirtenabled", "char", "MDL", (void*) &node.Mesh.nDirtEnabled, MDL_OFFSET + node.nOffset + nHeaderOffset + 314));
                TokenData.push_back(TokenDatum("dirttexture", "unsigned short", "MDL", (void*) &node.Mesh.nDirtTexture, MDL_OFFSET + node.nOffset + nHeaderOffset + 316));
                TokenData.push_back(TokenDatum("dirtcoordspace", "unsigned short", "MDL", (void*) &node.Mesh.nDirtCoordSpace, MDL_OFFSET + node.nOffset + nHeaderOffset + 318));
                TokenData.push_back(TokenDatum("hideinholograms", "char", "MDL", (void*) &node.Mesh.nHideInHolograms, MDL_OFFSET + node.nOffset + nHeaderOffset + 320));
            }
            TokenData.push_back(TokenDatum("animateuv", "int", "MDL", (void*) &node.Mesh.nAnimateUV, MDL_OFFSET + node.nOffset + nHeaderOffset + 232));
            TokenData.push_back(TokenDatum("uvdirectionx", "double", "MDL", (void*) &node.Mesh.fUVDirectionX, MDL_OFFSET + node.nOffset + nHeaderOffset + 236));
            TokenData.push_back(TokenDatum("uvdirectiony", "double", "MDL", (void*) &node.Mesh.fUVDirectionY, MDL_OFFSET + node.nOffset + nHeaderOffset + 240));
            TokenData.push_back(TokenDatum("uvjitter", "double", "MDL", (void*) &node.Mesh.fUVJitter, MDL_OFFSET + node.nOffset + nHeaderOffset + 244));
            TokenData.push_back(TokenDatum("uvjitterspeed", "double", "MDL", (void*) &node.Mesh.fUVJitterSpeed, MDL_OFFSET + node.nOffset + nHeaderOffset + 248));
            TokenData.push_back(TokenDatum("transparencyhint", "unsigned int", "MDL", (void*) &node.Mesh.nTransparencyHint, MDL_OFFSET + node.nOffset + nHeaderOffset + 84));
            TokenData.push_back(TokenDatum("diffuse_r", "double", "MDL", (void*) &node.Mesh.fDiffuse.fR, MDL_OFFSET + node.nOffset + nHeaderOffset + 60));
            TokenData.push_back(TokenDatum("diffuse_g", "double", "MDL", (void*) &node.Mesh.fDiffuse.fG, MDL_OFFSET + node.nOffset + nHeaderOffset + 64));
            TokenData.push_back(TokenDatum("diffuse_b", "double", "MDL", (void*) &node.Mesh.fDiffuse.fB, MDL_OFFSET + node.nOffset + nHeaderOffset + 68));
            TokenData.push_back(TokenDatum("ambient_r", "double", "MDL", (void*) &node.Mesh.fAmbient.fR, MDL_OFFSET + node.nOffset + nHeaderOffset + 72));
            TokenData.push_back(TokenDatum("ambient_g", "double", "MDL", (void*) &node.Mesh.fAmbient.fG, MDL_OFFSET + node.nOffset + nHeaderOffset + 76));
            TokenData.push_back(TokenDatum("ambient_b", "double", "MDL", (void*) &node.Mesh.fAmbient.fB, MDL_OFFSET + node.nOffset + nHeaderOffset + 80));
            TokenData.push_back(TokenDatum("inv_counter", "int", "MDL", (void*) &node.Mesh.nMeshInvertedCounter, MDL_OFFSET + node.Mesh.MeshInvertedCounterArray.nOffset));
            TokenData.push_back(TokenDatum("bmin_x", "double", "MDL", (void*) &node.Mesh.vBBmin.fX, MDL_OFFSET + node.nOffset + nHeaderOffset + 20));
            TokenData.push_back(TokenDatum("bmin_y", "double", "MDL", (void*) &node.Mesh.vBBmin.fY, MDL_OFFSET + node.nOffset + nHeaderOffset + 24));
            TokenData.push_back(TokenDatum("bmin_z", "double", "MDL", (void*) &node.Mesh.vBBmin.fZ, MDL_OFFSET + node.nOffset + nHeaderOffset + 28));
            TokenData.push_back(TokenDatum("bmax_x", "double", "MDL", (void*) &node.Mesh.vBBmax.fX, MDL_OFFSET + node.nOffset + nHeaderOffset + 32));
            TokenData.push_back(TokenDatum("bmax_y", "double", "MDL", (void*) &node.Mesh.vBBmax.fY, MDL_OFFSET + node.nOffset + nHeaderOffset + 36));
            TokenData.push_back(TokenDatum("bmax_z", "double", "MDL", (void*) &node.Mesh.vBBmax.fZ, MDL_OFFSET + node.nOffset + nHeaderOffset + 40));
            TokenData.push_back(TokenDatum("radius", "double", "MDL", (void*) &node.Mesh.fRadius, MDL_OFFSET + node.nOffset + nHeaderOffset + 44));
            TokenData.push_back(TokenDatum("average_x", "double", "MDL", (void*) &node.Mesh.vAverage.fX, MDL_OFFSET + node.nOffset + nHeaderOffset + 48));
            TokenData.push_back(TokenDatum("average_y", "double", "MDL", (void*) &node.Mesh.vAverage.fY, MDL_OFFSET + node.nOffset + nHeaderOffset + 52));
            TokenData.push_back(TokenDatum("average_z", "double", "MDL", (void*) &node.Mesh.vAverage.fZ, MDL_OFFSET + node.nOffset + nHeaderOffset + 56));
            TokenData.push_back(TokenDatum("totalarea", "double", "MDL", (void*) &node.Mesh.fTotalArea, MDL_OFFSET + node.nOffset + nHeaderOffset + (mdl.bK2 ? 324 : 316)));
        }
        else if(sItems.at(1) == "Vertices"){
            Vertex & vert = * (Vertex * ) lParam;
            int nNodeIndex = mdl.GetNodeIndexByNameIndex(vert.MDXData.nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("GetTokenData() error: dealing with a name index that does not have a node in geometry.");
            Node & node = Data.ArrayOfNodes.at(nNodeIndex);
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Mesh > Vertices > " << sItems.at(0);

            /// First let's get the number of the vertex from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 7) == "Vertex "){
                std::string sNum (safesubstr(sItems.at(0), 7));
                try{
                    nNum = stoi(sNum,(size_t*) NULL);
                }
                catch(...){
                    Error("The label of the vertex in the tree is not properly formatted!");
                }
            }
            if(nNum.Valid()){
                if(!mdl.bXbox){
                    TokenData.push_back(TokenDatum("mdl_vertex_x", "double", "MDL", (void*) &vert.fX, MDL_OFFSET + node.Mesh.nOffsetToVertArray + 12 * nNum + 0));
                    TokenData.push_back(TokenDatum("mdl_vertex_y", "double", "MDL", (void*) &vert.fY, MDL_OFFSET + node.Mesh.nOffsetToVertArray + 12 * nNum + 4));
                    TokenData.push_back(TokenDatum("mdl_vertex_z", "double", "MDL", (void*) &vert.fZ, MDL_OFFSET + node.Mesh.nOffsetToVertArray + 12 * nNum + 8));
                }
                if(mdl.Mdx){
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_VERTEX){
                        TokenData.push_back(TokenDatum("mdx_vertex_x", "double", "MDX", (void*) &vert.MDXData.vVertex.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxVertex + 0));
                        TokenData.push_back(TokenDatum("mdx_vertex_y", "double", "MDX", (void*) &vert.MDXData.vVertex.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxVertex + 4));
                        TokenData.push_back(TokenDatum("mdx_vertex_z", "double", "MDX", (void*) &vert.MDXData.vVertex.fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxVertex + 8));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_NORMAL){
                        TokenData.push_back(TokenDatum("mdx_normal_x", "double", "MDX", (void*) &vert.MDXData.vNormal.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxNormal + 0));
                        TokenData.push_back(TokenDatum("mdx_normal_y", "double", "MDX", (void*) &vert.MDXData.vNormal.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxNormal + 4));
                        TokenData.push_back(TokenDatum("mdx_normal_z", "double", "MDX", (void*) &vert.MDXData.vNormal.fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxNormal + 8));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV1){
                        TokenData.push_back(TokenDatum("mdx_uv1_x", "double", "MDX", (void*) &vert.MDXData.vUV1.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV1 + 0));
                        TokenData.push_back(TokenDatum("mdx_uv1_y", "double", "MDX", (void*) &vert.MDXData.vUV1.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV1 + 4));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV2){
                        TokenData.push_back(TokenDatum("mdx_uv2_x", "double", "MDX", (void*) &vert.MDXData.vUV2.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV2 + 0));
                        TokenData.push_back(TokenDatum("mdx_uv2_y", "double", "MDX", (void*) &vert.MDXData.vUV2.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV2 + 4));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV3){
                        TokenData.push_back(TokenDatum("mdx_uv3_x", "double", "MDX", (void*) &vert.MDXData.vUV3.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV3 + 0));
                        TokenData.push_back(TokenDatum("mdx_uv3_y", "double", "MDX", (void*) &vert.MDXData.vUV3.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV3 + 4));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV4){
                        TokenData.push_back(TokenDatum("mdx_uv4_x", "double", "MDX", (void*) &vert.MDXData.vUV4.fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV4 + 0));
                        TokenData.push_back(TokenDatum("mdx_uv4_y", "double", "MDX", (void*) &vert.MDXData.vUV4.fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxUV4 + 4));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_COLOR){
                        TokenData.push_back(TokenDatum("mdx_color_r", "double", "MDX", (void*) &vert.MDXData.cColor.fR, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxColor + 0));
                        TokenData.push_back(TokenDatum("mdx_color_g", "double", "MDX", (void*) &vert.MDXData.cColor.fG, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxColor + 4));
                        TokenData.push_back(TokenDatum("mdx_color_b", "double", "MDX", (void*) &vert.MDXData.cColor.fB, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxColor + 8));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1){
                        TokenData.push_back(TokenDatum("mdx_tangent1_tangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(0).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 0));
                        TokenData.push_back(TokenDatum("mdx_tangent1_tangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(0).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 4));
                        TokenData.push_back(TokenDatum("mdx_tangent1_tangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(0).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 8));
                        TokenData.push_back(TokenDatum("mdx_tangent1_bitangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(1).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 12));
                        TokenData.push_back(TokenDatum("mdx_tangent1_bitangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(1).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 16));
                        TokenData.push_back(TokenDatum("mdx_tangent1_bitangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(1).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 20));
                        TokenData.push_back(TokenDatum("mdx_tangent1_normal_x", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(2).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 24));
                        TokenData.push_back(TokenDatum("mdx_tangent1_normal_y", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(2).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 28));
                        TokenData.push_back(TokenDatum("mdx_tangent1_normal_z", "double", "MDX", (void*) &vert.MDXData.vTangent1.at(2).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent1 + 32));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2){
                        TokenData.push_back(TokenDatum("mdx_tangent2_tangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(0).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 0));
                        TokenData.push_back(TokenDatum("mdx_tangent2_tangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(0).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 4));
                        TokenData.push_back(TokenDatum("mdx_tangent2_tangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(0).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 8));
                        TokenData.push_back(TokenDatum("mdx_tangent2_bitangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(1).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 12));
                        TokenData.push_back(TokenDatum("mdx_tangent2_bitangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(1).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 16));
                        TokenData.push_back(TokenDatum("mdx_tangent2_bitangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(1).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 20));
                        TokenData.push_back(TokenDatum("mdx_tangent2_normal_x", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(2).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 24));
                        TokenData.push_back(TokenDatum("mdx_tangent2_normal_y", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(2).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 28));
                        TokenData.push_back(TokenDatum("mdx_tangent2_normal_z", "double", "MDX", (void*) &vert.MDXData.vTangent2.at(2).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent2 + 32));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3){
                        TokenData.push_back(TokenDatum("mdx_tangent3_tangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(0).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 0));
                        TokenData.push_back(TokenDatum("mdx_tangent3_tangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(0).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 4));
                        TokenData.push_back(TokenDatum("mdx_tangent3_tangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(0).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 8));
                        TokenData.push_back(TokenDatum("mdx_tangent3_bitangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(1).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 12));
                        TokenData.push_back(TokenDatum("mdx_tangent3_bitangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(1).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 16));
                        TokenData.push_back(TokenDatum("mdx_tangent3_bitangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(1).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 20));
                        TokenData.push_back(TokenDatum("mdx_tangent3_normal_x", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(2).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 24));
                        TokenData.push_back(TokenDatum("mdx_tangent3_normal_y", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(2).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 28));
                        TokenData.push_back(TokenDatum("mdx_tangent3_normal_z", "double", "MDX", (void*) &vert.MDXData.vTangent3.at(2).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent3 + 32));
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4){
                        TokenData.push_back(TokenDatum("mdx_tangent4_tangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(0).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 0));
                        TokenData.push_back(TokenDatum("mdx_tangent4_tangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(0).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 4));
                        TokenData.push_back(TokenDatum("mdx_tangent4_tangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(0).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 8));
                        TokenData.push_back(TokenDatum("mdx_tangent4_bitangent_x", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(1).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 12));
                        TokenData.push_back(TokenDatum("mdx_tangent4_bitangent_y", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(1).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 16));
                        TokenData.push_back(TokenDatum("mdx_tangent4_bitangent_z", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(1).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 20));
                        TokenData.push_back(TokenDatum("mdx_tangent4_normal_x", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(2).fX, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 24));
                        TokenData.push_back(TokenDatum("mdx_tangent4_normal_y", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(2).fY, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 28));
                        TokenData.push_back(TokenDatum("mdx_tangent4_normal_z", "double", "MDX", (void*) &vert.MDXData.vTangent4.at(2).fZ, node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Mesh.nOffsetToMdxTangent4 + 32));
                    }
                    if(node.Head.nType & NODE_SKIN){
                        TokenData.push_back(TokenDatum("mdx_weight_1", "double", "MDX", (void*) &vert.MDXData.Weights.fWeightValue.at(0), node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Skin.nOffsetToMdxWeightValues + 0));
                        TokenData.push_back(TokenDatum("mdx_weight_2", "double", "MDX", (void*) &vert.MDXData.Weights.fWeightValue.at(1), node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Skin.nOffsetToMdxWeightValues + 4));
                        TokenData.push_back(TokenDatum("mdx_weight_3", "double", "MDX", (void*) &vert.MDXData.Weights.fWeightValue.at(2), node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Skin.nOffsetToMdxWeightValues + 8));
                        TokenData.push_back(TokenDatum("mdx_weight_4", "double", "MDX", (void*) &vert.MDXData.Weights.fWeightValue.at(3), node.Mesh.nOffsetIntoMdx + node.Mesh.nMdxDataSize * nNum + node.Skin.nOffsetToMdxWeightValues + 12));
                    }
                }
            }
        }
        else if(sItems.at(1) == "Faces"){
            Face & face = * (Face*) lParam;
            int nNodeIndex = mdl.GetNodeIndexByNameIndex(face.nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("GetTokenData() error: dealing with a name index that does not have a node in geometry.");
            Node & node = Data.ArrayOfNodes.at(nNodeIndex);
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Mesh > Faces > " << sItems.at(0);

            /// First let's get the number of the bone from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 5) == "Face "){
                std::string sNum (safesubstr(sItems.at(0), 5));
                try{
                    nNum = stoi(sNum,(size_t*) NULL);
                }
                catch(...){
                    Error("The label of the face in the tree is not properly formatted!");
                }
            }
            if(nNum.Valid()){
                TokenData.push_back(TokenDatum("normal_x", "double", "MDL", (void*) &face.vNormal.fX, MDL_OFFSET + node.Mesh.FaceArray.nOffset + 32 * nNum + 0));
                TokenData.push_back(TokenDatum("normal_y", "double", "MDL", (void*) &face.vNormal.fY, MDL_OFFSET + node.Mesh.FaceArray.nOffset + 32 * nNum + 4));
                TokenData.push_back(TokenDatum("normal_z", "double", "MDL", (void*) &face.vNormal.fZ, MDL_OFFSET + node.Mesh.FaceArray.nOffset + 32 * nNum + 8));
                TokenData.push_back(TokenDatum("distance", "double", "MDL", (void*) &face.fDistance, MDL_OFFSET + node.Mesh.FaceArray.nOffset + 32 * nNum + 12));
                TokenData.push_back(TokenDatum("material_id", "int", "MDL", (void*) &face.nMaterialID, MDL_OFFSET + node.Mesh.FaceArray.nOffset + 32 * nNum + 16));
            }
        }
        else if(sItems.at(1) == "Bones"){
            Bone & bone = * (Bone*) lParam;
            int nNodeIndex = mdl.GetNodeIndexByNameIndex(bone.nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("GetTokenData() error: dealing with a name index that does not have a node in geometry.");
            Node & node = Data.ArrayOfNodes.at(nNodeIndex);
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Skin > Bones > " << sItems.at(0);

            /// First let's get the number of the bone from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 5) == "Bone "){
                std::string sNum (safesubstr(sItems.at(0), 5));
                nNum = mdl.GetNameIndex(sNum);
            }
            if(nNum.Valid()){
                TokenData.push_back(TokenDatum("tbone_x", "double", "MDL", (void*) &bone.TBone.fX, MDL_OFFSET + node.Skin.TBoneArray.nOffset + 12 * nNum + 0));
                TokenData.push_back(TokenDatum("tbone_y", "double", "MDL", (void*) &bone.TBone.fY, MDL_OFFSET + node.Skin.TBoneArray.nOffset + 12 * nNum + 4));
                TokenData.push_back(TokenDatum("tbone_z", "double", "MDL", (void*) &bone.TBone.fZ, MDL_OFFSET + node.Skin.TBoneArray.nOffset + 12 * nNum + 8));
                TokenData.push_back(TokenDatum("qbone_x", "double", "MDL", (void*) &bone.QBone.GetQuaternion().vAxis.fX, MDL_OFFSET + node.Skin.QBoneArray.nOffset + 16 * nNum + 0));
                TokenData.push_back(TokenDatum("qbone_y", "double", "MDL", (void*) &bone.QBone.GetQuaternion().vAxis.fY, MDL_OFFSET + node.Skin.QBoneArray.nOffset + 16 * nNum + 4));
                TokenData.push_back(TokenDatum("qbone_z", "double", "MDL", (void*) &bone.QBone.GetQuaternion().vAxis.fZ, MDL_OFFSET + node.Skin.QBoneArray.nOffset + 16 * nNum + 8));
                TokenData.push_back(TokenDatum("qbone_w", "double", "MDL", (void*) &bone.QBone.GetQuaternion().fW, MDL_OFFSET + node.Skin.QBoneArray.nOffset + 16 * nNum + 12));
            }
        }
        else if(sItems.at(0) == "Danglymesh"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Danglymesh";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_DANGLY);
            TokenData.push_back(TokenDatum("displacement", "double", "MDL", (void*) &node.Dangly.fDisplacement, MDL_OFFSET + node.nOffset + nHeaderOffset + 12));
            TokenData.push_back(TokenDatum("tightness", "double", "MDL", (void*) &node.Dangly.fTightness, MDL_OFFSET + node.nOffset + nHeaderOffset + 16));
            TokenData.push_back(TokenDatum("period", "double", "MDL", (void*) &node.Dangly.fPeriod, MDL_OFFSET + node.nOffset + nHeaderOffset + 20));
        }
        else if(sItems.at(1) == "Danglymesh"){
            Node & node = * (Node * ) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Danglymesh > " << sItems.at(0);

            /// First let's get the number of the vertex from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 14) == "Dangly Vertex "){
                std::string sNum (safesubstr(sItems.at(0), 14));
                try{
                    nNum = stoi(sNum,(size_t*) NULL);
                }
                catch(...){
                    Error("The label of the vertex in the tree is not properly formatted!");
                }
            }
            if(nNum.Valid()){
                TokenData.push_back(TokenDatum("dangly_vertex_x", "double", "MDL", (void*) &node.Dangly.Data2.at(nNum).fX, MDL_OFFSET + node.Dangly.nOffsetToData2 + 12 * nNum + 0));
                TokenData.push_back(TokenDatum("dangly_vertex_y", "double", "MDL", (void*) &node.Dangly.Data2.at(nNum).fY, MDL_OFFSET + node.Dangly.nOffsetToData2 + 12 * nNum + 4));
                TokenData.push_back(TokenDatum("dangly_vertex_z", "double", "MDL", (void*) &node.Dangly.Data2.at(nNum).fZ, MDL_OFFSET + node.Dangly.nOffsetToData2 + 12 * nNum + 8));
                TokenData.push_back(TokenDatum("constraint", "double", "MDL", (void*) &node.Dangly.Constraints.at(nNum), MDL_OFFSET + node.Dangly.ConstraintArray.nOffset + 12 * nNum + 0));
            }
        }
        else if(sItems.at(0) == "Lightsaber"){
            Node & node = * (Node*) lParam;
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Lightsaber";
            unsigned nHeaderOffset = mdl.GetHeaderOffset(node, NODE_SABER);
            TokenData.push_back(TokenDatum("inv_counter1", "int", "MDL", (void*) &node.Saber.nInvCount1, MDL_OFFSET + node.nOffset + nHeaderOffset + 12));
            TokenData.push_back(TokenDatum("inv_counter2", "int", "MDL", (void*) &node.Saber.nInvCount2, MDL_OFFSET + node.nOffset + nHeaderOffset + 16));
        }
        else if(sItems.at(1) == "Lightsaber"){
            VertexData & vert = * (VertexData * ) lParam;
            int nNodeIndex = mdl.GetNodeIndexByNameIndex(vert.nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("GetTokenData() error: dealing with a name index that does not have a node in geometry.");
            Node & node = Data.ArrayOfNodes.at(nNodeIndex);
            ssName << "Geometry > " << Data.Names.at(node.Head.nNameIndex).sName.c_str() << " > Lightsaber > " << sItems.at(0);

            /// First let's get the number of the vertex from its name.
            MdlInteger<unsigned int> nNum;
            if(safesubstr(sItems.at(0), 0, 18) == "Lightsaber Vertex "){
                std::string sNum (safesubstr(sItems.at(0), 18));
                try{
                    nNum = stoi(sNum,(size_t*) NULL);
                }
                catch(...){
                    Error("The label of the vertex in the tree is not properly formatted!");
                }
            }
            if(nNum.Valid()){
                TokenData.push_back(TokenDatum("lightsaber_vertex_x", "double", "MDL", (void*) &vert.vVertex.fX, MDL_OFFSET + node.Saber.nOffsetToSaberVerts + 12 * nNum + 0));
                TokenData.push_back(TokenDatum("lightsaber_vertex_y", "double", "MDL", (void*) &vert.vVertex.fY, MDL_OFFSET + node.Saber.nOffsetToSaberVerts + 12 * nNum + 4));
                TokenData.push_back(TokenDatum("lightsaber_vertex_z", "double", "MDL", (void*) &vert.vVertex.fZ, MDL_OFFSET + node.Saber.nOffsetToSaberVerts + 12 * nNum + 8));
                TokenData.push_back(TokenDatum("lightsaber_normal_x", "double", "MDL", (void*) &vert.vNormal.fX, MDL_OFFSET + node.Saber.nOffsetToSaberNormals + 12 * nNum + 0));
                TokenData.push_back(TokenDatum("lightsaber_normal_y", "double", "MDL", (void*) &vert.vNormal.fY, MDL_OFFSET + node.Saber.nOffsetToSaberNormals + 12 * nNum + 4));
                TokenData.push_back(TokenDatum("lightsaber_normal_z", "double", "MDL", (void*) &vert.vNormal.fZ, MDL_OFFSET + node.Saber.nOffsetToSaberNormals + 12 * nNum + 8));
                TokenData.push_back(TokenDatum("lightsaber_uv1_x"   , "double", "MDL", (void*) &vert.vUV1.fX, MDL_OFFSET + node.Saber.nOffsetToSaberUVs + 8 * nNum + 0));
                TokenData.push_back(TokenDatum("lightsaber_uv1_y"   , "double", "MDL", (void*) &vert.vUV1.fY, MDL_OFFSET + node.Saber.nOffsetToSaberUVs + 8 * nNum + 4));
            }
        }
        else if(sItems.at(1) == "Controllers"){
            Controller & ctrl = * (Controller *) lParam;
            int nNodeIndex = mdl.GetNodeIndexByNameIndex(ctrl.nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("GetTokenData() error: dealing with a name index that does not have a node in geometry.");
            Node & geonode = Data.ArrayOfNodes.at(nNodeIndex);
            Node * tempNode = nullptr;
            if(ctrl.nAnimation.Valid()){
                Animation & anim = Data.Animations.at(ctrl.nAnimation);
                for(int an = 0; an < static_cast<int>(anim.ArrayOfNodes.size()) && tempNode == nullptr; an++){
                    Node & animNode = anim.ArrayOfNodes.at(an);
                    if(ctrl.nNameIndex == animNode.Head.nNameIndex){
                        for(int ac = 0; ac < static_cast<int>(animNode.Head.Controllers.size()) && tempNode == nullptr; ac++){
                            if(&animNode.Head.Controllers.at(ac) == &ctrl) tempNode = &animNode;
                        }
                    }
                }
            }
            else tempNode = &geonode;
            Node & node = *tempNode;
            ssName << "Geometry > " << Data.Names.at(ctrl.nNameIndex).sName.c_str() << " > Controllers > " << sItems.at(0);

            if(ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION && ctrl.nColumnCount % 15 == 2){
                Error("Compressed quaternion values not supported yet!");
                return false;
            }

            std::string sCtrlName = ReturnControllerName(ctrl.nControllerType, geonode.Head.nType);
            ToLowerInPlace(sCtrlName);

            std::vector<std::string> sComponents;
            if(sCtrlName == "position"){
                sComponents.push_back("_x");
                sComponents.push_back("_y");
                sComponents.push_back("_z");
            }
            else if(sCtrlName == "selfillumcolor" || sCtrlName == "colorstart" || sCtrlName == "colormid" || sCtrlName == "colorend" || sCtrlName == "color"){
                sComponents.push_back("_r");
                sComponents.push_back("_g");
                sComponents.push_back("_b");
            }
            else if(sCtrlName == "orientation"&& ctrl.nColumnCount % 15 == 4){
                sComponents.push_back("_x");
                sComponents.push_back("_y");
                sComponents.push_back("_z");
                sComponents.push_back("_w");
            }
            if(ctrl.nAnimation.Valid()){
                for(int t = 0; t < ctrl.nValueCount; t++){
                    TokenData.push_back(TokenDatum("timekey" + ("_" + std::to_string(t)), "double", "MDL", (void*) &node.Head.ControllerData.at(ctrl.nTimekeyStart + t), MDL_OFFSET + node.Head.ControllerDataArray.nOffset + (ctrl.nTimekeyStart + t) * 4));
                }
            }
            for(int t = 0; t < ctrl.nValueCount; t++){
                int columns = ctrl.nColumnCount & 15;
                bool bBezier = ctrl.nColumnCount & 16;
                for(int n = 0; n < columns; n++){
                    TokenData.push_back(TokenDatum(sCtrlName + (ctrl.nAnimation.Valid() ? "_" + std::to_string(t) : "") + (columns > 1 ? sComponents.at(n) : ""), "double", "MDL",
                                                   (void*) &node.Head.ControllerData.at(ctrl.nDataStart + t * (bBezier ? columns * 3 : columns) + n),
                                                   MDL_OFFSET + node.Head.ControllerDataArray.nOffset + (ctrl.nDataStart + t * (bBezier ? columns * 3 : columns) + n) * 4));
                }
                if(bBezier){
                    for(int n = 0; n < columns; n++){
                        TokenData.push_back(TokenDatum(sCtrlName + (ctrl.nValueCount > 1 ? "_" + std::to_string(t) : "") + "_in" + (columns > 1 ? sComponents.at(n) : ""), "double", "MDL",
                                                       (void*) &node.Head.ControllerData.at(ctrl.nDataStart + t * columns * 3 + n + columns * 1),
                                                       MDL_OFFSET + node.Head.ControllerDataArray.nOffset + (ctrl.nDataStart + t * columns * 3 + n + columns * 1) * 4));
                    }
                    for(int n = 0; n < columns; n++){
                        TokenData.push_back(TokenDatum(sCtrlName + (ctrl.nValueCount > 1 ? "_" + std::to_string(t) : "") + "_out" + (columns > 1 ? sComponents.at(n) : ""), "double", "MDL",
                                                       (void*) &node.Head.ControllerData.at(ctrl.nDataStart + t * columns * 3 + n + columns * 2),
                                                       MDL_OFFSET + node.Head.ControllerDataArray.nOffset + (ctrl.nDataStart + t * columns * 3 + n + columns * 2) * 4));
                    }
                }
            }
        }
    }
    else{
        BWMHeader * p_bwmh = nullptr;
        std::string sBinFile;
        if(nFile == 1 && mdl.Wok){
            sBinFile = "WOK";
            p_bwmh = mdl.Wok->GetData().get();
        }
        else if(nFile == 2 && mdl.Pwk){
            sBinFile = "PWK";
            p_bwmh = mdl.Pwk->GetData().get();
        }
        else if(nFile == 3 && mdl.Dwk0){
            sBinFile = "DWK0";
            p_bwmh = mdl.Dwk0->GetData().get();
        }
        else if(nFile == 4 && mdl.Dwk1){
            sBinFile = "DWK1";
            p_bwmh = mdl.Dwk1->GetData().get();
        }
        else if(nFile == 5 && mdl.Dwk2){
            sBinFile = "DWK2";
            p_bwmh = mdl.Dwk2->GetData().get();
        }
        BWMHeader & bwm = *p_bwmh;

        ssName << sBinFile << " > ";

        if(sItems.at(0) == "Header"){
            ssName << "Header";
            TokenData.push_back(TokenDatum("use_hook_1_x", "double", sBinFile, (void*) &bwm.vUse1.fX, 12));
            TokenData.push_back(TokenDatum("use_hook_1_y", "double", sBinFile, (void*) &bwm.vUse1.fY, 16));
            TokenData.push_back(TokenDatum("use_hook_4_z", "double", sBinFile, (void*) &bwm.vDwk2.fZ, 56));
        }
    }

    return true;
}

void OpenEditorDlg(MDL & Mdl, std::vector<std::string> cItem, LPARAM lParam, int nFile){
    EditDlgs.reserve(10);
    if(EditDlgs.size() >= 10) return; /// Don't open a new window if we have 10 already

    std::stringstream sName;
    std::stringstream sPrint;

    sPrint.precision(7);
    sPrint << std::fixed;

    if(cItem[0] == "") return;

    EditorDlgWindow EDW;
    if(!EDW.GetTokenData(Mdl, cItem, lParam, sName, nFile)) return;

    /// Write out window contents
    for(int t = 0; t < static_cast<int>(EDW.TokenData.size()); t++){
        TokenDatum & token = EDW.TokenData.at(t);
        sPrint << token.sToken << " ";
        if(token.sDataType == "bool") sPrint << (* (bool*) token.data ? 1 : 0);
        else if(token.sDataType == "char") sPrint << (int) * (char*) token.data;
        else if(token.sDataType == "unsigned char") sPrint << (int) * (unsigned char*) token.data;
        else if(token.sDataType == "signed char") sPrint << (int) * (signed char*) token.data;
        else if(token.sDataType == "short") sPrint << * (short*) token.data;
        else if(token.sDataType == "unsigned short") sPrint << * (unsigned short*) token.data;
        else if(token.sDataType == "signed short") sPrint << * (signed short*) token.data;
        else if(token.sDataType == "int") sPrint << * (int*) token.data;
        else if(token.sDataType == "unsigned int") sPrint << * (unsigned int*) token.data;
        else if(token.sDataType == "signed int") sPrint << * (signed int*) token.data;
        else if(token.sDataType == "double") sPrint << PrepareFloat(* (double*) token.data);
        else if(token.sDataType == "string") sPrint << ((std::string*) token.data)->c_str();
        else if(token.sDataType == "bitflag") sPrint << ((* (unsigned int*) token.data) & token.nBitflag ? 1 : 0);
        if(t + 1 < static_cast<int>(EDW.TokenData.size())) sPrint << "\r\n";
        else sPrint << '\0';
    }

    EditDlgs.push_back(std::move(EDW));
    EditorDlgWindow & editdlg = EditDlgs.back();
    if(!editdlg.Run()){
        std::cout << "EditorDlgWindow creation failed!\n";
    }
    else{
        editdlg.SetData(Mdl);
        SetWindowText(editdlg.hMe, sName.str().c_str());
        SetWindowText(GetDlgItem(editdlg.hMe, IDDB_EDIT), sPrint.str().c_str());
    }
}

bool EditorDlgWindow::SaveData(){
    MDL & Mdl = *MdlPtr;
    std::string sID;
    std::string sGet;
    std::stringstream ssCompare;
    ssCompare.precision(7);
    ssCompare << std::fixed;

    nPosition = 0;
    bool bError = false;
    double fConvert = 0.0;
    int nConvert = 0;
    unsigned int uConvert = 0;


    while(nPosition < sBuffer.size()){
        if(EmptyRow()){
            SkipLine();
            continue;
        }

        bool bFound = ReadUntilText(sID);
        if(!bFound){
            SkipLine();
            continue;
        }

        ToLowerInPlace(sID);

        for(int t = 0; t < static_cast<int>(TokenData.size()); t++){
            TokenDatum & token = TokenData.at(t);
            if(token.sToken != sID) continue;

            bool bWrite = false;
          try{
            if(token.nBytes == 1){
                char cByte = 0;
                if(token.sDataType == "bool"){
                    bool * bVariable = (bool*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *bVariable = nConvert == 0 ? false : true;
                        /// Normalize binary output to 0|1
                        cByte = *bVariable ? 1 : 0; //nConvert;
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "char"){
                    char * cVariable = (char*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *cVariable = nConvert;
                        cByte = *cVariable;
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "unsigned char"){
                    unsigned char * cVariable = (unsigned char*) token.data;
                    if(ReadUInt(uConvert, &sGet)){
                        *cVariable = uConvert;
                        cByte = *cVariable;
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "signed char"){
                    signed char * cVariable = (signed char*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *cVariable = nConvert;
                        cByte = *cVariable;
                        bWrite = true;
                    }
                }
                if(bWrite){
                    if(token.sFile == "MDL") Mdl.WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "MDX" && Mdl.Mdx) Mdl.Mdx->WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "WOK" && Mdl.Wok) Mdl.Wok->WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "PWK" && Mdl.Pwk) Mdl.Pwk->WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK0" && Mdl.Dwk0) Mdl.Dwk0->WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK1" && Mdl.Dwk1) Mdl.Dwk1->WriteNumber(&cByte, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK2" && Mdl.Dwk2) Mdl.Dwk2->WriteNumber(&cByte, 0, "", &token.nOffset);
                }
            }
            if(token.nBytes == 2){
                unsigned short ph_bb2 = 0;
                if(token.sDataType == "short"){
                    short * nVariable = (short*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *nVariable = nConvert;
                        std::memcpy(&ph_bb2, nVariable, sizeof(ph_bb2));
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "unsigned short"){
                    unsigned short * nVariable = (unsigned short*) token.data;
                    if(ReadUInt(uConvert, &sGet)){
                        *nVariable = uConvert;
                        std::memcpy(&ph_bb2, nVariable, sizeof(ph_bb2));
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "signed short"){
                    signed short * nVariable = (signed short*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *nVariable = nConvert;
                        std::memcpy(&ph_bb2, nVariable, sizeof(ph_bb2));
                        bWrite = true;
                    }
                }

                if(bWrite){
                    if(token.sFile == "MDL") Mdl.WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "MDX" && Mdl.Mdx) Mdl.Mdx->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "WOK" && Mdl.Wok) Mdl.Wok->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "PWK" && Mdl.Pwk) Mdl.Pwk->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK0" && Mdl.Dwk0) Mdl.Dwk0->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK1" && Mdl.Dwk1) Mdl.Dwk1->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK2" && Mdl.Dwk2) Mdl.Dwk2->WriteNumber(&ph_bb2, 0, "", &token.nOffset);
                }
            }
            if(token.nBytes == 4){
                unsigned ph_bb4 = 0;
                if(token.sDataType == "int"){
                    int * nVariable = (int*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *nVariable = nConvert;
                        ph_bb4 = static_cast<unsigned>(*nVariable);
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "unsigned int"){
                    unsigned int * nVariable = (unsigned int*) token.data;
                    if(ReadUInt(uConvert, &sGet)){
                        *nVariable = uConvert;
                        ph_bb4 = static_cast<unsigned>(*nVariable);
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "signed int"){
                    signed int * nVariable = (signed int*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        *nVariable = nConvert;
                        ph_bb4 = static_cast<unsigned>(*nVariable);
                        bWrite = true;
                    }
                }
                else if(token.sDataType == "double"){
                    double * fVariable = (double*) token.data;
                    if(ReadFloat(fConvert, &sGet)){
                        ssCompare << (double*) token.data;
                        if(ssCompare.str() != sGet){
                            *fVariable = fConvert;
                            float fPH = (float) fConvert;
                            ph_bb4 = FloatBits(fPH);
                            bWrite = true;
                        }
                    }
                }
                else if(token.sDataType == "bitflag"){
                    unsigned int * nVariable = (unsigned int*) token.data;
                    if(ReadInt(nConvert, &sGet)){
                        if(nConvert != 0) *nVariable = *nVariable | token.nBitflag;
                        else *nVariable = *nVariable & (~token.nBitflag);
                        ph_bb4 = static_cast<unsigned>(*nVariable);
                        bWrite = true;
                    }
                }

                if(bWrite){
                    if(token.sFile == "MDL") Mdl.WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "MDX" && Mdl.Mdx) Mdl.Mdx->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "WOK" && Mdl.Wok) Mdl.Wok->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "PWK" && Mdl.Pwk) Mdl.Pwk->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK0" && Mdl.Dwk0) Mdl.Dwk0->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK1" && Mdl.Dwk1) Mdl.Dwk1->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                    else if(token.sFile == "DWK2" && Mdl.Dwk2) Mdl.Dwk2->WriteNumber(&ph_bb4, 0, "", &token.nOffset);
                }
            }
            else if(token.sDataType == "string"){
                std::string * sVariable = (std::string*) token.data;
                if(ReadUntilText(sGet, false)){
                    sGet.resize(token.nMaxString);
                    *sVariable = sGet;
                    if(token.sFile == "MDL") Mdl.WriteString(&sGet, sGet.size(), 0, "", &token.nOffset);
                }
            }
          }
          catch(const std::invalid_argument &){
            Error("The value " + sGet + " is invalid for " + token.sToken + ".");
                bError = true;
          }
          catch(const std::out_of_range &){
            Error("The value " + sGet + " for " + token.sToken + " is out of range (data type: " + token.sDataType + ").");
                bError = true;
          }
          catch(...){
            Error("An unknown exception occurred while recording the value " + sGet + " for " + token.sToken + " (data type: " + token.sDataType + ").");
                bError = true;
          }

            SkipLine();
        }
    }
    if(bError) return false;
    else return true;
    return false;
}
