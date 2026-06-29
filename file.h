#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED

#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <type_traits>
#include <limits>
#include <shlwapi.h>
#include "geometry.h"

HANDLE bead_CreateReadFile(const std::string & sFilename);
HANDLE bead_CreateReadFile(const std::wstring & sFilename);
HANDLE bead_CreateWriteFile(const std::string & sFilename);
HANDLE bead_CreateWriteFile(const std::wstring & sFilename);
long unsigned bead_GetFileLength(HANDLE hFile);
bool bead_ReadFile(HANDLE hFile, std::vector<char> & sBuffer, unsigned long nToRead = ~0);
bool bead_WriteFile(HANDLE hFile, const std::string & sBuffer, unsigned long nToWrite = ~0);
bool bead_WriteFile(HANDLE hFile, const std::vector<char> & sBuffer, unsigned long nToWrite = ~0);

template <class IntegerType>
class MdlInteger;

class Path{
  public:
    std::wstring sFilename;
    std::wstring sFullPath;
};

class File{
  protected:
    bool bDataLoaded = false;
    Path path;
    std::vector<char> sBuffer;
    unsigned int nPosition = 0;

  public:
    virtual ~File();

    //Getters
    std::vector<char> & GetBuffer(){ return sBuffer; }
    const std::vector<char> & GetBuffer() const { return sBuffer; }
    const std::wstring & GetFilename() const { return path.sFilename; }
    const std::wstring & GetFullPath() const { return path.sFullPath; }
    virtual const std::string GetName();
    bool empty(){ return !bDataLoaded; }
    void Export(std::string &sExport);

    //Setters
    void SetFilePath(const std::wstring & sPath);

    //Loaders/Unloaders
    virtual std::vector<char> & CreateBuffer(long unsigned nSize);
    virtual void FlushAll();

};

class BinaryFile: public File{

  protected:
    //For coloring bytes
    std::vector<int> bKnown;
    std::vector<char> sCompareBuffer;
    void MarkBytes(unsigned int nOffset, int nLength, int nClass);
    void MarkDataBorder(unsigned nOffset);

    //Reading functions
    unsigned char * ReadBytes(unsigned nBytes, int nMarking, const std::string & sDesc, unsigned * nPlaceholder = nullptr, const char * sAction = "read string"){
        unsigned nCurPos = nPlaceholder == nullptr ? nPosition : *nPlaceholder;
        (void)sDesc;

        if(nCurPos > sBuffer.size() || nBytes > sBuffer.size() - nCurPos){
            throw mdlexception(std::string("Attempting to ") + sAction + " in " + GetName() + ", which would read past the buffer size (" + std::to_string(sBuffer.size()) + ").");
        }

        MarkBytes(nCurPos, nBytes, nMarking);

        if(nPlaceholder == nullptr){
            if(nBytes > std::numeric_limits<unsigned int>::max() - nPosition){
                throw mdlexception("Attempting to advance read position in " + GetName() + " past the 32-bit offset limit.");
            }
            nPosition += nBytes;
        }
        else{
            if(nBytes > std::numeric_limits<unsigned int>::max() - *nPlaceholder){
                throw mdlexception("Attempting to advance placeholder in " + GetName() + " past the 32-bit offset limit.");
            }
            *nPlaceholder += nBytes;
        }

        if(nBytes == 0){
            return sBuffer.empty() ? nullptr : reinterpret_cast<unsigned char*>(sBuffer.data());
        }
        return reinterpret_cast<unsigned char*>(sBuffer.data() + nCurPos);
    }
    template <class NumType>
    static const char * ReadNumberAction(){
        return std::is_floating_point<NumType>::value ? "read float" : "read integer";
    }
    template <class NumType>
    NumType ReadNumber(NumType * p_number, int nMarking, const std::string & sDesc, unsigned * nPlaceholder = nullptr){
        NumType result;
        std::memcpy(&result, ReadBytes(sizeof(NumType), nMarking, sDesc, nPlaceholder, ReadNumberAction<NumType>()), sizeof(NumType));
        if(p_number) *p_number = result;
        return result;
    }
    template <class IntegerType>
    MdlInteger<IntegerType> ReadNumber(MdlInteger<IntegerType> * p_number, int nMarking, const std::string & sDesc, unsigned * nPlaceholder = nullptr){
        IntegerType raw = 0;
        std::memcpy(&raw, ReadBytes(sizeof(IntegerType), nMarking, sDesc, nPlaceholder, ReadNumberAction<IntegerType>()), sizeof(IntegerType));
        MdlInteger<IntegerType> result(raw);
        if(p_number) *p_number = result;
        return result;
    }
    std::string ReadString(std::string * p_string, unsigned nBytes, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        std::string result;
        unsigned nCurPos = p_insert != nullptr ? *p_insert : nPosition;
        if(nBytes){
            result.assign(reinterpret_cast<const char*>(ReadBytes(nBytes, nMarking, sDesc, p_insert, "read string")), nBytes);
        }
        else{
            if(nCurPos >= sBuffer.size()){
                throw mdlexception("Attempting to read string in " + GetName() + ", which would read past the buffer size (" + std::to_string(sBuffer.size()) + ").");
            }
            const char * pStart = sBuffer.data() + nCurPos;
            const std::size_t nRemaining = sBuffer.size() - nCurPos;
            const void * pTerminator = std::memchr(pStart, '\0', nRemaining);
            if(pTerminator == nullptr){
                throw mdlexception("Attempting to read unterminated string in " + GetName() + " at offset " + std::to_string(nCurPos) + ".");
            }
            const std::size_t nStringBytesSize = static_cast<const char*>(pTerminator) - pStart + 1;
            if(nStringBytesSize > std::numeric_limits<unsigned>::max()){
                throw mdlexception("Attempting to read an oversized string in " + GetName() + ".");
            }
            const unsigned nStringBytes = static_cast<unsigned>(nStringBytesSize);
            result = reinterpret_cast<const char*>(ReadBytes(nStringBytes, nMarking, sDesc, p_insert, "read string"));
        }
        if(p_string){
            *p_string = result;
        }
        return result;
    }

