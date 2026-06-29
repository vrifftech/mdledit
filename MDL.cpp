#include "MDL.h"
#include <atomic>
#include <fstream>
#include <algorithm>
#include <shlwapi.h>
#include <cstring>
#include <cmath>
/// This file should be a general initializer/implementor of MDL.h


VertexData::VertexData(){}
VertexData::VertexData(VertexData &&) = default;

VertexData::VertexData(const Vector & v1, const Vector & v2, int nName):
    vVertex(v1), vUV1(v2), nNameIndex(static_cast<unsigned short>(nName)) {}

Face::Face(Face &&) = default;
Aabb::~Aabb() = default;
Patch::~Patch() = default;
MeshHeader::~MeshHeader() = default;
Animation::Animation() = default;
Animation::~Animation() = default;
BWMHeader::BWMHeader() = default;

Node::Node() = default;
Node::Node(const Node &) = default;
Node::Node(Node &&) = default;
Node & Node::operator=(Node &&) = default;
Node::~Node() = default;

void MDL::Report(std::string sMessage){
    if(PtrReport != nullptr) PtrReport(sMessage);
}

MDL::~MDL(){}
void MDL::ProgressSize(int nMin, int nMax){
    if(PtrProgressSize != nullptr) PtrProgressSize(nMin, nMax);
}
void MDL::ProgressPos(int nPos){
    if(PtrProgressPos != nullptr) PtrProgressPos(nPos);
}

int MDL::GetNameIndex(const std::string & sName){
    if(!FH) throw mdlexception("MDL::GetNameIndex() ERROR: File header is not available.");
    std::vector<Name> & Names = FH->MH.Names;
    int nReturn = -1;
    for(int n = 0; n < static_cast<int>(Names.size()); n++){
        if(StringEqual(Names[n].sName, sName)){
            nReturn = n;
            break;
        }
    }
    return nReturn;
}


std::string & MDL::GetNodeName(Node & node){
    if(!FH) throw mdlexception("MDL::GetNodeName() ERROR: File header is not available.");
    if(!node.Head.nNameIndex.Valid() || node.Head.nNameIndex >= FH->MH.Names.size())
        throw mdlexception("MDL::GetNodeName() ERROR: Name index is too large to find a name.");
    return FH->MH.Names.at(node.Head.nNameIndex).sName;
}

extern MDL Model;

unsigned int GetFunctionPointer(unsigned int FN_PTR, int n, bool bXbox, bool bK2){
    if(bXbox){
        if(bK2){
            if(n == 1){
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_XBOX_K2_MODEL_1;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_XBOX_K2_ANIM_1;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_XBOX_K2_MESH_1;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_XBOX_K2_SKIN_1;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_XBOX_K2_DANGLY_1;
            }
            else{
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_XBOX_K2_MODEL_2;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_XBOX_K2_ANIM_2;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_XBOX_K2_MESH_2;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_XBOX_K2_SKIN_2;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_XBOX_K2_DANGLY_2;
            }
        }
        else{
            if(n == 1){
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_XBOX_K1_MODEL_1;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_XBOX_K1_ANIM_1;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_XBOX_K1_MESH_1;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_XBOX_K1_SKIN_1;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_XBOX_K1_DANGLY_1;
            }
            else{
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_XBOX_K1_MODEL_2;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_XBOX_K1_ANIM_2;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_XBOX_K1_MESH_2;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_XBOX_K1_SKIN_2;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_XBOX_K1_DANGLY_2;
            }
        }
    }
    else{
        if(bK2){
            if(n == 1){
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_PC_K2_MODEL_1;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_PC_K2_ANIM_1;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_PC_K2_MESH_1;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_PC_K2_SKIN_1;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_PC_K2_DANGLY_1;
            }
            else{
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_PC_K2_MODEL_2;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_PC_K2_ANIM_2;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_PC_K2_MESH_2;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_PC_K2_SKIN_2;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_PC_K2_DANGLY_2;
            }
        }
        else{
            if(n == 1){
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_PC_K1_MODEL_1;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_PC_K1_ANIM_1;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_PC_K1_MESH_1;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_PC_K1_SKIN_1;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_PC_K1_DANGLY_1;
            }
            else{
                if(FN_PTR == FN_PTR_MODEL) return FN_PTR_PC_K1_MODEL_2;
                else if(FN_PTR == FN_PTR_ANIM) return FN_PTR_PC_K1_ANIM_2;
                else if(FN_PTR == FN_PTR_MESH) return FN_PTR_PC_K1_MESH_2;
                else if(FN_PTR == FN_PTR_SKIN) return FN_PTR_PC_K1_SKIN_2;
                else if(FN_PTR == FN_PTR_DANGLY) return FN_PTR_PC_K1_DANGLY_2;
            }
        }
    }
    return -1;
}

unsigned int MDL::FunctionPointer1(unsigned int FN_PTR){
    return GetFunctionPointer(FN_PTR, 1, bXbox, bK2);
}

unsigned int MDL::FunctionPointer2(unsigned int FN_PTR){
    return GetFunctionPointer(FN_PTR, 2, bXbox, bK2);
}

std::vector<Vertex> MDL::GetWokVertData(Node & node){
    std::vector<Vertex> verts_wok;
    if(!(Wok && Wok->GetData())) return verts_wok;
    BWMHeader & data = *Wok->GetData();
    Vector vLyt;
    if(FH) vLyt = FH->MH.vLytPosition;
    if(data.faces.size() != node.Mesh.Faces.size()) return verts_wok;

    /// All the checks have passed, get to business. First, build a vector of Faces in the order of the wok.
    std::vector<Face> faces_wok;
    std::vector<Face> unwalkable;
    for(std::size_t f = 0; f < node.Mesh.Faces.size(); f++){
        Face & face = node.Mesh.Faces.at(f);
        if(IsMaterialWalkable(face.nMaterialID)) faces_wok.push_back(face);
        else unwalkable.push_back(face);
    }
    for(std::size_t f = 0; f < unwalkable.size(); f++){
        faces_wok.push_back(unwalkable.at(f));
    }

    /// Preserve the original vertex payload by default. Only overwrite the
    /// positions that can be mapped unambiguously from the WOK. Returning an
    /// empty vector means the WOK data cannot be represented without risking a
    /// malformed ASCII mesh.
    verts_wok = node.Mesh.Vertices;
    std::vector<bool> bAssigned(verts_wok.size(), false);
    for(std::size_t f = 0; f < node.Mesh.Faces.size(); f++){
        for(int i = 0; i < 3; i++){
            const auto & nMdlVert = faces_wok.at(f).nIndexVertex.at(i);
            const auto & nWokVert = data.faces.at(f).nIndexVertex.at(i);
            if(!nMdlVert.Valid() || !nWokVert.Valid()) return std::vector<Vertex>();

            const long long nMdlVertIndex = nMdlVert;
            const long long nWokVertIndex = nWokVert;
            if(nMdlVertIndex < 0 || nWokVertIndex < 0 ||
               static_cast<std::size_t>(nMdlVertIndex) >= verts_wok.size() ||
               static_cast<std::size_t>(nWokVertIndex) >= data.verts.size()){
                return std::vector<Vertex>();
            }

            const std::size_t nMdlIndex = static_cast<std::size_t>(nMdlVertIndex);
            const Vector vMapped = data.verts.at(static_cast<std::size_t>(nWokVertIndex)) - vLyt - node.Head.vFromRoot;
            if(bAssigned.at(nMdlIndex) && !verts_wok.at(nMdlIndex).MDXData.vVertex.Compare(vMapped, 0.00001)){
                return std::vector<Vertex>();
            }

            verts_wok.at(nMdlIndex).assign(vMapped, true);
            bAssigned.at(nMdlIndex) = true;
        }
    }
    return verts_wok;
}

