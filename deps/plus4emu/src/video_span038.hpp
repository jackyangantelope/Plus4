#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Plus4 {
// Offsets/lengths describe the ORIGINAL encoded-buffer coordinate system.
// Only the first two bytes of a span are stored; its remaining groups are holes.
// Gaps between spans contain ordinary 2/5-byte encoded groups.
struct VideoSpan038 { uint16_t offset, groups; };

inline bool expandVideoSpans038(const uint8_t* data, size_t bytes,
                               const VideoSpan038* spans, size_t count,
                               uint8_t* expanded) {
    size_t cursor = 0;
    for (size_t i = 0; i < count; ++i) {
        const size_t offset = spans[i].offset, groups = spans[i].groups;
        if (offset < cursor || offset > bytes || bytes - offset < 2 ||
            !groups || groups > (bytes - offset) / 2 || (data[offset] & 2))
            return false;
        if (offset > cursor)
            std::memcpy(expanded + cursor, data + cursor, offset - cursor);
        for (size_t g = 0; g < groups; ++g) {
            expanded[offset + g * 2] = data[offset];
            expanded[offset + g * 2 + 1] = data[offset + 1];
        }
        cursor = offset + groups * 2;
    }
    if (cursor < bytes)
        std::memcpy(expanded + cursor, data + cursor, bytes - cursor);
    return true;
}
}
