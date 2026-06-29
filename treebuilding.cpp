#include "MDL.h"
#include "frame.h"
#include <algorithm>

/**
    Functions:
    Append()             // Helper
    AppendAabb()         // Helper
    GetNodeTreePrefix()  // Helper
    BuildGeometryNode()  // Helper
    BuildAnimationTree() // frame.h
    BuildGeometryTree()  // frame.h
    BuildTree()          // frame.h (MDL)
    BuildTree()          // frame.h (BWM)
*/

HTREEITEM Append(const std::string & sString, LPARAM lParam = 0, HTREEITEM hParentNew = nullptr, HTREEITEM hAfterNew = nullptr, UINT Flags = 0){
    static HTREEITEM hPrev;
    if(sString.empty()) return hPrev;
    static HTREEITEM hParent;
    HTREEITEM hAfter;

    //Determine hParent
    if(hParentNew == NULL) {}
    else hParent = hParentNew;

    //Determine hAfter
    if(hAfterNew == NULL) hAfter = hPrev;
    else hAfter = hAfterNew;

    //Add item
    TVINSERTSTRUCT tvis{};
    TVITEMEX * item = &tvis.itemex;
    item->mask = TVIF_TEXT | TVIF_PARAM;
    item->pszText = (char*) sString.c_str();
    item->cchTextMax = sString.size();
    item->lParam = lParam;
    if(Flags != 0){
        item->mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
        item->state = Flags;
    }
    tvis.hParent = hParent;
    tvis.hInsertAfter = hAfter;
    hPrev = TreeView_InsertItem(hTree, &tvis);

    return hPrev;
}

void AppendAabb(Aabb * AABB, HTREEITEM TopLevel, int & nCount){
    HTREEITEM htiAabb = Append("aabb "+std::to_string(nCount), (LPARAM) AABB, TopLevel);
    nCount++;
    if(AABB->Child1.size() > 0) AppendAabb(&(AABB->Child1.front()), htiAabb, nCount);
    if(AABB->Child2.size() > 0) AppendAabb(&(AABB->Child2.front()), htiAabb, nCount);
}

std::string GetNodeTreePrefix(unsigned short nType){
    if(     nType == (NODE_HEADER | NODE_MESH | NODE_SABER)) return "(saber) ";
    else if(nType == (NODE_HEADER | NODE_MESH | NODE_AABB)) return "(aabb) ";
    else if(nType == (NODE_HEADER | NODE_MESH | NODE_DANGLY)) return "(dangly) ";
    else if(nType == (NODE_HEADER | NODE_MESH | NODE_SKIN)) return "(skin) ";
    else if(nType == (NODE_HEADER | NODE_MESH)) return "(mesh) ";
    else if(nType == (NODE_HEADER | NODE_REFERENCE)) return "(reference) ";
    else if(nType == (NODE_HEADER | NODE_EMITTER)) return "(emitter) ";
    else if(nType == (NODE_HEADER | NODE_LIGHT)) return "(light) ";
    else if(nType == NODE_HEADER) return "(basic) ";
    else return "(unknown) ";
}

namespace {
    Node * FindTreeRootNode(std::vector<Node> & nodes){
        Node * pRoot = nullptr;
        for(Node & node : nodes){
            if(node.Head.nType == 0 || !node.Head.nNameIndex.Valid()) continue;
            if(!node.Head.nParentIndex.Valid()){
                if(pRoot != nullptr) return nullptr;
                pRoot = &node;
            }
        }
        return pRoot;
    }
}

