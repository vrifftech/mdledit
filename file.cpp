#include "general.h"

#include "file.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cwchar>
#include <cstdint>
#include <limits>
#include <shlwapi.h>

BB2 ByteBlock2;
BB4 ByteBlock4;
BB8 ByteBlock8;

const std::string File::GetName(){
    return "";
}

std::vector<char> & File::CreateBuffer(long unsigned nSize){
    if(static_cast<std::size_t>(nSize) > sBuffer.max_size()){
        throw mdlexception("Requested file buffer is too large.");
    }
    bDataLoaded = true;
    nPosition = 0;
    sBuffer.assign(static_cast<std::size_t>(nSize), 0);
    return sBuffer;
}

void File::FlushAll(){
    bDataLoaded = false;
    nPosition = 0;
    std::vector<char>().swap(sBuffer);
}

File::~File(){}

void File::Export(std::string &sExport){
    sExport.assign(sBuffer.begin(), sBuffer.end());
}

std::vector<char> & BinaryFile::CreateBuffer(long unsigned nSize){
    const std::size_t nRequested = static_cast<std::size_t>(nSize);
    if(nRequested > sBuffer.max_size() || nRequested > bKnown.max_size()){
        throw mdlexception("Requested binary file buffer is too large.");
    }
    bDataLoaded = true;
    nPosition = 0;
    sBuffer.assign(nRequested, 0);
    bKnown.assign(nRequested, 0);
    return sBuffer;
}

void BinaryFile::FlushAll(){
    bDataLoaded = false;
    nPosition = 0;
    std::vector<char>().swap(sBuffer);
    std::vector<char>().swap(sCompareBuffer);
    std::vector<int>().swap(bKnown);
}

BinaryFile::~BinaryFile(){}

std::vector<char> & TextFile::CreateBuffer(long unsigned nSize){
    nLine = 1;
    return File::CreateBuffer(nSize);
}

void TextFile::FlushAll(){
    nLine = 1;
    File::FlushAll();
}

