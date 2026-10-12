
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "ff.h"
#include "hardware/psram.h"
#include "hardware/structs/xip.h"
#include "pico/platform/sections.h"
#include "pico/stdlib.h"

#include "jack_wave1_perf.h"
#include "jack_wave1_runtime.h"
#include "ted.hpp"
#include "plus4_arena.h"
#include "plus4_hot_code.h"
#if PLUS4_PROFILE048
#include "profile048.h"
#endif
#if PLUS4_PROFILE050
#include "profile050_sampler.h"
#endif
#if PLUS4_AUDIO045
#include "plus4_audio045.h"
#include "plus4_log045.h"
#endif
#if PLUS4_SPAN038
#include "video038.h"
#if !PLUS4_VIDEO031
#error "The span experiment requires the accepted video031 decoder"
#endif
#endif
#if PLUS4_PROFILE036
#include "profile036_ticks.h"
#include "hardware/structs/systick.h"
#endif
#if PLUS4_VIDEO031
#include "video031.h"
static_assert(JACK_WAVE1_SCREEN_WIDTH == 320 && JACK_WAVE1_SCREEN_HEIGHT == 240,
              "Video031 requires a 320x240 framebuffer");
#endif

#ifndef PLUS4_RAM_IN_SRAM
#define PLUS4_RAM_IN_SRAM 0
#endif
#ifndef PLUS4_PROFILE036
#define PLUS4_PROFILE036 0
#endif
#ifndef PLUS4_PROFILE048
#define PLUS4_PROFILE048 0
#endif
#ifndef PLUS4_PROFILE050
#define PLUS4_PROFILE050 0
#endif
#if PLUS4_PROFILE048 && PLUS4_PROFILE050
#error "Plus4 profile048 and profile050 both require SysTick"
#endif
#ifndef PLUS4_PROFILE024
#define PLUS4_PROFILE024 0
#endif
#ifndef PLUS4_HOT_CODE_SRAM
#define PLUS4_HOT_CODE_SRAM 0
#endif
#ifndef PLUS4_XIP_PROFILE
#define PLUS4_XIP_PROFILE 0
#endif
#ifndef PLUS4_BUILD_VERSION
#define PLUS4_BUILD_VERSION "0.2.3-frame-probe"
#endif
#ifndef PLUS4EMU_EMBEDDED_TED_DIRECT_MEMORY
#define PLUS4EMU_EMBEDDED_TED_DIRECT_MEMORY 0
#endif
#ifndef PLUS4_CORE_LTO
#define PLUS4_CORE_LTO 0
#endif
#ifndef PLUS4_NO_CALLBACKS
#define PLUS4_NO_CALLBACKS 0
#endif
#ifndef PLUS4_NO_DEBUGGER
#define PLUS4_NO_DEBUGGER 0
#endif
#ifndef PLUS4_MANUAL041
#define PLUS4_MANUAL041 0
#endif

namespace {

constexpr std::size_t kArenaBytes = 1536u * 1024u;
constexpr int kPalInputCycles0 = 354689;
constexpr unsigned kBootFrames = 150;
constexpr unsigned kCropFirstGroup = 25;
constexpr unsigned kCropGroupCount = 80;

alignas(std::max_align_t) static uint8_t __uninitialized_psram("plus4_ted_arena")
    g_arena[kArenaBytes];
#if PLUS4_RAM_IN_SRAM
alignas(std::max_align_t) static uint8_t g_machine_ram[64u * 1024u];
static Plus4Arena g_allocator(g_arena, sizeof(g_arena),
                             g_machine_ram, sizeof(g_machine_ram));
#else
static Plus4Arena g_allocator(g_arena, sizeof(g_arena));
#endif
static uint8_t g_basic[0x4000];
static uint8_t g_kernal[0x4000];
static char g_path[JACK_WAVE1_PATH_MAX];
static JackWave1Perf g_perf;

unsigned clamp5(float value)
{
    int n = static_cast<int>(value * 31.0f + 0.5f);
    if (n < 0) n = 0;
    if (n > 31) n = 31;
    return static_cast<unsigned>(n);
}

class Plus4TED final : public Plus4::TED7360 {
 public:
    void initializePalette()
    {
        for (unsigned i = 0; i < 256; ++i) {
            float y = 0.0f, u = 0.0f, v = 0.0f;
            convertPixelToYUV(static_cast<uint8_t>(i), false, y, u, v);
            const float r = y + 1.140f * v;
            const float g = y - 0.395f * u - 0.581f * v;
            const float b = y + 2.032f * u;
            palette_[i] = jack_wave1_rgb555(clamp5(r), clamp5(g), clamp5(b));
        }
        paletteReady_ = true;
    }

