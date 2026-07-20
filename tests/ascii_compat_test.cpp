#include "../ascii_compat.h"

#include <cassert>

int main(){
    using ascii_compat::IsCountedAabbHeader;

    assert(IsCountedAabbHeader(true, "0", true));
    assert(IsCountedAabbHeader(true, "1509", true));
    assert(IsCountedAabbHeader(true, "+1509", true));

    // Legacy NWMax puts the first AABB row on the keyword line.
    assert(!IsCountedAabbHeader(true, "-40.3664", false));
    assert(!IsCountedAabbHeader(true, "-40", false));

    // A floating-point token is never a row count, even by itself.
    assert(!IsCountedAabbHeader(true, "-40.3664", true));
    assert(!IsCountedAabbHeader(true, "1509x", true));
    assert(!IsCountedAabbHeader(false, "", true));

    unsigned int nRows = 0;
    assert(ascii_compat::InferLegacyAabbRowCount(0, nRows) && nRows == 0);
    assert(ascii_compat::InferLegacyAabbRowCount(288, nRows) && nRows == 575);
    assert(ascii_compat::InferLegacyAabbRowCount(755, nRows) && nRows == 1509);

    const std::size_t nTooManyFaces =
        static_cast<std::size_t>(std::numeric_limits<unsigned int>::max() / 2u) + 2u;
    assert(!ascii_compat::InferLegacyAabbRowCount(nTooManyFaces, nRows));
}
