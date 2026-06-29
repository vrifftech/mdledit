#ifndef MDL_H_INCLUDED
#define MDL_H_INCLUDED

#include "general.h"
#include "file.h"

#define MDL_OFFSET 12
#define ANIM_OFFSET 136

#define CONVERT_CONTROLLER_SINGLE        1
#define CONVERT_CONTROLLER_KEYED         2
#define CONVERT_CONTROLLERLESS_DATA      3
#define CONVERT_HEADER                   4
#define CONVERT_LIGHT                    5
#define CONVERT_EMITTER                  6
#define CONVERT_REFERENCE                7
#define CONVERT_MESH                     8
#define CONVERT_SKIN                     9
#define CONVERT_DANGLY                   10
#define CONVERT_AABB                     11
#define CONVERT_SABER                    12
#define CONVERT_NODE                     13
#define CONVERT_ANIMATION                14
#define CONVERT_ANIMATION_NODE           15
#define CONVERT_MODEL_GEO                16
#define CONVERT_MODEL                    17
#define CONVERT_WOK                      18
#define CONVERT_PWK                      19
#define CONVERT_DWK                      20

#define FN_PTR_MODEL    1
#define FN_PTR_ANIM     2
#define FN_PTR_MESH     3
#define FN_PTR_SKIN     4
#define FN_PTR_DANGLY   5

#define FN_PTR_PC_K1_MODEL_1    4273776
#define FN_PTR_PC_K1_MODEL_2    4216096
#define FN_PTR_PC_K1_ANIM_1     4273392
#define FN_PTR_PC_K1_ANIM_2     4451552
#define FN_PTR_PC_K1_MESH_1     4216656
#define FN_PTR_PC_K1_MESH_2     4216672
#define FN_PTR_PC_K1_SKIN_1     4216592
#define FN_PTR_PC_K1_SKIN_2     4216608
#define FN_PTR_PC_K1_DANGLY_1   4216640
#define FN_PTR_PC_K1_DANGLY_2   4216624

#define FN_PTR_PC_K2_MODEL_1    4285200
#define FN_PTR_PC_K2_MODEL_2    4216320
#define FN_PTR_PC_K2_ANIM_1     4284816
#define FN_PTR_PC_K2_ANIM_2     4522928
#define FN_PTR_PC_K2_MESH_1     4216880
#define FN_PTR_PC_K2_MESH_2     4216896
#define FN_PTR_PC_K2_SKIN_1     4216816
#define FN_PTR_PC_K2_SKIN_2     4216832
#define FN_PTR_PC_K2_DANGLY_1   4216864
#define FN_PTR_PC_K2_DANGLY_2   4216848

#define FN_PTR_XBOX_K1_MODEL_1  4254992
#define FN_PTR_XBOX_K1_MODEL_2  4255008
#define FN_PTR_XBOX_K1_ANIM_1   4253536
#define FN_PTR_XBOX_K1_ANIM_2   4573360
#define FN_PTR_XBOX_K1_MESH_1   4267376
#define FN_PTR_XBOX_K1_MESH_2   4264048
#define FN_PTR_XBOX_K1_SKIN_1   4264032
#define FN_PTR_XBOX_K1_SKIN_2   4264048
#define FN_PTR_XBOX_K1_DANGLY_1 4266736
#define FN_PTR_XBOX_K1_DANGLY_2 4266720

#define FN_PTR_XBOX_K2_MODEL_1  4285872
#define FN_PTR_XBOX_K2_MODEL_2  4216016
#define FN_PTR_XBOX_K2_ANIM_1   4285488
#define FN_PTR_XBOX_K2_ANIM_2   4523088
#define FN_PTR_XBOX_K2_MESH_1   4216576
#define FN_PTR_XBOX_K2_MESH_2   4216592
#define FN_PTR_XBOX_K2_SKIN_1   4216512
#define FN_PTR_XBOX_K2_SKIN_2   4216528
#define FN_PTR_XBOX_K2_DANGLY_1 4216560
#define FN_PTR_XBOX_K2_DANGLY_2 4216544

#define CLASS_OTHER           0x00
#define CLASS_EFFECT          0x01
#define CLASS_TILE            0x02 /// ???
#define CLASS_CHARACTER       0x04
#define CLASS_DOOR            0x08
#define CLASS_LIGHTSABER      0x10
#define CLASS_PLACEABLE       0x20
#define CLASS_FLYER           0x40
//#define CLASS_80            0x80

#define NODE_HEADER       0x0001
#define NODE_LIGHT        0x0002
#define NODE_EMITTER      0x0004
//#define NODE_CAMERA     0x0008
#define NODE_REFERENCE    0x0010
#define NODE_MESH         0x0020
#define NODE_SKIN         0x0040
//#define NODE_ANIM       0x0080
#define NODE_DANGLY       0x0100
#define NODE_AABB         0x0200
//#define NODE_400        0x0400
#define NODE_SABER        0x0800
//#define NODE_GOB        0x1000
//#define NODE_COLLISION  0x2000
//#define NODE_SPHERE     0x4000
//#define NODE_CAPSULE    0x8000

#define NODE_SIZE_HEADER      80
#define NODE_SIZE_LIGHT       92
#define NODE_SIZE_EMITTER     224
//#define NODE_SIZE_CAMERA    0
#define NODE_SIZE_REFERENCE   36
#define NODE_SIZE_MESH        340 // k1 has 8 less, xbox has 4 less
#define NODE_SIZE_SKIN        100
//#define NODE_SIZE_ANIM      0
#define NODE_SIZE_DANGLY      28
#define NODE_SIZE_AABB        4
//#define NODE_SIZE_400       0
#define NODE_SIZE_SABER       20

#define EMITTER_FLAG_P2P                0x0001
#define EMITTER_FLAG_P2P_SEL            0x0002
#define EMITTER_FLAG_AFFECTED_WIND      0x0004
#define EMITTER_FLAG_TINTED             0x0008
#define EMITTER_FLAG_BOUNCE             0x0010
#define EMITTER_FLAG_RANDOM             0x0020
#define EMITTER_FLAG_INHERIT            0x0040
#define EMITTER_FLAG_INHERIT_VEL        0x0080
#define EMITTER_FLAG_INHERIT_LOCAL      0x0100
#define EMITTER_FLAG_SPLAT              0x0200
#define EMITTER_FLAG_INHERIT_PART       0x0400
#define EMITTER_FLAG_DEPTH_TEXTURE      0x0800 //maybe, per ndix UR
#define EMITTER_FLAG_13                 0x1000
//#define EMITTER_FLAG_2000             0x2000
//#define EMITTER_FLAG_3000             0x4000
//#define EMITTER_FLAG_4000             0x8000

