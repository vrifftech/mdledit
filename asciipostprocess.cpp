#include "MDL.h"
#include "embedded_supermodels.h"
#include <algorithm>
#include <cmath>
#include "shlwapi.h"

namespace {
    bool BuildPreservedAabbRecursive(const std::vector<Aabb> & linear, unsigned int & cursor, Aabb & out){
        if(cursor >= linear.size()) return false;

        out = linear.at(cursor++);
        const bool bHasChild1 = out.nChild1.Valid() && out.nChild1 > 0;
        const bool bHasChild2 = out.nChild2.Valid() && out.nChild2 > 0;

        // nChild1/nChild2 are temporary child-presence flags when read from ASCII.
        // WriteAabb() regenerates real offsets from the Child vectors.
        out.nChild1 = 0;
        out.nChild2 = 0;
        out.Child1.clear();
        out.Child2.clear();

        if(bHasChild1){
            out.Child1.resize(1);
            if(!BuildPreservedAabbRecursive(linear, cursor, out.Child1.front())) return false;
        }
        if(bHasChild2){
            out.Child2.resize(1);
            if(!BuildPreservedAabbRecursive(linear, cursor, out.Child2.front())) return false;
        }
        return true;
    }

    Node * FindAsciiRootNode(std::vector<Node> & nodes, const std::string & sContext){
        Node * pRoot = nullptr;
        for(Node & node : nodes){
            if(node.Head.nType == 0 || !node.Head.nNameIndex.Valid()) continue;
            if(!node.Head.nParentIndex.Valid()){
                if(pRoot != nullptr){
                    throw mdlexception("ASCII post-processing found multiple root nodes in " + sContext + "; refusing to choose one and orphan the rest.");
                }
                pRoot = &node;
            }
        }
        return pRoot;
    }

    bool ContainsNameIndex(const std::vector<unsigned short> & values, unsigned short value){
        return std::find(values.begin(), values.end(), value) != values.end();
    }


    unsigned short CheckedAsciiVertexIndex(std::size_t index, const std::string & sContext){
        const std::size_t nInvalid = static_cast<std::size_t>(std::numeric_limits<unsigned short>::max());
        if(index >= nInvalid){
            throw mdlexception(sContext + " would require a vertex index outside the 16-bit MDL range.");
        }
        return static_cast<unsigned short>(index);
    }

    int FindAsciiNodeIndexByNameIndex(ModelHeader & header, unsigned short nNameIndex){
        for(std::size_t i = 0; i < header.ArrayOfNodes.size(); ++i){
            Node & node = header.ArrayOfNodes.at(i);
            if(node.Head.nNameIndex.Valid() && static_cast<unsigned short>(node.Head.nNameIndex) == nNameIndex){
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    Node & GetAsciiNodeByNameIndex(ModelHeader & header, unsigned short nNameIndex, const std::string & sContext){
        const int nIndex = FindAsciiNodeIndexByNameIndex(header, nNameIndex);
        if(nIndex < 0){
            throw mdlexception(sContext + " references node name index " + std::to_string(nNameIndex) + ", but no geometry node with that name index exists.");
        }
        return header.ArrayOfNodes.at(static_cast<unsigned int>(nIndex));
    }

    unsigned short CheckedAsciiNameIndex(ModelHeader & header, MdlInteger<unsigned short> nNameIndex, const std::string & sContext){
        if(!nNameIndex.Valid()){
            throw mdlexception(sContext + " has an invalid node name index.");
        }
        const unsigned short nIndex = static_cast<unsigned short>(nNameIndex);
        if(nIndex >= header.Names.size()){
            throw mdlexception(sContext + " references node name index " + std::to_string(nIndex) + ", which is outside the name table.");
        }
        return nIndex;
    }

    void GatherChildrenRecursive(Node & node,
                                 std::vector<Node> & arrayOfNodes,
                                 Vector vFromRoot,
                                 std::vector<unsigned short> & active,
                                 std::vector<unsigned short> & visited){
        if(!node.Head.nNameIndex.Valid()){
            throw mdlexception("GatherChildren() error: node has an invalid name index.");
        }

        const unsigned short nNameIndex = static_cast<unsigned short>(node.Head.nNameIndex);
        if(ContainsNameIndex(active, nNameIndex)){
            throw mdlexception("GatherChildren() error: cyclic node hierarchy detected while collecting children.");
        }
        if(ContainsNameIndex(visited, nNameIndex)){
            throw mdlexception("GatherChildren() error: hierarchy reaches the same node more than once.");
        }

        active.push_back(nNameIndex);

        /// Propagate only the Vector from the root through recursion. The node's
        /// own static orientation rotates the incoming offset before the node
        /// position is added.
        if(node.Head.nType & NODE_MESH){
            Location location = node.GetLocation();
            Quaternion qNode = location.oOrientation.GetQuaternion();
            vFromRoot.Rotate(qNode);
            vFromRoot += location.vPosition;
            node.Head.vFromRoot = vFromRoot;
        }

        std::vector<MdlInteger<unsigned short>> childIndices;
        childIndices.reserve(arrayOfNodes.size());
        for(Node & child : arrayOfNodes){
            if(child.Head.nNameIndex.Valid() && child.Head.nParentIndex == node.Head.nNameIndex){
                childIndices.push_back(child.Head.nNameIndex);
                GatherChildrenRecursive(child, arrayOfNodes, vFromRoot, active, visited);
            }
        }

        childIndices.shrink_to_fit();
        node.Head.ChildIndices = std::move(childIndices);
        active.pop_back();
        visited.push_back(nNameIndex);
    }
}

/*
    Functions:
    GetNormal()
    FindThirdIndex()
    MDL::GatherChildren()
    MDL::GetSupernodes()
    MDL::AsciiPostProcess()
*/



namespace {
inline void CalculateWorld(MDL& mdl, FileHeader& data, Patch& patch, bool bNormal, bool bTangent){
    if(!bNormal && !bTangent) return;
    Node& patch_node = data.MH.ArrayOfNodes.at(static_cast<unsigned int>(patch.nNodeArrayIndex));

    for(unsigned int face_ind : patch.FaceIndices){
        Face& face = patch_node.Mesh.Faces.at(face_ind);
        Vertex& v1 = patch_node.Mesh.Vertices.at(face.nIndexVertex[0]);
        Vertex& v2 = patch_node.Mesh.Vertices.at(face.nIndexVertex[1]);
        Vertex& v3 = patch_node.Mesh.Vertices.at(face.nIndexVertex[2]);

        Vector Edge1 = v2.vFromRoot - v1.vFromRoot;
        Vector Edge2 = v3.vFromRoot - v1.vFromRoot;
        Vector Edge3 = v3.vFromRoot - v2.vFromRoot;

        if(face.fAreaUV == 0.0) patch.bBadUV = true;
        if(face.fArea <= 0.0) patch.bBadGeo = true;

        Vector vAdd = cross(Edge1, Edge2);
        vAdd.Normalize();

        if(mdl.bSmoothAreaWeighting) vAdd *= (face.fArea > 0.000001 ? face.fArea : 0.0);

        if(mdl.bSmoothAngleWeighting){
            if(patch.nVertex == face.nIndexVertex[0]) vAdd *= Angle(Edge1, Edge2);
            else if(patch.nVertex == face.nIndexVertex[1]) vAdd *= Angle(Edge1, Edge3);
            else if(patch.nVertex == face.nIndexVertex[2]) vAdd *= Angle(Edge2, Edge3);
        }

        if(mdl.bCreaseAngle && Angle(patch.vWorldNormal, vAdd) > static_cast<double>(mdl.nCreaseAngle)) continue;

        if(bNormal) patch.vWorldNormal += vAdd;

        vAdd.Normalize();

        if(bTangent){
            Vector& UVvert1 = v1.MDXData.vUV1;
            Vector& UVvert2 = v2.MDXData.vUV1;
            Vector& UVvert3 = v3.MDXData.vUV1;
            Vector UVedge1 = UVvert2 - UVvert1;
            Vector UVedge2 = UVvert3 - UVvert1;

            double r = (UVedge1.fX * UVedge2.fY - UVedge1.fY * UVedge2.fX);
            if(r != 0.0) r = 1.0 / r;
            else r = 2406.6388;

            Vector vAddT = r * (Edge1 * UVedge2.fY - Edge2 * UVedge1.fY);
            Vector vAddB = r * (Edge2 * UVedge1.fX - Edge1 * UVedge2.fX);

            vAddT.Normalize();
            vAddB.Normalize();
            if(vAddT.Null()) vAddT = Vector(1.0, 0.0, 0.0);
            if(vAddB.Null()) vAddB = Vector(1.0, 0.0, 0.0);

            Vector vCross = cross(vAdd, vAddT);
            double fDot = dot(vCross, vAddB);
            if(fDot > 0.0000000001) vAddT *= -1.0;

            Vector vNormalUV = cross(UVedge1, UVedge2);
            if(vNormalUV.fZ < 0.0){
                vAddT *= -1.0;
                vAddB *= -1.0;
            }

            patch.vWorldT += vAddT;
            patch.vWorldB += vAddB;
            patch.vWorldN += cross(vAddB, vAddT);
        }
    }
}

inline void CalculateVertex(MDL& mdl, FileHeader& data, Patch& patch, bool bNormal, bool bTangent, std::vector<MdlInteger<unsigned int>>* patches = nullptr){
    (void)mdl;
    if(!bNormal && !bTangent) return;
    if(!patches) patches = &patch.SmoothedPatches;

    std::vector<Patch>& patch_group = data.MH.PatchArrayPointers.at(static_cast<unsigned int>(patch.nPatchGroup));
    std::vector<MdlInteger<unsigned int>>& smoothed_patches = *patches;

    Vector vWorking, vWorkingTST, vWorkingTSB, vWorkingTSN;
    patch.bGroupBadGeo = false;
    patch.bGroupBadUV = false;

    std::vector<int> CheckedPatches;
    CheckedPatches.reserve(smoothed_patches.size());
    for(int n = 0; n < static_cast<int>(smoothed_patches.size()); n++){
        Patch& curpatch = patch_group.at(static_cast<unsigned int>(smoothed_patches.at(n)));
        if(std::find(CheckedPatches.begin(), CheckedPatches.end(), curpatch.nNodeArrayIndex) != CheckedPatches.end()) continue;

        CheckedPatches.push_back(curpatch.nNodeArrayIndex);

        int nNormals = 0;
        Vector vMeshNormal, vMeshTST, vMeshTSB, vMeshTSN;

        for(int n2 = n; n2 < static_cast<int>(smoothed_patches.size()); n2++){
            Patch& curpatch2 = patch_group.at(static_cast<unsigned int>(smoothed_patches.at(n2)));
            if(curpatch.nNodeArrayIndex != curpatch2.nNodeArrayIndex) continue;

            if(!patch.bGroupBadGeo && curpatch2.bBadGeo) patch.bGroupBadGeo = true;
            if(!patch.bGroupBadUV && curpatch2.bBadUV) patch.bGroupBadUV = true;

            nNormals++;
            if(bNormal) vMeshNormal += curpatch2.vWorldNormal;
            if(bTangent) vMeshTSB += curpatch2.vWorldB;
            if(bTangent) vMeshTST += curpatch2.vWorldT;
            if(bTangent) vMeshTSN += curpatch2.vWorldN;
        }

        double fMeshNormalLength = vMeshNormal.GetLength();
        double fMeshTSBLength = vMeshTSB.GetLength();
        double fMeshTSTLength = vMeshTST.GetLength();
        double fMeshTSNLength = vMeshTSN.GetLength();
        if(bNormal && fMeshNormalLength > 0.000000000001 && std::isfinite(fMeshNormalLength)) vWorking += (nNormals * vMeshNormal / fMeshNormalLength);
        if(bTangent && fMeshTSBLength > 0.000000000001 && std::isfinite(fMeshTSBLength)) vWorkingTSB += (vMeshTSB / fMeshTSBLength);
        if(bTangent && fMeshTSTLength > 0.000000000001 && std::isfinite(fMeshTSTLength)) vWorkingTST += (vMeshTST / fMeshTSTLength);
        if(bTangent && fMeshTSNLength > 0.000000000001 && std::isfinite(fMeshTSNLength)) vWorkingTSN += (vMeshTSN / fMeshTSNLength);
    }

    if(bNormal) vWorking.Normalize();
    if(bTangent) vWorkingTSB.Normalize();
    if(bTangent) vWorkingTST.Normalize();
    if(bTangent) vWorkingTSN.Normalize();

    if(bNormal) patch.vVertexNormal = vWorking;
    if(bTangent) patch.vVertexB = vWorkingTSB;
    if(bTangent) patch.vVertexT = vWorkingTST;
    if(bTangent) patch.vVertexN = vWorkingTSN;
}


}

/// Utility function, gets triangular face normalized normal from vert vectors
Vector GetNormal(Vector v1, Vector v2, Vector v3){
    Vector vNormal = (v2 - v1) / (v3 - v1);
    vNormal.Normalize();
    return vNormal;
}

/// Utility function for mesh->saber conversion
/// Loops through the faces, until it finds one where both of the vert indices exits, then it returns the third index (unless it's being ignored), otherwise -1
int FindThirdIndex(const std::vector<Face> & faces, int ind1, int ind2, int ignore = -1){
    for(int f = 0; f < static_cast<int>(faces.size()); f++){
        const Face & face = faces.at(f);
        int nFound = 0;
        for(int i = 0; i < 3; i++){
            if(face.nIndexVertex.at(i) == ind1 || face.nIndexVertex.at(i) == ind2) nFound++;
        }
        if(nFound == 2){
            for(int i = 0; i < 3; i++){
                if(face.nIndexVertex.at(i) != ind1 && face.nIndexVertex.at(i) != ind2 && face.nIndexVertex.at(i) != ignore) return face.nIndexVertex.at(i);
            }
        }
    }
    return -1;
}

void MDL::GatherChildren(Node & node, std::vector<Node> & ArrayOfNodes, Vector vFromRoot){
    std::vector<unsigned short> active;
    std::vector<unsigned short> visited;
    active.reserve(ArrayOfNodes.size());
    visited.reserve(ArrayOfNodes.size());
    GatherChildrenRecursive(node, ArrayOfNodes, vFromRoot, active, visited);
}
void GetSupernodes(ModelHeader & MH, ModelHeader & superMH, int & nHighest, int & nTotalSupermodelNodes, int nNodeCurrentNameIndex, int nSupernodeCurrentNameIndex){
    if(nNodeCurrentNameIndex < 0) return;

    Node & node = GetAsciiNodeByNameIndex(MH,
        static_cast<unsigned short>(nNodeCurrentNameIndex),
        "GetSupernodes()");

    if(nSupernodeCurrentNameIndex == -1){
        if(nHighest < 0 || nHighest > std::numeric_limits<unsigned short>::max()){
            throw mdlexception("GetSupernodes() generated a supernode number outside the 16-bit node field range.");
        }
        node.Head.nSupernodeNumber = static_cast<unsigned short>(nHighest);
        nHighest++;
        for(auto childNameIndex : node.Head.ChildIndices){
            const unsigned short nChildNameIndex = CheckedAsciiNameIndex(MH, childNameIndex, "GetSupernodes() child");
            GetSupernodes(MH, superMH, nHighest, nTotalSupermodelNodes, nChildNameIndex, -2);
        }
    }
    else if(nSupernodeCurrentNameIndex == -2){
        /// Question: I am adding this to an already existing value in this variable.
        /// It is probably the name index, but is it possible that it actually has to be the node index?
        const unsigned int nBaseSupernode = static_cast<unsigned short>(node.Head.nSupernodeNumber);
        const unsigned int nAdjustedSupernode = nBaseSupernode + static_cast<unsigned int>(nTotalSupermodelNodes) + 1u;
        if(nTotalSupermodelNodes < 0 || nAdjustedSupernode > std::numeric_limits<unsigned short>::max()){
            throw mdlexception("GetSupernodes() adjusted a supernode number outside the 16-bit node field range.");
        }
        node.Head.nSupernodeNumber = static_cast<unsigned short>(nAdjustedSupernode);
        for(auto childNameIndex : node.Head.ChildIndices){
            const unsigned short nChildNameIndex = CheckedAsciiNameIndex(MH, childNameIndex, "GetSupernodes() child");
            GetSupernodes(MH, superMH, nHighest, nTotalSupermodelNodes, nChildNameIndex, -2);
        }
    }
    else{
        Node & supernode = GetAsciiNodeByNameIndex(superMH,
            static_cast<unsigned short>(nSupernodeCurrentNameIndex),
            "GetSupernodes() supermodel");
        node.Head.nSupernodeNumber = supernode.Head.nSupernodeNumber;
        for(auto childNameIndex : node.Head.ChildIndices){
            const unsigned short nChildNameIndex = CheckedAsciiNameIndex(MH, childNameIndex, "GetSupernodes() child");
            bool bFound = false;
            std::string sNodeName = MH.Names.at(nChildNameIndex).sName.c_str();
            ToLowerInPlace(sNodeName);
            for(auto superChildNameIndexValue : supernode.Head.ChildIndices){
                if(bFound) break;
                const unsigned short nSuperChildNameIndex = CheckedAsciiNameIndex(superMH, superChildNameIndexValue, "GetSupernodes() supermodel child");
                std::string sSupernodeName = superMH.Names.at(nSuperChildNameIndex).sName.c_str();
                ToLowerInPlace(sSupernodeName);
                if(sNodeName == sSupernodeName){
                    bFound = true;
                    GetSupernodes(MH, superMH, nHighest, nTotalSupermodelNodes, nChildNameIndex, nSuperChildNameIndex);
                }
            }
            if(!bFound) GetSupernodes(MH, superMH, nHighest, nTotalSupermodelNodes, nChildNameIndex, -1);
        }
    }
}

void MDL::AsciiPostProcess(std::vector<std::string> & sBumpmapped){
    ReportObject ReportMdl(*this);
    ReportMdl << "Ascii post-processing...\n";
    Report("Post-processing imported ASCII...");
    FileHeader & Data = *FH;

    /// PART 0 ///
    /// Get rid of the duplication marks
    for(int n = 0; n < static_cast<int>(Data.MH.Names.size()); n++){
        std::string & sNode = Data.MH.Names.at(n).sName;
        if(sNode.find("__dpl") != std::string::npos){
            sNode.resize(sNode.find("__dpl"));
        }
    }
    /// Implementation of 'bumpmapped_texture': applies bumpmaps
    for(int s = 0; s < static_cast<int>(sBumpmapped.size()); s++){
        for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
            //ReportMdl << "Checking node\n";
            Node & node = Data.MH.ArrayOfNodes.at(n);
            if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_AABB) && !(node.Head.nType & NODE_SABER)){
                if(std::string(node.Mesh.cTexture1.c_str()) != "" && std::string(node.Mesh.cTexture1.c_str()) != "NULL"){
                    if(StringEqual(sBumpmapped.at(s).c_str(), node.Mesh.cTexture1.c_str(), true)){
                        node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap | MDX_FLAG_TANGENT1;
                    }
                }
                if(node.Mesh.cTexture2.c_str() != std::string("") && node.Mesh.cTexture2.c_str() != std::string("NULL")){
                    if(StringEqual(sBumpmapped.at(s).c_str(), node.Mesh.cTexture2.c_str(), true)){
                        node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap | MDX_FLAG_TANGENT2;
                    }
                }
                if(node.Mesh.cTexture3.c_str() != std::string("") && node.Mesh.cTexture3.c_str() != std::string("NULL")){
                    if(StringEqual(sBumpmapped.at(s).c_str(), node.Mesh.cTexture3.c_str(), true)){
                        node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap | MDX_FLAG_TANGENT3;
                    }
                }
                if(node.Mesh.cTexture4.c_str() != std::string("") && node.Mesh.cTexture4.c_str() != std::string("NULL")){
                    if(StringEqual(sBumpmapped.at(s).c_str(), node.Mesh.cTexture4.c_str(), true)){
                        node.Mesh.nMdxDataBitmap = node.Mesh.nMdxDataBitmap | MDX_FLAG_TANGENT4;
                    }
                }
            }
        }
    }

