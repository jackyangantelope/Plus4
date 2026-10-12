
#pragma once
#include <cstdint>

class Plus4Pace043 {
 public:
    static constexpr uint32_t periodUs = 20000;
    static constexpr uint32_t jitterUs = 200;
    static constexpr uint32_t maxDebtUs = periodUs;
    explicit Plus4Pace043(uint32_t now) : deadline_(now + periodUs) {}
    uint32_t remaining(uint32_t now) const {
        const int32_t delta = static_cast<int32_t>(deadline_ - now);
        return delta > 0 ? static_cast<uint32_t>(delta) : 0;
    }
    uint32_t overdue(uint32_t now) const {
        const int32_t delta = static_cast<int32_t>(now - deadline_);
        return delta > 0 ? static_cast<uint32_t>(delta) : 0;
    }
    bool finish(uint32_t now) {
        const bool rebase = overdue(now) > maxDebtUs;
        deadline_ = rebase ? now + periodUs : deadline_ + periodUs;
        return rebase;
    }
 private:
    uint32_t deadline_;
};