#define MDX_FLAG_VERTEX                 0x0001
#define MDX_FLAG_UV1                    0x0002
#define MDX_FLAG_UV2                    0x0004
#define MDX_FLAG_UV3                    0x0008
#define MDX_FLAG_UV4                    0x0010
#define MDX_FLAG_NORMAL                 0x0020
#define MDX_FLAG_COLOR                  0x0040
#define MDX_FLAG_TANGENT1               0x0080
#define MDX_FLAG_TANGENT2               0x0100
#define MDX_FLAG_TANGENT3               0x0200
#define MDX_FLAG_TANGENT4               0x0400
//#define MDX_FLAG_0800                 0x0800
//#define MDX_FLAG_1000                 0x1000
//#define MDX_FLAG_2000                 0x2000
//#define MDX_FLAG_3000                 0x4000
//#define MDX_FLAG_4000                 0x8000

#define AABB_NO_CHILDREN    0x00
#define AABB_POSITIVE_X     0x01
#define AABB_POSITIVE_Y     0x02
#define AABB_POSITIVE_Z     0x04
#define AABB_NEGATIVE_X     0x08
#define AABB_NEGATIVE_Y     0x10
#define AABB_NEGATIVE_Z     0x20
//#define AABB_40           0x40
//#define AABB_80           0x80

#define MATERIAL_NONE           0
#define MATERIAL_DIRT           1
#define MATERIAL_OBSCURING      2
#define MATERIAL_GRASS          3
#define MATERIAL_STONE          4
#define MATERIAL_WOOD           5
#define MATERIAL_WATER          6
#define MATERIAL_NONWALK        7
#define MATERIAL_TRANSPARENT    8
#define MATERIAL_CARPET         9
#define MATERIAL_METAL          10
#define MATERIAL_PUDDLES        11
#define MATERIAL_SWAMP          12
#define MATERIAL_MUD            13
#define MATERIAL_LEAVES         14
#define MATERIAL_LAVA           15
#define MATERIAL_BOTTOMLESSPIT  16
#define MATERIAL_DEEPWATER      17
#define MATERIAL_DOOR           18
#define MATERIAL_SNOW           19
#define MATERIAL_SAND           20

bool IsMaterialWalkable(int nMat);

///All controller numbers apparently must be divisible by 4? Except for detonate
#define CONTROLLER_HEADER_POSITION              8
#define CONTROLLER_HEADER_ORIENTATION           20
#define CONTROLLER_HEADER_SCALING               36
///------------------------------------------------
#define CONTROLLER_MESH_SELFILLUMCOLOR          100
#define CONTROLLER_MESH_ALPHA                   132
///------------------------------------------------
#define CONTROLLER_LIGHT_COLOR                  76
#define CONTROLLER_LIGHT_RADIUS                 88
#define CONTROLLER_LIGHT_SHADOWRADIUS           96
#define CONTROLLER_LIGHT_VERTICALDISPLACEMENT   100
#define CONTROLLER_LIGHT_MULTIPLIER             140
///------------------------------------------------
#define CONTROLLER_EMITTER_ALPHAEND             80
#define CONTROLLER_EMITTER_ALPHASTART           84
#define CONTROLLER_EMITTER_BIRTHRATE            88
#define CONTROLLER_EMITTER_BOUNCE_CO            92
#define CONTROLLER_EMITTER_COMBINETIME          96
#define CONTROLLER_EMITTER_DRAG                 100
#define CONTROLLER_EMITTER_FPS                  104
#define CONTROLLER_EMITTER_FRAMEEND             108
#define CONTROLLER_EMITTER_FRAMESTART           112
#define CONTROLLER_EMITTER_GRAV                 116
#define CONTROLLER_EMITTER_LIFEEXP              120
#define CONTROLLER_EMITTER_MASS                 124
#define CONTROLLER_EMITTER_P2P_BEZIER2          128
#define CONTROLLER_EMITTER_P2P_BEZIER3          132
#define CONTROLLER_EMITTER_PARTICLEROT          136
#define CONTROLLER_EMITTER_RANDVEL              140
#define CONTROLLER_EMITTER_SIZESTART            144
#define CONTROLLER_EMITTER_SIZEEND              148
#define CONTROLLER_EMITTER_SIZESTART_Y          152
#define CONTROLLER_EMITTER_SIZEEND_Y            156
#define CONTROLLER_EMITTER_SPREAD               160
#define CONTROLLER_EMITTER_THRESHOLD            164
#define CONTROLLER_EMITTER_VELOCITY             168
#define CONTROLLER_EMITTER_XSIZE                172
#define CONTROLLER_EMITTER_YSIZE                176
#define CONTROLLER_EMITTER_BLURLENGTH           180
#define CONTROLLER_EMITTER_LIGHTNINGDELAY       184
#define CONTROLLER_EMITTER_LIGHTNINGRADIUS      188
#define CONTROLLER_EMITTER_LIGHTNINGSCALE       192
#define CONTROLLER_EMITTER_LIGHTNINGSUBDIV      196
#define CONTROLLER_EMITTER_LIGHTNINGZIGZAG      200
//#define CONTROLLER_EMITTER_204                204
//#define CONTROLLER_EMITTER_208                208
//#define CONTROLLER_EMITTER_212                212
#define CONTROLLER_EMITTER_ALPHAMID             216
#define CONTROLLER_EMITTER_PERCENTSTART         220
#define CONTROLLER_EMITTER_PERCENTMID           224
#define CONTROLLER_EMITTER_PERCENTEND           228
#define CONTROLLER_EMITTER_SIZEMID              232
#define CONTROLLER_EMITTER_SIZEMID_Y            236
#define CONTROLLER_EMITTER_RANDOMBIRTHRATE      240
//#define CONTROLLER_EMITTER 244                244
//#define CONTROLLER_EMITTER 248                248
#define CONTROLLER_EMITTER_TARGETSIZE           252
#define CONTROLLER_EMITTER_NUMCONTROLPTS        256
#define CONTROLLER_EMITTER_CONTROLPTRADIUS      260
#define CONTROLLER_EMITTER_CONTROLPTDELAY       264
#define CONTROLLER_EMITTER_TANGENTSPREAD        268
#define CONTROLLER_EMITTER_TANGENTLENGTH        272
//#define CONTROLLER_EMITTER_276                276
//#define CONTROLLER_EMITTER_280                280
#define CONTROLLER_EMITTER_COLORMID             284
/// ... many controller numbers ... ///
#define CONTROLLER_EMITTER_COLOREND             380
//#define CONTROLLER_EMITTER_384                384
//#define CONTROLLER_EMITTER_388                388
#define CONTROLLER_EMITTER_COLORSTART           392
/// ... many controller numbers ... ///
#define CONTROLLER_EMITTER_DETONATE             502

#define TS_NONE         0x00
#define TS_TANGENT      0x01
#define TS_BITANGENT    0x02
#define TS_NORMAL       0x04
#define TS_ALL          0x07

constexpr unsigned short INVALID_SHORT = 0xFFFF;
constexpr unsigned int INVALID_INT = 0xFFFFFFFF;

//Data structures
struct Edge;
struct Face;
struct Aabb;
struct Controller;
struct Event;
struct Name;
struct VertexData;
struct Vertex;
struct Color;
struct Bone;
struct Location;