    /// PART 1 ///
    /// Gather all the children (the indices!!!)
    /// This part means going from every node only specifying its parent to every node also specifying its children
    // 1. Gather children for animations
    for(Animation & anim : Data.MH.Animations){
        Node * pRoot = FindAsciiRootNode(anim.ArrayOfNodes, "animation '" + std::string(anim.sName.c_str()) + "'");
        if(pRoot == nullptr){
            if(!anim.ArrayOfNodes.empty()){
                throw mdlexception("ASCII post-processing could not find a root node for animation '" + std::string(anim.sName.c_str()) + "'.");
            }
        }
        else GatherChildren(*pRoot, anim.ArrayOfNodes, Vector());
    }
    // 2. Gather children for geometry
    Node * pGeometryRoot = FindAsciiRootNode(Data.MH.ArrayOfNodes, "geometry");
    if(pGeometryRoot == nullptr){
        throw mdlexception("ASCII post-processing could not find a geometry root node.");
    }
    GatherChildren(*pGeometryRoot, Data.MH.ArrayOfNodes, Vector());

    /// PART 2 ///
    /// Do supernodes
    /// This loads up all the supermodels and calculates the supernode numbers
    if(!Data.MH.bPreserveTotalNumberOfNodes){
        Data.MH.GH.nTotalNumberOfNodes = Data.MH.nNodeCount;
    }
    nSupermodel = 0; // As far as we're concerned, supermodel not loaded (yet).
    if(Data.MH.cSupermodelName != "NULL" && Data.MH.cSupermodelName != ""){
        std::unique_ptr<MDL> Supermodel;
        ModelHeader embeddedSupermodelHeader;
        ModelHeader * pSupermodelHeader = nullptr;

        // A correct neighboring binary remains authoritative, which preserves
        // support for custom or modified supermodels. For the stock K1/K2
        // humanoid supermodels, suppress the missing-file dialog because a
        // generated metadata-only fallback is available.
        const bool bEmbeddedAvailable = HasEmbeddedSupermodel(bK2, Data.MH.cSupermodelName);
        LoadSupermodel(*this, Supermodel, !bEmbeddedAvailable);
        if(Supermodel){
            pSupermodelHeader = &Supermodel->GetFileData()->MH;
            ReportMdl << "Using neighboring binary supermodel metadata.\n";
        }
        else if(BuildEmbeddedSupermodel(bK2, Data.MH.cSupermodelName, embeddedSupermodelHeader)){
            pSupermodelHeader = &embeddedSupermodelHeader;
            ReportMdl << "Using embedded " << (bK2 ? "K2" : "K1")
                      << " stock supermodel metadata for "
                      << Data.MH.cSupermodelName << ".\n";
        }

        // First, update the TotalNodeCount.
        if(pSupermodelHeader != nullptr){
            /// Supernode metadata loaded, record its status.
            nSupermodel = bK2 ? 2 : 1;

            int nTotalSupermodelNodes = static_cast<int>(pSupermodelHeader->GH.nTotalNumberOfNodes);
            ReportMdl << "Total Supermodel Nodes: " << nTotalSupermodelNodes << "\n";
            if(nTotalSupermodelNodes > 0 && !Data.MH.bPreserveTotalNumberOfNodes)
                Data.MH.GH.nTotalNumberOfNodes += 1 + nTotalSupermodelNodes;

            // Next we need the largest supernode number.
            int nMaxSupernode = 0;
            for(const Node & supermodelNode : pSupermodelHeader->ArrayOfNodes){
                nMaxSupernode = std::max(nMaxSupernode,
                                         static_cast<int>(supermodelNode.Head.nSupernodeNumber));
            }
            int nCurrentSupernode = nMaxSupernode + 1;

            Node * pSuperRoot = FindAsciiRootNode(
                pSupermodelHeader->ArrayOfNodes,
                "supermodel '" + pSupermodelHeader->GH.sName + "'");
            if(pSuperRoot == nullptr){
                throw mdlexception("ASCII post-processing could not find a root node in supermodel '" +
                                   pSupermodelHeader->GH.sName + "'.");
            }
            const unsigned short nGeometryRootNameIndex =
                CheckedAsciiNameIndex(Data.MH,
                                      pGeometryRoot->Head.nNameIndex,
                                      "GetSupernodes() geometry root");
            const unsigned short nSuperRootNameIndex =
                CheckedAsciiNameIndex(*pSupermodelHeader,
                                      pSuperRoot->Head.nNameIndex,
                                      "GetSupernodes() supermodel root");
            GetSupernodes(Data.MH,
                          *pSupermodelHeader,
                          nCurrentSupernode,
                          nTotalSupermodelNodes,
                          nGeometryRootNameIndex,
                          nSuperRootNameIndex);
        }
    }
    /// Apply supernode numbers to anim nodes
    for(Animation & anim : Data.MH.Animations){
        for(Node & anim_node : anim.ArrayOfNodes){
            int nNodeIndex = GetNodeIndexByNameIndex(anim_node.Head.nNameIndex);
            if(nNodeIndex != -1){
                anim_node.Head.nSupernodeNumber = Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nSupernodeNumber;
            }
            else{
                /// I don't know what to do with the name indices that are present as animation nodes but are not found as geometry nodes.
            }
        }
    }
    /// Build Array of Indices By Tree Order
    if(Data.MH.ArrayOfNodes.empty()){
        throw mdlexception("ASCII post-processing produced no geometry nodes.");
    }
    Data.MH.NameIndicesInTreeOrder.reserve(Data.MH.ArrayOfNodes.size());
    pGeometryRoot = FindAsciiRootNode(Data.MH.ArrayOfNodes, "geometry");
    if(pGeometryRoot == nullptr){
        throw mdlexception("ASCII post-processing could not find a geometry root node for tree-order construction.");
    }
    Data.MH.BuildTreeOrderArray(*pGeometryRoot);