void MDL::GetLytPositionFromWok(){
    if(!Wok) return;
    if(!FH) return;
    if(!Wok->GetData()) return;
    FileHeader & Data = *FH;
    BWMHeader & data = *Wok->GetData();
    if(data.faces.empty()) return;
    if(data.verts.empty()) return;
    if(!data.faces.front().nIndexVertex.at(0).Valid()) return;

    const unsigned int nWokVertIndex = data.faces.front().nIndexVertex.at(0);
    if(nWokVertIndex >= data.verts.size()) return;

    const unsigned int nSearchMaterial = data.faces.front().nMaterialID;
    for(Node & node : Data.MH.ArrayOfNodes){
        if(node.Head.nType & NODE_AABB){
            for(Face & face : node.Mesh.Faces){
                if(face.nMaterialID == nSearchMaterial){
                    if(!face.nIndexVertex.at(0).Valid()) return;
                    const unsigned int nMdlVertIndex = face.nIndexVertex.at(0);
                    if(nMdlVertIndex >= node.Mesh.Vertices.size()) return;

                    const Vector & wokVert = data.verts.at(nWokVertIndex);
                    const Vector & mdlVert = node.Mesh.Vertices.at(nMdlVertIndex).vFromRoot;
                    Data.MH.vLytPosition =
                        Vector(wokVert.fX - mdlVert.fX,
                               wokVert.fY - mdlVert.fY,
                               wokVert.fZ - mdlVert.fZ);
                    return;
                }
            }
            return;
        }
    }
}

std::unique_ptr<FileHeader> & MDL::GetFileData(){
    return FH;
}

std::string MDL::MakeUniqueName(int nNameIndex){
    if(!FH) throw mdlexception("MDL::MakeUniqueName() failed because we're running it on a model without FileHeader data.");
    std::vector<Name> & Names = FH->MH.Names;
    std::string sReturn = Names.at(nNameIndex).sName.c_str();
    std::vector<Node> & Nodes = FH->MH.ArrayOfNodes;
    if(Nodes.size() > static_cast<unsigned>(nNameIndex)){
        if(Nodes.at(nNameIndex).Head.nType & NODE_SABER && bLightsaberToTrimesh) sReturn = "2081__" + sReturn;
    }
    for(int n = 0; n < nNameIndex; n++){
        if(StringEqual(Names.at(n).sName, sReturn, true)) return sReturn + "__dpl" + std::to_string(static_cast<int>(nNameIndex));
    }
    return sReturn;
}

int MDL::GetNodeIndexByNameIndex(int nIndex, int nAnimation){
    if(!FH)
        throw mdlexception("MDL::GetNodeIndexByNameIndex() ERROR: File header not available.");
    FileHeader & Data = *FH;

    if(nIndex < 0 || static_cast<unsigned>(nIndex) >= Data.MH.Names.size()){
        std::string sMax = Data.MH.Names.empty() ? "none" : std::to_string(Data.MH.Names.size() - 1);
        throw mdlexception("MDL::GetNodeIndexByNameIndex() ERROR: The index " + std::to_string(nIndex) + " lies outside the range of valid indices (0-" + sMax + ").");
    }

    int nReturn = -1;
    if(nAnimation == -1){
        for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
            if(Data.MH.ArrayOfNodes.at(n).Head.nNameIndex == nIndex){
                nReturn = n;
                break;
            }
        }
    }
    else{
        for(int n = 0; n < static_cast<int>(Data.MH.Animations.at(nAnimation).ArrayOfNodes.size()); n++){
            if(Data.MH.Animations.at(nAnimation).ArrayOfNodes.at(n).Head.nNameIndex == nIndex){
                nReturn = n;
                break;
            }
        }
    }
    return nReturn;
}

bool MDL::HeadLinked(){
    if(FH->MH.GH.nOffsetToRootNode != FH->MH.nOffsetToHeadRootNode) return true;
    return false;
}

bool MDL::NodeExists(const std::string & sNodeName){
    if(GetNameIndex(sNodeName) != -1) return true;
    return false;
}

namespace {
    void BuildTreeOrderArrayImpl(ModelHeader & header, Node & node, std::vector<unsigned short> & active, std::vector<unsigned short> & visited){
        if(!node.Head.nNameIndex.Valid() || static_cast<unsigned short>(node.Head.nNameIndex) >= header.Names.size()){
            throw mdlexception("BuildTreeOrderArray() error, node name index is invalid or out of scope!");
        }
        const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
        if(std::find(active.begin(), active.end(), nNameIndex) != active.end()){
            throw mdlexception("BuildTreeOrderArray() error, cyclic child hierarchy detected!");
        }
        if(std::find(visited.begin(), visited.end(), nNameIndex) != visited.end()){
            throw mdlexception("BuildTreeOrderArray() error, hierarchy reaches the same node more than once!");
        }

        active.push_back(nNameIndex);
        visited.push_back(nNameIndex);
        header.NameIndicesInTreeOrder.push_back(nNameIndex);

        std::vector<unsigned short> localChildren;
        localChildren.reserve(node.Head.ChildIndices.size());
        for(auto n : node.Head.ChildIndices){
            if(!n.Valid()) throw mdlexception("BuildTreeOrderArray() error, child name index is invalid!");
            if(static_cast<unsigned short>(n) >= header.Names.size()) throw mdlexception("BuildTreeOrderArray() error, child name index out of scope!");
            const unsigned short nChildIndex = static_cast<unsigned short>(n);
            if(nChildIndex == nNameIndex) throw mdlexception("BuildTreeOrderArray() error, node is listed as its own child!");
            if(std::find(localChildren.begin(), localChildren.end(), nChildIndex) != localChildren.end()){
                throw mdlexception("BuildTreeOrderArray() error, duplicate child name index under one parent!");
            }
            localChildren.push_back(nChildIndex);

            bool bFound = false;
            for(Node & child : header.ArrayOfNodes){
                if(child.Head.nNameIndex == n){
                    bFound = true;
                    BuildTreeOrderArrayImpl(header, child, active, visited);
                    break;
                }
            }
            if(!bFound){
                throw mdlexception("BuildTreeOrderArray() error, child name index does not resolve to a node!");
            }
        }

        active.pop_back();
    }
}

void ModelHeader::BuildTreeOrderArray(Node & node){
    std::vector<unsigned short> oldOrder = NameIndicesInTreeOrder;
    std::vector<unsigned short> newOrder;
    std::vector<unsigned short> active;
    std::vector<unsigned short> visited;

    NameIndicesInTreeOrder.swap(newOrder);
    try{
        BuildTreeOrderArrayImpl(*this, node, active, visited);
        oldOrder = NameIndicesInTreeOrder;
    }
    catch(...){
        NameIndicesInTreeOrder.swap(oldOrder);
        throw;
    }
}

std::stringstream & MDL::GetReport(){
    return ssReport;
}

void MDL::SaveReport(){
    std::wstring sFile = GetFullPath();
    if(safesubstr(sFile, sFile.size() - 6, 6) == L".ascii") sFile = safesubstr(sFile, 0, sFile.size() - 6);
    if(safesubstr(sFile, sFile.size() - 4, 4) == L".mdl") sFile = safesubstr(sFile, 0, sFile.size() - 4);
    sFile += L"_report.txt";

    /// Create file
    HANDLE hFile = bead_CreateWriteFile(sFile);

    if(hFile == INVALID_HANDLE_VALUE){
        std::cout << "File creation failed for " << sFile.c_str() << ". Aborting.\n";
        return;
    }

    /// Write and close file
    if(!bead_WriteFile(hFile, ssReport.str())){
        std::cout << "File write failed for " << to_ansi(sFile) << ".\n";
    }
    CloseHandle(hFile);
}

