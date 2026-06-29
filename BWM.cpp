#include "MDL.h"
#include <algorithm>
#include <limits>

namespace {
    unsigned int CheckedFlatAabbIndex(std::size_t nIndex){
        if(nIndex >= std::numeric_limits<unsigned int>::max()){
            throw mdlexception("AABB tree is too large to flatten into 32-bit child indices without colliding with the invalid-child sentinel.");
        }
        return static_cast<unsigned int>(nIndex);
    }

    unsigned short CheckedBwmVertexIndex(std::size_t index, const std::string & sContext){
        const std::size_t nInvalid = static_cast<std::size_t>(std::numeric_limits<unsigned short>::max());
        if(index >= nInvalid){
            throw mdlexception(sContext + " would require a vertex index outside the 16-bit walkmesh range.");
        }
        return static_cast<unsigned short>(index);
    }

    struct RestoreAabbOnFailure{
        Aabb & target;
        Aabb original;
        bool committed = false;

        explicit RestoreAabbOnFailure(Aabb & value) : target(value), original(value) {}
        ~RestoreAabbOnFailure(){
            if(!committed) target = std::move(original);
        }
        void Commit(){ committed = true; }

        RestoreAabbOnFailure(const RestoreAabbOnFailure &) = delete;
        RestoreAabbOnFailure & operator=(const RestoreAabbOnFailure &) = delete;
    };

    void ValidateWokSourceMesh(const Node & node){
        const std::size_t nVertexCount = node.Mesh.Vertices.size();
        for(std::size_t f = 0; f < node.Mesh.Faces.size(); ++f){
            const Face & face = node.Mesh.Faces.at(f);
            for(int i = 0; i < 3; ++i){
                const MdlInteger<unsigned short> & nIndex = face.nIndexVertex.at(i);
                if(!nIndex.Valid()){
                    throw mdlexception("WOK export source face " + std::to_string(f) + " has an invalid vertex index.");
                }
                if(static_cast<unsigned short>(nIndex) >= nVertexCount){
                    throw mdlexception("WOK export source face " + std::to_string(f) + " references vertex " +
                                       std::to_string(static_cast<unsigned short>(nIndex)) +
                                       ", but the source mesh only has " + std::to_string(nVertexCount) + " vertices.");
                }
            }
        }
    }
}