    //Writing functions
    template <class ElementType>
    void ReserveForAppend(std::vector<ElementType> & buffer, std::size_t nAdditional){
        if(nAdditional > buffer.max_size() - buffer.size()){
            throw mdlexception("Attempting to grow " + GetName() + ", which would exceed the maximum buffer size.");
        }
        const std::size_t nNeeded = buffer.size() + nAdditional;
        if(buffer.capacity() >= nNeeded) return;

        std::size_t nNewCapacity = buffer.capacity();
        if(nNewCapacity == 0) nNewCapacity = 1024;
        while(nNewCapacity < nNeeded){
            const std::size_t nPrevious = nNewCapacity;
            nNewCapacity += nNewCapacity / 2;
            if(nNewCapacity <= nPrevious || nNewCapacity > buffer.max_size()){
                nNewCapacity = nNeeded;
                break;
            }
        }
        buffer.reserve(nNewCapacity);
    }

    unsigned WriteBytes(const unsigned char * p_bytes, unsigned nBytes, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        (void)sDesc;
        if(nBytes > 0 && p_bytes == nullptr){
            throw mdlexception("Attempting to write null bytes in " + GetName() + ".");
        }

        unsigned nReturn = p_insert == nullptr ? nPosition : *p_insert;

        if(p_insert != nullptr){
            if(*p_insert > sBuffer.size() || nBytes > sBuffer.size() - *p_insert){
                throw mdlexception("Attempting to write bytes in " + GetName() + ", which would write past the buffer size (" + std::to_string(sBuffer.size()) + ").");
            }
            if(nBytes > 0){
                std::copy(p_bytes, p_bytes + nBytes, sBuffer.begin() + *p_insert);
            }
            return nReturn;
        }

        if(nBytes > std::numeric_limits<unsigned int>::max() - nPosition){
            throw mdlexception("Attempting to write bytes in " + GetName() + ", which would exceed the 32-bit offset limit.");
        }

        if(!bKnown.empty()){
            bKnown.back() |= 0x10000;
        }
        if(nBytes > 0){
            ReserveForAppend(sBuffer, nBytes);
            ReserveForAppend(bKnown, nBytes);
            sBuffer.insert(sBuffer.end(), reinterpret_cast<const char*>(p_bytes), reinterpret_cast<const char*>(p_bytes + nBytes));
            bKnown.insert(bKnown.end(), nBytes, nMarking);
            bKnown.back() |= 0x10000;
        }
        nPosition += nBytes;
        return nReturn;
    }
    template <class NumType>
    unsigned WriteNumber(NumType * p_number, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        if(p_number == nullptr){
            throw mdlexception("Attempting to write a null number in " + GetName() + ".");
        }
        return WriteBytes(reinterpret_cast<const unsigned char*>(p_number), sizeof(NumType), nMarking, sDesc, p_insert);
    }
    template <class IntegerType>
    unsigned WriteNumber(MdlInteger<IntegerType> * p_number, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        if(p_number == nullptr){
            throw mdlexception("Attempting to write a null integer in " + GetName() + ".");
        }
        IntegerType raw = static_cast<IntegerType>(*p_number);
        return WriteBytes(reinterpret_cast<const unsigned char*>(&raw), sizeof(IntegerType), nMarking, sDesc, p_insert);
    }
    unsigned WriteString(std::string * p_string, unsigned nBytes, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        if(p_string == nullptr){
            throw mdlexception("Attempting to write a null string in " + GetName() + ".");
        }
        if(nBytes == 0){
            if(p_string->size() > static_cast<std::size_t>(std::numeric_limits<unsigned>::max()) - 1u){
                throw mdlexception("Attempting to write an oversized string in " + GetName() + ".");
            }
            return WriteBytes(reinterpret_cast<const unsigned char*>(p_string->c_str()), static_cast<unsigned>(p_string->size() + 1), nMarking, sDesc, p_insert);
        }
        if(p_string->size() >= nBytes){
            return WriteBytes(reinterpret_cast<const unsigned char*>(p_string->data()), nBytes, nMarking, sDesc, p_insert);
        }
        std::string sPadded = *p_string;
        sPadded.resize(nBytes, '\0');
        return WriteBytes(reinterpret_cast<const unsigned char*>(sPadded.data()), nBytes, nMarking, sDesc, p_insert);
    }
    unsigned WriteFloat(const double * p_double, int nMarking, const std::string & sDesc, unsigned * p_insert = nullptr){
        if(p_double == nullptr){
            throw mdlexception("Attempting to write a null float in " + GetName() + ".");
        }
        float fHelper = static_cast<float>(*p_double);
        return WriteNumber(&fHelper, nMarking, sDesc, p_insert);
    }