//Node structs
struct Node;
struct Header;
struct LightHeader;
struct EmitterHeader;
struct MeshHeader;
struct SkinHeader;
struct DanglymeshHeader;
struct WalkmeshHeader;
struct SaberHeader;

//Higher structures
struct Animation;
struct GeometryHeader;
struct ModelHeader;
struct FileHeader;

//File structs
class MDL;
class MDX;
class WOK;
class PWK;
class DWK;
class Ascii;

bool LoadSupermodel(MDL & curmdl, std::unique_ptr<MDL> & Supermodel, bool bWarnIfUnavailable = true);

/**** DATA STRUCTS ****/
template <class IntegerType>
class MdlInteger{
    IntegerType nData;
  public:
    MdlInteger(){
        nData = 0;
        nData = (~nData);
    }
    MdlInteger(IntegerType nInit) : nData(nInit) {}
    MdlInteger(const MdlInteger<IntegerType> & second) = default;
    MdlInteger<IntegerType> & operator=(const MdlInteger<IntegerType> & second) = default;
    void Set(IntegerType nSet) { nData = nSet; }
    bool Valid() const{
        IntegerType nMax = 0;
        //std::cout << "Comparing " << nData << " to " << (IntegerType) (~nMax) << " at byte size " << sizeof(nData) << "... Valid: " << (nData != (~nMax) ? "true" : "false") << "\n";
        return nData != (IntegerType) (~nMax);
    }/*
    IntegerType Get() const{
        if(!Valid()) throw mdlexception("MdlShort::Get(): Getting an invalid value!");
        return nData;
    }*/
    std::string Print(){
        if(!Valid()) return "-1";
        return std::to_string(nData);
    }
    IntegerType * GetPtr(){
        return &nData;
    }

    MdlInteger<IntegerType> & operator+=(const IntegerType & nIncrease){
        nData+=nIncrease;
        return *this;
    }
    MdlInteger<IntegerType> & operator++();
    MdlInteger<IntegerType> & operator++(int);

    template <class T>
    friend bool operator==(const MdlInteger<T> & mdlinteger, const T & integer);
    template <class T>
    friend bool operator==(const MdlInteger<T> & mdlinteger, const MdlInteger<T> & mdlinteger2);
    template <class T>
    friend bool operator<(const MdlInteger<T> & mdlinteger, const T & integer);
    template <class T>
    friend bool operator<(const MdlInteger<T> & mdlinteger, const MdlInteger<T> & mdlinteger2);
    template <class T>
    friend bool operator>(const MdlInteger<T> & mdlinteger, const T & integer);
    template <class T>
    friend bool operator>(const MdlInteger<T> & mdlinteger, const MdlInteger<T> & mdlinteger2);
    //friend MdlInteger & operator==(const T & integer, const MdlInteger<IntegerType> & mdlinteger);
    MdlInteger & operator=(const IntegerType & integer){
        nData = integer;
        return *this;
    }
    //MdlInteger & operator=(const MdlInteger & integer){ nData = integer.nData; }
    //MdlInteger & operator=(IntegerType & integer){ nData = integer; }
    //MdlInteger & operator=(MdlInteger & integer){ nData = integer.nData; }
    //MdlInteger & operator=(IntegerType integer){ nData = integer; }
    //MdlInteger & operator=(MdlInteger integer){ nData = integer.nData; }

    /// This function should allow implicit conversion, dispensing with the get function.
    operator IntegerType() const {
        return nData;
    }
};

template <class IntegerType>
MdlInteger<IntegerType> & MdlInteger<IntegerType>::operator++(){
    nData++;
    return *this;
}

template <class IntegerType>
MdlInteger<IntegerType> & MdlInteger<IntegerType>::operator++(int){
    nData++;
    return *this;
}

/// Template declarations

template<class IntegerType>
bool operator==(const MdlInteger<IntegerType> & mdlinteger, const IntegerType & integer){
    return mdlinteger.nData == integer;
}
template <class IntegerType>
bool operator==(const MdlInteger<IntegerType> & mdlinteger, const MdlInteger<IntegerType> & mdlinteger2){
    return mdlinteger.nData == mdlinteger2.nData;
}
template<class IntegerType>
bool operator<(const MdlInteger<IntegerType> & mdlinteger, const IntegerType & integer){
    return mdlinteger.nData < integer;
}
template <class IntegerType>
bool operator<(const MdlInteger<IntegerType> & mdlinteger, const MdlInteger<IntegerType> & mdlinteger2){
    return mdlinteger.nData < mdlinteger2.nData;
}
template<class IntegerType>
bool operator>(const MdlInteger<IntegerType> & mdlinteger, const IntegerType & integer){
    return mdlinteger.nData > integer;
}
template <class IntegerType>
bool operator>(const MdlInteger<IntegerType> & mdlinteger, const MdlInteger<IntegerType> & mdlinteger2){
    return mdlinteger.nData > mdlinteger2.nData;
}
template<class IntegerType>
bool operator!=(const MdlInteger<IntegerType> & mdlinteger, const IntegerType & integer){
    return !(mdlinteger == integer);
}
template <class IntegerType>
bool operator!=(const MdlInteger<IntegerType> & mdlinteger, const MdlInteger<IntegerType> & mdlinteger2){
    return !(mdlinteger == mdlinteger2);
}


struct Location{
    Vector vPosition;
    Orientation oOrientation;
};

struct Color{
    double fR = 0.0;
    double fG = 0.0;
    double fB = 0.0;
    int nR = 0;
    int nG = 0;
    int nB = 0;

    Color(){}
    Color(double f1, double f2, double f3){
        fR = f1;
        fG = f2;
        fB = f3;
    }
    void Set(double f1, double f2, double f3){
        fR = f1;
        fG = f2;
        fB = f3;
    }
    bool operator==(const Color & c){
        if(abs(fR-c.fR) < 0.0001 && abs(fG-c.fG) < 0.0001 && abs(fB-c.fB) < 0.0001) return true;
        //if(fR == c.fR && fG == c.fG && fB == c.fB) return true;
        return false;
    }
};

struct Weight{
    std::array<double, 4> fWeightValue = {1.0, 0.0, 0.0, 0.0};
    std::array<MdlInteger<unsigned short>, 4> nWeightIndex;
    bool operator==(const Weight & w){
        for(int n = 0; n < 4; n++){
            if(abs(fWeightValue.at(n) - w.fWeightValue.at(n)) >= 0.0001 || nWeightIndex.at(n) != w.nWeightIndex.at(n)) return false;
        }
        return true;
    }
};

struct VertexData{

    //Binary members
    Vector vVertex;
    Vector vNormal;
    Vector vUV1;
    Vector vUV2;
    Vector vUV3;
    Vector vUV4;
    Color cColor;
    std::array<Vector, 3> vTangent1; /// In the order B T N
    std::array<Vector, 3> vTangent2;
    std::array<Vector, 3> vTangent3;
    std::array<Vector, 3> vTangent4;
    Weight Weights;