void MDL::FlushData(){
    FH.reset();
    Ascii.reset();
    Mdx.reset();
    Wok.reset();
    Pwk.reset();
    Dwk0.reset();
    Dwk1.reset();
    Dwk2.reset();
    PwkAscii.reset();
    DwkAscii.reset();
    FlushAll();
    ClearStringstream(ssReport);

    src = NoSource;
}

//Setters/general
bool MDL::LinkHead(bool bLink){
    unsigned int nOffset;
    if(bLink){
        MdlInteger<unsigned short> nNameIndex;
        for(unsigned short n = 0; n < FH->MH.Names.size() && !nNameIndex.Valid(); n++){
            if(StringEqual(FH->MH.Names.at(n).sName, "neck_g", true)) nNameIndex = n;
        }
        if(nNameIndex.Valid()){
            int nNodeIndex = GetNodeIndexByNameIndex(nNameIndex);
            if(nNodeIndex == -1) throw mdlexception("MDL::LinkHead() error: dealing with a name index that does not have a node in geometry.");
            nOffset = FH->MH.ArrayOfNodes.at(nNodeIndex).nOffset;
        }
        else return false;
    }
    else{
        nOffset = FH->MH.GH.nOffsetToRootNode;
    }
    FH->MH.nOffsetToHeadRootNode = nOffset;
    unsigned nHeadLinkOffset = 180;
    WriteNumber(&nOffset, 0, "", &nHeadLinkOffset);
    return true;
}

void MDL::UpdateTexture(Node & node, const std::string & sNew, int nTex){
    std::string sNewTex = sNew.c_str();
    if(nTex == 1){
        node.Mesh.cTexture1 = sNewTex;
        sNewTex.resize(32);
        int nPos = node.nOffset + 12 + 168;
        for(std::size_t n = 0; n < sNewTex.size(); ++n){
            sBuffer.at(static_cast<std::size_t>(nPos) + n) = sNewTex.at(n);
        }
    }
    else if(nTex == 2){
        node.Mesh.cTexture2 = sNewTex;
        sNewTex.resize(32);
        int nPos = node.nOffset + 12 + 200;
        for(std::size_t n = 0; n < sNewTex.size(); ++n){
            sBuffer.at(static_cast<std::size_t>(nPos) + n) = sNewTex.at(n);
        }
    }
    else if(nTex == 3){
        node.Mesh.cTexture3 = sNewTex;
        sNewTex.resize(12);
        int nPos = node.nOffset + 12 + 232;
        for(std::size_t n = 0; n < sNewTex.size(); ++n){
            sBuffer.at(static_cast<std::size_t>(nPos) + n) = sNewTex.at(n);
        }
    }
    else if(nTex == 4){
        node.Mesh.cTexture4 = sNewTex;
        sNewTex.resize(12);
        int nPos = node.nOffset + 12 + 244;
        for(std::size_t n = 0; n < sNewTex.size(); ++n){
            sBuffer.at(static_cast<std::size_t>(nPos) + n) = sNewTex.at(n);
        }
    }
}

void MDL::WriteUintToPlaceholder(unsigned int nUint, int nOffset){
    unsigned nInsert = static_cast<unsigned>(nOffset);
    WriteNumber(&nUint, 0, "", &nInsert);
}

void MDL::WriteByteToPlaceholder(unsigned char nByte, int nOffset){
    unsigned nInsert = static_cast<unsigned>(nOffset);
    WriteNumber(&nByte, 0, "", &nInsert);
}

//ascii
void MDL::FlushAscii(){
    if(Ascii) Ascii->FlushAll();
}

std::vector<char> & MDL::CreateAsciiBuffer(int nSize){
    return Ascii->CreateBuffer(nSize);
}

bool MDL::ReadAscii(){
    ReportObject ReportMdl(*this);

    // Keep existing model state intact unless the full ASCII/companion-walkmesh
    // read succeeds.
    std::unique_ptr<FileHeader> oldFH = std::move(FH);
    std::unique_ptr<MDX> oldMdx = std::move(Mdx);
    std::unique_ptr<WOK> oldWok = std::move(Wok);
    std::unique_ptr<PWK> oldPwk = std::move(Pwk);
    std::unique_ptr<DWK> oldDwk0 = std::move(Dwk0);
    std::unique_ptr<DWK> oldDwk1 = std::move(Dwk1);
    std::unique_ptr<DWK> oldDwk2 = std::move(Dwk2);
    const ModelSource oldSrc = src;
    const bool oldK2 = bK2;
    const bool oldXbox = bXbox;

    auto RestoreAsciiReadState = [&](){
        FH = std::move(oldFH);
        Mdx = std::move(oldMdx);
        Wok = std::move(oldWok);
        Pwk = std::move(oldPwk);
        Dwk0 = std::move(oldDwk0);
        Dwk1 = std::move(oldDwk1);
        Dwk2 = std::move(oldDwk2);
        src = oldSrc;
        bK2 = oldK2;
        bXbox = oldXbox;
    };

    auto CommitAsciiReadState = [&](){
        if(!Ascii){
            // Companion-only ASCII import: preserve the existing model/MDX/WOK
            // state and only commit the companion data that was actually read.
            FH = std::move(oldFH);
            Mdx = std::move(oldMdx);
            Wok = std::move(oldWok);
            src = oldSrc;
            bK2 = oldK2;
            bXbox = oldXbox;

            if(!PwkAscii){
                Pwk = std::move(oldPwk);
            }
            if(!DwkAscii){
                Dwk0 = std::move(oldDwk0);
                Dwk1 = std::move(oldDwk1);
                Dwk2 = std::move(oldDwk2);
            }
        }
        // Full model ASCII import: Ascii::Read() committed a new FileHeader.
        // Do not reattach any previous binary payload or companion objects;
        // doing so can make the new ASCII model inherit stale MDX/WOK/PWK/DWK
        // data from the previously-open model.
    };

    try{
        if(Ascii){
            if(!Ascii->Read(*this)){
                RestoreAsciiReadState();
                return false;
            }
            else ReportMdl << "Mdl ascii read succesfully!\n";
        }
        if(PwkAscii){
            if(!PwkAscii->ReadWalkmesh(*this, true)){
                RestoreAsciiReadState();
                return false;
            }
            else ReportMdl << "Pwk ascii read succesfully!\n";
        }
        if(DwkAscii){
            if(!DwkAscii->ReadWalkmesh(*this, false)){
                RestoreAsciiReadState();
                return false;
            }
            else ReportMdl << "Dwk ascii read succesfully!\n";
        }
    }
    catch(...){
        RestoreAsciiReadState();
        throw;
    }
    CommitAsciiReadState();
    return true;
}

Location Node::GetLocation(){
    Location location;

    MdlInteger<unsigned short> nCtrlPos;
    MdlInteger<unsigned short> nCtrlOri;
    for(unsigned short c = 0; c < Head.Controllers.size(); c++){
        if(Head.Controllers.at(c).nControllerType == CONTROLLER_HEADER_POSITION){
            nCtrlPos = c;
        }
        if(Head.Controllers.at(c).nControllerType == CONTROLLER_HEADER_ORIENTATION){
            nCtrlOri = c;
        }
    }

    if(nCtrlPos.Valid()){
        Controller & posctrl = Head.Controllers.at(nCtrlPos);
        location.vPosition = Vector(Head.ControllerData.at(posctrl.nDataStart + 0),
                                    Head.ControllerData.at(posctrl.nDataStart + 1),
                                    Head.ControllerData.at(posctrl.nDataStart + 2));
    }
    if(nCtrlOri.Valid()){
        Controller & orictrl = Head.Controllers.at(nCtrlOri);
        if(orictrl.nColumnCount == 4){
            location.oOrientation = Orientation(Head.ControllerData.at(orictrl.nDataStart + 0),
                                                Head.ControllerData.at(orictrl.nDataStart + 1),
                                                Head.ControllerData.at(orictrl.nDataStart + 2),
                                                Head.ControllerData.at(orictrl.nDataStart + 3));
        }
        else if(orictrl.nColumnCount == 2){
            location.oOrientation.SetQuaternion(DecompressQuaternion((unsigned int) Head.ControllerData.at(orictrl.nDataStart)));
        }
    }

    return location;
}

