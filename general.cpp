#include "general.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <tchar.h>
#include <cstddef>
#include <limits>

double PI = 3.141592653589793;

std::string Version::Print(){
    return std::string("v") + std::to_string(nMajor) + "." + std::to_string(nMinor) + "." + std::to_string(nPatch) + (bBeta ? "b BETA" : "");
}


unsigned int FloatBits(float value){
    unsigned int bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float FloatFromBits(unsigned int bits){
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string Timer::GetTime(bool bRestart){
    DWORD nNewTime = GetTickCount();
    DWORD nElapsed = nNewTime - nReferenceTime; // unsigned subtraction handles GetTickCount() rollover
    int nSeconds = static_cast<int>(nElapsed / 1000);
    int nMiliseconds = static_cast<int>(nElapsed % 1000);
    if(bRestart) nReferenceTime = nNewTime;
    return (std::to_string(nSeconds) + "." + (nMiliseconds < 100 ? "0" : "") + (nMiliseconds < 10 ? "0" : "") + std::to_string(nMiliseconds) + "s");
}

mdlexception::mdlexception(const std::string & sNew): sException(sNew){
}

mdlexception::~mdlexception(){
}

const char* mdlexception::what() const throw(){
    return sException.c_str();
}

int pown(int base, int exp){
    int result = 1;
    for(int n = 0; n < exp; n++){
        result *= base;
    }
    /*
    while(exp){
        if(exp & 1) result *= base;
        exp /= 2;
        base *= base;
    }
    */
    return result;
}

float deg(float rad){
    return rad * 180.0 / PI;

}

float rad(float deg){
    return deg * PI / 180.0;
}

double deg(double rad){
    return rad * 180.0 / PI;

}

double rad(double deg){
    return deg * PI / 180.0;
}

char DecToHexDigit(int nDec){
    if(nDec >= 0 && nDec <= 9) return static_cast<char>('0' + nDec);
    if(nDec >= 10 && nDec <= 15) return static_cast<char>('A' + nDec - 10);
    return '0';
}

void AddSignificantZeroes(char * cInt, int nSignificant){
    // The caller must provide a buffer large enough for nSignificant digits plus the terminator.
    if(cInt == nullptr || nSignificant <= 0) return;
    const std::size_t nTarget = static_cast<std::size_t>(nSignificant);
    const std::size_t nLen = std::strlen(cInt);
    if(nLen >= nTarget) return;

    const std::size_t nPad = nTarget - nLen;
    std::memmove(cInt + nPad, cInt, nLen + 1);
    std::memset(cInt, '0', nPad);
}


//Removes final zeros, unless a decimal operator precedes it.
void TruncateDec(TCHAR * tcString){
    if(tcString == nullptr) return;

    size_t nLen = _tcslen(tcString);
    if(nLen == 0) return;

    while(nLen > 0 && tcString[nLen - 1] == TEXT('0')){
        if(nLen >= 2 && _istdigit(tcString[nLen - 2])){
            tcString[nLen - 1] = TEXT('\0');
            nLen--;
        }
        else break;
    }
}

//Removes final zeros, unless a decimal operator precedes it.
std::string TruncateDec(std::string sCopy){
    size_t n = sCopy.find('e');
    std::string sPart2;
    if(n != std::string::npos){
        sPart2 = safesubstr(sCopy, n);
        sCopy = safesubstr(sCopy, 0, n);
    }

    ///Delete string final occurrence
    if(sCopy.find('.') == std::string::npos){
        return sCopy + ".0";
    }
    else while(!sCopy.empty() && sCopy.back() == '0'){
        sCopy.pop_back();
    }
    if(sCopy.empty() || sCopy.back() == '.'){
        sCopy.push_back('0');
    }

    sCopy += sPart2;

    return sCopy;
}

//This replaces chars that display weirdly in font Consolas with spaces and replaces \0 with period.
void PrepareCharForDisplay(char * cChar){
    switch((unsigned char)*cChar){
        case 0x00:
            *cChar = '.';
        break;
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x7F:
        case 0x81:
        case 0x83:
        case 0x88:
        case 0x90:
        case 0x98:
            *cChar = ' ';
        break;
    }
}

void CharsToHex(char * cOutput, const std::vector<char> & cInput, int nOffset, int nNumber){
    if(cOutput == nullptr) return;
    cOutput[0] = '\0';
    if(nOffset < 0 || nNumber <= 0) return;

    const std::size_t nStart = static_cast<std::size_t>(nOffset);
    if(nStart >= cInput.size()) return;

    const int nAvailable = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(nNumber), cInput.size() - nStart));
    for(int n = 0; n < nAvailable; n++){
        const unsigned char cValue = static_cast<unsigned char>(cInput[nStart + static_cast<std::size_t>(n)]);
        const int nDigit1 = cValue / 16;
        const int nDigit2 = cValue - nDigit1 * 16;
        cOutput[n*3 + 0] = DecToHexDigit(nDigit1);
        cOutput[n*3 + 1] = DecToHexDigit(nDigit2);
        cOutput[n*3 + 2] = (n + 1 == nAvailable) ? '\0' : ' ';
    }
}