    //Added members
    MdlInteger<unsigned short> nNameIndex;

    VertexData();
    VertexData(const VertexData &) = default;
    VertexData(VertexData &&);
    VertexData & operator=(const VertexData &) = default;
    VertexData & operator=(VertexData &&) = default;
    VertexData(const Vector & v1, int nName = -1): vVertex(v1), nNameIndex(static_cast<unsigned short>(nName)) {}
    VertexData(const Vector & v1, const Vector & v2, int nName = -1);
    VertexData(const Vector & v1, const Vector & v2, const Vector & v3, int nName = -1): vVertex(v1), vNormal(v3), vUV1(v2), nNameIndex(static_cast<unsigned short>(nName)) {}
    bool operator==(const VertexData & vd){
        if(vVertex == vd.vVertex &&
           vNormal == vd.vNormal &&
           vUV1 == vd.vUV1 &&
           vUV2 == vd.vUV2 &&
           vUV3 == vd.vUV3 &&
           vUV4 == vd.vUV4 &&
           cColor == vd.cColor &&
           vTangent1.at(0) == vd.vTangent1.at(0) &&
           vTangent1.at(1) == vd.vTangent1.at(1) &&
           vTangent1.at(2) == vd.vTangent1.at(2) &&
           vTangent2.at(0) == vd.vTangent2.at(0) &&
           vTangent2.at(1) == vd.vTangent2.at(1) &&
           vTangent2.at(2) == vd.vTangent2.at(2) &&
           vTangent3.at(0) == vd.vTangent3.at(0) &&
           vTangent3.at(1) == vd.vTangent3.at(1) &&
           vTangent3.at(2) == vd.vTangent3.at(2) &&
           vTangent4.at(0) == vd.vTangent4.at(0) &&
           vTangent4.at(1) == vd.vTangent4.at(1) &&
           vTangent4.at(2) == vd.vTangent4.at(2) &&
           Weights == vd.Weights
           ) return true;
        else return false;
    }
};

struct Vertex: public Vector{

    unsigned nOffset = 0;
    VertexData MDXData;
    MdlInteger<unsigned int> nLinkedFacesIndex;

    Vertex assign(const Vector & v, bool bMdx = false){
        (void)bMdx;
        fX = v.fX;
        fY = v.fY;
        fZ = v.fZ;
        //MDXData = VertexData(v, MDXData.nNameIndex);
        MDXData.vVertex = v;
        return *this;
    }
    bool operator==(const Vertex & v){
        if(fX == v.fX && fY == v.fY && fZ == v.fZ && MDXData == v.MDXData) return true;
        else return false;
    }
    Vector vFromRoot;
};

struct Edge{

    MdlInteger<unsigned int> nIndex;
    MdlInteger<unsigned int> nTransition;

    Edge(MdlInteger<unsigned int> nIndex = -1, MdlInteger<unsigned int> nTransition = -1): nIndex(nIndex), nTransition(nTransition) {}
};

struct Perimeter{

    unsigned nPerimeter = 0;

    Perimeter(unsigned nPerimeter = 0): nPerimeter(nPerimeter) {}

    Perimeter & operator=(const unsigned & integer){
        nPerimeter = integer;
        return *this;
    }

    /// This function should allow implicit conversion.
    operator unsigned() const {
        return nPerimeter;
    }

};

struct Face{

    //Binary members
    Vector vNormal;
    double fDistance = 0.0;
    MdlInteger<unsigned> nMaterialID = 0;
    std::array<MdlInteger<unsigned short>, 3> nAdjacentFaces;
    std::array<MdlInteger<unsigned short>, 3> nIndexVertex;

    //Added members
    std::array<MdlInteger<unsigned short>, 3> nTempIndexVertex;
    std::array<MdlInteger<unsigned short>, 3> nIndexTvert;
    std::array<MdlInteger<unsigned short>, 3> nIndexTvert1;
    std::array<MdlInteger<unsigned short>, 3> nIndexTvert2;
    std::array<MdlInteger<unsigned short>, 3> nIndexTvert3;
    std::array<MdlInteger<unsigned short>, 3> nIndexColor;
    std::array<MdlInteger<unsigned short>, 3> nIndexNormal; // optional explicit MDX normal indices
    bool bPreserveRuntimeFaceData = false; // keep binary face normal/distance/adjacency when supplied
    Vector vPreservedNormal;
    double fPreservedDistance = 0.0;
    std::array<MdlInteger<unsigned short>, 3> nPreservedAdjacentFaces;
    std::array<bool, 3> bProcessed = {false, false, false};
    bool bProcessedSG = false;
    std::array<MdlInteger<unsigned int>, 3> nEdgeTransitions;
    MdlInteger<unsigned> nTextureCount = 0;
    unsigned int nSmoothingGroup = 0;
    double fArea = 0.0;
    double fAreaUV = 0.0;
    Vector vTangent;
    Vector vBitangent;
    Vector vBBmin;
    Vector vBBmax;
    Vector vCentroid;
    MdlInteger<unsigned short> nID;
    MdlInteger<unsigned short> nNameIndex;

    Face() = default;
    Face(const Face &) = default;
    Face(Face &&);
    Face & operator=(const Face &) = default;
    Face & operator=(Face &&) = default;
};


struct Aabb{

    //binary members
    Vector vBBmin;
    Vector vBBmax;
    MdlInteger<unsigned int> nChild1 = 0; //Offset (in the mdl) or Index (in the wok)
    MdlInteger<unsigned int> nChild2 = 0; //Offset (in the mdl) or Index (in the wok)
    MdlInteger<unsigned short> nID; //An index to the face of the walkmesh
    MdlInteger<unsigned> nProperty = 0; //AABB_* constants, most significant plane for Child 2

    //Added members
    unsigned int nOffset = 0;
    std::vector<Aabb> Child1;
    std::vector<Aabb> Child2;
    MdlInteger<unsigned> nExtra = 0;

    Aabb() = default;
    Aabb(const Aabb &) = default;
    Aabb(Aabb &&) = default;
    Aabb & operator=(const Aabb &) = default;
    Aabb & operator=(Aabb &&) = default;
    ~Aabb();
};

struct Controller{

    //Binary members
    MdlInteger<unsigned int> nControllerType = 0;
    MdlInteger<unsigned short> nUnknown2;
    MdlInteger<unsigned short> nValueCount = 0;
    MdlInteger<unsigned short> nTimekeyStart = 0;
    MdlInteger<unsigned short> nDataStart = 0;
    MdlInteger<unsigned char> nColumnCount = 0;
    std::array<char, 3> nPadding = {0, 0, 0};

    //Added members
    MdlInteger<unsigned short> nNameIndex;
    MdlInteger<unsigned int> nAnimation;
};

struct Event{
    double fTime = 0.0;
    std::string sName;
};

struct Name{
    unsigned int nOffset = 0;
    std::string sName;
};

// ArrayHeads should only be relevant during (de)compilation. Otherwise, use vector lengths.
struct ArrayHead{
    unsigned int nOffset = 0;
    unsigned int nCount = 0;
    unsigned int nCount2 = 0;