HANDLE bead_CreateReadFile(const std::string & sFilename){
    return CreateFileA(sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
}
HANDLE bead_CreateReadFile(const std::wstring & sFilename){
    return CreateFileW(sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
}
HANDLE bead_CreateWriteFile(const std::string & sFilename){
    return CreateFileA(sFilename.c_str(), GENERIC_WRITE, 0x00, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}
HANDLE bead_CreateWriteFile(const std::wstring & sFilename){
    return CreateFileW(sFilename.c_str(), GENERIC_WRITE, 0x00, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}
long unsigned bead_GetFileLength(HANDLE hFile){
    if(hFile == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER lnSize;
    if(GetFileSizeEx(hFile, &lnSize) && lnSize.QuadPart >= 0){
        const unsigned long long nSize = static_cast<unsigned long long>(lnSize.QuadPart);
        const unsigned long long nMax = static_cast<unsigned long long>(std::numeric_limits<long unsigned>::max());
        if(nSize > nMax) return std::numeric_limits<long unsigned>::max();
        return static_cast<long unsigned>(nSize);
    }
    return 0;
}

bool bead_ReadFile(HANDLE hFile, std::vector<char> & sBuffer, unsigned long nToRead){
    if(hFile == INVALID_HANDLE_VALUE) return false;

    const unsigned long fileLength = bead_GetFileLength(hFile);
    unsigned long length = nToRead;
    if(length == ~0ul) length = fileLength;
    if(length > fileLength) length = fileLength;

    // Most full-file callers allocate the buffer before reading. This keeps
    // that behavior, but also makes the helper safe for direct full-file reads
    // into an empty buffer. Header probe reads still keep their fixed small
    // buffers and explicit nToRead values.
    if(nToRead == ~0ul && sBuffer.empty() && length > 0){
        if(static_cast<std::size_t>(length) > sBuffer.max_size()) return false;
        sBuffer.assign(static_cast<std::size_t>(length), 0);
    }
    if(length > sBuffer.size()) length = static_cast<unsigned long>(sBuffer.size());
    if(length == 0) return true;
    if(sBuffer.empty()) return false;

    // Existing callers use bead_ReadFile both for header probes and for full-file
    // reads on the same handle. Preserve the previous behavior by reading from
    // the beginning of the file each time.
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    if(!SetFilePointerEx(hFile, zero, nullptr, FILE_BEGIN)) return false;

    unsigned long totalRead = 0;
    while(totalRead < length){
        DWORD chunk = static_cast<DWORD>(std::min<unsigned long>(length - totalRead, 0x7ffff000ul));
        DWORD bytesRead = 0;
        if(!ReadFile(hFile, sBuffer.data() + totalRead, chunk, &bytesRead, NULL)) return false;
        if(bytesRead == 0) return false;
        totalRead += bytesRead;
    }
    return true;
}
namespace {
    bool bead_WriteRawBuffer(HANDLE hFile, const char * data, size_t bufferLength, unsigned long nToWrite){
        if(hFile == INVALID_HANDLE_VALUE) return false;

        size_t length = (nToWrite == ~0ul) ? bufferLength : static_cast<size_t>(nToWrite);
        if(length > bufferLength) length = bufferLength;
        if(length == 0) return true;
        if(data == nullptr) return false;

        size_t totalWritten = 0;
        while(totalWritten < length){
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(length - totalWritten, 0x7ffff000ul));
            DWORD bytesWritten = 0;
            if(!WriteFile(hFile, data + totalWritten, chunk, &bytesWritten, NULL)) return false;
            if(bytesWritten == 0) return false;
            totalWritten += bytesWritten;
        }
        return true;
    }
}

bool bead_WriteFile(HANDLE hFile, const std::string & sBuffer, unsigned long nToWrite){
    return bead_WriteRawBuffer(hFile, sBuffer.data(), sBuffer.size(), nToWrite);
}

bool bead_WriteFile(HANDLE hFile, const std::vector<char> & sBuffer, unsigned long nToWrite){
    return bead_WriteRawBuffer(hFile, sBuffer.empty() ? nullptr : sBuffer.data(), sBuffer.size(), nToWrite);
}

void File::SetFilePath(const std::wstring & sPath){
    path.sFullPath = sPath;
    const wchar_t * pFilename = PathFindFileNameW(path.sFullPath.c_str());
    path.sFilename = pFilename != nullptr ? pFilename : L"";
}

void BinaryFile::MarkDataBorder(unsigned nOffset){
    if(nOffset >= bKnown.size()){
        throw mdlexception("Attempting to mark a data border in " + GetName() + " outside the buffer.");
    }
    bKnown.at(nOffset) = bKnown.at(nOffset) | 0x20000;
}

void BinaryFile::MarkBytes(unsigned int nOffset, int nLength, int nClass){
    //std::cout << "Setting known: offset " << nOffset << " length " << nLength << " class " << nClass << ".\n";
    if(nOffset > sBuffer.size() || nOffset > bKnown.size()){
        throw mdlexception("Attempting to mark data in " + GetName() + " outside the buffer.");
    }
    if(nOffset > 0) bKnown.at(nOffset - 1) = bKnown.at(nOffset - 1) | 0x10000;
    if(nLength <= 0) return;
    if(static_cast<unsigned>(nLength) > sBuffer.size() - nOffset || static_cast<unsigned>(nLength) > bKnown.size() - nOffset){
        throw mdlexception("Attempting to mark data in " + GetName() + " outside the buffer.");
    }
    for(unsigned n = 0; n < static_cast<unsigned>(nLength); n++){
        // Only bit 0 of the existing class word marks a duplicate
        // interpretation.
        int & nKnown = bKnown[nOffset + n];
        if((nKnown & 0x1) != 0){
            std::cout << "MarkBytes(): Warning! Data already interpreted as " << nKnown << " at offset " << nOffset + n << " in " << GetName() << "! Trying to reinterpret as " << nClass << ".\n";
            throw mdlexception("Interpreting already interpreted data in " + GetName() + ".");
        }
        // The executable writes the new class word directly; it does not carry
        // forward previous high marker bits except for the final-byte marker.
        nKnown = (nClass & 0xFFFF);
        if(n + 1 == static_cast<unsigned>(nLength)) nKnown |= 0x10000;
    }
}



namespace {
    inline void RequireBinaryRange(BinaryFile & file, const std::vector<char> & buffer, unsigned offset, unsigned length, const char * action){
        if(offset > buffer.size() || length > buffer.size() - offset){
            throw mdlexception(std::string("Attempting to ") + action + " in " + file.GetName() + ", which would read past the buffer size (" + std::to_string(buffer.size()) + ").");
        }
    }
}

int BinaryFile::ReadInt(unsigned int * nCurPos, int nMarking, int nBytes){
    if(nCurPos == nullptr) return -1;
    if(nBytes != 1 && nBytes != 2 && nBytes != 4){
        throw mdlexception("Attempting to read an integer in " + GetName() + " with an invalid byte width (" + std::to_string(nBytes) + ").");
    }
    RequireBinaryRange(*this, sBuffer, *nCurPos, static_cast<unsigned>(nBytes), "read integer");

    int nReturn = -1;
    if(nBytes == 4){
        std::memcpy(&ByteBlock4.i, &sBuffer[*nCurPos], 4);
        nReturn = ByteBlock4.i;
    }
    else if(nBytes == 2){
        std::memcpy(&ByteBlock2.i, &sBuffer[*nCurPos], 2);
        nReturn = ByteBlock2.i;
    }
    else if(nBytes == 1){
        nReturn = static_cast<int>(static_cast<signed char>(sBuffer[*nCurPos]));
    }
    else{
        return -1;
    }

    MarkBytes(*nCurPos, nBytes, nMarking);
    *nCurPos += nBytes;
    return nReturn;
}

float BinaryFile::ReadFloat(unsigned int * nCurPos, int nMarking, int nBytes){
    if(nCurPos == nullptr) return -1.0f;
    // nBytes is used for the range probe, while the read consumes one
    // 32-bit float. Require at least four bytes.
    unsigned nRange = static_cast<unsigned>(nBytes > 4 ? nBytes : 4);
    RequireBinaryRange(*this, sBuffer, *nCurPos, nRange, "read float");

    std::memcpy(&ByteBlock4.f, &sBuffer[*nCurPos], 4);
    MarkBytes(*nCurPos, 4, nMarking);
    *nCurPos += 4;
    return ByteBlock4.f;
}

Vector BinaryFile::ReadVector(unsigned int * nCurPos, int nMarking, int nBytes){
    if(nCurPos == nullptr) return Vector(-1.0, -1.0, -1.0);
    // nBytes is used for the range probe, while the read consumes three
    // 32-bit floats. Clamp the probe to at least twelve bytes.
    if(nBytes > 0 && static_cast<unsigned>(nBytes) > std::numeric_limits<unsigned>::max() / 3u){
        throw mdlexception("Attempting to read a vector in " + GetName() + " with an oversized byte probe.");
    }
    unsigned nProbeBytes = nBytes > 0 ? static_cast<unsigned>(nBytes) * 3u : 0u;
    if(nProbeBytes < 12u) nProbeBytes = 12u;
    RequireBinaryRange(*this, sBuffer, *nCurPos, nProbeBytes, "read vector");

    double Coords[3];
    for(int m = 0; m < 3; m++){
        std::memcpy(&ByteBlock4.f, &sBuffer[*nCurPos + m * 4], 4);
        Coords[m] = ByteBlock4.f;
        MarkBytes(*nCurPos + m * 4, 4, nMarking);
    }
    *nCurPos += 12;
    return Vector(Coords[0], Coords[1], Coords[2]);
}

void BinaryFile::ReadString(std::string & sArray1, unsigned int * nCurPos, int nMarking, int nNumber){
    if(nCurPos == nullptr) return;
    if(nNumber < 0){
        throw mdlexception("Attempting to read a string in " + GetName() + " with a negative length (" + std::to_string(nNumber) + ").");
    }
    RequireBinaryRange(*this, sBuffer, *nCurPos, static_cast<unsigned>(nNumber), "read string");

    if(nNumber == 0){
        sArray1.clear();
        MarkBytes(*nCurPos, 0, nMarking);
        return;
    }

    sArray1.assign(sBuffer.data() + *nCurPos, static_cast<std::size_t>(nNumber));
    MarkBytes(*nCurPos, nNumber, nMarking);
    *nCurPos += nNumber;
}

void BinaryFile::WriteIntToPH(int nInt, int nPH, unsigned int & nContainer){
    unsigned nOffset = static_cast<unsigned>(nPH);
    RequireBinaryRange(*this, sBuffer, nOffset, 4, "write integer placeholder");
    ByteBlock4.i = nInt;
    std::memcpy(&sBuffer[nOffset], ByteBlock4.bytes, 4);
    nContainer = static_cast<unsigned int>(nInt);
}

void BinaryFile::WriteAtOffset4(int nInt, unsigned int nOffset){
    RequireBinaryRange(*this, sBuffer, nOffset, 4, "write integer");
    ByteBlock4.i = nInt;
    std::memcpy(&sBuffer[nOffset], ByteBlock4.bytes, 4);
}

void BinaryFile::WriteAtOffset2(short nShort, unsigned int nOffset){
    RequireBinaryRange(*this, sBuffer, nOffset, 2, "write short");
    ByteBlock2.i = nShort;
    std::memcpy(&sBuffer[nOffset], ByteBlock2.bytes, 2);
}

void BinaryFile::WriteAtOffset1(char nChar, unsigned int nOffset){
    RequireBinaryRange(*this, sBuffer, nOffset, 1, "write byte");
    sBuffer[nOffset] = nChar;
}

void BinaryFile::WriteAtOffset(float fFloat, unsigned int nOffset){
    RequireBinaryRange(*this, sBuffer, nOffset, 4, "write float");
    ByteBlock4.f = fFloat;
    std::memcpy(&sBuffer[nOffset], ByteBlock4.bytes, 4);
}

void BinaryFile::WriteAtOffset(const std::string & sString, unsigned int nOffset){
    if(sString.length() > std::numeric_limits<unsigned>::max()){
        throw mdlexception("Attempting to write an oversized string in " + GetName() + ".");
    }
    RequireBinaryRange(*this, sBuffer, nOffset, static_cast<unsigned>(sString.length()), "write string");
    if(!sString.empty()) std::memcpy(&sBuffer[nOffset], sString.data(), sString.length());
}

void BinaryFile::WriteInt(int nInt, int nKnown, int nBytes){
    if(nBytes != 1 && nBytes != 2 && nBytes != 4 && nBytes != 8){
        Error("Cannot convert an integer to anything but 1, 2, 4 and 8 byte representations!");
        return;
    }

    unsigned char bytes[8] = {};
    if(nBytes == 1){
        const std::uint8_t value = static_cast<std::uint8_t>(nInt);
        std::memcpy(bytes, &value, sizeof(value));
    }
    else if(nBytes == 2){
        const std::uint16_t value = static_cast<std::uint16_t>(nInt);
        std::memcpy(bytes, &value, sizeof(value));
    }
    else if(nBytes == 4){
        const std::uint32_t value = static_cast<std::uint32_t>(nInt);
        std::memcpy(bytes, &value, sizeof(value));
    }
    else if(nBytes == 8){
        const std::uint64_t value = static_cast<std::uint64_t>(nInt);
        std::memcpy(bytes, &value, sizeof(value));
    }

    WriteBytes(bytes, static_cast<unsigned>(nBytes), nKnown, "write integer");
}

void BinaryFile::WriteFloat(float fFloat, int nKnown, int nBytes){
    (void)nBytes;
    unsigned char bytes[4] = {};
    std::memcpy(bytes, &fFloat, sizeof(fFloat));
    WriteBytes(bytes, sizeof(bytes), nKnown, "write float");
}

void BinaryFile::WriteString(const std::string & sString, int nKnown){
    if(sString.length() > static_cast<std::size_t>(std::numeric_limits<unsigned>::max())){
        throw mdlexception("Attempting to write an oversized string in " + GetName() + ".");
    }
    WriteBytes(reinterpret_cast<const unsigned char*>(sString.data()), static_cast<unsigned>(sString.length()), nKnown, "write string");
}

void BinaryFile::WriteByte(char cByte, int nKnown){
    WriteBytes(reinterpret_cast<const unsigned char*>(&cByte), 1, nKnown, "write byte");
}

bool TextFile::ReadFloat(double & fNew, std::string * sGet, bool bPrint){
    std::string sCheck;
    sCheck.reserve(32);

    auto IsTerminator = [](char c){
        return c == '#' || c == '\r' || c == '\n' || c == '\0';
    };

    if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
        if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading float at end of line!\n";
        return false;
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] == 0x20){
        nPosition++;
        if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
            if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading float at end of line!\n";
            return false;
        }
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] != 0x20 && !IsTerminator(sBuffer[nPosition])){
        sCheck.push_back(sBuffer[nPosition]);
        nPosition++;
    }
    if(sCheck.empty()) return false;

    //Report
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "TextFile::ReadFloat(): Reading: " << sCheck << ". ";

    if(sGet != nullptr) *sGet = sCheck;

    try{
        std::size_t nParsed = 0;
        fNew = std::stod(sCheck, &nParsed);
        if(nParsed != sCheck.size()){
            throw std::invalid_argument("trailing characters");
        }
    }
    catch(const std::invalid_argument &){
        std::cout << "TextFile::ReadFloat(): There was an error converting the string: " << sCheck << ". \n";
        throw mdlexception("Could not convert '" + sCheck + "' to float.");
    }
    catch(const std::out_of_range &){
        std::cout << "TextFile::ReadFloat(): The float is out of range: " << sCheck << ".\n";
        throw mdlexception("The float " + sCheck + " is out of range.");
    }
    catch(...){
        std::cout << "TextFile::ReadFloat(): An unknown exception occurred while converting: " << sCheck << ". \n";
        throw mdlexception("An unknown exception occurred while converting '" + sCheck + "' to float.");
    }
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "Converted: " << fNew << ".\n";
    return true;
}

