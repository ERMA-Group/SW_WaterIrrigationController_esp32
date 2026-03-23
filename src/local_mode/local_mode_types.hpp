#pragma once

#include <array>
#include <cstdint>

#include "bsw_cfg.hpp"

namespace app::local_mode {

constexpr uint32_t kMaxValves = app::sld_cfg::kNumberOfWiredOutputs;
constexpr uint32_t kMaxProgramsPerValve = 8U;
constexpr uint8_t kCurrentSchemaVersion = 1U;
constexpr uint32_t kMinDurationSec = 1U;
constexpr uint32_t kMaxDurationSec = 86400U;

struct ValveProgram {
    uint8_t enabled = 0U;
    uint8_t hour = 0U;
    uint8_t minute = 0U;
    uint16_t duration_sec = 0U;
    // Bitmask mapped to tm_wday semantics: bit0=Sunday ... bit6=Saturday.
    uint8_t days_mask = 0x7FU;
};

struct ValvePrograms {
    std::array<ValveProgram, kMaxProgramsPerValve> programs{};
};

struct LocalModeSettings {
    uint8_t schema_version = kCurrentSchemaVersion;
    uint8_t local_mode_enabled = 1U;
    std::array<ValvePrograms, kMaxValves> valves{};
};

inline bool is_valid_days_mask(uint8_t days_mask)
{
    return (days_mask & 0x7FU) != 0U;
}

inline bool is_valid_program_time(const ValveProgram& program)
{
    return program.hour <= 23U && program.minute <= 59U;
}

inline bool is_valid_program_duration(const ValveProgram& program)
{
    return program.duration_sec >= kMinDurationSec &&
           program.duration_sec <= kMaxDurationSec;
}

inline bool is_program_enabled(const ValveProgram& program)
{
    return program.enabled != 0U;
}

inline bool is_unix_time_plausible(uint32_t unix_ts)
{
    // 2000-01-01 00:00:00 UTC, avoids triggering schedules while RTC/NTP is not valid.
    return unix_ts >= 946684800U;
}

void set_defaults(LocalModeSettings& settings, uint32_t valve_count);
bool validate_and_sanitize(LocalModeSettings& settings, uint32_t valve_count);

} // namespace app::local_mode
