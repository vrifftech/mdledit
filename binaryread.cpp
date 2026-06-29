#include "MDL.h"
#include <limits>
#include <algorithm>
#include <utility>

/**
    Functions:
    MDL::LinearizeGeometry()
    MDL::LinearizeAnimation()
    MDL::DecompileModel()
    MDL::ParseAabb()
    MDL::ParseNode()
*/

bool bReadSmoothing = false;

namespace {
    unsigned int MdlDataOffset(unsigned int relativeOffset, const std::string & context){
        if(relativeOffset > std::numeric_limits<unsigned int>::max() - MDL_OFFSET){
            throw mdlexception(context + " offset is outside the MDL data buffer.");
        }
        return static_cast<unsigned int>(MDL_OFFSET + relativeOffset);
    }

    unsigned int MdlArrayDataOffset(const ArrayHead & array, const std::string & context){
        if(array.nCount != array.nCount2){
            throw mdlexception(context + " array header count fields do not match; refusing to guess which count is authoritative.");
        }
        if(array.nCount == 0){
            return 0;
        }
        if(array.nOffset == 0){
            throw mdlexception(context + " array has a non-zero count but a zero data offset; refusing to read data from the wrong region.");
        }
        return MdlDataOffset(array.nOffset, context);
    }

    unsigned int RequiredMdlDataOffset(unsigned int relativeOffset, const std::string & context){
        if(relativeOffset == 0){
            throw mdlexception(context + " offset is zero even though data is required; refusing to read from the wrong region.");
        }
        return MdlDataOffset(relativeOffset, context);
    }

    MdlInteger<unsigned short> SignedIntToUShortOrInvalid(int nValue, const std::string & context){
        if(nValue == -1) return MdlInteger<unsigned short>();
        if(nValue < -1){
            throw mdlexception(context + " contains a negative value other than -1; refusing to reinterpret it as a valid 16-bit index.");
        }
        const int nInvalid = static_cast<int>(std::numeric_limits<unsigned short>::max());
        if(nValue >= nInvalid){
            throw mdlexception(context + " is outside the valid 16-bit MDL index range.");
        }
        return MdlInteger<unsigned short>(static_cast<unsigned short>(nValue));
    }

    std::string DebugCStringAt(const std::vector<char> & buffer, unsigned offset){
        if(offset >= buffer.size()) return std::string("<out of range>");
        auto begin = buffer.begin() + offset;
        auto end = std::find(begin, buffer.end(), '\0');
        return std::string(begin, end);
    }

    template <class F>
    class ScopeExit{
        F fn;
        bool active = true;
      public:
        explicit ScopeExit(F f) : fn(std::move(f)) {}
        ScopeExit(const ScopeExit&) = delete;
        ScopeExit& operator=(const ScopeExit&) = delete;
        ScopeExit(ScopeExit && other) : fn(std::move(other.fn)), active(other.active) { other.active = false; }
        ScopeExit& operator=(ScopeExit &&) = delete;
        void Dismiss(){ active = false; }
        ~ScopeExit(){ if(active) fn(); }
    };

    template <class F>
    ScopeExit<F> MakeScopeExit(F f){
        return ScopeExit<F>(std::move(f));
    }

    template <class T>
    void ResizeBinaryVector(std::vector<T> & values, unsigned int nCount, const std::string & context){
        if(static_cast<std::size_t>(nCount) > values.max_size()){
            throw mdlexception(context + " count is too large to allocate safely.");
        }
        values.resize(static_cast<std::size_t>(nCount));
    }

    unsigned int BwmArrayOffset(unsigned int nCount, unsigned int nOffset, unsigned int nStride, std::size_t nBufferSize, const std::string & context){
        if(nCount == 0) return 0;
        if(nOffset == 0){
            throw mdlexception(context + " has a non-zero count but a zero data offset; refusing to read data from the header/unknown region.");
        }
        if(nStride != 0 && nCount > std::numeric_limits<unsigned int>::max() / nStride){
            throw mdlexception(context + " byte size overflows 32-bit BWM offsets.");
        }
        const std::size_t nBytes = static_cast<std::size_t>(nCount) * static_cast<std::size_t>(nStride);
        if(static_cast<std::size_t>(nOffset) > nBufferSize || nBytes > nBufferSize - static_cast<std::size_t>(nOffset)){
            throw mdlexception(context + " data range points outside the binary buffer; refusing to allocate or read malformed data.");
        }
        return nOffset;
    }

    unsigned short MaxNameIndexInTree(const Node & node){
        if(!node.Head.nNameIndex.Valid()){
            throw mdlexception("Binary node tree contains a node with an invalid name index.");
        }

        unsigned short nMax = static_cast<unsigned short>(node.Head.nNameIndex);
        for(const Node & child : node.Head.Children){
            if(child.nOffset == 0) continue;
            const unsigned short nChildMax = MaxNameIndexInTree(child);
            if(nChildMax > nMax) nMax = nChildMax;
        }
        return nMax;
    }

    void EnsureFlattenedNodeSlotAvailable(const std::vector<Node> & nodes, unsigned short nNameIndex){
        if(nNameIndex >= nodes.size()){
            throw mdlexception("LinearizeGeometry() error: node name index is outside the flattened node array.");
        }
        if(nodes.at(nNameIndex).Head.nNameIndex.Valid() || nodes.at(nNameIndex).Head.nType != 0 || nodes.at(nNameIndex).nOffset != 0){
            throw mdlexception("LinearizeGeometry() error: duplicate geometry node name index would overwrite an existing node.");
        }
    }
}


/// Linearize the nodes from being contained inside one another to the ArrayOfNodes in Name Index order.
void MDL::LinearizeGeometry(Node & node, std::vector<Node> & ArrayOfNodes){
    if(!node.Head.nNameIndex.Valid()){
        throw mdlexception("LinearizeGeometry() error: node name index is invalid.");
    }

    const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
    if(nNameIndex >= ArrayOfNodes.size()){
        throw mdlexception("LinearizeGeometry() error: node name index is outside the flattened node array.");
    }

    // Detach the parsed child tree before moving this node into the flat output.
    // The flat model uses ChildIndices for hierarchy; keeping moved-from child
    // vectors in the flattened nodes wastes memory and can expose stale state.
    std::vector<Node> children = std::move(node.Head.Children);
    node.Head.Children.clear();

    for(Node & child : children){
        if(child.nOffset != 0) LinearizeGeometry(child, ArrayOfNodes);
    }

    // Check at assignment time too, so a descendant or sibling with the same
    // name index cannot be silently overwritten.
    EnsureFlattenedNodeSlotAvailable(ArrayOfNodes, nNameIndex);
    ArrayOfNodes.at(nNameIndex) = std::move(node);
}

/// Linearize the nodes from being contained inside one another to the ArrayOfNodes in Node Index order (root first, then first child, its first child, etc.).
void MDL::LinearizeAnimation(Node & node, std::vector<Node> & ArrayOfNodes, int nChildIndex){
    (void)nChildIndex;

    // Do not recurse through references inside ArrayOfNodes after push_back().
    // Appending descendants can reallocate the vector and invalidate those
    // references. Detach children first and recurse over the stable local tree.
    std::vector<Node> children = std::move(node.Head.Children);
    node.Head.Children.clear();

    ArrayOfNodes.push_back(std::move(node));
    for(unsigned int n = 0; n < children.size(); n++){
        Node & child = children.at(n);
        if(child.nOffset != 0) LinearizeAnimation(child, ArrayOfNodes, n);
    }
}