bool TextFile::ReadInt(int & nNew, std::string * sGet, bool bPrint){
    std::string sCheck;
    sCheck.reserve(32);

    auto IsTerminator = [](char c){
        return c == '#' || c == '\r' || c == '\n' || c == '\0';
    };

    if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
        if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading int at end of line!\n";
        return false;
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] == 0x20){
        nPosition++;
        if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
            if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading int at end of line!\n";
            return false;
        }
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] != 0x20 && !IsTerminator(sBuffer[nPosition])){
        sCheck.push_back(sBuffer[nPosition]);
        nPosition++;
    }
    if(sCheck.empty()) return false;

    //Report
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "TextFile::ReadInt(): Reading: " << sCheck << ". ";

    if(sGet != nullptr) *sGet = sCheck;

    try{
        std::size_t nParsed = 0;
        nNew = std::stoi(sCheck, &nParsed);
        if(nParsed != sCheck.size()){
            throw std::invalid_argument("trailing characters");
        }
    }
    catch(const std::invalid_argument &){
        std::cout << "TextFile::ReadInt(): There was an error converting the string: " << sCheck << ". Printing 0xFFFFFFFF. \n";
        throw mdlexception("Could not convert '" + sCheck + "' to integer.");
    }
    catch(const std::out_of_range &){
        std::cout << "TextFile::ReadInt(): The integer is out of range: " << sCheck << ".\n";
        throw mdlexception("The integer " + sCheck + " is out of range.");
    }
    catch(...){
        std::cout << "TextFile::ReadInt(): An unknown exception occurred while converting: " << sCheck << ". Printing 0xFFFFFFFF. \n";
        throw mdlexception("An unknown exception occurred while converting '" + sCheck + "' to integer.");
    }
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "Converted: " << nNew << ".\n";
    return true;
}