    uint32_t completedFrames() const {
#if PLUS4_VIDEO031
        if (fastVideo_) return fastState_.frames;
#endif
        return completedFrames_;
    }
#if PLUS4_VIDEO031

    void selectFastVideo(bool enabled) { fastVideo_ = enabled; }
#endif
    uint32_t audioSamples() const { return audioSamples_; }
    uint8_t registerValue(unsigned index) const { return tedRegisters[index & 0x1fu]; }
    bool verifyRAMRegion(const uint8_t* base, size_t size) const
    {
        const uintptr_t first = reinterpret_cast<uintptr_t>(base);
        for (unsigned i = 0xfc; i <= 0xff; ++i) {
            const uintptr_t p = reinterpret_cast<uintptr_t>(getMemorySegmentBase(uint8_t(i)));
            if (p < first || p - first > size || size - (p - first) < 0x4000u)
                return false;
        }
        return true;
    }
#if PLUS4_PROFILE024
    void beginVideoTiming(bool enabled) {
        timeVideo_ = enabled; videoUs_ = 0; videoCalls_ = 0;
    }
    uint32_t videoUs() const { return videoUs_; }
    uint32_t videoCalls() const { return videoCalls_; }
#endif
#if PLUS4_PROFILE036
    void beginVideoTiming036(bool enabled) {
        timeVideo036_ = enabled; videoTicks036_ = 0; videoCalls036_ = 0;
    }
    uint64_t videoTicks036() const { return videoTicks036_; }
    uint32_t videoCalls036() const { return videoCalls036_; }
#endif

#if PLUS4_SPAN038
 protected:
    void videoOutputSpans038(const uint8_t* data, size_t bytes,
                            const Plus4::VideoSpan038* spans, size_t count) override
    {
        if (!paletteReady_) return;
        if (!Plus4Video038::decode(fastState_, data, bytes, spans, count,
                                  jack_wave1_framebuffer(), palette_))
            panic("Plus4 invalid span buffer");
    }
#endif
 protected:
#if PLUS4_AUDIO045
    __attribute__((section(".time_critical.plus4_playSample045")))
#endif
    void playSample(int16_t sample) override {
        ++audioSamples_;
#if PLUS4_AUDIO045 && !PLUS4_AUDIO045_SILENT_CONTROL
        if (audioCapturing_) {
#if PLUS4_PROFILE048
            const uint32_t pcmStart048 = Plus4Profile048::beginPcm();
#endif
            plus4_audio045_sample(sample);
#if PLUS4_PROFILE048
            Plus4Profile048::endPcm(pcmStart048);
#endif
        }
#else
        (void)sample;
#endif
    }

