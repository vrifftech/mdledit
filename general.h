#ifndef GENERAL_H_INCLUDED
#define GENERAL_H_INCLUDED

#include "win32_compat.h"
#include <math.h>
#include <array> //for std::array
#include <iostream> //for std::cout
#include <vector> //for std::vector
#include <memory> //for std::unique_ptr
#include <string> //for std::string
#include <sstream> //for std::stringstream
#include <exception> //for std::exception

#include "bead_winctrl.h"
#include "resource.h"
#include <cmath>
#include <cctype>

#define DEBUG_LEVEL 0

extern HWND hFrame;
extern HWND hStatusBar;
extern HWND hTree;
extern HWND hTabs;
extern HWND hProgress;
extern HWND hDisplayEdit;
extern double PI;
extern bool bShowHex;
char DecToHexDigit(int nDec);
void CharsToHex(char * cOutput, const std::vector<char> & cInput, int nOffset, int nNumber);
void AddSignificantZeroes(char * cInt, int nSignificant);
void TruncateDec(TCHAR * tcString);
std::string TruncateDec(std::string sCopy);
void PrepareCharForDisplay(char * cChar);
int pown(int base, int exp);
float deg(float rad);
float rad(float deg);
double deg(double rad);
double rad(double deg);
double RoundDec(double fNumber, int nDecPlaces);
double RoundDec(float fNumber, int nDecPlaces);
unsigned int FloatBits(float value);
float FloatFromBits(unsigned int bits);
bool bCursorOnLine(POINT pt, POINT ptLine1, POINT ptLine2, int nOffset);
std::string PrepareFloat(double fFloat, bool bFiniteOnly = true);
unsigned int stou(std::string const & str, unsigned int * idx = nullptr, int base = 10);
std::string safesubstr(const std::string & sParam, unsigned int nStart, unsigned int nLen = ~0u);
std::wstring safesubstr(const std::wstring & sParam, unsigned int nStart, unsigned int nLen = ~0u);
int Error(std::string sErrorMessage);
int WarningCancel(std::string sWarningMessage);
int WarningYesNo(std::string sWarningMessage);
int WarningYesNoCancel(std::string sWarningMessage);
int Warning(std::string sWarningMessage);
int Error(std::wstring sErrorMessage);
int WarningCancel(std::wstring sWarningMessage);
int WarningYesNo(std::wstring sWarningMessage);
int WarningYesNoCancel(std::wstring sWarningMessage);
int Warning(std::wstring sWarningMessage);
void ClearStringstream(std::stringstream & ssClearMe);
std::string to_ansi(const std::wstring & wString);
std::wstring to_wide(const std::string & sString);
void ToLowerInPlace(std::string & s);
void PrepareFileDialogBuffer(std::wstring & sBuffer, unsigned int nMaxChars = MAX_PATH);
void TrimFileDialogBuffer(std::wstring & sBuffer);
std::wstring ParentPath(const std::wstring & sPath);
std::wstring JoinPath(const std::wstring & sDirectory, const std::wstring & sFilename);
bool StringEqual(const std::string & s1, const std::string & s2, bool bCaseSensitive = false);

struct MenuLineAdder{
    HMENU hMenu;
    int nIndex;
};

struct Version{
    unsigned int nMajor;
    unsigned int nMinor;
    unsigned int nPatch;
    bool bBeta = false;
    Version(unsigned int n1, unsigned int n2, unsigned int n3, bool bB): nMajor(n1), nMinor(n2), nPatch(n3), bBeta(bB) {}
    std::string Print();
};

class Timer{
    DWORD nReferenceTime = 0;
  public:
    Timer(){
        nReferenceTime = GetTickCount();
    }
    std::string GetTime(bool bRestart = false);
};

class mdlexception: public std::exception {
    std::string sException;
  public:
    mdlexception(const std::string & sNew);
    virtual ~mdlexception();
    virtual const char* what() const throw();
};

#endif // GENERAL_H_INCLUDED
