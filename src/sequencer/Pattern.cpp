#include "Pattern.h"
#include <algorithm>

namespace wako::seq {

std::vector<int> Pattern::activeAtStep(int step) const {
    std::vector<int> result;
    result.reserve(MAX_PADS);
    for (int p = 0; p < MAX_PADS; ++p)
        if (grid[p][step]) result.push_back(p);
    return result;
}

Pattern& Pattern::clearAll() {
    for (auto& row : grid) row.fill(false);
    currentStep = 0;
    return *this;
}

Pattern& Pattern::clearPad(int pad) {
    if (pad >= 0 && pad < MAX_PADS) grid[pad].fill(false);
    return *this;
}

int Pattern::advance() {
    int played = currentStep;
    currentStep = (currentStep + 1) % patternLength;
    return played;
}

} // namespace wako::seq