double RoundDec(double fNumber, int nDecPlaces){
    if(nDecPlaces < 0){
        //Error
        printf("RoundDec() ERROR: number of decimal places indicated as a negative number. \n");
        return fNumber;
    }
    double fFactor = pow(10.0, -1.0 * (double) nDecPlaces);
    double fReturn = fNumber / fFactor;
    fReturn = round(fReturn);
    fReturn = fReturn * fFactor;
    return fReturn;
}

double RoundDec(float fNumber, int nDecPlaces){
    if(nDecPlaces < 0){
        //Error
        printf("RoundDec() ERROR: number of decimal places indicated as a negative number. \n");
        return fNumber;
    }
    double fFactor = pow(10.0, -1.0 * (double) nDecPlaces);
    double fReturn = ((double) fNumber) / fFactor;
    fReturn = round(fReturn);
    fReturn = fReturn * fFactor;
    return fReturn;
}

bool bCursorOnLine(POINT pt, POINT ptLine1, POINT ptLine2, int nOffset){
    int nx = ptLine1.x;
    int nx2 = ptLine2.x;
    int ny = ptLine1.y;
    int ny2 = ptLine2.y;
    if(nx==nx2){
        if(pt.x >= nx-nOffset && pt.x <= nx+nOffset && pt.y >= ny && pt.y <= ny2 && ny2 > ny) return true;
        else if(pt.x >= nx-nOffset && pt.x <= nx+nOffset && pt.y <= ny && pt.y >= ny2 && ny2 < ny) return true;
        else return false;
    }
    else if(ny==ny2){
        if(pt.y >= ny-nOffset && pt.y <= ny+nOffset && pt.x >= nx && pt.x <= nx2 && nx2 > nx) return true;
        else if(pt.y >= ny-nOffset && pt.y <= ny+nOffset && pt.x <= nx && pt.x >= nx2 && nx2 < nx) return true;
        else return false;
    }

    double fx1;
    double fy1;
    double fx2;
    double fy2;
    if(nx < nx2){
        fx1 = (double) nx;
        fx2 = (double) nx2;
        fy1 = (double) ny;
        fy2 = (double) ny2;
    }
    else{
        fx1 = (double) nx2;
        fx2 = (double) nx;
        fy1 = (double) ny2;
        fy2 = (double) ny;
    }
    double k = 1.0 * (fy2 - fy1) / (fx2 - fx1);
    double n = fy1 - k * fx1;
    double k2 = -1.0 * (fx2 - fx1) / (fy2 - fy1);
    double n2a = fy1 - k2 * fx1;
    double n2b = fy2 - k2 * fx2;
    double d = sqrt(pow((fy2 - fy1), 2.0) + pow((fx2 - fx1), 2.0));
    double p = (double) nOffset;
    double r = d * p / (fx2 - fx1);
    double x = (double) pt.x;
    double y = (double) pt.y;
    if(y >= k*x+n-r &&
        y <= k*x+n+r &&
        y >= k2*x+n2a &&
        y <= k2*x+n2b &&
        fy1 < fy2)
    {
        return true;
    }
    else if(y >= k*x+n-r &&
            y <= k*x+n+r &&
            y <= k2*x+n2a &&
            y >= k2*x+n2b &&
            fy1 > fy2)
    {
        return true;
    }
    else return false;
}