    bool GetDoCountsDiffer(){
        if(nCount == nCount2) return false;
        else return true; //We sure don't expect that!
    }
    void ResetToSize(unsigned int nSize){
        nOffset = 0;
        nCount = nSize;
        nCount2 = nSize;
    }
    bool empty(){
        if(nOffset == 0 && nCount == 0 && nCount2 == 0) return true;
        return false;
    }
};

struct Patch{
    MdlInteger<unsigned int> nPatchGroup;                       /// The number of the patch group this patch belongs to.
    MdlInteger<unsigned int> nNodeArrayIndex;                   /// This is the index of the mesh with our vert
    MdlInteger<unsigned int> nVertex;                           /// This is the index of the vert
    std::vector<unsigned int> FaceIndices;                      /// The faces using this vert
    unsigned int nSmoothingGroups = 0;                          /// The combined smoothing groups for the patch.
    std::vector<MdlInteger<unsigned int>> SmoothedPatches;      /// Other patches by num that this patch smooths to.
    std::vector<unsigned long int*> SmoothingGroupNumbers;      /// Pointers to smoothing group numbers
    Vector vWorldNormal;
    Vector vWorldT;
    Vector vWorldB;
    Vector vWorldN;
    Vector vVertexNormal;
    Vector vVertexT;
    Vector vVertexB;
    Vector vVertexN;
    bool bBadGeo = false, bGroupBadGeo = false;
    bool bBadUV = false, bGroupBadUV = false;

    Patch() = default;
    Patch(const Patch &) = default;
    Patch(Patch &&) = default;
    Patch & operator=(const Patch &) = default;
    Patch & operator=(Patch &&) = default;
    ~Patch();
};

struct Bone{

    MdlInteger<unsigned short> nBonemap;
    Orientation QBone;
    Vector TBone;
    unsigned int nPadding = 0;
    MdlInteger<unsigned short> nNameIndex;
};

/**** NODE ELEMENTS ****/

/// NODE_HEADER
struct Header{
    //Binary members
    MdlInteger<unsigned short> nType = 0;
    MdlInteger<unsigned short> nSupernodeNumber;
    MdlInteger<unsigned short> nNameIndex;
    unsigned short nPadding1 = 0;
    MdlInteger<unsigned int> nOffsetToRoot = 0;
    MdlInteger<unsigned int> nOffsetToParent = 0;
    Vector vPos;
    Orientation oOrient;
    ArrayHead ChildrenArray;
    ArrayHead ControllerArray;
    ArrayHead ControllerDataArray;

    //Added members
    std::vector<Node> Children;
    std::vector<Controller> Controllers;
    std::vector<double> ControllerData;
    std::vector<MdlInteger<unsigned short>> ChildIndices;
    MdlInteger<unsigned short> nParentIndex;
    Vector vFromRoot;
};

/// NODE_LIGHT
struct LightHeader{

    //Binary members
    double fFlareRadius = 0.0;                      /// 0   4B float
    ArrayHead UnknownArray;                         /// 4  12B uint32 (x3)
    ArrayHead FlareSizeArray;                       /// 16 12B uint32 (x3)
    ArrayHead FlarePositionArray;                   /// 28 12B uint32 (x3)
    ArrayHead FlareColorShiftArray;                 /// 40 12B uint32 (x3)
    ArrayHead FlareTextureNameArray;                /// 52 12B uint32 (x3)
    MdlInteger<unsigned int> nLightPriority = 0;    /// 64  4B uint32
    MdlInteger<unsigned int> nAmbientOnly = 0;      /// 68  4B uint32
    MdlInteger<unsigned int> nDynamicType = 0;      /// 72  4B uint32
    MdlInteger<unsigned int> nAffectDynamic = 0;    /// 76  4B uint32
    MdlInteger<unsigned int> nShadow = 0;           /// 80  4B uint32
    MdlInteger<unsigned int> nFlare = 0;            /// 84  4B uint32
    MdlInteger<unsigned int> nFadingLight = 0;      /// 88  4B uint32

    //Added members
    std::vector<Name> FlareTextureNames;
    std::vector<Color> FlareColorShifts;
    std::vector<double> FlarePositions;
    std::vector<double> FlareSizes;
};

/// NODE_EMITTER
struct EmitterHeader{
    //Binary Members
    double fDeadSpace = 0.0;                /// 0    4B float
    double fBlastRadius = 0.0;              /// 4    4B float
    double fBlastLength = 0.0;              /// 8    4B float
    unsigned int nBranchCount = 0;          /// 12   4B uint32
    double fControlPointSmoothing = 0.0;    /// 16   4B float
    unsigned int nxGrid = 0;                /// 20   4B uint32
    unsigned int nyGrid = 0;                /// 24   4B uint32
    unsigned int nSpawnType = 0;            /// 28   4B uint32
    std::string cUpdate;                    /// 32  32B string ("Fountain", "Single", "Explosion", "Lightning")
    std::string cRender;                    /// 64  32B string ("Normal", "Billboard_to_Local_Z", "Billboard_to_World_Z", "Aligned_to_World_Z", "Aligned_to_Particle_Dir", "Motion_Blur", "Linked")
    std::string cBlend;                     /// 96  32B string ("Normal", "Lighten", "PunchThrough", "Punch-Through")
    std::string cTexture;                   /// 128 32B string
    std::string cChunkName;                 /// 160 16B string
    unsigned int nTwosidedTex = 0;          /// 176  4B uint32
    unsigned int nLoop = 0;                 /// 180  4B uint32
    unsigned short nRenderOrder = 0;        /// 184  4B uint16 (0-8)
    unsigned char nFrameBlending = 0;       /// 186  1B ubyte
    std::string cDepthTextureName;          /// 187 32B string
    unsigned char nPadding1 = 0;            /// 219  1B padding
    unsigned int nFlags = 0;                /// 220  4B uint32 (flags)
};

/// NODE_REFERENCE
struct ReferenceHeader{
    //Binary members
    std::string sRefModel;                  /// 0  32B string
    unsigned int nReattachable = 0;         /// 32  4B uint32
};