    PLUS4_HOT_CODE(video_callback)
    void videoOutputCallback(const uint8_t* data, size_t bytes) override
    {
        if (!paletteReady_) return;
#if PLUS4_VIDEO031
        if (fastVideo_) {
#if PLUS4_PROFILE048
            const uint32_t videoStart048 = Plus4Profile048::beginVideo();
#endif
#if PLUS4_PROFILE036
            const uint32_t start036 = timeVideo036_ ? systick_hw->cvr : 0;
#endif
            Plus4Video031::decode(fastState_, data, bytes, jack_wave1_framebuffer(), palette_);
#if PLUS4_PROFILE048
            Plus4Profile048::endVideo(videoStart048);
#endif
#if PLUS4_PROFILE036
            if (timeVideo036_) {
                videoTicks036_ += Plus4Profile036::elapsed(start036, systick_hw->cvr);
                ++videoCalls036_;
            }
#endif
            return;
        }
#endif
#if PLUS4_PROFILE024
        const uint32_t callbackStart = timeVideo_ ? time_us_32() : 0;
#endif
        const uint8_t* p = data;
        const uint8_t* end = data + bytes;
        uint16_t* frame = jack_wave1_framebuffer();
        while (p < end) {
            const uint8_t flags = *p++;
            const bool fourPixels = (flags & 0x02u) != 0u;
            const unsigned colorBytes = fourPixels ? 4u : 1u;
            if (static_cast<size_t>(end - p) < colorBytes) break;
            const bool sync = (flags & 0x80u) != 0u;
            const bool vblank = (flags & 0x10u) != 0u;

            if (previousVblank_ && !vblank) {
                activeLine_ = 0;
                ++completedFrames_;
            }
            if (!previousSync_ && sync) {
                if (!vblank && activeLine_ < 0xffffu) ++activeLine_;
                groupFromSync_ = 0;
            }

            if (frame != nullptr && !vblank &&
                activeLine_ < JACK_WAVE1_SCREEN_HEIGHT &&
                groupFromSync_ >= kCropFirstGroup &&
                groupFromSync_ < kCropFirstGroup + kCropGroupCount) {
                const unsigned x = (groupFromSync_ - kCropFirstGroup) * 4u;
                uint16_t* out = frame + activeLine_ * JACK_WAVE1_SCREEN_WIDTH + x;
                if (fourPixels) {
                    out[0] = palette_[p[0]];
                    out[1] = palette_[p[1]];
                    out[2] = palette_[p[2]];
                    out[3] = palette_[p[3]];
                } else {
                    const uint16_t color = palette_[p[0]];
                    out[0] = color;
                    out[1] = color;
                    out[2] = color;
                    out[3] = color;
                }
            }

            p += colorBytes;
            ++groupFromSync_;
            previousSync_ = sync;
            previousVblank_ = vblank;
        }
#if PLUS4_PROFILE024
        if (timeVideo_) {
            videoUs_ += time_us_32() - callbackStart;
            ++videoCalls_;
        }
#endif
    }

 private:
#if PLUS4_AUDIO045
    bool audioCapturing_ = false;
 public:
    void beginAudio045() { audioCapturing_ = true; plus4_audio045_begin(); }
 private:
#endif
#if PLUS4_VIDEO031
    Plus4Video031::State fastState_;
    bool fastVideo_ = false;
#endif
    uint16_t palette_[256]{};
    unsigned groupFromSync_ = 0;
    unsigned activeLine_ = 0xffffu;
    uint32_t completedFrames_ = 0;
    uint32_t audioSamples_ = 0;
    bool previousSync_ = false;
    bool previousVblank_ = true;
    bool paletteReady_ = false;
#if PLUS4_PROFILE024
    bool timeVideo_ = false;
    uint32_t videoUs_ = 0, videoCalls_ = 0;
#endif
#if PLUS4_PROFILE036
    bool timeVideo036_ = false;
    uint64_t videoTicks036_ = 0;
    uint32_t videoCalls036_ = 0;
#endif
};

#if PLUS4_XIP_PROFILE
static void __no_inline_not_in_flash_func(runTEDMeasured)(Plus4TED& ted, int cycles,
                                                 uint32_t& hits,
                                                 uint32_t& accesses)
{
    xip_ctrl_hw->ctr_hit = 0;
    xip_ctrl_hw->ctr_acc = 0;
    ted.run(cycles);
    hits = xip_ctrl_hw->ctr_hit;
    accesses = xip_ctrl_hw->ctr_acc;
}
#endif

bool loadExact(const char* path, uint8_t* destination, size_t size)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    if (f_size(&file) != size) {
        f_close(&file);
        return false;
    }
    UINT count = 0;
    const bool ok = f_read(&file, destination, static_cast<UINT>(size), &count) == FR_OK &&
                    count == size;
    f_close(&file);
    return ok;
}

