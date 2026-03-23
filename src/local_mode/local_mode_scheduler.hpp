#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "local_mode/local_mode_types.hpp"

namespace app::local_mode {

struct DueRun {
    uint8_t valve_index = 0U;
    uint8_t program_index = 0U;
    uint16_t duration_sec = 0U;
};

class LocalModeScheduler {
public:
    LocalModeScheduler() = default;

    void reset();

    std::vector<DueRun> collect_due_runs(const LocalModeSettings& settings,
                                         uint32_t now_unix,
                                         uint32_t valve_count,
                                         const std::array<bool, kMaxValves>& manual_running);

private:
    std::array<std::array<uint32_t, kMaxProgramsPerValve>, kMaxValves> last_triggered_minute_key_{};
    bool initialized_ = false;

    static uint32_t make_minute_key(uint32_t now_unix);
};

} // namespace app::local_mode
