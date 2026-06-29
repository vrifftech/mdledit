#include "MDL.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>
#include <utility>

/**
    Functions:
    MDL::Compile()
    MDL::WriteAabb()
    MDL::WriteNodes()
*/

unsigned nMdxPrevPadding = 0;

namespace {
    unsigned char placeholder[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    std::string sAabbNodePrefix;
    std::string sWriteNodePrefix;
    std::vector<const Node*> vWrittenNodes;
    std::vector<std::pair<const Node*, unsigned>> vMdxOffsetPlaceholders;
    std::vector<std::pair<const Node*, unsigned int>> vNodeWriteOffsets;
    std::vector<unsigned int> vAnimationWriteOffsets;
    unsigned nHeadRootPlaceholder = 0;
    bool bHeadRootPlaceholderPending = false;

    class BinaryFileRollbackGuard{
        struct Entry{
            BinaryFile * file = nullptr;
            BinaryFile::BufferState state;
        };

        std::vector<Entry> entries;
        bool bActive = true;

      public:
        void Track(BinaryFile * file){
            if(file == nullptr) return;
            Entry entry;
            entry.file = file;
            entry.state = file->CaptureBufferState();
            entries.push_back(std::move(entry));
        }

        void Commit(){
            bActive = false;
        }

        void Rollback(){
            if(!bActive) return;
            for(auto it = entries.rbegin(); it != entries.rend(); ++it){
                if(it->file != nullptr) it->file->RestoreBufferState(it->state);
            }
            bActive = false;
        }

        ~BinaryFileRollbackGuard(){
            Rollback();
        }
    };

    class CompileOutputRollbackGuard{
        MDL & mdl;
        BinaryFileRollbackGuard buffers;
        bool bHadMdx = false;
        bool bActive = true;

      public:
        explicit CompileOutputRollbackGuard(MDL & mdl_) : mdl(mdl_), bHadMdx(static_cast<bool>(mdl_.Mdx)){
            buffers.Track(&mdl);
            if(mdl.Mdx) buffers.Track(mdl.Mdx.get());
            if(mdl.Wok) buffers.Track(mdl.Wok.get());
            if(mdl.Pwk) buffers.Track(mdl.Pwk.get());
            if(mdl.Dwk0) buffers.Track(mdl.Dwk0.get());
            if(mdl.Dwk1) buffers.Track(mdl.Dwk1.get());
            if(mdl.Dwk2) buffers.Track(mdl.Dwk2.get());
        }

        void Commit(){
            bActive = false;
            buffers.Commit();
        }

        ~CompileOutputRollbackGuard(){
            if(!bActive) return;
            // Restore tracked output buffers before discarding any writer-created
            // companion objects.
            buffers.Rollback();
            if(!bHadMdx) mdl.Mdx.reset();
        }
    };

    void RememberMdxOffsetPlaceholder(const Node & node, unsigned nPlaceholder){
        for(auto & entry : vMdxOffsetPlaceholders){
            if(entry.first == &node){
                entry.second = nPlaceholder;
                return;
            }
        }
        vMdxOffsetPlaceholders.push_back(std::make_pair(&node, nPlaceholder));
    }

    bool HasMdxOffsetPlaceholder(const Node & node){
        for(const auto & entry : vMdxOffsetPlaceholders){
            if(entry.first == &node) return true;
        }
        return false;
    }

    unsigned GetMdxOffsetPlaceholder(const Node & node){
        for(const auto & entry : vMdxOffsetPlaceholders){
            if(entry.first == &node) return entry.second;
        }
        throw mdlexception("Internal writer error: requested MDX offset placeholder before the mesh header was written.");
    }


    void RememberNodeWriteOffset(const Node & node, unsigned int nOffset){
        for(auto & entry : vNodeWriteOffsets){
            if(entry.first == &node){
                entry.second = nOffset;
                return;
            }
        }
        vNodeWriteOffsets.push_back(std::make_pair(&node, nOffset));
    }

    unsigned int GetNodeWriteOffset(const Node & node){
        for(const auto & entry : vNodeWriteOffsets){
            if(entry.first == &node) return entry.second;
        }
        throw mdlexception("Internal writer error: requested node offset before the node was written.");
    }

    void RememberAnimationWriteOffset(std::size_t nIndex, unsigned int nOffset){
        if(nIndex >= vAnimationWriteOffsets.size()){
            throw mdlexception("Internal writer error: animation offset index is out of range.");
        }
        vAnimationWriteOffsets[nIndex] = nOffset;
    }

    unsigned int GetAnimationWriteOffset(unsigned int nIndex){
        if(nIndex >= vAnimationWriteOffsets.size()){
            throw mdlexception("Internal writer error: animation root offset index is out of range.");
        }
        return vAnimationWriteOffsets[nIndex];
    }

    bool IsEmptyNodeSlot(const Node & node){
        return node.Head.nType == 0 && !node.Head.nNameIndex.Valid();
    }

    std::size_t CountNonEmptyNodes(const std::vector<Node> & nodes){
        std::size_t nCount = 0;
        for(const Node & node : nodes){
            if(!IsEmptyNodeSlot(node)) ++nCount;
        }
        return nCount;
    }

    void ValidateUniqueNodeNameIndices(const std::vector<Node> & nodes, std::size_t nNameCount, const std::string & sContext){
        std::vector<const Node*> seen(nNameCount, nullptr);
        for(const Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(node.Head.nType == 0 || !node.Head.nNameIndex.Valid()){
                throw mdlexception(sContext + " contains a non-empty node slot with no node type or an invalid name index; refusing to write ambiguous hierarchy data.");
            }
            const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
            if(nNameIndex >= nNameCount){
                throw mdlexception(sContext + " contains a node name index outside the model name table.");
            }
            if(seen.at(nNameIndex) != nullptr){
                throw mdlexception(sContext + " contains duplicate node name indices; refusing to choose one and possibly move child data to the wrong node.");
            }
            seen.at(nNameIndex) = &node;
        }
    }

    Node & FindNodeByNameIndex(std::vector<Node> & nodes, unsigned short nNameIndex, const std::string & sContext){
        for(Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(node.Head.nNameIndex.Valid() && static_cast<unsigned short>(node.Head.nNameIndex) == nNameIndex){
                return node;
            }
        }
        throw mdlexception("Cannot write " + sContext + ": root/tree-order node is not present in the node array.");
    }

    Node & FindGeometryRootNodeForWrite(ModelHeader & mh){
        if(!mh.NameIndicesInTreeOrder.empty()){
            const unsigned short nRootNameIndex = mh.NameIndicesInTreeOrder.front();
            if(nRootNameIndex >= mh.Names.size()){
                throw mdlexception("Cannot write geometry: preserved tree-order root is outside the model name table.");
            }
            return FindNodeByNameIndex(mh.ArrayOfNodes, nRootNameIndex, "geometry");
        }

        Node * pRoot = nullptr;
        for(Node & node : mh.ArrayOfNodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(!node.Head.nParentIndex.Valid()){
                if(pRoot != nullptr){
                    throw mdlexception("Cannot write geometry: multiple root nodes were found and no preserved tree order is available; refusing to choose one and omit the rest.");
                }
                pRoot = &node;
            }
        }
        if(pRoot == nullptr){
            throw mdlexception("Cannot write geometry: no root node is available.");
        }
        return *pRoot;
    }

    Node & FindAnimationRootNodeForWrite(std::vector<Node> & nodes, const std::string & sContext){
        Node * pRoot = nullptr;
        for(Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(!node.Head.nParentIndex.Valid()){
                if(pRoot != nullptr){
                    throw mdlexception("Cannot write " + sContext + ": multiple root nodes were found; refusing to choose one and omit the rest.");
                }
                pRoot = &node;
            }
        }
        if(pRoot != nullptr) return *pRoot;
        if(nodes.empty()){
            throw mdlexception("Cannot write " + sContext + ": animation contains no nodes.");
        }
        if(IsEmptyNodeSlot(nodes.front())){
            throw mdlexception("Cannot write " + sContext + ": the first animation node slot is empty and no root node was found.");
        }
        // Some animation arrays preserve a root-first ordering even when the parent
        // metadata is inherited from/supermodel-linked. Prefer preserving that
        // ordering over silently dropping the animation, then verify reachability.
        return nodes.front();
    }

    void EnsureAllNodesWritten(const std::vector<Node> & nodes, const std::string & sContext){
        for(const Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(std::find(vWrittenNodes.begin(), vWrittenNodes.end(), &node) == vWrittenNodes.end()){
                throw mdlexception(sContext + " contains a node that is not reachable from the selected root; refusing to silently drop it from the binary output.");
            }
        }
    }

    void ValidateHierarchyForWrite(const Node & root, const std::vector<Node> & nodes, std::size_t nNameCount, const std::string & sContext){
        std::vector<const Node*> byName(nNameCount, nullptr);
        for(const Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(!node.Head.nNameIndex.Valid()){
                throw mdlexception(sContext + " contains a node with an invalid name index.");
            }
            const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
            if(nNameIndex >= nNameCount){
                throw mdlexception(sContext + " contains a node name index outside the model name table.");
            }
            if(byName.at(nNameIndex) != nullptr){
                throw mdlexception(sContext + " contains duplicate node name indices in the hierarchy map.");
            }
            byName.at(nNameIndex) = &node;
        }

        std::vector<unsigned short> active;
        std::vector<unsigned short> visited;

        std::function<void(const Node&)> Visit = [&](const Node & node){
            if(!node.Head.nNameIndex.Valid()){
                throw mdlexception(sContext + " hierarchy contains a node with an invalid name index.");
            }
            const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
            if(nNameIndex >= nNameCount || byName.at(nNameIndex) != &node){
                throw mdlexception(sContext + " hierarchy root/child does not resolve to the node array by name index.");
            }
            if(std::find(active.begin(), active.end(), nNameIndex) != active.end()){
                throw mdlexception(sContext + " hierarchy contains a child cycle; refusing to write cyclic node data.");
            }
            if(std::find(visited.begin(), visited.end(), nNameIndex) != visited.end()){
                throw mdlexception(sContext + " hierarchy reaches the same node more than once; refusing to write ambiguous shared child data.");
            }

            active.push_back(nNameIndex);
            visited.push_back(nNameIndex);

            std::vector<unsigned short> localChildren;
            localChildren.reserve(node.Head.ChildIndices.size());
            for(const auto & childIndex : node.Head.ChildIndices){
                if(!childIndex.Valid()){
                    throw mdlexception(sContext + " hierarchy contains an invalid child name index.");
                }
                const unsigned short nChildIndex = static_cast<unsigned short>(childIndex);
                if(nChildIndex >= nNameCount){
                    throw mdlexception(sContext + " hierarchy contains a child name index outside the model name table.");
                }
                if(nChildIndex == nNameIndex){
                    throw mdlexception(sContext + " hierarchy contains a node listed as its own child.");
                }
                if(std::find(localChildren.begin(), localChildren.end(), nChildIndex) != localChildren.end()){
                    throw mdlexception(sContext + " hierarchy contains the same child more than once under one parent.");
                }
                const Node * child = byName.at(nChildIndex);
                if(child == nullptr){
                    throw mdlexception(sContext + " hierarchy child name index does not resolve to a node.");
                }
                if(!child->Head.nParentIndex.Valid()){
                    throw mdlexception(sContext + " hierarchy lists a root node as a child; refusing to write inconsistent parent/child pointers.");
                }
                if(static_cast<unsigned short>(child->Head.nParentIndex) != nNameIndex){
                    throw mdlexception(sContext + " hierarchy child parent index does not match the parent that lists it; refusing to write a node under one parent while its header points to another.");
                }
                localChildren.push_back(nChildIndex);
                Visit(*child);
            }

            active.pop_back();
        };

        Visit(root);
    }

    void ResetBinaryWriterState(){
        vWrittenNodes.clear();
        vMdxOffsetPlaceholders.clear();
        vNodeWriteOffsets.clear();
        vAnimationWriteOffsets.clear();
        nHeadRootPlaceholder = 0;
        bHeadRootPlaceholderPending = false;
    }

    template<typename T>
    struct ScopedValueRestore{
        T & ref;
        T oldValue;

        explicit ScopedValueRestore(T & value) : ref(value), oldValue(value) {}
        ~ScopedValueRestore(){ ref = oldValue; }

        ScopedValueRestore(const ScopedValueRestore &) = delete;
        ScopedValueRestore & operator=(const ScopedValueRestore &) = delete;
    };

    unsigned short SkinCompactSlotToFullBone(const Node & node, unsigned int nSlot){
        if(nSlot < 16) return node.Skin.nBoneIndices.at(nSlot);
        if(nSlot == 16) return node.Skin.nPadding1;
        throw mdlexception("Skin compact slot is outside the vanilla 0..16 range.");
    }

    unsigned short CheckedUShortCount(std::size_t nValue, const std::string & sWhat){
        if(nValue > std::numeric_limits<unsigned short>::max()){
            throw mdlexception(sWhat + " exceeds the 16-bit count limit and cannot be written without truncating data.");
        }
        return static_cast<unsigned short>(nValue);
    }

    unsigned int CheckedUIntCount(std::size_t nValue, const std::string & sWhat){
        if(nValue > std::numeric_limits<unsigned int>::max()){
            throw mdlexception(sWhat + " exceeds the 32-bit count limit and cannot be written without truncating data.");
        }
        return static_cast<unsigned int>(nValue);
    }

    unsigned int CheckedUIntTripleCount(std::size_t nValue, const std::string & sWhat){
        if(nValue > std::numeric_limits<unsigned int>::max() / 3u){
            throw mdlexception(sWhat + " exceeds the 32-bit count limit and cannot be written without truncating data.");
        }
        return static_cast<unsigned int>(nValue * 3u);
    }

    unsigned short MaxUsedNodeNameIndex(const std::vector<Node> & nodes, const std::string & sContext){
        bool bAnyNode = false;
        unsigned short nMaxNameIndex = 0;
        for(const Node & node : nodes){
            if(IsEmptyNodeSlot(node)) continue;
            if(!node.Head.nNameIndex.Valid()){
                throw mdlexception(sContext + " contains a non-empty node with an invalid name index.");
            }
            const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
            if(!bAnyNode || nNameIndex > nMaxNameIndex) nMaxNameIndex = nNameIndex;
            bAnyNode = true;
        }
        if(!bAnyNode){
            throw mdlexception(sContext + " contains no non-empty nodes.");
        }
        return nMaxNameIndex;
    }

    unsigned int AnimationNodeCountForWrite(const Animation & anim, const std::string & sContext, std::size_t nFallbackNameCount){
        const unsigned short nMaxUsedNameIndex = MaxUsedNodeNameIndex(anim.ArrayOfNodes, sContext);
        const std::size_t nMinimumCount = static_cast<std::size_t>(nMaxUsedNameIndex) + 1u;
        const std::size_t nLocalNodeCount = CountNonEmptyNodes(anim.ArrayOfNodes);
        const std::size_t nRequiredCount = std::max(nMinimumCount, nLocalNodeCount);
        const std::size_t nCandidateCount = anim.nNumberOfNames != 0 ? static_cast<std::size_t>(anim.nNumberOfNames) : nFallbackNameCount;
        if(nCandidateCount < nRequiredCount){
            throw mdlexception(sContext + " has a preserved node-count/header value smaller than the node name indices it actually uses; refusing to write a header that could truncate or move animation nodes.");
        }
        return CheckedUIntCount(nCandidateCount, sContext + " node count");
    }

    unsigned NextMdxPadding(unsigned nEndOffset){
        const unsigned nRemainder = nEndOffset % 16u;
        return nRemainder == 0u ? 0u : 16u - nRemainder;
    }


    struct MdxLayout{
        unsigned int nMdxDataSize = 0;
        unsigned short nNumberOfVerts = 0;
        MdlInteger<unsigned int> nOffsetToMdxVertex;
        MdlInteger<unsigned int> nOffsetToMdxNormal;
        MdlInteger<unsigned int> nOffsetToMdxColor;
        MdlInteger<unsigned int> nOffsetToMdxUV1;
        MdlInteger<unsigned int> nOffsetToMdxUV2;
        MdlInteger<unsigned int> nOffsetToMdxUV3;
        MdlInteger<unsigned int> nOffsetToMdxUV4;
        MdlInteger<unsigned int> nOffsetToMdxTangent1;
        MdlInteger<unsigned int> nOffsetToMdxTangent2;
        MdlInteger<unsigned int> nOffsetToMdxTangent3;
        MdlInteger<unsigned int> nOffsetToMdxTangent4;
        MdlInteger<unsigned int> nOffsetToMdxWeightValues;
        MdlInteger<unsigned int> nOffsetToMdxBoneIndices;
    };

    void AddMdxComponentOffset(bool bPresent, unsigned int nComponentSize, unsigned int & nMdxSize, MdlInteger<unsigned int> & nOffset){
        if(bPresent){
            nOffset = nMdxSize;
            if(nComponentSize > std::numeric_limits<unsigned int>::max() - nMdxSize){
                throw mdlexception("MDX vertex layout is too large to write safely.");
            }
            nMdxSize += nComponentSize;
        }
        else{
            nOffset = MdlInteger<unsigned int>();
        }
    }

    MdxLayout BuildMdxLayout(const Node & node, bool bXbox, const std::string & sNodeName){
        MdxLayout layout;
        layout.nNumberOfVerts = CheckedUShortCount(node.Mesh.Vertices.size(),
            "Node '" + sNodeName + "' vertex count");

        unsigned int nMdxSize = 0;
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_VERTEX) != 0, 12, nMdxSize, layout.nOffsetToMdxVertex);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_NORMAL) != 0, bXbox ? 4 : 12, nMdxSize, layout.nOffsetToMdxNormal);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_COLOR) != 0, 12, nMdxSize, layout.nOffsetToMdxColor);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_UV1) != 0, 8, nMdxSize, layout.nOffsetToMdxUV1);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_UV2) != 0, 8, nMdxSize, layout.nOffsetToMdxUV2);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_UV3) != 0, 8, nMdxSize, layout.nOffsetToMdxUV3);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_UV4) != 0, 8, nMdxSize, layout.nOffsetToMdxUV4);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1) != 0, bXbox ? 12 : 36, nMdxSize, layout.nOffsetToMdxTangent1);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT2) != 0, bXbox ? 12 : 36, nMdxSize, layout.nOffsetToMdxTangent2);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT3) != 0, bXbox ? 12 : 36, nMdxSize, layout.nOffsetToMdxTangent3);
        AddMdxComponentOffset((node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT4) != 0, bXbox ? 12 : 36, nMdxSize, layout.nOffsetToMdxTangent4);

        if(node.Head.nType & NODE_SKIN){
            layout.nOffsetToMdxWeightValues = nMdxSize;
            if(16u > std::numeric_limits<unsigned int>::max() - nMdxSize){
                throw mdlexception("MDX skin weight layout is too large to write safely.");
            }
            nMdxSize += 16u;
            layout.nOffsetToMdxBoneIndices = nMdxSize;
            const unsigned int nBoneIndexBytes = bXbox ? 8u : 16u;
            if(nBoneIndexBytes > std::numeric_limits<unsigned int>::max() - nMdxSize){
                throw mdlexception("MDX skin bone-index layout is too large to write safely.");
            }
            nMdxSize += nBoneIndexBytes;
        }

        layout.nMdxDataSize = nMdxSize;
        if(node.Mesh.Vertices.empty()) layout.nMdxDataSize = INVALID_INT;
        if(node.Head.nType & NODE_SABER) layout.nMdxDataSize = 0;
        return layout;
    }


    void ValidateMeshFaceIndices(const Node & node, const std::string & sNodeName){
        const std::size_t nVertexCount = node.Mesh.Vertices.size();
        for(std::size_t f = 0; f < node.Mesh.Faces.size(); ++f){
            const Face & face = node.Mesh.Faces.at(f);
            for(int i = 0; i < 3; ++i){
                const MdlInteger<unsigned short> & nIndex = face.nIndexVertex.at(i);
                if(!nIndex.Valid()){
                    throw mdlexception("Face " + std::to_string(f) + " on node '" + sNodeName + "' has an invalid vertex index.");
                }
                if(static_cast<unsigned short>(nIndex) >= nVertexCount){
                    throw mdlexception("Face " + std::to_string(f) + " on node '" + sNodeName + "' references vertex " +
                                       std::to_string(static_cast<unsigned short>(nIndex)) + ", but the node only has " +
                                       std::to_string(nVertexCount) + " vertices.");
                }
            }
        }
    }

    void ValidateMeshDerivedIndexData(const Node & node, const std::string & sNodeName){
        ValidateMeshFaceIndices(node, sNodeName);

        const std::size_t nVertexCount = node.Mesh.Vertices.size();
        if((node.Head.nType & NODE_MESH) && !(node.Head.nType & NODE_SABER) && !node.Mesh.Faces.empty()){
            if(node.Mesh.VertIndices.size() != node.Mesh.Faces.size()){
                throw mdlexception("Node '" + sNodeName + "' has " + std::to_string(node.Mesh.Faces.size()) +
                                   " faces but " + std::to_string(node.Mesh.VertIndices.size()) +
                                   " inverted-index rows; refusing to write malformed mesh index data.");
            }
            for(std::size_t f = 0; f < node.Mesh.VertIndices.size(); ++f){
                for(int i = 0; i < 3; ++i){
                    if(node.Mesh.VertIndices.at(f).at(i) >= nVertexCount){
                        throw mdlexception("Node '" + sNodeName + "' inverted-index row " + std::to_string(f) +
                                           " references vertex " + std::to_string(node.Mesh.VertIndices.at(f).at(i)) +
                                           ", which is outside the vertex array.");
                    }
                }
            }
        }
    }

    bool HasPreservedArrayHeader(const ArrayHead & array){
        return array.nOffset != 0 || array.nCount != 0 || array.nCount2 != 0;
    }

    void ValidateSkinArrayHeaderParity(const Node & node, const std::string & sNodeName){
        if(!(node.Head.nType & NODE_SKIN)) return;
        const unsigned int nBoneCount = CheckedUIntCount(node.Skin.Bones.size(),
            "Node '" + sNodeName + "' skin bone count");

        auto CheckArray = [&](const ArrayHead & array, const std::string & sArrayName){
            if(!HasPreservedArrayHeader(array)) return;
            if(array.nCount != nBoneCount || array.nCount2 != nBoneCount){
                throw mdlexception("Node '" + sNodeName + "' has a preserved " + sArrayName +
                                   " count that does not match the skin bone count; refusing to normalize and erase mismatched binary header data.");
            }
        };

        CheckArray(node.Skin.QBoneArray, "QBoneArray");
        CheckArray(node.Skin.TBoneArray, "TBoneArray");
        CheckArray(node.Skin.Array8Array, "boneconstantindices/Array8Array");
    }


    void ValidateModelTotalNodeCountForWrite(const ModelHeader & mh){
        const std::size_t nLocalNodeCount = CountNonEmptyNodes(mh.ArrayOfNodes);
        if(nLocalNodeCount > std::numeric_limits<unsigned int>::max()){
            throw mdlexception("Geometry node count exceeds the 32-bit header range.");
        }
        if(mh.GH.nTotalNumberOfNodes < nLocalNodeCount){
            throw mdlexception("Geometry totalnodes value is smaller than the number of local geometry nodes; refusing to write a header that can truncate or misrepresent the model hierarchy.");
        }
    }

    void ValidateBwmForWrite(const BWMHeader & data){
        const std::size_t nVertexCount = data.verts.size();
        const std::size_t nFaceCount = data.faces.size();
        if(nFaceCount > std::numeric_limits<unsigned int>::max() / 3u){
            throw mdlexception("Walkmesh face count is too large to validate adjacency indices safely.");
        }
        const unsigned int nMaxEdgeReference = static_cast<unsigned int>(nFaceCount * 3u);

        for(std::size_t f = 0; f < nFaceCount; ++f){
            const Face & face = data.faces.at(f);
            for(int i = 0; i < 3; ++i){
                const MdlInteger<unsigned short> & nIndex = face.nIndexVertex.at(i);
                if(!nIndex.Valid()){
                    throw mdlexception("Walkmesh face " + std::to_string(f) + " has an invalid vertex index; refusing to write malformed walkmesh data.");
                }
                if(static_cast<unsigned short>(nIndex) >= nVertexCount){
                    throw mdlexception("Walkmesh face " + std::to_string(f) + " references vertex " +
                                       std::to_string(static_cast<unsigned short>(nIndex)) +
                                       ", but the walkmesh only has " + std::to_string(nVertexCount) + " vertices.");
                }
                const MdlInteger<unsigned short> & nAdjacent = face.nAdjacentFaces.at(i);
                if(nAdjacent.Valid() && static_cast<unsigned short>(nAdjacent) >= nMaxEdgeReference){
                    throw mdlexception("Walkmesh face " + std::to_string(f) + " adjacent edge reference " +
                                       std::to_string(static_cast<unsigned short>(nAdjacent)) +
                                       " is outside the walkmesh edge-reference range.");
                }
            }
        }

        for(std::size_t a = 0; a < data.aabb.size(); ++a){
            const Aabb & aabb = data.aabb.at(a);
            auto CheckChild = [&](const MdlInteger<unsigned int> & nChild, const char * sChild){
                if(!nChild.Valid()) return;
                const unsigned int nChildIndex = static_cast<unsigned int>(nChild);
                if(nChildIndex >= data.aabb.size()){
                    throw mdlexception(std::string("Walkmesh AABB ") + std::to_string(a) + " " + sChild +
                                       " references node " + std::to_string(nChildIndex) +
                                       ", but only " + std::to_string(data.aabb.size()) + " AABB nodes exist.");
                }
                if(nChildIndex == a){
                    throw mdlexception(std::string("Walkmesh AABB ") + std::to_string(a) + " " + sChild +
                                       " references itself; refusing to write cyclic AABB data.");
                }
            };
            CheckChild(aabb.nChild1, "child1");
            CheckChild(aabb.nChild2, "child2");

            if(aabb.nID.Valid() && static_cast<unsigned short>(aabb.nID) >= nFaceCount){
                throw mdlexception("Walkmesh AABB " + std::to_string(a) + " references face " +
                                   std::to_string(static_cast<unsigned short>(aabb.nID)) +
                                   ", but the walkmesh only has " + std::to_string(nFaceCount) + " faces.");
            }
        }

        for(std::size_t e = 0; e < data.edges.size(); ++e){
            const Edge & edge = data.edges.at(e);
            if(edge.nIndex.Valid() && static_cast<unsigned int>(edge.nIndex) >= nMaxEdgeReference){
                throw mdlexception("Walkmesh outer edge " + std::to_string(e) + " references a face edge outside the flattened face-edge array.");
            }
        }

        for(std::size_t p = 0; p < data.perimeters.size(); ++p){
            if(data.perimeters.at(p).nPerimeter > data.edges.size()){
                throw mdlexception("Walkmesh perimeter " + std::to_string(p) + " references more edges than exist in the edge array.");
            }
        }
    }


    struct NodeWriteStateGuard{
        Node & node;
        unsigned int nOffset;
        MdlInteger<unsigned int> nHeadOffsetToParent;
        ArrayHead ChildrenArray;
        ArrayHead ControllerArray;
        ArrayHead ControllerDataArray;

        ArrayHead FlareSizeArray;
        ArrayHead FlarePositionArray;
        ArrayHead FlareColorShiftArray;
        ArrayHead FlareTextureNameArray;
        std::vector<unsigned int> FlareTextureNameOffsets;

        ArrayHead FaceArray;
        ArrayHead IndexCounterArray;
        ArrayHead IndexLocationArray;
        ArrayHead MeshInvertedCounterArray;
        unsigned int nMdxDataSize;
        MdlInteger<unsigned int> nOffsetToMdxVertex;
        MdlInteger<unsigned int> nOffsetToMdxNormal;
        MdlInteger<unsigned int> nOffsetToMdxColor;
        MdlInteger<unsigned int> nOffsetToMdxUV1;
        MdlInteger<unsigned int> nOffsetToMdxUV2;
        MdlInteger<unsigned int> nOffsetToMdxUV3;
        MdlInteger<unsigned int> nOffsetToMdxUV4;
        MdlInteger<unsigned int> nOffsetToMdxTangent1;
        MdlInteger<unsigned int> nOffsetToMdxTangent2;
        MdlInteger<unsigned int> nOffsetToMdxTangent3;
        MdlInteger<unsigned int> nOffsetToMdxTangent4;
        unsigned short nNumberOfVerts;
        unsigned int nOffsetToVertArray;
        unsigned int nVertIndicesCount;
        unsigned int nVertIndicesLocation;
        MdlInteger<unsigned int> nMeshInvertedCounter;

        MdlInteger<unsigned int> nOffsetToMdxWeightValues;
        MdlInteger<unsigned int> nOffsetToMdxBoneIndices;
        unsigned int nOffsetToBonemap;
        unsigned int nNumberOfBonemap;
        ArrayHead QBoneArray;
        ArrayHead TBoneArray;
        ArrayHead Array8Array;

        ArrayHead ConstraintArray;
        unsigned int nOffsetToData2;

        MdlInteger<unsigned int> nOffsetToAabb;

        unsigned int nOffsetToSaberVerts;
        unsigned int nOffsetToSaberUVs;
        unsigned int nOffsetToSaberNormals;

        explicit NodeWriteStateGuard(Node & node_) :
            node(node_),
            nOffset(node_.nOffset),
            nHeadOffsetToParent(node_.Head.nOffsetToParent),
            ChildrenArray(node_.Head.ChildrenArray),
            ControllerArray(node_.Head.ControllerArray),
            ControllerDataArray(node_.Head.ControllerDataArray),
            FlareSizeArray(node_.Light.FlareSizeArray),
            FlarePositionArray(node_.Light.FlarePositionArray),
            FlareColorShiftArray(node_.Light.FlareColorShiftArray),
            FlareTextureNameArray(node_.Light.FlareTextureNameArray),
            FaceArray(node_.Mesh.FaceArray),
            IndexCounterArray(node_.Mesh.IndexCounterArray),
            IndexLocationArray(node_.Mesh.IndexLocationArray),
            MeshInvertedCounterArray(node_.Mesh.MeshInvertedCounterArray),
            nMdxDataSize(node_.Mesh.nMdxDataSize),
            nOffsetToMdxVertex(node_.Mesh.nOffsetToMdxVertex),
            nOffsetToMdxNormal(node_.Mesh.nOffsetToMdxNormal),
            nOffsetToMdxColor(node_.Mesh.nOffsetToMdxColor),
            nOffsetToMdxUV1(node_.Mesh.nOffsetToMdxUV1),
            nOffsetToMdxUV2(node_.Mesh.nOffsetToMdxUV2),
            nOffsetToMdxUV3(node_.Mesh.nOffsetToMdxUV3),
            nOffsetToMdxUV4(node_.Mesh.nOffsetToMdxUV4),
            nOffsetToMdxTangent1(node_.Mesh.nOffsetToMdxTangent1),
            nOffsetToMdxTangent2(node_.Mesh.nOffsetToMdxTangent2),
            nOffsetToMdxTangent3(node_.Mesh.nOffsetToMdxTangent3),
            nOffsetToMdxTangent4(node_.Mesh.nOffsetToMdxTangent4),
            nNumberOfVerts(node_.Mesh.nNumberOfVerts),
            nOffsetToVertArray(node_.Mesh.nOffsetToVertArray),
            nVertIndicesCount(node_.Mesh.nVertIndicesCount),
            nVertIndicesLocation(node_.Mesh.nVertIndicesLocation),
            nMeshInvertedCounter(node_.Mesh.nMeshInvertedCounter),
            nOffsetToMdxWeightValues(node_.Skin.nOffsetToMdxWeightValues),
            nOffsetToMdxBoneIndices(node_.Skin.nOffsetToMdxBoneIndices),
            nOffsetToBonemap(node_.Skin.nOffsetToBonemap),
            nNumberOfBonemap(node_.Skin.nNumberOfBonemap),
            QBoneArray(node_.Skin.QBoneArray),
            TBoneArray(node_.Skin.TBoneArray),
            Array8Array(node_.Skin.Array8Array),
            ConstraintArray(node_.Dangly.ConstraintArray),
            nOffsetToData2(node_.Dangly.nOffsetToData2),
            nOffsetToAabb(node_.Walkmesh.nOffsetToAabb),
            nOffsetToSaberVerts(node_.Saber.nOffsetToSaberVerts),
            nOffsetToSaberUVs(node_.Saber.nOffsetToSaberUVs),
            nOffsetToSaberNormals(node_.Saber.nOffsetToSaberNormals)
        {
            FlareTextureNameOffsets.reserve(node_.Light.FlareTextureNames.size());
            for(const auto & name : node_.Light.FlareTextureNames){
                FlareTextureNameOffsets.push_back(name.nOffset);
            }
        }

        ~NodeWriteStateGuard(){
            node.nOffset = nOffset;
            node.Head.nOffsetToParent = nHeadOffsetToParent;
            node.Head.ChildrenArray = ChildrenArray;
            node.Head.ControllerArray = ControllerArray;
            node.Head.ControllerDataArray = ControllerDataArray;

            node.Light.FlareSizeArray = FlareSizeArray;
            node.Light.FlarePositionArray = FlarePositionArray;
            node.Light.FlareColorShiftArray = FlareColorShiftArray;
            node.Light.FlareTextureNameArray = FlareTextureNameArray;
            for(std::size_t i = 0; i < FlareTextureNameOffsets.size() && i < node.Light.FlareTextureNames.size(); ++i){
                node.Light.FlareTextureNames[i].nOffset = FlareTextureNameOffsets[i];
            }

            node.Mesh.FaceArray = FaceArray;
            node.Mesh.IndexCounterArray = IndexCounterArray;
            node.Mesh.IndexLocationArray = IndexLocationArray;
            node.Mesh.MeshInvertedCounterArray = MeshInvertedCounterArray;
            node.Mesh.nMdxDataSize = nMdxDataSize;
            node.Mesh.nOffsetToMdxVertex = nOffsetToMdxVertex;
            node.Mesh.nOffsetToMdxNormal = nOffsetToMdxNormal;
            node.Mesh.nOffsetToMdxColor = nOffsetToMdxColor;
            node.Mesh.nOffsetToMdxUV1 = nOffsetToMdxUV1;
            node.Mesh.nOffsetToMdxUV2 = nOffsetToMdxUV2;
            node.Mesh.nOffsetToMdxUV3 = nOffsetToMdxUV3;
            node.Mesh.nOffsetToMdxUV4 = nOffsetToMdxUV4;
            node.Mesh.nOffsetToMdxTangent1 = nOffsetToMdxTangent1;
            node.Mesh.nOffsetToMdxTangent2 = nOffsetToMdxTangent2;
            node.Mesh.nOffsetToMdxTangent3 = nOffsetToMdxTangent3;
            node.Mesh.nOffsetToMdxTangent4 = nOffsetToMdxTangent4;
            node.Mesh.nNumberOfVerts = nNumberOfVerts;
            node.Mesh.nOffsetToVertArray = nOffsetToVertArray;
            node.Mesh.nVertIndicesCount = nVertIndicesCount;
            node.Mesh.nVertIndicesLocation = nVertIndicesLocation;
            node.Mesh.nMeshInvertedCounter = nMeshInvertedCounter;

            node.Skin.nOffsetToMdxWeightValues = nOffsetToMdxWeightValues;
            node.Skin.nOffsetToMdxBoneIndices = nOffsetToMdxBoneIndices;
            node.Skin.nOffsetToBonemap = nOffsetToBonemap;
            node.Skin.nNumberOfBonemap = nNumberOfBonemap;
            node.Skin.QBoneArray = QBoneArray;
            node.Skin.TBoneArray = TBoneArray;
            node.Skin.Array8Array = Array8Array;

            node.Dangly.ConstraintArray = ConstraintArray;
            node.Dangly.nOffsetToData2 = nOffsetToData2;

            node.Walkmesh.nOffsetToAabb = nOffsetToAabb;

            node.Saber.nOffsetToSaberVerts = nOffsetToSaberVerts;
            node.Saber.nOffsetToSaberUVs = nOffsetToSaberUVs;
            node.Saber.nOffsetToSaberNormals = nOffsetToSaberNormals;
        }
    };
}