void BuildAnimationNode(Node & node, HTREEITEM Prev, std::vector<unsigned> & offsets, MDL & Mdl, int nAnim){
    if(!Mdl.GetFileData()) return;
    FileHeader & Data = *Mdl.GetFileData();
    Animation & anim = Data.MH.Animations.at(nAnim);

    /// If we're doing the hierarchy, we're just gonna add the children to this node and go recursive on this function and then return
    if(bModelHierarchy){
        offsets.push_back(node.nOffset);
        for(Node & curnode : anim.ArrayOfNodes){
            if(curnode.Head.nParentIndex == node.Head.nNameIndex){
                int nNodeIndex = Mdl.GetNodeIndexByNameIndex(curnode.Head.nNameIndex);
                HTREEITEM Child = Append(GetNodeTreePrefix(nNodeIndex == -1 ? static_cast<unsigned short>(0) : static_cast<unsigned short>(Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nType)) + Mdl.GetNodeName(curnode), (LPARAM) &curnode, Prev);

                /// In case we are looping, stop the loop
                if(std::find(offsets.begin(), offsets.end(), curnode.nOffset) == offsets.end()){
                    //std::cout << Data.MH.Animations.at(nAnim).sName.c_str() << " > " << Mdl.GetNodeName(curnode) << " is OK!" << std::endl;
                    BuildAnimationNode(curnode, Child, offsets, Mdl, nAnim);
                }
                //else std::cout << Data.MH.Animations.at(nAnim).sName.c_str() << " > " << Mdl.GetNodeName(curnode) << " is not OK!" << std::endl;
            }
        }
        return;
    }

    if(node.Head.nNameIndex != 0){
        int nNodeIndex = Mdl.GetNodeIndexByNameIndex(node.Head.nNameIndex);
        HTREEITEM Controllers = Append("Controllers", (LPARAM) &node, Prev);
        for(Controller & ctrl : node.Head.Controllers){
            if(nNodeIndex == -1) throw mdlexception("tree building error: we have controller data on an anim node that does not have a geo counterpart.");
            Append(ReturnControllerName(ctrl.nControllerType, Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nType) + (ctrl.nColumnCount > 15 ? "bezierkey" : "key"), (LPARAM) &ctrl, Controllers);
        }

        if(node.Head.nParentIndex.Valid()){
            int nNodeIndex2 = Mdl.GetNodeIndexByNameIndex(node.Head.nParentIndex, nAnim);
            if(nNodeIndex2 == -1) throw mdlexception("tree building error: dealing with a name index that does not have a node in animation.");
            Node & parent = Data.MH.Animations.at(nAnim).ArrayOfNodes.at(nNodeIndex2);
            Append("Parent: " + Mdl.GetNodeName(parent), (LPARAM) &parent, Prev);
        }
    }

    HTREEITEM Children = Append("Children", (LPARAM) &node, Prev);
    for(Node & child : anim.ArrayOfNodes){
        if(child.Head.nParentIndex == node.Head.nNameIndex){
            int nNodeIndex = Mdl.GetNodeIndexByNameIndex(child.Head.nNameIndex);
            Append(GetNodeTreePrefix(nNodeIndex == -1 ? 0 : static_cast<unsigned short>(Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nType)) + Mdl.GetNodeName(child), (LPARAM) &child, Children);
        }
    }
}

void BuildAnimationTree(HTREEITEM Animations, MDL & Mdl){
    if(!Mdl.GetFileData()) return;
    FileHeader & Data = *Mdl.GetFileData();

    /// Delete any leftover children, which means we can use this function to reconstruct this part of the tree
    TreeView_DeleteAllChildren(hTree, Animations);
  try{
    for(int an = 0; an < static_cast<int>(Data.MH.Animations.size()); an++){
        Animation & anim = Data.MH.Animations.at(an);
        HTREEITEM Anim = Append(anim.sName, (LPARAM) &anim, Animations);
        std::vector<unsigned> offsets;
        offsets.reserve(anim.ArrayOfNodes.size());
        if(bModelHierarchy){
            Node * pRoot = FindTreeRootNode(anim.ArrayOfNodes);
            if(pRoot == nullptr) throw mdlexception("tree building error: animation has zero or multiple root nodes.");
            int nNodeIndex = Mdl.GetNodeIndexByNameIndex(pRoot->Head.nNameIndex);
            HTREEITEM CurrentNode = Append(GetNodeTreePrefix(nNodeIndex == -1 ? 0 : static_cast<unsigned short>(Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nType)) + Mdl.GetNodeName(*pRoot), (LPARAM) pRoot, Anim);
            BuildAnimationNode(*pRoot, CurrentNode, offsets, Mdl, an);
        }
        else for(Node & node : anim.ArrayOfNodes){
            int nNodeIndex = Mdl.GetNodeIndexByNameIndex(node.Head.nNameIndex);
            HTREEITEM CurrentNode = Append(GetNodeTreePrefix(nNodeIndex == -1 ? 0 : static_cast<unsigned short>(Data.MH.ArrayOfNodes.at(nNodeIndex).Head.nType)) + Mdl.GetNodeName(node), (LPARAM) &node, Anim);
            BuildAnimationNode(node, CurrentNode, offsets, Mdl, an);
        }

        /// Increment progress bar here
    }
  }
  catch(std::exception & e){
    Error("An exception occurred in BuildAnimationTree(): " + std::string(e.what()));
  }
  catch(...){
    Error("An exception occurred in BuildAnimationTree()!");
  }
}

