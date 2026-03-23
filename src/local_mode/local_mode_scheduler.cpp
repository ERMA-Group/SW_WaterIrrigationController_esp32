#include "local_mode/local_mode_scheduler.hpp"

#include <ctime>

namespace app::local_mode {

namespace {

constexpr uint32_t kInvalidMinuteKey = 0xFFFFFFFFU;

} // namespace

void LocalModeScheduler::reset()
{
    for (auto& valve_entries : last_triggered_minute_key_)
    {
        valve_entries.fill(kInvalidMinuteKey);
    }
    initialized_ = true;
}

uint32_t LocalModeScheduler::make_minute_key(uint32_t now_unix)
{
    std::time_t now = static_cast<std::time_t>(now_unix);
    std::tm local_time = {};
    localtime_r(&now, &local_time);

    const uint32_t year = static_cast<uint32_t>(local_time.tm_year + 1900);
    const uint32_t yday = static_cast<uint32_t>(local_time.tm_yday);
    const uint32_t hour = static_cast<uint32_t>(local_time.tm_hour);
    const uint32_t minute = static_cast<uint32_t>(local_time.tm_min);

    return (((year * 366U) + yday) * 24U + hour) * 60U + minute;
}

std::vector<DueRun> LocalModeScheduler::collect_due_runs(const LocalModeSettings& settings,
                                                         uint32_t now_unix,
                                                         uint32_t valve_count,
                                                         const std::array<bool, kMaxValves>& manual_running)
{
    std::vector<DueRun> due_runs;

    if (!initialized_)
    {
        reset();
    }

    if (settings.local_mode_enabled == 0U || !is_unix_time_plausible(now_unix))
    {
        return due_runs;
    }

    std::time_t now = static_cast<std::time_t>(now_unix);
    std::tm local_time = {};
    localtime_r(&now, &local_time);

    const uint32_t max_valves = (valve_count <= kMaxValves) ? valve_count : kMaxValves;
    const uint32_t minute_key = make_minute_key(now_unix);
    const uint8_t day_bit = static_cast<uint8_t>(1U << static_cast<uint8_t>(local_time.tm_wday));

    for (uint32_t valve = 0; valve < max_valves; ++valve)
    {
        for (uint32_t program_index = 0; program_index < kMaxProgramsPerValve; ++program_index)
        {
            const ValveProgram& program = settings.valves[valve].programs[program_index];
            if (!is_program_enabled(program))
            {
                continue;
            }
            if (!is_valid_program_time(program) || !is_valid_program_duration(program))
            {
                continue;
            }
            if ((program.days_mask & day_bit) == 0U)
            {
                continue;
            }
            if (program.hour != static_cast<uint8_t>(local_time.tm_hour) ||
                program.minute != static_cast<uint8_t>(local_time.tm_min))
            {
                continue;
            }

            if (last_triggered_minute_key_[valve][program_index] == minute_key)
            {
                continue;
            }

            // Record this minute even if run is blocked by manual mode to avoid delayed catch-up.
            last_triggered_minute_key_[valve][program_index] = minute_key;

            if (manual_running[valve])
            {
                continue;
            }

            DueRun due{};
            due.valve_index = static_cast<uint8_t>(valve);
            due.program_index = static_cast<uint8_t>(program_index);
            due.duration_sec = program.duration_sec;
            due_runs.push_back(due);
        }
    }

    return due_runs;
}

} // namespace app::local_mode
