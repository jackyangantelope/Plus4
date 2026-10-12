
#pragma once
#include <cstdint>

namespace plus4_audio {

inline int16_t fir046_coefficients[32] = {
    -7,-18,-28,-38,-44,-43,-32,-8,30,79,131,176,199,187,129,22,
    -128,-303,-476,-609,-664,-603,-398,-37,473,1103,1805,2517,
    3169,3693,4032,4150
};

class FirDecimate5 {
    int16_t history_[128]{};
    uint32_t position_ = 0, phase_ = 0;
    int32_t dc_ = 0;
public:
    bool push(int16_t sample, int16_t &out) {
        history_[position_] = history_[position_ + 64] = sample;
        position_ = (position_ + 1) & 63u;
        if (++phase_ != 5) return false;
        phase_ = 0;
        const int16_t *h = history_ + position_;

        int32_t sum = int32_t(fir046_coefficients[31]) * h[32];
        for (unsigned i = 0; i < 31; ++i)
            sum += int32_t(fir046_coefficients[i]) * (int32_t(h[63-i]) + h[1+i]);
        const int32_t filtered = sum / 32768;
        dc_ += (filtered * 256 - dc_) / 256;

        const int32_t quiet = (filtered - dc_ / 256) / 4;
        out = static_cast<int16_t>(quiet);
        return true;
    }
};
}