const std::string MDL::sClassName = "MDL";
const std::string MDX::sClassName = "MDX";
const std::string WOK::sClassName = "WOK";
const std::string PWK::sClassName = "PWK";
const std::string DWK::sClassName = "DWK";

const std::string MDL::GetName(){
    return sClassName;
}

const std::string MDX::GetName(){
    return sClassName;
}

const std::string WOK::GetName(){
    return sClassName;
}

const std::string PWK::GetName(){
    return sClassName;
}

const std::string DWK::GetName(){
    return sClassName;
}

bool LoadSupermodel(MDL & curmdl, std::unique_ptr<MDL> & Supermodel, bool bWarnIfUnavailable){
    ReportObject ReportMdl(curmdl);
    std::string sSMname = curmdl.GetFileData()->MH.cSupermodelName.c_str();
    if(sSMname != "NULL" && sSMname != ""){
        std::wstring sNewMdl = JoinPath(ParentPath(curmdl.GetFullPath()),
                                         to_wide(curmdl.GetFileData()->MH.cSupermodelName) + L".mdl");

        //Create file
        HANDLE hFile = bead_CreateReadFile(sNewMdl);

        //Check for problems
        bool bOpen = true;
        bool bWrongGame = false;
        if(hFile == INVALID_HANDLE_VALUE)
            bOpen = false;
        if(bOpen){
            std::vector<char> cBinary(4,0);
            if(!bead_ReadFile(hFile, cBinary, 4)) bOpen = false;
            /// Make sure that what we've read is a binary .mdl as far as we can tell
            else if(cBinary[0]!='\0' || cBinary[1]!='\0' || cBinary[2]!='\0' || cBinary[3]!='\0') bOpen = false;

        }

        /// If we pass, then the file is definitely ready to be read.
        if(bOpen){
            std::vector<char> cBinary(17,0);
            if(!bead_ReadFile(hFile, cBinary, 16)) bOpen = false;
            unsigned nFunctionPointer = 0;
            if(bOpen) std::memcpy(&nFunctionPointer, &cBinary.at(12), sizeof(nFunctionPointer));

            if(bOpen && !((nFunctionPointer == FN_PTR_PC_K2_MODEL_1 || nFunctionPointer == FN_PTR_XBOX_K2_MODEL_1) && curmdl.bK2) &&
               !((nFunctionPointer == FN_PTR_PC_K1_MODEL_1 || nFunctionPointer == FN_PTR_XBOX_K1_MODEL_1) && !curmdl.bK2)){
                bOpen = false;
                bWrongGame = true;
            }
        }
        if(bOpen){
            Supermodel.reset(new MDL);
            ReportMdl << "Reading supermodel: \n" << to_ansi(sNewMdl.c_str()) << "\n";
            unsigned long length = bead_GetFileLength(hFile);
            std::vector<char> & sBufferRef = Supermodel->CreateBuffer(length);
            if(!bead_ReadFile(hFile, sBufferRef)){
                CloseHandle(hFile);
                throw mdlexception("Error reading binary supermodel " + to_ansi(sNewMdl) + ".");
            }
            Supermodel->SetFilePath(sNewMdl);
            CloseHandle(hFile);

            Supermodel->DecompileModel(true);
            return true;
        }
        else{
            if(hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
            if(bWarnIfUnavailable){
                if(bWrongGame) Warning(L"Binary supermodel " + std::wstring(sNewMdl.c_str()) + L" belongs to " + (curmdl.bK2 ? L"K1" : L"K2") + L", while the model is being loaded for " + (curmdl.bK2 ? L"K2" : L"K1") + L". "
                                       L"The supermodel must belong to the game you are targeting, otherwise the supermodel animations will not work on the new model! The model will now be loaded without its supermodel.");
                else Warning(L"Could not find binary supermodel " + std::wstring(sNewMdl.c_str()) + L" in the directory! "
                             L"The new model must be compiled with access to its supermodel, otherwise the supermodel animations will not work on the new model! The model will now be loaded without its supermodel.");
            }
            return false;
        }
    }
    return false;
}

/// This function is to be used both when compiling and decompiling (to determine smoothing groups)
/// It is very expensive, so modify with care to keep it efficient. Any calculations that can be performed outside, should be.
void MDL::CreatePatches(){
    FileHeader & Data = *FH;
    ReportObject ReportMdl(*this);
    Timer tPatches;
    extern std::atomic_bool bCancelSG;

    /// Build patches by walking unprocessed mesh vertices and
    /// collecting every matching vertex from the current node onward with
    /// a 1e-5 vertex-coordinate tolerance.
    Report("Building Patch array... (this may take a while)");
    ReportMdl << "Building Patch array...";
    std::cout << " (this may take a while)";
    ReportMdl << "\n";
    std::cout << "\n";

    struct PatchBuildCleanupGuard{
        FileHeader & Data;
        bool bCommit = false;

        explicit PatchBuildCleanupGuard(FileHeader & data) : Data(data) {}
        ~PatchBuildCleanupGuard(){ if(!bCommit) Reset(); }

        void Reset(){
            Data.MH.PatchArrayPointers.clear();
            Data.MH.nTotalVertCount = 0;
            Data.MH.nTotalTangent1Count = 0;
            Data.MH.nTotalTangent2Count = 0;
            Data.MH.nTotalTangent3Count = 0;
            Data.MH.nTotalTangent4Count = 0;
            for(Node & node : Data.MH.ArrayOfNodes){
                if(node.Head.nType & NODE_MESH){
                    for(Vertex & vert : node.Mesh.Vertices){
                        vert.nLinkedFacesIndex = static_cast<unsigned int>(-1);
                    }
                }
            }
        }

        void Commit(){ bCommit = true; }
    } PatchCleanup(Data);

    PatchCleanup.Reset();

    struct VertexFaceRefs{
        std::vector<unsigned int> FaceIndices;
        unsigned int nSmoothingGroups = 0;
    };
    std::vector<std::vector<VertexFaceRefs>> FaceRefsByNode(Data.MH.ArrayOfNodes.size());
    for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
        Node & node = Data.MH.ArrayOfNodes.at(n);
        if(!(node.Head.nType & NODE_MESH) || (node.Head.nType & NODE_SABER)) continue;
        std::vector<VertexFaceRefs> & FaceRefs = FaceRefsByNode.at(n);
        FaceRefs.resize(node.Mesh.Vertices.size());
        for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
            Face & face = node.Mesh.Faces.at(f);
            for(int i = 0; i < 3; i++){
                unsigned int nVertex = face.nIndexVertex.at(i);
                if(static_cast<std::size_t>(nVertex) >= FaceRefs.size()) continue;
                FaceRefs.at(nVertex).FaceIndices.push_back(static_cast<unsigned int>(f));
                FaceRefs.at(nVertex).nSmoothingGroups |= face.nSmoothingGroup;
            }
        }
    }

    for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
        /// Currently, this takes all meshes except lightsabers, with render on or off.
        if(Data.MH.ArrayOfNodes.at(n).Head.nType & NODE_MESH && !(Data.MH.ArrayOfNodes.at(n).Head.nType & NODE_SABER)){
            Node & node = Data.MH.ArrayOfNodes.at(n);

            /// Patch/tangent totals are counted from patches that actually
            /// participate in faces. Meshes can contain unused binary vertices;
            /// counting those as normals to recover makes smoothing recovery fail
            /// even though they do not affect rendering or ASCII reconstruction.

            /// For every vertex of every mesh.
            for(int v = 0; v < static_cast<int>(node.Mesh.Vertices.size()); v++){
                /// Proceed only if this vertex hasn't been processed yet.
                if(!node.Mesh.Vertices.at(v).nLinkedFacesIndex.Valid()){
                    Vertex & vert = node.Mesh.Vertices.at(v);
                    Vector & vCoords = vert.vFromRoot;

                    /// Create new vector.
                    std::vector<Patch> CurrentPatchGroup;
                    unsigned int nPatchGroupIndex = static_cast<unsigned int>(Data.MH.PatchArrayPointers.size());

                    /// We've already gone through the nodes up to n and linked any vertices, so we can skip those.
                    for(int n2 = n; n2 < static_cast<int>(Data.MH.ArrayOfNodes.size()); n2++){
                        if(Data.MH.ArrayOfNodes.at(n2).Head.nType & NODE_MESH && !(Data.MH.ArrayOfNodes.at(n2).Head.nType & NODE_SABER)){
                            Node & node2 = Data.MH.ArrayOfNodes.at(n2);

                            /// Loop through all the verts in the mesh and look for matching vertices.
                            int nFirstVertex = (n2 == n ? v : 0);
                            for(int v2 = nFirstVertex; v2 < static_cast<int>(node2.Mesh.Vertices.size()); v2++){
                                /// Skip if this vertex has been processed.
                                if(node2.Mesh.Vertices.at(v2).nLinkedFacesIndex.Valid()) continue;

                                /// Check if vertices are equal enough.
                                Vector & vCoords2 = node2.Mesh.Vertices.at(v2).vFromRoot;
                                if(vCoords.Compare(vCoords2, 0.00001)){
                                    /// Update reference to new vector.
                                    node2.Mesh.Vertices.at(v2).nLinkedFacesIndex = nPatchGroupIndex;

                                    const VertexFaceRefs & FaceRefs = FaceRefsByNode.at(n2).at(v2);
                                    if(!FaceRefs.FaceIndices.empty()){
                                        /// Build patch and add it to the group.
                                        Patch OtherPatch;
                                        OtherPatch.nNodeArrayIndex = n2;
                                        OtherPatch.nVertex = v2;
                                        OtherPatch.nPatchGroup = nPatchGroupIndex;
                                        OtherPatch.FaceIndices = FaceRefs.FaceIndices;
                                        OtherPatch.nSmoothingGroups = FaceRefs.nSmoothingGroups;

                                        Data.MH.nTotalVertCount++;
                                        if(node2.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1) Data.MH.nTotalTangent1Count++;
                                        if(node2.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2) Data.MH.nTotalTangent2Count++;
                                        if(node2.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3) Data.MH.nTotalTangent3Count++;
                                        if(node2.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4) Data.MH.nTotalTangent4Count++;
                                        CurrentPatchGroup.push_back(std::move(OtherPatch));
                                    }

                                    /// If ESC was pressed, abort here.
                                    if(bCancelSG.load()) return;
                                }
                            }
                        }
                    }

                    /// Record group if any vertices matched.
                    if(!CurrentPatchGroup.empty()) Data.MH.PatchArrayPointers.push_back(std::move(CurrentPatchGroup));
                }
            }
        }
    }

    /// Algorithm done.
    PatchCleanup.Commit();
    ReportMdl << "Done creating patches in " << tPatches.GetTime() << ".\n";
}