bool MDL::Compile(){
    ReportObject ReportMdl(*this);
    Timer tCompile;
    CompileOutputRollbackGuard compileRollback(*this);
    nPosition = 0;
    sBuffer.resize(0);
    bKnown.resize(0);
    bDataLoaded = true;
    ResetBinaryWriterState();
    nMdxPrevPadding = 0;
    if(!Mdx) Mdx.reset(new MDX());
    else{
        Mdx->GetBuffer().clear();
        Mdx->GetKnownData().clear();
        Mdx->nPosition = 0;
    }

    FileHeader &Data = *FH;

    std::string sFileHeader = "File Header";


    /// File header
    WriteNumber(&Data.nZero, 8, sFileHeader + " > Padding");
    unsigned PHnMdlLength = WriteBytes(placeholder, 4, 1, sFileHeader + " > MDL File Size"); // to be filled later
    unsigned PHnMdxLength = WriteBytes(placeholder, 4, 1, sFileHeader + " > MDX File Size"); // to be filled later
    MarkDataBorder(nPosition - 1);

    /// Geo Header
    std::string sGeometryHeader = "Geometry Header";

    // Function pointers
    unsigned int nModelFunctionPointer0 = FunctionPointer1(FN_PTR_MODEL);
    unsigned int nModelFunctionPointer1 = FunctionPointer2(FN_PTR_MODEL);
    WriteNumber(&nModelFunctionPointer0, 9, sGeometryHeader + " > Function Pointers");
    WriteNumber(&nModelFunctionPointer1, 9, sGeometryHeader + " > Function Pointers");

    // Model name
    WriteString(&Data.MH.GH.sName, 32, 3, sGeometryHeader + " > Name");

    // Write placeholder for root node offset
    unsigned PHnOffsetToRootNode = WriteBytes(placeholder, 4, 6, sGeometryHeader + " > Offset to Root Node");

    // Total number of nodes
    ValidateModelTotalNodeCountForWrite(Data.MH);
    unsigned int nTotalNumberOfNodesToWrite = Data.MH.GH.nTotalNumberOfNodes;
    WriteNumber(&nTotalNumberOfNodesToWrite, 1, sGeometryHeader + " > Number of Nodes");

    // Empty runtime arrays
    WriteNumber(&Data.MH.GH.RuntimeArray1.nOffset, 8, sGeometryHeader + " > Runtime Arrays");
    WriteNumber(&Data.MH.GH.RuntimeArray1.nCount, 8, sGeometryHeader + " > Runtime Arrays");
    WriteNumber(&Data.MH.GH.RuntimeArray1.nCount2, 8, sGeometryHeader + " > Runtime Arrays");
    WriteNumber(&Data.MH.GH.RuntimeArray2.nOffset, 8, sGeometryHeader + " > Runtime Arrays");
    WriteNumber(&Data.MH.GH.RuntimeArray2.nCount, 8, sGeometryHeader + " > Runtime Arrays");
    WriteNumber(&Data.MH.GH.RuntimeArray2.nCount2, 8, sGeometryHeader + " > Runtime Arrays");

    // Reference count
    WriteNumber(&Data.MH.GH.nRefCount, 8, sGeometryHeader + " > Reference Count");

    // Model type
    WriteNumber(&Data.MH.GH.nModelType, 7, sGeometryHeader + " > Type");

    // Padding (3 bytes)
    WriteNumber(&Data.MH.GH.nPadding[0], 11, sGeometryHeader + " > Padding");
    WriteNumber(&Data.MH.GH.nPadding[1], 11, sGeometryHeader + " > Padding");
    WriteNumber(&Data.MH.GH.nPadding[2], 11, sGeometryHeader + " > Padding");

    // Mark the end of geo header
    MarkDataBorder(nPosition - 1);

    /// Model header
    std::string sModelHeader = "Model Header";

    // Classification
    WriteNumber(&Data.MH.nClassification, 7, sModelHeader + " > Classification");

    // "Sub-classification" (not understood)
    WriteNumber(&Data.MH.nSubclassification, 10, sModelHeader + " > Unknown1");

    // Empty byte (can be used for SG presence marking)
    unsigned char nModelUnknown = bWriteSmoothing ? 1 : Data.MH.nUnknown;
    WriteNumber(&nModelUnknown, 8, sModelHeader + " > Unknown1");

    // Affected By Fog
    WriteNumber(&Data.MH.nAffectedByFog, 7, sModelHeader + " > Affected By Fog");

    // Child model count
    WriteNumber(&Data.MH.nChildModelCount, 8, sModelHeader + " > Number of Child Models");

    // Animation ArrayHead
    unsigned int nAnimationCountToWrite = CheckedUIntCount(Data.MH.Animations.size(), "model animation count");
    unsigned PHnOffsetToAnimationArray = WriteBytes(placeholder, 4, 6, sModelHeader + " > Offset to Animation Array");     // Write placeholder offset
    WriteNumber(&nAnimationCountToWrite, 1, sModelHeader + " > Number of Animations");                         // Write count1
    WriteNumber(&nAnimationCountToWrite, 1, sModelHeader + " > Number of Animations");                        // Write count2

    // Supermodel Reference
    WriteNumber(&Data.MH.nSupermodelReference, 11, sModelHeader + " > Supermodel Reference");

    // BB min and max, radius, scale
    WriteFloat(&Data.MH.vBBmin.fX, 2, sModelHeader + " > Bounding Box Min");
    WriteFloat(&Data.MH.vBBmin.fY, 2, sModelHeader + " > Bounding Box Min");
    WriteFloat(&Data.MH.vBBmin.fZ, 2, sModelHeader + " > Bounding Box Min");
    WriteFloat(&Data.MH.vBBmax.fX, 2, sModelHeader + " > Bounding Box Max");
    WriteFloat(&Data.MH.vBBmax.fY, 2, sModelHeader + " > Bounding Box Max");
    WriteFloat(&Data.MH.vBBmax.fZ, 2, sModelHeader + " > Bounding Box Max");
    WriteFloat(&Data.MH.fRadius, 2, sModelHeader + " > Radius");
    WriteFloat(&Data.MH.fScale, 2, sModelHeader + " > Animation Scale");

    // Supermodel name
    WriteString(&Data.MH.cSupermodelName, 32, 3, sModelHeader + " > Supermodel Name");

    // Offset to head root node
    unsigned PHnOffsetToHeadRootNode = WriteBytes(placeholder, 4, 6, sModelHeader + " > Offset to Head Root");

    // Padding (4 bytes)
    WriteNumber(&Data.MH.nPadding, 8, sModelHeader + " > Padding");

    // Mdx size
    unsigned PHnMdxLength2 = WriteBytes(placeholder, 4, 1, sModelHeader + " > MDX File Size");

    // Mdx offset
    WriteNumber(&Data.MH.nOffsetIntoMdx, 8, sModelHeader + " > MDX Data Offset");

    // Name ArrayHead
    unsigned int nNameCountToWrite = CheckedUIntCount(Data.MH.Names.size(), "model name count");
    unsigned PHnOffsetToNameArray = WriteBytes(placeholder, 4, 6, sModelHeader + " > Offset to Name Array");
    WriteNumber(&nNameCountToWrite, 1, sModelHeader + " > Number of Names");
    WriteNumber(&nNameCountToWrite, 1, sModelHeader + " > Number of Names");

    // Mark the end of model header
    MarkDataBorder(nPosition - 1);

    /// Create Name array
    std::string sNameArrayPointers = "Name Array > Pointers > Pointer ";
    std::string sNameArrayStrings = "Name Array > Strings > \"";

    // Record the offset of the name array
    unsigned int nNameArrayOffsetToWrite = Data.MH.Names.empty() ? 0u : nPosition - 12;
    WriteNumber(&nNameArrayOffsetToWrite, 0, "", &PHnOffsetToNameArray);

    std::vector<unsigned> PHnOffsetToName;
    for(unsigned c = 0; c < Data.MH.Names.size(); c++){
        // Write placeholder
        PHnOffsetToName.push_back(WriteBytes(placeholder, 4, 6, sNameArrayPointers + std::to_string(c)));

        MarkDataBorder(nPosition - 1);
    }
    for(unsigned c = 0; c < Data.MH.Names.size(); c++){
        // Write offset to placeholder
        unsigned int nNameOffsetToWrite = nPosition - 12;
        WriteNumber(&nNameOffsetToWrite, 0, "", &PHnOffsetToName.at(c));

        // Write name
        WriteString(&Data.MH.Names[c].sName, 0, 3, sNameArrayStrings + std::string(Data.MH.Names[c].sName.c_str()) + "\"");

        MarkDataBorder(nPosition - 1);
    }

    /// Create Animation array

    vAnimationWriteOffsets.assign(Data.MH.Animations.size(), 0u);

    // Write offset to placeholder
    unsigned int nAnimationArrayOffsetToWrite = Data.MH.Animations.empty() ? 0u : nPosition - 12;
    WriteNumber(&nAnimationArrayOffsetToWrite, 0, "", &PHnOffsetToAnimationArray);

    std::vector<unsigned> pnOffsetsToAnimation;
    for(unsigned c = 0; c < Data.MH.Animations.size(); c++){
        std::string sAnimationPointer = "Animations > Pointers > Pointer" + std::to_string(c);
        // Write placeholder
        pnOffsetsToAnimation.push_back(WriteBytes(placeholder, 4, 6, sAnimationPointer));

        MarkDataBorder(nPosition - 1);
    }
    for(unsigned c = 0; c < Data.MH.Animations.size(); c++){
        /// This is where we fill EVERYTHING about the animation
        Animation & anim = Data.MH.Animations[c];
        if(anim.ArrayOfNodes.empty()){
            throw mdlexception("Cannot write animation '" + anim.sName + "': animation contains no nodes.");
        }
        ValidateUniqueNodeNameIndices(anim.ArrayOfNodes, Data.MH.Names.size(), "Animation '" + anim.sName + "'");
        ScopedValueRestore<ArrayHead> restoreEventArray(anim.EventArray);
        ScopedValueRestore<unsigned int> restoreAnimationOffset(anim.nOffset);
        ScopedValueRestore<unsigned int> restoreRootAnimationOffset(anim.nOffsetToRootAnimationNode);
        std::string sAnimation = "Animations > "  + std::string(anim.sName.c_str()) + " > ";
        std::string sAnimationGeometryHeader = sAnimation + "Geometry Header";

        // Write offset to placeholder
        unsigned int nAnimationOffsetToWrite = nPosition - 12;
        RememberAnimationWriteOffset(c, nAnimationOffsetToWrite);
        WriteNumber(&nAnimationOffsetToWrite, 0, "", &pnOffsetsToAnimation[c]);


        // Write function pointers. Preserve binary-derived values exposed by ASCII;
        // synthesize defaults for author-authored ASCII that did not specify them.
        unsigned int nAnimFunctionPointer0 = anim.nFunctionPointer0 != 0 ? anim.nFunctionPointer0 : FunctionPointer1(FN_PTR_ANIM);
        unsigned int nAnimFunctionPointer1 = anim.nFunctionPointer1 != 0 ? anim.nFunctionPointer1 : FunctionPointer2(FN_PTR_ANIM);
        WriteNumber(&nAnimFunctionPointer0, 9, sAnimationGeometryHeader + " > Function Pointers");
        WriteNumber(&nAnimFunctionPointer1, 9, sAnimationGeometryHeader + " > Function Pointers");

        // Animation name
        WriteString(&anim.sName, 32, 3, sAnimationGeometryHeader + " > Name");

        // Offset to root node
        unsigned PHnOffsetToFirstNode = WriteBytes(placeholder, 4, 6, sAnimationGeometryHeader + " > Offset to Root Node");

        // Number of nodes (total possible = number of names). Preserve explicit
        // ASCII value when supplied; default to the current name count otherwise,
        // but refuse to under-report the local animation node table.
        unsigned int nAnimationNumberOfNames = AnimationNodeCountForWrite(anim, "Animation '" + anim.sName + "'", Data.MH.Names.size());
        WriteNumber(&nAnimationNumberOfNames, 1, sAnimationGeometryHeader + " > Number of Nodes");

        // Empty runtime arrays
        WriteNumber(&anim.RuntimeArray1.nOffset, 8, sAnimationGeometryHeader + " > Runtime Arrays");
        WriteNumber(&anim.RuntimeArray1.nCount, 8, sAnimationGeometryHeader + " > Runtime Arrays");
        WriteNumber(&anim.RuntimeArray1.nCount2, 8, sAnimationGeometryHeader + " > Runtime Arrays");
        WriteNumber(&anim.RuntimeArray2.nOffset, 8, sAnimationGeometryHeader + " > Runtime Arrays");
        WriteNumber(&anim.RuntimeArray2.nCount, 8, sAnimationGeometryHeader + " > Runtime Arrays");
        WriteNumber(&anim.RuntimeArray2.nCount2, 8, sAnimationGeometryHeader + " > Runtime Arrays");

        // Reference count
        WriteNumber(&anim.nRefCount, 8, sAnimationGeometryHeader + " > Reference Count");

        // Model Type
        WriteNumber(&anim.nModelType, 7, sAnimationGeometryHeader + " > Type");

        // Padding (3 bytes)
        WriteNumber(&anim.nPadding[0], 11, sAnimationGeometryHeader + " > Padding");
        WriteNumber(&anim.nPadding[1], 11, sAnimationGeometryHeader + " > Padding");
        WriteNumber(&anim.nPadding[2], 11, sAnimationGeometryHeader + " > Padding");

        MarkDataBorder(nPosition - 1);

        // Animation length
        WriteFloat(&anim.fLength, 2, sAnimation + "Header");

        // Animation transition
        WriteFloat(&anim.fTransition, 2, sAnimation + "Header");

        // AnimRoot
        WriteString(&anim.sAnimRoot, 32, 3, sAnimation + "Header");

        // Event ArrayHead
        unsigned int nEventCountToWrite = CheckedUIntCount(anim.Events.size(),
            "Animation '" + anim.sName + "' event count");
        unsigned PHnOffsetToEventArray = WriteBytes(placeholder, 4, 6, sAnimation + "Header");
        WriteNumber(&nEventCountToWrite, 1, sAnimation + "Header");
        WriteNumber(&nEventCountToWrite, 1, sAnimation + "Header");

        // Padding (4 bytes)
        WriteNumber(&anim.nPadding2, 8, sAnimation + "Header");

        MarkDataBorder(nPosition - 1);

        if(anim.Events.size() > 0){
            // Enter the offset to placeholder
            unsigned int nEventArrayOffsetToWrite = nPosition - 12;
            WriteNumber(&nEventArrayOffsetToWrite, 0, "", &PHnOffsetToEventArray);
            for(unsigned b = 0; b < anim.Events.size(); b++){
                WriteFloat(&anim.Events[b].fTime, 2, sAnimation + "Header");
                WriteString(&anim.Events[b].sName, 32, 3, sAnimation + "Header");
                MarkDataBorder(nPosition - 1);
            }
        }
        else{
            unsigned int nEventArrayOffsetToWrite = 0;
            WriteNumber(&nEventArrayOffsetToWrite, 0, "", &PHnOffsetToEventArray);
        }

        Node & animRoot = FindAnimationRootNodeForWrite(anim.ArrayOfNodes, "animation '" + anim.sName + "'");
        ValidateHierarchyForWrite(animRoot, anim.ArrayOfNodes, Data.MH.Names.size(), "Animation '" + anim.sName + "'");
        unsigned int nOffsetToRootAnimationNodeToWrite = nPosition - 12;
        WriteNumber(&nOffsetToRootAnimationNodeToWrite, 0, "", &PHnOffsetToFirstNode);
        vMdxOffsetPlaceholders.clear();
        vNodeWriteOffsets.clear();
        vWrittenNodes.clear();
        vWrittenNodes.reserve(anim.ArrayOfNodes.size());
        sWriteNodePrefix = sAnimation;
        WriteNodes(animRoot);
        EnsureAllNodesWritten(anim.ArrayOfNodes, "Animation '" + anim.sName + "'");

        for(Node & animNode : anim.ArrayOfNodes){
            if(HasMdxOffsetPlaceholder(animNode) && (animNode.Head.nType & NODE_MESH) && !(animNode.Head.nType & NODE_SKIN)) WriteMDX(animNode);
        }
        for(Node & animNode : anim.ArrayOfNodes){
            if(HasMdxOffsetPlaceholder(animNode) && (animNode.Head.nType & NODE_SKIN)) WriteMDX(animNode);
        }
        vMdxOffsetPlaceholders.clear();
    }

    if(Data.MH.ArrayOfNodes.empty()){
        throw mdlexception("Cannot write geometry: model contains no nodes.");
    }
    ValidateUniqueNodeNameIndices(Data.MH.ArrayOfNodes, Data.MH.Names.size(), "Geometry");
    Node & geometryRoot = FindGeometryRootNodeForWrite(Data.MH);
    ValidateHierarchyForWrite(geometryRoot, Data.MH.ArrayOfNodes, Data.MH.Names.size(), "Geometry");
    unsigned int nRootNodeOffsetToWrite = nPosition - 12;
    WriteNumber(&nRootNodeOffsetToWrite, 0, "", &PHnOffsetToRootNode);
    if(!Data.MH.bHeadLink || !NodeExists("neck_g")){
        unsigned int nHeadRootOffsetToWrite = nRootNodeOffsetToWrite;
        WriteNumber(&nHeadRootOffsetToWrite, 0, "", &PHnOffsetToHeadRootNode); //For now we will make these two equal in all cases
    }
    else{
        nHeadRootPlaceholder = PHnOffsetToHeadRootNode;
        bHeadRootPlaceholderPending = true;
    }
    vMdxOffsetPlaceholders.clear();
    vNodeWriteOffsets.clear();
    vWrittenNodes.clear();
    vWrittenNodes.reserve(Data.MH.ArrayOfNodes.size());
    sWriteNodePrefix = "Geometry > ";
    WriteNodes(geometryRoot);
    EnsureAllNodesWritten(Data.MH.ArrayOfNodes, "Geometry");
    if(bHeadRootPlaceholderPending){
        // If a head-link placeholder was requested but no geometry neck_g was written,
        // fall back to the geometry root instead of leaving the model-header field unresolved.
        unsigned int nHeadRootOffsetToWrite = nRootNodeOffsetToWrite;
        WriteNumber(&nHeadRootOffsetToWrite, 0, "", &nHeadRootPlaceholder);
        bHeadRootPlaceholderPending = false;
    }
    for(Node &node : Data.MH.ArrayOfNodes){
        // Patch MDX offsets for every mesh header written in this pass.
        // Lightsaber mesh nodes do not write MDX vertex payload, but their
        // mesh header still contains an MDX offset placeholder; WriteMDX()
        // patches those to zero.
        if(HasMdxOffsetPlaceholder(node) && (node.Head.nType & NODE_MESH) && !(node.Head.nType & NODE_SKIN)) WriteMDX(node);
    }
    for(Node &node : Data.MH.ArrayOfNodes){
        if(HasMdxOffsetPlaceholder(node) && (node.Head.nType & NODE_SKIN)) WriteMDX(node);
    }

    unsigned int nMdlLengthToWrite = nPosition - 12;
    WriteNumber(&nMdlLengthToWrite, 0, "", &PHnMdlLength);
    unsigned int nMdxLengthToWrite = Mdx->nPosition;
    WriteNumber(&nMdxLengthToWrite, 0, "", &PHnMdxLength);
    WriteNumber(&nMdxLengthToWrite, 0, "", &PHnMdxLength2);


    if(Wok) Wok->Compile();
    if(Pwk) Pwk->Compile();
    if(Dwk0) Dwk0->Compile();
    if(Dwk1) Dwk1->Compile();
    if(Dwk2) Dwk2->Compile();
    compileRollback.Commit();
    ReportMdl << "Model compiled in " << tCompile.GetTime() << ".\n";
    return true;
}