bool TextFile::ReadUInt(unsigned int & nNew, std::string * sGet, bool bPrint){
    std::string sCheck;
    sCheck.reserve(32);

    auto IsTerminator = [](char c){
        return c == '#' || c == '\r' || c == '\n' || c == '\0';
    };

    if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
        if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading int at end of line!\n";
        return false;
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] == 0x20){
        nPosition++;
        if(nPosition >= sBuffer.size() || IsTerminator(sBuffer[nPosition])){
            if(bPrint || DEBUG_LEVEL > 5) std::cout << "Reading int at end of line!\n";
            return false;
        }
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] != 0x20 && !IsTerminator(sBuffer[nPosition])){
        sCheck.push_back(sBuffer[nPosition]);
        nPosition++;
    }
    if(sCheck.empty()) return false;

    //Report
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "TextFile::ReadInt(): Reading: " << sCheck << ". ";

    if(sGet != nullptr) *sGet = sCheck;

    try{
        unsigned int nParsed = 0;
        nNew = stou(sCheck, &nParsed);
        if(nParsed != sCheck.size()){
            throw std::invalid_argument("trailing characters");
        }
    }
    catch(const std::invalid_argument &){
        std::cout << "TextFile::ReadInt(): There was an error converting the string: " << sCheck << ". Printing 0xFFFFFFFF. \n";
        throw mdlexception("Could not convert '" + sCheck + "' to integer.");
    }
    catch(const std::out_of_range &){
        std::cout << "TextFile::ReadInt(): The integer is out of range: " << sCheck << ".\n";
        throw mdlexception("The integer " + sCheck + " is out of range.");
    }
    catch(...){
        std::cout << "TextFile::ReadInt(): An unknown exception occurred while converting: " << sCheck << ". Printing 0xFFFFFFFF. \n";
        throw mdlexception("An unknown exception occurred while converting '" + sCheck + "' to integer.");
    }
    if(bPrint || DEBUG_LEVEL > 5) std::cout << "Converted: " << nNew << ".\n";
    return true;
}