void BuildGeometryNode(Node & node, HTREEITEM Prev, std::vector<unsigned> & offsets, MDL & Mdl){
    if(!Mdl.GetFileData()) return;
    FileHeader & Data = *Mdl.GetFileData();
    std::vector<Name> & Names = Data.MH.Names;

    /// Increment progress bar here

    /// If we're doing the hierarchy, we're just gonna add the children to this node and go recursive on this function and then return
    if(bModelHierarchy){
        offsets.push_back(node.nOffset);
        for(Node & curnode : Data.MH.ArrayOfNodes){
            if(curnode.Head.nParentIndex == node.Head.nNameIndex){
                HTREEITEM Child = Append(GetNodeTreePrefix(curnode.Head.nType) + Mdl.GetNodeName(curnode), (LPARAM) &curnode, Prev);

                /// In case we are looping, stop the loop
                if(std::find(offsets.begin(), offsets.end(), curnode.nOffset) == offsets.end())
                    BuildGeometryNode(curnode, Child, offsets, Mdl);
            }
        }
        return;
    }

    if(node.Head.nType & NODE_LIGHT){
        HTREEITEM Light = Append("Light", (LPARAM) &node, Prev);
        //HTREEITEM LensFlares = Append("Lens Flares", (LPARAM) &node, Light);
        int nMaxSize = std::max(node.Light.FlareSizes.size(),
                       std::max(node.Light.FlarePositions.size(),
                       std::max(node.Light.FlareColorShifts.size(),
                                node.Light.FlareTextureNames.size())));
        for(int n = 0; n < nMaxSize; n++)
            Append("Lens Flare " + std::to_string(n), (LPARAM) &node, Light);
    }
    if(node.Head.nType & NODE_EMITTER){
        Append("Emitter", (LPARAM) &node, Prev);
    }
    if(node.Head.nType & NODE_REFERENCE){
        Append("Reference", (LPARAM) &node, Prev);
    }
    if(node.Head.nType & NODE_MESH){
        HTREEITEM Mesh = Append("Mesh", (LPARAM) &node, Prev);
        HTREEITEM Vertices = Append("Vertices", (LPARAM) &node, Mesh);
        if(node.Mesh.Vertices.size() > 0){
            for(int n = 0; n < static_cast<int>(node.Mesh.Vertices.size()); n++)
                Append("Vertex " + std::to_string(n), (LPARAM) &(node.Mesh.Vertices[n]), Vertices);
        }
        HTREEITEM Faces = Append("Faces", (LPARAM) &node, Mesh);
        if(node.Mesh.Faces.size() > 0){
            for(int n = 0; n < static_cast<int>(node.Mesh.Faces.size()); n++){
                Append("Face " + std::to_string(n), (LPARAM) &(node.Mesh.Faces[n]), Faces);
            }
        }
    }
    if(node.Head.nType & NODE_SKIN){
        HTREEITEM Skin = Append("Skin", (LPARAM) &node, Prev);
        //HTREEITEM Bones = Append("Bones", (LPARAM) &node, Skin);
        if(node.Skin.Bones.size() > 0){
            for(int n = 0; n < static_cast<int>(node.Skin.Bones.size()); n++){
                std::string sBone = "Bone: " + Names.at(node.Skin.Bones.at(n).nNameIndex).sName;
                Append(sBone, (LPARAM) &(node.Skin.Bones.at(n)), Skin);
            }
        }
    }
    if(node.Head.nType & NODE_DANGLY){
        HTREEITEM Danglymesh = Append("Danglymesh", (LPARAM) &node, Prev);
        for(int i = 0; i < static_cast<int>(node.Dangly.Constraints.size()); i++)
            Append("Dangly Vertex " + std::to_string(i), (LPARAM) &(node), Danglymesh);
    }
    if(node.Head.nType & NODE_AABB){
        HTREEITEM Walkmesh = Append("Aabb", (LPARAM) &node, Prev);
        int nCounter = 0;
        if(node.Walkmesh.nOffsetToAabb.Valid() && node.Walkmesh.nOffsetToAabb > 0) AppendAabb(&(node.Walkmesh.RootAabb), Walkmesh, nCounter);
    }
    if(node.Head.nType & NODE_SABER){
        HTREEITEM Saber = Append("Lightsaber", (LPARAM) &node, Prev);
        if(node.Saber.SaberData.size() > 0){
            for(int i = 0; i < static_cast<int>(node.Saber.SaberData.size()); i++){
                Append("Lightsaber Vertex " + std::to_string(i), (LPARAM) &(node.Saber.SaberData[i]), Saber);
            }

        }
    }

    if(node.Head.nNameIndex != 0){
        HTREEITEM Controllers = Append("Controllers", (LPARAM) &node, Prev);
        for(Controller & ctrl : node.Head.Controllers){
            Append(ReturnControllerName(ctrl.nControllerType, node.Head.nType), (LPARAM) &ctrl, Controllers);
        }

        if(node.Head.nParentIndex.Valid()){
            int nNodeIndex = Mdl.GetNodeIndexByNameIndex(node.Head.nParentIndex);
            if(nNodeIndex == -1) throw mdlexception("tree building error: dealing with a name index that does not have a node in geometry.");
            Node & parent = Data.MH.ArrayOfNodes.at(nNodeIndex);
            Append("Parent: " + Mdl.GetNodeName(parent), (LPARAM) &parent, Prev);
        }
    }

    HTREEITEM Children = Append("Children", (LPARAM) &node, Prev);
    for(Node & curnode : Data.MH.ArrayOfNodes){
        if(curnode.Head.nParentIndex == node.Head.nNameIndex){
            Append(GetNodeTreePrefix(curnode.Head.nType) + Mdl.GetNodeName(curnode), (LPARAM) &curnode, Children);
        }
    }
}