/// This function will read the binary model.
/// When bMinimal is true, the algorithm will not read data such as the animations and will not output most of the debugging messages.
void MDL::DecompileModel(bool bMinimal){
    if(sBuffer.empty()) return;

    std::unique_ptr<FileHeader> pNewFileData(new FileHeader());
    std::unique_ptr<FileHeader> pOldFileData = std::move(FH);
    const ModelSource oldSource = src;
    const bool oldK2 = bK2;
    const bool oldXbox = bXbox;
    const int oldSupermodel = nSupermodel;
    const bool oldReadSmoothing = bReadSmoothing;
    const BufferState oldBufferState = CaptureBufferState();
    FH = std::move(pNewFileData);
    src = BinarySource;
    auto restoreReadState = MakeScopeExit([&](){
        RestoreBufferState(oldBufferState);
        FH = std::move(pOldFileData);
        src = oldSource;
        bK2 = oldK2;
        bXbox = oldXbox;
        nSupermodel = oldSupermodel;
        bReadSmoothing = oldReadSmoothing;
    });

    ReportObject ReportMdl (*this);

    /// Start timer
    Timer tDecompile;

    nPosition = 0; /// Set reading position to beginning of file.

    if(!bMinimal) ReportMdl << "Begin decompiling ";
    Report("Decompiling...");

    FileHeader & Data = *FH;
    //std::cout << "Data ready.\n";
    std::string sFileHeader = "File Header";


    //First read the file header, geometry header and model header
    ReadNumber(&Data.nZero, 8, sFileHeader + " > Padding");
    //std::cout << "Read first value, pos " << nPosition << ".\n";
    ReadNumber(&Data.nMdlLength, 1, sFileHeader + " > MDL File Size");
    ReadNumber(&Data.nMdxLength, 1, sFileHeader + " > MDX File Size");
    MarkDataBorder(nPosition - 1);
    //if(!bMinimal) ReportMdl << "File header read, pos " << nPosition << ".\n";

    std::string sGeometryHeader = "Geometry Header";

    ReadNumber(&Data.MH.GH.nFunctionPointer0, 9, sGeometryHeader + " > Function Pointers");
    ReadNumber(&Data.MH.GH.nFunctionPointer1, 9, sGeometryHeader + " > Function Pointers");
    //std::cout << "Read function pointers, now pos is: " << nPosition << ".\n";

    //Get game and platform
    //std::cout << "Function pointer 0: "<< Data.MH.GH.nFunctionPointer0 <<".\n";
    if(Data.MH.GH.nFunctionPointer0 == FN_PTR_PC_K1_MODEL_1) bK2 = false, bXbox = false;
    else if(Data.MH.GH.nFunctionPointer0 == FN_PTR_PC_K2_MODEL_1) bK2 = true, bXbox = false;
    else if(Data.MH.GH.nFunctionPointer0 == FN_PTR_XBOX_K2_MODEL_1) bK2 = true, bXbox = true;
    else if(Data.MH.GH.nFunctionPointer0 == FN_PTR_XBOX_K1_MODEL_1)  bK2 = false, bXbox = true;
    else throw mdlexception("Cannot interpret model function pointer (" + std::to_string(Data.MH.GH.nFunctionPointer0) + "), so cannot determine game and platform.");
    //std::cout << "Determined game.\n";

    ReadString(&Data.MH.GH.sName, 32, 3, sGeometryHeader + " > Name");
    if(!bMinimal) ReportMdl << Data.MH.GH.sName.c_str() << ".\n";
    ReadNumber(&Data.MH.GH.nOffsetToRootNode, 6, sGeometryHeader + " > Offset to Root Node");
    ReadNumber(&Data.MH.GH.nTotalNumberOfNodes, 1, sGeometryHeader + " > Number of Nodes");

    ReadNumber(&Data.MH.GH.RuntimeArray1.nOffset, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.RuntimeArray1.nCount, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.RuntimeArray1.nCount2, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.RuntimeArray2.nOffset, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.RuntimeArray2.nCount, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.RuntimeArray2.nCount2, 8, sGeometryHeader + " > Runtime Arrays");
    ReadNumber(&Data.MH.GH.nRefCount, 8, sGeometryHeader + " > Reference Count");

    ReadNumber(&Data.MH.GH.nModelType, 7, sGeometryHeader + " > Type");
    ReadNumber(&Data.MH.GH.nPadding[0], 11, sGeometryHeader + " > Padding");
    ReadNumber(&Data.MH.GH.nPadding[1], 11, sGeometryHeader + " > Padding");
    ReadNumber(&Data.MH.GH.nPadding[2], 11, sGeometryHeader + " > Padding");
    MarkDataBorder(nPosition - 1);
    //if(!bMinimal) ReportMdl << "Geometry header read.\n";

    std::string sModelHeader = "Model Header";

    ReadNumber(&Data.MH.nClassification, 7, sModelHeader + " > Classification");
    ReadNumber(&Data.MH.nSubclassification, 10, sModelHeader + " > Unknown1");
    ReadNumber(&Data.MH.nUnknown, 8, sModelHeader + " > Unknown1");
    ReadNumber(&Data.MH.nAffectedByFog, 7, sModelHeader + " > Affected By Fog");

    /// If the proper flags are on, enable SG reading
    bReadSmoothing = (bWriteSmoothing && Data.MH.nUnknown == 1) ? true : false;

    ReadNumber(&Data.MH.nChildModelCount, 8, sModelHeader + " > Number of Child Models");

    ReadNumber(&Data.MH.AnimationArray.nOffset, 6, sModelHeader + " > Offset to Animation Array");
    ReadNumber(&Data.MH.AnimationArray.nCount, 1, sModelHeader + " > Number of Animations");
    ReadNumber(&Data.MH.AnimationArray.nCount2, 1, sModelHeader + " > Number of Animations");
    ReadNumber(&Data.MH.nSupermodelReference, 11, sModelHeader + " > Supermodel Reference");

    Data.MH.vBBmin.fX = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Min");
    Data.MH.vBBmin.fY = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Min");
    Data.MH.vBBmin.fZ = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Min");
    Data.MH.vBBmax.fX = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Max");
    Data.MH.vBBmax.fY = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Max");
    Data.MH.vBBmax.fZ = ReadNumber<float>(nullptr, 2, sModelHeader + " > Bounding Box Max");
    Data.MH.fRadius = ReadNumber<float>(nullptr, 2, sModelHeader + " > Radius");
    Data.MH.fScale = ReadNumber<float>(nullptr, 2, sModelHeader + " > Animation Scale");

    ReadString(&Data.MH.cSupermodelName, 32, 3, sModelHeader + " > Supermodel Name");

    ReadNumber(&Data.MH.nOffsetToHeadRootNode, 6, sModelHeader + " > Offset to Head Root");
    ReadNumber(&Data.MH.nPadding, 8, sModelHeader + " > Padding");
    ReadNumber(&Data.MH.nMdxLength2, 1, sModelHeader + " > MDX File Size");
    ReadNumber(&Data.MH.nOffsetIntoMdx, 8, sModelHeader + " > MDX Data Offset");

    ReadNumber(&Data.MH.NameArray.nOffset, 6, sModelHeader + " > Offset to Name Array");
    ReadNumber(&Data.MH.NameArray.nCount, 1, sModelHeader + " > Number of Names");
    ReadNumber(&Data.MH.NameArray.nCount2, 1, sModelHeader + " > Number of Names");
    MarkDataBorder(nPosition - 1);
    //if(!bMinimal) ReportMdl << "Model header read.\n";

    /// The header is fully done!
    /// Now we're equipped to disassemble the rest
    /// First index names array
    //if(!bMinimal) ReportMdl << "Reading names.\n";
    if(Data.MH.NameArray.nCount > 0){
        std::string sNameArrayPointers = "Name Array > Pointers > Pointer ";
        std::string sNameArrayStrings = "Name Array > Strings > \"";
        Data.MH.Names.resize(Data.MH.NameArray.nCount);
        nPosition = MdlArrayDataOffset(Data.MH.NameArray, "name array");
        for(unsigned int n = 0; n < Data.MH.NameArray.nCount; n++){
            ReadNumber(&Data.MH.Names[n].nOffset, 6, sNameArrayPointers + std::to_string(n));
            MarkDataBorder(nPosition - 1);
            unsigned nPosData = RequiredMdlDataOffset(Data.MH.Names[n].nOffset, "name string");
            ReadString(&Data.MH.Names[n].sName, 0, 3, sNameArrayStrings + DebugCStringAt(GetBuffer(), nPosData) + "\"", &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    //if(!bMinimal) ReportMdl << "Name array read.\n";

    Report("Decompiling animations...");
    //if(!bMinimal) ReportMdl << "Reading animations.\n";
    /// Next, animations. Skip them if we're reading minimally
    if(Data.MH.AnimationArray.nCount > 0 && !bMinimal){
        Data.MH.Animations.resize(Data.MH.AnimationArray.nCount);
        nPosition = MdlArrayDataOffset(Data.MH.AnimationArray, "animation array");
        unsigned nAnimationPointerPosition = nPosition;
        for(unsigned int n = 0; n < Data.MH.AnimationArray.nCount; n++){
            nPosition = nAnimationPointerPosition;
            std::string sAnimationPointer = "Animations > Pointers > Pointer" + std::to_string(n);

            Animation & anim = Data.MH.Animations.at(n);
            ReadNumber(&anim.nOffset, 6, sAnimationPointer);
            MarkDataBorder(nPosition - 1);


            nAnimationPointerPosition = nPosition;
            nPosition = RequiredMdlDataOffset(anim.nOffset, "animation header");
            std::string sAnimation = "Animations > "  + DebugCStringAt(GetBuffer(), nPosition + 8) + " > ";
            std::string sAnimationGeometryHeader = sAnimation + "Geometry Header";
            ReadNumber(&anim.nFunctionPointer0, 9, sAnimationGeometryHeader + " > Function Pointers");
            ReadNumber(&anim.nFunctionPointer1, 9, sAnimationGeometryHeader + " > Function Pointers");

            ReadString(&anim.sName, 32, 3, sAnimationGeometryHeader + " > Name");

            ReadNumber(&anim.nOffsetToRootAnimationNode, 6, sAnimationGeometryHeader + " > Offset to Root Node");
            ReadNumber(&anim.nNumberOfNames, 1, sAnimationGeometryHeader + " > Number of Nodes");

            ReadNumber(&anim.RuntimeArray1.nOffset, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.RuntimeArray1.nCount, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.RuntimeArray1.nCount2, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.RuntimeArray2.nOffset, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.RuntimeArray2.nCount, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.RuntimeArray2.nCount2, 8, sAnimationGeometryHeader + " > Runtime Arrays");
            ReadNumber(&anim.nRefCount, 8, sAnimationGeometryHeader + " > Reference Count");

            ReadNumber(&anim.nModelType, 7, sAnimationGeometryHeader + " > Type");
            ReadNumber(&anim.nPadding[0], 11, sAnimationGeometryHeader + " > Padding");
            ReadNumber(&anim.nPadding[1], 11, sAnimationGeometryHeader + " > Padding");
            ReadNumber(&anim.nPadding[2], 11, sAnimationGeometryHeader + " > Padding");
            MarkDataBorder(nPosition - 1);

            anim.fLength = ReadNumber<float>(nullptr, 2, sAnimation + "Header");
            anim.fTransition = ReadNumber<float>(nullptr, 2, sAnimation + "Header");

            ReadString(&anim.sAnimRoot, 32, 3, sAnimation + "Header");

            ReadNumber(&anim.EventArray.nOffset, 6, sAnimation + "Header");
            ReadNumber(&anim.EventArray.nCount, 1, sAnimation + "Header");
            ReadNumber(&anim.EventArray.nCount2, 1, sAnimation + "Header");
            ReadNumber(&anim.nPadding2, 8, sAnimation + "Header");
            MarkDataBorder(nPosition - 1);

            if(anim.EventArray.nCount > 0){
                anim.Events.resize(anim.EventArray.nCount);
                nPosition = MdlArrayDataOffset(anim.EventArray, "animation event array"); /// No need to save the previous position because we've finished the header
                //std::cout << string_format("Offset to Animation Events is %i\n", anim.EventArray.nOffset);
                for(int e = 0; e < static_cast<int>(anim.Events.size()); e++){
                    Event & event = anim.Events.at(e);
                    event.fTime = ReadNumber<float>(nullptr, 2, sAnimation + "Header");
                    ReadString(&event.sName, 32, 3, sAnimation + "Header");
                    MarkDataBorder(nPosition - 1);
                }
            }

            /// We're done with the header, now we delve into animation nodes. It's a bit scary :(

            /// Prepare root node
            if(anim.nOffsetToRootAnimationNode == 0){
                throw mdlexception("Animation '" + anim.sName + "' has a zero root-node offset; refusing to read the animation header as a node.");
            }
            anim.RootAnimationNode.nOffset = anim.nOffsetToRootAnimationNode;
            anim.RootAnimationNode.nAnimation = n;

            /// Prepare for parsing and parse
            std::vector<unsigned int> offsets;
            offsets.reserve(Data.MH.nNodeCount);
            Vector vFromRoot;
            ParseNode(anim.RootAnimationNode, offsets, vFromRoot);

            /// Prepare for linearization and linearize
            anim.ArrayOfNodes.clear();
            anim.ArrayOfNodes.reserve(Data.MH.Names.size());
            LinearizeAnimation(anim.RootAnimationNode, anim.ArrayOfNodes, -1);
        }
    }
    //if(!bMinimal) ReportMdl << "Animation array read.\n";
    Report("Decompiling geometry...");
    //if(!bMinimal) ReportMdl << "Reading geometry.\n";
    if(Data.MH.Names.size() > 0){
        /// Set offset and animation
        if(Data.MH.GH.nOffsetToRootNode == 0){
            throw mdlexception("Geometry root-node offset is zero; refusing to read the geometry header as a node.");
        }
        Data.MH.RootNode.nOffset = Data.MH.GH.nOffsetToRootNode;
        Data.MH.RootNode.nAnimation = -1;

        /// Prepare variables for parsing nodes and then parse the nodes.
        std::vector<unsigned int> offsets;
        offsets.reserve(Data.MH.nNodeCount);
        Vector vFromRoot;
        ParseNode(Data.MH.RootNode, offsets, vFromRoot, bMinimal);

        /// Record total real node count
        Data.MH.nNodeCount = offsets.size();
        //std::cout << string_format("Node count for the Geometry: %i, compared to the number in the header, %i.\n", nNodeCounter, Data.MH.GH.nNumberOfNodes);
        //Data.MH.ArrayOfNodes.clear();

        /// Linearize read geometry into an Array Of Nodes
        const MdlInteger<unsigned short> nRootNameIndex = Data.MH.RootNode.Head.nNameIndex;
        if(!nRootNameIndex.Valid()){
            throw mdlexception("Binary read root node name index is invalid.");
        }
        const unsigned short nMaxGeometryNameIndex = MaxNameIndexInTree(Data.MH.RootNode);
        const std::size_t nFlattenedNodeCount = std::max<std::size_t>(Data.MH.nNodeCount, static_cast<std::size_t>(nMaxGeometryNameIndex) + 1u);
        Data.MH.ArrayOfNodes.clear();
        Data.MH.ArrayOfNodes.resize(nFlattenedNodeCount);
        LinearizeGeometry(Data.MH.RootNode, Data.MH.ArrayOfNodes);

        /// Build Array of Indices By Tree Order
        if(Data.MH.ArrayOfNodes.empty()){
            throw mdlexception("Binary read produced no geometry nodes.");
        }
        if(static_cast<unsigned short>(nRootNameIndex) >= Data.MH.ArrayOfNodes.size() ||
           !Data.MH.ArrayOfNodes.at(static_cast<unsigned short>(nRootNameIndex)).Head.nNameIndex.Valid()){
            throw mdlexception("Binary read root node name index is outside the flattened node array.");
        }
        Data.MH.NameIndicesInTreeOrder.reserve(Data.MH.ArrayOfNodes.size());
        Data.MH.BuildTreeOrderArray(Data.MH.ArrayOfNodes.at(static_cast<unsigned short>(nRootNameIndex)));

        /// Immediately fix all the skin bone->name maps
        for(Node & node : Data.MH.ArrayOfNodes){
            if(node.Head.nType & NODE_SKIN){
                int nMaxBonemapSlot = -1;
                for(unsigned int n = 0; n < node.Skin.Bones.size(); n++){
                    Bone & bone = node.Skin.Bones.at(n);
                    if(bone.nBonemap.Valid()){
                        const unsigned int nSlot = bone.nBonemap;
                        const unsigned int nMaxCompactSlot = bK2 ? 16u : 15u;
                        if(nSlot > nMaxCompactSlot){
                            throw mdlexception("Skin bone map slot is outside the compact-slot range for this game.");
                        }
                        if(nSlot > static_cast<unsigned int>(std::numeric_limits<int>::max())){
                            throw mdlexception("Skin bone map slot is too large.");
                        }
                        if(static_cast<int>(nSlot) > nMaxBonemapSlot) nMaxBonemapSlot = static_cast<int>(nSlot);
                    }
                }

                node.Skin.BoneNameIndices.clear();
                if(nMaxBonemapSlot >= 0){
                    node.Skin.BoneNameIndices.resize(static_cast<std::size_t>(nMaxBonemapSlot) + 1u);
                }

                for(unsigned int n = 0; n < node.Skin.Bones.size(); n++){
                    if(n >= Data.MH.NameIndicesInTreeOrder.size()){
                        throw mdlexception("Skin bone table references a node outside the tree-order name table.");
                    }
                    Bone & bone = node.Skin.Bones.at(n);
                    if(bone.nBonemap.Valid()){
                        const unsigned int nSlot = bone.nBonemap;
                        if(nSlot >= node.Skin.BoneNameIndices.size()){
                            throw mdlexception("Skin bone map slot is outside the compact bone-name table.");
                        }
                        node.Skin.BoneNameIndices.at(nSlot) = Data.MH.NameIndicesInTreeOrder.at(n);
                    }
                    bone.nNameIndex = Data.MH.NameIndicesInTreeOrder.at(n);
                }
            }
        }

    }
    //if(!bMinimal) ReportMdl << "Geometry read.\n";

    /// Here we'll go around and fix all the animation node numbers to match the geometry nodes.
    for(Animation & anim : Data.MH.Animations){
        /// Fix the name indices in node, name indices in controllers and child indices.
        for(Node & anim_node : anim.ArrayOfNodes){
            for(Node & geom_node : Data.MH.ArrayOfNodes){
                /// When you find the corresponding geom node, copy the name index
                if(anim_node.Head.nSupernodeNumber == geom_node.Head.nSupernodeNumber){
                    if(anim_node.Head.nNameIndex != geom_node.Head.nNameIndex){
                        /// We also need to correct it in all the controllers
                        for(Controller & ctrl : anim_node.Head.Controllers){
                            ctrl.nNameIndex = geom_node.Head.nNameIndex;
                        }
                        /// We need to find the parent node to change the child index
                        for(Node & anim_node2 : anim.ArrayOfNodes) if(anim_node2.Head.nSupernodeNumber == anim_node.Head.nParentIndex){
                            for(auto & child_ind : anim_node2.Head.ChildIndices) if(child_ind == anim_node.Head.nNameIndex)
                            {
                                child_ind = geom_node.Head.nNameIndex;
                                break;
                            }
                            break;
                        }
                        /// Finally, change the name index on the node itself
                        anim_node.Head.nNameIndex = geom_node.Head.nNameIndex;
                    }
                    break;
                }
            }
        }
        /// Fix parent indices.
        for(Node & anim_node : anim.ArrayOfNodes){
            for(Node & geom_node : Data.MH.ArrayOfNodes){
                /// When you find the corresponding parent geom node, copy the name index
                /// Q: why is the parent index set to the supernode number at this point?
                /// A: because otherwise how the heck are we gonna know that its the right one
                if(anim_node.Head.nParentIndex == geom_node.Head.nSupernodeNumber){
                    if(anim_node.Head.nParentIndex != geom_node.Head.nNameIndex){
                        anim_node.Head.nParentIndex = geom_node.Head.nNameIndex;
                    }
                    break;
                }
            }
        }
        /// Only now can we record the child indices // This is now done above already
        /*
        for(Node & anim_node : anim.ArrayOfNodes){
            anim_node.Head.ChildIndices.clear();
            for(Node & child : anim_node.Head.Children){
                anim_node.Head.ChildIndices.push_back(child.Head.nNameIndex);
            }
        }
        */
    }


    if(!bMinimal) ReportMdl << "Decompiled model in " << tDecompile.GetTime() << ".\n";
    if(!bReadSmoothing && bDetermineSmoothing && Mdx && !bMinimal) DetermineSmoothing();
    restoreReadState.Dismiss();
}

static unsigned nAabbCount = 0;
static std::string sAabbNodePrefix;
void MDL::ParseAabb(Aabb & aabb, std::vector<unsigned int> & visitedOffsets){
    std::string sAabb = sAabbNodePrefix + "Data > Aabb > Aabb Tree > Aabb Struct " + std::to_string(nAabbCount);
    nAabbCount++;

    if(aabb.nOffset == 0) throw mdlexception("An aabb node has offset 0; refusing to read the MDL header as AABB data.");
    if(aabb.nOffset == std::numeric_limits<unsigned int>::max()) throw mdlexception("An aabb node has offset -1.");
    if(std::find(visitedOffsets.begin(), visitedOffsets.end(), aabb.nOffset) != visitedOffsets.end()){
        throw mdlexception("The aabb (walkmesh) tree loops or reuses child offset " + std::to_string(aabb.nOffset) + "; refusing to duplicate or malform the preserved tree.");
    }
    visitedOffsets.push_back(aabb.nOffset);
    unsigned int nPosData = MdlDataOffset(aabb.nOffset, "AABB node");
    aabb.vBBmin.fX = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    aabb.vBBmin.fY = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    aabb.vBBmin.fZ = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    aabb.vBBmax.fX = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    aabb.vBBmax.fY = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    aabb.vBBmax.fZ = ReadNumber<float>(nullptr, 2, sAabb, &nPosData);
    ReadNumber(&aabb.nChild1, 6, sAabb, &nPosData);
    ReadNumber(&aabb.nChild2, 6, sAabb, &nPosData);
    aabb.nID = SignedIntToUShortOrInvalid(ReadNumber<signed int>(nullptr, 4, sAabb, &nPosData), sAabb + " face id");
    aabb.nProperty = ReadNumber<unsigned>(nullptr, 4, sAabb, &nPosData);
    MarkDataBorder(nPosData - 1);

    if(aabb.nChild1.Valid() && aabb.nChild1 > 0){
        aabb.Child1.resize(1);
        aabb.Child1[0].nOffset = aabb.nChild1;
        ParseAabb(aabb.Child1[0], visitedOffsets);
    }
    if(aabb.nChild2.Valid() && aabb.nChild2 > 0){
        aabb.Child2.resize(1);
        aabb.Child2[0].nOffset = aabb.nChild2;
        ParseAabb(aabb.Child2[0], visitedOffsets);
    }
}

void MDL::ParseNode(Node & node, std::vector<unsigned int> & offsets, Vector vFromRoot, bool bMinimal){
    if(node.nOffset == 0) throw mdlexception("ParseNode(): node offset is 0; refusing to read the MDL header as a node.");
    if(node.nOffset == std::numeric_limits<unsigned int>::max()) throw mdlexception("ParseNode(): node offset is -1; refusing to read invalid node data.");
    nPosition = RequiredMdlDataOffset(node.nOffset, "node header");
    unsigned int nPosData = 0;

    offsets.push_back(node.nOffset);

    if(nPosition > sBuffer.size() || 4 + sizeof(unsigned short) > sBuffer.size() - nPosition){
        throw mdlexception("ParseNode(): node header is truncated before the name index field.");
    }
    unsigned short nNodeNameIndex = 0;
    std::memcpy(&nNodeNameIndex, &sBuffer.at(nPosition + 4), sizeof(nNodeNameIndex));
    if(nNodeNameIndex >= GetFileData()->MH.Names.size()){
        throw mdlexception("ParseNode(): node name index " + std::to_string(nNodeNameIndex) +
                           " is outside the model name table; refusing to read from the wrong name/string data.");
    }
    std::string sNodeName = std::string(GetFileData()->MH.Names.at(nNodeNameIndex).sName.c_str());
    std::string sNodePrefix = "Geometry > ";
    if(node.nAnimation.Valid() && static_cast<unsigned>(node.nAnimation) < GetFileData()->MH.Animations.size()){
        sNodePrefix = "Animations > " + GetFileData()->MH.Animations.at(static_cast<unsigned>(node.nAnimation)).sName + " > ";
    }
    std::string sNode = sNodePrefix + sNodeName + " > ";

    ReadNumber(node.Head.nType.GetPtr(), 5, sNode + "Node Type");
    ReadNumber(node.Head.nSupernodeNumber.GetPtr(), 5, sNode + "Supernode Number");
    ReadNumber(node.Head.nNameIndex.GetPtr(), 5, sNode + "Node Number");
    ReadNumber(&node.Head.nPadding1, 8, sNode + (node.nAnimation.Valid() ? "Header" : "Header > Basic"));


    ReportObject ReportMdl (*this);
    //ReportMdl << "Reading " << (node.nAnimation.Valid() ? "animation" : "geometry") << " node " << GetNodeName(node) << " at offset " << nPosition - 8 << ".\n";

    if(!(node.Head.nType & NODE_HEADER)) throw mdlexception(std::string("The ") + (node.nAnimation.Valid() ? "animation" : "geometry") + " node " + GetNodeName(node) + " does not have a NODE_HEADER.");

    ReadNumber(node.Head.nOffsetToRoot.GetPtr(), 6, sNode + "Offset to Root");
    ReadNumber(node.Head.nOffsetToParent.GetPtr(), 6, sNode + "Offset to Parent");
    node.Head.vPos.fX = ReadNumber<float>(nullptr, 2, sNode + "Position");
    node.Head.vPos.fY = ReadNumber<float>(nullptr, 2, sNode + "Position");
    node.Head.vPos.fZ = ReadNumber<float>(nullptr, 2, sNode + "Position");

    double fQW = ReadNumber<float>(nullptr, 2, sNode + "Orientation");
    double fQX = ReadNumber<float>(nullptr, 2, sNode + "Orientation");
    double fQY = ReadNumber<float>(nullptr, 2, sNode + "Orientation");
    double fQZ = ReadNumber<float>(nullptr, 2, sNode + "Orientation");
    node.Head.oOrient.SetQuaternion(fQX, fQY, fQZ, fQW);

    ReadNumber(&node.Head.ChildrenArray.nOffset, 6, sNode + "Child Array");
    ReadNumber(&node.Head.ChildrenArray.nCount, 1, sNode + "Child Array");
    ReadNumber(&node.Head.ChildrenArray.nCount2, 1, sNode + "Child Array");

    ReadNumber(&node.Head.ControllerArray.nOffset, 6, sNode + "Controller Array");
    ReadNumber(&node.Head.ControllerArray.nCount, 1, sNode + "Controller Array");
    ReadNumber(&node.Head.ControllerArray.nCount2, 1, sNode + "Controller Array");

    ReadNumber(&node.Head.ControllerDataArray.nOffset, 6, sNode + "Controller Data Array");
    ReadNumber(&node.Head.ControllerDataArray.nCount, 1, sNode + "Controller Data Array");
    ReadNumber(&node.Head.ControllerDataArray.nCount2, 1, sNode + "Controller Data Array");
    MarkDataBorder(nPosition - 1);

    if(node.Head.ControllerDataArray.nCount > 0 && !bMinimal){
        /// We gots controll data!
        node.Head.ControllerData.resize(node.Head.ControllerDataArray.nCount);
        nPosData = MdlArrayDataOffset(node.Head.ControllerDataArray, "controller data array");
        for(unsigned int n = 0; n < node.Head.ControllerDataArray.nCount; n++){
            std::string sControllerData = sNode + (node.nAnimation.Valid() ? "Controller Data > Float " : "Data > Controller Data > Float ") + std::to_string(n);
            node.Head.ControllerData[n] = ReadNumber<float>(nullptr, 2, sControllerData, &nPosData);
            MarkDataBorder(nPosData - 1);
            //if(n == node.Head.ControllerDataArray.nCount) std::cout << string_format("Just filled %i floats of Controller Data\n", n);
        }
    }

    if(node.Head.ControllerArray.nCount > 0 && !bMinimal){
        node.Head.Controllers.resize(node.Head.ControllerArray.nCount);
        nPosData = MdlArrayDataOffset(node.Head.ControllerArray, "controller array");
        for(unsigned int n = 0; n < node.Head.ControllerArray.nCount; n++){
            Controller & ctrl = node.Head.Controllers.at(n);


            std::string sController = sNode + (node.nAnimation.Valid() ? "Controllers > Controller " : "Data > Controllers > Controller ") + std::to_string(n);
            ReadNumber(&ctrl.nControllerType, 4, sController, &nPosData);
            ReadNumber(&ctrl.nUnknown2, 10, sController, &nPosData);
            ReadNumber(&ctrl.nValueCount, 5, sController, &nPosData);
            ReadNumber(&ctrl.nTimekeyStart, 5, sController, &nPosData);
            ReadNumber(&ctrl.nDataStart, 5, sController, &nPosData);

            ReadNumber(&ctrl.nColumnCount, 5, sController, &nPosData);
            ReadNumber(&ctrl.nPadding[0], 11, sController, &nPosData);
            ReadNumber(&ctrl.nPadding[1], 11, sController, &nPosData);
            ReadNumber(&ctrl.nPadding[2], 11, sController, &nPosData);
            MarkDataBorder(nPosData - 1);

            ctrl.nNameIndex = node.Head.nNameIndex;
            ctrl.nAnimation = node.nAnimation;

            if(ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION && node.nAnimation.Valid()){
                if(ctrl.nColumnCount == 2 && !FH->MH.bCompressQuaternions) FH->MH.bCompressQuaternions = true;
                else if(ctrl.nColumnCount != 2 && FH->MH.bCompressQuaternions) FH->MH.bCompressQuaternions = false;
            }

            int nCount = ctrl.nColumnCount & 0x0F;
            if(ctrl.nControllerType == CONTROLLER_HEADER_ORIENTATION && nCount == 2) nCount = 1;
            if(ctrl.nColumnCount & 0x10) nCount *= 3;

            const std::size_t nColumns = static_cast<std::size_t>(std::max(nCount, 0));
            const std::size_t nValues = static_cast<std::size_t>(ctrl.nValueCount);
            const std::size_t nStart = static_cast<std::size_t>(ctrl.nDataStart);
            bool bMissingControllerData = false;
            if(nValues != 0 && nColumns > (std::numeric_limits<std::size_t>::max() - nStart) / nValues){
                bMissingControllerData = true;
            }
            else if(nStart + nColumns * nValues > node.Head.ControllerData.size()){
                bMissingControllerData = true;
            }

            if(bMissingControllerData){
                std::string sAnimName = "<geometry>";
                if(node.nAnimation.Valid() && static_cast<std::size_t>(static_cast<unsigned int>(node.nAnimation)) < FH->MH.Animations.size()){
                    sAnimName = FH->MH.Animations.at(static_cast<unsigned int>(node.nAnimation)).sName;
                }
                Warning("Missing controller data on a controller on node '" + GetNodeName(node) + "' in animation '" + sAnimName + "'.");
            }
        }
    }

    /// Calculate transforms after controller data has been read: rotate the
    /// incoming offset by the current node orientation, then add the current
    /// node position.
    Location location = node.GetLocation();
    Quaternion qNode = location.oOrientation.GetQuaternion();
    vFromRoot.Rotate(qNode);
    vFromRoot += location.vPosition;

    node.Head.vFromRoot = vFromRoot;

    unsigned nSavePosition = nPosition;

    if(node.Head.ChildrenArray.nCount > 0){
        /// We gots children!
        node.Head.Children.resize(node.Head.ChildrenArray.nCount);
        node.Head.ChildIndices.clear();
        nPosData = MdlArrayDataOffset(node.Head.ChildrenArray, "child array");
        for(int n = 0; n < static_cast<int>(node.Head.Children.size()); n++){
            Node & child = node.Head.Children.at(n);
            std::string sChildPointer = sNode + (node.nAnimation.Valid() ? "Child Pointers > Pointer " : "Data > Child Array > Pointer ") + std::to_string(n);
            ReadNumber(&child.nOffset, 6, sChildPointer, &nPosData);
            MarkDataBorder(nPosData - 1);
            child.nAnimation = node.nAnimation;

            /// If this is a geometry node, then the name index is reliable and we may use it for the parent index.
            /// If however this is an animation node, the name index is not reliable, so we will put in the supernode number instead
            /// and then use it later to find the correct name index.
            child.Head.nParentIndex = !node.nAnimation.Valid() ? node.Head.nNameIndex : node.Head.nSupernodeNumber;

            if(child.nOffset == 0 || child.nOffset == std::numeric_limits<unsigned int>::max()){
                ReportMdl << "Warning: node '" << GetNodeName(node) << "' has an invalid child pointer ("
                          << child.nOffset << "); ignoring that child entry.\n";
                child.nOffset = 0;
            }
            else if(std::find(offsets.begin(), offsets.end(), child.nOffset) == offsets.end()){
                ParseNode(child, offsets, vFromRoot, bMinimal);
            }
            else{
                /**
                    Okay, so it seems that the offset of our child has already been parsed. This means that there is a loop in the model.
                    I will solve this loop now by not parsing this child. So this will break the loop, but it will leave this unread child behind.
                    It is now the job of the linearization functions to ignore this child. I will mark this child by setting its offset to 0.
                    So, when the offset of a node is 0, linearization of that node should not take place.
                */
                child.nOffset = 0;
            }

            if(child.nOffset != 0 && child.Head.nNameIndex.Valid()){
                node.Head.ChildIndices.push_back(child.Head.nNameIndex); /// Sorting based on child indices? Does that even work for animations?
            }
        }
    }

    /// If we only want to read minimally, we're done at this point
    if(bMinimal) return;

    /// Return the position to where we were before doing all the children in between
    nPosition = nSavePosition;

    if(node.Head.nType & NODE_LIGHT){
        std::string sLight = sNode.substr(0, sNode.size() - 3) + " > Header > Light";
        node.Light.fFlareRadius = ReadNumber<float>(nullptr, 2, sLight);
        ReadNumber(&node.Light.UnknownArray.nOffset, 8, sLight);
        ReadNumber(&node.Light.UnknownArray.nCount, 8, sLight);
        ReadNumber(&node.Light.UnknownArray.nCount2, 8, sLight);
        ReadNumber(&node.Light.FlareSizeArray.nOffset, 6, sLight);
        ReadNumber(&node.Light.FlareSizeArray.nCount, 1, sLight);
        ReadNumber(&node.Light.FlareSizeArray.nCount2, 1, sLight);
        ReadNumber(&node.Light.FlarePositionArray.nOffset, 6, sLight);
        ReadNumber(&node.Light.FlarePositionArray.nCount, 1, sLight);
        ReadNumber(&node.Light.FlarePositionArray.nCount2, 1, sLight);
        ReadNumber(&node.Light.FlareColorShiftArray.nOffset, 6, sLight);
        ReadNumber(&node.Light.FlareColorShiftArray.nCount, 1, sLight);
        ReadNumber(&node.Light.FlareColorShiftArray.nCount2, 1, sLight);
        ReadNumber(&node.Light.FlareTextureNameArray.nOffset, 6, sLight);
        ReadNumber(&node.Light.FlareTextureNameArray.nCount, 1, sLight);
        ReadNumber(&node.Light.FlareTextureNameArray.nCount2, 1, sLight);

        ReadNumber(node.Light.nLightPriority.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nAmbientOnly.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nDynamicType.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nAffectDynamic.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nShadow.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nFlare.GetPtr(), 4, sLight);
        ReadNumber(node.Light.nFadingLight.GetPtr(), 4, sLight);
        MarkDataBorder(nPosition - 1);


        if(node.Light.FlareTextureNameArray.nCount > 0){
            node.Light.FlareTextureNames.resize(node.Light.FlareTextureNameArray.nCount);
            nPosData = MdlArrayDataOffset(node.Light.FlareTextureNameArray, "light flare texture-name array");
            for(unsigned int n = 0; n < node.Light.FlareTextureNameArray.nCount; n++){

                ReadNumber(&node.Light.FlareTextureNames[n].nOffset, 6, sLight, &nPosData);
                MarkDataBorder(nPosData - 1);

                unsigned nPosData2 = RequiredMdlDataOffset(node.Light.FlareTextureNames[n].nOffset, "light flare texture name");


                ReadString(&node.Light.FlareTextureNames[n].sName, 0, 3, sLight, &nPosData2);
            }
        }
        if(node.Light.FlareSizeArray.nCount > 0){
            node.Light.FlareSizes.resize(node.Light.FlareSizeArray.nCount);
            nPosData = MdlArrayDataOffset(node.Light.FlareSizeArray, "light flare size array");
            for(unsigned int n = 0; n < node.Light.FlareSizeArray.nCount; n++){

                node.Light.FlareSizes[n] = ReadNumber<float>(nullptr, 2, sLight, &nPosData);
                MarkDataBorder(nPosData - 1);
            }
        }
        if(node.Light.FlarePositionArray.nCount > 0){
            node.Light.FlarePositions.resize(node.Light.FlarePositionArray.nCount);
            nPosData = MdlArrayDataOffset(node.Light.FlarePositionArray, "light flare position array");
            for(unsigned int n = 0; n < node.Light.FlarePositionArray.nCount; n++){

                node.Light.FlarePositions[n] = ReadNumber<float>(nullptr, 2, sLight, &nPosData);
                MarkDataBorder(nPosData - 1);
            }
        }
        if(node.Light.FlareColorShiftArray.nCount > 0){
            node.Light.FlareColorShifts.resize(node.Light.FlareColorShiftArray.nCount);
            nPosData = MdlArrayDataOffset(node.Light.FlareColorShiftArray, "light flare color-shift array");
            for(unsigned int n = 0; n < node.Light.FlareColorShiftArray.nCount; n++){

                node.Light.FlareColorShifts[n].fR = ReadNumber<float>(nullptr, 2, sLight, &nPosData);
                node.Light.FlareColorShifts[n].fG = ReadNumber<float>(nullptr, 2, sLight, &nPosData);
                node.Light.FlareColorShifts[n].fB = ReadNumber<float>(nullptr, 2, sLight, &nPosData);
                MarkDataBorder(nPosData - 1);
            }
        }
    }

    if(node.Head.nType & NODE_EMITTER){
        std::string sEmitter = sNode.substr(0, sNode.size() - 3) + " > Header > Emitter";
        node.Emitter.fDeadSpace = ReadNumber<float>(nullptr, 2, sEmitter);
        node.Emitter.fBlastRadius = ReadNumber<float>(nullptr, 2, sEmitter);
        node.Emitter.fBlastLength = ReadNumber<float>(nullptr, 2, sEmitter);

        ReadNumber(&node.Emitter.nBranchCount, 1, sEmitter);
        node.Emitter.fControlPointSmoothing = ReadNumber<float>(nullptr, 2, sEmitter);

        ReadNumber(&node.Emitter.nxGrid, 4, sEmitter);
        ReadNumber(&node.Emitter.nyGrid, 4, sEmitter);

        ReadNumber(&node.Emitter.nSpawnType, 4, sEmitter);

        ReadString(&node.Emitter.cUpdate, 32, 3, sEmitter);
        ReadString(&node.Emitter.cRender, 32, 3, sEmitter);
        ReadString(&node.Emitter.cBlend, 32, 3, sEmitter);
        ReadString(&node.Emitter.cTexture, 32, 3, sEmitter);
        ReadString(&node.Emitter.cChunkName, 16, 3, sEmitter);

        ReadNumber(&node.Emitter.nTwosidedTex, 4, sEmitter);
        ReadNumber(&node.Emitter.nLoop, 4, sEmitter);
        ReadNumber(&node.Emitter.nRenderOrder, 5, sEmitter);
        ReadNumber(&node.Emitter.nFrameBlending, 7, sEmitter);
        ReadString(&node.Emitter.cDepthTextureName, 32, 3, sEmitter);
        ReadNumber(&node.Emitter.nPadding1, 11, sEmitter);
        ReadNumber(&node.Emitter.nFlags, 4, sEmitter);
        MarkDataBorder(nPosition - 1);
    }

    if(node.Head.nType & NODE_REFERENCE){
        std::string sReference = sNode.substr(0, sNode.size() - 3) + " > Header > Reference";
        ReadString(&node.Reference.sRefModel, 32, 3, sReference);
        ReadNumber(&node.Reference.nReattachable, 4, sReference);
        MarkDataBorder(nPosition - 1);
    }

    if(node.Head.nType & NODE_MESH){
        std::string sMesh = sNode.substr(0, sNode.size() - 3) + " > Header > Mesh";
        std::string sMeshData = sNode.substr(0, sNode.size() - 3);
      try{
        ReadNumber(&node.Mesh.nFunctionPointer0, 9, sMesh);
        ReadNumber(&node.Mesh.nFunctionPointer1, 9, sMesh);

        ReadNumber(&node.Mesh.FaceArray.nOffset, 6, sMesh);
        ReadNumber(&node.Mesh.FaceArray.nCount, 1, sMesh);
        ReadNumber(&node.Mesh.FaceArray.nCount2, 1, sMesh);

        node.Mesh.vBBmin.fX = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vBBmin.fY = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vBBmin.fZ = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vBBmax.fX = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vBBmax.fY = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vBBmax.fZ = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fRadius = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vAverage.fX = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vAverage.fY = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.vAverage.fZ = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fDiffuse.fR = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fDiffuse.fG = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fDiffuse.fB = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fAmbient.fR = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fAmbient.fG = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fAmbient.fB = ReadNumber<float>(nullptr, 2, sMesh);

        ReadNumber(&node.Mesh.nTransparencyHint, 4, sMesh);

        ReadString(&node.Mesh.cTexture1, 32, 3, sMesh);
        ReadString(&node.Mesh.cTexture2, 32, 3, sMesh);
        ReadString(&node.Mesh.cTexture3, 12, 3, sMesh);
        ReadString(&node.Mesh.cTexture4, 12, 3, sMesh);

        ReadNumber(&node.Mesh.IndexCounterArray.nOffset, 6, sMesh);
        ReadNumber(&node.Mesh.IndexCounterArray.nCount, 1, sMesh);
        ReadNumber(&node.Mesh.IndexCounterArray.nCount2, 1, sMesh);

        ReadNumber(&node.Mesh.IndexLocationArray.nOffset, 6, sMesh);
        ReadNumber(&node.Mesh.IndexLocationArray.nCount, 1, sMesh);
        ReadNumber(&node.Mesh.IndexLocationArray.nCount2, 1, sMesh);

        ReadNumber(&node.Mesh.MeshInvertedCounterArray.nOffset, 6, sMesh);
        ReadNumber(&node.Mesh.MeshInvertedCounterArray.nCount, 1, sMesh);
        ReadNumber(&node.Mesh.MeshInvertedCounterArray.nCount2, 1, sMesh);

        ReadNumber(&node.Mesh.nUnknown3[0], 11, sMesh);
        ReadNumber(&node.Mesh.nUnknown3[1], 8, sMesh);
        ReadNumber(&node.Mesh.nUnknown3[2], 8, sMesh);

        ReadNumber(&node.Mesh.nSaberUnknown1, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown2, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown3, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown4, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown5, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown6, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown7, 11, sMesh);
        ReadNumber(&node.Mesh.nSaberUnknown8, 11, sMesh);

        ReadNumber(&node.Mesh.nAnimateUV, 4, sMesh);
        node.Mesh.fUVDirectionX = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fUVDirectionY = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fUVJitter = ReadNumber<float>(nullptr, 2, sMesh);
        node.Mesh.fUVJitterSpeed = ReadNumber<float>(nullptr, 2, sMesh);

        ReadNumber(&node.Mesh.nMdxDataSize, 1, sMesh);
        ReadNumber(&node.Mesh.nMdxDataBitmap, 4, sMesh);

        ReadNumber(node.Mesh.nOffsetToMdxVertex.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxNormal.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxColor.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxUV1.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxUV2.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxUV3.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxUV4.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxTangent1.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxTangent2.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxTangent3.GetPtr(), 6, sMesh);
        ReadNumber(node.Mesh.nOffsetToMdxTangent4.GetPtr(), 6, sMesh);

        ReadNumber(&node.Mesh.nNumberOfVerts, 1, sMeshData + " > Data > Mesh > Pointer to Vert Number");
        ReadNumber(&node.Mesh.nTextureNumber, 1, sMesh);

        ReadNumber(&node.Mesh.nHasLightmap, 7, sMesh);
        ReadNumber(&node.Mesh.nRotateTexture, 7, sMesh);
        ReadNumber(&node.Mesh.nBackgroundGeometry, 7, sMesh);
        ReadNumber(&node.Mesh.nShadow, 7, sMesh);
        ReadNumber(&node.Mesh.nBeaming, 7, sMesh);
        ReadNumber(&node.Mesh.nRender, 7, sMesh);

        if(bK2) ReadNumber(&node.Mesh.nDirtEnabled, 7, sMesh);
        if(bK2) ReadNumber(&node.Mesh.nPadding1, 11, sMesh);
        if(bK2) ReadNumber(&node.Mesh.nDirtTexture, 5, sMesh);
        if(bK2) ReadNumber(&node.Mesh.nDirtCoordSpace, 5, sMesh);
        if(bK2) ReadNumber(&node.Mesh.nHideInHolograms, 7, sMesh);
        if(bK2) ReadNumber(&node.Mesh.nPadding2, 11, sMesh);

        ReadNumber(&node.Mesh.nPadding3, 11, sMesh);

        node.Mesh.fTotalArea = ReadNumber<float>(nullptr, 2, sMesh);
        ReadNumber(&node.Mesh.nPadding, 8, sMesh);
        ReadNumber(&node.Mesh.nOffsetIntoMdx, 6, sMesh);
        if(!bXbox) ReadNumber(&node.Mesh.nOffsetToVertArray, 6, sMesh);
        MarkDataBorder(nPosition - 1);
      }
      catch(const std::exception & e){
        throw mdlexception("In " + GetNodeName(node) + ", reading trimesh header: " + e.what());
      }

        if(node.Mesh.IndexCounterArray.nCount > 0){
            //I am assuming here that the pointer can only ever be a single one
            nPosData = MdlArrayDataOffset(node.Mesh.IndexCounterArray, "mesh index-counter array");
            ReadNumber(&node.Mesh.nVertIndicesCount, 1, sMeshData + " > Data > Mesh > Inverted Counter", &nPosData);
            MarkDataBorder(nPosData - 1);
        }
        else node.Mesh.nVertIndicesCount = 0;

        if(node.Mesh.IndexLocationArray.nCount > 0){
            //I am assuming here that the pointer can only ever be a single one
            nPosData = MdlArrayDataOffset(node.Mesh.IndexLocationArray, "mesh index-location array");
            ReadNumber(&node.Mesh.nVertIndicesLocation, 6, sMeshData + " > Data > Mesh > Pointer to Vert Indices", &nPosData);
            MarkDataBorder(nPosData - 1);
        }
        else node.Mesh.nVertIndicesLocation = 0;

        if(node.Mesh.MeshInvertedCounterArray.nCount > 0){
            //I am assuming here that the pointer can only ever be a single one
            nPosData = MdlArrayDataOffset(node.Mesh.MeshInvertedCounterArray, "mesh inverted-counter array");
            ReadNumber(node.Mesh.nMeshInvertedCounter.GetPtr(), 4, sMeshData + " > Data > Mesh > Inverted Counter", &nPosData);
            MarkDataBorder(nPosData - 1);
        }
        else node.Mesh.nMeshInvertedCounter = 0;

        if(node.Mesh.FaceArray.nCount > 0){
            node.Mesh.Faces.resize(node.Mesh.FaceArray.nCount);
            if(node.Mesh.IndexLocationArray.nCount > 0) node.Mesh.VertIndices.resize(node.Mesh.FaceArray.nCount);
            nPosData = MdlArrayDataOffset(node.Mesh.FaceArray, "mesh face array");
            unsigned int nPosData2;
            if(node.Mesh.IndexLocationArray.nCount > 0) nPosData2 = RequiredMdlDataOffset(node.Mesh.nVertIndicesLocation, "mesh inverted-index data");
            for(unsigned int n = 0; n < node.Mesh.FaceArray.nCount; n++){
                node.Mesh.Faces.at(n).nNameIndex = node.Head.nNameIndex;


                node.Mesh.Faces[n].vNormal.fX = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                node.Mesh.Faces[n].vNormal.fY = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                node.Mesh.Faces[n].vNormal.fZ = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                node.Mesh.Faces[n].fDistance = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);

                ReadNumber(&node.Mesh.Faces[n].nMaterialID, 4, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                //if(!bDetermineSmoothing) node.Mesh.Faces.at(n).nSmoothingGroup = node.Mesh.Faces[n].nMaterialID;

                ReadNumber(node.Mesh.Faces[n].nAdjacentFaces[0].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                ReadNumber(node.Mesh.Faces[n].nAdjacentFaces[1].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                ReadNumber(node.Mesh.Faces[n].nAdjacentFaces[2].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                ReadNumber(node.Mesh.Faces[n].nIndexVertex[0].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData);
                ReadNumber(node.Mesh.Faces[n].nIndexVertex[1].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData);
                ReadNumber(node.Mesh.Faces[n].nIndexVertex[2].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData);
                MarkDataBorder(nPosData - 1);

                if(node.Mesh.IndexLocationArray.nCount > 0){
                    //std::cout << string_format("Reading VertIndices for face %i of %i, at position %i.\n", n, node.Mesh.FaceArray.nCount, nPosData2);
                    ReadNumber(&node.Mesh.VertIndices[n].at(0), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData2);
                    ReadNumber(&node.Mesh.VertIndices[n].at(1), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData2);
                    ReadNumber(&node.Mesh.VertIndices[n].at(2), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(n), &nPosData2);
                    MarkDataBorder(nPosData2 - 1);
                }
            }
            if(bReadSmoothing){
                for(unsigned int n = 0; n < node.Mesh.FaceArray.nCount; n++){
                    ReadNumber(&node.Mesh.Faces.at(n).nSmoothingGroup, 4, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(n), &nPosData);
                    MarkDataBorder(nPosData - 1);
                }
            }
        }
    }

    if(node.Head.nType & NODE_DANGLY){
        std::string sDangly = sNode.substr(0, sNode.size() - 3) + " > Header > Danglymesh";
        std::string sDanglyData = sNode.substr(0, sNode.size() - 3);
        ReadNumber(&node.Dangly.ConstraintArray.nOffset, 6, sDangly);
        ReadNumber(&node.Dangly.ConstraintArray.nCount, 1, sDangly);
        ReadNumber(&node.Dangly.ConstraintArray.nCount2, 1, sDangly);

        node.Dangly.fDisplacement = ReadNumber<float>(nullptr, 2, sDangly);
        node.Dangly.fTightness = ReadNumber<float>(nullptr, 2, sDangly);
        node.Dangly.fPeriod = ReadNumber<float>(nullptr, 2, sDangly);

        ReadNumber(&node.Dangly.nOffsetToData2, 6, sDangly);
        MarkDataBorder(nPosition - 1);

        if(node.Dangly.ConstraintArray.nCount > 0){
            node.Dangly.Constraints.resize(node.Dangly.ConstraintArray.nCount);
            node.Dangly.Data2.resize(node.Dangly.ConstraintArray.nCount);
            nPosData = MdlArrayDataOffset(node.Dangly.ConstraintArray, "dangly constraint array");
            unsigned int nPosData2 = RequiredMdlDataOffset(node.Dangly.nOffsetToData2, "dangly data2 array");
            for(unsigned int n = 0; n < node.Dangly.ConstraintArray.nCount; n++){

                node.Dangly.Constraints.at(n) = ReadNumber<float>(nullptr, 2, sDanglyData + " > Data > Danglymesh > Constraints > Constraint " + std::to_string(n), &nPosData);
                MarkDataBorder(nPosData - 1);

                node.Dangly.Data2.at(n).fX = ReadNumber<float>(nullptr, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(n), &nPosData2);
                node.Dangly.Data2.at(n).fY = ReadNumber<float>(nullptr, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(n), &nPosData2);
                node.Dangly.Data2.at(n).fZ = ReadNumber<float>(nullptr, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(n), &nPosData2);
                MarkDataBorder(nPosData2 - 1);
            }
        }
    }

    if(node.Head.nType & NODE_SKIN){
        std::string sSkin = sNode.substr(0, sNode.size() - 3) + " > Header > Skin";
        std::string sSkinData = sNode.substr(0, sNode.size() - 3);
        ReadNumber(&node.Skin.UnknownArray.nOffset, 8, sSkin);
        ReadNumber(&node.Skin.UnknownArray.nCount, 8, sSkin);
        ReadNumber(&node.Skin.UnknownArray.nCount2, 8, sSkin);
        ReadNumber(&node.Skin.nOffsetToMdxWeightValues, 6, sSkin);
        ReadNumber(&node.Skin.nOffsetToMdxBoneIndices, 6, sSkin);

        ReadNumber(&node.Skin.nOffsetToBonemap, 6, sSkin);
        ReadNumber(&node.Skin.nNumberOfBonemap, 1, sSkin);

        ReadNumber(&node.Skin.QBoneArray.nOffset, 6, sSkin);
        ReadNumber(&node.Skin.QBoneArray.nCount, 1, sSkin);
        ReadNumber(&node.Skin.QBoneArray.nCount2, 1, sSkin);

        ReadNumber(&node.Skin.TBoneArray.nOffset, 6, sSkin);
        ReadNumber(&node.Skin.TBoneArray.nCount, 1, sSkin);
        ReadNumber(&node.Skin.TBoneArray.nCount2, 1, sSkin);

        ReadNumber(&node.Skin.Array8Array.nOffset, 6, sSkin);
        ReadNumber(&node.Skin.Array8Array.nCount, 1, sSkin);
        ReadNumber(&node.Skin.Array8Array.nCount2, 1, sSkin);

        for(int n = 0; n < 16; n++){
            ReadNumber(&node.Skin.nBoneIndices[n], 5, sSkin);
        }
        ReadNumber(&node.Skin.nPadding1, 11, sSkin);
        ReadNumber(&node.Skin.nPadding2, 11, sSkin);
        MarkDataBorder(nPosition - 1);

        if(node.Skin.nNumberOfBonemap != node.Skin.QBoneArray.nCount ||
           node.Skin.nNumberOfBonemap != node.Skin.QBoneArray.nCount2 ||
           node.Skin.nNumberOfBonemap != node.Skin.TBoneArray.nCount ||
           node.Skin.nNumberOfBonemap != node.Skin.TBoneArray.nCount2 ||
           node.Skin.nNumberOfBonemap != node.Skin.Array8Array.nCount ||
           node.Skin.nNumberOfBonemap != node.Skin.Array8Array.nCount2){
            throw mdlexception("Skin bone array counts do not match the bonemap count on node '" + GetNodeName(node) +
                               "'; refusing to load mismatched bind-pose data into the skin.");
        }
        if(node.Skin.nNumberOfBonemap > 0){
            node.Skin.Bones.resize(node.Skin.nNumberOfBonemap);
            // BoneNameIndices is compact-slot indexed and is rebuilt after
            // geometry linearization, once tree-order name indices are known.
            // Do not size it by vertex count here; that can leave stale or
            // misleading state if parsing fails before the later rebuild.
            nPosData = RequiredMdlDataOffset(node.Skin.nOffsetToBonemap, "skin bonemap array");
            unsigned int nPosData2 = MdlArrayDataOffset(node.Skin.QBoneArray, "skin qbone array");
            unsigned int nPosData3 = MdlArrayDataOffset(node.Skin.TBoneArray, "skin tbone array");
            unsigned int nPosData4 = MdlArrayDataOffset(node.Skin.Array8Array, "skin array8");
            for(unsigned int n = 0; n < node.Skin.nNumberOfBonemap; n++){
                if(bXbox){

                    ReadNumber(&node.Skin.Bones[n].nBonemap, 5, sSkinData + " > Data > Skin > Bonemap > " + std::to_string(n), &nPosData);
                }
                else{

                    node.Skin.Bones[n].nBonemap = (unsigned short) ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > Bonemap > " + std::to_string(n), &nPosData);
                }
                MarkDataBorder(nPosData - 1);


                double fQW = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(n), &nPosData2);
                double fQX = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(n), &nPosData2);
                double fQY = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(n), &nPosData2);
                double fQZ = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(n), &nPosData2);
                MarkDataBorder(nPosData2 - 1);
                node.Skin.Bones[n].QBone.SetQuaternion(fQX, fQY, fQZ, fQW);


                node.Skin.Bones[n].TBone.fX = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(n), &nPosData3);
                node.Skin.Bones[n].TBone.fY = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(n), &nPosData3);
                node.Skin.Bones[n].TBone.fZ = ReadNumber<float>(nullptr, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(n), &nPosData3);
                MarkDataBorder(nPosData3 - 1);


                ReadNumber(&node.Skin.Bones[n].nPadding, 11, sSkinData + " > Data > Skin > Array8 (unused)", &nPosData4);
                if(n + 1 >= node.Skin.nNumberOfBonemap) MarkDataBorder(nPosData4 - 1);
            }
        }
    }

    ///Need to do this later so that the Skin data is already read
    if(node.Head.nType & NODE_MESH){
        std::string sMdx = GetNodeName(node) + " > ";
        std::string sMeshData = sNode.substr(0, sNode.size() - 3);
        if(node.Mesh.nNumberOfVerts > 0){
            //ReportMdl << "Node " << node.Head.nNameIndex << ": \n";
            node.Mesh.Vertices.resize(node.Mesh.nNumberOfVerts);
            nPosData = !bXbox ? RequiredMdlDataOffset(node.Mesh.nOffsetToVertArray, "mesh MDL vertex array") : 0u;
            unsigned int nPosData2 = node.Mesh.nOffsetIntoMdx;
            if(nPosData2 > 0 && Mdx) Mdx->MarkDataBorder(nPosData2 - 1);
            unsigned nMaxDataStructs = 0;
            for(int n = 0; n < node.Mesh.nNumberOfVerts; n++){
                nMaxDataStructs++;
                if(!bXbox){

                    node.Mesh.Vertices[n].nOffset = nPosData;
                    node.Mesh.Vertices[n].fX = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(n), &nPosData);
                    node.Mesh.Vertices[n].fY = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(n), &nPosData);
                    node.Mesh.Vertices[n].fZ = ReadNumber<float>(nullptr, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(n), &nPosData);
                    MarkDataBorder(nPosData - 1);

                    node.Mesh.Vertices[n].vFromRoot = node.Mesh.Vertices[n];
                    node.Mesh.Vertices[n].vFromRoot.Rotate(qNode);
                    node.Mesh.Vertices[n].vFromRoot += node.Head.vFromRoot;
                }

                node.Mesh.Vertices[n].MDXData.nNameIndex = node.Head.nNameIndex;
                if(node.Mesh.nMdxDataSize > 0 && Mdx){
                try{
                    std::string sMdxVertex = sMdx + "Vertex " + std::to_string(n);

                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_VERTEX){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxVertex;

                        node.Mesh.Vertices[n].MDXData.vVertex.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vVertex.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vVertex.fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);

                        if(bXbox){
                            node.Mesh.Vertices[n].fX = node.Mesh.Vertices[n].MDXData.vVertex.fX;
                            node.Mesh.Vertices[n].fY = node.Mesh.Vertices[n].MDXData.vVertex.fY;
                            node.Mesh.Vertices[n].fZ = node.Mesh.Vertices[n].MDXData.vVertex.fZ;

                            node.Mesh.Vertices[n].vFromRoot = node.Mesh.Vertices[n];
                            node.Mesh.Vertices[n].vFromRoot.Rotate(qNode);
                            node.Mesh.Vertices[n].vFromRoot += node.Head.vFromRoot;
                        }
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_NORMAL){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxNormal;
                        if(!bXbox){
                            node.Mesh.Vertices[n].MDXData.vNormal.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.vNormal.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.vNormal.fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        }
                        else{
                            node.Mesh.Vertices[n].MDXData.vNormal = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 2, sMdxVertex, &nPosData2));
                        }
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_COLOR){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxColor;
                        node.Mesh.Vertices[n].MDXData.cColor.fR = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.cColor.fG = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.cColor.fB = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV1){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV1;
                        node.Mesh.Vertices[n].MDXData.vUV1.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vUV1.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV2){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV2;
                        node.Mesh.Vertices[n].MDXData.vUV2.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vUV2.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV3){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV3;
                        node.Mesh.Vertices[n].MDXData.vUV3.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vUV3.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV4){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV4;
                        node.Mesh.Vertices[n].MDXData.vUV4.fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.vUV4.fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent1;
                        for(int b = 0; b < 3; b++){
                            if(bXbox){
                                node.Mesh.Vertices[n].MDXData.vTangent1[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 2, sMdxVertex, &nPosData2));
                            }
                            else{
                                node.Mesh.Vertices[n].MDXData.vTangent1[b].fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent1[b].fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent1[b].fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            }
                        }
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent2;
                        for(int b = 0; b < 3; b++){
                            if(bXbox){
                                node.Mesh.Vertices[n].MDXData.vTangent2[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 2, sMdxVertex, &nPosData2));
                            }
                            else{
                                node.Mesh.Vertices[n].MDXData.vTangent2[b].fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent2[b].fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent2[b].fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            }
                        }
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent3;
                        for(int b = 0; b < 3; b++){
                            if(bXbox){
                                node.Mesh.Vertices[n].MDXData.vTangent3[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 2, sMdxVertex, &nPosData2));
                            }
                            else{
                                node.Mesh.Vertices[n].MDXData.vTangent3[b].fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent3[b].fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent3[b].fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            }
                        }
                    }
                    if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent4;
                        for(int b = 0; b < 3; b++){
                            if(bXbox){
                                node.Mesh.Vertices[n].MDXData.vTangent4[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 2, sMdxVertex, &nPosData2));
                            }
                            else{
                                node.Mesh.Vertices[n].MDXData.vTangent4[b].fX = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent4[b].fY = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                                node.Mesh.Vertices[n].MDXData.vTangent4[b].fZ = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            }
                        }
                    }
                    if(node.Head.nType & NODE_SKIN){
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Skin.nOffsetToMdxWeightValues;
                        node.Mesh.Vertices[n].MDXData.Weights.fWeightValue[0] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.Weights.fWeightValue[1] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.Weights.fWeightValue[2] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        node.Mesh.Vertices[n].MDXData.Weights.fWeightValue[3] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        nPosData2 = node.Mesh.nOffsetIntoMdx + n * node.Mesh.nMdxDataSize + node.Skin.nOffsetToMdxBoneIndices;
                        if(bXbox){
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[0] = Mdx->ReadNumber((unsigned short*) nullptr, 5, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[1] = Mdx->ReadNumber((unsigned short*) nullptr, 5, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[2] = Mdx->ReadNumber((unsigned short*) nullptr, 5, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[3] = Mdx->ReadNumber((unsigned short*) nullptr, 5, sMdxVertex, &nPosData2);
                        }
                        else{
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[0] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[1] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[2] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                            node.Mesh.Vertices[n].MDXData.Weights.nWeightIndex[3] = Mdx->ReadNumber<float>(nullptr, 2, sMdxVertex, &nPosData2);
                        }
                    }
                }
                catch(const std::exception & e){
                    std::cout << "Offset into MDX for " << GetNodeName(node) << " is " << node.Mesh.nOffsetIntoMdx << "\n";
                    throw mdlexception("Error reading MDX Data for '" + GetNodeName(node)  + "': " + e.what());
                }

                Mdx->MarkDataBorder(node.Mesh.nOffsetIntoMdx + (n + 1) * node.Mesh.nMdxDataSize - 1);

                }
            }

            /**
                Read the floats after each data block
                This is just an empty Mdx struct with the length of what came before it.
                But there is some logic to it. The first three numbers are floats, with values depending on node type:
                    // 1,000,000 for skins
                    //10,000,000 for meshes, danglymeshes
            **/
            //std::cout << "Read data, now let's read the dummy data" << std::endl;

            sMdx = GetNodeName(node) + " > ";
            std::string sMdxExtra = sMdx + "Extra Data";
            if(node.Mesh.nMdxDataSize > 0 && Mdx){
                node.Mesh.MDXData.nNameIndex = node.Head.nNameIndex;
              try{
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_VERTEX){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxVertex;
                    node.Mesh.MDXData.vVertex.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vVertex.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vVertex.fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_NORMAL){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxNormal;
                    if(!bXbox){
                        node.Mesh.MDXData.vNormal.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.vNormal.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.vNormal.fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    }
                    else{
                        node.Mesh.MDXData.vNormal = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 8, sMdxExtra, &nPosData2));
                    }
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_COLOR){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxColor;
                    node.Mesh.MDXData.cColor.fR = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.cColor.fG = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.cColor.fB = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV1){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV1;
                    node.Mesh.MDXData.vUV1.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vUV1.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV2){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV2;
                    node.Mesh.MDXData.vUV2.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vUV2.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV3){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV3;
                    node.Mesh.MDXData.vUV3.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vUV3.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_UV4){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxUV4;
                    node.Mesh.MDXData.vUV4.fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.vUV4.fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent1;
                    for(int b = 0; b < 3; b++){
                        if(bXbox){
                            node.Mesh.MDXData.vTangent1[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 8, sMdxExtra, &nPosData2));
                        }
                        else{
                            node.Mesh.MDXData.vTangent1[b].fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent1[b].fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent1[b].fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        }
                    }
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent2;
                    for(int b = 0; b < 3; b++){
                        if(bXbox){
                            node.Mesh.MDXData.vTangent2[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 8, sMdxExtra, &nPosData2));
                        }
                        else{
                            node.Mesh.MDXData.vTangent2[b].fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent2[b].fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent2[b].fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        }
                    }
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent3;
                    for(int b = 0; b < 3; b++){
                        if(bXbox){
                            node.Mesh.MDXData.vTangent3[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 8, sMdxExtra, &nPosData2));
                        }
                        else{
                            node.Mesh.MDXData.vTangent3[b].fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent3[b].fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent3[b].fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        }
                    }
                }
                if(node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4){
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Mesh.nOffsetToMdxTangent4;
                    for(int b = 0; b < 3; b++){
                        if(bXbox){
                            node.Mesh.MDXData.vTangent4[b] = DecompressVector(Mdx->ReadNumber<unsigned>(nullptr, 8, sMdxExtra, &nPosData2));
                        }
                        else{
                            node.Mesh.MDXData.vTangent4[b].fX = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent4[b].fY = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                            node.Mesh.MDXData.vTangent4[b].fZ = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        }
                    }
                }
                if(node.Head.nType & NODE_SKIN){
                    //if(node.Skin.nOffsetToMdxWeightValues != 32) std::cout << string_format("Warning! MDX Skin Data Pointer 1 in %s is not 32! I might be reading wrong!\n", FH->MH.Names[node.Head.nNameIndex].sName.c_str());
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Skin.nOffsetToMdxWeightValues;
                    node.Mesh.MDXData.Weights.fWeightValue[0] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.Weights.fWeightValue[1] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.Weights.fWeightValue[2] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    node.Mesh.MDXData.Weights.fWeightValue[3] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    //if(node.Skin.nOffsetToMdxBoneIndices != 48) std::cout << string_format("Warning! MDX Skin Data Pointer 2 in %s is not 48! I might be reading wrong!\n", FH->MH.Names[node.Head.nNameIndex].sName.c_str());
                    nPosData2 = node.Mesh.nOffsetIntoMdx + nMaxDataStructs * node.Mesh.nMdxDataSize + node.Skin.nOffsetToMdxBoneIndices;
                    if(bXbox){
                        node.Mesh.MDXData.Weights.nWeightIndex[0] = Mdx->ReadNumber((unsigned short*) nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[1] = Mdx->ReadNumber((unsigned short*) nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[2] = Mdx->ReadNumber((unsigned short*) nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[3] = Mdx->ReadNumber((unsigned short*) nullptr, 8, sMdxExtra, &nPosData2);
                    }
                    else{
                        node.Mesh.MDXData.Weights.nWeightIndex[0] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[1] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[2] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                        node.Mesh.MDXData.Weights.nWeightIndex[3] = Mdx->ReadNumber<float>(nullptr, 8, sMdxExtra, &nPosData2);
                    }
                }
                Mdx->MarkDataBorder(node.Mesh.nOffsetIntoMdx + (nMaxDataStructs + 1) * node.Mesh.nMdxDataSize - 1);
              }
              catch(mdlexception & e){
                /// If an exception is thrown, it is likely because we hit the end of the file.
                /// In such a case we may just continue
              }
            }
            //std::cout << "Read dummy data" << std::endl;
        }
    }

    if(node.Head.nType & NODE_AABB){
        try{
            std::string sAabb = sNode.substr(0, sNode.size() - 3) + " > Header > Aabb";
            ReadNumber(&node.Walkmesh.nOffsetToAabb, 6, sAabb);
            MarkDataBorder(nPosition - 1);

            nAabbCount = 0;
            sAabbNodePrefix = sNode;
            if(node.Walkmesh.nOffsetToAabb.Valid() && node.Walkmesh.nOffsetToAabb > 0){
                /// Set the offset on the root aabb, then start the recursive aabb reading.
                node.Walkmesh.RootAabb.nOffset = node.Walkmesh.nOffsetToAabb;
                std::vector<unsigned int> visitedAabbOffsets;
                ParseAabb(node.Walkmesh.RootAabb, visitedAabbOffsets);
            }
        }
        catch(const std::exception & e){
            throw mdlexception("In " + GetNodeName(node) + ", reading aabb: " + e.what());
        }
    }

    if(node.Head.nType & NODE_SABER){
        std::string sSaber = sNode.substr(0, sNode.size() - 3) + " > Header > Lightsaber";
        std::string sSaberData = sNode.substr(0, sNode.size() - 3);
        ReadNumber(&node.Saber.nOffsetToSaberVerts, 6, sSaber);
        ReadNumber(&node.Saber.nOffsetToSaberUVs, 6, sSaber);
        ReadNumber(&node.Saber.nOffsetToSaberNormals, 6, sSaber);
        ReadNumber(&node.Saber.nInvCount1, 4, sSaber);
        ReadNumber(&node.Saber.nInvCount2, 4, sSaber);
        MarkDataBorder(nPosition - 1);

        node.Saber.SaberData.resize(176);
        nPosData = RequiredMdlDataOffset(node.Saber.nOffsetToSaberVerts, "saber vertex array");
        unsigned int nPosData2 = RequiredMdlDataOffset(node.Saber.nOffsetToSaberUVs, "saber uv array");
        unsigned int nPosData3 = RequiredMdlDataOffset(node.Saber.nOffsetToSaberNormals, "saber normal array");
        for(int n = 0; n < 176; n++){
            node.Saber.SaberData[n].vVertex.fX = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(n), &nPosData);
            node.Saber.SaberData[n].vVertex.fY = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(n), &nPosData);
            node.Saber.SaberData[n].vVertex.fZ = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);

            node.Saber.SaberData[n].vNormal.fX = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(n), &nPosData3);
            node.Saber.SaberData[n].vNormal.fY = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(n), &nPosData3);
            node.Saber.SaberData[n].vNormal.fZ = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(n), &nPosData3);
            MarkDataBorder(nPosData3 - 1);

            node.Saber.SaberData[n].vUV1.fX = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > UVs > UV " + std::to_string(n), &nPosData2);
            node.Saber.SaberData[n].vUV1.fY = ReadNumber<float>(nullptr, 2, sSaberData + " > Data > Lightsaber > UVs > UV " + std::to_string(n), &nPosData2);
            MarkDataBorder(nPosData2 - 1);
        }
    }
}