/// NODE_MESH
struct MeshHeader{
    //Binary members
    unsigned int nFunctionPointer0 = 0;
    unsigned int nFunctionPointer1 = 0;
    ArrayHead FaceArray;
    Vector vBBmin; //not exported
    Vector vBBmax; //not exported
    double fRadius = 0.0; //not exported
    Vector vAverage; //not exported
    Color fDiffuse;
    Color fAmbient;
    unsigned int nTransparencyHint = 0; // 0 (mostly), 1, 2, 3, 4, 5, 7, 8, 13
    std::string cTexture1;
    std::string cTexture2;
    std::string cTexture3;   // Not supported by KotorMax
    std::string cTexture4;   // Not supported by KotorMax
    ArrayHead IndexCounterArray;
    ArrayHead IndexLocationArray;
    ArrayHead MeshInvertedCounterArray;
        /* MagnusII:
         * = 1, always one UInt32
         * sequence goes like this: first
         * trimesh node is 98, then:
         * 98..00, 100
         * 199..101, 200
         * 399..?
         */
    std::array<int, 3> nUnknown3 = {-1, -1, 0};
    char nSaberUnknown1 = 3; // 3 for non-saber
    char nSaberUnknown2 = 0; // 0 for non-saber
    char nSaberUnknown3 = 0; // 0 for non-saber
    char nSaberUnknown4 = 0; // 0 for non-saber
    char nSaberUnknown5 = 0; // 0 for non-saber
    char nSaberUnknown6 = 0; // 0 for non-saber
    char nSaberUnknown7 = 0; // 0 for non-saber
    char nSaberUnknown8 = 0; // 0 for non-saber
    int nAnimateUV = 0;
    double fUVDirectionX = 0.0;
    double fUVDirectionY = 0.0;
    double fUVJitter = 0.0;
    double fUVJitterSpeed = 0.0;

    unsigned int nMdxDataSize = 0;
    unsigned int nMdxDataBitmap = 0;
    MdlInteger<unsigned int> nOffsetToMdxVertex;
    MdlInteger<unsigned int> nOffsetToMdxNormal;
    MdlInteger<unsigned int> nOffsetToMdxColor; //never present
    MdlInteger<unsigned int> nOffsetToMdxUV1;
    MdlInteger<unsigned int> nOffsetToMdxUV2;
    MdlInteger<unsigned int> nOffsetToMdxUV3; //never present
    MdlInteger<unsigned int> nOffsetToMdxUV4; //never present
    MdlInteger<unsigned int> nOffsetToMdxTangent1;
    MdlInteger<unsigned int> nOffsetToMdxTangent2; //never present
    MdlInteger<unsigned int> nOffsetToMdxTangent3; //never present
    MdlInteger<unsigned int> nOffsetToMdxTangent4; //never present

    unsigned short nNumberOfVerts = 0;
    unsigned short nTextureNumber = 0;
    char nHasLightmap = 0;
    char nRotateTexture = 0;
    char nBackgroundGeometry = 0;
    char nShadow = 0;
    char nBeaming = 0;
    char nRender = 1;
    char nDirtEnabled = 0; //K2
    char nPadding1 = 0; //K2
    short nDirtTexture = 1; //K2
    short nDirtCoordSpace = 1; //K2
    char nHideInHolograms = 0; //K2
    char nPadding2 = 0; //K2
    short nPadding3 = 0;
    double fTotalArea = 0.0; //not exported
    unsigned int nPadding = 0;
    unsigned int nOffsetIntoMdx = 0;
    unsigned int nOffsetToVertArray = 0; //PC only

    /*** ndix UR's knowledge
    # item offset size (bytes) data type notes
    73 Dirt Enabled 314 1 ubyte K2ONLY
    74 Padding? 315 1 ubyte K2ONLY
    75 Dirt Texture 316 2 uint16 K2ONLY
    76 Dirt Coord space 318 2 uint16 K2ONLY
    77 Hide in holograms 320 1 ubyte K2ONLY
    78 Padding? 321 1 ubyte K2ONLY
    79 Padding? 322 1 uint16 K1@314
    80 Total Surface Area 324 4 float K1@316
    81 Padding 328 4 uint32 K1@320
    82 MDX data location 332 4 uint32 K1@324
    83 Vertex data location 336 4 uint32 K1@328
    */

    //Added members
    std::vector<Face> Faces;
    std::vector<std::array<unsigned short, 3>> VertIndices;
    std::vector<Vertex> Vertices;
    std::vector<Vertex> TempVertices;
    std::vector<Vector> TempVerts;
    std::vector<Vector> TempTverts;
    std::vector<Vector> TempTverts1;
    std::vector<Vector> TempTverts2;
    std::vector<Vector> TempTverts3;
    std::vector<Vector> TempNormals; // explicit MDX normals supplied by ASCII
    bool bPreserveMdxNormals = false; // true when ASCII supplied explicit normals; do not overwrite them after TempNormals is cleared
    std::vector<std::array<Vector, 3>> TempTangent1; // explicit MDX tangent basis 1, stored as B/T/N triples
    bool bPreserveMdxTangent1 = false;
    std::vector<Color> TempColors;
    std::array<bool, 4> TangentSpace = {false, false, false, false};
    VertexData MDXData;
    unsigned int nVertIndicesCount = 0;
    unsigned int nVertIndicesLocation = 0;
    MdlInteger<unsigned int> nMeshInvertedCounter;
    MeshHeader() = default;
    MeshHeader(const MeshHeader &) = default;
    MeshHeader(MeshHeader &&) = default;
    MeshHeader & operator=(const MeshHeader &) = default;
    MeshHeader & operator=(MeshHeader &&) = default;
    ~MeshHeader();
};

/// NODE_SKIN
struct SkinHeader{
    ArrayHead UnknownArray; //Always 0
    MdlInteger<unsigned int> nOffsetToMdxWeightValues;
    MdlInteger<unsigned int> nOffsetToMdxBoneIndices;
    unsigned int nOffsetToBonemap = 0;
    unsigned int nNumberOfBonemap = 0;
    ArrayHead QBoneArray;
    ArrayHead TBoneArray;
    ArrayHead Array8Array; // boneconstantindices / skin Array8, preserved through ASCII
    std::array<unsigned short, 16> nBoneIndices = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned short nPadding1 = 0; // K2/TSL uses this adjacent short as compact skin slot 16
    unsigned short nPadding2 = 0;

    std::vector<Bone> Bones;
    std::vector<MdlInteger<unsigned short>> BoneNameIndices;
    std::vector<Weight> TempWeights;
    std::vector<MdlInteger<unsigned short>> TempCompactBoneNameIndices; // optional compact slot -> name index from decompiled ASCII
    std::vector<MdlInteger<unsigned short>> TempCompactBoneRawIndices; // exact compact reverse-map header values, including unused/garbage slots
};

/// NODE_DANGLY
struct DanglymeshHeader{

    //Binary members
    ArrayHead ConstraintArray;
    double fDisplacement = 0.0;
    double fTightness = 0.0;
    double fPeriod = 0.0;
    unsigned int nOffsetToData2 = 0;

    //Added members
    std::vector<double> Constraints;
    std::vector<double> TempConstraints;
    std::vector<Vector> Data2;
};

/// NODE_AABB
struct WalkmeshHeader{
    //Binary members
    MdlInteger<unsigned int> nOffsetToAabb = 0;

    //Added members
    Aabb RootAabb;
    std::vector<Aabb> TempAabbNodes; // pre-order AABB tree nodes with child flags
    bool bPreserveAabbTree = false;  // true when ASCII supplied extended AABB nodes
};

/// NODE_SABER
struct SaberHeader{
    //Binary members
    unsigned int nOffsetToSaberVerts = 0;
    unsigned int nOffsetToSaberUVs = 0;
    unsigned int nOffsetToSaberNormals = 0;
    MdlInteger<unsigned int> nInvCount1;
    MdlInteger<unsigned int> nInvCount2;