std::string safesubstr(const std::string & sParam, unsigned int nStart, unsigned int nLen){
    if(nStart >= sParam.length() || nLen == 0) return std::string();
    return sParam.substr(nStart, nLen);
}

std::wstring safesubstr(const std::wstring & sParam, unsigned int nStart, unsigned int nLen){
    if(nStart >= sParam.length() || nLen == 0) return std::wstring();
    return sParam.substr(nStart, nLen);
}

std::string PrepareFloat(double fFloat, bool bFiniteOnly){
    std::stringstream ssReturn;
    ssReturn.precision(6);
    ssReturn.setf(std::ios::showpoint);
    if(!std::isfinite(fFloat)){
        if(bFiniteOnly) return "0.0";
        else return std::to_string(fFloat);
    }
    ssReturn << RoundDec(fFloat, 8);
    return TruncateDec(ssReturn.str());
}

unsigned int stou(std::string const & str, unsigned int * idx, int base){
    const char * cStart = str.c_str();
    char * cEnd = nullptr;

    if(!str.empty() && str.front() == '-') throw std::out_of_range("stoul");

    int nSavedErrno = errno;
    errno = 0;
    unsigned long nResult = strtoul(cStart, &cEnd, base);
    int nConvertErrno = errno;
    errno = nSavedErrno;

    if(cEnd == cStart){
        throw std::invalid_argument("stoul");
    }

    if(nConvertErrno == ERANGE || nResult > std::numeric_limits<unsigned int>::max()){
        throw std::out_of_range("stoul");
    }

    if(idx != nullptr) *idx = static_cast<unsigned int>(cEnd - cStart);
    return static_cast<unsigned int>(nResult);
}

int Error(std::string sErrorMessage){
    return MessageBox(hFrame, sErrorMessage.c_str(), "Error!", MB_OK | MB_ICONERROR);
}

int WarningCancel(std::string sWarningMessage){
    return MessageBox(hFrame, sWarningMessage.c_str(), "Warning!", MB_OKCANCEL | MB_ICONWARNING);
}

int WarningYesNo(std::string sWarningMessage){
    return MessageBox(hFrame, sWarningMessage.c_str(), "Warning!", MB_YESNO | MB_ICONWARNING);
}

int WarningYesNoCancel(std::string sWarningMessage){
    return MessageBox(hFrame, sWarningMessage.c_str(), "Warning!", MB_YESNOCANCEL | MB_ICONWARNING);
}

int Warning(std::string sWarningMessage){
    return MessageBox(hFrame, sWarningMessage.c_str(), "Warning!", MB_OK | MB_ICONWARNING);
}

int Error(std::wstring sErrorMessage){
    return MessageBoxW(hFrame, sErrorMessage.c_str(), L"Error!", MB_OK | MB_ICONERROR);
}

int WarningCancel(std::wstring sWarningMessage){
    return MessageBoxW(hFrame, sWarningMessage.c_str(), L"Warning!", MB_OKCANCEL | MB_ICONWARNING);
}

int WarningYesNo(std::wstring sWarningMessage){
    return MessageBoxW(hFrame, sWarningMessage.c_str(), L"Warning!", MB_YESNO | MB_ICONWARNING);
}

int WarningYesNoCancel(std::wstring sWarningMessage){
    return MessageBoxW(hFrame, sWarningMessage.c_str(), L"Warning!", MB_YESNOCANCEL | MB_ICONWARNING);
}

int Warning(std::wstring sWarningMessage){
    return MessageBoxW(hFrame, sWarningMessage.c_str(), L"Warning!", MB_OK | MB_ICONWARNING);
}

void ClearStringstream(std::stringstream & ssClearMe){
    ssClearMe.str(std::string());
    ssClearMe.clear();
}

