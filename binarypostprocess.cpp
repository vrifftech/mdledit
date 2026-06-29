#include "MDL.h"
#include <atomic>
#include <iomanip>
#include <shlwapi.h>
#include <algorithm>
#include <cmath>
#include <utility>

/**
    Functions:
    MDL::DetermineSmoothing()
    MDL::ConsolidateSmoothingGroups()
    MDL::GenerateSmoothingNumber()
    MDL::FindNormal()
    MDL::FindTangentSpace()
*/

extern std::atomic_bool bCancelSG;

namespace{
    inline void CalculatePatchWorld(MDL & mdl, FileHeader & Data, Patch & patch, bool bNormal, bool bTangent){
        if(!bNormal && !bTangent) return;
        Node & patch_node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
        for(int face_ind : patch.FaceIndices){
            Face & face = patch_node.Mesh.Faces.at(face_ind);

            Vertex & v1 = patch_node.Mesh.Vertices.at(face.nIndexVertex[0]);
            Vertex & v2 = patch_node.Mesh.Vertices.at(face.nIndexVertex[1]);
            Vertex & v3 = patch_node.Mesh.Vertices.at(face.nIndexVertex[2]);

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
                Vector & UVvert1 = v1.MDXData.vUV1;
                Vector & UVvert2 = v2.MDXData.vUV1;
                Vector & UVvert3 = v3.MDXData.vUV1;
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

    inline void CalculatePatch(MDL & /*mdl*/, FileHeader & Data, Patch & patch, bool bNormal, bool bTangent, std::vector<MdlInteger<unsigned int>> * patches = nullptr){
        if(!bNormal && !bTangent) return;
        if(!patches) patches = &patch.SmoothedPatches;

        std::vector<Patch> & patch_group = Data.MH.PatchArrayPointers.at(patch.nPatchGroup);
        std::vector<MdlInteger<unsigned int>> & smoothed_patches = *patches;

        Vector vWorking, vWorkingTST, vWorkingTSB, vWorkingTSN;

        patch.bGroupBadGeo = false;
        patch.bGroupBadUV = false;

        std::vector<int> CheckedPatches;
        CheckedPatches.reserve(smoothed_patches.size());
        for(int n = 0; n < static_cast<int>(smoothed_patches.size()); n++){
            Patch & curpatch = patch_group.at(smoothed_patches.at(n));

            if(std::find(CheckedPatches.begin(), CheckedPatches.end(), curpatch.nNodeArrayIndex) != CheckedPatches.end()) continue;
            CheckedPatches.push_back(curpatch.nNodeArrayIndex);

            int nNormals = 0;
            Vector vMeshNormal, vMeshTST, vMeshTSB, vMeshTSN;

            for(int n2 = n; n2 < static_cast<int>(smoothed_patches.size()); n2++){
                Patch & curpatch2 = patch_group.at(smoothed_patches.at(n2));

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

    struct SmoothingMutationGuard{
        struct MeshState{
            std::vector<Face> Faces;
            std::vector<MdlInteger<unsigned int>> LinkedFaces;
        };

        FileHeader & Data;
        std::vector<std::vector<Patch>> PatchArrayPointers;
        unsigned int nTotalVertCount;
        unsigned int nTotalTangent1Count;
        unsigned int nTotalTangent2Count;
        unsigned int nTotalTangent3Count;
        unsigned int nTotalTangent4Count;
        std::vector<MeshState> MeshStates;
        bool bCommit = false;

        explicit SmoothingMutationGuard(FileHeader & data) :
            Data(data),
            PatchArrayPointers(data.MH.PatchArrayPointers),
            nTotalVertCount(data.MH.nTotalVertCount),
            nTotalTangent1Count(data.MH.nTotalTangent1Count),
            nTotalTangent2Count(data.MH.nTotalTangent2Count),
            nTotalTangent3Count(data.MH.nTotalTangent3Count),
            nTotalTangent4Count(data.MH.nTotalTangent4Count)
        {
            MeshStates.reserve(Data.MH.ArrayOfNodes.size());
            for(Node & node : Data.MH.ArrayOfNodes){
                MeshState state;
                if(node.Head.nType & NODE_MESH){
                    state.Faces = node.Mesh.Faces;
                    state.LinkedFaces.reserve(node.Mesh.Vertices.size());
                    for(const Vertex & vert : node.Mesh.Vertices){
                        state.LinkedFaces.push_back(vert.nLinkedFacesIndex);
                    }
                }
                MeshStates.push_back(std::move(state));
            }
        }

        void Commit(){ bCommit = true; }

        ~SmoothingMutationGuard(){
            if(bCommit) return;
            Data.MH.PatchArrayPointers = std::move(PatchArrayPointers);
            Data.MH.nTotalVertCount = nTotalVertCount;
            Data.MH.nTotalTangent1Count = nTotalTangent1Count;
            Data.MH.nTotalTangent2Count = nTotalTangent2Count;
            Data.MH.nTotalTangent3Count = nTotalTangent3Count;
            Data.MH.nTotalTangent4Count = nTotalTangent4Count;

            const std::size_t nNodes = std::min(MeshStates.size(), Data.MH.ArrayOfNodes.size());
            for(std::size_t n = 0; n < nNodes; ++n){
                Node & node = Data.MH.ArrayOfNodes.at(n);
                MeshState & state = MeshStates.at(n);
                if(!(node.Head.nType & NODE_MESH)) continue;
                if(!state.Faces.empty() || !node.Mesh.Faces.empty()){
                    node.Mesh.Faces = std::move(state.Faces);
                }
                const std::size_t nVerts = std::min(state.LinkedFaces.size(), node.Mesh.Vertices.size());
                for(std::size_t v = 0; v < nVerts; ++v){
                    node.Mesh.Vertices.at(v).nLinkedFacesIndex = state.LinkedFaces.at(v);
                }
            }
        }
    };

}


/// This function is the main binary decompilation post-processing function.
void MDL::DetermineSmoothing(){
    FileHeader & Data = *FH;
    SmoothingMutationGuard smoothingGuard(Data);
    ReportObject ReportMdl (*this);
    Timer tBinPostProcess;

    //Create file /stringstream)
    std::stringstream file, summary;
    std::stringstream fileaabb;

    /** In this part, I will go through all the nodes and calculate for all meshes:
        1. face IDs
        2. aabb node face centroids
        3. face area and face UV area
        4. If debug is on, calculate the aabb tree and compare it.
    **/
    for(Node & node : Data.MH.ArrayOfNodes){
        if(node.Head.nType & NODE_MESH && !(node.Head.nType & NODE_SABER)){
            for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
                Face & face = node.Mesh.Faces.at(f);
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

                /// Mark faces with their ID
                face.nID = f;

                /// Calculate Centroid
                if(node.Head.nType & NODE_AABB){
                    face.vBBmax = Vector(-10000.0, -10000.0, -10000.0);
                    face.vBBmin = Vector(10000.0, 10000.0, 10000.0);
                    face.vCentroid = Vector(0.0, 0.0, 0.0);
                    for(int i = 0; i < 3; i++){
                        face.vBBmax.fX = std::max(face.vBBmax.fX, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fX);
                        face.vBBmax.fY = std::max(face.vBBmax.fY, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fY);
                        face.vBBmax.fZ = std::max(face.vBBmax.fZ, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fZ);
                        face.vBBmin.fX = std::min(face.vBBmin.fX, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fX);
                        face.vBBmin.fY = std::min(face.vBBmin.fY, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fY);
                        face.vBBmin.fZ = std::min(face.vBBmin.fZ, node.Mesh.Vertices.at(face.nIndexVertex.at(i)).fZ);
                        face.vCentroid += node.Mesh.Vertices.at(face.nIndexVertex.at(i));
                    }
                    face.vCentroid /= 3.0;
                }

                /// Area calculation
                face.fArea = HeronFormulaEdge(Edge1, Edge2, Edge3);
                face.fAreaUV = HeronFormulaEdge(EUV1, EUV2, EUV3);

                /// Cache per-face tangent-space vectors before the recursive
                /// smoothing search. The recursion adds these face vectors
                /// directly to the proposed base vectors.
                Vector vFaceNormal = cross(Edge1, Edge2);
                vFaceNormal.Normalize();
                double r = (EUV1.fX * EUV2.fY - EUV1.fY * EUV2.fX);
                if(r != 0.0) r = 1.0 / r;
                else r = 2406.6388;

                face.vTangent = r * (Edge1 * EUV2.fY - Edge2 * EUV1.fY);
                face.vBitangent = r * (Edge2 * EUV1.fX - Edge1 * EUV2.fX);
                face.vTangent.Normalize();
                face.vBitangent.Normalize();
                if(face.vTangent.Null()) face.vTangent = Vector(1.0, 0.0, 0.0);
                if(face.vBitangent.Null()) face.vBitangent = Vector(1.0, 0.0, 0.0);

                Vector vCross = cross(vFaceNormal, face.vTangent);
                double fDot = dot(vCross, face.vBitangent);
                if(fDot > 0.0000000001) face.vTangent *= -1.0;

                Vector vNormalUV = cross(EUV1, EUV2);
                if(vNormalUV.fZ < 0.0){
                    face.vTangent *= -1.0;
                    face.vBitangent *= -1.0;
                }
            }

            /// DEBUG: Verify aabb tree calculation
            if(bDebug && node.Head.nType & NODE_AABB){
                std::vector<Aabb> array1;
                std::vector<Aabb> array2;
                std::vector<Face*> faceptrs;
                for(int f = 0; f < static_cast<int>(node.Mesh.Faces.size()); f++){
                    faceptrs.push_back(&node.Mesh.Faces.at(f));
                }
                Aabb RecalculationAabb;
                std::stringstream ssTemp;
                BuildAabb(RecalculationAabb, faceptrs, &ssTemp);
                LinearizeAabb(RecalculationAabb, array1);
                Aabb VanillaCopy = node.Walkmesh.RootAabb;
                LinearizeAabb(VanillaCopy, array2);
                if(array1.size() != array2.size()) fileaabb << "ERROR! Aabb arrays have different sizes!";
                else{
                    fileaabb << "Vanilla vs New";
                    int nGood = 0;
                    int nTotal = 0;
                    std::stringstream ssTemp2;
                    for(int a = 0; a < static_cast<int>(array1.size()); a++){
                        bool bEq = (array1.at(a).nID == array2.at(a).nID);
                        if(array2.at(a).nID.Valid()){
                            nTotal++;
                            if(bEq) nGood++;
                        }
                        ssTemp2 << "\n (" << a << ") " << array2.at(a).nID.Print() << " " << (bEq? "==" : "!=") << " " << array1.at(a).nID.Print() << (bEq? "" : " DIFFERENT!!");
                    }
                    fileaabb << "\nCorrect: " << nGood << "/" << (nTotal) << " (" << std::setprecision(4) << ((double) nGood / (double) nTotal * 100.0) << "%)\n";
                    fileaabb << ssTemp2.str();
                    fileaabb << "\n\n\n" << ssTemp.str();
                }
            }
        }
    }

    bCancelSG.store(false); /// Reset the cancel here, we can only cancel if we're creating patches, recalculating vectors or calculating smoothing groups.

    //Create patches
    CreatePatches();

    if(bCancelSG.load()){
        /// Canceled during CreatePatches(), clean up the patches and return this function
        Data.MH.PatchArrayPointers.clear();
        Data.MH.PatchArrayPointers.shrink_to_fit();
        return;
    }

    /// Statistics counters
    int nNumOfFoundNormals = 0;
    int nNumOfFoundTS = 0;
    int nNumOfFoundTSB = 0;
    int nNumOfFoundTST = 0;
    int nNumOfFoundTSN = 0;
    int nBadUV = 0;
    int nBadGeo = 0;
    int nTangentPerfect = 0;

    /// Patch vector compare precision
    const double fSmoothingDiff = bXbox ? 0.01 : 0.00001;

    Timer tVectors;
    ProgressSize(0, Data.MH.PatchArrayPointers.size());
    ProgressPos(0);

    /// Prepare the SG array here, because the SGs are the same across all meshes.
    std::vector<std::vector<unsigned long int>> nSmoothingGroupNumbers;
    nSmoothingGroupNumbers.resize(Data.MH.PatchArrayPointers.size());

    /// Go through all patch groups
    bool bReportModel = false;
    for(int pg = 0; pg < static_cast<int>(Data.MH.PatchArrayPointers.size()); pg++){
        if(bCancelSG.load()){
            Data.MH.PatchArrayPointers.clear();
            Data.MH.PatchArrayPointers.shrink_to_fit();
            return;
        }
        std::vector<Patch> & patchgroup = Data.MH.PatchArrayPointers.at(pg);
        if(patchgroup.size() == 0){
            ReportMdl << "Warning: encountered an empty smoothing patch group; skipping it.\n";
            continue;
        }

        if(bDebug) file << (pg > 0 ? "\r\n" : "");

        /// Get the coordinates of the first patch group from the vertex of the first patch
        Node & first_node = Data.MH.ArrayOfNodes.at(patchgroup.front().nNodeArrayIndex);
        Vertex & first_vert = first_node.Mesh.Vertices.at(patchgroup.front().nVertex);
        Vector vFirstNormal = first_vert.MDXData.vNormal;

        /// This boolean will record if all more than one normals are equal
        bool bSingleNormal = true;
        if(patchgroup.size() < 2) bSingleNormal = false; /// If there is only one vert anyway, set it to false
        else for(Patch & patch : patchgroup){
            Node & patch_node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
            Vertex & patch_vert = patch_node.Mesh.Vertices.at(patch.nVertex);
            Vector vThisNormal = patch_vert.MDXData.vNormal;

            if(!vFirstNormal.Compare(vThisNormal, fSmoothingDiff)){
                bSingleNormal = false;
                break;
            }
        }

        /// bCombined marks whether the patch group is made up of more than one node
        bool bCombined = false;
        for(Patch & patch : patchgroup) if(patchgroup.front().nNodeArrayIndex != patch.nNodeArrayIndex){
            bCombined = true;
            break;
        }

        /// Report the patch group
        if(bDebug) file << (bCombined ? "Combined group " : "Group ") << pg << " " << first_vert.vFromRoot.Print() << " - " << patchgroup.size() << " patches.";
        if(bSingleNormal) if(bDebug) file << "     All normals equal. Expecting errors.\n";


        /*****************************/
        /**** 1 - First Patch Loop ***/
        /**
                In this one, we will only calculate the candidates for every patch separately.
        **/
        /*****************************/
        for(int p = 0; p < static_cast<int>(patchgroup.size()); p++){
            Patch & patch = patchgroup.at(p);
            Node & patch_node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);

            /// Report the patch
            if(bDebug) file << "\n" << "-> Patch " << p << "/" << patchgroup.size() - 1 << ", " << GetNodeName(patch_node) << ", vert " << patch.nVertex << ", faces";
            if(bDebug) for(int face_ind : patch.FaceIndices) file << " " << face_ind;

            CalculatePatchWorld(*this, Data, patch, true, true);
        }

        /*****************************/
        /*** 2 - Second Patch Loop ***/
        /**
                In this one, we will try to find the matches for our candidates
        **/
        /*****************************/
        for(int p = 0; p < static_cast<int>(patchgroup.size()); p++){
            Patch & patch = patchgroup.at(p);
            Node & patch_node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
            Vertex & patch_vert = patch_node.Mesh.Vertices.at(patch.nVertex);

            /// Report the patch
            if(bDebug) file << "\n" << "-> Patch " << p << "/" << patchgroup.size() - 1 << ", " << GetNodeName(patch_node) << ", vert " << patch.nVertex;
            std::string sOpener = "\n" + GetNodeName(patch_node) + ", vert " + std::to_string(patch.nVertex);
            if(bDebug) summary << sOpener;
            if(bDebug) for(int i = 0; i < (60 - static_cast<int>(sOpener.length())); i++) summary << " ";

            /// Clear the patch's smoothing arrays, we will be filling them soon
            patch.SmoothedPatches.reserve(patchgroup.size());
            patch.SmoothedPatches.clear();

            /// However, add this patch's index to the smoothed patches, because it is required for the algorithm later on.
            patch.SmoothedPatches.push_back(p);

            /*****************************/
            /***** 1 - Vertex Normal *****/
            /*****************************/

            /// Now we've got to construct a matching vertex normal
            /// Create a boolean to track this:
            bool bFoundVertexNormal = false; /// This boolean marks whether the matching vertex normal has been found

            /// This vector will be used for verification
            Vector vNormal = patch_vert.MDXData.vNormal;
            if(bDebug) file << "\r\n    Looking for normal (" << vNormal.Print() << ")";

            /// Find the match
            if(bSingleNormal){
                /// This is an optimization. If all the patches have the same normal,
                /// then they all must smooth between each other, so it makes no sense singling them out
                /// and looking for the right combination.
                if(bDebug) file << "\r\n    Patch group normals are uniform, apply smoothing to all!";
                bFoundVertexNormal = true;
                for(int p2 = 0; p2 < static_cast<int>(patchgroup.size()); p2++){
                    /// Don't add the index for this very patch because it has been added already above!
                    if(p2 != p) patch.SmoothedPatches.push_back(p2);
                }
            }
            else{
                /// First check if this patch's normal is enough.
                /// ! This calculation is correct only in the simple case that there is only one patch adding to this normal.
                Vector vCheck = patch.vWorldNormal;
                vCheck.Normalize();
                if(bDebug) file << "\r\n    Comparing to base (" << vCheck.Print() << ").";

                /// Non-verification version:
                CalculatePatch(*this, Data, patch, true, false);

                if(patch.vVertexNormal.Compare(patch_vert.MDXData.vNormal, fSmoothingDiff)){ //if(vCheck.Compare(vNormal, fSmoothingDiff)){
                    bFoundVertexNormal = true;
                }
                else if(patchgroup.size() < 12){
                    /// If the number of patches isn't so high as to severely slow down the algorithm,
                    /// run the recursive function to find the matching combination of patches for the normal
                    std::vector<int> nSmoothedPatches;
                    nSmoothedPatches.reserve(patch.SmoothedPatches.size());
                    for(const auto & nSmoothedPatch : patch.SmoothedPatches) if(nSmoothedPatch.Valid()) nSmoothedPatches.push_back(nSmoothedPatch);
                    bFoundVertexNormal = FindNormal(0, static_cast<int>(patchgroup.size()), p, pg, patch.vWorldNormal, patch_vert.MDXData.vNormal, nSmoothedPatches, file);
                }
            }

            if(bDebug){

                if(bFoundVertexNormal) summary << "found";
                else summary << ":(";
            }

            if(bFoundVertexNormal){
                if(bDebug) file << "\n:)    MATCH FOUND!\n";
                nNumOfFoundNormals++;
            }
            else if(patch.bBadGeo){
                if(bDebug) file << "\n:/    BAD GEOMETRY - NO MATCH FOUND!\n";
                nBadGeo++;
                ReportMdl << "Bad geometry for vertex normal in group " << pg
                          << ", patch " << p << "/" << (patchgroup.size() - 1)
                          << ", " << GetNodeName(patch_node)
                          << ", vert " << patch.nVertex << ".\n";
            }
            else{
                if(bDebug) file << "\n:(    NO MATCH FOUND!\n";
                ReportMdl << "No match for vertex normal in group " << pg
                          << ", patch " << p << "/" << (patchgroup.size() - 1)
                          << ", " << GetNodeName(patch_node)
                          << ", vert " << patch.nVertex << ".\n";
                if(!bReportModel && (!bCombined || patchgroup.size() < 3)) bReportModel = true;
            }


            /*****************************/
            /***** 2 - Tangent Space *****/
            /*****************************/

            /**
                This part is tricky. On one hand, we want to verify the existing tangent space vectors. On the other,
                we want to add the missing ones. The best solution is probably to put the whole thing under an if.
            **/

            if(patch_node.Mesh.nMdxDataBitmap & MDX_FLAG_TANGENT1){
                /// Do tangent space verification
                /// Create a boolean to track this:
                char cFoundTangentSpace = 0; /// This char marks whether the matching tangent space vectors have been found

                CalculatePatch(*this, Data, patch, false, true);

                /// Now, let's verify
                if(bDebug) file << "\n   Comparing TS bitangent (" << patch_vert.MDXData.vTangent1.at(0).Print() << ") to base (" << patch.vVertexB.Print() << ").";
                /// Report the vectors we got right
                if(patch.vVertexB.Compare(patch_vert.MDXData.vTangent1.at(0), fSmoothingDiff)){
                    if(bDebug) file << " bitangent";
                    cFoundTangentSpace = cFoundTangentSpace | TS_BITANGENT;
                }
                if(patch.vVertexT.Compare(patch_vert.MDXData.vTangent1.at(1), fSmoothingDiff)){
                    if(bDebug) file << " tangent";
                    cFoundTangentSpace = cFoundTangentSpace | TS_TANGENT;
                }
                if(patch.vVertexN.Compare(patch_vert.MDXData.vTangent1.at(2), fSmoothingDiff)){
                    if(bDebug) file << " normal";
                    cFoundTangentSpace = cFoundTangentSpace | TS_NORMAL;
                }
                if(!cFoundTangentSpace) if(bDebug) file << " none";

                /// Increment the counters and report based on the result
                if(bFoundVertexNormal && cFoundTangentSpace == TS_ALL){
                    nTangentPerfect++; /// Increment this. This is when we have the vertex normal matching with tangent space vectors, the perfect match
                    if(bDebug) file << " (perfect match)";
                }
                else if(patch.bBadUV){ if(bDebug) file << " (bad UV)"; }
                else if(cFoundTangentSpace){ if(bDebug) file << " (incomplete)"; }

                /// If we didn't get a match with the vertex normal and we didn't get any match with the base and the number of patches isn't too big
                if(!bFoundVertexNormal && !cFoundTangentSpace && patchgroup.size() < 12){
                    /// Run recursive function to find the tangent space vector
                    std::vector<int> nSmoothedPatches;
                    nSmoothedPatches.reserve(patch.SmoothedPatches.size());
                    for(const auto & nSmoothedPatch : patch.SmoothedPatches) if(nSmoothedPatch.Valid()) nSmoothedPatches.push_back(nSmoothedPatch);
                    cFoundTangentSpace = FindTangentSpace(0, static_cast<int>(patchgroup.size()), p, pg, patch.vVertexB, patch.vVertexT, patch.vVertexN, patch_vert.MDXData.vTangent1.at(0), patch_vert.MDXData.vTangent1.at(1), patch_vert.MDXData.vTangent1.at(2), nSmoothedPatches, file);

                }

                if(cFoundTangentSpace & TS_BITANGENT) nNumOfFoundTSB++;
                if(cFoundTangentSpace & TS_TANGENT) nNumOfFoundTST++;
                if(cFoundTangentSpace & TS_NORMAL) nNumOfFoundTSN++;

                if(bCombined) if(bDebug) file << " (combined)";

                if(cFoundTangentSpace == TS_ALL){
                    if(bDebug) file << "\n:)    MATCH FOUND!\n";
                    nNumOfFoundTS++;
                }
                else if(patch.bGroupBadUV){
                    if(bDebug) file << "\n:/    BAD UV!\n";
                    nBadUV++;
                }
                else{
                    if(bDebug) file << "\n:(    NO MATCH FOUND!\n";
                }
            }
            else{
                /// Do tangent space calculation
                /// The new algorithm
                /// Go through our patches

                CalculatePatch(*this, Data, patch, false, true);

                /// Add the vectors to the node
                patch_vert.MDXData.vTangent1.at(0) = patch.vVertexB;
                patch_vert.MDXData.vTangent1.at(1) = patch.vVertexT;
                patch_vert.MDXData.vTangent1.at(2) = patch.vVertexN;
            }
        }
        if(bDebug) summary << "\n";
        /** END SECOND PATCH LOOP **/

        /// When we get here all the data in the patch group has been worked over.
        /// Our patches should now contain the info about which patches they smooth to.
        /// Now we need to generate smoothing group numbers

        /// Make an array of smoothing group numbers for this patch group
        for(int n = 0; n < static_cast<int>(patchgroup.size())*(static_cast<int>(patchgroup.size()) - 1)/2 + 1; n++){
            nSmoothingGroupNumbers.at(pg).push_back(0);
        }

        /// Report
        if(bDebug) file << "Getting vertex smoothing groups.\n";

        /// The number of smoothing group numbers used inside this patch group
        int nSmoothingGroupCounter = 0;

        /// For every patch
        for(int p = 0; p < static_cast<int>(patchgroup.size()); p++){
            Patch & patch = patchgroup.at(p);

            /// Report
            if(bDebug) file << "  Patch " << p << " smooths to " << patch.SmoothedPatches.size() << " patches...\n";

            /// Get smoothing group numbers from the array of patches that this patch smooths to
            while(patch.SmoothedPatches.size() > 0){
                /// This is the SmoothedPatchesGroup, which will contain the indices of the patches that all
                /// smooth between each other. This means that they can be assigned a common (single) smoothing group number.
                std::vector<int> SmoothedPatchesGroup;

                /// Get last smoothed patch index
                int p2 = patch.SmoothedPatches.back();

                /// If this is this very patch, simply pop it and continue
                if(p2 == p){
                    patch.SmoothedPatches.pop_back();
                    continue;
                }

                /// Get last smoothed patch
                Patch & patch2 = patchgroup.at(p2);

                /// Add the indices of both patches to the group
                SmoothedPatchesGroup.push_back(p);
                SmoothedPatchesGroup.push_back(p2);

                /// Add the pointers to smoothing group integers, both get the same integer
                patch.SmoothingGroupNumbers.push_back(&(nSmoothingGroupNumbers.at(pg).at(nSmoothingGroupCounter)));
                patch2.SmoothingGroupNumbers.push_back(&(nSmoothingGroupNumbers.at(pg).at(nSmoothingGroupCounter)));

                /// Delete both patches from each other's SmoothedPatches
                patch.SmoothedPatches.pop_back();
                for(int i2 = 0; i2 < static_cast<int>(patch2.SmoothedPatches.size()); i2++){
                    if(patch2.SmoothedPatches.at(i2) == static_cast<unsigned int>(p)){
                        patch2.SmoothedPatches.erase(patch2.SmoothedPatches.begin() + i2);
                        break;
                    }
                }

                /// This function will go through all the patches in this patch group
                /// It will look at the patches that are not yet in SmoothedPatchesGroup and for each one
                /// it will check whether it smooths to all the patches currently in the smoothed patches group.
                /// If it does, then the patch will be added to the group.
                GenerateSmoothingNumber(SmoothedPatchesGroup, nSmoothingGroupNumbers.at(pg), nSmoothingGroupCounter, pg, file);

                /// Report
                if(bDebug) file << "  Added smoothing group " << nSmoothingGroupCounter+1 << " for patches:";
                for(int spg = 0; spg < static_cast<int>(SmoothedPatchesGroup.size()); spg++){
                    file << " " << SmoothedPatchesGroup.at(spg);
                }
                if(bDebug) file << "\n";

                /// Elevate smoothing group counter
                nSmoothingGroupCounter++;
            }
            patch.SmoothedPatches.clear();
            patch.SmoothedPatches.shrink_to_fit();

            /// Once we get here, we have checked (for patch p) all the patches we smooth over to and added their indices
            /// In case the patch doesn't smooth to any other patch, store an additional identity smoothing group for it
            if(patch.SmoothingGroupNumbers.size() == 0){
                if(bDebug) file << "  Patch " << p << " in patch group has no smoothing group, setting it to " << nSmoothingGroupCounter+1 << ".\n";
                patch.SmoothingGroupNumbers.push_back((unsigned long int*) &(nSmoothingGroupNumbers.at(pg).at(nSmoothingGroupCounter)));
                nSmoothingGroupCounter++;
            }

            /// Report
            if(bDebug) file << "  (patch " << p << " now has " << patch.SmoothingGroupNumbers.size() << " smoothing groups)\n";
        }
        ProgressPos(pg);
    }
    ProgressPos(0);
    if(bCancelSG.load()){
        Data.MH.PatchArrayPointers.clear();
        Data.MH.PatchArrayPointers.shrink_to_fit();
        return;
    }
    ReportMdl << "Recalculated vectors in " << tVectors.GetTime() << ".\n";
    if(bDebug) file << "\n" << summary.str();
    /**/

    /// Report results
    const int nNormalTotal = Data.MH.nTotalVertCount - Data.MH.nExcludedVerts;
    const int nNormalTotalWithoutBadGeo = nNormalTotal - nBadGeo;
    double fPercentage = (nNormalTotal > 0)
        ? ((double)nNumOfFoundNormals / (double)nNormalTotal) * 100.0
        : 100.0;
    double fPercentageWithoutBadGeo = (nNormalTotalWithoutBadGeo > 0)
        ? ((double)nNumOfFoundNormals / (double)nNormalTotalWithoutBadGeo) * 100.0
        : (nNormalTotal == 0 ? 100.0 : 0.0);
    double fPercentage2 = (nNormalTotal > 0)
        ? ((double)nBadGeo / (double)nNormalTotal) * 100.0
        : 0.0;

    /// Judge whether smoothing-group recovery is usable after excluding
    /// vertices rejected as bad geometry.
    bool bGoodEnough = (fPercentageWithoutBadGeo > 80.0 || nNormalTotal == 0);

    ReportMdl << "Found normals: " << nNumOfFoundNormals << "/" << nNormalTotal;
    if(nNormalTotal > 0) ReportMdl << " (" << round(fPercentage * 100.0) / 100.0 << "%)";
    ReportMdl << "\n";
    if(nNormalTotal > nNumOfFoundNormals && nBadGeo > 0){
        ReportMdl << "  Without bad geometry: " << nNumOfFoundNormals << "/" << nNormalTotalWithoutBadGeo;
        if(nNormalTotalWithoutBadGeo > 0) ReportMdl << " (" << round(fPercentageWithoutBadGeo * 100.0) / 100.0 << "%)";
        ReportMdl << "\n";
    }

    if(Data.MH.nTotalTangent1Count > 0){
        fPercentage = ((double)nNumOfFoundTS / (double)Data.MH.nTotalTangent1Count) * 100.0;
        fPercentage2 = (Data.MH.nTotalTangent1Count - nBadUV > 0)
            ? ((double)nNumOfFoundTS / (double)(Data.MH.nTotalTangent1Count - nBadUV)) * 100.0
            : 0.0;
        ReportMdl << "Found tangent spaces: " << nNumOfFoundTS << "/" << Data.MH.nTotalTangent1Count << " (" << std::setprecision(4) << fPercentage << "%)\n";
        ReportMdl << "  Without bad UVs: " << nNumOfFoundTS << "/" << (Data.MH.nTotalTangent1Count - nBadUV) << " (" << std::setprecision(4) << fPercentage2 << "%)\n";
        fPercentage2 = ((double)nTangentPerfect / (double)(Data.MH.nTotalTangent1Count)) * 100.0;
        ReportMdl << "  Perfect matches: " << nTangentPerfect << "/" << (Data.MH.nTotalTangent1Count) << " (" << std::setprecision(4) << fPercentage2 << "%)\n";
        fPercentage = ((double)nNumOfFoundTSB / (double)Data.MH.nTotalTangent1Count) * 100.0;
        fPercentage2 = (Data.MH.nTotalTangent1Count - nBadUV > 0)
            ? ((double)nNumOfFoundTSB / (double)(Data.MH.nTotalTangent1Count - nBadUV)) * 100.0
            : 0.0;
        ReportMdl << "  Found bitangents: " << nNumOfFoundTSB << "/" << Data.MH.nTotalTangent1Count << " (" << std::setprecision(4) << fPercentage << "%)\n";
        ReportMdl << "    Without bad UVs: " << nNumOfFoundTSB << "/" << (Data.MH.nTotalTangent1Count - nBadUV) << " (" << std::setprecision(4) << fPercentage2 << "%)\n";
        fPercentage = ((double)nNumOfFoundTST / (double)Data.MH.nTotalTangent1Count) * 100.0;
        fPercentage2 = (Data.MH.nTotalTangent1Count - nBadUV > 0)
            ? ((double)nNumOfFoundTST / (double)(Data.MH.nTotalTangent1Count - nBadUV)) * 100.0
            : 0.0;
        ReportMdl << "  Found tangents: " << nNumOfFoundTST << "/" << Data.MH.nTotalTangent1Count << " (" << std::setprecision(4) << fPercentage << "%)\n";
        ReportMdl << "    Without bad UVs: " << nNumOfFoundTST << "/" << (Data.MH.nTotalTangent1Count - nBadUV) << " (" << std::setprecision(4) << fPercentage2 << "%)\n";
        fPercentage = ((double)nNumOfFoundTSN / (double)Data.MH.nTotalTangent1Count) * 100.0;
        fPercentage2 = (Data.MH.nTotalTangent1Count - nBadUV > 0)
            ? ((double)nNumOfFoundTSN / (double)(Data.MH.nTotalTangent1Count - nBadUV)) * 100.0
            : 0.0;
        ReportMdl << "  Found normals: " << nNumOfFoundTSN << "/" << Data.MH.nTotalTangent1Count << " (" << std::setprecision(4) << fPercentage << "%)\n";
        ReportMdl << "    Without bad UVs: " << nNumOfFoundTSN << "/" << (Data.MH.nTotalTangent1Count - nBadUV) << " (" << std::setprecision(4) << fPercentage2 << "%)\n";
    }

    /// Smoothing-group recovery is only a best-effort ASCII convenience.
    /// Binary MDL/MDX normals and tangent bases are exported explicitly, so a
    /// poor smoothing reconstruction must not block opening/decompiling a valid
    /// model. Continue with the partial/conservative smoothing data we found.
    if(!bGoodEnough){
        ReportMdl << "Warning: smoothing-group recovery was unreliable; exported explicit normals/tangents remain authoritative.\n";
    }

    Report("Calculating smoothing groups...");

    /// This array will keep track of which patch groups we've processed already
    std::vector<bool> DonePatches(Data.MH.PatchArrayPointers.size(), false);

    /// Go through all the patch groups
    //file << "\n\n== Consolidation ==";
    for(int pg = 0; pg < static_cast<int>(Data.MH.PatchArrayPointers.size()); pg++){
        ConsolidateSmoothingGroups(pg, nSmoothingGroupNumbers, DonePatches);
        if(bCancelSG.load()){
            Data.MH.PatchArrayPointers.clear();
            Data.MH.PatchArrayPointers.shrink_to_fit();
            return;
        }
    }

    /// And finally finally, we merge the numbers for every face
    for(std::vector<Patch> & patchgroup : Data.MH.PatchArrayPointers){
        for(Patch & patch : patchgroup){
            unsigned long int nExistingSG = 0;
            for(int i = 0; i < static_cast<int>(patch.SmoothingGroupNumbers.size()); i++){
                nExistingSG |= *patch.SmoothingGroupNumbers.at(i);
            }
            patch.nSmoothingGroups = (unsigned int) nExistingSG;
            for(int face_ind : patch.FaceIndices){
                Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex).Mesh.Faces.at(face_ind).nSmoothingGroup |= nExistingSG;
            }
        }
    }

    /// Get rid of the patch array.
    Data.MH.PatchArrayPointers.clear();
    Data.MH.PatchArrayPointers.shrink_to_fit();

    /// If debug is on, print out all the debug info we've gathered
    if(bDebug){
        std::wstring sDir = JoinPath(ParentPath(path.sFullPath), L"debug.txt");
        std::wstring sDir2 = JoinPath(ParentPath(path.sFullPath), L"debug_aabb_comp.txt");
        ReportMdl << "Writing smoothing debug: " << to_ansi(std::wstring(sDir.c_str())) << "\n";
        ReportMdl << "Writing aabb debug: " << to_ansi(std::wstring(sDir2.c_str())) << "\n";
        HANDLE hFile = bead_CreateWriteFile(sDir);
        HANDLE hFile2 = bead_CreateWriteFile(sDir2);

        if(hFile == INVALID_HANDLE_VALUE){
            ReportMdl << "'debug.txt' does not exist. No debug will be written.\n";
        }
        else{
            bead_WriteFile(hFile, file.str());
            CloseHandle(hFile);
        }

        if(hFile2 == INVALID_HANDLE_VALUE){
            ReportMdl << "'debug_aabb_comp.txt' does not exist. No debug will be written.\n";
        }
        else{
            bead_WriteFile(hFile2, fileaabb.str());
            CloseHandle(hFile2);
        }
    }

    smoothingGuard.Commit();
    ReportMdl << "Done calculating smoothing groups!\n";
}

/** Algorithm
  1. Go through all of the patches in the patch group.
  2. If number is not single, skip it. Else if there is no assigned number for the patch
     make the next number in sequence on the patch.
  3. Go through all the faces and assign the same
     number to all the other single corners, then mark the face as processed.
  4. now we go through the processed faces and apply the same algorithm
*/
void MDL::ConsolidateSmoothingGroups(int nPatchGroup, std::vector<std::vector<unsigned long int>> & Numbers, std::vector<bool> & DoneGroups){
    if(bCancelSG.load()) return;
    FileHeader & Data = *FH;
    for(int p = 0; p < static_cast<int>(Data.MH.PatchArrayPointers.at(nPatchGroup).size()); p++){
        Patch & patch = Data.MH.PatchArrayPointers.at(nPatchGroup).at(p);
        if(patch.SmoothingGroupNumbers.size() == 1){
            if(*patch.SmoothingGroupNumbers.front() == 0){
                unsigned long int nBitflag = 0;

                /// First get all the used numbers
                for(int num = 0; num < static_cast<int>(Numbers.at(nPatchGroup).size()); num++){
                    nBitflag = nBitflag | Numbers.at(nPatchGroup).at(num);
                }

                /// Now find the first unused one and use it.
                for(int n = 0; *patch.SmoothingGroupNumbers.front() == 0 && n < 32; n++){
                    if(!(nBitflag & pown(2, n))) *patch.SmoothingGroupNumbers.front() = pown(2, n);
                }
            }

            for(int f = 0; f < static_cast<int>(patch.FaceIndices.size()); f++){
                Node & node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
                Face & face = node.Mesh.Faces.at(patch.FaceIndices.at(f));

                /// Skip if processed
                if(face.bProcessedSG) continue;

                /// Okay, we've got one corner, now we need to check the other two corners if they're single
                for(int v = 0; v < 3; v++){
                    Vertex & vert = node.Mesh.Vertices.at(face.nIndexVertex[v]);

                    /// Don't process yourself
                    if(face.nIndexVertex[v] == patch.nVertex) continue;

                    /// We get every vert's patch group
                    std::vector<Patch> & PatchVector = Data.MH.PatchArrayPointers.at(vert.nLinkedFacesIndex);
                    bool bFound = false;
                    int p2 = 0;
                    while(!bFound && p2 < static_cast<int>(PatchVector.size())){
                        /// Find the patch with the same vert index as stored with the face
                        if(patch.nNodeArrayIndex == PatchVector.at(p2).nNodeArrayIndex && PatchVector.at(p2).nVertex == face.nIndexVertex[v]){
                            bFound = true;
                        }
                        else p2++;
                    }
                    if(p2 == static_cast<int>(PatchVector.size())) break;
                    /// We get the patch that contains the current vert
                    Patch & vertpatch = PatchVector.at(p2);
                    if(vertpatch.SmoothingGroupNumbers.size() == 1){
                        /// Finally, copy the number to a single corner
                        *vertpatch.SmoothingGroupNumbers.front() = *patch.SmoothingGroupNumbers.front();
                    }
                }
                face.bProcessedSG = true;
            }
        }
    }

    DoneGroups.at(nPatchGroup) = true;

    for(int p = 0; p < static_cast<int>(Data.MH.PatchArrayPointers.at(nPatchGroup).size()); p++){
        Patch & patch = Data.MH.PatchArrayPointers.at(nPatchGroup).at(p);
        for(int f = 0; f < static_cast<int>(patch.FaceIndices.size()); f++){
            Node & node = Data.MH.ArrayOfNodes.at(patch.nNodeArrayIndex);
            Face & face = node.Mesh.Faces.at(patch.FaceIndices.at(f));

            /// Only processed faces
            if(!face.bProcessedSG) continue;

            for(int v = 0; v < 3; v++){
                Vertex & vert = node.Mesh.Vertices.at(face.nIndexVertex[v]);

                /// Don't process yourself
                if(face.nIndexVertex[v] == patch.nVertex) continue;

                /// AND NOW... DO RECURSION!!!
                if(!DoneGroups.at(vert.nLinkedFacesIndex)) ConsolidateSmoothingGroups(vert.nLinkedFacesIndex, Numbers, DoneGroups);
            }
        }
    }
}


void MDL::GenerateSmoothingNumber(std::vector<int> & SmoothedPatchesGroup, const std::vector<unsigned long int> & nSmoothingGroupNumbers, const int & nSmoothingGroupCounter, const int & pg, std::stringstream & /*file*/){
    if(bCancelSG.load()) return;
    FileHeader & Data = *FH;

    std::vector<Patch> & patchgroup = Data.MH.PatchArrayPointers.at(pg);
    for(int p = 0; p < static_cast<int>(patchgroup.size()); p++){
        Patch & patch = patchgroup.at(p);

        /// Check if patch p already exists in the smoothed patches group, if so skip it
        bool bExists = false;
        for(int sp = 0; sp < static_cast<int>(SmoothedPatchesGroup.size()) && !bExists; sp++){
            if(SmoothedPatchesGroup.at(sp) == p){
                bExists = true;
            }
        }
        if(bExists) continue;

        /// Go through the smoothed patches group; check that all patches in
        /// the smoothed patch group are linked in this patch's smoothed patches as well
        bool bFound = true;
        for(int sp = 0; sp < static_cast<int>(SmoothedPatchesGroup.size()) && bFound; sp++){
            bFound = false;

            /// Go through the current patch's smoothed patches
            for(int i = 0; i < static_cast<int>(patch.SmoothedPatches.size()) && !bFound; i++){
                /// If this patch smooths over to the current patch in the smoothed patches group
                if(patch.SmoothedPatches.at(i) == static_cast<unsigned int>(SmoothedPatchesGroup.at(sp))){
                    bFound = true;
                }
            }
        }
        if(bFound){
            /// Add the patch to the smoothed patches group
            SmoothedPatchesGroup.push_back(p);

            ///Add the same smoothing group number to the smoothing group numbers of the current patch.
            patch.SmoothingGroupNumbers.push_back((long unsigned int*)&(nSmoothingGroupNumbers.at(nSmoothingGroupCounter)));

            ///Make sure we delete the smoothed patch numbers on everyone
            for(int spg = 0; spg < static_cast<int>(SmoothedPatchesGroup.size()); spg++){
                Patch & spgpatch = patchgroup.at(SmoothedPatchesGroup.at(spg));
                for(int sp = 0; sp < static_cast<int>(spgpatch.SmoothedPatches.size()); sp++){
                    for(int spg2 = 0; spg2 < static_cast<int>(SmoothedPatchesGroup.size()); spg2++){
                        if(spgpatch.SmoothedPatches.at(sp) == static_cast<unsigned int>(SmoothedPatchesGroup.at(spg2))){
                            spgpatch.SmoothedPatches.erase(spgpatch.SmoothedPatches.begin() + sp);
                            sp--;
                            break;
                        }
                    }
                }
            }
        }
    }
}



bool MDL::FindNormal(int nCheckFrom, const int & nPatchCount, const int & nPatch, const int & nPatchGroup, const Vector & vBaseNormal, const Vector & vTargetNormal, std::vector<int> & SmoothedPatches, std::stringstream & file){
    extern std::atomic_bool bCancelSG;



    if(bCancelSG.load()) return false;
    const double fSmoothingDiff = bXbox ? 0.01 : 0.00001;
    if(!FH) return false;
    if(nPatchGroup < 0 || nPatchGroup >= static_cast<int>(FH->MH.PatchArrayPointers.size())) return false;

    std::vector<Patch> & patchgroup = FH->MH.PatchArrayPointers.at(static_cast<unsigned int>(nPatchGroup));
    if(nPatch < 0 || nPatch >= static_cast<int>(patchgroup.size())) return false;

    Patch & patch = patchgroup.at(static_cast<unsigned int>(nPatch));
    auto StoreSmoothedPatches = [&patch](const std::vector<int> & patches){
        patch.SmoothedPatches.clear();
        patch.SmoothedPatches.reserve(patches.size());
        for(int patchIndex : patches){
            if(patchIndex >= 0) patch.SmoothedPatches.push_back(static_cast<unsigned int>(patchIndex));
        }
    };
    int nLastPatch = std::min(nPatchCount, static_cast<int>(patchgroup.size())) - 1;

    for(int nCount = nLastPatch; nCount >= nCheckFrom; --nCount){
        if(bCancelSG.load()) return false;
        if(nCount == nPatch) continue;

        std::vector<int> OurSmoothedPatches = SmoothedPatches;
        OurSmoothedPatches.push_back(nCount);

        Patch & candidate = patchgroup.at(static_cast<unsigned int>(nCount));
        Node & candidate_node = FH->MH.ArrayOfNodes.at(candidate.nNodeArrayIndex);
        Vector vProposed = vBaseNormal;
        for(unsigned int face_ind : candidate.FaceIndices){
            Face & face = candidate_node.Mesh.Faces.at(face_ind);
            Vertex & v1 = candidate_node.Mesh.Vertices.at(face.nIndexVertex.at(0));
            Vertex & v2 = candidate_node.Mesh.Vertices.at(face.nIndexVertex.at(1));
            Vertex & v3 = candidate_node.Mesh.Vertices.at(face.nIndexVertex.at(2));

            Vector Edge1 = v2.vFromRoot - v1.vFromRoot;
            Vector Edge2 = v3.vFromRoot - v1.vFromRoot;
            Vector Edge3 = v3.vFromRoot - v2.vFromRoot;
            Vector vAdd = cross(Edge1, Edge2);
            vAdd.Normalize();

            if(bSmoothAreaWeighting) vAdd *= (face.fArea > 0.000001 ? face.fArea : 0.0);
            if(bSmoothAngleWeighting){
                if(candidate.nVertex == face.nIndexVertex.at(0)) vAdd *= Angle(Edge1, Edge2);
                else if(candidate.nVertex == face.nIndexVertex.at(1)) vAdd *= Angle(Edge1, Edge3);
                else if(candidate.nVertex == face.nIndexVertex.at(2)) vAdd *= Angle(Edge2, Edge3);
            }
            if(bCreaseAngle && Angle(vBaseNormal, vAdd) > static_cast<double>(nCreaseAngle)) continue;
            vProposed += vAdd;
        }
        vProposed.Normalize();

        if(bDebug) file << "\n    Comparing to proposed (" << vProposed.Print() << "). Included patches:";
        for(int g = 0; g < static_cast<int>(OurSmoothedPatches.size()); g++){
            if(OurSmoothedPatches.at(g) != nPatch) if(bDebug) file << " " << OurSmoothedPatches.at(g);
        }

        Vector vTargetNormalCopy = vTargetNormal;
        if(vTargetNormalCopy.Compare(vProposed, fSmoothingDiff)){
            SmoothedPatches = OurSmoothedPatches;
            StoreSmoothedPatches(SmoothedPatches);
            patch.vVertexNormal = vProposed;
            return true;
        }

        if(FindNormal(nCount + 1, nPatchCount, nPatch, nPatchGroup, vProposed, vTargetNormal, OurSmoothedPatches, file)){
            SmoothedPatches = OurSmoothedPatches;
            StoreSmoothedPatches(SmoothedPatches);
            return true;
        }
    }
    return false;
}

char MDL::FindTangentSpace(int nCheckFrom, const int & nPatchCount, const int & nPatch, const int & nPatchGroup, const Vector & vBaseBitangent, const Vector & vBaseTangent, const Vector & vBaseNormal, const Vector & vTargetBitangent, const Vector & vTargetTangent, const Vector & vTargetNormal, std::vector<int> & SmoothedPatches, std::stringstream & file){
    if(bCancelSG.load()) return 0;
    const double fSmoothingDiff = bXbox ? 0.01 : 0.00001;
    if(!FH) return 0;
    if(nPatchGroup < 0 || nPatchGroup >= static_cast<int>(FH->MH.PatchArrayPointers.size())) return 0;

    std::vector<Patch> & patchgroup = FH->MH.PatchArrayPointers.at(static_cast<unsigned int>(nPatchGroup));
    if(nPatch < 0 || nPatch >= static_cast<int>(patchgroup.size())) return 0;

    Patch & patch = patchgroup.at(static_cast<unsigned int>(nPatch));
    auto StoreSmoothedPatches = [&patch](const std::vector<int> & patches){
        patch.SmoothedPatches.clear();
        patch.SmoothedPatches.reserve(patches.size());
        for(int patchIndex : patches){
            if(patchIndex >= 0) patch.SmoothedPatches.push_back(static_cast<unsigned int>(patchIndex));
        }
    };
    int nLastPatch = std::min(nPatchCount, static_cast<int>(patchgroup.size())) - 1;

    for(int nCount = nLastPatch; nCount >= nCheckFrom; --nCount){
        if(bCancelSG.load()) return 0;
        if(nCount == nPatch) continue;

        std::vector<int> OurSmoothedPatches = SmoothedPatches;
        OurSmoothedPatches.push_back(nCount);

        Vector vProposedB = vBaseBitangent;
        Vector vProposedT = vBaseTangent;
        Vector vProposedN = vBaseNormal;
        bool bBadUV = false;
        Patch & candidate = patchgroup.at(static_cast<unsigned int>(nCount));
        Node & candidate_node = FH->MH.ArrayOfNodes.at(candidate.nNodeArrayIndex);
        for(unsigned int face_ind : candidate.FaceIndices){
            Face & face = candidate_node.Mesh.Faces.at(face_ind);
            vProposedB += face.vBitangent;
            vProposedT += face.vTangent;
            vProposedN += (face.vTangent / face.vBitangent);
            if(face.fAreaUV == 0.0) bBadUV = true;
        }

        vProposedB.Normalize();
        vProposedT.Normalize();
        vProposedN.Normalize();

        if(bDebug) file << "\n   Comparing TS bitangent (" << vTargetBitangent.Print() << ") to proposed (" << vProposedB.Print() << ").";
        if(bDebug) file << "\n   Comparing TS tangent (" << vTargetTangent.Print() << ") to proposed (" << vProposedT.Print() << ").";
        if(bDebug) file << "\n   Comparing TS normal (" << vTargetNormal.Print() << ") to proposed (" << vProposedN.Print() << ").";
        if(bDebug) file << "\n   Included patches:";
        for(int g = 0; g < static_cast<int>(OurSmoothedPatches.size()); g++){
            if(OurSmoothedPatches.at(g) != nPatch) if(bDebug) file << " " << OurSmoothedPatches.at(g);
        }
        if(bDebug) file << "\n   Correct:";

        char nReturn = 0;
        Vector vTargetBitangentCopy = vTargetBitangent;
        if(vTargetBitangentCopy.Compare(vProposedB, fSmoothingDiff)){
            if(bDebug) file << " bitangent";
            nReturn = (nReturn | TS_BITANGENT);
        }
        Vector vTargetTangentCopy = vTargetTangent;
        if(vTargetTangentCopy.Compare(vProposedT, fSmoothingDiff)){
            if(bDebug) file << " tangent";
            nReturn = (nReturn | TS_TANGENT);
        }
        Vector vTargetNormalCopy = vTargetNormal;
        if(vTargetNormalCopy.Compare(vProposedN, fSmoothingDiff)){
            if(bDebug) file << " normal";
            nReturn = (nReturn | TS_NORMAL);
        }

        if(nReturn == TS_ALL){
            SmoothedPatches = OurSmoothedPatches;
            StoreSmoothedPatches(SmoothedPatches);
            patch.vVertexB = vProposedB;
            patch.vVertexT = vProposedT;
            patch.vVertexN = vProposedN;
            return nReturn;
        }

        if(bBadUV){
            if(bDebug) file << " (bad UV)";
            SmoothedPatches = OurSmoothedPatches;
            StoreSmoothedPatches(SmoothedPatches);
            return nReturn;
        }

        if(bDebug){
            if(nReturn) file << " (incomplete)";
            else file << " none";
        }

        nReturn = FindTangentSpace(nCount + 1, nPatchCount, nPatch, nPatchGroup, vProposedB, vProposedT, vProposedN, vTargetBitangent, vTargetTangent, vTargetNormal, OurSmoothedPatches, file);
        if(nReturn > 0){
            SmoothedPatches = OurSmoothedPatches;
            StoreSmoothedPatches(SmoothedPatches);
            return nReturn;
        }
    }
    return 0;
}