    /// PART 3 ///
    /// Interpret ascii data
    /// This constructs the Mesh.Vertices, Mesh.VertIndices, Dangly.Data2, Dangly.Constraints and Saber.SaberData structures.
    /// And not to forget the weights. Also face normals, average, aabb tree .... everything.
    Report("Interpreting ascii data...");
    ProgressSize(0, Data.MH.ArrayOfNodes.size());
    ProgressPos(0);
    for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
        //ReportMdl << "n = " << n << "\n";
        Node & node = Data.MH.ArrayOfNodes.at(n);
        //std::cout << "Processing: " << GetNodeName(node) << std::endl;
        //ReportMdl << "Analyzing node " << Data.MH.Names.at(node.Head.nNameIndex).sName << " (" << n << "/" << Data.MH.ArrayOfNodes.size() << ")\n";

        //ReportMdl << "PART 3 - stage 1" << "\n";
        if(node.Head.nType & NODE_SABER){
            /// Saber interpretation goes here.
            const bool bHasPreservedSaberData = node.Saber.bPreserveSaberData;
            if(bHasPreservedSaberData){
                // Exact binary saber payload was supplied through ASCII. Do not regenerate it
                // from the old 16-point blade recipe. Keep parsed faces as-is and rebuild
                // only Mesh.Vertices for shared bookkeeping/bounding calculations.
                node.Mesh.Vertices.clear();
                node.Mesh.Vertices.reserve(node.Saber.SaberData.size());
                for(VertexData & sd : node.Saber.SaberData) node.Mesh.Vertices.push_back(Vertex().assign(sd.vVertex));
            }
            else if((node.Mesh.TempVerts.size() == 16 && node.Mesh.TempTverts.size() == 16) ||
                (node.Mesh.TempVerts.size() == 176 && node.Mesh.TempTverts.size() == 176)){

                int nBase = 8;
                if (node.Mesh.TempVerts.size() == 176) nBase = 88;

                Vector v0 = node.Mesh.TempVerts.at(0);
                Vector v1 = node.Mesh.TempVerts.at(1);
                Vector v2 = node.Mesh.TempVerts.at(2);
                Vector v3 = node.Mesh.TempVerts.at(3);
                Vector v4 = node.Mesh.TempVerts.at(4);
                Vector v5 = node.Mesh.TempVerts.at(5);
                Vector v6 = node.Mesh.TempVerts.at(6);
                Vector v7 = node.Mesh.TempVerts.at(7);
                Vector v8 = node.Mesh.TempVerts.at(nBase+0);
                Vector v9 = node.Mesh.TempVerts.at(nBase+1);
                Vector v10 = node.Mesh.TempVerts.at(nBase+2);
                Vector v11 = node.Mesh.TempVerts.at(nBase+3);
                Vector v12 = node.Mesh.TempVerts.at(nBase+4);
                Vector v13 = node.Mesh.TempVerts.at(nBase+5);
                Vector v14 = node.Mesh.TempVerts.at(nBase+6);
                Vector v15 = node.Mesh.TempVerts.at(nBase+7);

                node.Saber.SaberData.clear();
                    node.Saber.SaberData.reserve(176);
                node.Saber.SaberData.push_back(VertexData(v0, node.Mesh.TempTverts.at(0)));
                node.Saber.SaberData.push_back(VertexData(v1, node.Mesh.TempTverts.at(1)));
                node.Saber.SaberData.push_back(VertexData(v2, node.Mesh.TempTverts.at(2)));
                node.Saber.SaberData.push_back(VertexData(v3, node.Mesh.TempTverts.at(3)));
                node.Saber.SaberData.push_back(VertexData(v4, node.Mesh.TempTverts.at(4)));
                node.Saber.SaberData.push_back(VertexData(v5, node.Mesh.TempTverts.at(5)));
                node.Saber.SaberData.push_back(VertexData(v6, node.Mesh.TempTverts.at(6)));
                node.Saber.SaberData.push_back(VertexData(v7, node.Mesh.TempTverts.at(7)));
                for(int r = 0; r < 20; r++){
                    node.Saber.SaberData.push_back(VertexData(v0, node.Mesh.TempTverts.at(0)));
                    node.Saber.SaberData.push_back(VertexData(v1, node.Mesh.TempTverts.at(1)));
                    node.Saber.SaberData.push_back(VertexData(v2, node.Mesh.TempTverts.at(2)));
                    node.Saber.SaberData.push_back(VertexData(v3, node.Mesh.TempTverts.at(3)));
                }

                node.Saber.SaberData.push_back(VertexData(v8, node.Mesh.TempTverts.at(nBase+0)));
                node.Saber.SaberData.push_back(VertexData(v9, node.Mesh.TempTverts.at(nBase+1)));
                node.Saber.SaberData.push_back(VertexData(v10, node.Mesh.TempTverts.at(nBase+2)));
                node.Saber.SaberData.push_back(VertexData(v11, node.Mesh.TempTverts.at(nBase+3)));
                node.Saber.SaberData.push_back(VertexData(v12, node.Mesh.TempTverts.at(nBase+4)));
                node.Saber.SaberData.push_back(VertexData(v13, node.Mesh.TempTverts.at(nBase+5)));
                node.Saber.SaberData.push_back(VertexData(v14, node.Mesh.TempTverts.at(nBase+6)));
                node.Saber.SaberData.push_back(VertexData(v15, node.Mesh.TempTverts.at(nBase+7)));
                for(int r = 0; r < 20; r++){
                    node.Saber.SaberData.push_back(VertexData(v8, node.Mesh.TempTverts.at(nBase+0)));
                    node.Saber.SaberData.push_back(VertexData(v9, node.Mesh.TempTverts.at(nBase+1)));
                    node.Saber.SaberData.push_back(VertexData(v10, node.Mesh.TempTverts.at(nBase+2)));
                    node.Saber.SaberData.push_back(VertexData(v11, node.Mesh.TempTverts.at(nBase+3)));
                }
                node.Mesh.Faces.resize(0);
                node.Mesh.Faces.shrink_to_fit();
            }
            else{
                ReportMdl << "Warning! Requirements for saber mesh not met for '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'! Converting to trimesh...\n";
                node.Head.nType = NODE_HEADER | NODE_MESH;
            }
        }
        //ReportMdl << "PART 3 - stage 2" << "\n";

