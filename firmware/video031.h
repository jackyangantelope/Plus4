
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Video031 packed pixel stores require little-endian byte order
#endif

namespace Plus4Video031 {
struct State {
    unsigned group = 0, line = 0xffffu;
    uint32_t frames = 0;
    uint8_t previous = 0x10;
};

inline void decode(State& state, const uint8_t* data, size_t bytes,
                   uint16_t* frame, const uint16_t* palette) {
    const uint8_t* p = data;
    const uint8_t* end = p + bytes;
    unsigned group = state.group, line = state.line;
    uint32_t frames = state.frames;
    uint8_t previous = state.previous;

    uint16_t* row = frame && !(previous & 0x10) && line < 240
                  ? frame + line * 320 : nullptr;
    while (p < end) {
        const uint8_t flags = *p++;
        const unsigned colors = (flags & 2) ? 4 : 1;
        if (size_t(end - p) < colors) break;
        const uint8_t current = flags & 0x90;
        if (current != previous) {
            if ((previous & 0x10) && !(current & 0x10)) {
                line = 0;
                ++frames;
            }
            if (!(previous & 0x80) && (current & 0x80)) {
                if (!(current & 0x10) && line < 0xffffu) ++line;
                group = 0;
            }
            row = frame && !(current & 0x10) && line < 240
                ? frame + line * 320 : nullptr;
            previous = current;
        }
        // Unsigned underflow excludes the left border.
        const unsigned column = group - 25u;
        if (row && column < 80u) {
            uint16_t* out = row + column * 4;
            uint32_t pair0, pair1;
            if (colors == 4) {
                pair0 = uint32_t(palette[p[0]]) | (uint32_t(palette[p[1]]) << 16);
                pair1 = uint32_t(palette[p[2]]) | (uint32_t(palette[p[3]]) << 16);
            } else {
                const uint32_t color = palette[p[0]];
                pair0 = pair1 = color | (color << 16);
            }

            std::memcpy(out, &pair0, 4);
            std::memcpy(out + 2, &pair1, 4);
        }
        p += colors;
        ++group;
    }
    state.group = group; state.line = line;
    state.frames = frames; state.previous = previous;
}
}