// Performs binary BWM/WOK/PWK/DWK parsing.
void BWM::ProcessBWM(){
    if(sBuffer.empty()) return;

    const BufferState oldBufferState = CaptureBufferState();
    std::unique_ptr<BWMHeader> pOldBwm = std::move(Bwm);
    Bwm.reset(new BWMHeader);
    auto restoreBwmOnFailure = MakeScopeExit([&](){
        RestoreBufferState(oldBufferState);
        Bwm = std::move(pOldBwm);
    });

    std::string sName = GetName() + (GetName() == "DWK" ? " " + std::to_string(((DWK*) this)->GetDwk()) : std::string());

    BWMHeader & Data = *Bwm;

    nPosition = 0;

    try{
        std::string sHeader ("Header > ");


        std::string sFileType, sVersion;

        /// Mark version info
        ReadString(&sFileType, 4, 3, sHeader + "Version");
        ReadString(&sVersion, 4, 3, sHeader + "Version");

        ReadNumber(&Data.nType, 4, sHeader + "Walkmesh Type");
        Data.vUse1.fX = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vUse1.fY = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vUse1.fZ = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vUse2.fX = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vUse2.fY = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vUse2.fZ = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk1.fX = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk1.fY = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk1.fZ = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk2.fX = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk2.fY = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vDwk2.fZ = ReadNumber<float>(nullptr, 2, sHeader + "Use Hooks");
        Data.vPosition.fX = ReadNumber<float>(nullptr, 2, sHeader + "Position");
        Data.vPosition.fY = ReadNumber<float>(nullptr, 2, sHeader + "Position");
        Data.vPosition.fZ = ReadNumber<float>(nullptr, 2, sHeader + "Position");

        ReadNumber(&Data.nNumberOfVerts, 1, sHeader + "Number of Vertices"); /// This is not equal in the wok and mdl
        ReadNumber(&Data.nOffsetToVerts, 6, sHeader + "Offset to Vertex Coordinates");
        ReadNumber(&Data.nNumberOfFaces, 1, sHeader + "Number of Faces"); /// In my test model this equals the number in the mdl
        ReadNumber(&Data.nOffsetToIndices, 6, sHeader + "Offset to Vertex Indices");
        ReadNumber(&Data.nOffsetToMaterials, 6, sHeader + "Offset to Material IDs");
        ReadNumber(&Data.nOffsetToNormals, 6, sHeader + "Offset to Face Normals");
        ReadNumber(&Data.nOffsetToDistances, 6, sHeader + "Offset to Plane Distances");
        ReadNumber(&Data.nNumberOfAabb, 1, sHeader + "Number of Aabb Structs"); /// In my test model this equals the number of aabb in the mdl
        ReadNumber(&Data.nOffsetToAabb, 6, sHeader + "Offset to Aabb Structs");
        ReadNumber(&Data.nPadding, 11, sHeader + "Unknown");
        ReadNumber(&Data.nNumberOfAdjacentFaces, 1, sHeader + "Number of Adjacent Edges");
        ReadNumber(&Data.nOffsetToAdjacentFaces, 6, sHeader + "Offset to Adjacent Edges");
        ReadNumber(&Data.nNumberOfEdges, 1, sHeader + "Number of Outer Edges");
        ReadNumber(&Data.nOffsetToEdges, 6, sHeader + "Offset to Outer Edges");
        ReadNumber(&Data.nNumberOfPerimeters, 1, sHeader + "Number of Perimeters");
        ReadNumber(&Data.nOffsetToPerimeters, 6, sHeader + "Offset to Perimeters");
        MarkDataBorder(nPosition - 1);
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM header: ") + e.what());
    }

    unsigned nPosData = 0;

    try{
        nPosData = BwmArrayOffset(Data.nNumberOfVerts, Data.nOffsetToVerts, 12u, sBuffer.size(), "BWM vertex array");
        ResizeBinaryVector(Data.verts, Data.nNumberOfVerts, "BWM vertex array");
        for(unsigned int n = 0; n < Data.nNumberOfVerts; n++){

            //Collect verts
            Data.verts.at(n).fX = ReadNumber<float>(nullptr, 2, "Vertices > Vertex " + std::to_string(n), &nPosData);
            Data.verts.at(n).fY = ReadNumber<float>(nullptr, 2, "Vertices > Vertex " + std::to_string(n), &nPosData);
            Data.verts.at(n).fZ = ReadNumber<float>(nullptr, 2, "Vertices > Vertex " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM vertex data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfFaces, Data.nOffsetToIndices, 12u, sBuffer.size(), "BWM face index array");
        ResizeBinaryVector(Data.faces, Data.nNumberOfFaces, "BWM face array");
        for(unsigned int n = 0; n < Data.nNumberOfFaces; n++){

            //Collect indices
            for(int i = 0; i < 3; i++){
                const std::string sIndexDesc = "Vertex Indices > Face " + std::to_string(n) + " > Index " + std::to_string(i);
                Data.faces.at(n).nIndexVertex[i] = SignedIntToUShortOrInvalid(ReadNumber<signed int>(nullptr, 4, sIndexDesc, &nPosData), sIndexDesc);
            }
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM face data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfFaces, Data.nOffsetToMaterials, 4u, sBuffer.size(), "BWM material array");
        for(unsigned int n = 0; n < Data.nNumberOfFaces; n++){

            //Collect materials
            ReadNumber(&Data.faces.at(n).nMaterialID, 4, "Materials > Material " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM surface ID data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfFaces, Data.nOffsetToNormals, 12u, sBuffer.size(), "BWM normal array");
        for(unsigned int n = 0; n < Data.nNumberOfFaces; n++){

            //Collect normals
            Data.faces.at(n).vNormal.fX = ReadNumber<float>(nullptr, 2, "Face Normals > Normal " + std::to_string(n), &nPosData);
            Data.faces.at(n).vNormal.fY = ReadNumber<float>(nullptr, 2, "Face Normals > Normal " + std::to_string(n), &nPosData);
            Data.faces.at(n).vNormal.fZ = ReadNumber<float>(nullptr, 2, "Face Normals > Normal " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM face normal data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfFaces, Data.nOffsetToDistances, 4u, sBuffer.size(), "BWM plane-distance array");
        for(unsigned int n = 0; n < Data.nNumberOfFaces; n++){

            //Collect distances
            Data.faces.at(n).fDistance = ReadNumber<float>(nullptr, 2, "Plane Distances > Distance " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM plane distance data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfAabb, Data.nOffsetToAabb, 44u, sBuffer.size(), "BWM AABB array");
        ResizeBinaryVector(Data.aabb, Data.nNumberOfAabb, "BWM AABB array");
        for(unsigned int n = 0; n < Data.nNumberOfAabb; n++){

            //Collect aabb
            Data.aabb.at(n).vBBmin.fX = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            Data.aabb.at(n).vBBmin.fY = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            Data.aabb.at(n).vBBmin.fZ = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            Data.aabb.at(n).vBBmax.fX = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            Data.aabb.at(n).vBBmax.fY = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            Data.aabb.at(n).vBBmax.fZ = ReadNumber<float>(nullptr, 2, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            const std::string sAabbDesc = "Aabb Tree > Aabb Struct " + std::to_string(n);
            Data.aabb.at(n).nID = SignedIntToUShortOrInvalid(ReadNumber<signed int>(nullptr, 4, sAabbDesc, &nPosData), sAabbDesc + " face id");
            ReadNumber(&Data.aabb.at(n).nExtra, 4, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            ReadNumber(&Data.aabb.at(n).nProperty, 4, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            ReadNumber(&Data.aabb.at(n).nChild1, 4, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            ReadNumber(&Data.aabb.at(n).nChild2, 4, "Aabb Tree > Aabb Struct " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM aabb data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfAdjacentFaces, Data.nOffsetToAdjacentFaces, 12u, sBuffer.size(), "BWM adjacent-face array");
        for(unsigned int n = 0; n < Data.nNumberOfAdjacentFaces; n++){
            if(static_cast<std::size_t>(n) < Data.faces.size()){

                for(int i = 0; i < 3; i++){
                    const std::string sAdjacentDesc = "Adjacent Faces > Face " + std::to_string(n) + " > Edge " + std::to_string(i);
                    Data.faces.at(n).nAdjacentFaces[i] = SignedIntToUShortOrInvalid(ReadNumber<signed int>(nullptr, 4, sAdjacentDesc, &nPosData), sAdjacentDesc);
                }
                MarkDataBorder(nPosData - 1);
            }
            else throw mdlexception("BWM adjacent-face count exceeds the face count; refusing to ignore extra adjacency rows.");
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM adjacent edge data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfEdges, Data.nOffsetToEdges, 8u, sBuffer.size(), "BWM edge array");
        ResizeBinaryVector(Data.edges, Data.nNumberOfEdges, "BWM edge array");
        for(unsigned int n = 0; n < Data.nNumberOfEdges; n++){

            ReadNumber(&Data.edges.at(n).nIndex, 4, "Edges > Edge " + std::to_string(n), &nPosData);
            ReadNumber(&Data.edges.at(n).nTransition, 4, "Edges > Edge " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM edge data: ") + e.what());
    }
    try{
        nPosData = BwmArrayOffset(Data.nNumberOfPerimeters, Data.nOffsetToPerimeters, 4u, sBuffer.size(), "BWM perimeter array");
        ResizeBinaryVector(Data.perimeters, Data.nNumberOfPerimeters, "BWM perimeter array");
        for(unsigned int n = 0; n < Data.nNumberOfPerimeters; n++){

            ReadNumber(&Data.perimeters.at(n).nPerimeter, 4, "Perimeters > Perimeter " + std::to_string(n), &nPosData);
            MarkDataBorder(nPosData - 1);
        }
    }
    catch(const std::exception & e){
        throw mdlexception(std::string("Reading BWM perimeter data: ") + e.what());
    }

    restoreBwmOnFailure.Dismiss();
}

