#ifndef ASCII_COMPAT_H_INCLUDED
#define ASCII_COMPAT_H_INCLUDED

#include <cstddef>
#include <limits>
#include <string>

namespace ascii_compat {
    inline bool IsStrictSignedDecimalInteger(const std::string & value){
        if(value.empty()) return false;

        std::size_t n = 0;
        if(value[n] == '+' || value[n] == '-') n++;
        if(n == value.size()) return false;

        for(; n < value.size(); n++){
            if(value[n] < '0' || value[n] > '9') return false;
        }
        return true;
    }

    inline bool IsCountedAabbHeader(bool bHasFirstValue,
                                    const std::string & sFirstValue,
                                    bool bAtLineEnd){
        return bHasFirstValue && bAtLineEnd && IsStrictSignedDecimalInteger(sFirstValue);
    }

    inline bool InferLegacyAabbRowCount(std::size_t nFaceCount, unsigned int & nRowCount){
        const std::size_t nMaxFaceCount =
            static_cast<std::size_t>(std::numeric_limits<unsigned int>::max() / 2u) + 1u;
        if(nFaceCount > nMaxFaceCount) return false;

        nRowCount = nFaceCount == 0u ? 0u : static_cast<unsigned int>(nFaceCount * 2u - 1u);
        return true;
    }
}

#endif // ASCII_COMPAT_H_INCLUDED
