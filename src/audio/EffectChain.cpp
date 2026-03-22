#include "EffectChain.h"

namespace wako::audio {

void EffectChain::setSampleRate(int sr) {
    eq_.setSampleRate(sr);
    rev_.setSampleRate(sr);
    del_.setSampleRate(sr);
}

void EffectChain::reset() {
    eq_.reset();
    rev_.reset();
    del_.reset();
}

void EffectChain::process(float* stereo, int frames) noexcept {
    sat_.process(stereo, frames);
    eq_.process(stereo, frames);
    rev_.process(stereo, frames);
    del_.process(stereo, frames);
}

} // namespace wako::audio