static unsigned nAabbCount = 0;
void MDL::WriteAabb(const Aabb & aabb){
    std::string sAabb = sAabbNodePrefix + "Data > Aabb > Aabb Tree > Aabb Struct " + std::to_string(nAabbCount);
    nAabbCount++;


    // BB min and max
    WriteFloat(&aabb.vBBmin.fX, 2, sAabb);
    WriteFloat(&aabb.vBBmin.fY, 2, sAabb);
    WriteFloat(&aabb.vBBmin.fZ, 2, sAabb);
    WriteFloat(&aabb.vBBmax.fX, 2, sAabb);
    WriteFloat(&aabb.vBBmax.fY, 2, sAabb);
    WriteFloat(&aabb.vBBmax.fZ, 2, sAabb);

    // Child offsets. Use local write values so binary export does not dirty the
    // in-memory AABB tree with file offsets. Refuse to write stale/raw child
    // offsets when the parsed child payload is missing, because that would emit
    // dangling child pointers into the AABB tree.
    if(aabb.Child1.size() > 1 || aabb.Child2.size() > 1){
        throw mdlexception("AABB node has more than one child in a binary child slot; refusing to drop extra child data while writing.");
    }
    if(aabb.Child1.empty() && aabb.nChild1.Valid() && aabb.nChild1 > 0){
        throw mdlexception("AABB node has a raw child1 offset but no parsed child payload; refusing to write a dangling child pointer.");
    }
    if(aabb.Child2.empty() && aabb.nChild2.Valid() && aabb.nChild2 > 0){
        throw mdlexception("AABB node has a raw child2 offset but no parsed child payload; refusing to write a dangling child pointer.");
    }
    MdlInteger<unsigned int> nChild1ToWrite(0);
    MdlInteger<unsigned int> nChild2ToWrite(0);
    unsigned PHnChild1 = WriteNumber(&nChild1ToWrite, 6, sAabb);
    unsigned PHnChild2 = WriteNumber(&nChild2ToWrite, 6, sAabb);

    // Face ID - this is stored as a short in mdledit because face IDs only fit in a short in other places. Here it has to be converted to int
    signed int nConvertedID = aabb.nID.Valid() ? static_cast<signed int>(aabb.nID) : -1;
    WriteNumber(&nConvertedID, 4, sAabb);

    // Write significant plane
    MdlInteger<unsigned int> nPropertyToWrite = aabb.nProperty;
    WriteNumber(&nPropertyToWrite, 4, sAabb);

    MarkDataBorder(nPosition - 1);

    // Go through children
    if(aabb.Child1.size() > 0){
        nChild1ToWrite = nPosition - 12;
        WriteNumber(&nChild1ToWrite, 0, "", &PHnChild1);
        WriteAabb(aabb.Child1.front());
    }
    if(aabb.Child2.size() > 0){
        nChild2ToWrite = nPosition - 12;
        WriteNumber(&nChild2ToWrite, 0, "", &PHnChild2);
        WriteAabb(aabb.Child2.front());
    }
}