        if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_SABER)){
            std::vector<Vector> vectorarray;
            if(node.Mesh.Faces.size() > vectorarray.max_size() / 3u){
                throw mdlexception("Node '" + Data.MH.Names.at(node.Head.nNameIndex).sName + "' has too many faces to allocate temporary vertex data safely.");
            }
            vectorarray.reserve(node.Mesh.Faces.size()*3u);
            node.Mesh.fTotalArea = 0.0;

            /// Build mdx bitmap
            if(node.Mesh.TempVerts.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_VERTEX | MDX_FLAG_NORMAL);
            if(node.Mesh.TempNormals.size() > 0){
                node.Mesh.nMdxDataBitmap |= MDX_FLAG_NORMAL;
                node.Mesh.bPreserveMdxNormals = true;
            }
            if(node.Mesh.TempColors.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_COLOR);
            if(node.Mesh.TempTverts.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_UV1);
            if(node.Mesh.TempTverts1.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_UV2);
            if(node.Mesh.TempTverts2.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_UV3);
            if(node.Mesh.TempTverts3.size() > 0) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_UV4);
            if(node.Mesh.TempTangent1.size() > 0){
                node.Mesh.TangentSpace.at(0) = true;
                node.Mesh.bPreserveMdxTangent1 = true;
            }
            if(node.Mesh.TangentSpace.at(0)) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_TANGENT1);
            if(node.Mesh.TangentSpace.at(1)) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_TANGENT2);
            if(node.Mesh.TangentSpace.at(2)) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_TANGENT3);
            if(node.Mesh.TangentSpace.at(3)) node.Mesh.nMdxDataBitmap |= (MDX_FLAG_TANGENT4);

            /// If this a skin, we need to build the bonemap, the bone indices and convert the name indices in the weights to bone indices
            if(node.Head.nType & NODE_SKIN){
                /// First, get the correct name index to all the bones
                for(int nb = 0; nb < static_cast<int>(node.Skin.Bones.size()); nb++){
                    Bone & bone = node.Skin.Bones.at(nb);
                    bone.nNameIndex = Data.MH.NameIndicesInTreeOrder.at(nb);
                }

                /// Next, go through the weights and build the actual bones.
                /// Keep the compact palette order from decompiled ASCII when present.
                /// TSL/K2 has one more compact skin slot than K1; in the binary header
                /// that 17th reverse-map slot lives in nPadding1. K1 uses nPadding1
                /// as an actual padding short, and vanilla K1 models often contain
                /// arbitrary non-zero values there, so do not treat it as slot 16.
                const unsigned int nMaxCompactSlots = bK2 ? 17 : 16;
                std::vector<int> nBoneIndices(nMaxCompactSlots, -1); // compact slot -> full/tree-order bone index

                // Preserve raw compact reverse-map header values before applying
                // semantic compactbonemap overrides or assigning slots for edited
                // weights. The binary header stores 18 uint16 values: 16 compact
                // slots, then nPadding1 and nPadding2. K2 treats nPadding1 as
                // semantic slot 16; K1 treats both trailing shorts as raw padding.
                const unsigned int nRawCompactHeaderShorts = 18;
                for(unsigned int nSlot = 0; nSlot < node.Skin.TempCompactBoneRawIndices.size() && nSlot < nRawCompactHeaderShorts; nSlot++){
                    MdlInteger<unsigned short> nRawFullBone = node.Skin.TempCompactBoneRawIndices.at(nSlot);
                    if(!nRawFullBone.Valid()) continue;
                    if(nSlot < 16) node.Skin.nBoneIndices.at(nSlot) = static_cast<unsigned short>(nRawFullBone);
                    else if(nSlot == 16) node.Skin.nPadding1 = static_cast<unsigned short>(nRawFullBone);
                    else node.Skin.nPadding2 = static_cast<unsigned short>(nRawFullBone);

                    if(nSlot < nMaxCompactSlots &&
                       static_cast<unsigned short>(nRawFullBone) < node.Skin.Bones.size() &&
                       node.Skin.Bones.at(static_cast<unsigned short>(nRawFullBone)).nBonemap.Valid() &&
                       static_cast<unsigned short>(node.Skin.Bones.at(static_cast<unsigned short>(nRawFullBone)).nBonemap) == nSlot){
                        nBoneIndices.at(nSlot) = static_cast<unsigned short>(nRawFullBone);
                    }
                }
                for(unsigned int nSlot = nRawCompactHeaderShorts; nSlot < node.Skin.TempCompactBoneRawIndices.size(); nSlot++){
                    if(node.Skin.TempCompactBoneRawIndices.at(nSlot).Valid()){
                        throw mdlexception("compactbonemapraw on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                           "' contains more raw header values than the binary skin header can store.");
                    }
                }

                auto FindTreeOrderIndex = [&](MdlInteger<unsigned short> nNameIndex, unsigned short & nNodeIndex) -> bool {
                    if(!nNameIndex.Valid()) return false;
                    for(unsigned short ind2 = 0; ind2 < Data.MH.NameIndicesInTreeOrder.size(); ind2++){
                        if(nNameIndex == Data.MH.NameIndicesInTreeOrder.at(ind2)){
                            nNodeIndex = ind2;
                            return true;
                        }
                    }
                    return false;
                };

                auto SetCompactSlot = [&](unsigned short nSlot, unsigned short nNodeIndex, MdlInteger<unsigned short> nNameIndex){
                    if(nSlot >= nMaxCompactSlots) throw mdlexception("Skin compact slot is outside the active game range.");
                    if(nBoneIndices.at(nSlot) != -1 && nBoneIndices.at(nSlot) != nNodeIndex){
                        throw mdlexception("Conflicting compact skin palette data on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName + "'.");
                    }
                    nBoneIndices.at(nSlot) = nNodeIndex;
                    if(node.Skin.BoneNameIndices.size() <= nSlot) node.Skin.BoneNameIndices.resize(nSlot + 1);
                    node.Skin.BoneNameIndices.at(nSlot) = nNameIndex;
                    node.Skin.Bones.at(nNodeIndex).nBonemap = nSlot;
                    if(nSlot < 16) node.Skin.nBoneIndices.at(nSlot) = nNodeIndex;
                    else node.Skin.nPadding1 = nNodeIndex;
                };

                for(unsigned int nSlot = 0; nSlot < node.Skin.TempCompactBoneNameIndices.size() && nSlot < nMaxCompactSlots; nSlot++){
                    MdlInteger<unsigned short> nNameIndex = node.Skin.TempCompactBoneNameIndices.at(nSlot);
                    if(!nNameIndex.Valid()) continue;

                    unsigned short nNodeIndex = 0;
                    if(!FindTreeOrderIndex(nNameIndex, nNodeIndex)){
                        throw mdlexception("compactbonemap on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                           "' references a bone that is not present in the geometry tree.");
                    }
                    SetCompactSlot(nSlot, nNodeIndex, nNameIndex);
                }
                for(unsigned int nSlot = nMaxCompactSlots; nSlot < node.Skin.TempCompactBoneNameIndices.size(); nSlot++){
                    if(node.Skin.TempCompactBoneNameIndices.at(nSlot).Valid()){
                        throw mdlexception("compactbonemap on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                           "' defines a compact slot that this game does not support.");
                    }
                }

                for(Weight & w : node.Skin.TempWeights){
                    for(int nWeightSlot = 0; nWeightSlot < 4; nWeightSlot++){
                        MdlInteger<unsigned short> & ind = w.nWeightIndex.at(nWeightSlot);
                        double fWeightValue = w.fWeightValue.at(nWeightSlot);

                        if(abs(fWeightValue) < 0.0000001){
                            ind = -1;
                            continue;
                        }
                        if(!std::isfinite(fWeightValue)){
                            throw mdlexception("Skin weight on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName + "' is not finite.");
                        }
                        if(!ind.Valid()){
                            throw mdlexception("Active skin weight on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                               "' has no valid bone. 'root' is not a valid active skin influence in compiled MDX data.");
                        }

                        unsigned short nNodeIndex = 0;
                        if(!FindTreeOrderIndex(ind, nNodeIndex)){
                            throw mdlexception("Skin weight on node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                               "' references a bone that is not present in the geometry tree.");
                        }

                        int nBoneIndex = -1;
                        for(int nSlot = 0; nSlot < static_cast<int>(nBoneIndices.size()); nSlot++){
                            if(nBoneIndices.at(nSlot) == nNodeIndex){
                                nBoneIndex = nSlot;
                                break;
                            }
                        }

                        if(nBoneIndex == -1){
                            for(int nSlot = 0; nSlot < static_cast<int>(nBoneIndices.size()); nSlot++){
                                if(nBoneIndices.at(nSlot) == -1){
                                    nBoneIndex = nSlot;
                                    break;
                                }
                            }
                            if(nBoneIndex == -1){
                                throw mdlexception("Skin node '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                                   "' uses more active compact bones than this game supports.");
                            }
                            SetCompactSlot(static_cast<unsigned short>(nBoneIndex), nNodeIndex, ind);
                        }

                        /// Now that we have a compact bone index, update the one in the weights.
                        ind = static_cast<unsigned short>(nBoneIndex);
                    }
                }
            }
            //std::cout << "Done with bones" << std::endl;

            /// Go through all the faces
            for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
                Face & face = node.Mesh.Faces.at(f);
                face.nID = f; /// Why is this necessary?

                /// MDLOps may leave out texindicesX arrays, I need to check for unset indices and make them use the diffuse ones instead.
                for(int i = 0; i < 3; i++){
                    if(node.Mesh.TempTverts1.size() > 0 && !face.nIndexTvert1.at(i).Valid()){
                        for(int i2 = 0; i2 < 3; i2++) face.nIndexTvert1.at(i2) = face.nIndexTvert.at(i2);
                    }
                    if(node.Mesh.TempTverts2.size() > 0 && !face.nIndexTvert2.at(i).Valid()){
                        for(int i2 = 0; i2 < 3; i2++) face.nIndexTvert2.at(i2) = face.nIndexTvert.at(i2);
                    }
                    if(node.Mesh.TempTverts3.size() > 0 && !face.nIndexTvert3.at(i).Valid()){
                        for(int i2 = 0; i2 < 3; i2++) face.nIndexTvert3.at(i2) = face.nIndexTvert.at(i2);
                    }
                }

                /// Go through all the verts in the face
                for(int i = 0; i < 3; i++){
                    if(!face.bProcessed[i]){
                        bool bIgnoreVert = true, bIgnoreTvert = true, bIgnoreTvert1 = true, bIgnoreTvert2 = true, bIgnoreTvert3 = true, bIgnoreColor = true, bIgnoreNormal = true, bIgnoreTangent1 = true;
                        Vertex vert;
                        vert.MDXData.nNameIndex = node.Head.nNameIndex;

                        if(node.Mesh.TempVerts.size() > 0){
                            bIgnoreVert = false;
                            vert.assign(node.Mesh.TempVerts.at(face.nIndexVertex[i]));

                            //Add to vectorarray if no identical
                            bool bAdd = true;
                            for(int v = 0; v < static_cast<int>(vectorarray.size()) && bAdd; v++){
                                if(vectorarray.at(v).Compare(node.Mesh.TempVerts.at((unsigned short) face.nIndexVertex[i]))) bAdd = false;
                            }
                            if(bAdd){
                                vectorarray.push_back(node.Mesh.TempVerts.at((unsigned short) face.nIndexVertex[i]));
                            }

                            vert.vFromRoot = node.Mesh.TempVerts.at((unsigned short) face.nIndexVertex[i]);
                            vert.vFromRoot.Rotate(node.GetLocation().oOrientation.GetQuaternion());
                            vert.vFromRoot += node.Head.vFromRoot;

                            vert.MDXData.vVertex = node.Mesh.TempVerts.at((unsigned short) face.nIndexVertex[i]);

                            if(node.Head.nType & NODE_DANGLY){
                                node.Dangly.Data2.push_back(node.Mesh.TempVerts.at((unsigned short) face.nIndexVertex[i]));
                                node.Dangly.Constraints.push_back(node.Dangly.TempConstraints.at((unsigned short) face.nIndexVertex[i]));
                            }

                            if(node.Head.nType & NODE_SKIN){
                                vert.MDXData.Weights = node.Skin.TempWeights.at((unsigned short) face.nIndexVertex[i]);
                                double fTotalWeight = 0.0;
                                fTotalWeight += vert.MDXData.Weights.fWeightValue.at(0);
                                fTotalWeight += vert.MDXData.Weights.fWeightValue.at(1);
                                fTotalWeight += vert.MDXData.Weights.fWeightValue.at(2);
                                fTotalWeight += vert.MDXData.Weights.fWeightValue.at(3);
                                if(abs(fTotalWeight - 1.0) >= 0.0001) ReportMdl << "Warning! Skin weights for ascii vertex " << (unsigned short) face.nIndexVertex[i] << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "' do not equal 1.0, instead they equal " << fTotalWeight << ". This may cause problems in the game.\n";
                            }
                        }
                        if(node.Mesh.TempTverts.size() > 0){
                            bIgnoreTvert = false;
                            vert.MDXData.vUV1 = node.Mesh.TempTverts.at((unsigned short) face.nIndexTvert[i]);
                        }
                        if(node.Mesh.TempTverts1.size() > 0){
                            bIgnoreTvert1 = false;
                            vert.MDXData.vUV2 = node.Mesh.TempTverts1.at((unsigned short) face.nIndexTvert1[i]);
                        }
                        if(node.Mesh.TempTverts2.size() > 0){
                            bIgnoreTvert2 = false;
                            vert.MDXData.vUV3 = node.Mesh.TempTverts2.at((unsigned short) face.nIndexTvert2[i]);
                        }
                        if(node.Mesh.TempTverts3.size() > 0){
                            bIgnoreTvert3 = false;
                            vert.MDXData.vUV4 = node.Mesh.TempTverts3.at((unsigned short) face.nIndexTvert3[i]);
                        }
                        if(node.Mesh.TempColors.size() > 0){
                            bIgnoreColor = false;
                            vert.MDXData.cColor = node.Mesh.TempColors.at((unsigned short) face.nIndexColor[i]);
                        }
                        if(node.Mesh.TempNormals.size() > 0){
                            bIgnoreNormal = false;
                            MdlInteger<unsigned short> nNormalIndex = face.nIndexNormal[i];
                            if(!nNormalIndex.Valid()) nNormalIndex = face.nIndexVertex[i];
                            vert.MDXData.vNormal = node.Mesh.TempNormals.at((unsigned short) nNormalIndex);
                        }
                        if(node.Mesh.TempTangent1.size() > 0){
                            bIgnoreTangent1 = false;
                            vert.MDXData.vTangent1 = node.Mesh.TempTangent1.at((unsigned short) face.nIndexVertex[i]);
                        }

                        //Find identical verts
                        for(int f2 = f; f2 < static_cast<int>(node.Mesh.Faces.size()); f2++){
                            Face & face2 = node.Mesh.Faces.at(f2);
                            for(int i2 = 0; i2 < 3; i2++){
                                //Make sure that we're only changing what's past our current position if we are in the same face.
                                if(f2 != f || i2 > i){
                                    if(bMinimizeVerts){
                                        try{
                                            if( !face2.bProcessed[i2] &&
                                                (bIgnoreVert || node.Mesh.TempVerts.at(face2.nIndexVertex[i2]).Compare(node.Mesh.TempVerts.at(face.nIndexVertex[i]), 0.00001) ) &&
                                                (bIgnoreVert || !(node.Head.nType & NODE_DANGLY) || node.Dangly.TempConstraints.at(face2.nIndexVertex[i2]) == node.Dangly.TempConstraints.at(face.nIndexVertex[i]) ) &&
                                                (bIgnoreVert || !(node.Head.nType & NODE_SKIN) || node.Skin.TempWeights.at(face2.nIndexVertex[i2]) == node.Skin.TempWeights.at(face.nIndexVertex[i]) ) &&
                                                (bIgnoreTvert || node.Mesh.TempTverts.at(face2.nIndexTvert[i2]) == node.Mesh.TempTverts.at(face.nIndexTvert[i]) ) &&
                                                (bIgnoreTvert1 || node.Mesh.TempTverts1.at(face2.nIndexTvert1[i2]) == node.Mesh.TempTverts1.at(face.nIndexTvert1[i]) ) &&
                                                (bIgnoreTvert2 || node.Mesh.TempTverts2.at(face2.nIndexTvert2[i2]) == node.Mesh.TempTverts2.at(face.nIndexTvert2[i]) ) &&
                                                (bIgnoreTvert3 || node.Mesh.TempTverts3.at(face2.nIndexTvert3[i2]) == node.Mesh.TempTverts3.at(face.nIndexTvert3[i]) ) &&
                                                (bIgnoreColor || node.Mesh.TempColors.at(face2.nIndexColor[i2]) == node.Mesh.TempColors.at(face.nIndexColor[i]) ) &&
                                                (bIgnoreNormal || node.Mesh.TempNormals.at(face2.nIndexNormal[i2].Valid() ? static_cast<unsigned short>(face2.nIndexNormal[i2]) : static_cast<unsigned short>(face2.nIndexVertex[i2])) == node.Mesh.TempNormals.at(face.nIndexNormal[i].Valid() ? static_cast<unsigned short>(face.nIndexNormal[i]) : static_cast<unsigned short>(face.nIndexVertex[i])) ) &&
                                                (bIgnoreTangent1 ||
                                                    (node.Mesh.TempTangent1.at(face2.nIndexVertex[i2]).at(0) == node.Mesh.TempTangent1.at(face.nIndexVertex[i]).at(0) &&
                                                     node.Mesh.TempTangent1.at(face2.nIndexVertex[i2]).at(1) == node.Mesh.TempTangent1.at(face.nIndexVertex[i]).at(1) &&
                                                     node.Mesh.TempTangent1.at(face2.nIndexVertex[i2]).at(2) == node.Mesh.TempTangent1.at(face.nIndexVertex[i]).at(2))) &&
                                                (!bIgnoreNormal || (face.nSmoothingGroup & face2.nSmoothingGroup)))
                                            {
                                                //If we find a reference to the exact same vert, we have to link to it
                                                //Actually we only need to link vert indices, the correct UV are now already included in the Vertex struct
                                                face2.nIndexVertex[i2] = CheckedAsciiVertexIndex(node.Mesh.Vertices.size(), "ASCII mesh post-processing");
                                                face2.bProcessed[i2] = true;
                                            }
                                        }
                                        catch(const std::exception & e){
                                            throw mdlexception("Exception while handling temp arrays (face2=" + std::to_string(f2) + ", i2=" + std::to_string(i2) + ") node '" + Data.MH.Names.at(node.Head.nNameIndex).sName + "':\n" + e.what());
                                        }
                                    }
                                    else{
                                        if( (bIgnoreVert || face2.nIndexVertex[i2] == face.nIndexVertex[i] ) &&
                                            (bIgnoreTvert || face2.nIndexTvert[i2] == face.nIndexTvert[i] ) &&
                                            (bIgnoreTvert1 || face2.nIndexTvert1[i2] == face.nIndexTvert1[i] ) &&
                                            (bIgnoreTvert2 || face2.nIndexTvert2[i2] == face.nIndexTvert2[i] ) &&
                                            (bIgnoreTvert3 || face2.nIndexTvert3[i2] == face.nIndexTvert3[i] ) &&
                                            (bIgnoreColor || face2.nIndexColor[i2] == face.nIndexColor[i] ) &&
                                            (bIgnoreNormal || face2.nIndexNormal[i2] == face.nIndexNormal[i] ) &&
                                            (bIgnoreTangent1 || face2.nIndexVertex[i2] == face.nIndexVertex[i] ) &&
                                            !face2.bProcessed[i2] &&
                                            (!bIgnoreNormal || (face.nSmoothingGroup & face2.nSmoothingGroup)))
                                        {
                                            //If we find a reference to the exact same vert, we have to link to it
                                            //Actually we only need to link vert indices, the correct UV are now already included in the Vertex struct
                                            face2.nIndexVertex[i2] = CheckedAsciiVertexIndex(node.Mesh.Vertices.size(), "ASCII mesh post-processing");
                                            face2.bProcessed[i2] = true;
                                        }
                                    }
                                }
                            }
                        }

                        //Now we're allowed to link the original vert as well
                        face.nIndexVertex[i] = CheckedAsciiVertexIndex(node.Mesh.Vertices.size(), "ASCII mesh post-processing");
                        face.bProcessed[i] = true;

                        //Put the new vert into the array
                        node.Mesh.Vertices.push_back(std::move(vert));
                    }
                }

                std::array<unsigned short, 3> vertindicesarray = {face.nIndexVertex[0], face.nIndexVertex[1], face.nIndexVertex[2]};
                node.Mesh.VertIndices.push_back(std::move(vertindicesarray));

                /// Surprise! Face normal calculation! Moved here so it can be used by BuildAABB
                Vertex & v1 = node.Mesh.Vertices.at(face.nIndexVertex[0]);
                Vertex & v2 = node.Mesh.Vertices.at(face.nIndexVertex[1]);
                Vertex & v3 = node.Mesh.Vertices.at(face.nIndexVertex[2]);
                Vector & v1UV = v1.MDXData.vUV1;
                Vector & v2UV = v2.MDXData.vUV1;
                Vector & v3UV = v3.MDXData.vUV1;
                Vector Edge1 = v2 - v1;
                Vector Edge2 = v3 - v1;
                Vector Edge3 = v3 - v2;
                Vector EUV1 = v2UV - v1UV;
                Vector EUV2 = v3UV - v1UV;
                Vector EUV3 = v3UV - v2UV;

                /// This is for the face normal
                face.vNormal = cross(Edge1, Edge2); //Cross product, unnormalized
                face.vNormal.Normalize();

                /// This is for the distance.
                face.fDistance = - (face.vNormal.fX * v1.fX +
                                    face.vNormal.fY * v1.fY +
                                    face.vNormal.fZ * v1.fZ);

                // When the binary face runtime fields were
                // supplied, keep them rather than mutating them during compile.
                // Area/bounds/tangent calculations below still use the preserved
                // face plane so user-visible edits can coexist with preservation.
                if(face.bPreserveRuntimeFaceData){
                    face.vNormal = face.vPreservedNormal;
                    face.fDistance = face.fPreservedDistance;
                    face.nAdjacentFaces = face.nPreservedAdjacentFaces;
                }

                /// Area calculation
                face.fArea = HeronFormulaEdge(Edge1, Edge2, Edge3);
                face.fAreaUV = HeronFormulaEdge(EUV1, EUV2, EUV3);
                /// TODO: report problematic cases
                if(face.fArea != -1.0) node.Mesh.fTotalArea += face.fArea;

                /// Tangent space vectors
                //Now comes the calculation. Will be using edges 1 and 2
                double r = (EUV1.fX * EUV2.fY - EUV1.fY * EUV2.fX);
                //This is division, need to check for 0
                if(r != 0){
                    r = 1.0 / r;
                }
                else{
                    /*
                    It can be 0 in several ways.
                    1. any of the two edges is zero (ie. we're dealing with a line, not a triangle) - this happens
                    2. both x's or both y's are zero, implying parallel edges, but we cannot have any in a triangle
                    3. both x's are the same and both y's are the same, therefore they have the same angle and are parallel
                    4. both edges have the same x and y, they both have a 45° angle and are therefore parallel
                    */
                    //ndix UR's magic factor
                    r = 2406.6388;
                }
                face.vTangent = r * (Edge1 * EUV2.fY - Edge2 * EUV1.fY);
                face.vBitangent = r * (Edge2 * EUV1.fX - Edge1 * EUV2.fX);
                face.vTangent.Normalize();
                face.vBitangent.Normalize();
                if(face.vTangent.Null()) face.vTangent = Vector(1.0, 0.0, 0.0);
                if(face.vBitangent.Null()) face.vBitangent = Vector(1.0, 0.0, 0.0);

                //Handedness
                Vector vCross = cross(face.vNormal, face.vTangent);
                double fDot = dot(vCross, face.vBitangent);
                if(fDot > 0.0000000001) face.vTangent *= -1.0;

                //Now check if we need to invert  T and B. But first we need a UV normal
                Vector vNormalUV = cross(EUV1, EUV2); //cross product
                if(vNormalUV.fZ < 0.0){
                    face.vTangent *= -1.0;
                    face.vBitangent *= -1.0;
                }

                /// Face Bounding Box calculation for AABB tree
                if(node.Head.nType & NODE_AABB){
                    face.vBBmax = Vector(-10000.0, -10000.0, -10000.0);
                    face.vBBmin = Vector( 10000.0,  10000.0,  10000.0);
                    face.vCentroid = Vector(0.0, 0.0, 0.0);
                    for(int i = 0; i < 3; i++){
                        face.vBBmax.fX = std::max(face.vBBmax.fX, node.Mesh.Vertices.at(face.nIndexVertex[i]).fX);
                        face.vBBmax.fY = std::max(face.vBBmax.fY, node.Mesh.Vertices.at(face.nIndexVertex[i]).fY);
                        face.vBBmax.fZ = std::max(face.vBBmax.fZ, node.Mesh.Vertices.at(face.nIndexVertex[i]).fZ);
                        face.vBBmin.fX = std::min(face.vBBmin.fX, node.Mesh.Vertices.at(face.nIndexVertex[i]).fX);
                        face.vBBmin.fY = std::min(face.vBBmin.fY, node.Mesh.Vertices.at(face.nIndexVertex[i]).fY);
                        face.vBBmin.fZ = std::min(face.vBBmin.fZ, node.Mesh.Vertices.at(face.nIndexVertex[i]).fZ);
                        face.vCentroid += node.Mesh.Vertices.at(face.nIndexVertex[i]);
                    }
                    face.vCentroid /= 3.0;
                }
            }

            /// Surprise 2!! Average and BB calculation!
            node.Mesh.vAverage = Vector(0.0, 0.0, 0.0);
            node.Mesh.vBBmin = Vector(0.0, 0.0, 0.0); /// Wrong, but Bioware-correct
            node.Mesh.vBBmax = Vector(0.0, 0.0, 0.0); /// Wrong, but Bioware-correct
            for(int v = 0; v < static_cast<int>(vectorarray.size()); v++){
                node.Mesh.vBBmin.fX = std::min(node.Mesh.vBBmin.fX, vectorarray.at(v).fX);
                node.Mesh.vBBmin.fY = std::min(node.Mesh.vBBmin.fY, vectorarray.at(v).fY);
                node.Mesh.vBBmin.fZ = std::min(node.Mesh.vBBmin.fZ, vectorarray.at(v).fZ);
                node.Mesh.vBBmax.fX = std::max(node.Mesh.vBBmax.fX, vectorarray.at(v).fX);
                node.Mesh.vBBmax.fY = std::max(node.Mesh.vBBmax.fY, vectorarray.at(v).fY);
                node.Mesh.vBBmax.fZ = std::max(node.Mesh.vBBmax.fZ, vectorarray.at(v).fZ);
                node.Mesh.vAverage += vectorarray.at(v);
                //if(v % 1000 == 0) std::cout << "Done with average for vert " << v << std::endl;
            }
            node.Mesh.vAverage /= (double) vectorarray.size();
            //std::cout << "Done with average for verts" << std::endl;

            /// Now find the radius as well!
            node.Mesh.fRadius = 0.0;
            for(int v = 0; v < static_cast<int>(vectorarray.size()); v++){
                node.Mesh.fRadius = std::max(node.Mesh.fRadius, Vector(vectorarray.at(v) - node.Mesh.vAverage).GetLength());
                //if(v % 1000 == 0) std::cout << "Done with radius for vert " << v << std::endl;
            }
            //std::cout << "Done with radius for verts" << std::endl;

            //Calculate adjacent faces unless the ASCII supplied binary face
            //runtime data for every face. In that case an invalid adjacency value
            //is intentional boundary data, not something to fill in.
            bool bPreserveAllRuntimeFaceData = !node.Mesh.Faces.empty();
            for(Face & facePreserveCheck : node.Mesh.Faces){
                if(!facePreserveCheck.bPreserveRuntimeFaceData){
                    bPreserveAllRuntimeFaceData = false;
                    break;
                }
            }
            if(!bPreserveAllRuntimeFaceData){
                for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
                    Face & face = node.Mesh.Faces.at(f);

                    // Skip if none is -1
                    if(face.nAdjacentFaces[0].Valid() &&
                       face.nAdjacentFaces[1].Valid() &&
                       face.nAdjacentFaces[2].Valid() ) continue;

                    //Go through all the faces coming after this one
                    for(int f2 = f+1; f2 < static_cast<int>(node.Mesh.Faces.size()); f2++){
                        Face & compareface = node.Mesh.Faces.at(f2);
                        std::vector<bool> VertMatches(3, false);
                        std::vector<bool> VertMatchesCompare(3, false);
                        for(int i3 = 0; i3 < 3; i3++){
                            unsigned short nVertIndex = face.nIndexVertex[i3];
                            Vector & ourvect = node.Mesh.Vertices.at(nVertIndex).vFromRoot;
                            for(int i4 = 0; i4 < 3; i4++){
                                Vector & othervect = node.Mesh.Vertices.at(compareface.nIndexVertex[i4]).vFromRoot;
                                if(ourvect.Compare(othervect)){
                                    VertMatches.at(i3) = true;
                                    VertMatchesCompare.at(i4) = true;
                                    i4 = 3; // we can only have one matching vert in a face per vert. Once we find a match, we're done.
                                }
                            }
                        }
                        if(VertMatches.at(0) && VertMatches.at(1)){
                            if(face.nAdjacentFaces[0].Valid()) ReportMdl<<"Found an additional adjacent face on edge 0 for face " << f << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else face.nAdjacentFaces[0] = f2;
                        }
                        else if(VertMatches.at(1) && VertMatches.at(2)){
                            if(face.nAdjacentFaces[1].Valid()) ReportMdl<<"Found an additional adjacent face on edge 1 for face " << f << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else face.nAdjacentFaces[1] = f2;
                        }
                        else if(VertMatches.at(2) && VertMatches.at(0)){
                            if(face.nAdjacentFaces[2].Valid()) ReportMdl<<"Found an additional adjacent face on edge 2 for face " << f << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else face.nAdjacentFaces[2] = f2;
                        }
                        if(VertMatchesCompare.at(0) && VertMatchesCompare.at(1)){
                            if(compareface.nAdjacentFaces[0].Valid()) ReportMdl<<"Found an additional adjacent face on edge 0 for face " << f2 << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else compareface.nAdjacentFaces[0] = f;
                        }
                        else if(VertMatchesCompare.at(1) && VertMatchesCompare.at(2)){
                            if(compareface.nAdjacentFaces[1].Valid()) ReportMdl<<"Found an additional adjacent face on edge 1 for face " << f2 << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else compareface.nAdjacentFaces[1] = f;
                        }
                        else if(VertMatchesCompare.at(2) && VertMatchesCompare.at(0)){
                            if(compareface.nAdjacentFaces[2].Valid()) ReportMdl<<"Found an additional adjacent face on edge 2 for face " << f2 << " on '" << Data.MH.Names.at(node.Head.nNameIndex).sName << "'...\n";
                            else compareface.nAdjacentFaces[2] = f;
                        }
                        if(face.nAdjacentFaces[0].Valid() &&
                           face.nAdjacentFaces[1].Valid() &&
                           face.nAdjacentFaces[2].Valid() ){
                            f2 = node.Mesh.Faces.size(); //Found them all, maybe I finish early?
                        }
                    }
                    //if(f % 100 == 0) std::cout << "Done with face " << f << std::endl;
                }
            }
            //std::cout << "Done with faces" << std::endl;

            /// Texture count depends on the UVs.
            node.Mesh.nTextureNumber = (node.Mesh.nMdxDataBitmap & MDX_FLAG_UV1 ? 1 : 0) +
                                       (node.Mesh.nMdxDataBitmap & MDX_FLAG_UV2 ? 1 : 0) +
                                       (node.Mesh.nMdxDataBitmap & MDX_FLAG_UV3 ? 1 : 0) +
                                       (node.Mesh.nMdxDataBitmap & MDX_FLAG_UV4 ? 1 : 0);

            node.Mesh.TempVerts.resize(0);
            node.Mesh.TempTverts.resize(0);
            node.Mesh.TempTverts1.resize(0);
            node.Mesh.TempTverts2.resize(0);
            node.Mesh.TempTverts3.resize(0);
            node.Mesh.TempNormals.resize(0);
            node.Mesh.TempTangent1.resize(0);
            node.Dangly.TempConstraints.resize(0);
            node.Skin.TempWeights.resize(0);
            node.Skin.TempCompactBoneNameIndices.resize(0);
            node.Skin.TempCompactBoneRawIndices.resize(0);
            node.Mesh.TempVerts.shrink_to_fit();
            node.Mesh.TempTverts.shrink_to_fit();
            node.Mesh.TempTverts1.shrink_to_fit();
            node.Mesh.TempTverts2.shrink_to_fit();
            node.Mesh.TempTverts3.shrink_to_fit();
            node.Mesh.TempNormals.shrink_to_fit();
            node.Mesh.TempTangent1.shrink_to_fit();
            node.Dangly.TempConstraints.shrink_to_fit();
            node.Skin.TempWeights.shrink_to_fit();
            node.Skin.TempCompactBoneNameIndices.shrink_to_fit();
            node.Skin.TempCompactBoneRawIndices.shrink_to_fit();
        }
        //ReportMdl << "PART 3 - stage 3" << "\n";

        if(node.Head.nType & NODE_MESH &&
           !(node.Head.nType & NODE_SABER) &&
           Data.MH.Names.at(node.Head.nNameIndex).sName.substr(0, 6) == "2081__" &&
           (node.Mesh.Faces.size() == 12  /*|| node.Mesh.Faces.size() == 24*/) &&
           node.Mesh.Vertices.size() == 16 )
        {
            bool bAbort = false; /// If this is set to true at any time, we will abort the conversion to saber

            std::array<int, 16> VertRefsArray = {0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0};

            /// Go through the faces and build the VertRefsArray
            /// If a reference to a higher than 16th vertex, abort
            /// If the number of references to a vertex is greater than 4, abort
            for(int f = 0; f < 12 && !bAbort; f++){
                Face & face = node.Mesh.Faces.at(f);
                for(int i = 0; i < 3; i++){
                    if(face.nIndexVertex.at(i) < 16){
                        VertRefsArray.at(face.nIndexVertex.at(i)) += 1;
                        if ( VertRefsArray.at(face.nIndexVertex.at(i)) > 4) bAbort = true;
                    }
                    else bAbort = true;
                }
            }

            /// Now we need to find these guys
            MdlInteger<unsigned int> nOuter1;
            MdlInteger<unsigned int> nOuter2;
            MdlInteger<unsigned int> nCorner1;
            MdlInteger<unsigned int> nCorner2;

            /// Go through the verts, find the two that are referenced by 4 faces
            /// Put them into outer1 and outer2
            /// If there's more than two of those, abort
            int nFound = 0;
            for(int v = 0; v < 16 && !bAbort; v++){
                if(VertRefsArray.at(v) == 4){
                    if(nFound == 0){
                        nOuter1 = v;
                        nFound++;
                    }
                    else if(nFound == 1){
                        nOuter2 = v;
                        nFound++;
                    }
                    else bAbort = true;
                }
            }

            /// Go through faces again and find the corners – the only ones adjacent to outer1 & 2 that have 1 ref
            for(int f = 0; f < 12 && !bAbort; f++){
                Face & face = node.Mesh.Faces.at(f);
                /// Go through the indices
                for(int i = 0; i < 3; i++){
                    if( face.nIndexVertex.at(i) == nOuter1){
                        /// Go through the same indices again
                        for(int i2 = 0; i2 < 3; i2++){
                            if(!nCorner1.Valid() && VertRefsArray.at(face.nIndexVertex.at(i2)) == 1){
                                nCorner1 = face.nIndexVertex.at(i2);
                                break;
                            }
                            else if(nCorner1.Valid() && VertRefsArray.at(face.nIndexVertex.at(i2)) == 1){
                                ReportMdl << "Error! nCorner1 found several times, this shouldn't be happening!!\n";
                                bAbort = true;
                                break;
                            }
                        }
                        break;
                    }
                    else if( face.nIndexVertex.at(i) == nOuter2){
                        /// Go through the same indices again
                        for(int i2 = 0; i2 < 3; i2++){
                            if(!nCorner2.Valid() && VertRefsArray.at(face.nIndexVertex.at(i2)) == 1){
                                nCorner2 = face.nIndexVertex.at(i2);
                                break;
                            }
                            else if(nCorner2.Valid() && VertRefsArray.at(face.nIndexVertex.at(i2)) == 1){
                                ReportMdl << "Error! nCorner2 found several times, this shouldn't be happening!!\n";
                                bAbort = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            if(nOuter1.Valid() && nOuter2.Valid() && nCorner1.Valid() && nCorner2.Valid() && !bAbort){
                /// Build blade vert arrays
                std::array<MdlInteger<unsigned int>, 8> Blade1VertArray;
                std::array<MdlInteger<unsigned int>, 8> Blade2VertArray;

                Blade1VertArray.at(6) = nOuter1;
                Blade1VertArray.at(7) = nCorner1;
                Blade1VertArray.at(3) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(6), Blade1VertArray.at(7));
                Blade1VertArray.at(2) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(3), Blade1VertArray.at(6), Blade1VertArray.at(7));
                Blade1VertArray.at(1) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(2), Blade1VertArray.at(6), Blade1VertArray.at(3));
                Blade1VertArray.at(5) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(1), Blade1VertArray.at(6), Blade1VertArray.at(2));
                Blade1VertArray.at(0) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(1), Blade1VertArray.at(5), Blade1VertArray.at(6));
                Blade1VertArray.at(4) = FindThirdIndex(node.Mesh.Faces, Blade1VertArray.at(0), Blade1VertArray.at(5), Blade1VertArray.at(1));

                Blade2VertArray.at(6) = nOuter2;
                Blade2VertArray.at(7) = nCorner2;
                Blade2VertArray.at(3) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(6), Blade2VertArray.at(7));
                Blade2VertArray.at(2) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(3), Blade2VertArray.at(6), Blade2VertArray.at(7));
                Blade2VertArray.at(1) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(2), Blade2VertArray.at(6), Blade2VertArray.at(3));
                Blade2VertArray.at(5) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(1), Blade2VertArray.at(6), Blade2VertArray.at(2));
                Blade2VertArray.at(0) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(1), Blade2VertArray.at(5), Blade2VertArray.at(6));
                Blade2VertArray.at(4) = FindThirdIndex(node.Mesh.Faces, Blade2VertArray.at(0), Blade2VertArray.at(5), Blade2VertArray.at(1));

                /// if there is a -1 in any of the two arrays, abort
                if(std::find(Blade1VertArray.begin(), Blade1VertArray.end(), INVALID_INT) != Blade1VertArray.end() ||
                   std::find(Blade2VertArray.begin(), Blade2VertArray.end(), INVALID_INT) != Blade2VertArray.end()) bAbort = true;

                if(!bAbort){
                    ///Now all we need to do is decide which of the two blades to invert.
                    std::array<MdlInteger<unsigned int>, 8> Blade1, Blade2;
                    if(node.Mesh.Vertices.at(Blade1VertArray.at(6)).fZ - node.Mesh.Vertices.at(Blade1VertArray.at(5)).fZ >
                       node.Mesh.Vertices.at(Blade2VertArray.at(6)).fZ - node.Mesh.Vertices.at(Blade2VertArray.at(5)).fZ )
                    {
                        Blade1 = Blade1VertArray;
                        Blade2 = {Blade2VertArray[3], Blade2VertArray[2], Blade2VertArray[1], Blade2VertArray[0],
                                  Blade2VertArray[7], Blade2VertArray[6], Blade2VertArray[5], Blade2VertArray[4]};
                    }
                    else{
                        Blade1 = Blade2VertArray;
                        Blade2 = {Blade1VertArray[3], Blade1VertArray[2], Blade1VertArray[1], Blade1VertArray[0],
                                  Blade1VertArray[7], Blade1VertArray[6], Blade1VertArray[5], Blade1VertArray[4]};
                    }

                    node.Saber.SaberData.clear();
                    node.Saber.SaberData.reserve(176);
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(0)), node.Mesh.Vertices.at(Blade1.at(0)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(1)), node.Mesh.Vertices.at(Blade1.at(1)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(2)), node.Mesh.Vertices.at(Blade1.at(2)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(3)), node.Mesh.Vertices.at(Blade1.at(3)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(4)), node.Mesh.Vertices.at(Blade1.at(4)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(5)), node.Mesh.Vertices.at(Blade1.at(5)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(6)), node.Mesh.Vertices.at(Blade1.at(6)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(7)), node.Mesh.Vertices.at(Blade1.at(7)).MDXData.vUV1));
                    for(int r = 0; r < 20; r++){
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(0)), node.Mesh.Vertices.at(Blade1.at(0)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(1)), node.Mesh.Vertices.at(Blade1.at(1)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(2)), node.Mesh.Vertices.at(Blade1.at(2)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade1.at(3)), node.Mesh.Vertices.at(Blade1.at(3)).MDXData.vUV1));
                    }

                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(0)), node.Mesh.Vertices.at(Blade2.at(0)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(1)), node.Mesh.Vertices.at(Blade2.at(1)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(2)), node.Mesh.Vertices.at(Blade2.at(2)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(3)), node.Mesh.Vertices.at(Blade2.at(3)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(4)), node.Mesh.Vertices.at(Blade2.at(4)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(5)), node.Mesh.Vertices.at(Blade2.at(5)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(6)), node.Mesh.Vertices.at(Blade2.at(6)).MDXData.vUV1));
                    node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(7)), node.Mesh.Vertices.at(Blade2.at(7)).MDXData.vUV1));
                    for(int r = 0; r < 20; r++){
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(0)), node.Mesh.Vertices.at(Blade2.at(0)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(1)), node.Mesh.Vertices.at(Blade2.at(1)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(2)), node.Mesh.Vertices.at(Blade2.at(2)).MDXData.vUV1));
                        node.Saber.SaberData.push_back(VertexData(node.Mesh.Vertices.at(Blade2.at(3)), node.Mesh.Vertices.at(Blade2.at(3)).MDXData.vUV1));
                    }

                    /// Convert trimesh to lightsaber here
                    node.Head.nType = node.Head.nType | NODE_SABER;
                    Data.MH.Names.at(node.Head.nNameIndex).sName = Data.MH.Names.at(node.Head.nNameIndex).sName.substr(6);
                }
            }
        }
        //ReportMdl << "PART 3 - stage 4" << "\n";

        if(node.Head.nType & NODE_SABER && !node.Saber.bPreserveSaberData){
            std::array<std::array<int, 3>, 12> FaceIndices = {{{0,4,5},{1,0,5},{1,5,2},
                                                              {2,5,6},{3,2,6},{3,6,7},
                                                              {88+4,88+0,88+5},{88+0,88+1,88+5},{88+5,88+1,88+2},
                                                              {88+5,88+2,88+6},{88+2,88+3,88+6},{88+6,88+3,88+7}}};

            std::array<Vector, 12> vFaceNormals;
            for(int v = 0; v < 12; v++){
                vFaceNormals[v] = GetNormal(node.Saber.SaberData.at(FaceIndices[v][0]).vVertex,
                                            node.Saber.SaberData.at(FaceIndices[v][1]).vVertex,
                                            node.Saber.SaberData.at(FaceIndices[v][2]).vVertex);
            }

            std::array<double, 12> fFaceAreas;
            for(int v = 0; v < 12; v++){
                fFaceAreas[v] = HeronFormulaVert(node.Saber.SaberData.at(FaceIndices[v][0]).vVertex,
                                                 node.Saber.SaberData.at(FaceIndices[v][1]).vVertex,
                                                 node.Saber.SaberData.at(FaceIndices[v][2]).vVertex);
            }

            std::array<Vector, 8> vVertNormals;

            for(int v = 0; v < 8; v++){
                Vector & vCurrent = vVertNormals.at(v);
                int nCurrent = v;
                if(nCurrent > 3) nCurrent += 84;
                for(int f = 0; f < 12; f++){
                    for(int i = 0; i < 3; i++){
                        if(FaceIndices[f][i] == nCurrent){
                            Vector vAdd = vFaceNormals[f];
                            if(bSmoothAreaWeighting) vAdd *= (fFaceAreas[f] > 0.000001 ? fFaceAreas[f] : 0.0);
                            if(bSmoothAngleWeighting){
                                if(i == 0){
                                    vAdd *= Angle(node.Saber.SaberData.at(FaceIndices[f][2]).vVertex - node.Saber.SaberData.at(FaceIndices[f][0]).vVertex,
                                                  node.Saber.SaberData.at(FaceIndices[f][1]).vVertex - node.Saber.SaberData.at(FaceIndices[f][0]).vVertex);
                                }
                                else if(i == 1){
                                    vAdd *= Angle(node.Saber.SaberData.at(FaceIndices[f][2]).vVertex - node.Saber.SaberData.at(FaceIndices[f][1]).vVertex,
                                                  node.Saber.SaberData.at(FaceIndices[f][0]).vVertex - node.Saber.SaberData.at(FaceIndices[f][1]).vVertex);
                                }
                                else if(i == 2){
                                    vAdd *= Angle(node.Saber.SaberData.at(FaceIndices[f][1]).vVertex - node.Saber.SaberData.at(FaceIndices[f][2]).vVertex,
                                                  node.Saber.SaberData.at(FaceIndices[f][0]).vVertex - node.Saber.SaberData.at(FaceIndices[f][2]).vVertex);
                                }
                            }
                            vCurrent += vAdd;
                        }
                    }
                }
                vCurrent.Normalize();
            }
            for(int v = 0; v < static_cast<int>(node.Saber.SaberData.size()); v++){
                if(v < static_cast<int>(node.Saber.SaberData.size())/2) node.Saber.SaberData.at(v).vNormal = vVertNormals.at(v%4);
                else node.Saber.SaberData.at(v).vNormal = vVertNormals.at(4 + v%4);
            }
            node.Mesh.Vertices.reserve(node.Saber.SaberData.size());
            for(VertexData & sd : node.Saber.SaberData) node.Mesh.Vertices.push_back(Vertex().assign(sd.vVertex));
            node.Mesh.Faces.resize(12);
            node.Mesh.Faces.at(0).nIndexVertex = {0, 1, 2};
            node.Mesh.Faces.at(1).nIndexVertex = {3, 4, 5};
            node.Mesh.Faces.at(2).nIndexVertex = {6, 7, 8};
            node.Mesh.Faces.at(3).nIndexVertex = {9, 10, 11};
            node.Mesh.Faces.at(4).nIndexVertex = {12, 13, 14};
            node.Mesh.Faces.at(5).nIndexVertex = {15, 16, 17};
            node.Mesh.Faces.at(6).nIndexVertex = {18, 19, 20};
            node.Mesh.Faces.at(7).nIndexVertex = {21, 22, 23};
            node.Mesh.Faces.at(8).nIndexVertex = {24, 25, 26};
            node.Mesh.Faces.at(9).nIndexVertex = {27, 28, 29};
            node.Mesh.Faces.at(10).nIndexVertex = {30, 31, 32};
            node.Mesh.Faces.at(11).nIndexVertex = {33, 32, 31};
        }
        //ReportMdl << "PART 3 - stage 5" << "\n";

        if(node.Head.nType & NODE_AABB){
            if(Wok) Warning("Found an aabb node, but Wok already exists! Skipping this node...");
            else{
                Wok.reset(new WOK());
                std::vector<Face*> allfaces;
                for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
                    allfaces.push_back(&node.Mesh.Faces.at(f));
                }
                std::stringstream file2;
                bool bUsedPreservedAabb = false;
                if(node.Walkmesh.bPreserveAabbTree && !node.Walkmesh.TempAabbNodes.empty()){
                    unsigned int nAabbCursor = 0;
                    Aabb preservedRoot;
                    bUsedPreservedAabb = BuildPreservedAabbRecursive(node.Walkmesh.TempAabbNodes, nAabbCursor, preservedRoot) &&
                                         nAabbCursor == node.Walkmesh.TempAabbNodes.size();
                    if(bUsedPreservedAabb){
                        node.Walkmesh.RootAabb = preservedRoot;
                    }
                    else{
                        throw mdlexception("Preserved AABB tree for '" + Data.MH.Names.at(node.Head.nNameIndex).sName +
                                           "' is malformed; refusing to rebuild it from faces and erase the supplied tree structure.");
                    }
                }
                if(!bUsedPreservedAabb){
                    BuildAabb(node.Walkmesh.RootAabb, allfaces, &file2);
                }
                else{
                    file2 << "Preserved AABB tree from ASCII; offsets will be regenerated during binary write.\r\n";
                }

                //Write to Wok
                std::stringstream file;
                ReportMdl << "Should write wok.\n";
                Wok->WriteWok(node, Data.MH.vLytPosition, &file);
                file << "\r\n\r\nAABB\r\n";
                file << file2.str();

                if(bDebug){
                    std::wstring sDir = JoinPath(ParentPath(GetFullPath()), L"debug_aabb.txt");
                    ReportMdl << "Will write aabb debug to: " << to_ansi(sDir.c_str()) << "\n";
                    HANDLE hFile = bead_CreateWriteFile(sDir);

                    //if(!filewrite.is_open()){
                    if(hFile == INVALID_HANDLE_VALUE){
                        ReportMdl << "'debug_aabb.txt' does not exist. No debug will be written.\n";
                    }
                    else{
                        //filewrite << file.str();
                        bead_WriteFile(hFile, file.str());
                        //filewrite.close();
                        CloseHandle(hFile);
                    }
                }
            }
        }
        //ReportMdl << "PART 3 - stage 6" << "\n";
        ProgressPos(n);
    }
    //ReportMdl << "Done with PART 3\n";
    ProgressPos(Data.MH.ArrayOfNodes.size());

    /// PART 4 ///
    /// Do the necessary mesh calculations
    /// Mesh: inverted counter
    /// Skin: T-bones, Q-bones
    /// Saber: inverted counter
    int nMeshCounter = 0;
    for(int n = 0; n < static_cast<int>(Data.MH.ArrayOfNodes.size()); n++){
        Node & node = Data.MH.ArrayOfNodes.at(n);

        if(node.Head.nType & NODE_SABER && !node.Saber.nInvCount1.Valid() && !node.Saber.nInvCount2.Valid()){
            //inverted counter
            nMeshCounter++;
            int Quo = nMeshCounter/100;
            int Mod = nMeshCounter%100;
            node.Saber.nInvCount1 = pown(2, Quo)*100-nMeshCounter + (Mod ? Quo*100 : 0) + (Quo ? 0 : -1);
            nMeshCounter++;
            Quo = nMeshCounter/100;
            Mod = nMeshCounter%100;
            node.Saber.nInvCount2 = pown(2, Quo)*100-nMeshCounter + (Mod ? Quo*100 : 0) + (Quo ? 0 : -1);
        }
        else if(node.Head.nType & NODE_MESH){
            //inverted counter
            if(!node.Mesh.nMeshInvertedCounter.Valid()){
                nMeshCounter++;
                int Quo = nMeshCounter/100;
                int Mod = nMeshCounter%100;
                node.Mesh.nMeshInvertedCounter = pown(2, Quo)*100-nMeshCounter + (Mod ? Quo*100 : 0) + (Quo ? 0 : -1);
            }

            if(node.Head.nType & NODE_SKIN){
                /// First, declare our empty location as a starting point
                /// IIUC, these values can be used to translate skin coords to world coords
                Vector vPath;
                Quaternion qPath;

                /// Now we need to construct a path by adding all the locations from this node through all its parents to the root
                MdlInteger<unsigned short> nIndex = node.Head.nNameIndex; /// The first name index is the name index of the skin we are processing
                while(nIndex.Valid()){
                    int nNodeIndex = GetNodeIndexByNameIndex(nIndex); /// Get the node index for the ancestor
                    if(nNodeIndex == -1) throw mdlexception("T and Q bone calculation error: dealing with a name index that does not have a node in geometry.");
                    Node & ancestor = Data.MH.ArrayOfNodes.at(nNodeIndex); /// Get the ancestor

                    /// Get ancestor location and get the vectors from it
                    Location lAncestor = ancestor.GetLocation();
                    Vector vAncestor = lAncestor.vPosition;
                    Quaternion qAncestor = lAncestor.oOrientation.GetQuaternion();

                    /// Now perform a common operation in this algorithm:
                    /// Multiply the vector by -1, reverse the quaternion, then rotate the vector by the quaternion.
                    /// I assume this is the reverse from applying the move and rotation.
                    vAncestor *= -1.0;
                    qAncestor = qAncestor.reverse();
                    vAncestor.Rotate(qAncestor);

                    /// Add the result to the main vectors, we are simply adding them together to accumulate them
                    vPath += vAncestor;
                    qPath *= qAncestor;
                    // (On the first round, because loc is initialized with the identity orientation, locNode orientation is simply copied)

                    /// Set the new index to the parent of ancestor
                    nIndex = ancestor.Head.nParentIndex;
                }
                /// We now have a vector and quaternion that can be used to translate skin coords into world coords.

                /// Now we will take this transformation as a base and translate back into the skin.
                /// First, copy the path values
                Vector vSkin = vPath; // Make copy
                Quaternion qSkin = qPath; // Make copy

                /// Collect the name indices of the ancestors of the skin in order from the skin to the root
                nIndex = node.Head.nNameIndex; // Take the name index of the skin again
                std::vector<int> Indices; // This vector will contain the indices from the skin to the root in order.
                while(nIndex.Valid()){ // The price we have to pay for not going recursive
                    Indices.push_back(nIndex);
                    int nNodeIndex = GetNodeIndexByNameIndex(nIndex);
                    if(nNodeIndex == -1) throw mdlexception("T and Q bone calculation error: dealing with a name index that does not have a node in geometry.");
                    nIndex = Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nParentIndex;
                }

                /// Now we go from the root to the skin...
                for(std::size_t a = Indices.size(); a-- > 0;){
                    int nNodeIndex = GetNodeIndexByNameIndex(Indices.at(a)); /// Get the node index for the ancestor
                    if(nNodeIndex == -1) throw mdlexception("T and Q bone calculation error: dealing with a name index that does not have a node in geometry.");
                    Node & ancestor = Data.MH.ArrayOfNodes.at(nNodeIndex); /// Get the ancestor

                    /// Get ancestor location and get the vectors from it
                    Location lAncestor = ancestor.GetLocation();
                    Vector vAncestor = lAncestor.vPosition;
                    Quaternion qAncestor = lAncestor.oOrientation.GetQuaternion();

                    /// Preform a different operation:
                    /// Rotate the Ancestor vector by the accumulated Skin quaternion
                    /// This time we are applying the transformation
                    vAncestor.Rotate(qSkin); /// Note: rotating with the Skin rotation!

                    /// Add the result to the Base vectors
                    vSkin += vAncestor;
                    qSkin *= qAncestor;
                }

                /// Now perform a common operation in this algorithm:
                /// Multiply the vector by -1, reverse the quaternion, then rotate the vector by the quaternion.
                /// I assume this is the reverse from applying the move and rotation.
                vSkin *= -1.0;
                qSkin = qSkin.reverse();
                vSkin.Rotate(qSkin);

                /// IIUC, the skin variables now contain the transformation which converts from skin coords to skin coords but with the additional
                /// complication of converting to world coords in between. If the conversions are different, then it makes sense. But I don't know
                /// how they are different.

                /// Now we need to do something similar for all the other nodes, which represent the bones
                /// In other words, now we are calculating tBones and qBones.
                for(Node & bone : FH->MH.ArrayOfNodes){
                    /// Now we will take the transformation from skin coords to world coords as a base and translate into a bone.
                    /// First, copy the path values
                    Vector vBone = vPath; //Make copy
                    Quaternion qBone = qPath; //Make copy

                    /// Collect the name indices of the ancestors of the bone in order from the bone to the root
                    nIndex = bone.Head.nNameIndex; // Take the name index of the bone
                    Indices.clear(); // Reuse the indices vector
                    while(nIndex.Valid()){ // The price we have to pay for not going recursive
                        Indices.push_back(nIndex);
                        int nNodeIndex = GetNodeIndexByNameIndex(nIndex);
                        if(nNodeIndex == -1) throw mdlexception("T and Q bone calculation error: dealing with a name index that does not have a node in geometry.");
                        nIndex = Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nParentIndex;
                    }

                    /// Now we go from the root to the bone...
                    for(std::size_t a = Indices.size(); a-- > 0;){
                        int nNodeIndex = GetNodeIndexByNameIndex(Indices.at(a)); /// Get the node index for the ancestor
                        if(nNodeIndex == -1) throw mdlexception("T and Q bone calculation error: dealing with a name index that does not have a node in geometry.");
                        Node & ancestor = Data.MH.ArrayOfNodes.at(nNodeIndex); /// Get the ancestor

                        /// Get ancestor location and get the vectors from it
                        Location lAncestor = ancestor.GetLocation();
                        Vector vAncestor = lAncestor.vPosition;
                        Quaternion qAncestor = lAncestor.oOrientation.GetQuaternion();

                        /// Preform a different operation:
                        /// Rotate the Ancestor vector by the accumulated Bone quaternion
                        vAncestor.Rotate(qBone); /// Note: rotating with the Bone rotation!

                        /// Add the result to the Bone vectors
                        vBone += vAncestor;
                        qBone *= qAncestor;
                    }

                    /// This code should fix the t-bone problems. Solution by ndix UR.
                    /// So in order for this transformation to reach the bones, we must apply the SkinDiff transformation.
                    vBone += vSkin;

                    /// Now perform a common operation in this algorithm:
                    /// Multiply the vector by -1, reverse the quaternion, then rotate the vector by the quaternion.
                    /// I assume this is the reverse from applying the move and rotation.
                    vBone *= -1.0;
                    qBone = qBone.reverse();
                    vBone.Rotate(qBone);
                    /// We now have the vector and quaternion for the transformation from bone coords to skin coords.

                    /// Convert the name index into the tree order index
                    int nNodeIndex = 0;
                    for(int ind2 = 0; ind2 < static_cast<int>(Data.MH.NameIndicesInTreeOrder.size()); ind2++)
                        if(Data.MH.NameIndicesInTreeOrder.at(ind2) == bone.Head.nNameIndex)
                            nNodeIndex = ind2;

                    /// Apply the tBone and qBone
                    node.Skin.Bones.at(nNodeIndex).TBone = vBone;
                    node.Skin.Bones.at(nNodeIndex).QBone.SetQuaternion(qBone);
                }
            }
        }
    }
    //ReportMdl << "Done with PART 4\n";

    /// PART 5 ///
    /// Create patches through linked faces
    /// This will take a while and needs to be optimized for speed, anything that can be taken out of it, should be
    CreatePatches();

    /// PART 6 ///
    /// Calculate vertex normals and vertex tangent space vectors
    Report("Calculating vertex normals and vertex tangent space vectors...");
    for(int pg = 0; pg < static_cast<int>(Data.MH.PatchArrayPointers.size()); pg++){
        std::vector<Patch> & patch_group = Data.MH.PatchArrayPointers.at(pg);
        int nPatchCount = Data.MH.PatchArrayPointers.at(pg).size();

        /*****************************/
        /**** 1 - First Patch Loop ***/
        /**
                In this one, we will only calculate the candidates for every patch separately.
        **/
        /*****************************/
        for(int p = 0; p < nPatchCount; p++){
            Patch & patch = Data.MH.PatchArrayPointers.at(pg).at(p);

            CalculateWorld(*this, Data, patch, true, true);
        }

        /*****************************/
        /*** 2 - Second Patch Loop ***/
        /**
                In this one, we will actually calculate the vertex normals and tangent space vectors.
        **/
        /*****************************/
        for(int p = 0; p < nPatchCount; p++){
            Patch & patch = Data.MH.PatchArrayPointers.at(pg).at(p);
            Node & node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
            Vertex & vert = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex).Mesh.Vertices.at(patch.nVertex);

            /// Determine for this patch which patches it smooths to
            for(int n = 0; n < static_cast<int>(patch_group.size()); n++){
                Patch & curpatch = patch_group.at(n);

                /// If this patch is not smoothed to, continue
                if(&patch != &curpatch && !(curpatch.nSmoothingGroups & patch.nSmoothingGroups)) continue;

                /// Otherwise, add this patch's index to our patch's smoothed patches
                patch.SmoothedPatches.push_back(n);
            }

            /// Calculate both the vertex normals and tangent space vectors
            CalculateVertex(*this, Data, patch, true, true);

            /// Apply the candidates. If ASCII supplied explicit normals, preserve
            /// those binary normals instead of recomputing them from smoothing groups.
            if(!node.Mesh.bPreserveMdxNormals) vert.MDXData.vNormal = patch.vVertexNormal;
            if(!node.Mesh.bPreserveMdxTangent1){
                vert.MDXData.vTangent1.at(0) = patch.vVertexB;
                vert.MDXData.vTangent1.at(1) = patch.vVertexT;
                vert.MDXData.vTangent1.at(2) = patch.vVertexN;
            }
        }
    }

    /// No need for the patches anymore, get rid of them
    Data.MH.PatchArrayPointers.clear();
    Data.MH.PatchArrayPointers.shrink_to_fit();

    /// DONE ///
    ReportMdl << "Done post-processing ascii...\n";
}