void BuildGeometryTree(HTREEITEM Nodes, MDL & Mdl){
    if(!Mdl.GetFileData()) return;
    FileHeader & Data = *Mdl.GetFileData();

    /// Delete any leftover children, which means we can use this function to reconstruct this part of the tree
    TreeView_DeleteAllChildren(hTree, Nodes);

  try{
    std::vector<unsigned> offsets;
    offsets.reserve(Data.MH.nNodeCount);
    if(bModelHierarchy){
        Node * pRoot = FindTreeRootNode(Data.MH.ArrayOfNodes);
        if(pRoot == nullptr) throw mdlexception("tree building error: geometry has zero or multiple root nodes.");
        HTREEITEM CurrentNode = Append(GetNodeTreePrefix(pRoot->Head.nType) + Mdl.GetNodeName(*pRoot), (LPARAM) pRoot, Nodes);
        BuildGeometryNode(*pRoot, CurrentNode, offsets, Mdl);
    }
    else for(Node & node : Data.MH.ArrayOfNodes){
        HTREEITEM CurrentNode = Append(GetNodeTreePrefix(node.Head.nType) + Mdl.GetNodeName(node), (LPARAM) &node, Nodes);
        BuildGeometryNode(node, CurrentNode, offsets, Mdl);
    }
  }
  catch(std::exception & e){
    Error("An exception occurred in BuildGeometryTree(): " + std::string(e.what()));
  }
  catch(...){
    Error("An exception occurred in BuildGeometryTree()!");
  }
}

void BuildTree(MDL & Mdl){
    if(!Mdl.GetFileData()){
        std::cout << "No data. Do not build tree.\n";
        return;
    }
    FileHeader & Data = *Mdl.GetFileData();

    HTREEITEM Root = Append(to_ansi(Mdl.GetFilename()), 0, TVI_ROOT);
    Append("Header", (LPARAM) &(Data.MH), Root);

    HTREEITEM Animations = Append("Animations", 0, Root);
    BuildAnimationTree(Animations, Mdl);

    HTREEITEM Nodes = Append("Geometry", 0, Root);
    BuildGeometryTree(Nodes, Mdl);

    TreeView_Expand(hTree, Root, TVE_EXPAND);
    std::cout << "Model tree building done!\n";
}

void BuildTree(BWM & Bwm){
    if(!Bwm.GetData()) return;
    BWMHeader & Walkmesh = *Bwm.GetData();

    HTREEITEM Root = Append(to_ansi(Bwm.GetFilename()), 0, TVI_ROOT);
    Append("Header", (LPARAM) &Walkmesh, Root);
    HTREEITEM Verts = Append("Vertices", 0, Root);
    for(int n = 0; n < static_cast<int>(Walkmesh.verts.size()); n++){
        Append("Vertex " + std::to_string(n), (LPARAM) &Walkmesh.verts[n], Verts);
    }

    HTREEITEM Faces = Append("Faces", 0, Root);
    for(int n = 0; n < static_cast<int>(Walkmesh.faces.size()); n++){
        Append("Face " + std::to_string(n), (LPARAM) &Walkmesh.faces[n], Faces);
    }

    HTREEITEM Aabb = Append("Aabb", 0, Root);
    for(int n = 0; n < static_cast<int>(Walkmesh.aabb.size()); n++){
        Append("aabb " + std::to_string(n), (LPARAM) &Walkmesh.aabb[n], Aabb);
    }

    HTREEITEM Array2 = Append("Edges", 0, Root);
    for(int n = 0; n < static_cast<int>(Walkmesh.edges.size()); n++){
        Append("Edge " + std::to_string(n), (LPARAM) &Walkmesh.edges[n], Array2);
    }

    HTREEITEM Array3 = Append("Perimeters", 0, Root);
    for(int n = 0; n < static_cast<int>(Walkmesh.perimeters.size()); n++){
        Append("Perimeter " + std::to_string(n), (LPARAM) &Walkmesh.perimeters[n], Array3);
    }

    InvalidateRect(hTree, nullptr, true);
    std::cout << "Walkmesh tree building done!\n";
}