unsigned MDL::GetHeaderOffset(const Node & node, unsigned short nHeader){
    unsigned nReturn = 0;

    if(nHeader == NODE_HEADER) return nReturn;
    else if(node.Head.nType & NODE_HEADER) nReturn += NODE_SIZE_HEADER;
    if(nHeader == NODE_LIGHT) return nReturn;
    else if(node.Head.nType & NODE_LIGHT) nReturn += NODE_SIZE_LIGHT;
    if(nHeader == NODE_EMITTER) return nReturn;
    else if(node.Head.nType & NODE_EMITTER) nReturn += NODE_SIZE_EMITTER;
    if(nHeader == NODE_REFERENCE) return nReturn;
    else if(node.Head.nType & NODE_REFERENCE) nReturn += NODE_SIZE_REFERENCE;
    if(nHeader == NODE_MESH) return nReturn;
    else if(node.Head.nType & NODE_MESH){
        nReturn += NODE_SIZE_MESH;
        if(bXbox) nReturn -= 4;
        if(!bK2) nReturn -= 8;
    }
    if(nHeader == NODE_SKIN) return nReturn;
    else if(node.Head.nType & NODE_SKIN) nReturn += NODE_SIZE_SKIN;
    if(nHeader == NODE_DANGLY) return nReturn;
    else if(node.Head.nType & NODE_DANGLY) nReturn += NODE_SIZE_DANGLY;
    if(nHeader == NODE_AABB) return nReturn;
    else if(node.Head.nType & NODE_AABB) nReturn += NODE_SIZE_AABB;
    if(nHeader == NODE_SABER) return nReturn;
    else if(node.Head.nType & NODE_SABER) nReturn += NODE_SIZE_SABER;

    throw mdlexception("MDL::GetHeaderOffset(): unknown node header " + std::to_string(nHeader) + ".");
}

bool IsMaterialWalkable(int nMat){
    if(nMat == MATERIAL_NONWALK ||
       nMat == MATERIAL_OBSCURING ||
       nMat == MATERIAL_TRANSPARENT ||
       nMat == MATERIAL_LAVA ||
       //nMat == MATERIAL_BOTTOMLESSPIT ||
       nMat == MATERIAL_DEEPWATER ||
       nMat == MATERIAL_SNOW) return false;
    return true;
}

