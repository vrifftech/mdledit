#include "MDL.h"
#include "ascii_compat.h"
#include <algorithm>
#include <limits>
#include <utility>

// Functions:
//   TruncateLongName3216()
//   ASCII::Read()

void TruncateLongName3216(std::string & sID, const std::string & sContext, unsigned int nLine, bool bWarnAt16 = true){
    if(sID.size() > 32){
        Warning(sContext + " (" + sID + ") at line " + std::to_string(nLine) + " is larger than the limit, 32 characters! Will truncate and continue.");
        sID.resize(32);
    }
    else if(sID.size() > 16 && bWarnAt16){
        Warning(sContext + " (" + sID + ") at line " + std::to_string(nLine) + " is larger than 16 characters! This may or may not cause problems in the game.");
    }
}

void SetAsciiListCount(MdlInteger<unsigned int> & nDataMax, int nCount, const std::string & sListName){
    if(nCount < 0) throw mdlexception(sListName + " count cannot be negative.");
    nDataMax = static_cast<unsigned int>(nCount);
}

void SetAsciiListCountSize(MdlInteger<unsigned int> & nDataMax, std::size_t nCount, const std::string & sListName){
    if(nCount > std::numeric_limits<unsigned int>::max()){
        throw mdlexception(sListName + " count is too large.");
    }
    nDataMax = static_cast<unsigned int>(nCount);
}

MdlInteger<unsigned short> AsciiUShortOrInvalid(int nValue, const std::string & sField){
    if(nValue == -1) return MdlInteger<unsigned short>();
    if(nValue < -1){
        throw mdlexception(sField + " cannot be less than -1.");
    }
    const int nInvalid = static_cast<int>(std::numeric_limits<unsigned short>::max());
    if(nValue >= nInvalid){
        throw mdlexception(sField + " must be -1 or in the range 0.." + std::to_string(nInvalid - 1) + ".");
    }
    return MdlInteger<unsigned short>(static_cast<unsigned short>(nValue));
}

MdlInteger<unsigned int> AsciiUIntOrInvalid(int nValue, const std::string & sField){
    if(nValue == -1) return MdlInteger<unsigned int>();
    if(nValue < -1){
        throw mdlexception(sField + " cannot be less than -1.");
    }
    return MdlInteger<unsigned int>(static_cast<unsigned int>(nValue));
}

MdlInteger<unsigned short> AsciiUShortIndex(unsigned int nValue, const std::string & sField){
    const unsigned int nInvalid = static_cast<unsigned int>(std::numeric_limits<unsigned short>::max());
    if(nValue >= nInvalid){
        throw mdlexception(sField + " must be in the range 0.." + std::to_string(nInvalid - 1) + ".");
    }
    return MdlInteger<unsigned short>(static_cast<unsigned short>(nValue));
}

void EnsureUniqueAsciiVisibleName(const std::vector<Name> & names, const std::string & sName, unsigned int nLine){
    for(const Name & existing : names){
        if(StringEqual(existing.sName, sName)){
            throw mdlexception("Duplicate ASCII name '" + sName + "' at line " + std::to_string(nLine) +
                               "; node/name tokens must be unique before __dpl suffix stripping so references resolve to the original binary name index.");
        }
    }
}

MdlInteger<unsigned short> LookupAsciiNameIndex(MDL & Mdl, const std::string & sName, const std::string & sContext){
    int nNameIndex = Mdl.GetNameIndex(sName);
    if(nNameIndex < 0){
        throw mdlexception(sContext + ": node/name '" + sName + "' was not indexed in the ASCII name table.");
    }
    return AsciiUShortIndex(static_cast<unsigned int>(nNameIndex), sContext + " name index");
}

void ThrowTooManyListRows(const std::string & sListName, const std::string & sNodeName){
    throw mdlexception("Too many " + sListName + " rows for node '" + sNodeName + "'; refusing to consume following ASCII tokens as list data.");
}

template <class T>
void ResizeAsciiVector(std::vector<T> & values, std::size_t nCount, const std::string & sListName){
    if(nCount > values.max_size()){
        throw mdlexception(sListName + " count is too large to allocate safely.");
    }
    values.resize(nCount);
}

template <class T>
void ResizeAsciiVector(std::vector<T> & values, std::size_t nCount, const T & value, const std::string & sListName){
    if(nCount > values.max_size()){
        throw mdlexception(sListName + " count is too large to allocate safely.");
    }
    values.resize(nCount, value);
}

template <class T>
void EnsureAsciiVectorSize(std::vector<T> & values, std::size_t nCount, const std::string & sListName){
    if(nCount > values.max_size()){
        throw mdlexception(sListName + " count is too large to allocate safely.");
    }
    if(values.size() < nCount) values.resize(nCount);
}

template <class T>
void EnsureAsciiVectorSize(std::vector<T> & values, std::size_t nCount, const T & value, const std::string & sListName){
    if(nCount > values.max_size()){
        throw mdlexception(sListName + " count is too large to allocate safely.");
    }
    if(values.size() < nCount) values.resize(nCount, value);
}
namespace {
    template <class F>
    class AsciiScopeExit{
        F fn;
        bool active = true;
      public:
        explicit AsciiScopeExit(F f) : fn(std::move(f)) {}
        AsciiScopeExit(const AsciiScopeExit &) = delete;
        AsciiScopeExit & operator=(const AsciiScopeExit &) = delete;
        AsciiScopeExit(AsciiScopeExit && other) : fn(std::move(other.fn)), active(other.active){
            other.active = false;
        }
        AsciiScopeExit & operator=(AsciiScopeExit &&) = delete;
        void Dismiss(){ active = false; }
        ~AsciiScopeExit(){ if(active) fn(); }
    };

    template <class F>
    AsciiScopeExit<F> MakeAsciiScopeExit(F f){
        return AsciiScopeExit<F>(std::move(f));
    }
}