    //Added members
    std::vector<VertexData> SaberData;
    bool bPreserveSaberData = false; // exact binary saber verts/uvs/normals were supplied
};

struct Node{

    unsigned int nOffset = 0;
    MdlInteger<unsigned int> nAnimation;

    Header Head;
    LightHeader Light;
    EmitterHeader Emitter;
    ReferenceHeader Reference;
    MeshHeader Mesh;
    SkinHeader Skin;
    DanglymeshHeader Dangly;
    WalkmeshHeader Walkmesh;
    SaberHeader Saber;

    Node();
    Node(const Node &);
    Node(Node &&);
    Node & operator=(Node &&);
    ~Node();

    Location GetLocation();
};

/**** HIGHER ELEMENTS ****/

struct Animation{

    //Binary members

    /// Geoheader part
    unsigned int nFunctionPointer0 = 0;
    unsigned int nFunctionPointer1 = 0;
    std::string sName;
    unsigned int nOffsetToRootAnimationNode = 0;
    unsigned int nNumberOfNames = 0;
    ArrayHead RuntimeArray1; //Always 0
    ArrayHead RuntimeArray2; //Always 0
    unsigned int nRefCount = 0; //Always 0
    unsigned char nModelType = 5; //1 - geometry, 2 - model, 5 - animation
    std::array<unsigned char, 3> nPadding = {0, 0, 0};

    /// Animation-specific part
    double fLength = 0.0;
    double fTransition = 0.0;
    std::string sAnimRoot;
    ArrayHead EventArray;
    unsigned int nPadding2 = 0; //Always 0

    //Added members
    unsigned int nOffset = 0;
    Node RootAnimationNode;
    std::vector<Event> Events;
    std::vector<Node> ArrayOfNodes;

    Animation();
    Animation(const Animation &) = default;
    Animation(Animation &&) = default;
    Animation & operator=(const Animation &) = delete;
    Animation & operator=(Animation &&) = default;
    ~Animation();
};

struct GeometryHeader{
    //Binary members
    unsigned int nFunctionPointer0 = 0;
    unsigned int nFunctionPointer1 = 0;
    std::string sName;
    unsigned int nOffsetToRootNode = 0;
    unsigned int nTotalNumberOfNodes = 0; // Total number of nodes = Number of nodes in this model + Total number of nodes in supermodel + 1
    ArrayHead RuntimeArray1; //Always 0
    ArrayHead RuntimeArray2; //Always 0
    unsigned int nRefCount = 0;  //Always 0
    unsigned char nModelType = 2; //1 - geometry, 2 - model, 5 - animation
    std::array<unsigned char, 3> nPadding = {0, 0, 0};
};

struct ModelHeader{

    //Binary members
    unsigned char nClassification = 0;
    unsigned char nSubclassification = 0;
    unsigned char nUnknown = 0;
    unsigned char nAffectedByFog = 1;
    unsigned int nChildModelCount = 0; //Always 0
    ArrayHead AnimationArray;

    unsigned int nSupermodelReference = 0;
    /* Pointer to supermodel?
    Always present when there is a supermodel
    but very variable, models sharing a supermodel
    do not share this value.
    */
    Vector vBBmin;
    Vector vBBmax;
    double fRadius = 0.0;
    double fScale = 0.0;
    std::string cSupermodelName;
    unsigned int nOffsetToHeadRootNode;
    int nPadding = 0; //Always 0
    unsigned int nMdxLength2 = 0;
    unsigned int nOffsetIntoMdx = 0; //Always 0
    ArrayHead NameArray;

    //Added members
    unsigned nExcludedVerts = 0;
    unsigned nTotalVertCount = 0;
    unsigned nTotalTangent1Count = 0;
    unsigned nTotalTangent2Count = 0;
    unsigned nTotalTangent3Count = 0;
    unsigned nTotalTangent4Count = 0;
    unsigned nNodeCount = 0; //Only the nodes that actually exist, and only the ones in this model
    Vector vLytPosition;
    bool bCompressQuaternions = false;
    bool bHeadLink = false;
    bool bPreserveTotalNumberOfNodes = false; // ASCII supplied totalnodes; do not recompute away supermodel-inclusive binary value
    GeometryHeader GH;
    Node RootNode;
    std::vector<Animation> Animations;
    std::vector<Name> Names;
    std::vector<Node> ArrayOfNodes;
    std::vector<unsigned short> NameIndicesInTreeOrder;
    //std::vector<std::vector<LinkedFace>> LinkedFacesPointers;
    std::vector<std::vector<Patch>> PatchArrayPointers;

    void BuildTreeOrderArray(Node& node);
};

struct FileHeader{
    //Binary members
    unsigned int nZero = 0; //Always 0
    unsigned int nMdlLength;
    unsigned int nMdxLength;

    //Added members
    ModelHeader MH;
};

struct BWMHeader{

    //Data
    int nType = 0;
    Vector vUse1;
    Vector vUse2;
    Vector vDwk1;
    Vector vDwk2;
    Vector vPosition;
    unsigned int nNumberOfVerts = 0;
    unsigned int nOffsetToVerts = 0;
    unsigned int nNumberOfFaces = 0;
    unsigned int nOffsetToIndices = 0;
    unsigned int nOffsetToMaterials = 0;
    unsigned int nOffsetToNormals = 0;
    unsigned int nOffsetToDistances = 0;
    unsigned int nNumberOfAabb = 0;
    unsigned int nOffsetToAabb = 0;
    unsigned int nPadding = 0;
    unsigned int nNumberOfAdjacentFaces = 0;
    unsigned int nOffsetToAdjacentFaces = 0;
    unsigned int nNumberOfEdges= 0;
    unsigned int nOffsetToEdges = 0;
    unsigned int nNumberOfPerimeters = 0;
    unsigned int nOffsetToPerimeters = 0;
    std::vector<Aabb> aabb;
    std::vector<Face> faces;
    std::vector<Vertex> verts;
    std::vector<Edge> edges;
    std::vector<Perimeter> perimeters;

    BWMHeader();
};

/// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
  /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
/// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///

class MDX: public BinaryFile{
    static const std::string sClassName;
  public:
     //Getters
     const std::string GetName();

    //Friends
    friend class MDL;
    //We need this so that MDL::functions can call protected members inherited from BinaryFile
};

class ASCII: public TextFile{
  public:
    bool Read(MDL & Mdl);
    bool ReadWalkmesh(MDL & Mdl, bool bPwk);
};

enum ModelSource {NoSource = 0, AsciiSource, BinarySource};
enum DuplicateNodeTactic {NoTactic = 0, AlwaysKeep, AlwaysRemove};
extern Version version;

class MDL: public BinaryFile{
    static const std::string sClassName;
    std::unique_ptr<FileHeader> FH;
    ModelSource src = NoSource;
    std::stringstream ssReport;