std::string to_ansi(const std::wstring & wString){
    if(wString.empty()) return std::string();

    int nChars = WideCharToMultiByte(CP_ACP, 0, wString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if(nChars <= 0) return std::string(wString.begin(), wString.end());

    std::string sReturn(static_cast<std::size_t>(nChars), '\0');
    int nWritten = WideCharToMultiByte(CP_ACP, 0, wString.c_str(), -1, &sReturn[0], nChars, nullptr, nullptr);
    if(nWritten <= 0) return std::string();
    if(!sReturn.empty() && sReturn.back() == '\0') sReturn.pop_back();
    return sReturn;
}

std::wstring to_wide(const std::string & sString){
    if(sString.empty()) return std::wstring();

    int nChars = MultiByteToWideChar(CP_ACP, 0, sString.c_str(), -1, nullptr, 0);
    if(nChars <= 0) return std::wstring(sString.begin(), sString.end());

    std::wstring wReturn(static_cast<std::size_t>(nChars), L'\0');
    int nWritten = MultiByteToWideChar(CP_ACP, 0, sString.c_str(), -1, &wReturn[0], nChars);
    if(nWritten <= 0) return std::wstring();
    if(!wReturn.empty() && wReturn.back() == L'\0') wReturn.pop_back();
    return wReturn;
}


void PrepareFileDialogBuffer(std::wstring & sBuffer, unsigned int nMaxChars){
    if(nMaxChars == 0) nMaxChars = MAX_PATH;

    // Preserve the current filename, but guarantee that the Win32 common
    // dialog has a correctly-sized writable buffer. lpstrFile is only allowed
    // to contain nMaxChars characters including the terminator; if a caller
    // provides a longer default path, truncate the editable buffer instead of
    // passing an overlong string with a smaller nMaxFile.
    std::size_t nTerminator = sBuffer.find(L'\0');
    if(nTerminator != std::wstring::npos){
        sBuffer.resize(nTerminator);
    }
    if(sBuffer.size() >= nMaxChars){
        sBuffer.resize(nMaxChars - 1);
    }
    sBuffer.resize(nMaxChars, L'\0');
}

void TrimFileDialogBuffer(std::wstring & sBuffer){
    std::size_t nTerminator = sBuffer.find(L'\0');
    if(nTerminator != std::wstring::npos){
        sBuffer.resize(nTerminator);
    }
}


std::wstring ParentPath(const std::wstring & sPath){
    std::wstring sClean = sPath;
    const std::size_t nTerminator = sClean.find(L'\0');
    if(nTerminator != std::wstring::npos){
        sClean.resize(nTerminator);
    }
    if(sClean.empty()) return std::wstring();

    while(!sClean.empty() && (sClean.back() == L'\\' || sClean.back() == L'/')){
        if(sClean.size() == 3 && sClean[1] == L':') break;
        if(sClean.size() == 1) break;
        sClean.pop_back();
    }

    const std::size_t nSlash = sClean.find_last_of(L"\\/");
    if(nSlash == std::wstring::npos) return std::wstring();
    if(nSlash == 0) return sClean.substr(0, 1);
    if(nSlash == 2 && sClean.size() >= 3 && sClean[1] == L':') return sClean.substr(0, 3);
    return sClean.substr(0, nSlash);
}

std::wstring JoinPath(const std::wstring & sDirectory, const std::wstring & sFilename){
    if(sDirectory.empty()) return sFilename;
    if(sFilename.empty()) return sDirectory;
    if(sDirectory.back() == L'\\' || sDirectory.back() == L'/') return sDirectory + sFilename;
    return sDirectory + L"\\" + sFilename;
}

void ToLowerInPlace(std::string & s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){
        return static_cast<char>(std::tolower(c));
    });
}

bool StringEqual(const std::string & s1, const std::string & s2, bool bCaseSensitive){
    if(bCaseSensitive) return s1 == s2;
    if(s1.size() != s2.size()) return false;

    for(std::size_t i = 0; i < s1.size(); i++){
        unsigned char c1 = static_cast<unsigned char>(s1[i]);
        unsigned char c2 = static_cast<unsigned char>(s2[i]);
        if(std::tolower(c1) != std::tolower(c2)) return false;
    }
    return true;
}