void MDL::WriteMDX(Node & node){
    if(!(node.Head.nType & NODE_MESH)) return;
    unsigned PHnOffsetIntoMdx = GetMdxOffsetPlaceholder(node);
    std::string sMdx = GetNodeName(node) + " > ";

    if((node.Head.nType & NODE_SABER) || node.Mesh.Vertices.size() == 0){
        MdlInteger<unsigned int> nZeroOffset(0);
        WriteNumber(&nZeroOffset, 0, "", &PHnOffsetIntoMdx);
        return;
    }

    /// First, write the padding that we collected from the previous MDX item
    std::string sPadding;
    sPadding.resize(nMdxPrevPadding, '\0');
    if(sPadding.size()) Mdx->WriteString(&sPadding, sPadding.size(), 0, "Padding");
    if(Mdx->nPosition > 0) Mdx->MarkDataBorder(Mdx->nPosition - 1);

    MdlInteger<unsigned int> nMdxOffsetToWrite(Mdx->nPosition);
    WriteNumber(&nMdxOffsetToWrite, 0, "", &PHnOffsetIntoMdx);

    // Runtime validation for the vanilla skin paths. Ignore unused
    // zero-weight garbage slots, but every active vertex influence must map
    // through compactSlot -> fullBone -> bonemap back to the same compact slot.
    const unsigned int nVertexCountToWrite = CheckedUIntCount(node.Mesh.Vertices.size(),
        "Node '" + GetNodeName(node) + "' MDX vertex count");

    if(node.Head.nType & NODE_SKIN){
        const unsigned short nMaxCompactSlot = bK2 ? 16 : 15;
        for(unsigned d = 0; d < nVertexCountToWrite; d++){
            Vertex & vert = node.Mesh.Vertices.at(d);
            bool bHasActiveInfluence = false;
            double fTotalWeight = 0.0;

            for(int nInfluence = 0; nInfluence < 4; nInfluence++){
                double fWeightValue = vert.MDXData.Weights.fWeightValue.at(nInfluence);
                if(!std::isfinite(fWeightValue)){
                    throw mdlexception("Skin vertex " + std::to_string(d) + " on node '" +
                                       GetNodeName(node) + "' has a non-finite weight.");
                }
                if(std::abs(fWeightValue) < 0.0000001) continue;

                bHasActiveInfluence = true;
                fTotalWeight += fWeightValue;

                MdlInteger<unsigned short> nCompactSlot = vert.MDXData.Weights.nWeightIndex.at(nInfluence);
                if(!nCompactSlot.Valid() || static_cast<unsigned short>(nCompactSlot) > nMaxCompactSlot){
                    throw mdlexception("Skin vertex " + std::to_string(d) + " on node '" +
                                       GetNodeName(node) + "' has an active influence with an invalid compact bone slot for this game.");
                }

                unsigned short nFullBone = SkinCompactSlotToFullBone(node, static_cast<unsigned short>(nCompactSlot));
                if(nFullBone >= node.Skin.Bones.size() || !node.Skin.Bones.at(nFullBone).nBonemap.Valid()){
                    throw mdlexception("Skin vertex " + std::to_string(d) + " on node '" +
                                       GetNodeName(node) + "' references a compact slot that has no uploaded full bone.");
                }

                if(static_cast<unsigned short>(node.Skin.Bones.at(nFullBone).nBonemap) != static_cast<unsigned short>(nCompactSlot)){
                    throw mdlexception("Skin palette mismatch on node '" + GetNodeName(node) +
                                       "': compact slot -> full bone -> bonemap does not resolve consistently.");
                }
            }

            if(!bHasActiveInfluence){
                throw mdlexception("Skin vertex " + std::to_string(d) + " on node '" +
                                   GetNodeName(node) + "' has no active skin influence.");
            }
            if(std::abs(fTotalWeight - 1.0) >= 0.0001){
                std::stringstream ss;
                ss << "Warning! Skin vertex " << d << " on node '" << GetNodeName(node)
                   << "' has active weights that sum to " << fTotalWeight << " instead of 1.0.";
                Warning(ss.str());
            }
        }
    }

    for(unsigned d = 0; d < nVertexCountToWrite; d++){
        std::string sMdxVertex = sMdx + "Vertex " + std::to_string(d);
        Vertex &vert = node.Mesh.Vertices.at(d);
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_VERTEX){

            Mdx->WriteFloat(&vert.MDXData.vVertex.fX, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vVertex.fY, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vVertex.fZ, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_NORMAL){
            if(!bXbox){
                Mdx->WriteFloat(&vert.MDXData.vNormal.fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vNormal.fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vNormal.fZ, 2, sMdxVertex);
            }
            else{
                unsigned nCompressed = CompressVector(vert.MDXData.vNormal);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
            }
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_COLOR){
            Mdx->WriteFloat(&vert.MDXData.cColor.fR, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.cColor.fG, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.cColor.fB, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV1){
            Mdx->WriteFloat(&vert.MDXData.vUV1.fX, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vUV1.fY, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV2){
            Mdx->WriteFloat(&vert.MDXData.vUV2.fX, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vUV2.fY, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV3){
            Mdx->WriteFloat(&vert.MDXData.vUV3.fX, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vUV3.fY, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV4){
            Mdx->WriteFloat(&vert.MDXData.vUV4.fX, 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.vUV4.fY, 2, sMdxVertex);
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT1){
            if(!bXbox){
                Mdx->WriteFloat(&vert.MDXData.vTangent1[0].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[0].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[0].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[1].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[1].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[1].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[2].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[2].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent1[2].fZ, 2, sMdxVertex);
            }
            else{
                unsigned nCompressed = CompressVector(vert.MDXData.vTangent1[0]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent1[1]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent1[2]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
            }
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT2){
            if(!bXbox){
                Mdx->WriteFloat(&vert.MDXData.vTangent2[0].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[0].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[0].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[1].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[1].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[1].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[2].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[2].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent2[2].fZ, 2, sMdxVertex);
            }
            else{
                unsigned nCompressed = CompressVector(vert.MDXData.vTangent2[0]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent2[1]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent2[2]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
            }
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT3){
            if(!bXbox){
                Mdx->WriteFloat(&vert.MDXData.vTangent3[0].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[0].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[0].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[1].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[1].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[1].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[2].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[2].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent3[2].fZ, 2, sMdxVertex);
            }
            else{
                unsigned nCompressed = CompressVector(vert.MDXData.vTangent3[0]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent3[1]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent3[2]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
            }
        }
        if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT4){
            if(!bXbox){
                Mdx->WriteFloat(&vert.MDXData.vTangent4[0].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[0].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[0].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[1].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[1].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[1].fZ, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[2].fX, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[2].fY, 2, sMdxVertex);
                Mdx->WriteFloat(&vert.MDXData.vTangent4[2].fZ, 2, sMdxVertex);
            }
            else{
                unsigned nCompressed = CompressVector(vert.MDXData.vTangent4[0]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent4[1]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
                nCompressed = CompressVector(vert.MDXData.vTangent4[2]);
                Mdx->WriteNumber(&nCompressed, 2, sMdxVertex);
            }
        }
        if(node.Head.nType &NODE_SKIN){
            Mdx->WriteFloat(&vert.MDXData.Weights.fWeightValue[0], 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.Weights.fWeightValue[1], 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.Weights.fWeightValue[2], 2, sMdxVertex);
            Mdx->WriteFloat(&vert.MDXData.Weights.fWeightValue[3], 2, sMdxVertex);
            if(bXbox){
                Mdx->WriteNumber(vert.MDXData.Weights.nWeightIndex[0].GetPtr(), 5, sMdxVertex);
                Mdx->WriteNumber(vert.MDXData.Weights.nWeightIndex[1].GetPtr(), 5, sMdxVertex);
                Mdx->WriteNumber(vert.MDXData.Weights.nWeightIndex[2].GetPtr(), 5, sMdxVertex);
                Mdx->WriteNumber(vert.MDXData.Weights.nWeightIndex[3].GetPtr(), 5, sMdxVertex);
            }
            else{
                double fIndex = static_cast<double>(vert.MDXData.Weights.nWeightIndex[0].Valid() ? static_cast<int>(vert.MDXData.Weights.nWeightIndex[0]) : -1);
                Mdx->WriteFloat(&fIndex, 2, sMdxVertex);
                fIndex = static_cast<double>(vert.MDXData.Weights.nWeightIndex[1].Valid() ? static_cast<int>(vert.MDXData.Weights.nWeightIndex[1]) : -1);
                Mdx->WriteFloat(&fIndex, 2, sMdxVertex);
                fIndex = static_cast<double>(vert.MDXData.Weights.nWeightIndex[2].Valid() ? static_cast<int>(vert.MDXData.Weights.nWeightIndex[2]) : -1);
                Mdx->WriteFloat(&fIndex, 2, sMdxVertex);
                fIndex = static_cast<double>(vert.MDXData.Weights.nWeightIndex[3].Valid() ? static_cast<int>(vert.MDXData.Weights.nWeightIndex[3]) : -1);
                Mdx->WriteFloat(&fIndex, 2, sMdxVertex);
            }
        }
        Mdx->MarkDataBorder(Mdx->nPosition - 1);
    }

    /// We first need to save the current position for the padding afterwards
    unsigned nCurrentMdx = Mdx->nPosition;

    /// Also write the extra empty vert data
    sMdx = GetNodeName(node) + " > ";
    std::string sMdxExtra = sMdx + "Extra Data";
    VertexData extraMdx = node.Mesh.MDXData;
    extraMdx.nNameIndex = node.Head.nNameIndex;
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_VERTEX){
        if(node.Head.nType &NODE_SKIN) extraMdx.vVertex.Set(1000000.0, 1000000.0, 1000000.0);
        else extraMdx.vVertex.Set(10000000.0, 10000000.0, 10000000.0);
        Mdx->WriteFloat(&extraMdx.vVertex.fX, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vVertex.fY, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vVertex.fZ, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_NORMAL){
        extraMdx.vNormal.Set(0.0, 0.0, 0.0);
        if(!bXbox){
            Mdx->WriteFloat(&extraMdx.vNormal.fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vNormal.fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vNormal.fZ, 8, sMdxExtra);
        }
        else{
            unsigned nCompressed = CompressVector(extraMdx.vNormal);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
        }
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_COLOR){
        extraMdx.cColor.Set(0.0, 0.0, 0.0);
        Mdx->WriteFloat(&extraMdx.cColor.fR, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.cColor.fG, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.cColor.fB, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV1){
        extraMdx.vUV1.Set(0.0, 0.0, 0.0);
        Mdx->WriteFloat(&extraMdx.vUV1.fX, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vUV1.fY, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV2){
        extraMdx.vUV2.Set(0.0, 0.0, 0.0);
        Mdx->WriteFloat(&extraMdx.vUV2.fX, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vUV2.fY, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV3){
        extraMdx.vUV3.Set(0.0, 0.0, 0.0);
        Mdx->WriteFloat(&extraMdx.vUV3.fX, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vUV3.fY, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_UV4){
        extraMdx.vUV4.Set(0.0, 0.0, 0.0);
        Mdx->WriteFloat(&extraMdx.vUV4.fX, 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.vUV4.fY, 8, sMdxExtra);
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT1){
        extraMdx.vTangent1[0].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent1[1].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent1[2].Set(0.0, 0.0, 0.0);
        if(!bXbox){
            Mdx->WriteFloat(&extraMdx.vTangent1[0].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[0].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[0].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[1].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[1].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[1].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[2].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[2].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent1[2].fZ, 8, sMdxExtra);
        }
        else{
            unsigned nCompressed = CompressVector(extraMdx.vTangent1[0]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent1[1]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent1[2]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
        }
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT2){
        extraMdx.vTangent2[0].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent2[1].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent2[2].Set(0.0, 0.0, 0.0);
        if(!bXbox){
            Mdx->WriteFloat(&extraMdx.vTangent2[0].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[0].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[0].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[1].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[1].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[1].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[2].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[2].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent2[2].fZ, 8, sMdxExtra);
        }
        else{
            unsigned nCompressed = CompressVector(extraMdx.vTangent2[0]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent2[1]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent2[2]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
        }
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT3){
        extraMdx.vTangent3[0].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent3[1].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent3[2].Set(0.0, 0.0, 0.0);
        if(!bXbox){
            Mdx->WriteFloat(&extraMdx.vTangent3[0].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[0].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[0].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[1].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[1].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[1].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[2].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[2].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent3[2].fZ, 8, sMdxExtra);
        }
        else{
            unsigned nCompressed = CompressVector(extraMdx.vTangent3[0]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent3[1]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent3[2]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
        }
    }
    if(node.Mesh.nMdxDataBitmap &MDX_FLAG_TANGENT4){
        extraMdx.vTangent4[0].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent4[1].Set(0.0, 0.0, 0.0);
        extraMdx.vTangent4[2].Set(0.0, 0.0, 0.0);
        if(!bXbox){
            Mdx->WriteFloat(&extraMdx.vTangent4[0].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[0].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[0].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[1].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[1].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[1].fZ, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[2].fX, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[2].fY, 8, sMdxExtra);
            Mdx->WriteFloat(&extraMdx.vTangent4[2].fZ, 8, sMdxExtra);
        }
        else{
            unsigned nCompressed = CompressVector(extraMdx.vTangent4[0]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent4[1]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
            nCompressed = CompressVector(extraMdx.vTangent4[2]);
            Mdx->WriteNumber(&nCompressed, 8, sMdxExtra);
        }
    }
    if(node.Head.nType & NODE_SKIN){
        extraMdx.Weights.fWeightValue[0] = 1.0;
        extraMdx.Weights.fWeightValue[1] = 0.0;
        extraMdx.Weights.fWeightValue[2] = 0.0;
        extraMdx.Weights.fWeightValue[3] = 0.0;
        extraMdx.Weights.nWeightIndex[0] = 0;
        extraMdx.Weights.nWeightIndex[1] = 0;
        extraMdx.Weights.nWeightIndex[2] = 0;
        extraMdx.Weights.nWeightIndex[3] = 0;
        Mdx->WriteFloat(&extraMdx.Weights.fWeightValue[0], 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.Weights.fWeightValue[1], 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.Weights.fWeightValue[2], 8, sMdxExtra);
        Mdx->WriteFloat(&extraMdx.Weights.fWeightValue[3], 8, sMdxExtra);
        if(bXbox){
            Mdx->WriteNumber(extraMdx.Weights.nWeightIndex[0].GetPtr(), 8, sMdxExtra);
            Mdx->WriteNumber(extraMdx.Weights.nWeightIndex[1].GetPtr(), 8, sMdxExtra);
            Mdx->WriteNumber(extraMdx.Weights.nWeightIndex[2].GetPtr(), 8, sMdxExtra);
            Mdx->WriteNumber(extraMdx.Weights.nWeightIndex[3].GetPtr(), 8, sMdxExtra);
        }
        else{
            double fIndex = static_cast<double>(extraMdx.Weights.nWeightIndex[0].Valid() ? static_cast<int>(extraMdx.Weights.nWeightIndex[0]) : -1);
            Mdx->WriteFloat(&fIndex, 8, sMdxExtra);
            fIndex = static_cast<double>(extraMdx.Weights.nWeightIndex[1].Valid() ? static_cast<int>(extraMdx.Weights.nWeightIndex[1]) : -1);
            Mdx->WriteFloat(&fIndex, 8, sMdxExtra);
            fIndex = static_cast<double>(extraMdx.Weights.nWeightIndex[2].Valid() ? static_cast<int>(extraMdx.Weights.nWeightIndex[2]) : -1);
            Mdx->WriteFloat(&fIndex, 8, sMdxExtra);
            fIndex = static_cast<double>(extraMdx.Weights.nWeightIndex[3].Valid() ? static_cast<int>(extraMdx.Weights.nWeightIndex[3]) : -1);
            Mdx->WriteFloat(&fIndex, 8, sMdxExtra);
        }
    }
    Mdx->MarkDataBorder(Mdx->nPosition - 1);

    /// Save the padding size for the next MDX item to use. Derive this from
    /// the actual bytes written instead of re-computing the record size from
    /// layout metadata; that keeps padding correct if a rare MDX layout differs
    /// from the expected stride calculation.
    (void)nCurrentMdx;
    nMdxPrevPadding = NextMdxPadding(Mdx->nPosition);
}

void MDL::WriteNodes(Node & node){
    FileHeader & Data = *FH;
    ReportObject ReportMdl(*this);
    NodeWriteStateGuard writeStateGuard(node);
    const unsigned int nNodeOffset = nPosition - 12;
    RememberNodeWriteOffset(node, nNodeOffset);
    vWrittenNodes.push_back(&node);


    std::string sNode = sWriteNodePrefix + std::string(GetNodeName(node).c_str()) + " > ";

    if(node.Head.nType & NODE_MESH){
        ValidateMeshDerivedIndexData(node, GetNodeName(node));
    }
    ValidateSkinArrayHeaderParity(node, GetNodeName(node));

    if(bHeadRootPlaceholderPending && !node.nAnimation.Valid() && Data.MH.bHeadLink && StringEqual(Data.MH.Names.at(node.Head.nNameIndex).sName, "neck_g", true)){
        unsigned int nHeadRootOffsetToWrite = nNodeOffset;
        WriteNumber(&nHeadRootOffsetToWrite, 0, "", &nHeadRootPlaceholder);
        bHeadRootPlaceholderPending = false;
    }
    WriteNumber(&node.Head.nType, 5, sNode + "Node Type");
    WriteNumber(&node.Head.nSupernodeNumber, 5, sNode + "Supernode Number");
    WriteNumber(&node.Head.nNameIndex, 5, sNode + "Node Number");
    WriteNumber(&node.Head.nPadding1, 8, sNode + (node.nAnimation.Valid() ? "Header" : "Header > Basic"));

    //Next is offset to root. For geo this is 0, for animations this is offset to animation
    if(!node.nAnimation.Valid()){
        MdlInteger<unsigned int> nOffsetToRootToWrite(0);
        WriteNumber(&nOffsetToRootToWrite, 6, sNode + "Offset to Root");
    }
    else{
        MdlInteger<unsigned int> nOffsetToRootToWrite(GetAnimationWriteOffset(static_cast<unsigned int>(node.nAnimation)));
        WriteNumber(&nOffsetToRootToWrite, 6, sNode + "Offset to Root");
    }

    //Next is offset to parent.
    if(!node.Head.nParentIndex.Valid()){
        MdlInteger<unsigned int> nOffsetToParentToWrite(0);
        WriteNumber(&nOffsetToParentToWrite, 6, sNode + "Offset to Parent");
    }
    else{
        //ReportMdl << "Found parent " << parent.Head.nNameIndex << ". His offset is " << parent.nOffset << ".\n";
        int nNodeIndex = GetNodeIndexByNameIndex(node.Head.nParentIndex, node.nAnimation.Valid() ? static_cast<int>(node.nAnimation) : -1);
        if(nNodeIndex == -1) throw mdlexception("converting something to binary error: could not find node with the specified parent name index.");
        std::vector<Node> * p_arrayofnodes = nullptr;
        if(!node.nAnimation.Valid()) p_arrayofnodes = &Data.MH.ArrayOfNodes;
        else p_arrayofnodes = &Data.MH.Animations.at(node.nAnimation).ArrayOfNodes;
        Node &parent = p_arrayofnodes->at(nNodeIndex);
        MdlInteger<unsigned int> nOffsetToParentToWrite(GetNodeWriteOffset(parent));
        WriteNumber(&nOffsetToParentToWrite, 6, sNode + "Offset to Parent");
    }

    //Write position
    WriteFloat(&node.Head.vPos.fX, 2, sNode + "Position");
    WriteFloat(&node.Head.vPos.fY, 2, sNode + "Position");
    WriteFloat(&node.Head.vPos.fZ, 2, sNode + "Position");

    //Write orientation - convert using a value copy so saving does not
    //turn an in-memory axis-angle orientation into a cached quaternion.
    Quaternion qNodeOrientation = node.Head.oOrient.GetQuaternionValue();
    WriteFloat(&qNodeOrientation.fW, 2, sNode + "Orientation");
    WriteFloat(&qNodeOrientation.vAxis.fX, 2, sNode + "Orientation");
    WriteFloat(&qNodeOrientation.vAxis.fY, 2, sNode + "Orientation");
    WriteFloat(&qNodeOrientation.vAxis.fZ, 2, sNode + "Orientation");

    /// Children Array
    unsigned PHnOffsetToChildren = WriteBytes(placeholder, 4, 6, sNode + "Child Array");
    node.Head.ChildrenArray.ResetToSize(node.Head.ChildIndices.size());
    WriteNumber(&node.Head.ChildrenArray.nCount, 1, sNode + "Child Array");
    WriteNumber(&node.Head.ChildrenArray.nCount2, 1, sNode + "Child Array");

    /// Controller Array
    unsigned PHnOffsetToControllers = WriteBytes(placeholder, 4, 6, sNode + "Controller Array");
    node.Head.ControllerArray.ResetToSize(node.Head.Controllers.size());
    WriteNumber(&node.Head.ControllerArray.nCount, 1, sNode + "Controller Array");
    WriteNumber(&node.Head.ControllerArray.nCount2, 1, sNode + "Controller Array");

    /// Controller Data Array
    unsigned PHnOffsetToControllerData = WriteBytes(placeholder, 4, 6, sNode + "Controller Data Array");
    node.Head.ControllerDataArray.ResetToSize(node.Head.ControllerData.size());
    WriteNumber(&node.Head.ControllerDataArray.nCount, 1, sNode + "Controller Data Array");
    WriteNumber(&node.Head.ControllerDataArray.nCount2, 1, sNode + "Controller Data Array");
    MarkDataBorder(nPosition - 1);


    //Here comes all the subheader bullshit.
    /// LIGHT HEADER
    std::string sLight = sNode.substr(0, sNode.size() - 3) + " > Header > Light";
    unsigned PHnOffsetToFlareSizes, PHnOffsetToFlarePositions, PHnOffsetToFlareTextureNames, PHnOffsetToFlareColorShifts;
    if(node.Head.nType & NODE_LIGHT){
        WriteFloat(&node.Light.fFlareRadius, 2, sLight);
        WriteNumber(&node.Light.UnknownArray.nOffset, 8, sLight);
        WriteNumber(&node.Light.UnknownArray.nCount, 8, sLight);
        WriteNumber(&node.Light.UnknownArray.nCount2, 8, sLight);
        PHnOffsetToFlareSizes = WriteBytes(placeholder, 4, 6, sLight);
        node.Light.FlareSizeArray.ResetToSize(node.Light.FlareSizes.size());
        WriteNumber(&node.Light.FlareSizeArray.nCount, 1, sLight);
        WriteNumber(&node.Light.FlareSizeArray.nCount2, 1, sLight);
        PHnOffsetToFlarePositions = WriteBytes(placeholder, 4, 6, sLight);
        node.Light.FlarePositionArray.ResetToSize(node.Light.FlarePositions.size());
        WriteNumber(&node.Light.FlarePositionArray.nCount, 1, sLight);
        WriteNumber(&node.Light.FlarePositionArray.nCount2, 1, sLight);
        PHnOffsetToFlareColorShifts = WriteBytes(placeholder, 4, 6, sLight);
        node.Light.FlareColorShiftArray.ResetToSize(node.Light.FlareColorShifts.size());
        WriteNumber(&node.Light.FlareColorShiftArray.nCount, 1, sLight);
        WriteNumber(&node.Light.FlareColorShiftArray.nCount2, 1, sLight);
        PHnOffsetToFlareTextureNames = WriteBytes(placeholder, 4, 6, sLight);
        node.Light.FlareTextureNameArray.ResetToSize(node.Light.FlareTextureNames.size());
        WriteNumber(&node.Light.FlareTextureNameArray.nCount, 1, sLight);
        WriteNumber(&node.Light.FlareTextureNameArray.nCount2, 1, sLight);

        WriteNumber(&node.Light.nLightPriority, 4, sLight);
        WriteNumber(&node.Light.nAmbientOnly, 4, sLight);
        WriteNumber(&node.Light.nDynamicType, 4, sLight);
        WriteNumber(&node.Light.nAffectDynamic, 4, sLight);
        WriteNumber(&node.Light.nShadow, 4, sLight);
        WriteNumber(&node.Light.nFlare, 4, sLight);
        WriteNumber(&node.Light.nFadingLight, 4, sLight);
        MarkDataBorder(nPosition - 1);
    }

    /// EMITTER HEADER
    std::string sEmitter = sNode.substr(0, sNode.size() - 3) + " > Header > Emitter";
    if(node.Head.nType &NODE_EMITTER){
        WriteFloat(&node.Emitter.fDeadSpace, 2, sEmitter);
        WriteFloat(&node.Emitter.fBlastRadius, 2, sEmitter);
        WriteFloat(&node.Emitter.fBlastLength, 2, sEmitter);

        WriteNumber(&node.Emitter.nBranchCount, 1, sEmitter);
        WriteFloat(&node.Emitter.fControlPointSmoothing, 2, sEmitter);

        WriteNumber(&node.Emitter.nxGrid, 4, sEmitter);
        WriteNumber(&node.Emitter.nyGrid, 4, sEmitter);
        WriteNumber(&node.Emitter.nSpawnType, 4, sEmitter);

        WriteString(&node.Emitter.cUpdate, 32, 3, sEmitter);
        WriteString(&node.Emitter.cRender, 32, 3, sEmitter);
        WriteString(&node.Emitter.cBlend, 32, 3, sEmitter);
        WriteString(&node.Emitter.cTexture, 32, 3, sEmitter);
        WriteString(&node.Emitter.cChunkName, 16, 3, sEmitter);

        WriteNumber(&node.Emitter.nTwosidedTex, 4, sEmitter);
        WriteNumber(&node.Emitter.nLoop, 4, sEmitter);
        WriteNumber(&node.Emitter.nRenderOrder, 5, sEmitter);
        WriteNumber(&node.Emitter.nFrameBlending, 7, sEmitter);

        WriteString(&node.Emitter.cDepthTextureName, 32, 3, sEmitter);

        WriteNumber(&node.Emitter.nPadding1, 11, sEmitter);
        WriteNumber(&node.Emitter.nFlags, 4, sEmitter);
        MarkDataBorder(nPosition - 1);
    }

    /// REFERENCE HEADER
    std::string sReference = sNode.substr(0, sNode.size() - 3) + " > Header > Reference";
    if(node.Head.nType &NODE_REFERENCE){
        WriteString(&node.Reference.sRefModel, 32, 3, sReference);
        WriteNumber(&node.Reference.nReattachable, 4, sReference);
        MarkDataBorder(nPosition - 1);
    }

    /// MESH HEADER
    std::string sMesh = sNode.substr(0, sNode.size() - 3) + " > Header > Mesh";
    std::string sMeshData = sNode.substr(0, sNode.size() - 3);
    unsigned PHnOffsetToFaces, PHnOffsetToIndexCount, PHnOffsetToIndexLocation, PHnOffsetToInvertedCounter, PHnOffsetIntoMdx, PHnOffsetToVertArray;
    if(node.Head.nType & NODE_MESH){
        //Write function pointers
        unsigned int nMeshFunctionPointer0 = 0;
        unsigned int nMeshFunctionPointer1 = 0;
        if(node.Head.nType & NODE_DANGLY){
            nMeshFunctionPointer0 = FunctionPointer1(FN_PTR_DANGLY);
            nMeshFunctionPointer1 = FunctionPointer2(FN_PTR_DANGLY);
        }
        else if(node.Head.nType & NODE_SKIN){
            nMeshFunctionPointer0 = FunctionPointer1(FN_PTR_SKIN);
            nMeshFunctionPointer1 = FunctionPointer2(FN_PTR_SKIN);
        }
        else{
            nMeshFunctionPointer0 = FunctionPointer1(FN_PTR_MESH);
            nMeshFunctionPointer1 = FunctionPointer2(FN_PTR_MESH);
        }
        WriteNumber(&nMeshFunctionPointer0, 9, sMesh);
        WriteNumber(&nMeshFunctionPointer1, 9, sMesh);
        node.Mesh.FaceArray.ResetToSize(node.Mesh.Faces.size());
        PHnOffsetToFaces = WriteNumber(&node.Mesh.FaceArray.nOffset, 6, sMesh); /// This will remain 0 in case there are no faces
        WriteNumber(&node.Mesh.FaceArray.nCount, 1, sMesh);
        WriteNumber(&node.Mesh.FaceArray.nCount2, 1, sMesh);

        //Tons of floats
        WriteFloat(&node.Mesh.vBBmin.fX, 2, sMesh);
        WriteFloat(&node.Mesh.vBBmin.fY, 2, sMesh);
        WriteFloat(&node.Mesh.vBBmin.fZ, 2, sMesh);
        WriteFloat(&node.Mesh.vBBmax.fX, 2, sMesh);
        WriteFloat(&node.Mesh.vBBmax.fY, 2, sMesh);
        WriteFloat(&node.Mesh.vBBmax.fZ, 2, sMesh);
        WriteFloat(&node.Mesh.fRadius, 2, sMesh);
        WriteFloat(&node.Mesh.vAverage.fX, 2, sMesh);
        WriteFloat(&node.Mesh.vAverage.fY, 2, sMesh);
        WriteFloat(&node.Mesh.vAverage.fZ, 2, sMesh);
        WriteFloat(&node.Mesh.fDiffuse.fR, 2, sMesh);
        WriteFloat(&node.Mesh.fDiffuse.fG, 2, sMesh);
        WriteFloat(&node.Mesh.fDiffuse.fB, 2, sMesh);
        WriteFloat(&node.Mesh.fAmbient.fR, 2, sMesh);
        WriteFloat(&node.Mesh.fAmbient.fG, 2, sMesh);
        WriteFloat(&node.Mesh.fAmbient.fB, 2, sMesh);

        WriteNumber(&node.Mesh.nTransparencyHint, 4, sMesh);

        WriteString(&node.Mesh.cTexture1, 32, 3, sMesh);
        WriteString(&node.Mesh.cTexture2, 32, 3, sMesh);
        WriteString(&node.Mesh.cTexture3, 12, 3, sMesh);
        WriteString(&node.Mesh.cTexture4, 12, 3, sMesh);

        int nSingleSize = (node.Mesh.Faces.size() == 0 || (node.Head.nType &NODE_SABER)) ? 0 : 1;
        node.Mesh.IndexCounterArray.ResetToSize(nSingleSize);
        node.Mesh.IndexLocationArray.ResetToSize(nSingleSize);
        node.Mesh.MeshInvertedCounterArray.ResetToSize(nSingleSize);
        PHnOffsetToIndexCount = WriteBytes(placeholder, 4, 6, sMesh); /// This must always be updated to where the index count would be if it existed
        WriteNumber(&node.Mesh.IndexCounterArray.nCount, 1, sMesh);
        WriteNumber(&node.Mesh.IndexCounterArray.nCount, 1, sMesh);
        PHnOffsetToIndexLocation = WriteNumber(&node.Mesh.IndexLocationArray.nOffset, 6, sMesh); /// This will remain 0 in case there are no faces
        WriteNumber(&node.Mesh.IndexLocationArray.nCount, 1, sMesh);
        WriteNumber(&node.Mesh.IndexLocationArray.nCount, 1, sMesh);
        PHnOffsetToInvertedCounter = WriteNumber(&node.Mesh.MeshInvertedCounterArray.nOffset, 6, sMesh); /// This will remain 0 in case there are no faces
        WriteNumber(&node.Mesh.MeshInvertedCounterArray.nCount, 1, sMesh);
        WriteNumber(&node.Mesh.MeshInvertedCounterArray.nCount, 1, sMesh);

        //Write unknown -1 -1 0
        WriteNumber(&node.Mesh.nUnknown3.at(0), 11, sMesh);
        WriteNumber(&node.Mesh.nUnknown3.at(1), 8, sMesh);
        WriteNumber(&node.Mesh.nUnknown3.at(2), 8, sMesh);

        WriteNumber(&node.Mesh.nSaberUnknown1, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown2, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown3, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown4, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown5, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown6, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown7, 11, sMesh);
        WriteNumber(&node.Mesh.nSaberUnknown8, 11, sMesh);

        //animated UV stuff
        WriteNumber(&node.Mesh.nAnimateUV, 4, sMesh);
        WriteFloat(&node.Mesh.fUVDirectionX, 2, sMesh);
        WriteFloat(&node.Mesh.fUVDirectionY, 2, sMesh);
        WriteFloat(&node.Mesh.fUVJitter, 2, sMesh);
        WriteFloat(&node.Mesh.fUVJitterSpeed, 2, sMesh);

        //MDX stuff
        //Take care of the mdx pointer stuff. The only thing we need set is the bitmap, which should be done on import.
        MdxLayout mdxLayout = BuildMdxLayout(node, bXbox, GetNodeName(node));
        unsigned int nMdxDataSizeToWrite = mdxLayout.nMdxDataSize;

        WriteNumber(&nMdxDataSizeToWrite, 1, sMesh);
        WriteNumber(&node.Mesh.nMdxDataBitmap, 4, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxVertex, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxNormal, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxColor, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxUV1, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxUV2, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxUV3, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxUV4, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxTangent1, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxTangent2, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxTangent3, 6, sMesh);
        WriteNumber(&mdxLayout.nOffsetToMdxTangent4, 6, sMesh);

        unsigned short nNumberOfVertsToWrite = CheckedUShortCount(node.Mesh.Vertices.size(),
            "Node '" + GetNodeName(node) + "' vertex count");
        WriteNumber(&nNumberOfVertsToWrite, 1, sMeshData + " > Data > Mesh > Pointer to Vert Number");
        WriteNumber(&node.Mesh.nTextureNumber, 1, sMesh);

        WriteNumber(&node.Mesh.nHasLightmap, 7, sMesh);
        WriteNumber(&node.Mesh.nRotateTexture, 7, sMesh);
        WriteNumber(&node.Mesh.nBackgroundGeometry, 7, sMesh);
        WriteNumber(&node.Mesh.nShadow, 7, sMesh);
        WriteNumber(&node.Mesh.nBeaming, 7, sMesh);
        WriteNumber(&node.Mesh.nRender, 7, sMesh);

        if(bK2) WriteNumber(&node.Mesh.nDirtEnabled, 7, sMesh);
        if(bK2) WriteNumber(&node.Mesh.nPadding1, 11, sMesh);
        if(bK2) WriteNumber(&node.Mesh.nDirtTexture, 5, sMesh);
        if(bK2) WriteNumber(&node.Mesh.nDirtCoordSpace, 5, sMesh);
        if(bK2) WriteNumber(&node.Mesh.nHideInHolograms, 7, sMesh);
        if(bK2) WriteNumber(&node.Mesh.nPadding2, 11, sMesh);

        WriteNumber(&node.Mesh.nPadding3, 11, sMesh);

        WriteFloat(&node.Mesh.fTotalArea, 2, sMesh);
        WriteNumber(&node.Mesh.nPadding, 8, sMesh);

        MdlInteger<unsigned int> nZeroMdxOffset(0);
        PHnOffsetIntoMdx = WriteNumber(&nZeroMdxOffset, 6, sMesh); /// Will stay like this if there are no verts

        /// We moved the mdx writing to a separate function, so remember the
        /// placeholder separately instead of storing it in the live model field.
        RememberMdxOffsetPlaceholder(node, PHnOffsetIntoMdx);

        if(!bXbox){
            unsigned int nOffsetToVertArrayToWrite = 0;
            PHnOffsetToVertArray = WriteNumber(&nOffsetToVertArrayToWrite, 6, sMesh); /// Will stay like this if there are no verts
        }
        MarkDataBorder(nPosition - 1);
    }

    /// SKIN HEADER
    std::string sSkin = sNode.substr(0, sNode.size() - 3) + " > Header > Skin";
    std::string sSkinData = sNode.substr(0, sNode.size() - 3);
    unsigned PHnOffsetToBonemap, PHnOffsetToQBones, PHnOffsetToTBones, PHnOffsetToArray8;
    if(node.Head.nType &NODE_SKIN){
        WriteNumber(&node.Skin.UnknownArray.nOffset, 8, sSkin); //Unknown int32
        WriteNumber(&node.Skin.UnknownArray.nCount, 8, sSkin); //Unknown int32
        WriteNumber(&node.Skin.UnknownArray.nCount2, 8, sSkin); //Unknown int32
        MdxLayout mdxLayout = BuildMdxLayout(node, bXbox, GetNodeName(node));
        WriteNumber(&mdxLayout.nOffsetToMdxWeightValues, 6, sSkin);
        WriteNumber(&mdxLayout.nOffsetToMdxBoneIndices, 6, sSkin);
        PHnOffsetToBonemap = WriteBytes(placeholder, 4, 6, sSkin);
        unsigned int nNumberOfBonemapToWrite = CheckedUIntCount(node.Skin.Bones.size(),
            "Node '" + GetNodeName(node) + "' skin bonemap count");
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        PHnOffsetToQBones = WriteBytes(placeholder, 4, 6, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        PHnOffsetToTBones = WriteBytes(placeholder, 4, 6, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        PHnOffsetToArray8 = WriteBytes(placeholder, 4, 6, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        WriteNumber(&nNumberOfBonemapToWrite, 1, sSkin);
        for(unsigned a = 0; a < 16; a++){
            WriteNumber(&node.Skin.nBoneIndices[a], 5, sSkin);
        }
        WriteNumber(&node.Skin.nPadding1, 11, sSkin);
        WriteNumber(&node.Skin.nPadding2, 11, sSkin);
        MarkDataBorder(nPosition - 1);
    }

    /// DANGLY HEADER
    std::string sDangly = sNode.substr(0, sNode.size() - 3) + " > Header > Danglymesh";
    std::string sDanglyData = sNode.substr(0, sNode.size() - 3);
    unsigned PHnOffsetToConstraints, PHnOffsetToData2;
    if(node.Head.nType & NODE_DANGLY){
        PHnOffsetToConstraints = WriteBytes(placeholder, 4, 6, sDangly);
        node.Dangly.ConstraintArray.ResetToSize(node.Dangly.Constraints.size());
        WriteNumber(&node.Dangly.ConstraintArray.nCount, 1, sDangly);
        WriteNumber(&node.Dangly.ConstraintArray.nCount2, 1, sDangly);
        WriteFloat(&node.Dangly.fDisplacement, 2, sDangly);
        WriteFloat(&node.Dangly.fTightness, 2, sDangly);
        WriteFloat(&node.Dangly.fPeriod, 2, sDangly);
        PHnOffsetToData2 = WriteBytes(placeholder, 4, 6, sDangly);
        MarkDataBorder(nPosition - 1);

    }

    /// AABB HEADER
    std::string sAabb = sNode.substr(0, sNode.size() - 3) + " > Header > Aabb";
    unsigned PHnOffsetToAabb;
    if(node.Head.nType &NODE_AABB){
        PHnOffsetToAabb = WriteBytes(placeholder, 4, 6, sAabb);
        MarkDataBorder(nPosition - 1);
    }

    /// SABER HEADER
    std::string sSaber = sNode.substr(0, sNode.size() - 3) + " > Header > Lightsaber";
    std::string sSaberData = sNode.substr(0, sNode.size() - 3);
    unsigned PHnOffsetToSaberVerts, PHnOffsetToSaberUVs, PHnOffsetToSaberNormals;
    if(node.Head.nType & NODE_SABER){
        PHnOffsetToSaberVerts = WriteBytes(placeholder, 4, 6, sSaber);
        PHnOffsetToSaberUVs = WriteBytes(placeholder, 4, 6, sSaber);
        PHnOffsetToSaberNormals = WriteBytes(placeholder, 4, 6, sSaber);
        WriteNumber(&node.Saber.nInvCount1, 4, sSaber);
        WriteNumber(&node.Saber.nInvCount2, 4, sSaber);
        MarkDataBorder(nPosition - 1);
    }

    //now comes all the data in reversed order
    /// SABER DATA
    if(node.Head.nType & NODE_SABER){
        unsigned int nOffsetToSaberVertsToWrite = nPosition - 12;
        WriteNumber(&nOffsetToSaberVertsToWrite, 0, "", &PHnOffsetToSaberVerts);
        for(unsigned d = 0; d < node.Saber.SaberData.size(); d++){
            WriteFloat(&node.Saber.SaberData[d].vVertex.fX, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(d));
            WriteFloat(&node.Saber.SaberData[d].vVertex.fY, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(d));
            WriteFloat(&node.Saber.SaberData[d].vVertex.fZ, 2, sSaberData + " > Data > Lightsaber > Vertices > Vertex " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
        unsigned int nOffsetToSaberNormalsToWrite = nPosition - 12;
        WriteNumber(&nOffsetToSaberNormalsToWrite, 0, "", &PHnOffsetToSaberNormals);
        for(unsigned d = 0; d < node.Saber.SaberData.size(); d++){
            WriteFloat(&node.Saber.SaberData[d].vNormal.fX, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(d));
            WriteFloat(&node.Saber.SaberData[d].vNormal.fY, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(d));
            WriteFloat(&node.Saber.SaberData[d].vNormal.fZ, 2, sSaberData + " > Data > Lightsaber > Normals > Normal " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
        unsigned int nOffsetToSaberUVsToWrite = nPosition - 12;
        WriteNumber(&nOffsetToSaberUVsToWrite, 0, "", &PHnOffsetToSaberUVs);
        for(unsigned d = 0; d < node.Saber.SaberData.size(); d++){
            WriteFloat(&node.Saber.SaberData[d].vUV1.fX, 2, sSaberData + " > Data > Lightsaber > UVs > UV " + std::to_string(d));
            WriteFloat(&node.Saber.SaberData[d].vUV1.fY, 2, sSaberData + " > Data > Lightsaber > UVs > UV " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }

    }

    /// AABB DATA
    if(node.Head.nType & NODE_AABB){
        MdlInteger<unsigned int> nOffsetToAabbToWrite(nPosition - 12);
        WriteNumber(&nOffsetToAabbToWrite, 0, "", &PHnOffsetToAabb);
        nAabbCount = 0;
        sAabbNodePrefix = sNode;
        WriteAabb(node.Walkmesh.RootAabb);
        MarkDataBorder(nPosition - 1);
    }

    /// DANGLY DATA
    if(node.Head.nType & NODE_DANGLY){
        node.Dangly.ConstraintArray.ResetToSize(node.Dangly.Constraints.size());
        if(node.Dangly.Constraints.size() > 0){
            node.Dangly.ConstraintArray.nOffset = nPosition - 12;
            WriteNumber(&node.Dangly.ConstraintArray.nOffset, 0, "", &PHnOffsetToConstraints);
        }
        else{
            node.Dangly.ConstraintArray.nOffset = 0;
            WriteNumber(&node.Dangly.ConstraintArray.nOffset, 0, "", &PHnOffsetToConstraints);
        }
        for(unsigned d = 0; d < node.Dangly.Constraints.size(); d++){
            WriteNumber(&node.Dangly.Constraints.at(d), 2, sDanglyData + " > Data > Danglymesh > Constraints > Constraint " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
        node.Dangly.nOffsetToData2 = nPosition - 12;
        WriteNumber(&node.Dangly.nOffsetToData2, 0, "", &PHnOffsetToData2);
        for(unsigned d = 0; d < node.Dangly.Data2.size(); d++){
            WriteFloat(&node.Dangly.Data2.at(d).fX, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(d));
            WriteFloat(&node.Dangly.Data2.at(d).fY, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(d));
            WriteFloat(&node.Dangly.Data2.at(d).fZ, 2, sDanglyData + " > Data > Danglymesh > Data2 > Vertex " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
    }

    /// SKIN DATA
    if(node.Head.nType &NODE_SKIN){
        node.Skin.nNumberOfBonemap = node.Skin.Bones.size();
        if(node.Skin.Bones.size() > 0){
            node.Skin.nOffsetToBonemap = nPosition - 12;
            WriteNumber(&node.Skin.nOffsetToBonemap, 0, "", &PHnOffsetToBonemap);
        }
        else{
            node.Skin.nOffsetToBonemap = 0;
            WriteNumber(&node.Skin.nOffsetToBonemap, 0, "", &PHnOffsetToBonemap);
        }
        for(unsigned d = 0; d < node.Skin.Bones.size(); d++){
            if(bXbox) WriteNumber(node.Skin.Bones.at(d).nBonemap.GetPtr(), 5, sSkinData + " > Data > Skin > Bonemap > " + std::to_string(d));
            else{
                double fHelper = static_cast<double>(node.Skin.Bones.at(d).nBonemap.Valid() ? static_cast<int>(node.Skin.Bones.at(d).nBonemap) : -1);
                WriteFloat(&fHelper, 2, sSkinData + " > Data > Skin > Bonemap > " + std::to_string(d));
            }
            MarkDataBorder(nPosition - 1);
        }
        node.Skin.QBoneArray.ResetToSize(node.Skin.Bones.size());
        if(node.Skin.Bones.size() > 0){
            node.Skin.QBoneArray.nOffset = nPosition - 12;
            WriteNumber(&node.Skin.QBoneArray.nOffset, 0, "", &PHnOffsetToQBones);
        }
        else{
            node.Skin.QBoneArray.nOffset = 0;
            WriteNumber(&node.Skin.QBoneArray.nOffset, 0, "", &PHnOffsetToQBones);
        }
        for(unsigned d = 0; d < node.Skin.Bones.size(); d++){
            Quaternion qBone = node.Skin.Bones.at(d).QBone.GetQuaternionValue();
            WriteFloat(&qBone.fW, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(d));
            WriteFloat(&qBone.vAxis.fX, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(d));
            WriteFloat(&qBone.vAxis.fY, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(d));
            WriteFloat(&qBone.vAxis.fZ, 2, sSkinData + " > Data > Skin > Q-Bones > " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
        node.Skin.TBoneArray.ResetToSize(node.Skin.Bones.size());
        if(node.Skin.Bones.size() > 0){
            node.Skin.TBoneArray.nOffset = nPosition - 12;
            WriteNumber(&node.Skin.TBoneArray.nOffset, 0, "", &PHnOffsetToTBones);
        }
        else{
            node.Skin.TBoneArray.nOffset = 0;
            WriteNumber(&node.Skin.TBoneArray.nOffset, 0, "", &PHnOffsetToTBones);
        }
        for(unsigned d = 0; d < node.Skin.Bones.size(); d++){
            WriteFloat(&node.Skin.Bones.at(d).TBone.fX, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(d));
            WriteFloat(&node.Skin.Bones.at(d).TBone.fY, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(d));
            WriteFloat(&node.Skin.Bones.at(d).TBone.fZ, 2, sSkinData + " > Data > Skin > T-Bones > " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }
        node.Skin.Array8Array.ResetToSize(node.Skin.Bones.size());
        if(node.Skin.Bones.size() > 0){
            node.Skin.Array8Array.nOffset = nPosition - 12;
            WriteNumber(&node.Skin.Array8Array.nOffset, 0, "", &PHnOffsetToArray8);
        }
        else{
            node.Skin.Array8Array.nOffset = 0;
            WriteNumber(&node.Skin.Array8Array.nOffset, 0, "", &PHnOffsetToArray8);
        }
        for(unsigned d = 0; d < node.Skin.Bones.size(); d++){
            WriteNumber(&node.Skin.Bones.at(d).nPadding, 11, sSkinData + " > Data > Skin > Array8 (unused)"); //This array is not in use, might as well fill it with zeros
        }
        MarkDataBorder(nPosition - 1);
    }

    /// MESH DATA
    if(node.Head.nType &NODE_MESH){

        /// Write faces
        if(node.Mesh.Faces.size() > 0){
            node.Mesh.FaceArray.nOffset = nPosition - 12;
            WriteNumber(&node.Mesh.FaceArray.nOffset, 0, "", &PHnOffsetToFaces);
        }
        else{
            node.Mesh.FaceArray.nOffset = 0;
            WriteNumber(&node.Mesh.FaceArray.nOffset, 0, "", &PHnOffsetToFaces);
        }
        for(unsigned d = 0; d < node.Mesh.Faces.size(); d++){
            Face & face = node.Mesh.Faces.at(d);


            WriteFloat(&face.vNormal.fX, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteFloat(&face.vNormal.fY, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteFloat(&face.vNormal.fZ, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteFloat(&face.fDistance, 2, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteNumber(&face.nMaterialID, 4, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteNumber(face.nAdjacentFaces[0].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteNumber(face.nAdjacentFaces[1].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteNumber(face.nAdjacentFaces[2].GetPtr(), 5, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
            WriteNumber(face.nIndexVertex[0].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
            WriteNumber(face.nIndexVertex[1].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
            WriteNumber(face.nIndexVertex[2].GetPtr(), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
            MarkDataBorder(nPosition - 1);
        }

        /// If SG recording is enabled, write SGs
        if(bWriteSmoothing){
            for(unsigned d = 0; d < node.Mesh.Faces.size(); d++){
                Face & face = node.Mesh.Faces.at(d);
                WriteNumber(&face.nSmoothingGroup, 4, sMeshData + " > Data > Mesh > Faces > Face " + std::to_string(d));
                MarkDataBorder(nPosition - 1);
            }
        }

        if(node.Head.nType & NODE_SABER){
            node.Mesh.IndexCounterArray.ResetToSize(0);
            //WriteIntToPH(0, PHnOffsetToIndexCount, node.Mesh.IndexCounterArray.nOffset);
            node.Mesh.IndexCounterArray.nOffset = nPosition - 12;
            WriteNumber(&node.Mesh.IndexCounterArray.nOffset, 0, "", &PHnOffsetToIndexCount);

            /// This should possibly be deleted for the saber
            if(!bXbox){
                unsigned int nOffsetToVertArrayToWrite = nPosition - 12;
                WriteNumber(&nOffsetToVertArrayToWrite, 0, "", &PHnOffsetToVertArray);
                for(unsigned d = 0; d < node.Mesh.Vertices.size(); d++){
                    WriteFloat(&node.Mesh.Vertices.at(d).fX, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                    WriteFloat(&node.Mesh.Vertices.at(d).fY, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                    WriteFloat(&node.Mesh.Vertices.at(d).fZ, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                    MarkDataBorder(nPosition - 1);
                }
            }

            node.Mesh.IndexLocationArray.ResetToSize(0);
            //WriteIntToPH(0, PHnOffsetToIndexLocation, node.Mesh.IndexLocationArray.nOffset);
            node.Mesh.IndexLocationArray.nOffset = nPosition - 12;
            WriteNumber(&node.Mesh.IndexLocationArray.nOffset, 0, "", &PHnOffsetToIndexLocation);

            node.Mesh.MeshInvertedCounterArray.ResetToSize(0);
            //WriteIntToPH(0, PHnOffsetToInvertedCounter, node.Mesh.MeshInvertedCounterArray.nOffset);
            node.Mesh.MeshInvertedCounterArray.nOffset = nPosition - 12;
            WriteNumber(&node.Mesh.MeshInvertedCounterArray.nOffset, 0, "", &PHnOffsetToInvertedCounter);
            //WriteIntToPH(0, PHnPointerToIndexLocation, node.Mesh.nVertIndicesLocation);
        }
        else{
            /// The index counter offset is always set
            node.Mesh.IndexCounterArray.nOffset = nPosition - 12;
            WriteNumber(&node.Mesh.IndexCounterArray.nOffset, 0, "", &PHnOffsetToIndexCount);

            if(node.Mesh.Faces.size() > 0){
                const unsigned int nVertIndicesCountToWrite = CheckedUIntTripleCount(node.Mesh.Faces.size(),
                    "Node '" + GetNodeName(node) + "' inverted-index count");
                WriteNumber(&nVertIndicesCountToWrite, 1, sMeshData + " > Data > Mesh > Inverted Counter");
                MarkDataBorder(nPosition - 1);

                if(!bXbox){
                    unsigned int nOffsetToVertArrayToWrite = nPosition - 12;
                    WriteNumber(&nOffsetToVertArrayToWrite, 0, "", &PHnOffsetToVertArray);
                    for(unsigned d = 0; d < node.Mesh.Vertices.size(); d++){
                        WriteFloat(&node.Mesh.Vertices.at(d).fX, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                        WriteFloat(&node.Mesh.Vertices.at(d).fY, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                        WriteFloat(&node.Mesh.Vertices.at(d).fZ, 2, sMeshData + " > Data > Mesh > Vert Coordinates > Vert " + std::to_string(d));
                        MarkDataBorder(nPosition - 1);
                    }
                }

                node.Mesh.IndexLocationArray.ResetToSize(1);
                node.Mesh.IndexLocationArray.nOffset = nPosition - 12;
                WriteNumber(&node.Mesh.IndexLocationArray.nOffset, 0, "", &PHnOffsetToIndexLocation);

                unsigned PHnPointerToIndexLocation = WriteBytes(placeholder, 4, 6, sMeshData + " > Data > Mesh > Pointer to Vert Indices");
                MarkDataBorder(nPosition - 1);

                node.Mesh.MeshInvertedCounterArray.ResetToSize(1);
                node.Mesh.MeshInvertedCounterArray.nOffset = nPosition - 12;
                WriteNumber(&node.Mesh.MeshInvertedCounterArray.nOffset, 0, "", &PHnOffsetToInvertedCounter);
                WriteNumber(&node.Mesh.nMeshInvertedCounter, 4, sMeshData + " > Data > Mesh > Inverted Counter");
                MarkDataBorder(nPosition - 1);

                const unsigned int nVertIndicesLocationToWrite = nPosition - 12;
                WriteNumber(&nVertIndicesLocationToWrite, 0, "", &PHnPointerToIndexLocation);
                for(unsigned d = 0; d < node.Mesh.VertIndices.size(); d++){
                    WriteNumber(&node.Mesh.VertIndices.at(d).at(0), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
                    WriteNumber(&node.Mesh.VertIndices.at(d).at(1), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
                    WriteNumber(&node.Mesh.VertIndices.at(d).at(2), 5, sMeshData + " > Data > Mesh > Vert Indices > Face " + std::to_string(d));
                    MarkDataBorder(nPosition - 1);
                }
            }
        }
    }

    /// LIGHT DATA
    if(node.Head.nType &NODE_LIGHT){
        node.Light.FlareTextureNameArray.ResetToSize(node.Light.FlareTextureNames.size());
        if(node.Light.FlareTextureNames.size() > 0){
            node.Light.FlareTextureNameArray.nOffset = nPosition - 12;
            WriteNumber(&node.Light.FlareTextureNameArray.nOffset, 0, "", &PHnOffsetToFlareTextureNames);
        }
        else{
            node.Light.FlareTextureNameArray.nOffset = 0;
            WriteNumber(&node.Light.FlareTextureNameArray.nOffset, 0, "", &PHnOffsetToFlareTextureNames);
        }

        std::vector<unsigned> PHnTextureNameOffset;
        for(unsigned d = 0; d < node.Light.FlareTextureNames.size(); d++){
            PHnTextureNameOffset.push_back(WriteBytes(placeholder, 4, 6, sLight));
            MarkDataBorder(nPosition - 1);
        }
        for(unsigned d = 0; d < node.Light.FlareTextureNames.size(); d++){
            node.Light.FlareTextureNames.at(d).nOffset = nPosition - 12;
            WriteNumber(&node.Light.FlareTextureNames.at(d).nOffset, 0, "", &PHnTextureNameOffset.at(d));
            WriteString(&node.Light.FlareTextureNames.at(d).sName, 0, 3, sLight);
            MarkDataBorder(nPosition - 1);
        }

        if(node.Light.FlareSizes.size() > 0){
            node.Light.FlareSizeArray.nOffset = nPosition - 12;
            WriteNumber(&node.Light.FlareSizeArray.nOffset, 0, "", &PHnOffsetToFlareSizes);
        }
        else{
            node.Light.FlareSizeArray.nOffset = 0;
            WriteNumber(&node.Light.FlareSizeArray.nOffset, 0, "", &PHnOffsetToFlareSizes);
        }

        for(unsigned d = 0; d < node.Light.FlareSizes.size(); d++){
            WriteFloat(&node.Light.FlareSizes.at(d), 2, sLight);
            MarkDataBorder(nPosition - 1);
        }

        if(node.Light.FlarePositions.size() > 0){
            node.Light.FlarePositionArray.nOffset = nPosition - 12;
            WriteNumber(&node.Light.FlarePositionArray.nOffset, 0, "", &PHnOffsetToFlarePositions);
        }
        else{
            node.Light.FlarePositionArray.nOffset = 0;
            WriteNumber(&node.Light.FlarePositionArray.nOffset, 0, "", &PHnOffsetToFlarePositions);
        }

        for(unsigned d = 0; d < node.Light.FlarePositions.size(); d++){
            WriteFloat(&node.Light.FlarePositions.at(d), 2, sLight);
            MarkDataBorder(nPosition - 1);
        }

        if(node.Light.FlareColorShifts.size() > 0){
            node.Light.FlareColorShiftArray.nOffset = nPosition - 12;
            WriteNumber(&node.Light.FlareColorShiftArray.nOffset, 0, "", &PHnOffsetToFlareColorShifts);
        }
        else{
            node.Light.FlareColorShiftArray.nOffset = 0;
            WriteNumber(&node.Light.FlareColorShiftArray.nOffset, 0, "", &PHnOffsetToFlareColorShifts);
        }

        for(unsigned d = 0; d < node.Light.FlareColorShifts.size(); d++){
            WriteFloat(&node.Light.FlareColorShifts.at(d).fR, 2, sLight);
            WriteFloat(&node.Light.FlareColorShifts.at(d).fG, 2, sLight);
            WriteFloat(&node.Light.FlareColorShifts.at(d).fB, 2, sLight);
            MarkDataBorder(nPosition - 1);
        }
    }
    /// DONE WITH SUBHEADERS

    //We get to the children array
    node.Head.ChildrenArray.ResetToSize(node.Head.ChildIndices.size());
    node.Head.ChildrenArray.nOffset = node.Head.ChildIndices.empty() ? 0u : nPosition - 12;
    WriteNumber(&node.Head.ChildrenArray.nOffset, 0, "", &PHnOffsetToChildren);

    std::vector<unsigned> PHnOffsetToChild;
    for(unsigned a = 0; a < node.Head.ChildIndices.size(); a++){
        std::string sChildPointer = sNode + (node.nAnimation.Valid() ? "Child Pointers > Pointer " : "Data > Child Array > Pointer ") + std::to_string(a);
        PHnOffsetToChild.push_back(WriteBytes(placeholder, 4, 6, sChildPointer));
        MarkDataBorder(nPosition - 1);
    }

    for(unsigned a = 0; a < node.Head.ChildIndices.size(); a++){
        int nNodeIndex = GetNodeIndexByNameIndex(node.Head.ChildIndices.at(a), node.nAnimation.Valid() ? static_cast<int>(node.nAnimation) : -1);
        if(nNodeIndex == -1) throw mdlexception("To binary exception: Couldn't find child node index by name index " + std::to_string(node.Head.ChildIndices.at(a)) + " in animation " + std::to_string(node.nAnimation) + ", node " + GetNodeName(node) + ".");
        Node * p_node = nullptr;
        if(!node.nAnimation.Valid()) p_node = &Data.MH.ArrayOfNodes.at(nNodeIndex);
        else p_node = &Data.MH.Animations.at(node.nAnimation).ArrayOfNodes.at(nNodeIndex);
        if(std::find(vWrittenNodes.begin(), vWrittenNodes.end(), p_node) == vWrittenNodes.end()){
            unsigned int nChildOffsetToWrite = nPosition - 12;
            WriteNumber(&nChildOffsetToWrite, 0, "", &PHnOffsetToChild.at(a));
            WriteNodes(*p_node);
            //WriteNodes(node.Head.Children.at(a));
        }
        else{
            unsigned int nChildOffsetToWrite = GetNodeWriteOffset(*p_node);
            WriteNumber(&nChildOffsetToWrite, 0, "", &PHnOffsetToChild.at(a));
        }
    }

    /// We get to the controller array
    node.Head.ControllerArray.ResetToSize(node.Head.Controllers.size());
    if(node.Head.Controllers.size() > 0){
        node.Head.ControllerArray.nOffset = nPosition-12;
        WriteNumber(&node.Head.ControllerArray.nOffset, 0, "", &PHnOffsetToControllers);
    }
    else{
        node.Head.ControllerArray.nOffset = 0;
        WriteNumber(&node.Head.ControllerArray.nOffset, 0, "", &PHnOffsetToControllers);
    }

    for(unsigned a = 0; a < node.Head.Controllers.size(); a++){
        Controller &ctrl = node.Head.Controllers.at(a);
        std::string sController = sNode + (node.nAnimation.Valid() ? "Controllers > Controller " : "Data > Controllers > Controller ") + std::to_string(a);
        WriteNumber(&ctrl.nControllerType, 4, sController);
        WriteNumber(&ctrl.nUnknown2, 10, sController);
        WriteNumber(&ctrl.nValueCount, 5, sController);
        WriteNumber(&ctrl.nTimekeyStart, 5, sController);
        WriteNumber(&ctrl.nDataStart, 5, sController);
        WriteNumber(&ctrl.nColumnCount, 7, sController);
        WriteNumber(&ctrl.nPadding[0], 11, sController);
        WriteNumber(&ctrl.nPadding[1], 11, sController);
        WriteNumber(&ctrl.nPadding[2], 11, sController);
        MarkDataBorder(nPosition - 1);
    }

    /// We get to the controller data array
    node.Head.ControllerDataArray.ResetToSize(node.Head.ControllerData.size());
    if(node.Head.ControllerData.size() > 0){
        node.Head.ControllerDataArray.nOffset = nPosition - 12;
        WriteNumber(&node.Head.ControllerDataArray.nOffset, 0, "", &PHnOffsetToControllerData);
    }
    else{
        node.Head.ControllerDataArray.nOffset = 0;
        WriteNumber(&node.Head.ControllerDataArray.nOffset, 0, "", &PHnOffsetToControllerData);
    }

    for(unsigned a = 0; a < node.Head.ControllerData.size(); a++){
        std::string sControllerData = sNode + (node.nAnimation.Valid() ? "Controller Data > Float " : "Data > Controller Data > Float ") + std::to_string(a);
        WriteFloat(&node.Head.ControllerData.at(a), 2, sControllerData);
        MarkDataBorder(nPosition - 1);
    }

    /// Done you are, padawan
}

void BWM::Compile(){
    if(!Bwm) return;
    BinaryFileRollbackGuard compileRollback;
    compileRollback.Track(this);
    BWMHeader &data = *Bwm;
    ValidateBwmForWrite(data);
    nPosition = 0;
    sBuffer.resize(0);
    bKnown.resize(0);
    bDataLoaded = true;
    std::string sHeader ("Header > ");

    std::string sFileType = "BWM ";
    std::string sVersion = "V1.0";
    WriteString(&sFileType, 4, 3, sHeader + "Version");
    WriteString(&sVersion, 4, 3, sHeader + "Version");
    WriteNumber(&data.nType, 4, sHeader + "Walkmesh Type");

    WriteFloat(&data.vUse1.fX, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vUse1.fY, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vUse1.fZ, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vUse2.fX, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vUse2.fY, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vUse2.fZ, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk1.fX, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk1.fY, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk1.fZ, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk2.fX, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk2.fY, 2, sHeader + "Use Hooks");
    WriteFloat(&data.vDwk2.fZ, 2, sHeader + "Use Hooks");

    WriteFloat(&data.vPosition.fX, 2, sHeader + "Position");
    WriteFloat(&data.vPosition.fY, 2, sHeader + "Position");
    WriteFloat(&data.vPosition.fZ, 2, sHeader + "Position");

    unsigned int nNumberOfVertsToWrite = CheckedUIntCount(data.verts.size(), "walkmesh vertex count");
    WriteNumber(&nNumberOfVertsToWrite, 1, sHeader + "Number of Vertices");
    unsigned nOffsetVerts = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Vertex Coordinates");

    unsigned int nNumberOfFacesToWrite = CheckedUIntCount(data.faces.size(), "walkmesh face count");
    WriteNumber(&nNumberOfFacesToWrite, 1, sHeader + "Number of Faces");
    unsigned nOffsetVertIndices = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Vertex Indices");
    unsigned nOffsetMaterials = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Material IDs");
    unsigned nOffsetNormals = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Face Normals");
    unsigned nOffsetDistances = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Plane Distances");

    unsigned int nNumberOfAabbToWrite = CheckedUIntCount(data.aabb.size(), "walkmesh AABB count");
    WriteNumber(&nNumberOfAabbToWrite, 1, sHeader + "Number of Aabb Structs");
    unsigned nOffsetAabb = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Aabb Structs");

    WriteNumber(&data.nPadding, 11, sHeader + "Unknown");

    std::size_t nCount = 0;
    if(data.nType == 1){
        for(unsigned f = 0; f < data.faces.size(); f++){
            if(IsMaterialWalkable(data.faces.at(f).nMaterialID)) nCount++;
        }
    }
    unsigned int nNumberOfAdjacentFacesToWrite = (data.nType == 1)
        ? CheckedUIntCount(nCount, "walkmesh adjacent-face count")
        : 0u;
    WriteNumber(&nNumberOfAdjacentFacesToWrite, 1, sHeader + "Number of Adjacent Edges");
    unsigned nOffsetAdjacent = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Adjacent Edges");

    unsigned int nNumberOfEdgesToWrite = (data.nType == 1)
        ? CheckedUIntCount(data.edges.size(), "walkmesh edge count")
        : 0u;
    WriteNumber(&nNumberOfEdgesToWrite, 1, sHeader + "Number of Outer Edges");
    unsigned nOffsetEdges = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Outer Edges");

    unsigned int nNumberOfPerimetersToWrite = (data.nType == 1)
        ? CheckedUIntCount(data.perimeters.size(), "walkmesh perimeter count")
        : 0u;
    WriteNumber(&nNumberOfPerimetersToWrite, 1, sHeader + "Number of Perimeters");
    unsigned nOffsetPerimeters = WriteBytes(placeholder, 4, 6, sHeader + "Offset to Perimeters");
    MarkDataBorder(nPosition - 1);

    unsigned int nOffsetToVertsToWrite = (data.verts.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToVertsToWrite, 0, "", &nOffsetVerts);
    for(unsigned v = 0; v < data.verts.size(); v++){
        Vector &vert = data.verts.at(v);
        WriteFloat(&vert.fX, 2, "Vertices > Vertex " + std::to_string(v));
        WriteFloat(&vert.fY, 2, "Vertices > Vertex " + std::to_string(v));
        WriteFloat(&vert.fZ, 2, "Vertices > Vertex " + std::to_string(v));
        MarkDataBorder(nPosition - 1);
    }

    unsigned int nOffsetToIndicesToWrite = (data.faces.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToIndicesToWrite, 0, "", &nOffsetVertIndices);
    for(unsigned f = 0; f < data.faces.size(); f++){
        Face & face = data.faces.at(f);

        for(int i = 0; i < 3; i++){
            int nTempIndex = face.nIndexVertex.at(i).Valid() ? static_cast<int>(face.nIndexVertex.at(i)) : -1;
            WriteNumber(&nTempIndex, 4, "Vertex Indices > Face " + std::to_string(f) + " > Index " + std::to_string(i));
        }

        MarkDataBorder(nPosition - 1);
    }
    unsigned int nOffsetToMaterialsToWrite = (data.faces.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToMaterialsToWrite, 0, "", &nOffsetMaterials);
    for(unsigned f = 0; f < data.faces.size(); f++){
        Face &face = data.faces.at(f);
        WriteNumber(&face.nMaterialID, 4, "Materials > Material " + std::to_string(f));
        MarkDataBorder(nPosition - 1);
    }
    unsigned int nOffsetToNormalsToWrite = (data.faces.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToNormalsToWrite, 0, "", &nOffsetNormals);
    for(unsigned f = 0; f < data.faces.size(); f++){
        Face &face = data.faces.at(f);
        WriteFloat(&face.vNormal.fX, 2, "Face Normals > Normal " + std::to_string(f));
        WriteFloat(&face.vNormal.fY, 2, "Face Normals > Normal " + std::to_string(f));
        WriteFloat(&face.vNormal.fZ, 2, "Face Normals > Normal " + std::to_string(f));
        MarkDataBorder(nPosition - 1);
    }
    unsigned int nOffsetToDistancesToWrite = (data.faces.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToDistancesToWrite, 0, "", &nOffsetDistances);
    for(unsigned f = 0; f < data.faces.size(); f++){
        Face &face = data.faces.at(f);
        WriteFloat(&face.fDistance, 2, "Plane Distances > Distance " + std::to_string(f));
        MarkDataBorder(nPosition - 1);
    }

    unsigned int nOffsetToAabbToWrite = (data.aabb.size() == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToAabbToWrite, 0, "", &nOffsetAabb);
    for(unsigned v = 0; v < data.aabb.size(); v++){
        Aabb &aabb = data.aabb.at(v);
        WriteFloat(&aabb.vBBmin.fX, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteFloat(&aabb.vBBmin.fY, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteFloat(&aabb.vBBmin.fZ, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteFloat(&aabb.vBBmax.fX, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteFloat(&aabb.vBBmax.fY, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteFloat(&aabb.vBBmax.fZ, 2, "Aabb Tree > Aabb Struct " + std::to_string(v));
        int nTempIndex = aabb.nID.Valid() ? static_cast<signed int>(aabb.nID) : -1;
        WriteNumber(&nTempIndex, 4, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteNumber(&aabb.nExtra, 4, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteNumber(&aabb.nProperty, 4, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteNumber(&aabb.nChild1, 4, "Aabb Tree > Aabb Struct " + std::to_string(v));
        WriteNumber(&aabb.nChild2, 4, "Aabb Tree > Aabb Struct " + std::to_string(v));
        MarkDataBorder(nPosition - 1);
    }

    unsigned int nOffsetToAdjacentFacesToWrite = (nNumberOfAdjacentFacesToWrite == 0 || data.nType == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToAdjacentFacesToWrite, 0, "", &nOffsetAdjacent);
    for(int f = 0; f < static_cast<int>(data.faces.size()) &&data.nType == 1; f++){
        Face &face = data.faces.at(f);
        if(IsMaterialWalkable(face.nMaterialID)){
            for(int i = 0; i < 3; i++){
                int nTempIndex = face.nAdjacentFaces.at(i).Valid() ? static_cast<int>(face.nAdjacentFaces.at(i)) : -1;
                WriteNumber(&nTempIndex, 4, "Adjacent Faces > Face " + std::to_string(f) + " > Edge " + std::to_string(i));
            }
            MarkDataBorder(nPosition - 1);
        }
    }
    unsigned int nOffsetToEdgesToWrite = (data.edges.size() == 0 || data.nType == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToEdgesToWrite, 0, "", &nOffsetEdges);
    for(unsigned f = 0; f < data.edges.size() && data.nType == 1; f++){
        Edge & edge = data.edges.at(f);
        WriteNumber(&edge.nIndex, 4, "Edges > Edge " + std::to_string(f));
        WriteNumber(&edge.nTransition, 4, "Edges > Edge " + std::to_string(f));
        MarkDataBorder(nPosition - 1);
    }
    unsigned int nOffsetToPerimetersToWrite = (data.perimeters.size() == 0 || data.nType == 0) ? 0u : nPosition;
    WriteNumber(&nOffsetToPerimetersToWrite, 0, "", &nOffsetPerimeters);
    for(unsigned f = 0; f < data.perimeters.size() && data.nType == 1; f++){
        unsigned & perimeter = data.perimeters.at(f).nPerimeter;
        WriteNumber(&perimeter, 4, "Perimeters > Perimeter " + std::to_string(f));
        MarkDataBorder(nPosition - 1);
    }

    compileRollback.Commit();
}