    /// Recursive reading functions
    void ParseNode(Node & node, std::vector<unsigned int> & offsets, Vector vFromRoot, bool bMinimal = false);
    void ParseAabb(Aabb & aabb, std::vector<unsigned int> & visitedOffsets);
    void LinearizeGeometry(Node & node, std::vector<Node> & ArrayOfNodes);
    void LinearizeAnimation(Node & node, std::vector<Node> & ArrayOfNodes, int nChildIndex = -1);

    bool CheckNodes(std::vector<Node> & nodes, std::stringstream & ssReturn, int nAnimation);

    //Writing
    void WriteNodes(Node & node);
    void WriteMDX(Node & node);
    void WriteAabb(const Aabb & aabb);
    void GatherChildren(Node & node, std::vector<Node> & ArrayOfNodes, Vector vFromRoot);

    //Calculating
    void CreatePatches();
    void AsciiPostProcess(std::vector<std::string> & sBumpmapped);
    void DetermineSmoothing();
    bool FindNormal(int nCheckFrom, const int & nPatchCount, const int & nPatch, const int & nPatchGroup, const Vector & vBaseNormal, const Vector & vTargetNormal, std::vector<int> & SmoothedPatches, std::stringstream & file);
    char FindTangentSpace(int nCheckFrom, const int & nPatchCount, const int & nPatch, const int & nPatchGroup, const Vector & vBaseBitangent, const Vector & vBaseTangent, const Vector & vBaseNormal, const Vector & vTargetBitangent, const Vector & vTargetTangent, const Vector & vTargetNormal, std::vector<int> & SmoothedPatches, std::stringstream & file);
    void GenerateSmoothingNumber(std::vector<int> & SmoothingGroup, const std::vector<unsigned long int> & nSmoothinGroupNumbers, const int & nSmoothinGroupCounter, const int & pg, std::stringstream & file);
    void ConsolidateSmoothingGroups(int pg, std::vector<std::vector<unsigned long int>> & Numbers, std::vector<bool> & DoneGroups);
    std::string MakeUniqueName(int nNameIndex);
    std::vector<Vertex> GetWokVertData(Node & node);

    //Getters
    const std::string GetName();
    unsigned int FunctionPointer1(unsigned int FN_PTR);
    unsigned int FunctionPointer2(unsigned int FN_PTR);

    //Reporters
    void Report(std::string);
    void ProgressSize(int, int);
    void ProgressPos(int);


  public:
    ~MDL();

    std::unique_ptr<ASCII> Ascii;
    std::unique_ptr<ASCII> PwkAscii;
    std::unique_ptr<ASCII> DwkAscii;
    std::unique_ptr<MDX> Mdx;
    std::unique_ptr<WOK> Wok;
    std::unique_ptr<PWK> Pwk;
    std::unique_ptr<DWK> Dwk0;
    std::unique_ptr<DWK> Dwk1;
    std::unique_ptr<DWK> Dwk2;

    //Function pointers
    void (*PtrReport)(std::string) = nullptr;
    void (*PtrProgressSize)(int, int) = nullptr;
    void (*PtrProgressPos)(int) = nullptr;

    //Friends
    friend ASCII;
    friend Node;

    //Options
    bool bK2 = true;
    int nSupermodel = 0; /// 0 = not present, 1 = K1, 2 = K2
    bool bXbox = false;
    bool bDebug = false;
    bool bWriteSmoothing = false;
    bool bDetermineSmoothing = true;
    bool bSmoothAreaWeighting = true;
    bool bSmoothAngleWeighting = false;
    bool bMinimizeVerts = false;
    bool bMinimizeVerts2 = false;
    bool bWriteAnimations = true;
    bool bSkinToTrimesh = false;
    bool bLightsaberToTrimesh = false;
    bool bBezierToLinear = false;
    bool bExportWok = false;
    bool bCreaseAngle = false;
    unsigned nCreaseAngle = 60;
    bool bUseWokData = true;

    //Getters
    std::unique_ptr<FileHeader> & GetFileData();
    std::vector<char> & GetAsciiBuffer();
    int GetNameIndex(const std::string & sName);
    int GetNodeIndexByNameIndex(int nIndex, int nAnimation = -1);
    bool HeadLinked();
    bool NodeExists(const std::string & sNodeName);
    std::string & GetNodeName(Node & node);
    void UpdateTexture(Node & node, const std::string & sNew, int nTex);
    void GetLytPositionFromWok();
    unsigned GetHeaderOffset(const Node & node, unsigned short nHeader);
    std::stringstream & GetReport();

    //Printers
    void SaveReport();

    //Loaders
    bool Compile();
    void DecompileModel(bool bMinimal = false);
    void BwmAsciiPostProcess(BWMHeader & data, std::vector<Vector> & vertices, bool bDwk = true);
    void CheckPeculiarities();
    void FlushData();
    void ConvertToAscii(int nDataType, std::stringstream & sReturn, void * Data);

    //Setters/general
    bool LinkHead(bool bLink);
    void WriteUintToPlaceholder(unsigned int nUint, int nOffset);
    void WriteByteToPlaceholder(unsigned char nByte, int nOffset);

    //ascii
    void ExportAscii(std::string &sExport);
    void ExportPwkAscii(std::string &sExport);
    void ExportDwkAscii(std::string &sExport);
    void ExportWokAscii(std::string &sExport);
    void FlushAscii();
    std::vector<char> & CreateAsciiBuffer(int nSize);
    bool ReadAscii();
};

class ReportObject{
    MDL * ptr_mdl = nullptr;

  public:
    ReportObject(const MDL & mdl): ptr_mdl(const_cast<MDL*>(&mdl)) {}
    template<class T>
    ReportObject & operator<<(const T & os){
        std::cout << os;
        if(ptr_mdl != nullptr) ptr_mdl->GetReport() << os;
        return *this;
    }
};

class BWM: public BinaryFile{
    std::unique_ptr<BWMHeader> Bwm;

  public:
    //Getters
    std::unique_ptr<BWMHeader> & GetData() { return Bwm; }

    //Loaders
    void ProcessBWM();
    void Compile();
};

class PWK: public BWM{
    static const std::string sClassName;

  public:
    //Getters
    const std::string GetName();
};

class DWK: public BWM{
    static const std::string sClassName;
    unsigned char nDWK = 0;

  public:
    //Getters
    const std::string GetName();
    void SetDwk(unsigned char n){ nDWK = n; }
    unsigned char GetDwk(){ return nDWK; }
};

class WOK: public BWM{
    static const std::string sClassName;

  public:
    //Getters
    const std::string GetName();
    void WriteWok(Node & node, Vector vLytPos, std::stringstream * ptrssFile);
};

int ReturnController(std::string sController, int nType);
std::string ReturnClassificationName(int nClassification);
std::string ReturnControllerName(int, int nType);

void BuildAabb(Aabb & aabb, const std::vector<Face*> & faces, std::stringstream * file = nullptr);
void LinearizeAabb(const Aabb & aabbroot, std::vector<Aabb> & aabbarray);


#endif // MDL_H_INCLUDED
