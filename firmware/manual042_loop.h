
#pragma once
#if PLUS4_AUDIO045
#define PLUS4_PERF_PRINTF plus4_log045_printf
#else
#define PLUS4_PERF_PRINTF std::printf
#endif
#if PLUS4_MANUAL043
#include "pace043.h"
using Plus4ManualPace = Plus4Pace043;
#else
#include "pace042.h"
using Plus4ManualPace = Plus4Pace042;
#endif

[[noreturn]] void plus4Manual042Loop(Plus4TED& ted)
{
    constexpr uint32_t reportUs = 5000000u;
#if PLUS4_PROFILE048
    Plus4Profile048::initialize();
#endif
#if PLUS4_PROFILE050
    plus4_profile050_init();
    plus4_profile050_start();
    unsigned pcWindow050 = 0;
#endif
#if PLUS4_MANUAL043
    std::printf("PLUS4_PACE target_fps=50 period_us=20000 max_debt_us=20000 telemetry_ms=5000 input=overlap\n");
    uint32_t rebases = 0, lastReportUs = 0;
#else
    std::printf("PLUS4_PACE target_fps=50 period_us=20000 telemetry_ms=5000 input=overlap\n");
#endif
    uint32_t windowStart = time_us_32();
    uint32_t videoStart = jack_wave1_frame_counter();
    Plus4ManualPace pace(windowStart);
    uint32_t updates = 0, late = 0, maxLate = 0;
    uint64_t inputUs = 0, tedUs = 0, serviceUs = 0, waitUs = 0;
    unsigned emulatedFrame = 0;
    for (;;) {
#if PLUS4_AUDIO045
        plus4_log045_pump();
#endif
        const uint32_t begin = time_us_32();
        const uint16_t buttons = jack_wave1_poll_buttons();
        setJoystick(ted, buttons);
        const uint32_t inputEnd = time_us_32();
#if PLUS4_PROFILE048
        Plus4Profile048::beginFrame();
#endif
        ted.run(frameCycles(emulatedFrame++));
        const uint32_t coreEnd = time_us_32();
#if PLUS4_PROFILE048
        Plus4Profile048::endFrame(coreEnd - inputEnd);
#endif
#if PLUS4_AUDIO045
        plus4_log045_pump();
#endif
        jack_wave1_usb_service();
        const uint32_t serviceEnd = time_us_32();
        const uint32_t over = pace.overdue(serviceEnd);
        maxLate = over > maxLate ? over : maxLate;
        late += over > Plus4ManualPace::jitterUs;
        uint32_t remaining;
        while ((remaining = pace.remaining(time_us_32())) != 0u) {

            sleep_us(remaining > 1000u ? 1000u : remaining);
            jack_wave1_usb_service();
#if PLUS4_AUDIO045
            plus4_log045_pump();
#endif
        }
        const uint32_t finish = time_us_32();
#if PLUS4_MANUAL043
        rebases += pace.finish(finish);
#else
        pace.finish(finish);
#endif
#if PLUS4_AUDIO045
        plus4_audio045_frame();
#endif
        ++updates;
        inputUs += inputEnd - begin;
        tedUs += coreEnd - inputEnd;
        serviceUs += serviceEnd - coreEnd;
        waitUs += finish - serviceEnd;
        const uint32_t elapsed = finish - windowStart;
        if (elapsed < reportUs) continue;
#if PLUS4_PROFILE050
        plus4_profile050_stop();
#endif
        const uint32_t video = jack_wave1_frame_counter();
        const uint32_t fps100 = static_cast<uint32_t>(uint64_t(updates) * 100000000u / elapsed);
        const uint32_t video100 = static_cast<uint32_t>(uint64_t(video - videoStart) * 100000000u / elapsed);
#if PLUS4_MANUAL043
        const uint32_t reportBegin = time_us_32();
#endif

        PLUS4_PERF_PRINTF("PLUS4_PERF fps=%lu.%02lu video_fps=%lu.%02lu updates=%lu elapsed_us=%lu "
                    "input_us=%lu ted_us=%lu service_us=%lu wait_us=%lu late=%lu max_late_us=%lu "
                    "buttons=%04x usb_reports=%lu"
#if PLUS4_MANUAL043
                    " rebases=%lu report_us=%lu"
#endif
                    "\n",
                    (unsigned long)(fps100 / 100u), (unsigned long)(fps100 % 100u),
                    (unsigned long)(video100 / 100u), (unsigned long)(video100 % 100u),
                    (unsigned long)updates, (unsigned long)elapsed,
                    (unsigned long)(inputUs / updates), (unsigned long)(tedUs / updates),
                    (unsigned long)(serviceUs / updates), (unsigned long)(waitUs / updates),
                    (unsigned long)late, (unsigned long)maxLate, unsigned(buttons),
                    (unsigned long)jack_wave1_usb_report_count()
#if PLUS4_MANUAL043
                    , (unsigned long)rebases, (unsigned long)lastReportUs
#endif
                    );
#if PLUS4_MANUAL043
        lastReportUs = time_us_32() - reportBegin;
        rebases = 0;
#endif
#if PLUS4_AUDIO045
        plus4_audio045_report();
        plus4_audio045_transport_report();
#endif
#if PLUS4_PROFILE048
        Plus4Profile048::report();
#endif
#if PLUS4_PROFILE050
        plus4_profile050_report(pcWindow050++);
        plus4_profile050_start();
#endif
        windowStart = finish;
        videoStart = video;
        updates = late = maxLate = 0;
        inputUs = tedUs = serviceUs = waitUs = 0;
    }
}
#undef PLUS4_PERF_PRINTF