void MDL::BwmAsciiPostProcess(BWMHeader & data, std::vector<Vector> & vertices, bool bDwk){
    data.nType = 0;
    if(bDwk){
        data.vDwk1 = data.vUse1 + data.vPosition;
        data.vDwk2 = data.vUse2 + data.vPosition;
    }

    for(int f = 0; f < static_cast<int>(data.faces.size()); f++){
        Face & face = data.faces.at(f);
        for(int i = 0; i < 3; i++){
            if(!face.bProcessed[i]){
                Vertex vert;

                if(vertices.size() > 0){
                    if(!face.nIndexVertex[i].Valid() || static_cast<unsigned short>(face.nIndexVertex[i]) >= vertices.size()){
                        throw mdlexception("ASCII walkmesh face references a vertex outside the source vertex array.");
                    }
                    vert.assign(vertices.at(face.nIndexVertex[i]));
                }

                //Find identical verts
                for(int f2 = f; f2 < static_cast<int>(data.faces.size()); f2++){
                    Face & face2 = data.faces.at(f2);
                    for(int i2 = 0; i2 < 3; i2++){
                        //Make sure that we're only changing what's past our current position if we are in the same face.
                        if(f2 != f || i2 > i){
                            if( (face2.nIndexVertex[i2] == face.nIndexVertex[i]) &&
                                !face2.bProcessed[i2] )
                            {
                                face2.nIndexVertex[i2] = CheckedAsciiVertexIndex(data.verts.size(), "ASCII walkmesh post-processing");
                                face2.bProcessed[i2] = true;
                            }
                        }
                    }
                }

                //Now we're allowed to link the original vert as well
                face.nIndexVertex[i] = CheckedAsciiVertexIndex(data.verts.size(), "ASCII walkmesh post-processing");
                face.bProcessed[i] = true;

                //Put the new vert into the array
                data.verts.push_back(std::move(vert));
            }
        }

        Vector & v1 = data.verts.at(face.nIndexVertex[0]);
        Vector & v2 = data.verts.at(face.nIndexVertex[1]);
        Vector & v3 = data.verts.at(face.nIndexVertex[2]);
        Vector Edge1 = v2 - v1;
        Vector Edge2 = v3 - v1;

        //This is for face normal
        face.vNormal = cross(Edge1, Edge2); //Cross product, unnormalized
        face.vNormal.Normalize();

        //This is for the distance.
        face.fDistance = - (face.vNormal.fX * v1.fX +
                            face.vNormal.fY * v1.fY +
                            face.vNormal.fZ * v1.fZ);
    }

    /*
    //Calculate adjacent edges
    for(int f = 0; f < static_cast<int>(data.faces.size()); f++){
        Face & face = data.faces.at(f);

        face.nID = f;

        // Skip if none is -1
        if((face.nAdjacentFaces[0]==-1 ||
           face.nAdjacentFaces[1]==-1 ||
           face.nAdjacentFaces[2]==-1 ) && IsMaterialWalkable(face.nMaterialID)){
            //Go through all the faces coming after this one
            for(int f2 = f+1; f2 < data.faces.size(); f2++){
                Face & compareface = data.faces.at(f2);
                if(IsMaterialWalkable(compareface.nMaterialID)){
                    std::vector<bool> VertMatches(3, false);
                    std::vector<bool> VertMatchesCompare(3, false);
                    for(int i = 0; i < 3; i++){
                        int nVertIndex = face.nIndexVertex[i];
                        Vector & ourvect = data.verts.at(nVertIndex);
                        for(int i2 = 0; i2 < 3; i2++){
                            Vector & othervect = data.verts.at(compareface.nIndexVertex[i2]);
                            if(ourvect.Compare(othervect)){
                                VertMatches.at(i) = true;
                                VertMatchesCompare.at(i2) = true;
                                i2 = 3; // we can only have one matching vert in a face per vert. Once we find a match, we're done.
                            }
                        }
                    }
                    int vertmatch = -1;
                    int comparevertmatch = -1;
                    if(VertMatches.at(0) && VertMatches.at(1)) vertmatch = 0;
                    else if(VertMatches.at(1) && VertMatches.at(2)) vertmatch = 1;
                    else if(VertMatches.at(2) && VertMatches.at(0)) vertmatch = 2;
                    if(VertMatchesCompare.at(0) && VertMatchesCompare.at(1)) comparevertmatch = 0;
                    else if(VertMatchesCompare.at(1) && VertMatchesCompare.at(2)) comparevertmatch = 1;
                    else if(VertMatchesCompare.at(2) && VertMatchesCompare.at(0)) comparevertmatch = 2;
                    if(vertmatch != -1 && comparevertmatch != -1){
                        if(VertMatches.at(0) && VertMatches.at(1)){
                            if(face.nAdjacentFaces[0] != -1) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 0...\n";
                            else face.nAdjacentFaces[0] = f2*3 + comparevertmatch;
                        }
                        else if(VertMatches.at(1) && VertMatches.at(2)){
                            if(face.nAdjacentFaces[1] != -1) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 1...\n";
                            else face.nAdjacentFaces[1] = f2*3 + comparevertmatch;
                        }
                        else if(VertMatches.at(2) && VertMatches.at(0)){
                            if(face.nAdjacentFaces[2] != -1) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 2...\n";
                            else face.nAdjacentFaces[2] = f2*3 + comparevertmatch;
                        }
                        if(VertMatchesCompare.at(0) && VertMatchesCompare.at(1)){
                            if(compareface.nAdjacentFaces[0] != -1) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 0...\n";
                            else compareface.nAdjacentFaces[0] = f*3 + vertmatch;
                        }
                        else if(VertMatchesCompare.at(1) && VertMatchesCompare.at(2)){
                            if(compareface.nAdjacentFaces[1] != -1) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 1...\n";
                            else compareface.nAdjacentFaces[1] = f*3 + vertmatch;
                        }
                        else if(VertMatchesCompare.at(2) && VertMatchesCompare.at(0)){
                            if(compareface.nAdjacentFaces[2] != -1) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 2...\n";
                            else compareface.nAdjacentFaces[2] = f*3 + vertmatch;
                        }
                    }
                    if(face.nAdjacentFaces[0]!=-1 &&
                       face.nAdjacentFaces[1]!=-1 &&
                       face.nAdjacentFaces[2]!=-1 ){
                        f2 = data.faces.size(); //Found them all, maybe I finish early?
                    }
                }
            }
        }
    }
    */
}