void TextFile::SkipLine(){
    if(nPosition >= sBuffer.size()){
        nPosition = static_cast<unsigned int>(sBuffer.size());
        return;
    }

    while(nPosition < sBuffer.size() && sBuffer[nPosition] != 0x0D && sBuffer[nPosition] != 0x0A){
        nPosition++;
    }

    if(nPosition >= sBuffer.size()) return;

    if(sBuffer[nPosition] == 0x0A){
        nPosition += 1;
        nLine++;
        return;
    }
    else if(sBuffer[nPosition] == 0x0D && nPosition + 1 < sBuffer.size() && sBuffer[nPosition + 1] == 0x0A){
        nPosition += 2;
        nLine++;
        return;
    }
    else if(sBuffer[nPosition] == 0x0D){
        nPosition += 1;
        nLine++;
        return;
    }
}


bool TextFile::EmptyRow(){
    std::size_t n = nPosition; //Do not use the iterator
    while( n < sBuffer.size() &&
           sBuffer[n] != 0x0A &&
           sBuffer[n] != 0x0D &&
           sBuffer[n] != 0x00 &&
           sBuffer[n] != '#' )
    {
        if(sBuffer[n] != 0x20) return false;
        n++;
    }
    return true;
}

bool TextFile::ReadUntilText(std::string & sHandle, bool bToLowercase){
    sHandle.clear(); //Make sure the handle is cleared
    sHandle.reserve(32);
    while(nPosition < sBuffer.size() &&
          sBuffer[nPosition] != '#' &&
          sBuffer[nPosition] != 0x0A &&
          sBuffer[nPosition] != 0x0D &&
          sBuffer[nPosition] != 0x00 )
    {
        if(sBuffer[nPosition] == 0x20){
            //Skip space
            nPosition++;
        }
        else{
            //Now it gets interesting - we may actually have relevant text now
            do{
                sHandle.push_back(sBuffer[nPosition]);
                nPosition++;
            }
            while(nPosition < sBuffer.size() &&
                  sBuffer[nPosition] != 0x20 &&
                  sBuffer[nPosition] != '#' &&
                  sBuffer[nPosition] != 0x0D &&
                  sBuffer[nPosition] != 0x0A &&
                  sBuffer[nPosition] != 0x00);

            //convert to lowercase
            if(bToLowercase) ToLowerInPlace(sHandle);

            //Go back and tell them you've found something
            return true;
        }
    }
    //Go back and tell them you're done
    return false;
}