bool loadPRG(Plus4TED& ted, const char* path, uint16_t& loadAddress, size_t& payloadSize)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    uint8_t header[2]{};
    UINT count = 0;
    bool ok = f_read(&file, header, sizeof(header), &count) == FR_OK && count == sizeof(header);
    const size_t fileSize = static_cast<size_t>(f_size(&file));
    loadAddress = static_cast<uint16_t>(header[0] | (uint16_t(header[1]) << 8));
    payloadSize = fileSize >= 2 ? fileSize - 2 : 0;
    if (!ok || payloadSize == 0 || size_t(loadAddress) + payloadSize > 0x10000u) {
        f_close(&file);
        return false;
    }

    uint8_t buffer[512];
    size_t address = loadAddress;
    size_t remaining = payloadSize;
    while (remaining != 0) {
        const UINT request = static_cast<UINT>(remaining > sizeof(buffer) ? sizeof(buffer) : remaining);
        count = 0;
        if (f_read(&file, buffer, request, &count) != FR_OK || count != request) {
            ok = false;
            break;
        }
        for (UINT i = 0; i < request; ++i)
            ted.writeMemoryRaw(0x003F0000u | static_cast<uint32_t>(address++), buffer[i]);
        remaining -= request;
    }
    f_close(&file);
    if (!ok) return false;

    const uint16_t endAddress = static_cast<uint16_t>(loadAddress + payloadSize);
    const uint8_t lo = static_cast<uint8_t>(endAddress);
    const uint8_t hi = static_cast<uint8_t>(endAddress >> 8);
    for (uint16_t p : {uint16_t(0x002D), uint16_t(0x002F), uint16_t(0x0031),
                       uint16_t(0x009D)}) {
        ted.writeMemoryCPU(p, lo);
        ted.writeMemoryCPU(static_cast<uint16_t>(p + 1), hi);
    }
    ted.writeMemoryCPU(0x0033, ted.readMemoryCPU(0x0037));
    ted.writeMemoryCPU(0x0034, ted.readMemoryCPU(0x0038));
    return true;
}

uint16_t findSYSAddress(const Plus4TED& ted, uint16_t loadAddress, size_t payloadSize)
{
    if (payloadSize < 8) return 0;
    const size_t limit = payloadSize < 64 ? payloadSize : 64;
    for (size_t i = 4; i + 1 < limit; ++i) {
        if (ted.readMemoryCPU(static_cast<uint16_t>(loadAddress + i), true) != 0x9eu) continue;
        uint32_t address = 0;
        bool haveDigit = false;
        for (size_t j = i + 1; j < limit; ++j) {
            const uint8_t c = ted.readMemoryCPU(static_cast<uint16_t>(loadAddress + j), true);
            if (c < '0' || c > '9') break;
            haveDigit = true;
            address = address * 10u + uint32_t(c - '0');
            if (address > 0xffffu) return 0;
        }
        if (haveDigit) return static_cast<uint16_t>(address);
    }
    return 0;
}

void queueRUN(Plus4TED& ted)
{
    static constexpr uint8_t command[] = {'R', 'U', 'N', '\r'};
    for (unsigned i = 0; i < sizeof(command); ++i)
        ted.writeMemoryCPU(static_cast<uint16_t>(0x0527u + i), command[i]);
    ted.writeMemoryCPU(0x00EF, static_cast<uint8_t>(sizeof(command)));
}

void setJoystick(Plus4TED& ted, uint16_t buttons)
{
    ted.setKeyState(72, (buttons & JACK_BUTTON_UP) != 0);
    ted.setKeyState(73, (buttons & JACK_BUTTON_DOWN) != 0);
    ted.setKeyState(74, (buttons & JACK_BUTTON_LEFT) != 0);
    ted.setKeyState(75, (buttons & JACK_BUTTON_RIGHT) != 0);
    ted.setKeyState(79, (buttons & (JACK_BUTTON_A | JACK_BUTTON_B | JACK_BUTTON_X)) != 0);
}

int frameCycles(unsigned frame)
{
    return kPalInputCycles0 + static_cast<int>(frame & 1u);
}

struct MemoryFingerprint {
    uint32_t hash;
    uint32_t nonzero;
};

MemoryFingerprint fingerprintRAM(const Plus4TED& ted, uint16_t start, size_t size)
{
    MemoryFingerprint result{2166136261u, 0u};
    for (size_t i = 0; i < size; ++i) {
        const uint8_t value = ted.readMemoryRaw(
            0x003F0000u | static_cast<uint16_t>(start + i));
        result.hash = (result.hash ^ value) * 16777619u;
        if (value != 0) ++result.nonzero;
    }
    return result;
}