//This function together with the next one, checks the currently loaded data in MDL for any special properties
void MDL::CheckPeculiarities(){
    ReportObject ReportMdl(*this);
    FileHeader & Data = *FH;
    std::stringstream ssReturn;
    bool bUpdate = false;
    Report("Checking for peculiarities...");
    ssReturn << "Lucky you! Your model has some peculiarities:";
    if(!Data.MH.GH.RuntimeArray1.empty()){
        ssReturn << "\r\n - First empty runtime array in the GH has a some nonzero value!";
        bUpdate = true;
    }
    if(!Data.MH.GH.RuntimeArray2.empty()){
        ssReturn << "\r\n - Second empty runtime array in the GH has a some nonzero value!";
        bUpdate = true;
    }
    if(Data.MH.GH.nRefCount != 0){
        ssReturn << "\r\n - RefCount has a value!";
        bUpdate = true;
    }
    if(Data.MH.GH.nModelType != 2){
        ssReturn << "\r\n - Header ModelType different than 2!";
        bUpdate = true;
    }
    if(Data.MH.nUnknown != 0 && !bWriteSmoothing){
        ssReturn << "\r\n - Second classification padding number different than 0!";
        bUpdate = true;
    }
    if(Data.MH.nChildModelCount != 0){
        ssReturn << "\r\n - ChildModelCount has a value!";
        bUpdate = true;
    }
    if(Data.MH.AnimationArray.GetDoCountsDiffer()){
        ssReturn << "\r\n - AnimationArray counts differ!";
        bUpdate = true;
    }
    if(Data.MH.nPadding != 0){
        ssReturn << "\r\n - Unknown int32 after the Root Head Node Offset in the MH has a value!";
        bUpdate = true;
    }
    if(Data.MH.nMdxLength2 != Data.nMdxLength){
        ssReturn << "\r\n - MdxLength in FH and MdxLength2 in MH don't have the same value!";
        bUpdate = true;
    }
    if(Data.MH.nOffsetIntoMdx != 0){
        ssReturn << "\r\n - OffsetIntoMdx after the MdxLength2 in the MH has a value!";
        bUpdate = true;
    }
    if(Data.MH.NameArray.GetDoCountsDiffer()){
        ssReturn << "\r\n - NameArray counts differ!";
        bUpdate = true;
    }
    for(unsigned int a = 0; a < Data.MH.AnimationArray.nCount; ++a){
        if(!Data.MH.Animations.at(a).RuntimeArray1.empty()){
            ssReturn << "\r\n   - First empty runtime array in the Animation GH has a some nonzero value!";
            bUpdate = true;
        }
        if(!Data.MH.Animations.at(a).RuntimeArray2.empty()){
            ssReturn << "\r\n   - Second empty runtime array in the Animation GH has a some nonzero value!";
            bUpdate = true;
        }
        if(Data.MH.Animations.at(a).nRefCount != 0){
            ssReturn << "\r\n   - Animation counterpart to RefCount has a value!";
            bUpdate = true;
        }
        if(Data.MH.Animations.at(a).EventArray.GetDoCountsDiffer()){
            ssReturn << "\r\n   - EventArray counts differ!";
            bUpdate = true;
        }
        if(Data.MH.Animations.at(a).nPadding2 != 0){
            ssReturn << "\r\n   - Unknown int32 after EventArrayHead has a value!";
            bUpdate = true;
        }
        if(Data.MH.Animations.at(a).nModelType != 5){
            ssReturn << "\r\n   - Animation ModelType is not 5!";
            bUpdate = true;
        }
        if(CheckNodes(Data.MH.Animations.at(a).ArrayOfNodes, ssReturn, a)) bUpdate = true;
    }
    if(CheckNodes(Data.MH.ArrayOfNodes, ssReturn, -1)) bUpdate = true;
    if(!bUpdate){
        ReportMdl << "Checked for peculiarities, nothing to report.\n";
        return;
    }
    MessageBox(NULL, ssReturn.str().c_str(), "Notification", MB_OK);
}

bool MDL::CheckNodes(std::vector<Node> & NodeArray, std::stringstream & ssReturn, int nAnimation){
    bool bMasterUpdate = false;
    for(std::size_t b = 0; b < NodeArray.size(); ++b){
        if(NodeArray.at(b).Head.nType == 0){
            //Ghost node
        }
        else{
            bool bUpdate = false;
            std::stringstream ssAdd;
            std::string sCont;
            if(nAnimation == -1) sCont = "geometry";
            else sCont = FH->MH.Animations.at(nAnimation).sName.c_str();
            ssAdd << "\r\n - " << FH->MH.Names.at(NodeArray.at(b).Head.nNameIndex).sName << " (" << sCont << ")";
            if(NodeArray.at(b).Head.nType & NODE_HEADER){
                if(NodeArray.at(b).Head.nPadding1 != 0){
                    ssAdd << "\r\n     - Header: Unknown short after NameIndex has a value!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Head.ChildrenArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Header: ChildArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Head.ControllerArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Header: ControllerArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Head.ControllerDataArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Header: ControllerDataArray counts differ!";
                    bUpdate = true;
                }
                for(std::size_t c = 0; c < NodeArray.at(b).Head.Controllers.size(); ++c){
                    Controller & ctrl = NodeArray.at(b).Head.Controllers.at(c);

                    if( (ctrl.nControllerType == CONTROLLER_HEADER_POSITION ||
                         ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION) &&
                        ctrl.nAnimation.Valid() &&
                        ctrl.nUnknown2 == ctrl.nControllerType + 8){}
                    else if(!ctrl.nUnknown2.Valid() &&
                            ( (ctrl.nControllerType != CONTROLLER_HEADER_POSITION &&
                               ctrl.nControllerType != CONTROLLER_HEADER_ORIENTATION) ||
                              !ctrl.nAnimation.Valid()) ){}
                    else{
                        std::string sLoc;
                        if(!ctrl.nAnimation.Valid()) sLoc = "geometry";
                        else sLoc = FH->MH.Animations.at(ctrl.nAnimation).sName.c_str();
                        ssAdd << "\r\n     - Header: New controller unknown2 value! (" << ctrl.nUnknown2 << " - " << ReturnControllerName(ctrl.nControllerType, FH->MH.ArrayOfNodes.at(ctrl.nNameIndex).Head.nType) << " controller in node " << FH->MH.Names.at(ctrl.nNameIndex).sName << " in " << sLoc << ")";
                        bUpdate = true;
                    }
                    /*
                        This if for checking controller "padding" values. These numbers are in no way random.
                        Header and light controllers always have 0 for the third number, while emitter and mesh controllers have it greater than 0.
                        In keyed controllers, light and emitter seem to group together against header and mesh.
                        Selfillumcolor usually has the same padding values as scale, but they are different
                        in for example: n_admrlsaulkar or 003ebof
                    if(ctrl.nControllerType==CONTROLLER_HEADER_POSITION ||
                       ctrl.nControllerType==CONTROLLER_HEADER_ORIENTATION ||
                       ctrl.nControllerType==CONTROLLER_HEADER_SCALING && ctrl.nAnimation == -1 &&
                        (ctrl.nPadding[0] == 12 &&
                         ctrl.nPadding[1] == 76 &&
                         ctrl.nPadding[2] == 0   )){}
                    else if(ctrl.nControllerType==CONTROLLER_HEADER_ORIENTATION && ctrl.nAnimation == -1 &&
                             (ctrl.nPadding[0] == -59 &&
                              ctrl.nPadding[1] == 73 &&
                              ctrl.nPadding[2] == 0   )){}
                    else if(ctrl.nControllerType==CONTROLLER_HEADER_SCALING && ctrl.nAnimation == -1 &&
                             (ctrl.nPadding[0] == 49 &&
                              ctrl.nPadding[1] == 18 &&
                              ctrl.nPadding[2] == 0   )){}
                    else if( (ctrl.nControllerType==CONTROLLER_LIGHT_COLOR || ctrl.nControllerType==CONTROLLER_LIGHT_MULTIPLIER || ctrl.nControllerType==CONTROLLER_LIGHT_RADIUS ||
                              ctrl.nControllerType==CONTROLLER_LIGHT_SHADOWRADIUS || ctrl.nControllerType==CONTROLLER_LIGHT_VERTICALDISPLACEMENT) && ctrl.nAnimation == -1 &&
                             (ctrl.nPadding[0] == -5 &&
                              ctrl.nPadding[1] == 54 &&
                              ctrl.nPadding[2] == 0   )){}
                    else if( (ctrl.nControllerType==CONTROLLER_HEADER_POSITION ||
                              ctrl.nControllerType==CONTROLLER_HEADER_ORIENTATION ||
                              ctrl.nControllerType==CONTROLLER_HEADER_SCALING ||
                              GetNodeByNameIndex(ctrl.nNameIndex).Head.nType & NODE_MESH) && ctrl.nAnimation != -1 &&
                             (ctrl.nPadding[0] == 50 &&
                              ctrl.nPadding[1] == 18 &&
                              ctrl.nPadding[2] == 0   )){}
                    else if(  ctrl.nAnimation != -1 &&
                             (ctrl.nPadding[0] == 51 &&
                              ctrl.nPadding[1] == 18 &&
                              ctrl.nPadding[2] == 0   )){}
                    /// the following are all emitter and mesh single controllers (as long as the last value is non-0)
                    else if( (GetNodeByNameIndex(ctrl.nNameIndex).Head.nType & NODE_EMITTER ||
                              GetNodeByNameIndex(ctrl.nNameIndex).Head.nType & NODE_MESH) &&
                              ctrl.nPadding[2] > 0 && ctrl.nAnimation == -1 ){}
                    else{
                        //ssAdd << "\r\n     - Header: Previously unseen controller padding! (" << (int)ctrl.nPadding[0] << ", " << (int)ctrl.nPadding[1] << ", " << (int)ctrl.nPadding[2] << ")";
                        //bUpdate = true;
                    }
                    */
                }
            }
            if(NodeArray.at(b).Head.nType & NODE_LIGHT){
                if(NodeArray.at(b).Light.UnknownArray.nCount != 0){
                    ssAdd << "\r\n     - Light: Unknown array not empty!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Light.FlareSizeArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Light: FlareSizeArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Light.FlarePositionArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Light: FlarePositionArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Light.FlareColorShiftArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Light: FlareColorShiftArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Light.FlareTextureNameArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Light: FlareTextureNameArray counts differ!";
                    bUpdate = true;
                }
            }
            if(NodeArray.at(b).Head.nType & NODE_EMITTER){
                /*
                if(NodeArray.at(b).Emitter.nUnknown1 != 0){
                    ssAdd << "\r\n     - Emitter: Unknown short after Loop has a value!";
                    bUpdate = true;
                }
                */
            }
            if(NodeArray.at(b).Head.nType & NODE_MESH){
                if(NodeArray.at(b).Mesh.nUnknown3[0] != -1){
                    ssAdd << "\r\n     - Mesh: The unknown -1 -1 0 array has a different value!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Mesh.FaceArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Mesh: FaceArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Mesh.IndexCounterArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Mesh: IndexCounterArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Mesh.IndexLocationArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Mesh: IndexLocationArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Mesh.MeshInvertedCounterArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Mesh: MeshInvertedCounterArray counts differ!";
                    bUpdate = true;
                }
            }
            if(NodeArray.at(b).Head.nType & NODE_SKIN){
                if(!NodeArray.at(b).Skin.UnknownArray.empty()){
                    ssAdd << "\r\n     - Skin: Unknown empty array has some nonzero value!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Skin.QBoneArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Skin: QBoneArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Skin.TBoneArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Skin: TBoneArray counts differ!";
                    bUpdate = true;
                }
                if(NodeArray.at(b).Skin.Array8Array.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Skin: Array8Array counts differ!";
                    bUpdate = true;
                }
            }
            if(NodeArray.at(b).Head.nType & NODE_DANGLY){
                if(NodeArray.at(b).Dangly.ConstraintArray.GetDoCountsDiffer()){
                    ssAdd << "\r\n     - Dangly: ConstraintArray counts differ!";
                    bUpdate = true;
                }
            }
            if(bUpdate){
                bMasterUpdate = true;
                ssReturn << ssAdd.str();
            }
        }
    }
    return bMasterUpdate;
}