void IniFile::ReadIni(std::wstring & sIni){
    //std::ifstream fIni (sIni.c_str(), std::ios::binary);
    HANDLE fIni = bead_CreateReadFile(sIni); //CreateFileW(sIni.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    //if(!fIni.is_open()){
    if(fIni == INVALID_HANDLE_VALUE){
        throw mdlexception("Error opening .ini file.");
    }

    //fIni.seekg(0, std::ios::end);
    //std::streampos length = fIni.tellg();
    //fIni.seekg(0, std::ios::beg);
    //LARGE_INTEGER lnSize;
    //if(!GetFileSizeEx(fIni, &lnSize)){
    //    throw mdlexception("Error: .ini file empty .");
    //}
    //long unsigned length = lnSize.QuadPart;

    std::vector<char> & sBufferRef = CreateBuffer(bead_GetFileLength(fIni));

    //fIni.read(&sBufferRef[0], length);
    if(!bead_ReadFile(fIni, sBufferRef)){
        CloseHandle(fIni);
        throw mdlexception("Error reading .ini file.");
    }

    //fIni.close();
    CloseHandle(fIni);

    SetFilePath(sIni);

    std::string sID;
    int nConvert = 0;
    unsigned uConvert = 0;
    double fConvert = 0.0;

    while(nPosition < sBuffer.size()){
        if(EmptyRow()){
            SkipLine();
        }
        else{
            ReadUntilText(sID, true);
            for(IniOption & option : Options){
                std::string sToken = option.sToken;
                ToLowerInPlace(sToken);

                if(sID != sToken) continue;

                switch(option.nType){
                    case DT_bool: if(ReadInt(nConvert)) *((bool*) option.lpVariable) = (nConvert == 0 ? false : true);
                    break;
                    case DT_int: if(ReadInt(nConvert)) *((int*) option.lpVariable) = nConvert;
                    break;
                    case DT_uint: if(ReadUInt(uConvert)) *((unsigned*) option.lpVariable) = uConvert;
                    break;
                    case DT_float: if(ReadFloat(fConvert)) *((double*) option.lpVariable) = fConvert;
                    break;
                    case DT_string: if(ReadUntilText(sID, false)) *((std::string*) option.lpVariable) = sID;
                    break;
                }
            }
        }
    }
}