MemoryFingerprint fingerprintFramebuffer()
{
    MemoryFingerprint result{2166136261u, 0u};
    const uint16_t* frame = jack_wave1_framebuffer();
    if (frame == nullptr) return result;
    const size_t pixelCount = JACK_WAVE1_SCREEN_WIDTH * JACK_WAVE1_SCREEN_HEIGHT;
    for (size_t i = 0; i < pixelCount; ++i) {
        result.hash = (result.hash ^ frame[i]) * 16777619u;
        if (frame[i] != 0) ++result.nonzero;
    }
    return result;
}
void logVideoDiagnostics(const Plus4TED& ted)
{
    const MemoryFingerprint bitmap = fingerprintRAM(ted, 0xE000u, 0x2000u);
    const MemoryFingerprint luma = fingerprintRAM(ted, 0x0800u, 1000u);
    const MemoryFingerprint chroma = fingerprintRAM(ted, 0x0C00u, 1000u);
    const MemoryFingerprint frame = fingerprintFramebuffer();
    std::printf("PLUS4_VIDEO ff06=%02x ff07=%02x ff12=%02x ff13=%02x ff14=%02x "
                "bitmap=%08lx/%lu luma=%08lx/%lu chroma=%08lx/%lu "
                "frame=%08lx/%lu\n",
                ted.registerValue(0x06), ted.registerValue(0x07),
                ted.registerValue(0x12), ted.registerValue(0x13),
                ted.registerValue(0x14),
                static_cast<unsigned long>(bitmap.hash),
                static_cast<unsigned long>(bitmap.nonzero),
                static_cast<unsigned long>(luma.hash),
                static_cast<unsigned long>(luma.nonzero),
                static_cast<unsigned long>(chroma.hash),
                static_cast<unsigned long>(chroma.nonzero),
                static_cast<unsigned long>(frame.hash),
                static_cast<unsigned long>(frame.nonzero));
}

void runBootFrame(Plus4TED& ted, unsigned frame)
{
    const uint32_t videoFrame = jack_wave1_frame_counter();
    ted.run(frameCycles(frame));
    jack_wave1_usb_service();
    (void)jack_wave1_wait_frame_boundary(videoFrame);
}

}

void* operator new[](std::size_t size)
{
    void* result = g_allocator.allocate(size);
    if (result == nullptr) panic("Plus/4 TED arena exhausted or split allocation");
    return result;
}

void operator delete[](void*) noexcept {}
void operator delete[](void*, std::size_t) noexcept {}

#if PLUS4_MANUAL042
#include "manual042_loop.h"
#endif