    // Raw helper API used by lower-level binary readers.
    int ReadInt(unsigned int * nPosition, int nMarking, int nBytes = 4);
    float ReadFloat(unsigned int * nPosition, int nMarking, int nBytes = 4);
    void ReadString(std::string & sArray1, unsigned int *nPosition, int nMarking, int nNumber);
    Vector ReadVector(unsigned int * nPosition, int nMarking, int nBytes = 4);

    void WriteInt(int nInt, int nKnown, int nBytes = 4);
    void WriteFloat(float fFloat, int nKnown, int nBytes = 4);
    void WriteString(const std::string & sString, int nKnown);
    void WriteByte(char cByte, int nKnown);
    void WriteIntToPH(int nInt, int nPH, unsigned int & nContainer); //PH is placeholder
    void WriteAtOffset4(int nInt, unsigned int nOffset);
    void WriteAtOffset2(short nShort, unsigned int nOffset);
    void WriteAtOffset1(char nChar, unsigned int nOffset);
    void WriteAtOffset(float fFloat, unsigned int nOffset);
    void WriteAtOffset(const std::string & sString, unsigned int nOffset);

  public:
    ~BinaryFile();

    //Friends
    friend class EditorDlgWindow;

    struct BufferState{
        std::vector<char> sBuffer;
        std::vector<int> bKnown;
        unsigned int nPosition = 0;
        bool bDataLoaded = false;
    };

    //Getters
    std::vector<int> & GetKnownData(){ return bKnown; }
    std::vector<char> & GetCompareData(){ return sCompareBuffer; }
    BufferState CaptureBufferState() const{
        BufferState state;
        state.sBuffer = sBuffer;
        state.bKnown = bKnown;
        state.nPosition = nPosition;
        state.bDataLoaded = bDataLoaded;
        return state;
    }
    void RestoreBufferState(const BufferState & state){
        sBuffer = state.sBuffer;
        bKnown = state.bKnown;
        nPosition = state.nPosition;
        bDataLoaded = state.bDataLoaded;
    }
    //Loaders/Unloaders
    std::vector<char> & CreateBuffer(long unsigned nSize) override;
    void FlushAll() override;
    void Compare(File & file){
        sCompareBuffer = file.GetBuffer();
    }
};

class TextFile: public File{
  protected:
    unsigned int nLine = 1;
    bool ReadFloat(double & fNew, std::string * sGet = nullptr, bool bPrint = false);
    //bool ReadFloat(double & fNew, std::string & sGetFloat, bool bPrint = false);
    bool ReadInt(int & nNew, std::string * sGet = nullptr, bool bPrint = false);
    bool ReadUInt(unsigned int & nNew, std::string * sGet = nullptr, bool bPrint = false);
    bool ReadUntilText(std::string & sHandle, bool bToLowercase = true);
    void SkipLine();
    bool EmptyRow();

  public:
    std::vector<char> & CreateBuffer(long unsigned nSize) override;
    void FlushAll() override;
};

enum DataType {
    DT_string,
    DT_int,
    DT_uint,
    DT_bool,
    DT_float
};
struct IniOption{
    std::string sToken;
    DataType nType = static_cast<DataType>(0);
    void * lpVariable = nullptr;

    IniOption(){}
    IniOption(const std::string & s, DataType t, void * lptr): sToken(s), nType(t), lpVariable(lptr) {}
};

class IniFile: public TextFile{
    std::vector<IniOption> Options;
  public:
    void AddIniOption(const std::string & s, DataType t, void * lptr){
        Options.push_back(IniOption(s, t, lptr));
    }
    void ReadIni(std::wstring &);
    void WriteIni(std::wstring &);
};
// Byte-conversion helper unions.
union BB2{ short i; unsigned short u; char bytes[2]; };
union BB4{ int i; unsigned int u; float f; char bytes[4]; };
union BB8{ long i; unsigned long u; double d; char bytes[8]; };
extern BB2 ByteBlock2;
extern BB4 ByteBlock4;
extern BB8 ByteBlock8;
#endif // FILE_H_INCLUDED