std::string ReturnClassificationName(int nClassification){
    switch(nClassification){
        case CLASS_OTHER: return "other";
        case CLASS_EFFECT: return "effect";
        case CLASS_TILE: return "tile";
        case CLASS_CHARACTER: return "character";
        case CLASS_DOOR: return "door";
        case CLASS_LIGHTSABER: return "lightsaber";
        case CLASS_PLACEABLE: return "placeable";
        case CLASS_FLYER: return "flyer";
    }
    std::cout << "ReturnClassification() ERROR: Unknown classification " << nClassification << ".\n";
    return "unknown";
}

int ReturnController(std::string sController, int nType){
    if(sController == "position") return CONTROLLER_HEADER_POSITION;
    else if(sController == "orientation") return CONTROLLER_HEADER_ORIENTATION;
    else if(sController == "scale") return CONTROLLER_HEADER_SCALING;
    else if(nType & NODE_LIGHT){
        if(sController == "color") return CONTROLLER_LIGHT_COLOR;
        else if(sController == "radius") return CONTROLLER_LIGHT_RADIUS;
        else if(sController == "shadowradius") return CONTROLLER_LIGHT_SHADOWRADIUS;          //Missing from NWmax
        else if(sController == "verticaldisplacement") return CONTROLLER_LIGHT_VERTICALDISPLACEMENT;  //Missing from NWmax
        else if(sController == "multiplier") return CONTROLLER_LIGHT_MULTIPLIER;
    }
    else if(nType & NODE_EMITTER){
        if(sController == "alphaend") return CONTROLLER_EMITTER_ALPHAEND;
        else if(sController == "alphastart") return CONTROLLER_EMITTER_ALPHASTART;
        else if(sController == "birthrate") return CONTROLLER_EMITTER_BIRTHRATE;
        else if(sController == "bounce_co") return CONTROLLER_EMITTER_BOUNCE_CO;
        else if(sController == "combinetime") return CONTROLLER_EMITTER_COMBINETIME;
        else if(sController == "drag") return CONTROLLER_EMITTER_DRAG;
        else if(sController == "fps") return CONTROLLER_EMITTER_FPS;
        else if(sController == "frameend") return CONTROLLER_EMITTER_FRAMEEND;
        else if(sController == "framestart") return CONTROLLER_EMITTER_FRAMESTART;
        else if(sController == "grav") return CONTROLLER_EMITTER_GRAV;
        else if(sController == "lifeexp") return CONTROLLER_EMITTER_LIFEEXP;
        else if(sController == "mass") return CONTROLLER_EMITTER_MASS;
        else if(sController == "p2p_bezier2") return CONTROLLER_EMITTER_P2P_BEZIER2;
        else if(sController == "p2p_bezier3") return CONTROLLER_EMITTER_P2P_BEZIER3;
        else if(sController == "particlerot") return CONTROLLER_EMITTER_PARTICLEROT;
        else if(sController == "randvel") return CONTROLLER_EMITTER_RANDVEL;
        else if(sController == "sizestart") return CONTROLLER_EMITTER_SIZESTART;
        else if(sController == "sizeend") return CONTROLLER_EMITTER_SIZEEND;
        else if(sController == "sizestart_y") return CONTROLLER_EMITTER_SIZESTART_Y;
        else if(sController == "sizeend_y") return CONTROLLER_EMITTER_SIZEEND_Y;
        else if(sController == "spread") return CONTROLLER_EMITTER_SPREAD;
        else if(sController == "threshold") return CONTROLLER_EMITTER_THRESHOLD;
        else if(sController == "velocity") return CONTROLLER_EMITTER_VELOCITY;
        else if(sController == "xsize") return CONTROLLER_EMITTER_XSIZE;
        else if(sController == "ysize") return CONTROLLER_EMITTER_YSIZE;
        else if(sController == "blurlength") return CONTROLLER_EMITTER_BLURLENGTH;
        else if(sController == "lightningdelay") return CONTROLLER_EMITTER_LIGHTNINGDELAY;
        else if(sController == "lightningradius") return CONTROLLER_EMITTER_LIGHTNINGRADIUS;
        else if(sController == "lightningscale") return CONTROLLER_EMITTER_LIGHTNINGSCALE;
        else if(sController == "lightningsubdiv") return CONTROLLER_EMITTER_LIGHTNINGSUBDIV;
        else if(sController == "lightningzigzag") return CONTROLLER_EMITTER_LIGHTNINGZIGZAG;    //Missing from NWmax
        else if(sController == "alphamid") return CONTROLLER_EMITTER_ALPHAMID;           //Missing from NWmax
        else if(sController == "percentstart") return CONTROLLER_EMITTER_PERCENTSTART;       //Missing from NWmax
        else if(sController == "percentmid") return CONTROLLER_EMITTER_PERCENTMID;         //Missing from NWmax
        else if(sController == "percentend") return CONTROLLER_EMITTER_PERCENTEND;         //Missing from NWmax
        else if(sController == "sizemid") return CONTROLLER_EMITTER_SIZEMID;            //Missing from NWmax
        else if(sController == "sizemid_y") return CONTROLLER_EMITTER_SIZEMID_Y;          //Missing from NWmax
        else if(sController == "m_frandombirthrate") return CONTROLLER_EMITTER_RANDOMBIRTHRATE;    //Missing from NWmax
        else if(sController == "targetsize") return CONTROLLER_EMITTER_TARGETSIZE;         //Missing from NWmax
        else if(sController == "numcontrolpts") return CONTROLLER_EMITTER_NUMCONTROLPTS;      //Missing from NWmax
        else if(sController == "controlptradius") return CONTROLLER_EMITTER_CONTROLPTRADIUS;    //Missing from NWmax
        else if(sController == "controlptdelay") return CONTROLLER_EMITTER_CONTROLPTDELAY;     //Missing from NWmax
        else if(sController == "tangentspread") return CONTROLLER_EMITTER_TANGENTSPREAD;      //Missing from NWmax
        else if(sController == "tangentlength") return CONTROLLER_EMITTER_TANGENTLENGTH;      //Missing from NWmax
        else if(sController == "colormid") return CONTROLLER_EMITTER_COLORMID;           //Missing from NWmax
        else if(sController == "colorend") return CONTROLLER_EMITTER_COLOREND;
        else if(sController == "colorstart") return CONTROLLER_EMITTER_COLORSTART;
        else if(sController == "detonate") return CONTROLLER_EMITTER_DETONATE;           //Missing from NWmax
    }
    else if(nType & NODE_MESH){
        if(sController == "selfillumcolor") return CONTROLLER_MESH_SELFILLUMCOLOR;
        else if(sController == "alpha") return CONTROLLER_MESH_ALPHA;
    }
    return 0;
}