int main()
{
#if PLUS4_MANUAL041
    if (!jack_wave1_init("PLUS/4 FRUIT JAM")) for (;;) tight_loop_contents();
    std::printf("PLUS4_MODE manual=1 sys_hz=252000000 video031=1 cpu_multiplier=1\n");
#else
    if (!jack_wave1_init("PLUS/4")) for (;;) tight_loop_contents();
#endif
    std::printf("PLUS4_BOOT version=" PLUS4_BUILD_VERSION
                " core=plus4emu-ted7360 mem=direct ram="
                "%s profile=%u hotcode=%u xip=%u directmem=%u lto=%u nocallbacks=%u nodebugger=%u\n",
                PLUS4_RAM_IN_SRAM ? "sram" : "psram",
                unsigned(PLUS4_PROFILE024), unsigned(PLUS4_HOT_CODE_SRAM),
                unsigned(PLUS4_XIP_PROFILE),
                unsigned(PLUS4EMU_EMBEDDED_TED_DIRECT_MEMORY),
                unsigned(PLUS4_CORE_LTO),
                unsigned(PLUS4_NO_CALLBACKS),
                unsigned(PLUS4_NO_DEBUGGER));
    if (!psram_is_available() || psram_get_size() < 8u * 1024u * 1024u ||
        !jack_wave1_mount_sd()) {
        jack_wave1_show_error("PLUS/4", "PSRAM or SD unavailable");
        for (;;) tight_loop_contents();
    }

    jack_wave1_set_picker_auto_select_single(false);
    if (!jack_wave1_select_file("SELECT PLUS/4 GAME", "/roms/PLUS4", ".prg",
                                g_path, sizeof(g_path))) {
        jack_wave1_show_error("PLUS/4", "No .prg game found");
        for (;;) tight_loop_contents();
    }

    const bool haveSystemROMs =
        loadExact("/BIOS/PLUS4/basic.rom", g_basic, sizeof(g_basic)) &&
        loadExact("/BIOS/PLUS4/kernal.rom", g_kernal, sizeof(g_kernal));

    alignas(Plus4TED) static uint8_t tedStorage[sizeof(Plus4TED)];
    g_allocator.reset();
    auto* ted = new (tedStorage) Plus4TED();
#if PLUS4_MANUAL041
    ted->selectFastVideo(true);
#endif
    ted->initializePalette();
    ted->setRAMSize(64, 0);
#if PLUS4_RAM_IN_SRAM
    const uint8_t* ramBase = g_machine_ram;
#else
    const uint8_t* ramBase = g_arena;
#endif
    const bool ramVerified = ted->verifyRAMRegion(ramBase, 64u * 1024u);
    std::printf("PLUS4_MEMORY ram=%s verified=%u base=%08lx bytes=65536\n",
                PLUS4_RAM_IN_SRAM ? "sram" : "psram", unsigned(ramVerified),
                static_cast<unsigned long>(reinterpret_cast<uintptr_t>(ramBase)));
    if (!ramVerified) panic("Plus/4 machine RAM allocation order changed");
    if (haveSystemROMs) {
        ted->loadROM(0, 0x0000, sizeof(g_basic), g_basic);
        ted->loadROM(0, 0x4000, sizeof(g_kernal), g_kernal);
    }
    ted->reset(true);
#if PLUS4_MANUAL041
    ted->setCPUClockMultiplier(1);
#endif
    jack_wave1_clear(0);
    jack_wave1_wait_buttons_released();

    if (haveSystemROMs) {
        for (unsigned frame = 0; frame < kBootFrames; ++frame) runBootFrame(*ted, frame);
    }

    uint16_t loadAddress = 0;
    size_t payloadSize = 0;
    if (!loadPRG(*ted, g_path, loadAddress, payloadSize)) {
        jack_wave1_show_error("PLUS/4", "Invalid or oversized PRG");
        for (;;) tight_loop_contents();
    }
    const uint16_t sysAddress = findSYSAddress(*ted, loadAddress, payloadSize);
    const char* launchMode = nullptr;
    if (haveSystemROMs && loadAddress == 0x1001u) {
        queueRUN(*ted);
        launchMode = "basic-run";
    } else {

        Plus4::M7501Registers registers;
        ted->getRegisters(registers);
        registers.reg_PC = sysAddress != 0 ? sysAddress : loadAddress;
        registers.reg_SR = 0x24;
        registers.reg_SP = 0xfd;
        ted->setRegisters(registers);
        launchMode = sysAddress != 0 ? "sys-direct" : "load-direct";
    }

    std::printf("PLUS4_START prg=%s load=%04x bytes=%lu bios=%u mode=%s entry=%04x "
                "ram_kb=%lu arena=%lu\n",
                g_path, loadAddress, static_cast<unsigned long>(payloadSize),
                haveSystemROMs ? 1u : 0u, launchMode,
                sysAddress != 0 ? sysAddress : loadAddress,
                static_cast<unsigned long>(ted->getRAMSize()),
                static_cast<unsigned long>(g_allocator.used()));
    if (!haveSystemROMs)
        std::printf("PLUS4_BIOS_MISSING need=/BIOS/PLUS4/basic.rom,/BIOS/PLUS4/kernal.rom "
                    "fallback=%s\n", launchMode);

#if PLUS4_MANUAL042
#if PLUS4_AUDIO045
    plus4_log045_start();
    ted->beginAudio045();
#endif
    plus4Manual042Loop(*ted);
#else
    g_perf.begin();
    unsigned emulatedFrame = 0;
#if !PLUS4_MANUAL041
    uint32_t diagnosticsDeadline = time_us_32() + 1000000u;
#endif
#if PLUS4_PROFILE024
    uint32_t detailFrames = 0, sampledFrames = 0;
    uint64_t inputUs = 0, tedUs = 0, sampledTedUs = 0, videoUs = 0, videoCalls = 0;
#endif
#if PLUS4_XIP_PROFILE
    uint32_t xipFrames = 0;
    uint64_t xipHits = 0, xipAccesses = 0;
#endif
    for (;;) {
        const uint32_t videoFrame = jack_wave1_frame_counter();
        const uint32_t coreStart = time_us_32();
        const uint16_t buttons = jack_wave1_poll_buttons();
        setJoystick(*ted, buttons);
#if PLUS4_PROFILE024
        const bool sampleVideo = (emulatedFrame & 31u) == 0;
        ted->beginVideoTiming(sampleVideo);
        const uint32_t tedStart = time_us_32();
#endif
#if PLUS4_XIP_PROFILE
        uint32_t frameXipHits = 0, frameXipAccesses = 0;
        runTEDMeasured(*ted, frameCycles(emulatedFrame++),
                       frameXipHits, frameXipAccesses);
        ++xipFrames;
        xipHits += frameXipHits;
        xipAccesses += frameXipAccesses;
#else
        ted->run(frameCycles(emulatedFrame++));
#endif

        const uint32_t coreEnd = time_us_32();
        const uint32_t drawEnd = time_us_32();
        const bool waited = jack_wave1_wait_frame_boundary(videoFrame);
        const uint32_t waitEnd = time_us_32();
#if PLUS4_PROFILE024
        ++detailFrames;
        inputUs += tedStart - coreStart;
        tedUs += coreEnd - tedStart;
        if (sampleVideo) {
            ++sampledFrames;
            sampledTedUs += coreEnd - tedStart;
            videoUs += ted->videoUs();
            videoCalls += ted->videoCalls();
        }
#endif
        jack_wave1_usb_service();

        Plus4::M7501Registers registers;
        ted->getRegisters(registers);
        char state[64];
        std::snprintf(state, sizeof(state), "pc=%04x tedf=%lu snd=%lu%s",
                      registers.reg_PC,
                      static_cast<unsigned long>(ted->completedFrames()),
                      static_cast<unsigned long>(ted->audioSamples()),
                      waited ? "" : " late");
        g_perf.sample("PLUS4", coreEnd - coreStart, drawEnd - coreEnd,
                      waitEnd - drawEnd, buttons, state, jack_wave1_perf_stdout);
#if !PLUS4_MANUAL041
        if (static_cast<int32_t>(time_us_32() - diagnosticsDeadline) >= 0) {
            logVideoDiagnostics(*ted);
#if PLUS4_XIP_PROFILE
            const uint64_t xipMisses =
                xipAccesses > xipHits ? xipAccesses - xipHits : 0;
            const uint32_t hitPercentX100 = xipAccesses
                ? static_cast<uint32_t>((xipHits * 10000u) / xipAccesses) : 0;
            std::printf("PLUS4_XIP frames=%lu hit=%llu access=%llu miss=%llu "
                        "hit_pct_x100=%lu hotcode=%u\n",
                        static_cast<unsigned long>(xipFrames),
                        static_cast<unsigned long long>(xipHits),
                        static_cast<unsigned long long>(xipAccesses),
                        static_cast<unsigned long long>(xipMisses),
                        static_cast<unsigned long>(hitPercentX100),
                        unsigned(PLUS4_HOT_CODE_SRAM));
            xipFrames = 0;
            xipHits = xipAccesses = 0;
#endif
#if PLUS4_PROFILE024
            std::printf("PLUS4_DETAIL frames=%lu input_mean_us=%lu ted_mean_us=%lu "
                        "sample_frames=%lu sample_ted_mean_us=%lu video_mean_us=%lu "
                        "video_calls_mean=%lu sampling=1/32\n",
                        static_cast<unsigned long>(detailFrames),
                        static_cast<unsigned long>(detailFrames ? inputUs / detailFrames : 0),
                        static_cast<unsigned long>(detailFrames ? tedUs / detailFrames : 0),
                        static_cast<unsigned long>(sampledFrames),
                        static_cast<unsigned long>(sampledFrames ? sampledTedUs / sampledFrames : 0),
                        static_cast<unsigned long>(sampledFrames ? videoUs / sampledFrames : 0),
                        static_cast<unsigned long>(sampledFrames ? videoCalls / sampledFrames : 0));
            detailFrames = sampledFrames = 0;
            inputUs = tedUs = sampledTedUs = videoUs = videoCalls = 0;
#endif
            diagnosticsDeadline += 1000000u;
        }
#endif
    }
#endif
}