bool ASCII::Read(MDL & Mdl){
    //std::cout << "We made it into ASCII::Read.\n";
    ReportObject ReportMdl(Mdl);

    std::unique_ptr<FileHeader> pOldFileData = std::move(Mdl.FH);
    std::unique_ptr<MDX> pOldMdx = std::move(Mdl.Mdx);
    std::unique_ptr<WOK> pOldWok = std::move(Mdl.Wok);
    const ModelSource oldSource = Mdl.src;
    const bool oldK2 = Mdl.bK2;
    const bool oldXbox = Mdl.bXbox;
    const int oldSupermodel = Mdl.nSupermodel;
    Mdl.FH.reset(new FileHeader);
    auto restoreReadState = MakeAsciiScopeExit([&](){
        Mdl.FH = std::move(pOldFileData);
        Mdl.Mdx = std::move(pOldMdx);
        Mdl.Wok = std::move(pOldWok);
        Mdl.src = oldSource;
        Mdl.bK2 = oldK2;
        Mdl.bXbox = oldXbox;
        Mdl.nSupermodel = oldSupermodel;
    });

    FileHeader & Data = *Mdl.FH;
    std::string sID; /// Will be reading the ascii text into this string in parts.

    Mdl.src = AsciiSource;
    Mdl.Report("Reading ASCII...");

    //Set stuff to zero
    nPosition = 0; /// Our position when reading the ascii file
    nLine = 1;
    Data.MH.Animations.resize(0);
    Data.MH.Names.resize(0);

    /// Conversion variables
    int nConvert;
    unsigned int uConvert;
    double fConvert;

    //bool bFound = false;
    bool bGeometry = false;     /// Are we inside geometry?
    int nNode = 0;              /// The current node type
    MdlInteger<unsigned int> nAnimation;        /// The current animation
    bool bAnimation = false;    /// Are we inside an animation?

    /// Data type variables
    bool bEventlist = false;
    bool bVerts = false;
    bool bFaces = false;
    bool bFaceExtra = false;
    bool bTverts = false;
    bool bTexIndices1 = false;
    bool bTverts1 = false;
    bool bTexIndices2 = false;
    bool bTverts2 = false;
    bool bTexIndices3 = false;
    bool bTverts3 = false;
    bool bNormalIndices = false;
    bool bNormals = false;
    bool bTangentBasis = false;
    bool bWeights = false;
    bool bCompactBoneMap = false;
    bool bCompactBoneMapRaw = false;
    bool bBoneConstantIndices = false;
    bool bExtraData = false;
    bool bAabb = false;
    bool bSaberData = false;
    bool bConstraints = false;
    bool bKeys = false;
    bool bTextureNames = false;
    bool bControllerRaw = false;
    bool bControllerDataRaw = false;
    bool bFlarePositions = false;
    bool bFlareSizes = false;
    bool bFlareColorShifts = false;
    bool bRoomLinks = false;
    bool bColors = false;
    bool bColorIndices = false;

    /// Variables for reading data
    MdlInteger<unsigned int> nDataMax;
    unsigned int nDataCounter;

    bool bMagnusll = false; /// Magnus11-style lightmap reading
    unsigned int nNodeCount = 0;
    MdlInteger<unsigned short> nCurrentIndex;

    std::vector<std::string> sBumpmapped; /// This variable collects the bumpmappable texture names specified in the ascii.

    ///This first loop is for building the name array, which we need for the weights
    /// Actually, it seems that the ascii is potentially ambiguous. In binary, two objects may have the same name,
    /// and they will still be distinguished in all cases, because they are always referred to by their name index.
    /// In ascii, the name replaces the name index. This means that weights, which do not rely on a bonemap, will be
    /// ambiguous when it comes to two objects with the same name. Actually the same applies for the parent field,
    /// it is potentially possible for either of the objects to be the real parent. This should be addressed in the
    /// conversion to ascii, so that the names are disambiguated. The user then only needs to make sure that the ascii
    /// models they provide for reading are well-formed in that respect.
    /// In short, we will now assume that all names are unique.
    try{
        /// Go through the ascii file and stop recording names when you get to endmodelgeom.
        /// Skip all lines except the ones that start with node or name.
        /// Get the name from both node and name lines, and record the number of node lines (actual nodes).
        /// By doing this we also define the name order. The order of the names is the order in which they appear in the ascii.

        ///
        bool bGeometryEnd = false;
        while(nPosition < sBuffer.size()){
            ReadUntilText(sID);
            if(sID == "endmodelgeom") // Don't break because we're also getting the total file size
                bGeometryEnd = true;
            if(bGeometryEnd || (sID != "node" && sID != "name"))
                SkipLine();
            else{
                //Once we get here, we have found a node. The name is only a keyword's strlen away!
                if(sID == "node"){
                    if(!ReadUntilText(sID)){
                        throw mdlexception("Malformed node declaration in ASCII name-index pass: node type is missing at line " + std::to_string(nLine) + ".");
                    }
                    nNodeCount++; //Also record the number of actual nodes
                }
                if(!ReadUntilText(sID, false)){
                    throw mdlexception("Malformed node/name declaration in ASCII name-index pass: name is missing at line " + std::to_string(nLine) + ".");
                }
                // v1.0.104b did not apply its 16-character warning to node
                // identifiers. Keep accepting the full binary-safe width here.
                TruncateLongName3216(sID, "ASCII node/name table entry", nLine, false);

                //Got it! Now to save it the name array. Do the uniqueness check
                // after enforcing the binary name width, otherwise two long
                // ASCII names with the same first 32 bytes would both resolve
                // to distinct in-memory names and then collide during binary
                // write.
                EnsureUniqueAsciiVisibleName(Data.MH.Names, sID, nLine);
                Name name; //our new name
                name.sName = sID;
                Data.MH.Names.push_back(std::move(name));
            }
        }
        if(Data.MH.Names.size() > static_cast<std::size_t>(std::numeric_limits<unsigned short>::max())){
            throw mdlexception("ASCII name table has too many entries to preserve 16-bit MDL name indices.");
        }
        ReportMdl << "Done indexing names (" << Data.MH.Names.size() << ").\n";

        /// Reset position and line because we need to start again from the beginning
        nPosition = 0, nLine = 1;

        ///Loops for every row
        auto CurrentAsciiNodeNameForError = [&]() -> std::string {
            if(nAnimation.Valid() && !Data.MH.Animations.empty() && !Data.MH.Animations.back().ArrayOfNodes.empty()){
                const Node & node = Data.MH.Animations.back().ArrayOfNodes.back();
                if(node.Head.nNameIndex.Valid()){
                    const unsigned short nName = static_cast<unsigned short>(node.Head.nNameIndex);
                    if(nName < Data.MH.Names.size()) return Data.MH.Names.at(nName).sName;
                }
            }
            if(nCurrentIndex.Valid()){
                const unsigned short nName = static_cast<unsigned short>(nCurrentIndex);
                if(nName < Data.MH.Names.size()) return Data.MH.Names.at(nName).sName;
            }
            return std::string("<unknown>");
        };

        bool * lpbList = nullptr;
        while(nPosition < sBuffer.size()){
            //First set our current list
            if(bKeys) lpbList = &bKeys;
            else if(bWeights) lpbList = &bWeights;
            else if(bEventlist) lpbList = &bEventlist;
            else if(bAabb) lpbList = &bAabb;
            else if(bSaberData) lpbList = &bSaberData;
            else if(bVerts) lpbList = &bVerts;
            else if(bFaces) lpbList = &bFaces;
            else if(bFaceExtra) lpbList = &bFaceExtra;
            else if(bTverts) lpbList = &bTverts;
            else if(bTexIndices1) lpbList = &bTexIndices1;
            else if(bTverts1) lpbList = &bTverts1;
            else if(bTexIndices2) lpbList = &bTexIndices2;
            else if(bTverts2) lpbList = &bTverts2;
            else if(bTexIndices3) lpbList = &bTexIndices3;
            else if(bTverts3) lpbList = &bTverts3;
            else if(bNormalIndices) lpbList = &bNormalIndices;
            else if(bNormals) lpbList = &bNormals;
            else if(bTangentBasis) lpbList = &bTangentBasis;
            else if(bCompactBoneMap) lpbList = &bCompactBoneMap;
            else if(bCompactBoneMapRaw) lpbList = &bCompactBoneMapRaw;
            else if(bBoneConstantIndices) lpbList = &bBoneConstantIndices;
            else if(bExtraData) lpbList = &bExtraData;
            else if(bConstraints) lpbList = &bConstraints;
            else if(bFlarePositions) lpbList = &bFlarePositions;
            else if(bFlareSizes) lpbList = &bFlareSizes;
            else if(bFlareColorShifts) lpbList = &bFlareColorShifts;
            else if(bTextureNames) lpbList = &bTextureNames;
            else if(bControllerRaw) lpbList = &bControllerRaw;
            else if(bControllerDataRaw) lpbList = &bControllerDataRaw;
            else if(bRoomLinks) lpbList = &bRoomLinks;
            else if(bColors) lpbList = &bColors;
            else if(bColorIndices) lpbList = &bColorIndices;
            else lpbList = nullptr;

            //First, check if we have a blank line, we'll just skip it here.
            if(EmptyRow())
                SkipLine();
            //Second, check if we are in some list of data. We should not look for a keyword then.
            else if(lpbList != nullptr){
                if(nDataMax.Valid() && nDataMax == 0){
                    const unsigned int nSavePos = nPosition;
                    if(ReadUntilText(sID) && sID == "endlist"){
                        SkipLine();
                    }
                    else{
                        nPosition = nSavePos;
                    }
                    *lpbList = false;
                    continue;
                }

                //First, check if we're done. Save your position, cuz we'll need to revert if we're not done
                int nSavePos = nPosition;
                ReadUntilText(sID);
                if(sID == "endlist" || sID == "endnode"){
                    *lpbList = false;
                    if(sID == "endnode") nNode = 0;
                    SkipLine();
                }
                else{
                    //Revert back to old position
                    nPosition = nSavePos;

                    /// Read the data
                    try{
                    if(bKeys){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading key data " << nDataCounter << ".\n";
                        //We've already read the timekeys, we're left with the values
                        Animation & anim = Data.MH.Animations.back();
                        Node & node = anim.ArrayOfNodes.back();
                        int nNodeIndex = Mdl.GetNodeIndexByNameIndex(node.Head.nNameIndex);
                        if(nNodeIndex == -1) throw mdlexception("ReadAscii() reading keys error: dealing with a name index that does not have a node in geometry.");
                        Node & geonode = Data.MH.ArrayOfNodes.at(nNodeIndex);
                        Location loc = geonode.GetLocation();
                        Controller & ctrl = node.Head.Controllers.back();

                        //First read the timekey, also check that we're valid
                        if(!ReadFloat(fConvert)) throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");

                        if(ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION && ctrl.nColumnCount < 16){
                            double fX, fY, fZ, fA;
                            if(ReadFloat(fConvert)) fX = fConvert;
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fY = fConvert;
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fZ = fConvert;
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fA = fConvert;
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            Orientation NewOrientKey;
                            NewOrientKey.SetAxisAngle(fX, fY, fZ, fA);
                            if(Data.MH.bCompressQuaternions){
                                Quaternion q = NewOrientKey.GetQuaternion();
                                if(q.fW < 0.0) q.vAxis *= -1.0;
                                unsigned nCompressed = CompressQuaternion(q);
                                node.Head.ControllerData.push_back(FloatFromBits(nCompressed));
                            }
                            else{
                                node.Head.ControllerData.push_back(NewOrientKey.GetQuaternion().vAxis.fX);
                                node.Head.ControllerData.push_back(NewOrientKey.GetQuaternion().vAxis.fY);
                                node.Head.ControllerData.push_back(NewOrientKey.GetQuaternion().vAxis.fZ);
                                node.Head.ControllerData.push_back(NewOrientKey.GetQuaternion().fW);
                            }
                        }
                        else if(ctrl.nControllerType == CONTROLLER_HEADER_POSITION){
                            if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert - loc.vPosition.fX);
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert - loc.vPosition.fY);
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert - loc.vPosition.fZ);
                            else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ctrl.nColumnCount > 16){
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                                if(ReadFloat(fConvert)) node.Head.ControllerData.push_back(fConvert);
                                else throw mdlexception("Error reading animation key data for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                        }
                        else{
                            while(true){
                                if(ReadFloat(fConvert)){
                                    //ReportMdl << "Reading non-position keys...\n";
                                    node.Head.ControllerData.push_back(fConvert);
                                }
                                else break;
                            }
                        }
                    }
                    else if(bEventlist){
                        Animation & anim = Data.MH.Animations.back();
                        Event sound; //New sound

                        if(ReadFloat(fConvert)) sound.fTime = fConvert;
                        else throw mdlexception("Error reading eventlist data for animation '" + anim.sName + "'. Event time missing.");

                        std::string sValue;
                        if(!ReadUntilText(sValue, false)){
                            ReportMdl << "ReadUntilText() Event name is missing!\n";
                            throw mdlexception("Error reading eventlist data for animation '" + std::string(anim.sName.c_str()) + "'. Event name missing");
                        }
                        TruncateLongName3216(sValue, "Event name in " + std::string(anim.sName.c_str()), nLine);
                        sound.sName = sValue;

                        anim.Events.push_back(sound);
                    }
                    else if(bAabb){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Aabb aabb;

                        if(ReadFloat(fConvert)) aabb.vBBmin.fX = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmin x) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) aabb.vBBmin.fY = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmin y) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) aabb.vBBmin.fZ = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmin z) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) aabb.vBBmax.fX = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmax x) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) aabb.vBBmax.fY = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmax y) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) aabb.vBBmax.fZ = fConvert;
                        else throw mdlexception("Error reading aabb data (bbmax z) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) aabb.nID = AsciiUShortOrInvalid(nConvert, "aabb face id");
                        else throw mdlexception("Error reading aabb data (face id) for node '" + Mdl.GetNodeName(node) + "'.");

                        int nOptionalStart = nPosition;
                        if(ReadInt(nConvert)){
                            if(nConvert < -1){
                                throw mdlexception("Error reading aabb data for node '" + Mdl.GetNodeName(node) + "': property must be -1 or non-negative.");
                            }
                            aabb.nProperty = static_cast<unsigned int>(nConvert);
                            int nChild1 = 0;
                            int nChild2 = 0;
                            if(ReadInt(nChild1) && ReadInt(nChild2)){
                                if((nChild1 != 0 && nChild1 != 1) || (nChild2 != 0 && nChild2 != 1)){
                                    throw mdlexception("Error reading aabb data for node '" + Mdl.GetNodeName(node) + "': child flags must be 0 or 1.");
                                }
                                aabb.nChild1 = static_cast<unsigned int>(nChild1);
                                aabb.nChild2 = static_cast<unsigned int>(nChild2);
                                node.Walkmesh.bPreserveAabbTree = true;
                            }
                            else{
                                // Old ASCII aabb format only carried the first seven values.
                                // Keep accepting it, but let postprocess rebuild the tree.
                                nPosition = nOptionalStart;
                                node.Walkmesh.bPreserveAabbTree = false;
                            }
                        }
                        else{
                            nPosition = nOptionalStart;
                            node.Walkmesh.bPreserveAabbTree = false;
                        }

                        node.Walkmesh.TempAabbNodes.push_back(std::move(aabb));
                    }
                    else if(bVerts){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading verts data " << nDataCounter << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        Vector vert;
                        if(ReadFloat(fConvert)) vert.fX = fConvert;
                        else throw mdlexception("Error reading vert coord X data for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vert.fY = fConvert;
                        else throw mdlexception("Error reading vert coord Y data for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vert.fZ = fConvert;
                        else throw mdlexception("Error reading vert coord Z data for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempVerts.push_back(std::move(vert));
                    }
                    else if(bFaces){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading faces data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Face face;
                        face.nNameIndex = nCurrentIndex;

                        //Currently we read the regular NWMax version with only a single set of tvert indices
                        if(ReadInt(nConvert)) face.nIndexVertex[0] = AsciiUShortOrInvalid(nConvert, "face vertex index 1");
                        else throw mdlexception("Error reading face data (vert index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) face.nIndexVertex[1] = AsciiUShortOrInvalid(nConvert, "face vertex index 2");
                        else throw mdlexception("Error reading face data (vert index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) face.nIndexVertex[2] = AsciiUShortOrInvalid(nConvert, "face vertex index 3");
                        else throw mdlexception("Error reading face data (vert index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)) face.nSmoothingGroup = uConvert;
                        else throw mdlexception("Error reading face data (smoothing group) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) face.nIndexTvert[0] = AsciiUShortOrInvalid(nConvert, "face tvert index 1");
                        else throw mdlexception("Error reading face data (tvert index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) face.nIndexTvert[1] = AsciiUShortOrInvalid(nConvert, "face tvert index 2");
                        else throw mdlexception("Error reading face data (tvert index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) face.nIndexTvert[2] = AsciiUShortOrInvalid(nConvert, "face tvert index 3");
                        else throw mdlexception("Error reading face data (tvert index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                        face.nTextureCount++;
                        if(ReadInt(nConvert)) face.nMaterialID = AsciiUIntOrInvalid(nConvert, "face material id");
                        else throw mdlexception("Error reading face data (material ID) for node '" + Mdl.GetNodeName(node) + "'.");

                        if(bMagnusll){ //read tverts magnusll style
                            int ntv1ind1, ntv1ind2, ntv1ind3;
                            //bitmap2
                            bool bTry = true;
                            if(ReadInt(nConvert)) ntv1ind1 = nConvert;
                            else bTry = false;
                            if(ReadInt(nConvert)) ntv1ind2 = nConvert;
                            else bTry = false;
                            if(ReadInt(nConvert)) ntv1ind3 = nConvert;
                            else bTry = false;
                            if(bTry){
                                face.nIndexTvert1[0] = AsciiUShortOrInvalid(ntv1ind1, "bitmap2 tvert index 1");
                                face.nIndexTvert1[1] = AsciiUShortOrInvalid(ntv1ind2, "bitmap2 tvert index 2");
                                face.nIndexTvert1[2] = AsciiUShortOrInvalid(ntv1ind3, "bitmap2 tvert index 3");
                                face.nTextureCount++;

                                //texture0
                                if(ReadInt(nConvert)) ntv1ind1 = nConvert;
                                else bTry = false;
                                if(ReadInt(nConvert)) ntv1ind2 = nConvert;
                                else bTry = false;
                                if(ReadInt(nConvert)) ntv1ind3 = nConvert;
                                else bTry = false;
                                if(bTry){
                                    face.nIndexTvert2[0] = AsciiUShortOrInvalid(ntv1ind1, "texture0 tvert index 1");
                                    face.nIndexTvert2[1] = AsciiUShortOrInvalid(ntv1ind2, "texture0 tvert index 2");
                                    face.nIndexTvert2[2] = AsciiUShortOrInvalid(ntv1ind3, "texture0 tvert index 3");
                                    face.nTextureCount++;

                                    //texture1
                                    if(ReadInt(nConvert)) ntv1ind1 = nConvert;
                                    else bTry = false;
                                    if(ReadInt(nConvert)) ntv1ind2 = nConvert;
                                    else bTry = false;
                                    if(ReadInt(nConvert)) ntv1ind3 = nConvert;
                                    else bTry = false;
                                    if(bTry){
                                        face.nIndexTvert3[0] = AsciiUShortOrInvalid(ntv1ind1, "texture1 tvert index 1");
                                        face.nIndexTvert3[1] = AsciiUShortOrInvalid(ntv1ind2, "texture1 tvert index 2");
                                        face.nIndexTvert3[2] = AsciiUShortOrInvalid(ntv1ind3, "texture1 tvert index 3");
                                        face.nTextureCount++;
                                    }
                                }
                            }
                        }

                        node.Mesh.Faces.push_back(face);
                    }
                    else if(bFaceExtra){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);
                            Vector vPreservedNormal;
                            double fPreservedDistance = 0.0;
                            std::array<MdlInteger<unsigned short>, 3> nPreservedAdjacentFaces;

                            if(ReadFloat(fConvert)) vPreservedNormal.fX = fConvert;
                            else throw mdlexception("Error reading faceextra data (normal X) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) vPreservedNormal.fY = fConvert;
                            else throw mdlexception("Error reading faceextra data (normal Y) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) vPreservedNormal.fZ = fConvert;
                            else throw mdlexception("Error reading faceextra data (normal Z) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fPreservedDistance = fConvert;
                            else throw mdlexception("Error reading faceextra data (distance) for node '" + Mdl.GetNodeName(node) + "'.");

                            if(ReadInt(nConvert)) nPreservedAdjacentFaces[0] = AsciiUShortOrInvalid(nConvert, "faceextra adjacent index 1");
                            else throw mdlexception("Error reading faceextra data (adjacent 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nPreservedAdjacentFaces[1] = AsciiUShortOrInvalid(nConvert, "faceextra adjacent index 2");
                            else throw mdlexception("Error reading faceextra data (adjacent 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nPreservedAdjacentFaces[2] = AsciiUShortOrInvalid(nConvert, "faceextra adjacent index 3");
                            else throw mdlexception("Error reading faceextra data (adjacent 3) for node '" + Mdl.GetNodeName(node) + "'.");

                            face.bPreserveRuntimeFaceData = true;
                            face.vPreservedNormal = vPreservedNormal;
                            face.fPreservedDistance = fPreservedDistance;
                            face.nPreservedAdjacentFaces = nPreservedAdjacentFaces;
                            face.vNormal = face.vPreservedNormal;
                            face.fDistance = face.fPreservedDistance;
                            face.nAdjacentFaces = face.nPreservedAdjacentFaces;
                        }
                        else{
                            ThrowTooManyListRows("faceextra", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bTverts){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading tverts data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Vector vUV;
                        if(ReadFloat(fConvert)) vUV.fX = fConvert;
                        else throw mdlexception("Error reading tvert data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vUV.fY = fConvert;
                        else throw mdlexception("Error reading tvert data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempTverts.push_back(vUV);
                    }
                    else if(bTexIndices1){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);

                            std::array<MdlInteger<unsigned short>, 3> nIndexTvert1;
                            if(ReadInt(nConvert)) nIndexTvert1[0] = AsciiUShortOrInvalid(nConvert, "texindices1 index 1");
                            else throw mdlexception("Error reading texindices1 data (index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert1[1] = AsciiUShortOrInvalid(nConvert, "texindices1 index 2");
                            else throw mdlexception("Error reading texindices1 data (index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert1[2] = AsciiUShortOrInvalid(nConvert, "texindices1 index 3");
                            else throw mdlexception("Error reading texindices1 data (index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                            face.nIndexTvert1 = nIndexTvert1;
                            face.nTextureCount++;
                        }
                        else{
                            ThrowTooManyListRows("texindices1", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bTverts1){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading tverts data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Vector vUV;
                        if(ReadFloat(fConvert)) vUV.fX = fConvert;
                        else throw mdlexception("Error reading tverts1 data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vUV.fY = fConvert;
                        else throw mdlexception("Error reading tverts1 data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempTverts1.push_back(vUV);
                    }
                    else if(bTexIndices2){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);
                            std::array<MdlInteger<unsigned short>, 3> nIndexTvert2;
                            if(ReadInt(nConvert)) nIndexTvert2[0] = AsciiUShortOrInvalid(nConvert, "texindices2 index 1");
                            else throw mdlexception("Error reading texindices2 data (index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert2[1] = AsciiUShortOrInvalid(nConvert, "texindices2 index 2");
                            else throw mdlexception("Error reading texindices2 data (index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert2[2] = AsciiUShortOrInvalid(nConvert, "texindices2 index 3");
                            else throw mdlexception("Error reading texindices2 data (index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                            face.nIndexTvert2 = nIndexTvert2;
                            face.nTextureCount++;
                        }
                        else{
                            ThrowTooManyListRows("texindices2", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bTverts2){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading tverts data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Vector vUV;
                        if(ReadFloat(fConvert)) vUV.fX = fConvert;
                        else throw mdlexception("Error reading tverts2 data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vUV.fY = fConvert;
                        else throw mdlexception("Error reading tverts2 data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempTverts2.push_back(vUV);
                    }
                    else if(bTexIndices3){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);
                            std::array<MdlInteger<unsigned short>, 3> nIndexTvert3;
                            if(ReadInt(nConvert)) nIndexTvert3[0] = AsciiUShortOrInvalid(nConvert, "texindices3 index 1");
                            else throw mdlexception("Error reading texindices3 data (index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert3[1] = AsciiUShortOrInvalid(nConvert, "texindices3 index 2");
                            else throw mdlexception("Error reading texindices3 data (index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexTvert3[2] = AsciiUShortOrInvalid(nConvert, "texindices3 index 3");
                            else throw mdlexception("Error reading texindices3 data (index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                            face.nIndexTvert3 = nIndexTvert3;
                            face.nTextureCount++;
                        }
                        else{
                            ThrowTooManyListRows("texindices3", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bTverts3){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading tverts data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Vector vUV;
                        if(ReadFloat(fConvert)) vUV.fX = fConvert;
                        else throw mdlexception("Error reading tvert3 data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vUV.fY = fConvert;
                        else throw mdlexception("Error reading tvert3 data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempTverts3.push_back(vUV);
                    }
                    else if(bNormalIndices){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);

                            std::array<MdlInteger<unsigned short>, 3> nIndexNormal;
                            if(ReadInt(nConvert)) nIndexNormal[0] = AsciiUShortOrInvalid(nConvert, "normalindices index 1");
                            else throw mdlexception("Error reading normalindices data (index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexNormal[1] = AsciiUShortOrInvalid(nConvert, "normalindices index 2");
                            else throw mdlexception("Error reading normalindices data (index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexNormal[2] = AsciiUShortOrInvalid(nConvert, "normalindices index 3");
                            else throw mdlexception("Error reading normalindices data (index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                            face.nIndexNormal = nIndexNormal;
                        }
                        else{
                            ThrowTooManyListRows("normalindices", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bNormals){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        Vector vNormal;
                        if(ReadFloat(fConvert)) vNormal.fX = fConvert;
                        else throw mdlexception("Error reading normal data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vNormal.fY = fConvert;
                        else throw mdlexception("Error reading normal data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) vNormal.fZ = fConvert;
                        else throw mdlexception("Error reading normal data (coord Z) for node '" + Mdl.GetNodeName(node) + "'.");
                        node.Mesh.TempNormals.push_back(vNormal);
                    }
                    else if(bTangentBasis){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::array<Vector, 3> basis;
                        for(int nBasis = 0; nBasis < 3; nBasis++){
                            if(ReadFloat(fConvert)) basis.at(nBasis).fX = fConvert;
                            else throw mdlexception("Error reading tangentbasis data (coord X) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) basis.at(nBasis).fY = fConvert;
                            else throw mdlexception("Error reading tangentbasis data (coord Y) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) basis.at(nBasis).fZ = fConvert;
                            else throw mdlexception("Error reading tangentbasis data (coord Z) for node '" + Mdl.GetNodeName(node) + "'.");
                        }
                        node.Mesh.TempTangent1.push_back(basis);
                    }
                    else if(bSaberData){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        VertexData sd;
                        if(ReadFloat(fConvert)) sd.vVertex.fX = fConvert;
                        else throw mdlexception("Error reading saberdata vertex X for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vVertex.fY = fConvert;
                        else throw mdlexception("Error reading saberdata vertex Y for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vVertex.fZ = fConvert;
                        else throw mdlexception("Error reading saberdata vertex Z for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vUV1.fX = fConvert;
                        else throw mdlexception("Error reading saberdata UV U for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vUV1.fY = fConvert;
                        else throw mdlexception("Error reading saberdata UV V for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vNormal.fX = fConvert;
                        else throw mdlexception("Error reading saberdata normal X for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vNormal.fY = fConvert;
                        else throw mdlexception("Error reading saberdata normal Y for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) sd.vNormal.fZ = fConvert;
                        else throw mdlexception("Error reading saberdata normal Z for node '" + Mdl.GetNodeName(node) + "'.");
                        node.Saber.SaberData.push_back(std::move(sd));
                        node.Saber.bPreserveSaberData = true;
                    }
                    else if(bColorIndices){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        if(nDataCounter < node.Mesh.Faces.size()){
                            Face & face = node.Mesh.Faces.at(nDataCounter);

                            std::array<MdlInteger<unsigned short>, 3> nIndexColor;
                            if(ReadInt(nConvert)) nIndexColor[0] = AsciiUShortOrInvalid(nConvert, "colorindices index 1");
                            else throw mdlexception("Error reading color indices data (index 1) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexColor[1] = AsciiUShortOrInvalid(nConvert, "colorindices index 2");
                            else throw mdlexception("Error reading color indices data (index 2) for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadInt(nConvert)) nIndexColor[2] = AsciiUShortOrInvalid(nConvert, "colorindices index 3");
                            else throw mdlexception("Error reading color indices data (index 3) for node '" + Mdl.GetNodeName(node) + "'.");
                            face.nIndexColor = nIndexColor;
                            face.nTextureCount++;
                        }
                        else{
                            ThrowTooManyListRows("colorindices", Mdl.GetNodeName(node));
                        }
                    }
                    else if(bColors){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading color data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        Color cColor;
                        if(ReadFloat(fConvert)) cColor.fR = fConvert;
                        else throw mdlexception("Error reading color data (1) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) cColor.fG = fConvert;
                        else throw mdlexception("Error reading color data (2) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) cColor.fB = fConvert;
                        else throw mdlexception("Error reading color data (3) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Mesh.TempColors.push_back(std::move(cColor));
                    }
                    else if(bConstraints){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading constraints data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Dangly.TempConstraints.push_back(fConvert);
                        else throw mdlexception("Error reading constraint data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bWeights){
                        //if(DEBUG_LEVEL > 3) ReportMdl << "Reading weights data" << "" << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        //node.Skin.Bones[nCurrentIndex].nNameIndex = nCurrentIndex;
                        int z = 0;
                        int nNameIndex = 0;
                        Weight weight;
                        //std::vector<int> & nWeightIndices = node.Skin.BoneNameIndices;
                        while(z < 4){
                            //Get first name
                            if(!ReadUntilText(sID, false)) break;
                            std::string sCheck (sID);
                            ToLowerInPlace(sCheck);
                            if(sCheck != "root" && sCheck != "null" && sCheck != "-1"){
                                TruncateLongName3216(sID, "weight bone name", nLine, false);
                            }

                            // Resolve every active weight through the ASCII name table.
                            // Older code special-cased the literal token "root" by
                            // reading its numeric value but never storing either the
                            // bone index or the weight, which silently converted an
                            // active influence into an inactive zero-weight slot.
                            for(nNameIndex = 0; nNameIndex < static_cast<int>(Data.MH.Names.size()); nNameIndex++){
                                if(StringEqual(Data.MH.Names[nNameIndex].sName, sID)) break;
                            }
                            if(nNameIndex == static_cast<int>(Data.MH.Names.size())){
                                ReportMdl << "Reading weights data: failed to find name in name array! Name: " << sID << ".\n";
                                throw mdlexception("Error reading weight data for node '" + Mdl.GetNodeName(node) + "'. Could not find bone '" + sID + "' by name.");
                            }

                            weight.nWeightIndex[z] = AsciiUShortIndex(static_cast<unsigned int>(nNameIndex), "weight bone name index");
                            if(ReadFloat(fConvert)) weight.fWeightValue[z] = fConvert;
                            else throw mdlexception("Error reading weight data for node '" + Mdl.GetNodeName(node) + "'. No weight value for bone '" + sID + "'.");
                            z++;
                        }
                        if(z < 1){
                            //This means we exited before writing a single piece of data
                            ReportMdl << "Didn't even find one name" << "" << ".\n";
                            ReportMdl << "DataCounter: " << nDataCounter << ". DataMax: " << nDataMax.Print() << ".\n";
                            throw mdlexception("Error reading weight data for node '" + Mdl.GetNodeName(node) + "'. No weights specified for a vertex.");
                        }
                        else{
                            /// Here we assume that everything went fine, so we add the weight to our list
                            node.Skin.TempWeights.push_back(std::move(weight));
                        }
                    }
                    else if(bCompactBoneMap){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(!ReadUntilText(sID, false)) throw mdlexception("Error reading compactbonemap data for node '" + Mdl.GetNodeName(node) + "'.");

                        std::string sCheck(sID);
                        ToLowerInPlace(sCheck);
                        if(sCheck != "root" && sCheck != "null" && sCheck != "-1"){
                            TruncateLongName3216(sID, "compactbonemap bone name", nLine, false);
                            sCheck = sID;
                            ToLowerInPlace(sCheck);
                        }
                        MdlInteger<unsigned short> nNameIndex;
                        if(sCheck != "root" && sCheck != "null" && sCheck != "-1"){
                            bool bFoundName = false;
                            for(unsigned int n = 0; n < Data.MH.Names.size(); n++){
                                if(StringEqual(Data.MH.Names.at(n).sName, sID)){
                                    nNameIndex = AsciiUShortIndex(n, "compactbonemap bone name index");
                                    bFoundName = true;
                                    break;
                                }
                            }
                            if(!bFoundName) throw mdlexception("compactbonemap references unknown bone '" + sID + "' on node '" + Mdl.GetNodeName(node) + "'.");
                        }

                        EnsureAsciiVectorSize(node.Skin.TempCompactBoneNameIndices, static_cast<std::size_t>(nDataCounter) + 1u, "compactbonemap");
                        node.Skin.TempCompactBoneNameIndices.at(nDataCounter) = nNameIndex;
                    }
                    else if(bCompactBoneMapRaw){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("compactbonemapraw value is outside uint16 range.");
                            EnsureAsciiVectorSize(node.Skin.TempCompactBoneRawIndices, static_cast<std::size_t>(nDataCounter) + 1u, "compactbonemapraw");
                            node.Skin.TempCompactBoneRawIndices.at(nDataCounter) = static_cast<unsigned short>(nConvert);
                        }
                        else throw mdlexception("Error reading compactbonemapraw data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bBoneConstantIndices){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadUInt(uConvert)){
                            if(nDataCounter >= node.Skin.Bones.size()){
                                throw mdlexception("boneconstantindices row exceeds the current skin bone table for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                            node.Skin.Bones.at(nDataCounter).nPadding = uConvert;
                        }
                        else throw mdlexception("Error reading boneconstantindices data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bExtraData){
                        Animation & anim = Data.MH.Animations.back();
                        Node & node = anim.ArrayOfNodes.back();
                        if(ReadFloat(fConvert)){
                            EnsureAsciiVectorSize(node.Head.ControllerData, static_cast<std::size_t>(nDataCounter) + 1u, 0.0, "extra_data");
                            node.Head.ControllerData.at(nDataCounter) = fConvert;
                        }
                        else throw mdlexception("Error reading extra_data payload for animation node '" + Data.MH.Names.at(node.Head.nNameIndex).sName + "'.");
                    }
                    else if(bControllerRaw){
                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Controller ctrl;
                        if(ReadUInt(uConvert)) ctrl.nControllerType = uConvert;
                        else throw mdlexception("Error reading controllerraw type for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 65535) throw mdlexception("controllerraw unknown2 is outside uint16 range.");
                            ctrl.nUnknown2 = static_cast<unsigned short>(uConvert);
                        }
                        else throw mdlexception("Error reading controllerraw unknown2 for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 65535) throw mdlexception("controllerraw value count is outside uint16 range.");
                            ctrl.nValueCount = static_cast<unsigned short>(uConvert);
                        }
                        else throw mdlexception("Error reading controllerraw value count for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 65535) throw mdlexception("controllerraw timekey start is outside uint16 range.");
                            ctrl.nTimekeyStart = static_cast<unsigned short>(uConvert);
                        }
                        else throw mdlexception("Error reading controllerraw timekey start for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 65535) throw mdlexception("controllerraw data start is outside uint16 range.");
                            ctrl.nDataStart = static_cast<unsigned short>(uConvert);
                        }
                        else throw mdlexception("Error reading controllerraw data start for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 255) throw mdlexception("controllerraw column count is outside uint8 range.");
                            ctrl.nColumnCount = static_cast<unsigned char>(uConvert);
                        }
                        else throw mdlexception("Error reading controllerraw column count for node '" + Mdl.GetNodeName(node) + "'.");
                        for(int nPad = 0; nPad < 3; nPad++){
                            if(ReadUInt(uConvert)){
                                if(uConvert > 255) throw mdlexception("controllerraw padding byte is outside uint8 range.");
                                ctrl.nPadding.at(nPad) = static_cast<char>(static_cast<unsigned char>(uConvert));
                            }
                            else throw mdlexception("Error reading controllerraw padding for node '" + Mdl.GetNodeName(node) + "'.");
                        }
                        ctrl.nNameIndex = node.Head.nNameIndex;
                        ctrl.nAnimation = nAnimation;
                        node.Head.Controllers.push_back(ctrl);
                    }
                    else if(bControllerDataRaw){
                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadUInt(uConvert)) node.Head.ControllerData.push_back(FloatFromBits(uConvert));
                        else throw mdlexception("Error reading controllerdataraw data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bTextureNames){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            Name name;
                            name.sName = sValue;
                            node.Light.FlareTextureNames.push_back(name);
                        }
                        else throw mdlexception("Error reading flare texture name data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bFlareSizes){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Light.FlareSizes.push_back(fConvert);
                        else throw mdlexception("Error reading flare size data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bFlarePositions){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Light.FlarePositions.push_back(fConvert);
                        else throw mdlexception("Error reading flare position data for node '" + Mdl.GetNodeName(node) + "'.");
                    }
                    else if(bFlareColorShifts){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        float f1, f2, f3;
                        if(ReadFloat(fConvert)) f1 = fConvert;
                        else throw mdlexception("Error reading flare color shifts data (1) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) f2 = fConvert;
                        else throw mdlexception("Error reading flare color shifts data (2) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) f3 = fConvert;
                        else throw mdlexception("Error reading flare color shifts data (3) for node '" + Mdl.GetNodeName(node) + "'.");

                        node.Light.FlareColorShifts.push_back(Color(f1, f2, f3));
                    }
                    else if(bRoomLinks){
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Edge edge;
                        if(ReadInt(nConvert)){
                            if(nConvert < 0) throw mdlexception("roomlink edge index cannot be negative.");
                            edge.nIndex = static_cast<unsigned int>(nConvert);
                        }
                        else throw mdlexception("Error reading roomlink data (index) for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadInt(nConvert)) edge.nTransition = AsciiUIntOrInvalid(nConvert, "roomlink transition");
                        else throw mdlexception("Error reading roomlink data (transition) for node '" + Mdl.GetNodeName(node) + "'.");

                        const unsigned int nFaceIndex = static_cast<unsigned int>(edge.nIndex) / 3u;
                        const unsigned int nEdgeIndex = static_cast<unsigned int>(edge.nIndex) % 3u;
                        if(nFaceIndex >= node.Mesh.Faces.size()) throw mdlexception("roomlink edge index references a face outside the mesh.");
                        node.Mesh.Faces.at(nFaceIndex).nEdgeTransitions.at(nEdgeIndex) = edge.nTransition;
                    }
                    }
                    catch(const std::exception & e){
                        std::string sData;
                        if(bRoomLinks) sData = "room link";
                        else if(bVerts) sData = "vert";
                        else if(bFaces) sData = "face";
                        else if(bFaceExtra) sData = "faceextra";
                        else if(bTverts) sData = "tvert";
                        else if(bTverts1) sData = "tvert1";
                        else if(bTverts2) sData = "tvert2";
                        else if(bTverts3) sData = "tvert3";
                        else if(bNormalIndices) sData = "normalindices";
                        else if(bNormals) sData = "normal";
                        else if(bTangentBasis) sData = "tangentbasis";
                        else if(bAabb) sData = "aabb";
                        else if(bSaberData) sData = "saberdata";
                        else if(bWeights) sData = "weight";
                        else if(bCompactBoneMap) sData = "compactbonemap";
                        else if(bCompactBoneMapRaw) sData = "compactbonemapraw";
                        else if(bBoneConstantIndices) sData = "boneconstantindices";
                        else if(bExtraData) sData = "extra_data";
                        else if(bConstraints) sData = "constraint";
                        else if(bEventlist) sData = "eventlist";
                        else if(bKeys) sData = "animation key";
                        else if(bTexIndices1) sData = "texindices1";
                        else if(bTexIndices2) sData = "texindices2";
                        else if(bTexIndices3) sData = "texindices3";
                        else if(bTextureNames) sData = "flare texture name";
                        else if(bControllerRaw) sData = "controllerraw";
                        else if(bControllerDataRaw) sData = "controllerdataraw";
                        else if(bFlareColorShifts) sData = "flare color shift";
                        else if(bFlarePositions) sData = "flare position";
                        else if(bFlareSizes) sData = "flare size";
                        throw mdlexception("Exception while reading " + sData + " data for node '" + CurrentAsciiNodeNameForError() + "': " + e.what());
                    }
                    nDataCounter++;
                    if(nDataMax.Valid() && nDataCounter >= nDataMax){
                        *lpbList = false;
                    }
                    SkipLine();
                }
            }
            //So no data. Find the next keyword then.
            else{
                //ReportMdl << "No data to read. Read keyword instead" << "" << ".\n";
                if(!ReadUntilText(sID)) SkipLine(); //Read the next token and skip line if no text, otherwise get into the main reading block.
                else{
                    /// Main header stuff
                    if(sID == "newmodel"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        //Read the model name
                        if(!ReadUntilText(sID, false)){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Model name is missing!\n";
                            throw mdlexception("No specified model name.");
                        }

                        TruncateLongName3216(sID, "Model name", nLine);
                        Data.MH.GH.sName = sID;

                        //Initialize all header information
                        Data.MH.vBBmin.fX = -5.0;
                        Data.MH.vBBmin.fY = -5.0;
                        Data.MH.vBBmin.fZ = -1.0;
                        Data.MH.vBBmax.fX = 5.0;
                        Data.MH.vBBmax.fY = 5.0;
                        Data.MH.vBBmax.fZ = 10.0;
                        Data.MH.fRadius = 7.0;
                        Data.MH.fScale = 1.0;
                        Data.MH.cSupermodelName = "NULL";
                        ResizeAsciiVector(Data.MH.ArrayOfNodes, Data.MH.Names.size(), "node array");

                        SkipLine();
                    }
                    else if(sID == "game" || sID == "targetgame" || sID == "kotorversion"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        if(ReadUntilText(sID, false)){
                            std::string sGame(sID);
                            ToLowerInPlace(sGame);
                            if(sGame == "1" || sGame == "k1" || sGame == "kotor1" || sGame == "swkotor"){
                                Mdl.bK2 = false;
                            }
                            else if(sGame == "2" || sGame == "k2" || sGame == "tsl" || sGame == "kotor2" || sGame == "swkotor2"){
                                Mdl.bK2 = true;
                            }
                            else{
                                ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! Unknown game '" << sID << "'. Keeping current game setting.\n";
                            }
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! Missing value for game. Keeping current game setting.\n";
                        SkipLine();
                    }
                    else if(sID == "platform" || sID == "targetplatform"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        if(ReadUntilText(sID, false)){
                            std::string sPlatform(sID);
                            ToLowerInPlace(sPlatform);
                            if(sPlatform == "pc" || sPlatform == "windows"){
                                Mdl.bXbox = false;
                            }
                            else if(sPlatform == "xbox" || sPlatform == "xb"){
                                Mdl.bXbox = true;
                            }
                            else{
                                ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! Unknown platform '" << sID << "'. Keeping current platform setting.\n";
                            }
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! Missing value for platform. Keeping current platform setting.\n";
                        SkipLine();
                    }
                    else if(sID == "setsupermodel"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        ReadUntilText(sID, false); //This should get us the model name first. I won't verify at this point.
                        if(!ReadUntilText(sID, false)){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Supermodel name is missing! Setting it to NULL instead.\n";
                            sID = "NULL";
                        }

                        TruncateLongName3216(sID, "Supermodel name", nLine);
                        Data.MH.cSupermodelName = sID;

                        SkipLine();
                    }
                    else if(sID == "classification"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        if(!ReadUntilText(sID)){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Classification is missing! Setting it to 'other' instead.\n";
                            sID = "other";
                            //throw mdlexception("No specified classification.");
                        }
                        if(sID == "other") Data.MH.nClassification = CLASS_OTHER;
                        else if(sID == "unknown"){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Corrected classification '" << sID << "' to classification 'other'.\n";
                            Data.MH.nClassification = CLASS_OTHER;
                        }
                        else if(sID == "effect") Data.MH.nClassification = CLASS_EFFECT;
                        else if(sID == "tile") Data.MH.nClassification = CLASS_TILE;
                        else if(sID == "character") Data.MH.nClassification = CLASS_CHARACTER;
                        else if(sID == "door") Data.MH.nClassification = CLASS_DOOR;
                        else if(sID == "placeable") Data.MH.nClassification = CLASS_PLACEABLE;
                        else if(sID == "lightsaber") Data.MH.nClassification = CLASS_LIGHTSABER;
                        else if(sID == "flyer") Data.MH.nClassification = CLASS_FLYER;
                        else{
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "ReadUntilText() has found a classification token that we cannot interpret: " << sID << "\n";
                            throw mdlexception("Unknown classification '" + sID + "'.");
                        }

                        SkipLine();
                    }
                    else if(sID == "classification_unk1"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) Data.MH.nSubclassification = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "ignorefog"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) Data.MH.nAffectedByFog = nConvert ? 0 : 1;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "setanimationscale"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadFloat(fConvert)) Data.MH.fScale = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "compress_quaternions"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) Data.MH.bCompressQuaternions = (nConvert == 0) ? false : true;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "headlink"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) Data.MH.bHeadLink = (nConvert == 0) ? false : true;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "totalnodes" || sID == "totalnodecount" || sID == "totalnodeswithsupermodel"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)){
                            if(nConvert < 0) throw mdlexception("totalnodes cannot be negative.");
                            Data.MH.GH.nTotalNumberOfNodes = static_cast<unsigned int>(nConvert);
                            Data.MH.bPreserveTotalNumberOfNodes = true;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using computed value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "geometryheaderpadding" || sID == "geometrypadding" || sID == "geometry_header_padding"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        for(int nPad = 0; nPad < 3; nPad++){
                            if(ReadInt(nConvert)){
                                if(nConvert < 0 || nConvert > 255) throw mdlexception("geometryheaderpadding value is outside byte range.");
                                Data.MH.GH.nPadding[nPad] = static_cast<unsigned char>(nConvert);
                            }
                            else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read geometry header padding byte " << nPad << ". Using default value instead.\n";
                        }
                        SkipLine();
                    }
                    else if(sID == "supermodelreference" || sID == "supermodelref" || sID == "supermodel_reference"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadUInt(uConvert)) Data.MH.nSupermodelReference = uConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "beginmodelgeom"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bGeometry = true;
                        SkipLine();
                    }
                    else if(sID == "layoutposition" && bGeometry && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadFloat(fConvert)) Data.MH.vLytPosition.fX = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float X. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vLytPosition.fY = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Y. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vLytPosition.fZ = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Z. Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "bmin" && bGeometry && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmin.fX = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float X. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmin.fY = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Y. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmin.fZ = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Z. Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "bmax" && bGeometry && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmax.fX = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float X. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmax.fY = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Y. Using default value instead.\n";
                        if(ReadFloat(fConvert)) Data.MH.vBBmax.fZ = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " float Z. Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "radius" && bGeometry && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadFloat(fConvert)) Data.MH.fRadius = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "name"){
                        // Name-only entries were already indexed in the first pass.
                        // They intentionally leave an empty geometry node slot so
                        // binary name indices remain stable.
                        SkipLine();
                    }
                    /// Common case for nodes, also for animation nodes
                    else if(sID == "node" && (bGeometry || bAnimation) && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        Node node; //our new node

                        //Read type
                        int nType;
                        if(!ReadUntilText(sID)){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "A node is without a type and name specification!\n";
                            throw mdlexception("Error reading node. No specified type.");
                        }
                        if(sID == "dummy")
                            nType = NODE_HEADER;
                        else if(sID == "light")
                            nType = NODE_HEADER | NODE_LIGHT;
                        else if(sID == "emitter")
                            nType = NODE_HEADER | NODE_EMITTER;
                        else if(sID == "reference")
                            nType = NODE_HEADER | NODE_REFERENCE;
                        else if(sID == "trimesh")
                            nType = NODE_HEADER | NODE_MESH;
                        else if(sID == "skin")
                            nType = NODE_HEADER | NODE_MESH | NODE_SKIN;
                        else if(sID == "danglymesh")
                            nType = NODE_HEADER | NODE_MESH | NODE_DANGLY;
                        else if(sID == "aabb")
                            nType = NODE_HEADER | NODE_MESH | NODE_AABB;
                        else if(sID == "lightsaber")
                            nType = NODE_HEADER | NODE_MESH | NODE_SABER;
                        else{
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "ReadUntilText() has found some text (type?) that we cannot interpret: " << sID << "\n";
                            throw mdlexception("Error reading node. Invalid type: " + sID + ".");
                        }

                        node.Head.nType = nType;

                        /// Read name
                        if(!ReadUntilText(sID, false)){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "A node is without a name!\n";
                            throw mdlexception("Error reading node. No specified name.");
                        }
                        TruncateLongName3216(sID, "ASCII node name", nLine, false);

                        /// Because the names are read in the same order as the nodes, we get the Name Index just by counting how many nodes (and names)
                        /// we've read so far. Since the names must be unique, we could also do it by searching through the names, but that is potentially
                        /// less secure, and it takes more time. However, this only works for geometry, once we get to animations we do need to look for
                        /// the names by searching for them.
                        if(bGeometry){
                            node.Head.nNameIndex = LookupAsciiNameIndex(Mdl, sID, "geometry node at line " + std::to_string(nLine));
                            //ReportMdl << "Reading node " << sID << " at line " << nLine << ".\n";
                        }
                        else{
                            node.Head.nNameIndex = LookupAsciiNameIndex(Mdl, sID, "animation node at line " + std::to_string(nLine));
                            //ReportMdl << "Reading animation node " << sID << " at line " << nLine << ".\n";
                        }

                        /// This is a placeholder value for the supernode number
                        node.Head.nSupernodeNumber = node.Head.nNameIndex;

                        /// Get animation number (automatically -1 if geo)
                        node.nAnimation = nAnimation;

                        //Initialize node <-- This is now mostly taken care of in the struct definitions, so no need for anything but the default values here.
                        if(nType & NODE_HEADER){
                            node.Head.vPos.Set(0.0, 0.0, 0.0);
                            node.Head.oOrient.SetQuaternion(0.0, 0.0, 0.0, 1.0);
                        }
                        if(nType & NODE_EMITTER){
                            //node.Emitter.cDepthTextureName = "NULL";
                        }
                        if(nType & NODE_MESH){
                            node.Mesh.nSaberUnknown1 = 3;
                        }

                        /// Finish up
                        if(bGeometry){
                            /// Update the current name index
                            nCurrentIndex = node.Head.nNameIndex;

                            /// Update the current node type
                            nNode = nType;

                            /// Append node at its indexed name-table slot.
                            Node & targetNodeSlot = Data.MH.ArrayOfNodes.at(static_cast<unsigned short>(node.Head.nNameIndex));
                            if(targetNodeSlot.Head.nType != 0 || targetNodeSlot.Head.nNameIndex.Valid()){
                                throw mdlexception("Duplicate geometry node for ASCII name '" + sID + "' at line " + std::to_string(nLine) +
                                                   "; refusing to overwrite the earlier node slot.");
                            }
                            targetNodeSlot = std::move(node);
                        }
                        else if(bAnimation){
                            /// Update the current name index
                            nCurrentIndex = node.Head.nNameIndex;

                            /// Update the current node type. Animation-node ASCII must resolve
                            /// to an existing geometry node; otherwise subsequent property/list
                            /// tokens would be interpreted using a stale previous node type.
                            nNode = 0;
                            bool bGeometryNodeFound = false;
                            for(const Node & node2 : Data.MH.ArrayOfNodes) if(node2.Head.nNameIndex == node.Head.nNameIndex){
                                if(node2.Head.nType == 0 || !node2.Head.nNameIndex.Valid()) break;
                                nNode = node2.Head.nType;
                                bGeometryNodeFound = true;
                                break;
                            }
                            if(!bGeometryNodeFound){
                                throw mdlexception("Animation node '" + sID + "' at line " + std::to_string(nLine) +
                                                   " does not resolve to a geometry node; refusing to parse it with stale node-type state.");
                            }

                            /// Append node
                            Animation & anim = Data.MH.Animations.back();
                            anim.ArrayOfNodes.push_back(std::move(node));
                        }

                        SkipLine();
                    }
                    else if(sID == "parent" && nNode & NODE_HEADER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(bGeometry){
                            Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                            if(ReadUntilText(sID, false)){
                                std::string sParentCheck = sID;
                                ToLowerInPlace(sParentCheck);
                                if(sParentCheck == "null") node.Head.nParentIndex = -1;
                                else{
                                    TruncateLongName3216(sID, "parent node name", nLine, false);
                                    unsigned short nNameIndex = 0;
                                    /// if we found a name, loop through the name array to find our name index
                                    while(nNameIndex < Data.MH.Names.size()){
                                        /// check if there is a match
                                        if(StringEqual(Data.MH.Names.at(nNameIndex).sName, sID)){
                                            /// We have found the name index for the current name, exit the loop with the current name index
                                            break;
                                        }
                                        else nNameIndex++;
                                    }
                                    if(nNameIndex == Data.MH.Names.size()){
                                        ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Parent node with name '" << sID << "' not found!\n";
                                        throw mdlexception("Error reading parent name for node '" + Mdl.GetNodeName(node) + "'. The specified parent name (" + sID + ") does not exist.");
                                    }
                                    else node.Head.nParentIndex = nNameIndex;
                                }
                            }
                            else{
                                ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Parent name missing!\n";
                                throw mdlexception("Error reading parent name for node '" + Mdl.GetNodeName(node) + "'. Parent name missing.");
                            }
                        }
                        else if(bAnimation){
                            Animation & anim = Data.MH.Animations.back();
                            Node & animnode = anim.ArrayOfNodes.back();
                            if(ReadUntilText(sID, false)){
                                std::string sParentCheck = sID;
                                ToLowerInPlace(sParentCheck);
                                if(sParentCheck == "null") animnode.Head.nParentIndex = -1;
                                else{
                                    TruncateLongName3216(sID, "animation parent node name", nLine, false);
                                    unsigned short nNameIndex = 0;
                                    /// if we found a name, loop through the name array to find our name index
                                    while(nNameIndex < Data.MH.Names.size()){
                                        /// check if there is a match
                                        if(StringEqual(Data.MH.Names[nNameIndex].sName, sID)){
                                            /// We have found the name index for the current name
                                            break;
                                        }
                                        else nNameIndex++;
                                    }
                                    if(nNameIndex == Data.MH.Names.size()){
                                        ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Parent node with name '" << sID << "' not found!\n";
                                        throw mdlexception("Error reading parent name for node '" + Data.MH.Names.at(animnode.Head.nNameIndex).sName + "'. The specified parent name (" + sID + ") does not exist.");
                                    }
                                    else animnode.Head.nParentIndex = nNameIndex;
                                }
                            }
                            else{
                                ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Parent name missing!\n";
                                throw mdlexception("Error reading parent name for node '" + Data.MH.Names.at(animnode.Head.nNameIndex).sName + "'. Parent name missing.");
                            }
                        }
                        SkipLine();
                    }
                    else if((sID == "nodepadding" || sID == "node_padding") && nNode & NODE_HEADER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("nodepadding is outside uint16 range.");
                            node.Head.nPadding1 = static_cast<unsigned short>(nConvert);
                        }
                        else throw mdlexception("Error reading nodepadding for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    /// Now come the various node fields
                    /// For LIGHT
                    else if(sID == "lightpriority" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nLightPriority = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "shadow" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nShadow = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "affectdynamic" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nAffectDynamic = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "ndynamictype" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nDynamicType = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "ambientonly" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nAmbientOnly = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "fadinglight" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nFadingLight = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if((sID == "lensflares" || sID == "flare") && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Light.nFlare = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "flareradius" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Light.fFlareRadius = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    /// For EMITTER
                    else if(sID == "deadspace" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Emitter.fDeadSpace = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "blastlength" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Emitter.fBlastLength = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "blastradius" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Emitter.fBlastRadius = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "numbranches" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nBranchCount = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "controlptsmoothing" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Emitter.fControlPointSmoothing = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "xgrid" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nxGrid = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "ygrid" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nyGrid = nConvert;
                        SkipLine();
                    }
                    else if(sID == "spawntype" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nSpawnType = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "twosidedtex" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nTwosidedTex = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "loop" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nLoop = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "renderorder" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nRenderOrder = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "m_bframeblending" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Emitter.nFrameBlending = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "m_sdepthtexturename" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Depth texture name (Emitter)", nLine);
                            node.Emitter.cDepthTextureName = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "update" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Update name (Emitter)", nLine, false);
                            node.Emitter.cUpdate = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "render" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Render name (Emitter)", nLine, false);
                            node.Emitter.cRender = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "blend" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Blend name (Emitter)", nLine, false);
                            node.Emitter.cBlend = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "texture" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Texture name (Emitter)", nLine);
                            node.Emitter.cTexture = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "chunkname" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            if(sValue.size() > 16){
                                Warning("ChunkName (" + sValue + ") at line " + std::to_string(nLine) + " is larger than the limit, 16 characters! Will truncate and continue.");
                                sValue.resize(16);
                            }
                            if(sValue.c_str() != std::string("NULL")) node.Emitter.cChunkName = sValue;
                            else node.Emitter.cChunkName = "";
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if((sID == "emitterpadding1" || sID == "emitter_padding1" || sID == "emitterpadding") && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 255) throw mdlexception("emitterpadding1 is outside byte range.");
                            node.Emitter.nPadding1 = static_cast<unsigned char>(nConvert);
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if((sID == "emitterflagsraw" || sID == "emitterflags" || sID == "emitter_flags") && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadUInt(uConvert)){
                            node.Emitter.nFlags = uConvert;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if((sID == "skinpadding1" || sID == "skin_padding1") && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("skinpadding1 is outside uint16 range.");
                            node.Skin.nPadding1 = static_cast<unsigned short>(nConvert);
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if((sID == "skinpadding2" || sID == "skin_padding2") && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("skinpadding2 is outside uint16 range.");
                            node.Skin.nPadding2 = static_cast<unsigned short>(nConvert);
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "p2p" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_P2P;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_P2P;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "p2p_sel" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_P2P_SEL;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_P2P_SEL;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "affectedbywind" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_AFFECTED_WIND;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_AFFECTED_WIND;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "m_istinted" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_TINTED;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_TINTED;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "bounce" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_BOUNCE;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_BOUNCE;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "random" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_RANDOM;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_RANDOM;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "inherit" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_INHERIT;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_INHERIT;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "inheritvel" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_INHERIT_VEL;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_INHERIT_VEL;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "inherit_local" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_INHERIT_LOCAL;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_INHERIT_LOCAL;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "splat" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_SPLAT;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_SPLAT;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "inherit_part" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_INHERIT_PART;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_INHERIT_PART;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "depth_texture" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_DEPTH_TEXTURE;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_DEPTH_TEXTURE;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "emitterflag13" && nNode & NODE_EMITTER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert) node.Emitter.nFlags = node.Emitter.nFlags | EMITTER_FLAG_13;
                            else node.Emitter.nFlags = node.Emitter.nFlags & ~EMITTER_FLAG_13;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    /// For REFERENCE
                    else if(sID == "refmodel" && nNode & NODE_REFERENCE){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            TruncateLongName3216(sValue, "Reference model name", nLine);
                            node.Reference.sRefModel = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "reattachable" && nNode & NODE_REFERENCE){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Reference.nReattachable = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    /// For MESH
                    else if(sID == "bitmap" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            if(sValue != "") node.Mesh.nTextureNumber++;
                            TruncateLongName3216(sValue, "Bitmap name", nLine);
                            node.Mesh.cTexture1 = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "bitmap2" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            if(sValue != "") node.Mesh.nTextureNumber++;
                            TruncateLongName3216(sValue, "Bitmap2 name", nLine);
                            node.Mesh.cTexture2 = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "texture0" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            if(sValue != "") node.Mesh.nTextureNumber++;
                            if(sValue.size() > 12){
                                Warning("Texture0 name (" + sValue + ") at line " + std::to_string(nLine) + " is larger than the limit, 12 characters! Will truncate and continue.");
                                sValue.resize(12);
                            }
                            node.Mesh.cTexture3 = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "texture1" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            if(sValue != "") node.Mesh.nTextureNumber++;
                            if(sValue.size() > 12){
                                Warning("Texture1 name (" + sValue + ") at line " + std::to_string(nLine) + " is larger than the limit, 12 characters! Will truncate and continue.");
                                sValue.resize(12);
                            }
                            node.Mesh.cTexture4 = sValue;
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "lightmap" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        std::string sValue;
                        if(ReadUntilText(sValue, false)){
                            bMagnusll = true;
                            if(sValue != "") node.Mesh.nTextureNumber++;
                            TruncateLongName3216(sValue, "Lightmap name", nLine);
                            node.Mesh.cTexture2 = sValue;
                            node.Mesh.nHasLightmap = 1; //Do this if we're using magnusII's version, cuz we won't have it separately
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "diffuse" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fDiffuse.fR = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) node.Mesh.fDiffuse.fG = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) node.Mesh.fDiffuse.fB = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if(sID == "ambient" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fAmbient.fR = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) node.Mesh.fAmbient.fG = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        if(ReadFloat(fConvert)) node.Mesh.fAmbient.fB = fConvert;
                        else throw mdlexception("Error reading '" + sID + "' for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if((sID == "meshpadding1" || sID == "mesh_padding1") && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 255) throw mdlexception("meshpadding1 is outside byte range.");
                            node.Mesh.nPadding1 = static_cast<char>(static_cast<unsigned char>(nConvert));
                        }
                        else throw mdlexception("Error reading meshpadding1 for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if((sID == "meshpadding2" || sID == "mesh_padding2") && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 255) throw mdlexception("meshpadding2 is outside byte range.");
                            node.Mesh.nPadding2 = static_cast<char>(static_cast<unsigned char>(nConvert));
                        }
                        else throw mdlexception("Error reading meshpadding2 for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if((sID == "meshpadding3" || sID == "mesh_padding3") && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < -32768 || nConvert > 32767) throw mdlexception("meshpadding3 is outside int16 range.");
                            node.Mesh.nPadding3 = static_cast<short>(nConvert);
                        }
                        else throw mdlexception("Error reading meshpadding3 for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if((sID == "meshpadding" || sID == "mesh_padding" || sID == "meshpadding4") && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadUInt(uConvert)){
                            node.Mesh.nPadding = static_cast<unsigned int>(uConvert);
                        }
                        else throw mdlexception("Error reading meshpadding for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if(sID == "tangentspace" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            node.Mesh.TangentSpace.at(0) = (nConvert != 0);
                            if(ReadInt(nConvert)){
                                node.Mesh.TangentSpace.at(1) = (nConvert != 0);
                                if(ReadInt(nConvert)){
                                    node.Mesh.TangentSpace.at(2) = (nConvert != 0);
                                    if(ReadInt(nConvert)){
                                        node.Mesh.TangentSpace.at(3) = (nConvert != 0);
                                    }
                                }
                            }
                        }
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "bumpmapped_texture"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        std::string sValue;
                        if(ReadUntilText(sValue, false)) sBumpmapped.push_back(sValue);
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "lightmapped" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nHasLightmap = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "rotatetexture" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nRotateTexture = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "m_bisbackgroundgeometry" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nBackgroundGeometry = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "dirt_enabled" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nDirtEnabled = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "dirt_texture" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nDirtTexture = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "dirt_worldspace" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nDirtCoordSpace = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "hologram_donotdraw" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nHideInHolograms = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "beaming" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nBeaming = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "render" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nRender = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "shadow" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nShadow = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "transparencyhint" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nTransparencyHint = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "inv_count" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(nNode & NODE_SABER){
                            if(ReadInt(nConvert)) node.Saber.nInvCount1 = nConvert;
                            else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " (1). Using default value instead.\n";
                            if(ReadInt(nConvert)) node.Saber.nInvCount2 = nConvert;
                            else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << " (2). Using default value instead.\n";
                        }
                        else if(ReadInt(nConvert)) node.Mesh.nMeshInvertedCounter = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "animateuv" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) node.Mesh.nAnimateUV = nConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "uvdirectionx" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fUVDirectionX = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "uvdirectiony" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fUVDirectionY = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "uvjitter" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fUVJitter = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "uvjitterspeed" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Mesh.fUVJitterSpeed = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    /// For DANGLY
                    else if(sID == "displacement" && nNode & NODE_DANGLY){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Dangly.fDisplacement = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "tightness" && nNode & NODE_DANGLY){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Dangly.fTightness = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "period" && nNode & NODE_DANGLY){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadFloat(fConvert)) node.Dangly.fPeriod = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    /// Next we have the DATA LISTS
                    else if(sID == "verts" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bVerts = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "faces" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bFaces = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "faceextra" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bFaceExtra = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "tverts" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTverts = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "texindices1" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTexIndices1 = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(((sID == "lightmaptverts") || (sID == "tverts1")) && (nNode & NODE_MESH)){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTverts1 = true;
                        if(sID == "lightmaptverts") bMagnusll = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "texindices2" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTexIndices2 = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "tverts2" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTverts2 = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "texindices3" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTexIndices3 = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "tverts3" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTverts3 = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "normalindices" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bNormalIndices = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "normals" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bNormals = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "tangentbasis" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        bTangentBasis = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        node.Mesh.TangentSpace.at(0) = true;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "colorindices" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bColorIndices = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "colors" && nNode & NODE_MESH){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bColors = true;
                        (void) Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "weights" && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        bWeights = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        ResizeAsciiVector(node.Skin.Bones, Data.MH.Names.size(), "skin bone table");
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if((sID == "skinpadding1" || sID == "skin_padding1") && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("skinpadding1 is outside uint16 range.");
                            node.Skin.nPadding1 = static_cast<unsigned short>(nConvert);
                        }
                        else throw mdlexception("Error reading skinpadding1 for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if((sID == "skinpadding2" || sID == "skin_padding2") && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            if(nConvert < 0 || nConvert > 65535) throw mdlexception("skinpadding2 is outside uint16 range.");
                            node.Skin.nPadding2 = static_cast<unsigned short>(nConvert);
                        }
                        else throw mdlexception("Error reading skinpadding2 for node '" + Mdl.GetNodeName(node) + "'.");
                        SkipLine();
                    }
                    else if(sID == "compactbonemap" && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bCompactBoneMap = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            SetAsciiListCount(nDataMax, nConvert, sID);
                            ResizeAsciiVector(node.Skin.TempCompactBoneNameIndices, static_cast<unsigned int>(nDataMax), sID);
                        }
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "compactbonemapraw" && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bCompactBoneMapRaw = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            SetAsciiListCount(nDataMax, nConvert, sID);
                            ResizeAsciiVector(node.Skin.TempCompactBoneRawIndices, static_cast<unsigned int>(nDataMax), sID);
                        }
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "boneconstantindices" && nNode & NODE_SKIN){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bBoneConstantIndices = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        if(ReadInt(nConvert)){
                            SetAsciiListCount(nDataMax, nConvert, sID);
                            const unsigned int nBoneconstantCount = static_cast<unsigned int>(nDataMax);
                            if(nBoneconstantCount > 0 && node.Skin.Bones.empty()){
                                throw mdlexception("boneconstantindices must appear after the skin weights have established the bone table for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                            if(!node.Skin.Bones.empty() && nBoneconstantCount != node.Skin.Bones.size()){
                                throw mdlexception("boneconstantindices count does not match the current skin bone table for node '" + Mdl.GetNodeName(node) + "'; refusing to resize the bone table and move skin data.");
                            }
                        }
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "saberdata" && nNode & NODE_SABER){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        bSaberData = true;
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        node.Saber.SaberData.clear();
                        node.Saber.bPreserveSaberData = true;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "aabb" && nNode & NODE_AABB){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Node & node = Data.MH.ArrayOfNodes.at(nCurrentIndex);

                        const unsigned int nSavePos = nPosition; // Save position after the aabb token.
                        std::string sFirstValue;
                        const bool bHasInlineValue = ReadUntilText(sFirstValue, false);

                        // Counted MDLedit ASCII has exactly one integer after the
                        // keyword. Legacy NWMax ASCII places the first seven-value
                        // AABB row on that same line, and its first coordinate can
                        // itself look like an integer. Inspect the whole remainder
                        // of the line before choosing a format, and do not probe a
                        // floating-point coordinate with the throwing integer reader.
                        std::size_t nRemainder = nPosition;
                        while(nRemainder < sBuffer.size() && sBuffer[nRemainder] == 0x20) nRemainder++;
                        const bool bAtLineEnd = nRemainder >= sBuffer.size() ||
                                                sBuffer[nRemainder] == '#' ||
                                                sBuffer[nRemainder] == 0x0D ||
                                                sBuffer[nRemainder] == 0x0A ||
                                                sBuffer[nRemainder] == 0x00;
                        const bool bCountedFormat = ascii_compat::IsCountedAabbHeader(
                            bHasInlineValue, sFirstValue, bAtLineEnd);

                        nPosition = nSavePos;
                        if(bCountedFormat){
                            if(!ReadInt(nConvert)){
                                throw mdlexception("aabb count is missing for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                            // New MDLedit ASCII writes an explicit AABB row count:
                            //   aabb <count>
                            //       <rows...>
                            // Do not leave that count in the stream as the first bbmin value.
                            SetAsciiListCount(nDataMax, nConvert, sID);
                            node.Walkmesh.TempAabbNodes.clear();
                            node.Walkmesh.bPreserveAabbTree = false;
                            bAabb = true;
                            nDataCounter = 0;
                            SkipLine();
                        }
                        else{
                            // The v1.0.104b reader did not read a count here. It used
                            // (2 * face count - 1), float-probed the inline value, and
                            // rewound when the first row started on the keyword line.
                            const std::size_t nFaceCount = node.Mesh.Faces.size();
                            unsigned int nAabbAsciiCount = 0;
                            if(!ascii_compat::InferLegacyAabbRowCount(nFaceCount, nAabbAsciiCount)){
                                throw mdlexception("aabb inferred node count is too large for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                            SetAsciiListCountSize(nDataMax, nAabbAsciiCount, sID);
                            node.Walkmesh.TempAabbNodes.clear();
                            node.Walkmesh.bPreserveAabbTree = false;
                            bAabb = true;
                            nDataCounter = 0;
                            if(!bHasInlineValue) SkipLine(); // Data starts on the next line, or the list is empty.
                        }
                    }
                    else if(sID == "roomlinks" && nNode & NODE_AABB){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bRoomLinks = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "constraints" && nNode & NODE_DANGLY){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bConstraints = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "texturenames" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bTextureNames = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "flaresizes" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bFlareSizes = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "flarepositions" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bFlarePositions = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "flarecolorshifts" && nNode & NODE_LIGHT){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bFlareColorShifts = true;
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if((sID == "animationpadding" || sID == "animpadding") && nAnimation.Valid() && nNode == 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        for(int nPad = 0; nPad < 3; nPad++){
                            if(ReadInt(nConvert)){
                                if(nConvert < 0 || nConvert > 255) throw mdlexception("animationpadding value is outside byte range.");
                                anim.nPadding.at(nPad) = static_cast<unsigned char>(nConvert);
                            }
                            else throw mdlexception("Error reading animationpadding byte " + std::to_string(nPad) + ".");
                        }
                        SkipLine();
                    }
                    /// Extra (controllerless) data
                    else if(sID == "controllerraw" && (bGeometry || bAnimation) && nNode != 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        node.Head.Controllers.clear();
                        bControllerRaw = true;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "controllerdataraw" && (bGeometry || bAnimation) && nNode != 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        node.Head.ControllerData.clear();
                        bControllerDataRaw = true;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "extra_data" && nAnimation.Valid()){
                        Animation & anim = Data.MH.Animations.back();
                        Node & node = anim.ArrayOfNodes.back();
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else SetAsciiListCount(nDataMax, 0, sID);
                        const unsigned int nData = static_cast<unsigned int>(nDataMax);
                        ResizeAsciiVector(node.Head.ControllerData, nData, 0.0, sID);
                        bExtraData = true;
                        nDataCounter = 0;
                        SkipLine();
                    }
                    /// Next we have bezier controllers
                    else if(safesubstr(sID, sID.length()-9) == "bezierkey" && ReturnController(safesubstr(sID, 0, sID.length()-9), nNode)){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        Node & node = anim.ArrayOfNodes.back();
                        Controller ctrl;
                        ctrl.nAnimation = nAnimation;
                        ctrl.nNameIndex = node.Head.nNameIndex;
                        ctrl.nControllerType = ReturnController(safesubstr(sID, 0, sID.length()-9), nNode);
                        ctrl.nTimekeyStart = node.Head.ControllerData.size();
                        ctrl.nUnknown2 = -1;
                        if(ctrl.nControllerType == CONTROLLER_HEADER_POSITION ||
                           ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION){
                            ctrl.nUnknown2 = ctrl.nControllerType + 8;
                        }
                        //This is all we can tell right now.

                        unsigned nControllerTokenPosition = nPosition;
                        unsigned nControllerTokenLine = nLine;

                        //To continue let's first get the column count
                        nDataMax = -1;
                        if(!EmptyRow()){
                            if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                            else{
                                ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Something weird is going on after the controller keyword.\n";
                                throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                   "The expected count after the controller name is not an integer.");
                            }
                        }
                        SkipLine();
                        nDataCounter = 0;
                        while(!nDataMax.Valid() || nDataMax > 0){
                            if(!EmptyRow() || nDataCounter > 0){
                                if(ReadFloat(fConvert, &sID)){
                                    nDataCounter++;
                                }
                                else{
                                    ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "This is not a float: " << sID << ".\n";
                                    throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                       "This is not a float: " + sID + ".");
                                }
                                if(EmptyRow()) break;
                            }
                            else{
                                SkipLine();
                            }
                        }
                        if(nDataCounter == 0){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Keyed controller error: no data at all in the first line after the token line.\n";
                            throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. No data.");
                        }

                        ctrl.nColumnCount = 16 + (nDataCounter - 1) / 3;
                        //ReportMdl << "Column count for " << sID << " is " << nDataCounter - 1 << "\n";

                        //Now let's get the row count. It is actually best to also read the timekeys in this step
                        nPosition = nControllerTokenPosition;
                        nLine = nControllerTokenLine;

                        SkipLine();
                        nDataCounter = 0;
                        int nSavePos2;
                        while(!nDataMax.Valid() || nDataCounter < nDataMax){
                            //ReportMdl << "Looking.. Position=" << nPosition << ".\n";
                            if(EmptyRow()){
                                SkipLine();
                            }
                            else{
                                nSavePos2 = nPosition;
                                ReadUntilText(sID);
                                if(sID == "endlist") break;
                                else{
                                    nPosition = nSavePos2;
                                    if(!EmptyRow()){
                                        if(ReadFloat(fConvert)){
                                            node.Head.ControllerData.push_back(fConvert);
                                        }
                                        else{
                                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "This is not a float: " << sID << ".\n";
                                            throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                               "This is not a float: " + sID + ".");

                                        }
                                        nDataCounter++;
                                    }
                                    SkipLine();
                                }
                            }
                        }
                        if(nDataCounter == 0){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Keyed controller error: no data at all in the first line after the token line.\n";
                            throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. No data.");
                        }
                        ctrl.nValueCount = nDataCounter;
                        ctrl.nDataStart = node.Head.ControllerData.size();

                        //We now have all the data, append the controller, reset the position and prepare for actually reading the keys.
                        nPosition = nControllerTokenPosition;
                        nLine = nControllerTokenLine;
                        node.Head.Controllers.push_back(ctrl);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        bKeys = true;
                        SkipLine();
                    }
                    /// Next we have keyed controllers
                    else if(safesubstr(sID, sID.length()-3) == "key" && ReturnController(safesubstr(sID, 0, sID.length()-3), nNode)){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        Node & node = anim.ArrayOfNodes.back();
                        Controller ctrl;
                        ctrl.nAnimation = nAnimation;
                        ctrl.nNameIndex = node.Head.nNameIndex;
                        ctrl.nControllerType = ReturnController(safesubstr(sID, 0, sID.length()-3), nNode);
                        ctrl.nTimekeyStart = node.Head.ControllerData.size();
                        ctrl.nUnknown2 = -1;
                        if(ctrl.nControllerType == CONTROLLER_HEADER_POSITION ||
                           ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION){
                            ctrl.nUnknown2 = ctrl.nControllerType + 8;
                        }
                        //This is all we can tell right now.

                        unsigned nControllerTokenPosition = nPosition;
                        unsigned nControllerTokenLine = nLine;

                        //To continue let's first get the column count
                        nDataMax = -1;
                        if(!EmptyRow()){
                            if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                            else{
                                ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Something weird is going on after the controller keyword.\n";
                                throw mdlexception("Error reading keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                   "The expected count after the controller name is not an integer.");
                            }
                        }
                        SkipLine();
                        nDataCounter = 0;
                        while(!nDataMax.Valid() || nDataMax > 0){
                            if(!EmptyRow() || nDataCounter > 0){
                                if(ReadFloat(fConvert, &sID)){
                                    nDataCounter++;
                                }
                                else{
                                    ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "This is not a float: " << sID << ".\n";
                                    throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                       "This is not a float: " + sID + ".");
                                }
                                if(EmptyRow()) break;;
                            }
                            else SkipLine();
                        }
                        if(nDataCounter == 0){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Keyed controller error: no data at all in the first line after the token line.\n";
                            throw mdlexception("Error reading keyed controller data for node '" + Mdl.GetNodeName(node) + "'. No data.");
                        }

                        ctrl.nColumnCount = nDataCounter - 1;
                        if(Data.MH.bCompressQuaternions && ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION) ctrl.nColumnCount = 2;
                        //ReportMdl << "Column count for " << sID << " is " << nDataCounter - 1 << "\n";

                        //Now let's get the row count. It is actually best to also read the timekeys in this step
                        nPosition = nControllerTokenPosition;
                        nLine = nControllerTokenLine;
                        SkipLine();
                        nDataCounter = 0;
                        int nSavePos2;
                        while(!nDataMax.Valid() || nDataCounter < nDataMax){
                            if(EmptyRow()) SkipLine();
                            else{
                                nSavePos2 = nPosition;
                                ReadUntilText(sID);
                                if(sID=="endlist") break;
                                else{
                                    nPosition = nSavePos2;
                                    if(!EmptyRow()){
                                        if(ReadFloat(fConvert)){
                                            node.Head.ControllerData.push_back(fConvert);
                                        }
                                        else{
                                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "This is not a float: " << sID << ".\n";
                                            throw mdlexception("Error reading bezier keyed controller data for node '" + Mdl.GetNodeName(node) + "'. "
                                                               "This is not a float: " + sID + ".");
                                        }
                                        nDataCounter++;
                                    }
                                    SkipLine();
                                }
                            }
                        }
                        if(nDataCounter == 0){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Keyed controller error: no data at all in the first line after the token line.\n";
                            throw mdlexception("Error reading keyed controller data for node '" + Mdl.GetNodeName(node) + "'. No data.");
                        }
                        ctrl.nValueCount = nDataCounter;
                        ctrl.nDataStart = node.Head.ControllerData.size();

                        //We now have all the data, append the controller, reset the position and prepare for actually reading the keys.
                        nPosition = nControllerTokenPosition;
                        nLine = nControllerTokenLine;
                        node.Head.Controllers.push_back(ctrl);
                        if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                        else nDataMax = -1;
                        nDataCounter = 0;
                        bKeys = true;
                        SkipLine();
                    }
                    /// Next we have single controllers
                    else if(ReturnController(sID, nNode)){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";

                        Node & node = nAnimation.Valid() ? Data.MH.Animations.back().ArrayOfNodes.back() : Data.MH.ArrayOfNodes.at(nCurrentIndex);
                        Controller ctrl;
                        ctrl.nControllerType = ReturnController(sID, nNode);
                        ctrl.nUnknown2 = -1;
                        if(nAnimation.Valid() &&
                           ( ctrl.nControllerType == CONTROLLER_HEADER_POSITION ||
                             ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION ) ){
                            ctrl.nUnknown2 = ctrl.nControllerType + 8;
                        }
                        ctrl.nTimekeyStart = node.Head.ControllerData.size();
                        ctrl.nDataStart = node.Head.ControllerData.size() + 1;
                        ctrl.nValueCount = 1;
                        ctrl.nNameIndex = node.Head.nNameIndex;
                        ctrl.nAnimation = nAnimation;

                        //First put in the 0.0 to fill the required timekey
                        node.Head.ControllerData.push_back(0.0);

                        //Now fill values
                        if(ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION){
                            double fX, fY, fZ, fAngle;
                            if(ReadFloat(fConvert)) fX = fConvert;
                            else throw mdlexception("Error reading single orientation controller data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fY = fConvert;
                            else throw mdlexception("Error reading single orientation controller data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fZ = fConvert;
                            else throw mdlexception("Error reading single orientation controller data for node '" + Mdl.GetNodeName(node) + "'.");
                            if(ReadFloat(fConvert)) fAngle = fConvert;
                            else throw mdlexception("Error reading single orientation controller data for node '" + Mdl.GetNodeName(node) + "'.");
                            node.Head.oOrient.SetAxisAngle(fX, fY, fZ, fAngle);
                            node.Head.ControllerData.push_back(node.Head.oOrient.GetQuaternion().vAxis.fX);
                            node.Head.ControllerData.push_back(node.Head.oOrient.GetQuaternion().vAxis.fY);
                            node.Head.ControllerData.push_back(node.Head.oOrient.GetQuaternion().vAxis.fZ);
                            node.Head.ControllerData.push_back(node.Head.oOrient.GetQuaternion().fW);
                            ctrl.nColumnCount = 4;
                        }
                        else{
                            nDataCounter = 0;
                            while(true){
                                if(ReadFloat(fConvert)){
                                    nDataCounter++;
                                    node.Head.ControllerData.push_back(fConvert);
                                }
                                else break;
                            }
                            if(nDataCounter == 0){
                                ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Single controller error: no data at all.\n";
                                throw mdlexception("Error reading single controller data for node '" + Mdl.GetNodeName(node) + "'.");
                            }
                            ctrl.nColumnCount = nDataCounter;

                            if(ctrl.nControllerType == CONTROLLER_HEADER_POSITION){
                                node.Head.vPos.fX = node.Head.ControllerData.at(ctrl.nDataStart + 0);
                                node.Head.vPos.fY = node.Head.ControllerData.at(ctrl.nDataStart + 1);
                                node.Head.vPos.fZ = node.Head.ControllerData.at(ctrl.nDataStart + 2);
                                //std::cout << "Added node position: " << node.Head.vPos.Print() << std::endl;
                            }
                        }

                        node.Head.Controllers.push_back(ctrl);
                        SkipLine();
                    }
                    /// General ending tokens
                    else if(sID == "endnode" && nNode > 0){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bMagnusll = false;
                        nNode = 0;
                        nCurrentIndex = -1;
                        SkipLine();
                    }
                    else if(sID == "endmodelgeom"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bGeometry = false;
                        Data.MH.nNodeCount = nNodeCount;
                        SkipLine();

                    }
                    /// Animation tokens
                    else if(sID == "newanim" && !bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bAnimation = true;
                        nAnimation++;

                        //Get Animation Name
                        if(!ReadUntilText(sID, false)){
                            ReportMdl << "ASCII::Read(): Error at line " << nLine << "! " << "Animation name is missing!\n";
                            throw mdlexception("No animation name has been found after a 'newanim' token.");
                        }
                        TruncateLongName3216(sID, "Animation name", nLine);

                        Animation anim;
                        anim.sName = sID;

                        //Initialize animation in case something is left undefined
                        if(Data.MH.Names.empty()){
                            throw mdlexception("Cannot initialize animation root: the model contains no node names.");
                        }
                        anim.sAnimRoot = Data.MH.Names.front().sName;
                        anim.fLength = 0.0;
                        anim.fTransition = 0.0;
                        anim.EventArray.nCount = 0;
                        anim.Events.resize(0);
                        anim.ArrayOfNodes.reserve(Data.MH.Names.size());

                        //Finish up
                        Data.MH.Animations.push_back(anim);
                        SkipLine();
                    }
                    else if(sID == "length" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        if(ReadFloat(fConvert)) anim.fLength = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "transtime" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        if(ReadFloat(fConvert)) anim.fTransition = fConvert;
                        else ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Couldn't read the value for " << sID << ". Using default value instead.\n";
                        SkipLine();
                    }
                    else if(sID == "animroot" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        std::string sValue;
                        if(!ReadUntilText(sValue, false)){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "An animation's AnimRoot name is missing. Using a default zero.\n";
                            sValue = "";
                        }
                        TruncateLongName3216(sValue, "AnimRoot name", nLine);
                        Animation & anim = Data.MH.Animations.back();
                        anim.sAnimRoot = sValue;
                        SkipLine();
                    }
                    else if(sID == "animheader" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        Animation & anim = Data.MH.Animations.back();
                        if(ReadUInt(uConvert)) anim.nFunctionPointer0 = uConvert;
                        else throw mdlexception("animheader missing function pointer 0.");
                        if(ReadUInt(uConvert)) anim.nFunctionPointer1 = uConvert;
                        else throw mdlexception("animheader missing function pointer 1.");
                        if(ReadUInt(uConvert)) anim.nNumberOfNames = uConvert;
                        else throw mdlexception("animheader missing number of names.");
                        if(ReadUInt(uConvert)) anim.nRefCount = uConvert;
                        else throw mdlexception("animheader missing reference count.");
                        if(ReadUInt(uConvert)){
                            if(uConvert > 255) throw mdlexception("animheader model type is outside uint8 range.");
                            anim.nModelType = static_cast<unsigned char>(uConvert);
                        }
                        else throw mdlexception("animheader missing model type.");
                        for(int nPad = 0; nPad < 3; nPad++){
                            if(ReadUInt(uConvert)){
                                if(uConvert > 255) throw mdlexception("animheader padding byte is outside uint8 range.");
                                anim.nPadding.at(nPad) = static_cast<unsigned char>(uConvert);
                            }
                            else throw mdlexception("animheader missing padding byte.");
                        }
                        if(ReadUInt(uConvert)) anim.nPadding2 = uConvert;
                        else throw mdlexception("animheader missing padding2.");
                        SkipLine();
                    }
                    else if(sID == "eventlist" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bEventlist = true;
                        // eventlist is an open-ended list terminated by endlist.
                        // Clear any previous counted-list limit so a prior zero-count
                        // list cannot immediately close this list and skip events.
                        nDataMax = MdlInteger<unsigned int>();
                        nDataCounter = 0;
                        SkipLine();
                    }
                    else if(sID == "event" && bAnimation){
                        Event sound; //New sound
                        if(!ReadFloat(fConvert)){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Error converting event time!\n";
                            throw mdlexception("Error converting event time.");
                        }
                        sound.fTime = fConvert;

                        std::string sValue;
                        if(!ReadUntilText(sValue, false)){
                            ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "Event name is missing!\n";
                            throw mdlexception("Event name is missing.");
                        }
                        TruncateLongName3216(sValue, "Event name", nLine);
                        sound.sName = sValue;
                        Animation & anim = Data.MH.Animations.back();
                        anim.Events.push_back(sound);
                        SkipLine();
                    }
                    else if(sID == "doneanim" && bAnimation){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        bAnimation = false;
                        SkipLine();
                    }
                    else if(sID == "donemodel"){
                        if(DEBUG_LEVEL > 3) ReportMdl << "Reading " << sID << ".\n";
                        break; /// End the reading loop
                    }
                    //These are the names we should ignore, because we expect them to appear, but we have no use for them
                    else if(sID == "multimaterial"){
                        if(ReadInt(nConvert)){
                            SkipLine();
                            for(int nj = 0; nj < nConvert; nj++){
                                SkipLine();
                            }
                        }
                        else SkipLine();
                    }
                    else if(sID == "filedependancy") SkipLine();
                    else if(sID == "specular") SkipLine();
                    else if(sID == "wirecolor") SkipLine();
                    else if(sID == "shininess") SkipLine();
                    else if(sID == "inheritcolor") SkipLine();
                    else if(sID == "tilefade") SkipLine();
                    else if(sID == "center") SkipLine();
                    else if(sID == "bmin") SkipLine();
                    else if(sID == "bmax") SkipLine();
                    else if(sID == "radius") SkipLine();
                    else if(sID == "average") SkipLine();
                    else{
                        ReportMdl << "ASCII::Read(): Warning at line " << nLine << "! " << "ReadUntilText() has found some text that we cannot interpret: " << sID << "\n";
                        //ReportMdl << "nNode = " << nNode << "\n";
                        //throw mdlexception("An exception that shouldn't be here!");
                        SkipLine();
                    }
                }
            }
        }
    }
    catch(const std::exception & e){
        Error("An exception occurred at line " + std::to_string(nLine) + " while reading the ascii model:\n\n" + std::string(e.what()) + "\n\nThe program will now cleanup what it has read since the data is now broken.");
        return false;
    }
    catch(...){
        Error("An unknown exception occurred at line " + std::to_string(nLine) + " while reading the ascii model!\n\nThe program will now cleanup what it has read since the data is now broken.");
        return false;
    }

    ReportMdl << "Done reading ascii!\n";

    /// Post Process
    try{
        Mdl.AsciiPostProcess(sBumpmapped);
    }
    catch(const std::exception & e){
        Error("An exception occurred while post-processing the ascii model:\n\n" + std::string(e.what()) +
              "\n\nThe program will now cleanup what it has read since the data is now broken.");
        return false;
    }
    catch(...){
        Error("An unknown exception occurred while post-processing the ascii model!\n\nThe program will now cleanup what it has read since the data is now broken.");
        return false;
    }

    restoreReadState.Dismiss();
    return true;
}

bool ASCII::ReadWalkmesh(MDL & Mdl, bool bPwk){
    ReportObject ReportMdl(Mdl);

    std::unique_ptr<PWK> pOldPwk;
    std::unique_ptr<DWK> pOldDwk0;
    std::unique_ptr<DWK> pOldDwk1;
    std::unique_ptr<DWK> pOldDwk2;
    if(bPwk) pOldPwk = std::move(Mdl.Pwk);
    else{
        pOldDwk0 = std::move(Mdl.Dwk0);
        pOldDwk1 = std::move(Mdl.Dwk1);
        pOldDwk2 = std::move(Mdl.Dwk2);
    }
    auto restoreWalkmeshReadState = MakeAsciiScopeExit([&](){
        if(bPwk) Mdl.Pwk = std::move(pOldPwk);
        else{
            Mdl.Dwk0 = std::move(pOldDwk0);
            Mdl.Dwk1 = std::move(pOldDwk1);
            Mdl.Dwk2 = std::move(pOldDwk2);
        }
    });

	if(bPwk){
		Mdl.Pwk.reset(new PWK);
		std::wstring sPwk = GetFilename();
		Mdl.Pwk->SetFilePath(sPwk);
		Mdl.Pwk->GetData().reset(new BWMHeader);
	}
	else{
		Mdl.Dwk0.reset(new DWK);
		Mdl.Dwk1.reset(new DWK);
		Mdl.Dwk2.reset(new DWK);
		Mdl.Dwk0->SetDwk(0);
		Mdl.Dwk1->SetDwk(1);
		Mdl.Dwk2->SetDwk(2);
		std::wstring sDwk;
		sDwk =  GetFilename()+L" (closed)";
		Mdl.Dwk0->SetFilePath(sDwk);
		sDwk =  GetFilename()+L" (open1)";
		Mdl.Dwk1->SetFilePath(sDwk);
		sDwk =  GetFilename()+L" (open2)";
		Mdl.Dwk2->SetFilePath(sDwk);
		Mdl.Dwk0->GetData().reset(new BWMHeader);
		Mdl.Dwk1->GetData().reset(new BWMHeader);
		Mdl.Dwk2->GetData().reset(new BWMHeader);
	}

    std::string sID;
    nPosition = 0;

    int nConvert;
    double fConvert;
    bool bError = false;
    bool bFound = false;
    bool bVerts = false;
    bool bFaces = false;
    bool * lpbList = nullptr;
    int nNode = 0;
    MdlInteger<unsigned int> nDataMax;
    MdlInteger<unsigned int> nDataCounter;
    BWMHeader * DATA = nullptr;
    std::vector<Vector> * TempVerts = nullptr;
    std::vector<Vector> TempVerts0;
    std::vector<Vector> TempVerts1;
    std::vector<Vector> TempVerts2;
    std::string sNodeName;

    while(nPosition < sBuffer.size() && !bError){
        //First set our current list
        if(bVerts) lpbList = &bVerts;
        else if(bFaces) lpbList = &bFaces;
        else lpbList = nullptr;

        //First, check if we have a blank line, we'll just skip it here.
        if(EmptyRow()){
            SkipLine();
        }
        //Second, check if we are in some list of data. We should not look for a keyword then.
        else if(lpbList != nullptr && DATA != nullptr){
            if(nDataMax.Valid() && nDataMax == 0){
                const unsigned int nSavePos = nPosition;
                if(ReadUntilText(sID) && sID == "endlist"){
                    SkipLine();
                }
                else{
                    nPosition = nSavePos;
                }
                *lpbList = false;
                continue;
            }

            //First, check if we're done. Save your position, cuz we'll need to revert if we're not done
            int nSavePos = nPosition;
            ReadUntilText(sID);
            if(sID == "endlist"){
                *lpbList = false;
                SkipLine();
            }
            else{
                //Revert back to old position
                nPosition = nSavePos;

                /// Read the data
                if(bVerts && TempVerts != nullptr){
                    //if(DEBUG_LEVEL > 3) std::cout << "Reading verts data " << nDataCounter << ".\n";
                    bFound = true;
                    Vector vert;
                    if(ReadFloat(fConvert)) vert.fX = fConvert;
                    else bFound = false;
                    if(ReadFloat(fConvert)) vert.fY = fConvert;
                    else bFound = false;
                    if(ReadFloat(fConvert)) vert.fZ = fConvert;
                    else bFound = false;
                    if(bFound){
                        TempVerts->push_back(std::move(vert));
                    }
                    else bError = true;
                }
                else if(bFaces){
                    //if(DEBUG_LEVEL > 3) std::cout << "Reading faces data" << "" << ".\n";
                    bFound = true;
                    Face face;
                    //std::cout << "Reading walk face.\n";

                    //Currently we read the regular NWMax version with only a single set of tvert indices
                    if(ReadInt(nConvert)) face.nIndexVertex[0] = AsciiUShortOrInvalid(nConvert, "face vertex index 1");
                    else bFound = false;
                    if(ReadInt(nConvert)) face.nIndexVertex[1] = AsciiUShortOrInvalid(nConvert, "face vertex index 2");
                    else bFound = false;
                    if(ReadInt(nConvert)) face.nIndexVertex[2] = AsciiUShortOrInvalid(nConvert, "face vertex index 3");
                    else bFound = false;
                    if(!ReadInt(nConvert)) bFound = false;
                    if(!ReadInt(nConvert)) bFound = false;
                    if(!ReadInt(nConvert)) bFound = false;
                    if(!ReadInt(nConvert)) bFound = false;
                    if(ReadInt(nConvert)) face.nMaterialID = AsciiUIntOrInvalid(nConvert, "face material id");
                    else bFound = false;

                    if(bFound){
                        //std::cout << "Added walk face to faces.\n";
                        DATA->faces.push_back(std::move(face));
                    }
                    else bError = true;
                }

                nDataCounter++;
                if(nDataCounter >= nDataMax && nDataMax.Valid()){
                    *lpbList = false;
                }
                SkipLine();
            }
        }
        //So no data. Find the next keyword then.
        else{
            //std::cout << "No data to read. Read keyword instead" << "" << ".\n";
            bFound = ReadUntilText(sID);
            if(!bFound) SkipLine(); //This will have already been done above, no need to look for it again
            else{
                /// Common case for nodes
                if(sID == "node" && nNode == 0){
                    if(DEBUG_LEVEL > 3) std::cout << "Reading " << sID << ".\n";

                    // Do not let an unrecognized walkmesh/helper node reuse the
                    // DATA/TempVerts pointers from the previous node. Otherwise a
                    // later position/verts/faces block could append to or move the
                    // wrong WOK/PWK/DWK data set.
                    DATA = nullptr;
                    TempVerts = nullptr;

                    //Read type
                    int nType = 0;
                    bFound = ReadUntilText(sID); //Get type
                    if(!bFound){
                        std::cout << "ReadUntilText() ERROR: a node is without any other specification.\n";
                    }
                    if(sID == "dummy") nType = NODE_HEADER;
                    else if(sID == "trimesh") nType = NODE_HEADER | NODE_MESH;
                    else if(sID == "aabb") nType = NODE_HEADER | NODE_MESH | NODE_AABB;
                    else if(bFound){
                        std::cout << "ReadUntilText() has found some text (type?) that we do not support: " << sID << "\n";
                        bError = true;
                    }

                    //Read name
                    bFound = ReadUntilText(sID, false); //Name
                    if(!bFound){
                        std::cout << "ReadUntilText() ERROR: a node is without a name.\n";
                    }
                    else{
                        sNodeName = sID;
                        //std::cout << "Reading " << sNodeName << ".\n";
						if(bPwk){
							DATA = Mdl.Pwk->GetData().get();
                            TempVerts = &TempVerts0;
						}
                        else if(sNodeName.find("closed") != std::string::npos){
                            DATA = Mdl.Dwk0->GetData().get();
                            TempVerts = &TempVerts0;
                        }
                        else if(sNodeName.find("open1") != std::string::npos){
                            DATA = Mdl.Dwk1->GetData().get();
                            TempVerts = &TempVerts1;
                        }
                        else if(sNodeName.find("open2") != std::string::npos){
                            DATA = Mdl.Dwk2->GetData().get();
                            TempVerts = &TempVerts2;
                        }
                    }

                    //Finish up
                    nNode = nType;
                    if(!bFound) bError = true;
                    SkipLine();
                }
                /// Next we have the DATA LISTS
                else if(sID == "verts" && nNode & NODE_MESH && DATA != nullptr && TempVerts != nullptr && (sNodeName.find("_wg") != std::string::npos)){
                    if(DEBUG_LEVEL > 3) std::cout << "Reading " << sID << ".\n";
                    //std::cout << "Reading verts.\n";
                    bVerts = true;
                    if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                    else nDataMax = -1;
                    nDataCounter = 0;
                    SkipLine();
                }
                else if(sID == "faces" && nNode & NODE_MESH && DATA != nullptr && TempVerts != nullptr && (sNodeName.find("_wg") != std::string::npos)){
                    if(DEBUG_LEVEL > 3) std::cout << "Reading " << sID << ".\n";
                    //std::cout << "Reading faces.\n";
                    bFaces = true;
                    if(ReadInt(nConvert)) SetAsciiListCount(nDataMax, nConvert, sID);
                    else nDataMax = -1;
                    nDataCounter = 0;
                    SkipLine();
                }
                else if(sID == "position" && nNode && DATA != nullptr){
                    if(DEBUG_LEVEL > 3) std::cout << "Reading " << sID << ".\n";
                    double fX = 0.0, fY = 0.0, fZ = 0.0;
                    bool bPositionValid = true;
                    if(ReadFloat(fConvert)) fX = fConvert;
                    else bPositionValid = false;
                    if(ReadFloat(fConvert)) fY = fConvert;
                    else bPositionValid = false;
                    if(ReadFloat(fConvert)) fZ = fConvert;
                    else bPositionValid = false;

                    if(!bPositionValid){
                        bError = true;
                    }
                    else if(sNodeName.find("pwk_use01") != std::string::npos || sNodeName.find("pwk_dp_use_01") != std::string::npos || sNodeName.find("DWK_dp_closed_01") != std::string::npos || sNodeName.find("DWK_dp_open1_01") != std::string::npos || sNodeName.find("DWK_dp_open2_01") != std::string::npos){
                        DATA->vUse1 = Vector(fX, fY, fZ);
                    }
                    else if(sNodeName.find("pwk_use02") != std::string::npos || sNodeName.find("pwk_dp_use_02") != std::string::npos || sNodeName.find("DWK_dp_closed_02") != std::string::npos || sNodeName.find("DWK_dp_open1_02") != std::string::npos || sNodeName.find("DWK_dp_open2_02") != std::string::npos){
                        DATA->vUse2 = Vector(fX, fY, fZ);
                    }
                    else if(sNodeName.find("_wg") != std::string::npos){
                        DATA->vPosition = Vector(fX, fY, fZ);
                    }

                    SkipLine();
                }
                /// General ending tokens
                else if(sID == "endnode" && nNode > 0){
                    if(DEBUG_LEVEL > 3) std::cout << "Reading " << sID << ".\n";
                    nNode = 0;
                    DATA = nullptr;
                    TempVerts = nullptr;
                    SkipLine();
                }
                //These are the names we should ignore, because we expect them to appear, but we have no use for them
                else if(sID == "multimaterial"){
                    if(ReadInt(nConvert)){
                        SkipLine();
                        for(int nj = 0; nj < nConvert; nj++){
                            SkipLine();
                        }
                    }
                    else SkipLine();
                }
                else if(sID == "parent") SkipLine();
                else if(sID == "orientation") SkipLine();
                else if(sID == "bitmap") SkipLine();
                else if(sID == "filedependancy") SkipLine();
                else if(sID == "specular") SkipLine();
                else if(sID == "wirecolor") SkipLine();
                else if(sID == "shininess") SkipLine();
                else if(sID == "name") SkipLine();
                else if(sID == "inheritcolor") SkipLine();
                else if(sID == "tilefade") SkipLine();
                else if(sID == "center") SkipLine();
                else{
                    std::cout << "ReadUntilText() has found some text that we cannot interpret: " << sID << "\n";
                    SkipLine();
                }
            }
        }
    }
    if(bError){
        Error("Some kind of error has occured! Check the console! The program will now clean up what it has read since the data is now broken.");
        return false;
    }
    ReportMdl << "Done reading walkmesh ascii, checking for errors...\n";

	try{
		if(bPwk){
			Mdl.BwmAsciiPostProcess(*Mdl.Pwk->GetData(), TempVerts0, false);
		}
		else{
			Mdl.BwmAsciiPostProcess(*Mdl.Dwk0->GetData(), TempVerts0);
			Mdl.BwmAsciiPostProcess(*Mdl.Dwk1->GetData(), TempVerts1);
			Mdl.BwmAsciiPostProcess(*Mdl.Dwk2->GetData(), TempVerts2);
		}
	}
	catch(const std::exception & e){
		Error("An exception occurred while post-processing the ascii walkmesh:\n\n" + std::string(e.what()) +
		      "\n\nThe program will now cleanup what it has read since the data is now broken.");
		return false;
	}
	catch(...){
		Error("An unknown exception occurred while post-processing the ascii walkmesh!\n\nThe program will now cleanup what it has read since the data is now broken.");
		return false;
	}

    restoreWalkmeshReadState.Dismiss();
    return true;
}