void WOK::WriteWok(Node & node, Vector vLytPos, std::stringstream * ptrssFile){
    // Build into a temporary header and commit only after the full WOK data
    // has been generated successfully. A failed WOK export should not erase
    // or partially replace any existing walkmesh data attached to this object.
    ValidateWokSourceMesh(node);
    std::unique_ptr<BWMHeader> newData(new BWMHeader);
    BWMHeader & Data = *newData;

    std::cout << "Writing wok.\n";
    Data.nType = 1;
    Data.vPosition = node.Head.vPos; // Shouldn't this be from root???

    //First copy our faces over to the Wok data. Put all walkables to the front.
    std::vector<Face> unwalkable;
    unwalkable.reserve(node.Mesh.Faces.size());
    Data.faces.reserve(node.Mesh.Faces.size());

    for(Face & face : node.Mesh.Faces){
        Vector v1 = node.Mesh.Vertices.at(face.nIndexVertex[0]).vFromRoot + vLytPos;
        //std::cout << "Adding aabb face (" << face.nIndexVertex[0] << "," << face.nIndexVertex[1] << "," << face.nIndexVertex[2] << ")\n";
        if(IsMaterialWalkable(face.nMaterialID)){
            Data.faces.push_back(face);
            Data.faces.back().bProcessed[0] = false;
            Data.faces.back().bProcessed[1] = false;
            Data.faces.back().bProcessed[2] = false;
            Data.faces.back().nAdjacentFaces[0] = INVALID_SHORT;
            Data.faces.back().nAdjacentFaces[1] = INVALID_SHORT;
            Data.faces.back().nAdjacentFaces[2] = INVALID_SHORT;
            Data.faces.back().fDistance = - (face.vNormal.fX * v1.fX +
                                             face.vNormal.fY * v1.fY +
                                             face.vNormal.fZ * v1.fZ);
        }
        else{
            unwalkable.push_back(face);
            unwalkable.back().bProcessed[0] = false;
            unwalkable.back().bProcessed[1] = false;
            unwalkable.back().bProcessed[2] = false;
            unwalkable.back().nAdjacentFaces[0] = INVALID_SHORT;
            unwalkable.back().nAdjacentFaces[1] = INVALID_SHORT;
            unwalkable.back().nAdjacentFaces[2] = INVALID_SHORT;
            unwalkable.back().fDistance = - (face.vNormal.fX * v1.fX +
                                             face.vNormal.fY * v1.fY +
                                             face.vNormal.fZ * v1.fZ);
        }
    }

    for(Face & unwalk : unwalkable){
        Data.faces.push_back(unwalk);
    }

    //Calculate adjacent edges
    for(std::size_t f = 0; f < Data.faces.size(); ++f){
        Face & face = Data.faces.at(f);
        //std::cout << "Checking aabb face (" << face.nIndexVertex[0] << "," << face.nIndexVertex[1] << "," << face.nIndexVertex[2] << ")\n";

        face.nID = f;

        // Skip if none is -1
        if((face.nAdjacentFaces.at(0).Valid() &&
            face.nAdjacentFaces.at(1).Valid() &&
            face.nAdjacentFaces.at(2).Valid()) ||
           !IsMaterialWalkable(face.nMaterialID)) continue;

        //Go through all the faces coming after this one
        for(std::size_t f2 = f + 1; f2 < Data.faces.size(); ++f2){
            Face & compareface = Data.faces.at(f2);

            if(IsMaterialWalkable(compareface.nMaterialID)){
                std::array<bool, 3> VertMatches = {false, false, false};
                std::array<bool, 3> VertMatchesCompare = {false, false, false};
                for(int i = 0; i < 3; i++){
                    Vector & ourvect = node.Mesh.Vertices.at(face.nIndexVertex[i]).vFromRoot;
                    for(int i2 = 0; i2 < 3; i2++){
                        Vector & othervect = node.Mesh.Vertices.at(compareface.nIndexVertex[i2]).vFromRoot;

                        if(ourvect.Compare(othervect)){
                            VertMatches.at(i) = true;
                            VertMatchesCompare.at(i2) = true;
                            break; // we can only have one matching vert in a face per vert. Once we find a match, we're done.
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
                        if(face.nAdjacentFaces[0].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 0...\n";
                        else face.nAdjacentFaces[0] = f2*3 + comparevertmatch;
                    }
                    else if(VertMatches.at(1) && VertMatches.at(2)){
                        if(face.nAdjacentFaces[1].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 1...\n";
                        else face.nAdjacentFaces[1] = f2*3 + comparevertmatch;
                    }
                    else if(VertMatches.at(2) && VertMatches.at(0)){
                        if(face.nAdjacentFaces[2].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f << ") for edge 2...\n";
                        else face.nAdjacentFaces[2] = f2*3 + comparevertmatch;
                    }
                    if(VertMatchesCompare.at(0) && VertMatchesCompare.at(1)){
                        if(compareface.nAdjacentFaces[0].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 0...\n";
                        else compareface.nAdjacentFaces[0] = f*3 + vertmatch;
                    }
                    else if(VertMatchesCompare.at(1) && VertMatchesCompare.at(2)){
                        if(compareface.nAdjacentFaces[1].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 1...\n";
                        else compareface.nAdjacentFaces[1] = f*3 + vertmatch;
                    }
                    else if(VertMatchesCompare.at(2) && VertMatchesCompare.at(0)){
                        if(compareface.nAdjacentFaces[2].Valid()) std::cout << "Well, we found too many adjacent edges (to " << f2 << ") for edge 2...\n";
                        else compareface.nAdjacentFaces[2] = f*3 + vertmatch;
                    }
                }
                if(face.nAdjacentFaces[0].Valid() &&
                   face.nAdjacentFaces[1].Valid() &&
                   face.nAdjacentFaces[2].Valid() ){
                    break; //Found them all, maybe I finish early?
                }
            }
        }
    }

    //First, find the first -1
    MdlInteger<unsigned int> nFace;
    MdlInteger<unsigned int> nEdge;
    MdlInteger<unsigned short> nAdjacent;
    for(unsigned int f = 0; f < Data.faces.size() && !nFace.Valid(); f++){
        //std::cout << "Checking2 aabb face (" << Data.faces.at(f).nIndexVertex[0] << "," << Data.faces.at(f).nIndexVertex[1] << "," << Data.faces.at(f).nIndexVertex[2] << ")\n";
        for(unsigned int i = 0; i < 3 && !nEdge.Valid(); i++){
            if(Data.faces.at(f).nAdjacentFaces.at(i).Valid() ||
               Data.faces.at(f).bProcessed.at(i) ||
               !IsMaterialWalkable(Data.faces.at(f).nMaterialID))
                continue;

            //std::cout << "Found starting point at face " << f << ", edge " << i << ".\n";
            nFace = f;
            nEdge = i;
            nAdjacent = -1;
            while(nFace.Valid() && nEdge.Valid()){
                //std::cout << "Looping through face " << nFace << " and edge " << nEdge << ".\n";
                nAdjacent = Data.faces.at(nFace).nAdjacentFaces.at(nEdge);
                if(!nAdjacent.Valid()){
                    if(!Data.faces.at(nFace).bProcessed.at(nEdge)){
                        Data.edges.push_back(Edge(3*nFace + nEdge, Data.faces.at(nFace).nEdgeTransitions.at(nEdge)));
                        Data.faces.at(nFace).bProcessed.at(nEdge) = true;
                        nEdge = (nEdge+1) % 3;
                    }
                    else{
                        nFace = -1;
                        nEdge = -1;
                        Data.perimeters.push_back(Perimeter(Data.edges.size()));
                    }
                }
                else{
                    if(Data.faces.size() > std::numeric_limits<unsigned int>::max() / 3u){
                        throw mdlexception("WOK export generated too many faces to validate adjacent-face references safely.");
                    }
                    const unsigned int nAdjacentValue = static_cast<unsigned short>(nAdjacent);
                    const unsigned int nMaxAdjacentValue = static_cast<unsigned int>(Data.faces.size() * 3u);
                    if(nAdjacentValue >= nMaxAdjacentValue){
                        throw mdlexception("WOK export encountered an adjacent-face reference outside the generated face array.");
                    }
                    nFace = nAdjacentValue / 3;
                    nEdge = nAdjacentValue % 3;
                    nEdge = (nEdge+1) % 3;
                }
            }
        }
    }
    //std::cout << "Done with edge loops.\n";

    /// Set all processed flags on all faces to false
    for(Face & face : Data.faces){
        //std::cout << "Checking3 aabb face (" << face.nIndexVertex[0] << "," << face.nIndexVertex[1] << "," << face.nIndexVertex[2] << ")\n";
        for(int i = 0; i < 3; i++){
            face.bProcessed.at(i) = false;
        }
    }

    /// Build verts array and update the indices
    /**
        Verts are compared by position here, so they will be reduced / welded.
    **/
    for(std::size_t f = 0; f < Data.faces.size(); ++f){
        Face & face = Data.faces.at(f);
        //std::cout << "Checking4 aabb face (" << face.nIndexVertex[0] << "," << face.nIndexVertex[1] << "," << face.nIndexVertex[2] << ")\n";
        for(int i = 0; i < 3; i++){
            //std::cout << "Checking out vertex " << face.nIndexVertex[i] << "\n";
            if(face.bProcessed[i]) continue;

            ////std::cout << "Processing vertex " << face.nIndexVertex[i] << "\n";

            Vertex vert;
            vert.assign(node.Mesh.Vertices.at(face.nIndexVertex[i]).vFromRoot);

            for(std::size_t f2 = f; f2 < Data.faces.size(); ++f2){
                Face & face2 = Data.faces.at(f2);
                for(int i2 = 0; i2 < 3; i2++){
                    //Make sure that we're only changing what's past our current position if we are in the same face.
                    if(f2 == f && i2 <= i) continue;

                    //std::cout << "Comparing verts [" << f << "," << i << "," << face.nIndexVertex[i] << "] " << vert.Print() << " and [" << f2 << "," << i2 << "," << face2.nIndexVertex[i2] << "] " << node.Mesh.Vertices.at(face2.nIndexVertex[i2]).vFromRoot.Print() << "...\n";
                    if(face2.bProcessed[i2] || !vert.Compare(node.Mesh.Vertices.at(face2.nIndexVertex[i2]).vFromRoot)) continue;
                    //std::cout << "Match found!\n";

                    face2.nIndexVertex[i2] = CheckedBwmVertexIndex(Data.verts.size(), "WOK/BWM vertex welding");
                    face2.bProcessed[i2] = true;
                }
            }

            face.nIndexVertex[i] = CheckedBwmVertexIndex(Data.verts.size(), "WOK/BWM vertex welding");
            face.bProcessed[i] = true;

            //std::cout << "Adding vertex " << face.nIndexVertex[i] << "\n";
            vert += vLytPos;
            Data.verts.push_back(std::move(vert));
        }
        face.vBBmax = Vector(-10000.0, -10000.0, -10000.0);
        face.vBBmin = Vector(10000.0, 10000.0, 10000.0);
        face.vCentroid = Vector(0.0, 0.0, 0.0);
        for(int i = 0; i < 3; i++){
            face.vBBmax.fX = std::max(face.vBBmax.fX, Data.verts.at(face.nIndexVertex[i]).fX);
            face.vBBmax.fY = std::max(face.vBBmax.fY, Data.verts.at(face.nIndexVertex[i]).fY);
            face.vBBmax.fZ = std::max(face.vBBmax.fZ, Data.verts.at(face.nIndexVertex[i]).fZ);
            face.vBBmin.fX = std::min(face.vBBmin.fX, Data.verts.at(face.nIndexVertex[i]).fX);
            face.vBBmin.fY = std::min(face.vBBmin.fY, Data.verts.at(face.nIndexVertex[i]).fY);
            face.vBBmin.fZ = std::min(face.vBBmin.fZ, Data.verts.at(face.nIndexVertex[i]).fZ);
            face.vCentroid += Data.verts.at(face.nIndexVertex[i]);
        }
        face.vCentroid /= 3.0;
        face.vBBmax += Vector(0.01, 0.01, 0.01);
        face.vBBmin -= Vector(0.01, 0.01, 0.01);
    }

    //Create aabb tree
    std::vector<Face*> allfaces;
    for(std::size_t f = 0; f < Data.faces.size(); ++f){
        allfaces.push_back(&Data.faces.at(f));
    }
    Aabb rootaabb;
    BuildAabb(rootaabb, allfaces, ptrssFile);
    LinearizeAabb(rootaabb, Data.aabb);

    GetData() = std::move(newData);
}


struct FaceSort{
    Face * p_face = nullptr;
    double centroid = 0.0;
    double maxdiff = 0.0;
    double fMax = 0.0;
    double fMin = 0.0;
    bool operator<(const FaceSort & facesort) const;
    bool operator==(const FaceSort & facesort) const{
        return (centroid == facesort.centroid);
    }
};

bool FaceSort::operator<(const FaceSort & facesort) const{
    if(centroid == facesort.centroid && p_face != nullptr && facesort.p_face != nullptr){
        if(RoundDec(p_face->fDistance, 4) != RoundDec(facesort.p_face->fDistance, 4)){
            return (abs(p_face->fDistance) < abs(facesort.p_face->fDistance));
        }
        /*
        else{
            return (p_face->nID < facesort.p_face->nID);
        }
        */
    }
    return (centroid < facesort.centroid);
}

struct AxisSort{
    std::string sName;
    double fSort;
    bool operator<(const AxisSort & axissort) const{
        return (fSort < axissort.fSort);
    }
    AxisSort(std::string sName = "", double fSort = 0.0): sName(sName), fSort(fSort) {}
};

struct AxisSort2{
    std::string sName;
    int nSize;
    bool operator<(const AxisSort2 & axissort) const;
    AxisSort2(std::string sName = "", int nSize = 0): sName(sName), nSize(nSize) {}
};

bool AxisSort2::operator<(const AxisSort2 & axissort) const{
    if(nSize == axissort.nSize){
        if(sName == "Z" && (axissort.sName == "Y" || axissort.sName == "X")) return true;
        if(sName == "Y" &&  axissort.sName == "X") return true;
    }
    return (nSize < axissort.nSize);
}

void BuildAabb(Aabb & aabb, const std::vector<Face*> & faces, std::stringstream * file){
    if(file != nullptr) (*file).precision(5);
    if(faces.empty()){
        throw mdlexception("BuildAabb(): cannot build an AABB tree from an empty face list.");
    }

    // BuildAabb() mutates the destination tree while it recursively splits
    // faces. Restore the original tree if any later split/validation fails so
    // a failed rebuild cannot erase or half-replace preserved AABB data.
    RestoreAabbOnFailure restoreAabb(aabb);

    // BuildAabb() is a rebuild path. Clear any preserved/previous child state
    // before writing the new node so a fallback rebuild cannot inherit stale
    // children from a malformed preserved AABB tree.
    aabb.Child1.clear();
    aabb.Child2.clear();
    aabb.nChild1 = 0;
    aabb.nChild2 = 0;

    if(faces.size() == 1){
        //This is the leaf
        Face & face = *faces.front();
        aabb.nID = face.nID;
        aabb.vBBmax = face.vBBmax;
        aabb.vBBmin = face.vBBmin;
        aabb.nProperty = 0;
        aabb.nChild1 = 0;
        aabb.nChild2 = 0;
        if(file != nullptr) *file << "Wrote leaf: " << aabb.nID << "\n";
    }
    else{
        if(file != nullptr) *file << "Processing non-leaf, faces: " << faces.size() << "\n";
        aabb.nID = -1;
        aabb.vBBmax = Vector(-10000.0, -10000.0, -10000.0);
        aabb.vBBmin = Vector(10000.0, 10000.0, 10000.0);
        Vector vAxisBBmax = Vector(-10000.0, -10000.0, -10000.0);
        Vector vAxisBBmin = Vector(10000.0, 10000.0, 10000.0);
        Vector vAverage;
        Vector vAverageBB;
        for(std::size_t f = 0; f < faces.size(); ++f){
            Face & face = *faces.at(f);
            aabb.vBBmax.fX = std::max(aabb.vBBmax.fX, face.vBBmax.fX);
            aabb.vBBmax.fY = std::max(aabb.vBBmax.fY, face.vBBmax.fY);
            aabb.vBBmax.fZ = std::max(aabb.vBBmax.fZ, face.vBBmax.fZ);
            aabb.vBBmin.fX = std::min(aabb.vBBmin.fX, face.vBBmin.fX);
            aabb.vBBmin.fY = std::min(aabb.vBBmin.fY, face.vBBmin.fY);
            aabb.vBBmin.fZ = std::min(aabb.vBBmin.fZ, face.vBBmin.fZ);
            vAxisBBmax.fX = std::max(vAxisBBmax.fX, face.vCentroid.fX);
            vAxisBBmax.fY = std::max(vAxisBBmax.fY, face.vCentroid.fY);
            vAxisBBmax.fZ = std::max(vAxisBBmax.fZ, face.vCentroid.fZ);
            vAxisBBmin.fX = std::min(vAxisBBmin.fX, face.vCentroid.fX);
            vAxisBBmin.fY = std::min(vAxisBBmin.fY, face.vCentroid.fY);
            vAxisBBmin.fZ = std::min(vAxisBBmin.fZ, face.vCentroid.fZ);
            vAverageBB.fX += aabb.vBBmin.fX + (aabb.vBBmax.fX - aabb.vBBmin.fX)/2.0;
            vAverageBB.fY += aabb.vBBmin.fY + (aabb.vBBmax.fY - aabb.vBBmin.fY)/2.0;
            vAverageBB.fZ += aabb.vBBmin.fZ + (aabb.vBBmax.fZ - aabb.vBBmin.fZ)/2.0;
            vAverage += face.vCentroid;
        }
        vAverage /= faces.size();
        vAverageBB /= faces.size();
        aabb.vBBmax += Vector(0.0001, 0.0001, 0.0001);
        if(file != nullptr) *file << "Bounding box: " << aabb.vBBmin.fX << ", " << aabb.vBBmin.fY << ", " << aabb.vBBmin.fZ << ", " << aabb.vBBmax.fX << ", " << aabb.vBBmax.fY << ", " << aabb.vBBmax.fZ << "\n";

        std::vector<AxisSort> axispriority;
        axispriority.push_back(AxisSort("X", aabb.vBBmax.fX - aabb.vBBmin.fX));
        axispriority.push_back(AxisSort("Y", aabb.vBBmax.fY - aabb.vBBmin.fY));
        axispriority.push_back(AxisSort("Z", aabb.vBBmax.fZ - aabb.vBBmin.fZ));
        sort(axispriority.begin(), axispriority.end());
        std::reverse(axispriority.begin(), axispriority.end());
        if(file != nullptr) *file << "Priority List: " << axispriority.at(0).sName << ": " << axispriority.at(0).fSort << ", " << axispriority.at(1).sName << ": " << axispriority.at(1).fSort << ", " << axispriority.at(2).sName << ": " << axispriority.at(2).fSort << "\n";
        int nCurrentPriority = 0;
        std::vector<double> vectorX;
        std::vector<double> vectorY;
        std::vector<double> vectorZ;
        for(std::size_t f = 0; f < faces.size(); ++f){
            Face & face = *faces.at(f);
            if(std::find(vectorX.begin(), vectorX.end(), face.vCentroid.fX) == vectorX.end()) vectorX.push_back(face.vCentroid.fX);
            if(std::find(vectorY.begin(), vectorY.end(), face.vCentroid.fY) == vectorY.end()) vectorY.push_back(face.vCentroid.fY);
            if(std::find(vectorZ.begin(), vectorZ.end(), face.vCentroid.fZ) == vectorZ.end()) vectorZ.push_back(face.vCentroid.fZ);
        }
        std::vector<AxisSort2> axispriority2;
        axispriority2.push_back(AxisSort2("X", vectorX.size()));
        axispriority2.push_back(AxisSort2("Y", vectorY.size()));
        axispriority2.push_back(AxisSort2("Z", vectorZ.size()));
        sort(axispriority2.begin(), axispriority2.end());
        std::reverse(axispriority2.begin(), axispriority2.end());
        if(file != nullptr) *file << "Priority List 2: " << axispriority2.at(0).sName << ": " << axispriority2.at(0).nSize << ", " << axispriority2.at(1).sName << ": " << axispriority2.at(1).nSize << ", " << axispriority2.at(2).sName << ": " << axispriority2.at(2).nSize << "\n";
        /*if(faces.size()%2 == 0){
            axispriority.at(0).sName = axispriority2.at(0).sName;
            axispriority.at(1).sName = axispriority2.at(1).sName;
            axispriority.at(2).sName = axispriority2.at(2).sName;
        }*/
        std::vector<AxisSort> axispriority3;
        axispriority3.push_back(AxisSort("X", vAxisBBmax.fX - vAxisBBmin.fX));
        axispriority3.push_back(AxisSort("Y", vAxisBBmax.fY - vAxisBBmin.fY));
        axispriority3.push_back(AxisSort("Z", vAxisBBmax.fZ - vAxisBBmin.fZ));
        sort(axispriority3.begin(), axispriority3.end());
        std::reverse(axispriority3.begin(), axispriority3.end());
        Vector vDeviance;
        Vector vDeviance2;
        if(file != nullptr) *file << "Priority List 3: " << axispriority3.at(0).sName << ": " << axispriority3.at(0).fSort << ", " << axispriority3.at(1).sName << ": " << axispriority3.at(1).fSort << ", " << axispriority3.at(2).sName << ": " << axispriority3.at(2).fSort << "\n";
        for(std::size_t f = 0; f < faces.size(); ++f){
            Face & face = *faces.at(f);
            vDeviance += face.vCentroid - vAverage;
            vDeviance2.fX += (face.vBBmin.fX + (face.vBBmax.fX - face.vBBmin.fX)/2.0 - vAverageBB.fX);
            vDeviance2.fY += (face.vBBmin.fY + (face.vBBmax.fY - face.vBBmin.fY)/2.0 - vAverageBB.fY);
            vDeviance2.fZ += (face.vBBmin.fZ + (face.vBBmax.fZ - face.vBBmin.fZ)/2.0 - vAverageBB.fZ);
        }
        vDeviance /= (faces.size() );
        vDeviance2 /= (faces.size() );
        std::vector<AxisSort> axispriority4;
        axispriority4.push_back(AxisSort("X", vDeviance.fX));
        axispriority4.push_back(AxisSort("Y", vDeviance.fY));
        axispriority4.push_back(AxisSort("Z", vDeviance.fZ));
        sort(axispriority4.begin(), axispriority4.end());
        if(file != nullptr) *file << "Priority List 4: " << axispriority4.at(0).sName << ": " << axispriority4.at(0).fSort << ", " << axispriority4.at(1).sName << ": " << axispriority4.at(1).fSort << ", " << axispriority4.at(2).sName << ": " << axispriority4.at(2).fSort << "\n";

        std::vector<AxisSort> axispriority5;
        axispriority5.push_back(AxisSort("X", vDeviance2.fX));
        axispriority5.push_back(AxisSort("Y", vDeviance2.fY));
        axispriority5.push_back(AxisSort("Z", vDeviance2.fZ));
        sort(axispriority5.begin(), axispriority5.end());
        if(file != nullptr) *file << "Priority List 5: " << axispriority5.at(0).sName << ": " << axispriority5.at(0).fSort << ", " << axispriority5.at(1).sName << ": " << axispriority5.at(1).fSort << ", " << axispriority5.at(2).sName << ": " << axispriority5.at(2).fSort << "\n";

        //axispriority.at(0).sName = axispriority4.at(0).sName;
        //axispriority.at(1).sName = axispriority4.at(1).sName;
        //axispriority.at(2).sName = axispriority4.at(2).sName;

        std::vector<Face*> half1;
        std::vector<Face*> half2;
        std::vector<FaceSort> centroids;

        bool bNegative = false;
        bool bEven;
        int nIndex;
        int nTry = 0;
        bool bOk = false;
        std::stringstream ssFaces;
        while(nTry < 7 && !bOk){
            bOk = true;

            //Clear
            centroids.resize(0);
            ssFaces.str(std::string());

            //Fill centroids
            for(std::size_t f = 0; f < faces.size(); ++f){
                Face & face = *faces.at(f);
                FaceSort newfs;
                newfs.p_face = &face;
                if(axispriority.at(nCurrentPriority).sName == "X"){
                    newfs.fMin = face.vBBmin.fX;
                    newfs.fMax = face.vBBmax.fX;
                    newfs.centroid = face.vCentroid.fX; //+ (face.vBBmax.fX - face.vBBmin.fX) / 2;
                    //newfs.centroid = face.vBBmin.fX + (face.vBBmax.fX - face.vBBmin.fX)/2;
                    newfs.maxdiff = face.vBBmin.fX + (face.vBBmax.fX - face.vBBmin.fX)/2;
                }
                else if(axispriority.at(nCurrentPriority).sName == "Y"){
                    newfs.fMin = face.vBBmin.fY;
                    newfs.fMax = face.vBBmax.fY;
                    newfs.centroid = face.vCentroid.fY; //+ (face.vBBmax.fY - face.vBBmin.fY) / 2;
                    //newfs.centroid = face.vBBmin.fY + (face.vBBmax.fY - face.vBBmin.fY)/2;
                    newfs.maxdiff = face.vBBmin.fY + (face.vBBmax.fY - face.vBBmin.fY)/2;
                }
                else if(axispriority.at(nCurrentPriority).sName == "Z"){
                    newfs.fMin = face.vBBmin.fZ;
                    newfs.fMax = face.vBBmax.fZ;
                    newfs.centroid = face.vCentroid.fZ; //+ (face.vBBmax.fZ - face.vBBmin.fZ) / 2;
                    //newfs.centroid = face.vBBmin.fZ + (face.vBBmax.fZ - face.vBBmin.fZ)/2;
                    newfs.maxdiff = face.vBBmin.fZ + (face.vBBmax.fZ - face.vBBmin.fZ)/2;
                }
                else{
                    throw mdlexception("AABB tree: significant split axis was not assigned.");
                }
                centroids.push_back(std::move(newfs));
            }
            //if(centroids.size() == 0) Error("Major error, centroid size 0!!");
            sort(centroids.begin(), centroids.end());
            bEven = (centroids.size() % 2 == 0);
            /*
            if(centroids.size() >= 2 && bEven){
                *file << centroids.at(centroids.size() / 2 - 1).fMin << " > " << centroids.at(centroids.size() / 2).fMin << "\n";
                if(!(centroids.at(centroids.size() / 2 - 1).fMin > centroids.at(centroids.size() / 2).fMin)){
                    bNegative = true;
                }
                else bNegative = false;
            }
            else bNegative = false;*/
            if(bNegative) std::reverse(centroids.begin(), centroids.end());

            if(bEven) nIndex = centroids.size() / 2 - 1;
            else nIndex = centroids.size() / 2;

            if(file != nullptr){
                *file << "Priority: " << (bNegative? "-" : "+") << axispriority.at(nCurrentPriority).sName << "\n";
                *file << "Faces:\n";
                for(std::size_t c = 0; c < centroids.size(); ++c){
                    ssFaces << "  " << centroids.at(c).centroid << ", " << centroids.at(c).fMin + (centroids.at(c).fMax - centroids.at(c).fMin)/2.0;
                    ssFaces << " (" << centroids.at(c).fMin << " -> " << centroids.at(c).fMax << ") - face " << centroids.at(c).p_face->nID << "\n";
                }
                *file << ssFaces.str();
            }

            bool bTryAxis = false;
            if(bTryAxis){
                if(nTry < 5){
                    if(!bEven){
                        /*
                        if(!bNegative && centroids.size() > 2){
                            if(centroids.at(nIndex).maxdiff == centroids.at(nIndex-1).maxdiff){
                                bNegative = true;
                                bOk = false;
                            }
                        }
                        else*/ if(centroids.size() > 2){
                            if(centroids.at(nIndex).centroid == centroids.at(nIndex-1).centroid){
                                bNegative = false;
                                if(nCurrentPriority < 2) nCurrentPriority++;
                                bOk = false;
                            }
                        }
                    }
                    else{
                        /*
                        if(!bNegative && centroids.size() > 2){
                            if(//centroids.at(nIndex).maxdiff == centroids.at(nIndex+1).maxdiff ||
                               centroids.at(nIndex).maxdiff == centroids.at(nIndex-1).maxdiff){
                                bNegative = true;
                                bOk = false;
                            }
                        }
                        else*/ if(centroids.size() > 2 && static_cast<std::size_t>(nIndex + 1) < centroids.size() && centroids.at(nIndex).maxdiff == centroids.at(nIndex+1).maxdiff){
                            bNegative = false;
                            if(nCurrentPriority < 2) nCurrentPriority++;
                            bOk = false;
                        }
                    }
                }
                else{
                    nCurrentPriority = 0;
                    bNegative = false;
                    bOk = false;
                }
            }
            nTry++;
        }

        double fMedian = centroids.at(nIndex).centroid;
        if(bEven){
            nIndex++;
            fMedian = (fMedian + centroids.at(nIndex).centroid) / 2;
        }
        if(file != nullptr){
            *file << "Median: " << fMedian << " (";
            if(bEven) *file << centroids.at(nIndex-1).p_face->nID << ", ";
            *file << centroids.at(nIndex).p_face->nID;
            *file << ")\n";
        }

        if(axispriority.at(nCurrentPriority).sName == "X" && !bNegative) aabb.nProperty = AABB_POSITIVE_X;
        else if(axispriority.at(nCurrentPriority).sName == "X" && bNegative) aabb.nProperty = AABB_NEGATIVE_X;
        else if(axispriority.at(nCurrentPriority).sName == "Y" && !bNegative) aabb.nProperty = AABB_POSITIVE_Y;
        else if(axispriority.at(nCurrentPriority).sName == "Y" && bNegative) aabb.nProperty = AABB_NEGATIVE_Y;
        else if(axispriority.at(nCurrentPriority).sName == "Z" && !bNegative) aabb.nProperty = AABB_POSITIVE_Z;
        else if(axispriority.at(nCurrentPriority).sName == "Z" && bNegative) aabb.nProperty = AABB_NEGATIVE_Z;
        else aabb.nProperty = 0;

        const std::size_t splitIndex = static_cast<std::size_t>(nIndex);
        for(std::size_t c = 0; c < centroids.size(); ++c){
            if(c < splitIndex) half1.push_back(centroids.at(c).p_face);
            else half2.push_back(centroids.at(c).p_face);
        }
        centroids.resize(0);
        centroids.shrink_to_fit();

        if(!half1.empty() && !half2.empty()){
            aabb.Child1.resize(1);
            BuildAabb(aabb.Child1.front(), half1, file);
            aabb.Child2.resize(1);
            BuildAabb(aabb.Child2.front(), half2, file);
        }
        else{
            if(file != nullptr) *file << "ERROR: One of the halves is empty!\n";
            throw mdlexception("AABB tree build failed: one of the split halves is empty.");
        }
    }

    restoreAabb.Commit();
}

void LinearizeAabb(const Aabb & aabbroot, std::vector<Aabb> & aabbarray){
    if(aabbroot.Child1.size() > 1 || aabbroot.Child2.size() > 1){
        throw mdlexception("Cannot linearize AABB tree: a node has more than one child in a binary child slot; refusing to drop extra child data.");
    }
    // Keep the source tree intact while producing the flat BWM AABB array.
    // The previous implementation moved child nodes out of the source tree and
    // then recursed through references to elements inside aabbarray. A vector
    // reallocation during recursion could invalidate those references and
    // corrupt the tree being linearized.
    const bool bHasChild1 = !aabbroot.Child1.empty();
    const bool bHasChild2 = !aabbroot.Child2.empty();

    const unsigned int nIndex = CheckedFlatAabbIndex(aabbarray.size());
    Aabb out = aabbroot;
    out.nExtra = 4;
    out.Child1.clear();
    out.Child2.clear();
    aabbarray.push_back(std::move(out));

    if(bHasChild1){
        aabbarray.at(nIndex).nChild1 = CheckedFlatAabbIndex(aabbarray.size());
        LinearizeAabb(aabbroot.Child1.front(), aabbarray);
    }
    else aabbarray.at(nIndex).nChild1 = static_cast<unsigned int>(~0u);

    if(bHasChild2){
        aabbarray.at(nIndex).nChild2 = CheckedFlatAabbIndex(aabbarray.size());
        LinearizeAabb(aabbroot.Child2.front(), aabbarray);
    }
    else aabbarray.at(nIndex).nChild2 = static_cast<unsigned int>(~0u);
}