void IniFile::WriteIni(std::wstring & sIni){
    //std::ofstream fIni (sIni, std::ios::binary);
    //if(!fIni.is_open()){
    //    throw mdlexception("Error opening .ini file.");
    //}
    std::stringstream fIni;

    for(IniOption & option : Options){
        fIni << option.sToken << " ";

        switch(option.nType){
            case DT_bool: fIni << (*((bool*) option.lpVariable) ? 1 : 0);
            break;
            case DT_int: fIni << *((int*) option.lpVariable);
            break;
            case DT_uint: fIni << *((unsigned*) option.lpVariable);
            break;
            case DT_float: fIni << PrepareFloat(*((double*) option.lpVariable), true);
            break;
            case DT_string: fIni << *((std::string*) option.lpVariable);
            break;
        }

        if(&option != &Options.back()) fIni << "\r\n";
    }

    HANDLE hIni = bead_CreateWriteFile(sIni); //CreateFileW(sIni.c_str(), GENERIC_WRITE, 0x00, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if(hIni == INVALID_HANDLE_VALUE){
        throw mdlexception("Error opening .ini file.");
    }

    if(!bead_WriteFile(hIni, fIni.str())){
        CloseHandle(hIni);
        throw mdlexception("Error writing .ini file.");
    }

    CloseHandle(hIni);
}