std::string ReturnControllerName(int nController, int nType){
    switch(nController){
        case CONTROLLER_HEADER_POSITION:            return "position";
        case CONTROLLER_HEADER_ORIENTATION:         return "orientation";
        case CONTROLLER_HEADER_SCALING:             return "scale";
    }

    if(nType & NODE_LIGHT){
        switch(nController){
        case CONTROLLER_LIGHT_COLOR:                return "color";
        case CONTROLLER_LIGHT_RADIUS:               return "radius";
        case CONTROLLER_LIGHT_SHADOWRADIUS:         return "shadowradius";          //Missing from NWmax
        case CONTROLLER_LIGHT_VERTICALDISPLACEMENT: return "verticaldisplacement";  //Missing from NWmax
        case CONTROLLER_LIGHT_MULTIPLIER:           return "multiplier";
        }
    }
    else if(nType & NODE_EMITTER){
        switch(nController){
        case CONTROLLER_EMITTER_ALPHAEND:           return "alphaEnd";
        case CONTROLLER_EMITTER_ALPHASTART:         return "alphaStart";
        case CONTROLLER_EMITTER_BIRTHRATE:          return "birthrate";
        case CONTROLLER_EMITTER_BOUNCE_CO:          return "bounce_co";
        case CONTROLLER_EMITTER_COMBINETIME:        return "combinetime";
        case CONTROLLER_EMITTER_DRAG:               return "drag";
        case CONTROLLER_EMITTER_FPS:                return "fps";
        case CONTROLLER_EMITTER_FRAMEEND:           return "frameEnd";
        case CONTROLLER_EMITTER_FRAMESTART:         return "frameStart";
        case CONTROLLER_EMITTER_GRAV:               return "grav";
        case CONTROLLER_EMITTER_LIFEEXP:            return "lifeExp";
        case CONTROLLER_EMITTER_MASS:               return "mass";
        case CONTROLLER_EMITTER_P2P_BEZIER2:        return "p2p_bezier2";
        case CONTROLLER_EMITTER_P2P_BEZIER3:        return "p2p_bezier3";
        case CONTROLLER_EMITTER_PARTICLEROT:        return "particleRot";
        case CONTROLLER_EMITTER_RANDVEL:            return "randvel";
        case CONTROLLER_EMITTER_SIZESTART:          return "sizeStart";
        case CONTROLLER_EMITTER_SIZEEND:            return "sizeEnd";
        case CONTROLLER_EMITTER_SIZESTART_Y:        return "sizeStart_y";
        case CONTROLLER_EMITTER_SIZEEND_Y:          return "sizeEnd_y";
        case CONTROLLER_EMITTER_SPREAD:             return "spread";
        case CONTROLLER_EMITTER_THRESHOLD:          return "threshold";
        case CONTROLLER_EMITTER_VELOCITY:           return "velocity";
        case CONTROLLER_EMITTER_XSIZE:              return "xsize";
        case CONTROLLER_EMITTER_YSIZE:              return "ysize";
        case CONTROLLER_EMITTER_BLURLENGTH:         return "blurlength";
        case CONTROLLER_EMITTER_LIGHTNINGDELAY:     return "lightningDelay";
        case CONTROLLER_EMITTER_LIGHTNINGRADIUS:    return "lightningRadius";
        case CONTROLLER_EMITTER_LIGHTNINGSCALE:     return "lightningScale";
        case CONTROLLER_EMITTER_LIGHTNINGSUBDIV:    return "lightningSubDiv";
        case CONTROLLER_EMITTER_LIGHTNINGZIGZAG:    return "lightningzigzag";    //Missing from NWmax
        case CONTROLLER_EMITTER_ALPHAMID:           return "alphaMid";           //Missing from NWmax
        case CONTROLLER_EMITTER_PERCENTSTART:       return "percentStart";       //Missing from NWmax
        case CONTROLLER_EMITTER_PERCENTMID:         return "percentMid";         //Missing from NWmax
        case CONTROLLER_EMITTER_PERCENTEND:         return "percentEnd";         //Missing from NWmax
        case CONTROLLER_EMITTER_SIZEMID:            return "sizeMid";            //Missing from NWmax
        case CONTROLLER_EMITTER_SIZEMID_Y:          return "sizeMid_y";          //Missing from NWmax
        case CONTROLLER_EMITTER_RANDOMBIRTHRATE:    return "m_fRandomBirthRate";    //Missing from NWmax
        case CONTROLLER_EMITTER_TARGETSIZE:         return "targetsize";         //Missing from NWmax
        case CONTROLLER_EMITTER_NUMCONTROLPTS:      return "numcontrolpts";      //Missing from NWmax
        case CONTROLLER_EMITTER_CONTROLPTRADIUS:    return "controlptradius";    //Missing from NWmax
        case CONTROLLER_EMITTER_CONTROLPTDELAY:     return "controlptdelay";     //Missing from NWmax
        case CONTROLLER_EMITTER_TANGENTSPREAD:      return "tangentspread";      //Missing from NWmax
        case CONTROLLER_EMITTER_TANGENTLENGTH:      return "tangentlength";      //Missing from NWmax
        case CONTROLLER_EMITTER_COLORMID:           return "colorMid";           //Missing from NWmax
        case CONTROLLER_EMITTER_COLOREND:           return "colorEnd";
        case CONTROLLER_EMITTER_COLORSTART:         return "colorStart";
        case CONTROLLER_EMITTER_DETONATE:           return "detonate";           //Missing from NWmax
        }
    }
    else{
        switch(nController){
        case CONTROLLER_MESH_SELFILLUMCOLOR:        return "selfillumcolor";
        case CONTROLLER_MESH_ALPHA:                 return "alpha";
        }
    }
    std::cout << "ReturnController() ERROR: Unknown controller " << nController << " (type " << nType << ").\n";
    return "unknown";
}
